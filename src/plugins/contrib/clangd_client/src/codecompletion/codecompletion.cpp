/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
+ * $Revision$
+ * $Id$
+ * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <algorithm>
    #include <iterator>
    #include <set> // for handling unique items in some places
    #include "assert.h"

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

#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/html/htmlwin.h>
#include <wx/app.h>             // wxWakeUpIdle()
#include <wx/display.h>
#include <wx/stdpaths.h>

#include <globals.h>
#include "cbworkspace.h"
#include <cbstyledtextctrl.h>
#include <editor_hooks.h>
#include <filegroupsandmasks.h>
#include <multiselectdlg.h>

#include "codecompletion.h"
#include <annoyingdialog.h>
#include <filefilters.h>
//-#include "compilerfactory.h"

#include "Version.h"

#include "cbexception.h"

#include "ccoptionsdlg.h"
#include "ccoptionsprjdlg.h"
#include "insertclassmethoddlg.h"
#include "selectincludefile.h"
#include "parser/ccdebuginfo.h"
#include "parser/cclogger.h"
#include "parser/parser.h"
//-#include "parser/tokenizer.h"
#include "doxygen_parser.h" // for DocumentationPopup and DoxygenParser
#include "gotofunctiondlg.h"
#include <searchresultslog.h>        // LSP references event
#include <encodingdetector.h>
#include "infowindow.h"
#include "lspdiagresultslog.h"       // LSP
#include "ccmanager.h"
#include "../ClangLocator.h"

#if defined(_WIN32)
#include "winprocess/asyncprocess/procutils.h" // LSP
#else
#include "procutils.h"
#endif //_Win32


#include "LSPEventCallbackHandler.h"

#define CC_CODECOMPLETION_DEBUG_OUTPUT 0
//#define CC_CODECOMPLETION_DEBUG_OUTPUT 1

// let the global debug macro overwrite the local debug macro value
#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #undef CC_CODECOMPLETION_DEBUG_OUTPUT
    #define CC_CODECOMPLETION_DEBUG_OUTPUT CC_GLOBAL_DEBUG_OUTPUT
#endif

#if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...)
#elif CC_CODECOMPLETION_DEBUG_OUTPUT == 2
    #define TRACE(format, args...)                                              \
        do                                                                      \
        {                                                                       \
            if (g_EnableDebugTrace)                                             \
                CCLogger::Get()->DebugLog(wxString::Format(format, ##args));                   \
        }                                                                       \
        while (false)
    #define TRACE2(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
#else
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif

/// Scopes choice name for global functions in CC's toolbar.
static wxString g_GlobalScope(_T("<global>"));

// this auto-registers the plugin
// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{
    // this auto-registers the plugin
    PluginRegistrant<ClgdCompletion> reg(_T("clangd_client"));

    const char STX = '\u0002';
    const wxString STXstring = "\u0002";

    // LSP_Symbol identifiers
    // #include "../LSP_SymbolKind.h" //clangd symbol definition are here

    wxString sep = wxString(wxFileName::GetPathSeparator());
    bool wxFound(int result){return result != wxNOT_FOUND;}
    bool shutTFU = wxFound(0); //shutup 'not used' compiler err msg when wxFound not used

    #if defined(_WIN32) //Thanks Andrew
        wxString clangdexe("clangd.exe");
        wxString oldCC_PluginLibName("codecompletion" + FileFilters::DYNAMICLIB_DOT_EXT);
    #else
        wxString clangdexe("clangd");
        wxString oldCC_PluginLibName("libcodecompletion" + FileFilters::DYNAMICLIB_DOT_EXT);
    #endif

    wxString ns_DefaultCompilerMasterPath = wxString(); //Default compiler master path

}
// ----------------------------------------------------------------------------
namespace CodeCompletionHelper
// ----------------------------------------------------------------------------
{
    // compare method for the sort algorithm for our FunctionScope struct
    inline bool LessFunctionScope(const ClgdCompletion::FunctionScope& fs1, const ClgdCompletion::FunctionScope& fs2)
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

    inline bool EqualFunctionScope(const ClgdCompletion::FunctionScope& fs1, const ClgdCompletion::FunctionScope& fs2)
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
            const wxRegEx reg(_T("^[ \t]*#[ \t]*include[ \t]+[\"<]([^\">]+)[\">]"));
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

}//end namespace CodeCompletionHelper

// ----------------------------------------------------------------------------
// menu IDs
// ----------------------------------------------------------------------------
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
int idEditorFileReparse         = wxNewId();
int idEditorSubMenu             = wxNewId();
int idClassMethod               = wxNewId();
int idUnimplementedClassMethods = wxNewId();
int idGotoDeclaration           = wxNewId();
int idGotoImplementation        = wxNewId();
int idOpenIncludeFile           = wxNewId();

int idRealtimeParsingTimer      = wxNewId();
int idToolbarTimer              = XRCID("idToolbarTimer");
int idProjectSavedTimer         = wxNewId();
int idReparsingTimer            = wxNewId();
int idEditorActivatedTimer      = wxNewId();
int LSPeventID                  = wxNewId();
int idPauseParsing              = wxNewId();
int idProjectPauseParsing       = wxNewId();
int idStartupDelayTimer         = wxNewId();

int idSpecifiedFileReparse      = XRCID("idSpecifiedFileReparse");

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

// ----------------------------------------------------------------------------
// Event table
// ----------------------------------------------------------------------------
BEGIN_EVENT_TABLE(ClgdCompletion, cbCodeCompletionPlugin)
    EVT_UPDATE_UI_RANGE(idMenuGotoFunction, idCurrentProjectReparse, ClgdCompletion::OnUpdateUI)

    EVT_MENU(idMenuGotoFunction,                   ClgdCompletion::OnGotoFunction             )
    EVT_MENU(idMenuGotoPrevFunction,               ClgdCompletion::OnGotoPrevFunction         )
    EVT_MENU(idMenuGotoNextFunction,               ClgdCompletion::OnGotoNextFunction         )
    EVT_MENU(idMenuGotoDeclaration,                ClgdCompletion::OnGotoDeclaration          )
    EVT_MENU(idMenuGotoImplementation,             ClgdCompletion::OnGotoDeclaration          )
    EVT_MENU(idMenuFindReferences,                 ClgdCompletion::OnFindReferences           )
    EVT_MENU(idMenuRenameSymbols,                  ClgdCompletion::OnRenameSymbols            )
    EVT_MENU(idClassMethod,                        ClgdCompletion::OnClassMethod              )
    EVT_MENU(idUnimplementedClassMethods,          ClgdCompletion::OnUnimplementedClassMethods)
    EVT_MENU(idGotoDeclaration,                    ClgdCompletion::OnGotoDeclaration          )
    EVT_MENU(idGotoImplementation,                 ClgdCompletion::OnGotoDeclaration          )
    EVT_MENU(idOpenIncludeFile,                    ClgdCompletion::OnOpenIncludeFile          )
    EVT_MENU(idMenuOpenIncludeFile,                ClgdCompletion::OnOpenIncludeFile          )

    EVT_MENU(idViewClassBrowser,                   ClgdCompletion::OnViewClassBrowser      )
    EVT_MENU(idCurrentProjectReparse,              ClgdCompletion::OnCurrentProjectReparse )
    EVT_MENU(idSelectedProjectReparse,             ClgdCompletion::OnReparseSelectedProject)
    EVT_MENU(idSelectedFileReparse,                ClgdCompletion::OnSelectedFileReparse   )
    EVT_MENU(idSpecifiedFileReparse,               ClgdCompletion::OnSpecifiedFileReparse  )
    EVT_MENU(idEditorFileReparse,                  ClgdCompletion::OnActiveEditorFileReparse   )
    EVT_MENU(idPauseParsing,                       ClgdCompletion::OnSelectedPauseParsing )
    EVT_MENU(idProjectPauseParsing,                ClgdCompletion::OnProjectPauseParsing )

    // CC's toolbar
    EVT_CHOICE(XRCID("chcCodeCompletionScope"),    ClgdCompletion::OnScope   )
    EVT_CHOICE(XRCID("chcCodeCompletionFunction"), ClgdCompletion::OnFunction)

    EVT_IDLE(                                      ClgdCompletion::OnIdle)
    //-EVT_ACTIVATE(                                  CodeCompletion::OnActivated) does not work for dialogs
    //-EVT_MENU(XRCID("idLSP_Process_Terminated"),    CodeCompletion::OnLSP_ProcessTerminated )

END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// static members
// ----------------------------------------------------------------------------
bool ClgdCompletion::m_CCHasTreeLock; // static member
std::vector<ClgdCCToken> ClgdCompletion::m_CompletionTokens; // cached tokens

class cbStyledTextCtrl; // Forward declaration
static int CalcStcFontSize(cbStyledTextCtrl *stc); // Forward declaration
// ----------------------------------------------------------------------------
ClgdCompletion::ClgdCompletion() :
// ----------------------------------------------------------------------------
    m_InitDone(false),
    m_pCodeRefactoring(nullptr), //re-initialized in ctor
    m_EditorHookId(0),
    //m_TimerRealtimeParsing(this, idRealtimeParsingTimer),
    m_TimerToolbar(this, idToolbarTimer),
    m_TimerEditorActivated(this, idEditorActivatedTimer),
    m_LastEditor(0),
    m_TimerStartupDelay(this, idStartupDelayTimer),
    m_ToolBar(0),
    m_Function(0),
    m_Scope(0),
    m_ToolbarNeedRefresh(true),
    m_ToolbarNeedReparse(false),
    m_CurrentLine(0),
    //m_NeedReparse(false),
    m_CurrentLength(-1),
    m_NeedsBatchColour(true),
    m_CCMaxMatches(256),
    m_CCAutoAddParentheses(true),
    m_CCDetectImplementation(false),
    m_CCDelay(300),
    m_CCEnableHeaders(false),
    m_CCEnablePlatformCheck(true)
    //m_DocHelper(GetParseManager())
{
    m_CCHasTreeLock = false;
    m_CC_initDeferred = true; //reset to false if code reaches bottom of ctor

    // Trap calls to OnAttach() to see if legacy CodeCompletion is actually enabled/attached
    // when there's no info in .conf for a plugin CB says it's disabled but runs it anyway.
    Manager::Get()->RegisterEventSink(cbEVT_PLUGIN_ATTACHED,      new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnPluginAttached));

    bool clgdEnabled = Manager::Get()->GetConfigManager(_T("plugins"))->ReadBool("/clangd_client");
	if (not clgdEnabled)
    {
        // Make sure the .conf has a specified 'disabled' status. Not just a missing status.
        // Else OnAttach() will get unwelcomely screwed since CB runs
        // plugins without .conf info as if they were enabled but says they're disabled.
        SetClangdClient_Disabled();
        return;
    }

    // If no compiler or no master path, clangd_client cannot work.
    ConfigManager* pCfgMgr = Manager::Get()->GetConfigManager("compiler");
    wxString defaultCompiler = pCfgMgr->Read("/default_compiler");
    ns_DefaultCompilerMasterPath = defaultCompiler.Length() ? pCfgMgr->Read("/sets/"+defaultCompiler+"/master_path") : wxString();
    if (ns_DefaultCompilerMasterPath.empty())
    {
        SetClangdClient_Disabled();
        return;
    }

    m_OldCC_enabled = IsOldCCEnabled();
    if (m_OldCC_enabled)
    {
        //Clangd_client must not run when legacy CodeCompletion is enabled and Dll/Lib is loaded.
        SetClangdClient_Disabled();

        // legacy CodeCompletion is loaded and running
        wxString msg = _("The Clangd client plugin cannot run while the \"Code completion\" plugin is enabled.\n"
                         "The Clangd client plugin will now inactivate itself. :-(\n\n"
                         "If you wish to use the Clangd_client rather than the legacy CodeCompletion plugin,\n"
                         "navigate to Plugins->Manage plugins... and disable CodeCompletion, then enable Clangd_client.\n\n"
                         "RESTART CodeBlocks after closing the \"Manage plugins\" dialog.");
        msg << "\n\n-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.\n";
        msg << _("Only one (Clangd_Client or CodeCompletion) should be enabled.");
        wxWindow* pTopWindow = GetTopWxWindow();
        //avoid later assert in PluginManager if user changes selection in parent window
        pTopWindow->Freeze();
        cbMessageBox(msg, _("Clangd_client plugin"), wxOK, pTopWindow);
        pTopWindow->Thaw();

        return;
    }

    // Read the initial enabled condition for Clangd_client; defaults to false
    //  even when CodeCompletion is actually enabled but when no info in .conf file.
    m_ctorClientStartupStatusEnabled = Manager::Get()->GetConfigManager("plugins")->ReadBool("/Clangd_Client");

    // get top window to use as cbMessage parent, else message boxes will hide behind dialog and main window
    wxWindow* topWindow = wxFindWindowByName(_("Manage plugins"));
    if (not topWindow) topWindow = Manager::Get()->GetAppWindow();

    // create Idle time CallbackHandler
    LSPEventCallbackHandler* pNewLSPEventSinkHandler = new LSPEventCallbackHandler();
    pLSPEventSinkHandler.reset( pNewLSPEventSinkHandler);

    // ParseManager / CodeRefactoring creation
    m_pParseManager.reset( new ParseManager(pNewLSPEventSinkHandler) );
    GetParseManager()->SetClientEventHandler(this);
    m_pCodeRefactoring = new CodeRefactoring(m_pParseManager.get());
    m_pDocHelper = new DocumentationHelper(m_pParseManager.get());

    // CCLogger are the log event bridges, those events were finally handled by its parent, here
    // it is the CodeCompletion plugin ifself.
    CCLogger::Get()->Init(this, g_idCCLogger, g_idCCErrorLogger, g_idCCDebugLogger, g_idCCDebugErrorLogger);

    if (!Manager::LoadResource(_T("clangd_client.zip")))
        NotifyMissingFile(_T("clangd_client.zip"));

    // handling events send from CCLogger
    Connect(g_idCCLogger,                wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCLogger)     );
    Connect(g_idCCErrorLogger,           wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCLogger)     );
    Connect(g_idCCDebugLogger,           wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCDebugLogger));
    Connect(g_idCCDebugErrorLogger,      wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCDebugLogger));

    Connect(idToolbarTimer,         wxEVT_TIMER, wxTimerEventHandler(ClgdCompletion::OnToolbarTimer) );
    Connect(XRCID("idToolbarTimer"),wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ClgdCompletion::InvokeToolbarTimer));
    //-Connect(idEditorActivatedTimer, wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnEditorActivatedTimer));
    Connect(idStartupDelayTimer, wxEVT_TIMER, wxTimerEventHandler(ClgdCompletion::DoParseOpenedProjectAndActiveEditor));

    Connect(XRCID("idLSP_Process_Terminated"), wxEVT_COMMAND_MENU_SELECTED,
            wxCommandEventHandler(ClgdCompletion::OnLSP_ProcessTerminated));

    Bind(wxEVT_COMMAND_MENU_SELECTED, &ClgdCompletion::OnLSP_Event, this, LSPeventID);
    Bind(wxEVT_COMMAND_MENU_SELECTED, &ClgdCompletion::OnReActivateProject, this,XRCID("OnReActivateProject"));
    Bind(wxEVT_COMMAND_MENU_SELECTED, &ClgdCompletion::OnRequestCodeActionApply, this, XRCID("idRequestCodeActionApply"));

    // Disable legacy CodeCompletion plugin for safety (to avoid conflict crashes)
    // Note that if there's no plugin entry in the .conf, a plugin gets loaded and run
    // even though ConfigManager states it's not enabled. OnAttach() gets called anyway.
    // Since clangd_client is about to run, make sure legacy CodeCompletion cannot.
    Manager::Get()->GetConfigManager(_T("plugins"))->Write(_T("/codecompletion"), false );

    // Allow clangd_client to initialize
    m_CC_initDeferred = false;
}
// ----------------------------------------------------------------------------
ClgdCompletion::~ClgdCompletion()
// ----------------------------------------------------------------------------
{
    if (m_CC_initDeferred) return;

    Disconnect(g_idCCLogger,                wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCLogger));
    Disconnect(g_idCCErrorLogger,           wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCLogger));
    Disconnect(g_idCCDebugLogger,           wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCDebugLogger));
    Disconnect(g_idCCDebugErrorLogger,      wxEVT_COMMAND_MENU_SELECTED, CodeBlocksThreadEventHandler(ClgdCompletion::OnCCDebugLogger));

    Disconnect(idToolbarTimer,         wxEVT_TIMER, wxTimerEventHandler(ClgdCompletion::OnToolbarTimer)        );
    Disconnect(idToolbarTimer,         wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ClgdCompletion::InvokeToolbarTimer));
    //-Disconnect(idEditorActivatedTimer, wxEVT_TIMER, wxTimerEventHandler(CodeCompletion::OnEditorActivatedTimer));
    Disconnect(idStartupDelayTimer,    wxEVT_TIMER, wxTimerEventHandler(ClgdCompletion::DoParseOpenedProjectAndActiveEditor));

    Disconnect(XRCID("idLSP_Process_Terminated"), wxEVT_COMMAND_MENU_SELECTED,
            wxCommandEventHandler(ClgdCompletion::OnLSP_ProcessTerminated));

    Unbind(wxEVT_COMMAND_MENU_SELECTED, &ClgdCompletion::OnLSP_Event, this, LSPeventID);
    Unbind(wxEVT_COMMAND_MENU_SELECTED, &ClgdCompletion::OnReActivateProject, this,XRCID("OnReActivateProject"));

}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnAttach()
// ----------------------------------------------------------------------------
{
    AppVersion appVersion;
    appVersion.m_AppName = "clangd_client";
    // Set current plugin version
	PluginInfo* pInfo = (PluginInfo*)(Manager::Get()->GetPluginManager()->GetPluginInfo(this));
	pInfo->version = appVersion.GetVersion();

	// Re-Init CCLogger now that we can get PluginInfo
    // CCLogger are the log event bridges, those events were finally handled by its parent, here
    // it is the CodeCompletion plugin ifself.
    CCLogger::Get()->Init(this, g_idCCLogger, g_idCCErrorLogger, g_idCCDebugLogger, g_idCCDebugErrorLogger);


	// clangd_client cannot work without a compiler master path
    ConfigManager* pCfgMgr = Manager::Get()->GetConfigManager("compiler");
    wxString defaultCompiler = pCfgMgr->Read("/default_compiler");
    ns_DefaultCompilerMasterPath = defaultCompiler.Length() ? pCfgMgr->Read("/sets/"+defaultCompiler+"/master_path") : wxString();
    if (ns_DefaultCompilerMasterPath.empty())
    {
        // Show plugin version as "Inactive".
        // Can't disable the plugin. CB will just re-enable it on return to plugin manager.
        // But we can cripple the code and show it as inactive.
        if (not pInfo->version.Contains(" Inactive"))
            pInfo->version = appVersion.GetVersion().Append(" Inactive");
        InfoWindow::Display(_("Clangd_client is inactive"),
            _("To run clangd_client, restart CodeBlocks after\n setting a compiler and disabling CodeCompletion plugin"), 10000);
        return;
    }

    // clangd_client cannot run if legacy CodeCompletion plugin is able to run
    m_OldCC_enabled = IsOldCCEnabled();

    if (m_OldCC_enabled)
    {
        // legacy CodeCompletion is loaded and running

        //SetClangdClient_Disabled(); //<< This won't work here. PluginManger will enable on return anyway.

        wxString msg = _("The Clangd client plugin cannot run while the \"Code completion\" plugin is enabled.\n"
                         "The Clangd client plugin will now inactivate itself. :-(\n\n"
                         "If you wish to use the Clangd_client rather than the legacy CodeCompletion plugin,\n"
                         "navigate to Plugins->Manage plugins... and disable CodeCompletion, then enable Clangd_client.\n\n"
                         "RESTART CodeBlocks after closing the \"Manage plugins\" dialog.");
        msg << "\n\n-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.\n";
        msg << _("Only one (Clangd_Client or CodeCompletion) should be enabled.");
        wxWindow* pTopWindow = GetTopWxWindow();
        //avoid later assert in PluginManager if user changes selection in parent window
        pTopWindow->Freeze();
        cbMessageBox(msg, _("Clangd_client plugin"), wxOK, pTopWindow);
        pTopWindow->Thaw();

        pTopWindow = Manager::Get()->GetAppWindow();
        if (pTopWindow)
        {
            // Disable ourself from the application's event handling chain...
            // which cbPlugin.cpp just placed before calling OnAttach()
            if (GetParseManager()->FindEventHandler(this))
                GetParseManager()->FindEventHandler(this)->SetEvtHandlerEnabled(false);
        }
        pInfo->version = appVersion.GetVersion().BeforeFirst(' ') + " Inactive";
        return;
    }//endif Old CC is running

    // If oldCC is enabled but the dll is missing, behave like oldCC is disabled.
    // Restart CB when ctor Init was defered or when we just enabled it.
    if ( ((not m_OldCC_enabled ) and m_CC_initDeferred) /* or (not m_ClgdClientEnabled)*/)
    {
        // Old CC is disabled but we haven't initialize ourself.
        cbMessageBox(_("Clangd_Client plugin needs you to RESTART codeblocks before it can function properly."),
                        _("CB restart needed"), wxOK, GetTopWxWindow());
        wxWindow* appWindow = Manager::Get()->GetAppWindow();
        if (appWindow)
        {
            // Disable ourself from the application's event handling chain...
            // which cbPlugin.cpp just placed before this call
            // Else an attempt to re-enable this plugin will cause a hang.
            if (GetParseManager()->FindEventHandler(this))
                GetParseManager()->FindEventHandler(this)->SetEvtHandlerEnabled(false);
        }
        return;
    }

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
    cbAssert(GetParseManager() != nullptr);
    GetParseManager()->SetNextHandler(this);

    GetParseManager()->CreateClassBrowser();

    // hook to editors
    // both ccmanager and cc have hooks, but they don't conflict. ccmanager are mainly
    // hooking to the event such as key stroke or mouse dwell events, so the code completion, call tip
    // and tool tip will be handled in ccmanager. The other cases such as caret movement triggers
    // updating the CC's toolbar, modifying the editor causing the real time content reparse will be
    // handled inside cc's own editor hook.
    EditorHooks::HookFunctorBase* myhook = new EditorHooks::HookFunctor<ClgdCompletion>(this, &ClgdCompletion::EditorEventHook);
    m_EditorHookId = EditorHooks::RegisterHook(myhook);

    // register event sinks
    Manager* pm = Manager::Get();

    pm->RegisterEventSink(cbEVT_APP_STARTUP_DONE,     new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnAppStartupDone));

    pm->RegisterEventSink(cbEVT_WORKSPACE_CHANGED,    new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnWorkspaceChanged));
    pm->RegisterEventSink(cbEVT_WORKSPACE_CLOSING_BEGIN,new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnWorkspaceClosingBegin));
    pm->RegisterEventSink(cbEVT_WORKSPACE_CLOSING_COMPLETE,new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnWorkspaceClosingEnd));

    pm->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,     new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnProjectActivated));
    pm->RegisterEventSink(cbEVT_PROJECT_CLOSE,        new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnProjectClosed));
    pm->RegisterEventSink(cbEVT_PROJECT_OPEN,         new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnProjectOpened));
    pm->RegisterEventSink(cbEVT_PROJECT_SAVE,         new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnProjectSaved));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_ADDED,   new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnProjectFileAdded));
    pm->RegisterEventSink(cbEVT_PROJECT_FILE_REMOVED, new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnProjectFileRemoved));

    pm->RegisterEventSink(cbEVT_EDITOR_SAVE,          new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnEditorSave));
    pm->RegisterEventSink(cbEVT_EDITOR_OPEN,          new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnEditorOpen));
    pm->RegisterEventSink(cbEVT_EDITOR_ACTIVATED,     new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnEditorActivated));
    pm->RegisterEventSink(cbEVT_EDITOR_CLOSE,         new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnEditorClosed));

    pm->RegisterEventSink(cbEVT_DEBUGGER_STARTED,     new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnDebuggerStarting));
    pm->RegisterEventSink(cbEVT_DEBUGGER_FINISHED,    new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnDebuggerFinished));

    pm->RegisterEventSink(cbEVT_PLUGIN_ATTACHED,      new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnPluginAttached));
    pm->RegisterEventSink(cbEVT_COMPILER_STARTED,     new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnCompilerStarted));
    pm->RegisterEventSink(cbEVT_COMPILER_FINISHED,    new cbEventFunctor<ClgdCompletion, CodeBlocksEvent>(this, &ClgdCompletion::OnCompilerFinished));

    m_pDocHelper->OnAttach();
}
// ----------------------------------------------------------------------------
bool ClgdCompletion::CanDetach() const
// ----------------------------------------------------------------------------
{
    if (m_CC_initDeferred) return true;

    wxWindow* pTopWindow = wxFindWindowByName(_("Manage plugins"));
    if (not pTopWindow) pTopWindow = ((ClgdCompletion*)this)->GetTopWxWindow();
    int prjCount = Manager::Get()->GetProjectManager()->GetProjects()->GetCount();
    if (prjCount)
    {
        wxString msg = _("Please close the workspace before disabling or uninstalling clangd_client plugin.");
        cbMessageBox(msg, _("Uninstall") , wxOK, pTopWindow);
        return false;
    }
    return true;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnRelease(bool appShutDown)
// ----------------------------------------------------------------------------
{
    // FYI:
    // A crash occurs when user uninstalls this plugin while a project is loaded.
    // It happends in pluginmanager.cpp at "PluginManager::Configure()" exit.
    // If a project is NOT loaded, the crash does not occur.
    // Closing the workspace in this function does not help the issue.
    // Reinstalling the missing support code for "virtual bool CanDetach() const { return true; }"
    // into the sdk allows us to ask the user to close the project before uninstalling
    // this plugin.
    // Update: svn rev 12640 restored the missing CanDetach() query code.

    m_InitDone = false;

    bool oldCCEnabled = IsOldCCEnabled();
    bool clgdEnabled = Manager::Get()->GetConfigManager(_T("plugins"))->ReadBool("/clangd_client");
    if (oldCCEnabled and clgdEnabled)
        SetClangdClient_Disabled();

    // If plugin initialization never took place, nothing needs to be done here.
    if (m_CC_initDeferred) return;

    GetParseManager()->SetPluginIsShuttingDown();

    GetParseManager()->RemoveClassBrowser(appShutDown);
    GetParseManager()->ClearParsers();

    // remove chained handler
    GetParseManager()->SetNextHandler(nullptr);

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

    m_pDocHelper->OnRelease();

    GetParseManager()->CloseAllClients();

    // Remove the Proxy Project's compile_commands.json clangd database (if there is one)
    wxString userDataFolder = ConfigManager::GetConfigFolder();
    wxString compileCommandsFilename = userDataFolder + "/compile_commands.json";
    if (wxFileExists(compileCommandsFilename))
    {
        //We don't want to hear about another CB having the file open
        // or that it doesn't exist. Thus the nullLog.
        wxLogNull nullLog;
        wxRemoveFile(compileCommandsFilename);
    }

    // If the plugins is being disabled only (ie., CB is NOT shutting down)
    // emit a warning that CB should be reloaded to clear out resouces that may
    // conflict with enabling legacy CodeCompletion.
    if (not appShutDown)
    {
        wxString msg = _("You should RESTART Code::Blocks to remove Clangd_Client resources\n"
                         "  if you intend to re-enable the legacy CodeCompletion plugin.");

        //wxWindow* pTopWindow = GetTopWxWindow();
        wxWindow* pManageWindow = wxFindWindowByName(_("Manage plugins"));
        if(pManageWindow)
        {
            // Use ShowWindowModal() to avoid Linux shifting this dlg behind current window
            wxMessageDialog dlg(pManageWindow,
                msg,
                _("RESTART required"),
                wxOK|wxSTAY_ON_TOP|wxCAPTION|wxCENTRE);
            PlaceWindow(&dlg);
            #ifdef __WXGTK__
                // For Linux Use ShowWindowModal() instead of ShowModal()
                // otherwise this message can hide behind the "Manage plugins window"
                // when using "focus follows mouse"
            dlg.ShowWindowModal();
            #else
            dlg.ShowModal();
            #endif
        }
    }//endif appShutDown
}//endif OnRelease
// ----------------------------------------------------------------------------
cbConfigurationPanel* ClgdCompletion::GetConfigurationPanel(wxWindow* parent)
// ----------------------------------------------------------------------------
{
    if (m_CC_initDeferred) return nullptr;
    if ( not IsAttached()) return nullptr;
    return new CCOptionsDlg(parent, GetParseManager(), this, m_pDocHelper);
}
// ----------------------------------------------------------------------------
cbConfigurationPanel* ClgdCompletion::GetProjectConfigurationPanel(wxWindow* parent, cbProject* project)
// ----------------------------------------------------------------------------
{
    return new CCOptionsProjectDlg(parent, project, GetParseManager());
}
// ----------------------------------------------------------------------------
void ClgdCompletion::BuildMenu(wxMenuBar* menuBar)
// ----------------------------------------------------------------------------
{
    // if not attached, exit
    if ((not IsAttached()) or m_CC_initDeferred)
    {
        SetEvtHandlerEnabled(false);
        return;
    }


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
// ----------------------------------------------------------------------------
void ClgdCompletion::BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
// ----------------------------------------------------------------------------
{
    // if not attached, exit
    if (!menu || !IsAttached() || (not m_InitDone) )
        return;
    if (m_CC_initDeferred) return;

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();

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
            else if (pEditor and GetLSP_Initialized(pEditor) )  // Fix crash when client shutdown
            {
                int initialPos = pluginManager->GetFindMenuItemFirst();
                int pos = initialPos;
                wxString msg;
                msg.Printf(_("Find declaration of: '%s'"), NameUnderCursor.wx_str());
                menu->Insert(pos++, idGotoDeclaration, msg);

                msg.Printf(_("Find implementation of: '%s'"), NameUnderCursor.wx_str());
                menu->Insert(pos++, idGotoImplementation, msg);

                if (pEditor and GetLSPClient(pEditor) and GetParseManager()->GetParser().Done())     // This fixes crash when client is shutdown.
                {
                    msg.Printf(_("Find references of: '%s'"), NameUnderCursor.wx_str());
                    menu->Insert(pos++, idMenuFindReferences, msg);
                }
                else if ( pEditor and GetLSPClient(pEditor)
                            and GetLSP_IsEditorParsed(pEditor) )
                 {
                     msg.Printf(_("Find references of: '%s'"), NameUnderCursor.wx_str());
                     menu->Insert(pos++, idMenuFindReferences, msg);
                 }

                pluginManager->RegisterFindMenuItems(false, pos - initialPos);
            }
        }

        const int insertId = menu->FindItem(_("Insert/Refactor"));
        if ( wxFound(insertId) and pEditor and GetLSP_Initialized(pEditor) )
        {
            if (wxMenuItem* insertMenu = menu->FindItem(insertId, 0))
            {
                if (wxMenu* subMenu = insertMenu->GetSubMenu())
                {
                    subMenu->Append(idClassMethod, _("Class method declaration/implementation..."));
                    subMenu->Append(idUnimplementedClassMethods, _("All class methods without implementation..."));

                    subMenu->AppendSeparator();

                    const bool enableRename = (GetParseManager()->GetParser().Done() && nameUnderCursor && !IsInclude);
                    subMenu->Append(idMenuRenameSymbols, _("Rename symbols"), _("Rename symbols under cursor"));
                    subMenu->Enable(idMenuRenameSymbols, enableRename);
                }
                else
                    CCLogger::Get()->DebugLog(_T("Could not find Insert menu 3!"));
            }
            else
                CCLogger::Get()->DebugLog(_T("Could not find Insert menu 2!"));
        }
        else if (not wxFound(insertId))
            CCLogger::Get()->DebugLog(_T("Could not find Insert menu!"));

        if (wxFound(insertId) and pEditor /* and GetLSP_Initialized(pEditor)*/)
        {
            // insert "Reparse this file" under "Insert/Refactor"
            size_t posn = 0;
            wxMenuItem* insertMenuItem = menu->FindChildItem(insertId, &posn);
            if (insertMenuItem)
                menu->Insert(posn+1, idEditorFileReparse, _("Reparse this file"), _("Reparse current editors file"));
        }
        if (wxFound(insertId) and pEditor and (not GetLSP_Initialized(pEditor)) )
            menu->Enable(insertId, false);

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
                menu->Insert(position, idSelectedProjectReparse, _("Reparse current project"), _("Reparse current actived project"));
                cbProject* pProject = data->GetProject();
                if (pProject)
                {
                    Parser* pParser = dynamic_cast<Parser*>(GetParseManager()->GetParserByProject(pProject));
                    if (pParser)
                    {
                        menu->InsertCheckItem(position + 1, idPauseParsing, _("Pause parsing (toggle)"), _("Toggle Resume/Pause LSP parsing"));
                        menu->Check(idPauseParsing, pParser and pParser->GetUserParsingPaused());
                    }
                }
                else
                    menu->Check(idPauseParsing, false);
            }
            else if (data->GetKind() == FileTreeData::ftdkFile)
                menu->Append(idSelectedFileReparse, _("Reparse this file"), _("Reparse current selected file"));
        }
    }
}
// ----------------------------------------------------------------------------
bool ClgdCompletion::BuildToolBar(wxToolBar* toolBar)
// ----------------------------------------------------------------------------
{
    if (not IsAttached()) return false;
    if (m_CC_initDeferred) return false;

    if (m_OldCC_enabled or m_CC_initDeferred)
        return false;

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
// ----------------------------------------------------------------------------
void ClgdCompletion::OnWindowActivated(wxActivateEvent& event) //on Window activated
// ----------------------------------------------------------------------------
{
    event.Skip();
    if (m_CC_initDeferred) return;
 }
// --------------------------------------------------------------
void ClgdCompletion::OnPluginAttached(CodeBlocksEvent& event)
// --------------------------------------------------------------
{
    //From DoAddPlugin(plug) in PluginManager;

    // Messages here can end up unseen behind the splash screen during CB startup;
    // and codeblocks freezes waiting for the messageBox "Ok" button to be pressed.

    bool isClangdClientPlugin = false;
    bool isOldCodeCompletion = false;

    bool clgdEnabled = Manager::Get()->GetConfigManager(_T("plugins"))->ReadBool("/clangd_client");
    bool oldCCEnabled = IsOldCCEnabled();

    PluginManager* pPlgnMgr = Manager::Get()->GetPluginManager();
    cbPlugin* pPlugin = event.GetPlugin();
    if (pPlugin)
    {
        const PluginInfo* info = Manager::Get()->GetPluginManager()->GetPluginInfo(pPlugin);
        wxString infoName = info->name.Lower();
        if (infoName == "clangd_client")
        {
            isClangdClientPlugin = true;
            if (oldCCEnabled)
                SetClangdClient_Disabled();
        }

        if (infoName =="codecompletion")
        {
            isOldCodeCompletion = true;
            //Make sure manager sets legacy CodeCompletion enabled
            Manager::Get()->GetConfigManager(_T("plugins"))->Write(_T("/codecompletion"), true );
            m_OldCC_enabled = true;
            // Tell Manager to disable Clangd_client
            SetClangdClient_Disabled();
        }
    }

    // Legacy CodeCompletion should never be running at same time as clangd_client
    if ((not m_CC_initDeferred) and isOldCodeCompletion)
    {
        PluginElement* pPluginElement = pPlgnMgr->FindElementByName("CodeCompletion");
        wxString plgnFilename = pPluginElement ? pPluginElement->fileName : wxString();
        // clangd_client is running and legacy CodeCompletion plugin is being loaded.
        wxString msg = _("The CodeCompletion plugin should not be enabled when 'Clangd_client' is running.\n"
                         "The plugins are not compatible with one another.\n\n"
                         "Disable either CodeCompletion or Clangd_client and\n"
                         "RESTART Code::Blocks to avoid crashes and effects of incompatibilities.");
        if (plgnFilename.Length())
            msg << wxString::Format(_("\n\nPlugin location:\n%s"), plgnFilename);
        cbMessageBox(msg, _("ERROR"), wxOK, GetTopWxWindow());
        return;
    }
    // if clangd_client was disabled and user has just enabled it, we need to invoke
    // OnAppStartupDone() in order to initialize it.
    //if ((not m_InitDone) and (not m_CC_initDeferred) and (m_ctorClientStartupStatusEnabled == false) )
    if ( isClangdClientPlugin and (not m_InitDone)
            and (not m_CC_initDeferred) and ns_DefaultCompilerMasterPath.Length() )
    {
        wxWindow* pTopWindow = GetTopWxWindow();
        wxWindow* pManageWindow = wxFindWindowByName(_("Manage plugins"));
        // If this is a response to the user just having enabled Clangd_Client,
        // we need to call OnAppStartupDone to fully initialize.
        cbPlugin* pPlugin = event.GetPlugin();
        if (pPlugin && pManageWindow && pTopWindow)
        {
            // Use wxMessageDialog to avoid being hidden behind "Manage plugins" dialog
            // window when "focus follows mouse"
            wxMessageDialog dlg(pManageWindow,
                _("Clangd_Client plugin needs you to RESTART codeblocks..."),
                _("CB restart needed"),
                wxOK | wxSTAY_ON_TOP | wxCAPTION | wxCENTRE);
            PlaceWindow(&dlg);
            #ifdef __WXGTK__
                // For Linux, use ShowWindowModal() else window can be hidden behind
                // the "Manage plugins" dialog when using "focus follows mouse"
                dlg.ShowWindowModal();
            #else
                dlg.ShowModal();
            #endif

            CallAfter(&ClgdCompletion::OnPluginEnabled); //calls OnAppStartupDone()
        }//endif pPlugin

        return;
    }//endif isClangdClientPlugin...

    // ----------------------------------------------------------------------------
    // What we do if legacy CodeCompletion gets enabled while clangd_client is running.
    // ----------------------------------------------------------------------------
    cbPlugin* plug = event.GetPlugin();
    if (plug and clgdEnabled)
    {
        const PluginInfo* info = Manager::Get()->GetPluginManager()->GetPluginInfo(plug);
        wxString msg = info ? info->title : wxString(_("<Unknown plugin>"));
        if (info->name == "CodeCompletion")
        {
            wxString msg = _("The CodeCompletion plugin should not be enabled when 'Clangd_client' is running.\n"
                             "The plugins are not compatible with one another.\n\n"
                             "Disable either CodeCompletion or Clangd_client and\n"
                             "RESTART Code::Blocks to avoid crashes and effects of incompatibilities.");
            cbMessageBox(msg, _("ERROR"), wxOK, GetTopWxWindow());
        }
        //Manager::Get()->GetLogManager()->DebugLog(F(_T("%s plugin activated"), msg.wx_str())); // **Debugging**
        if ( info->name.Lower() == "clangd_client" )
        {
            // This means that legacy CodeCompletion should be disabled.
            // But there's no way to do that from here.
        }
    }
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnIdle(wxIdleEvent& event)
// ----------------------------------------------------------------------------
{
    event.Skip(); //always event.Skip() to allow others use of idle events
    if (m_CC_initDeferred) return;

    if (not GetParseManager()) return;
    if (Manager::IsAppShuttingDown() or GetParseManager()->GetPluginIsShuttingDown() )
        return;
    if (ProjectManager::IsBusy() or (not IsAttached()) or (not m_InitDone) )
        return;

    // Honor a pending code completion request when user stops typing
    if (m_PendingCompletionRequest)
    {
        m_PendingCompletionRequest = false;
        CodeBlocksEvent evt(cbEVT_COMPLETE_CODE);
        Manager::Get()->ProcessEvent(evt);
    }

}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnCompilerMenuSelected(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // This event is no longer Bind()ed after Nightly 221022

    // This function bound from OnAppStartupDone() to intercept compiler menu selections
    // in order to avoid hanging this plugin because compilergcc plugin sends
    // no EVT_COMPIER_FINISHED event on a compiler Run() command.
    // FIXME (ph#): This trap is unnecessary after a nightly for rev 12975

    event.Skip(); /// <------ very important

    // code no longer necessary after Nightly 221022
    //ns_CompilerEventId = event.GetId();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnCompilerStarted(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    //#warning Developer should remove the line below when using CB rev 12975 and above.
    //if (ns_CompilerEventId == XRCID("idCompilerMenuRun")) return;
    // Above code remove. No longer necessary after Nightly 221022

    GetParseManager()->SetCompilerIsRunning(true);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnCompilerFinished(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    GetParseManager()->SetCompilerIsRunning(false);
}
// ----------------------------------------------------------------------------
ClgdCompletion::CCProviderStatus ClgdCompletion::GetProviderStatusFor(cbEditor* ed)
// ----------------------------------------------------------------------------
{
    if (m_CC_initDeferred) return ccpsInactive;

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
// ----------------------------------------------------------------------------
std::vector<ClgdCompletion::CCToken> ClgdCompletion::GetAutocompList(bool isAuto, cbEditor* ed, int& tknStart, int& tknEnd)
// ----------------------------------------------------------------------------
{
    // Called directly from ccmanager.cpp to get list of completions

    // On the first call from ccmanager we send clangd a completion request, then return empty tokens.
    // This event will be reissued after OnLSP_completionResponse() receives the completion items.

    // This routine will be entered the second time after clangd returns the completions and
    // function OnLSP_CompletionResponse() has filled in m_completionsTokens vector.
    // OnLSP_CompletionsResponse() will reissues the cbEVT_COMPLETE_CODE event to re-enter here the second time.
    // This routine will then return a filled-in tokens vector with the completions from m_completionsTokens.

    std::vector<ClgdCCToken> cldTokens; //extended ccTokens for Clangd (unused)
    std::vector<CCToken> ccTokens;     // regular ccTokens for ccManager


    if (!IsAttached() || !m_InitDone)
        return ccTokens;
    if (m_CC_initDeferred) return ccTokens;

    if (not GetLSP_Initialized(ed)) return ccTokens;

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
            return ccTokens;  //return empty tokens container
        }
    }

    // Clear completion token cache when new pattern does not match the old
    wxString newCompletionPattern = stc->GetTextRange(tknStart, tknEnd);
    if (m_PreviousCompletionPattern != newCompletionPattern)
    {
        m_CompletionTokens.clear();
        m_PreviousCompletionPattern = newCompletionPattern;
    }

    // ----------------------------------------------------------------------------
    // LSP Code Completion
    // ----------------------------------------------------------------------------
    // On second call from ccmanager, we should have some tokens to return
    // else the second call is never initiated by OnLSP_Completion().
    if (m_CompletionTokens.size() )
    {
        // We have some completions, hand them back to ccmanager

        // **debugging** LogManager* pLogMgr = CCLogger::Get()->;
        //for (size_t tknNdx; tknNdx<m_CompletionTokens.size(); ++tknNdx)
        //for (cbCodeCompletionPlugin::CCToken tknNdx : m_CompletionTokens)

        bool caseSensitive = GetParseManager()->GetParser().Options().caseSensitive;
        wxString pattern  = stc->GetTextRange(tknStart, tknEnd);

        // if use CC icons in completion list enabled
        std::set<int> alreadyRegistered;
        cbStyledTextCtrl* stc = ed->GetControl();
        const int fontSize = CalcStcFontSize(stc);
        wxImageList* ilist = GetParseManager()->GetImageList(fontSize);
        stc->ClearRegisteredImages();
        //-#endif

        for(size_t ii=0; ii<m_CompletionTokens.size(); ++ii)
        {
            // **debugging** CCToken look = m_CompletionTokens[ii];
            ClgdCCToken cldToken = m_CompletionTokens[ii];
            wxString tkn_displayName = cldToken.displayName;
            if (tkn_displayName.empty() ) continue;

            //wxString tkn_name = m_CompletionTokens[ii].name;
            if (not caseSensitive)
            {
                pattern = pattern.Lower();
                tkn_displayName = tkn_displayName.Lower();
                //tkn_name = tkn_name.Lower();
            }
            if (tkn_displayName.StartsWith(pattern))
            {
                ccTokens.push_back(cldToken);

               // if use CC icons in completion list enabled
               if (GetParseManager()->GetUseCCIconsOption())
               {
                    // Set the image index for each token
                    int semanticTokenType = cldToken.semanticTokenType;
                    // fake token to use for convertion to image index
                    Token token(wxString(),0,0,0);
                    token.m_TokenKind = (TokenKind)semanticTokenType;
                    if (semanticTokenType == LSP_SemanticTokenType::Unknown)
                        cldToken.category = (TokenKind)-1;
                    token.m_TokenKind =  GetParseManager()->GetParser().ConvertLSPSemanticTypeToCCTokenKind(semanticTokenType);
                    int iidx = GetParseManager()->GetTokenImageFromSemanticTokenType(&token);
                    if (iidx < 0) iidx = -1; // -1 tells ccManager to ignore this item
                    cldToken.category = iidx;
                    if ( (iidx > 0) and (alreadyRegistered.find(iidx) == alreadyRegistered.end()))
                    {
                        stc->RegisterImage(iidx, ilist->GetBitmap(iidx));
                        alreadyRegistered.insert(iidx);
                    }
                    // update the category for the last pushed cctoken
                    ccTokens.back().category = cldToken.category;
               }
            }

            // **debugging** info
            //wxString cmpltnStr = wxString::Format(
            //        "Completion:id[%d],category[%d],weight[%d],displayName[%s],name[%s]",
            //                        tokens[ii].id,
            //                        tokens[ii].category,
            //                        tokens[ii].weight,
            //                        tokens[ii].displayName,
            //                        tokens[ii].name
            //                        );
            //pLogMgr->DebugLog(cmpltnStr);
        }
        // Move clear() to next request for completions to preserve tokens for DoAutoComplete()
        //- Moved - m_CompletionTokens.clear(); //clear to use next time and return the tokens

        return ccTokens;
    }

    // We have no completion data, issue a LSP_Completion() call, and return.
    // When the OnLSP_Completionresponse() event occurs, it will re-enter this function
    // with m_CompletionTokens full of clangd completion items.
    if (GetLSP_IsEditorParsed(ed) )
    {
        if (   stc->IsString(style)
            || stc->IsComment(style)
            || stc->IsCharacter(style)
            //- || stc->IsPreprocessor(style) ) allow preprocessors
             )
        {
            return ccTokens; //For styles above ignore this request
        }

        //For users who type faster, say at 75 WPM, the gap that would indicate the end of typing would be only 0.3 seconds (300 milliseconds.)
        //-int mSecsSinceLastModify = GetLSPClient(ed)->GetDurationMilliSeconds(m_LastModificationMilliTime);
        //-if (mSecsSinceLastModify > m_CCDelay) <--- Is this necessary ?
        if (1) /// <-- experimentation
        {
            // FYI: LSP_Completion() will send LSP_DidChange() notification to LSP server for the current line.
            // else LSP may crash for out-of-range conditions trying to complete text it's never seen.

            // Ignore completing tokens ending in blank, CR, or LF
            m_PendingCompletionRequest = false;
            if ( (curChar == ' ') or (curChar == '\n') or (curChar == '\r') )
                return ccTokens;  //return empty tokens

            //- m_CompletionTokens.clear(); //clear to use next time and return the token
            /// ^^ Don't clear; a call to show html documentation popup may need them.
            GetLSPClient(ed)->LSP_CompletionRequest(ed);
        }
        else {
            // time between typed keys too short. Wait awhile.
            m_PendingCompletionRequest = true;
            wxWakeUpIdle();
        }
    }

    return ccTokens; //return empty tokens on first call from ccmanager.

    // useful debugging output
    //for(size_t ii=0; ii< tokens.size(); ++ii)
    //{
    //    CCToken look = tokens[ii];
    //    int id = look.id;                        //!< CCManager will pass this back unmodified. Use it as an internal identifier for the token.
    //    int category = look.category;;           //!< The category corresponding to the index of the registered image (during autocomplete).
    //    int weight = look.weight;                //!< Lower numbers are placed earlier in listing, 5 is default; try to keep 0-10.
    //    wxString displayName = look.displayName; //!< Verbose string representing the token.
    //    wxString name = look.name;               //!< Minimal name of the token that CCManager may choose to display in restricted circumstances.
    //    wxString msg = wxString::Format("CCToken(%d) id:%d category:%d weight:%d dispName:%s name:%s",
    //                ii, id, category, weight, displayName, name);
    //    CCLogger::Get()->DebugLog(msg);
    //}

    return ccTokens;
}
// ----------------------------------------------------------------------------
static int CalcStcFontSize(cbStyledTextCtrl *stc)
// ----------------------------------------------------------------------------
{
    wxFont defaultFont = stc->StyleGetFont(wxSCI_STYLE_DEFAULT);
    defaultFont.SetPointSize(defaultFont.GetPointSize() + stc->GetZoom());
    int fontSize;
    stc->GetTextExtent(wxT("A"), nullptr, &fontSize, nullptr, nullptr, &defaultFont);
    return fontSize;
}
// ----------------------------------------------------------------------------
std::vector<ClgdCompletion::CCCallTip> ClgdCompletion::GetCallTips(int pos, int style, cbEditor* ed, int& argsPos)
// ----------------------------------------------------------------------------
{
    std::vector<CCCallTip> tips;

    if (not GetLSP_Initialized(ed)) return tips; //empty tips
    if (!IsAttached() || !m_InitDone || style == wxSCI_C_WXSMITH || !GetParseManager()->GetParser().Done())
        return tips;
    if (m_CC_initDeferred) return tips;

    // If waiting for clangd LSP_HoverResponse() return empty tips
    if (GetParseManager()->GetHoverRequestIsActive())
        return tips;    //empty tips

    // If not waiting for Hover response, and the signature help tokens are empty,
    // issue a LSP_Hover request via GetTokenAt().
    // Note that m_SignatureTokens is no longer used. 2023/07/2
    //  because asking for LsP_SignatureHelp always return empty results.
    if (0 == m_SignatureTokens.size())
    {
        if (not GetLSPClient(ed)) return tips; //empty tips

        //LSP_SignatureHelp always returns empty. Try using Hover instead
        //-GetLSPClient(ed)->LSP_SignatureHelp(ed, pos);
        bool allowCallTip = true;
        GetTokenAt(pos, ed, allowCallTip);

        return tips; //empty tips
    }

    m_SignatureTokens.clear(); //so we can ask for Signatures again
    return tips; //signature Help entries from clangd

}
// ----------------------------------------------------------------------------
wxString ClgdCompletion::GetDocumentation(const CCToken& token)
// ----------------------------------------------------------------------------
{
    // For clangd client we issue a hover request to get clangd data
    // OnLSP_CompletionPopupHoverResponse() will push the data int m_HoverTokens and
    // reissue the GetDocumentation request

    if (token.id == -1) return wxString();

    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return wxString();
    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (not pParser) return wxString();
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor) return wxString();
    ProcessLanguageClient* pClient = GetLSPClient(pEditor); // gets this editors LSP_client
    if (not pClient) return wxString();
    if (pClient and (not pClient->GetLSP_IsEditorParsed(pEditor)) )
        return wxString();
    if (GetCompletionTokens()->empty())
        return wxString();
    // The token id is actually an index into m_CompletionTokens
    ClgdCCToken cccToken = GetCompletionTokens()->at(token.id);
    wxString htmlDocumentation = pParser->GetCompletionPopupDocumentation(cccToken);
    if (htmlDocumentation.empty()) return wxString();
    //AutoCompPopup could have been cancelled before we obtained html documentation
    if (pEditor->GetControl() and pEditor->GetControl()->AutoCompActive())
        return htmlDocumentation;
    return wxString();
}

// ----------------------------------------------------------------------------
std::vector<ClgdCompletion::CCToken> ClgdCompletion::GetTokenAt(int pos, cbEditor* ed, bool& WXUNUSED(allowCallTip))
// ----------------------------------------------------------------------------
{
    std::vector<CCToken> tokens;

    if (!IsAttached() || !m_InitDone)
        return tokens; //It's empty
    if (m_CC_initDeferred) return tokens;

    GetParseManager()->SetHoverRequestIsActive(false);

    // ignore comments, strings, preprocessors, etc
    cbStyledTextCtrl* stc = ed->GetControl();
    const int style = stc->GetStyleAt(pos);
    if (   stc->IsString(style)
        || stc->IsComment(style)
        || stc->IsCharacter(style)
        || stc->IsPreprocessor(style) )
    {
        return tokens; //It's empty
    }

    // ----------------------------------------------------
    // LSP Hover
    // ----------------------------------------------------
    // On second call from ccmanager, we should have some tokens to return
    // else the second call is never initiated by OnLSP_HoverResponse().
    if (m_HoverTokens.size() )
    {
        tokens.clear();
        //wxString hoverMsg = wxString::Format("GetTokenAt() sees %d tokens.\n", int(m_HoverTokens.size()));
        //CCLogger::Get()->DebugLog(hoverMsg);
        for(size_t ii=0; ii<m_HoverTokens.size(); ++ii)
        {
            CCToken look = m_HoverTokens[ii]; //debugging
            tokens.push_back(m_HoverTokens[ii]);
        }
        m_HoverTokens.clear();
        GetParseManager()->SetHoverRequestIsActive(false);
        return tokens;
    }
    // On the first call from ccmanager, issue LSP_Hover() to clangd and return empty tokens
    // while waiting for clangd to respond. Once we get response data, OnLSP_HoverResponse()
    // will re-issue this event (cbEVT_EDITOR_TOOLTIP) to display the results.
    if (GetLSP_IsEditorParsed(ed) )
    {
        GetParseManager()->SetHoverRequestIsActive(true);
        m_HoverLastPosition = pos;
        GetLSPClient(ed)->LSP_Hover(ed, pos);
    }
    tokens.clear();
    return tokens; //return empty tokens on first call from ccmanager.

}
// ----------------------------------------------------------------------------
wxString ClgdCompletion::OnDocumentationLink(wxHtmlLinkEvent& event, bool& dismissPopup)
// ----------------------------------------------------------------------------
{
    // user has clicked link in HTML documentation popup window
    return m_pDocHelper->OnDocumentationLink(event, dismissPopup);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::DoAutocomplete(const CCToken& token, cbEditor* ed)
// ----------------------------------------------------------------------------
{
    // wxScintilla Callback after code completion selection

    // Finish code completion for LSP
    return LSP_DoAutocomplete(token, ed); // Finish code completion for LSP
}
// ----------------------------------------------------------------------------
void ClgdCompletion::LSP_DoAutocomplete(const CCToken& token, cbEditor* ed)
// ----------------------------------------------------------------------------
{
    // wxScintilla Callback after code completion selection
    ///NB: the token.id is an index into the parsers m_CompletionTokens vector or -1. It's NOT a CCToken.id

    if (not GetLSP_Initialized(ed)) return;

    struct UnlockTokenTree
    {
        UnlockTokenTree(){}
        ~UnlockTokenTree()
        {
            if (m_CCHasTreeLock)
            {
                // -----------------------------------------------
                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex);
                s_TokenTreeMutex_Owner = wxString();
                // -----------------------------------------------
            }
            //- do not m_CompletionTokens.clear(); //Say we're done with the completion list
            // ^^^ Don't clear the cache, user may ask for html documentation
        }

    } unlockTokenTree;

    cbProject* pProject = ed->GetProjectFile() ? ed->GetProjectFile()->GetParentProject() : nullptr;
    if (not pProject) pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return;

    wxString itemText = CodeCompletionHelper::AutocompGetName(token.displayName); //clips to "(: "
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
        itemText = token.displayName; // use the unclipped text
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
            wxRegEx ppIf(wxT("^[ \t]*#[ \t]*if"));
            wxRegEx ppEnd(wxT("^[ \t]*#[ \t]*endif"));
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
                    wxRegEx pp(wxT("^[ \t]*#[ \t]*[a-z]*([ \t]+([a-zA-Z0-9_]+)|())"));
                    pp.Matches(stc->GetLine(ppLine));
                    if (!pp.GetMatch(stc->GetLine(ppLine), 2).IsEmpty())
                        itemText.Append(wxT(" // ") + pp.GetMatch(stc->GetLine(ppLine), 2));
                    break;
                }
            }
        }
        needReparse = true;
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

    ///NB: the token.id is an index into the parsers m_CompletionTokens vector or -1. It's NOT a CCToken.id
    if ( (token.id != -1) and m_CCAutoAddParentheses)
    {

        int clgdCCTokenIdx = -1;
        wxString name; wxString displayName;
        ClgdCCToken cccToken(-1, displayName, name);    //id and names

        if ((token.id >= 0) and (int(m_CompletionTokens.size()) > token.id) )
        {
            clgdCCTokenIdx = token.id;
            cccToken = m_CompletionTokens[clgdCCTokenIdx];
        }

        wxString tokenArgs;
        if ( (clgdCCTokenIdx >= 0)
            and (cccToken.semanticTokenType != LSP_SemanticTokenType::Unknown) )
        {
            wxString tknName = cccToken.displayName.BeforeFirst('(', &tokenArgs);
            bool addParentheses = not tokenArgs.empty();

            // add back the beginning paren
            if (addParentheses and tokenArgs.size())
                tokenArgs.Prepend("(");

            if (addParentheses)
            {
                bool insideFunction = true;
                if (m_CCDetectImplementation)
                {
                    // ----------------------------------------------------------------------------
                    //CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
                    // ----------------------------------------------------------------------------
                    /// Lock token tree. Unlock occurs in UnlockTokenTree struct dtor above
                    auto locker_result = s_TokenTreeMutex.LockTimeout(250);
                    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
                    if (locker_result != wxMUTEX_NO_ERROR)
                    {
                        // lock failed, do not block the UI thread
                        m_CCHasTreeLock = false;
                    }
                    else /*lock succeeded*/
                    {
                        m_CCHasTreeLock = true;
                        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
                        // TokenTree lock will be released when function returns. Cf: UnlockToken struct dtor above
                    }

                    TokenTree* tree = m_CCHasTreeLock ? GetParseManager()->GetParser().GetTokenTree() : nullptr;

                    ccSearchData searchData = { stc, ed->GetFilename() };
                    int funcToken;
                    if (GetParseManager()->FindCurrentFunctionStart(m_CCHasTreeLock, &searchData, 0, 0, &funcToken, -1) == -1)
                    {
                        // global scope
                        itemText += tokenArgs;
                        insideFunction = false;
                    }
                    else // Found something, but result may be false positive.
                    {
                        const Token* parent = tree->at(funcToken);
                        // Make sure that parent is not container (class, etc)
                        if (parent && (parent->m_TokenKind & tkAnyFunction) == 0)
                        {
                            // class scope
                            itemText += tokenArgs;
                            insideFunction = false;
                        }

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
        } // if clgdCCTokenIdx
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
        TRACE(F("CodeCompletion::%s: Starting m_TimerRealtimeParsing.", __FUNCTION__));
        //m_TimerRealtimeParsing.Start(1, wxTIMER_ONE_SHOT);
    }
    stc->ChooseCaretX();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::EditorEventHook(cbEditor* editor, wxScintillaEvent& event)
// ----------------------------------------------------------------------------
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
    if (not GetLSP_Initialized(editor)) { event.Skip(); return;}

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
    if (   GetParseManager()->GetParser().Options().whileTyping
        && (   (event.GetModificationType() & wxSCI_MOD_INSERTTEXT)
            || (event.GetModificationType() & wxSCI_MOD_DELETETEXT) ) )
    {
       // m_NeedReparse = true;
    }
    // ----------------------------------------------------------------------------
    // Support for LSP code completion calls with keyboard dwell time (ph 2021/01/31)
    // ----------------------------------------------------------------------------
    if (   ((event.GetModificationType() & wxSCI_MOD_INSERTTEXT)
            || (event.GetModificationType() & wxSCI_MOD_DELETETEXT))
            and (GetLSPClient(editor))
        )
    {
        // set time of modification
        m_LastModificationMilliTime =  GetLSPClient(editor)->GetNowMilliSeconds();
        GetLSPClient(editor)->SetLSP_EditorModified(editor, true);

        // Ctrl-Z undo and redo may not set EditorBase::SetModified() especially on
        // the last undo/redo which places the editor text in the original condition.
        // But this leaves the LSP server in an out of sync condition.
        // Resync the LSP server to the current editor text.
        if (not editor->GetModified())
            GetLSPClient(editor)->LSP_DidChange(editor);
    }

    if (control->GetCurrentLine() != m_CurrentLine)
    {
        // reparsing the editor only happens in the condition that the caret's line number
        // is changed.
        if (0 /*m_NeedReparse*/)
        {
            TRACE(_T("CodeCompletion::EditorEventHook: Starting m_TimerRealtimeParsing."));
            //m_TimerRealtimeParsing.Start(REALTIME_PARSING_DELAY, wxTIMER_ONE_SHOT);
            m_CurrentLength = control->GetLength();
            //m_NeedReparse = false;
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
// ----------------------------------------------------------------------------
void ClgdCompletion::RereadOptions()
// ----------------------------------------------------------------------------
{
    // Keep this in sync with CCOptionsDlg::CCOptionsDlg and CCOptionsDlg::OnApply

    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");

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
    m_CCMaxMatches           = cfg->ReadInt(_T("/max_matches"),            256);
    m_CCAutoAddParentheses   = cfg->ReadBool(_T("/auto_add_parentheses"),  true);
    m_CCDetectImplementation = cfg->ReadBool(_T("/detect_implementation"), false); //depends on auto_add_parentheses
    m_CCFillupChars          = cfg->Read(_T("/fillup_chars"),              wxEmptyString);
    m_CCDelay                = cfg->ReadInt("/cc_delay",                   300);
    m_CCEnableHeaders        = cfg->ReadBool(_T("/enable_headers"),        true);
    m_CCEnablePlatformCheck  = cfg->ReadBool(_T("/platform_check"),        true);

    // update the CC toolbar option, and tick the timer for toolbar
    // NOTE (ollydbg#1#12/06/14): why?
    // Because the user may have changed a view option?
    if (m_ToolBar)
    {
        UpdateToolBar();
        CodeBlocksLayoutEvent evt(cbEVT_UPDATE_VIEW_LAYOUT);
        Manager::Get()->ProcessEvent(evt);
        m_ToolbarNeedReparse = true;
        TRACE(_T("CodeCompletion::RereadOptions: Starting m_TimerToolbar."));
        m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
    }

    m_pDocHelper->RereadOptions(cfg);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::UpdateToolBar()
// ----------------------------------------------------------------------------
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");
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
// ----------------------------------------------------------------------------
void ClgdCompletion::OnUpdateUI(wxUpdateUIEvent& event)
// ----------------------------------------------------------------------------
{
    wxString NameUnderCursor;
    bool IsInclude = false;
    const bool HasNameUnderCursor = CodeCompletionHelper::EditorHasNameUnderCursor(NameUnderCursor, IsInclude);
    bool HasEd = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor() != 0;
    bool hasLSPclient = HasEd ? GetLSP_Initialized(Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor()) : false;
    HasEd = HasEd and hasLSPclient;

    if (m_EditMenu)
    {
        const bool RenameEnable = hasLSPclient and HasNameUnderCursor && !IsInclude && GetParseManager()->GetParser().Done();
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
        const bool FindEnable = HasNameUnderCursor && !IsInclude && GetParseManager()->GetParser().Done();
        m_SearchMenu->Enable(idMenuFindReferences, FindEnable);
        const bool IncludeEnable = HasNameUnderCursor && IsInclude;
        m_SearchMenu->Enable(idMenuOpenIncludeFile, IncludeEnable);
    }

    if (m_ViewMenu)
    {
        bool isVis = IsWindowReallyShown((wxWindow*)GetParseManager()->GetClassBrowser());
        m_ViewMenu->Check(idViewClassBrowser, isVis);
    }

    if (m_ProjectMenu)
    {
        cbProject* pActivePrj = Manager::Get()->GetProjectManager()->GetActiveProject();
        m_ProjectMenu->Enable(idCurrentProjectReparse, pActivePrj);
    }

    // must do...
    event.Skip();
}//OnUpdateUI
// ----------------------------------------------------------------------------
void ClgdCompletion::OnViewClassBrowser(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (!Manager::Get()->GetConfigManager("clangd_client")->ReadBool(_T("/use_symbols_browser"), false))
    {
        cbMessageBox(_("The symbols browser is disabled in code-completion options.\n"
                        "Please enable it there first..."), _("Information"), wxICON_INFORMATION);
        return;
    }
    CodeBlocksDockEvent evt(event.IsChecked() ? cbEVT_SHOW_DOCK_WINDOW : cbEVT_HIDE_DOCK_WINDOW);
    evt.pWindow = (wxWindow*)GetParseManager()->GetClassBrowser();
    Manager::Get()->ProcessEvent(evt);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnGotoFunction(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return;

    TRACE(_T("OnGotoFunction"));

    // --------------------------------------------------------
    // LSP GoToFunction checks
    // --------------------------------------------------------
    cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pActiveProject) return;
    if (not GetLSPClient(pActiveProject) )
        return;

    // If file owned by proxy project, ask for symbols
    cbProject* pEdProject = ed->GetProjectFile() ? ed->GetProjectFile()->GetParentProject() : nullptr;
    if (pEdProject and (pEdProject == GetParseManager()->GetProxyProject()) )
    {
        // Issue request for symbols and queue response to Parser::OnLSP_GoToFunctionResponse(wxCommandEvent& event)
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pEdProject);
        if (not pParser) return;
        // Register a callback redirected to OnLSP_GoToFunctionResponse() for the LSP response
        size_t id = GetParseManager()->GetLSPEventSinkHandler()->LSP_RegisterEventSink(XRCID("textDocument/documentSymbol"), pParser, &Parser::OnLSP_GoToFunctionResponse, event);
        // Ask clangd for symbols in this editor, OnLSP_GoToFunctionResponse() will handle the response
        GetLSPClient(ed)->LSP_RequestSymbols(ed, id);
        return;
    }

    wxString msg = VerifyEditorHasSymbols(ed);
    if (not msg.empty())
    {
        msg += wxString::Format("\n(%s)",__FUNCTION__);
        InfoWindow::Display("LSP", msg, 7000);
        return;
    }

    TokenTree* tree = nullptr;

    //the clgdCompletion way to gather functions from token tree
    tree = GetLSP_Initialized(ed) ? GetParseManager()->GetParser().GetTokenTree() : nullptr ;

    // -----------------------------------------------------
    //CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // -----------------------------------------------------
    auto locker_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread, call back when idle
        if (GetParseManager()->GetIdleCallbackHandler(pActiveProject)->IncrQCallbackOk(lockFuncLine))
            GetIdleCallbackHandler(pActiveProject)->QueueCallback(this, &ClgdCompletion::OnGotoFunction, event);
        return;
    }
    else /*lock succeeded*/
    {
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        GetParseManager()->GetIdleCallbackHandler(pActiveProject)->ClearQCallbackPosn(lockFuncLine);
    }


    if ( (not tree) or tree->empty())
    {
        // ----------------------------------------------
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        s_TokenTreeMutex_Owner = wxString();
        // ----------------------------------------------
        cbMessageBox(_("No functions parsed in this file.\n(Empty symbols tree)."),
                     wxString::Format("%s",__FUNCTION__));
    }
    else
    {
        wxString edFilename = ed->GetFilename();
        edFilename.Replace('\\','/');
        GotoFunctionDlg::Iterator iterator;

        for (size_t i = 0; i < tree->size(); i++)
        {
            Token* token = tree->at(i);
            bool isImpl = ParserCommon::FileType(edFilename) == ParserCommon::ftSource;
            if ( (not isImpl) && token && (token->GetFilename() != edFilename) ) continue;
            if ( isImpl && token &&   (token->GetImplFilename() != edFilename) ) continue;
            if ( token && (token->m_TokenKind & tkAnyFunction) )
            {
                //wxString tknFilename = token->GetFilename();          //**debugging**
                //wxString tknImplFilename = token->GetImplFilename();  //**debugging**
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

        // ----------------------------------------------------------------------------
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        s_TokenTreeMutex_Owner = wxString();
        // ----------------------------------------------------------------------------

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
                    if (ParserCommon::FileType(edFilename) == ParserCommon::ftSource)
                        ed->GotoTokenPosition(ft->implLine - 1, ft->name);
                    else
                        ed->GotoTokenPosition(ft->line - 1, ft->name);
                }
            }//if selection
        }//if dlg
    }//else
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnGotoPrevFunction(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (not ed) return;
    if (not GetLSP_IsEditorParsed(ed) ) return;

    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return;
    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (not pParser) return;
    // Register a callback redirected to OnLSP_GoToPrevFunctionResponse() for the LSP response
    size_t id = GetParseManager()->GetLSPEventSinkHandler()->LSP_RegisterEventSink(XRCID("textDocument/documentSymbol"), pParser, &Parser::OnLSP_GoToPrevFunctionResponse, event);
    // Ask clangd for symbols in this editor, OnLSP_GoToPrevFunctionResponse() will handle the response
    GetLSPClient(ed)->LSP_RequestSymbols(ed, id);
    return;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnGotoNextFunction(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (not ed) return;
    if (not GetLSP_IsEditorParsed(ed) ) return;

    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return;
    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (not pParser) return;
    // Register a callback redirected to OnLSP_GoToNextFunctionResponse() for the LSP response
    size_t id = GetParseManager()->GetLSPEventSinkHandler()->LSP_RegisterEventSink(XRCID("textDocument/documentSymbol"), pParser, &Parser::OnLSP_GoToNextFunctionResponse, event);
    // Ask clangd for symbols in this editor. OnLSP_GoToNextFunctionResponse() will handle the response.
    GetLSPClient(ed)->LSP_RequestSymbols(ed, id);
    return;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnClassMethod(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)return;
    if (not GetLSP_Initialized(ed) ) return;

    DoClassMethodDeclImpl();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnUnimplementedClassMethods(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    DoAllMethodsImpl();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnGotoDeclaration(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    //NB: if the user asks for a declaration while already positioned on it
    //    clangd will respond with the implementation location.
    //    if the user asks for a definition while already positioned on it
    //    clangd will respond with the declaration location.

    ProjectManager* pPrjMgr = Manager::Get()->GetProjectManager();
    cbProject* pActiveProject = pPrjMgr->GetActiveProject();
    if (not GetLSPClient(pActiveProject)) return;

    EditorManager* pEdMgr  = Manager::Get()->GetEditorManager();
    cbEditor*      pActiveEditor = pEdMgr->GetBuiltinActiveEditor();
    if (!pActiveEditor)
        return;

    TRACE(_T("OnGotoDeclaration"));

    wxString msg = VerifyEditorParsed(pActiveEditor);
    if (not msg.empty()) //return reason editor is not yet ready
    {
        msg += wxString::Format("\n%s",__FUNCTION__);
        InfoWindow::Display("LSP", msg, 7000);
        return;
    }

    const int pos      = pActiveEditor->GetControl()->GetCurrentPos();
    const int startPos = pActiveEditor->GetControl()->WordStartPosition(pos, true);
    const int endPos   = pActiveEditor->GetControl()->WordEndPosition(pos, true);

    wxString targetText;
    targetText << pActiveEditor->GetControl()->GetTextRange(startPos, endPos);
    if (targetText.IsEmpty())
        return;

    // prepare a boolean filter for declaration/implementation
    bool isDecl = event.GetId() == idGotoDeclaration    || event.GetId() == idMenuGotoDeclaration;
    bool isImpl = event.GetId() == idGotoImplementation || event.GetId() == idMenuGotoImplementation;

   // ----------------------------------------------------------------------------
   // LSP Goto Declaration/definition
   // ----------------------------------------------------------------------------

    // if max parsing, spit out parsing is delayed message
    if (ParsingIsVeryBusy()) {;}

   // Confusing behaviour for original CC vs Clangd:
   // if caret is already on the definition (.h) clangd wont find it
    if (isDecl)
    {
        GetLSPClient(pActiveEditor)->LSP_GoToDeclaration(pActiveEditor, GetCaretPosition(pActiveEditor));
    }
    // Confusing behaviour of clangd which switches back and forth between def and decl
    if (isImpl)
    {
        GetLSPClient(pActiveEditor)->LSP_GoToDefinition(pActiveEditor, GetCaretPosition(pActiveEditor));
    }
    return;
}//end OnGotoDeclaration()
// ----------------------------------------------------------------------------
void ClgdCompletion::OnFindReferences(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // -unused- ProjectManager* pPrjMgr = Manager::Get()->GetProjectManager();
    // ----------------------------------------------------------------------------
    // LSP_FindReferences
    // ----------------------------------------------------------------------------
    // Call LSP now, else CodeRefactoring will change the editor
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    cbProject* pEdProject = pProjectFile ? pProjectFile->GetParentProject() : nullptr;

    // LSP: differentiate missing project vs clangd_client
    ProcessLanguageClient* pClient = GetLSPClient(pEditor);
    wxString filename = pEditor->GetFilename();

    if ( (not pEdProject) or (not pClient) )
    {
        wxString msg;
        if (not pEdProject)
           msg = _("Editor's file is not contained as member of a project.");
        if (not pClient)
            msg << _("\nThe project is not associated with a clangd_client (not parsed).");
        msg << _("\nMake sure the editors file has been added to a project and the file or project has been parsed.\n\n"
                 "Right-click the item in the Projects tree and choose Reparse this project\n"
                 "or right-click in the editor and choose Reparse this file.");
        cbMessageBox(msg, wxString("LSP: ") << __FUNCTION__);
        return;
    }

    // show info msg if clangd parsing not finished
    wxString msg = VerifyEditorParsed(pEditor);
    if (not msg.empty())
    {
        msg += wxString::Format("\n%s",__FUNCTION__);
        InfoWindow::Display("LSP", msg, 7000);
        return;
    }
    // check count of currently parsing files, and print info msg if max is parsing.
    if  (ParsingIsVeryBusy()) {;}

    GetLSPClient(pEditor)->LSP_FindReferences(pEditor, GetCaretPosition(pEditor));
    return;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnRenameSymbols(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    const wxString targetText = m_pCodeRefactoring->GetSymbolUnderCursor();
    if (targetText.IsEmpty())
        return;
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!pEditor)
        return;
    cbStyledTextCtrl* control = pEditor->GetControl();
    const int style = control->GetStyleAt(control->GetCurrentPos());
    if (control->IsString(style) || control->IsComment(style))
        return;
    // If any active editors are modified emit warning message
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    for (int ii=0; ii<pEdMgr->GetEditorsCount(); ++ii)
    {
        if (pEdMgr->GetEditor(ii)->GetModified())
        {
            wxString msg = _("Some editors may need saving\n before refactoring can be successful.");
            InfoWindow::Display(_("Some editors modified"), msg, 7000);
            break;
        }
    }
    const int pos = pEditor->GetControl()->GetCurrentPos();

    wxString replaceText = cbGetTextFromUser(_("Rename symbols under cursor"),
                                             _("Code Refactoring"),
                                             targetText,
                                             Manager::Get()->GetAppWindow());

    if (not replaceText.IsEmpty() && (replaceText != targetText) )
    {
        GetParseManager()->SetRenameSymbolToChange(targetText);
        GetLSPClient(pEditor)->LSP_RequestRename(pEditor, pos, replaceText);
    }
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnOpenIncludeFile(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
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

    wxArrayString foundSet = GetParseManager()->GetParser().FindFileInIncludeDirs(NameUnderCursor); // search in all parser's include dirs

    // look in the same dir as the source file
    wxFileName fname = NameUnderCursor;
    wxFileName base = lastIncludeFileFrom;
    NormalizePath(fname, base.GetPath());
    if (wxFileExists(fname.GetFullPath()) )
        foundSet.Add(fname.GetFullPath());

    // search for the file in project files
    cbProject* project = GetParseManager()->GetProjectByEditor(editor);
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

    cbMessageBox(wxString::Format(_("Not found: %s"), NameUnderCursor.c_str()), _("Warning"), wxICON_WARNING);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::ClearReparseConditions()
// ----------------------------------------------------------------------------
{
    // Clear all conditions that may prevent a reparse
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return;
    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (not pParser) return;

    wxArrayString pauseReasons;
    pParser->GetArrayOfPauseParsingReasons(pauseReasons);
    wxString msg;
    for (size_t ii=0; ii<pauseReasons.GetCount(); ++ii)
        msg = pauseReasons[ii] + "\n";
    if (GetParseManager()->IsCompilerRunning())
    {
        msg += _("Compiler was running, now cleared.\n");
        GetParseManager()->SetCompilerIsRunning(false);
    }
    if (pParser->GetUserParsingPaused())
    {
        pParser->SetUserParsingPaused(false);
        msg += _("User paused parsing, now cleared.\n");
    }

    if (msg.Length())
    {
        msg.Prepend(_("Cleared:\n"));
        InfoWindow::Display(_(" Pause(s) Cleared. "), msg, 7000);
    }
    return;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnCurrentProjectReparse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    /// Don't do Skip(). All connects and binds are about to be re-established
    // and this event will become invalid. Let wxWidgets delete it.
    //eventSkip() ==> causes crash;

    // Invoked from menu event "Reparse current project" and Symbols window root context menu "Re-parse now"

    #if defined(MEASURE_wxIDs)
    CCLogger::Get()->SetGlobalwxIDStart(__FUNCTION__, __LINE__);
    #endif

    // **Debugging**  Display pause parse reasons if any.
    // if Alt-Shift keys are down, Show pause status of all projects in the workspace.
    // Hold the keys down first then hold down the left mouse continuously
    // while sliding the mouse from main menu/Project/ onto the submenu item
    // "Reparse current project"
    if (wxGetKeyState(WXK_ALT) && wxGetKeyState(WXK_SHIFT))
    {
        // Get array of projects in the workspace
        // Get paused reasons for each project
        // Display each project and the pause reasons.
        DisplayPausedStatusOfAllProjects();
        return;
    }

    ClearReparseConditions();

    // ----------------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // ----------------------------------------------------
    // If lock is busy, queue a callback for idle time
    auto locker_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread, call back when idle
        if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
            GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::OnCurrentProjectReparse,event);
        return;
    }
    else /*lock succeeded*/
    {
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
    }

    // This structs dtor unlocks the Token tree after any return statement
    struct TokenTreeUnlock
    {   //CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        TokenTreeUnlock(){}
        ~TokenTreeUnlock() {
                // ---------------------------------------------------
                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex);
                // ---------------------------------------------------
                s_TokenTreeMutex_Owner = wxString();
            }
    } tokenTreeUnlock;


    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (pProject)
    {
        // Send a shutdown command with "needs cleanup" request.
        if (GetLSPClient(pProject))
            GetLSPClient(pProject)->SetProjectNeedsCleanup(pProject->GetFilename());
        ShutdownLSPclient(pProject);
        // Close and create a new parser
        GetParseManager()->ReparseCurrentProject();
        // Then create a new ProcessLanguageClient
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        // LSP_DidOpen() any active file in an editor belong to this project
        if (pParser)
        {
            // The new parser has already queued files to be parsed.
            // Freeze parsing for this parser and create a client.
            // The response to LSP initialization will unfreeze the parser.
            if ( int cnt = pParser->PauseParsingForReason("AwaitClientInitialization", true) > 1)
            {
                // error if greater than 1 **Debugging**
                wxString msg = wxString::Format("%s: AwaitClientInitialization count(%d) > 1", __FUNCTION__, cnt);
                //cbMessageBox(msg, "Error");
                Manager::Get()->GetLogManager()->DebugLogError(msg);
            }
            ProcessLanguageClient* pClient = GetParseManager()->CreateNewLanguageServiceProcess(pProject, LSPeventID);
            if (not pClient)
            {
                // stop the batch parse timer and clear the Batch parsing queue
                pParser->ClearBatchParse();
                wxString msg = wxString::Format(_("%s failed to create an LSP client"), __FUNCTION__);
                cbMessageBox(msg, _("Error"));
                return;
            }

            // Issue idle event to do DidOpen()s for this parser.
            // It will await client initialization, then do client DidOpen()s for
            // this new parser/client process before allowing parsing to proceed.
            //  Here's the re-schedule call for the Idle time Callback queue
            GetIdleCallbackHandler(pProject)->QueueCallback(pParser, &Parser::LSP_OnClientInitialized, pProject);
        }//endif parser
    }//endif project
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnReparseSelectedProject(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // do NOT event.skip();
    // ----------------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // ----------------------------------------------------
    // If lock is busy, queue a callback for idle time
    auto locker_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread, call back when idle
        if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
            GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::OnReparseSelectedProject,event);
        return;
    }
    else /*lock succeeded*/
    {
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
    }

    // This structs dtor unlocks the Token tree after any return statement
    struct TokenTreeUnlock
    {   //CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        TokenTreeUnlock(){}
        ~TokenTreeUnlock(){
             // ------------------------------------------------
             CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex);
             // ------------------------------------------------
             s_TokenTreeMutex_Owner = wxString();
             }
    } tokenTreeUnlock;


    switch (1)
    {
        // Shutdown the current LSP client/server and start another one.
        default:

        wxTreeCtrl* tree = Manager::Get()->GetProjectManager()->GetUI().GetTree();
        if (!tree) break;

        wxTreeItemId treeItem = Manager::Get()->GetProjectManager()->GetUI().GetTreeSelection();
        if (!treeItem.IsOk()) break;

        const FileTreeData* data = static_cast<FileTreeData*>(tree->GetItemData(treeItem));
        if (!data) break;

        if (data->GetKind() == FileTreeData::ftdkProject)
        {
            cbProject* project = data->GetProject();
            if (project)
            {
                ClearReparseConditions();

                // Send a quit command with cleanup flag
                if (GetLSPClient(project))
                    GetLSPClient(project)->SetProjectNeedsCleanup(project->GetFilename());
                ShutdownLSPclient(project);
                // Close and create a new parser
                GetParseManager()->ReparseSelectedProject();
                // Then create a new ProcessLanguageClient
                Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(project);
                // LSP_DidOpen() any active file in an editor belong to this project
                if (pParser)
                {
                    // The new parser has already queued files to be parsed.
                    // Freeze parsing for this parser and create a client.
                    // The response to LSP initialization will unfreeze the parser.
                    if ( int cnt = pParser->PauseParsingForReason("AwaitClientInitialization", true) > 1)
                    {
                        // error if greater than 1 **Debugging**
                        wxString msg = wxString::Format("%s: AwaitClientInitialization count(%d) > 1", __FUNCTION__, cnt);
                        //cbMessageBox(msg, "Error");
                        Manager::Get()->GetLogManager()->DebugLogError(msg);
                    }
                    ProcessLanguageClient* pClient = GetParseManager()->CreateNewLanguageServiceProcess(project, LSPeventID);
                    if (not pClient)
                    {
                        // stop the batch parse timer and clear the Batch parsing queue
                        pParser->ClearBatchParse();
                        wxString msg = wxString::Format(_("%s failed to create an LSP client"), __FUNCTION__);
                        cbMessageBox(msg, _("Error"));
                        return;
                    }

                    // Issue idle event to do DidOpen()s for this parser.
                    // It will await client initialization, then do client DidOpen()s for
                    // this new parser/client process before allowing parsing to proceed.
                    //  Here's the re-schedule call for the Idle time Callback queue
                    GetParseManager()->GetIdleCallbackHandler(project)->QueueCallback(pParser, &Parser::LSP_OnClientInitialized, project);
                }
            }
        }
    }//end switch(1)

    return;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnSelectedPauseParsing(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    //    // if Alt-Shift keys are down, toggle ccLogger external logging on/off
    //    if (wxGetKeyState(WXK_ALT) && wxGetKeyState(WXK_SHIFT))
    //    {
    //        bool logStat = CCLogger::Get()->GetExternalLogStatus();
    //        logStat = (not logStat);
    //        CCLogger::Get()->SetExternalLog(logStat);
    //        wxString infoTitle = wxString::Format(_("External CCLogging is %s"), logStat?_("ON"):_("OFF"));
    //        wxString infoText = wxString::Format(_("External CCLogging now %s"), logStat?_("ON"):_("OFF"));
    //        InfoWindow::Display(infoTitle, infoText, 7000);
    //
    //        return;
    //    }

    //    // **Debugging**
    //    // if shift key is down, do a client Shutdown to allow testing (restart, menu status etc)
    //    // without having to debug the debugees debugee (two levels of debuggers).
    //    if (wxGetKeyState(WXK_SHIFT))
    //    {
    //        cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    //        if (pProject)
    //            ShutdownLSPclient(pProject);
    //        return;
    //    }

    //Toggle pause LSP parsing on or off for selected project
    wxTreeCtrl* tree = Manager::Get()->GetProjectManager()->GetUI().GetTree();
    if (!tree) return;

    wxTreeItemId treeItem = Manager::Get()->GetProjectManager()->GetUI().GetTreeSelection();
    if (!treeItem.IsOk()) return;

    const FileTreeData* data = static_cast<FileTreeData*>(tree->GetItemData(treeItem));
    if (!data) return;

    if (data->GetKind() == FileTreeData::ftdkProject)
    {
        cbProject* pProject = data->GetProject();
        if (not pProject) return;

        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        if (pParser) //active parser
        {
            wxString projectTitle = pProject->GetTitle();
            bool paused = pParser->GetUserParsingPaused();
            paused = (not paused);
            pParser->SetUserParsingPaused(paused);
            wxString infoTitle = wxString::Format(_("Parsing is %s"), paused?_("PAUSED"):_("ACTIVE"));
            wxString infoText = wxString::Format(_("%s parsing now %s"), projectTitle, paused?_("PAUSED"):_("ACTIVE"));
            infoText << _("\nRight-click project in  Projects tree to toggle.");
            InfoWindow::Display(infoTitle, infoText, 7000);
        }
    }
}//end OnSelectedPauseParsing
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectPauseParsing(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    //Toggle pause LSP parsing on or off for project
    // event string must contain "on" or "off",else status will toggle

        cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pProject) return;

        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        if (pParser) //active parser
        {
            wxString projectTitle = pProject->GetTitle();
            bool paused = pParser->GetUserParsingPaused();
            paused = (not paused); //toggle
            if (event.GetString() == "on") paused = true;
            if (event.GetString() == "off") paused = false;
            pParser->SetUserParsingPaused(paused);
            wxString infoTitle = wxString::Format(_("Parsing is %s"), paused?_("PAUSED"):_("ACTIVE"));
            wxString infoText = wxString::Format(_("%s parsing now %s"), projectTitle, paused?_("PAUSED"):_("ACTIVE"));
            infoText << _("\nRight-click project in  Projects tree to toggle.");
            InfoWindow::Display(infoTitle, infoText, 7000);
        }
}//end OnSelectedPauseParsing
// ----------------------------------------------------------------------------
void ClgdCompletion::OnSelectedFileReparse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    event.Skip();
    return OnLSP_SelectedFileReparse(event);

}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnLSP_SelectedFileReparse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // Reparse a file selected from project tree

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

        if (project and pf)
        {
            ProcessLanguageClient* pClient = GetLSPClient(project);
            if (not pClient) return;
            Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(project);
            if (pParser) ClearReparseConditions();

            // if file is open in editor, send a didSave() causing a clangd reparse
            // if file is not open in editor do a didOpen()/didClose() sequence
            //      to cause a background parse.
            EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
            wxString filename = pf->file.GetFullPath();
            cbEditor* pEditor = pEdMgr->GetBuiltinEditor(filename);
            if (pEditor) pClient->LSP_DidSave(pEditor);
            else {
                // do a background didOpen(). It will be didClose()ed in OnLSP_RequestedSymbolsResponse();
                // If its a header file, OnLSP_DiagnosticsResponse() will do the LSP idClose().
                // We don't ask for symbols on headers because they incorrectly clobbler the TokenTree .cpp symbols.
                pClient->LSP_DidOpen(filename, project);
            }
        }
    }

    event.Skip();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::DisplayPausedStatusOfAllProjects()
// ----------------------------------------------------------------------------
{
    // Get array of projects in the workspace
    // Get paused reasons for each project
    // Display each project and the pause reasons.

    LogManager* pLogMgr = Manager::Get()->GetLogManager();
    ProjectManager* pPrjMgr = Manager::Get()->GetProjectManager();
    ProjectsArray* pPrjArray = pPrjMgr->GetProjects();
    for (size_t ii=0; ii < pPrjArray->GetCount(); ++ii)
    {
        cbProject* pProject = pPrjArray->Item(ii);
        wxArrayString pauseReasons;

        Parser* pParser = GetParseManager()->GetParserByProject(pProject);
        wxString msg = pProject->GetTitle() + ": ";
        if (not pParser)
        {   msg += "Never activated";
        }
        else //have project with parser
        {
            pParser->GetArrayOfPauseParsingReasons(pauseReasons);
            if (not pauseReasons.GetCount())
                msg.Append(": No pause reasons.");
            for (size_t ii=0; ii<pauseReasons.GetCount(); ++ii)
                msg.Append(pauseReasons[ii] + "; ");
        }
        if (0 == ii) msg.Prepend('\n');
        pLogMgr->DebugLog(msg);
    }
}

// ----------------------------------------------------------------------------
void ClgdCompletion::OnActiveEditorFileReparse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // Redirection to OnLSP_EditorFileReparse()
    return OnLSP_EditorFileReparse(event);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnLSP_EditorFileReparse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor) return;
    wxFileName fnFilename = pEditor->GetFilename();
    cbProject* pProject = nullptr;

    if (fnFilename.Exists())
    {
        ProjectFile* pf = pEditor->GetProjectFile();
        pProject = pf ? pf->GetParentProject() : nullptr;
        if (pProject and pf)
        {
            ProcessLanguageClient* pClient = GetLSPClient(pProject);
            if (not pClient)
            {
                wxString msg = _("The project needs to be parsed first.");
                msg << _("\n Reason: Did not find associated Clangd client");
                //cbMessageBox(msg, __FUNCTION__);
                InfoWindow::Display(__FUNCTION__, msg, 7000);
                return;
            }
            // if file is open in editor, send a didSave() causing a clangd reparse
            // if file is not open in editor do a didOpen()/didClose() sequence
            //      to cause a background parse.
            wxString filename = pf->file.GetFullPath();

            // **Debugging** show status of parse pausing map
            //wxArrayString pauseParsingReasons;
            //Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
            //if (pParser) pParser->GetArrayOfPauseParsingReasons(pauseParsingReasons);
            //LogManager* pLogMgr = Manager::Get()->GetLogManager();
            //for (size_t ii=0; ii < pauseParsingReasons.GetCount(); ++ii)
            //{
            //    if (0==ii) pLogMgr->DebugLog("--- Parser pause reasons ---");
            //    pLogMgr->DebugLog("\t" + pauseParsingReasons[ii]);
            //}
            ClearReparseConditions();

            #if defined(MEASURE_wxIDs)
             // **Debugging** 3 ids are used up betwn reparses
            CCLogger::Get()->SetGlobalwxIDStart(__FUNCTION__, __LINE__) ;
            #endif

            if (pEditor and pClient and pClient->GetLSP_IsEditorParsed(pEditor))
                pClient->LSP_DidSave(pEditor);
            else {
                // do didOpen(). It will be didClose()ed in OnLSP_RequestedSymbolsResponse();
                // If its a header file, OnLSP_DiagnosticsResponse() will do the LSP didClose().
                // We don't ask for symbols on headers because they incorrectly clobbler the TokenTree .cpp symbols.

                //-pClient->LSP_DidOpen(filename, pProject); //for file not in editor
                if (pClient->GetLSP_EditorIsOpen(pEditor))
                    pClient->LSP_DidClose(pEditor);
                if (pClient) pClient->LSP_DidOpen(pEditor); //for file in editor
            }
        }//endif project and pf
        else
        {
            wxString msg = _("File does not appear to be included within a project.");
            cbMessageBox(msg, "__FUNCTION__");
        }
    }//end if exists
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnSpecifiedFileReparse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // Reparse a file by filename within open cbEditor
    wxString filename = event.GetString();
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor(filename);
    if (not pEditor) return;

    wxFileName fnFilename = pEditor->GetFilename();

    if (fnFilename.Exists())
    {
        ProjectFile* pf = pEditor->GetProjectFile();
        cbProject* pProject = pf ? pf->GetParentProject() : nullptr;
        if (pProject and pf)
        {
            ProcessLanguageClient* pClient = GetLSPClient(pProject);
            if (not pClient)
            {
                wxString msg = _("The project needs to be parsed first.");
                msg << _("\n Did not find associated Clangd client.");
                //cbMessageBox(msg, __FUNCTION__);
                InfoWindow::Display(__FUNCTION__, msg, 7000);
                return;
            }
            // if file is open in editor, send a didSave() causing a clangd reparse
            // if file is not open in editor do a didOpen()/didClose() sequence
            //      to cause a background parse.
            wxString filename = pf->file.GetFullPath();

            // **Debugging** show status of parse pausing map
            //wxArrayString pauseParsingReasons;
            //Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
            //if (pParser) pParser->GetArrayOfPauseParsingReasons(pauseParsingReasons);
            //LogManager* pLogMgr = Manager::Get()->GetLogManager();
            //for (size_t ii=0; ii < pauseParsingReasons.GetCount(); ++ii)
            //{
            //    if (0==ii) pLogMgr->DebugLog("--- Parser pause reasons ---");
            //    pLogMgr->DebugLog("\t" + pauseParsingReasons[ii]);
            //}
            ClearReparseConditions();

            if (pEditor and pClient and pClient->GetLSP_IsEditorParsed(pEditor))
                pClient->LSP_DidSave(pEditor);
            else {
                // do didOpen(). It will be didClose()ed in OnLSP_RequestedSymbolsResponse();
                // If its a header file, OnLSP_DiagnosticsResponse() will do the LSP idClose().
                // We don't ask for symbols on headers because they incorrectly clobbler the TokenTree .cpp symbols.
                pClient->LSP_DidOpen(filename, pProject);
            }
            wxString msg = _("LSP Reparsing: "); msg << fnFilename.GetFullName();
            Manager::Get()->GetLogManager()->DebugLog(msg);

        }//endif project and pf
        else
        {
            wxString msg = _("File does not appear to be included within a project.\n");
            //cbMessageBox(msg, "__FUNCTION__");
            msg << fnFilename.GetPath() << "\n" << fnFilename.GetFullName();
            InfoWindow::Display("Reparsing File", msg, 8000);

        }
    }//end if exists
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnPluginEnabled()
// ----------------------------------------------------------------------------
{
    // This is a CallAfter() from OnPluginAttached()
    CodeBlocksEvent evt(cbEVT_APP_STARTUP_DONE);
    OnAppStartupDone(evt);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnAppStartupDone(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // ----------------------------------------------------------------------------
    // Verify an existent clangd.exe path before creating a proxy project
    // ----------------------------------------------------------------------------
    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");
    wxString cfgClangdMasterPath = cfg->Read("/LLVM_MasterPath", wxEmptyString);

    if (cfgClangdMasterPath.Length())
    {
        Manager::Get()->GetMacrosManager()->ReplaceMacros(cfgClangdMasterPath);
        wxFileName fnClangdName(cfgClangdMasterPath);
        if ( ( not fnClangdName.FileExists()) || (not fnClangdName.GetFullName().Lower().StartsWith(clangdexe)) )
        {
            wxString msg;
            msg << wxString::Format(_("The clangd path:\n'%s' does not exist."), cfgClangdMasterPath);
            msg << _("\nUse Settings/Editor/Clangd_client/ 'C/C++ parser' tab to set it's path.");
            msg << _("\n\nThis requires a restart of CodeBlocks for Clangd to function correctly.");
            wxWindow* pTopWindow = GetTopWxWindow();
            cbMessageBox(msg, _("ERROR: Clangd client"), wxOK, pTopWindow);
            return;
        }
    }
    else //no cfgClangMasterPath set
    {
        ClangLocator clangLocator;
        wxFileName fnClangdPath(clangLocator.Locate_ClangdDir(), clangdexe);

        wxString msg;
        msg << _("The clangd path has not been set.\n");

        if (fnClangdPath.FileExists())
        {
            msg << _("\nThe clangd detected is:");
            msg <<   "\n'" << fnClangdPath.GetFullPath() << "'.";
            msg << _("\n\nTo use a different clangd, use the Settings/Editor/Clangd_client \n'C/C++ parser' tab to set it's path.");
            msg << _("\nThis REQUIRES A RESTART of CodeBlocks for Clangd to function correctly.");
            msg << _("\n\nDo you want to use the detected clangd?");

            if (cbMessageBox(msg, _("ERROR: Clangd client"), wxICON_QUESTION | wxYES_NO, GetTopWxWindow() ) == wxID_YES)
            {
                cfg->Write("/LLVM_MasterPath", fnClangdPath.GetFullPath());
                // update cached options
                GetParseManager()->RereadParserOptions();
                msg = _("Please RESTART CodeBlocks for this change to take effect.");
                cbMessageBox(msg, _("WARNING: Clangd client"), wxICON_WARNING | wxOK, GetTopWxWindow());
            }
        }
        else
        {
            msg << _("\nUse Settings/Editor/Clangd_client/ 'C/C++ parser' tab to set it's path.");
            msg << _("\n\nThis requires a restart of CodeBlocks for Clangd to function correctly.");

            wxWindow* pTopWindow = GetTopWxWindow();
            cbMessageBox(msg, _("ERROR: Clangd client"), wxOK, pTopWindow);
        }

        return;
    }

    if (!m_InitDone)
    {
        // Set a timer callback to allow time for the splash screen to clear.
        // Allows CB to appear to start faster.
        // Timer pop will call DoParseOpenedProjectAndActiveEditor() to do further work.
        m_TimerStartupDelay.Start(300, wxTIMER_ONE_SHOT);
    }
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnWorkspaceClosingBegin(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    GetParseManager()->ClearAllIdleCallbacks();
    if(GetParseManager()->GetProxyProject())
        GetParseManager()->GetProxyProject()->SetModified(false);
    m_WorkspaceClosing = true;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnWorkspaceClosingEnd(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    if (m_WorkspaceClosing
            and (not ProjectManager::IsBusy())
            and (not Manager::IsAppShuttingDown()) )
    {
        ProjectsArray* pProjectsArray = Manager::Get()->GetProjectManager()->GetProjects();
        int prjCount = 0; // count of open projects
        if (pProjectsArray) prjCount = pProjectsArray->GetCount();
        if (not prjCount) // projects must be closed
            GetParseManager()->UpdateClassBrowser(/*force*/true);
    }

    m_WorkspaceClosing = false;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnWorkspaceChanged(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // EVT_WORKSPACE_CHANGED is a powerful event, it's sent after any project
    // has finished loading or closing. It's the *LAST* event to be sent when
    // the workspace has been changed. So it's the ideal time to parse files
    // and update your widgets.

    if (m_WorkspaceClosing)
        return;

    if (IsAttached() && m_InitDone)
    {
        // Hide the ~ProxyProject~ because workspace/project tree was redrawn
        cbProject* pProxyProject = GetParseManager()->GetProxyProject();
        if (pProxyProject)
            ;// testing Manager::Get()->GetProjectManager()->GetUI().RemoveProject(pProxyProject);

        cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        // if we receive a workspace changed event, but the project is NULL, this means two conditions
        // could happen.
        // (1) the user closed the application, so we don't need to update the UI here.
        // (2) the user just opened a new project after cb started up
        if (pActiveProject)
        {
            bool LSPsucceeded = false;
            if (not GetParseManager()->GetParserByProject(pActiveProject))
            {
                GetParseManager()->CreateParser(pActiveProject);
                Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pActiveProject);
                if (pParser and (not pParser->GetLSPClient()) )
                    LSPsucceeded = GetParseManager()->CreateNewLanguageServiceProcess(pActiveProject, LSPeventID);

                // Pause parsing if this is a makefile project
                if ( pParser and LSPsucceeded and pActiveProject->IsMakefileCustom())
                {
                    wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idProjectPauseParsing);
                    evt.SetString("on"); // turn on pause
                    cbPlugin* pPlgn = Manager::Get()->GetPluginManager()->FindPluginByName("clangd_client");
                    if (pPlgn) pPlgn->ProcessEvent(evt);
                }

            }//if not parser

            // Update the Function toolbar
            TRACE(_T("CodeCompletion::OnWorkspaceChanged: Starting m_TimerToolbar."));
            m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);

            // Update the class browser
            if (GetParseManager()->GetParser().ClassBrowserOptions().displayFilter == bdfProject)
            {
                s_ClassBrowserCaller = wxString::Format("%s:%d",__FUNCTION__, __LINE__);
                GetParseManager()->UpdateClassBrowser();
            }

            // ----------------------------------------------------------------------------
            // create LSP process for any editor of the active project that may have been missed during project loading
            // ----------------------------------------------------------------------------
            EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
            if (LSPsucceeded)
            for (int ii=0; ii< pEdMgr->GetEditorsCount(); ++ii)
            {
                cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(ii);
                if (pcbEd)
                {
                    // don't re-open an already open editor
                    // An opened editor will have, at least, a didOpen requested
                    ProcessLanguageClient* pClient = GetLSPClient(pcbEd);
                    if (pClient) continue; //file already processed

                    // Find the ProjectFile and project containing this editors file.
                    ProjectFile* pProjectFile = pcbEd->GetProjectFile();
                    if (not pProjectFile) continue;
                    cbProject* pEdProject = pProjectFile->GetParentProject();
                    // For LSP, file must belong to a project, because LSP needs target compile parameters.
                    if (not pEdProject) continue;
                    if (pEdProject != pActiveProject) continue;
                    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pActiveProject);
                    if (not pParser) continue;
                    if (pParser->GetLSPClient()) continue;
                    // creating the client/server, will initialize it and issue LSP didOpen for its open project files.
                    GetParseManager()->CreateNewLanguageServiceProcess(pActiveProject, LSPeventID);
                }//endif pcbEd
            }//endfor editor count
        }//endif project

    }//endif attached

    event.Skip();
}//end onWorkspaceChanged
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectOpened(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // This event is called once when a project is opened
    // When a new project is created, this event is called twice.
    // Once with evtProject != activeproject
    // Then WorkspaceChanged is called
    // then ProjectActivated is called
    // then this event is called again with evtProject == activeproject

    cbProject* evtProject = event.GetProject();
    cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (evtProject == activeProject)    //check for both parser and clangd_client created
    {
        // This is the second call for ProjectOpened (ususally a new project was created).
        // If no parser, then time to create one.
        if ((not GetParseManager()->GetParserByProject(activeProject)))
            OnProjectActivated(event);
    }
    return;
}

// ----------------------------------------------------------------------------
void ClgdCompletion::OnReActivateProject(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // event issued locally to re-activate the project after m_InitDone is set.
    // Do NOT event.skip(); own the event
    if (not m_InitDone) return;
    CodeBlocksEvent evt;
    evt.SetProject(Manager::Get()->GetProjectManager()->GetActiveProject() );
    if (evt.GetProject() )
        OnProjectActivated(evt);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectActivated(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // FYI: OnEditorOpen can occur before this project activate event

    if (m_PrevProject != m_CurrProject) m_PrevProject = m_CurrProject;
    m_CurrProject = event.GetProject();

    ProjectManager* pPrjMgr = Manager::Get()->GetProjectManager();
    //LogManager*     pLogMgr = Manager::Get()->GetLogManager();

    if ((not ProjectManager::IsBusy()) && IsAttached() && m_InitDone)
    {
        cbProject* project = event.GetProject();

        if (project && (not GetParseManager()->GetParserByProject(project))
                                                    && project->GetFilesCount() > 0)
            GetParseManager()->CreateParser(project); //Reads options and connects events to new parser

        if (GetParseManager()->GetParser().ClassBrowserOptions().displayFilter == bdfProject)
        {
            s_ClassBrowserCaller = wxString::Format("%s:%d",__FUNCTION__, __LINE__);
            GetParseManager()->UpdateClassBrowser();
        }
    }

    // when debugging, the cwd may be the executable path, not project path
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    wxString projDir =  pProject->GetBasePath();
    if (wxGetCwd().Lower() != projDir.Lower())
        wxSetWorkingDirectory(projDir);

    m_NeedsBatchColour = true;
    // ----------------------------------------------------------------
    // LSP
    // ----------------------------------------------------------------
    // OnProjectOpened may have already started a LSP client/server for the project.
    // Do not use IsBusy(). IsLoading() is true even though CB is activating the last loaded project.
    if (IsAttached() && m_InitDone & (not pPrjMgr->IsClosingWorkspace()) )
    {
        cbProject* pProject = event.GetProject();

        if ( (not GetLSPClient(pProject)) //if no project yet
            and GetParseManager()->GetParserByProject(pProject) ) // but has Parser
            GetParseManager()->CreateNewLanguageServiceProcess(pProject, LSPeventID);
        // Pause parsing for the previous, deactivated project
        if (m_PrevProject and (m_PrevProject != m_CurrProject) )
        {
            Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(m_PrevProject);
            if (pParser)
                pParser->PauseParsingForReason("Deactivated",true);
        }
        // Unpause parsing for previously deactivated project
        if (m_CurrProject and GetParseManager()->GetParserByProject(m_CurrProject))
        {
            Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(m_CurrProject);
            if (pParser and pParser->PauseParsingCount("Deactivated"))
                pParser->PauseParsingForReason("Deactivated",false);
        }
        // Pause parsing if this is a makefile project
        if (m_CurrProject->IsMakefileCustom())
        {
            wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idProjectPauseParsing);
            evt.SetString("on"); // turn on pause
            cbPlugin* pPlgn = Manager::Get()->GetPluginManager()->FindPluginByName("clangd_client");
            if (pPlgn) pPlgn->ProcessEvent(evt);
        }

    }//endif attached

    // During project setup, OnEditorActivated() is only called when the Project Manager is busy.
    // After the Project Manager is set NOT busy, OnEditorActivated() is never called from
    // CB to activate the currently active editor. Do it here.
    EditorBase* pActiveEditor = Manager::Get()->GetEditorManager()->GetActiveEditor();
    if ((not ProjectManager::IsBusy()) && IsAttached() && m_InitDone && pActiveEditor)
    {
        // clear the m_LastFile so parser re-records the active editor filename
        EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
        EditorBase* pEditor = pEdMgr->GetActiveEditor();
        wxString activeEdFilename = pEditor ? pEditor->GetFilename() : wxString();
        if ( pEditor and (activeEdFilename == m_LastFile))
            m_LastFile.Clear();
        CodeBlocksEvent edEvent;
        edEvent.SetEditor(pActiveEditor);
        edEvent.SetOldEditor(nullptr);
        OnEditorActivated(edEvent);
    }

}//end OnProjectActivated()
// ----------------------------------------------------------------------------
void ClgdCompletion::OnLSP_ProcessTerminated(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    cbProject* pProject = (cbProject*)event.GetEventObject();
    #if defined(cbDEBUG)
    cbAssertNonFatal(pProject && "Entered with null project ptr.")
    #endif
    if (not pProject) return;

    //The pipe died, not the client.
    ProcessLanguageClient* pClient = GetLSPClient(pProject);

    //The I/O pipeProcess died for this project, close the LSP client and the Parser
    if (pClient)
    {
        wxString msg = _("Unusual termination of LanguageProcessClient(LSP) occured.");
        msg += "\n\n" + _("Project: ") + pProject->GetTitle();
        if (pClient->lspClientLogFile.IsOpened() )
            msg += "\n" + _("Client Log: ") + pClient->lspClientLogFile.GetName();
        if (pClient->lspServerLogFile.IsOpened() )
            msg += "\n" + _("Server Log: ") + pClient->lspServerLogFile.GetName();
        //#if defined(_WIN32)
        cbMessageBox(msg, "clangd client"); //Crashes with X window error on Linux Mint 20.2
        //#else
        msg.Replace("\n\n","\n");
        CCLogger::Get()->LogError(msg);
        CCLogger::Get()->DebugLogError(msg);
        //#endif

        ShutdownLSPclient(pProject);
        // MS Windows had a lock on closed clangd logs until the project was closed.
        CleanUpLSPLogs();
        GetParseManager()->DoUnlockClangd_CacheAccess(pProject);
        CleanOutClangdTempFiles();
        if (pProject && GetParseManager()->GetParserByProject(pProject))
        {
           // remove the CC Parser instance associated with the project
           GetParseManager()->DeleteParser(pProject);
        }
    }

    return;
}
// ----------------------------------------------------------------------------
wxString ClgdCompletion::GetFilenameFromLSP_Response(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    json* pJson = (json*)event.GetClientData();

    // fetch filename from json id
    wxString URI;
    wxString eventString = event.GetString();
    if (eventString.Contains("id:"))
    {
        try{
            URI = GetwxUTF8Str(pJson->at("id:").get<std::string>());
        }catch(std::exception &e) {
            return wxString();
        }
    }
    else if (eventString.Contains(STXstring+"params")) //textdocument/<something>
    {
        try {
            URI = GetwxUTF8Str(pJson->at("params").at("uri").get<std::string>());
        }catch(std::exception &e) {
            return wxString();
        }
    }
    else if (eventString.Contains(STXstring + "file:/")) //texdocument/symbols etc
    {
        URI = eventString.AfterFirst(STX);
        if (URI.Contains(STXstring))
            URI = URI.BeforeFirst(STX);

    }
    else if (eventString.Contains(STXstring + "result")) //textdocument/declaration etc
    {
        // This response can have multiple uri's
        // And there's no data to determine for what original file
        // Let the active project handle it.
        return wxString();
    }
    else return wxString();

    if (URI.empty()) return wxString();
    wxFileName fnFilename = fileUtils.FilePathFromURI(URI);
    wxString filename = fnFilename.GetFullPath();
    if (filename.Length()) return filename;
    return wxString();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnLSP_Event(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{

    if (Manager::IsAppShuttingDown())
        return;

    wxString evtString = event.GetString();
    ProcessLanguageClient* pClient = (ProcessLanguageClient*)event.GetEventObject();
    cbProject* pProject = nullptr;
    wxString filename = GetFilenameFromLSP_Response(event);
    if (pClient)
    {
        // Find the project that owns this file
        pProject = GetParseManager()->GetProjectByClientAndFilename(pClient, filename);
        if (not pProject)
        {
            //There's no associated filename with the initialization event.
            if (evtString.StartsWith("LSP_Initialized:"))
                pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
            if (not pProject)
            {
                wxString msg = wxString::Format("%s(): No project found for file <%s>",__FUNCTION__, filename);
                msg += "\nEvent String was: " + evtString;
                CCLogger::Get()->DebugLogError(msg);
            }
        }

    }

    // Verify Client and Project, but let the clangd response proceed anyway.
    if ( not (pClient and pProject) )
    {
        wxString msg;
        if (not pClient)
            msg = "OnLSP_Event() without a client ptr (see CB Debug log)" ;
        else if (not pProject)
            msg = "OnLSP_Event() without a Project pointer";
        cbMessageBox(msg, "OnLSP_Event()");
    }

    // ----------------------------------------------------------------------------
    ///Take ownership of event client data pointer to assure it's freed
    // ----------------------------------------------------------------------------
    std::unique_ptr<json> pJson;
    pJson.reset( (json*)event.GetClientData() );

    // create an event id from the the LSP event string, eg.,"textDocument/definition"STX"RRID####"
    // Extract the RequestResponseID from the event string
    // *unused for now* int lspID = XRCID(evtString.BeforeFirst(STX)); //LSP request type
    long lspRRID = 0;
    int posn = wxNOT_FOUND;
    if ( wxFound(posn = evtString.Find(wxString(STX) + "RRID")) )
    {
        wxString RRIDstr = evtString.Mid(posn+1);
        RRIDstr = RRIDstr.BeforeFirst(STX); //eliminate any other trailing strings
        RRIDstr = RRIDstr.Mid(4);           //skip over 'RRID" to char number
        bool ok = RRIDstr.ToLong(&lspRRID);
        if (not ok) lspRRID = 0;
    }
    // ----------------------------------------------------------------------------
    // Check for LSP Error message
    // ----------------------------------------------------------------------------
    if (evtString.EndsWith(wxString(STX) + "error"))
    {
        if (pJson->contains("error"))
        {
            wxString errorMsg = GetwxUTF8Str(pJson->at("error").at("message").get<std::string>());
            if (errorMsg == "drop older completion request")
                return;
            if (errorMsg == "Request cancelled because the document was modified")
                return;
            if (errorMsg == "invalid AST")
            {
                // Queue this file for a reparse. Error usually caused during or after
                // running the debugger.
                //11:22:37.546 >>> readJson() len:207:{"error":{"code":-32001,"message":"invalid AST"},"id":"textDocument/semanticTokens/full\u0002file:///F:/usr/Proj/Clangd_Client-work/trunk/clangd_client/src/codecompletion/codecompletion.cpp","jsonrpc":"2.0"}
                if ( filename.Length() and wxFileExists(filename) )
                {
                    wxCommandEvent reParseEvt(wxEVT_COMMAND_MENU_SELECTED, idSpecifiedFileReparse);
                    reParseEvt.SetString(filename);
                    Manager::Get()->GetAppWindow()->GetEventHandler()->AddPendingEvent(reParseEvt);
                }
                return;
            }//if errormsg

            CCLogger::Get()->DebugLogError("LSP Error: " + errorMsg);
            //cbMessageBox("LSP: " + errorMsg); // **Debugging**
            InfoWindow::Display("LSP Error", errorMsg);
        }

        // clear any call back for this request/response id
        if (lspRRID)
        {
            GetLSPEventSinkHandler()->ClearLSPEventCallback(lspRRID);
            return;
        }
    }

    // ----------------------------------------------------------------------------
    // Invoke any queued call backs first; if none, fall into default processing
    // ----------------------------------------------------------------------------
    if (GetLSPEventSinkHandler()->Count() and lspRRID)
    {
        GetLSPEventSinkHandler()->OnLSPEventCallback(lspRRID, event);
        // FIXME (ph#): shouldn't this loop to check for more callbacks?
        // So far, it hasn't affected anything, even performance //(2022/04/8)
        return;
    }
    // **Debugging**
    //    wxString msg = "LSP_Event recieved: " + evtString;
    //    Manager::Get()->GetLogManager()->DebugLog(msg);
    // ----------------------------------------------------
    // LSP client/server Initialized
    // ----------------------------------------------------
    if ( evtString.StartsWith("LSP_Initialized:true"))
    {

        // ----------------------------------------------------------------------
        // capture the semanticTokens legends returned from LSP initialization response
        //https://microsoft.github.io/language-server-protocol/specification#textDocument_semanticTokens
        // ----------------------------------------------------------------------
        try{
            if (pJson->at("result")["capabilities"].contains("semanticTokensProvider") )
            {
                json legend = pJson->at("result")["capabilities"]["semanticTokensProvider"]["legend"];
                m_SemanticTokensTypes = legend["tokenTypes"].get<std::vector<std::string>>();
                m_SemanticTokensModifiers = legend["tokenModifiers"].get<std::vector<std::string>>();
                // **Debugging**  log the semantic tokens data
                //    LogManager* pLogMgr = Manager::Get()->GetLogManager();
                //    pLogMgr->DebugLog("--- SemanticTokensTypes ---");
                //    for (size_t ii=0; ii<m_SemanticTokensTypes.size(); ++ii)
                //        pLogMgr->DebugLog(m_SemanticTokensTypes[ii]);
                //    pLogMgr->DebugLog("--- SemanticTokensModifiers ---");
                //    for (size_t ii=0; ii<m_SemanticTokensModifiers.size(); ++ii)
                //        pLogMgr->DebugLog(m_SemanticTokensModifiers[ii]);
            }//endif
        }catch (std::exception &e)
        {
            wxString msg = wxString::Format("%s() %s", __FUNCTION__, e.what());
            cbMessageBox(msg, "LSP initialization");
            return;
        }

        return;
    }//endif LSP_Initialized

    // ----------------------------------------------------------------------------
    // textDocument/declaration textDocument/definition event
    // ----------------------------------------------------------------------------
    // LSP Result of FindDeclaration or FindImplementation
    // a CB "find declaration"   == Clangd/Clangd "declaration/signature"
    // a CB "find implementation == Clangd/Clangd "definition"
    // this event.string contains type of eventType:result or error
    // example:
    // {"jsonrpc":"2.0","id":"textDocument/definition","result":[{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","range":{"start":{"line":89,"character":24},"end":{"line":89,"character":30}}}]}

    bool isDecl = false; bool isImpl = false;
    if (evtString.StartsWith("textDocument/declaration") )
        isDecl = true;
    else if (evtString.StartsWith("textDocument/definition") )
        isImpl = true;

    if ( isImpl or isDecl)
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        return pParser->OnLSP_DeclDefResponse(event); //default processing
    }

    // ----------------------------------------------------
    // textDocument references event
    // ----------------------------------------------------
    if (evtString.StartsWith("textDocument/references") )
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_ReferencesResponse(event);
        return;
    }
    // ----------------------------------------------------------------------------
    // textDocument/DocumentSymbol event
    // ----------------------------------------------------------------------------
    else if (evtString.StartsWith("textDocument/documentSymbol") )
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_RequestedSymbolsResponse(event);

    }//end textDocument/documentSymbol
    // ----------------------------------------------------------------------------
    // textDocument/publishDiagnostics method
    // ----------------------------------------------------------------------------
    else if (evtString.StartsWith("textDocument/publishDiagnostics") )
    {
        // There might be two for each file: an empty one for DidClose() and a good one for DidOpen()
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_DiagnosticsResponse(event);

    }//endiftextDocument/publishDiagnostics
    // ----------------------------------------------------------------------------
    // Completion event
    // ----------------------------------------------------------------------------
    else if ( evtString.StartsWith("textDocument/completion"))
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_CompletionResponse(event, m_CompletionTokens);
    }
    // ----------------------------------------------------------------------------
    // Hover event
    // ----------------------------------------------------------------------------
    else if ( evtString.StartsWith("textDocument/hover"))
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_HoverResponse(event, m_HoverTokens, m_HoverLastPosition);
    }
    // ----------------------------------------------------------------------------
    // SignatureHelp event
    // ----------------------------------------------------------------------------
    else if ( evtString.StartsWith("textDocument/signatureHelp"))
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_SignatureHelpResponse(event, m_SignatureTokens, m_HoverLastPosition);
    }
    // ----------------------------------------------------------------------------
    // "textDocument/rename" event
    // ----------------------------------------------------------------------------
    else if ( evtString.StartsWith("textDocument/rename"))
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_RenameResponse(event);
    }
    else if (evtString.StartsWith("textDocument/semanticTokens") )
    {
        Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        pParser->OnLSP_RequestedSemanticTokensResponse(event);
    }
}
// ----------------------------------------------------------------------------
void ClgdCompletion::ShutdownLSPclient(cbProject* pProject)
// ----------------------------------------------------------------------------
{
    if (IsAttached() && m_InitDone)
    {
        // ------------------------------------------------------------
        //  LSP client/server
        // ------------------------------------------------------------
        ProcessLanguageClient* pClient = GetLSPClient(pProject);
        if (pClient)
        {
            // Stop all parsing for this parser
            Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
            if (pParser)
                pParser->PauseParsingForReason("ShutDown", true);

            // If project is the current active project do a didClose for its files.
            // Tell LSP server to didClose() all open files for this project, then delete server for this project
            // If project is not active, LSP server needs to do nothing.

            // Get current time
            int startMillis = pClient->GetDurationMilliSeconds(0);

            // Tell LSP we closed all open files for this project
            EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
            if (pClient and pClient->Has_LSPServerProcess())
                for (int ii=0; ii< pEdMgr->GetEditorsCount(); ++ii)
                {
                    cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(ii);
                    if (not pcbEd) continue; //happens because of "Start here" tab
                    ProjectFile* pPrjFile = pcbEd->GetProjectFile();
                    if (not pPrjFile) continue;
                    if (pPrjFile->GetParentProject() == pProject)
                        GetLSPClient(pProject)->LSP_DidClose(pcbEd);
                }
            long closing_pid = pClient->GetLSP_Server_PID();

            // Tell LSP server to quit
            pClient->LSP_Shutdown();
            GetParseManager()->m_LSP_Clients.erase(pProject); // erase first or crash
            delete pClient;
            pClient = nullptr;

            // The clangd process is probably already terminated by LSP_Shutdown above.
            // but it sometimes gets stuck waiting for cpu access
            int waitLimit = 40; //40*50mils = 2 seconds max wait for clangd to terminate
            while( (waitLimit > 0) and (not Manager::IsAppShuttingDown()) )
            {
                wxString cmdLine = ProcUtils::GetProcessNameByPid(closing_pid);
                if (cmdLine.empty()) break;
                if (cmdLine.Contains("defunct")) break; //Linux
                if (not Manager::IsAppShuttingDown())
                    Manager::Yield(); //give time for process to shutdown
                wxMilliSleep(50);
                waitLimit -= 1;
            }

            pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
            if (pParser) pParser->SetLSP_Client(nullptr);

            // The event project just got deleted, see if there's another project we can use
            // to get client info.
            cbProject* pActiveProject =  Manager::Get()->GetProjectManager()->GetActiveProject();
            if (pActiveProject && GetLSPClient(pActiveProject) )
                CCLogger::Get()->DebugLog(wxString::Format("LSP OnProjectClosed duration:%zu millisecs. ", GetLSPClient(pActiveProject)->GetDurationMilliSeconds(startMillis)) );

        }//endif m_pLSPclient
    }
}//end ShutdownLSPclient()
// ----------------------------------------------------------------------------
void ClgdCompletion::CleanUpLSPLogs()
// ----------------------------------------------------------------------------
{

    // MS Windows has a lock on closed clangd logs.
    // Windows will not allow deletion of clangd logs until the project closes.
    // Having to do it here is stupid, but works.

    // Delete any logs not listed in %temp%/CBclangd_LogsIndex.txt
    // eg., CBclangd_client-10412.log or CBclangd_server-6224.log
    // The index file contains log names for recent log files we want to keep for awhile.
    // They are usually the last active log files and will be removed from the index when
    // new logs are created.
    wxString logIndexFilename(wxFileName::GetTempDir() + wxFILE_SEP_PATH + "CBclangd_LogsIndex.txt");
    if ( wxFileExists(logIndexFilename))
    switch (1)
    {
        default:
        // The log index file contains entries of log filenames to keep.
        // The first item is the PID of an active log. For example:
        // 21320;02/14/21_12:49:58;F:\user\programs\msys64\mingw64\bin\clangd.exe;F:\usr\Proj\HelloWxWorld\HelloWxWorld.cbp
        wxLogNull noLog;
        wxTextFile logIndexFile(logIndexFilename);
        logIndexFile.Open();
        if (not logIndexFile.IsOpened()) break;
        if (not logIndexFile.GetLineCount() ) break;
        size_t logIndexLineCount = logIndexFile.GetLineCount();

        wxString tempDirName = wxFileName::GetTempDir();
        wxArrayString logFilesVec;

        wxString logFilename = wxFindFirstFile(tempDirName + wxFILE_SEP_PATH + "CBclangd_*-*.log", wxFILE);
        while(logFilename.Length())
        {
            logFilesVec.Add(logFilename);
            logFilename = wxFindNextFile();
            if (logFilename.empty()) break;
        }
        if (not logFilesVec.GetCount()) break;
        // Delete log files with PIDs not in the log index
        for (size_t ii=0; ii<logFilesVec.GetCount(); ++ii)
        {
            wxString logName = logFilesVec[ii];
            wxString logPID = logName.AfterFirst('-').BeforeFirst('.');
            // if log filename pid == an index file pid, it lives
            for (size_t jj=0; jj<logIndexLineCount; ++jj)
            {
                // pid string is the first item in the log index
                wxString ndxPID = logIndexFile.GetLine(jj).BeforeFirst(';');
                if (logPID == ndxPID) break;   //This log lives, get next log.
                if (jj == logIndexLineCount-1) //no match for pids, delete this log
                    wxRemoveFile(logName);
            }//endfor index filenames
        }//endfor log Filenames
        if (logIndexFile.IsOpened())
            logIndexFile.Close();
    }//end switch
}//end CleanUpLSPLogs()
// ----------------------------------------------------------------------------
void ClgdCompletion::CleanOutClangdTempFiles()
// ----------------------------------------------------------------------------
{
    // clangd can leave behind a bunch of .pch and .tmp files with a pattern
    // of "preamble-*.pch" and "preamble-*.tmp"
    //Linux is allowing the removal of open files. So we trod carefully.
    // --------------------------------------------------------------
    //          Windows
    // --------------------------------------------------------------
    #if defined(_WIN32)
         wxLogNull NoLog;   //avoid windows "not allowed" message boxes
        // Get a list of temporary preamble-*.tmp files.
        wxString tempDir = wxFileName::GetTempDir();
        wxArrayString tmpFiles;
        wxDir::GetAllFiles(tempDir, &tmpFiles, "preamble-*.tmp", wxDIR_FILES);
        for (size_t ii=0; ii<tmpFiles.GetCount(); ++ii)
            wxRemoveFile(tmpFiles[ii]);
        // Get a list of temporary preamble-*.pch files
        tmpFiles.Clear();
        wxDir::GetAllFiles(tempDir, &tmpFiles, "preamble-*.pch", wxDIR_FILES);
        for (size_t ii=0; ii<tmpFiles.GetCount(); ++ii)
            wxRemoveFile(tmpFiles[ii]);
    #endif //end _WIN32
    // --------------------------------------------------------------
    //          Linux
    // --------------------------------------------------------------
    #if not defined(_WIN32)

        if (not wxFileExists("/usr/bin/lsof")) return;
         wxLogNull NoLog;   //avoid message boxes of errors
        // Get a list of temporary preamble-*.tmp files.
        wxString tempDir = wxFileName::GetTempDir();
        ProcUtils procUtils;

        // Get list of clangd temp files
        wxArrayString tmpFiles;
        wxArrayString lsofList;
        wxString cmd;

        wxDir::GetAllFiles(tempDir, &tmpFiles, "preamble-*.tmp", wxDIR_FILES);
        if (tmpFiles.GetCount())
        {
            // Get a list of open preamble-*.tmp files
            lsofList.Clear();
            cmd = "/usr/bin/lsof /tmp/preamble-*.tmp";
            procUtils.ExecuteCommand(cmd, lsofList);
            // Remove clangd temp files not currently open
            for (size_t ii=0; ii<tmpFiles.GetCount(); ++ii)
            {
                bool doDelete = true;
                for (size_t jj=0; jj<lsofList.GetCount(); ++jj)
                    if (lsofList[jj].Contains(tmpFiles[ii])) doDelete=false;
                if (doDelete)
                    wxRemoveFile(tmpFiles[ii]);
            }
        }

        tmpFiles.Clear();
        // Get a list of /tmp/preamble-*.pch files
        wxDir::GetAllFiles(tempDir, &tmpFiles, "preamble-*.pch", wxDIR_FILES);
        if (tmpFiles.GetCount())
        {
            // Get a list of open preamble-*.tmp files
            lsofList.Clear();
            cmd = "/usr/bin/lsof /tmp/preamble-*.pch";
            procUtils.ExecuteCommand(cmd, lsofList);
            // Remove any closed /tmp/preamble-*.pch files
            for (size_t ii=0; ii<tmpFiles.GetCount(); ++ii)
            {
                bool doDelete = true;
                for (size_t jj=0; jj<lsofList.GetCount(); ++jj)
                    if (lsofList[jj].Contains(tmpFiles[ii])) doDelete=false;
                if (doDelete)
                    wxRemoveFile(tmpFiles[ii]);
            }
        }//endif

    #endif // NOT windows

    // Remove the ProxyProject .cache\Clangd-cache.lock file
    // It's not needed and an old bug filled it up with ProxyProject.cbp entries.
    wxString configDir = Manager::Get()->GetConfigManager("app")->GetConfigFolder();
    if (configDir.Length())
    {
        //C:\Users\<user>\AppData\Roaming\CodeBlocks\.cache\Clangd-cache.lock
        wxString cacheLockFile = configDir + sep + ".cache\\Clangd-cache.lock";
        cacheLockFile.Replace("\\", sep); // in case not windows
        if (wxFileExists(cacheLockFile))
            wxRemoveFile(cacheLockFile) ;
    }

}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectClosed(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // After this, the Class Browser needs to be updated. It will happen
    // when we receive the next EVT_PROJECT_ACTIVATED event.
    if (IsAttached() && m_InitDone)
    {
        cbProject* project = event.GetProject();
        if (project == m_PrevProject) m_PrevProject = nullptr;
        if (project == m_CurrProject) m_CurrProject = nullptr;

        // ------------------------------------------------------------
        //  LSP client/server
        // ------------------------------------------------------------
        if (GetLSPClient(project))
        {
            // Tell LSP server to didClose() all open files for this project
            // and delete LSP client/server for this project
            ShutdownLSPclient(project);
            // MS Windows had a lock on closed clangd logs until the project was closed.
            CleanUpLSPLogs();
            GetParseManager()->DoUnlockClangd_CacheAccess(project);
            CleanOutClangdTempFiles();

        }//endif m_pLSPclient

       if (project && GetParseManager()->GetParserByProject(project))
       {
           // remove the CC Parser instance associated with the project
           GetParseManager()->DeleteParser(project);
        }
    }
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectSaved(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    event.Skip();
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectFileAdded(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    if (( not IsAttached()) or (not m_InitDone) ) return;

    // ------------------------------------------------------------
    //  LSP client/server
    // ------------------------------------------------------------
    switch(1)
    {
        default:
        cbProject* pProject = event.GetProject();
        if (not GetLSPClient(pProject)) break;
        wxString filename = event.GetString();
        cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor(filename);
        if (not pEditor) break;
        if (GetLSPClient(pProject)->GetLSP_EditorIsOpen(pEditor))
            break;

        // CodeBlocks has not yet added the ProjectFile* prior to calling this event.
        // Delay GetLSPClient(pProject)->LSP_DidOpen(pEditor) with a callback.
        // Allows ProjectManager to add ProjectFile* to the project following this
        // event but before the callback event is invoked.
         CallAfter(&ClgdCompletion::OnLSP_ProjectFileAdded, pProject, filename);
    }

    GetParseManager()->AddFileToParser(event.GetProject(), event.GetString());
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnLSP_ProjectFileAdded(cbProject* pProject, wxString filename)
// ----------------------------------------------------------------------------
{
    // This is a delayed callback caused by the fact that cbEVT_PROJECT_FILE_ADDED
    // is sent before a ProjectFile* is set in either the editor or the project.
    // It's nill until after the event completes.

    if (( not IsAttached()) or (not m_InitDone) ) return;

    // ------------------------------------------------------------
    //  LSP client/server
    // ------------------------------------------------------------
    if (not GetLSPClient(pProject)) return;
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor(filename);
    if (not pEditor) return;
    if (GetLSPClient(pProject)->GetLSP_EditorIsOpen(pEditor))
        return;
    ProjectFile* pProjectFile = pProject->GetFileByFilename(filename, false);
    if (not pProjectFile)
        return;
    bool ok = GetLSPClient(pProject)->LSP_DidOpen(pEditor);
    if (ok) CCLogger::Get()->DebugLog(wxString::Format("%s() DidOpen %s",__FUNCTION__, filename));
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnProjectFileRemoved(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    if (IsAttached() && m_InitDone)
        GetParseManager()->RemoveFileFromParser(event.GetProject(), event.GetString());
    event.Skip();
}
// ----------------------------------------------------------------------------
//void CodeCompletion::OnProjectFileChanged(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
//{
//    This event is never issued by CodeBlocks.
//
//    if (IsAttached() && m_InitDone)
//    {
//        // TODO (Morten#5#) make sure the event.GetProject() is valid.
//        cbProject* project = event.GetProject();
//        wxString filename = event.GetString();
//        if (!project)
//            project = GetParseManager()->GetProjectByFilename(filename);
//        if (project && GetParseManager()->ReparseFile(project, filename))
//            CCLogger::Get()->DebugLog(_T("Reparsing when file changed: ") + filename);
//    }
//    event.Skip();
//}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnEditorSave(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    if (!ProjectManager::IsBusy() && IsAttached() && m_InitDone && event.GetEditor())
    {

        // ----------------------------------------------------------------------------
        // LSP didSave
        // ----------------------------------------------------------------------------
        EditorBase* pEb = event.GetEditor();
        cbEditor* pcbEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(pEb);
        // Note: headers dont get initialized bec. clangd does not send back
        // diagnostics for unchanged editors. Check if changed.
        if (GetLSP_IsEditorParsed(pcbEd) or pcbEd->GetModified())
        {
            GetLSPClient(pcbEd)->LSP_DidSave(pcbEd);
        }
        return;
    }

}//end OnEditorSave()
// ----------------------------------------------------------------------------
void ClgdCompletion::OnEditorOpen(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{

    /// Note Well: the ProjectFile in the cbEditor is a nullptr

    if (!Manager::IsAppShuttingDown() && IsAttached() && m_InitDone)
    {
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());
        if (ed)
        {
            FunctionsScopePerFile* funcdata = &(m_AllFunctionsScopes[ed->GetFilename()]);
            funcdata->parsed = false;
            m_OnEditorOpenEventOccured = true;
        }//end if ed

    }//endif IsApp....

    event.Skip(); //Unnecessary to skip CodeBlockEvents.
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnEditorActivatedCallback(wxString edfilename, bool isOpening)
// ----------------------------------------------------------------------------
{
    // Use to Callback OnEditorActivated() after the Project/Editor managers
    // set the Editors missing ProjectFile->ParentProject pointer.
    // Avoids crashes if the editor has closed between the time the callback
    // was issued and the time the IdleCallbackHander invoked this function.
    CodeBlocksEvent evt(cbEVT_EDITOR_ACTIVATED);
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    EditorBase* pEditor = pEdMgr->GetBuiltinEditor(edfilename);
    if (pEditor)
    {
        if (isOpening) m_OnEditorOpenEventOccured = true;
        CodeBlocksEvent evt(cbEVT_EDITOR_ACTIVATED);
        evt.SetEditor(pEditor);
        OnEditorActivated(evt);
    }
    else m_OnEditorOpenEventOccured = false;
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnEditorActivated(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    TRACE(_T("CodeCompletion::OnEditorActivated(): Enter"));

    if (m_WorkspaceClosing)
        return;

    LogManager* pLogMgr = Manager::Get()->GetLogManager(); wxUnusedVar(pLogMgr);

    if (ProjectManager::IsBusy()) return;
    if (not IsAttached()) return;
    if (not m_InitDone) return;
    if (not event.GetEditor()) return;

    m_LastEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());

    TRACE(_T("CodeCompletion::OnEditorActivated(): Starting m_TimerEditorActivated."));

    // Timer that updates the Compiler toolbar (showing current class and function)
    if (m_TimerToolbar.IsRunning())
        m_TimerToolbar.Stop();

    // The LSP didOpen is issued here because OnEditorOpen() does not have the editors ProjectFile ptr set.
    // Verify that it was OnEditorOpen() that activated this editor.
    cbEditor* pEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());
    if (not pEd) return;

    if (pEd and m_OnEditorOpenEventOccured)
    {
        // ---------------------------------
        // New editor has been opened
        // ---------------------------------
        m_OnEditorOpenEventOccured = false;
        // Here for Language Service Process to send didOpen().
        // The OnEditorOpen() event cannot be used because the editor does not yet have
        // a ProjectFile pointer to determine the file's parent project.

        // FYI: This OnEditorActivated() event occurs prior to OnProjectActivated().
        // The LSP server may not be initialized when editors are opened during project load.
        // To compensate for any missed editor opens, LSP_Initialize() will scan the editors notebook
        // and send didOpen() for files already opened before event OnProjectActivated().

        // Here, we check for editors opened AFTER OnProjectActivated() to send a single
        // didOpen() to the LSP server. The m_OnEditorOpenEventOccured flag indicates
        // this was a valid open event, not just a user click on the notebook tab, or a
        // mouse focus activation.

        // Find the project and ProjectFile on which this editor is dependent.
        // For an editor that's not associated with the active project
        // we need the associated non-active project and base project .cbp file location
        // in order to do a LSP didOpen() on a file.
        cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pActiveProject) return;
        ProcessLanguageClient* pActiveProjectClient = GetLSPClient(pActiveProject);
        if (not pActiveProjectClient)
        {
            // No active project client fully initialized yet. Call back later.
            GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::OnEditorActivatedCallback, event.GetEditor()->GetFilename(), true);
            return;
        }
        // We have an active project. Get the ProjectFile ptr for this editor.
        ProjectFile* pProjectFile = pEd->GetProjectFile();
        if (not pProjectFile)
        {
            // The ProjectFile* has not yet been entered into the cbEditor object.
            // Will there ever be a ProjectFile ptr ? (Not for standalone files.)
            // Ask ProjectManager if there will ever be a ProjectFile ptr in the editor.
            Manager::Get()->GetProjectManager()->FindProjectForFile(pEd->GetFilename(), &pProjectFile, false, false);
            if (pProjectFile)
            {
                // After this event returns, ProjectManager will enter the project file ptr into the editor.
                // So callback when the ProjectFile* has actually been stowed into the editor.
                GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::OnEditorActivatedCallback, event.GetEditor()->GetFilename(), true);
                return;
            }
            else //Activated editor has a non-project file, let the ProxyProject handle it.
            {
                if (wxFileExists(pEd->GetFilename()))
                {
                    // Add this non-project file to the proxyParser
                    pProjectFile = GetParseManager()->GetProxyProject()->AddFile(0, pEd->GetFilename(), true, false);
                    pEd->SetProjectFile(pProjectFile);
                    // Add file to ProxyParser to get parsed
                    GetParseManager()->GetProxyParser()->AddFile(pEd->GetFilename(), GetParseManager()->GetProxyProject());
                    return;
                }
            }
        }//endif not projectfile

        if (not pProjectFile) return;

        // ----------------------------------------------------------------------------
        // We now have an active project, and an active editor with a project file ptr
        // ----------------------------------------------------------------------------
        cbProject* pEdProject = pProjectFile->GetParentProject();
        if (not pEdProject) return;
        Parser* pParser = dynamic_cast<Parser*>( GetParseManager()->GetParserByProject(pEdProject));
        // LSP Open files only with specified ParserCommon::EFileType extensions (specified in config)
        ParserCommon::EFileType filetype = ParserCommon::FileType(pProjectFile->relativeFilename);
        bool fileTypeOK = (filetype == ParserCommon::ftHeader) or (filetype == ParserCommon::ftSource);
        bool didOpenOk = false; //assume LSP will fail open, then do LSP didOpen()

        if ( GetLSP_Initialized(pEdProject) and pParser and fileTypeOK ) do
        {
            bool paused = pParser->GetUserParsingPaused(); // get paused status
            if (paused) break;

            // If editor not already LSP didOpen()'ed, do a LSP didOpen() call
            // cland does not like us doing multiple opens on a file.
            if (not (didOpenOk = GetLSPClient(pEd)->GetLSP_EditorIsOpen(pEd)) )
                didOpenOk = GetLSPClient(pEd)->LSP_DidOpen(pEd);
            if (not didOpenOk)
            {
                wxString msg = wxString::Format("%s Failed to LSP_DidOpen()\n%s", __FUNCTION__, pEd->GetFilename());
                cbMessageBox(msg, "Failed LSP_DidOpen()");
            } else CCLogger::Get()->DebugLog(wxString::Format("%s() DidOpen %s", __FUNCTION__, pEd->GetFilename()));

        }while(0);
    }//endif (pEd and m_OnEditorOpenEventOccured)

    // ----------------------------------------------------------------------------
    //This is an editor activation that's not preceeded by an Editor open event
    // ----------------------------------------------------------------------------
    cbProject* pEdProject = pEd->GetProjectFile() ? pEd->GetProjectFile()->GetParentProject() : nullptr;
    // If the editor has been fully initialized by ProjectManager, switch to editor's ClassBrowser
    if (pEd and pEdProject)
    {
        // switch ClassBrowser to this editor project/file via callback.
        // and update the namespace/function toolbar if needed.
        GetIdleCallbackHandler(pEdProject)->QueueCallback(this, &ClgdCompletion::NotifyParserEditorActivated, event);
    }

    // ----------------------------------------------------------------------------
    // Handle editors containing a file that does not belong to this project
    // ----------------------------------------------------------------------------
    // We might have an editor that belongs to a non-active project.
    // Ususally caused by having saved/loaded a workspace layout with open editors
    // belonging to a non-active project.
    cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    ProcessLanguageClient* pActiveProjectClient = GetLSPClient(pActiveProject);
    ProcessLanguageClient* pEdClient = (pEdProject ? GetLSPClient(pEdProject) : nullptr);
    cbProject* pProxyProject = GetParseManager()->GetProxyProject();

    if (pActiveProject and pEd and pEdProject and pEdClient
                and (pEdProject == pProxyProject) )
    {
        // If we previouly used the ProxyProject, see if we can now use the ActiveProject
        // This situation can be caused by Menu/File/New/File... beging added to the active project
        // or a previously opened non-project file now being opened by it's newly activated project.
        // See if this file belongs to the active project.
        ProjectFile* pActiveProjectFile = pActiveProject->GetFileByFilename(pEd->GetFilename(),false);
        // If just got a ProjectFile ptr, this file belongs to the active project
        if (pActiveProjectFile)
        {
            // This editor and file now belong to the active project
            // If the ProxyProject currently owns this editor
            // tell LSP to didClose() this open editor and didOpen() it for the active project.
            if (pEdClient->GetLSP_EditorIsOpen(pEd))
                pEdClient->LSP_DidClose(pEd);
            // Switch this editor ProjectFile to active project
            pEd->SetProjectFile(pActiveProjectFile);    //reset the editors projectfile to the active project
            pEdClient = pActiveProjectClient;
            // If the file has been LSP didOpend()ed, reparse the editors file if needed.
            if (not GetLSP_IsEditorParsed(pEd))
                GetParseManager()->AddFileToParser(pActiveProject, pEd->GetFilename());
            // else tell LSP to didOpen() the file using the active project
            else if ( not pEdClient->GetLSP_EditorIsOpen(pEd) )
            {
                bool ok = pEdClient->LSP_DidOpen(pEd);
                if (ok)
                    CCLogger::Get()->DebugLog(wxString::Format("%s() DidOpen %s",__FUNCTION__, pEd->GetFilename()));
            }
        }//endif pActiveProjectFile
    }//endif  (pActiveProject and pEd and pEdProject and pEdClient and (pEdProject == pProxyProject) )

    // ----------------------------------------------------------------------------
    // We've got an editor that belongs to a non-active project in the workspace.
    // ----------------------------------------------------------------------------
    // If the editor's project has no LSP client yet, assign the editor to the ProxyProject
    if (pEd and pEdProject and (not pEdClient) )
    {
        // If the active project contains this file, use it's LSP_client, else use the ProxyProject
        ProjectFile* pActiveProjectFile = pActiveProject ? pActiveProject->GetFileByFilename(pEd->GetFilename(),false) : nullptr;
        if (pActiveProjectFile)
            pEd->SetProjectFile(pActiveProjectFile);
        else //use the ProxyProjects LSP_client to do the parsing.
        {
            // Use the ProxyProject to parse this editor
            ProjectFile* pProjectFile = pProxyProject->AddFile(0, pEd->GetFilename(), true, false);
            if (pProjectFile) //can be nullptr if addFile fails
                pEd->SetProjectFile(pProjectFile);
        }
        // We have an editor, a project and may have added the file to the ProxyProject
        if (GetLSPClient(pEd)) //if no client, we found no ProjectFile ptr from the editor
        {
            // We now have an assigned LSP client, tell LSP client to didOpen() the file.
            bool ok = GetLSPClient(pEd)->LSP_DidOpen(pEd);
            if (ok) CCLogger::Get()->DebugLog(wxString::Format("%s() DidOpen %s",__FUNCTION__, pEd->GetFilename()));
        }

    }//endif parse non-ActiveProject editor

    TRACE(_T("CodeCompletion::OnEditorActivated(): Leave"));
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnEditorClosed(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    if (!pEdMgr)
    {
        event.Skip();
        return;
    }

    wxString activeFile;
    EditorBase* pEdBase = pEdMgr->GetActiveEditor();
    if (pEdBase)
        activeFile = pEdBase->GetFilename();

    TRACE(_T("CodeCompletion::OnEditorClosed(): Closed editor's file is %s"), activeFile.wx_str());

    // ----------------------------------------------------------------------------
    // Tell Language Service Process that an editor was closed
    // ----------------------------------------------------------------------------
    // Invoke didClose for the event editor, not the active editor
    // This file may have already been didClose'd by OnProjectClose() LSP shutdown.
    cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(event.GetEditor());
    ProcessLanguageClient* pClient = GetLSPClient(pcbEd);
    //-if (pcbEd and GetLSP_Initialized(pcbEd) and GetLSPClient(pcbEd) )
    //^^ NoteToSelf: editor would not show initialized if it never got diagnostics
     if (pcbEd and pClient and pClient->GetLSP_EditorIsOpen(pcbEd))
    {
            GetLSPClient(pcbEd)->LSP_DidClose(pcbEd);
            m_pParseManager->ClearDiagnostics(pcbEd->GetFilename());  //(Christo 2024/03/30)
    }

    if (m_LastEditor == event.GetEditor())
    {
        m_LastEditor = nullptr;
        if (m_TimerEditorActivated.IsRunning())
            m_TimerEditorActivated.Stop();
    }

    // tell m_ParseManager that a builtin editor was closed
    if ( pEdMgr->GetBuiltinEditor(event.GetEditor()) )
        GetParseManager()->OnEditorClosed(event.GetEditor());

    m_LastFile.Clear();

    // we need to clear CC toolbar only when we are closing last editor
    // in other situations OnEditorActivated does this job
    // If no editors were opened, or a non-buildin-editor was active, disable the CC toolbar
    if (pEdMgr->GetEditorsCount() == 0 || !pEdMgr->GetActiveEditor() || !pEdMgr->GetActiveEditor()->IsBuiltinEditor())
    {
        EnableToolbarTools(false);

        // clear toolbar when closing last editor
        if (m_Scope)
            m_Scope->Clear();
        if (m_Function)
            m_Function->Clear();

        cbEditor* ed = pEdMgr->GetBuiltinEditor(event.GetEditor());
        wxString filename;
        if (ed)
            filename = ed->GetFilename();

        m_AllFunctionsScopes[filename].m_FunctionsScope.clear();
        m_AllFunctionsScopes[filename].m_NameSpaces.clear();
        m_AllFunctionsScopes[filename].parsed = false;

        if (GetParseManager()->GetParser().ClassBrowserOptions().displayFilter == bdfFile)
        {
            s_ClassBrowserCaller = wxString::Format("%s:%d",__FUNCTION__, __LINE__);
            GetParseManager()->UpdateClassBrowser();
        }
    }

    event.Skip();
}

// ----------------------------------------------------------------------------
void ClgdCompletion::OnCCLogger(CodeBlocksThreadEvent& event)
// ----------------------------------------------------------------------------
{
    if (Manager::IsAppShuttingDown()) return;
    if (event.GetId() == g_idCCErrorLogger)
        Manager::Get()->GetLogManager()->LogError(event.GetString());
    if (event.GetId() == g_idCCLogger)
        Manager::Get()->GetLogManager()->Log(event.GetString());
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnCCDebugLogger(CodeBlocksThreadEvent& event)
// ----------------------------------------------------------------------------
{
    if (Manager::IsAppShuttingDown()) return;
    if (event.GetId() == g_idCCDebugLogger)
     Manager::Get()->GetLogManager()->DebugLog(event.GetString());
    if (event.GetId() == g_idCCDebugErrorLogger)
     Manager::Get()->GetLogManager()->DebugLogError(event.GetString());
}
// ----------------------------------------------------------------------------
int ClgdCompletion::DoClassMethodDeclImpl()
// ----------------------------------------------------------------------------
{
    // ContextMenu->Insert-> declaration/implementation

    if (!IsAttached() || !m_InitDone)
        return -1;

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return -3;

    FileType ft = FileTypeOf(ed->GetShortName());
    ParserCommon::EFileType eft = ParserCommon::FileType(ed->GetShortName());
    if ( (eft != ParserCommon::ftHeader) && (eft != ParserCommon::ftSource) && (ft != ftTemplateSource) ) // only parse source/header files
        return -4;

    if (not GetLSP_Initialized(ed))
    {
        wxString msg = _("The Parser is still parsing files.");
        CCLogger::Get()->DebugLog(msg);
        return -5;
    }

    int success = -6;

    // ----------------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // ----------------------------------------------------
    // Do not block the main UI. If the lock is busy, this code re-queues a callback on idle time.
    auto lock_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (lock_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, don't block UI thread, requeue a callback on the idle queue instead.
        if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
            return -6; //exceeded lock tries

        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idClassMethod);
        GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::OnClassMethod, evt);
        return -5; //parser is busy, try again later
    }
    else
    {
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/  \
        GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
    }

    // open the insert class dialog
    wxString filename = ed->GetFilename();
    InsertClassMethodDlg dlg(Manager::Get()->GetAppWindow(), &GetParseManager()->GetParser(), filename);
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

    // ---------------------------------------------------
    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
    // ---------------------------------------------------
    s_TokenTreeMutex_Owner = wxString();

    return success;
}
// ----------------------------------------------------------------------------
int ClgdCompletion::DoAllMethodsImpl()
// ----------------------------------------------------------------------------
{
    if (!IsAttached() || !m_InitDone)
        return -1;

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    //LogManager* pLogMgr = Manager::Get()->GetLogManager();

    if (!ed)
        return -3;
    if (not GetLSP_Initialized(ed))
        return -3;

    //FileType ft = FileTypeOf(ed->GetShortName()); -
    ParserCommon::EFileType ft = ParserCommon::FileType(ed->GetShortName());
    if ( (ft != ParserCommon::ftHeader) && (ft != ParserCommon::ftSource)) // only parse source/header files
        return -4;

    wxArrayString paths = GetParseManager()->GetAllPathsByFilename(ed->GetFilename());
    TokenTree*    tree  = GetParseManager()->GetParser().GetTokenTree();

    // ----------------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // ----------------------------------------------------
    auto lock_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (lock_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, but don't block UI thread, requeue an idle time callback instead.
        if (not GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine) ) //verify tries < 8
            return -6; // lock attempt exceeded

        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idClassMethod);
        GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::OnClassMethod, evt);
        return -5; //parser is busy try again later
    }
    else
    {   s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
        m_CCHasTreeLock = true;
    }

    // This destructor invoked on any further return statement and unlocks the token tree
    struct UnlockTokenTree
    {
        UnlockTokenTree(){}
        ~UnlockTokenTree()
        {
            if (m_CCHasTreeLock)
            {
                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex);
                s_TokenTreeMutex_Owner = wxString();
            }
        }

    } unlockTokenTree;

    // get all filenames' indices matching our mask
    wxString msg;
    TokenFileSet result;
    for (size_t i = 0; i < paths.GetCount(); ++i)
    {
        msg = wxString::Format(_("%s: Trying to find matches for:%s"), __FUNCTION__, paths[i]);
        CCLogger::Get()->DebugLog(msg);
        TokenFileSet result_file;
        tree->GetFileMatches(paths[i], result_file, true, true);
        for (TokenFileSet::const_iterator it = result_file.begin(); it != result_file.end(); ++it)
            result.insert(*it);
    }

    if (result.empty())
    {
        // ------------------------------------------------
        // CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex) unlocked by ~UnlockTokenTree()
        // ------------------------------------------------

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
        // ------------------------------------------------
        //CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex) unlocked by ~UnlockTokenTree()
        // ------------------------------------------------

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

        bool addDoxgenComment = Manager::Get()->GetConfigManager("clangd_client")->ReadBool(_T("/add_doxgen_comment"), false);

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
            wxString args = token->GetStrippedArgs();
            if (args.empty()) args = "()";

            //-str << token->m_Name << token->GetStrippedArgs();
            str << token->m_Name << args;

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

    // ----------------------------------------------------
    //CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex) unlocked by ~UnlockTokenTree()
    // ----------------------------------------------------

    return success;
}//end DoAllMethodsImpl()
// ----------------------------------------------------------------------------
void ClgdCompletion::MatchCodeStyle(wxString& str, int eolStyle, const wxString& indent, bool useTabs, int tabSize)
// ----------------------------------------------------------------------------
{
    str.Replace(wxT("\n"), GetEOLStr(eolStyle) + indent);
    if (!useTabs)
        str.Replace(wxT("\t"), wxString(wxT(' '), tabSize));
    if (!indent.IsEmpty())
        str.RemoveLast(indent.Length());
}
// help method in finding the function position in the vector for the function containing the current line
// ----------------------------------------------------------------------------
void ClgdCompletion::FunctionPosition(int &scopeItem, int &functionItem) const
// ----------------------------------------------------------------------------
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
// ----------------------------------------------------------------------------
void ClgdCompletion::GotoFunctionPrevNext(bool next /* = false */)
// ----------------------------------------------------------------------------
{
    // Original CC code is faster than a LSP request/response
    // but it has a habit of displaying the wrong line because ClassBrowser
    // may be stale. So the clangd version is used instead.

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if (!ed)
        return;
    if (not GetLSP_IsEditorParsed(ed))
    {
        InfoWindow::Display("LSP " + wxString(__FUNCTION__), _("Editor not parsed yet."), 7000);
        return;
    }

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
// ----------------------------------------------------------------------------
int ClgdCompletion::NameSpacePosition() const
// ----------------------------------------------------------------------------
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
// ----------------------------------------------------------------------------
void ClgdCompletion::OnScope(wxCommandEvent&)
// ----------------------------------------------------------------------------
{
    int sel = m_Scope->GetSelection();
    if (sel != -1 && sel < static_cast<int>(m_ScopeMarks.size()))
        UpdateFunctions(sel);
}

// ----------------------------------------------------------------------------
void ClgdCompletion::OnFunction(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
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

/** Here is the expansion of how the two wxChoices are constructed.
 * for a file which has such contents below
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
// ----------------------------------------------------------------------------
void ClgdCompletion::ParseFunctionsAndFillToolbar()
// ----------------------------------------------------------------------------
{
    TRACE(_T("ParseFunctionsAndFillToolbar() Entered: m_ToolbarNeedReparse=%d, m_ToolbarNeedRefresh=%d, "),
          m_ToolbarNeedReparse?1:0, m_ToolbarNeedRefresh?1:0);

    if (not m_ToolBar) return;

    EditorManager* edMan = Manager::Get()->GetEditorManager();
    if (!edMan) // Closing the app?
        return;

    cbEditor* ed = edMan->GetBuiltinActiveEditor();
    if ( !ed || !ed->GetControl())
    {
        if (m_Function)
            m_Function->Clear();
        if (m_Scope)
            m_Scope->Clear();

        EnableToolbarTools(false);
        m_LastFile.Clear();
        return;
    }

    if (not GetLSP_Initialized(ed)) return;

    const wxString filename = ed->GetFilename();
    if (filename.IsEmpty())
        return;

    bool fileParseFinished = GetParseManager()->GetParser().IsFileParsed(filename);

    // FunctionsScopePerFile contains all the function and namespace information for
    // a specified file, m_AllFunctionsScopes[filename] will implicitly insert a new element in
    // the map if no such key(filename) is found.
    FunctionsScopePerFile* funcdata = &(m_AllFunctionsScopes[filename]);

    #if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
        wxString debugMsg = wxString::Format("ParseFunctionsAndFillToolbar() : m_ToolbarNeedReparse=%d, m_ToolbarNeedRefresh=%d, ",
              m_ToolbarNeedReparse?1:0, m_ToolbarNeedRefresh?1:0);
        debugMsg += wxString::Format("\n%s: funcdata->parsed[%d] ", __FUNCTION__, funcdata->parsed);
        CCLogger::Get()->pLogMgr->DebugLog(debugMsg);
    #endif

    // *** Part 1: Parse the file (if needed) ***
    if (m_ToolbarNeedReparse || !funcdata->parsed)
    {
        TRACE("ParseFunctionsAndFillToolbar() Entered Part 1: Parse the file (if needed)");

        // -----------------------------------------------------
        //CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)             // LOCK TokenTree
        // -----------------------------------------------------
        // If the lock is busy, a callback is queued for idle time.
        auto locker_result = s_TokenTreeMutex.LockTimeout(250);
        wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
        if (locker_result != wxMUTEX_NO_ERROR)
        {
            // lock failed, do not block the UI thread, call back when idle
            if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
                GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::ParseFunctionsAndFillToolbar);
            return;
        }
        else /*lock succeeded*/
        {
            s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
            GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
        }

        if (m_ToolbarNeedReparse)
        {
            m_ToolbarNeedReparse = false;
            //-m_ToolbarNeedRefresh = true;
        }

        funcdata->m_FunctionsScope.clear();
        funcdata->m_NameSpaces.clear();


        // collect the function implementation information, just find the specified tokens in the TokenTree
        TokenIdxSet result;
        bool hasTokenTreeLock = true;
        GetParseManager()->GetParser().FindTokensInFile(hasTokenTreeLock, filename, result,
                                                    tkAnyFunction | tkEnum | tkClass | tkNamespace);
        if ( ! result.empty())
            funcdata->parsed = true;    // if the file had some containers, flag it as parsed
        else
            fileParseFinished = false;  // this indicates the batch parser has not finish parsing for the current file

        TokenTree* tree = GetParseManager()->GetParser().GetTokenTree();

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

        // ----------------------------------------------------------------------------
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        // ----------------------------------------------------------------------------
        s_TokenTreeMutex_Owner = wxString();

        FunctionsScopeVec& functionsScopes = funcdata->m_FunctionsScope;
        NameSpaceVec& nameSpaces = funcdata->m_NameSpaces;

        // collect the namespace information in the current file, this is done by running a parserthread
        // on the editor's buffer
        //-GetParseManager()->GetParser().ParseBufferForNamespaces(ed->GetControl()->GetText(), nameSpaces);
        ///^^ namespaces already parsed by clangd and entered into the tree by OnLSP_RequestedSymbolsResponse()

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

        TRACE(F(_T("Found %lu namespace locations"), static_cast<unsigned long>(nameSpaces.size())));
    #if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
        for (unsigned int i = 0; i < nameSpaces.size(); ++i)
            CCLogger::Get()->DebugLog(wxString::Format(_T("\t%s (%d:%d)"),
                nameSpaces[i].Name.wx_str(), nameSpaces[i].StartLine, nameSpaces[i].EndLine));
    #endif

        if (!m_ToolbarNeedRefresh)
            m_ToolbarNeedRefresh = true;
    }

    // *** Part 2: Fill the toolbar ***
    TRACE("ParseFunctionsAndFillToolbar() Entered: Part 2: Fill the toolbar");
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

    TRACE(F(_T("Parsed %lu functionScope items"), static_cast<unsigned long>(m_FunctionsScope.size())));
    #if CC_CODECOMPLETION_DEBUG_OUTPUT == 1
    for (unsigned int i = 0; i < m_FunctionsScope.size(); ++i)
        CCLogger::Get()->DebugLog(wxString::Format(_T("\t%s%s (%d:%d)"),
            m_FunctionsScope[i].Scope.wx_str(), m_FunctionsScope[i].Name.wx_str(),
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

        TRACE(wxString::Format("%s(): m_Scope[%d] m_FunctionScope[%d]", __FUNCTION__, int(m_ScopeMarks.size()), int(m_FunctionsScope.size()) ));
        //- **debugging** CCLogger::Get()->DebugLog(wxString::Format("%s(): m_Scope[%d] m_FunctionScope[%d]", __FUNCTION__, m_ScopeMarks.size(), int(m_FunctionsScope.size()) ));

        // ...and refresh the toolbars.
        if (m_Function)
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
        else if (m_Function)
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

    // Control the toolbar state, if the batch parser has not finished parsing the file, no need to update CC toolbar.
    EnableToolbarTools(fileParseFinished);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::FindFunctionAndUpdate(int currentLine)
// ----------------------------------------------------------------------------
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
// ----------------------------------------------------------------------------
void ClgdCompletion::UpdateFunctions(unsigned int scopeItem)
// ----------------------------------------------------------------------------
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
// ----------------------------------------------------------------------------
void ClgdCompletion::EnableToolbarTools(bool enable)
// ----------------------------------------------------------------------------
{
    if (m_Scope)
        m_Scope->Enable(enable);
    if (m_Function)
        m_Function->Enable(enable);
}

// ----------------------------------------------------------------------------
void ClgdCompletion::DoParseOpenedProjectAndActiveEditor(wxTimerEvent& event)
// ----------------------------------------------------------------------------
{
    // Here from the StartupDelay timer pop to let the app startup before parsing.
    // This is to prevent the Splash Screen from delaying so much. By calling
    // here on a timer, the splash screen is closed and Code::Blocks doesn't appear
    // to take so long in starting.

    m_InitDone = false; //set this true before return
    wxString msg;
    LogManager* pLogMgr = Manager::Get()->GetLogManager();

    // Create a ProxyProject to use for non-project files (if not already existent )
    GetParseManager()->SetProxyProject(nullptr); // create the hidden proxy project
    cbProject* pProxyProject = GetParseManager()->GetProxyProject();
    // Create a ProxyClient to hold non-project file info and the clangd interface.
    ProcessLanguageClient* pProxyClient = GetParseManager()->CreateNewLanguageServiceProcess(pProxyProject, LSPeventID);
    if (not pProxyClient)
    {
        msg = wxString::Format("Error: %s failed to allocate a Clangd client for %s",
                                __FUNCTION__, pProxyProject->GetTitle());
        pLogMgr->LogError(msg);
        pLogMgr->DebugLogError(msg);
    }
    // Parser was already created by SetProxyProject();
    Parser* pProxyParser = GetParseManager()->GetParserByProject(pProxyProject);
    // Set the ProxyProject to use this new clangd client.
    if (pProxyProject and pProxyClient and pProxyParser)
    {
        GetParseManager()->m_LSP_Clients[pProxyProject] = pProxyClient;
        pProxyParser->SetLSP_Client(pProxyClient);
        pProxyClient->SetParser((Parser*)pProxyParser);
    }
    cbWorkspace* pWorkspace = Manager::Get()->GetProjectManager()->GetWorkspace();
    pWorkspace->SetModified(false);

    // Dreaded DDE-open bug related: do not touch the following lines unless for a good reason
    // Parse any files opened through DDE or the command-line
    EditorBase* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (editor)
        GetParseManager()->OnEditorActivated(editor);

    // ------------------------------------------------------------------------
    //  Check to see if this is a DDE CodeBlocks cold startup
    // ------------------------------------------------------------------------
    // This is the "I hate DDE" section.
    // If ProxyProject and active project and (not Clangd_Client),
    // re-activate the project to create a clangd_client.
    // This is necessary because because, for a DDE cold CB startup, the
    // first OnProjectActivated() is called before local initialization is done.
    // When CB has been previously started, the DDE is a load workspace, not a
    // execute CB with a .cbp load at the same time.
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    bool reactivateTheProject = pProject and pProxyParser
                                and (not GetLSPClient(pProject));
    m_InitDone = true;

    if (reactivateTheProject)
    {
        wxString msg = wxString::Format("%s: ReActivating project from possible DDE event", __FUNCTION__);
        pLogMgr->DebugLog(msg);
        // Let all DDE initializations settle, else we get specious error msgs
        wxMilliSleep(1000);
        // There's an active project loaded before our OnProjectActivated() could create a clangd_client.
        // Now that init is done, re-activate the project with a local event.
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, XRCID("OnReActivateProject"));
        //wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idCurrentProjectReparse); this will work also.
        cbPlugin* pPlgn = Manager::Get()->GetPluginManager()->FindPluginByName("clangd_client");
        if (pPlgn) pPlgn->AddPendingEvent(evt);
    }
}
// ----------------------------------------------------------------------------
void ClgdCompletion::UpdateEditorSyntax(cbEditor* ed)
// ----------------------------------------------------------------------------
{
    if (!Manager::Get()->GetConfigManager(wxT("clangd_client"))->ReadBool(wxT("/semantic_keywords"), false))
        return;
    if (!ed)
        ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed || ed->GetControl()->GetLexer() != wxSCI_LEX_CPP)
        return;
    if (not GetLSP_Initialized(ed)) return;

    TokenIdxSet result;
    int flags = tkAnyContainer | tkAnyFunction;
    if (ed->GetFilename().EndsWith(wxT(".c")))
        flags |= tkVariable;

    // -----------------------------------------------------
    //CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // -----------------------------------------------------
    // Avoid blocking main thread, If the lock is busy, queue a callback at idle time.
    auto locker_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread, call back when idle
        if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
            GetIdleCallbackHandler()->QueueCallback(this, &ClgdCompletion::UpdateEditorSyntax, ed);
        return;
    }
    else /*lock succeeded*/
    {
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
    }

    bool hasTokenTreeLock = true;
    GetParseManager()->GetParser().FindTokensInFile(hasTokenTreeLock, ed->GetFilename(), result, flags);
    TokenTree* tree = GetParseManager()->GetParser().GetTokenTree();

    std::set<wxString> varList;
    TokenIdxSet parsedTokens;

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

    // ---------------------------------------------------
    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
    // ---------------------------------------------------
    s_TokenTreeMutex_Owner = wxString();

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
// ----------------------------------------------------------------------------
void ClgdCompletion::InvokeToolbarTimer(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // Allow others to call OnToolbarTimer() via event.
    // Invoked by Parser::OnLSP_ParseDocumentSymbols() to update the scope toolbar

    if (not m_InitDone) return;
    m_ToolbarNeedReparse = true;
    m_ToolbarNeedRefresh = true;

    wxTimerEvent evt(m_TimerToolbar);
    OnToolbarTimer(evt);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnToolbarTimer(cb_unused wxTimerEvent& event)
// ----------------------------------------------------------------------------
{
    // Update the Code completion tool bar

    TRACE(_T("CodeCompletion::OnToolbarTimer(): Enter"));

    // stop any timer event since non timer events can enter here, esp., from parsers
    if (m_TimerToolbar.IsRunning())
        m_TimerToolbar.Stop();

    if (m_InitDone and (not ProjectManager::IsBusy()))
        ParseFunctionsAndFillToolbar();
    else
    {
        TRACE(_T("CodeCompletion::OnToolbarTimer(): Starting m_TimerToolbar."));
        m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);
    }

    TRACE(_T("CodeCompletion::OnToolbarTimer(): Leave"));
}
// ----------------------------------------------------------------------------
void ClgdCompletion::NotifyParserEditorActivated(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // This is the old OnEditorActivatedTimer() which has been deprecated
    // and replaced with an IdleTimeCallback in OnEditorActivated()

    // The m_LastEditor variable was updated in CodeCompletion::OnEditorActivated, after that,
    // the editor-activated-timer was started. So, here in the timer handler, we need to check
    // whether the saved editor and the current editor are the same, otherwise, no need to update
    // the toolbar, because there must be another editor activated before the timer hits.
    // Note: only the builtin active editor is considered.

    // stop the timer; It gets re-started on each CodeCompletion::OnEditorActivated()
    m_TimerEditorActivated.Stop(); //stop!  especially while I'm debugging

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
        TRACE(_T("ClgdCompletion::NotifyParserEditorActivated(): Same as the last activated file(%s)."), curFile.wx_str());
        return;
    }

    TRACE("ClgdCompletion::NotifyParserEditorActivated(): Need to notify ParseManager and Refresh toolbar.");

    GetParseManager()->OnEditorActivated(editor);

    // If the above started a parser, start a clangd_client also
    cbEditor* pcbEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(editor);
    cbProject* pProject = pcbEd ? GetParseManager()->GetProjectByEditor(pcbEd) : nullptr;
    if (pProject && GetParseManager()->GetParserByProject(pProject))
    {
        if (not GetParseManager()->GetLSPclientAllocated(pProject))
        {
            ProcessLanguageClient* pClient = GetParseManager()->CreateNewLanguageServiceProcess(pProject, LSPeventID);
            if (pClient)
            {
                wxCommandEvent reparse_evt(wxEVT_COMMAND_MENU_SELECTED);
                reparse_evt.SetId(idEditorFileReparse);
                Manager::Get()->GetAppFrame()->GetEventHandler()->AddPendingEvent(reparse_evt);
            }
        }
    }

    TRACE(_T("CodeCompletion::OnEditorActivatedTimer: Starting m_TimerToolbar."));
    m_TimerToolbar.Start(TOOLBAR_REFRESH_DELAY, wxTIMER_ONE_SHOT);

    TRACE(_T("CodeCompletion::OnEditorActivatedTimer(): Current activated file is %s"), curFile.wx_str());
    UpdateEditorSyntax();

    // If different editor activated and is modified, request LSP sematicTokens update for completions
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    cbEditor* pActiveEditor = pEdMgr->GetBuiltinActiveEditor();
    bool useDocumentationPopup = Manager::Get()->GetConfigManager("ccmanager")->ReadBool("/documentation_popup", false);
    if (useDocumentationPopup and pActiveEditor and pProject and pActiveEditor->GetModified())
        ((Parser*)(GetParseManager()->GetParserByProject(pProject)))->RequestSemanticTokens(pActiveEditor);

}
// ----------------------------------------------------------------------------
wxBitmap ClgdCompletion::GetImage(ImageId::Id id, int fontSize) //unused
// ----------------------------------------------------------------------------
{
    const int size = cbFindMinSize16to64(fontSize);
    const ImageId key(id, size);
    ImagesMap::const_iterator it = m_images.find(key);
    if (it != m_images.end())
        return it->second;

    // Image was not found, add it to the map
    wxString prefix(ConfigManager::GetDataFolder() + "/clangd_client.zip#zip:images/");
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
            CCLogger::Get()->DebugLog(msg);
        }
    }

    m_images[key] = bitmap;
    return bitmap;
}
// ----------------------------------------------------------------------------
wxString ClgdCompletion::GetTargetsOutFilename(cbProject* pProject)
// ----------------------------------------------------------------------------
{
    // Return the build targets output file name or nullString

    ProjectBuildTarget* pTarget = nullptr;
    //-Compiler* actualCompiler = 0;
    wxString buildOutputFile;
    wxString activeBuildTarget;

    if ( pProject)
    {
        //-Log(_("Selecting target: "));
        activeBuildTarget = pProject->GetActiveBuildTarget();
        if (not pProject->BuildTargetValid(activeBuildTarget, false))
        {
            int tgtIdx = pProject->SelectTarget();
            if (tgtIdx == -1)
            {
                //-Log(_("canceled"));
                return wxString();
            }
            pTarget = pProject->GetBuildTarget(tgtIdx);
            activeBuildTarget = (pTarget ? pTarget->GetTitle() : wxString(wxEmptyString));
        }
        else
            pTarget = pProject->GetBuildTarget(activeBuildTarget);

        // make sure it's not a commands-only target
        if (pTarget && pTarget->GetTargetType() == ttCommandsOnly)
        {
            //cbMessageBox(_("The selected target is only running pre/post build step commands\n"
            //               "Can't debug such a target..."), _("Information"), wxICON_INFORMATION);
            //Log(_("aborted"));
            return wxString();
        }
        //-if (target) Log(target->GetTitle());

        if (pTarget )
            buildOutputFile = pTarget->GetOutputFilename();
    }

    if (buildOutputFile.Length())
    {
        return buildOutputFile;
    }

    return wxString();

}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnDebuggerStarting(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    GetParseManager()->SetDebuggerIsRunning(true);

    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    PluginManager* pPlugMgr = Manager::Get()->GetPluginManager();
    ProcessLanguageClient* pClient = GetLSPClient(pProject);
    if (not pClient) return;

    PluginElement* pPlugElements = pPlugMgr->FindElementByName("clangd_client");
    wxFileName pluginLibName = pPlugElements->fileName;

    // Tell the project's parser that the debugger started up
    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (pParser) pParser->OnDebuggerStarting(event);

    // if projects filename matches the LSP client/server dll,
    // shutdown the debuggers LSP client/server to avoid clobbering symbols cache.
    wxFileName fnOutFilename = GetTargetsOutFilename(pProject);

    wxString outFilename = fnOutFilename.GetName().Lower();
    wxString pluginDllName = pluginLibName.GetName().Lower();
    if ( not (outFilename.Contains(pluginDllName.Lower())) )
        return;

    wxString msg = _("Clangd client/server can be shutdown for the project about to be debugged");
    msg += _("\n to avoid multiple processes writing to the same clangd symbols cache.");
    msg += _("\n If you are going to load a project OTHER than the current project as the debuggee");
    msg += _("\n you do not have to shut down the current clangd client.");
    msg += _("\n\n If you choose to shutdown, you can, later, restart clangd via menu 'Project/Reparse current project'.");
    msg += _("\n\nShut down clangd client for this project?");
    AnnoyingDialog annoyingDlg(_("Debugger Starting"), msg, wxART_QUESTION, AnnoyingDialog::YES_NO, AnnoyingDialog::rtSAVE_CHOICE);
    PlaceWindow(&annoyingDlg);
    int answ = annoyingDlg.ShowModal();
    if (answ == AnnoyingDialog::rtNO) return;

    // User wants to shutdown the debugger clangd_client for this project.
    ShutdownLSPclient(pProject); //Shutdown
    // Remove this CodeBlocks pid (this debugger) from the clangd cache lock file
    // for this project. Allow debuggee to clobber current clangd index files.
    GetParseManager()->DoUnlockClangd_CacheAccess(pProject);
}
// ----------------------------------------------------------------------------
void ClgdCompletion::OnDebuggerFinished(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    GetParseManager()->SetDebuggerIsRunning(false);

    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return;
    ProcessLanguageClient* pClient = GetLSPClient(pProject);
    if (not pClient) return;

    // Tell the project's parser that the debugger finished
    Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (pParser) pParser->OnDebuggerFinished(event);
    else return;

    // Reparse open editors for the active project because their clangd info
    // becomes empty after debugging. I don't know why.
    // FIXME (ph#): Find out why clangd info becomes empty after using he debugger.
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    int edCount = pClient ? pEdMgr->GetEditorsCount() : 0;
    for (int ii=0; ii< edCount; ++ii)
    {
        cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(ii);
        if (not pcbEd) continue; //happens because of "Start here" tab
        ProjectFile* pPrjFile = pcbEd->GetProjectFile();
        if (not pPrjFile) continue;
        if (pPrjFile->GetParentProject() == pProject)
        {
            wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, idSpecifiedFileReparse);
            evt.SetString(pcbEd->GetFilename());
            //Event to call OnSpecifiedFileReparse(evt with cbEditor filename);
            Manager::Get()->GetAppFrame()->GetEventHandler()->AddPendingEvent(evt);
        }
    }

}
// ----------------------------------------------------------------------------
bool ClgdCompletion::ParsingIsVeryBusy()
// ----------------------------------------------------------------------------
{
    // suggestion: max parallel files parsing should be no more than half of processors
    int max_parallel_processes = std::max(1, wxThread::GetCPUCount());
    if (max_parallel_processes > 1) max_parallel_processes = max_parallel_processes >> 1; //use only half of cpus
    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");
    int cfg_parallel_processes    = std::max(cfg->ReadInt("/max_threads", 1), 1);            //don't allow 0
    max_parallel_processes        = std::min(max_parallel_processes, cfg_parallel_processes);

    //int cfg_parsers_while_compiling  = std::min(cfg->ReadInt("/max_parsers_while_compiling", 0), max_parallel_processes);
    //int max_parsers_while_compiling  = std::min(cfg_parsers_while_compiling, max_parallel_processes);

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor) return false;

    ProcessLanguageClient* pClient = GetLSPClient(pEditor);
    if ( int(pClient->LSP_GetServerFilesParsingCount()) > max_parallel_processes)
    {
        wxString msg = _("Parsing is very busy, response may be delayed.");
        InfoWindow::Display(_("LSP parsing"), msg, 6000);
        return true;
    }
    return false;
}
// ----------------------------------------------------------------------------
wxString ClgdCompletion::VerifyEditorParsed(cbEditor* pEd)
// ----------------------------------------------------------------------------
{
    wxString msg = _("Error: No active project");
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return msg;

    msg = _("Error: Editor is not eligible for clangd parsing.");
    ProjectFile* pProjectFile = pEd->GetProjectFile();
    pProject = pProjectFile ? pProjectFile->GetParentProject() : nullptr;
    if (not pProject)
    {
        msg += _("\nNo associated project.");
        return msg;
    }

    ProcessLanguageClient* pClient = GetLSPClient(pProject);
    if (not GetLSPClient(pProject) )
    {
        msg += _("\nNo associated Clangd Client");
        return msg;
    }

    bool isEditorInitialized  = pClient ? GetLSP_Initialized(pProject) : false;
    bool isEditorOpen         = isEditorInitialized ? GetLSPClient(pEd)->GetLSP_EditorIsOpen(pEd) : false;
    bool isFileParsing        = pClient ? pClient->IsServerFilesParsing(pEd->GetFilename()) : false;
    bool isEditorParsed       = isEditorOpen ? GetLSP_IsEditorParsed(pEd) : false;

    msg.Clear();
    if (not isEditorInitialized )
       msg = wxString::Format(_("Try again...\nEditor is NOT fully INITIALIZED.\n%s"),
                        wxFileName(pEd->GetFilename()).GetFullName() );
    else if (not isEditorOpen )
       msg = wxString::Format(_("Try again...\nEditor is queued for PARSING.\n%s"),
                    wxFileName(pEd->GetFilename()).GetFullName() );
    else if (isFileParsing or (not isEditorParsed) )
       msg = wxString::Format(_("Try again...\nEditor is BEING PARSED.\n%s"),
                wxFileName(pEd->GetFilename()).GetFullName() );

    if (isEditorOpen and (not isFileParsing) and (not isEditorParsed))
    {
        //something is wrong
        msg = _("Error: Something went wrong.\n Editor is opened for clangd_client. But...");
        if (not isFileParsing)
            msg += "\n" + _("File is not Parsing.");
        if (not isEditorParsed)
            msg += "\n" + _("Editor is not parsed.");
        msg += "\n" + pEd->GetFilename();
        msg += "\n" + _("Project:");
        pProject ? msg += pProject->GetTitle() : _("none");
        CCLogger::Get()->DebugLogError(msg);
        cbMessageBox(msg, _("ERROR: VerifyEditorParsed()"));
    }

    return msg;
}//VerifyEditorParsed
// ----------------------------------------------------------------------------
void ClgdCompletion::OnRequestCodeActionApply(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    wxString msg;
    wxArrayString params = GetArrayFromString(event.GetString(),"|",true);
    wxString filename = params[0];
    wxString lineNumStr = params[1];
    wxString logText = params[2];
    int lineNumInt = -1; //an impossible line number
    try { lineNumInt = std::stoi(lineNumStr.ToStdString()); }
    catch(std::exception &e) { lineNumInt = -1;}

    if (filename.empty() or (not wxFileExists(filename)) or (lineNumInt == -1) )
    {
        msg = wxString::Format(_("%s or line %s not found.\n"), filename, lineNumStr );
    }
    cbEditor* pEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(filename);
    if (not pEd)
    {   msg << wxString::Format(_("No open editor for filename:%s\n"), filename);
    }
    cbProject* pProject = pEd->GetProjectFile() ? pEd->GetProjectFile()->GetParentProject() : nullptr;

    if (not pProject) pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject)
    {
        msg << wxString::Format(_("No project found containing filename:\n %s."), filename);
    }

    if (not msg.empty())
    {
        if (msg.EndsWith('\n')) msg.RemoveLast();
        InfoWindow::Display(_("Apply fix failed"), msg );
        return;
    }

    if (GetLSPClient(pProject) and GetParseManager()->GetParserByProject(pProject))
        GetParseManager()->GetParserByProject(pProject)->OnRequestCodeActionApply(event);

    return;
}//OnRequestCodeActionApply
// ----------------------------------------------------------------------------
wxString ClgdCompletion::VerifyEditorHasSymbols(cbEditor* pEd)
// ----------------------------------------------------------------------------
{
    wxString msg = VerifyEditorParsed(pEd);
    if (not msg.empty()) return msg;

    // VerifyEditorParsed() assures that we have good pProject, pClient
    // VerifyEditorHasSymbols() assures that we have received response for Document Symbols
    // Note that ProxyProject is never requested to produce Document symbols

    cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    ProjectFile* pProjectFile = pEd->GetProjectFile();
    cbProject* pProject = pProjectFile ? pProjectFile->GetParentProject() : nullptr;
    ProcessLanguageClient* pClient = GetLSPClient(pProject);

    bool hasDocSymbols = pClient ? pClient->GetLSP_EditorHasSymbols(pEd) : false;
    if ( (not hasDocSymbols)
        and ((pProject == GetParseManager()->GetProxyProject()) or (pProject != pActiveProject)) )
    {
        msg = _("Editor not associated with active project.\nNo symbols available.");
        return msg;
    }

    // Here: (project == active project) but no DocumentSymbols for this editor.
    // LSP_ParseDocumenSymbols has not responded or was never requested (why?)
    if (not hasDocSymbols)
    {
       msg = _("Try again... Editor symbols DOWNLOADING.");
       msg += _("\n or rightClick in window and select Reparse this file");
       msg += wxString::Format("\n%s", pEd->GetFilename());
    }

    return msg;
}//VerifyEditorParsed
// ----------------------------------------------------------------------------
bool ClgdCompletion::DoShowDiagnostics( cbEditor* ed, int line)  //(Christo 2024/03/30)
// ----------------------------------------------------------------------------
{
	return m_pParseManager->DoShowDiagnostics(ed->GetFilename(), line);
}
