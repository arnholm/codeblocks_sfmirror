/***************************************************************
 * Name:      ThreadSearchView
 * Purpose:   This class implements the panel that is added to
 *            Code::Blocks Message notebook or C::B layout.
 * Author:    Jerome ANTOINE
 * Created:   2007-10-08
 * Copyright: Jerome ANTOINE
 * License:   GPL
 **************************************************************/

#include "sdk.h"
#include <wx/bitmap.h>
#include <wx/bmpbuttn.h>
#include <wx/display.h>
#include <wx/statline.h>
#ifndef CB_PRECOMP
    #include <wx/combobox.h>
    #include <wx/menu.h>
    #include <wx/sizer.h>
    #include <wx/splitter.h>
    #include <wx/statbox.h>
    #include <wx/stattext.h>
    #include <wx/settings.h>
    #include <wx/toolbar.h>

    #include "cbeditor.h"
    #include "configmanager.h"
    #include "editorcolourset.h"
    #include "infowindow.h"
    #include "logmanager.h"
#endif

#include "cbstyledtextctrl.h"
#include "editor_utils.h"
#include "encodingdetector.h"
#include "SearchInPanel.h"
#include "DirectoryParamsPanel.h"
#include "ThreadSearch.h"
#include "ThreadSearchView.h"
#include "ThreadSearchEvent.h"
#include "ThreadSearchThread.h"
#include "ThreadSearchFindData.h"
#include "ThreadSearchCommon.h"
#include "ThreadSearchConfPanel.h"
#include "ThreadSearchControlIds.h"
#include "wx/tglbtn.h"

// Timer value for events handling (events sent by worker thread)
const int TIMER_PERIOD = 100;

ThreadSearchView::ThreadSearchView(ThreadSearch& threadSearchPlugin) :
    wxPanel(Manager::Get()->GetAppWindow()),
    m_ThreadSearchPlugin(threadSearchPlugin),
    m_Timer(this, controlIDs.Get(ControlIDs::idTmrListCtrlUpdate)),
    m_StoppingThread(0),
    m_LastFocusedWindow(nullptr)
{
    m_pFindThread = nullptr;
    m_pToolBar = nullptr;

    // Create icons 1, 2 and 8 in the message pane

    // begin wxGlade: ThreadSearchView::ThreadSearchView
    m_pSplitter = new wxSplitterWindow(this, -1, wxDefaultPosition, wxSize(1,1), wxSP_3D|wxSP_BORDER|wxSP_PERMIT_UNSPLIT);
    m_pPnlPreview = new wxPanel(m_pSplitter, -1, wxDefaultPosition, wxSize(1,1));
    m_pSizerSearchDirItems_staticbox = new wxStaticBox(this, -1, _("Directory parameters"));
    m_pCboSearchExpr = new wxComboBox(this, controlIDs.Get(ControlIDs::idCboSearchExpr), wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                      wxCB_DROPDOWN|wxTE_PROCESS_ENTER);

    m_pPnlSearchIn = new SearchInPanel(this, -1);
    const wxSize butSize(m_pPnlSearchIn->GetButtonSize());

#if wxCHECK_VERSION(3, 1, 6)
    const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
    const wxSize bmpSize(16, 16);
#else
    const wxString prefix(GetImagePrefix(false, Manager::Get()->GetAppWindow()));
#endif

    m_pBtnSearch = new wxButton(this, controlIDs.Get(ControlIDs::idBtnSearch), wxEmptyString, wxDefaultPosition, butSize);
#if wxCHECK_VERSION(3, 1, 6)
    m_pBtnSearch->SetBitmapLabel(cbLoadBitmapBundleFromSVG(prefix+"findf.svg", bmpSize));
#else
    m_pBtnSearch->SetBitmapLabel(cbLoadBitmap(prefix+"findf.png"));
#endif

    m_pBtnOptions = new wxButton(this, controlIDs.Get(ControlIDs::idBtnOptions), wxEmptyString, wxDefaultPosition, butSize);
#if wxCHECK_VERSION(3, 1, 6)
    m_pBtnOptions->SetBitmapLabel(cbLoadBitmapBundleFromSVG(prefix+"options.svg", bmpSize));
#else
    m_pBtnOptions->SetBitmapLabel(cbLoadBitmap(prefix+"options.png"));
#endif

    m_pStaticLine1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL);
    m_pStaTxtSearchIn = new wxStaticText(this, -1, _("Search in "));
    m_pStaticLine2 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL);

    m_pBtnShowDirItems = new wxButton(this, controlIDs.Get(ControlIDs::idBtnShowDirItemsClick), wxEmptyString, wxDefaultPosition, butSize);
#if wxCHECK_VERSION(3, 1, 6)
    m_pBtnShowDirItems->SetBitmapLabel(cbLoadBitmapBundleFromSVG(prefix+"showdir.svg", bmpSize));
#else
    m_pBtnShowDirItems->SetBitmapLabel(cbLoadBitmap(prefix+"showdir.png"));
#endif

    m_pPnlDirParams = new DirectoryParamsPanel(&threadSearchPlugin.GetFindData(), this, -1);
    m_pSearchPreview = new cbStyledTextCtrl(m_pPnlPreview, wxID_ANY, wxDefaultPosition, wxSize(1,1));
    m_pLogger = ThreadSearchLoggerBase::Build(*this, m_ThreadSearchPlugin,
                                              m_ThreadSearchPlugin.GetLoggerType(),
                                              m_ThreadSearchPlugin.GetFileSorting(), m_pSplitter,
                                              controlIDs.Get(ControlIDs::idWndLogger));

    set_properties();
    do_layout();
    // end wxGlade

    // Dynamic event connections.
    // Static events table (BEGIN_EVENT_TABLE) doesn't work for all events
    long id = m_pSearchPreview->GetId();
    Connect(id, wxEVT_SCI_MARGINCLICK,
            (wxObjectEventFunction) (wxEventFunction) (wxScintillaEventFunction)
            &ThreadSearchView::OnMarginClick);

    Connect(id, wxEVT_CONTEXT_MENU,
            (wxObjectEventFunction) (wxEventFunction) (wxContextMenuEventFunction)
            &ThreadSearchView::OnContextMenu);

    Connect(wxEVT_THREAD_SEARCH_ERROR,
            (wxObjectEventFunction)&ThreadSearchView::OnThreadSearchErrorEvent);

    m_pPnlDirParams->Enable(m_pPnlSearchIn->GetSearchInDirectory());
}

ThreadSearchView::~ThreadSearchView()
{
    if (m_pFindThread)
        StopThread();

    // I don't know if it is necessary to remove event connections
    // so I do it myself
    const long id = m_pSearchPreview->GetId();
    Disconnect(id, wxEVT_SCI_MARGINCLICK,
            (wxObjectEventFunction) (wxEventFunction) (wxScintillaEventFunction)
            &ThreadSearchView::OnMarginClick);

    Disconnect(id, wxEVT_CONTEXT_MENU,
            (wxObjectEventFunction) (wxEventFunction) (wxContextMenuEventFunction)
            &ThreadSearchView::OnContextMenu);

    Disconnect(wxEVT_THREAD_SEARCH_ERROR,
            (wxObjectEventFunction)&ThreadSearchView::OnThreadSearchErrorEvent);

    m_ThreadSearchPlugin.OnThreadSearchViewDestruction();

    delete m_pLogger;
    m_pLogger = nullptr;
}

// As SearchInPanel and DirectoryParamsPanel are generic, their
// events are managed by parent ie this ThreadSearchView class.
BEGIN_EVENT_TABLE(ThreadSearchView, wxPanel)
    // begin wxGlade: ThreadSearchView::event_table
    EVT_TEXT_ENTER(controlIDs.Get(ControlIDs::idCboSearchExpr), ThreadSearchView::OnCboSearchExprEnter)
    EVT_TEXT_ENTER(controlIDs.Get(ControlIDs::idSearchDirPath), ThreadSearchView::OnCboSearchExprEnter)
    EVT_TEXT_ENTER(controlIDs.Get(ControlIDs::idSearchMask), ThreadSearchView::OnCboSearchExprEnter)
    EVT_BUTTON(controlIDs.Get(ControlIDs::idBtnSearch), ThreadSearchView::OnBtnSearchClick)
    EVT_BUTTON(controlIDs.Get(ControlIDs::idBtnOptions), ThreadSearchView::OnBtnOptionsClick)

    EVT_MENU(controlIDs.Get(ControlIDs::idOptionDialog), ThreadSearchView::OnShowOptionsDialog)
    EVT_MENU(controlIDs.Get(ControlIDs::idOptionWholeWord), ThreadSearchView::OnQuickOptions)
    EVT_MENU(controlIDs.Get(ControlIDs::idOptionStartWord), ThreadSearchView::OnQuickOptions)
    EVT_MENU(controlIDs.Get(ControlIDs::idOptionMatchCase), ThreadSearchView::OnQuickOptions)
    EVT_MENU(controlIDs.Get(ControlIDs::idOptionMatchInComments), ThreadSearchView::OnQuickOptions)
    EVT_MENU(controlIDs.Get(ControlIDs::idOptionRegEx), ThreadSearchView::OnQuickOptions)
    EVT_MENU(controlIDs.Get(ControlIDs::idOptionResetAll), ThreadSearchView::OnQuickOptions)

    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idBtnSearch), ThreadSearchView::OnQuickOptionsUpdateUI)
    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idOptionWholeWord), ThreadSearchView::OnQuickOptionsUpdateUI)
    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idOptionStartWord), ThreadSearchView::OnQuickOptionsUpdateUI)
    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idOptionMatchCase), ThreadSearchView::OnQuickOptionsUpdateUI)
    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idOptionMatchInComments), ThreadSearchView::OnQuickOptionsUpdateUI)
    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idOptionRegEx), ThreadSearchView::OnQuickOptionsUpdateUI)
    EVT_UPDATE_UI(controlIDs.Get(ControlIDs::idOptionResetAll), ThreadSearchView::OnQuickOptionsUpdateUI)

    EVT_BUTTON(controlIDs.Get(ControlIDs::idBtnShowDirItemsClick), ThreadSearchView::OnBtnShowDirItemsClick)
    EVT_SPLITTER_DCLICK(-1, ThreadSearchView::OnSplitterDoubleClick)
    // end wxGlade

    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchOpenFiles),      ThreadSearchView::OnBtnSearchOpenFiles)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchTargetFiles),    ThreadSearchView::OnBtnSearchTargetFiles)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchProjectFiles),   ThreadSearchView::OnBtnSearchProjectFiles)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchWorkspaceFiles), ThreadSearchView::OnBtnSearchWorkspaceFiles)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchDirectoryFiles), ThreadSearchView::OnBtnSearchDirectoryFiles)

    EVT_TIMER(controlIDs.Get(ControlIDs::idTmrListCtrlUpdate),          ThreadSearchView::OnTmrListCtrlUpdate)
END_EVENT_TABLE()

void ThreadSearchView::OnThreadSearchErrorEvent(const ThreadSearchEvent& event)
{
    Manager::Get()->GetLogManager()->Log(wxString::Format("ThreadSearch: %s", event.GetString()));
    InfoWindow::Display(_("Thread Search Error"), event.GetString());
}

void ThreadSearchView::OnCboSearchExprEnter(wxCommandEvent &/*event*/)
{
    // Event handler used when user clicks on enter after typing
    // in combo box text control.
    // Runs a multi threaded search.

    wxString value = m_pCboSearchExpr->GetValue();
    if (value.empty())
    {
        // If the value of the combo box is empty we search for the last
        // searched string and use it instead
        const wxArrayString& strings = m_pCboSearchExpr->GetStrings();
        if (strings.size() == 0)
            return;

        value = strings.Item(0);
        m_pCboSearchExpr->SetValue(value);
    }

    ThreadSearchFindData findData = m_ThreadSearchPlugin.GetFindData();
    findData.SetFindText(value);
    ThreadedSearch(findData);
}

void ThreadSearchView::OnBtnSearchClick(wxCommandEvent &/*event*/)
{
    // User clicked on Search/Cancel
    // m_ThreadSearchEventsArray is shared by two threads, we
    // use m_MutexSearchEventsArray to have a safe access.
    // As button action depends on m_ThreadSearchEventsArray,
    // we lock the mutex to process it correctly.
    if (m_MutexSearchEventsArray.Lock() == wxMUTEX_NO_ERROR)
    {
        const size_t nbEvents = m_ThreadSearchEventsArray.GetCount();
        m_MutexSearchEventsArray.Unlock();
        if (m_pFindThread != nullptr)
        {
            // A threaded search is running...
            UpdateSearchButtons(false);
            StopThread();
        }
        else if (nbEvents)
        {
            // A threaded search has run but the events array is
            // not completely processed...
            UpdateSearchButtons(false);
            if (!ClearThreadSearchEventsArray())
                cbMessageBox(_("Failed to clear events array."), _("Error"), wxICON_ERROR);
        }
        else
        {
            // We start the thread search
            ThreadSearchFindData findData = m_ThreadSearchPlugin.GetFindData();
            wxString value = m_pCboSearchExpr->GetValue();
            if (value.empty())
            {
                // if the search value is empty we check if the search history is >0 and use the last searched
                // word to repeat the search
                const wxArrayString& strings = m_pCboSearchExpr->GetStrings();
                if (strings.IsEmpty())
                    return;

                value = strings.Item(0);
                m_pCboSearchExpr->SetValue(value);
            }

            findData.SetFindText(value);
            ThreadedSearch(findData);
        }
    }
}

void ThreadSearchView::OnBtnOptionsClick(wxCommandEvent &/*event*/)
{
    wxMenu menu;
    menu.Append(controlIDs.Get(ControlIDs::idOptionDialog), _("Options"), _("Shows the options dialog"));
    menu.AppendSeparator();
    menu.AppendCheckItem(controlIDs.Get(ControlIDs::idOptionWholeWord),
                         _("Whole word"), _("Search text matches only whole words"));
    menu.AppendCheckItem(controlIDs.Get(ControlIDs::idOptionStartWord),
                         _("Start word"), _("Matches only word starting with search expression"));
    menu.AppendCheckItem(controlIDs.Get(ControlIDs::idOptionMatchCase), _("Match case"), _("Case sensitive search."));
    menu.AppendCheckItem(controlIDs.Get(ControlIDs::idOptionMatchInComments), _("Match in C++ style comments"), _("Also searches in C++ style comments ('//')"));
    menu.AppendCheckItem(controlIDs.Get(ControlIDs::idOptionRegEx),
                         _("Regular expression"), _("Search expression is a regular expression"));
    menu.AppendSeparator();
    menu.Append(controlIDs.Get(ControlIDs::idOptionResetAll), _("Reset All"),
                _("Resets all options"));

    PopupMenu(&menu);
}

void ThreadSearchView::OnShowOptionsDialog(wxCommandEvent &/*event*/)
{
    // Displays a dialog box with a ThreadSearchConfPanel.
    // All parameters can be set on this dialog.
    // It is the same as doing 'Settings/environment/Thread search'
    // Settings are updated by the cbConfigurationDialog
    cbConfigurationDialog dialog(Manager::Get()->GetAppWindow(), -1, _("Options"));
    ThreadSearchConfPanel* pConfPanel = new ThreadSearchConfPanel(m_ThreadSearchPlugin, nullptr, &dialog);
    pConfPanel->SetSearchAndMaskHistory(GetSearchDirsHistory(), GetSearchMasksHistory());
    dialog.AttachConfigurationPanel(pConfPanel);
    PlaceWindow(&dialog);
    if (dialog.ShowModal() == wxID_OK)
        UpdateSettings();
}

void ThreadSearchView::OnQuickOptions(wxCommandEvent &event)
{
    ThreadSearchFindData findData = m_ThreadSearchPlugin.GetFindData();
    bool hasChange = false;
    if (event.GetId() == controlIDs.Get(ControlIDs::idOptionWholeWord))
    {
        findData.SetMatchWord(event.IsChecked());
        hasChange = true;
    }
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionStartWord))
    {
        findData.SetStartWord(event.IsChecked());
        hasChange = true;
    }
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionMatchCase))
    {
        findData.SetMatchCase(event.IsChecked());
        hasChange = true;
    }
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionMatchInComments))
    {
        findData.SetMatchInComments(event.IsChecked());
        hasChange = true;
    }
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionRegEx))
    {
        findData.SetRegEx(event.IsChecked());
        hasChange = true;
    }
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionResetAll))
    {
        findData.SetMatchWord(false);
        findData.SetStartWord(false);
        findData.SetMatchCase(false);
        findData.SetMatchInComments(false);
        findData.SetRegEx(false);
        hasChange = true;
    }

    if (hasChange)
    {
        m_ThreadSearchPlugin.SetFindData(findData);
        UpdateOptionsButtonImage(findData);
    }
}

void ThreadSearchView::UpdateOptionsButtonImage(const ThreadSearchFindData &findData)
{
    // Updates optiuns button in message pane and toolbar

    const wxString name(findData.IsOptionEnabled() ? "optionsactive" : "options");

    {
#if wxCHECK_VERSION(3, 1, 6)
        const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
        m_pBtnOptions->SetBitmapLabel(cbLoadBitmapBundleFromSVG(prefix+name+".svg", wxSize(16, 16)));
#else
        const wxString prefix(GetImagePrefix(false, m_pBtnOptions));
        m_pBtnOptions->SetBitmapLabel(cbLoadBitmap(prefix+name+".png"));
#endif
    }

    if (m_pToolBar)
    {
#if wxCHECK_VERSION(3, 1, 6)
        const int height = m_pToolBar->GetToolBitmapSize().GetHeight();
        const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
        m_pToolBar->SetToolNormalBitmap(controlIDs.Get(ControlIDs::idBtnOptions), cbLoadBitmapBundleFromSVG(prefix+name+".svg", wxSize(height, height)));
#else
        const wxString prefix(GetImagePrefix(true));
        m_pToolBar->SetToolNormalBitmap(controlIDs.Get(ControlIDs::idBtnOptions), cbLoadBitmap(prefix+name+".png"));
#endif
    }
}

void ThreadSearchView::OnQuickOptionsUpdateUI(wxUpdateUIEvent &event)
{
    ThreadSearchFindData &findData = m_ThreadSearchPlugin.GetFindData();
    if (event.GetId() == controlIDs.Get(ControlIDs::idBtnSearch))
    {
        // We enable the search button when a search string is present in the combo box
        // or if the search history is not empty. If the user searches with an empty combo box
        // the last performed search will be repeated (search word taken from combo box history)
        const bool hasValue = !m_pCboSearchExpr->GetValue().empty() || m_pCboSearchExpr->GetStrings().size() > 0;
        event.Enable(hasValue);
    }
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionWholeWord))
        event.Check(findData.GetMatchWord());
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionStartWord))
        event.Check(findData.GetStartWord());
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionMatchCase))
        event.Check(findData.GetMatchCase());
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionMatchInComments))
        event.Check(findData.GetMatchInComments());
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionRegEx))
        event.Check(findData.GetRegEx());
    else if (event.GetId() == controlIDs.Get(ControlIDs::idOptionResetAll))
    {
        bool enabled = findData.GetMatchWord();
        enabled |= findData.GetStartWord();
        enabled |= findData.GetMatchCase();
        enabled |= findData.GetRegEx();

        event.Enable(enabled);
    }
}

void ThreadSearchView::OnBtnShowDirItemsClick(wxCommandEvent& WXUNUSED(event))
{
    // Shows/Hides directory parameters panel and updates button label.
    wxSizer* pTopSizer = GetSizer();
    wxASSERT(m_pSizerSearchDirItems && pTopSizer);

    const bool show = !m_pPnlDirParams->IsShown();
    m_ThreadSearchPlugin.SetShowDirControls(show);

    pTopSizer->Show(m_pSizerSearchDirItems, show, true);
    m_pBtnShowDirItems->SetToolTip(show ? _("Hide dir items") : _("Show dir items"));
    pTopSizer->Layout();
}

void ThreadSearchView::OnSplitterDoubleClick(wxSplitterEvent &/*event*/)
{
    m_ThreadSearchPlugin.SetShowCodePreview(false);
    ApplySplitterSettings(false, m_pSplitter->GetSplitMode());

    // Informs user on how to show code preview later.
    cbMessageBox(_("To re-enable code preview, check the \"Show code preview editor\" in ThreadSearch options panel."),
                 _("ThreadSearchInfo"), wxICON_INFORMATION);
}

// wxGlade: add ThreadSearchView event handlers

void ThreadSearchView::set_properties()
{
    // Update icons 1, 2 and 8 in the message pane

#if wxCHECK_VERSION(3, 1, 6)
    const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
    const wxSize bmpSize(16, 16);
#else
    const wxString prefix(GetImagePrefix(false, this));
#endif

    // begin wxGlade: ThreadSearchView::set_properties
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

    SetWindowMinMaxSize(*m_pCboSearchExpr, 80, 180);

    m_pBtnSearch->SetToolTip(_("Search in files"));
#if wxCHECK_VERSION(3, 1, 6)
    m_pBtnSearch->SetBitmapDisabled(cbLoadBitmapBundleFromSVG(prefix+"findfdisabled.svg", bmpSize));
#else
    m_pBtnSearch->SetBitmapDisabled(cbLoadBitmap(prefix+"findfdisabled.png"));
#endif

    m_pBtnOptions->SetToolTip(_("Options"));
#if wxCHECK_VERSION(3, 1, 6)
    m_pBtnOptions->SetBitmapDisabled(cbLoadBitmapBundleFromSVG(prefix+"optionsdisabled.svg", bmpSize));
#else
    m_pBtnOptions->SetBitmapDisabled(cbLoadBitmap(prefix+"optionsdisabled.png"));
#endif

    m_pBtnShowDirItems->SetToolTip(_("Show dir items"));
#if wxCHECK_VERSION(3, 1, 6)
    m_pBtnShowDirItems->SetBitmapDisabled(cbLoadBitmapBundleFromSVG(prefix+"showdirdisabled.svg", bmpSize));
#else
    m_pBtnShowDirItems->SetBitmapDisabled(cbLoadBitmap(prefix+"showdirdisabled.png"));
#endif

    m_pPnlPreview->SetMinSize(wxSize(25, -1));
    // end wxGlade

    m_pSearchPreview->SetReadOnly(true);

    ThreadSearchFindData findData;
    m_ThreadSearchPlugin.GetFindData(findData);

    m_pPnlDirParams->SetSearchDirHidden(findData.GetHiddenSearch());
    m_pPnlDirParams->SetSearchDirRecursively(findData.GetRecursiveSearch());
    m_pPnlDirParams->SetSearchDirPath(findData.GetSearchPath());
    m_pPnlDirParams->SetSearchMask(findData.GetSearchMask());

    m_pPnlSearchIn->SetSearchInOpenFiles(findData.MustSearchInOpenFiles());
    m_pPnlSearchIn->SetSearchInTargetFiles(findData.MustSearchInTarget());
    m_pPnlSearchIn->SetSearchInProjectFiles(findData.MustSearchInProject());
    m_pPnlSearchIn->SetSearchInWorkspaceFiles(findData.MustSearchInWorkspace());
    m_pPnlSearchIn->SetSearchInDirectory(findData.MustSearchInDirectory());

    UpdateOptionsButtonImage(findData);
}

void ThreadSearchView::do_layout()
{
    // begin wxGlade: ThreadSearchView::do_layout
    m_pSizerSearchItems = new wxBoxSizer(wxHORIZONTAL);
    m_pSizerSearchItems->Add(m_pCboSearchExpr, 2, wxALL|wxALIGN_CENTER_VERTICAL, 4);
    m_pSizerSearchItems->Add(m_pBtnSearch, 0, wxALL|wxALIGN_CENTER_VERTICAL, 4);
    m_pSizerSearchItems->Add(m_pBtnOptions, 0, wxALL|wxALIGN_CENTER_VERTICAL, 4);
    m_pSizerSearchItems->Add(m_pStaticLine1, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    m_pSizerSearchItems->Add(m_pStaTxtSearchIn, 0, wxALL|wxALIGN_CENTER_VERTICAL, 4);
    m_pSizerSearchItems->Add(m_pPnlSearchIn, 0, wxALIGN_CENTER_VERTICAL, 0);
    m_pSizerSearchItems->Add(m_pStaticLine2, 0, wxLEFT|wxRIGHT|wxEXPAND, 2);
    m_pSizerSearchItems->Add(m_pBtnShowDirItems, 0, wxALL|wxALIGN_CENTER_VERTICAL, 4);

    m_pSizerSearchDirItems = new wxStaticBoxSizer(m_pSizerSearchDirItems_staticbox, wxHORIZONTAL);
    m_pSizerSearchDirItems->Add(m_pPnlDirParams, 1, wxALIGN_CENTER_VERTICAL, 0);

    wxBoxSizer* m_pSizerSearchPreview = new wxBoxSizer(wxHORIZONTAL);
    m_pSizerSearchPreview->Add(m_pSearchPreview, 1, wxEXPAND, 0);
    m_pPnlPreview->SetAutoLayout(true);
    m_pPnlPreview->SetSizer(m_pSizerSearchPreview);

    m_pSplitter->SplitVertically(m_pPnlPreview, m_pLogger);
    wxBoxSizer* m_pSizerSplitter = new wxBoxSizer(wxHORIZONTAL);
    m_pSizerSplitter->Add(m_pSplitter, 1, wxEXPAND, 0);

    wxBoxSizer* m_pSizerTop = new wxBoxSizer(wxVERTICAL);
    m_pSizerTop->Add(m_pSizerSearchItems, 0, wxEXPAND, 0);
    m_pSizerTop->Add(m_pSizerSearchDirItems, 0, wxBOTTOM|wxEXPAND, 4);
    m_pSizerTop->Add(m_pSizerSplitter, 1, wxEXPAND, 0);
    SetAutoLayout(true);
    SetSizer(m_pSizerTop);
    m_pSizerTop->Fit(this);
    m_pSizerTop->SetSizeHints(this);
    // end wxGlade

    m_pSplitter->SetMinimumPaneSize(50);
}

void ThreadSearchView::OnThreadExit()
{
    // This method must be called only from ThreadSearchThread::OnExit
    // because delete is performed in the base class.
    // We reset the pointer to be sure it is not used.
    m_pFindThread = nullptr;

    if (m_StoppingThread > 0)
        m_StoppingThread--;
}

void ThreadSearchView::ThreadedSearch(const ThreadSearchFindData& aFindData)
{
    // We don't search empty patterns
    if (aFindData.GetFindText() != wxEmptyString)
    {
        ThreadSearchFindData findData(aFindData);

        // Prepares logger
        m_pLogger->OnSearchBegin(aFindData);
        m_hasSearchItems = false;

        // Two steps thread creation
        m_pFindThread = new ThreadSearchThread(this, findData);
        if (m_pFindThread != nullptr)
        {
            if (m_pFindThread->Create() == wxTHREAD_NO_ERROR)
            {
                // Thread execution
                if (m_pFindThread->Run() != wxTHREAD_NO_ERROR)
                {
                    m_pFindThread->Delete();
                    m_pFindThread = nullptr;
                    cbMessageBox(_("Failed to run search thread"));
                }
                else
                {
                    // Update combo box search history
                    AddExpressionToSearchCombos(findData.GetFindText(), findData.GetSearchPath(),
                                                findData.GetSearchMask());
                    UpdateSearchButtons(true, cancel);
                    EnableControls(false);

                    // Starts the timer used to managed events sent by m_pFindThread
                    m_Timer.Start(TIMER_PERIOD, wxTIMER_CONTINUOUS);
                }
            }
            else
            {
                // Error
                m_pFindThread->Delete();
                m_pFindThread = nullptr;
                cbMessageBox(_("Failed to create search thread (2)"));
            }
        }
        else
        {
            // Error
            cbMessageBox(_("Failed to create search thread (1)"));
        }
    }
    else
    {
        // Error
        cbMessageBox(_("Search expression is empty !"));
    }
}


bool ThreadSearchView::UpdatePreview(const wxString& file, long line)
{
    bool success(true);

    if (line > 0)
    {
        // Line display begins at 1 but line index at 0
        line--;
    }

    // Disable read only
    m_pSearchPreview->Enable(false);
    m_pSearchPreview->SetReadOnly(false);

    // Loads file if different from current loaded
    wxFileName filename(file);
    if ( (m_PreviewFilePath != file) || (m_PreviewFileDate != filename.GetModificationTime()) )
    {
        ConfigManager* mgr = Manager::Get()->GetConfigManager("editor");

        // Remember current file path and modification time
        m_PreviewFilePath = file;
        m_PreviewFileDate = filename.GetModificationTime();

        EncodingDetector enc(m_PreviewFilePath, false);
        success = enc.IsOK();
        m_pSearchPreview->SetText(enc.GetWxStr());

        // Colorize
        cbEditor::ApplyStyles(m_pSearchPreview);
        EditorColourSet EdColSet;
        EdColSet.Apply(EdColSet.GetLanguageForFilename(m_PreviewFilePath), m_pSearchPreview, false,
                       true);

        cb::SetFoldingMarkers(m_pSearchPreview, mgr->ReadInt("/folding/indicator", 2));
        cb::UnderlineFoldedLines(m_pSearchPreview, mgr->ReadBool("/folding/underline_folded_line", true));
    }

    if (success)
    {
        // Display the selected line
        const int onScreen = m_pSearchPreview->LinesOnScreen() >> 1;
        m_pSearchPreview->GotoLine(line - onScreen);
        m_pSearchPreview->GotoLine(line + onScreen);
        m_pSearchPreview->GotoLine(line);
        m_pSearchPreview->EnsureVisible(line);

        const int startPos = m_pSearchPreview->PositionFromLine(line);
        const int endPos   = m_pSearchPreview->GetLineEndPosition(line);
        m_pSearchPreview->SetSelectionVoid(endPos, startPos);
    }

    // Enable read only
    m_pSearchPreview->SetReadOnly(true);
    m_pSearchPreview->Enable(true);

    return success;
}

void ThreadSearchView::OnLoggerClick(const wxString& file, long line)
{
    // Updates preview editor with selected file at requested line.
    // We don't check that file changed since search was performed.
    UpdatePreview(file, line);
}

void ThreadSearchView::OnLoggerDoubleClick(const wxString& file, long line)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->Open(file);
    if (!line || !ed)
        return;

    line--;
    ed->Activate();

    // cbEditor already implements the line centering
    ed->GotoLine(line);

    // Show even if line is folded
    if (cbStyledTextCtrl* control = ed->GetControl())
    {
        control->EnsureVisible(line);

        wxFocusEvent ev(wxEVT_SET_FOCUS);
        ev.SetWindow(this);
        control->GetEventHandler()->AddPendingEvent(ev);
    }
}

void ThreadSearchView::OnMarginClick(wxScintillaEvent& event)
{
    // We handle click on preview editor margin
    // For now, all is read only; we manage folding but not
    // breakpoints and bookmarks.
    switch (event.GetMargin())
    {
        case 2: // folding margin
        {
            const int lineYpix = event.GetPosition();
            const int line = m_pSearchPreview->LineFromPosition(lineYpix);
            m_pSearchPreview->ToggleFold(line);
            break;
        }
        case 1: // bookmarks and breakpoints margin
        {
            // For now I ignore bookmarks and breakpoints
            /*
            #define BOOKMARK_MARKER    2
            int line = m_pSearchPreview->LineFromPosition(event.GetPosition());

            if ( m_pSearchPreview->MarkerGet(line) & (1 << BOOKMARK_MARKER) )
            {
                m_pSearchPreview->MarkerDelete(line, BOOKMARK_MARKER);
            }
            else
            {
                m_pSearchPreview->MarkerAdd(line, BOOKMARK_MARKER);
            }
            */
            break;
        }
        default:
        {
            break;
        }
    }
}

void ThreadSearchView::OnContextMenu(wxContextMenuEvent& event)
{
    event.StopPropagation();
}

void ThreadSearchView::AddExpressionToSearchCombos(const wxString& expression, const wxString& path, const wxString& mask)
{
    // We perform tests on view combo and don't check toolbar one
    // because their contents are identical
    // Gets toolbar combo pointer
    const long id = controlIDs.Get(ControlIDs::idCboSearchExpr);
    wxComboBox* pToolBarCombo = static_cast<wxComboBox*>(m_pToolBar->FindControl(id));

    AddItemToCombo(m_pCboSearchExpr, expression);
    AddItemToCombo(pToolBarCombo, expression);

    m_pPnlDirParams->AddExpressionToCombos(path, mask);
}

void ThreadSearchView::FocusSearchCombo(const wxString &searchWord)
{
    if (!searchWord.empty())
        m_pCboSearchExpr->SetValue(searchWord);
    m_pCboSearchExpr->SetFocus();
}

void ThreadSearchView::Update()
{
    ThreadSearchFindData findData;
    m_ThreadSearchPlugin.GetFindData(findData);

    m_pPnlSearchIn->SetSearchInOpenFiles     (findData.MustSearchInOpenFiles());
    m_pPnlSearchIn->SetSearchInTargetFiles   (findData.MustSearchInTarget   ());
    m_pPnlSearchIn->SetSearchInProjectFiles  (findData.MustSearchInProject  ());
    m_pPnlSearchIn->SetSearchInWorkspaceFiles(findData.MustSearchInWorkspace());
    m_pPnlSearchIn->SetSearchInDirectory     (findData.MustSearchInDirectory());

    m_pPnlDirParams->SetSearchDirHidden      (findData.GetHiddenSearch());
    m_pPnlDirParams->SetSearchDirRecursively (findData.GetRecursiveSearch());
    m_pPnlDirParams->SetSearchDirPath        (findData.GetSearchPath());
    m_pPnlDirParams->SetSearchMask           (findData.GetSearchMask());

    m_pPnlDirParams->AddExpressionToCombos(findData.GetSearchPath(), findData.GetSearchMask());

    ShowSearchControls(m_ThreadSearchPlugin.GetShowSearchControls());
    SetLoggerType(m_ThreadSearchPlugin.GetLoggerType());
    m_pLogger->Update();

    ApplySplitterSettings(m_ThreadSearchPlugin.GetShowCodePreview(), m_ThreadSearchPlugin.GetSplitterMode());
}

void ThreadSearchView::OnBtnSearchOpenFiles(wxCommandEvent &event)
{
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeOpenFiles, m_pPnlSearchIn->GetSearchInOpenFiles());
    event.Skip();
}

void ThreadSearchView::OnBtnSearchTargetFiles(wxCommandEvent &event)
{
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeTargetFiles, m_pPnlSearchIn->GetSearchInTargetFiles());
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeProjectFiles, false);
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeWorkspaceFiles, false);
    event.Skip();
}

void ThreadSearchView::OnBtnSearchProjectFiles(wxCommandEvent &event)
{
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeProjectFiles, m_pPnlSearchIn->GetSearchInProjectFiles());
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeTargetFiles, false);
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeWorkspaceFiles, false);
    event.Skip();
}

void ThreadSearchView::OnBtnSearchWorkspaceFiles(wxCommandEvent &event)
{
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeWorkspaceFiles, m_pPnlSearchIn->GetSearchInWorkspaceFiles());
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeTargetFiles, false);
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeProjectFiles, false);
    event.Skip();
}

void ThreadSearchView::OnBtnSearchDirectoryFiles(wxCommandEvent &event)
{
    const bool enable = m_pPnlSearchIn->GetSearchInDirectory();
    m_ThreadSearchPlugin.GetFindData().UpdateSearchScope(ScopeDirectoryFiles, enable);
    m_pPnlDirParams->Enable(enable);
    event.Skip();
}

void ThreadSearchView::EnableControls(bool enable)
{
    // Used to disable search parameters controls during
    // threaded search in notebook panel and toolbar.
    ControlIDs::IDs idsArray[] =
    {
        ControlIDs::idBtnDirSelectClick,
        ControlIDs::idBtnOptions,
        ControlIDs::idCboSearchExpr,
        ControlIDs::idChkSearchDirRecurse,
        ControlIDs::idChkSearchDirHidden,
        ControlIDs::idBtnSearchOpenFiles,
        ControlIDs::idBtnSearchTargetFiles,
        ControlIDs::idBtnSearchProjectFiles,
        ControlIDs::idBtnSearchWorkspaceFiles,
        ControlIDs::idBtnSearchDirectoryFiles,
        ControlIDs::idSearchDirPath,
        ControlIDs::idSearchMask
    };

    wxWindow* focused = wxWindow::FindFocus();

    // Disabled controls cannot be focussed (at least in GTK), so we store the pointer to the
    // focussed control, so we could later restore it.
    if (!enable)
        m_LastFocusedWindow = focused;

    for (size_t i = 0; i < sizeof(idsArray)/sizeof(idsArray[0]); ++i)
    {
        wxWindow* pWnd = wxWindow::FindWindow(controlIDs.Get(idsArray[i]));
        if (pWnd)
        {
            pWnd->Enable(enable);
        }
        else
        {
            cbMessageBox(wxString::Format(_("Failed to Enable window (id=%ld)"), idsArray[i]),
                         _("Error"), wxOK|wxICON_ERROR, this);
        }
    }

    wxWindow* tabControl = m_pToolBar->FindControl(controlIDs.Get(ControlIDs::idCboSearchExpr));
    tabControl->Enable(enable);

    m_pToolBar->EnableTool(controlIDs.Get(ControlIDs::idBtnOptions), enable);
    m_pToolBar->Update();

    // When we re-enable the control we want to restore the focus if there is no control with the
    // focus at the moment and we started with one of our controls focussed.
    if (enable && !focused && m_LastFocusedWindow)
    {
        if (m_LastFocusedWindow == m_pCboSearchExpr || m_LastFocusedWindow == tabControl)
            m_LastFocusedWindow->SetFocus();
    }
}

void ThreadSearchView::PostThreadSearchEvent(const ThreadSearchEvent& event)
{
    // Clone the worker thread event to the mutex protected m_ThreadSearchEventsArray
    if ( m_MutexSearchEventsArray.Lock() == wxMUTEX_NO_ERROR )
    {
        // Events are handled in ThreadSearchView::OnTmrListCtrlUpdate
        // ThreadSearchView::OnTmrListCtrlUpdate is called automatically
        // by m_Timer wxTimer
        m_ThreadSearchEventsArray.Add(event.Clone());
        m_MutexSearchEventsArray.Unlock();
    }
}

void ThreadSearchView::OnTmrListCtrlUpdate(wxTimerEvent& /*event*/)
{
    if (m_MutexSearchEventsArray.Lock() == wxMUTEX_NO_ERROR)
    {
        if (m_ThreadSearchEventsArray.GetCount() > 0)
        {
            ThreadSearchEvent* pEvent = static_cast<ThreadSearchEvent*>(m_ThreadSearchEventsArray[0]);
            m_pLogger->OnThreadSearchEvent(*pEvent);
            delete pEvent;
            m_ThreadSearchEventsArray.RemoveAt(0,1);
            m_hasSearchItems = true;
        }

        if ((m_ThreadSearchEventsArray.GetCount() == 0) && !m_pFindThread)
        {
            // Thread search is finished (m_pFindThread == nullptr) and m_ThreadSearchEventsArray
            // is empty (m_ThreadSearchEventsArray.GetCount() == 0).
            // We stop the timer to spare resources
            m_Timer.Stop();

            m_pLogger->OnSearchEnd();

            // Clear the search string, so the user can type a new one. This makes using
            // middle-click paste on linux a lot more usable. But don't clear the search if there
            // are no results, this might make it possible for the user to edit the search query a
            // bit more easily.
            if (m_hasSearchItems)
            {
                m_pCboSearchExpr->SetValue(wxString());
                const long id = controlIDs.Get(ControlIDs::idCboSearchExpr);
                wxComboBox* pToolBarCombo = static_cast<wxComboBox*>(m_pToolBar->FindControl(id));
                if (pToolBarCombo)
                    pToolBarCombo->SetValue(wxString());
            }

            // Restores label and enables all search params graphical widgets.
            UpdateSearchButtons(true, search);
            EnableControls(true);
        }

        m_MutexSearchEventsArray.Unlock();
    }
}

bool ThreadSearchView::ClearThreadSearchEventsArray()
{
    const bool success = (m_MutexSearchEventsArray.Lock() == wxMUTEX_NO_ERROR);
    if (success)
    {
        size_t i = m_ThreadSearchEventsArray.GetCount();
        ThreadSearchEvent* pEvent = nullptr;
        while ( i != 0 )
        {
            pEvent = static_cast<ThreadSearchEvent*>(m_ThreadSearchEventsArray[0]);
            delete pEvent;
            m_ThreadSearchEventsArray.RemoveAt(0, 1);
            i--;
        }

        m_MutexSearchEventsArray.Unlock();
    }

    return success;
}

bool ThreadSearchView::StopThread()
{
    bool success = false;
    if ((m_StoppingThread == 0) && (m_pFindThread != nullptr))
    {
        // A search thread is running. We stop it.
        m_StoppingThread++;
        m_pFindThread->Delete();

        // We stop the timer responsible for list control update and we wait twice
        // its period to delete all waiting events (not processed yet)
        m_Timer.Stop();
        wxThread::Sleep(2*TIMER_PERIOD);

        success = ClearThreadSearchEventsArray();
        if (!success)
            cbMessageBox(_("Failed to clear events array."), _("Error"), wxICON_ERROR);

        // Restores label and enables all search params graphical widgets.
        UpdateSearchButtons(true, search);
        EnableControls(true);
    }

    return success;
}

bool ThreadSearchView::IsSearchRunning()
{
    bool searchRunning = (m_pFindThread != nullptr);

    if (m_MutexSearchEventsArray.Lock() == wxMUTEX_NO_ERROR)
    {
        // If user clicked on Cancel or thread is finished, there may be remaining
        // events to display in the array. In this case, we consider the search is
        // stil running even if thread is over.
        searchRunning = searchRunning || (m_ThreadSearchEventsArray.GetCount() > 0);

        m_MutexSearchEventsArray.Unlock();
    }

    return searchRunning;
}


void ThreadSearchView::UpdateSearchButtons(bool enable, eSearchButtonLabel label)
{
    // Labels and pictures paths
    wxString searchButtonLabels[] = {_("Search"), _("Cancel search"), ""};

    wxString searchButtonPathsEnabled[]  = {"findf",         "stop",         ""};
    wxString searchButtonPathsDisabled[] = {"findfdisabled", "stopdisabled", ""};

    // Gets toolbar search button pointer
    // Changes label/bitmap only if requested
    if (label != skip)
    {
        {
#if wxCHECK_VERSION(3, 1, 6)
            const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
            wxBitmapBundle bmpSearch = cbLoadBitmapBundleFromSVG(prefix+searchButtonPathsEnabled[label]+".svg", wxSize(16, 16));
            wxBitmapBundle bmpSearchDisabled = cbLoadBitmapBundleFromSVG(prefix+searchButtonPathsDisabled[label]+".svg", wxSize(16, 16));
#else
            const wxString prefix(GetImagePrefix(false, m_pBtnSearch));
            wxBitmap bmpSearch = cbLoadBitmap(prefix+searchButtonPathsEnabled[label]+".png");
            wxBitmap bmpSearchDisabled = cbLoadBitmap(prefix+searchButtonPathsDisabled[label]+".png");
#endif
            m_pBtnSearch->SetToolTip(searchButtonLabels[label]);
            m_pBtnSearch->SetBitmapLabel(bmpSearch);
            m_pBtnSearch->SetBitmapDisabled(bmpSearchDisabled);
        }

        {
            //Toolbar buttons
#if wxCHECK_VERSION(3, 1, 6)
            const int height = m_pToolBar->GetToolBitmapSize().GetHeight();
            const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
            wxBitmapBundle bmpSearch = cbLoadBitmapBundleFromSVG(prefix+searchButtonPathsEnabled[label]+".svg", wxSize(height, height));
            wxBitmapBundle bmpSearchDisabled = cbLoadBitmapBundleFromSVG(prefix+searchButtonPathsDisabled[label]+".svg", wxSize(height, height));
#else
            const wxString prefix(GetImagePrefix(true));
            wxBitmap bmpSearch = cbLoadBitmap(prefix+searchButtonPathsEnabled[label]+".png");
            wxBitmap bmpSearchDisabled = cbLoadBitmap(prefix+searchButtonPathsDisabled[label]+".png");
#endif
            m_pToolBar->SetToolNormalBitmap(controlIDs.Get(ControlIDs::idBtnSearch), bmpSearch);
            m_pToolBar->SetToolDisabledBitmap(controlIDs.Get(ControlIDs::idBtnSearch), bmpSearchDisabled);
        }
    }

    // Sets enable state
    m_pBtnSearch->Enable(enable);
    m_pToolBar->EnableTool(controlIDs.Get(ControlIDs::idBtnSearch), enable);
}

wxString GetImagePrefix(bool toolbar, wxWindow *window)
{
    int size;

    if (toolbar)
    {
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Toolbars);
    }
    else
    {
        cbAssert(window != nullptr);
        const int targetHeight = wxRound(16 * cbGetActualContentScaleFactor(*window));
        size = cbFindMinSize16to64(targetHeight);
    }

    return ConfigManager::GetDataFolder()+wxString::Format("/ThreadSearch.zip#zip:images/%dx%d/", size, size);
}

void SetWindowMinMaxSize(wxWindow &window, int numChars, int minSize)
{
    window.SetMinSize(wxSize(minSize, -1));

    const wxString s('W', numChars);
    const wxSize textSize = window.GetTextExtent(s);
    window.SetMaxSize(wxSize(std::max(minSize, textSize.x), -1));
}

void ThreadSearchView::ShowSearchControls(bool show)
{
    bool     redraw    = false;
    wxSizer* pTopSizer = GetSizer();

    // ThreadSearchPlugin update
    m_ThreadSearchPlugin.SetShowSearchControls(show);

    // We show/hide search controls only if necessary
    if ( m_pBtnSearch->IsShown() != show )
    {
        pTopSizer->Show(m_pSizerSearchItems, show, true);
        redraw = true;
    }

    // When we show search controls, user might have hidden the
    // directory search controls to spare space.
    // In this case, we restore dir control show state
    if ( show == true )
        show = m_ThreadSearchPlugin.GetShowDirControls();

    if ( m_pPnlDirParams->IsShown() != show )
    {
        pTopSizer->Show(m_pSizerSearchDirItems, show, true);
        redraw = true;
    }

    if (redraw)
        pTopSizer->Layout();
}


void ThreadSearchView::ApplySplitterSettings(bool showCodePreview, long splitterMode)
{
    if (showCodePreview)
    {
        if ((m_pSplitter->IsSplit() == false) || (splitterMode != m_pSplitter->GetSplitMode()))
        {
            if (m_pSplitter->IsSplit())
                m_pSplitter->Unsplit();

            if (splitterMode == wxSPLIT_HORIZONTAL)
                m_pSplitter->SplitHorizontally(m_pLogger, m_pPnlPreview);
            else
                m_pSplitter->SplitVertically(m_pPnlPreview, m_pLogger);
        }
    }
    else if (m_pSplitter->IsSplit())
        m_pSplitter->Unsplit(m_pPnlPreview);
}

void ThreadSearchView::SetLoggerType(ThreadSearchLoggerBase::eLoggerTypes lgrType)
{
    if (lgrType != m_pLogger->GetLoggerType())
    {
        ThreadSearchLoggerBase *oldLogger = m_pLogger;
        m_pLogger = ThreadSearchLoggerBase::Build(*this, m_ThreadSearchPlugin, lgrType,
                                                  m_ThreadSearchPlugin.GetFileSorting(),
                                                  m_pSplitter,
                                                  controlIDs.Get(ControlIDs::idWndLogger));

        if (m_pSplitter->ReplaceWindow(oldLogger, m_pLogger))
            delete oldLogger;
    }
}

void ThreadSearchView::SetSashPosition(int position, const bool redraw)
{
    m_pSplitter->SetSashPosition(position, redraw);
}

int ThreadSearchView::GetSashPosition() const
{
    return m_pSplitter->GetSashPosition();
}

void ThreadSearchView::SetSearchHistory(const wxArrayString& searchPatterns, const wxArrayString& searchDirs,
                                        const wxArrayString& searchMasks)
{
    m_pCboSearchExpr->Append(searchPatterns);
    if (searchPatterns.GetCount() > 0)
        m_pCboSearchExpr->SetSelection(0);

    m_pPnlDirParams->SetSearchHistory(searchDirs, searchMasks);
}

wxArrayString ThreadSearchView::GetSearchHistory() const
{
    return m_pCboSearchExpr->GetStrings();
}

wxArrayString ThreadSearchView::GetSearchDirsHistory() const
{
    return m_pPnlDirParams->GetSearchDirsHistory();
}

wxArrayString ThreadSearchView::GetSearchMasksHistory() const
{
    return m_pPnlDirParams->GetSearchMasksHistory();
}

void ThreadSearchView::UpdateSettings()
{
    if (m_pLogger)
        m_pLogger->UpdateSettings();

    if (m_pPnlDirParams)
        m_pPnlDirParams->Enable(m_pPnlSearchIn->GetSearchInDirectory());
}

void ThreadSearchView::EditorLinesAddedOrRemoved(cbEditor *editor, int startLine, int linesAdded)
{
    if (m_pLogger)
        m_pLogger->EditorLinesAddedOrRemoved(editor, startLine, linesAdded);
}
