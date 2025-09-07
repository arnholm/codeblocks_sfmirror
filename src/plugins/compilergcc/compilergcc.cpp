/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#ifndef CB_PRECOMP
    #include <wx/app.h>
    #include <wx/toolbar.h>
    #include <wx/frame.h> // GetMenuBar
    #include <wx/gauge.h> // Needs to be before compilergcc.h if NOPCH on wxMSW
    #include <wx/listctrl.h>
    #include <wx/xrc/xmlres.h>
    #include <wx/sizer.h>
    #include <wx/button.h>
    #include <wx/stattext.h>
    #include <wx/statline.h>
    #include <wx/ffile.h>
    #include <wx/utils.h>

    #include "prep.h"
    #include "manager.h"
    #include "sdk_events.h"
    #include "pipedprocess.h"
    #include "configmanager.h"
    #include "compilercommandgenerator.h"
    #include "logmanager.h"
    #include "macrosmanager.h"
    #include "projectmanager.h"
    #include "editormanager.h"
    #include "scriptingmanager.h"
    #include "configurationpanel.h"
    #include "pluginmanager.h"
    #include "cbeditor.h"
    #include "infowindow.h"
    #include "globals.h"
#endif

#include <wx/uri.h>
#include <wx/xml/xml.h>

#include "annoyingdialog.h"
#include "debuggermanager.h"
#include "filefilters.h"
#include "incremental_select_helper.h"

#include "compilergcc.h"
#include "compileroptionsdlg.h"
#include "directcommands.h"
#include "cbart_provider.h"
#include "cbworkspace.h"
#include "cbstyledtextctrl.h"
#include "scripting/bindings/sc_utils.h"
#include "scripting/bindings/sc_typeinfo_all.h"

#include "compilerMINGW.h"
#include "compilerGNUARM.h"
#include "compilerMSVC.h"
#include "compilerMSVC8.h"
#include "compilerMSVC10.h"
#include "compilerMSVC17.h"
#include "compilerOW.h"
#include "compilerGNUARM.h"
#include "compilerCYGWIN.h"
#include "compilerLCC.h"
#include "compilerKeilC51.h"
#include "compilerIAR.h"
#include "compilerICC.h"
#include "compilerGDC.h"
#include "compilerGNUFortran.h"
#include "compilerG95.h"
#include "compilerXML.h"

namespace ScriptBindings
{
    static int gBuildLogId = -1;

    // global funcs
    SQInteger gBuildLog(HSQUIRRELVM v)
    {
        // env table, msg
        ExtractParams2<SkipParam, const wxString *> extractor(v);
        if (!extractor.Process("LogBuild"))
            return extractor.ErrorMessage();

        Manager::Get()->GetLogManager()->Log(*extractor.p1, gBuildLogId);
        return 0;
    }
}

const int idBuildLog = wxNewId();

class BuildLogger : public TextCtrlLogger
{
    wxPanel* panel;
    wxBoxSizer* sizer;
public:
    wxGauge* progress;

    BuildLogger() : TextCtrlLogger(true), panel(nullptr), sizer(nullptr), progress(nullptr) {}

    void UpdateSettings() override
    {
        TextCtrlLogger::UpdateSettings();

        style[caption].SetAlignment(wxTEXT_ALIGNMENT_DEFAULT);
        style[caption].SetFont(style[error].GetFont());
        style[error].SetFont(style[info].GetFont());
    }

    wxWindow* CreateControl(wxWindow* parent) override
    {
        panel = new wxPanel(parent);

        TextCtrlLogger::CreateControl(panel);
        control->SetId(idBuildLog);

        sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(control, 1, wxEXPAND, 0);
        panel->SetSizer(sizer);

        return panel;
    }

    void AddBuildProgressBar()
    {
        if (!progress)
        {
            progress = new wxGauge(panel, -1, 0, wxDefaultPosition, wxSize(-1, 12));
            sizer->Add(progress, 0, wxEXPAND);
            sizer->Layout();
        }
    }

    void RemoveBuildProgressBar()
    {
        if (progress)
        {
            sizer->Detach(progress);
            progress->Destroy();
            progress = 0;
            sizer->Layout();
        }
    }

    void OpenLink(long urlStart, long urlEnd)
    {
        if (!control)
            return;
        wxString url = control->GetRange(urlStart, urlEnd);
        if (platform::windows && url.StartsWith(_T("file://")))
            url.Remove(0, 7);
        cbMimePlugin* p = Manager::Get()->GetPluginManager()->GetMIMEHandlerForFile(url);
        if (p)
            p->OpenFile(url);
        else
            wxLaunchDefaultBrowser(url);
    }
};

namespace
{
    PluginRegistrant<CompilerGCC> reg(_T("Compiler"));

    static const wxString strCONSOLE_RUNNER(platform::windows ? _T("cb_console_runner.exe") : _T("cb_console_runner"));
    static const wxString strSLASH(_T("/"));
    static const wxString strSPACE(_T(" "));
    static const wxString strQUOTE(platform::windows ? _T("\"") : _T("'"));
}

// menu IDS
// just because we don't know other plugins' used identifiers,
// we use wxNewId() to generate a guaranteed unique ID ;), instead of enum
// (don't forget that, especially in a plugin)
int idTimerPollCompiler                            = XRCID("idTimerPollCompiler");
int idMenuCompile                                  = XRCID("idCompilerMenuCompile");
int idMenuCompileTarget                            = wxNewId();
int idMenuCompileFromProjectManager                = wxNewId();
int idMenuProjectCompilerOptions                   = wxNewId();
int idMenuProjectCompilerOptionsFromProjectManager = wxNewId();
int idMenuTargetCompilerOptions                    = wxNewId();
int idMenuTargetCompilerOptionsSub                 = wxNewId();
int idMenuCompileFile                              = XRCID("idCompilerMenuCompileFile");
int idMenuCompileFileFromProjectManager            = wxNewId();
int idMenuCleanFileFromProjectManager              = wxNewId();
int idMenuRebuild                                  = XRCID("idCompilerMenuRebuild");
int idMenuRebuildTarget                            = wxNewId();
int idMenuRebuildFromProjectManager                = wxNewId();
int idMenuClean                                    = XRCID("idCompilerMenuClean");
int idMenuBuildWorkspace                           = XRCID("idCompilerMenuBuildWorkspace");
int idMenuRebuildWorkspace                         = XRCID("idCompilerMenuRebuildWorkspace");
int idMenuCleanWorkspace                           = XRCID("idCompilerMenuCleanWorkspace");
int idMenuCleanTarget                              = wxNewId();
int idMenuCleanFromProjectManager                  = wxNewId();
int idMenuCompileAndRun                            = XRCID("idCompilerMenuCompileAndRun");
int idMenuRun                                      = XRCID("idCompilerMenuRun");
int idMenuKillProcess                              = XRCID("idCompilerMenuKillProcess");
int idMenuSelectTarget                             = XRCID("idCompilerMenuSelectTarget");

// Limit the number of menu items to try to make them all visible on the screen.
// Scrolling menus is not the best user experience.
const int maxTargetInMenus = 40;
int idMenuSelectTargetOther[maxTargetInMenus]; // initialized in ctor
int idMenuSelectTargetDialog                       = XRCID("idMenuSelectTargetDialog");
int idMenuSelectTargetHasMore                      = wxNewId();

int idMenuNextError                                = XRCID("idCompilerMenuNextError");
int idMenuPreviousError                            = XRCID("idCompilerMenuPreviousError");
int idMenuClearErrors                              = XRCID("idCompilerMenuClearErrors");
int idMenuSettings                                 = XRCID("idCompilerMenuSettings");

int idToolTarget                                   = XRCID("idToolTarget");

int idGCCProcess = wxNewId();

BEGIN_EVENT_TABLE(CompilerGCC, cbCompilerPlugin)
    EVT_UPDATE_UI(idMenuCompile,                       CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCompileTarget,                 CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCompileFromProjectManager,     CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuProjectCompilerOptions,        CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuTargetCompilerOptions,         CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuTargetCompilerOptionsSub,      CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCompileFile,                   CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCompileFileFromProjectManager, CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCleanFileFromProjectManager,   CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuRebuild,                       CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuRebuildTarget,                 CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuRebuildFromProjectManager,     CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuBuildWorkspace,                CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuRebuildWorkspace,              CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuClean,                         CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCleanWorkspace,                CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCleanTarget,                   CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCleanFromProjectManager,       CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuCompileAndRun,                 CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuRun,                           CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuKillProcess,                   CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuSelectTarget,                  CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuNextError,                     CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuPreviousError,                 CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuClearErrors,                   CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuSettings,                      CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idToolTarget,                        CompilerGCC::OnUpdateUI)
    EVT_UPDATE_UI(idMenuSelectTargetDialog,            CompilerGCC::OnUpdateUI)

    EVT_IDLE(                                       CompilerGCC::OnIdle)
    EVT_TIMER(idTimerPollCompiler,                  CompilerGCC::OnTimer)

    EVT_MENU(idMenuRun,                             CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCompileAndRun,                   CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCompile,                         CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCompileFromProjectManager,       CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCompileFile,                     CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCompileFileFromProjectManager,   CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCleanFileFromProjectManager,     CompilerGCC::Dispatcher)
    EVT_MENU(idMenuRebuild,                         CompilerGCC::Dispatcher)
    EVT_MENU(idMenuRebuildFromProjectManager,       CompilerGCC::Dispatcher)
    EVT_MENU(idMenuBuildWorkspace,                  CompilerGCC::Dispatcher)
    EVT_MENU(idMenuRebuildWorkspace,                CompilerGCC::Dispatcher)
    EVT_MENU(idMenuProjectCompilerOptions,          CompilerGCC::Dispatcher)
    EVT_MENU(idMenuProjectCompilerOptionsFromProjectManager, CompilerGCC::Dispatcher)
    EVT_MENU(idMenuTargetCompilerOptions,           CompilerGCC::Dispatcher)
    EVT_MENU(idMenuClean,                           CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCleanWorkspace,                  CompilerGCC::Dispatcher)
    EVT_MENU(idMenuCleanFromProjectManager,         CompilerGCC::Dispatcher)
    EVT_MENU(idMenuKillProcess,                     CompilerGCC::Dispatcher)
    EVT_MENU(idMenuNextError,                       CompilerGCC::Dispatcher)
    EVT_MENU(idMenuPreviousError,                   CompilerGCC::Dispatcher)
    EVT_MENU(idMenuClearErrors,                     CompilerGCC::Dispatcher)
    EVT_MENU(idMenuSettings,                        CompilerGCC::Dispatcher)

    EVT_TEXT_URL(idBuildLog,                        CompilerGCC::TextURL)

    EVT_CHOICE(idToolTarget,                        CompilerGCC::OnSelectTarget)
    EVT_MENU(idMenuSelectTargetDialog,              CompilerGCC::OnSelectTarget)

    EVT_PIPEDPROCESS_STDOUT(idGCCProcess, CompilerGCC::OnGCCOutput)
    EVT_PIPEDPROCESS_STDERR(idGCCProcess, CompilerGCC::OnGCCError)
    EVT_PIPEDPROCESS_TERMINATED(idGCCProcess, CompilerGCC::OnGCCTerminated)
END_EVENT_TABLE()

CompilerGCC::CompilerGCC() :
    m_RealTargetsStartIndex(0),
    m_RealTargetIndex(0),
    m_PageIndex(-1),
    m_ListPageIndex(-1),
    m_Menu(nullptr),
    m_TargetMenu(nullptr),
    m_TargetIndex(-1),
    m_pErrorsMenu(nullptr),
    m_pProject(nullptr),
    m_pTbar(nullptr),
    m_pLog(nullptr),
    m_pListLog(nullptr),
    m_pToolTarget(nullptr),
    m_RunAfterCompile(false),
    m_LastExitCode(0),
    m_NotifiedMaxErrors(false),
    m_pBuildingProject(nullptr),
    m_BuildJob(bjIdle),
    m_NextBuildState(bsNone),
    m_pLastBuildingProject(nullptr),
    m_pLastBuildingTarget(nullptr),
    m_Clean(false),
    m_Build(false),
    m_LastBuildStep(true),
    m_RunTargetPostBuild(false),
    m_RunProjectPostBuild(false),
    m_IsWorkspaceOperation(false),
    m_LogBuildProgressPercentage(false),
    m_pArtProvider(nullptr)
{
    if (!Manager::LoadResource(_T("compiler.zip")))
        NotifyMissingFile(_T("compiler.zip"));

    m_StartedEventSent = false;
}

CompilerGCC::~CompilerGCC()
{
}

void CompilerGCC::OnAttach()
{
    // reset all vars
    m_RealTargetsStartIndex = 0;
    m_RealTargetIndex = 0;
    m_PageIndex = -1;
    m_ListPageIndex = -1;
    m_Menu = nullptr;
    m_TargetMenu = nullptr;
    m_TargetIndex = -1;
    m_pErrorsMenu = nullptr;
    m_pProject = nullptr;
    m_pTbar = nullptr;
    m_pLog = nullptr;
    m_pListLog = nullptr;
    m_pToolTarget = nullptr;
    m_RunAfterCompile = false;
    m_LastExitCode = 0;
    m_NotifiedMaxErrors = false;
    m_pBuildingProject = nullptr;
    m_BuildJob = bjIdle;
    m_NextBuildState = bsNone;
    m_pLastBuildingProject = nullptr;
    m_pLastBuildingTarget = nullptr;
    m_RunTargetPostBuild = false;
    m_RunProjectPostBuild = false;
    m_Clean = false;
    m_Build = false;
    m_LastBuildStep = true;
    m_IsWorkspaceOperation = false;

    m_timerIdleWakeUp.SetOwner(this, idTimerPollCompiler);

    for (int i = 0; i < maxTargetInMenus; ++i)
        idMenuSelectTargetOther[i] = wxNewId();

    DoRegisterCompilers();

    AllocProcesses();

    LogManager* msgMan = Manager::Get()->GetLogManager();

    {
        const wxString prefix(ConfigManager::GetDataFolder() + wxT("/compiler.zip#zip:/images"));
        m_pArtProvider = new cbArtProvider(prefix);

#if wxCHECK_VERSION(3, 1, 6)
        const wxString ext(".svg");
#else
        const wxString ext(".png");
#endif

        m_pArtProvider->AddMapping("compiler/compile",     "compile"+ext);
        m_pArtProvider->AddMapping("compiler/run",         "run"+ext);
        m_pArtProvider->AddMapping("compiler/compile_run", "compilerun"+ext);
        m_pArtProvider->AddMapping("compiler/rebuild",     "rebuild"+ext);
        m_pArtProvider->AddMapping("compiler/stop",        "stop"+ext);

        wxArtProvider::Push(m_pArtProvider);
    }

    // create compiler's log
    m_pLog = new BuildLogger();
    m_PageIndex = msgMan->SetLog(m_pLog);
    msgMan->Slot(m_PageIndex).title = _("Build log");
//    msgMan->SetBatchBuildLog(m_PageIndex);
    // set log image
    wxString prefix(ConfigManager::GetDataFolder()+"/resources.zip#zip:/images/infopane/");
#if wxCHECK_VERSION(3, 1, 6)
    wxBitmapBundle* bmp = new wxBitmapBundle(cbLoadBitmapBundleFromSVG(prefix+"svg/misc.svg", wxSize(16, 16)));
#else
    const int uiSize = Manager::Get()->GetImageSize(Manager::UIComponent::InfoPaneNotebooks);
    prefix << wxString::Format("%dx%d/", uiSize, uiSize);
    wxBitmap* bmp = new wxBitmap(cbLoadBitmap(prefix+"misc.png", wxBITMAP_TYPE_PNG));
#endif
    msgMan->Slot(m_PageIndex).icon = bmp;

    // create warnings/errors log
    wxArrayString titles;
    wxArrayInt widths;
    titles.Add(_("File"));
    titles.Add(_("Line"));
    titles.Add(_("Message"));
    widths.Add(128);
    widths.Add(48);
    widths.Add(640);

    m_pListLog = new CompilerMessages(titles, widths);
    m_pListLog->SetCompilerErrors(&m_Errors);
    m_ListPageIndex = msgMan->SetLog(m_pListLog);
    msgMan->Slot(m_ListPageIndex).title = _("Build messages");
    // set log image
#if wxCHECK_VERSION(3, 1, 6)
    bmp = new wxBitmapBundle(cbLoadBitmapBundleFromSVG(prefix+"svg/flag.svg", wxSize(16, 16)));
#else
    bmp = new wxBitmap(cbLoadBitmap(prefix+"flag.png", wxBITMAP_TYPE_PNG));
#endif
    msgMan->Slot(m_ListPageIndex).icon = bmp;

    CodeBlocksLogEvent evtAdd1(cbEVT_ADD_LOG_WINDOW, m_pLog, msgMan->Slot(m_PageIndex).title, msgMan->Slot(m_PageIndex).icon);
    Manager::Get()->ProcessEvent(evtAdd1);
    if (!Manager::IsBatchBuild())
    {
        CodeBlocksLogEvent evtAdd2(cbEVT_ADD_LOG_WINDOW, m_pListLog, msgMan->Slot(m_ListPageIndex).title, msgMan->Slot(m_ListPageIndex).icon);
        Manager::Get()->ProcessEvent(evtAdd2);
    }

    m_LogBuildProgressPercentage = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/build_progress/percentage"), false);
    bool hasBuildProg = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/build_progress/bar"), false);
    if (hasBuildProg)
        m_pLog->AddBuildProgressBar();

    // set default compiler for new projects
    CompilerFactory::SetDefaultCompiler(Manager::Get()->GetConfigManager(_T("compiler"))->Read(_T("/default_compiler"), _T("gcc")));
    LoadOptions();

    {
        // register compiler's script functions
        // make sure the VM is initialized
        ScriptingManager* scriptMgr = Manager::Get()->GetScriptingManager();
        HSQUIRRELVM vm = scriptMgr->GetVM();
        if (vm)
        {
            // FIXME (squirrel) Write documentation about this in the wiki
            ScriptBindings::PreserveTop preserveTop(vm);
            sq_pushroottable(vm);
            ScriptBindings::gBuildLogId = m_PageIndex;
            ScriptBindings::BindMethod(vm, _SC("LogBuild"), ScriptBindings::gBuildLog, nullptr);
            sq_poptop(vm);
        }
        else
            ScriptBindings::gBuildLogId = -1;
    }

    // register event sink
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_ACTIVATE,         new cbEventFunctor<CompilerGCC, CodeBlocksEvent>(this, &CompilerGCC::OnProjectActivated));
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_OPEN,             new cbEventFunctor<CompilerGCC, CodeBlocksEvent>(this, &CompilerGCC::OnProjectLoaded));
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_CLOSE,            new cbEventFunctor<CompilerGCC, CodeBlocksEvent>(this, &CompilerGCC::OnProjectUnloaded));
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_TARGETS_MODIFIED, new cbEventFunctor<CompilerGCC, CodeBlocksEvent>(this, &CompilerGCC::OnProjectActivated));
    Manager::Get()->RegisterEventSink(cbEVT_WORKSPACE_CLOSING_COMPLETE, new cbEventFunctor<CompilerGCC, CodeBlocksEvent>(this, &CompilerGCC::OnWorkspaceClosed));

    Manager::Get()->RegisterEventSink(cbEVT_COMPILE_FILE_REQUEST,     new cbEventFunctor<CompilerGCC, CodeBlocksEvent>(this, &CompilerGCC::OnCompileFileRequest));
}

void CompilerGCC::OnRelease(bool appShutDown)
{
    // disable script functions
    ScriptBindings::gBuildLogId = -1;

    SaveOptions();
    Manager::Get()->GetConfigManager(_T("compiler"))->Write(_T("/default_compiler"), CompilerFactory::GetDefaultCompilerID());
    LogManager* logManager = Manager::Get()->GetLogManager();
    if (logManager)
    {
        // for batch builds, the log is deleted by the manager
        if (!Manager::IsBatchBuild())
        {
            CodeBlocksLogEvent evt(cbEVT_REMOVE_LOG_WINDOW, m_pLog);
            Manager::Get()->ProcessEvent(evt);
        }

        {
            // TODO: This is wrong. We need some automatic way for this to happen!!!
            LogSlot &listSlot = logManager->Slot(m_ListPageIndex);
            delete listSlot.icon;
            listSlot.icon = nullptr;

            LogSlot &slot = logManager->Slot(m_PageIndex);
            delete slot.icon;
            slot.icon = nullptr;
        }

        m_pLog = nullptr;

        CodeBlocksLogEvent evt(cbEVT_REMOVE_LOG_WINDOW, m_pListLog);
        m_pListLog->DestroyControls();
        Manager::Get()->ProcessEvent(evt);
        m_pListLog = nullptr;
    }

    // let wx handle this on shutdown ( if we return here Valgrind will be sad :'( )
    if (!appShutDown)
        DoClearTargetMenu();

    m_timerIdleWakeUp.Stop();

    FreeProcesses();

    CompilerFactory::UnregisterCompilers();

    wxArtProvider::Delete(m_pArtProvider);
    m_pArtProvider = nullptr;
}

int CompilerGCC::Configure(cbProject* project, ProjectBuildTarget* target, wxWindow* parent)
{
    cbConfigurationDialog dlg(parent, wxID_ANY, _("Project build options"));
    cbConfigurationPanel* panel = new CompilerOptionsDlg(&dlg, this, project, target);
    panel->SetParentDialog(&dlg);
    dlg.AttachConfigurationPanel(panel);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        SaveOptions();
        Manager::Get()->GetMacrosManager()->Reset();

        bool hasBuildProg = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/build_progress/bar"), false);
        if (hasBuildProg)
            m_pLog->AddBuildProgressBar();
        else
            m_pLog->RemoveBuildProgressBar();

        CodeBlocksEvent settingsEvent(cbEVT_SETTINGS_CHANGED);
        settingsEvent.SetInt(int(cbSettingsType::BuildOptions));
        settingsEvent.SetProject(project);
        settingsEvent.SetBuildTargetName(target ? target->GetTitle() : wxString());
        Manager::Get()->ProcessEvent(settingsEvent);
    }
//    delete panel;
    return 0;
}

cbConfigurationPanel* CompilerGCC::GetConfigurationPanel(wxWindow* parent)
{
    CompilerOptionsDlg* dlg = new CompilerOptionsDlg(parent, this, nullptr, nullptr);
    return dlg;
}

void CompilerGCC::OnConfig(cb_unused wxCommandEvent& event)
{
    Configure(nullptr, nullptr, Manager::Get()->GetAppWindow());
}

void CompilerGCC::BuildMenu(wxMenuBar* menuBar)
{
    if (!IsAttached())
        return;

    m_Menu = Manager::Get()->LoadMenu(_T("compiler_menu"),true);

    // target selection menu
    wxMenuItem* tmpitem=m_Menu->FindItem(idMenuSelectTarget, nullptr);
    m_TargetMenu = tmpitem ? tmpitem->GetSubMenu() : new wxMenu(_T(""));
    DoRecreateTargetMenu();
    //m_Menu->Append(idMenuSelectTarget, _("Select target..."), m_TargetMenu);

    // ok, now, where do we insert?
    // three possibilities here:
    // a) locate "Debug" menu and insert before it
    // b) locate "Project" menu and insert after it
    // c) if not found (?), insert at pos 5
    int finalPos = 5;
    int projMenuPos = menuBar->FindMenu(_("&Debug"));
    if (projMenuPos != wxNOT_FOUND)
        finalPos = projMenuPos;
    else
    {
        projMenuPos = menuBar->FindMenu(_("&Project"));
        if (projMenuPos != wxNOT_FOUND)
            finalPos = projMenuPos + 1;
    }
    menuBar->Insert(finalPos, m_Menu, _("&Build"));

    // now add some entries in Project menu
    projMenuPos = menuBar->FindMenu(_("&Project"));
    if (projMenuPos != wxNOT_FOUND)
    {
        wxMenu* prj = menuBar->GetMenu(projMenuPos);
        // look if we have a "Properties" item. If yes, we 'll insert
        // before it, else we 'll just append...
        size_t propsPos = prj->GetMenuItemCount(); // append
        const int idMenuProjectProperties = prj->FindItem(_("Properties..."));
        if (idMenuProjectProperties != wxNOT_FOUND)
            prj->FindChildItem(idMenuProjectProperties, &propsPos);
        prj->Insert(propsPos, idMenuProjectCompilerOptions, _("Build options..."), _("Set the project's build options"));
        prj->InsertSeparator(propsPos);
    }
}

void CompilerGCC::BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
{
    if (!IsAttached())
        return;
    // we 're only interested in project manager's menus
    if (type != mtProjectManager || !menu)
        return;

    if (!CheckProject())
        return;

    if (!data || data->GetKind() == FileTreeData::ftdkUndefined)
    {
        // popup menu in empty space in ProjectManager
        if (menu->GetMenuItemCount() > 0)
            menu->AppendSeparator();
        menu->Append(idMenuBuildWorkspace,   _("Build workspace"));
        menu->Append(idMenuRebuildWorkspace, _("Rebuild workspace"));
        menu->Append(idMenuCleanWorkspace,   _("Clean workspace"));

        if (IsRunning())
        {
            menu->Enable(idMenuBuildWorkspace, false);
            menu->Enable(idMenuRebuildWorkspace, false);
            menu->Enable(idMenuCleanWorkspace, false);
        }
    }
    else if (data && data->GetKind() == FileTreeData::ftdkProject)
    {
        // popup menu on a project
        wxMenuItem* itm = menu->FindItemByPosition(menu->GetMenuItemCount() - 1);
        if (itm && !itm->IsSeparator())
            menu->AppendSeparator();
        menu->Append(idMenuCompileFromProjectManager, _("Build"));
        menu->Append(idMenuRebuildFromProjectManager, _("Rebuild"));
        menu->Append(idMenuCleanFromProjectManager,   _("Clean"));
        menu->AppendSeparator();
        menu->Append(idMenuProjectCompilerOptionsFromProjectManager, _("Build options..."));

        cbPlugin* otherRunning = Manager::Get()->GetProjectManager()->GetIsRunning();
        if (IsRunning() || (otherRunning && otherRunning != this))
        {
            menu->Enable(idMenuCompileFromProjectManager, false);
            menu->Enable(idMenuRebuildFromProjectManager, false);
            menu->Enable(idMenuCleanFromProjectManager, false);
            menu->Enable(idMenuProjectCompilerOptionsFromProjectManager, false);
        }
    }
    else if (data && data->GetKind() == FileTreeData::ftdkFile)
    {
        FileType ft = FileTypeOf(data->GetProjectFile()->relativeFilename);
        if (ft == ftSource || ft == ftHeader || ft == ftTemplateSource)
        {
            // popup menu on a compilable file
            menu->AppendSeparator();
            menu->Append(idMenuCompileFileFromProjectManager, _("Build file"));
            menu->Append(idMenuCleanFileFromProjectManager,   _("Clean file"));
            if (IsRunning())
            {
                menu->Enable(idMenuCompileFileFromProjectManager, false);
                menu->Enable(idMenuCleanFileFromProjectManager,   false);
            }
        }
    }
}

bool CompilerGCC::BuildToolBar(wxToolBar* toolBar)
{
    if (!IsAttached() || !toolBar)
        return false;

    m_pTbar = toolBar;
    Manager::Get()->AddonToolBar(toolBar, _T("compiler_toolbar"));
    m_pToolTarget = XRCCTRL(*toolBar, "idToolTarget", wxChoice);
    toolBar->Realize();
    toolBar->SetInitialSize();
    DoRecreateTargetMenu(); // make sure the tool target combo is up-to-date
    return true;
}

void CompilerGCC::Dispatcher(wxCommandEvent& event)
{
    int eventId = event.GetId();

//    Manager::Get()->GetMessageManager()->Log(wxT("Dispatcher")));

    if (eventId == idMenuRun)
        OnRun(event);
    else if (eventId == idMenuCompileAndRun)
        OnCompileAndRun(event);
    else if (eventId == idMenuCompile)
        OnCompile(event);
    else if (eventId == idMenuCompileFromProjectManager)
        OnCompile(event);
    else if (eventId == idMenuCompileFile)
        OnCompileFile(event);
    else if (eventId == idMenuCompileFileFromProjectManager)
        OnCompileFile(event);
    else if (eventId == idMenuCleanFileFromProjectManager)
        OnCleanFile(event);
    else if (eventId == idMenuRebuild)
        OnRebuild(event);
    else if (eventId == idMenuRebuildFromProjectManager)
        OnRebuild(event);
    else if (eventId == idMenuBuildWorkspace)
        OnCompileAll(event);
    else if (eventId == idMenuRebuildWorkspace)
        OnRebuildAll(event);
    else if (   eventId == idMenuProjectCompilerOptions
             || eventId == idMenuProjectCompilerOptionsFromProjectManager )
        OnProjectCompilerOptions(event);
    else if (eventId == idMenuTargetCompilerOptions)
        OnTargetCompilerOptions(event);
    else if (eventId == idMenuClean)
        OnClean(event);
    else if (eventId == idMenuCleanWorkspace)
        OnCleanAll(event);
    else if (eventId == idMenuCleanFromProjectManager)
        OnClean(event);
    else if (eventId == idMenuKillProcess)
        OnKillProcess(event);
    else if (eventId == idMenuNextError)
        OnNextError(event);
    else if (eventId == idMenuPreviousError)
        OnPreviousError(event);
    else if (eventId == idMenuClearErrors)
        OnClearErrors(event);
    else if (eventId == idMenuSettings)
        OnConfig(event);

    // Return focus to current editor
    cbEditor* ed = nullptr;
    if ( (ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor()) )
        ed->GetControl()->SetFocus();
}

void CompilerGCC::TextURL(wxTextUrlEvent& event)
{
    if (event.GetId() == idBuildLog && event.GetMouseEvent().ButtonDown(wxMOUSE_BTN_LEFT))
        m_pLog->OpenLink(event.GetURLStart(), event.GetURLEnd());
    else
        event.Skip();
}

void CompilerGCC::SetupEnvironment()
{
    // Special case so "No Compiler" is valid, but I'm not sure there is
    // any valid reason to continue with this function.
    // If we do continue there are wx3 asserts, because of empty paths.
    if (m_CompilerId == wxT("null"))
        return;

    Compiler* compiler = CompilerFactory::GetCompiler(m_CompilerId);
    if (!compiler)
        return;

    wxString currentPath;
    if ( !wxGetEnv(_T("PATH"), &currentPath) )
    {
        InfoWindow::Display(_("Environment error"),
                            _("Could not read the PATH environment variable!\n"
                              "This can't be good. There may be problems running\n"
                              "system commands and the application might not behave\n"
                              "the way it was designed to..."),
                            15000, 3000);
        return;
    }

//    Manager::Get()->GetLogManager()->DebugLogError(_T("PATH environment:"));
//    Manager::Get()->GetLogManager()->DebugLogError(currentPath);

    const wxString pathApp  = platform::windows ? _T(";") : _T(":");
    const wxString pathSep  = wxFileName::GetPathSeparator(); // "\" or "/"
    const bool     caseSens = !(platform::windows);

    wxString      cApp       = compiler->GetPrograms().C;
    Manager::Get()->GetMacrosManager()->ReplaceMacros(cApp);
    wxArrayString extraPaths = compiler->GetExtraPaths();
    wxString      extraPathsBinPath(wxEmptyString);

    // Get configured masterpath, expand macros and remove trailing separators
    wxString masterPath = compiler->GetMasterPath();

    Manager::Get()->GetMacrosManager()->ReplaceMacros(masterPath);
    while (   !masterPath.IsEmpty()
           && ((masterPath.Last() == '\\') || (masterPath.Last() == '/')) )
        masterPath.RemoveLast();

    // Compile new PATH list...
    wxPathList pathList;
    // [1] Pre-pend "master path" and "master path\bin"...
    if ( !masterPath.Trim().IsEmpty() ) // Would be very bad, if it *is* empty
    {
        pathList.Add(masterPath + pathSep + _T("bin"));
        pathList.Add(masterPath); // in case there is no "bin" sub-folder
    }

    // [2] Get configured extrapath(s), expand macros and remove trailing separators
    for (size_t i = 0; i < extraPaths.GetCount(); ++i)
    {
        wxString extraPath = extraPaths[i];
        if (!extraPath.empty())
        {
            Manager::Get()->GetMacrosManager()->ReplaceMacros(extraPath);
            while (!extraPath.empty() && (extraPath.Last() == '\\' || extraPath.Last() == '/'))
                extraPath.RemoveLast();

            if (!extraPath.Trim().empty())
            {
                // Remember, if we found the C application in the extra path's:
                if (extraPathsBinPath.empty() && wxFileExists(extraPath + pathSep + cApp))
                    extraPathsBinPath = extraPath;

                pathList.Add(extraPath);
            }
        }
    }

    // [3] Append what has already been in the PATH envvar...
    // If we do it this way, paths are automatically normalized and doubles are removed
    wxPathList pathArray;
    pathArray.AddEnvList(_T("PATH"));
    pathList.Add(pathArray);

    // Try to locate the path to the C compiler:
    wxString binPath = pathList.FindAbsoluteValidPath(cApp);

    // It seems, under Win32, the above command doesn't search in paths with spaces...
    // Look directly for the file in question in masterPath if it is not already found.
    if (    binPath.IsEmpty()
        || (pathList.Index(wxPathOnly(binPath), caseSens)==wxNOT_FOUND) )
    {
        if      (wxFileExists(masterPath + pathSep + _T("bin") + pathSep + cApp))
            binPath = masterPath + pathSep + _T("bin");
        else if (wxFileExists(masterPath + pathSep + cApp))
            binPath = masterPath;
        else if (!extraPathsBinPath.IsEmpty())
            binPath = extraPathsBinPath;
    }
    else
        binPath = wxPathOnly(binPath);

    /* TODO (jens#1#): Is the above correct ?
       Or should we search in the whole systempath (pathList in this case) for the executable? */
    // Try again...
    if ((binPath.IsEmpty() || (pathList.Index(binPath, caseSens)==wxNOT_FOUND)))
    {
        InfoWindow::Display(_("Environment error"),
                            wxString::Format(_("Can't find compiler executable in your configured search paths for %s\n"), compiler->GetName()));
        Manager::Get()->GetLogManager()->DebugLogError(wxString::Format(_("Can't find compiler executable in your configured search paths (for %s)..."), compiler->GetName()));

        return; // Failed to locate compiler executable in path's as provided!
    }

    // Convert the pathList into a string to apply.
    wxString envPath(binPath); // make sure the bin-path we found is in front
    // and remove it from pathList
    pathList.Remove(binPath);
    for (size_t i=0; i<pathList.GetCount(); ++i)
        envPath += ( pathApp + pathList[i] );

//    Manager::Get()->GetLogManager()->DebugLogError(_T("Updating compiler PATH environment:"));
//    Manager::Get()->GetLogManager()->DebugLogError(envPath);

    if ( !wxSetEnv(_T("PATH"), envPath) )
    {
        InfoWindow::Display(_("Environment error"),
                            _("Can't set PATH environment variable! That's bad and the compiler might not work."));
        Manager::Get()->GetLogManager()->DebugLog(_T("Can't set PATH environment variable! That's bad and the compiler might not work.\n"));
    }
}

bool CompilerGCC::StopRunningDebugger()
{
    cbDebuggerPlugin* dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    // is the debugger running?
    if (dbg && dbg->IsRunning())
    {
        int ret = cbMessageBox(_("The debugger must be stopped to do a (re-)build.\n"
                                 "Do you want to stop the debugger now?"),
                                 _("Information"),
                                wxYES_NO | wxCANCEL | wxICON_QUESTION);
        switch (ret)
        {
            case wxID_YES:
            {
                m_pLog->Clear();
                Manager::Get()->GetLogManager()->Log(_("Stopping debugger..."), m_PageIndex);
                dbg->Stop();
                break;
            }
            case wxID_NO: // fall through
            default:
                Manager::Get()->GetLogManager()->Log(_("Aborting (re-)build."), m_PageIndex);
                return false;
        }
    }

    return true;
}

void CompilerGCC::SaveOptions()
{
    // save compiler sets
    CompilerFactory::SaveSettings();
}

void CompilerGCC::LoadOptions()
{
    // load compiler sets
    CompilerFactory::LoadSettings();
}

void CompilerGCC::DoRegisterCompilers()
{
    bool nonPlatComp = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/non_plat_comp"), false);

    // register built-in compilers
    CompilerFactory::RegisterCompiler(new CompilerMINGW);
    if (platform::windows || nonPlatComp)
    {
        CompilerFactory::RegisterCompiler(new CompilerMSVC);
        CompilerFactory::RegisterCompiler(new CompilerMSVC8);
        CompilerFactory::RegisterCompiler(new CompilerMSVC10);
        CompilerFactory::RegisterCompiler(new CompilerMSVC17);
        CompilerFactory::RegisterCompiler(new CompilerOW);
        CompilerFactory::RegisterCompiler(new CompilerCYGWIN);
        CompilerFactory::RegisterCompiler(new CompilerLCC);
        CompilerFactory::RegisterCompiler(new CompilerKeilC51);
        CompilerFactory::RegisterCompiler(new CompilerKeilCX51);
        CompilerFactory::RegisterCompiler(new CompilerIAR(wxT("8051")));
        CompilerFactory::RegisterCompiler(new CompilerIAR(wxT("ARM")));
    }
    CompilerFactory::RegisterCompiler(new CompilerICC);
    CompilerFactory::RegisterCompiler(new CompilerGDC);
    CompilerFactory::RegisterCompiler(new CompilerGNUFortran);
    CompilerFactory::RegisterCompiler(new CompilerG95);
    if (platform::windows || platform::Linux || nonPlatComp)
        CompilerFactory::RegisterCompiler(new CompilerGNUARM);

    // register pure XML compilers
    // user paths first
    wxDir dir;
    wxString filename;
    wxArrayString compilers;
    wxString path = ConfigManager::GetFolder(sdDataUser) + wxT("/compilers/");
    if (wxDirExists(path) && dir.Open(path))
    {
        bool ok = dir.GetFirst(&filename, wxT("compiler_*.xml"), wxDIR_FILES);
        while (ok)
        {
            compilers.Add(path + filename);
            ok = dir.GetNext(&filename);
        }
    }
    // global paths next
    path = ConfigManager::GetFolder(sdDataGlobal) + wxT("/compilers/");
    if (wxDirExists(path) && dir.Open(path))
    {
        bool ok = dir.GetFirst(&filename, wxT("compiler_*.xml"), wxDIR_FILES);
        while (ok)
        {
            for (size_t i = 0; i < compilers.GetCount(); ++i)
            {
                if (compilers[i].EndsWith(filename))
                {
                    ok = false;
                    break;
                }
            }
            if (ok) // user compilers of the same name take precedence
                compilers.Add(path + filename);
            ok = dir.GetNext(&filename);
        }
    }
    for (size_t i = 0; i < compilers.GetCount(); ++i)
    {
        wxXmlDocument compiler;
        if (!compiler.Load(compilers[i]) || compiler.GetRoot()->GetName() != "CodeBlocks_compiler")
            Manager::Get()->GetLogManager()->Log(wxString::Format(_("Error: Invalid Code::Blocks compiler definition '%s'."), compilers[i]));
        else
        {
            bool compatible_compiler = true;
            wxString compiler_platform;
            if (!nonPlatComp && compiler.GetRoot()->GetAttribute(wxT("platform"), &compiler_platform))
            {
                if (compiler_platform == wxT("windows"))
                    compatible_compiler = platform::windows;
                else if (compiler_platform == wxT("macosx"))
                    compatible_compiler = platform::macosx;
                else if (compiler_platform == wxT("linux"))
                    compatible_compiler = platform::Linux;
                else if (compiler_platform == wxT("freebsd"))
                    compatible_compiler = platform::freebsd;
                else if (compiler_platform == wxT("netbsd"))
                    compatible_compiler = platform::netbsd;
                else if (compiler_platform == wxT("openbsd"))
                    compatible_compiler = platform::openbsd;
                else if (compiler_platform == wxT("darwin"))
                    compatible_compiler = platform::darwin;
                else if (compiler_platform == wxT("solaris"))
                    compatible_compiler = platform::solaris;
                else if (compiler_platform == wxT("unix"))
                    compatible_compiler = platform::Unix;
            }
            if (compatible_compiler)
            {
                CompilerFactory::RegisterCompiler(
                  new CompilerXML(compiler.GetRoot()->GetAttribute(wxT("name"), wxEmptyString),
                                  compiler.GetRoot()->GetAttribute(wxT("id"),   wxEmptyString),
                                  compilers[i]));
            }
        }
    }

    // register (if any) user-copies of built-in compilers
    CompilerFactory::RegisterUserCompilers();
}

const wxString& CompilerGCC::GetCurrentCompilerID()
{
    static wxString def = wxEmptyString;
    return CompilerFactory::GetCompiler(m_CompilerId) ? m_CompilerId : def;
}

void CompilerGCC::SwitchCompiler(const wxString& id)
{
    if (!CompilerFactory::GetCompiler(id))
        return;

    m_CompilerId = id;

    SetupEnvironment();
}

void CompilerGCC::PrepareCompileFilePM(wxFileName& file)
{
    // we 're called from a menu in ProjectManager
    // let's check the selected project...
    FileTreeData* ftd = DoSwitchProjectTemporarily();
    ProjectFile* pf = ftd->GetProjectFile();
    if (!pf)
        return;

    file = pf->file;
    CheckProject();
}

void CompilerGCC::PrepareCompileFile(wxFileName& file)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        // make sure it is saved
        ed->Save();
        file.Assign(ed->GetFilename());

        // Now activate the project this file belongs to
        ProjectFile* pf = ed->GetProjectFile();
        if (pf)
        {
            cbProject* CurProject = pf->GetParentProject();
            if (CurProject)
            {
                Manager::Get()->GetProjectManager()->SetProject(CurProject, true);
                CheckProject();
            }
        }
    }
}

bool CompilerGCC::CheckProject()
{
    AskForActiveProject();

    // switch compiler for the project (if needed)
    if      ( m_pProject && m_pProject->GetCompilerID() != m_CompilerId)
        SwitchCompiler(m_pProject->GetCompilerID());
    // switch compiler for single file (if needed)
    else if (!m_pProject && m_CompilerId != CompilerFactory::GetDefaultCompilerID())
        SwitchCompiler(CompilerFactory::GetDefaultCompilerID());

    return (m_pProject != nullptr);
}

void CompilerGCC::AskForActiveProject()
{
    m_pProject = m_pBuildingProject
                ? m_pBuildingProject
                : Manager::Get()->GetProjectManager()->GetActiveProject();
}

void CompilerGCC::StartCompileFile(wxFileName file)
{
    if (m_pProject)
    {
        if (!m_pProject->SaveAllFiles())
            Manager::Get()->GetLogManager()->Log(_("Could not save all files..."));

        file.MakeRelativeTo(m_pProject->GetBasePath());
    }

    wxString fname = file.GetFullPath();
    if (!fname.IsEmpty())
    {
        CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, m_pLog);
        Manager::Get()->ProcessEvent(evtSwitch);

        CompileFile( UnixFilename(fname) );
    }
}

wxString CompilerGCC::ProjectMakefile()
{
    AskForActiveProject();

    if (!m_pProject)
        return wxEmptyString;

    return m_pProject->GetMakefile();
}

void CompilerGCC::ClearLog(bool switchToLog)
{
    if (m_IsWorkspaceOperation)
        return;

    if (IsProcessRunning())
        return;

    if (switchToLog)
    {
        CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, m_pLog);
        Manager::Get()->ProcessEvent(evtSwitch);
    }

    if (m_pLog)
        m_pLog->Clear();
}

FileTreeData* CompilerGCC::DoSwitchProjectTemporarily()
{
    ProjectManager* manager = Manager::Get()->GetProjectManager();
    wxTreeCtrl* tree = manager->GetUI().GetTree();
    wxTreeItemId sel = manager->GetUI().GetTreeSelection();
    FileTreeData* ftd = sel.IsOk() ? (FileTreeData*)tree->GetItemData(sel) : nullptr;
    if (!ftd)
        return nullptr;
    // We're not rebuilding the tree, so the ftd pointer is still valid after the call.
    Manager::Get()->GetProjectManager()->SetProject(ftd->GetProject(), false);
    AskForActiveProject();

    return ftd;
}

void CompilerGCC::AddToCommandQueue(const wxArrayString& commands)
{
    ProjectBuildTarget* bt = m_pBuildingProject ? m_pBuildingProject->GetBuildTarget(GetTargetIndexFromName(m_pBuildingProject, m_BuildingTargetName)) : nullptr;
    m_CurrentProgress = 0;
    m_MaxProgress = 0;
    bool isLink = false;
    bool mustWait = false;
    size_t count = commands.GetCount();
    for (size_t i = 0; i < count; ++i)
    {
        wxString cmd = commands[i];

        // logging
        if (cmd.StartsWith(COMPILER_SIMPLE_LOG))
        {
            cmd.Remove(0, COMPILER_SIMPLE_LOG.Length());
            m_CommandQueue.Add(new CompilerCommand(wxEmptyString, cmd, m_pBuildingProject, bt));
        }
        // compiler change
        else if (cmd.StartsWith(COMPILER_TARGET_CHANGE))
        {
            ; // nothing to do for now
        }
        else if (cmd.StartsWith(COMPILER_WAIT))
        {
            mustWait = true;
        }
        else if (cmd.StartsWith(COMPILER_WAIT_LINK))
        {
            isLink = true;
        }
        else
        {
            // compiler command
            CompilerCommand* p = new CompilerCommand(cmd, wxEmptyString, m_pBuildingProject, bt);
            p->mustWait = mustWait;
            p->isLink = isLink;
            m_CommandQueue.Add(p);
            isLink = false;
            mustWait = false;
            ++m_MaxProgress;
        }
    }

    if (m_pLog->progress)
    {
        m_pLog->progress->SetRange(m_MaxProgress);
        m_pLog->progress->SetValue(m_CurrentProgress);
    }
}

void CompilerGCC::AllocProcesses()
{
    // create the parallel processes array
    size_t parallel_processes = Manager::Get()->GetConfigManager(_T("compiler"))->ReadInt(_T("/parallel_processes"), 0);
    if (parallel_processes == 0)
        parallel_processes = std::max(1, wxThread::GetCPUCount());
    m_CompilerProcessList.resize(parallel_processes);
    for (CompilerProcess &p : m_CompilerProcessList)
    {
        p.pProcess = nullptr;
        p.PID = 0;
    }
}

void CompilerGCC::FreeProcesses()
{
    // free the parallel processes array
    for (CompilerProcess &p : m_CompilerProcessList)
        Delete(p.pProcess);
    m_CompilerProcessList.clear();
}

bool CompilerGCC::ReAllocProcesses()
{
    FreeProcesses();
    AllocProcesses();
    return true;
}

bool CompilerGCC::IsProcessRunning(int idx) const
{
    // invalid process index
    if (m_CompilerProcessList.empty() || idx >= (int)m_CompilerProcessList.size())
        return false;

    // specific process
    if (idx >= 0)
        return (m_CompilerProcessList.at(static_cast<size_t>(idx)).pProcess != nullptr);

    // any process (idx = -1)
    for (const CompilerProcess &p : m_CompilerProcessList)
    {
        if (p.pProcess)
            return true;
    }
    return false;
}

int CompilerGCC::GetNextAvailableProcessIndex() const
{
    for (size_t i = 0; i < m_CompilerProcessList.size(); ++i)
    {
        const CompilerProcess &p = m_CompilerProcessList[i];
        if (!p.pProcess && p.PID == 0)
            return i;
    }
    return -1;
}

int CompilerGCC::GetActiveProcessCount() const
{
    int count = 0;
    for (const CompilerProcess &p : m_CompilerProcessList)
    {
        if (p.pProcess)
            ++count;
    }
    return count;
}

int CompilerGCC::DoRunQueue()
{
    // leave if already running
    int procIndex = GetNextAvailableProcessIndex();
    if (procIndex == -1)
        return -2;

    // if next command is linking and compilation is still in progress, abort
    if (IsProcessRunning())
    {
        CompilerCommand* cmd = m_CommandQueue.Peek();
        if (cmd && (cmd->mustWait || cmd->isLink))
            return -3;
    }

    CompilerCommand* cmd = m_CommandQueue.Next();
    if (!cmd)
    {
        if (IsProcessRunning())
            return 0;

        while (1)
        {
            // keep switching build states until we have commands to run or reach end of states
            BuildStateManagement();
            cmd = m_CommandQueue.Next();
            if (!cmd && m_BuildState == bsNone && m_NextBuildState == bsNone)
            {
                NotifyJobDone(true);
                ResetBuildState();
                if (m_RunAfterCompile)
                {
                    m_RunAfterCompile = false;
                    if (Run() == 0)
                        DoRunQueue();
                }
                return 0;
            }

            if (cmd)
                break;
        }
    }

    wxString dir = cmd->dir;

    // log file
    bool hasLog = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/save_html_build_log"), false);
    bool saveFull = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/save_html_build_log/full_command_line"), false);
    if (hasLog)
    {
        if (!cmd->command.IsEmpty() && saveFull)
            LogMessage(cmd->command, cltNormal, ltFile);
        else if (!cmd->message.IsEmpty() && !saveFull)
            LogMessage(cmd->message, cltNormal, ltFile);
    }

    if (cmd->command.IsEmpty())
    {
        // log message
        if (!cmd->message.IsEmpty())
            LogMessage(cmd->message, cltNormal, ltMessages, false, false, true);

        int ret = DoRunQueue();
        delete cmd;
        return ret;
    }
    else if (cmd->command.StartsWith(_T("#run_script")))
    {
        // log message
        if (!cmd->message.IsEmpty())
            LogMessage(cmd->message, cltNormal, ltMessages, false, false, true);

        // special "run_script" command
        wxString script = cmd->command.AfterFirst(_T(' '));
        if (script.IsEmpty())
        {
            wxString msg = _("The #run_script command must be followed by a script filename");
            LogMessage(msg, cltError);
        }
        else
        {
            Manager::Get()->GetMacrosManager()->ReplaceMacros(script);
            wxString msg = _("Running script: ") + script;
            LogMessage(msg);

            Manager::Get()->GetScriptingManager()->LoadScript(script);
        }
        int ret = DoRunQueue();
        delete cmd;
        return ret;
    }

    wxString oldLibPath; // keep old PATH/LD_LIBRARY_PATH contents
    wxGetEnv(CB_LIBRARY_ENVVAR, &oldLibPath);
    wxString oldPath;    // keep old PATH environment
    wxGetEnv("PATH", &oldPath);

    bool pipe = true;
    int flags = wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER;
    if (cmd->isRun)
    {
        pipe   = false; // no need to pipe output channels...
        flags |= wxEXEC_SHOW_CONSOLE;
        dir    = m_CdRun;

        // setup dynamic linker path
        wxString newLibPath = cbGetDynamicLinkerPathForTarget(m_pProject, cmd->target);
        newLibPath = cbMergeLibPaths(oldLibPath, newLibPath);
        wxSetEnv(CB_LIBRARY_ENVVAR, newLibPath);
        LogMessage(wxString(_("Set variable: ")) + CB_LIBRARY_ENVVAR wxT("=") + newLibPath, cltInfo);

        // setup PATH environment
        wxString newPath = cbGetCompilerPathForTarget(m_pProject, cmd->target);
        newPath = cbMergeLibPaths(oldPath, newPath);
        wxSetEnv("PATH", newPath);
        LogMessage(wxString(_("Set variable: PATH=") + newPath, cltInfo);
    }

    // log message here, so the logging for run executable commands is done after the log message
    // for set variable.
    if (!cmd->message.IsEmpty())
        LogMessage(cmd->message, cltNormal, ltMessages, false, false, true);

    // special shell used only for build commands
    if (!cmd->isRun)
    {
        cbExpandBackticks(cmd->command);

        // Run the command in a shell, so stream redirections (<, >, << and >>),
        // piping and other shell features can be evaluated.
        if (!platform::windows)
        {
            const wxString shell(Manager::Get()->GetConfigManager("app")->Read("/console_shell", DEFAULT_CONSOLE_SHELL));
            cmd->command = shell + " '" + cmd->command + "'";
        }
    }

    // create a new process
    CompilerProcess &process = m_CompilerProcessList.at(procIndex);
    process.OutputFile = (cmd->isLink && cmd->target) ? cmd->target->GetOutputFilename() : wxString(wxEmptyString);
    process.pProcess = new PipedProcess(&(process.pProcess), this, idGCCProcess, pipe, dir, procIndex);

    process.PID = process.pProcess->Launch(cmd->command, flags);
    if (!process.PID)
    {
        wxString err = wxString::Format(_("Execution of '%s' in '%s' failed."),
                                        cmd->command, wxGetCwd());
        LogMessage(err, cltError);
        LogWarningOrError(cltError, nullptr, wxEmptyString, wxEmptyString, err);
        if (!m_CommandQueue.LastCommandWasRun())
        {
            if ( !IsProcessRunning() )
            {
                wxString msg = wxString::Format("%s (%s)", GetErrWarnStr(), GetMinSecStr());
                LogMessage(msg, cltError, ltAll, true);
                LogWarningOrError(cltNormal, nullptr, wxEmptyString, wxEmptyString,
                                  wxString::Format(_("=== Build failed: %s ==="), msg));
                if (!Manager::IsBatchBuild())
                    m_pListLog->AutoFitColumns(2);

                SaveBuildLog();
            }
            if (!Manager::IsBatchBuild() && m_pLog->progress)
                m_pLog->progress->SetValue(0);
        }
        Delete(process.pProcess);
        m_CommandQueue.Clear();
        ResetBuildState();
    }
    else
        m_timerIdleWakeUp.Start(100);

    // restore old dynamic linker path
    wxSetEnv(CB_LIBRARY_ENVVAR, oldLibPath);
    // restore old PATH environment
    wxSetEnv("PATH", oldPath);

    delete cmd;
    return DoRunQueue();
}

void CompilerGCC::DoClearTargetMenu()
{
    if (m_TargetMenu)
    {
        bool foundFirstSeparator = false;
        wxMenuItemList& items = m_TargetMenu->GetMenuItems();
        for (wxMenuItemList::iterator it = items.begin(); it != items.end(); )
        {
            wxMenuItem* item = *it;
            // Make sure we increment valid iterator (Delete will invalidate it).
            ++it;
            if (item)
            {
                if (item->GetKind() == wxITEM_SEPARATOR)
                {
                    if (!foundFirstSeparator)
                        foundFirstSeparator = true;
                }
                // Delete menu items only after the first separator.
                // We do this because we don't want to delete the first item, because we want to
                // make it possible for users to assign keyboard shortcuts for it.
                else if (foundFirstSeparator)
                    m_TargetMenu->Delete(item);
            }
        }
// mandrav: The following lines DO NOT clear the menu!
//        wxMenuItemList& items = m_TargetMenu->GetMenuItems();
//        bool olddelete=items.GetDeleteContents();
//        items.DeleteContents(true);
//        items.Clear();
//        items.DeleteContents(olddelete);
    }
}

bool CompilerGCC::IsValidTarget(const wxString &target) const
{
    if ( target.IsEmpty() )
        return false;
    if ( m_Targets.Index(target) == -1 )
        return false;
    const ProjectBuildTarget* tgt = Manager::Get()->GetProjectManager()->GetActiveProject()->GetBuildTarget(target);
    if ( tgt && ! tgt->SupportsCurrentPlatform() )
        return false;
    return true;
}

void CompilerGCC::DoRecreateTargetMenu()
{
    if (!IsAttached())
        return;

    if (m_pToolTarget)
        m_pToolTarget->Freeze();
    wxMenuBar* mbar = Manager::Get()->GetAppFrame()->GetMenuBar();
    if (mbar)
        mbar->Freeze();

    do
    {
        // clear menu and combo
        DoClearTargetMenu();
        if (m_pToolTarget)
            m_pToolTarget->Clear();

        // if no project, leave
        if (!CheckProject())
            break;

        // if no targets, leave
        if (!m_Targets.GetCount())
            break;

        wxString tgtStr(m_pProject->GetFirstValidBuildTargetName());

        // find out the should-be-selected target
        if (cbWorkspace* wsp = Manager::Get()->GetProjectManager()->GetWorkspace())
        {
          const wxString preferredTarget = wsp->GetPreferredTarget();
          tgtStr = preferredTarget;
          if ( !IsValidTarget(tgtStr) )
              tgtStr = m_pProject->GetActiveBuildTarget();
          if ( !IsValidTarget(tgtStr) )
              tgtStr = m_pProject->GetFirstValidBuildTargetName(); // last-chance default
          if ( preferredTarget.IsEmpty() )
              wsp->SetPreferredTarget(tgtStr);
        }


        // fill the menu and combo
        for (int x = 0; x < int(m_Targets.size()); ++x)
        {
            if (m_TargetMenu && x < maxTargetInMenus)
            {
                wxString help;
                help.Printf(_("Build target '%s' in current project"), GetTargetString(x));
                m_TargetMenu->AppendCheckItem(idMenuSelectTargetOther[x], GetTargetString(x), help);
            }
            if (m_pToolTarget)
                m_pToolTarget->Append(GetTargetString(x));
        }

        if (m_TargetMenu && int(m_Targets.size()) > maxTargetInMenus)
        {
            m_TargetMenu->Append(idMenuSelectTargetHasMore, _("More targets available..."),
                                 _("Use the select target menu item to see them!"));
            m_TargetMenu->Enable(idMenuSelectTargetHasMore, false);
        }

        // connect menu events
        Connect(idMenuSelectTargetOther[0], idMenuSelectTargetOther[maxTargetInMenus - 1],
                wxEVT_COMMAND_MENU_SELECTED,
                (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
                &CompilerGCC::OnSelectTarget);

        // housekeeping
        m_TargetIndex = m_Targets.Index(tgtStr);
        m_RealTargetIndex = m_TargetIndex - m_RealTargetsStartIndex;
        if (m_RealTargetIndex < 0)
            m_RealTargetIndex = -1;

        DoUpdateTargetMenu(m_TargetIndex);

        // update combo
        if (m_pToolTarget)
            m_pToolTarget->SetSelection(m_TargetIndex);

        // finally, make sure we 're using the correct compiler for the project
        SwitchCompiler(m_pProject->GetCompilerID());
    }
    while (false);

    if (mbar)
        mbar->Thaw();
    if (m_pToolTarget)
        m_pToolTarget->Thaw();
}

void CompilerGCC::DoUpdateTargetMenu(int targetIndex)
{
    // update indices
    m_TargetIndex = targetIndex;
    m_RealTargetIndex = m_TargetIndex - m_RealTargetsStartIndex;
    if (m_RealTargetIndex < 0)
        m_RealTargetIndex = -1;

    if (m_TargetIndex == -1)
        m_TargetIndex = 0;

    if (m_pProject)
        m_pProject->SetActiveBuildTarget(GetTargetString(m_TargetIndex));

    // update menu
    if (m_TargetMenu)
    {
        for (int i = 0; i < maxTargetInMenus; ++i)
        {
            wxMenuItem* item = m_TargetMenu->FindItem(idMenuSelectTargetOther[i]);
            if (!item || !item->IsCheckable())
                continue;
            item->Check(i == m_TargetIndex);
        }
    }

    // the tool combo is updated in DoRecreateTargetMenu()
    // can't set it here, because this function is called by the
    // tool combo's event handler
//    DBGLOG(_T("m_TargetIndex=%d, m_pToolTarget->GetCurrentSelection()=%d, m_RealTargetsStartIndex=%d"), m_TargetIndex, m_pToolTarget->GetCurrentSelection(), m_RealTargetsStartIndex);
}

void CompilerGCC::UpdateProjectTargets(cbProject* project)
{
    m_Targets.Clear();
    if (!project)
        return;

    // update the list of targets (virtual + real)
    wxArrayString virtuals = project->GetVirtualBuildTargets();
    for (size_t i = 0; i < virtuals.GetCount(); ++i)
        m_Targets.Add(virtuals[i]);

    for (int i = 0; i < project->GetBuildTargetsCount(); ++i)
    {
        ProjectBuildTarget* tgt = project->GetBuildTarget(i);
        if ( tgt->SupportsCurrentPlatform() )
            m_Targets.Add( tgt->GetTitle() );
    }

    // keep the index for the first real target
    m_RealTargetsStartIndex = virtuals.GetCount();

    // actually rebuild menu and combo
    DoRecreateTargetMenu();
    if (!Manager::IsBatchBuild())
        m_pTbar->Fit();
}

wxString CompilerGCC::GetTargetString(int index)
{
    if (index == -1)
        index = m_TargetIndex;
    if (index >= 0 && index < (int)m_Targets.GetCount())
        return m_Targets[index];
    return wxEmptyString;
}

void CompilerGCC::DoPrepareQueue(bool clearLog)
{
    if (m_CommandQueue.GetCount() == 0)
    {
        CodeBlocksEvent evt(cbEVT_COMPILER_STARTED, 0, m_pProject, nullptr, this);
        Manager::Get()->ProcessEvent(evt);
        //Make sure we force sending the compiler finish event, else plugins will hang
        m_StartedEventSent = true;

        if (clearLog)
        {
            ClearLog(true);
            DoClearErrors();
        }
        // wxStartTimer();
        m_StartTime = wxGetLocalTimeMillis();
    }
    Manager::Yield();
}

void CompilerGCC::NotifyCleanProject(const wxString& target)
{
    if (m_CommandQueue.GetCount() == 0)
    {
        CodeBlocksEvent evt(cbEVT_CLEAN_PROJECT_STARTED, 0, m_pProject, nullptr, this);
        evt.SetBuildTargetName(target);
        Manager::Get()->ProcessEvent(evt);
    }
    Manager::Yield();
}

void CompilerGCC::NotifyCleanWorkspace()
{
    if (m_CommandQueue.GetCount() == 0)
    {
        CodeBlocksEvent evt(cbEVT_CLEAN_WORKSPACE_STARTED, 0, nullptr, nullptr, this);
        Manager::Get()->ProcessEvent(evt);
    }
    Manager::Yield();
}

ProjectBuildTarget* CompilerGCC::DoAskForTarget()
{
    if (!CheckProject())
        return nullptr;

    return m_pProject->GetBuildTarget(m_RealTargetIndex);
}

int CompilerGCC::DoGUIAskForTarget()
{
    if (!CheckProject())
        return -1;

    return m_pProject->SelectTarget(m_RealTargetIndex);
}

bool CompilerGCC::UseMake(cbProject* project)
{
    if (!project)
        project = m_pProject;
    if (!project)
        return false;
    wxString idx = project->GetCompilerID();
    if (CompilerFactory::GetCompiler(idx))
        return project->IsMakefileCustom();

    return false;
}

wxString CompilerGCC::GetCurrentCompilerID(ProjectBuildTarget* target)
{
    if (target)
        return target->GetCompilerID();
    if (m_pBuildingProject)
        return m_pBuildingProject->GetCompilerID();
    if (m_pProject)
        return m_pProject->GetCompilerID();
    return wxEmptyString;
}

auto CompilerGCC::CompilerValid(ProjectBuildTarget* target) -> CompilerValidResult
{
    CompilerValidResult result;
    if (!target)
        result.compiler = CompilerFactory::GetDefaultCompiler();
    else
    {
        wxString idx = GetCurrentCompilerID(target);
        result.compiler = CompilerFactory::GetCompiler(idx);
    }
    if (result.compiler)
        result.isValid = result.compiler->IsValid();
    return result;
}

void CompilerGCC::PrintInvalidCompiler(ProjectBuildTarget *target, Compiler* compiler, const wxString &finalMessage)
{
    wxString compilerName, compilerName2(_("unknown"));
    if (compiler)
    {
        compilerName = wxT("(") + compiler->GetName() + wxT(") ");
        compilerName2 = compiler->GetName();
    }

    wxString title;
    if (target)
        title = target->GetFullTitle();
    else
        title = _("unknown");

    wxString msg;
    msg.Printf(_("Project/Target: \"%s\":\n") +
               _("  The compiler's setup %s is invalid, so Code::Blocks cannot find/run the compiler.\n") +
               _("  Probably the toolchain path within the compiler options is not setup correctly?!\n") +
               _("  Do you have a compiler installed?\n") +
               _("Goto \"Settings->Compiler...->Global compiler settings->%s->Toolchain executables\" and fix the compiler's setup.\n"),
               title, compilerName, compilerName2);

    LogManager* logger = Manager::Get()->GetLogManager();
    logger->LogError(msg, m_PageIndex);
    if (compiler)
        logger->LogError(compiler->MakeInvalidCompilerMessages(), m_PageIndex);
    logger->LogError(finalMessage, m_PageIndex);
}

void CompilerGCC::PrintBanner(BuildAction action, cbProject* prj, ProjectBuildTarget* target)
{
    if (!CompilerValid(target).isValid)
        return;

    CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
    Manager::Get()->ProcessEvent(evtShow);

    if (!prj)
        prj = m_pProject;

    wxString Action;
    switch (action)
    {
    case baClean:
        Action = _("Clean");
        break;
    case baRun:
        Action = _("Run");
        break;
    case baBuildFile:
        Action = _("Build file");
        break;
    default:
    case baBuild:
        Action = _("Build");
        break;
    }

    wxString compilerName(_("unknown"));
    Compiler* compiler = CompilerFactory::GetCompiler(GetCurrentCompilerID(target));
    if (compiler)
        compilerName = compiler->GetName();

    wxString targetName = target ? target->GetTitle() : wxString(_("\"no target\""));
    wxString projectName = prj ? prj->GetTitle() : wxString(_("\"no project\""));

    wxString banner;
    banner.Printf(_("%s: %s in %s (compiler: %s)"), Action, targetName, projectName, compilerName);
    Manager::Get()->GetMacrosManager()->ReplaceMacros(banner);
    LogWarningOrError(cltNormal, nullptr, wxString(), wxString(), "=== " + banner + " ===");
    LogMessage("-------------- " + banner + "---------------", cltNormal, ltAll, false, true);
    if (!Manager::IsBatchBuild())
        m_pListLog->AutoFitColumns(2);
}

void CompilerGCC::DoGotoNextError()
{
    CodeBlocksLogEvent eventSwitchLog(cbEVT_SWITCH_TO_LOG_WINDOW, m_pListLog);
    Manager::Get()->ProcessEvent(eventSwitchLog);

    m_Errors.Next();
    m_pListLog->FocusError(m_Errors.GetFocusedError());
}

void CompilerGCC::DoGotoPreviousError()
{
    CodeBlocksLogEvent eventSwitchLog(cbEVT_SWITCH_TO_LOG_WINDOW, m_pListLog);
    Manager::Get()->ProcessEvent(eventSwitchLog);

    m_Errors.Previous();
    m_pListLog->FocusError(m_Errors.GetFocusedError());
}

void CompilerGCC::DoClearErrors()
{
    m_Errors.Clear();
    m_pListLog->Clear();
    m_NotifiedMaxErrors = false;
}

int CompilerGCC::RunSingleFile(const wxString& filename)
{
    wxFileName fname(filename);

    if (fname.GetExt() == _T("script"))
    {
        Manager::Get()->GetScriptingManager()->LoadScript(filename);
        return 0;
    }

    m_CdRun = fname.GetPath();
    fname.SetExt(FileFilters::EXECUTABLE_EXT);
    wxString exe_filename = fname.GetFullPath();
    wxString command;

    if (!platform::windows)
    {
        // for non-win platforms, use m_ConsoleTerm to run the console app
        wxString term = Manager::Get()->GetConfigManager(_T("app"))->Read(_T("/console_terminal"), DEFAULT_CONSOLE_TERM);
        term.Replace(_T("$TITLE"), _T("'") + exe_filename + _T("'"));
        command << term << strSPACE;
    }

    wxString baseDir = ConfigManager::GetExecutableFolder();
    wxString crunnStr = strQUOTE + baseDir + strSLASH + strCONSOLE_RUNNER + strQUOTE;
    if ( wxFileExists(baseDir + strSLASH + strCONSOLE_RUNNER) )
        command << crunnStr << strSPACE;

    if (!command.Replace(_T("$SCRIPT"), exe_filename))
        command << strQUOTE << exe_filename << strQUOTE; // if they didn't specify $SCRIPT, append:

    Manager::Get()->GetLogManager()->Log(_("Checking for existence: ") + exe_filename, m_PageIndex);
    if ( !wxFileExists(exe_filename) )
    {
        int ret = cbMessageBox(_("It seems that this file has not been built yet.\n"
                                 "Do you want to build it now?"),
                                _("Information"),
                                wxYES_NO | wxCANCEL | wxICON_QUESTION);
        switch (ret)
        {
            case wxID_YES:
            {
                m_RunAfterCompile = true;
                Build(wxEmptyString);
                return -1;
            }
            case wxID_NO:
                break;
            default:
                return -1;
        }
    }

    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(m_CdRun);
    Manager::Get()->GetLogManager()->Log(wxString::Format(_("Executing: '%s' (in '%s')"), command, m_CdRun), m_PageIndex);
    m_CommandQueue.Add(new CompilerCommand(command, wxEmptyString, nullptr, nullptr, true));
    return 0;
}

bool CompilerGCC::ExecutableExists(cbProject* prj)
{
    // A project is not mandatory to execute the file in the editor, but
    // then at least one editor (not the "Start here" one) must be active
    if (!prj)
    {
        EditorManager* edmgr = Manager::Get()->GetEditorManager();
        EditorBase* currentEditor = edmgr->GetActiveEditor();
        if (!currentEditor)
            return false;

        return (currentEditor != edmgr->GetEditor(_("Start here")));
    }

    // Get target name
    const wxString activeTarget(prj->GetActiveBuildTarget());

    // Is a virtual target?
    if (prj->HasVirtualBuildTarget(activeTarget))
        return true;

    ProjectBuildTarget* pTarget = prj->GetBuildTarget(activeTarget);
    if (!pTarget)
        return false;

    if (pTarget->GetTargetType() == ttCommandsOnly)
        return true;

    // Check if the output filename exists
    wxString out = UnixFilename(pTarget->GetOutputFilename());
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(out);
    wxFileName file(out);
    file.MakeAbsolute(prj->GetBasePath());
    return file.FileExists();
}

int CompilerGCC::Run(const wxString& target)
{
    if (!CheckProject())
        return -1;
    return Run(m_pProject->GetBuildTarget(target.IsEmpty() ? m_LastTargetName : target));
}

int CompilerGCC::Run(ProjectBuildTarget* target)
{
    bool commandIsQuoted = false; // remember if we quoted the command, avoid unneeded quotes, because they break execution with "konsole" under KDE
    if (!CheckProject())
    {
        if (Manager::Get()->GetEditorManager()->GetActiveEditor())
            return RunSingleFile(Manager::Get()->GetEditorManager()->GetActiveEditor()->GetFilename());
        return -1;
    }
    else
    {
        target = m_pProject->GetBuildTarget(m_pProject->GetActiveBuildTarget());
    }
    PrintBanner(baRun, m_pProject, target);

    DoPrepareQueue(false);
    if (   !(target && (   target->GetTargetType() == ttCommandsOnly // do not require compiler for commands-only target
                        || target->GetCompilerID() == wxT("null") ))) // do not require compiler for "No Compiler" (why would you?)
    {
        CompilerValidResult result = CompilerValid(target);
        if (!result.isValid)
        {
            PrintInvalidCompiler(target, result.compiler, _("Run aborted..."));
            return -1;
        }
    }
//    DBGLOG(_T("1) target=%s, m_RealTargetIndex=%d, m_TargetIndex=%d"), target ? target->GetTitle().c_str() : _T("null"), m_RealTargetIndex, m_TargetIndex);

    if (!target)
    {
        if (m_RealTargetIndex == -1) // only ask for target if a virtual target is selected
        {
            int idx = -1;
            int bak = m_RealTargetIndex;
            if (m_pProject->GetBuildTargetsCount() == 1)
                idx = 0;
            else
                idx = DoGUIAskForTarget();

            m_RealTargetIndex = idx;
            target = DoAskForTarget();
            m_RealTargetIndex = bak;
        }
        else
            target = DoAskForTarget();
    }
//    DBGLOG(_T("2) target=%s, m_RealTargetIndex=%d, m_TargetIndex=%d"), target ? target->GetTitle().c_str() : _T("null"), m_RealTargetIndex, m_TargetIndex);

    if (!target)
        return -1;

    m_pProject->SetCurrentlyCompilingTarget(target); // help macros manager

    wxString out = UnixFilename(target->GetOutputFilename());
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(out);

    wxString cmd;
    wxString command;
    wxFileName f(out);
    f.MakeAbsolute(m_pProject->GetBasePath());

    m_CdRun = target->GetWorkingDir();
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(m_CdRun);
    wxFileName cd(m_CdRun);
    if (cd.IsRelative())
        cd.MakeAbsolute(m_pProject->GetBasePath());
    m_CdRun = cd.GetFullPath();
    wxString baseDir = ConfigManager::GetExecutableFolder();

    wxString titleStr = platform::windows
                      ? strQUOTE + m_pProject->GetTitle() + strQUOTE
                      : EscapeSpaces(m_pProject->GetTitle());
    wxString dirStr = platform::windows
                    ? strQUOTE + m_CdRun + strQUOTE
                    : EscapeSpaces(m_CdRun);
    wxString crunnStr = platform::windows
                      ? strQUOTE + baseDir + strSLASH + strCONSOLE_RUNNER + strQUOTE
                      : EscapeSpaces(baseDir + strSLASH + strCONSOLE_RUNNER);
    wxString hostapStr = platform::windows
                       ? strQUOTE + target->GetHostApplication() + strQUOTE
                       : EscapeSpaces(target->GetHostApplication());
    wxString execStr = platform::windows
                     ? strQUOTE + f.GetFullPath() + strQUOTE
                     : EscapeSpaces(f.GetFullPath());

    // for console projects, use helper app to wait for a key after
    // execution ends...
    if (target->GetTargetType() == ttConsoleOnly || target->GetRunHostApplicationInTerminal())
    {
        if (!platform::windows)
        {
            // for non-win platforms, use m_ConsoleTerm to run the console app
            wxString term = Manager::Get()->GetConfigManager(_T("app"))->Read(_T("/console_terminal"), DEFAULT_CONSOLE_TERM);
            term.Replace(_T("$TITLE"), titleStr);
            term.Replace(_T("$WORKDIR"), dirStr);
            cmd << term << strSPACE;

            wxString shell;
            wxGetEnv(_T("SHELL"), &shell);
            if (shell.Contains(_T("csh")))
            {
                // "The csh is a tool utterly inadequate for programming,
                //  and its use for such purposes should be strictly banned!"
                //                 -- Csh Programming Considered Harmful
                command << DEFAULT_CONSOLE_SHELL << strSPACE;
                // each shell execution must be enclosed to "":
                // xterm -T X -e /bin/sh -c "/usr/bin/cb_console_runner X"
                // here is first \"
                command << strQUOTE;
                commandIsQuoted = true;
            }
        }

        if (target->GetUseConsoleRunner())
        {
            if (wxFileExists(baseDir + strSLASH + strCONSOLE_RUNNER))
            {
                command << crunnStr << strSPACE;

                if (!platform::windows)
                {
                    // set LD_LIBRARY_PATH
                    command << CB_LIBRARY_ENVVAR << _T("=$") << CB_LIBRARY_ENVVAR << _T(':');
                    // we have to quote the string, just escape the spaces does not work
                    wxString strLinkerPath=cbGetDynamicLinkerPathForTarget(m_pProject, target);
                    QuoteStringIfNeeded(strLinkerPath);
                    command << strLinkerPath << strSPACE;
                }
            }
        }
    }

    if (   target->GetTargetType() == ttDynamicLib
        || target->GetTargetType() == ttStaticLib )
    {
        // check for hostapp
        if (target->GetHostApplication().IsEmpty())
        {
            cbMessageBox(_("You must select a host application to \"run\" a library..."));
            m_pProject->SetCurrentlyCompilingTarget(nullptr);
            return -1;
        }

        command << hostapStr << strSPACE;
        command << target->GetExecutionParameters();
    }
    else if (target->GetTargetType() != ttCommandsOnly)
    {
        command << execStr << strSPACE;
        command << target->GetExecutionParameters();
        // each shell execution must be enclosed to "":
        // xterm -T X -e /bin/sh -c "/usr/bin/cb_console_runner X"
        // here is last \"
        if (commandIsQuoted)
            command << strQUOTE;
    }
    else
    {
        // commands-only target?
        if (target->GetHostApplication().IsEmpty())
        {
            cbMessageBox(_("You must select a host application to \"run\" a commands-only target..."));
            m_pProject->SetCurrentlyCompilingTarget(nullptr);
            return -1;
        }
        command << hostapStr << strSPACE;
        command << target->GetExecutionParameters();
    }
    Manager::Get()->GetMacrosManager()->ReplaceMacros(command, target);
    wxString script = command;

    if (platform::macosx)
    {
        if (target->GetTargetType() == ttConsoleOnly &&
            script.GetChar(0) == '\'' && script.GetChar(script.length()-1) == '\'')
        script = script.Mid(1,script.length()-2); // skip outmost single-quotes

        // convert embedded quotes to AppleScript syntax
        script.Replace(_T("\""), _T("\"&quote&\""), true);
        script.Replace(_T("\'"), _T("\"&ASCII character 39&\""), true);
    }

    if (!cmd.Replace(_T("$SCRIPT"), script))
        // if they didn't specify $SCRIPT, append:
        cmd << command;

    Manager::Get()->GetLogManager()->Log(_("Checking for existence: ") + f.GetFullPath(), m_PageIndex);
    if ( (target->GetTargetType() != ttCommandsOnly) && !wxFileExists(f.GetFullPath()) )
    {
        int ret = cbMessageBox(_("It seems that this project has not been built yet.\n"
                                 "Do you want to build it now?"),
                               _("Information"),
                               wxYES_NO | wxCANCEL | wxICON_QUESTION);
        switch (ret)
        {
            case wxID_YES:
            {
                m_pProject->SetCurrentlyCompilingTarget(nullptr);
                m_RunAfterCompile = true;
                Build(target);
                return -1;
            }
            case wxID_NO:
                break;
            default:
                m_pProject->SetCurrentlyCompilingTarget(nullptr);
                return -1;
        }
    }

    const wxString& message = wxString::Format(_("Executing: %s (in %s)"), cmd, m_CdRun);
    m_CommandQueue.Add(new CompilerCommand(cmd, message, m_pProject, target, true));

    m_pProject->SetCurrentlyCompilingTarget(nullptr);

    Manager::Get()->GetProjectManager()->SetIsRunning(this);
    return 0;
}

wxString CompilerGCC::GetMakeCommandFor(MakeCommand cmd, cbProject* project, ProjectBuildTarget* target)
{
    if (!project)
        return wxEmptyString;

    wxString compilerId = target ? target->GetCompilerID() : project->GetCompilerID();
    if (!CompilerFactory::IsValidCompilerID(compilerId))
        compilerId = CompilerFactory::GetDefaultCompilerID();
    wxString command = target && !target->GetMakeCommandFor(cmd).empty() ?
                       target->GetMakeCommandFor(cmd) : project->GetMakeCommandFor(cmd);

    Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
    command.Replace(_T("$makefile"), project->GetMakefile());
    command.Replace(_T("$make"), compiler ? compiler->GetPrograms().MAKE : _T("make"));
    command.Replace(_T("$target"), target ? target->GetTitle() : _T(""));
    Manager::Get()->GetMacrosManager()->ReplaceMacros(command);

//    Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("Make: %s"), command.c_str()));
    return command;
}

void CompilerGCC::DoClean(const wxArrayString& commands)
{
    for (unsigned int i = 0; i < commands.GetCount(); ++i)
        if (wxFileExists(commands[i]))
            wxRemoveFile(commands[i]);
}

int CompilerGCC::Clean(ProjectBuildTarget* target)
{
    return Clean(target ? target->GetTitle() : _T(""));
}

int CompilerGCC::Clean(const wxString& target)
{
    m_LastBuildStep = true;
    return DoBuild(target, true, false);
}

static inline wxString getBuildTargetName(const ProjectBuildTarget* bt)
{
    return bt ? bt->GetTitle() : wxString(_("<all targets>"));
}

bool CompilerGCC::DoCleanWithMake(ProjectBuildTarget* bt)
{
    wxString cmd = GetMakeCommandFor(mcClean, m_pBuildingProject, bt);
    if (cmd.empty())
    {
        LogMessage(COMPILER_ERROR_LOG +
                   _("Make command for 'Clean project/target' is empty. Nothing will be cleaned!"),
                   cltError);
        return false;
    }

    Compiler* tgtCompiler = CompilerFactory::GetCompiler(bt->GetCompilerID());
    if (!tgtCompiler)
    {
        const wxString message = wxString::Format(_("Invalid compiler selected for target '%s'!"), getBuildTargetName(bt));

        LogMessage(COMPILER_ERROR_LOG + message, cltError);
        return false;
    }

    bool showOutput = (tgtCompiler->GetSwitches().logging == clogFull);

    wxArrayString output, errors;
    wxSetWorkingDirectory(m_pBuildingProject->GetExecutionDir());

    cbExpandBackticks(cmd);

    // Run the clean command in the same shell used for building
    if (!platform::windows)
    {
        const wxString shell(Manager::Get()->GetConfigManager("app")->Read("/console_shell", DEFAULT_CONSOLE_SHELL));
        cmd = shell + " '" + cmd + "'";
    }

    if (showOutput)
        LogMessage(wxString::Format(_("Executing clean command: %s"), cmd), cltNormal);

    long result = wxExecute(cmd, output, errors, wxEXEC_SYNC);
    if (showOutput)
    {
        for(size_t i = 0; i < output.GetCount(); i++)
            LogMessage(output[i], cltNormal);
        for(size_t i = 0; i < errors.GetCount(); i++)
            LogMessage(errors[i], cltNormal);
    }

    return (result == 0);
}

int CompilerGCC::DistClean(const wxString& target)
{
    if (!CheckProject())
        return -1;
    return DistClean(m_pProject->GetBuildTarget(target.IsEmpty() ? m_LastTargetName : target));
}

int CompilerGCC::DistClean(ProjectBuildTarget* target)
{
    // make sure all project files are saved
    if (m_pProject && !m_pProject->SaveAllFiles())
        Manager::Get()->GetLogManager()->Log(_("Could not save all files..."));

    if (!m_IsWorkspaceOperation)
        DoPrepareQueue(true);
    if (!CompilerValid(target).isValid)
        return -1;

//    Manager::Get()->GetMacrosManager()->Reset();

    if (m_pProject)
        wxSetWorkingDirectory(m_pProject->GetBasePath());

    if ( UseMake() )
    {
        wxString cmd = GetMakeCommandFor(mcDistClean, m_pProject, target);
        m_CommandQueue.Add(new CompilerCommand(cmd, wxEmptyString, m_pProject, target));
        return DoRunQueue();
    }
    else
    {
        NotImplemented(_("CompilerGCC::DistClean() without a custom Makefile"));
        return -1;
    }
    return 0;
}

void CompilerGCC::InitBuildState(BuildJob job, const wxString& target)
{
    m_BuildJob             = job;
    m_BuildState           = bsNone;
    m_NextBuildState       = bsProjectPreBuild;
    m_pBuildingProject     = nullptr;
    m_pLastBuildingProject = nullptr;
    m_pLastBuildingTarget  = nullptr;
    m_BuildingTargetName   = target;
    m_CommandQueue.Clear();
}

void CompilerGCC::ResetBuildState()
{
    if (m_pBuildingProject)
        m_pBuildingProject->SetCurrentlyCompilingTarget(nullptr);
    else if (m_pProject)
        m_pProject->SetCurrentlyCompilingTarget(nullptr);

    // reset state
    m_BuildJob = bjIdle;
    m_BuildState = bsNone;
    m_NextBuildState = bsNone;
    m_pBuildingProject = nullptr;
    m_BuildingTargetName.Clear();

    m_pLastBuildingProject = nullptr;
    m_pLastBuildingTarget = nullptr;

    m_CommandQueue.Clear();

    // Clear the Active Project's currently compiling target
    // NOTE (rickg22#1#): This way we can prevent Codeblocks from shutting down
    // when a project is being compiled.
    // NOTE (mandrav#1#): Make sure no open project is marked as compiling
    ProjectsArray* arr = Manager::Get()->GetProjectManager()->GetProjects();
    for (size_t i = 0; i < arr->GetCount(); ++i)
    {
        arr->Item(i)->SetCurrentlyCompilingTarget(nullptr);
    }
}

inline wxString StateToString(BuildState bs)
{
    switch (bs)
    {
        case bsNone:             return _T("bsNone");
        case bsProjectPreBuild:  return _T("bsProjectPreBuild");
        case bsTargetPreBuild:   return _T("bsTargetPreBuild");
        case bsTargetClean:      return _T("bsTargetClean");
        case bsTargetBuild:      return _T("bsTargetBuild");
        case bsTargetPostBuild:  return _T("bsTargetPostBuild");
        case bsTargetDone:       return _T("bsTargetDone");
        case bsProjectPostBuild: return _T("bsProjectPostBuild");
        case bsProjectDone:      return _T("bsProjectDone");
        default:                 break;
    }
    return _T("Huh!?!");
}

BuildState CompilerGCC::GetNextStateBasedOnJob()
{
    bool clean = m_Clean;
    bool build = m_Build;

    switch (m_BuildState)
    {
        case bsProjectPreBuild:
        {
            if (clean && !build)
                return bsTargetClean;

            return bsTargetPreBuild;
        }

        case bsTargetPreBuild:
        {
            if      (clean)
                return bsTargetClean;
            else if (build)
                return bsTargetBuild;

            return bsTargetPostBuild;
        }

        case bsTargetClean:
        {
            if (build)
                return bsTargetBuild;

            return bsTargetDone;
        }

        case bsTargetBuild:
            return bsTargetPostBuild;

        case bsTargetPostBuild:
            return bsTargetDone;

        // advance target in the project
        case bsTargetDone:
        {
            // get next build job
            if (m_BuildJob != bjTarget)
            {
                const BuildJobTarget& bj = PeekNextJob();
                if (bj.project && bj.project == m_pBuildingProject)
                {
                    // same project, switch target
                    m_BuildingTargetName = bj.targetName;
                    GetNextJob(); // remove job from queue, bj points to a destructed object
                    // switching targets
                    if (clean && !build)
                        return bsTargetClean;

                    return bsTargetPreBuild;
                }
                // switch project
                // don't run postbuild step, if we only clean the project
                if (build)
                    return bsProjectPostBuild;

                return bsProjectDone;
            }
            m_pBuildingProject->SetCurrentlyCompilingTarget(nullptr);
            break; // all done
        }

        case bsProjectPostBuild:
            return bsProjectDone;

        case bsProjectDone:
        {
            // switch to next project in workspace
            if (m_pBuildingProject)
                m_pBuildingProject->SetCurrentlyCompilingTarget(nullptr);
            m_NextBuildState = bsProjectPreBuild;
            // DoBuild runs ProjectPreBuild, next step has to be TargetClean or TargetPreBuild
            if (DoBuild(clean, build) >= 0)
            {
                if (clean && !build)
                    return bsTargetClean;

                return bsTargetPreBuild;
            }
            else
                return bsNone;
        }

        case bsNone: // fall-through
        default:
            break;
    }
    return bsNone;
}

void CompilerGCC::BuildStateManagement()
{
//    Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("BuildStateManagement")));
    if (IsProcessRunning())
        return;

    Manager::Yield();
    if (!m_pBuildingProject)
    {
        ResetBuildState();
        return;
    }

    ProjectBuildTarget* bt = m_pBuildingProject->GetBuildTarget(GetTargetIndexFromName(m_pBuildingProject, m_BuildingTargetName));
    if (!bt)
    {
        ResetBuildState();
        return;
    }

    if (m_pBuildingProject != m_pLastBuildingProject || bt != m_pLastBuildingTarget)
    {
        Manager::Get()->GetMacrosManager()->RecalcVars(m_pBuildingProject, Manager::Get()->GetEditorManager()->GetActiveEditor(), bt);
        if (bt)
            SwitchCompiler(bt->GetCompilerID());

        if (m_pBuildingProject != m_pLastBuildingProject)
        {
            m_pLastBuildingProject = m_pBuildingProject;
            wxSetWorkingDirectory(m_pBuildingProject->GetBasePath());
        }
        if (bt != m_pLastBuildingTarget)
            m_pLastBuildingTarget = bt;
    }

    m_pBuildingProject->SetCurrentlyCompilingTarget(bt);
    DirectCommands dc(this, CompilerFactory::GetCompiler(bt->GetCompilerID()), m_pBuildingProject, m_PageIndex);
    dc.m_doYield = true;

    m_BuildState = m_NextBuildState;
    wxArrayString cmds;
    switch (m_NextBuildState)
    {
        case bsProjectPreBuild:
        {
            // don't run project pre-build steps if we only clean it
            if (m_Build)
                cmds = dc.GetPreBuildCommands(nullptr);
            break;
        }

        case bsTargetPreBuild:
        {
            // check if it should build with "All"
            // run target pre-build steps
            cmds = dc.GetPreBuildCommands(bt);
            // Print Build banner here, else preBuild commands appear to belong to previous target
            PrintBanner(baBuild, m_pBuildingProject, bt);
            break;
        }

        case bsTargetClean:
        {
            PrintBanner(baClean, m_pBuildingProject, bt);

            bool result;
            if ( UseMake(m_pBuildingProject) )
                result = DoCleanWithMake(bt);
            else
            {
                wxArrayString clean = dc.GetCleanCommands(bt, true);
                DoClean(clean);
                result = true;
            }

            if (result)
            {
                wxString message;
                message.Printf(_("Cleaned \"%s - %s\""), m_pBuildingProject->GetTitle(), getBuildTargetName(bt));
                Manager::Get()->GetMacrosManager()->ReplaceMacros(message);
                LogMessage(message, cltNormal);
            }
            else
            {
                wxString message;
                message.Printf(_("Error cleaning \"%s - %s\""), m_pBuildingProject->GetTitle(), getBuildTargetName(bt));
                Manager::Get()->GetMacrosManager()->ReplaceMacros(message);
                LogMessage(COMPILER_ERROR_LOG + message, cltError);
            }
            break;
        }

        case bsTargetBuild:
        {
            // Build banner has already been printed at bsTargetPreBuild
            // run target build
            if ( UseMake(m_pBuildingProject) )
            {
                wxArrayString output, error;
                wxSetWorkingDirectory(m_pBuildingProject->GetExecutionDir());

                const wxString &askCmd = GetMakeCommandFor(mcAskRebuildNeeded, m_pBuildingProject, bt);

                Compiler* tgtCompiler = CompilerFactory::GetCompiler(bt->GetCompilerID());

                bool runMake = false;
                if (!askCmd.empty())
                {
                    if (tgtCompiler && tgtCompiler->GetSwitches().logging == clogFull)
                        cmds.Add(COMPILER_SIMPLE_LOG + _("Checking if target is up-to-date: ") + askCmd);

                    runMake = (wxExecute(askCmd, output, error, wxEXEC_SYNC | wxEXEC_NODISABLE) != 0);
                }
                else
                {
                    cmds.Add(COMPILER_SIMPLE_LOG +
                             _("The command that asks if a rebuild is needed is empty. Assuming rebuild is needed!"));
                    runMake = true;
                }

                if (runMake && tgtCompiler)
                {
                    bool isEmpty = false;
                    switch (tgtCompiler->GetSwitches().logging)
                    {
                        case clogFull:
                        {
                            const wxString &cmd = GetMakeCommandFor(mcBuild, m_pBuildingProject, bt);
                            if (!cmd.empty())
                            {
                                cmds.Add(COMPILER_SIMPLE_LOG + _("Running command: ") + cmd);
                                cmds.Add(cmd);
                            }
                            else
                                isEmpty = true;
                            break;
                        }

                        case clogSimple:
                            cmds.Add(COMPILER_SIMPLE_LOG + _("Using makefile: ") + m_pBuildingProject->GetMakefile());
                        case clogNone:
                        {
                            const wxString &cmd = GetMakeCommandFor(mcSilentBuild, m_pBuildingProject, bt);
                            if (!cmd.empty())
                                cmds.Add(cmd);
                            else
                                isEmpty = true;
                            break;
                        }

                        default:
                            break;
                    }

                    if (isEmpty)
                    {
                        cmds.Add(COMPILER_ERROR_LOG +
                                 _("Make command for 'Build project/target' is empty. Nothing will be built!"));
                    }

                }
            }
            else
                cmds = dc.GetCompileCommands(bt);

            bool hasCommands = cmds.GetCount();
            m_RunTargetPostBuild = hasCommands;
            m_RunProjectPostBuild = hasCommands;
            if (!hasCommands)
                LogMessage(_("Target is up to date."));
            break;
        }

        case bsTargetPostBuild:
        {
            // run target post-build steps
            if (m_RunTargetPostBuild || bt->GetAlwaysRunPostBuildSteps())
                cmds = dc.GetPostBuildCommands(bt);
            // reset
            m_RunTargetPostBuild = false;
            break;
        }

        case bsProjectPostBuild:
        {
            // run project post-build steps
            if (m_RunProjectPostBuild || m_pBuildingProject->GetAlwaysRunPostBuildSteps())
                cmds = dc.GetPostBuildCommands(nullptr);
            // reset
            m_pLastBuildingTarget = nullptr;
            m_RunProjectPostBuild = false;
            break;
        }

        case bsProjectDone:
        {
            m_pLastBuildingProject = nullptr;
            break;
        }

        case bsTargetDone: // fall-through
        case bsNone:       // fall-through
        default:
            break;
    }
    m_NextBuildState = GetNextStateBasedOnJob();
    AddToCommandQueue(cmds);
    Manager::Yield();
}

int CompilerGCC::GetTargetIndexFromName(cbProject* prj, const wxString& name)
{
    if (!prj || name.IsEmpty())
        return -1;
    for (int i = 0; i < prj->GetBuildTargetsCount(); ++i)
    {
        ProjectBuildTarget* bt_search =  prj->GetBuildTarget(i);
        if (bt_search->GetTitle() == name)
            return i;
    }
    return -1;
}

void CompilerGCC::ExpandTargets(cbProject* project, const wxString& targetName, wxArrayString& result)
{
    result.Clear();
    if (project)
    {
        ProjectBuildTarget* bt =  project->GetBuildTarget(targetName);
        if (bt) // real target
            result.Add(targetName);
        else // virtual target
            result = project->GetExpandedVirtualBuildTargetGroup(targetName);
    }
}

void CompilerGCC::PreprocessJob(cbProject* project, const wxString& targetName)
{
    wxArrayString tlist;

    // if not a workspace operation, clear any remaining (old) build jobs
    if (!m_IsWorkspaceOperation)
    {
        while (!m_BuildJobTargetsList.empty())
            m_BuildJobTargetsList.pop();
    }

    // calculate project/workspace dependencies
    wxArrayInt deps;
    if (!project)
        CalculateWorkspaceDependencies(deps);
    else
        CalculateProjectDependencies(project, deps);

    // loop all projects in the dependencies list
//    Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("** Creating deps")));
    for (size_t i = 0; i < deps.GetCount(); ++i)
    {
        cbProject* prj = Manager::Get()->GetProjectManager()->GetProjects()->Item(deps[i]);

        if (!prj->SupportsCurrentPlatform())
        {
            wxString msg;
            msg.Printf("\"%s\" does not support the current platform. Skipping...",
                       prj->GetTitle());

            Manager::Get()->GetLogManager()->LogWarning(msg, m_PageIndex);
            continue;
        }

        ExpandTargets(prj, targetName, tlist);

        if (tlist.GetCount() == 0)
        {
            wxString msg;
            msg.Printf("Warning: No target named '%s' in project '%s'. Project will not be built...",
                       targetName, prj->GetTitle());

            Manager::Get()->GetLogManager()->LogWarning(msg);
        }

        // add all matching targets in the job list
        for (size_t x = 0; x < tlist.GetCount(); ++x)
        {
            ProjectBuildTarget* tgt = prj->GetBuildTarget(tlist[x]);
            if (!tgt->SupportsCurrentPlatform())
            {
                wxString msg;
                msg.Printf("\"%s - %s\" does not support the current platform. Skipping...",
                           prj->GetTitle(), tlist[x]);

                Manager::Get()->GetLogManager()->LogWarning(msg, m_PageIndex);
                continue;
            }

            CompilerValidResult result = CompilerValid(tgt);
            if (!result.isValid)
            {
                PrintInvalidCompiler(tgt, result.compiler, "Skipping...");
                continue;
            }

            BuildJobTarget bjt;
            bjt.project = prj;
            bjt.targetName = tlist[x];

            m_BuildJobTargetsList.push(bjt);

//            Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("Job: %s - %s"), prj->GetTitle().c_str(), prj->GetBuildTarget(tlist[x])->GetTitle().c_str()));
        }
    }

    // were there any jobs generated?
    if (m_BuildJobTargetsList.empty())
        NotifyJobDone(true);

//    Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("** Done creating deps")));
}

CompilerGCC::BuildJobTarget CompilerGCC::GetNextJob()
{
    BuildJobTarget ret;
    if (m_BuildJobTargetsList.empty())
        return ret;
    ret = m_BuildJobTargetsList.front();
    m_BuildJobTargetsList.pop();
    return ret;
}

const CompilerGCC::BuildJobTarget& CompilerGCC::PeekNextJob()
{
    static BuildJobTarget ret;

    if (m_BuildJobTargetsList.empty())
        return ret;
    return m_BuildJobTargetsList.front();
}

int CompilerGCC::DoBuild(bool clean, bool build)
{
    BuildJobTarget bj = GetNextJob();

    // no jobs list?
    if (!bj.project)
        return -2;

    // make sure all project files are saved
    if (    bj.project
        && (bj.project != m_pBuildingProject)
        && !bj.project->SaveAllFiles() )  // avoid saving when we only switch targets
    {
        Manager::Get()->GetLogManager()->Log(_("Could not save all files..."));
    }

    m_pBuildingProject = bj.project;
    m_BuildingTargetName = bj.targetName;
    ProjectBuildTarget* bt = bj.project->GetBuildTarget(bj.targetName);

    m_Clean = clean;
    m_Build = build;

    if (!bt || !CompilerValid(bt).isValid)
        return -2;

    BuildStateManagement();

    return 0;
}

void CompilerGCC::CalculateWorkspaceDependencies(wxArrayInt& deps)
{
    deps.Clear();
    ProjectsArray* arr = Manager::Get()->GetProjectManager()->GetProjects();
    for (size_t i = 0; i < arr->GetCount(); ++i)
    {
        CalculateProjectDependencies(arr->Item(i), deps);
    }
}

void CompilerGCC::CalculateProjectDependencies(cbProject* prj, wxArrayInt& deps)
{
    int prjidx = Manager::Get()->GetProjectManager()->GetProjects()->Index(prj);
    const ProjectsArray* arr = Manager::Get()->GetProjectManager()->GetDependenciesForProject(prj);
    if (!arr || !arr->GetCount())
    {
        // no dependencies; add the project in question and exit
        if (deps.Index(prjidx) == wxNOT_FOUND)
        {
//            Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("Adding dependency: %s"), prj->GetTitle().c_str()));
            deps.Add(prjidx);
        }
        return;
    }

    for (size_t i = 0; i < arr->GetCount(); ++i)
    {
        cbProject* thisprj = arr->Item(i);
        if (!Manager::Get()->GetProjectManager()->CausesCircularDependency(prj, thisprj))
        {
            // recursively check dependencies
            CalculateProjectDependencies(thisprj, deps);

            // find out project's index in full (open) projects array
            ProjectsArray* parr = Manager::Get()->GetProjectManager()->GetProjects();
            int idx = parr->Index(thisprj);
            if (idx != wxNOT_FOUND)
            {
                // avoid duplicates
                if (deps.Index(idx) == wxNOT_FOUND)
                {
//                    Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("Adding dependency: %s"), thisprj->GetTitle().c_str()));
                    deps.Add(idx);
                }
            }
        }
        else
            Manager::Get()->GetLogManager()->Log(wxString::Format(_("Circular dependency detected between \"%s\" and \"%s\". Skipping..."), prj->GetTitle(), thisprj->GetTitle()), m_PageIndex, Logger::warning);
    }

    // always add the project in question
    if (deps.Index(prjidx) == wxNOT_FOUND)
    {
//        Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("Adding dependency: %s"), prj->GetTitle().c_str()));
        deps.Add(prjidx);
    }
}

int CompilerGCC::DoBuild(const wxString& target, bool clean, bool build, bool clearLog)
{
    wxString realTarget = target;
    if (realTarget.IsEmpty())
        realTarget = GetTargetString();

    if (!StopRunningDebugger())
        return -1;

    if (!CheckProject())
    {
        // no active project
        if (Manager::Get()->GetEditorManager()->GetActiveEditor())
            return CompileFile(Manager::Get()->GetEditorManager()->GetActiveEditor()->GetFilename());
        return -1;
    }

    if (realTarget.IsEmpty())
        return -1;

    if (!m_IsWorkspaceOperation)
    {
        DoClearErrors();
        InitBuildLog(false);
        DoPrepareQueue(clearLog);
        if (clean)
            NotifyCleanProject(realTarget);
    }

    PreprocessJob(m_pProject, realTarget);
    if (m_BuildJobTargetsList.empty())
        return -1;

    InitBuildState(bjProject, realTarget);
    if (DoBuild(clean, build))
        return -2;

    return DoRunQueue();
}

int CompilerGCC::Build(const wxString& target)
{
    m_LastBuildStep = true;
    cbClearBackticksCache();
    return DoBuild(target, false, true);
}

int CompilerGCC::Build(ProjectBuildTarget* target)
{
    return Build(target ? target->GetTitle() : _T(""));
}

int CompilerGCC::Rebuild(ProjectBuildTarget* target)
{
    return Rebuild(target ? target->GetTitle() : _T(""));
}

int CompilerGCC::Rebuild(const wxString& target)
{
    cbClearBackticksCache();
    m_LastBuildStep = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/rebuild_seperately"), false);
    if (m_LastBuildStep)
        return DoBuild(target, true, true);

    int result = DoBuild(target, true, false);
    m_LastBuildStep = true;
    return result + DoBuild(target, false, true, false);
}

int CompilerGCC::DoWorkspaceBuild(const wxString& target, bool clean, bool build, bool clearLog)
{
    wxString realTarget = target;
    if (realTarget.IsEmpty())
        realTarget = GetTargetString();
    if (realTarget.IsEmpty())
        return -1;

    if (!StopRunningDebugger())
        return -1;

    DoPrepareQueue(clearLog);
    if (clean)
        NotifyCleanWorkspace();
    m_IsWorkspaceOperation = true;

    InitBuildLog(true);

    // save files from all projects as they might require each other...
    ProjectsArray* arr = Manager::Get()->GetProjectManager()->GetProjects();
    if (arr)
    {
        for (size_t i = 0; i < arr->GetCount(); ++i)
        {
            cbProject* prj = arr->Item(i);
            if (prj && !prj->SaveAllFiles())
                Manager::Get()->GetLogManager()->Log(wxString::Format(_("Could not save all files of %s..."), prj->GetTitle()), m_PageIndex);
        }
    }

    // create list of jobs to run (project->realTarget pairs)
    PreprocessJob(nullptr, realTarget);
    if (m_BuildJobTargetsList.empty())
        return -1;

    InitBuildState(bjWorkspace, realTarget);

    DoBuild(clean,build);
    m_IsWorkspaceOperation = false;

    return DoRunQueue();
}

int CompilerGCC::BuildWorkspace(const wxString& target)
{
    cbClearBackticksCache();
    return DoWorkspaceBuild(target, false, true);
}

int CompilerGCC::RebuildWorkspace(const wxString& target)
{
    cbClearBackticksCache();
    m_LastBuildStep = Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/rebuild_seperately"), false);
    if (m_LastBuildStep)
        return DoWorkspaceBuild(target, true, true);

    int result = DoWorkspaceBuild(target, true, false);
    m_LastBuildStep = true;
    return result + DoWorkspaceBuild(target, false, true, false);
}

int CompilerGCC::CleanWorkspace(const wxString& target)
{
    cbClearBackticksCache();
    return DoWorkspaceBuild(target, true, false);
}

int CompilerGCC::KillProcess()
{
    ResetBuildState();
    m_RunAfterCompile = false;
    if (!IsProcessRunning())
        return 0;
    if (!m_CommandQueue.LastCommandWasRun())
        LogMessage(_("Aborting build..."), cltInfo, ltMessages);
    wxKillError ret = wxKILL_OK;

    m_CommandQueue.Clear();

    ProjectManager* projectManager = Manager::Get()->GetProjectManager();
    bool isRunning = projectManager->GetIsRunning() == this;

    for (CompilerProcess &p : m_CompilerProcessList)
    {
        if (!p.pProcess)
            continue;

        #if defined(WIN32) && defined(ENABLE_SIGTERM)
            ::GenerateConsoleCtrlEvent(0, p.PID);
        #endif

        // Close input pipe
        p.pProcess->CloseOutput();
        ((PipedProcess*) p.pProcess)->ForfeitStreams();

        wxLogNull nullLog;

        if (isRunning)
        {
            // We are running a target, so just kill it to prevent any SIGTERM handlers to prevent
            // the termination.
            ret = wxProcess::Kill(p.PID, wxSIGKILL, wxKILL_CHILDREN);
        }
        else
        {
            // This is a compilation process and compilers generally don't prevent SIGTERM, in fact
            // they handle it correctly and clean up file, so we're supposed to use SIGTERM on them.
            // If we use SIGKILL they might leave some partially written files, partially written
            // object files pretty bad, because they lead to broken incremental builds (the linking
            // fails).
            ret = wxProcess::Kill(p.PID, wxSIGTERM, wxKILL_CHILDREN);
        }


        // According wxWidgets Documentation [1] OnTerminate is never called if we use
        // wxProcess::Kill. The pointer to the wxProcess object is used to check if the
        // process is still running. It is set to nullptr in the OnTerminate  function
        // of PipedProcess. But if we use wxProcess::Kill this function is never
        // called, so we have to set it to nullptr here. The wxProcess object is
        // deleted by wxWidgets, so we simply can set this pointer to nullptr
        // after killing the process...
        //
        // [1] https://docs.wxwidgets.org/trunk/classwx_process.html#aa378b7e705c9191431cad51a81581836
        p.pProcess = nullptr;

        if (!platform::windows)
        {
            if (ret != wxKILL_OK)
            {
                // No need to tell the user about the errors - just keep him waiting.
                Manager::Get()->GetLogManager()->Log(wxString::Format(_("Aborting process %ld ..."), p.PID), m_PageIndex);
            }
            else switch (ret)
            {
                case wxKILL_OK:
                    Manager::Get()->GetLogManager()->Log(_("Process aborted (killed)."), m_PageIndex);
//                case wxKILL_ACCESS_DENIED: cbMessageBox(_("Access denied"));     break;
//                case wxKILL_NO_PROCESS:    cbMessageBox(_("No process"));        break;
//                case wxKILL_BAD_SIGNAL:    cbMessageBox(_("Bad signal"));        break;
//                case wxKILL_ERROR:         cbMessageBox(_("Unspecified error")); break;
                case wxKILL_ACCESS_DENIED: // fall-through
                case wxKILL_NO_PROCESS:    // fall-through
                case wxKILL_BAD_SIGNAL:    // fall-through
                case wxKILL_ERROR:         // fall-through
                default:                   break;
            }
        }
    }

    if (isRunning)
        projectManager->SetIsRunning(nullptr);
    return ret;
}

bool CompilerGCC::IsRunning() const
{
    return m_BuildJob != bjIdle || IsProcessRunning() || m_CommandQueue.GetCount();
}

ProjectBuildTarget* CompilerGCC::GetBuildTargetForFile(ProjectFile* pf)
{
    if (!pf)
        return nullptr;

    if (!pf->buildTargets.GetCount())
    {
        cbMessageBox(_("That file isn't assigned to any target."),
                    _("Information"), wxICON_INFORMATION);
        return nullptr;
    }
    // If a virtual target is selected, ask for build target.
    if (m_RealTargetIndex == -1)
    {
        int idx = DoGUIAskForTarget();
        if (idx == -1)
            return nullptr;
        return m_pProject->GetBuildTarget(idx);
    }

    // Use currently selected non-virtual target.
    // If the file is not added to this target return nullptr.
    const wxString &targetName = m_Targets[m_TargetIndex];
    if (std::find(pf->buildTargets.begin(), pf->buildTargets.end(), targetName) == pf->buildTargets.end())
        return nullptr;
    return m_pProject->GetBuildTarget(targetName);
}

int CompilerGCC::CompileFile(const wxString& file)
{
    CheckProject();
    DoClearErrors();
    DoPrepareQueue(false);

    ProjectFile* pf = m_pProject ? m_pProject->GetFileByFilename(file, true, false) : nullptr;
    ProjectBuildTarget* bt = GetBuildTargetForFile(pf);

    PrintBanner(baBuildFile, m_pProject, bt);

    if ( !CompilerValid(bt).isValid )
        return -1;
    if (!pf) // compile single file not belonging to a project
        return CompileFileWithoutProject(file);
    if (!bt)
    {
        const wxString err(_("error: Cannot find target for file"));
        LogMessage(pf->relativeToCommonTopLevelPath + ": " + err, cltError);
        LogWarningOrError(cltError, m_pProject, pf->relativeToCommonTopLevelPath, wxEmptyString, err);
        return -2;
    }
    if (m_pProject)
        wxSetWorkingDirectory(m_pProject->GetBasePath());
    return CompileFileDefault(m_pProject, pf, bt); // compile file using default build system
}

int CompilerGCC::CompileFileWithoutProject(const wxString& file)
{
    // compile single file not belonging to a project
    Manager::Get()->GetEditorManager()->Save(file);

    // switch to the default compiler
    SwitchCompiler(CompilerFactory::GetDefaultCompilerID());
    Manager::Get()->GetMacrosManager()->Reset();

    Compiler* compiler = CompilerFactory::GetDefaultCompiler();

    // get compile commands for file (always linked as console-executable)
    DirectCommands dc(this, compiler, nullptr, m_PageIndex);
    wxArrayString compile = dc.GetCompileSingleFileCommand(file);
    AddToCommandQueue(compile);

    return DoRunQueue();
}

int CompilerGCC::CompileFileDefault(cbProject* project, ProjectFile* pf, ProjectBuildTarget* bt)
{
    Compiler* compiler = CompilerFactory::GetCompiler(bt->GetCompilerID());
    if (!compiler)
    {
        const wxString &err = wxString::Format(_("error: Cannot build file for target '%s'. Compiler '%s' cannot be found!"),
                                               bt->GetTitle().wx_str(), bt->GetCompilerID().wx_str());
        LogMessage(pf->relativeToCommonTopLevelPath + ": " + err, cltError);
        LogWarningOrError(cltError, project, pf->relativeToCommonTopLevelPath, wxEmptyString, err);
        return -3;
    }

    DirectCommands dc(this, compiler, project, m_PageIndex);
    wxArrayString compile = dc.CompileFile(bt, pf);
    AddToCommandQueue(compile);

    return DoRunQueue();
}

// events

void CompilerGCC::OnIdle(wxIdleEvent& event)
{
    if (IsProcessRunning())
    {
        for (const CompilerProcess &p : m_CompilerProcessList)
        {
            if (p.pProcess && (static_cast<PipedProcess*>(p.pProcess))->HasInput())
            {
                event.RequestMore();
                break;
            }
        }
    }
    else
        event.Skip();
}

void CompilerGCC::OnTimer(cb_unused wxTimerEvent& event)
{
    wxWakeUpIdle();
}

void CompilerGCC::OnRun(cb_unused wxCommandEvent& event)
{
    if (Run() == 0)
        DoRunQueue();
}

void CompilerGCC::OnCompileAndRun(cb_unused wxCommandEvent& event)
{
    ProjectBuildTarget* target = nullptr;
    m_RunAfterCompile = true;
    Build(target);
}

void CompilerGCC::OnCompile(wxCommandEvent& event)
{
    int bak = m_RealTargetIndex;
    if (event.GetId() == idMenuCompileFromProjectManager)
    {
        // we 're called from a menu in ProjectManager
        // let's check the selected project...
        DoSwitchProjectTemporarily();
    }
    ProjectBuildTarget* target = nullptr;
    Build(target);
    m_RealTargetIndex = bak;
}

void CompilerGCC::OnCompileFile(wxCommandEvent& event)
{
    // TODO (Rick#1#): Clean the file so it will always recompile
    wxFileName file;
    if (event.GetId() == idMenuCompileFileFromProjectManager)
        PrepareCompileFilePM(file);
    else
        PrepareCompileFile(file);

    StartCompileFile(file);
}

void CompilerGCC::OnCleanFile(wxCommandEvent& event)
{
    if (event.GetId() == idMenuCleanFileFromProjectManager)
    {
        FileTreeData* ftd = DoSwitchProjectTemporarily();
        ProjectFile* pf = ftd->GetProjectFile();
        if (!pf)
            return;

        ProjectBuildTarget* bt = GetBuildTargetForFile(pf);
        if (!bt)
            return;

        Compiler* compiler = CompilerFactory::GetCompiler(bt->GetCompilerID());
        if (!compiler)
            return;

        if (!CheckProject()) // ensures m_pProject is not nullptr
          return;

        wxSetWorkingDirectory(m_pProject->GetBasePath());

        wxFileName fn(pf->GetObjName());
        wxString obj_name = (compiler->GetSwitches().UseFlatObjects) ? fn.GetFullName() : fn.GetFullPath();
        wxString obj_file = wxFileName(bt->GetObjectOutput() + wxFILE_SEP_PATH + obj_name).GetFullPath();
        Manager::Get()->GetMacrosManager()->ReplaceEnvVars(obj_file);

        if ( wxFileExists(obj_file) )
        {
            if ( wxRemoveFile(obj_file) )
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format("File has been removed: %s", obj_file));
            else
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Removing file failed for: %s", obj_file));
        }
        else
            Manager::Get()->GetLogManager()->DebugLog(wxString::Format("File to remove does not exist: %s", obj_file));
    }
}

void CompilerGCC::OnRebuild(wxCommandEvent& event)
{
    CheckProject();
    AnnoyingDialog dlg(_("Rebuild project"),
                        _("Rebuilding the project will cause the deletion of all "
                        "object files and building it from scratch.\nThis action "
                        "might take a while, especially if your project contains "
                        "more than a few files.\nAnother factor is your CPU "
                        "and the available system memory.\n\n"
                        "Are you sure you want to rebuild the entire project?"),
                    wxART_QUESTION);
    PlaceWindow(&dlg);
    if (m_pProject && dlg.ShowModal() == AnnoyingDialog::rtNO)
        return;

    int bak = m_RealTargetIndex;
    if (event.GetId() == idMenuRebuildFromProjectManager)
    {
        // we 're called from a menu in ProjectManager
        // let's check the selected project...
        DoSwitchProjectTemporarily();
    }
    ProjectBuildTarget* target = nullptr;
    Rebuild(target);
    m_RealTargetIndex = bak;
}

void CompilerGCC::OnCompileAll(cb_unused wxCommandEvent& event)
{
    BuildWorkspace();
}

void CompilerGCC::OnRebuildAll(cb_unused wxCommandEvent& event)
{
    AnnoyingDialog dlg(_("Rebuild workspace"),
                        _("Rebuilding ALL the open projects will cause the deletion of all "
                        "object files and building them from scratch.\nThis action "
                        "might take a while, especially if your projects contain "
                        "more than a few files.\nAnother factor is your CPU "
                        "and the available system memory.\n\n"
                        "Are you sure you want to rebuild ALL the projects?"),
                    wxART_QUESTION);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == AnnoyingDialog::rtNO)
        return;

    RebuildWorkspace();
}

void CompilerGCC::OnCleanAll(cb_unused wxCommandEvent& event)
{
    AnnoyingDialog dlg(_("Clean project"),
                        _("Cleaning ALL the open projects will cause the deletion "
                        "of all relevant object files.\nThis means that you will "
                        "have to build ALL your projects from scratch next time you "
                        "'ll want to build them.\nThat action "
                        "might take a while, especially if your projects contain "
                        "more than a few files.\nAnother factor is your CPU "
                        "and the available system memory.\n\n"
                        "Are you sure you want to proceed to cleaning?"),
                    wxART_QUESTION);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == AnnoyingDialog::rtNO)
        return;

    CleanWorkspace();
}

void CompilerGCC::OnClean(wxCommandEvent& event)
{
    CheckProject();
    AnnoyingDialog dlg(_("Clean project"),
                        _("Cleaning the target or project will cause the deletion "
                        "of all relevant object files.\nThis means that you will "
                        "have to build your project from scratch next time you "
                        "'ll want to build it.\nThat action "
                        "might take a while, especially if your project contains "
                        "more than a few files.\nAnother factor is your CPU "
                        "and the available system memory.\n\n"
                        "Are you sure you want to proceed to cleaning?"),
                    wxART_QUESTION);
    PlaceWindow(&dlg);
    if (m_pProject && dlg.ShowModal() == AnnoyingDialog::rtNO)
        return;

    int bak = m_RealTargetIndex;
    if (event.GetId() == idMenuCleanFromProjectManager)
    {
        // we 're called from a menu in ProjectManager
        // let's check the selected project...
        DoSwitchProjectTemporarily();
    }
    ProjectBuildTarget* target = nullptr;
    Clean(target);
    m_RealTargetIndex = bak;
}

void CompilerGCC::OnProjectCompilerOptions(cb_unused wxCommandEvent& event)
{
    ProjectManager* manager = Manager::Get()->GetProjectManager();
    wxTreeCtrl* tree = manager->GetUI().GetTree();
    wxTreeItemId sel = manager->GetUI().GetTreeSelection();
    FileTreeData* ftd = sel.IsOk() ? (FileTreeData*)tree->GetItemData(sel) : nullptr;
    if (ftd)
    {
        // 'configure' selected target, if other than 'All'
        ProjectBuildTarget* target = nullptr;
        cbProject* currentProject = ftd->GetProject();
        if (m_TargetIndex != -1 && !m_Targets.empty())
        {
            const wxString &targetName = m_Targets[m_TargetIndex];
            if (currentProject == m_pProject)
            {
                target = m_pProject->GetBuildTarget(targetName);
            }
            else
            {
                // If the users wants to change the options for the non-active project,
                // we try to find a target with the same name as the currently selected
                // target in the active project (if the target is not 'All').
                if (!targetName.empty())
                    target = currentProject->GetBuildTarget(targetName);
            }
        }
        Configure(currentProject, target, Manager::Get()->GetAppWindow());
    }
    else
    {
        if (cbProject* prj = Manager::Get()->GetProjectManager()->GetActiveProject())
            Configure(prj, nullptr, Manager::Get()->GetAppWindow());
    }
}

void CompilerGCC::OnTargetCompilerOptions(cb_unused wxCommandEvent& event)
{
    int bak = m_RealTargetIndex;
    // we 're called from a menu in ProjectManager
    int idx = DoGUIAskForTarget();
    if (idx == -1)
        return;
    else
        m_RealTargetIndex = idx; // TODO: check

    // let's check the selected project...
    DoSwitchProjectTemporarily();

    ProjectBuildTarget* target = nullptr;
    m_RealTargetIndex = bak;
    Configure(m_pProject, target, Manager::Get()->GetAppWindow());
}

void CompilerGCC::OnKillProcess(cb_unused wxCommandEvent& event)
{
    KillProcess();
}

void CompilerGCC::OnSelectTarget(wxCommandEvent& event)
{
    int selection = -1;
    bool updateTools = false;

    if (event.GetId() == idToolTarget)
    {   // through the toolbar
        selection = event.GetSelection();
    }
    else if (event.GetId() == idMenuSelectTargetDialog)
    {
        // The select target menu is clicked, so we want to show a dialog with all targets
        IncrementalSelectArrayIterator iterator(m_Targets);
        IncrementalSelectDialog dlg(Manager::Get()->GetAppWindow(), &iterator, _("Select target..."),
                                    _("Choose target:"));
        PlaceWindow(&dlg);
        if (dlg.ShowModal() == wxID_OK)
        {
            selection = dlg.GetSelection();
            updateTools = true;
        }
    }
    else
    {   // through Build->SelectTarget
        selection = event.GetId() - idMenuSelectTargetOther[0];
        updateTools = true;
    }

    if (selection >= 0)
    {
        Manager::Get()->GetProjectManager()->GetWorkspace()->SetPreferredTarget( GetTargetString(selection) );
        DoUpdateTargetMenu(selection);
        if (updateTools && m_pToolTarget)
            m_pToolTarget->SetSelection(selection);
    }
}

void CompilerGCC::OnNextError(cb_unused wxCommandEvent& event)
{
    DoGotoNextError();
}

void CompilerGCC::OnPreviousError(cb_unused wxCommandEvent& event)
{
    DoGotoPreviousError();
}

void CompilerGCC::OnClearErrors(cb_unused wxCommandEvent& event)
{
    DoClearErrors();
}

void CompilerGCC::OnUpdateUI(wxUpdateUIEvent& event)
{
    const int id = event.GetId();
    if  (id == idMenuKillProcess)
    {
        event.Enable(IsRunning());
        return;
    }

    if (IsRunning())
    {
        event.Enable(false);
        return;
    }

    ProjectManager* projectManager = Manager::Get()->GetProjectManager();
    cbPlugin* runningPlugin = projectManager->GetIsRunning();
    if (runningPlugin && runningPlugin != this)
    {
        event.Enable(false);
        return;
    }

    cbProject* prj = projectManager->GetActiveProject();
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();

    if (id == idMenuRun)
        event.Enable(ExecutableExists(prj));
    else if (id == idMenuCompile || id == idMenuCompileAndRun)
        event.Enable(prj || ed);
    else if (id == idMenuBuildWorkspace || id == idMenuRebuild || id == idMenuRebuildWorkspace
        || id == idMenuClean || id == idMenuCleanWorkspace || id == idMenuSelectTarget
        || id == idMenuSelectTargetDialog || id == idMenuProjectCompilerOptions || idToolTarget)
    {
        event.Enable(prj);
    }
    else if (id == idMenuCompileFile)
        event.Enable(ed);
    else if  (id == idMenuNextError)
        event.Enable((prj || ed) && m_Errors.HasNextError());
    else if  (id == idMenuPreviousError)
        event.Enable((prj || ed) && m_Errors.HasPreviousError());
    else if  (id == idMenuClearErrors)
        event.Enable(true);
}

void CompilerGCC::OnProjectActivated(CodeBlocksEvent& event)
{
    //NOTE: this function is also called on PROJECT_TARGETS_MODIFIED events
    //      to keep the combobox in sync

    cbProject* active = Manager::Get()->GetProjectManager()->GetActiveProject();
//    DBGLOG(_T("Active: %s, Event: %s"),
//            active ? active->GetTitle().c_str() : _T("<none>"),
//            event.GetProject()->GetTitle().c_str());
    if (event.GetProject() == active)
        UpdateProjectTargets(event.GetProject());
}

void CompilerGCC::OnProjectLoaded(cb_unused CodeBlocksEvent& event)
{
}

void CompilerGCC::OnProjectUnloaded(CodeBlocksEvent& event)
{
    // just make sure we don't keep an invalid pointer around
    if (m_pProject == event.GetProject())
        m_pProject = nullptr;
}

void CompilerGCC::OnWorkspaceClosed(cb_unused CodeBlocksEvent& event)
{
    ClearLog(false);
    DoClearErrors();
}

void CompilerGCC::OnCompileFileRequest(CodeBlocksEvent& event)
{
    cbProject*  prj = event.GetProject();
    EditorBase* eb  = event.GetEditor();
    if (!prj || !eb)
    {
//        Manager::Get()->GetLogManager()->DebugLog(_T("Compile file request skipped due to missing project or editor."));
        return;
    }

    const wxString& ed_filename = eb->GetFilename();
    wxFileName wx_filename;
    wx_filename.Assign(ed_filename);
    wx_filename.MakeRelativeTo( prj->GetBasePath() );

    wxString filepath = wx_filename.GetFullPath();
    if (filepath.IsEmpty())
    {
//        Manager::Get()->GetLogManager()->DebugLog(_T("Compile file request skipped due to unresolvable file."));
        return;
    }

    ProjectFile* pf = prj->GetFileByFilename(UnixFilename(filepath), true, false);
    if (!pf || !pf->buildTargets.GetCount())
    {
//            Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Skipping incoming compile file request for '%s' (no project file or build targets).", filepath));
        return;
    }

    ProjectBuildTarget* bt = nullptr;
    if (pf->buildTargets.GetCount() == 1)
        bt = prj->GetBuildTarget(pf->buildTargets[0]);
    else // belongs to two or more build targets, but maybe a valid virtual target is selected
        bt = prj->GetBuildTarget(m_RealTargetIndex); // pick the selected target
    if (!bt)
    {
//        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Skipping incoming compile file request for '%s' (no build target).", filepath));
        return;
    }

    Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Executing incoming compile file request for '%s'.", filepath));
    CompileFileDefault(prj, pf, bt);
}

void CompilerGCC::OnGCCOutput(CodeBlocksEvent& event)
{
    wxString msg = event.GetString();
    if (!msg.IsEmpty() &&
        !msg.Matches(_T("# ??*")))  // gcc 3.4 started displaying a line like this filter
                                    // when calculating dependencies. Until I check out
                                    // why this happens (and if there is a switch to
                                    // turn it off), I put this condition here to avoid
                                    // displaying it...
    {
        AddOutputLine(msg);
    }
}

void CompilerGCC::OnGCCError(CodeBlocksEvent& event)
{
    wxString msg = event.GetString();
    if (!msg.IsEmpty())
        AddOutputLine(msg);
}

void CompilerGCC::OnGCCTerminated(CodeBlocksEvent& event)
{
    const int index = event.GetX();
    OnJobEnd(index, event.GetInt());
}

void CompilerGCC::AddOutputLine(const wxString& output, bool forceErrorColour)
{
    wxArrayString ignore_output = Manager::Get()->GetConfigManager("compiler")->ReadArrayString("/ignore_output");
    if (!ignore_output.IsEmpty())
    {
        for (size_t i = 0; i<ignore_output.GetCount(); ++i)
        {
            if (output.Find(ignore_output.Item(i)) != wxNOT_FOUND)
            {
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Ignoring compiler output: %s", output));
                return;
            }
        }
    }

    Compiler* compiler = CompilerFactory::GetCompiler(m_CompilerId);
    if (!compiler)
        return;
    CompilerLineType clt = compiler->CheckForWarningsAndErrors(output);

    // if max_errors reached, display a one-time message and do not log any more
    size_t maxErrors = Manager::Get()->GetConfigManager(_T("compiler"))->ReadInt(_T("/max_reported_errors"), 50);
    if (maxErrors > 0 && m_Errors.GetCount(cltError) == maxErrors)
    {
        // no matter what, everything goes into the build log
        LogMessage(output, clt, ltFile, forceErrorColour);

        if (!m_NotifiedMaxErrors)
        {
            m_NotifiedMaxErrors = true;

            // if we reached the max errors count, notify about it
            LogWarningOrError(cltNormal, nullptr, wxEmptyString, wxEmptyString, _("More errors follow but not being shown."));
            LogWarningOrError(cltNormal, nullptr, wxEmptyString, wxEmptyString, _("Edit the max errors limit in compiler options..."));
        }
        return;
    }

    // log to build messages if info/warning/error (aka != normal)
    if (clt != cltNormal)
    {
        // actually log message
        wxString last_error_filename = compiler->GetLastErrorFilename();
        if ( UseMake() )
        {
            wxFileName last_error_file(last_error_filename);
            if (!last_error_file.IsAbsolute())
            {
                cbProject* project = m_pProject;
                if (m_pLastBuildingTarget)
                    project = m_pLastBuildingTarget->GetParentProject();
                else
                {
                    AskForActiveProject();
                    project = m_pProject;
                }
                last_error_file = project->GetExecutionDir() + wxFileName::GetPathSeparator() + last_error_file.GetFullPath();
                last_error_file.MakeRelativeTo(project->GetBasePath());
                last_error_filename = last_error_file.GetFullPath();
            }
        }
        wxString msg = compiler->GetLastError();
        if (!compiler->WithMultiLineMsg() || (compiler->WithMultiLineMsg() && !msg.IsEmpty()))
            LogWarningOrError(clt, m_pBuildingProject, last_error_filename, compiler->GetLastErrorLine(), msg);
    }

    // add to log
    LogMessage(output, clt, ltAll, forceErrorColour);
}

void CompilerGCC::LogWarningOrError(CompilerLineType lt, cbProject* prj, const wxString& filename, const wxString& line, const wxString& msg)
{
    // add build message
    wxArrayString errors;
    errors.Add(filename);
    errors.Add(line);

    wxString msgFix = msg; msgFix.Replace(wxT("\t"), wxT("    "));
    errors.Add(msgFix);

    Logger::level lv = (lt == cltError)   ? Logger::error
                     : (lt == cltWarning) ? Logger::warning : Logger::info;

    // when there are many lines (thousands) of output, auto fitting column width
    // is very expensive, so rate limit it to a maximum of 1 fit per 3 seconds
    static wxDateTime lastAutofitTime = wxDateTime((time_t)0);
    if ( lastAutofitTime < (wxDateTime::Now() - wxTimeSpan::Seconds(3)) )
    {
        lastAutofitTime = wxDateTime::Now();
        m_pListLog->Append(errors, lv, 2); // auto fit the 'Message' column
    }
    else
        m_pListLog->Append(errors, lv);

    // add to error keeping struct
    m_Errors.AddError(lt, prj, filename, line.IsEmpty() ? 0 : atoi(wxSafeConvertWX2MB(line.wc_str())), msg);
}

void CompilerGCC::LogMessage(const wxString& message, CompilerLineType lt, LogTarget log, bool forceErrorColour, bool isTitle, bool updateProgress)
{
    // Strip the
    wxString msgInput, msg;
    if (message.StartsWith(COMPILER_SIMPLE_LOG, &msg))
        msgInput = msg;
    else
        msgInput = message;

    if (msgInput.StartsWith(COMPILER_NOTE_ID_LOG, &msg))
        LogWarningOrError(lt, nullptr, wxEmptyString, wxEmptyString, msg);
    else if(msgInput.StartsWith(COMPILER_ONLY_NOTE_ID_LOG, &msg))
    {
        LogWarningOrError(lt, nullptr, wxEmptyString, wxEmptyString, msg);
        updateProgress = false;
    }
    else if (msgInput.StartsWith(COMPILER_WARNING_ID_LOG, &msg))
    {
        if (lt != cltError)
            lt = cltWarning;
        LogWarningOrError(lt, nullptr, wxEmptyString, wxEmptyString, msg);
    }
    else if (msgInput.StartsWith(COMPILER_ERROR_ID_LOG, &msg))
    {
        if (lt != cltError)
            lt = cltWarning;
        LogWarningOrError(cltError, nullptr, wxEmptyString, wxEmptyString, msg);
    }
    else
        msg = msgInput;

    // log file
    if (log & ltFile)
    {
        if (forceErrorColour)
            m_BuildLogContents << _T("<font color=\"#a00000\">");
        else if (lt == cltError)
            m_BuildLogContents << _T("<font color=\"#ff0000\">");
        else if (lt == cltWarning)
            m_BuildLogContents << _T("<font color=\"#0000ff\">");

        if (isTitle)
            m_BuildLogContents << _T("<b>");

        // Replace the script quotation marks family by "
        // Using UTF codes to avoid "error: converting to execution character set: Illegal byte sequence"
        // -> for UTF codes see here: http://www.utf8-chartable.de/unicode-utf8-table.pl
        wxString sQuoted(msg);
        wxString sGA = wxString::FromUTF8("\x60");     // GRAVE ACCENT
        wxString sAA = wxString::FromUTF8("\xC2\xB4"); // ACUTE ACCENT
        sQuoted.Replace(sGA,     _T("\""),    true);
        sQuoted.Replace(sAA,     _T("\""),    true);
        // avoid conflicts with html-tags
        sQuoted.Replace(_T("&"), _T("&amp;"), true);
        sQuoted.Replace(_T("<"), _T("&lt;"),  true);
        sQuoted.Replace(_T(">"), _T("&gt;"),  true);
        m_BuildLogContents << sQuoted;

        if (isTitle)
            m_BuildLogContents << _T("</b>");

        if (lt == cltWarning || lt == cltError || forceErrorColour)
            m_BuildLogContents << _T("</font>");

        m_BuildLogContents << _T("<br />\n");
    }

    // log window
    if (log & ltMessages)
    {
        Logger::level lv = isTitle ? Logger::caption : Logger::info;
        if (forceErrorColour)
            lv = Logger::critical;
        else if (lt == cltError)
            lv = Logger::error;
        else if (lt == cltWarning)
            lv = Logger::warning;

        wxString progressMsg;
        if (updateProgress && m_CurrentProgress < m_MaxProgress)
        {
            ++m_CurrentProgress;
            if (m_LogBuildProgressPercentage)
            {
                float p = (float)(m_CurrentProgress * 100.0f) / (float)m_MaxProgress;
                progressMsg.Printf(_T("[%5.1f%%] "), p);
            }
            if (m_pLog->progress)
            {
                m_pLog->progress->SetRange(m_MaxProgress);
                m_pLog->progress->SetValue(m_CurrentProgress);
            }
        }

        Manager::Get()->GetLogManager()->Log(progressMsg + msg, m_PageIndex, lv);
        Manager::Get()->GetLogManager()->LogToStdOut(progressMsg + msg);
    }
}

void CompilerGCC::InitBuildLog(bool workspaceBuild)
{
    wxString title;
    wxString basepath;
    wxString basename;
    if (!workspaceBuild && m_pProject)
    {
        title = m_pProject->GetTitle();
        basepath = m_pProject->GetBasePath();
        basename = wxFileName(m_pProject->GetFilename()).GetName();
    }
    else if (workspaceBuild)
    {
        cbWorkspace* wksp = Manager::Get()->GetProjectManager()->GetWorkspace();
        title = wksp->GetTitle();
        basepath = wxFileName(wksp->GetFilename()).GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
        basename = wxFileName(wksp->GetFilename()).GetName();
    }

    if (basename.IsEmpty())
        basename = _T("unnamed");

    // init HTML build log
    m_BuildStartTime = wxDateTime::Now();
    m_BuildLogTitle = title + _(" build log");
    m_BuildLogFilename = basepath;
    m_BuildLogFilename << basename << _T("_build_log.html");
    m_BuildLogContents.Clear();
    m_MaxProgress = 0;
    m_CurrentProgress = 0;
}

void CompilerGCC::SaveBuildLog()
{
    // if not enabled in the configuration, leave
    if (!Manager::Get()->GetConfigManager(_T("compiler"))->ReadBool(_T("/save_html_build_log"), false))
        return;

    if (m_BuildLogFilename.IsEmpty())
        return;

    // NOTE: if we want to add a CSS later on, we 'd have to edit:
    //       - this function and
    //       - LogMessage()

    wxFile f(m_BuildLogFilename, wxFile::write);
    if (!f.IsOpened())
        return;

    // first output the standard header blurb
    f.Write(_T("<html>\n"));
    f.Write(_T("<head>\n"));
    f.Write(_T("<title>") + m_BuildLogTitle + _T("</title>\n"));
    f.Write(_T("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"));

    f.Write(_T("</head>\n"));
    f.Write(_T("<body>\n"));

    // use fixed-width font
    f.Write(_T("<tt>\n"));

    // write the start-end time of the build
    f.Write(_("Build started on: "));
    f.Write(_T("<u>"));
    f.Write(m_BuildStartTime.Format(_T("%d-%m-%Y at %H:%M.%S")));
    f.Write(_T("</u><br />\n"));
    f.Write(_("Build ended on: "));
    f.Write(_T("<u>"));
    f.Write(wxDateTime::Now().Format(_T("%d-%m-%Y at %H:%M.%S")));
    f.Write(_T("</u><p />\n"));

    // output the main body
    f.Write(m_BuildLogContents);

    // done with fixed-width font
    f.Write(_T("</tt>\n"));

    // finally output the footer
    f.Write(_T("</body>\n"));
    f.Write(_T("</html>\n"));

    Manager::Get()->GetLogManager()->Log(_("Build log saved as: "), m_PageIndex);
    wxString tempBuildLogFilename = m_BuildLogFilename;
    tempBuildLogFilename.Replace("\\", "/");
    wxURI tmpFilename = tempBuildLogFilename;

    Manager::Get()->GetLogManager()->Log(wxString::Format("file://%s", tmpFilename.BuildURI()), m_PageIndex, Logger::warning);
}

void CompilerGCC::OnJobEnd(size_t procIndex, int exitCode)
{
//    Manager::Get()->GetMessageManager()->Log(m_PageIndex, _T("JobDone: index=%u, exitCode=%d"), procIndex, exitCode));
    m_timerIdleWakeUp.Stop();
    CompilerProcess &process = m_CompilerProcessList.at(procIndex);
    process.PID = 0;
    process.pProcess = nullptr;
    wxString oFile = UnixFilename(process.OutputFile);

    if (m_LastExitCode == 0 || exitCode != 0) // prevent exit errors from being overwritten during multi-threaded build
        m_LastExitCode = exitCode;
    bool success(exitCode == 0);
    Compiler* compiler = CompilerFactory::GetCompiler(m_CompilerId);
    if (compiler)
        success = (exitCode >= 0) && (exitCode <= compiler->GetSwitches().statusSuccess);

    Manager::Get()->GetMacrosManager()->ReplaceMacros(oFile); // might contain macros!
    if (success && !oFile.IsEmpty())
    {
        wxLogNull silence; // In case opening the file fails
        wxFFile f(oFile.wx_str(), _T("r"));
        if (f.IsOpened())
        {
            size_t size = f.Length();
            f.Close();

            float displaySize;
            wxString units;
            if (size < 1024)
            {
                displaySize = (float)size;
                units = _("bytes");
            }
            else if (size < 1048576)
            {
                displaySize = (float)size / 1024.0f;
                units = _("KB");
            }
            else
            {
                displaySize = (float)size / 1048576.0f;
                units = _("MB");
            }
            wxString msg;
            msg.Printf(_("Output file is %s with size %.2f %s"), oFile.wx_str(), displaySize, units.wx_str());
            LogMessage(msg, cltNormal);
        }
    }
    if (success)
        m_LastExitCode = 0;
    if (m_CommandQueue.GetCount() != 0 && success)
        DoRunQueue(); // continue running commands while last exit code was 0.
    else
    {
        if (success)
        {
            if (IsProcessRunning())
            {
                DoRunQueue();
                return;
            }

            while (1)
            {
                BuildStateManagement();
                if (m_CommandQueue.GetCount())
                {
                    DoRunQueue();
                    return;
                }
                if (m_BuildState == bsNone && m_NextBuildState == bsNone)
                    break;
            }
        }
        m_CommandQueue.Clear();
        ResetBuildState();
        // clear any remaining jobs (e.g. in case of build errors)
        while (!m_BuildJobTargetsList.empty())
            m_BuildJobTargetsList.pop();

        wxString msg = wxString::Format(_("Process terminated with status %d (%s)"), exitCode, GetMinSecStr().wx_str());
        if (m_LastExitCode == exitCode) // do not log extra if there is failure during multi-threaded build
            LogMessage(msg, success ? cltWarning : cltError, ltAll, !success);
        if (!m_CommandQueue.LastCommandWasRun())
        {
            if ( !IsProcessRunning() )
            {
                msg = wxString::Format("%s (%s)", GetErrWarnStr(), GetMinSecStr());
                success = (m_LastExitCode >= 0) && (m_LastExitCode <= compiler->GetSwitches().statusSuccess);
                LogMessage(msg, success ? cltWarning : cltError, ltAll, !success);
                LogWarningOrError(cltNormal, nullptr, wxEmptyString, wxEmptyString,
                                  wxString::Format(_("=== Build %s: %s ==="),
                                                   wxString(m_LastExitCode == 0 ? _("finished") : _("failed")).wx_str(), msg.wx_str()));
                if (!Manager::IsBatchBuild())
                    m_pListLog->AutoFitColumns(2);

                SaveBuildLog();
            }
            if (!Manager::IsBatchBuild() && m_pLog->progress)
                m_pLog->progress->SetValue(0);
        }
        else
        {
            // last command was "Run"
            // force exit code to zero (0) or else debugger will think build failed if last run returned non-zero...
// TODO (mandrav##): Maybe create and use GetLastRunExitCode()? Is it needed?
            m_LastExitCode = 0; // *might* not be needed any more, see NotifyJobDone()
        }
        Manager::Get()->GetLogManager()->Log(" ", m_PageIndex); // blank line

        NotifyJobDone();

        if (!Manager::IsBatchBuild() && m_Errors.GetCount(cltError))
        {
            if (Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_show_build_errors"), true))
            {
                CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
                Manager::Get()->ProcessEvent(evtShow);
            }
            CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, m_pListLog);
            Manager::Get()->ProcessEvent(evtSwitch);

            if (Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_focus_build_errors"), true))
                m_pListLog->FocusError(m_Errors.GetFirstError());
        }
        else
        {
            if (m_RunAfterCompile)
            {
                m_RunAfterCompile = false;
                if (Run() == 0)
                    DoRunQueue();
            }
            else if (!Manager::IsBatchBuild())
            {
                // switch to the "Build messages" window only if the active log window is "Build log"
                CodeBlocksLogEvent evtGetActive(cbEVT_GET_ACTIVE_LOG_WINDOW);
                Manager::Get()->ProcessEvent(evtGetActive);
                if (evtGetActive.logger == m_pLog)
                {
                    // don't close the message manager (if auto-hiding), if warnings are required to keep it open
                    if (m_Errors.GetCount(cltWarning) &&
                        Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_show_build_warnings"), true))
                    {
                        CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
                        Manager::Get()->ProcessEvent(evtShow);

                        CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, m_pListLog);
                        Manager::Get()->ProcessEvent(evtSwitch);
                    }
                    else // if message manager is auto-hiding, unlock it (i.e. close it)
                    {
                        CodeBlocksLogEvent evtShow(cbEVT_HIDE_LOG_MANAGER);
                        Manager::Get()->ProcessEvent(evtShow);
                    }
                }
            }
        }

        m_RunAfterCompile = false;

        // no matter what happened with the build, return the focus to the active editor
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(Manager::Get()->GetEditorManager()->GetActiveEditor());
        if (ed)
            ed->GetControl()->SetFocus();
    }
}

void CompilerGCC::NotifyJobDone(bool showNothingToBeDone)
{
    if (!m_LastBuildStep)
        return;

    m_BuildJob = bjIdle;
    if (showNothingToBeDone && m_Errors.GetCount(cltError) == 0)
    {
        LogMessage(m_Clean ? _("Done.\n") : _("Nothing to be done (all items are up-to-date).\n"));
        // if message manager is auto-hiding, unlock it (i.e. close it)
        CodeBlocksLogEvent evtShow(cbEVT_HIDE_LOG_MANAGER);
        Manager::Get()->ProcessEvent(evtShow);
    }

    if (!IsProcessRunning())
    {
        ProjectManager* manager = Manager::Get()->GetProjectManager();

        // Check if this was a run operation and the application has been closed.
        // If this is the case we don't need to send cbEVT_COMPILER_FINISHED event.
        if (manager->GetIsRunning() == this)
            manager->SetIsRunning(nullptr);
        // The above is not true for the idMenuRun which sends the compiler started event.
        // If we sent the started event, make sure we send the finish event, else plugins hang.
        if (m_StartedEventSent)
        {
            CodeBlocksEvent evt(cbEVT_COMPILER_FINISHED, 0, m_pProject, nullptr, this);
            evt.SetInt(m_LastExitCode);
            Manager::Get()->ProcessEvent(evt);
            m_StartedEventSent = false;
        }
        m_LastExitCode = 0;
    }
}

wxString CompilerGCC::GetErrWarnStr()
{
#ifdef NO_TRANSLATION
    return wxString::Format(wxT("%u error%s, %u warning%s"),
                            m_Errors.GetCount(cltError),   wxString(m_Errors.GetCount(cltError)   == 1 ? wxT("") : wxT("s")).wx_str(),
                            m_Errors.GetCount(cltWarning), wxString(m_Errors.GetCount(cltWarning) == 1 ? wxT("") : wxT("s")).wx_str());
#else
    return wxString::Format(_("%u error(s), %u warning(s)"),
                            m_Errors.GetCount(cltError), m_Errors.GetCount(cltWarning));
#endif // NO_TRANSLATION
}

wxString CompilerGCC::GetMinSecStr()
{
    long int elapsed = (wxGetLocalTimeMillis() - m_StartTime).ToLong() / 1000;
    int mins =  elapsed / 60;
    int secs = (elapsed % 60);
#ifdef NO_TRANSLATION
    return wxString::Format(wxT("%d minute%s, %d second%s"),
                            mins, wxString(mins == 1 ? wxT("") : wxT("s")).wx_str(),
                            secs, wxString(secs == 1 ? wxT("") : wxT("s")).wx_str());
#else
    return wxString::Format(_("%d minute(s), %d second(s)"), mins, secs);
#endif // NO_TRANSLATION
}
