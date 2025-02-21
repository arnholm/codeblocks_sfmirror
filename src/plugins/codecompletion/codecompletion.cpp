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
    #include <algorithm>
    #include <iterator>
    #include <set> // for handling unique items in some places

    #include <wx/choicdlg.h>
    #include <wx/choice.h>
    #include <wx/dir.h>
    #include <wx/filename.h>
    #include <wx/fs_zip.h>
    #include <wx/menu.h>
    #include <wx/mimetype.h>
    #include <wx/msgdlg.h>
    #include <wx/regex.h>
    #include <wx/tipwin.h>
    #include <wx/toolbar.h>
    #include <wx/utils.h>
    #include <wx/xrc/xmlres.h>
    #include <wx/wxscintilla.h>

    #include <cbeditor.h>
    #include <configmanager.h>
    #include <editorcolourset.h>
    #include <editormanager.h>
    #include <globals.h>
    #include <logmanager.h>
    #include <macrosmanager.h>
    #include <manager.h>
    #include <projectmanager.h>
    #include <sdk_events.h>
#endif

#include <wx/tokenzr.h>
#include <wx/html/htmlwin.h>

#include <cbstyledtextctrl.h>
#include <editor_hooks.h>
#include <filegroupsandmasks.h>
#include <multiselectdlg.h>

#include "codecompletion.h"

#include "ccoptionsdlg.h"
#include "ccoptionsprjdlg.h"
#include "insertclassmethoddlg.h"
#include "selectincludefile.h"
#include "parser/ccdebuginfo.h"
#include "parser/cclogger.h"
#include "parser/parser.h"
#include "parser/tokenizer.h"
#include "doxygen_parser.h" // for DocumentationPopup and DoxygenParser
#include "gotofunctiondlg.h"

#define CC_CODECOMPLETION_DEBUG_OUTPUT 0

// let the global debug macro overwrite the local debug macro value
#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #undef CC_CODECOMPLETION_DEBUG_OUTPUT
    #define CC_CODECOMPLETION_DEBUG_OUTPUT CC_GLOBAL_DEBUG_OUTPUT
#endif

#if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(F(format, ##args))
    #define TRACE2(format, args...)
#elif CC_CODECOMPLETION_DEBUG_OUTPUT == 2
    #define TRACE(format, args...)                            \
        do                                                    \
        {                                                     \
            if (g_EnableDebugTrace)                           \
                CCLogger::Get()->DebugLog(F(format, ##args)); \
        }                                                     \
        while (false)
    #define TRACE2(format, args...) \
        CCLogger::Get()->DebugLog(F(format, ##args))
#else
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif

/// Scopes choice name for global functions in CC's toolbar.
static wxString g_GlobalScope(_T("<global>"));

// this auto-registers the plugin
namespace
{
    PluginRegistrant<CodeCompletion> reg(_T("CodeCompletion"));
}

namespace CodeCompletionHelper
{
    // compare method for the sort algorithm for our FunctionScope struct
    inline bool LessFunctionScope(const CodeCompletion::FunctionScope& fs1, const CodeCompletion::FunctionScope& fs2)
    {
        int result = wxStricmp(fs1.Scope, fs2.Scope);
        if (result == 0)
        {
            result = wxStricmp(fs1.Name, fs2.Name);
            if (result == 0)
                result = fs1.StartLine - fs2.StartLine;
        }

        return result < 0;
    }

    inline bool EqualFunctionScope(const CodeCompletion::FunctionScope& fs1, const CodeCompletion::FunctionScope& fs2)
    {
        int result = wxStricmp(fs1.Scope, fs2.Scope);
        if (result == 0)
            result = wxStricmp(fs1.Name, fs2.Name);

        return result == 0;
    }

    inline bool LessNameSpace(const NameSpace& ns1, const NameSpace& ns2)
    {
        return ns1.Name < ns2.Name;
    }

    inline bool EqualNameSpace(const NameSpace& ns1, const NameSpace& ns2)
    {
        return ns1.Name == ns2.Name;
    }

    /// for OnGotoFunction(), search backward
    /// @code
    /// xxxxx  /* yyy */
    ///     ^             ^
    ///     result        begin
    /// @endcode
    inline wxChar GetLastNonWhitespaceChar(cbStyledTextCtrl* control, int position)
    {
        if (!control)
            return 0;

        while (--position > 0)
        {
            const int style = control->GetStyleAt(position);
            if (control->IsComment(style))
                continue;

            const wxChar ch = control->GetCharAt(position);
            if (ch <= _T(' '))
                continue;

            return ch;
        }

        return 0;
    }

    /// for OnGotoFunction(), search forward
    ///        /* yyy */  xxxxx
    ///     ^             ^
    ///     begin         result
    inline wxChar GetNextNonWhitespaceChar(cbStyledTextCtrl* control, int position)
    {
        if (!control)
            return 0;

        const int totalLength = control->GetLength();
        --position;
        while (++position < totalLength)
        {
            const int style = control->GetStyleAt(position);
            if (control->IsComment(style))
                continue;

            const wxChar ch = control->GetCharAt(position);
            if (ch <= _T(' '))
                continue;

            return ch;
        }

        return 0;
    }

    /**  Sorting in GetLocalIncludeDirs() */
    inline int CompareStringLen(const wxString& first, const wxString& second)
    {
        return second.Len() - first.Len();
    }

    /**  for CodeCompleteIncludes()
     * a line has some pattern like below
     @code
        # [space or tab] include
     @endcode
     */
    inline bool TestIncludeLine(wxString const &line)
    {
        size_t index = line.find(_T('#'));
        if (index == wxString::npos)
            return false;
        ++index;

        for (; index < line.length(); ++index)
        {
            if (line[index] != _T(' ') && line[index] != _T('\t'))
            {
                if (line.Mid(index, 7) == _T("include"))
                    return true;
                break;
            }
        }
        return false;
    }

    /** return identifier like token string under the current cursor pointer
     * @param[out] NameUnderCursor the identifier like token string
     * @param[out] IsInclude true if it is a #include command
     * @return true if the underlining text is a #include command, or a normal identifier
     */
    inline bool EditorHasNameUnderCursor(wxString& NameUnderCursor, bool& IsInclude)
    {
        bool ReturnValue = false;
        if (cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor())
        {
            cbStyledTextCtrl* control = ed->GetControl();
            const int pos = control->GetCurrentPos();
            const wxString line = control->GetLine(control->LineFromPosition(pos));
            const wxRegEx reg("^[[:blank:]]*#[[:blank:]]*include[[:blank:]]+[\"<]([^\">]+)[\">]");
            wxString inc;
            if (reg.Matches(line))
                inc = reg.GetMatch(line, 1);

            if (!inc.IsEmpty())
            {
                NameUnderCursor = inc;
                ReturnValue = true;
                IsInclude = true;
            }
            else
            {
                const int start = control->WordStartPosition(pos, true);
                const int end = control->WordEndPosition(pos, true);
                const wxString word = control->GetTextRange(start, end);
                if (!word.IsEmpty())
                {
                    NameUnderCursor.Clear();
                    NameUnderCursor << word;
                    ReturnValue = true;
                    IsInclude = false;
                }
            }
        }
        return ReturnValue;
    }
    /** used to record the position of a token when user click find declaration or implementation */
    struct GotoDeclarationItem
    {
        wxString filename;
        unsigned line;
    };

    /** when user select one item in the suggestion list, the selected contains the full display
     * name, for example, "function_name():function_return_type", and we only need to insert the
     * "function_name" to the editor, so this function just get the actual inserted text.
     * @param selected a full display name of the selected token in the suggestion list
     * @return the stripped text which are used to insert to the editor
     */
    static wxString AutocompGetName(const wxString& selected)
    {
        size_t nameEnd = selected.find_first_of(_T("(: "));
        return selected.substr(0,nameEnd);
    }

}//namespace CodeCompletionHelper

// menu IDs
// just because we don't know other plugins' used identifiers,
// we use wxNewId() to generate a guaranteed unique ID ;), instead of enum
// (don't forget that, especially in a plugin)
// used in the wxFrame's main menu
int idMenuGotoFunction          = wxNewId();
int idMenuGotoPrevFunction      = wxNewId();
int idMenuGotoNextFunction      = wxNewId();
int idMenuGotoDeclaration       = wxNewId();
int idMenuGotoImplementation    = wxNewId();
int idMenuOpenIncludeFile       = wxNewId();
int idMenuFindReferences        = wxNewId();
int idMenuRenameSymbols         = wxNewId();
int idViewClassBrowser          = wxNewId();
// used in context menu
int idCurrentProjectReparse     = wxNewId();
int idSelectedProjectReparse    = wxNewId();
int idSelectedFileReparse       = wxNewId();
int idEditorSubMenu             = wxNewId();
int idClassMethod               = wxNewId();
int idUnimplementedClassMethods = wxNewId();
int idGotoDeclaration           = wxNewId();
int idGotoImplementation        = wxNewId();
int idOpenIncludeFile           = wxNewId();

int idRealtimeParsingTimer      = wxNewId();
int idToolbarTimer              = wxNewId();
int idProjectSavedTimer         = wxNewId();
int idReparsingTimer            = wxNewId();
int idEditorActivatedTimer      = wxNewId();

// all the below delay time is in milliseconds units
// when the user enables the parsing while typing option, this is the time delay when parsing
// would happen after the editor has changed.
#define REALTIME_PARSING_DELAY    500

// there are many reasons to trigger the refreshing of CC toolbar. But to avoid refreshing
// the toolbar too often, we add a timer to delay the refresh, this is just like a mouse dwell
// event, which means we do the real job when the editor is stable for a while (no event
// happens in the delay time period).
#define TOOLBAR_REFRESH_DELAY     150

// the time delay between an editor activated event and the updating of the CC toolbar.
// Note that we are only interest in a stable activated editor, so if another editor is activated
// during the time delay, the timer will be restarted.
#define EDITOR_ACTIVATED_DELAY    300

BEGIN_EVENT_TABLE(CodeCompletion, cbCodeCompletionPlugin)
    EVT_UPDATE_UI_RANGE(idMenuGotoFunction, idCurrentProjectReparse, CodeCompletion::OnUpdateUI)

    EVT_MENU(idMenuGotoFunction,                   CodeCompletion::OnGotoFunction             )
    EVT_MENU(idMenuGotoPrevFunction,               CodeCompletion::OnGotoPrevFunction         )
    EVT_MENU(idMenuGotoNextFunction,               CodeCompletion::OnGotoNextFunction         )
    EVT_MENU(idMenuGotoDeclaration,                CodeCompletion::OnGotoDeclaration          )
    EVT_MENU(idMenuGotoImplementation,             CodeCompletion::OnGotoDeclaration          )
    EVT_MENU(idMenuFindReferences,                 CodeCompletion::OnFindReferences           )
    EVT_MENU(idMenuRenameSymbols,                  CodeCompletion::OnRenameSymbols            )
    EVT_MENU(idClassMethod,                        CodeCompletion::OnClassMethod              )
    EVT_MENU(idUnimplementedClassMethods,          CodeCompletion::OnUnimplementedClassMethods)
    EVT_MENU(idGotoDeclaration,                    CodeCompletion::OnGotoDeclaration          )
    EVT_MENU(idGotoImplementation,                 CodeCompletion::OnGotoDeclaration          )
    EVT_MENU(idOpenIncludeFile,                    CodeCompletion::OnOpenIncludeFile          )
    EVT_MENU(idMenuOpenIncludeFile,                CodeCompletion::OnOpenIncludeFile          )

    EVT_MENU(idViewClassBrowser,                   CodeCompletion::OnViewClassBrowser      )
    EVT_MENU(idCurrentProjectReparse,              CodeCompletion::OnCurrentProjectReparse )
    EVT_MENU(idSelectedProjectReparse,             CodeCompletion::OnSelectedProjectReparse)
    EVT_MENU(idSelectedFileReparse,                CodeCompletion::OnSelectedFileReparse   )

    // CC's toolbar
    EVT_CHOICE(XRCID("chcCodeCompletionScope"),    CodeCompletion::OnScope   )
    EVT_CHOICE(XRCID("chcCodeCompletionFunction"), CodeCompletion::OnFunction)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
CodeCompletion::CodeCompletion() :
    // ----------------------------------------------------------------------------
    m_InitDone(false),
    m_CodeRefactoring(m_ParseManager),
    m_EditorHookId(0),
    m_TimerRealtimeParsing(this, idRealtimeParsingTimer),
    m_TimerToolbar(this, idToolbarTimer),
    m_TimerProjectSaved(this, idProjectSavedTimer),
    m_TimerReparsing(this, idReparsingTimer),
    m_TimerEditorActivated(this, idEditorActivatedTimer),
    m_LastEditor(0),
    m_ToolBar(0),
    m_Function(0),
    m_Scope(0),
    m_ToolbarNeedRefresh(true),
    m_ToolbarNeedReparse(false),
    m_CurrentLine(0),
    m_NeedReparse(false),
    m_CurrentLength(-1),
    m_NeedsBatchColour(true),
    m_CCMaxMatches(16384),
    m_CCAutoAddParentheses(true),
    m_CCDetectImplementation(false),
    m_CCEnableHeaders(false),
    m_CCEnablePlatformCheck(true),
    m_SystemHeadersThreadCS(),
    m_DocHelper(this)
{

    // CCLogger are the log event bridges, those events were finally handled by its parent, here
    // it is the CodeCompletion plugin ifself.
    CCLogger::Get()->Init(this, g_idCCLogger, g_idCCDebugLogger);

    if (!Manager::LoadResource(_T("codecompletion.zip")))
        NotifyMissingFile(_T("codecompletion.zip"));

    // handling events send from CCLogger
    Connect(g_idCCLogger,                wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(CodeCompletion::OnCCLogger)     );
    Connect(g_idCCDebugLogger,           wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(CodeCompletion::OnCCDebugLogger));

    // the two events below were generated from ParseManager, as currently, CodeCompletionPlugin is
    // set as the next event handler for m_ParseManager, so it get chance to handle them.
    Connect(ParserCommon::idParserStart, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CodeCompletion::OnParserStart)  );
    Connect(ParserCommon::idParserEnd,   wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CodeCompletion::OnParserEnd)    );

    Connect(idRealtimeParsingTimer, wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnRealtimeParsingTimer));
    Connect(idToolbarTimer,         wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnToolbarTimer)        );
    Connect(idProjectSavedTimer,    wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnProjectSavedTimer)   );
    Connect(idReparsingTimer,       wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnReparsingTimer)      );
    Connect(idEditorActivatedTimer, wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnEditorActivatedTimer));

    Connect(idSystemHeadersThreadMessage, wxEVT_COMMAND_MENU_SELECTED,
            CodeBlocksThreadEventHandler(CodeCompletion::OnSystemHeadersThreadMessage));
    Connect(idSystemHeadersThreadFinish, wxEVT_COMMAND_MENU_SELECTED,
            CodeBlocksThreadEventHandler(CodeCompletion::OnSystemHeadersThreadFinish));
}

// ----------------------------------------------------------------------------
CodeCompletion::~CodeCompletion()
// ----------------------------------------------------------------------------
{
    Disconnect(g_idCCLogger,                wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(CodeCompletion::OnCCLogger));
    Disconnect(g_idCCDebugLogger,           wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(CodeCompletion::OnCCDebugLogger));
    Disconnect(ParserCommon::idParserStart, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CodeCompletion::OnParserStart));
    Disconnect(ParserCommon::idParserEnd,   wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CodeCompletion::OnParserEnd));

    Disconnect(idRealtimeParsingTimer, wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnRealtimeParsingTimer));
    Disconnect(idToolbarTimer,         wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnToolbarTimer)        );
    Disconnect(idProjectSavedTimer,    wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnProjectSavedTimer)   );
    Disconnect(idReparsingTimer,       wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnReparsingTimer)      );
    Disconnect(idEditorActivatedTimer, wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnEditorActivatedTimer));

    Disconnect(idSystemHeadersThreadMessage, wxEVT_COMMAND_MENU_SELECTED,
               CodeBlocksThreadEventHandler(CodeCompletion::OnSystemHeadersThreadMessage));
    Disconnect(idSystemHeadersThreadFinish, wxEVT_COMMAND_MENU_SELECTED,
               CodeBlocksThreadEventHandler(CodeCompletion::OnSystemHeadersThreadFinish));

    // clean up all the running thread
    while (!m_SystemHeadersThreads.empty())
    {
        SystemHeadersThread* thread = m_SystemHeadersThreads.front();
        thread->Wait();
        delete thread;
        m_SystemHeadersThreads.pop_front();
    }
}

// ----------------------------------------------------------------------------
void CodeCompletion::OnAttach()
// ----------------------------------------------------------------------------
{
    m_EditMenu    = 0;
    m_SearchMenu  = 0;
    m_ViewMenu    = 0;
    m_ProjectMenu = 0;
    // toolbar related variables
    m_ToolBar     = 0;
    m_Function    = 0;
    m_Scope       = 0;
    m_FunctionsScope.clear();
    m_NameSpaces.clear();
    m_AllFunctionsScopes.clear();
    m_ToolbarNeedRefresh = true; // by default

    m_LastFile.clear();

    // read options from configure file
    RereadOptions();

    // Events which m_ParseManager does not handle will go to the the next event
    // handler which is the instance of a CodeCompletion.
    m_ParseManager.SetNextHandler(this);

    m_ParseManager.CreateClassBrowser();

    // hook to editors
    // both ccmanager and cc have hooks, but they don't conflict. ccmanager are mainly
    // hooking to the event such as key stroke or mouse dwell events, so the code completion, call tip
    // and tool tip will be handled in ccmanager. The other cases such as caret movement triggers
    // updating the CC's toolbar, modifying the editor causing the real time content reparse will be
    // handled inside cc's own editor hook.
    EditorHooks::HookFunctorBase* myhook = new EditorHooks::HookFunctor<CodeCompletion>(this, &CodeCompletion::EditorEventHook);
    m_EditorHookId = EditorHooks::RegisterHook(myhook);

    // register event sinks
    Manager* pm = Manager::Get();

    pm->RegisterEventSink(cbEVT_APP_STARTUP_DONE,     new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnAppDoneStartup));

    pm->RegisterEventSink(cbEVT_WORKSPACE_CHANGED,    new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnWorkspaceChanged));

    pm->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,     new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnProjectActivated));
    pm->RegisterEventSink(cbEVT_PROJECT_CLOSE,        new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnProjectClosed));
    pm->RegisterEventSink(cbEVT_PROJECT_SAVE,         new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnProjectSaved));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_ADDED,   new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnProjectFileAdded));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_REMOVED, new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnProjectFileRemoved));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_CHANGED, new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnProjectFileChanged));

    pm->RegisterEventSink(cbEVT_EDITOR_SAVE,          new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnEditorSave));
    pm->RegisterEventSink(cbEVT_EDITOR_OPEN,          new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnEditorOpen));
    pm->RegisterEventSink(cbEVT_EDITOR_ACTIVATED,     new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnEditorActivated));
    pm->RegisterEventSink(cbEVT_EDITOR_CLOSE,         new cbEventFunctor<CodeCompletion, CodeBlocksEvent>(this, &CodeCompletion::OnEditorClosed));

    m_DocHelper.OnAttach();
}

void CodeCompletion::OnRelease(bool appShutDown)
{
    m_ParseManager.RemoveClassBrowser(appShutDown);
    m_ParseManager.ClearParsers();

    // remove chained handler
    m_ParseManager.SetNextHandler(nullptr);

    // unregister hook
    // 'true' will delete the functor too
    EditorHooks::UnregisterHook(m_EditorHookId, true);

    // remove registered event sinks
    Manager::Get()->RemoveAllEventSinksFor(this);

    m_FunctionsScope.clear();
    m_NameSpaces.clear();
    m_AllFunctionsScopes.clear();
    m_ToolbarNeedRefresh = false;

/* TODO (mandrav#1#): Delete separator line too... */
    if (m_EditMenu)
        m_EditMenu->Delete(idMenuRenameSymbols);

    if (m_SearchMenu)
    {
        m_SearchMenu->Delete(idMenuGotoFunction);
        m_SearchMenu->Delete(idMenuGotoPrevFunction);
        m_SearchMenu->Delete(idMenuGotoNextFunction);
        m_SearchMenu->Delete(idMenuGotoDeclaration);
        m_SearchMenu->Delete(idMenuGotoImplementation);
        m_SearchMenu->Delete(idMenuFindReferences);
        m_SearchMenu->Delete(idMenuOpenIncludeFile);
    }

    m_DocHelper.OnRelease();
}

cbConfigurationPanel* CodeCompletion::GetConfigurationPanel(wxWindow* parent)
{
    return new CCOptionsDlg(parent, &m_ParseManager, this, &m_DocHelper);
}

cbConfigurationPanel* CodeCompletion::GetProjectConfigurationPanel(wxWindow* parent, cbProject* project)
{
    return new CCOptionsProjectDlg(parent, project, &m_ParseManager);
}

void CodeCompletion::BuildMenu(wxMenuBar* menuBar)
{
    // if not attached, exit
    if (!IsAttached())
        return;

    int pos = menuBar->FindMenu(_("&Edit"));
    if (pos != wxNOT_FOUND)
    {
        m_EditMenu = menuBar->GetMenu(pos);
        m_EditMenu->AppendSeparator();
        m_EditMenu->Append(idMenuRenameSymbols, _("Rename symbols\tAlt-N"));
    }
    else
        CCLogger::Get()->DebugLog(_T("Could not find Edit menu!"));

    pos = menuBar->FindMenu(_("Sea&rch"));
    if (pos != wxNOT_FOUND)
    {
        m_SearchMenu = menuBar->GetMenu(pos);
        m_SearchMenu->Append(idMenuGotoFunction,       _("Goto function...\tCtrl-Shift-G"));
        m_SearchMenu->Append(idMenuGotoPrevFunction,   _("Goto previous function\tCtrl-PgUp"));
        m_SearchMenu->Append(idMenuGotoNextFunction,   _("Goto next function\tCtrl-PgDn"));
        m_SearchMenu->Append(idMenuGotoDeclaration,    _("Goto declaration\tCtrl-Shift-."));
        m_SearchMenu->Append(idMenuGotoImplementation, _("Goto implementation\tCtrl-."));
        m_SearchMenu->Append(idMenuFindReferences,     _("Find references\tAlt-."));
        m_SearchMenu->Append(idMenuOpenIncludeFile,    _("Open include file"));
    }
    else
        CCLogger::Get()->DebugLog(_T("Could not find Search menu!"));

    // add the classbrowser window in the "View" menu
    int idx = menuBar->FindMenu(_("&View"));
    if (idx != wxNOT_FOUND)
    {
        m_ViewMenu = menuBar->GetMenu(idx);
        wxMenuItemList& items = m_ViewMenu->GetMenuItems();
        bool inserted = false;

        // find the first separator and insert before it
        for (size_t i = 0; i < items.GetCount(); ++i)
        {
            if (items[i]->IsSeparator())
            {
                m_ViewMenu->InsertCheckItem(i, idViewClassBrowser, _("Symbols browser"), _("Toggle displaying the symbols browser"));
                inserted = true;
                break;
            }
        }

        // not found, just append
        if (!inserted)
            m_ViewMenu->AppendCheckItem(idViewClassBrowser, _("Symbols browser"), _("Toggle displaying the symbols browser"));
    }
    else
        CCLogger::Get()->DebugLog(_T("Could not find View menu!"));

    // add Reparse item in the "Project" menu
    idx = menuBar->FindMenu(_("&Project"));
    if (idx != wxNOT_FOUND)
    {
        m_ProjectMenu = menuBar->GetMenu(idx);
        wxMenuItemList& items = m_ProjectMenu->GetMenuItems();
        bool inserted = false;

        // find the first separator and insert before it
        for (size_t i = items.GetCount() - 1; i > 0; --i)
        {
            if (items[i]->IsSeparator())
            {
                m_ProjectMenu->InsertSeparator(i);
                m_ProjectMenu->Insert(i + 1, idCurrentProjectReparse, _("Reparse current project"), _("Reparse of the final switched project"));
                inserted = true;
                break;
            }
        }

        // not found, just append
        if (!inserted)
        {
            m_ProjectMenu->AppendSeparator();
            m_ProjectMenu->Append(idCurrentProjectReparse, _("Reparse current project"), _("Reparse of the final switched project"));
        }
    }
    else
        CCLogger::Get()->DebugLog(_T("Could not find Project menu!"));
}

void CodeCompletion::BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
{
    // if not attached, exit
    if (!menu || !IsAttached() || !m_InitDone)
        return;

    if (type == mtEditorManager)
    {
        if (cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor())
        {
            if ( !IsProviderFor(ed) )
                return;
        }

        wxString NameUnderCursor;
        bool IsInclude = false;
        const bool nameUnderCursor = CodeCompletionHelper::EditorHasNameUnderCursor(NameUnderCursor, IsInclude);
        if (nameUnderCursor)
        {
            PluginManager *pluginManager = Manager::Get()->GetPluginManager();

            if (IsInclude)
            {
                wxString msg;
                msg.Printf(_("Open #include file: '%s'"), NameUnderCursor.wx_str());
                menu->Insert(0, idOpenIncludeFile, msg);
                menu->Insert(1, wxID_SEPARATOR, wxEmptyString);
                pluginManager->RegisterFindMenuItems(true, 2);
            }
            else
            {
                int initialPos = pluginManager->GetFindMenuItemFirst();
                int pos = initialPos;
                wxString msg;
                msg.Printf(_("Find declaration of: '%s'"), NameUnderCursor.wx_str());
                menu->Insert(pos++, idGotoDeclaration, msg);

                msg.Printf(_("Find implementation of: '%s'"), NameUnderCursor.wx_str());
                menu->Insert(pos++, idGotoImplementation, msg);

                if (m_ParseManager.GetParser().Done())
                {
                    msg.Printf(_("Find references of: '%s'"), NameUnderCursor.wx_str());
                    menu->Insert(pos++, idMenuFindReferences, msg);
                }
                pluginManager->RegisterFindMenuItems(false, pos - initialPos);
            }
        }

        const int insertId = menu->FindItem(_("Insert/Refactor"));
        if (insertId != wxNOT_FOUND)
        {
            if (wxMenuItem* insertMenu = menu->FindItem(insertId, 0))
            {
                if (wxMenu* subMenu = insertMenu->GetSubMenu())
                {
                    subMenu->Append(idClassMethod, _("Class method declaration/implementation..."));
                    subMenu->Append(idUnimplementedClassMethods, _("All class methods without implementation..."));

                    subMenu->AppendSeparator();

                    const bool enableRename = (m_ParseManager.GetParser().Done() && nameUnderCursor && !IsInclude);
                    subMenu->Append(idMenuRenameSymbols, _("Rename symbols"), _("Rename symbols under cursor"));
                    subMenu->Enable(idMenuRenameSymbols, enableRename);
                }
                else
                    CCLogger::Get()->DebugLog(_T("Could not find Insert menu 3!"));
            }
            else
                CCLogger::Get()->DebugLog(_T("Could not find Insert menu 2!"));
        }
        else
            CCLogger::Get()->DebugLog(_T("Could not find Insert menu!"));
    }
    else if (type == mtProjectManager)
    {
        if (data)
        {
            if (data->GetKind() == FileTreeData::ftdkProject)
            {
                size_t position = menu->GetMenuItemCount();
                int id = menu->FindItem(_("Build"));
                if (id != wxNOT_FOUND)
                    menu->FindChildItem(id, &position);
                menu->Insert(position, idSelectedProjectReparse, _("Reparse this project"),
                             _("Reparse current actived project"));
            }
            else if (data->GetKind() == FileTreeData::ftdkFile)
                menu->Append(idSelectedFileReparse, _("Reparse this file"), _("Reparse current selected file"));
        }
    }
}

bool CodeCompletion::BuildToolBar(wxToolBar* toolBar)
{
    // load the toolbar resource
    Manager::Get()->AddonToolBar(toolBar,_T("codecompletion_toolbar"));
    // get the wxChoice control pointers
    m_Function = XRCCTRL(*toolBar, "chcCodeCompletionFunction", wxChoice);
    m_Scope    = XRCCTRL(*toolBar, "chcCodeCompletionScope",    wxChoice);

    m_ToolBar = toolBar;

    // set the wxChoice and best toolbar size
    UpdateToolBar();

    // disable the wxChoices
    EnableToolbarTools(false);

    return true;
}

CodeCompletion::CCProviderStatus CodeCompletion::GetProviderStatusFor(cbEditor* ed)
{
    EditorColourSet *colour_set = ed->GetColourSet();
    if (colour_set && ed->GetLanguage() == colour_set->GetHighlightLanguage(wxT("C/C++")))
        return ccpsActive;

    switch (ParserCommon::FileType(ed->GetFilename()))
    {
        case ParserCommon::ftHeader:
        case ParserCommon::ftSource:
            return ccpsActive;

        case ParserCommon::ftOther:
            return ccpsInactive;
        default:
            break;
    }
    return ccpsUniversal;
}

std::vector<CodeCompletion::CCToken> CodeCompletion::GetAutocompList(bool isAuto, cbEditor* ed, int& tknStart, int& tknEnd)
{
    std::vector<CCToken> tokens;

    if (!IsAttached() || !m_InitDone)
        return tokens;

    cbStyledTextCtrl* stc = ed->GetControl();
    const int style = stc->GetStyleAt(tknEnd);
    const wxChar curChar = stc->GetCharAt(tknEnd - 1);

    if (isAuto) // filter illogical cases of auto-launch
    {
        // AutocompList can be prompt after user typed "::" or "->"
        // or if in preprocessor directive, after user typed "<" or "\"" or "/"
        if (   (   curChar == wxT(':') // scope operator
                && stc->GetCharAt(tknEnd - 2) != wxT(':') )
            || (   curChar == wxT('>') // '->'
                && stc->GetCharAt(tknEnd - 2) != wxT('-') )
            || (   wxString(wxT("<\"/")).Find(curChar) != wxNOT_FOUND // #include directive
                && !stc->IsPreprocessor(style) ) )
        {
            return tokens;
        }
    }

    const int lineIndentPos = stc->GetLineIndentPosition(stc->GetCurrentLine());
    const wxChar lineFirstChar = stc->GetCharAt(lineIndentPos);

    if (lineFirstChar == wxT('#'))
    {
        const int startPos = stc->WordStartPosition(lineIndentPos + 1, true);
        const int endPos = stc->WordEndPosition(lineIndentPos + 1, true);
        const wxString str = stc->GetTextRange(startPos, endPos);

        if (str == wxT("include") && tknEnd > endPos)
        {
            DoCodeCompleteIncludes(ed, tknStart, tknEnd, tokens);
        }
        else if (endPos >= tknEnd && tknEnd > lineIndentPos)
            DoCodeCompletePreprocessor(tknStart, tknEnd, ed, tokens);
        else if ( (   str == wxT("define")
                   || str == wxT("if")
                   || str == wxT("ifdef")
                   || str == wxT("ifndef")
                   || str == wxT("elif")
                   || str == wxT("elifdef")
                   || str == wxT("elifndef")
                   || str == wxT("undef") )
                 && tknEnd > endPos )
        {
            DoCodeComplete(tknEnd, ed, tokens, true);
        }
        return tokens;
    }
    else if (curChar == wxT('#'))
        return tokens;
    else if (lineFirstChar == wxT(':') && curChar == _T(':'))
        return tokens;

    if (   stc->IsString(style)
        || stc->IsComment(style)
        || stc->IsCharacter(style)
        || stc->IsPreprocessor(style) )
    {
        return tokens;
    }

    DoCodeComplete(tknEnd, ed, tokens);
    return tokens;
}

static int CalcStcFontSize(cbStyledTextCtrl *stc)
{
    wxFont defaultFont = stc->StyleGetFont(wxSCI_STYLE_DEFAULT);
    defaultFont.SetPointSize(defaultFont.GetPointSize() + stc->GetZoom());
    int fontSize;
    stc->GetTextExtent(wxT("A"), nullptr, &fontSize, nullptr, nullptr, &defaultFont);
    return fontSize;
}

void CodeCompletion::DoCodeComplete(int caretPos, cbEditor* ed, std::vector<CCToken>& tokens, bool preprocessorOnly)
{
    const bool caseSens = m_ParseManager.GetParser().Options().caseSensitive;
    cbStyledTextCtrl* stc = ed->GetControl();

    TokenIdxSet result;
    if (   m_ParseManager.MarkItemsByAI(result, m_ParseManager.GetParser().Options().useSmartSense, true, caseSens, caretPos)
        || m_ParseManager.LastAISearchWasGlobal() ) // enter even if no match (code-complete C++ keywords)
    {
        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("%zu results", result.size()));
        TRACE(wxString::Format("%zu results", result.size()));

        if (result.size() <= m_CCMaxMatches)
        {
            if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(wxT("Generating tokens list..."));

            const int fontSize = CalcStcFontSize(stc);
            wxImageList* ilist = m_ParseManager.GetImageList(fontSize);
            stc->ClearRegisteredImages();

            tokens.reserve(result.size());
            std::set<int> alreadyRegistered;
            StringSet uniqueStrings; // ignore keywords with same name as parsed tokens

            TokenTree* tree = m_ParseManager.GetParser().GetTokenTree();

            CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

            for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
            {
                const Token* token = tree->at(*it);
                if (!token || token->m_Name.IsEmpty())
                    continue;

                if (preprocessorOnly && token->m_TokenKind != tkMacroDef)
                    continue;

                int iidx = m_ParseManager.GetTokenKindImage(token);
                if (alreadyRegistered.find(iidx) == alreadyRegistered.end())
                {
                    stc->RegisterImage(iidx, ilist->GetBitmap(iidx));
                    alreadyRegistered.insert(iidx);
                }

                wxString dispStr;
                if (token->m_TokenKind & tkAnyFunction)
                {
                    if (m_DocHelper.IsEnabled())
                        dispStr = wxT("(): ") + token->m_FullType;
                    else
                        dispStr = token->GetFormattedArgs() << _T(": ") << token->m_FullType;
                }
                else if (token->m_TokenKind == tkVariable)
                    dispStr = wxT(": ") + token->m_FullType;
                tokens.push_back(CCToken(token->m_Index, token->m_Name + dispStr, token->m_Name, token->m_IsTemp ? 0 : 5, iidx));
                uniqueStrings.insert(token->m_Name);

                if (token->m_TokenKind == tkNamespace && token->m_Aliases.size())
                {
                    for (size_t i = 0; i < token->m_Aliases.size(); ++i)
                    {
                        // dispStr will currently be empty, but contain something in the future...
                        tokens.push_back(CCToken(token->m_Index, token->m_Aliases[i] + dispStr, token->m_Aliases[i], 5, iidx));
                        uniqueStrings.insert(token->m_Aliases[i]);
                    }
                }
            }

            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

            if (m_ParseManager.LastAISearchWasGlobal() && !preprocessorOnly)
            {
                // empty or partial search phrase: add theme keywords in search list
                if (g_DebugSmartSense)
                    CCLogger::Get()->DebugLog(_T("Last AI search was global: adding theme keywords in list"));

                EditorColourSet* colour_set = ed->GetColourSet();
                if (colour_set)
                {
                    wxString lastSearch = m_ParseManager.LastAIGlobalSearch().Lower();
                    int iidx = ilist->GetImageCount();
                    FileType fTp = FileTypeOf(ed->GetShortName());
                    bool isC = (fTp == ftHeader || fTp == ftSource|| fTp == ftTemplateSource);
                    // theme keywords
                    HighlightLanguage lang = ed->GetLanguage();
                    if (lang == HL_NONE)
                        lang = colour_set->GetLanguageForFilename(ed->GetFilename());
                    wxString strLang = colour_set->GetLanguageName(lang);
                    // if its sourcecode/header file and a known fileformat, show the corresponding icon
                    if (isC && strLang == wxT("C/C++"))
                        stc->RegisterImage(iidx, GetImage(ImageId::KeywordCPP, fontSize));
                    else if (isC && strLang == wxT("D"))
                        stc->RegisterImage(iidx, GetImage(ImageId::KeywordD, fontSize));
                    else
                        stc->RegisterImage(iidx, GetImage(ImageId::Unknown, fontSize));
                    // the first two keyword sets are the primary and secondary keywords (for most lexers at least)
                    // but this is now configurable in global settings
                    for (int i = 0; i <= wxSCI_KEYWORDSET_MAX; ++i)
                    {
                        if (!m_LexerKeywordsToInclude[i])
                            continue;

                        wxString keywords = colour_set->GetKeywords(lang, i);
                        wxStringTokenizer tkz(keywords, wxT(" \t\r\n"), wxTOKEN_STRTOK);
                        while (tkz.HasMoreTokens())
                        {
                            wxString kw = tkz.GetNextToken();
                            if (   kw.Lower().StartsWith(lastSearch)
                                && uniqueStrings.find(kw) == uniqueStrings.end() )
                            {
                                tokens.push_back(CCToken(wxNOT_FOUND, kw, iidx));
                            }
                        }
                    }
                }
            }
        }
        else if (!stc->CallTipActive())
        {
            wxString msg = _("Too many results.\n"
                             "Please edit results' limit in code-completion options,\n"
                             "or type at least one more character to narrow the scope down.");
            stc->CallTipShow(stc->GetCurrentPos(), msg);
        }
    }
    else if (!stc->CallTipActive())
    {
        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxT("0 results"));

        if (!m_ParseManager.GetParser().Done())
        {
            wxString msg = _("The Parser is still parsing files.");
            stc->CallTipShow(stc->GetCurrentPos(), msg);
            msg += m_ParseManager.GetParser().NotDoneReason();
            CCLogger::Get()->DebugLog(msg);
        }
    }
}

void CodeCompletion::DoCodeCompletePreprocessor(int tknStart, int tknEnd, cbEditor* ed, std::vector<CCToken>& tokens)
{
    cbStyledTextCtrl* stc = ed->GetControl();
    if (stc->GetLexer() != wxSCI_LEX_CPP)
    {
        const FileType fTp = FileTypeOf(ed->GetShortName());
        if (   fTp != ftSource
            && fTp != ftHeader
            && fTp != ftTemplateSource
            && fTp != ftResource )
        {
            return; // not C/C++
        }
    }
    const wxString text = stc->GetTextRange(tknStart, tknEnd);

    wxStringVec macros;
    macros.push_back(wxT("define"));
    macros.push_back(wxT("elif"));
    macros.push_back(wxT("elifdef"));
    macros.push_back(wxT("elifndef"));
    macros.push_back(wxT("else"));
    macros.push_back(wxT("endif"));
    macros.push_back(wxT("error"));
    macros.push_back(wxT("if"));
    macros.push_back(wxT("ifdef"));
    macros.push_back(wxT("ifndef"));
    macros.push_back(wxT("include"));
    macros.push_back(wxT("line"));
    macros.push_back(wxT("pragma"));
    macros.push_back(wxT("undef"));
    // const wxString idxStr = wxString::Format("\n%d", PARSER_IMG_MACRO_DEF);
    for (size_t i = 0; i < macros.size(); ++i)
    {
        if (text.IsEmpty() || macros[i][0] == text[0]) // ignore tokens that start with a different letter
            tokens.push_back(CCToken(wxNOT_FOUND, macros[i], PARSER_IMG_MACRO_DEF));
    }
    stc->ClearRegisteredImages();
    const int fontSize = CalcStcFontSize(stc);
    stc->RegisterImage(PARSER_IMG_MACRO_DEF,
                       m_ParseManager.GetImageList(fontSize)->GetBitmap(PARSER_IMG_MACRO_DEF));
}

void CodeCompletion::DoCodeCompleteIncludes(cbEditor* ed, int& tknStart, int tknEnd, std::vector<CCToken>& tokens)
{
    if (!m_CCEnableHeaders)
        return;

    const wxString curFile(ed->GetFilename());
    const wxString curPath(wxFileName(curFile).GetPath());

    FileType ft = FileTypeOf(ed->GetShortName());
    if ( ft != ftHeader && ft != ftSource && ft != ftTemplateSource) // only parse source/header files
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    const int lineStartPos = stc->PositionFromLine(stc->GetCurrentLine());
    wxString line = stc->GetCurLine();
    line.Trim();
    if (line.IsEmpty() || !CodeCompletionHelper::TestIncludeLine(line))
        return;

    int keyPos = line.Find(wxT('"'));
    if (keyPos == wxNOT_FOUND || keyPos >= tknEnd - lineStartPos)
        keyPos = line.Find(wxT('<'));
    if (keyPos == wxNOT_FOUND || keyPos >= tknEnd - lineStartPos)
        return;
    ++keyPos;

    // now, we are after the quote prompt, "filename" is the text we already typed after the
    // #include directive, such as #include <abc|  , so that we need to prompt all the header files
    // which has the prefix "abc"
    wxString filename = line.SubString(keyPos, tknEnd - lineStartPos - 1);
    filename.Replace(wxT("\\"), wxT("/"), true);
    if (!filename.empty() && (filename.Last() == wxT('"') || filename.Last() == wxT('>')))
        filename.RemoveLast();

    size_t maxFiles = m_CCMaxMatches;
    if (filename.IsEmpty() && maxFiles > 3000)
        maxFiles = 3000; // do not try to collect too many files if there is no context (prevent hang)

    // fill a list of matching files
    StringSet files;

    // #include < or #include "
    cbProject* project = m_ParseManager.GetProjectByEditor(ed);

    // since we are going to access the m_SystemHeadersMap, we add a locker here
    // here we collect all the header files names which is under "system include search dirs"
    if (m_SystemHeadersThreadCS.TryEnter())
    {
        // if the project get modified, fetch the dirs again, otherwise, use cached dirs
        wxArrayString& incDirs = GetSystemIncludeDirs(project, project ? project->GetModified() : true);
        for (size_t i = 0; i < incDirs.GetCount(); ++i)
        {
            // shm_it means system_header_map_iterator
            SystemHeadersMap::const_iterator shm_it = m_SystemHeadersMap.find(incDirs[i]);
            if (shm_it != m_SystemHeadersMap.end())
            {
                const StringSet& headers = shm_it->second;
                for (StringSet::const_iterator ss_it = headers.begin(); ss_it != headers.end(); ++ss_it)
                {
                    const wxString& file = *ss_it;
                    // if find a value matches already typed "filename", add to the result
                    if (file.StartsWith(filename))
                    {
                        files.insert(file);
                        if (files.size() > maxFiles)
                            break; // exit inner loop
                    }
                }
                if (files.size() > maxFiles)
                    break; // exit outer loop
            }
        }
        m_SystemHeadersThreadCS.Leave();
    }

    // #include "
    if (project)
    {
        if (m_SystemHeadersThreadCS.TryEnter())
        {
            wxArrayString buildTargets;
            ProjectFile* pf = project ? project->GetFileByFilename(curFile, false) : 0;
            if (pf)
                buildTargets = pf->buildTargets;

            const wxArrayString localIncludeDirs = GetLocalIncludeDirs(project, buildTargets);
            for (FilesList::const_iterator it = project->GetFilesList().begin();
                                           it != project->GetFilesList().end(); ++it)
            {
                pf = *it;
                if (pf && FileTypeOf(pf->relativeFilename) == ftHeader)
                {
                    wxString file = pf->file.GetFullPath();
                    wxString header;
                    for (size_t j = 0; j < localIncludeDirs.GetCount(); ++j)
                    {
                        const wxString& dir = localIncludeDirs[j];
                        if (file.StartsWith(dir))
                        {
                            header = file.Mid(dir.Len());
                            header.Replace(wxT("\\"), wxT("/"));
                            break;
                        }
                    }

                    if (header.IsEmpty())
                    {
                        if (pf->buildTargets != buildTargets)
                            continue;

                        wxFileName fn(file);
                        fn.MakeRelativeTo(curPath);
                        header = fn.GetFullPath(wxPATH_UNIX);
                    }

                    if (header.StartsWith(filename))
                    {
                        files.insert(header);
                        if (files.size() > maxFiles)
                            break;
                    }
                }
            }
            m_SystemHeadersThreadCS.Leave();
        }
    }

    if (!files.empty())
    {
        tknStart = lineStartPos + keyPos;
        tokens.reserve(files.size());
        for (StringSet::const_iterator ssIt = files.begin(); ssIt != files.end(); ++ssIt)
            tokens.push_back(CCToken(wxNOT_FOUND, *ssIt, 0));
        stc->ClearRegisteredImages();
        const int fontSize = CalcStcFontSize(stc);
        stc->RegisterImage(0, GetImage(ImageId::HeaderFile, fontSize));
    }
}

std::vector<CodeCompletion::CCCallTip> CodeCompletion::GetCallTips(int pos, int style, cbEditor* ed, int& argsPos)
{
    std::vector<CCCallTip> tips;
    if (!IsAttached() || !m_InitDone || style == wxSCI_C_WXSMITH || !m_ParseManager.GetParser().Done())
        return tips;

    int typedCommas = 0;
    wxArrayString items;
    argsPos = m_ParseManager.GetCallTips(items, typedCommas, ed, pos);
    StringSet uniqueTips; // check against this before inserting a new tip in the list
    for (size_t i = 0; i < items.GetCount(); ++i)
    {
        // allow only unique, non-empty items with equal or more commas than what the user has already typed
        if (   uniqueTips.find(items[i]) == uniqueTips.end() // unique
            && !items[i].IsEmpty() // non-empty
            && typedCommas <= m_ParseManager.CountCommas(items[i], 0) ) // commas satisfied
        {
            uniqueTips.insert(items[i]);
            int hlStart = wxSCI_INVALID_POSITION;
            int hlEnd   = wxSCI_INVALID_POSITION;
            m_ParseManager.GetCallTipHighlight(items[i], &hlStart, &hlEnd, typedCommas);
            tips.push_back(CCCallTip(items[i], hlStart, hlEnd));
        }
    }
    return tips;
}

wxString CodeCompletion::GetDocumentation(const CCToken& token)
{
    return m_DocHelper.GenerateHTML(token.id, m_ParseManager.GetParser().GetTokenTree());
}

std::vector<CodeCompletion::CCToken> CodeCompletion::GetTokenAt(int pos, cbEditor* ed, bool& WXUNUSED(allowCallTip))
{
    std::vector<CCToken> tokens;
    if (!IsAttached() || !m_InitDone)
        return tokens;

    // ignore comments, strings, preprocessors, etc
    cbStyledTextCtrl* stc = ed->GetControl();
    const int style = stc->GetStyleAt(pos);
    if (   stc->IsString(style)
        || stc->IsComment(style)
        || stc->IsCharacter(style)
        || stc->IsPreprocessor(style) )
    {
        return tokens;
    }

    TokenIdxSet result;
    int endOfWord = stc->WordEndPosition(pos, true);
    if (m_ParseManager.MarkItemsByAI(result, true, false, true, endOfWord))
    {
        TokenTree* tree = m_ParseManager.GetParser().GetTokenTree();

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
        {
            const Token* token = tree->at(*it);
            if (token)
            {
                tokens.push_back(CCToken(*it, token->DisplayName()));
                if (tokens.size() > 32)
                    break;
            }
        }

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
    }

    return tokens;
}

wxString CodeCompletion::OnDocumentationLink(wxHtmlLinkEvent& event, bool& dismissPopup)
{
    return m_DocHelper.OnDocumentationLink(event, dismissPopup);
}

void CodeCompletion::DoAutocomplete(const CCToken& token, cbEditor* ed)
{
    wxString itemText = CodeCompletionHelper::AutocompGetName(token.displayName);
    cbStyledTextCtrl* stc = ed->GetControl();

    int curPos = stc->GetCurrentPos();
    int startPos = stc->WordStartPosition(curPos, true);
    if (   itemText.GetChar(0) == _T('~') // special handle for dtor
        && startPos > 0
        && stc->GetCharAt(startPos - 1) == _T('~'))
    {
        --startPos;
    }
    bool needReparse = false;

    if (stc->IsPreprocessor(stc->GetStyleAt(curPos)))
    {
        curPos = stc->GetLineEndPosition(stc->GetCurrentLine()); // delete rest of line
        bool addComment = (itemText == wxT("endif"));
        for (int i = stc->GetCurrentPos(); i < curPos; ++i)
        {
            if (stc->IsComment(stc->GetStyleAt(i)))
            {
                curPos = i; // preserve line comment
                if (wxIsspace(stc->GetCharAt(i - 1)))
                    --curPos; // preserve a space before the comment
                addComment = false;
                break;
            }
        }
        if (addComment) // search backwards for the #if*
        {
            wxRegEx ppIf("^[[:blank:]]*#[[:blank:]]*if");
            wxRegEx ppEnd("^[[:blank:]]*#[[:blank:]]*endif");
            int depth = -1;
            for (int ppLine = stc->GetCurrentLine() - 1; ppLine >= 0; --ppLine)
            {
                if (stc->GetLine(ppLine).Find(wxT('#')) != wxNOT_FOUND) // limit testing due to performance cost
                {
                    if (ppIf.Matches(stc->GetLine(ppLine))) // ignore else's, elif's, ...
                        ++depth;
                    else if (ppEnd.Matches(stc->GetLine(ppLine)))
                        --depth;
                }
                if (depth == 0)
                {
                    wxRegEx pp("^[[:blank:]]*#[[:blank:]]*[a-z]*([[:blank:]]+([a-zA-Z0-9_]+)|())");
                    pp.Matches(stc->GetLine(ppLine));
                    if (!pp.GetMatch(stc->GetLine(ppLine), 2).IsEmpty())
                        itemText.Append(wxT(" // ") + pp.GetMatch(stc->GetLine(ppLine), 2));
                    break;
                }
            }
        }
        needReparse = true;

        int   pos = startPos - 1;
        wxChar ch = stc->GetCharAt(pos);
        while (ch != _T('<') && ch != _T('"') && ch != _T('#') && (pos>0))
            ch = stc->GetCharAt(--pos);
        if (ch == _T('<') || ch == _T('"'))
            startPos = pos + 1;

        if (ch == _T('"'))
            itemText << _T('"');
        else if (ch == _T('<'))
            itemText << _T('>');
    }
    else
    {
        const int endPos = stc->WordEndPosition(curPos, true);
        const wxString& alreadyText = stc->GetTextRange(curPos, endPos);
        if (!alreadyText.IsEmpty() && itemText.EndsWith(alreadyText))
            curPos = endPos;
    }

    int positionModificator = 0;
    bool insideParentheses = false;
    if (token.id != -1 && m_CCAutoAddParentheses)
    {
        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        TokenTree* tree = m_ParseManager.GetParser().GetTokenTree();
        const Token* tkn = tree->at(token.id);

        if (!tkn)
        {   CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex) }
        else
        {
            bool addParentheses = tkn->m_TokenKind & tkAnyFunction;
            if (!addParentheses && (tkn->m_TokenKind & tkMacroDef))
            {
                if (tkn->m_Args.size() > 0)
                    addParentheses = true;
            }
            // cache args to avoid locking
            wxString tokenArgs = tkn->GetStrippedArgs();

            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

            if (addParentheses)
            {
                bool insideFunction = true;
                if (m_CCDetectImplementation)
                {
                    ccSearchData searchData = { stc, ed->GetFilename() };
                    int funcToken;
                    if (m_ParseManager.FindCurrentFunctionStart(&searchData, 0, 0, &funcToken) == -1)
                    {
                        // global scope
                        itemText += tokenArgs;
                        insideFunction = false;
                    }
                    else // Found something, but result may be false positive.
                    {
                        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

                        const Token* parent = tree->at(funcToken);
                        // Make sure that parent is not container (class, etc)
                        if (parent && (parent->m_TokenKind & tkAnyFunction) == 0)
                        {
                            // class scope
                            itemText += tokenArgs;
                            insideFunction = false;
                        }

                        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
                    }
                }

                if (insideFunction)
                {
                    // Inside block
                    // Check if there are brace behind the target
                    if (stc->GetCharAt(curPos) != _T('('))
                    {
                        itemText += _T("()");
                        if (tokenArgs.size() > 2) // more than '()'
                        {
                            positionModificator = -1;
                            insideParentheses = true;
                        }
                    }
                    else
                        positionModificator = 1; // Set caret after '('
                }
            }
        } // if tkn
    } // if token.id

    stc->SetTargetStart(startPos);
    stc->SetTargetEnd(curPos);

    stc->AutoCompCancel();
    if (stc->GetTextRange(startPos, curPos) != itemText)
        stc->ReplaceTarget(itemText);
    stc->GotoPos(startPos + itemText.Length() + positionModificator);

    if (insideParentheses)
    {
        stc->EnableTabSmartJump();
        int tooltipMode = Manager::Get()->GetConfigManager(wxT("ccmanager"))->ReadInt(wxT("/tooltip_mode"), 1);
        if (tooltipMode != 3) // keybound only
        {
            CodeBlocksEvent evt(cbEVT_SHOW_CALL_TIP);
            Manager::Get()->ProcessEvent(evt);
        }
    }

    if (needReparse)
    {
        TRACE(_T("CodeCompletion::EditorEventHook: Starting m_TimerRealtimeParsing."));
        m_TimerRealtimeParsing.Start(1, wxTIMER_ONE_SHOT);
    }
    stc->ChooseCaretX();
}

wxArrayString CodeCompletion::GetLocalIncludeDirs(cbProject* project, const wxArrayString& buildTargets)
{
    wxArrayString dirs;
    // Do not try to operate include directories if the project is not for this platform
    if (m_CCEnablePlatformCheck && !project->SupportsCurrentPlatform())
        return dirs;
    // add project level compiler include search paths
    const wxString prjPath = project->GetCommonTopLevelPath();
    GetAbsolutePath(prjPath, project->GetIncludeDirs(), dirs);
    // add target level compiler include search paths
    for (size_t i = 0; i < buildTargets.GetCount(); ++i)
    {
        ProjectBuildTarget* tgt = project->GetBuildTarget(buildTargets[i]);
        if (!tgt)
            continue;
        // Do not try to operate include directories if the target is not for this platform
        if (!m_CCEnablePlatformCheck || tgt->SupportsCurrentPlatform())
        {
            GetAbsolutePath(prjPath, tgt->GetIncludeDirs(), dirs);
        }
    }

    // if a path has prefix with the project's path, it is a local include search dir
    // other wise, it is a system level include search dir, we try to collect all the system dirs
    wxArrayString sysDirs;
    for (size_t i = 0; i < dirs.GetCount();)
    {
        // if the dir with the prefix of project path, then it is a "local dir"
        if (dirs[i].StartsWith(prjPath))
            ++i;
        else // otherwise, it is a "system dir", so add to "sysDirs"
        {
            if (m_SystemHeadersMap.find(dirs[i]) == m_SystemHeadersMap.end())
                sysDirs.Add(dirs[i]);
            // remove the system dir in dirs
            dirs.RemoveAt(i);
        }
    }

    if (!sysDirs.IsEmpty())
    {
        cbAssert(m_CCEnableHeaders);
        // Create a worker thread associated with "sysDirs". Put it in a queue and run it.
        SystemHeadersThread* thread = new SystemHeadersThread(this, &m_SystemHeadersThreadCS,
                                                              m_SystemHeadersMap, sysDirs);
        m_SystemHeadersThreads.push_back(thread);
        thread->Run();
    }

    dirs.Sort(CodeCompletionHelper::CompareStringLen);
    return dirs; // return all the local dirs
}

wxArrayString& CodeCompletion::GetSystemIncludeDirs(cbProject* project, bool force)
{
    static cbProject*    lastProject = nullptr;
    static wxArrayString incDirs;

    if (!force && project == lastProject) // force == false means we can use the cached dirs
        return incDirs;
    else
    {
        incDirs.Clear();
        lastProject = project;
    }

    wxString prjPath;
    if (project)
        prjPath = project->GetCommonTopLevelPath();

    ParserBase* parser = m_ParseManager.GetParserByProject(project);
    if (!parser)
        return incDirs;

    incDirs = parser->GetIncludeDirs();
    // we try to remove the dirs which belong to the project
    for (size_t i = 0; i < incDirs.GetCount();)
    {
        if (incDirs[i].Last() != wxFILE_SEP_PATH)
            incDirs[i].Append(wxFILE_SEP_PATH);
        // the dirs which have prjPath prefix are local dirs, so they should be removed
        if (project && incDirs[i].StartsWith(prjPath))
            incDirs.RemoveAt(i);
        else
            ++i;
    }

    return incDirs;
}

void CodeCompletion::GetAbsolutePath(const wxString& basePath, const wxArrayString& targets, wxArrayString& dirs)
{
    for (size_t i = 0; i < targets.GetCount(); ++i)
    {
        wxString includePath = targets[i];
        Manager::Get()->GetMacrosManager()->ReplaceMacros(includePath);
        wxFileName fn(includePath, wxEmptyString);
        if (fn.IsRelative())
        {
            const wxArrayString oldDirs = fn.GetDirs();
            fn.SetPath(basePath);
            for (size_t j = 0; j < oldDirs.GetCount(); ++j)
                fn.AppendDir(oldDirs[j]);
        }

        // Detect if this directory is for the file system root and skip it. Sometimes macro
        // replacements create such paths and we don't want to scan whole disks because of this.
        if (fn.IsAbsolute() && fn.GetDirCount() == 0)
            continue;

        const wxString path = fn.GetFullPath();
        if (dirs.Index(path) == wxNOT_FOUND)
            dirs.Add(path);
    }
}

void CodeCompletion::EditorEventHook(cbEditor* editor, wxScintillaEvent& event)
{
    if (!IsAttached() || !m_InitDone)
    {
        event.Skip();
        return;
    }

    if ( !IsProviderFor(editor) )
    {
        event.Skip();
        return;
    }

    cbStyledTextCtrl* control = editor->GetControl();

    if      (event.GetEventType() == wxEVT_SCI_CHARADDED)
    {   TRACE(_T("wxEVT_SCI_CHARADDED")); }
    else if (event.GetEventType() == wxEVT_SCI_CHANGE)
    {   TRACE(_T("wxEVT_SCI_CHANGE")); }
    else if (event.GetEventType() == wxEVT_SCI_MODIFIED)
    {   TRACE(_T("wxEVT_SCI_MODIFIED")); }
    else if (event.GetEventType() == wxEVT_SCI_AUTOCOMP_SELECTION)
    {   TRACE(_T("wxEVT_SCI_AUTOCOMP_SELECTION")); }
    else if (event.GetEventType() == wxEVT_SCI_AUTOCOMP_CANCELLED)
    {   TRACE(_T("wxEVT_SCI_AUTOCOMP_CANCELLED")); }

    // if the user is modifying the editor, then CC should try to reparse the editor's content
    // and update the token tree.
    if (   m_ParseManager.GetParser().Options().whileTyping
        && (   (event.GetModificationType() & wxSCI_MOD_INSERTTEXT)
            || (event.GetModificationType() & wxSCI_MOD_DELETETEXT) ) )
    {
        m_NeedReparse = true;
    }

    if (control->GetCurrentLine() != m_CurrentLine)
    {
        // reparsing the editor only happens in the condition that the caret's line number
        // is changed.
        if (m_NeedReparse)
        {
            TRACE(_T("CodeCompletion::EditorEventHook: Starting m_TimerRealtimeParsing."));
            m_TimerRealtimeParsing.Start(REALTIME_PARSING_DELAY, wxTIMER_ONE_SHOT);
            m_CurrentLength = control->GetLength();
            m_NeedReparse = false;
        }
        // wxEVT_SCI_UPDATEUI will be sent on caret's motion, but we are only interested in the
        // cases where line number is changed. Then we need to update the CC's toolbar.
        if (event.GetEventType() == wxEVT_SCI_UPDATEUI)
        {
            m_ToolbarNeedRefresh = true;
            TRACE(_T("CodeCompletion::EditorEventHook: Starting m_TimerToolbar."));
            if (m_TimerEditorActivated.IsRunning())
                m_TimerToolbar.Start(EDITOR_ACTIVATED_DELAY + 1, wxTIMER_ONE_SHOT);
            else
                m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
        }
    }

    // allow others to handle this event
    event.Skip();
}

void CodeCompletion::RereadOptions()
{
    // Keep this in sync with CCOptionsDlg::CCOptionsDlg and CCOptionsDlg::OnApply

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("code_completion"));

    m_LexerKeywordsToInclude[0] = cfg->ReadBool(_T("/lexer_keywords_set1"), true);
    m_LexerKeywordsToInclude[1] = cfg->ReadBool(_T("/lexer_keywords_set2"), true);
    m_LexerKeywordsToInclude[2] = cfg->ReadBool(_T("/lexer_keywords_set3"), false);
    m_LexerKeywordsToInclude[3] = cfg->ReadBool(_T("/lexer_keywords_set4"), false);
    m_LexerKeywordsToInclude[4] = cfg->ReadBool(_T("/lexer_keywords_set5"), false);
    m_LexerKeywordsToInclude[5] = cfg->ReadBool(_T("/lexer_keywords_set6"), false);
    m_LexerKeywordsToInclude[6] = cfg->ReadBool(_T("/lexer_keywords_set7"), false);
    m_LexerKeywordsToInclude[7] = cfg->ReadBool(_T("/lexer_keywords_set8"), false);
    m_LexerKeywordsToInclude[8] = cfg->ReadBool(_T("/lexer_keywords_set9"), false);

    // for CC
    m_CCMaxMatches           = cfg->ReadInt(_T("/max_matches"),            16384);
    m_CCAutoAddParentheses   = cfg->ReadBool(_T("/auto_add_parentheses"),  true);
    m_CCDetectImplementation = cfg->ReadBool(_T("/detect_implementation"), false); //depends on auto_add_parentheses
    m_CCFillupChars          = cfg->Read(_T("/fillup_chars"),              wxEmptyString);
    m_CCEnableHeaders        = cfg->ReadBool(_T("/enable_headers"),        true);
    m_CCEnablePlatformCheck  = cfg->ReadBool(_T("/platform_check"),        true);

    // update the CC toolbar option, and tick the timer for toolbar
    // NOTE (ollydbg#1#12/06/14): why?
    if (m_ToolBar)
    {
        UpdateToolBar();
        CodeBlocksLayoutEvent evt(cbEVT_UPDATE_VIEW_LAYOUT);
        Manager::Get()->ProcessEvent(evt);
        m_ToolbarNeedReparse = true;
        TRACE(_T("CodeCompletion::RereadOptions: Starting m_TimerToolbar."));
        m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
    }

    m_DocHelper.RereadOptions(cfg);
}

void CodeCompletion::UpdateToolBar()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("code_completion"));
    const bool showScope = cfg->ReadBool(_T("/scope_filter"), true);
    const int scopeLength = cfg->ReadInt(_T("/toolbar_scope_length"), 280);
    const int functionLength = cfg->ReadInt(_T("/toolbar_function_length"), 660);

    if (showScope && !m_Scope)
    {
        // Show the scope choice
        m_Scope = new wxChoice(m_ToolBar, XRCID("chcCodeCompletionScope"), wxPoint(0, 0), wxSize(scopeLength, -1), 0, 0);
        m_ToolBar->InsertControl(0, m_Scope);
    }
    else if (!showScope && m_Scope)
    {
        // Hide the scope choice
        m_ToolBar->DeleteTool(m_Scope->GetId());
        m_Scope = nullptr;
    }
    else if (m_Scope)
    {
        // Just apply new size to scope choice
        m_Scope->SetSize(wxSize(scopeLength, -1));
    }

    m_Function->SetSize(wxSize(functionLength, -1));

    m_ToolBar->Realize();
    m_ToolBar->SetInitialSize();
}

void CodeCompletion::OnUpdateUI(wxUpdateUIEvent& event)
{
    wxString NameUnderCursor;
    bool IsInclude = false;
    const bool HasNameUnderCursor = CodeCompletionHelper::EditorHasNameUnderCursor(NameUnderCursor, IsInclude);

    const bool HasEd = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor() != 0;
    if (m_EditMenu)
    {
        const bool RenameEnable = HasNameUnderCursor && !IsInclude && m_ParseManager.GetParser().Done();
        m_EditMenu->Enable(idMenuRenameSymbols, RenameEnable);
    }

    if (m_SearchMenu)
    {
        m_SearchMenu->Enable(idMenuGotoFunction,       HasEd);
        m_SearchMenu->Enable(idMenuGotoPrevFunction,   HasEd);
        m_SearchMenu->Enable(idMenuGotoNextFunction,   HasEd);

        const bool GotoEnable = HasNameUnderCursor && !IsInclude;
        m_SearchMenu->Enable(idMenuGotoDeclaration,    GotoEnable);
        m_SearchMenu->Enable(idMenuGotoImplementation, GotoEnable);
        const bool FindEnable = HasNameUnderCursor && !IsInclude && m_ParseManager.GetParser().Done();
        m_SearchMenu->Enable(idMenuFindReferences, FindEnable);
        const bool IncludeEnable = HasNameUnderCursor && IsInclude;
        m_SearchMenu->Enable(idMenuOpenIncludeFile, IncludeEnable);
    }

    if (m_ViewMenu)
    {
        bool isVis = IsWindowReallyShown((wxWindow*)m_ParseManager.GetClassBrowser());
        m_ViewMenu->Check(idViewClassBrowser, isVis);
    }

    if (m_ProjectMenu)
    {
        cbProject* project = m_ParseManager.GetCurrentProject();
        m_ProjectMenu->Enable(idCurrentProjectReparse, project);
    }

    // must do...
    event.Skip();
}

void CodeCompletion::OnViewClassBrowser(wxCommandEvent& event)
{
    if (!Manager::Get()->GetConfigManager(_T("code_completion"))->ReadBool(_T("/use_symbols_browser"), true))
    {
        cbMessageBox(_("The symbols browser is disabled in code-completion options.\n"
                        "Please enable it there first..."), _("Information"), wxICON_INFORMATION);
        return;
    }
    CodeBlocksDockEvent evt(event.IsChecked() ? cbEVT_SHOW_DOCK_WINDOW : cbEVT_HIDE_DOCK_WINDOW);
    evt.pWindow = (wxWindow*)m_ParseManager.GetClassBrowser();
    Manager::Get()->ProcessEvent(evt);
}

void CodeCompletion::OnGotoFunction(cb_unused wxCommandEvent& event)
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return;

    TRACE(_T("OnGotoFunction"));

    m_ParseManager.GetParser().ParseBufferForFunctions(ed->GetControl()->GetText());


    TokenTree* tree = m_ParseManager.GetParser().GetTempTokenTree();

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    if (tree->empty())
    {
        cbMessageBox(_("No functions parsed in this file..."));
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
    }
    else
    {
        GotoFunctionDlg::Iterator iterator;

        for (size_t i = 0; i < tree->size(); i++)
        {
            Token* token = tree->at(i);
            if (token && token->m_TokenKind & tkAnyFunction)
            {
                GotoFunctionDlg::FunctionToken ft;
                // We need to clone the internal data of the strings to make them thread safe.
                ft.displayName = wxString(token->DisplayName().c_str());
                ft.name = wxString(token->m_Name.c_str());
                ft.line = token->m_Line;
                ft.implLine = token->m_ImplLine;
                if (!token->m_FullType.empty())
                    ft.paramsAndreturnType = wxString((token->m_Args + wxT(" -> ") + token->m_FullType).c_str());
                else
                    ft.paramsAndreturnType = wxString(token->m_Args.c_str());
                ft.funcName = wxString((token->GetNamespace() + token->m_Name).c_str());

                iterator.AddToken(ft);
            }
        }

        tree->clear();

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        iterator.Sort();
        GotoFunctionDlg dlg(Manager::Get()->GetAppWindow(), &iterator);
        PlaceWindow(&dlg);
        if (dlg.ShowModal() == wxID_OK)
        {
            int selection = dlg.GetSelection();
            if (selection != wxNOT_FOUND) {
                const GotoFunctionDlg::FunctionToken *ft = iterator.GetToken(selection);
                if (ed && ft)
                {
                    TRACE(F(_T("OnGotoFunction() : Token '%s' found at line %u."), ft->name.wx_str(), ft->line));
                    ed->GotoTokenPosition(ft->implLine - 1, ft->name);
                }
            }
        }
    }
}

void CodeCompletion::OnGotoPrevFunction(cb_unused wxCommandEvent& event)
{
    GotoFunctionPrevNext(); // prev function
}

void CodeCompletion::OnGotoNextFunction(cb_unused wxCommandEvent& event)
{
    GotoFunctionPrevNext(true); // next function
}

void CodeCompletion::OnClassMethod(cb_unused wxCommandEvent& event)
{
    DoClassMethodDeclImpl();
}

void CodeCompletion::OnUnimplementedClassMethods(cb_unused wxCommandEvent& event)
{
    DoAllMethodsImpl();
}

void CodeCompletion::OnGotoDeclaration(wxCommandEvent& event)
{
    EditorManager* edMan  = Manager::Get()->GetEditorManager();
    cbEditor*      editor = edMan->GetBuiltinActiveEditor();
    if (!editor)
        return;

    TRACE(_T("OnGotoDeclaration"));

    const int pos      = editor->GetControl()->GetCurrentPos();
    const int startPos = editor->GetControl()->WordStartPosition(pos, true);
    const int endPos   = editor->GetControl()->WordEndPosition(pos, true);
    wxString target;
    // if there is a tilde "~", the token can either be a destructor or an Bitwise NOT (One's
    // Complement) operator
    bool isDestructor = false;
    if (CodeCompletionHelper::GetLastNonWhitespaceChar(editor->GetControl(), startPos) == _T('~'))
        isDestructor = true;
    target << editor->GetControl()->GetTextRange(startPos, endPos);
    if (target.IsEmpty())
        return;

    // prepare a boolean filter for declaration/implementation
    bool isDecl = event.GetId() == idGotoDeclaration    || event.GetId() == idMenuGotoDeclaration;
    bool isImpl = event.GetId() == idGotoImplementation || event.GetId() == idMenuGotoImplementation;

    // get the matching set
    TokenIdxSet result;
    m_ParseManager.MarkItemsByAI(result, true, false, true, endPos);

    TokenTree* tree = m_ParseManager.GetParser().GetTokenTree();

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    // handle destructor function, first we try to see if it is a destructor, we simply do a semantic
    // check of the token under cursor, otherwise, it is a variable.
    if (isDestructor)
    {
        TokenIdxSet tmp = result;
        result.clear();

        for (TokenIdxSet::const_iterator it = tmp.begin(); it != tmp.end(); ++it)
        {
            const Token* token = tree->at(*it);
            if (token && token->m_TokenKind == tkClass)
            {
                token = tree->at(tree->TokenExists(_T("~") + target, token->m_Index, tkDestructor));
                if (token)
                    result.insert(token->m_Index);
            }
        }

        // no destructor found, this could be a variable.
        if (result.empty())
            result = tmp;
    }
    else // handle constructor and functions
    {
        // AAA * p = new AAA();
        // ^^^--------------------------case1:go to class definition kind tokens
        //               ^^^------------case2:go to function kind tokens such as constructors
        // if a token is followed by a '(', it is regarded as a function
        const bool isFunction = CodeCompletionHelper::GetNextNonWhitespaceChar(editor->GetControl(), endPos)   == _T('(');
        // copy the token index set for a fall back case later
        TokenIdxSet savedResult = result;
        // loop the result, and strip unrelated tokens
        for (TokenIdxSet::const_iterator it = result.begin(); it != result.end();)
        {
            const Token* token = tree->at(*it);
            if (isFunction && token && token->m_TokenKind == tkClass)
                result.erase(it++); // a class kind token is removed since we need a function
            else if (!isFunction && token && token->m_TokenKind == tkConstructor)
                result.erase(it++); // a constructor token is removed since we don't need a function
            else
                ++it;
        }
        // fall back: restore the saved result in a special case that a class doesn't have a constructor
        // defined (implicitly defined by compiler)
        // E.g. hover on case2 go to class AAA token since no AAA::AAA(); is defined or declared.
        if (!result.size())
            result = savedResult;
    }

    // handle function overloading
    if (result.size() > 1)
    {
        const size_t curLine = editor->GetControl()->GetCurrentLine() + 1;
        for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
        {
            const Token* token = tree->at(*it);
            if (token && (token->m_Line == curLine || token->m_ImplLine == curLine) )
            {
                const int theOnlyOne = *it;
                result.clear();
                result.insert(theOnlyOne);
                break;
            }
        }
    }

    // data for the choice dialog
    std::deque<CodeCompletionHelper::GotoDeclarationItem> foundItems;
    wxArrayString selections;

    wxString editorFilename;
    unsigned editorLine = -1;
    bool     tokenFound = false;

    // one match
    if (result.size() == 1)
    {
        Token* token = NULL;
        Token* sel = tree->at(*(result.begin()));
        if (   (isImpl && !sel->GetImplFilename().IsEmpty())
            || (isDecl && !sel->GetFilename().IsEmpty()) )
        {
            token = sel;
        }
        if (token)
        {
            // FIXME: implement this correctly, because now it is not showing full results
            if (   wxGetKeyState(WXK_CONTROL)
                && wxGetKeyState(WXK_SHIFT)
                && (  event.GetId() == idGotoDeclaration
                   || event.GetId() == idGotoImplementation ) )
            {
                // FIXME: this  code can lead to a deadlock (because of double locking from single thread)
                CCDebugInfo info(nullptr, &m_ParseManager.GetParser(), token);
                PlaceWindow(&info);
                info.ShowModal();
            }
            else if (isImpl)
            {
                editorFilename = token->GetImplFilename();
                editorLine     = token->m_ImplLine - 1;
            }
            else if (isDecl)
            {
                editorFilename = token->GetFilename();
                editorLine     = token->m_Line - 1;
            }

            tokenFound = true;
        }
    }
    // if more than one match, display a selection dialog
    else if (result.size() > 1)
    {
        // TODO: we could parse the line containing the text so
        // if namespaces were included, we could limit the results (and be more accurate)
        for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
        {
            const Token* token = tree->at(*it);
            if (token)
            {
                CodeCompletionHelper::GotoDeclarationItem item;

                if (isImpl)
                {
                    item.filename = token->GetImplFilename();
                    item.line     = token->m_ImplLine - 1;
                }
                else if (isDecl)
                {
                    item.filename = token->GetFilename();
                    item.line     = token->m_Line - 1;
                }

                // only match tokens that have filename info
                if (!item.filename.empty())
                {
                    selections.Add(token->DisplayName());
                    foundItems.push_back(item);
                }
            }
        }
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    if (selections.GetCount() > 1)
    {
        int sel = cbGetSingleChoiceIndex(_("Please make a selection:"), _("Multiple matches"), selections,
                                         Manager::Get()->GetAppWindow(), wxSize(400, 400));
        if (sel == -1)
            return;

        const CodeCompletionHelper::GotoDeclarationItem &item = foundItems[sel];
        editorFilename = item.filename;
        editorLine     = item.line;
        tokenFound     = true;
    }
    else if (selections.GetCount() == 1)
    {
        // number of selections can be < result.size() due to the if tests, so in case we fall
        // back on 1 entry no need to show a selection
        const CodeCompletionHelper::GotoDeclarationItem &item = foundItems.front();
        editorFilename = item.filename;
        editorLine     = item.line;
        tokenFound     = true;
    }

    // do we have a token?
    if (tokenFound)
    {
        cbEditor* targetEditor = edMan->Open(editorFilename);
        if (targetEditor)
            targetEditor->GotoTokenPosition(editorLine, target);
        else
        {
            if (isImpl)
                cbMessageBox(wxString::Format(_("Implementation not found: %s"), target),
                             _("Warning"), wxICON_WARNING);
            else if (isDecl)
                cbMessageBox(wxString::Format(_("Declaration not found: %s"), target),
                             _("Warning"), wxICON_WARNING);
        }
    }
    else
        cbMessageBox(wxString::Format(_("Not found: %s"), target), _("Warning"), wxICON_WARNING);
}

void CodeCompletion::OnFindReferences(cb_unused wxCommandEvent& event)
{
    m_CodeRefactoring.FindReferences();
}

void CodeCompletion::OnRenameSymbols(cb_unused wxCommandEvent& event)
{
    m_CodeRefactoring.RenameSymbols();
}

void CodeCompletion::OnOpenIncludeFile(cb_unused wxCommandEvent& event)
{
    wxString lastIncludeFileFrom;
    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (editor)
        lastIncludeFileFrom = editor->GetFilename();

    // check one more time because menu entries are enabled only when it makes sense
    // but the shortcut accelerator can always be executed
    bool MoveOn = false;
    wxString NameUnderCursor;
    bool IsInclude = false;
    if (CodeCompletionHelper::EditorHasNameUnderCursor(NameUnderCursor, IsInclude))
    {
        if (IsInclude)
            MoveOn = true;
    }

    if (!MoveOn)
        return; // nothing under cursor or thing under cursor is not an include

    TRACE(_T("OnOpenIncludeFile"));

    wxArrayString foundSet = m_ParseManager.GetParser().FindFileInIncludeDirs(NameUnderCursor); // search in all parser's include dirs

    // look in the same dir as the source file
    wxFileName fname = NameUnderCursor;
    wxFileName base = lastIncludeFileFrom;
    NormalizePath(fname, base.GetPath());
    if (wxFileExists(fname.GetFullPath()) )
        foundSet.Add(fname.GetFullPath());

    // search for the file in project files
    cbProject* project = m_ParseManager.GetProjectByEditor(editor);
    if (project)
    {
        for (FilesList::const_iterator it = project->GetFilesList().begin();
                                       it != project->GetFilesList().end(); ++it)
        {
            ProjectFile* pf = *it;
            if (!pf)
                continue;

            if ( IsSuffixOfPath(NameUnderCursor, pf->file.GetFullPath()) )
                foundSet.Add(pf->file.GetFullPath());
        }
    }

    // Remove duplicates
    for (int i = 0; i < (int)foundSet.Count() - 1; i++)
    {
        for (int j = i + 1; j < (int)foundSet.Count(); )
        {
            if (foundSet.Item(i) == foundSet.Item(j))
                foundSet.RemoveAt(j);
            else
                j++;
        }
    }

    wxString selectedFile;
    if (foundSet.GetCount() > 1)
    {    // more than 1 hit : let the user choose
        SelectIncludeFile Dialog(Manager::Get()->GetAppWindow());
        Dialog.AddListEntries(foundSet);
        PlaceWindow(&Dialog);
        if (Dialog.ShowModal() == wxID_OK)
            selectedFile = Dialog.GetIncludeFile();
        else
            return; // user cancelled the dialog...
    }
    else if (foundSet.GetCount() == 1)
        selectedFile = foundSet[0];

    if (!selectedFile.IsEmpty())
    {
        EditorManager* edMan = Manager::Get()->GetEditorManager();
        edMan->Open(selectedFile);
        return;
    }

    cbMessageBox(wxString::Format(_("Not found: %s"), NameUnderCursor), _("Warning"), wxICON_WARNING);
}

void CodeCompletion::OnCurrentProjectReparse(wxCommandEvent& event)
{
    m_ParseManager.ReparseCurrentProject();
    event.Skip();
}

void CodeCompletion::OnSelectedProjectReparse(wxCommandEvent& event)
{
    m_ParseManager.ReparseSelectedProject();
    event.Skip();
}

void CodeCompletion::OnSelectedFileReparse(wxCommandEvent& event)
{
    wxTreeCtrl* tree = Manager::Get()->GetProjectManager()->GetUI().GetTree();
    if (!tree)
        return;

    wxTreeItemId treeItem = Manager::Get()->GetProjectManager()->GetUI().GetTreeSelection();
    if (!treeItem.IsOk())
        return;

    const FileTreeData* data = static_cast<FileTreeData*>(tree->GetItemData(treeItem));
    if (!data)
        return;

    if (data->GetKind() == FileTreeData::ftdkFile)
    {
        cbProject* project = data->GetProject();
        ProjectFile* pf = data->GetProjectFile();
        if (pf && m_ParseManager.ReparseFile(project, pf->file.GetFullPath()))
        {
             CCLogger::Get()->DebugLog(_T("Reparsing the selected file ") +
                                       pf->file.GetFullPath());
        }
    }

    event.Skip();
}

void CodeCompletion::OnAppDoneStartup(CodeBlocksEvent& event)
{
    if (!m_InitDone)
        DoParseOpenedProjectAndActiveEditor();

    event.Skip();
}

void CodeCompletion::OnWorkspaceChanged(CodeBlocksEvent& event)
{
    // EVT_WORKSPACE_CHANGED is a powerful event, it's sent after any project
    // has finished loading or closing. It's the *LAST* event to be sent when
    // the workspace has been changed. So it's the ideal time to parse files
    // and update your widgets.
    if (IsAttached() && m_InitDone)
    {
        cbProject* project = Manager::Get()->GetProjectManager()->GetActiveProject();
        // if we receive a workspace changed event, but the project is NULL, this means two condition
        // could happen.
        // (1) the user try to close the application, so we don't need to update the UI here.
        // (2) the user just open a new project after cb started up
        if (project)
        {
            if (!m_ParseManager.GetParserByProject(project))
                m_ParseManager.CreateParser(project);

            // Update the Function toolbar
            TRACE(_T("CodeCompletion::OnWorkspaceChanged: Starting m_TimerToolbar."));
            m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);

            // Update the class browser
            if (m_ParseManager.GetParser().ClassBrowserOptions().displayFilter == bdfProject)
                m_ParseManager.UpdateClassBrowser();
        }
    }
    event.Skip();
}

void CodeCompletion::OnProjectActivated(CodeBlocksEvent& event)
{
    // The Class browser shouldn't be updated if we're in the middle of loading/closing
    // a project/workspace, because the class browser would need to be updated again.
    // So we need to update it with the EVT_WORKSPACE_CHANGED event, which gets
    // triggered after everything's finished loading/closing.
    if (!ProjectManager::IsBusy() && IsAttached() && m_InitDone)
    {
        cbProject* project = event.GetProject();
        if (project && !m_ParseManager.GetParserByProject(project) && project->GetFilesCount() > 0)
            m_ParseManager.CreateParser(project);

        if (m_ParseManager.GetParser().ClassBrowserOptions().displayFilter == bdfProject)
            m_ParseManager.UpdateClassBrowser();
    }

    m_NeedsBatchColour = true;

    event.Skip();
}

void CodeCompletion::OnProjectClosed(CodeBlocksEvent& event)
{
    // After this, the Class Browser needs to be updated. It will happen
    // when we receive the next EVT_PROJECT_ACTIVATED event.
    if (IsAttached() && m_InitDone)
    {
        cbProject* project = event.GetProject();
        if (project && m_ParseManager.GetParserByProject(project))
        {
            // there may be some pending files to be reparsed in m_ReparsingMap
            // so just remove them
            ReparsingMap::iterator it = m_ReparsingMap.find(project);
            if (it != m_ReparsingMap.end())
                m_ReparsingMap.erase(it);

            // remove the Parser instances associated with the project
            while (m_ParseManager.DeleteParser(project));
        }
    }
    event.Skip();
}

void CodeCompletion::OnProjectSaved(CodeBlocksEvent& event)
{
    // reparse project (compiler search dirs might have changed)
    m_TimerProjectSaved.SetClientData(event.GetProject());
    // we need more time for waiting wxExecute in ParseManager::AddCompilerPredefinedMacros
    TRACE(_T("CodeCompletion::OnProjectSaved: Starting m_TimerProjectSaved."));
    m_TimerProjectSaved.Start(200, wxTIMER_ONE_SHOT);

    event.Skip();
}

void CodeCompletion::OnProjectFileAdded(CodeBlocksEvent& event)
{
    if (IsAttached() && m_InitDone)
        m_ParseManager.AddFileToParser(event.GetProject(), event.GetString());
    event.Skip();
}

void CodeCompletion::OnProjectFileRemoved(CodeBlocksEvent& event)
{
    if (IsAttached() && m_InitDone)
        m_ParseManager.RemoveFileFromParser(event.GetProject(), event.GetString());
    event.Skip();
}

void CodeCompletion::OnProjectFileChanged(CodeBlocksEvent& event)
{
    if (IsAttached() && m_InitDone)
    {
        // TODO (Morten#5#) make sure the event.GetProject() is valid.
        cbProject* project = event.GetProject();
        wxString filename = event.GetString();
        if (!project)
            project = m_ParseManager.GetProjectByFilename(filename);
        if (project && m_ParseManager.ReparseFile(project, filename))
            CCLogger::Get()->DebugLog(_T("Reparsing when file changed: ") + filename);
    }
    event.Skip();
}

void CodeCompletion::OnEditorSave(CodeBlocksEvent& event)
{
    if (!ProjectManager::IsBusy() && IsAttached() && m_InitDone && event.GetEditor())
    {
        cbProject* project = event.GetProject();

        // we know which project the editor belongs to, so put a (project, file) pair to the
        // m_ReparsingMap
        ReparsingMap::iterator it = m_ReparsingMap.find(project);
        if (it == m_ReparsingMap.end())
            it = m_ReparsingMap.insert(std::make_pair(project, wxArrayString())).first;

        const wxString& filename = event.GetEditor()->GetFilename();
        if (it->second.Index(filename) == wxNOT_FOUND)
            it->second.Add(filename);

        // start the timer, so that it will be handled in timer event handler
        TRACE(_T("CodeCompletion::OnEditorSave: Starting m_TimerReparsing."));
        m_TimerReparsing.Start(EDITOR_ACTIVATED_DELAY + it->second.GetCount() * 10, wxTIMER_ONE_SHOT);
    }

    event.Skip();
}

void CodeCompletion::OnEditorOpen(CodeBlocksEvent& event)
{
    if (!Manager::IsAppShuttingDown() && IsAttached() && m_InitDone)
    {
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());
        if (ed)
        {
            FunctionsScopePerFile* funcdata = &(m_AllFunctionsScopes[ed->GetFilename()]);
            funcdata->parsed = false;
        }
    }

    event.Skip();
}

void CodeCompletion::OnEditorActivated(CodeBlocksEvent& event)
{
    TRACE(_T("CodeCompletion::OnEditorActivated(): Enter"));

    if (!ProjectManager::IsBusy() && IsAttached() && m_InitDone && event.GetEditor())
    {
        m_LastEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());

        TRACE(_T("CodeCompletion::OnEditorActivated(): Starting m_TimerEditorActivated."));
        m_TimerEditorActivated.Start(EDITOR_ACTIVATED_DELAY, wxTIMER_ONE_SHOT);

        if (m_TimerToolbar.IsRunning())
            m_TimerToolbar.Stop();
    }

    event.Skip();
    TRACE(_T("CodeCompletion::OnEditorActivated(): Leave"));
}

void CodeCompletion::OnEditorClosed(CodeBlocksEvent& event)
{
    EditorManager* edm = Manager::Get()->GetEditorManager();
    if (!edm)
    {
        event.Skip();
        return;
    }

    wxString activeFile;
    EditorBase* eb = edm->GetActiveEditor();
    if (eb)
        activeFile = eb->GetFilename();

    TRACE(_T("CodeCompletion::OnEditorClosed(): Closed editor's file is %s"), activeFile.wx_str());

    if (m_LastEditor == event.GetEditor())
    {
        m_LastEditor = nullptr;
        if (m_TimerEditorActivated.IsRunning())
            m_TimerEditorActivated.Stop();
    }

    // tell m_ParseManager that a builtin editor was closed
    if ( edm->GetBuiltinEditor(event.GetEditor()) )
        m_ParseManager.OnEditorClosed(event.GetEditor());

    m_LastFile.Clear();

    // we need to clear CC toolbar only when we are closing last editor
    // in other situations OnEditorActivated does this job
    // If no editors were opened, or a non-buildin-editor was active, disable the CC toolbar
    if (edm->GetEditorsCount() == 0 || !edm->GetActiveEditor() || !edm->GetActiveEditor()->IsBuiltinEditor())
    {
        EnableToolbarTools(false);

        // clear toolbar when closing last editor
        if (m_Scope)
            m_Scope->Clear();
        if (m_Function)
            m_Function->Clear();

        cbEditor* ed = edm->GetBuiltinEditor(event.GetEditor());
        wxString filename;
        if (ed)
            filename = ed->GetFilename();

        m_AllFunctionsScopes[filename].m_FunctionsScope.clear();
        m_AllFunctionsScopes[filename].m_NameSpaces.clear();
        m_AllFunctionsScopes[filename].parsed = false;
        if (m_ParseManager.GetParser().ClassBrowserOptions().displayFilter == bdfFile)
            m_ParseManager.UpdateClassBrowser();
    }

    event.Skip();
}

// ----------------------------------------------------------------------------
void CodeCompletion::OnCCLogger(CodeBlocksThreadEvent& event)
// ----------------------------------------------------------------------------
{
    if (!Manager::IsAppShuttingDown())
        Manager::Get()->GetLogManager()->Log(event.GetString());
}

void CodeCompletion::OnCCDebugLogger(CodeBlocksThreadEvent& event)
{
    if (!Manager::IsAppShuttingDown())
        Manager::Get()->GetLogManager()->DebugLog(event.GetString());
}

void CodeCompletion::OnParserStart(wxCommandEvent& event)
{
    cbProject*                project = static_cast<cbProject*>(event.GetClientData());
    ParserCommon::ParserState state   = static_cast<ParserCommon::ParserState>(event.GetInt());
    // Parser::OnBatchTimer will send this Parser Start event
    // If it starts a full parsing(ptCreateParser), we should prepare some data for the header
    // file crawler
    if (state == ParserCommon::ptCreateParser)
    {
        if (m_CCEnableHeaders)
        {
            wxArrayString &dirs = GetSystemIncludeDirs(project, true); // true means update the cache
            if (!dirs.empty())
            {
                SystemHeadersThread* thread = new SystemHeadersThread(this,
                                                                      &m_SystemHeadersThreadCS,
                                                                      m_SystemHeadersMap, dirs);
                m_SystemHeadersThreads.push_back(thread);
                thread->Run();
            }
        }

        cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (m_ParseManager.GetProjectByEditor(editor) == project)
            EnableToolbarTools(false);


    }
}

void CodeCompletion::OnParserEnd(wxCommandEvent& event)
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* editor = edMan->GetBuiltinActiveEditor();
    if (editor)
    {
        m_ToolbarNeedReparse = true;
        TRACE(_T("CodeCompletion::OnParserEnd: Starting m_TimerToolbar."));
        m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
    }

    if (m_NeedsBatchColour)
    {
        for (int edIdx = edMan->GetEditorsCount() - 1; edIdx >= 0; --edIdx)
        {
            editor = edMan->GetBuiltinEditor(edIdx);
            if (editor)
                UpdateEditorSyntax(editor);
        }
        m_NeedsBatchColour = false;
    }

    event.Skip();
}

void CodeCompletion::OnSystemHeadersThreadMessage(CodeBlocksThreadEvent& event)
{
    CCLogger::Get()->DebugLog(event.GetString());
}

void CodeCompletion::OnSystemHeadersThreadFinish(CodeBlocksThreadEvent& event)
{
    if (m_SystemHeadersThreads.empty())
        return;
    // Wait for the current thread to finish and remove it from the thread list.
    SystemHeadersThread* thread = static_cast<SystemHeadersThread*>(event.GetClientData());

    for (std::list<SystemHeadersThread*>::iterator it = m_SystemHeadersThreads.begin();
         it != m_SystemHeadersThreads.end();
         ++it)
    {
        if (*it == thread)
        {
            if (!event.GetString().IsEmpty())
                CCLogger::Get()->DebugLog(event.GetString());
            thread->Wait();
            delete thread;
            m_SystemHeadersThreads.erase(it);
            break;
        }
    }
}

int CodeCompletion::DoClassMethodDeclImpl()
{
    if (!IsAttached() || !m_InitDone)
        return -1;

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return -3;

    FileType ft = FileTypeOf(ed->GetShortName());
    if ( ft != ftHeader && ft != ftSource && ft != ftTemplateSource) // only parse source/header files
        return -4;

    if (!m_ParseManager.GetParser().Done())
    {
        wxString msg = _("The Parser is still parsing files.");
        msg += m_ParseManager.GetParser().NotDoneReason();
        CCLogger::Get()->DebugLog(msg);
        return -5;
    }

    int success = -6;

//    TokenTree* tree = m_ParseManager.GetParser().GetTokenTree(); // The one used inside InsertClassMethodDlg

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    // open the insert class dialog
    wxString filename = ed->GetFilename();
    InsertClassMethodDlg dlg(Manager::Get()->GetAppWindow(), &m_ParseManager.GetParser(), filename);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        cbStyledTextCtrl* control = ed->GetControl();
        int pos = control->GetCurrentPos();
        int line = control->LineFromPosition(pos);
        control->GotoPos(control->PositionFromLine(line));

        wxArrayString result = dlg.GetCode();
        for (unsigned int i = 0; i < result.GetCount(); ++i)
        {
            pos = control->GetCurrentPos();
            line = control->LineFromPosition(pos);
            // get the indent string from previous line
            wxString str = ed->GetLineIndentString(line - 1) + result[i];
            MatchCodeStyle(str, control->GetEOLMode(), ed->GetLineIndentString(line - 1), control->GetUseTabs(), control->GetTabWidth());
            control->SetTargetStart(pos);
            control->SetTargetEnd(pos);
            control->ReplaceTarget(str);
            control->GotoPos(pos + str.Length());// - 3);
        }
        success = 0;
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    return success;
}

int CodeCompletion::DoAllMethodsImpl()
{
    if (!IsAttached() || !m_InitDone)
        return -1;

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return -3;

    FileType ft = FileTypeOf(ed->GetShortName());
    if ( ft != ftHeader && ft != ftSource && ft != ftTemplateSource) // only parse source/header files
        return -4;

    wxArrayString paths = m_ParseManager.GetAllPathsByFilename(ed->GetFilename());
    TokenTree*    tree  = m_ParseManager.GetParser().GetTokenTree();

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    // get all filenames' indices matching our mask
    TokenFileSet result;
    for (size_t i = 0; i < paths.GetCount(); ++i)
    {
        CCLogger::Get()->DebugLog(_T("CodeCompletion::DoAllMethodsImpl(): Trying to find matches for: ") + paths[i]);
        TokenFileSet result_file;
        tree->GetFileMatches(paths[i], result_file, true, true);
        for (TokenFileSet::const_iterator it = result_file.begin(); it != result_file.end(); ++it)
            result.insert(*it);
    }

    if (result.empty())
    {
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        cbMessageBox(_("Could not find any file match in parser's database."), _("Warning"), wxICON_WARNING);
        return -5;
    }

    // loop matching files, loop tokens in file and get list of un-implemented functions
    wxArrayString arr; // for selection (keeps strings)
    wxArrayInt arrint; // for selection (keeps indices)
    typedef std::map<int, std::pair<int, wxString> > ImplMap;
    ImplMap im;
    for (TokenFileSet::const_iterator itf = result.begin(); itf != result.end(); ++itf)
    {
        const TokenIdxSet* tokens = tree->GetTokensBelongToFile(*itf);
        if (!tokens) continue;

        // loop tokens in file
        for (TokenIdxSet::const_iterator its = tokens->begin(); its != tokens->end(); ++its)
        {
            const Token* token = tree->at(*its);
            if (   token // valid token
                && (token->m_TokenKind & (tkFunction | tkConstructor | tkDestructor)) // is method
                && token->m_ImplLine == 0 ) // is un-implemented
            {
                im[token->m_Line] = std::make_pair(*its, token->DisplayName());
            }
        }
    }

    for (ImplMap::const_iterator it = im.begin(); it != im.end(); ++it)
    {
        arrint.Add(it->second.first);
        arr.Add(it->second.second);
    }

    if (arr.empty())
    {
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        cbMessageBox(_("No classes declared or no un-implemented class methods found."), _("Warning"), wxICON_WARNING);
        return -5;
    }

    int success = -5;

    // select tokens
    MultiSelectDlg dlg(Manager::Get()->GetAppWindow(), arr, true);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        cbStyledTextCtrl* control = ed->GetControl();
        int pos = control->GetCurrentPos();
        int line = control->LineFromPosition(pos);
        control->GotoPos(control->PositionFromLine(line));

        bool addDoxgenComment = Manager::Get()->GetConfigManager(_T("code_completion"))->ReadBool(_T("/add_doxgen_comment"), false);

        wxArrayInt indices = dlg.GetSelectedIndices();
        for (size_t i = 0; i < indices.GetCount(); ++i)
        {
            const Token* token = tree->at(arrint[indices[i]]);
            if (!token)
                continue;

            pos  = control->GetCurrentPos();
            line = control->LineFromPosition(pos);

            // actual code generation
            wxString str;
            if (i > 0)
                str << _T("\n");
            else
                str << ed->GetLineIndentString(line - 1);
            if (addDoxgenComment)
                str << _T("/** @brief ") << token->m_Name << _T("\n  *\n  * @todo: document this function\n  */\n");
            wxString type = token->m_FullType;
            if (!type.IsEmpty())
            {
                // "int *" or "int &" ->  "int*" or "int&"
                if (   (type.Last() == _T('&') || type.Last() == _T('*'))
                    && type[type.Len() - 2] == _T(' '))
                {
                    type[type.Len() - 2] = type.Last();
                    type.RemoveLast();
                }
                str << type << _T(" ");
            }
            if (token->m_ParentIndex != -1)
            {
                const Token* parent = tree->at(token->m_ParentIndex);
                if (parent)
                    str << parent->m_Name << _T("::");
            }
            str << token->m_Name << token->GetStrippedArgs();
            if (token->m_IsConst)
                str << _T(" const");
            if (token->m_IsNoExcept)
                str << _T(" noexcept");
            str << _T("\n{\n\t\n}\n");

            MatchCodeStyle(str, control->GetEOLMode(), ed->GetLineIndentString(line - 1), control->GetUseTabs(), control->GetTabWidth());

            // add code in editor
            control->SetTargetStart(pos);
            control->SetTargetEnd(pos);
            control->ReplaceTarget(str);
            control->GotoPos(pos + str.Length());
        }
        if (!indices.IsEmpty())
        {
            pos  = control->GetCurrentPos();
            line = control->LineFromPosition(pos);
            control->GotoPos(control->GetLineEndPosition(line - 2));
        }
        success = 0;
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    return success;
}

void CodeCompletion::MatchCodeStyle(wxString& str, int eolStyle, const wxString& indent, bool useTabs, int tabSize)
{
    str.Replace(wxT("\n"), GetEOLStr(eolStyle) + indent);
    if (!useTabs)
        str.Replace(wxT("\t"), wxString(wxT(' '), tabSize));
    if (!indent.IsEmpty())
        str.RemoveLast(indent.Length());
}

// help method in finding the function position in the vector for the function containing the current line
void CodeCompletion::FunctionPosition(int &scopeItem, int &functionItem) const
{
    scopeItem = -1;
    functionItem = -1;

    for (unsigned int idxSc = 0; idxSc < m_ScopeMarks.size(); ++idxSc)
    {
        // this is the start and end of a scope
        unsigned int start = m_ScopeMarks[idxSc];
        unsigned int end = (idxSc + 1 < m_ScopeMarks.size()) ? m_ScopeMarks[idxSc + 1] : m_FunctionsScope.size();

        // the scope could have many functions, so loop on the functions
        for (int idxFn = 0; start + idxFn < end; ++idxFn)
        {
            const FunctionScope fs = m_FunctionsScope[start + idxFn];
            if (m_CurrentLine >= fs.StartLine && m_CurrentLine <= fs.EndLine)
            {
                scopeItem = idxSc;
                functionItem = idxFn;
            }
        }
    }
}

void CodeCompletion::GotoFunctionPrevNext(bool next /* = false */)
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return;

    int current_line = ed->GetControl()->GetCurrentLine();

    if (!m_FunctionsScope.size())
        return;

    // search previous/next function from current line, default: previous
    int          line            = -1;
    unsigned int best_func       = 0;
    bool         found_best_func = false;
    for (unsigned int idx_func=0; idx_func<m_FunctionsScope.size(); ++idx_func)
    {
        int best_func_line  = m_FunctionsScope[best_func].StartLine;
        int func_start_line = m_FunctionsScope[idx_func].StartLine;
        if (next)
        {
            if         (best_func_line  > current_line)     // candidate: is after current line
            {
                if (   (func_start_line > current_line  )   // another candidate
                    && (func_start_line < best_func_line) ) // decide which is more near
                { best_func = idx_func; found_best_func = true; }
            }
            else if    (func_start_line > current_line)     // candidate: is after current line
            {     best_func = idx_func; found_best_func = true; }
        }
        else // prev
        {
            if         (best_func_line  < current_line)     // candidate: is before current line
            {
                if (   (func_start_line < current_line  )   // another candidate
                    && (func_start_line > best_func_line) ) // decide which is closer
                { best_func = idx_func; found_best_func = true; }
            }
            else if    (func_start_line < current_line)     // candidate: is before current line
            {     best_func = idx_func; found_best_func = true; }
        }
    }

    if      (found_best_func)
    { line = m_FunctionsScope[best_func].StartLine; }
    else if ( next && m_FunctionsScope[best_func].StartLine>current_line)
    { line = m_FunctionsScope[best_func].StartLine; }
    else if (!next && m_FunctionsScope[best_func].StartLine<current_line)
    { line = m_FunctionsScope[best_func].StartLine; }

    if (line != -1)
    {
        ed->GotoLine(line);
        ed->SetFocus();
    }
}

// help method in finding the namespace position in the vector for the namespace containing the current line
int CodeCompletion::NameSpacePosition() const
{
    int pos = -1;
    int startLine = -1;
    for (unsigned int idxNs = 0; idxNs < m_NameSpaces.size(); ++idxNs)
    {
        const NameSpace& ns = m_NameSpaces[idxNs];
        if (m_CurrentLine >= ns.StartLine && m_CurrentLine <= ns.EndLine && ns.StartLine > startLine)
        {
            // got one, maybe there might be a better fitting namespace
            // (embedded namespaces) so keep on looking
            pos = static_cast<int>(idxNs);
            startLine = ns.StartLine;
        }
    }

    return pos;
}

void CodeCompletion::OnScope(wxCommandEvent&)
{
    int sel = m_Scope->GetSelection();
    if (sel != -1 && sel < static_cast<int>(m_ScopeMarks.size()))
        UpdateFunctions(sel);
}

void CodeCompletion::OnFunction(cb_unused wxCommandEvent& event)
{
    int selSc = (m_Scope) ? m_Scope->GetSelection() : 0;
    if (selSc != -1 && selSc < static_cast<int>(m_ScopeMarks.size()))
    {
        int idxFn = m_ScopeMarks[selSc] + m_Function->GetSelection();
        if (idxFn != -1 && idxFn < static_cast<int>(m_FunctionsScope.size()))
        {
            cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
            if (ed)
                ed->GotoTokenPosition(m_FunctionsScope[idxFn].StartLine,
                                      m_FunctionsScope[idxFn].ShortName);
        }
    }
}

/** Here is the explanation of how the two wxChoices are constructed.
 * for a file have such contents below
 * @code{.cpp}
 *  Line  0     void g_func1(){
 *  Line  1     }
 *  Line  2
 *  Line  3     void ClassA::func1(){
 *  Line  4     }
 *  Line  5
 *  Line  6     void ClassA::func2(){
 *  Line  7     }
 *  Line  8
 *  Line  9     void ClassB::func1(){
 *  Line 10     }
 *  Line 11
 *  Line 12     void ClassB::func2(){
 *  Line 13     }
 *  Line 14
 *  Line 15     namespace NamespaceA{
 *  Line 16         void func3(){
 *  Line 17         }
 *  Line 18
 *  Line 19         class ClassC {
 *  Line 20
 *  Line 21             void func4(){
 *  Line 22             }
 *  Line 23         }
 *  Line 24     }
 *  Line 25
 *
 * @endcode
 *
 * The two key variable will be constructed like below
 * @code
 *  m_FunctionsScope is std::vector of length 9, capacity 9 =
 *  {
 *  {StartLine = 0, EndLine = 1, ShortName = L"g_func1", Name = L"g_func1() : void", Scope = L"<global>"},
 *  {StartLine = 3, EndLine = 4, ShortName = L"func1", Name = L"func1() : void", Scope = L"ClassA::"},
 *  {StartLine = 6, EndLine = 7, ShortName = L"func2", Name = L"func2() : void", Scope = L"ClassA::"},
 *  {StartLine = 9, EndLine = 10, ShortName = L"func1", Name = L"func1() : void", Scope = L"ClassB::"},
 *  {StartLine = 12, EndLine = 13, ShortName = L"func2", Name = L"func2() : void", Scope = L"ClassB::"},
 *  {StartLine = 14, EndLine = 23, ShortName = L"", Name = L"", Scope = L"NamespaceA::"},
 *  {StartLine = 16, EndLine = 17, ShortName = L"func3", Name = L"func3() : void", Scope = L"NamespaceA::"},
 *  {StartLine = 19, EndLine = 23, ShortName = L"", Name = L"", Scope = L"NamespaceA::ClassC::"},
 *  {StartLine = 21, EndLine = 22, ShortName = L"func4", Name = L"func4() : void", Scope = L"NamespaceA::ClassC::"}
 *  }
 *
 *  m_NameSpaces is std::vector of length 1, capacity 1 =
 *  {{Name = L"NamespaceA::", StartLine = 14, EndLine = 23}}
 *
 *  m_ScopeMarks is std::vector of length 5, capacity 8 = {0, 1, 3, 5, 7}
 * which is the start of Scope "<global>", Scope "ClassA::" and Scope "ClassB::",
 * "NamespaceA::" and "NamespaceA::ClassC::"
 * @endcode
 *
 * Then we have wxChoice Scopes and Functions like below
 * @code
 *      <global>          ClassA::        ClassB::
 *        |- g_func1()      |- func1()      |- func1()
 *                          |- func2()      |- func2()
 *
 *      NamespaceA::      NamespaceA::ClassC::
 *        |- func3()        |- func4()
 * @endcode
 */
void CodeCompletion::ParseFunctionsAndFillToolbar()
{
    TRACE(_T("ParseFunctionsAndFillToolbar() : m_ToolbarNeedReparse=%d, m_ToolbarNeedRefresh=%d, "),
          m_ToolbarNeedReparse?1:0, m_ToolbarNeedRefresh?1:0);

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    if (!edMan) // Closing the app?
        return;

    cbEditor* ed = edMan->GetBuiltinActiveEditor();

    bool isCC_FileType = true;
    // Process only CC specified file types. //(2023/11/11)
    if (ed and ed->GetControl())
    {
        switch (ParserCommon::FileType(ed->GetFilename()))
        {
            case ParserCommon::ftHeader:
            case ParserCommon::ftSource:
                isCC_FileType = true; break;
            case ParserCommon::ftOther:
                isCC_FileType = false; break;
            default:
                isCC_FileType  = false; break;
        }
    }

    // When not CC editor or file type, clear toolbar display
    if (!ed || !ed->GetControl() || (not isCC_FileType))
    {
        if (m_Function)
            m_Function->Clear();
        if (m_Scope)
            m_Scope->Clear();

        EnableToolbarTools(false);
        m_LastFile.Clear();
        return;
    }

    const wxString filename = ed->GetFilename();
    if (filename.IsEmpty())
        return;

    bool fileParseFinished = m_ParseManager.GetParser().IsFileParsed(filename);

    // FunctionsScopePerFile contains all the function and namespace information for
    // a specified file, m_AllFunctionsScopes[filename] will implicitly insert an new element in
    // the map if no such key(filename) is found.
    FunctionsScopePerFile* funcdata = &(m_AllFunctionsScopes[filename]);

    // *** Part 1: Parse the file (if needed) ***
    if (m_ToolbarNeedReparse || !funcdata->parsed)
    {
        if (m_ToolbarNeedReparse)
            m_ToolbarNeedReparse = false;

        funcdata->m_FunctionsScope.clear();
        funcdata->m_NameSpaces.clear();

        // collect the function implementation information, just find the specified tokens in the TokenTree
        TokenIdxSet result;
        m_ParseManager.GetParser().FindTokensInFile(filename, result,
                                                    tkAnyFunction | tkEnum | tkClass | tkNamespace);
        if (!result.empty())
            funcdata->parsed = true;    // if the file did have some containers, flag it as parsed
        else
            fileParseFinished = false;  // this indicates the batch parser does not finish parsing for the current file

        TokenTree* tree = m_ParseManager.GetParser().GetTokenTree();

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
        {
            const Token* token = tree->at(*it);
            if (token && token->m_ImplLine != 0)
            {
                FunctionScope fs;
                fs.StartLine = token->m_ImplLine    - 1;
                fs.EndLine   = token->m_ImplLineEnd - 1;
                const size_t fileIdx = tree->InsertFileOrGetIndex(filename);
                if (token->m_TokenKind & tkAnyFunction && fileIdx == token->m_ImplFileIdx)
                {
                    fs.Scope = token->GetNamespace();
                    if (fs.Scope.IsEmpty())
                        fs.Scope = g_GlobalScope;
                    wxString result_str = token->m_Name;
                    fs.ShortName = result_str;
                    result_str << token->GetFormattedArgs();
                    if (!token->m_BaseType.IsEmpty())
                        result_str << _T(" : ") << token->m_BaseType;
                    fs.Name = result_str;
                    funcdata->m_FunctionsScope.push_back(fs);
                }
                else if (token->m_TokenKind & (tkEnum | tkClass | tkNamespace))
                {
                    fs.Scope = token->GetNamespace() + token->m_Name + _T("::");
                    funcdata->m_FunctionsScope.push_back(fs);
                }
            }
        }

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        FunctionsScopeVec& functionsScopes = funcdata->m_FunctionsScope;
        NameSpaceVec& nameSpaces = funcdata->m_NameSpaces;

        // collect the namespace information in the current file, this is done by running a parserthread
        // on the editor's buffer
        m_ParseManager.GetParser().ParseBufferForNamespaces(ed->GetControl()->GetText(), nameSpaces);
        std::sort(nameSpaces.begin(), nameSpaces.end(), CodeCompletionHelper::LessNameSpace);

        // copy the namespace information collected in ParseBufferForNamespaces() to
        // the functionsScopes, note that the element type FunctionScope has a constructor
        // FunctionScope(const NameSpace& ns), type conversion is done automatically
        std::copy(nameSpaces.begin(), nameSpaces.end(), back_inserter(functionsScopes));
        std::sort(functionsScopes.begin(), functionsScopes.end(), CodeCompletionHelper::LessFunctionScope);

        // remove consecutive duplicates
        FunctionsScopeVec::const_iterator it;
        it = unique(functionsScopes.begin(), functionsScopes.end(), CodeCompletionHelper::EqualFunctionScope);
        functionsScopes.resize(it - functionsScopes.begin());

        TRACE(wxString::Format("Found %zu namespace locations", nameSpaces.size()));
#if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
        for (unsigned int i = 0; i < nameSpaces.size(); ++i)
            CCLogger::Get()->DebugLog(wxString::Format("\t%s (%d:%d)",
                nameSpaces[i].Name, nameSpaces[i].StartLine, nameSpaces[i].EndLine));
#endif

        if (!m_ToolbarNeedRefresh)
            m_ToolbarNeedRefresh = true;
    }

    // *** Part 2: Fill the toolbar ***
    m_FunctionsScope = funcdata->m_FunctionsScope;
    m_NameSpaces     = funcdata->m_NameSpaces;

    m_ScopeMarks.clear();
    unsigned int fsSize = m_FunctionsScope.size();
    if (!m_FunctionsScope.empty())
    {
        m_ScopeMarks.push_back(0);

        if (m_Scope) // show scope wxChoice
        {
            wxString lastScope = m_FunctionsScope[0].Scope;
            for (unsigned int idx = 1; idx < fsSize; ++idx)
            {
                const wxString& currentScope = m_FunctionsScope[idx].Scope;

                // if the scope name has changed, push a new index
                if (lastScope != currentScope)
                {
                    m_ScopeMarks.push_back(idx);
                    lastScope = currentScope;
                }
            }
        }
    }

    TRACE(wxString::Format("Parsed %zu functionscope items", m_FunctionsScope.size()));
#if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
    for (unsigned int i = 0; i < m_FunctionsScope.size(); ++i)
        CCLogger::Get()->DebugLog(wxString::Format("\t%s%s (%d:%d)",
            m_FunctionsScope[i].Scope, m_FunctionsScope[i].Name,
            m_FunctionsScope[i].StartLine, m_FunctionsScope[i].EndLine));
#endif

    // Does the toolbar need a refresh?
    if (m_ToolbarNeedRefresh || m_LastFile != filename)
    {
        // Update the last editor and changed flag...
        if (m_ToolbarNeedRefresh)
            m_ToolbarNeedRefresh = false;
        if (m_LastFile != filename)
        {
            TRACE(_T("ParseFunctionsAndFillToolbar() : Update last file is %s"), filename.wx_str());
            m_LastFile = filename;
        }

        // ...and refresh the toolbars.
        m_Function->Clear();

        if (m_Scope)
        {
            m_Scope->Freeze();
            m_Scope->Clear();

            // add to the choice controls
            for (unsigned int idxSc = 0; idxSc < m_ScopeMarks.size(); ++idxSc)
            {
                int idxFn = m_ScopeMarks[idxSc];
                const FunctionScope& fs = m_FunctionsScope[idxFn];
                m_Scope->Append(fs.Scope);
            }

            m_Scope->Thaw();
        }
        else
        {
            m_Function->Freeze();

            for (unsigned int idxFn = 0; idxFn < m_FunctionsScope.size(); ++idxFn)
            {
                const FunctionScope& fs = m_FunctionsScope[idxFn];
                if (fs.Name != wxEmptyString)
                    m_Function->Append(fs.Scope + fs.Name);
                else if (fs.Scope.EndsWith(wxT("::")))
                    m_Function->Append(fs.Scope.substr(0, fs.Scope.length()-2));
                else
                    m_Function->Append(fs.Scope);
            }

            m_Function->Thaw();
        }
    }

    // Find the current function and update
    FindFunctionAndUpdate(ed->GetControl()->GetCurrentLine());

    // Control the toolbar state, if the batch parser does not finish parsing the file, no need to update CC toolbar.
    EnableToolbarTools(fileParseFinished);
}

void CodeCompletion::FindFunctionAndUpdate(int currentLine)
{
    if (currentLine == -1)
        return;

    m_CurrentLine = currentLine;

    int selSc, selFn;
    FunctionPosition(selSc, selFn);

    if (m_Scope)
    {
        if (selSc != -1 && selSc != m_Scope->GetSelection())
        {
            m_Scope->SetSelection(selSc);
            UpdateFunctions(selSc);
        }
        else if (selSc == -1)
            m_Scope->SetSelection(-1);
    }

    if (selFn != -1 && selFn != m_Function->GetSelection())
        m_Function->SetSelection(selFn);
    else if (selFn == -1)
    {
        m_Function->SetSelection(-1);

        wxChoice* choice = (m_Scope) ? m_Scope : m_Function;

        int NsSel = NameSpacePosition();
        if (NsSel != -1)
            choice->SetStringSelection(m_NameSpaces[NsSel].Name);
        else if (!m_Scope)
            choice->SetSelection(-1);
        else
        {
            choice->SetStringSelection(g_GlobalScope);
            wxCommandEvent evt(wxEVT_COMMAND_CHOICE_SELECTED, XRCID("chcCodeCompletionScope"));
            wxPostEvent(this, evt);
        }
    }
}

void CodeCompletion::UpdateFunctions(unsigned int scopeItem)
{
    m_Function->Freeze();
    m_Function->Clear();

    unsigned int idxEnd = (scopeItem + 1 < m_ScopeMarks.size()) ? m_ScopeMarks[scopeItem + 1] : m_FunctionsScope.size();
    for (unsigned int idxFn = m_ScopeMarks[scopeItem]; idxFn < idxEnd; ++idxFn)
    {
        const wxString &name = m_FunctionsScope[idxFn].Name;
        m_Function->Append(name);
    }

    m_Function->Thaw();
}

void CodeCompletion::EnableToolbarTools(bool enable)
{
    if (m_Scope)
        m_Scope->Enable(enable);
    if (m_Function)
        m_Function->Enable(enable);
}

void CodeCompletion::DoParseOpenedProjectAndActiveEditor()
{
    // Let the app startup before parsing
    // This is to prevent the Splash Screen from delaying so much. By adding
    // the timer, the splash screen is closed and Code::Blocks doesn't take
    // so long in starting.
    m_InitDone = true;

    // Dreaded DDE-open bug related: do not touch the following lines unless for a good reason

    // parse any projects opened through DDE or the command-line
    cbProject* curProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (curProject && !m_ParseManager.GetParserByProject(curProject))
        m_ParseManager.CreateParser(curProject);

    // parse any files opened through DDE or the command-line
    EditorBase* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (editor)
        m_ParseManager.OnEditorActivated(editor);
}

void CodeCompletion::UpdateEditorSyntax(cbEditor* ed)
{
    if (!Manager::Get()->GetConfigManager(wxT("code_completion"))->ReadBool(wxT("/semantic_keywords"), false))
        return;
    if (!ed)
        ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed || ed->GetControl()->GetLexer() != wxSCI_LEX_CPP)
        return;

    TokenIdxSet result;
    int flags = tkAnyContainer | tkAnyFunction;
    if (ed->GetFilename().EndsWith(wxT(".c")))
        flags |= tkVariable;
    m_ParseManager.GetParser().FindTokensInFile(ed->GetFilename(), result, flags);
    TokenTree* tree = m_ParseManager.GetParser().GetTokenTree();

    std::set<wxString> varList;
    TokenIdxSet parsedTokens;

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
    {
        Token* token = tree->at(*it);
        if (!token)
            continue;
        if (token->m_TokenKind == tkVariable) // global var - only added in C
        {
            varList.insert(token->m_Name);
            continue;
        }
        else if (token->m_TokenKind & tkAnyFunction) // find parent class
        {
            if (token->m_ParentIndex == wxNOT_FOUND)
                continue;
            else
                token = tree->at(token->m_ParentIndex);
        }
        if (!token || parsedTokens.find(token->m_Index) != parsedTokens.end())
            continue; // no need to check the same token multiple times
        parsedTokens.insert(token->m_Index);
        for (TokenIdxSet::const_iterator chIt = token->m_Children.begin();
             chIt != token->m_Children.end(); ++chIt)
        {
            const Token* chToken = tree->at(*chIt);
            if (chToken && chToken->m_TokenKind == tkVariable)
            {
                varList.insert(chToken->m_Name);
            }
        }
        // inherited members
        if (token->m_Ancestors.empty())
            tree->RecalcInheritanceChain(token);
        for (TokenIdxSet::const_iterator ancIt = token->m_Ancestors.begin();
             ancIt != token->m_Ancestors.end(); ++ancIt)
        {
            const Token* ancToken = tree->at(*ancIt);
            if (!ancToken || parsedTokens.find(ancToken->m_Index) != parsedTokens.end())
                continue;
            for (TokenIdxSet::const_iterator chIt = ancToken->m_Children.begin();
                 chIt != ancToken->m_Children.end(); ++chIt)
            {
                const Token* chToken = tree->at(*chIt);
                if (   chToken && chToken->m_TokenKind == tkVariable
                    && chToken->m_Scope != tsPrivate) // cannot inherit these...
                {
                    varList.insert(chToken->m_Name);
                }
            }
        }
    }
    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    EditorColourSet* colour_set = Manager::Get()->GetEditorManager()->GetColourSet();
    if (!colour_set)
        return;

    wxString keywords = colour_set->GetKeywords(ed->GetLanguage(), 3);
    for (std::set<wxString>::const_iterator keyIt = varList.begin();
         keyIt != varList.end(); ++keyIt)
    {
        keywords += wxT(" ") + *keyIt;
    }
    ed->GetControl()->SetKeyWords(3, keywords);
    ed->GetControl()->Colourise(0, -1);
}

void CodeCompletion::OnToolbarTimer(cb_unused wxTimerEvent& event)
{
    TRACE(_T("CodeCompletion::OnToolbarTimer(): Enter"));

    if (!ProjectManager::IsBusy())
        ParseFunctionsAndFillToolbar();
    else
    {
        TRACE(_T("CodeCompletion::OnToolbarTimer(): Starting m_TimerToolbar."));
        m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
    }

    TRACE(_T("CodeCompletion::OnToolbarTimer(): Leave"));
}

void CodeCompletion::OnRealtimeParsingTimer(cb_unused wxTimerEvent& event)
{
    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!editor)
        return;

    TRACE(_T("OnRealtimeParsingTimer"));

    // the real time parsing timer event has arrived, but the document size has changed, in this
    // case, we should fire another timer event, and do the parsing job later
    const int curLen = editor->GetControl()->GetLength();
    if (curLen != m_CurrentLength)
    {
        m_CurrentLength = curLen;
        TRACE(_T("CodeCompletion::OnRealtimeParsingTimer: Starting m_TimerRealtimeParsing."));
        m_TimerRealtimeParsing.Start(REALTIME_PARSING_DELAY, wxTIMER_ONE_SHOT);
        return;
    }

    cbProject* project = m_ParseManager.GetProjectByEditor(editor);
    if (project && !project->GetFileByFilename(m_LastFile, false, true))
        return;
    if (m_ParseManager.ReparseFile(project, m_LastFile))
        CCLogger::Get()->DebugLog(_T("Reparsing when typing for editor ") + m_LastFile);
}

void CodeCompletion::OnProjectSavedTimer(cb_unused wxTimerEvent& event)
{
    cbProject* project = static_cast<cbProject*>(m_TimerProjectSaved.GetClientData());
    m_TimerProjectSaved.SetClientData(NULL);

    ProjectsArray* projs = Manager::Get()->GetProjectManager()->GetProjects();
    if (projs->Index(project) == wxNOT_FOUND)
        return;

    if (IsAttached() && m_InitDone && project)
    {
        TRACE(_T("OnProjectSavedTimer"));
        if (project &&  m_ParseManager.GetParserByProject(project))
        {
            ReparsingMap::iterator it = m_ReparsingMap.find(project);
            if (it != m_ReparsingMap.end())
                m_ReparsingMap.erase(it);
            if (m_ParseManager.DeleteParser(project))
            {
                CCLogger::Get()->DebugLog(_T("Reparsing project."));
                m_ParseManager.CreateParser(project);
            }
        }
    }
}

void CodeCompletion::OnReparsingTimer(cb_unused wxTimerEvent& event)
{
    if (ProjectManager::IsBusy() || !IsAttached() || !m_InitDone)
    {
        m_ReparsingMap.clear();
        CCLogger::Get()->DebugLog(_T("Reparsing files failed!"));
        return;
    }

    TRACE(_T("OnReparsingTimer"));

    ReparsingMap::iterator it = m_ReparsingMap.begin();
    if (it != m_ReparsingMap.end() && m_ParseManager.Done())
    {
        cbProject* project = it->first;
        wxArrayString& files = it->second;
        if (!project)
            project = m_ParseManager.GetProjectByFilename(files[0]);

        if (project && Manager::Get()->GetProjectManager()->IsProjectStillOpen(project))
        {
            wxString curFile;
            EditorBase* editor = Manager::Get()->GetEditorManager()->GetActiveEditor();
            if (editor)
                curFile = editor->GetFilename();

            size_t reparseCount = 0;
            while (!files.IsEmpty())
            {
                if (m_ParseManager.ReparseFile(project, files.Last()))
                {
                    ++reparseCount;
                    TRACE(_T("OnReparsingTimer: Reparsing file : ") + files.Last());
                    if (files.Last() == curFile)
                    {
                        m_ToolbarNeedReparse = true;
                        TRACE(_T("CodeCompletion::OnReparsingTimer: Starting m_TimerToolbar."));
                        m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
                    }
                }

                files.RemoveAt(files.GetCount() - 1);
            }

            if (reparseCount)
                CCLogger::Get()->DebugLog(wxString::Format("Re-parsed %zu files.", reparseCount));
        }

        if (files.IsEmpty())
            m_ReparsingMap.erase(it);
    }

    if (!m_ReparsingMap.empty())
    {
        TRACE(_T("CodeCompletion::OnReparsingTimer: Starting m_TimerReparsing."));
        m_TimerReparsing.Start(EDITOR_ACTIVATED_DELAY, wxTIMER_ONE_SHOT);
    }
}

void CodeCompletion::OnEditorActivatedTimer(cb_unused wxTimerEvent& event)
{
    // the m_LastEditor variable was updated in CodeCompletion::OnEditorActivated, after that,
    // the editor-activated-timer was started. So, here in the timer handler, we need to check
    // whether the saved editor and the current editor are the same, otherwise, no need to update
    // the toolbar, because there must be another editor activated before the timer hits.
    // Note: only the builtin editor was considered.
    EditorBase* editor  = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!editor || editor != m_LastEditor)
    {
        TRACE(_T("CodeCompletion::OnEditorActivatedTimer(): Not a builtin editor."));
        //m_LastEditor = nullptr;
        EnableToolbarTools(false);
        return;
    }

    const wxString& curFile = editor->GetFilename();
    // if the same file was activated, no need to update the toolbar
    if ( !m_LastFile.IsEmpty() && m_LastFile == curFile )
    {
        TRACE(_T("CodeCompletion::OnEditorActivatedTimer(): Same as the last activated file(%s)."), curFile.wx_str());
        return;
    }

    TRACE(_T("CodeCompletion::OnEditorActivatedTimer(): Need to notify ParseManager and Refresh toolbar."));

    m_ParseManager.OnEditorActivated(editor);
    TRACE(_T("CodeCompletion::OnEditorActivatedTimer: Starting m_TimerToolbar."));
    m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
    TRACE(_T("CodeCompletion::OnEditorActivatedTimer(): Current activated file is %s"), curFile.wx_str());
    UpdateEditorSyntax();
}

wxBitmap CodeCompletion::GetImage(ImageId::Id id, int fontSize)
{
    const int size = cbFindMinSize16to64(fontSize);

    // Check if it is in the cache
    const ImageId key(id, size);
    ImagesMap::const_iterator it = m_images.find(key);
    if (it != m_images.end())
        return it->second;

    // Image was not found, add it to the map
    wxString prefix(ConfigManager::GetDataFolder() + "/codecompletion.zip#zip:images/");
#if wxCHECK_VERSION(3, 1, 6)
    prefix << "svg/";
    const wxString ext(".svg");
#else
    prefix << wxString::Format("%dx%d/", size, size);
    const wxString ext(".png");
#endif

    wxString filename;
    switch (id)
    {
        case ImageId::HeaderFile:
            filename = prefix + "header" + ext;
            break;
        case ImageId::KeywordCPP:
            filename = prefix + "keyword_cpp" + ext;
            break;
        case ImageId::KeywordD:
            filename = prefix + "keyword_d" + ext;
            break;
        case ImageId::Unknown:
            filename = prefix + "unknown" + ext;
            break;
        case ImageId::Last:
        default:
            ;
    }

    wxBitmap bitmap;
    if (!filename.empty())
    {
#if wxCHECK_VERSION(3, 1, 6)
        bitmap = cbLoadBitmapBundleFromSVG(filename, wxSize(size, size)).GetBitmap(wxDefaultSize);
#else
        bitmap = cbLoadBitmap(filename);
#endif
        if (!bitmap.IsOk())
        {
            const wxString msg(wxString::Format(_("Cannot load image: '%s'!"), filename));
            Manager::Get()->GetLogManager()->LogError(msg);
        }
    }

    // If the bitmap is invalid create a valid one (a black square) for visual feedback
    if (!bitmap.IsOk())
        bitmap.Create(size, size);

    m_images[key] = bitmap;
    return bitmap;
}
