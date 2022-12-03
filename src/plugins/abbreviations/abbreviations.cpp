/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include "cbeditor.h"
    #include "cbstyledtextctrl.h"
    #include "configmanager.h"
    #include "configurationpanel.h"
    #include "editorcolourset.h"
    #include "editormanager.h"
    #include "logmanager.h"
    #include "macrosmanager.h"
    #include "scriptingmanager.h"
#endif

#include "abbreviations.h"
#include "abbreviationsconfigpanel.h"
#include "ccmanager.h"
#include "editor_hooks.h"
#include "scripting/bindings/sc_utils.h"
#include "scripting/bindings/sc_typeinfo_all.h"

// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
    PluginRegistrant<Abbreviations> reg("Abbreviations");
    const int idEditAutoComplete = wxNewId();
}

Abbreviations* Abbreviations::m_Singleton = nullptr;

// events handling
BEGIN_EVENT_TABLE(Abbreviations, cbPlugin)
    // add any events you want to handle here
    EVT_MENU(idEditAutoComplete,      Abbreviations::OnEditAutoComplete)
    EVT_UPDATE_UI(idEditAutoComplete, Abbreviations::OnEditMenuUpdateUI)
END_EVENT_TABLE()

wxString defaultLanguageStr = "--default--";

// constructor
Abbreviations::Abbreviations()
{
    // Make sure our resources are available.
    // In the generated boilerplate code we have no resources but when
    // we add some, it will be nice that this code is in place already ;)
    if (!Manager::LoadResource("abbreviations.zip"))
        NotifyMissingFile("abbreviations.zip");

    m_IsAutoCompVisible = false;
}

// destructor
Abbreviations::~Abbreviations()
{
}

void Abbreviations::OnAttach()
{
    // do whatever initialization you need for your plugin
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be TRUE...
    // You should check for it in other functions, because if it
    // is FALSE, it means that the application did *not* "load"
    // (see: does not need) this plugin...

    //wxASSERT(m_Singleton == 0);
    m_Singleton = this;

    LoadAutoCompleteConfig();
    RegisterScripting();

    // hook to editors
    EditorHooks::HookFunctorBase* myhook = new EditorHooks::HookFunctor<Abbreviations>(this, &Abbreviations::EditorEventHook);
    m_EditorHookId = EditorHooks::RegisterHook(myhook);
}

void Abbreviations::OnRelease(cb_unused bool appShutDown)
{
    // do de-initialization for your plugin
    // if appShutDown is true, the plugin is unloaded because Code::Blocks is being shut down,
    // which means you must not use any of the SDK Managers
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be FALSE...

    UnregisterScripting();
    SaveAutoCompleteConfig();

    if (m_Singleton == this)
        m_Singleton = nullptr;

    // unregister hook
    // 'true' will delete the functor too
    EditorHooks::UnregisterHook(m_EditorHookId, true);

    ClearAutoCompLanguageMap();
}


namespace ScriptBindings
{
    /** Try to auto-complete the current word.
      *
      * This has nothing to do with code-completion plugins. Editor auto-completion
      * is a feature that saves typing common blocks of code, e.g.
      *
      * If you have typed "forb" (no quotes) and select auto-complete, then
      * it will convert "forb" to "for ( ; ; ){ }".
      * If the word up to the caret position is an unknown keyword, nothing happens.
      *
      * These keywords/code pairs can be edited in the editor configuration
      * dialog.
      */
    SQInteger CallDoAutoComplete(HSQUIRRELVM v)
    {
        // this, ed
        ExtractParams2<SkipParam, cbEditor *> extractor(v);
        if (!extractor.Process("Abbreviations::AutoComplete"))
                return extractor.ErrorMessage();
        if (Abbreviations::Get())
            Abbreviations::Get()->DoAutoComplete(extractor.p1);
        return 0;
    }
} // namespace ScriptBindings

void Abbreviations::RegisterScripting()
{
    HSQUIRRELVM vm = Manager::Get()->GetScriptingManager()->GetVM();
    if (vm)
    {
        ScriptBindings::PreserveTop preserveTop(vm);
        sq_pushroottable(vm);
        ScriptBindings::BindMethod(vm, _SC("AutoComplete"), ScriptBindings::CallDoAutoComplete,
                                   nullptr);
        sq_poptop(vm); // Pop root table.
    }
}

void Abbreviations::UnregisterScripting()
{
    HSQUIRRELVM vm = Manager::Get()->GetScriptingManager()->GetVM();
    if (vm)
    {
        ScriptBindings::PreserveTop preserveTop(vm);
        sq_pushroottable(vm);
        sq_pushstring(vm, _SC("AutoComplete"), -1);
        sq_deleteslot(vm, -2, false);
        sq_poptop(vm);
    }
}

void Abbreviations::BuildMenu(wxMenuBar* menuBar)
{
    //NOTE: Be careful in here... The application's menubar is at your disposal.

    // if not attached, exit
    if ( !IsAttached() )
        return;

    int editmenuPos = menuBar->FindMenu(_("&Edit"));
    if (editmenuPos == wxNOT_FOUND) return;
    wxMenu* editMenu = menuBar->GetMenu(editmenuPos);

    if (editMenu)
    {
        editMenu->AppendSeparator();
        editMenu->Append(idEditAutoComplete, _("Auto-complete")+"\tCtrl-J", _("Auto-completes the word under the caret (nothing to do with code-completion plugins)"));
    }
}

static int CalcStcFontSize(cbStyledTextCtrl *stc)
{
    wxFont defaultFont = stc->StyleGetFont(wxSCI_STYLE_DEFAULT);
    defaultFont.SetPointSize(defaultFont.GetPointSize() + stc->GetZoom());
    int fontSize;
    stc->GetTextExtent("A", nullptr, &fontSize, nullptr, nullptr, &defaultFont);
    return fontSize;
}

void Abbreviations::OnEditAutoComplete(cb_unused wxCommandEvent& event)
{
    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!editor)
        return;
    cbStyledTextCtrl* control = editor->GetControl();

    const AutoCompleteMap& acm = *GetCurrentACMap(editor);

    int curPos = control->GetCurrentPos();
    int startPos = control->WordStartPosition(curPos, true);
    const int endPos = control->WordEndPosition(curPos, true);

    const wxString keyword = control->GetTextRange(startPos, endPos);
    AutoCompleteMap::const_iterator acm_it = acm.find(keyword);

    if (acm_it != acm.end())
    {
        DoAutoComplete(editor);
    }
    else
    {
        wxArrayString items;
        for (acm_it = acm.begin(); acm_it != acm.end(); ++acm_it)
        {
            if (acm_it->first.Lower().StartsWith(keyword))
                items.Add(acm_it->first + "?0");
        }

        if (!items.IsEmpty())
        {
            control->ClearRegisteredImages();
            wxString prefix(ConfigManager::GetDataFolder()+"/abbreviations.zip#zip:images/");
            const int fontSize = CalcStcFontSize(control);
            const int size = cbFindMinSize16to64(fontSize);
#if wxCHECK_VERSION(3, 1, 6)
            prefix << "svg/";
            control->RegisterImage(0, cbLoadBitmapBundleFromSVG(prefix+"arrow.svg", wxSize(size, size)).GetBitmap(wxDefaultSize));
#else
            prefix << wxString::Format("%dx%d/", size, size);
            control->RegisterImage(0, cbLoadBitmap(prefix+"arrow.png"));
#endif
            items.Sort();
            wxString itemsStr = GetStringFromArray(items, " ");
            control->AutoCompSetSeparator(' ');
            control->AutoCompSetTypeSeparator('?');
            Manager::Get()->GetCCManager()->InjectAutoCompShow(endPos - startPos, itemsStr);
        }

        m_IsAutoCompVisible = control->AutoCompActive();
    }

}

void Abbreviations::OnEditMenuUpdateUI(wxUpdateUIEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    event.Enable(ed != NULL);
}

void Abbreviations::DoAutoComplete(cbEditor* ed)
{
    if (!ed)
        return;

    cbStyledTextCtrl* control = ed->GetControl();
    if (!control)
        return;

    if (control->AutoCompActive())
        control->AutoCompCancel();

    if (control->CallTipActive())
        control->CallTipCancel();

    m_IsAutoCompVisible = false;

    LogManager* logMan = Manager::Get()->GetLogManager();
    int curPos = control->GetCurrentPos();
    int wordStartPos = control->WordStartPosition(curPos, true);
    const int endPos = control->WordEndPosition(curPos, true);
    wxString keyword = control->GetTextRange(wordStartPos, endPos);
    wxString lineIndent = ed->GetLineIndentString(control->GetCurrentLine());
    logMan->DebugLog("Auto-complete keyword: " + keyword);

    AutoCompleteMap* pAutoCompleteMap = GetCurrentACMap(ed);
    AutoCompleteMap::iterator it = pAutoCompleteMap->find(keyword);
    if (it != pAutoCompleteMap->end() )
    {
        // found; auto-complete it
        logMan->DebugLog("Auto-complete match for keyword found.");

        // indent code accordingly
        wxString code = it->second;
        code.Replace("\n", "\n" + lineIndent);

        // look for and replace macros
        int macroPos = code.Find("$(");
        while (macroPos != -1)
        {
            // locate ending parenthesis
            int macroPosEnd = macroPos + 2;
            int len = (int)code.Length();

            while (macroPosEnd < len && code.GetChar(macroPosEnd) != ')')
                ++macroPosEnd;

            if (macroPosEnd == len)
                return; // no ending parenthesis

            wxString macroName = code.SubString(macroPos + 2, macroPosEnd - 1);
            logMan->DebugLog("Found macro: " + macroName);
            wxString macro = cbGetTextFromUser(wxString::Format(_("Please enter the text for \"%s\":"), macroName),
                                               _("Macro substitution"));
            if (macro.IsEmpty())
                return;

            code.Replace("$(" + macroName + ")", macro);
            macroPos = code.Find("$(");
        }

        control->BeginUndoAction();

        // delete keyword
        control->SetSelectionVoid(wordStartPos, endPos);
        control->ReplaceSelection(wxEmptyString);
        curPos = wordStartPos;

        // replace any other macros in the generated code
        Manager::Get()->GetMacrosManager()->ReplaceMacros(code);
        // match current EOL mode
        if (control->GetEOLMode() == wxSCI_EOL_CRLF)
            code.Replace("\n", "\r\n");
        else if (control->GetEOLMode() == wxSCI_EOL_CR)
            code.Replace("\n", "\r");

        // add the text
        control->InsertText(curPos, code);

        // put cursor where "|" appears in code (if it appears)
        int caretPos = code.Find('|');
        if (caretPos != -1)
        {
            control->SetCurrentPos(curPos + caretPos);
            control->SetSelectionVoid(curPos + caretPos, curPos + caretPos + 1);
            control->ReplaceSelection(wxEmptyString);
        }
        control->ChooseCaretX();

        control->EndUndoAction();
    }
}

void Abbreviations::LoadAutoCompleteConfig()
{
    ClearAutoCompLanguageMap();
    AutoCompleteMap* pAutoCompleteMap;
    ConfigManager* cfgMgr = Manager::Get()->GetConfigManager("editor");
    wxArrayString list = cfgMgr->EnumerateSubPaths("/auto_complete");
    for (unsigned int i = 0; i < list.GetCount(); ++i)
    {
        wxString langStr = cfgMgr->Read("/auto_complete/" + list[i] + "/language", defaultLanguageStr);
        wxString name = cfgMgr->Read("/auto_complete/" + list[i] + "/name", wxEmptyString);
        wxString code = cfgMgr->Read("/auto_complete/" + list[i] + "/code", wxEmptyString);
        if (m_AutoCompLanguageMap.find(langStr) == m_AutoCompLanguageMap.end())
            m_AutoCompLanguageMap[langStr] = new AutoCompleteMap();

        pAutoCompleteMap = m_AutoCompLanguageMap[langStr];

        if (name.empty())
            continue;

        // convert non-printable chars to printable
        const size_t codeLen = code.length();
        wxString resolved;
        resolved.Alloc(codeLen);
        for (size_t pos = 0; pos < codeLen; ++pos)
        {
            if (code[pos] == '\\' && pos < codeLen - 1)
            {
                ++pos;
                if (code[pos] == 'n')
                    resolved += '\n';
                else if (code[pos] == 'r') // should not exist, remove in next step
                    resolved += '\r';
                else if (code[pos] == 't')
                    resolved += '\t';
                else if (code[pos] == '\\')
                    resolved += '\\';
                else // ?!
                {
                    resolved += '\\';
                    resolved += code[pos];
                }
            }
            else
                resolved += code[pos];
        }

        // should not exist, but remove if it does (EOL style is matched just before code generation)
        resolved.Replace("\r\n", "\n");
        resolved.Replace("\r",   "\n");
        (*pAutoCompleteMap)[name] = resolved;
    }

    if (m_AutoCompLanguageMap.find(defaultLanguageStr) == m_AutoCompLanguageMap.end())
        m_AutoCompLanguageMap[defaultLanguageStr] = new AutoCompleteMap();

    pAutoCompleteMap = m_AutoCompLanguageMap[defaultLanguageStr];
    if (pAutoCompleteMap->empty())
    {
        // default auto-complete items
        (*pAutoCompleteMap)["if"]     = "if (|)\n\t;";
        (*pAutoCompleteMap)["ifb"]    = "if (|)\n{\n\t\n}";
        (*pAutoCompleteMap)["ife"]    = "if (|)\n{\n\t\n}\nelse\n{\n\t\n}";
        (*pAutoCompleteMap)["ifei"]   = "if (|)\n{\n\t\n}\nelse if ()\n{\n\t\n}\nelse\n{\n\t\n}";
        (*pAutoCompleteMap)["guard"]  = "#ifndef $(Guard token)\n#define $(Guard token)\n\n|\n\n#endif // $(Guard token)\n";
        (*pAutoCompleteMap)["while"]  = "while (|)\n\t;";
        (*pAutoCompleteMap)["whileb"] = "while (|)\n{\n\t\n}";
        (*pAutoCompleteMap)["do"]     = "do\n{\n\t\n} while (|);";
        (*pAutoCompleteMap)["switch"] = "switch (|)\n{\ncase :\n\tbreak;\n\ndefault:\n\tbreak;\n}\n";
        (*pAutoCompleteMap)["for"]    = "for (|; ; )\n\t;";
        (*pAutoCompleteMap)["forb"]   = "for (|; ; )\n{\n\t\n}";
        (*pAutoCompleteMap)["class"]  = "class $(Class name)|\n{\npublic:\n\t$(Class name)();\n\t~$(Class name)();\nprotected:\nprivate:\n};\n";
        (*pAutoCompleteMap)["struct"] = "struct |\n{\n\t\n};\n";
    }
    ExchangeTabAndSpaces(*pAutoCompleteMap);
    // date and time macros
    // these are auto-added if they 're found to be missing
    const wxString timeAndDate[9][2] =
    {
        { "tday",   "$TDAY" },
        { "tdayu",  "$TDAY_UTC" },
        { "today",  "$TODAY" },
        { "todayu", "$TODAY_UTC" },
        { "now",    "$NOW" },
        { "nowl",   "$NOW_L" },
        { "nowu",   "$NOW_UTC" },
        { "nowlu",  "$NOW_L_UTC" },
        { "wdu",    "$WEEKDAY_UTC" }
    };

    for (int i = 0; i < 9; ++i)
    {
        if (pAutoCompleteMap->find(timeAndDate[i][0]) == pAutoCompleteMap->end())
            (*pAutoCompleteMap)[timeAndDate[i][0]] = timeAndDate[i][1];
    }

    const wxString langFortran("Fortran");
    if (m_AutoCompLanguageMap.find(langFortran) == m_AutoCompLanguageMap.end())
        m_AutoCompLanguageMap[langFortran] = new AutoCompleteMap();

    pAutoCompleteMap = m_AutoCompLanguageMap[langFortran];
    if (pAutoCompleteMap->empty())
    {
        // default auto-complete items for Fortran
        (*pAutoCompleteMap)["if"]  = "if (|) then\n\t\nend if\n";
        (*pAutoCompleteMap)["do"]  = "do |\n\t\nend do\n";
        (*pAutoCompleteMap)["dw"]  = "do while (|)\n\t\nend do\n";
        (*pAutoCompleteMap)["sc"]  = "select case (|)\n\tcase ()\n\t\t\n\tcase default\n\t\t\nend select\n";
        (*pAutoCompleteMap)["fun"] = "function |()\n\t\nend function\n";
        (*pAutoCompleteMap)["sub"] = "subroutine |()\n\t\nend subroutine\n";
        (*pAutoCompleteMap)["mod"] = "module |\n\t\nend module\n";
        (*pAutoCompleteMap)["ty"]  = "type |\n\t\nend type\n";
    }
    ExchangeTabAndSpaces(*pAutoCompleteMap);
}

void Abbreviations::ExchangeTabAndSpaces(AutoCompleteMap& map)
{
    ConfigManager* cfgMgr = Manager::Get()->GetConfigManager("editor");
    const bool useTabs = cfgMgr->ReadBool("/use_tab", false);
    const int tabSize = cfgMgr->ReadInt("/tab_size", 4);
    const wxString tabSpace = wxString(' ', tabSize);
    for (AutoCompleteMap::iterator it = map.begin(); it != map.end(); ++it)
    {
        wxString& item = it->second;
        if (useTabs)
            item.Replace(tabSpace, "\t", true);
        else
            item.Replace("\t", tabSpace, true);
    }
}

void Abbreviations::SaveAutoCompleteConfig()
{
    ConfigManager* cfgMgr = Manager::Get()->GetConfigManager("editor");
    cfgMgr->DeleteSubPath("/auto_complete");
    AutoCompLanguageMap::iterator itlan;
    AutoCompleteMap::iterator it;
    int count = 0;
    for (itlan = m_AutoCompLanguageMap.begin(); itlan != m_AutoCompLanguageMap.end(); ++itlan)
    {
        wxString langStr = itlan->first;
        wxString langStrLw = langStr.Lower();
        AutoCompleteMap* pAutoCompleteMap = itlan->second;
        for (it = pAutoCompleteMap->begin(); it != pAutoCompleteMap->end(); ++it)
        {
            wxString code = it->second;
            // convert non-printable chars to printable
            code.Replace("\\",   "\\\\");
            code.Replace("\r\n", "\\n"); // EOL style will be matched just before code generation
            code.Replace("\n",   "\\n");
            code.Replace("\r",   "\\n");
            code.Replace("\t",   "\\t");

            ++count;
            wxString key;
            if (!langStr.IsSameAs(defaultLanguageStr))
            {
                key.Printf("/auto_complete/entry%d/language", count);
                cfgMgr->Write(key, langStr);
            }

            key.Printf("/auto_complete/entry%d/name", count);
            cfgMgr->Write(key, it->first);
            key.Printf("/auto_complete/entry%d/code", count);
            cfgMgr->Write(key, code);
        }
    }
}

void Abbreviations::EditorEventHook(cbEditor* editor, wxScintillaEvent& event)
{
    cbStyledTextCtrl* control = editor->GetControl();

    if ( !IsAttached() || !m_IsAutoCompVisible || !control )
    {
        event.Skip();
        return;
    }

    if (event.GetEventType() == wxEVT_SCI_AUTOCOMP_SELECTION)
    {
        const wxString& itemText = event.GetText();
        int curPos = control->GetCurrentPos();
        int startPos = control->WordStartPosition(curPos, true);
        const int endPos = control->WordEndPosition(curPos, true);

        control->BeginUndoAction();
        control->SetTargetStart(startPos);
        control->SetTargetEnd(endPos);
        control->ReplaceTarget(itemText);
        control->GotoPos(startPos + itemText.size() );
        control->EndUndoAction();

        DoAutoComplete(editor);

        // prevent other plugins from insertion this keyword
        event.SetText(wxEmptyString);
        event.SetEventType(wxEVT_NULL);
    }
    else // here should be: else if (event.GetEventType() == wxEVT_SCI_AUTOCOMP_CANCELLED)
    {    // but is this event doesn't occur.
        m_IsAutoCompVisible = control->AutoCompActive();
    }

    if (!m_IsAutoCompVisible)
      event.Skip(); // allow others to handle this event
}

cbConfigurationPanel* Abbreviations::GetConfigurationPanel(wxWindow* parent)
{
    return new AbbreviationsConfigPanel(parent, this);
}

void Abbreviations::ClearAutoCompLanguageMap()
{
    AutoCompLanguageMap::iterator it;
    for (it=m_AutoCompLanguageMap.begin(); it!=m_AutoCompLanguageMap.end(); ++it)
    {
        it->second->clear();
        delete it->second;
        it->second = 0;
    }
    m_AutoCompLanguageMap.clear();
}

AutoCompleteMap* Abbreviations::GetCurrentACMap(cbEditor* ed)
{
    AutoCompleteMap* pAutoCompleteMap;
    EditorColourSet* colour_set = ed->GetColourSet();
    if (colour_set)
    {
        wxString strLang = colour_set->GetLanguageName(ed->GetLanguage());

        if (strLang == "Fortran77") // the same abbreviations for Fortran and Fortran77
            strLang = "Fortran";

        if (m_AutoCompLanguageMap.find(strLang) == m_AutoCompLanguageMap.end())
            pAutoCompleteMap = m_AutoCompLanguageMap[defaultLanguageStr];
        else
            pAutoCompleteMap = m_AutoCompLanguageMap[strLang];
    }
    else
        pAutoCompleteMap = m_AutoCompLanguageMap[defaultLanguageStr];

    return pAutoCompleteMap;
}
