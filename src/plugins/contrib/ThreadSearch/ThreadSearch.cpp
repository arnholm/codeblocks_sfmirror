/***************************************************************
 * Name:      ThreadSearch
 * Purpose:   ThreadSearch Code::Blocks plugin
 *            Most of the interactions with C::B are handled here.
 * Author:    Jerome ANTOINE
 * Created:   2007-10-08
 * Copyright: Jerome ANTOINE
 * License:   GPL
 **************************************************************/

#include <sdk.h> // Code::Blocks SDK
#ifndef CB_PRECOMP
    #include <wx/combobox.h>
    #include <wx/menu.h>
    #include <wx/toolbar.h>
    #include <wx/xrc/xmlres.h>
    #include "cbeditor.h"
    #include "configmanager.h"
    #include "sdk_events.h"
#endif

#include "cbstyledtextctrl.h"
#include "editor_hooks.h"
#include "ThreadSearch.h"
#include "ThreadSearchView.h"
#include "ThreadSearchCommon.h"
#include "ThreadSearchConfPanel.h"
#include "ThreadSearchControlIds.h"
#include "ThreadSearchLoggerSTC.h"
#include "logging.h" //(pecan 2007/7/26)

// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
    PluginRegistrant<ThreadSearch> reg(_T("ThreadSearch"));
}

// ----------------------------------------------------------------------------
// CodeBlocks main.cpp managers all the following UI entries in one routine.
// So if only one changes, all will change. Therefore, to enable/disable copy/paste,
// we have to capture all the following to see if it really belongs to us
int idEditUndo = XRCID("idEditUndo");
int idEditRedo = XRCID("idEditRedo");
int idEditCopy = XRCID("idEditCopy");
int idEditCut = XRCID("idEditCut");
int idEditPaste = XRCID("idEditPaste");
int idEditSwapHeaderSource = XRCID("idEditSwapHeaderSource");
int idEditGotoMatchingBrace = XRCID("idEditGotoMatchingBrace");
int idEditBookmarks = XRCID("idEditBookmarks");
int idEditBookmarksToggle = XRCID("idEditBookmarksToggle");
int idEditBookmarksPrevious = XRCID("idEditBookmarksPrevious");
int idEditBookmarksNext = XRCID("idEditBookmarksNext");
int idEditFoldAll = XRCID("idEditFoldAll");
int idEditUnfoldAll = XRCID("idEditUnfoldAll");
int idEditToggleAllFolds = XRCID("idEditToggleAllFolds");
int idEditFoldBlock = XRCID("idEditFoldBlock");
int idEditUnfoldBlock = XRCID("idEditUnfoldBlock");
int idEditToggleFoldBlock = XRCID("idEditToggleFoldBlock");
int idEditEOLCRLF = XRCID("idEditEOLCRLF");
int idEditEOLCR = XRCID("idEditEOLCR");
int idEditEOLLF = XRCID("idEditEOLLF");
int idEditEncoding = XRCID("idEditEncoding");
int idEditSelectAll = XRCID("idEditSelectAll");
int idEditCommentSelected = XRCID("idEditCommentSelected");
int idEditUncommentSelected = XRCID("idEditUncommentSelected");
int idEditToggleCommentSelected = XRCID("idEditToggleCommentSelected");
int idEditAutoComplete = XRCID("idEditAutoComplete");
// ----------------------------------------------------------------------------

int idMenuEditCopy = XRCID("idEditCopy");
int idMenuEditPaste = XRCID("idEditPaste");

// events handling
BEGIN_EVENT_TABLE(ThreadSearch, cbPlugin)
    // add any events you want to handle here
    EVT_UPDATE_UI (controlIDs.Get(ControlIDs::idMenuViewThreadSearch),   ThreadSearch::OnMnuViewThreadSearchUpdateUI)
    EVT_MENU      (controlIDs.Get(ControlIDs::idMenuViewThreadSearch),   ThreadSearch::OnMnuViewThreadSearch)
    EVT_UPDATE_UI (controlIDs.Get(ControlIDs::idMenuViewFocusThreadSearch),   ThreadSearch::OnUpdateUISearchRunning)
    EVT_MENU      (controlIDs.Get(ControlIDs::idMenuViewFocusThreadSearch),   ThreadSearch::OnMnuViewFocusThreadSearch)
    EVT_UPDATE_UI (controlIDs.Get(ControlIDs::idMenuSearchThreadSearch), ThreadSearch::OnUpdateUISearchRunning)
    EVT_MENU      (controlIDs.Get(ControlIDs::idMenuSearchThreadSearch), ThreadSearch::OnMnuSearchThreadSearch)
    EVT_MENU      (controlIDs.Get(ControlIDs::idMenuCtxThreadSearch),    ThreadSearch::OnCtxThreadSearch)
    EVT_MENU      (idMenuEditCopy,           ThreadSearch::OnMnuEditCopy)
    EVT_UPDATE_UI (idMenuEditCopy,           ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_MENU      (idMenuEditPaste,          ThreadSearch::OnMnuEditPaste)
    EVT_TOOL      (controlIDs.Get(ControlIDs::idBtnOptions),             ThreadSearch::OnBtnOptionsClick)
    EVT_TOOL      (controlIDs.Get(ControlIDs::idBtnSearch),              ThreadSearch::OnBtnSearchClick)
    EVT_UPDATE_UI (controlIDs.Get(ControlIDs::idBtnSearch),              ThreadSearch::OnUpdateUIBtnSearch)
    EVT_TEXT_ENTER(controlIDs.Get(ControlIDs::idCboSearchExpr),          ThreadSearch::OnCboSearchExprEnter)
    EVT_TEXT      (controlIDs.Get(ControlIDs::idCboSearchExpr),          ThreadSearch::OnCboSearchExprEnter)
    EVT_TEXT_ENTER(controlIDs.Get(ControlIDs::idSearchDirPath),       ThreadSearch::OnCboSearchExprEnter)
    EVT_TEXT_ENTER(controlIDs.Get(ControlIDs::idSearchMask),          ThreadSearch::OnCboSearchExprEnter)
// ---------------------------------------------------------------------------
    // CodeBlocks main.cpp managers all the following UI entires in ONE routine.
    // So if only one changes, all may change.
    //Therefore, to enable/disable copy/paste, we have to capture all the following
    // to see if the event actually belongs to us.
    EVT_UPDATE_UI(idEditUndo, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditRedo, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditCopy, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditCut, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditPaste, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditSwapHeaderSource, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditGotoMatchingBrace, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditFoldAll, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditUnfoldAll, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditToggleAllFolds, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditFoldBlock, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditUnfoldBlock, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditToggleFoldBlock, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditEOLCRLF, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditEOLCR, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditEOLLF, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditEncoding, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditSelectAll, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksToggle, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksNext, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditBookmarksPrevious, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditCommentSelected, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditAutoComplete, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditUncommentSelected, ThreadSearch::OnMnuEditCopyUpdateUI)
    EVT_UPDATE_UI(idEditToggleCommentSelected, ThreadSearch::OnMnuEditCopyUpdateUI)
END_EVENT_TABLE()

// constructor
ThreadSearch::ThreadSearch()
             :m_SearchedWord(wxEmptyString),
              m_pThreadSearchView(nullptr),
              m_pViewManager(nullptr),
              m_pToolbar(nullptr),
              m_CtxMenuIntegration(true),
              m_UseDefValsForThreadSearch(true),
              m_ShowSearchControls(true),
              m_ShowDirControls(false),
              m_ShowCodePreview(true),
              m_DeletePreviousResults(true),
              m_LoggerType(ThreadSearchLoggerBase::TypeList),
              m_DisplayLogHeaders(true),
              m_DrawLogLines(false),
              m_AutosizeLogColumns(false),
              m_pCboSearchExpr(nullptr),
              m_SplitterMode(wxSPLIT_VERTICAL),
              m_FileSorting(InsertIndexManager::SortByFilePath),
              m_EditorHookId(-1)
{
}

// destructor
ThreadSearch::~ThreadSearch()
{
}

void ThreadSearch::CreateView(ThreadSearchViewManagerBase::eManagerTypes externalMgrType,
                              bool forceType)
{
    int sashPosition;
    ThreadSearchViewManagerBase::eManagerTypes mgrType;
    wxArrayString searchPatterns, searchDirs, searchMasks;

    // Loads configuration from default.conf
    LoadConfig(sashPosition, mgrType, searchPatterns, searchDirs, searchMasks);
    if (forceType)
        mgrType = externalMgrType;

    // Register the colours for the STC logger. We do this in case it is not the selected logger.
    ThreadSearchLoggerSTC::RegisterColours();

    // Adds window to the manager
    m_pThreadSearchView = new ThreadSearchView(*this);
    m_pThreadSearchView->SetSearchHistory(searchPatterns, searchDirs, searchMasks);

    // Sets splitter sash in the middle of the width of the window
    // and creates columns as it is not managed in ctor on Linux
    int x, y;
    m_pThreadSearchView->GetSize(&x, &y);
    m_pThreadSearchView->SetSashPosition(x/2);
    m_pThreadSearchView->Update();

    // Set the splitter posn from the config
    if (sashPosition != 0)
        m_pThreadSearchView->SetSashPosition(sashPosition);

    // Shows/Hides search widgets on the Messages notebook ThreadSearch panel
    m_pThreadSearchView->ShowSearchControls(m_ShowSearchControls);

    // Builds manager
    delete m_pViewManager;
    m_pViewManager = ThreadSearchViewManagerBase::BuildThreadSearchViewManagerBase(m_pThreadSearchView, true, mgrType);
    m_pViewManager->ShowView(ThreadSearchViewManagerBase::Show | ThreadSearchViewManagerBase::PreserveFocus);
}

void ThreadSearch::OnAttach()
{
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be TRUE...
    // You should check for it in other functions, because if it
    // is FALSE, it means that the application did *not* "load"
    // (see: does not need) this plugin...

    #if LOGGING
     wxLog::EnableLogging(true);
     m_pLog = new wxLogWindow(Manager::Get()->GetAppWindow(), _T(" ThreadSearch Plugin"), true, false);
     wxLog::SetActiveTarget( m_pLog);
     m_pLog->Flush();
     m_pLog->GetFrame()->SetSize(20,30,600,300);
     LOGIT( _T("ThreadSearch Plugin Logging Started"));
    #endif

    CreateView(ThreadSearchViewManagerBase::TypeMessagesNotebook, false);

    typedef cbEventFunctor<ThreadSearch, CodeBlocksEvent> Event;

    Manager::Get()->RegisterEventSink(cbEVT_SETTINGS_CHANGED,
                                      new Event(this, &ThreadSearch::OnSettingsChanged));
    EditorHooks::HookFunctorBase* hook = new EditorHooks::HookFunctor<ThreadSearch>(this, &ThreadSearch::OnEditorHook);
    m_EditorHookId = EditorHooks::RegisterHook(hook);

    // true if it enters in OnRelease for the first time
    m_OnReleased = false;
}

void ThreadSearch::OnRelease(bool /*appShutDown*/)
{
    // do de-initialization for your plugin
    // if appShutDown is false, the plugin is unloaded because Code::Blocks is being shut down,
    // which means you must not use any of the SDK Managers
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be FALSE...

    // --------------------------------------------------------------
    // Carefull! This routine can be entered consecutive times
    // --------------------------------------------------------------
    if (m_OnReleased)
        return;
    m_OnReleased = true;

    EditorHooks::UnregisterHook(m_EditorHookId, true);

    Manager::Get()->RemoveAllEventSinksFor(this);

    // Removes Thread search menu item from the View menu
    RemoveMenuItems();

    m_pToolbar = nullptr;

    if (m_pThreadSearchView != nullptr)
    {
        m_pViewManager->RemoveViewFromManager();
        m_pThreadSearchView = nullptr;
    }

    delete m_pViewManager;
    m_pViewManager = nullptr;
}

void ThreadSearch::OnThreadSearchViewDestruction()
{
    // Method is called from view destructor.
    // Destruction is either made by plugin or
    // Messages Notebook.

    // We show code preview to save a consistent
    // value of splitter sash position.
    m_pThreadSearchView->ApplySplitterSettings(m_ShowCodePreview, m_SplitterMode);

    // Saves configuration to default.conf
    SaveConfig(m_pThreadSearchView->GetSashPosition(),
               m_pThreadSearchView->GetSearchHistory(),
               m_pThreadSearchView->GetSearchDirsHistory(),
               m_pThreadSearchView->GetSearchMasksHistory());

    // Reset of the pointer as view is being deleted
    m_pThreadSearchView = nullptr;
}

void ThreadSearch::BuildMenu(wxMenuBar* menuBar)
{
    //The application is offering its menubar for your plugin,
    //to add any menu items you want...
    //Append any items you need in the menu...
    //NOTE: Be careful in here... The application's menubar is at your disposal.
    size_t i;
    int idx = menuBar->FindMenu(_("&View"));
    if (idx != wxNOT_FOUND)
    {
        wxMenu* menu = menuBar->GetMenu(idx);
        wxMenuItemList& items = menu->GetMenuItems();

        // find the first separator and insert before it
        for (i = 0; i < items.GetCount(); ++i)
        {
            if (items[i]->IsSeparator())
            {
                menu->InsertCheckItem(i, controlIDs.Get(ControlIDs::idMenuViewThreadSearch), _("Thread search"),
                                      _("Toggle displaying the 'Thread search' panel"));
                break;
            }
        }

        if ( i == items.GetCount() )
        {
            // not found, just append
            menu->AppendCheckItem(controlIDs.Get(ControlIDs::idMenuViewThreadSearch), _("Thread search"),
                                  _("Toggle displaying the 'Thread search' panel"));
        }

        menu->Append(controlIDs.Get(ControlIDs::idMenuViewFocusThreadSearch), _("Focus Thread Search"),
                     _("Makes the search box of the Thread search panel the focused control"));
    }

    idx = menuBar->FindMenu(_("Sea&rch"));
    if (idx != wxNOT_FOUND)
    {
        wxMenu* menu = menuBar->GetMenu(idx);
        wxMenuItemList& items = menu->GetMenuItems();

        // find the first separator and insert separator + entry before it
        for (i = 0; i < items.GetCount(); ++i)
        {
            if (items[i]->IsSeparator())
            {
                menu->Insert(i, controlIDs.Get(ControlIDs::idMenuSearchThreadSearch), _("Thread search"),
                                _("Perform a Threaded search with the current word"));
                menu->InsertSeparator(i);
                break;
            }
        }

        if ( i == items.GetCount() )
        {
            // not found, just append
            menu->Append(controlIDs.Get(ControlIDs::idMenuSearchThreadSearch), _("Thread search"),
                            _("Perform a Threaded search with the current word"));
            menu->AppendSeparator();
        }
    }
}

void ThreadSearch::RemoveMenuItems()
{
    // Removes 'Thread search' item from View and Search menu
    wxMenuBar* menuBar = Manager::Get()->GetAppFrame()->GetMenuBar();
    int idx = menuBar->FindMenu(_("&View"));
    if (idx != wxNOT_FOUND)
    {
        wxMenu* viewMenu = menuBar->GetMenu(idx);
        if ( viewMenu != 0 )
        {
            wxMenuItem *item = viewMenu->Remove(controlIDs.Get(ControlIDs::idMenuViewThreadSearch));
            delete item;
        }
    }

    idx = menuBar->FindMenu(_("Sea&rch"));
    if (idx != wxNOT_FOUND)
    {
        wxMenu* searchMenu = menuBar->GetMenu(idx);
        if ( searchMenu != 0 )
        {
            wxMenuItem *item = searchMenu->Remove(controlIDs.Get(ControlIDs::idMenuSearchThreadSearch));
            delete item;
        }
    }
}


void ThreadSearch::OnMnuViewThreadSearch(wxCommandEvent& event)
{
    if (!IsAttached())
        return;

    uint32_t flags = ThreadSearchViewManagerBase::None;
    if (event.IsChecked())
    {
        if (m_pThreadSearchView == nullptr)
        {
            // The view might have been destroyed if the notebook page has been removed.
            // So we need to recreate it.
            CreateView(ThreadSearchViewManagerBase::TypeMessagesNotebook, false);
            m_pThreadSearchView->SetToolBar(m_pToolbar);
            return;
        }
        flags |= ThreadSearchViewManagerBase::Show;
    }

    m_pViewManager->ShowView(flags);
}

void ThreadSearch::OnMnuSearchThreadSearch(wxCommandEvent& /*event*/)
{
    if (!IsAttached())
        return;

    // Need to get the cursor word first and ensure it is consistent.
    if ((GetCursorWord(m_SearchedWord) == true) && (m_SearchedWord.IsEmpty() == false))
    {
        // m_SearchedWord is Ok => Search
        RunThreadSearch(m_SearchedWord, true);
    }
    else
    {
        // Word is KO, just show the panel
        m_pViewManager->ShowView(ThreadSearchViewManagerBase::Show);
    }
}

void ThreadSearch::OnMnuViewFocusThreadSearch(wxCommandEvent& /*event*/)
{
    if (!IsAttached())
        return;

    GetCursorWord(m_SearchedWord);

    m_pViewManager->ShowView(ThreadSearchViewManagerBase::Show);
    m_pViewManager->Raise();
    m_pThreadSearchView->FocusSearchCombo(m_SearchedWord);
}


void ThreadSearch::OnCtxThreadSearch(wxCommandEvent& /*event*/)
{
    if ( !IsAttached() )
        return;

    // m_SearchedWord was set in BuildModuleMenu
    RunThreadSearch(m_SearchedWord, true);
}


void ThreadSearch::OnMnuViewThreadSearchUpdateUI(wxUpdateUIEvent& /*event*/)
{
    if ( !IsAttached() )
        return;

    wxMenuBar *menubar = Manager::Get()->GetAppFrame()->GetMenuBar();
    menubar->Check(controlIDs.Get(ControlIDs::idMenuViewThreadSearch), m_pViewManager->IsViewShown());
}

void ThreadSearch::OnUpdateUISearchRunning(wxUpdateUIEvent& event)
{
    if (!IsAttached())
        return;

    const bool isRunning = (m_pThreadSearchView && m_pThreadSearchView->IsSearchRunning());
    event.Enable(!isRunning);
}

void ThreadSearch::BuildModuleMenu(const ModuleType type, wxMenu* pMenu, const FileTreeData* /*data*/)
{
    wxMenuItem* pMenuItem = NULL;
    if (!pMenu || !IsAttached())
        return;

    // Triggs editor events if 'Find occurrences' is integrated in context menu
    if ( (type == mtEditorManager) && (m_CtxMenuIntegration == true) )
    {
        // Gets current word
        if ( GetCursorWord(m_SearchedWord) == true )
        {
            wxString sText = _("Find occurrences of: '") + m_SearchedWord + wxT("'");

            PluginManager *pluginManager = Manager::Get()->GetPluginManager();
            int dIndex = pluginManager->GetFindMenuItemFirst() + pluginManager->GetFindMenuItemCount();
            pMenuItem = pMenu->Insert(dIndex, controlIDs.Get(ControlIDs::idMenuCtxThreadSearch), sText);
            Manager::Get()->GetPluginManager()->RegisterFindMenuItems(false, 1);

            // Disables item if a threaded search is running
            pMenuItem->Enable(!m_pThreadSearchView->IsSearchRunning());
        }
    }
}

cbConfigurationPanel* ThreadSearch::GetConfigurationPanelEx(wxWindow* parent,
                                                            cbConfigurationPanelColoursInterface *coloursInterface)
{
    if (!IsAttached())
        return NULL;

    ThreadSearchConfPanel *panel = new ThreadSearchConfPanel(*this, coloursInterface, parent);

    if (m_pThreadSearchView)
    {
        panel->SetSearchAndMaskHistory(m_pThreadSearchView->GetSearchDirsHistory(),
                                       m_pThreadSearchView->GetSearchMasksHistory());
    }
    return panel;
}


void ThreadSearch::Notify()
{
    if ( !IsAttached() )
        return;

    m_pThreadSearchView->Update();
    SaveConfig(m_pThreadSearchView->GetSashPosition(),
               m_pThreadSearchView->GetSearchHistory(),
               m_pThreadSearchView->GetSearchDirsHistory(),
               m_pThreadSearchView->GetSearchMasksHistory());
}


void ThreadSearch::LoadConfig(int& sashPosition,
                              ThreadSearchViewManagerBase::eManagerTypes& mgrType,
                              wxArrayString& searchPatterns, wxArrayString& searchDirs,
                              wxArrayString& searchMasks)
{
    if ( !IsAttached() )
        return;

    ConfigManager* pCfg = Manager::Get()->GetConfigManager(_T("ThreadSearch"));

    m_FindData.SetMatchWord       (pCfg->ReadBool(wxT("/MatchWord"),             true));
    m_FindData.SetStartWord       (pCfg->ReadBool(wxT("/StartWord"),             false));
    m_FindData.SetMatchCase       (pCfg->ReadBool(wxT("/MatchCase"),             true));
    m_FindData.SetMatchInComments (pCfg->ReadBool(wxT("/MatchInComments"),       true));
    m_FindData.SetRegEx           (pCfg->ReadBool(wxT("/RegEx"),                 false));
    m_FindData.SetHiddenSearch    (pCfg->ReadBool(wxT("/HiddenSearch"),          true));
    m_FindData.SetRecursiveSearch (pCfg->ReadBool(wxT("/RecursiveSearch"),       true));

    m_CtxMenuIntegration         = pCfg->ReadBool(wxT("/CtxMenuIntegration"),    true);
    m_UseDefValsForThreadSearch  = pCfg->ReadBool(wxT("/UseDefaultValues"),      true);
    m_ShowSearchControls         = pCfg->ReadBool(wxT("/ShowSearchControls"),    true);
    m_ShowDirControls            = pCfg->ReadBool(wxT("/ShowDirControls"),       false);
    m_ShowCodePreview            = pCfg->ReadBool(wxT("/ShowCodePreview"),       false);
    m_DeletePreviousResults      = pCfg->ReadBool(wxT("/DeletePreviousResults"), false);
    m_DisplayLogHeaders          = pCfg->ReadBool(wxT("/DisplayLogHeaders"),     true);
    m_DrawLogLines               = pCfg->ReadBool(wxT("/DrawLogLines"),          false);
    m_AutosizeLogColumns         = pCfg->ReadBool(wxT("/AutosizeLogColumns"),    true);

    m_FindData.SetScope           (pCfg->ReadInt (wxT("/Scope"),                 ScopeProjectFiles));

    m_FindData.SetSearchPath      (pCfg->Read(wxT("/DirPath"), wxString()));
    m_FindData.SetSearchMask      (pCfg->Read(wxT("/Mask"), wxT("*.cpp;*.c;*.h")));

    wxArrayString fullList;
    pCfg->Read(wxT("/DirPathFullList"), &fullList);
    m_FindData.SetSearchPathFullList(fullList);

    sashPosition                 = pCfg->ReadInt(wxT("/SplitterPosn"),           0);
    int splitterMode             = pCfg->ReadInt(wxT("/SplitterMode"),           wxSPLIT_VERTICAL);
    m_SplitterMode               = wxSPLIT_VERTICAL;
    if ( splitterMode == wxSPLIT_HORIZONTAL )
    {
        m_SplitterMode = wxSPLIT_HORIZONTAL;
    }

    const int managerType = pCfg->ReadInt(wxT("/ViewManagerType"),
                                          ThreadSearchViewManagerBase::TypeMessagesNotebook);
    mgrType = ThreadSearchViewManagerBase::TypeMessagesNotebook;
    if (managerType == ThreadSearchViewManagerBase::TypeLayout)
    {
        mgrType = ThreadSearchViewManagerBase::TypeLayout;
    }

    const int loggerType = pCfg->ReadInt(wxT("/LoggerType"), ThreadSearchLoggerBase::TypeList);
    if (loggerType >= 0 && loggerType < ThreadSearchLoggerBase::TypeLast)
    {
        m_LoggerType = ThreadSearchLoggerBase::eLoggerTypes(loggerType);
    }
    else
    {
        m_LoggerType = ThreadSearchLoggerBase::TypeList;
    }

    searchPatterns = pCfg->ReadArrayString(wxT("/SearchPatterns"));
    searchDirs = pCfg->ReadArrayString(wxT("/SearchDirs"));
    if (searchDirs.empty())
        searchDirs.push_back(m_FindData.GetSearchPath());
    searchMasks = pCfg->ReadArrayString(wxT("/SearchMasks"));
    if (searchMasks.empty())
        searchMasks.push_back(m_FindData.GetSearchMask());
}


void ThreadSearch::SaveConfig(int sashPosition, const wxArrayString& searchPatterns,
                              const wxArrayString& searchDirs, const wxArrayString& searchMasks)
{
    ConfigManager* pCfg = Manager::Get()->GetConfigManager(_T("ThreadSearch"));

    pCfg->Write(wxT("/MatchWord"),             m_FindData.GetMatchWord());
    pCfg->Write(wxT("/StartWord"),             m_FindData.GetStartWord());
    pCfg->Write(wxT("/MatchCase"),             m_FindData.GetMatchCase());
    pCfg->Write(wxT("/MatchInComments"),       m_FindData.GetMatchInComments());
    pCfg->Write(wxT("/RegEx"),                 m_FindData.GetRegEx());
    pCfg->Write(wxT("/HiddenSearch"),          m_FindData.GetHiddenSearch());
    pCfg->Write(wxT("/RecursiveSearch"),       m_FindData.GetRecursiveSearch());

    pCfg->Write(wxT("/CtxMenuIntegration"),    m_CtxMenuIntegration);
    pCfg->Write(wxT("/UseDefaultValues"),      m_UseDefValsForThreadSearch);
    pCfg->Write(wxT("/ShowSearchControls"),    m_ShowSearchControls);
    pCfg->Write(wxT("/ShowDirControls"),       m_ShowDirControls);
    pCfg->Write(wxT("/ShowCodePreview"),       m_ShowCodePreview);
    pCfg->Write(wxT("/DeletePreviousResults"), m_DeletePreviousResults);
    pCfg->Write(wxT("/DisplayLogHeaders"),     m_DisplayLogHeaders);
    pCfg->Write(wxT("/DrawLogLines"),          m_DrawLogLines);
    pCfg->Write(wxT("/AutosizeLogColumns"),    m_AutosizeLogColumns);

    pCfg->Write(wxT("/Scope"),                 m_FindData.GetScope());

    pCfg->Write(wxT("/DirPath"),               m_FindData.GetSearchPath());
    pCfg->Write("/DirPathFullList",            m_FindData.GetSearchPathFullList());

    pCfg->Write(wxT("/Mask"),                  m_FindData.GetSearchMask());

    pCfg->Write(wxT("/SplitterPosn"),          sashPosition);
    pCfg->Write(wxT("/SplitterMode"),          (int)m_SplitterMode);
    pCfg->Write(wxT("/ViewManagerType"),       m_pViewManager->GetManagerType());
    pCfg->Write(wxT("/LoggerType"),            m_LoggerType);
    pCfg->Write(wxT("/FileSorting"),           m_FileSorting);

    pCfg->Write(wxT("/SearchPatterns"),        searchPatterns);
    pCfg->Write(wxT("/SearchDirs"),            searchDirs);
    pCfg->Write(wxT("/SearchMasks"),           searchMasks);
}

bool ThreadSearch::BuildToolBar(wxToolBar* toolBar)
{
    if ( !IsAttached() || !toolBar )
        return false;

    m_pToolbar = toolBar;
    m_pThreadSearchView->SetToolBar(toolBar);

    const wxString &prefix = GetImagePrefix(true);

    wxSize textSize = Manager::Get()->GetAppWindow()->GetTextExtent(wxString(wxT('A'), 20));
    textSize.y = -1;
    textSize.x = std::max(textSize.x, 200);
    m_pCboSearchExpr = new wxComboBox(toolBar, controlIDs.Get(ControlIDs::idCboSearchExpr),
                                      wxEmptyString, wxDefaultPosition, textSize, 0, nullptr,
                                      wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    m_pCboSearchExpr->SetToolTip(_("Text to search"));

    const double scaleFactor = cbGetContentScaleFactor(*toolBar);

    wxBitmap bmpFind = cbLoadBitmapScaled(prefix + wxT("findf.png"), wxBITMAP_TYPE_PNG,
                                          scaleFactor);
    wxBitmap bmpFindDisabled = cbLoadBitmapScaled(prefix + wxT("findfdisabled.png"),
                                                  wxBITMAP_TYPE_PNG, scaleFactor);
    wxBitmap bmpOptions = cbLoadBitmapScaled(prefix + wxT("options.png"), wxBITMAP_TYPE_PNG,
                                             scaleFactor);
    wxBitmap bmpOptionsDisabled = cbLoadBitmapScaled(prefix + wxT("optionsdisabled.png"),
                                                     wxBITMAP_TYPE_PNG, scaleFactor);

    toolBar->AddControl(m_pCboSearchExpr);
    toolBar->AddTool(controlIDs.Get(ControlIDs::idBtnSearch), wxString(),
                     bmpFind, bmpFindDisabled,
                     wxITEM_NORMAL, _("Run search"));
    toolBar->AddTool(controlIDs.Get(ControlIDs::idBtnOptions), wxString(),
                     bmpOptions, bmpOptionsDisabled,
                     wxITEM_NORMAL, _("Show options window"));
    m_pThreadSearchView->UpdateOptionsButtonImage(m_FindData);

    m_pCboSearchExpr->Append(m_pThreadSearchView->GetSearchHistory());
    if (m_pCboSearchExpr->GetCount() > 0)
    {
        m_pCboSearchExpr->SetSelection(0);
    }

    toolBar->Realize();
    toolBar->SetInitialSize();

    return true;
}


void ThreadSearch::OnBtnOptionsClick(wxCommandEvent &event)
{
    if ( !IsAttached() )
        return;

    m_pThreadSearchView->OnBtnOptionsClick(event);
}


void ThreadSearch::OnBtnSearchClick(wxCommandEvent &event)
{
    if ( !IsAttached() )
        return;

    // Behaviour differs if a search is running.
    if ( m_pThreadSearchView->IsSearchRunning() )
    {
        // In this case, user wants to stops search,
        // we just transmit event
        m_pThreadSearchView->OnBtnSearchClick(event);

    }
    else
    {
        // User wants to search for a word.
        // Forwarding the event would search for the view combo text whereas we want
        // to look for the toolbar combo text.
        const long id = controlIDs.Get(ControlIDs::idCboSearchExpr);
        wxComboBox* pCboBox = static_cast<wxComboBox*>(m_pToolbar->FindControl(id));
        wxASSERT(pCboBox != NULL);

        wxString searchValue = pCboBox->GetValue();
        if(searchValue.empty())
        {
            // If the user string is empty, we use the last searched string from the combo box
            const wxArrayString& strings = pCboBox->GetStrings();
            if(strings.size() == 0)
                return;
            searchValue = strings.Item(0);
            pCboBox->SetValue(searchValue);
        }
        RunThreadSearch(searchValue);
    }
}

void ThreadSearch::OnUpdateUIBtnSearch(wxUpdateUIEvent &event)
{
    if (!m_pToolbar)
        return;
    const long id = controlIDs.Get(ControlIDs::idCboSearchExpr);
    wxComboBox* comboBox = static_cast<wxComboBox*>(m_pToolbar->FindControl(id));
    if (comboBox)
    {
        // If the combo box has old search values we enable the search button.
        // if the user searches with an empty comboBox we simply repeat the last search
        event.Enable(comboBox->GetStrings().size() > 0);
    }
}

void ThreadSearch::RunThreadSearch(const wxString& text, bool isCtxSearch/*=false*/)
{
    if ( !IsAttached() )
        return;

    ThreadSearchFindData findData = m_FindData;

    // User may prefer to set default options for contextual search
    if ( (isCtxSearch == true) && (m_UseDefValsForThreadSearch == true) )
    {
        findData.SetMatchCase(true);
        findData.SetMatchWord(true);
        findData.SetStartWord(false);
        findData.SetMatchInComments(true);
        findData.SetRegEx    (false);
    }

    // m_SearchedWord was set in BuildModuleMenu
    findData.SetFindText(text);

    // Displays m_pThreadSearchView in manager
    m_pViewManager->ShowView(ThreadSearchViewManagerBase::Show | ThreadSearchViewManagerBase::PreserveFocus);

    // Runs the search through a worker thread
    m_pThreadSearchView->ThreadedSearch(findData);
}


void ThreadSearch::OnCboSearchExprEnter(wxCommandEvent &event)
{
    if (!IsAttached())
        return;
    if (event.GetEventType() != wxEVT_COMMAND_TEXT_ENTER)
        return;

    // Event handler used when user clicks on enter after typing
    // in combo box text control.
    // Runs a multi threaded search with combo text
    const long id = controlIDs.Get(ControlIDs::idCboSearchExpr);
    wxComboBox* pCboBox = static_cast<wxComboBox*>(m_pToolbar->FindControl(id));
    wxASSERT(pCboBox != NULL);

    const wxString &value = pCboBox->GetValue();
    if (!value.empty())
        RunThreadSearch(value);
}


void ThreadSearch::ShowToolBar(bool show)
{
    if ( !IsAttached() )
        return;

    bool isShown = IsWindowReallyShown(m_pToolbar);

    if ( show != isShown )
    {
        CodeBlocksDockEvent evt(show ? cbEVT_SHOW_DOCK_WINDOW : cbEVT_HIDE_DOCK_WINDOW);
        evt.pWindow = (wxWindow*)m_pToolbar;
        evt.shown = show;
        Manager::Get()->ProcessEvent(evt);
    }
}


bool ThreadSearch::IsToolbarVisible()
{
    if ( !IsAttached() )
        return false;

    return IsWindowReallyShown(m_pToolbar);
}


bool ThreadSearch::GetCursorWord(wxString& sWord)
{
    bool wordFound = false;
    sWord = wxEmptyString;

    // Gets active editor
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if ( ed != NULL )
    {
        cbStyledTextCtrl* control = ed->GetControl();

        sWord = control->GetSelectedText();
        if (sWord != wxEmptyString)
        {
            sWord.Trim(true);
            sWord.Trim(false);

            wxString::size_type pos = sWord.find(wxT('\n'));
            if (pos != wxString::npos)
            {
                sWord.Remove(pos, sWord.length() - pos);
                sWord.Trim(true);
                sWord.Trim(false);
            }

            return !sWord.IsEmpty();
        }

        // Gets word under cursor
        int pos = control->GetCurrentPos();
        int ws  = control->WordStartPosition(pos, true);
        int we  = control->WordEndPosition(pos, true);
        const wxString word = control->GetTextRange(ws, we);
        if (!word.IsEmpty()) // Avoid empty strings
        {
            sWord.Clear();

            // m_SearchedWord will be used if 'Find occurrences' ctx menu is clicked
            sWord << word;
            wordFound = true;
        }
    }

    return wordFound;
}


void ThreadSearch::OnMnuEditCopy(wxCommandEvent& event)
{
       if ( !IsAttached() )
    {
        event.Skip();
        return;
    }

    wxWindow* pFocused = wxWindow::FindFocus();

    // if the following window have the focus, own the copy.
    if ( pFocused == m_pCboSearchExpr )
    {
        if ( m_pCboSearchExpr->CanCopy() )
            m_pCboSearchExpr->Copy();
        LOGIT( _T("OnMnuEditcopy for m_pCboSearchExpr") );
    }
    else if ( pFocused == m_pThreadSearchView->m_pCboSearchExpr )
    {
        if ( m_pThreadSearchView->m_pCboSearchExpr->CanCopy() )
            m_pThreadSearchView->m_pCboSearchExpr->Copy();
        LOGIT( _T("OnMnuEditcopy for m_pThreadSearchView->m_pCboSearchExpr") );
    }
    else if ( pFocused == static_cast<wxWindow*>(m_pThreadSearchView->m_pSearchPreview) )
    {
        bool hasSel = m_pThreadSearchView->m_pSearchPreview->GetSelectionStart() != m_pThreadSearchView->m_pSearchPreview->GetSelectionEnd();
        if (hasSel)
            m_pThreadSearchView->m_pSearchPreview->Copy();
        LOGIT( _T("OnMnuEditcopy for m_pSearchPreview") );
    }
    else
    {
        event.Skip();
    }

    // If you Skip(), CB main.cpp will wrongly paste your text into the current editor
    // because CB main.cpp thinks it owns the clipboard.
    //- event.Skip();
    return; //own the event
}


void ThreadSearch::OnMnuEditCopyUpdateUI(wxUpdateUIEvent& event)
{
    if ( !IsAttached() )
    {
        event.Skip(); return;
    }

    wxWindow* pFocused = wxWindow::FindFocus();
    if (not pFocused) return;

    wxMenuBar* mbar = Manager::Get()->GetAppFrame()->GetMenuBar();
    if (not mbar) return;

    bool hasSel = false;
    // if the following window have the focus, own the copy.
    if ( pFocused == m_pCboSearchExpr )
    {
        //event.Enable(m_pCboSearchExpr->CanCopy());
        hasSel =  m_pCboSearchExpr->CanCopy() ;
        //LOGIT( _T("OnMnuEditCopyUpdateUI m_pCboSearchExpr") );
    }
    else if (m_pThreadSearchView)
    {
        if ( pFocused == m_pThreadSearchView->m_pCboSearchExpr )
        {
            //event.Enable(m_pThreadSearchView->m_pCboSearchExpr->CanCopy());
            hasSel = m_pThreadSearchView->m_pCboSearchExpr->CanCopy();
            //LOGIT( _T("OnMnuEditCopyUpdateUI m_pThreadSearchView->m_pCboSearchExpr") );
        }
        else if ( pFocused == static_cast<wxWindow*>(m_pThreadSearchView->m_pSearchPreview) )
        {
            hasSel = m_pThreadSearchView->m_pSearchPreview->GetSelectionStart() != m_pThreadSearchView->m_pSearchPreview->GetSelectionEnd();
            //LOGIT( _T("OnMnuEditCopyUpdateUI m_pSearchPreview") );
        }
    }

    if ( hasSel )
    {
        mbar->Enable(idMenuEditCopy, hasSel);
        wxToolBar* pMainToolBar = (wxToolBar*) ::wxFindWindowByName(wxT("toolbar"), NULL);
        if (pMainToolBar) pMainToolBar->EnableTool(idMenuEditCopy, hasSel);
        return;
    }

    event.Skip();
    return;

}
// ----------------------------------------------------------------------------
void ThreadSearch::OnMnuEditPaste(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // Process clipboard data only if we have the focus

    // ----------------------------------------------------------------
    // NB:  A bug in CB main.cpp causes a ctrl-v to always paste into the
    //      current editor. Here, we'll make checks to see if the paste
    //      is for our search combo boxes and paste the data there.
    //      If the focused window is one of ours that shouldn't get pasted
    //      data, we'll simply ignore it.
    //      If the window isn't one of ours, we'll event.Skip();
    // ----------------------------------------------------------------

       if ( !IsAttached() )
        { event.Skip(); return; }

    if (not m_IsAttached) {event.Skip(); return;}

    wxWindow* pFocused = wxWindow::FindFocus();
    if (not pFocused) { event.Skip(); return; }

//    DBGLOG(wxT("OnMnuEditPaste:Focused[%p][%s]"), pFocused, pFocused->GetName());

    // don't allow paste when the following windows have the focus
    if ( (pFocused == m_pThreadSearchView->m_pSearchPreview) ||
         (pFocused == (wxWindow*)m_pThreadSearchView->m_pLogger) )
    {
        return;
    }

    // if the following window have the focus, own the paste.
    if ( (pFocused != m_pCboSearchExpr)
        && (pFocused != m_pThreadSearchView->m_pCboSearchExpr) )
        { event.Skip(); return;}

    if (pFocused == m_pCboSearchExpr)
        m_pCboSearchExpr->Paste();
    if (pFocused == m_pThreadSearchView->m_pCboSearchExpr)
        m_pThreadSearchView->m_pCboSearchExpr->Paste();

    // If you Skip(), CB main.cpp will wrongly paste your text into the current editor
    // because CB main.cpp thinks it owns the clipboard.
    //- event.Skip();
    return; //own the event
}//OnMnuEditPaste


void ThreadSearch::SetManagerType(ThreadSearchViewManagerBase::eManagerTypes mgrType)
{
    // Is type different from current one ?
    if (mgrType != m_pViewManager->GetManagerType())
    {
        // Destroy current view manager.
        if (m_pViewManager != nullptr)
        {
            m_pViewManager->RemoveViewFromManager();
            delete m_pViewManager;
            m_pViewManager = nullptr;
        }

        CreateView(mgrType, true);
        m_pThreadSearchView->SetToolBar(m_pToolbar);
    }
}

void ThreadSearch::OnSettingsChanged(CodeBlocksEvent &event)
{
    if (event.GetInt() == int(cbSettingsType::Environment) && m_pThreadSearchView)
    {
        m_pThreadSearchView->UpdateSettings();
    }
}

void ThreadSearch::OnEditorHook(cbEditor *editor, wxScintillaEvent &event)
{
    if (m_pThreadSearchView == nullptr)
        return;

    const int modificationType = event.GetModificationType();
    if ((modificationType & (wxSCI_MOD_INSERTTEXT | wxSCI_MOD_DELETETEXT)) == 0)
        return;
    const int linesAdded = event.GetLinesAdded();
    if (linesAdded == 0)
        return;
    cbStyledTextCtrl *control = editor->GetControl();
    if (control != event.GetEventObject())
        return;
    const int startLine = control->LineFromPosition(event.GetPosition());
    m_pThreadSearchView->EditorLinesAddedOrRemoved(editor, startLine + 1, linesAdded);
}
