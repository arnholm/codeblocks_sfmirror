/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 77 $
 * $Id: ccoptionsdlg.cpp 77 2022-09-13 18:15:25Z pecanh $
 * $HeadURL: https://svn.code.sf.net/p/cb-clangd-client/code/trunk/clangd_client/src/codecompletion/ccoptionsdlg.cpp $
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/colordlg.h>
    #include <wx/combobox.h>
    #include <wx/intl.h>
    #include <wx/listbox.h>
    #include <wx/radiobut.h>
    #include <wx/regex.h>
    #include <wx/slider.h>
    #include <wx/spinctrl.h>
    #include <wx/stattext.h>
    #include <wx/treectrl.h>
    #include <wx/xrc/xmlres.h>

    #include <cbstyledtextctrl.h>
    #include <configmanager.h>
    #include <globals.h>
    #include <logmanager.h>
    #include <manager.h>
#endif

#include <wx/filedlg.h>
#include <wx/listbook.h>

#include <editpairdlg.h>

#include "cbcolourmanager.h"
#include "ccoptionsdlg.h"
#include "codecompletion.h"
#include "doxygen_parser.h" // For DocumentationHelper
#include "../ClangLocator.h"

static const wxString g_SampleClasses =
    _T("class A_class"
    "{"
    "    public:"
    "        int someInt_A;"
    "    protected:"
    "        bool mSomeVar_A;"
    "    private:"
    "        char* mData_A;"
    "};"
    "class B_class"
    "{"
    "    public:"
    "        int someInt_B;"
    "    protected:"
    "        bool mSomeVar_B;"
    "    private:"
    "        char* mData_B;"
    "};"
    "class C_class : public A_class"
    "{"
    "    public:"
    "        int someInt_C;"
    "    protected:"
    "        bool mSomeVar_C;"
    "    private:"
    "        char* mData_C;"
    "};"
    "enum SomeEnum"
    "{"
    "    optOne,"
    "    optTwo,"
    "    optThree"
    "};"
    "int x;"
    "int y;"
    "#define SOME_DEFINITION\n"
    "#define SOME_DEFINITION_2\n\n");

BEGIN_EVENT_TABLE(CCOptionsDlg, wxPanel)
    EVT_UPDATE_UI(-1,                       CCOptionsDlg::OnUpdateUI)
    EVT_BUTTON(XRCID("btnColour"),          CCOptionsDlg::OnChooseColour)
    EVT_COMMAND_SCROLL(XRCID("sldCCDelay"), CCOptionsDlg::OnCCDelayScroll)
    EVT_BUTTON(XRCID("btnDocBgColor"),      CCOptionsDlg::OnChooseColour)
    EVT_BUTTON(XRCID("btnDocTextColor"),    CCOptionsDlg::OnChooseColour)
    EVT_BUTTON(XRCID("btnDocLinkColor"),    CCOptionsDlg::OnChooseColour)
    EVT_BUTTON(XRCID("btnAutoDetect"),      CCOptionsDlg::OnClangd_AutoDetect)
    EVT_BUTTON(XRCID("btnMasterPath"),      CCOptionsDlg::OnFindDirClangd_Dlg)  // [...] button

END_EVENT_TABLE()

// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{
    #if defined(_WIN32)
        wxString clangdexe = "clangd.exe";
        wxString LLVM_Dirmaybe = "c:\\program files\\llvm\\bin";
    #else
        wxString clangdexe = "clangd";
        wxString LLVM_Dirmaybe = "/usr/bin";
    #endif
}
// ----------------------------------------------------------------------------
CCOptionsDlg::CCOptionsDlg(wxWindow* parent, ParseManager* np, ClgdCompletion* cc, DocumentationHelper* dh)
// ----------------------------------------------------------------------------
  : m_ParseManager(np),
    m_CodeCompletion(cc),
    m_Parser(np->GetParser()),
    m_Documentation(dh)
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));

    wxXmlResource::Get()->LoadPanel(this, parent, _T("dlgCCSettings"));

    // -----------------------------------------------------------------------
    // Handle all options that are being directly applied from config
    // -----------------------------------------------------------------------

    // Page "clangd_client"
    XRCCTRL(*this, "chkNoSemantic",         wxCheckBox)->SetValue(!cfg->ReadBool(_T("/semantic_keywords"),   false));
    XRCCTRL(*this, "chkAutoAddParentheses", wxCheckBox)->SetValue(cfg->ReadBool(_T("/auto_add_parentheses"), true));
    XRCCTRL(*this, "chkDetectImpl",         wxCheckBox)->SetValue(cfg->ReadBool(_T("/detect_implementation"),false));
    XRCCTRL(*this, "chkAddDoxgenComment",   wxCheckBox)->SetValue(cfg->ReadBool(_T("/add_doxgen_comment"),   false));
    XRCCTRL(*this, "chkEnableHeaders",      wxCheckBox)->SetValue(cfg->ReadBool(_T("/enable_headers"),       true));
    XRCCTRL(*this, "spnMaxMatches",         wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/max_matches"),           16384));
    XRCCTRL(*this, "txtFillupChars",        wxTextCtrl)->SetValue(cfg->Read(_T("/fillup_chars"),             wxEmptyString));
    XRCCTRL(*this, "sldCCDelay",            wxSlider)->SetValue(cfg->ReadInt(_T("/cc_delay"),                300) / 100);
    UpdateCCDelayLabel();
    XRCCTRL(*this, "chkKL_1",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set1"),  true));
    XRCCTRL(*this, "chkKL_2",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set2"),  true));
    XRCCTRL(*this, "chkKL_3",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set3"),  false));
    XRCCTRL(*this, "chkKL_4",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set4"),  false));
    XRCCTRL(*this, "chkKL_5",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set5"),  false));
    XRCCTRL(*this, "chkKL_6",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set6"),  false));
    XRCCTRL(*this, "chkKL_7",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set7"),  false));
    XRCCTRL(*this, "chkKL_8",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set8"),  false));
    XRCCTRL(*this, "chkKL_9",               wxCheckBox)->SetValue(cfg->ReadBool(_T("/lexer_keywords_set9"),  false));

    // Page "C / C++ parser"
    // NOTE (Morten#1#): Keep this in sync with files in the XRC file (settings.xrc) and ParseManager.cpp
    XRCCTRL(*this, "spnThreadsNum",            wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/max_threads"), 1));
    XRCCTRL(*this, "spnThreadsNum",            wxSpinCtrl)->Enable(true);   //(ph 2021/07/17)
    XRCCTRL(*this, "spnParsersWhileCompiling", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/max_parsers_while_compiling"), 0));
    XRCCTRL(*this, "spnParsersWhileCompiling", wxSpinCtrl)->Enable(true);   //(ph 2022/04/25)

    // Page "C / C++ parser (adv.)"
    // NOTE (Morten#1#): Keep this in sync with files in the XRC file (settings.xrc) and parser.cpp
    XRCCTRL(*this, "txtCCFileExtHeader",       wxTextCtrl)->SetValue(cfg->Read(_T("/header_ext"),    _T("h,hpp,hxx,hh,h++,tcc,xpm")));
    XRCCTRL(*this, "chkCCFileExtEmpty",        wxCheckBox)->SetValue(cfg->ReadBool(_T("/empty_ext"), true));
    XRCCTRL(*this, "txtCCFileExtSource",       wxTextCtrl)->SetValue(cfg->Read(_T("/source_ext"),    _T("c,cpp,cxx,cc,c++")));

    // Page "Symbol browser"
    XRCCTRL(*this, "chkNoSB",        wxCheckBox)->SetValue(!cfg->ReadBool(_T("/use_symbols_browser"), true));
    XRCCTRL(*this, "chkFloatCB",     wxCheckBox)->SetValue(cfg->ReadBool(_T("/as_floating_window"), false));

    // The toolbar section
    wxCheckBox *scopeFilter = XRCCTRL(*this, "chkScopeFilter", wxCheckBox);
    scopeFilter->SetValue(cfg->ReadBool(_T("/scope_filter"), true));
    wxSpinCtrl *spinScopeLength = XRCCTRL(*this, "spnChoiceScopeLength", wxSpinCtrl);
    spinScopeLength->Enable(scopeFilter->GetValue());
    spinScopeLength->SetValue(cfg->ReadInt(_T("/toolbar_scope_length"), 280));
    XRCCTRL(*this, "spnChoiceFunctionLength", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/toolbar_function_length"), 660));

    // -----------------------------------------------------------------------
    // Handle all options that are being handled by m_Parser
    // -----------------------------------------------------------------------

    // Page "clangd_client"
    XRCCTRL(*this, "chkUseSmartSense",      wxCheckBox)->SetValue(!m_Parser.Options().useSmartSense);
    XRCCTRL(*this, "chkWhileTyping",        wxCheckBox)->SetValue(m_Parser.Options().whileTyping);

    // Page "C / C++ parser"
    XRCCTRL(*this, "chkLocals",             wxCheckBox)->SetValue(m_Parser.Options().followLocalIncludes);
    XRCCTRL(*this, "chkGlobals",            wxCheckBox)->SetValue(m_Parser.Options().followGlobalIncludes);
    XRCCTRL(*this, "chkPreprocessor",       wxCheckBox)->SetValue(m_Parser.Options().wantPreprocessor);
    XRCCTRL(*this, "chkComplexMacros",      wxCheckBox)->SetValue(m_Parser.Options().parseComplexMacros);
    XRCCTRL(*this, "chkPlatformCheck",      wxCheckBox)->SetValue(m_Parser.Options().platformCheck);
    XRCCTRL(*this, "chkLogClangdClient",    wxCheckBox)->SetValue(m_Parser.Options().logClangdClientCheck);
    XRCCTRL(*this, "chkLogClangdServer",    wxCheckBox)->SetValue(m_Parser.Options().logClangdServerCheck);
    XRCCTRL(*this, "chkLogPluginInfo",      wxCheckBox)->SetValue(m_Parser.Options().logPluginInfoCheck);
    XRCCTRL(*this, "chkLogPluginDebug",     wxCheckBox)->SetValue(m_Parser.Options().logPluginDebugCheck);
    XRCCTRL(*this, "chkLSPMsgsFocusOnSave", wxCheckBox)->SetValue(m_Parser.Options().lspMsgsFocusOnSaveCheck);
    XRCCTRL(*this, "chkLSPMsgsClearOnSave", wxCheckBox)->SetValue(m_Parser.Options().lspMsgsClearOnSaveCheck);
    XRCCTRL(*this, "txtMasterPath",         wxTextCtrl)->SetValue(m_Parser.Options().LLVM_MasterPath);  //(ph 2021/11/7)

    m_Old_LLVM_MasterPath = m_Parser.Options().LLVM_MasterPath; //save for onApply() check

    // FIXME (ph#): implement these unused hidden check boxes ?
    XRCCTRL(*this, "chkLocals",        wxCheckBox)->Hide(); //(ph 2021/11/9)
    XRCCTRL(*this, "chkGlobals",       wxCheckBox)->Hide();
    XRCCTRL(*this, "chkPreprocessor",  wxCheckBox)->Hide();
    XRCCTRL(*this, "chkComplexMacros", wxCheckBox)->Hide();

    // Page "Symbol browser"
    XRCCTRL(*this, "chkInheritance",        wxCheckBox)->SetValue(m_Parser.ClassBrowserOptions().showInheritance);
    // FIXME (ph#): Display inheritance is causin a loop //(ph 2022/05/31)
    XRCCTRL(*this, "chkInheritance",        wxCheckBox)->SetValue(false);
    XRCCTRL(*this, "chkExpandNS",           wxCheckBox)->SetValue(m_Parser.ClassBrowserOptions().expandNS);
    XRCCTRL(*this, "chkTreeMembers",        wxCheckBox)->SetValue(m_Parser.ClassBrowserOptions().treeMembers);

    // Page Documentation
    XRCCTRL(*this, "chkDocumentation",      wxCheckBox)->SetValue(m_Documentation->IsEnabled());

    ColourManager *colours = Manager::Get()->GetColourManager();
    XRCCTRL(*this, "btnDocBgColor",         wxButton)->SetBackgroundColour(colours->GetColour(wxT("cc_docs_back")));
    XRCCTRL(*this, "btnDocTextColor",       wxButton)->SetBackgroundColour(colours->GetColour(wxT("cc_docs_fore")));
    XRCCTRL(*this, "btnDocLinkColor",       wxButton)->SetBackgroundColour(colours->GetColour(wxT("cc_docs_link")));

//    m_Parser.ParseBuffer(g_SampleClasses, true);
//    m_Parser.BuildTree(*XRCCTRL(*this, "treeClasses", wxTreeCtrl));
}
// ----------------------------------------------------------------------------
CCOptionsDlg::~CCOptionsDlg()
// ----------------------------------------------------------------------------
{
}
// ----------------------------------------------------------------------------
void CCOptionsDlg::OnPageChanging()
// ----------------------------------------------------------------------------
{
    // plugin about to be shown
    // This event does NOT occur when others get the focus
    // so we never know if we've lost focused or not.
}
// ----------------------------------------------------------------------------
void CCOptionsDlg::OnApply()
// ----------------------------------------------------------------------------
{
    wxString activePageTitle;
    // Get the title of the currently active/focused configuration page
    wxWindow* pTopWindow = wxFindWindowByName(_("Configure editor"));
    if (not pTopWindow)
        pTopWindow = m_CodeCompletion->GetTopWxWindow();
    if (pTopWindow)
    {
        wxListbook* lb = XRCCTRL(*pTopWindow, "nbMain", wxListbook);
        wxWindow* page = lb ? lb->GetCurrentPage() : nullptr;
        int pageID = page ? lb->FindPage(page) : 0;
        activePageTitle = lb ? lb->GetPageText(pageID) : wxString();
    }

    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");

    // -----------------------------------------------------------------------
    // Handle all options that are being directly applied / written from UI:
    // -----------------------------------------------------------------------

    // Page "clangd_client"
    cfg->Write(_T("/semantic_keywords"),    (bool)!XRCCTRL(*this, "chkNoSemantic",         wxCheckBox)->GetValue());
    cfg->Write(_T("/use_SmartSense"),       (bool) XRCCTRL(*this, "chkUseSmartSense",      wxCheckBox)->GetValue());
    cfg->Write(_T("/while_typing"),         (bool) XRCCTRL(*this, "chkWhileTyping",        wxCheckBox)->GetValue());
    cfg->Write(_T("/auto_add_parentheses"), (bool) XRCCTRL(*this, "chkAutoAddParentheses", wxCheckBox)->GetValue());
    cfg->Write(_T("/detect_implementation"),(bool) XRCCTRL(*this, "chkDetectImpl",         wxCheckBox)->GetValue());
    cfg->Write(_T("/add_doxgen_comment"),   (bool) XRCCTRL(*this, "chkAddDoxgenComment",   wxCheckBox)->GetValue());
    cfg->Write(_T("/enable_headers"),       (bool) XRCCTRL(*this, "chkEnableHeaders",      wxCheckBox)->GetValue());
    cfg->Write(_T("/max_matches"),          (int)  XRCCTRL(*this, "spnMaxMatches",         wxSpinCtrl)->GetValue());
    cfg->Write(_T("/fillup_chars"),                XRCCTRL(*this, "txtFillupChars",        wxTextCtrl)->GetValue());
    cfg->Write(_T("/cc_delay"),             (int)  XRCCTRL(*this, "sldCCDelay",            wxSlider)->GetValue() * 100);
    cfg->Write(_T("/lexer_keywords_set1"),  (bool) XRCCTRL(*this, "chkKL_1",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set2"),  (bool) XRCCTRL(*this, "chkKL_2",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set3"),  (bool) XRCCTRL(*this, "chkKL_3",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set4"),  (bool) XRCCTRL(*this, "chkKL_4",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set5"),  (bool) XRCCTRL(*this, "chkKL_5",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set6"),  (bool) XRCCTRL(*this, "chkKL_6",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set7"),  (bool) XRCCTRL(*this, "chkKL_7",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set8"),  (bool) XRCCTRL(*this, "chkKL_8",               wxCheckBox)->GetValue());
    cfg->Write(_T("/lexer_keywords_set9"),  (bool) XRCCTRL(*this, "chkKL_9",               wxCheckBox)->GetValue());

    // Page "C / C++ parser"
    cfg->Write(_T("/parser_follow_local_includes"),  (bool) XRCCTRL(*this, "chkLocals",                wxCheckBox)->GetValue());
    cfg->Write(_T("/parser_follow_global_includes"), (bool) XRCCTRL(*this, "chkGlobals",               wxCheckBox)->GetValue());
    cfg->Write(_T("/want_preprocessor"),             (bool) XRCCTRL(*this, "chkPreprocessor",          wxCheckBox)->GetValue());
    cfg->Write(_T("/parse_complex_macros"),          (bool) XRCCTRL(*this, "chkComplexMacros",         wxCheckBox)->GetValue());
    cfg->Write(_T("/platform_check"),                (bool) XRCCTRL(*this, "chkPlatformCheck",         wxCheckBox)->GetValue());
    cfg->Write(_T("/logClangdClient_check"),         (bool) XRCCTRL(*this, "chkLogClangdClient",       wxCheckBox)->GetValue());
    cfg->Write(_T("/logClangdServer_check"),         (bool) XRCCTRL(*this, "chkLogClangdServer",       wxCheckBox)->GetValue());
    cfg->Write(_T("/logPluginInfo_check"),           (bool) XRCCTRL(*this, "chkLogPluginInfo",         wxCheckBox)->GetValue());
    cfg->Write(_T("/logPluginDebug_check"),          (bool) XRCCTRL(*this, "chkLogPluginDebug",        wxCheckBox)->GetValue());
    cfg->Write(_T("/lspMsgsFocusOnSave_check"),      (bool) XRCCTRL(*this, "chkLSPMsgsFocusOnSave",    wxCheckBox)->GetValue());
    cfg->Write(_T("/lspMsgsClearOnSave_check"),      (bool) XRCCTRL(*this, "chkLSPMsgsClearOnSave",    wxCheckBox)->GetValue());
    cfg->Write(_T("/LLVM_MasterPath"),                      XRCCTRL(*this, "txtMasterPath",            wxTextCtrl)->GetValue());

    cfg->Write(_T("/max_threads"),                   (int)  XRCCTRL(*this, "spnThreadsNum",            wxSpinCtrl)->GetValue());
    cfg->Write(_T("/max_parsers_while_compiling"),   (int)  XRCCTRL(*this, "spnParsersWhileCompiling", wxSpinCtrl)->GetValue());

    // Page "C / C++ parser (adv.)"
    cfg->Write(_T("/header_ext"),        XRCCTRL(*this, "txtCCFileExtHeader", wxTextCtrl)->GetValue());
    cfg->Write(_T("/empty_ext"),  (bool) XRCCTRL(*this, "chkCCFileExtEmpty",  wxCheckBox)->GetValue());
    cfg->Write(_T("/source_ext"),        XRCCTRL(*this, "txtCCFileExtSource", wxTextCtrl)->GetValue());

    // Page "Symbol browser"
    cfg->Write(_T("/use_symbols_browser"),      (bool)!XRCCTRL(*this, "chkNoSB",        wxCheckBox)->GetValue());
    cfg->Write(_T("/browser_show_inheritance"), (bool) XRCCTRL(*this, "chkInheritance", wxCheckBox)->GetValue());
    // // FIXME (ph#): Show Inheritance is causing a loop
    cfg->Write(_T("/browser_show_inheritance"), false);//(bool) XRCCTRL(*this, "chkInheritance", wxCheckBox)->GetValue());
    cfg->Write(_T("/browser_expand_ns"),        (bool) XRCCTRL(*this, "chkExpandNS",    wxCheckBox)->GetValue());
    cfg->Write(_T("/as_floating_window"),       (bool) XRCCTRL(*this, "chkFloatCB",     wxCheckBox)->GetValue());
    cfg->Write(_T("/browser_tree_members"),     (bool) XRCCTRL(*this, "chkTreeMembers", wxCheckBox)->GetValue());

    // The toolbar section
    cfg->Write(_T("/scope_filter"), (bool) XRCCTRL(*this, "chkScopeFilter", wxCheckBox)->GetValue());
    cfg->Write(_T("/toolbar_scope_length"), (int)XRCCTRL(*this, "spnChoiceScopeLength", wxSpinCtrl)->GetValue());
    cfg->Write(_T("/toolbar_function_length"), (int)XRCCTRL(*this, "spnChoiceFunctionLength", wxSpinCtrl)->GetValue());

    // Page "Documentation"
    cfg->Write(_T("/use_documentation_helper"), (bool) XRCCTRL(*this, "chkDocumentation", wxCheckBox)->GetValue());
    cfg->Write(_T("/documentation_helper_background_color"), (wxColour) XRCCTRL(*this, "btnDocBgColor",   wxButton)->GetBackgroundColour());
    cfg->Write(_T("/documentation_helper_text_color"),       (wxColour) XRCCTRL(*this, "btnDocTextColor", wxButton)->GetBackgroundColour());
    cfg->Write(_T("/documentation_helper_link_color"),       (wxColour) XRCCTRL(*this, "btnDocLinkColor", wxButton)->GetBackgroundColour());
    // -----------------------------------------------------------------------
    // Handle all options that are being read by m_Parser.ReadOptions():
    // -----------------------------------------------------------------------

    // Force parser to read its options that we write in the config
    // Also don't forget to update the Parser option according UI!
    m_Parser.ReadOptions();

    // Page "clangd_client"
    m_Parser.Options().useSmartSense = !XRCCTRL(*this, "chkUseSmartSense",    wxCheckBox)->GetValue();
    m_Parser.Options().whileTyping   =  XRCCTRL(*this, "chkWhileTyping",      wxCheckBox)->GetValue();

    // Page "C / C++ parser"
    m_Parser.Options().followLocalIncludes  = XRCCTRL(*this, "chkLocals",             wxCheckBox)->GetValue();
    m_Parser.Options().followGlobalIncludes = XRCCTRL(*this, "chkGlobals",            wxCheckBox)->GetValue();
    m_Parser.Options().wantPreprocessor     = XRCCTRL(*this, "chkPreprocessor",       wxCheckBox)->GetValue();
    m_Parser.Options().parseComplexMacros   = XRCCTRL(*this, "chkComplexMacros",      wxCheckBox)->GetValue();
    m_Parser.Options().platformCheck        = XRCCTRL(*this, "chkPlatformCheck",      wxCheckBox)->GetValue();
    m_Parser.Options().logClangdClientCheck = XRCCTRL(*this, "chkLogClangdClient",    wxCheckBox)->GetValue();
    m_Parser.Options().logClangdServerCheck = XRCCTRL(*this, "chkLogClangdServer",    wxCheckBox)->GetValue();
    m_Parser.Options().logPluginInfoCheck   = XRCCTRL(*this, "chkLogPluginInfo",      wxCheckBox)->GetValue();
    m_Parser.Options().logPluginDebugCheck  = XRCCTRL(*this, "chkLogPluginDebug",     wxCheckBox)->GetValue();
    m_Parser.Options().lspMsgsFocusOnSaveCheck = XRCCTRL(*this, "chkLSPMsgsFocusOnSave",  wxCheckBox)->GetValue();
    m_Parser.Options().lspMsgsClearOnSaveCheck = XRCCTRL(*this, "chkLSPMsgsClearOnSave",  wxCheckBox)->GetValue();
    m_Parser.Options().LLVM_MasterPath         = XRCCTRL(*this, "txtMasterPath",          wxTextCtrl)->GetValue();

    // The LLVM_MasterPath may contain macros like $(TARGET_COMPILER_DIR); so deprecate the following //(ph 2022/03/23)
    //if (wxFileExists(m_Parser.Options().LLVM_MasterPath))
    //    m_Parser.Options().LLVM_MasterPath  = XRCCTRL(*this, "txtMasterPath",         wxTextCtrl)->GetValue();
    //else {
    //    wxString msg; msg << _("The clangd path:\n") << m_Parser.Options().LLVM_MasterPath << _(" does not exist.");
    //    msg << _("\nCode completion will be inoperable.");
    //    if ((activePageTitle == "clangd_client") or (activePageTitle == _("clangd_client")) )
    //        cbMessageBox(msg, _("ERROR: Clangd client") );
    //    m_Parser.Options().LLVM_MasterPath = m_Old_LLVM_MasterPath;
    //}

    // Page "Symbol browser"
    m_Parser.ClassBrowserOptions().showInheritance = XRCCTRL(*this, "chkInheritance", wxCheckBox)->GetValue();
    // FIXME (ph#): Show inheritance is causing a loop (ph 2022/05/31)
    m_Parser.ClassBrowserOptions().showInheritance = false; //XRCCTRL(*this, "chkInheritance", wxCheckBox)->GetValue();

    m_Parser.ClassBrowserOptions().expandNS        = XRCCTRL(*this, "chkExpandNS",    wxCheckBox)->GetValue();
    m_Parser.ClassBrowserOptions().treeMembers     = XRCCTRL(*this, "chkTreeMembers", wxCheckBox)->GetValue();

    // Page "Documentation"
    m_Documentation->RereadOptions(cfg);

    m_Parser.Options().storeDocumentation    = XRCCTRL(*this, "chkDocumentation",  wxCheckBox)->GetValue();
    m_Documentation->SetEnabled(               XRCCTRL(*this, "chkDocumentation",  wxCheckBox)->GetValue() );

    ColourManager *colours = Manager::Get()->GetColourManager();
    wxColor colour = XRCCTRL(*this, "btnDocBgColor",   wxButton)->GetBackgroundColour();
    colours->SetColour(wxT("cc_docs_back"), colour);
    colour = XRCCTRL(*this, "btnDocTextColor",   wxButton)->GetBackgroundColour();
    colours->SetColour(wxT("cc_docs_text"), colour);
    colour = XRCCTRL(*this, "btnDocLinkColor",   wxButton)->GetBackgroundColour();
    colours->SetColour(wxT("cc_docs_link"), colour);

    // Now write the parser options and re-read them again to make sure they are up-to-date
    m_Parser.WriteOptions();
    m_ParseManager->RereadParserOptions();
    m_Documentation->WriteOptions(cfg);
    m_CodeCompletion->RereadOptions();

    // If a project is loaded and the clangd location changed, say something.
    if (Manager::Get()->GetProjectManager()->GetActiveProject()
        and (m_Old_LLVM_MasterPath != m_Parser.Options().LLVM_MasterPath) )
    {
        wxString msg = _("CodeBlocks needs to be restarted to accomodate the clangd location change.");
        if ((activePageTitle == "clangd_client") or (activePageTitle == _("clangd_client")) )
            cbMessageBox(msg, _("Settings changed"));
    }

}
// ----------------------------------------------------------------------------
void CCOptionsDlg::OnChooseColour(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    wxColourData data;
    wxWindow* sender = FindWindowById(event.GetId());
    data.SetColour(sender->GetBackgroundColour());

    wxColourDialog dlg(this, &data);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxColour colour = dlg.GetColourData().GetColour();
        sender->SetBackgroundColour(colour);
    }
}

// ----------------------------------------------------------------------------
void CCOptionsDlg::OnCCDelayScroll(cb_unused wxScrollEvent& event)
// ----------------------------------------------------------------------------
{
    UpdateCCDelayLabel();
}

// ----------------------------------------------------------------------------
void CCOptionsDlg::OnUpdateUI(cb_unused wxUpdateUIEvent& event)
// ----------------------------------------------------------------------------
{
    // ccmanager's config Settings/Editor/Code completion
    ConfigManager* ccmcfg = Manager::Get()->GetConfigManager(_T("ccmanager"));
    //-bool en = ccmcfg->ReadBool(_T("/code_completion"), false); //<==  CCManagers  main setting, NOT clangd_client's
    bool en = Manager::Get()->GetConfigManager("plugins")->ReadBool("/Clangd_Client",false);

    bool aap = XRCCTRL(*this, "chkAutoAddParentheses", wxCheckBox)->GetValue();

    // Page "clangd_client"
    XRCCTRL(*this, "chkUseSmartSense",              wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkWhileTyping",                wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkAutoAddParentheses",         wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkDetectImpl",                 wxCheckBox)->Enable(en && aap);
    XRCCTRL(*this, "chkAddDoxgenComment",           wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkEnableHeaders",              wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkNoSemantic",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "lblMaxMatches",                 wxStaticText)->Enable(en);
    XRCCTRL(*this, "spnMaxMatches",                 wxSpinCtrl)->Enable(en);
    XRCCTRL(*this, "lblFillupChars",                wxStaticText)->Enable(en);
    XRCCTRL(*this, "txtFillupChars",                wxTextCtrl)->Enable(en);
    XRCCTRL(*this, "sldCCDelay",                    wxSlider)->Enable(en);

    // keyword sets
    XRCCTRL(*this, "chkKL_1",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_2",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_3",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_4",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_5",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_6",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_7",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_8",                 wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkKL_9",                 wxCheckBox)->Enable(en);

    // Page "C / C++ parser"
    XRCCTRL(*this, "chkLocals",                     wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkGlobals",                    wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkPreprocessor",               wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkComplexMacros",              wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkPlatformCheck",              wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkLogClangdClient",            wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkLogClangdServer",            wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkLogPluginInfo",              wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkLogPluginDebug",             wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkLSPMsgsFocusOnSave",         wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkLSPMsgsClearOnSave",         wxCheckBox)->Enable(en);
    XRCCTRL(*this, "txtMasterPath",                 wxTextCtrl)->Enable(en);    //(ph 2021/11/7)

    // Page "C / C++ parser (adv.)"
    en = ccmcfg->ReadBool(_T("/clangd_client"), true);
    XRCCTRL(*this, "txtCCFileExtHeader",      wxTextCtrl)->Enable(en);
    XRCCTRL(*this, "chkCCFileExtEmpty",       wxCheckBox)->Enable(en);
    XRCCTRL(*this, "txtCCFileExtSource",      wxTextCtrl)->Enable(en);

    // Page "Symbol browser"
    en = !XRCCTRL(*this, "chkNoSB",           wxCheckBox)->GetValue();
    // FIXME (ph#): Show inhertance is causing aloop (ph 2022/05/31)
    //XRCCTRL(*this, "chkInheritance",          wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkInheritance",          wxCheckBox)->Enable(false);
    XRCCTRL(*this, "chkExpandNS",             wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkFloatCB",              wxCheckBox)->Enable(en);
    XRCCTRL(*this, "chkTreeMembers",          wxCheckBox)->Enable(en);

    // Toolbar section
    wxCheckBox *scopeFilter = XRCCTRL(*this, "chkScopeFilter", wxCheckBox);
    XRCCTRL(*this, "spnChoiceScopeLength", wxSpinCtrl)->Enable(scopeFilter->GetValue());

    // Page "Documentation"
    en = XRCCTRL(*this, "chkDocumentation",   wxCheckBox)->GetValue();
    XRCCTRL(*this, "btnDocBgColor",           wxButton)->Enable(en);
    XRCCTRL(*this, "btnDocTextColor",         wxButton)->Enable(en);
    XRCCTRL(*this, "btnDocLinkColor",         wxButton)->Enable(en);
}

// ----------------------------------------------------------------------------
void CCOptionsDlg::UpdateCCDelayLabel()
// ----------------------------------------------------------------------------
{
    int position = XRCCTRL(*this, "sldCCDelay", wxSlider)->GetValue();
    wxString lbl;
    if (position >= 10)
        lbl.Printf(_("%d.%d sec"), position / 10, position % 10);
    else
        lbl.Printf(_("%d ms"), position * 100);
    XRCCTRL(*this, "lblDelay", wxStaticText)->SetLabel(lbl);
}

// ----------------------------------------------------------------------------
bool CCOptionsDlg::ValidateReplacementToken(wxString& from, wxString& to)
// ----------------------------------------------------------------------------

{
    // cut off any leading / trailing spaces
    from.Trim(true).Trim(false);
    to.Trim(true).Trim(false);

    if (to.IsEmpty())
    {
        // Allow removing a token, but ask the user if that's OK.
        if (cbMessageBox( _("This setup will replace the token with an empty string.\n"
                            "This will *remove* the token and probably break CC for some cases.\n"
                            "Do you really want to *remove* that token?"),
                          _("Confirmation"),
                          wxICON_QUESTION | wxYES_NO ) == wxID_YES)
        {
            return true;
        }
    }
    else if (to.Contains(from))
    {
        cbMessageBox(_("Replacement token cannot contain search token.\n"
                       "This would cause an infinite loop otherwise."),
                     _("Error"), wxICON_ERROR);
        return false;
    }

    wxRegEx re(_T("[A-Za-z_]+[0-9]*[A-Za-z_]*"));
    if (!re.Matches(from))
    {
        cbMessageBox(_("Search token can only contain alphanumeric characters and underscores."),
                     _("Error"), wxICON_ERROR);
        return false;
    }
    if (!re.Matches(to))
    {
        // Allow replacing with special characters only if the user says it's ok.
        if (cbMessageBox( _("You are replacing a token with a string that contains\n"
                            "characters other than alphanumeric and underscores.\n"
                            "This could make parsing the file impossible.\n"
                            "Are you sure?"),
                          _("Confirmation"),
                          wxICON_QUESTION | wxYES_NO ) != wxID_YES)
        {
            return false;
        }
    }

    return true;
}
// ----------------------------------------------------------------------------
void CCOptionsDlg::OnClangd_AutoDetect(cb_unused wxCommandEvent& event)       //(ph 2021/11/8)
// ----------------------------------------------------------------------------
{
    // OnLLVM_AutoDetect clangd location
    // Locate folders for Clang and Clangd
    ClangLocator clangLocator;
    wxFileName fnClangdPath(clangLocator.Locate_ClangdDir(), clangdexe);         // clangd (note the 'd')
    if (not fnClangdPath.Exists())
        fnClangdPath.Clear();

    if (fnClangdPath.GetPath().empty())
    {
        // Try the obvious places
        fnClangdPath.Assign(LLVM_Dirmaybe, clangdexe);
        if (not fnClangdPath.Exists())
            fnClangdPath.Clear();
        // tha..tha..that's all folks!
    }
    if (fnClangdPath.GetPath().empty())
    {
        wxString msg; msg << __FUNCTION__ << _("() Could not find clangd installation.");
        cbMessageBox( msg, _("Auto detect Error"));
        return;
    }

    // Verify clangd version is above 12
    wxString clangdVersion = clangLocator.GetClangdVersion(fnClangdPath.GetFullPath());
    //eg., clangd version 13.0,0
    clangdVersion = clangdVersion.BeforeFirst('.').AfterLast(' ');
    int versionNum = std::stoi(clangdVersion.ToStdString());
    if (versionNum < 13)
    {
        cbMessageBox("clangd version must be 13 or newer.", "Error");
        return;
    }

    if (fnClangdPath.Exists() )
        m_Parser.Options().LLVM_MasterPath = fnClangdPath.GetFullPath();
    XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->SetValue(m_Parser.Options().LLVM_MasterPath);  //(ph 2021/11/8)
    return;
}
// ----------------------------------------------------------------------------
void CCOptionsDlg::OnFindDirClangd_Dlg(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    wxTextCtrl* obj = 0L;
    if (event.GetId() == XRCID("btnMasterPath"))
        obj = XRCCTRL(*this, "txtMasterPath", wxTextCtrl);

    if (!obj)
        return; // called from invalid caller

    wxFileDialog dlg(this,
                     _("Select clangd executable file"),
                    #if defined(__WXMAC__) || defined(__WXGTK__)
                        "/",                                // const wxString &  	defaultDir = wxEmptyString,
                        clangdexe,                          // const wxString &  	defaultFile = wxEmptyString,
                        _("All files (*)|*"),               // const wxString &  	wildcard = wxFileSelectorDefaultWildcardStr,
                    #else
                        "",                                 // const wxString &  	defaultDir = wxEmptyString,
                        clangdexe,                          // const wxString &  	defaultFile = wxEmptyString,
                        _("Executable files (*.exe)|*.exe"),// const wxString &  	wildcard = wxFileSelectorDefaultWildcardStr,
                    #endif
                    wxFD_OPEN | wxFD_FILE_MUST_EXIST | compatibility::wxHideReadonly
                    #if defined(__WXMAC__) && wxCHECK_VERSION(3,1,3)   // wxFD_SHOW_HIDDEN added in 3.1.3
                        | wxFD_SHOW_HIDDEN         // Needed to be able to go into /usr or other system directries!!!
                    #endif // defined(__WXMAC__) && wxCHECK_VERSION(3,1,3)
                    );

    dlg.SetFilterIndex(0);

    PlaceWindow(&dlg);
    if (dlg.ShowModal() != wxID_OK)
        return;

    //-wxChar dirSep = wxFILE_SEP_PATH;
    wxString fullPath = dlg.GetPath();
    wxFileName fname(fullPath);
    if (not fname.GetName().Contains("clangd")) //could be clangd-12.exe for example
    {
        wxString msg = _("Failed to select the clangd executable.");
        cbMessageBox(msg,_("ERROR"));
        fname.Clear();

    }

    // should check the version here and issue warning message
    if (fname.GetFullPath().Length())
    {
        ClangLocator clangdLocator;
        wxString versionID = clangdLocator.GetClangdVersionID(fname.GetFullPath());
        if (versionID.empty())
        {
            wxString msg(_("Warning: Could not detect the version of this executable.\nIs his really clangd?"));
            cbMessageBox(msg, _("Warning: clangd_client"));
        }
        int versionPrimaryID = std::stoi(versionID.BeforeFirst('.').ToStdString());
        if (versionPrimaryID < 13)
        {
            wxString msg(_("Warning: clangd version should be 13 or newer; else unexpected behavior occurs."));
            cbMessageBox(msg, _("Warning: clangd_client"));
        }
    }

    obj->SetValue(fname.GetFullPath());

} // OnSelectProgramClick
