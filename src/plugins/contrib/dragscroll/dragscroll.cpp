/***************************************************************
 * Name:      dragscroll.cpp
 * Purpose:   Code::Blocks plugin
 * Author:    Pecan<>
 * Copyright: (c) Pecan
 * License:   GPL
 **************************************************************/

//-#if defined(__GNUG__) && !defined(__APPLE__)
#include "wx/event.h"
#if defined(__GNUG__) && !defined(__APPLE__) && !defined(__MINGW32__)
	#pragma implementation "dragscroll.h"
#endif


#include <sdk.h>
#ifndef CB_PRECOMP
    #include <wx/app.h>
	#include <wx/intl.h>
	#include <wx/listctrl.h>
	#include "configmanager.h"
	#include "manager.h"
	#include "personalitymanager.h"
	#include "sdk_events.h" // EVT_APP_STARTUP_DONE
#endif

#include <wx/fileconf.h> // wxFileConfig
#include <wx/html/htmlwin.h>
#include <wx/tokenzr.h>
#include <wx/window.h>

#include "cbstyledtextctrl.h"
#include "dragscroll.h"
#include "dragscrollcfg.h"
#include "dragscrollevent.h"
#include "logmanager.h"
#include "loggers.h"
#include "projectmanager.h"
#include "editormanager.h"

#include "startherepage.h"

// ----------------------------------------------------------------------------
//  TextCtrlLogger class to allow IsLoggerControl() access to "control" pointer
// ----------------------------------------------------------------------------
class dsTextCtrlLogger : public TextCtrlLogger
// ----------------------------------------------------------------------------
{
    //Helper class to verify a logger control
   friend class cbDragScroll;
   public:
    dsTextCtrlLogger(){};
    ~dsTextCtrlLogger(){};
};
// ----------------------------------------------------------------------------
class dsStartHerePage : public StartHerePage
// ----------------------------------------------------------------------------
{
    //Helper class to verify a htmlWindow
    friend class cbDragScroll;
    dsStartHerePage(wxEvtHandler* owner, wxWindow* parent);
    ~dsStartHerePage();
};
// ----------------------------------------------------------------------------
// Register the plugin
// ----------------------------------------------------------------------------
namespace
{
    PluginRegistrant<cbDragScroll> reg(_T("cbDragScroll"));
    int ID_DLG_DONE = wxNewId();
    bool wxFound(int result) __attribute__((unused)); // avoid unused error msg
    bool wxFound(int result) {return result != wxNOT_FOUND;}
};
// ----------------------------------------------------------------------------
//  Events table
// ----------------------------------------------------------------------------
BEGIN_EVENT_TABLE(cbDragScroll, cbPlugin)
	// End Configuration event
    EVT_UPDATE_UI(ID_DLG_DONE, cbDragScroll::OnDoConfigRequests)
    // DragScroll Event types
    EVT_DRAGSCROLL_EVENT( wxID_ANY, cbDragScroll::OnDragScrollEvent_Dispatcher )

END_EVENT_TABLE()
// ----------------------------------------------------------------------------
//  Statics
// ----------------------------------------------------------------------------
// global used by mouse events to get user configuration settings
cbDragScroll* cbDragScroll::pDragScroll;
// ----------------------------------------------------------------------------
cbDragScroll::cbDragScroll()
// ----------------------------------------------------------------------------
{
	//ctor
	// anchor to this one and only object
    pDragScroll = this;
    m_pMouseEventsHandler = new MouseEventsHandler();
    m_pCB_AppWindow = Manager::Get()->GetAppWindow();
}
// ----------------------------------------------------------------------------
cbDragScroll::~cbDragScroll()
// ----------------------------------------------------------------------------
{
	//dtor
	delete m_pMouseEventsHandler;
	m_pMouseEventsHandler = nullptr;
}

// ----------------------------------------------------------------------------
void cbDragScroll::OnAttach()
// ----------------------------------------------------------------------------
{
	// do whatever initialization you need for your plugin
	// NOTE: after this function, the inherited member variable
	// IsAttached() will be TRUE...
	// You should check for it in other functions, because if it
	// is FALSE, it means that the application did *not* "load"
	// (see: does not need) this plugin...

    pMyLog = NULL;
    m_bNotebooksAttached = false;
    m_ZoomWindowIds      = wxEmptyString;
    m_ZoomFontSizes      = wxEmptyString;
    m_ZoomWindowIdsArray.Clear();
    m_ZoomFontSizesArray.Clear();

    m_pCB_AppWindow = Manager::Get()->GetAppWindow();

    #if defined(LOGGING)
        // create a small debugging logger window
        wxWindow* pcbWindow = m_pCB_AppWindow;
        wxLog::EnableLogging(true);
        //wxLogWindow*
        pMyLog = new wxLogWindow(pcbWindow, wxT("DragScroll Log"), true, false);
        pMyLog->PassMessages(false); // Enable "Clear" menu item
        wxLog::SetActiveTarget(pMyLog);
        pMyLog->Flush();
        // Move the log window to screen position (0, 0)
        wxFrame* logFrame = pMyLog->GetFrame();  // Get the underlying wxFrame
        if (logFrame)
            logFrame->SetPosition(wxPoint(0, 0));  // Set position to (0, 0)
    #endif //LOGGING

	// Use a separate class to catch the mouse events
    if (not m_pMouseEventsHandler )
        m_pMouseEventsHandler = new MouseEventsHandler();

    // Attach to windows for which we want to catch mouse events
    // names of windows to which we're allowed to attach
    m_UsableWindows.Add(_T("text"));        // compiler logs
    m_UsableWindows.Add(_T("listctrl"));    // compiler errors
    m_UsableWindows.Add(_T("textctrl"));    // logs
    #if defined(__WXMSW__)
        // Scrolling a tree ctrl stalls linux //(ph 2025/01/16)
        m_UsableWindows.Add(_T("treectrl"));    // management trees
    #endif
    m_UsableWindows.Add(_T("treeAll"));
    m_UsableWindows.Add(_T("treeMembers"));
    m_UsableWindows.Add(_T("csTreeCtrl"));  // codesnippets
    m_UsableWindows.Add(_T("sciwindow"));   // editor controls
    m_UsableWindows.Add(_T("htmlwindow"));  // start here page

    MouseDragScrollEnabled  = true;
    MouseEditorFocusEnabled = false;
    MouseFocusEnabled       = false;
    MouseDragDirection      = 0;
    MouseDragKey            = dragKeyType::Right_Mouse; //default
    MouseDragSensitivity    = 1; // set mouse speed sensitivity to lowest
    MouseWheelZoom          = false;
    PropagateLogZoomSize    = false;
    m_MouseHtmlFontSize     = 0;
    m_MouseWheelZoomReverse = false;

    // Create filename like cbDragScroll.ini
    // I've separated this from the .cbp to avoid others from inheriting my settings
    //  else I have to remember to clean out the cbp for every commit.
    //memorize the key file name as {%HOME%}\cbDragScroll.ini
    m_ConfigFolder = ConfigManager::GetConfigFolder();
    m_DataFolder = ConfigManager::GetDataFolder();
    m_ExecuteFolder = FindAppPath(wxTheApp->argv[0], ::wxGetCwd(), wxEmptyString);

    //GTK GetConfigFolder is returning double "//?, eg, "/home/pecan//.codeblocks"
    // remove the double //s from filename //+v0.4.11
    m_ConfigFolder.Replace(_T("//"),_T("/"));
    m_ExecuteFolder.Replace(_T("//"),_T("/"));

    // get the CodeBlocks "personality" argument
    wxString m_Personality = Manager::Get()->GetPersonalityManager()->GetPersonality();
	if (m_Personality == wxT("default")) m_Personality = wxEmptyString;
	#if defined(LOGGING)
     LOGIT( _T("Personality is[%s]"), m_Personality.GetData() );
	#endif

    // if DragScroll.ini is in the executable folder, use it
    // else use the default config folder
    m_CfgFilenameStr = m_ExecuteFolder + wxFILE_SEP_PATH;
    if (not m_Personality.IsEmpty()) m_CfgFilenameStr << m_Personality + wxT(".") ;
    m_CfgFilenameStr << _T("DragScroll.ini");

    if (::wxFileExists(m_CfgFilenameStr)) {;/*OK Use exe path*/}
    else //use the default.conf folder
    {   m_CfgFilenameStr = m_ConfigFolder + wxFILE_SEP_PATH;
        if (not m_Personality.IsEmpty()) m_CfgFilenameStr << m_Personality + wxT(".") ;
        m_CfgFilenameStr << _T("DragScroll.ini");
    }
    #if defined(LOGGING)
    LOGIT(_T("DragScroll Config Filename:[%s]"), m_CfgFilenameStr.GetData());
    #endif
    // read configuaton file
    wxFileConfig cfgFile(wxEmptyString,     // appname
                        wxEmptyString,      // vendor
                        m_CfgFilenameStr,   // local filename
                        wxEmptyString,      // global file
                        wxCONFIG_USE_LOCAL_FILE);

	cfgFile.Read(_T("MouseDragScrollEnabled"),  &MouseDragScrollEnabled ) ;
	cfgFile.Read(_T("MouseEditorFocusEnabled"), &MouseEditorFocusEnabled ) ;
	cfgFile.Read(_T("MouseFocusEnabled"),       &MouseFocusEnabled ) ;
	cfgFile.Read(_T("MouseDragDirection"),      &MouseDragDirection ) ;
	cfgFile.Read(_T("MouseDragKey"),            &MouseDragKey ) ;
	cfgFile.Read(_T("MouseDragSensitivity"),    &MouseDragSensitivity ) ;
	cfgFile.Read(_T("MouseWheelZoom"),          &MouseWheelZoom) ;
	cfgFile.Read(_T("PropagateLogZoomSize"),    &PropagateLogZoomSize) ;
	cfgFile.Read(_T("MouseHtmlFontSize"),       &m_MouseHtmlFontSize, 0) ;
	cfgFile.Read(_T("ZoomWindowIds"),           &m_ZoomWindowIds, wxEmptyString) ;
	cfgFile.Read(_T("ZoomFontSizes"),           &m_ZoomFontSizes, wxEmptyString) ;
	cfgFile.Read(_T("MouseWheelZoomReverse"),   &m_MouseWheelZoomReverse, false) ; //2019/03/30

    #ifdef LOGGING
        LOGIT(_T("MouseDragScrollEnabled:%d"),  MouseDragScrollEnabled ) ;
        LOGIT(_T("MouseEditorFocusEnabled:%d"), MouseEditorFocusEnabled ) ;
        LOGIT(_T("MouseFocusEnabled:%d"),       MouseFocusEnabled ) ;
        LOGIT(_T("MouseDragDirection:%d"),      MouseDragDirection ) ;
        LOGIT(_T("MouseDragKey:%d"),            MouseDragKey ) ;
        LOGIT(_T("MouseDragSensitivity:%d"),    MouseDragSensitivity ) ;
        LOGIT(_T("MouseWheelZoom:%d"),          MouseWheelZoom ) ;
        LOGIT(_T("PropagateLogZoomSize:%d"),    PropagateLogZoomSize ) ;
        LOGIT(_T("MouseHtmlFontSize:%d"),       m_MouseHtmlFontSize ) ;
        LOGIT(_T("ZoomWindowIds:[%s]"),         m_ZoomWindowIds.c_str() ) ;
        LOGIT(_T("ZoomFontSizes:[%s]"),         m_ZoomFontSizes.c_str() ) ;
        LOGIT(_T("MouseWheelZoomReverse:[%d]"), m_MouseWheelZoomReverse ) ;
    #endif //LOGGING

    // Fill ZoomWindowIds and ZoomFontSizes arrays from config strings
    // The strings contain last sessions window ids and font sizes
    GetZoomWindowsArraysFrom( m_ZoomWindowIds, m_ZoomFontSizes );

    // Catch creation of windows
    Connect( wxEVT_CREATE,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnWindowOpen);

    // Catch Destroyed windows
    Connect( wxEVT_DESTROY,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnWindowClose);

    // Catch External requests to support a window //(2021/06/25)
    Connect(idDragScrollAddWindow, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Connect(idDragScrollRemoveWindow, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Connect(idDragScrollRescan, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Connect(idDragScrollReadConfig, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Connect(idDragScrollInvokeConfig, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);

    // Set current plugin version
	PluginInfo* pInfo = (PluginInfo*)(Manager::Get()->GetPluginManager()->GetPluginInfo(this));
	pInfo->version = wxT(VERSION);
	// Allow other plugins to find our Event ID
	m_DragScrollFirstId = wxString::Format( _T("%d"), wxEVT_DRAGSCROLL_EVENT);
	pInfo->authorWebsite = m_DragScrollFirstId;

	#if defined(LOGGING)
	LOGIT( _T("DragScroll EventTypes[%d]"), wxEVT_DRAGSCROLL_EVENT);
	#endif

	// register event sink
    Manager::Get()->RegisterEventSink(cbEVT_APP_STARTUP_DONE, new cbEventFunctor<cbDragScroll, CodeBlocksEvent>(this, &cbDragScroll::OnAppStartupDone));
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_CLOSE, new cbEventFunctor<cbDragScroll, CodeBlocksEvent>(this, &cbDragScroll::OnProjectClose));
    Manager::Get()->RegisterEventSink(cbEVT_APP_START_SHUTDOWN, new cbEventFunctor<cbDragScroll, CodeBlocksEvent>(this, &cbDragScroll::OnStartShutdown));

	return ;
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnRelease(bool /*appShutDown*/)
// ----------------------------------------------------------------------------
{
	// do de-initialization for your plugin
	// if appShutDown is false, the plugin is unloaded because Code::Blocks is being shut down,
	// which means you must not use any of the SDK Managers
	// NOTE: after this function, the inherited member variable
	// IsAttached() will be FALSE...

	// Remove all Mouse event handlers
    // Disconnect from creation of windows //(2021/06/25)
    Disconnect( wxEVT_CREATE,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnWindowOpen);

    // Disonnect from Destroyed windows
    Disconnect( wxEVT_DESTROY,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnWindowClose);

    // Disconnect from External requests to support a window
    Disconnect(idDragScrollAddWindow, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Disconnect(idDragScrollRemoveWindow, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Disconnect(idDragScrollRescan, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Disconnect(idDragScrollReadConfig, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);
    Disconnect(idDragScrollInvokeConfig, wxEVT_COMMAND_MENU_SELECTED,
        (wxObjectEventFunction) (wxEventFunction)
        (wxCommandEventFunction) &cbDragScroll::OnDragScrollEvent_Dispatcher);

	// Disconnect all Mouse event handlers
	DetachAll();
}
// ----------------------------------------------------------------------------
cbConfigurationPanel* cbDragScroll::GetConfigurationPanel(wxWindow* parent)
// ----------------------------------------------------------------------------
{
	//create and display the configuration dialog for your plugin
    if(!IsAttached()) {	return 0;}
    // Create a configuration dialogue and hand it off to codeblocks

    //cbConfigurationPanel* pDlg = new cbDragScrollCfg(parent, this);
    cbDragScrollCfg* pDlg = new cbDragScrollCfg(parent, this);
    // set current mouse scrolling options
    pDlg->SetMouseDragScrollEnabled ( MouseDragScrollEnabled );
    pDlg->SetMouseEditorFocusEnabled ( MouseEditorFocusEnabled );
    pDlg->SetMouseFocusEnabled ( MouseFocusEnabled );
    pDlg->SetMouseDragDirection ( MouseDragDirection );
    pDlg->SetMouseDragKey ( MouseDragKey );
    pDlg->SetMouseDragSensitivity ( MouseDragSensitivity );
    pDlg->SetMouseWheelZoom ( MouseWheelZoom );
    pDlg->SetPropagateLogZoomSize ( PropagateLogZoomSize );
    pDlg->SetMouseWheelZoomReverse ( m_MouseWheelZoomReverse ); //2019/03/30

    // when the configuration panel is closed with OK, OnDialogDone() will be called
    return pDlg;
}
// ----------------------------------------------------------------------------
int cbDragScroll::Configure(wxWindow* parent)
// ----------------------------------------------------------------------------
{
	if ( !IsAttached() )
		return -1;

	// Creates and displays the configuration dialog for the plugin
	cbConfigurationDialog dlg(Manager::Get()->GetAppWindow(), wxID_ANY, wxT("DragScroll"));
	cbConfigurationPanel* panel = GetConfigurationPanel(&dlg);
	if (panel)
	{
		dlg.AttachConfigurationPanel(panel);
        if (parent)
            CenterChildOnParent( parent, &dlg);
        else
            PlaceWindow(&dlg,pdlConstrain);

		return dlg.ShowModal() == wxID_OK ? 0 : -1;
	}
	return -1;
}
// ----------------------------------------------------------------------------
void cbDragScroll::CenterChildOnParent(wxWindow* parent, wxWindow* child)
// ----------------------------------------------------------------------------
{
    int displayX; int displayY;
    ::wxDisplaySize(&displayX, &displayY);

    int childx = 1, childy = 1;
    // place bottomLeft child at bottomLeft of parent window
        int childsizex,childsizey;
        parent->GetScreenPosition(&childx,&childy);
        child->GetSize(&childsizex,&childsizey);
        // Make sure child is not off right/bottom of screen
        if ( (childx+childsizex) > displayX)
            childx = displayX-childsizex;
        if ( (childy+childsizey) > displayY)
            childy = displayY-childsizey;
        // Make sure child is not off left/top of screen
        if ( childx < 1) childx = 1;
        if ( childy < 1) childy = 1;

    child->Move( childx, childy);
    return;
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDialogDone(cbDragScrollCfg* pDlg)
// ----------------------------------------------------------------------------
{
    // The configuration panel has run its OnApply() function.
    // So here it's as if we're using ShowModal() and it just returned wxID_OK.

    MouseDragScrollEnabled  = pDlg->GetMouseDragScrollEnabled();
    MouseEditorFocusEnabled = pDlg->GetMouseEditorFocusEnabled();
    MouseFocusEnabled       = pDlg->GetMouseFocusEnabled();
    MouseDragDirection      = pDlg->GetMouseDragDirection();
    MouseDragKey            = pDlg->GetchosenDragKey();
    MouseDragSensitivity    = pDlg->GetMouseDragSensitivity();
    MouseWheelZoom          = pDlg->GetMouseWheelZoom();
    PropagateLogZoomSize    = pDlg->IsLogZoomSizePropagated() and MouseWheelZoom;
    m_MouseWheelZoomReverse = pDlg->GetMouseWheelZoomReverse(); //2019/03/30

    #ifdef LOGGING
     LOGIT(_T("MouseDragScrollEnabled:%d"),     MouseDragScrollEnabled);
     LOGIT(_T("MouseEditorFocusEnabled:%d"),    MouseEditorFocusEnabled);
     LOGIT(_T("MouseFocusEnabled:%d"),          MouseFocusEnabled);
     LOGIT(_T("MouseDragDirection:%d"),         MouseDragDirection);
     LOGIT(_T("MouseDragKey:%d"),              MouseDragKey);
     LOGIT(_T("MouseDragSensitivity:%d"),       MouseDragSensitivity);
     LOGIT(_T("MouseMouseWheelZoom:%d"),        MouseWheelZoom);
     LOGIT(_T("PropagateLogZoomSize:%d"),       PropagateLogZoomSize);
     LOGIT(_T("MouseMouseWheelZoomReverse:%d"), m_MouseWheelZoomReverse); //2019/03/30
     LOGIT(_T("-----------------------------"));
    #endif //LOGGING

    // Post a pending request to later update the configuration requests
    // Executing code here will stall the dlg window on top of the editor
    // This is what's now-a-days called a CallAfter()
    wxUpdateUIEvent evtdone(ID_DLG_DONE);
    evtdone.SetEventObject( m_pCB_AppWindow );
    m_pCB_AppWindow->GetEventHandler()->AddPendingEvent(evtdone);

    // don't delete dlg; Codeblocks should destroy the dialog

}//OnDialogDone
// ----------------------------------------------------------------------------
void cbDragScroll::OnDoConfigRequests(wxUpdateUIEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    // This is an event triggered by OnDialogDone() to update config settings
    #if defined(LOGGING)
    LOGIT(_T("OnDoConfigRequest event"));
    #endif

    // Attach or Detach windows to match  Mouse Enabled config setting
    if (GetMouseDragScrollEnabled() )  //v04.14
    {   if (not m_bNotebooksAttached)
        {
            AttachRecursively(m_pCB_AppWindow);
            m_bNotebooksAttached = true;
        }
    }//if
    else {
        DetachAll();
        m_bNotebooksAttached = false;
    }//else

    // update/write configuaton file
    UpdateConfigFile();

}//OnDoConfigRequests
// ----------------------------------------------------------------------------
void cbDragScroll::UpdateConfigFile()
// ----------------------------------------------------------------------------
{

    // update/write configuaton file

    #if defined(LOGGING)
    LOGIT(_T("UpdateConfigFile"));
    #endif

    wxFileConfig cfgFile(wxEmptyString,     // appname
                        wxEmptyString,      // vendor
                        m_CfgFilenameStr,   // local filename
                        wxEmptyString,      // global file
                        wxCONFIG_USE_LOCAL_FILE);

	cfgFile.Write(_T("MouseDragScrollEnabled"),  MouseDragScrollEnabled ) ;
	cfgFile.Write(_T("MouseEditorFocusEnabled"), MouseEditorFocusEnabled ) ;
	cfgFile.Write(_T("MouseFocusEnabled"),       MouseFocusEnabled ) ;
	cfgFile.Write(_T("MouseDragDirection"),      MouseDragDirection ) ;
	cfgFile.Write(_T("MouseDragKey"),            MouseDragKey ) ;
	cfgFile.Write(_T("MouseDragSensitivity"),    MouseDragSensitivity ) ;
	cfgFile.Write(_T("MouseWheelZoom"),          MouseWheelZoom ) ;
	cfgFile.Write(_T("PropagateLogZoomSize"),    PropagateLogZoomSize ) ;
	cfgFile.Write(_T("MouseHtmlFontSize"),       m_MouseHtmlFontSize ) ;
	cfgFile.Write(_T("MouseWheelZoomReverse"),   m_MouseWheelZoomReverse ) ;

	if ( not m_ZoomWindowIds.IsEmpty() )
	{
        cfgFile.Write(_T("ZoomWindowIds"),       m_ZoomWindowIds ) ;
        cfgFile.Write(_T("ZoomFontSizes"),       m_ZoomFontSizes ) ;
	}

}//UpdateConfigFile
// ----------------------------------------------------------------------------
int cbDragScroll::GetZoomWindowsArraysFrom( wxString zoomWindowIds, wxString zoomFontSizes )
// ----------------------------------------------------------------------------
{
    // Fill ZoomWindowIds and ZoomFontSizes arrays from config strings
    // The strings contain last sessions window ids and font sizes

    wxStringTokenizer ids(  zoomWindowIds, wxT(","));
    wxStringTokenizer sizes(zoomFontSizes, wxT(","));
    while ( ids.HasMoreTokens()  && sizes.HasMoreTokens() )
    {
        long winId ; long size;
        ids.GetNextToken().ToLong(&winId);
        sizes.GetNextToken().ToLong(&size);
        m_ZoomWindowIdsArray.Add( winId );
        m_ZoomFontSizesArray.Add( size );
    }

    return m_ZoomWindowIdsArray.GetCount();
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollEvent_Dispatcher(wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Received a request to process an event
    // Namely, other processes have asked DragScroll for a service
    //  to scroll their window. Eg. Clang_client uses DragScroll to scroll
    //  it's LSP messages window.

    if ( not IsAttached() )
        return;

    int id = event.GetId();

    switch ( id )
    {
        default:
	    if (id == idDragScrollAddWindow)
	    {
	        if (not GetMouseDragScrollEnabled() )
                return;
            OnDragScrollEventAddWindow( event );
	        break;
	    }
	    else if (id == idDragScrollRemoveWindow)
	    {
            OnDragScrollEventRemoveWindow( event );
	        break;
	    }
	    else if (id == idDragScrollRescan)
	    {
 	        if (not GetMouseDragScrollEnabled() )
                return;
            OnDragScrollEventRescan( event );
	        break;
	    }
	    else if (id == idDragScrollReadConfig)
        {
            OnDragScrollEvent_RereadConfig( event );
	        break;
        }
	    else if (id == idDragScrollInvokeConfig)
        {
            OnDragScrollEvent_InvokeConfig( event );
	        break;
        }
    }//switch
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollEventAddWindow(wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Received a request to add a scrollable window

    wxWindow* pWin = (wxWindow*)event.GetEventObject();
    wxString winName = event.GetString();
    if ( (not winName.IsEmpty()) && (wxNOT_FOUND == m_UsableWindows.Index(winName)) )
        m_UsableWindows.Add(winName);

    Attach( pWin );

    #if defined(LOGGING)
    int windowID = event.GetId();
    LOGIT( _T("cbDragScroll::OnDragScrollEvent AddWindow[%d][%p][%s]"), windowID, pWin, pWin->GetName().c_str());
    #endif
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollEventRemoveWindow(wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Received a request to remove a window pointer
    // from our array of monitored windows
    wxWindow* pWin = (wxWindow*)event.GetEventObject();
    Detach( pWin );

    #if defined(LOGGING)
    int windowID = event.GetId();
    wxString winName = "NotFound";
    if (winExists(pWin))
        winName = pWin->GetName();
    //Don't try to obtain window internals. Window could be destroyed already.
    //wxString winName = pWin->GetName().GetData(); <== causes crash
    LOGIT( _T("cbDragScroll::OnDragScrollEvent RemoveWindow[%d][%p][%s]"), windowID, pWin, winName);
    #endif
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollEventRescan(wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Received a request to rescan for child windows starting
    // at the given event.GetWindow() pointer. This allows us
    // to scroll windows not on the main frame tree.

    // But first, clean out any dead window pointers. This occurs
    // when a window is deleted w/o being closed first, eg.
    // ThreadSearch cbStyledTextCtrl preView control
    CleanUpWindowPointerArray();

    // Rescan for scrollable children starting from the window provided
    wxWindow* pWin = (wxWindow*)event.GetEventObject();
    wxString winName = event.GetString();
    if ( (not winName.IsEmpty()) && (wxNOT_FOUND == m_UsableWindows.Index(winName)) )
        m_UsableWindows.Add(winName);
    if (pWin)
        AttachRecursively( pWin );

    #if defined(LOGGING)
    if (pWin)
    LOGIT( _T("cbDragScroll::OnDragScrollEvent Rescan[%p][%s]"), pWin, pWin->GetName().c_str());
    #endif
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollEvent_RereadConfig(wxCommandEvent& /*event*/ )
// ----------------------------------------------------------------------------
{
    #if defined(LOGGING)
    LOGIT( _T("CodeSnippets:DragScroll RereadConfig"));
    #endif

    wxString cfgFilenameStr = m_CfgFilenameStr;
    #if defined(LOGGING)
    LOGIT(_T("DragScroll Config Filename:[%s]"), cfgFilenameStr.GetData());
    #endif
    // read configuaton file
    wxFileConfig cfgFile(wxEmptyString,     // appname
                        wxEmptyString,      // vendor
                        cfgFilenameStr,     // local filename
                        wxEmptyString,      // global file
                        wxCONFIG_USE_LOCAL_FILE);

	cfgFile.Read(_T("MouseDragScrollEnabled"),  &MouseDragScrollEnabled ) ;
	cfgFile.Read(_T("MouseEditorFocusEnabled"), &MouseEditorFocusEnabled ) ;
	cfgFile.Read(_T("MouseFocusEnabled"),       &MouseFocusEnabled ) ;
	cfgFile.Read(_T("MouseDragDirection"),      &MouseDragDirection ) ;
	cfgFile.Read(_T("MouseDragKey"),            &MouseDragKey ) ;
	cfgFile.Read(_T("MouseDragSensitivity"),    &MouseDragSensitivity ) ;
	cfgFile.Read(_T("MouseWheelZoom"),          &MouseWheelZoom) ;
	cfgFile.Read(_T("PropagateLogZoomSize"),    &PropagateLogZoomSize) ;
	cfgFile.Read(_T("MouseHtmlFontSize"),       &m_MouseHtmlFontSize, 0 ) ;
	cfgFile.Read(_T("MouseWheelZoom"),          &m_MouseWheelZoomReverse) ; //2019/03/30

	//-// Don't allow less than 10 mils on context/scroll delay.
	//-if ( MouseContextDelay < 10) { MouseContextDelay = 10;}

    #ifdef LOGGING
        LOGIT(_T("MouseDragScrollEnabled:%d"),  MouseDragScrollEnabled ) ;
        LOGIT(_T("MouseEditorFocusEnabled:%d"), MouseEditorFocusEnabled ) ;
        LOGIT(_T("MouseFocusEnabled:%d"),       MouseFocusEnabled ) ;
        LOGIT(_T("MouseDragDirection:%d"),      MouseDragDirection ) ;
        LOGIT(_T("MouseDragKey:%d"),            MouseDragKey ) ;
        LOGIT(_T("MouseDragSensitivity:%d"),    MouseDragSensitivity ) ;
        LOGIT(_T("MouseWheelZoom:%d"),          MouseWheelZoom ) ;
        LOGIT(_T("PropagateLogZoomSize:%d"),    PropagateLogZoomSize ) ;
        LOGIT(_T("MouseHtmlFontSize:%d"),       m_MouseHtmlFontSize ) ;
        LOGIT(_T("MouseWheelZoom:%d"),          MouseWheelZoom ) ;
        LOGIT(_T("MouseWheelZoomReverse:%d"),   m_MouseWheelZoomReverse ) ; //2019/03/30
    #endif //LOGGING

}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollEvent_InvokeConfig(wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    wxWindow* parent = (wxWindow*)event.GetEventObject() ;
    Configure( parent );
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnDragScrollTestRescan(DragScrollEvent& /*event*/ )
// ----------------------------------------------------------------------------
{
    #if defined(LOGGING)
    LOGIT( _T("TESING DragScrollevent"));
    #endif
}
// ----------------------------------------------------------------------------
void cbDragScroll::CleanUpWindowPointerArray()
// ----------------------------------------------------------------------------
{
    unsigned int i = 0;
    while (i < m_WindowPtrs.GetCount() )
    {
        if ( not winExists((wxWindow*)m_WindowPtrs.Item(i)) )
        {    m_WindowPtrs.RemoveAt(i);
            #if defined(LOGGING)
            //LOGIT( _T("csDragScroll CleanedUp[%d][%p]"), i, m_WindowPtrs.Item(i));
            #endif
        }
        else
            ++i;
    }
}
// ----------------------------------------------------------------------------
MouseEventsHandler* cbDragScroll::GetMouseEventsHandler()
// ----------------------------------------------------------------------------
{
    if (not m_pMouseEventsHandler)
        m_pMouseEventsHandler = new MouseEventsHandler();
    return m_pMouseEventsHandler;
}
// ----------------------------------------------------------------------------
bool cbDragScroll::IsAttachedTo(wxWindow* p)
// ----------------------------------------------------------------------------
{
    // Test if the window has already been recorded by DragScroll
    if (not m_WindowPtrs.size())
        return false;
    if ( wxNOT_FOUND == m_WindowPtrs.Index(p))
        return false;
    #if defined(LOGGING)
    LOGIT( _T("IsAttachedTo previously[%p][%s]"), p, p->GetName().c_str());
    #endif
    return true;

}//IsAttachedTo
// ----------------------------------------------------------------------------
void cbDragScroll::Attach(wxWindow *pWin)
// ----------------------------------------------------------------------------{
{
	if (!pWin || IsAttachedTo(pWin))
		return;		// already attached !!!

    // allow only static windows to be attached by codeblocks
    // Disappearing frames/windows cause crashes
    // eg., wxArrayString m_UsableWindows = "sciwindow notebook";

    wxString windowName = pWin->GetName().MakeLower();

    wxString windowTitle, windowLabel;
    if (pWin->IsKindOf(CLASSINFO(wxFrame)))
        windowTitle = ((wxFrame*)pWin)->GetTitle();
    if (pWin->IsKindOf(CLASSINFO(wxWindow)))
        windowLabel = pWin->GetLabel();
    if (windowTitle.Length() or windowLabel.Length() )
    {
        // Do not attach to our log window
        if ((windowTitle == "Dragscroll Log")
            or (windowLabel == "DragScroll Log") )
        return;
    }

    if (wxNOT_FOUND == m_UsableWindows.Index(windowName,false))
     {
        #if defined(LOGGING)
        LOGIT(wxT("cbDS::Attach skipping [%s]"), pWin->GetName().c_str());
        #endif
        return;
     }
    #if defined(LOGGING)
    LOGIT(wxT("cbDS::Attach - attaching to [%s][%d][%p]"), pWin->GetName().c_str(),pWin->GetId(),pWin);
    #endif

    // add window to our array, create a mouse event handler
    // and memorize event handler instance
    m_WindowPtrs.Add(pWin);

    MouseEventsHandler* thisEvtHndlr = GetMouseEventsHandler();

    pWin->Connect(wxEVT_MIDDLE_DOWN,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseMiddleDown,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_MIDDLE_UP,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseMiddleUp,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_RIGHT_DOWN,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseRightDown,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_RIGHT_UP,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseRightUp,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_TREE_ITEM_RIGHT_CLICK,
                    (wxObjectEventFunction)(wxEventFunction) //(ph 2024/09/05)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseRightUp,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_MOTION,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseMotion,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_ENTER_WINDOW,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseEnterWindow,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_LEAVE_WINDOW,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseLeaveWindow,
                     NULL, thisEvtHndlr);
    pWin->Connect(wxEVT_MOUSEWHEEL,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&cbDragScroll::OnMouseWheel,
                     NULL, this);
    pWin->Connect(wxEVT_MOUSE_CAPTURE_LOST,
                    (wxObjectEventFunction)(wxEventFunction)
                    (wxMouseEventFunction)&MouseEventsHandler::OnMouseCaptureLost,
                     NULL, this);

    #if defined(LOGGING)
     LOGIT(_T("cbDS:Attach Window:%p Handler:%p"), pWin,thisEvtHndlr);
    #endif
}

// ----------------------------------------------------------------------------
void cbDragScroll::AttachRecursively(wxWindow *p)
// ----------------------------------------------------------------------------{
{
    // Add attachable allowable window to our windows array

 	if (!p)
		return;

	Attach(p);

 	// this is the standard way wxWidgets uses to iterate through children...
	for (wxWindowList::compatibility_iterator node = p->GetChildren().GetFirst();
		node;
		node = node->GetNext())
	{
		// recursively attach each child
		wxWindow *win = (wxWindow *)node->GetData();

		if (win)
			AttachRecursively(win);
	}
}
// ----------------------------------------------------------------------------
wxWindow* cbDragScroll::FindWindowRecursively(const wxWindow* parent, const wxWindow* handle)
// ----------------------------------------------------------------------------{
{
    // Find a window in the windows chain by window handle

    if ( parent )
    {
        // see if this is the window we're looking for
        if ( parent == handle )
            return (wxWindow *)parent;

        // It wasn't, so check all its children
        for ( wxWindowList::compatibility_iterator node = parent->GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            // recursively check each child
            wxWindow *win = (wxWindow *)node->GetData();
            wxWindow *retwin = FindWindowRecursively(win, handle);
            if (retwin)
                return retwin;
        }
    }

    // Not found
    return NULL;
}
// ----------------------------------------------------------------------------
wxWindow* cbDragScroll::winExists(wxWindow *parent)
// ----------------------------------------------------------------------------{
{
    // Verify that a window actually exists

    if ( !parent )
        return NULL;

    // start at very top of wx's windows
    for ( wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
          node;
          node = node->GetNext() )
    {
        // recursively check each window & its children
        wxWindow* win = node->GetData();
        wxWindow* retwin = FindWindowRecursively(win, parent);
        if (retwin)
            return retwin;
    }

    return NULL;
}//winExists
// ----------------------------------------------------------------------------
void cbDragScroll::Detach(wxWindow* pWindow)
// ----------------------------------------------------------------------------
{
    // Remove a window from our windows array

    if ( (pWindow) && (m_WindowPtrs.Index(pWindow) != wxNOT_FOUND))
    {
         #if defined(LOGGING)
          LOGIT(_T("cbDS:Detaching %p"), pWindow);
         #endif

        m_WindowPtrs.Remove(pWindow);

        MouseEventsHandler* thisEvtHandler = GetMouseEventsHandler();
        // If win already deleted, dont worry about disconnectng events
	    if ( not winExists(pWindow) )
	    {
            #if defined(LOGGING)
	        LOGIT(_T("cbDS:Detach window NOT found %p Handlr: %p"),
                    pWindow, thisEvtHandler);
            #endif
            return;
	    } else {
            pWindow->Disconnect(wxEVT_MIDDLE_DOWN,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseMiddleDown,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_MIDDLE_UP,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseMiddleUp,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_RIGHT_DOWN,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseRightDown,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_RIGHT_UP,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseRightUp,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_MOTION,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseMotion,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_ENTER_WINDOW,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseEnterWindow,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_LEAVE_WINDOW,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&MouseEventsHandler::OnMouseLeaveWindow,
                             NULL, thisEvtHandler);
            pWindow->Disconnect(wxEVT_MOUSEWHEEL,
                            (wxObjectEventFunction)(wxEventFunction)
                            (wxMouseEventFunction)&cbDragScroll::OnMouseWheel,
                             NULL, this);

        }//else

        #if defined(LOGGING)
         LOGIT(_T("Detach: Editor:%p EvtHndlr: %p"),pWindow,thisEvtHandler);
        #endif
    }//if (pWindow..
}//Detach
// ----------------------------------------------------------------------------
void cbDragScroll::DetachAll()
// ----------------------------------------------------------------------------
{
	// delete all handlers

	#if defined(LOGGING)
	LOGIT(wxString::Format("cbDS:DetachAll - detaching all [%zu] targets", m_WindowPtrs.GetCount()));
	#endif

    // Detach from memorized windows and remove event handlers
    while( m_WindowPtrs.GetCount() )
    {
	    wxWindow* pw = (wxWindow*)m_WindowPtrs.Item(0);
        Detach(pw);
    }//elihw

    m_WindowPtrs.Empty();

    // say no windows attached
    m_bNotebooksAttached = false;
    //-m_pSearchResultsWindow = 0;
    return;

}//DetachAll
// ----------------------------------------------------------------------------
wxString cbDragScroll::FindAppPath(const wxString& argv0, const wxString& cwd, const wxString& appVariableName)
// ----------------------------------------------------------------------------
{
    // Find the absolute path where this application has been run from.
    // argv0 is wxTheApp->argv[0]
    // cwd is the current working directory (at startup)
    // appVariableName is the name of a variable containing the directory for this app, e.g.
    // MYAPPDIR. This is checked first.

    wxString str;

    // Try appVariableName
    if (!appVariableName.IsEmpty())
    {
        str = wxGetenv(appVariableName);
        if (!str.IsEmpty())
            return str;
    }

#if defined(__WXMAC__) && !defined(__DARWIN__)
    // On Mac, the current directory is the relevant one when
    // the application starts.
    return cwd;
#endif

    if (wxIsAbsolutePath(argv0))
        return wxPathOnly(argv0);
    else
    {
        // Is it a relative path?
        wxString currentDir(cwd);
        if (currentDir.Last() != wxFILE_SEP_PATH)
            currentDir += wxFILE_SEP_PATH;

        str = currentDir + argv0;
        if (wxFileExists(str))
            return wxPathOnly(str);
    }

    // OK, it's neither an absolute path nor a relative path.
    // Search PATH.

    wxPathList pathList;
    pathList.AddEnvList(wxT("PATH"));
    str = pathList.FindAbsoluteValidPath(argv0);
    if (!str.IsEmpty())
        return wxPathOnly(str);

    // Failed
    return wxEmptyString;
}
// ----------------------------------------------------------------------------
//    cbDragScroll Routines to push/remove mouse event handlers
// ----------------------------------------------------------------------------
void cbDragScroll::OnAppStartupDone(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // EVT_APP_STARTUP_DONE

    //attach windows
    #if defined(LOGGING)
    LOGIT(_T("cbDragScroll::AppStartupDone"));
    #endif
    OnAppStartupDoneInit();

    event.Skip();
    return;
}//OnAppStartupDone
// ----------------------------------------------------------------------------
void cbDragScroll::OnAppStartupDoneInit()
// ----------------------------------------------------------------------------
{
    // This routine may be entered multiple times during initialization,
    // but the Attach() routine guards against duplicate window attaches.
    // This function catches windows that open after we initialize.

    #if defined(LOGGING)
    LOGIT( _T("OnAppStartUpDoneInit()"));
    #endif
    if (not GetMouseDragScrollEnabled() )
        return;

    AttachRecursively( m_pCB_AppWindow );
    m_bNotebooksAttached = true;

    // For Linux:
    // OnWindowOpen() misses the first main.cpp open of the StartHere page.
    // So find & issue the users font zoom change here.
    if ( GetMouseWheelZoom() ) do
    {   // Tell mouse handler to initalize the mouseWheel data
        // after the htmlWindow is fully initialized
        const EditorBase* sh = Manager::Get()->GetEditorManager()->GetEditor(_T("Start here"));
        if (not sh) break;
        wxWindow* pWindow = ((dsStartHerePage*)sh)->m_pWin; //htmlWindow
        if (not pWindow) break;
        wxMouseEvent wheelEvt(wxEVT_MOUSEWHEEL);
        wheelEvt.SetEventObject(pWindow);
        wheelEvt.m_controlDown = true;
        wheelEvt.m_wheelRotation = 0;
        wheelEvt.m_wheelDelta = 1; //Avoid FPE wx3.0
        pWindow->GetEventHandler()->AddPendingEvent(wheelEvt);
    }while(0);

    // Issue SetFont() for saved font sizes on our monitored windows
    // Arrays contain the previous sessions window id and the font size for that window
    if ( GetMouseWheelZoom() )
    for (int i=0; i<(int)m_WindowPtrs.GetCount(); ++i)
    {
        wxWindow* pWindow = (wxWindow*)m_WindowPtrs.Item(i);
        // verify the window still exists (htmWindows disappear without notice)
        if (not winExists(pWindow))
        {
            m_WindowPtrs.RemoveAt(i);
            --i;
            if (i<0) break;
            continue;
        }
        // check for font size change
        #if defined(LOGGING)
        LOGIT( _T("pWindow GetName[%s]"), pWindow->GetName().c_str());
        #endif
        if ( (pWindow->GetName() not_eq  _T("SCIwindow"))
                and (pWindow->GetName() not_eq  _T("htmlWindow")) )
        {
            int windowId = pWindow->GetId();
            int posn;
            int fontSize = 0;
            wxFont font;
            if ( wxNOT_FOUND not_eq (posn = m_ZoomWindowIdsArray.Index( windowId)) )
            {
                fontSize = m_ZoomFontSizesArray.Item(posn);
                font = pWindow->GetFont();
                font.SetPointSize( fontSize );
                pWindow->SetFont( font);
                // Tell mouse handler to refresh new font size
                // after the window is fully initialied
                wxMouseEvent wheelEvt(wxEVT_MOUSEWHEEL);
                wheelEvt.SetEventObject(pWindow);
                wheelEvt.m_controlDown = true;
                wheelEvt.m_wheelRotation = 0;
                wheelEvt.m_wheelDelta = 1; //Avoid FPE wx3.0
                pWindow->GetEventHandler()->AddPendingEvent(wheelEvt);
                #if defined(LOGGING)
                //LOGIT( _T("OnAppStartupDoneInit Issued Wheel Zoom event 0[%p]size[%d]"),pWindow, fontSize);
                #endif
            }//if
        }//if
    }//for

}
// ----------------------------------------------------------------------------
void cbDragScroll::OnProjectClose(CodeBlocksEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    // Ask DragScoll to clean up any orphaned windows and rescan for
    // (validate) our monitored windows

    // We only want to know if there are no more projects open before
    // cleaning up

    if (Manager::IsAppShuttingDown())
        return;

    ProjectsArray* prjary = Manager::Get()->GetProjectManager()->GetProjects();
    if ( prjary->GetCount() )
        return;

    // Issue a pending event so we rescan after other events have settled down.
    DragScrollEvent dsEvt(wxEVT_DRAGSCROLL_EVENT, idDragScrollRescan);
    dsEvt.SetEventObject( m_pCB_AppWindow);
    dsEvt.SetString( _T("") );
    this->AddPendingEvent(dsEvt);
    return;
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnStartShutdown(CodeBlocksEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    //NOTE: CB is invoking this event TWICE

    // Save DragScroll configuration .ini file

    // Create ini entry with this session Zoom window id's & Zoom font sizes
    // They'll be used next session to re-instate the zoom font sizes

    // Make sure the array is clear of destroyed window pointers
    CleanUpWindowPointerArray();
    wxString zoomWindowIds = _T("");
    wxString zoomFontSizes = _T("");
    if ( GetMouseWheelZoom() )
    {
        for (size_t i=0; i<m_WindowPtrs.GetCount(); ++i )
        {
            #if defined(LOGGING)
            LOGIT( _T("OnStartShutdown[%d][%p][%d]"), int(i), m_WindowPtrs.Item(i),int(((wxWindow*)m_WindowPtrs.Item(i))->GetId()));
            #endif
            zoomWindowIds << wxString::Format(_T("%d,"),((wxWindow*)m_WindowPtrs.Item(i))->GetId() );
            wxFont font = ((wxWindow*)m_WindowPtrs.Item(i))->GetFont();
            zoomFontSizes << wxString::Format(_T("%d,"),font.GetPointSize() );
            #if defined(LOGGING)
            //LOGIT( _T("WindowPtr[%p]Id[%d]fontSize[%d]"),
            //    m_WindowPtrs.Item(i),
            //    ((wxWindow*)m_WindowPtrs.Item(i))->GetId(),
            //    font.GetPointSize()
            //);
            #endif
        }
        // Remove trailing comma
        zoomWindowIds.Truncate(zoomWindowIds.Length()-1);
        zoomFontSizes.Truncate(zoomFontSizes.Length()-1);
        #if defined(LOGGING)
        //LOGIT( _T("ZoomWindowIds[%s]"), zoomWindowIds.c_str());
        //LOGIT( _T("ZoomFontSizes[%s]"), zoomFontSizes.c_str());
        #endif
    }//if GetMouseWheelZoom

    SetZoomWindowsStrings(zoomWindowIds, zoomFontSizes);

    // Write out any outstanding config data changes
    UpdateConfigFile();
}
// ----------------------------------------------------------------------------
void cbDragScroll::OnWindowOpen(wxEvent& event)
// ----------------------------------------------------------------------------
{
    // wxEVT_CREATE entry

    wxWindow* window = (wxWindow*)(event.GetEventObject());

    // Some code (at times) is not issueing EVT_APP_STARTUP_DONE;
    // so here we do it ourselves. If not initialized and this is the first
    // scintilla window, initialize now.

    if ( (not m_bNotebooksAttached)
        && ( window->GetName().Lower() == wxT("sciwindow")) )
    {
        #if defined(LOGGING)
        LOGIT( _T("OnWindowOpen[%s]"), window->GetName().c_str());
        #endif
        OnAppStartupDoneInit();
    }

    // Attach a window
    if ( m_bNotebooksAttached ) do
    {
        wxWindow* pWindow = (wxWindow*)(event.GetEventObject());
        if ( pWindow )
        {
            #if defined(LOGGING)
            LOGIT( _T("OnWindowOpen by[%s]"), pWindow->GetName().GetData());
            #endif
            if ( (pWindow->GetName() ==  _T("SCIwindow"))
                or (pWindow->GetName() ==  _T("htmlWindow")) )
            {
                // Clean this address from our array of window pointers.
                // Some child windows are deleted by wxWidgets and never get
                // a wxEVT_DESTROY (eg., htmlWindow in StartHerePage).
                Detach(pWindow);

                #ifdef LOGGING
                    LOGIT( _T("OnWindowOpen Attaching:%p name: %s"),
                            pWindow, pWindow->GetName().GetData() );
                #endif //LOGGING
                // Cleanly re-attach the window
                Attach(pWindow);
            }
        }//fi (ed)
        // For Windows: issueing the StartHerePage font change here
        // avoids the "font pop" redraw seen on Linux.
        if ( pWindow->GetName() ==  _T("htmlWindow"))
        {   if ( GetMouseWheelZoom() )
                {
                    // Tell mouse handler to initalize the font
                    // after the htmlWindow is fully initialied
                    wxMouseEvent wheelEvt(wxEVT_MOUSEWHEEL);
                    wheelEvt.SetEventObject(pWindow);
                    wheelEvt.m_controlDown = true;
                    wheelEvt.m_wheelRotation = 0; //set user font
                    wheelEvt.m_wheelDelta = 1; //Avoid FPE wx3.0
                    pWindow->GetEventHandler()->AddPendingEvent(wheelEvt);
                    #if defined(LOGGING)
                    //LOGIT( _T("OnWindowOpen Issued htmlWindow Zoom event"));
                    #endif
                }
            break;
        }

    }while(0);//if

    event.Skip();
}//OnWindowOpen
// ----------------------------------------------------------------------------
void cbDragScroll::OnWindowClose(wxEvent& event)
// ----------------------------------------------------------------------------
{
    // wxEVT_DESTROY entry

    wxWindow* pWindow = (wxWindow*)(event.GetEventObject());
    #if defined(LOGGING)
    //LOGIT( _T("OnWindowClose[%p]"), pWindow);
    #endif
    if ( (pWindow) && (m_WindowPtrs.Index(pWindow) != wxNOT_FOUND))
    {   // window is one of ours
        Detach(pWindow);
        #ifdef LOGGING
         LOGIT( _T("OnWindowClose Detached %p"), pWindow);
        #endif //LOGGING
    }
    event.Skip();
}//OnWindowClose
// ----------------------------------------------------------------------------
void cbDragScroll::OnMouseWheel(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
    // This routines does a font zoom when the user uses ctrl-wheelmouse
    // on one of our monitored tree, list, or text (non-scintilla) controls.

    cbDragScroll* pDSplugin = cbDragScroll::pDragScroll;
    if (not pDSplugin->GetMouseWheelZoom() )
    {   event.Skip(); return; }

    //remember event window pointer
    wxObject* pEvtObject = event.GetEventObject();
    wxWindow* pEvtWindow = (wxWindow*)pEvtObject;

    // Ctrl-MouseWheel Zoom (ctrl-mousewheel)
    if ( event.GetEventType() ==  wxEVT_MOUSEWHEEL)
    {
        bool mouseCtrlKeyDown = event.ControlDown();
        if (not mouseCtrlKeyDown) {event.Skip(); return;}
        if ( pEvtWindow->GetName() == _T("SCIwindow"))
        {
            // check if user wants default zoom direction reversed //2019/03/30
            if (not GetMouseWheelZoomReverse()) //2019/03/30
                { event.Skip(); return; }
            else
            {
                int direction = event.GetWheelRotation();
                event.m_wheelRotation = (direction *= -1);
                event.Skip();
                return;
            }
        }

        if ( pEvtWindow->GetName() == _T("htmlWindow"))
        {
            if ( not OnMouseWheelInHtmlWindowEvent(event))
                event.Skip();
            return;
        }

        #ifdef LOGGING
        //LOGIT(wxT("OnMouseWheel[%p][%d][%s]"), pEvtWindow, pEvtWindow->GetId(), pEvtWindow->GetName().c_str() );
        #endif

        int nRotation = event.GetWheelRotation();
        if (GetMouseWheelZoomReverse() )        //2019/03/30
            nRotation *= -1;
        wxFont ctrlFont = pEvtWindow->GetFont();

        if ( nRotation > 0)
            ctrlFont.SetPointSize( ctrlFont.GetPointSize()+1); //2019/03/30
        if ( nRotation < 0)
            ctrlFont.SetPointSize( ctrlFont.GetPointSize()-1);  //2019/03/30
        // a rotation of 0 means to refresh (set) the current window font size
        pEvtWindow->SetFont(ctrlFont);

        // if wxListCtrl, issue SetItemFont() because wxWindow->SetFont() won't do it.
        if ( pEvtWindow->IsKindOf(CLASSINFO(wxListCtrl)) )
        {
            wxListCtrl* pListCtrl = (wxListCtrl*)pEvtWindow;
            for (int i=0; i<pListCtrl->GetItemCount(); ++i)
            {
                wxFont font = pListCtrl->GetItemFont(i);
                font.SetPointSize(ctrlFont.GetPointSize());
                pListCtrl->SetItemFont( i, font );
            }//for
            pEvtWindow->Refresh(); //update colume header fonts
            pEvtWindow->Update();
        }//if

        // If Logger, and option "propagate font size to all loggers" is true,
        // update font for all list & text loggers
        if ( IsLogZoomSizePropagated() )
        {   if ( pEvtWindow->IsKindOf(CLASSINFO(wxListCtrl))
                    or pEvtWindow->IsKindOf(CLASSINFO(wxTextCtrl)) )
               if ( IsLoggerControl((wxTextCtrl*)pEvtWindow) )
               {
                    Manager::Get()->GetConfigManager(_T("message_manager"))->Write(_T("/log_font_size"),ctrlFont.GetPointSize() );
                    Manager::Get()->GetLogManager()->NotifyUpdate();
                    // remove this when SetFont/SetItemFont patch accepted in loggers.cpp
                    // Accepted 2008/08/17
                    //-UpdateAllLoggerWindowFonts(ctrlFont.GetPointSize());
               }
        }
        else //update only this particular logger font
        if ( pEvtWindow->IsKindOf(CLASSINFO(wxListCtrl))
                or pEvtWindow->IsKindOf(CLASSINFO(wxTextCtrl)) )
        {
            dsTextCtrlLogger* pLogger = 0;
            if ( (pLogger = IsLoggerControl((wxTextCtrl*)pEvtWindow)) )
            {
                int newSize = ctrlFont.GetPointSize();
                int oldSize = Manager::Get()->GetConfigManager(_T("message_manager"))->ReadInt(_T("/log_font_size"), platform::macosx ? 10 : 8);
                Manager::Get()->GetConfigManager(_T("message_manager"))->Write(_T("/log_font_size"), newSize );
                pLogger->UpdateSettings();
                Manager::Get()->GetConfigManager(_T("message_manager"))->Write(_T("/log_font_size"),oldSize );
            }
        }

    }//if

}//OnMouseWheel
// ----------------------------------------------------------------------------
bool cbDragScroll::OnMouseWheelInHtmlWindowEvent(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
    // This routines does a font zoom when the user uses ctrl-wheelmouse
    // on one of our monitored html windows

    //- **Debugging**: to verify we're getting the right window
    //-const EditorBase* sh = Manager::Get()->GetEditorManager()->GetEditor(_T("Start here"));
    //-if (not sh) return false;
    //-dsStartHerePage* psh = (dsStartHerePage*)sh;

    #if defined(LOGGING)
    //LOGIT( _T("OnMouseWheelInHtmlWindowEvent Begin"));
    #endif

    wxHtmlWindow* pEvtWindow = (wxHtmlWindow*)event.GetEventObject();
    if ( pEvtWindow->GetName() not_eq  _T("htmlWindow"))
        return false;

    //-Debugging: to verify we get the right window
    //-if ( psh->m_pWin not_eq (wxHtmlWindow*)pEvtWindow )
    //-    return false;
    //-#if defined(LOGGING)
    //-//LOGIT( _T("Have the StartHerePage[%p]"), psh);
    //-#endif

    int nRotation = event.GetWheelRotation();
    if (GetMouseWheelZoomReverse() )            //2019/03/30
        nRotation *= -1;
    wxFont ctrlFont = pEvtWindow->GetFont();
    if (not m_MouseHtmlFontSize)
        m_MouseHtmlFontSize = ctrlFont.GetPointSize();

    // A WHEEL Rotation of 0 means just set the last users font.
    // It's issued by cbDragScroll::OnWindowOpen() to reset users font
    // when the htmlWindow is re-created.
    if ( nRotation > 0)
        ctrlFont.SetPointSize( ++m_MouseHtmlFontSize);  //2019/03/30
    if ( nRotation < 0)
        ctrlFont.SetPointSize( --m_MouseHtmlFontSize);  //2019/03/30
    #if defined(LOGGING)
    //LOGIT( _T("wheel rotation[%d]font[%d]"), nRotation, m_MouseHtmlFontSize);
    #endif

	int sizes[7] = {};
	for (int i = 0; i < 7; ++i)
        sizes[i] = m_MouseHtmlFontSize;
    //-psh->m_pWin->SetFonts(wxEmptyString, wxEmptyString, &sizes[0]); //debug
    pEvtWindow->SetFonts(wxEmptyString, wxEmptyString, &sizes[0]);

    #if defined(LOGGING)
    //LOGIT( _T("OnMouseWheelInHtmlWindowEvent End"));
    #endif
    return true;
}//OnMouseWheelInHtmlWindowEvent
// ----------------------------------------------------------------------------
dsTextCtrlLogger* cbDragScroll::IsLoggerControl(const wxTextCtrl* pControl)
// ----------------------------------------------------------------------------
{
    // Verify that pControl is actually a text or list logger
    dsTextCtrlLogger* pTextLogger;

    LogManager* pLogMgr = Manager::Get()->GetLogManager();
    int nNumLogs = 10; //just a guess
    for (int i=0; i<nNumLogs; ++i)
    {
        LogSlot& logSlot = pLogMgr->Slot(i);
        if (pLogMgr->FindIndex(logSlot.log)== pLogMgr->invalid_log)
            continue;
        pTextLogger = (dsTextCtrlLogger*)logSlot.GetLogger();
        if ( pTextLogger )
            if ( pTextLogger->control == pControl)
                return pTextLogger;
    }//for

    return 0;
}
// ----------------------------------------------------------------------------
///   MouseEventsHandler - MOUSE DRAG and SCROLL Routines
// ----------------------------------------------------------------------------
BEGIN_EVENT_TABLE(MouseEventsHandler, wxEvtHandler)
    //-Deprecated- EVT_MOUSE_EVENTS( MouseEventsHandler::OnMouseEvent)
    // Using Connect/Disconnect events  and EVT_CREATE/EVT_DESTROY
END_EVENT_TABLE()
// ----------------------------------------------------------------------------
MouseEventsHandler::MouseEventsHandler() //ctor
// ----------------------------------------------------------------------------
{
    #if defined(LOGGING)
     //LOGIT(_T("MouseEventsHandler ctor"));
    #endif
    m_Direction      = -1;
    m_gtkContextDelay = 240 ;
    m_skipOrAddCount = 0;
    return;
}//dtor
// ----------------------------------------------------------------------------
MouseEventsHandler::~MouseEventsHandler() //Dtor
// ----------------------------------------------------------------------------
{
    #if defined(LOGGING)
     //LOGIT(_T("MouseEventsHandler dtor"));
    #endif
    return;
}//dtor

///////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
///      MOUSE SCROLLING ROUTINES
// ----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseMiddleDown(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
     LOGIT("\n%s entered %p", __FUNCTION__, event.GetEventObject());

    if ( (not event.GetEventObject()->IsKindOf(CLASSINFO(wxWindow)))
        or (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject())) )
    {
        event.Skip(); return;
    }

    int chosenDragKey = pDSplugin->GetchosenDragKey();
    m_isScrollKeyValid = false; // key needs validation

    // NOte: we know that the middle_mouse key is down since we're here.

    // if chosen drag key does not contain middle mouse, ignore this mouse event
    if ( chosenDragKey < pDSplugin->dragKeyType::Middle_Mouse)
        { event.Skip(); return;}

    bool isAltDown = wxGetKeyState(WXK_ALT);
    bool isShiftDown = wxGetKeyState(WXK_SHIFT);

    // if chosenDragKey is ONLY MIddle_Mouse, there should be no modifier keys down
    if ( (chosenDragKey == pDSplugin->dragKeyType::Middle_Mouse)
        and (isAltDown or isShiftDown))
        {event.Skip(); return;}

    // verify modifier keys if chosenDragKey is [alt|shift] MIddle_mouse
    if (chosenDragKey == pDSplugin->dragKeyType::Alt_Middle_Mouse
        and ((not isAltDown) or (isShiftDown)) )
            {event.Skip(); return;}
    if (chosenDragKey == pDSplugin->dragKeyType::Shift_Middle_Mouse
        and ((not isShiftDown) or (isAltDown)))
            {event.Skip(); return;}

    m_isScrollKeyValid = true;

    //-m_rightIsDown = true;
    m_didScroll =   false;
    m_firstMouseY = event.GetY();
    m_firstMouseX = event.GetX();
    m_lastMouseY = event.GetY();
    m_lastMouseX = event.GetX();

    wxObject* pEvtObject = event.GetEventObject();

    // if StyledTextCtrl, remember for later scrolling
    m_pStyledTextCtrl = 0;
    if ( ((wxWindow*)pEvtObject)->GetName() == _T("SCIwindow"))
        m_pStyledTextCtrl = (wxScintilla*)pEvtObject;
    else
    {
        // not a scintilla editor
        //LOGIT("OnMouseRightDown: window is NOT wxStyledTextCtrl");
    }

    event.Skip();
    return;

}//end OnMouseMiddleDown
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseMiddleUp(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
    LOGIT("%s entered", __FUNCTION__);

    if ( (not event.GetEventObject()->IsKindOf(CLASSINFO(wxWindow)))
        or (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject())) )
        {event.Skip(); return;}

    if ( not m_didScroll )
    {
        LOGIT("%s MIddleMouse did NOT scroll %p", __FUNCTION__, event.GetEventObject());
        event.Skip(); return;
    }

    //remember event window pointer
    wxObject* pEvtObject = event.GetEventObject();
    wxWindow* pWindow = nullptr; wxUnusedVar(pWindow);
    if (pEvtObject && pEvtObject->IsKindOf(wxCLASSINFO(wxWindow)))
        pWindow = dynamic_cast<wxWindow*>(pEvtObject);

    // if StyledTextCtrl, remember
    m_pStyledTextCtrl = nullptr;
    if ( ((wxWindow*)pEvtObject)->GetName() == _T("SCIwindow"))
        m_pStyledTextCtrl = (wxScintilla*)pEvtObject;

    m_isScrollKeyValid = false;

    // deprecated, Capture is  never set for DragScroll
    //    if (pWindow and pWindow->HasCapture())
    //            pWindow->ReleaseMouse();

    m_dragging = false;

    return;

    // If no mouse movement/scrolling took place, must be a middle mouse paste
    if (not m_didScroll)
    {
        if (platform::gtk == true) // only if OnMouseMiddleDown is not already implemented by the OS
            {event.Skip(); return;}

        if (not Manager::Get()->GetConfigManager(_T("editor"))->ReadBool(_T("/enable_middle_mouse_paste"), false))
            {event.Skip(); return;}

        event.Skip();
    }
}//end OnMouseMIddleUp

//=====================================================================================
//   __        __ _             _                         ___          _
//   \ \      / /(_) _ __    __| |  ___ __      __ ___   / _ \  _ __  | | _   _
//    \ \ /\ / / | || '_ \  / _` | / _ \\ \ /\ / // __| | | | || '_ \ | || | | |
//     \ V  V /  | || | | || (_| || (_) |\ V  V / \__ \ | |_| || | | || || |_| |
//      \_/\_/   |_||_| |_| \__,_| \___/  \_/\_/  |___/  \___/ |_| |_||_| \__, |
//                                                                        |___/
//=====================================================================================
#if defined(__WXMSW__)
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseRightDown(wxMouseEvent& event) /// Windows only
// ---------------------------------------------------------------------------
{
    /// See linux_functions.cpp for the Linux version of this funciton

    // **Debugging** //(ph 2024/09/04)
//    wxWindow* pFocusedWin = wxWindow::FindFocus();
//    wxWindow* pThisWindow = dynamic_cast<wxWindow*>(event.GetEventObject());
//    if (pThisWindow)
//       wxMenu* popupMenu = pThisWindow->GetPopupMenu();


    m_didScroll = false;    // (ph 25/10/09)

    LOGIT("\n%s entered %p", __FUNCTION__, event.GetEventObject());
    if (m_ignoreThisEvent)
        { m_ignoreThisEvent--; return; }

    if ( (not event.GetEventObject()->IsKindOf(CLASSINFO(wxWindow)))
        or (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject())) )
        { event.Skip(); return; }

    wxWindow* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());

    int chosenDragKey = pDSplugin->GetchosenDragKey();
    m_isScrollKeyValid = false;

    //Note: We know the Right_mouse_Button is down (because we're here).

    // if chosen drag key contains middle mouse, ignore this Right mouse event
    if ( chosenDragKey >= pDSplugin->dragKeyType::Middle_Mouse)
        { event.Skip(); return; }

    bool isAltDown   = wxGetKeyState(WXK_ALT);
    bool isShiftDown = wxGetKeyState(WXK_SHIFT);

    // if chosenDragKey is ONLY Right_Mouse, there should be no modifier keys down
    if ( (chosenDragKey == pDSplugin->dragKeyType::Right_Mouse)
        and (isAltDown or isShiftDown))
        { event.Skip(); return; }

    // verify modifier keys if chosenDragKey is [alt|shift] Right_mouse
    if (chosenDragKey == pDSplugin->dragKeyType::Alt_Right_Mouse
        and ((not isAltDown) or (isShiftDown)) )
            { event.Skip(); return; }
    if (chosenDragKey == pDSplugin->dragKeyType::Shift_Right_Mouse
        and ((not isShiftDown) or (isAltDown)))
            { event.Skip(); return; }

    // mark the current scroll key valid
    m_isScrollKeyValid = true;

    m_didScroll =   false;
    // use wxGetMousePosition() for consistency throught out the code.
    // Mixing wxGetMousePosition and event.GetX/Y causes grief.
    wxPoint mousePos = pWindow->ScreenToClient(wxGetMousePosition());
    m_firstMouseY = mousePos.y;
    m_firstMouseX = mousePos.x;
    m_lastMouseY  = m_firstMouseY;
    m_lastMouseX  = m_firstMouseX;

    wxObject* pEvtObject = event.GetEventObject();

    // if StyledTextCtrl, remember for later scrolling
    m_pStyledTextCtrl = nullptr;
    if ( ((wxWindow*)pEvtObject)->GetName() == _T("SCIwindow"))
    {
        m_pStyledTextCtrl = (wxScintilla*)pEvtObject;
        // remember the caret position so it can be restored after scrolling
        m_lastCaretPosition = m_pStyledTextCtrl->GetCurrentPos();
    }
    else
    {
        // not a scintilla editor
        //LOGIT("OnMouseRightDown: window is NOT wxStyledTextCtrl");
    }
    return;

}//end OnMouseRightDown
// Simulate a right mouse button down event at a specific position
// ----------------------------------------------------------------------------
void MouseEventsHandler::SimulateRightMouseDown(wxWindow* pWindow, const wxPoint& pos)
// ----------------------------------------------------------------------------
{
    // Simulate a RightMouseDown by sending a context menu event

    LOGIT( _T("%s %p %d %d"), __FUNCTION__, pWindow, pos.x, pos.y);

    // Incomming pos is client position
    // wxEVT_CONTEXT_MENU needs screen position, not client position
    wxPoint screenPos = wxGetMousePosition(); //Screen coordinants
    wxContextMenuEvent contextEvt(wxEVT_CONTEXT_MENU,
                            pWindow->GetId(),
                            screenPos );
    contextEvt.SetEventObject(pWindow);
    LOGIT( _T("%s %p %d %d"), "NewPosns", pWindow, screenPos.x, screenPos.y);
    contextEvt.SetEventObject(pWindow);  // Set the target window
    // Send the event to the window
    pWindow->GetEventHandler()->ProcessEvent(contextEvt);
}
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseRightUp(wxMouseEvent& event) /// Windows only
// ----------------------------------------------------------------------------
{
    // See linux_functions.cpp for the Linux version of this functions

    // Re-issue the mouseDown context for the one we ate while waiting
    // for mouseMotion to tell us if user was really going to scroll or not.

    LOGIT("%s entered", __FUNCTION__);
    assert(m_ignoreThisEvent >= 0);
    if (m_ignoreThisEvent)
    {
        LOGIT( _T("[%s %s]"), __FUNCTION__, "ignored event");
        wxWindow* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());
        if (pWindow)
        {
            wxPoint posn = pWindow->ScreenToClient(wxGetMousePosition());
            if (posn.x){} ; //avoid unused msgs
            LOGIT("mouse@: %p %d %d", pWindow, posn.x, posn.y) ;
        }
        m_ignoreThisEvent--;
        event.Skip(); return;
    }

    if ( not event.GetEventObject()->IsKindOf(CLASSINFO(wxWindow)))
    {event.Skip(); return;}

    if (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject()))
        { event.Skip(); return; }

    wxWindow*   pWindow   = dynamic_cast<wxWindow*>(event.GetEventObject());
    //unused wxListCtrl* pListCtrl = dynamic_cast<wxListCtrl*>(event.GetEventObject());
    wxTreeCtrl* pTreeCtrl = dynamic_cast<wxTreeCtrl*>(event.GetEventObject());

    if ( (not m_didScroll) and (not m_dragging) ) // set in OnMouseMotion() //(ph 2024/09/04)
    {
        LOGIT("%s RightMouse did NOT scroll %p", __FUNCTION__, event.GetEventObject());
        // We need to replace the mouse down/up context menu we've already eaten.

        // If a cb wxTreeCtrl, it needs a wxEVT_TREE_ITEM_RIGHT_CLICK event
        // to replace the one we captured.
        if (pTreeCtrl)
        {
            bool isTreeMultiSelection = pTreeCtrl->GetWindowStyleFlag() & wxTR_MULTIPLE;
            // The event needs a tree itemId
            wxPoint pt = wxGetMousePosition();  // get the mouse position
            pt = pTreeCtrl->ScreenToClient(pt); // convert to client coordinates
            wxTreeItemId itemId = pTreeCtrl->HitTest(pt);
            if (not itemId.IsOk())
            {   event.Skip(); return;}

            // Clear all selections for non-multi selection tree
            if (not isTreeMultiSelection)
                pTreeCtrl->UnselectAll();

            bool ok = SelectItemUnderCursor(pTreeCtrl);
            if (not ok) {event.Skip(); return;}

            // Re-issue the tree right mouse down click that
            // was captured when waiting for possible scrolling to occur.
            wxTreeEvent treeEvent(wxEVT_TREE_ITEM_RIGHT_CLICK, pTreeCtrl, itemId);
            treeEvent.SetEventObject(pTreeCtrl);
            wxKeyEvent keyEvent(wxEVT_KEY_DOWN);
            keyEvent.m_keyCode = WXK_RBUTTON;
            treeEvent.SetKeyEvent(keyEvent);
            treeEvent.SetPoint(wxPoint(m_firstMouseX, m_firstMouseY));
            // add the event, and ignore the next mouse up (avoids loops)
            m_ignoreThisEvent++;    // let next mouse up event pass unmolested
            pTreeCtrl->GetEventHandler()->AddPendingEvent(treeEvent);
            m_popupActive = true; // Tell DragScroll not to molest the next mouse down
        }//endif pTreeCtrl

        // For a non-tree control just re-issue the context menu request
        if (not pTreeCtrl ) //not TreeCtrl
        {
            // Simulate a right-mouse-down/up via a context event
            CallAfter(&MouseEventsHandler::SimulateRightMouseDown, pWindow, wxPoint(m_firstMouseX,m_firstMouseY));
            m_popupActive = true;
        }

        m_isScrollKeyValid = false;
        m_dragging = false;
        return; //own the event
    }//endif not scrolling

    m_isScrollKeyValid = false;
    m_dragging = false;

    return;
}//end OnMouseRightUp
#endif // __WXMSW__



//======================================================================================//
//   ____                                             _____                             //
//  / ___| ___   _ __ ___   _ __ ___    ___   _ __   |  ___|_   _  _ __    ___  ___     //
// | |    / _ \ | '_ ` _ \ | '_ ` _ \  / _ \ | '_ \  | |_  | | | || '_ \  / __|/ __|    //
// | |___| (_) || | | | | || | | | | || (_) || | | | |  _| | |_| || | | || (__ \__ \    //
//  \____|\___/ |_| |_| |_||_| |_| |_| \___/ |_| |_| |_|    \__,_||_| |_| \___||___/    //
//                                                                                      //
//======================================================================================//

// ----------------------------------------------------------------------------
long MouseEventsHandler::GetItemIndexUnderMouse(wxListCtrl* pListCtrl)
// ----------------------------------------------------------------------------
{
    // Get the current mouse position relative to the list control
    wxPoint mousePos = wxPoint(m_firstMouseX,m_firstMouseY);

    // Variable to store hit test flags
    int flags = 0;

    // Hit test to find the item under the mouse cursor
    long itemIndex = pListCtrl->HitTest(mousePos, flags);

    // Check if an item was found
    if (itemIndex == wxNOT_FOUND)
        return wxNOT_FOUND;

    return itemIndex;
}
// ----------------------------------------------------------------------------
bool MouseEventsHandler::SelectItemUnderCursor(wxTreeCtrl* pTreeCtrl)
// ----------------------------------------------------------------------------
{
    wxPoint pt = wxGetMousePosition();
    pt = pTreeCtrl->ScreenToClient(pt); // convert to client coords
    wxTreeItemId itemId = pTreeCtrl->HitTest(pt);
    if (itemId.IsOk())
    {
       pTreeCtrl->SelectItem(itemId);
        return true;
    }
    return false;
}
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseMotion(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
    //LOGIT("%s entered", __FUNCTION__);

    m_didScroll = false;    // (ph 25/10/09)

    if (not m_isScrollKeyValid)
    {
        m_draggingX = m_draggingY = 0;
         event.Skip(); return;
    }

    // Clear m_popupActive since being here means the popup has closed.
    // Popups capture mouse events. so if we get events, popup is gone.
    if (m_popupActive)
    { m_popupActive = false; event.Skip(); return;} //(ph 2024/09/04)

    //if event window pointer not associated with DragScroll, exit
    wxObject* pEvtObject = event.GetEventObject();
    if ( (not pEvtObject->IsKindOf(CLASSINFO(wxWindow)))
        or (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject())) )
    {
        //-m_draggingX = m_draggingY = 0; //(ph 2024/09/04)
        event.Skip(); return;
    }

    wxWindow* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());

    // **Debugging**  Ignore motion in the DragScroll Log window
    #if defined (LOGGING)
    wxWindow* p = pWindow;
    if (p) do
    {
        wxString windowTitle, windowLabel;
        if (p->IsKindOf(CLASSINFO(wxFrame)))
            windowTitle = ((wxFrame*)p)->GetTitle();
        if (p->IsKindOf(CLASSINFO(wxWindow)))
            windowLabel = p->GetLabel();
        if (not p) break;
        //if (windowTitle.Length() or windowLabel.Length() )
        {
            if ((windowTitle == "Dragscroll Log")
                or (windowLabel == "DragScroll Log") )
                return;

            p = p->GetParent();
            if (not p) break;
        }

    }while(p);
    #endif // defined LOGGING

    // If not a user scrolling key is down, exit
    if ( not m_isScrollKeyValid  )
    {
        m_dragging = false;
        m_didScroll = false;
        m_draggingX = m_draggingY = 0;
        event.Skip(); return;
    }

    // if dragging in a non wxWindow, exit
    if (event.Dragging())
    {
        LOGIT("%s Dragging", __FUNCTION__);
        m_dragging = true;
        m_didScroll = false;
        if (not pWindow)
        {
            m_dragging = false;
            m_didScroll = false;
            m_draggingX = m_draggingY = 0;
            LOGIT("OnMouseMotion NOT a window");
            event.Skip(); return;
        }

        LOGIT("EntryMouseXY %d : %d", m_lastMouseX, m_lastMouseY);
        // use ScreenToClient(wxGetMousePosition()) for consistency across event types
        // mixing mouseEvent().GetX/GetY and wxGetMousePositin causes errors.
        wxPoint mousePos = pWindow->ScreenToClient(wxGetMousePosition());
        int currentMouseX = mousePos.x;
        int currentMouseY = mousePos.y;
        int deltaX = currentMouseX - m_lastMouseX;
        int deltaY = currentMouseY - m_lastMouseY;

        m_lastMouseX = currentMouseX;
        m_lastMouseY = currentMouseY;

        // count the pixels if dragged across
        if (m_dragging)
        {
            m_draggingX += abs(deltaX);
            m_draggingY += abs(deltaY);
            LOGIT("Collective pixel dragging %d", m_draggingX + m_draggingY);
        }

        if ( (m_draggingX + m_draggingY) < 3) //(ph 2024/09/03)
        {
            LOGIT("Collective pixels less than 3, exiting.");
            event.Skip(); return;
        }
        //-m_draggingX = m_draggingY = 0; //(ph 2024/09/04)

        LOGIT("currentMouseXY %d : %d", currentMouseX, currentMouseY);
        LOGIT("New LastMouseXY %d : %d", m_lastMouseX, m_lastMouseY);
        LOGIT("deltaXY %d : %d", deltaX, deltaY);

        /// ------- Speed and direction adjustment code begin ------------------
        // Mouse sensitivity is adjusted via config 1  2  3  4  5 6  7  8  9  10
        //                   interpreted as scale  -5 -4 -3 -2 -1 0 +1 +2 +3  +4
        // where a minus is motion events to skip and positive is lines to add
        // The skip events make the mouse work harder to make a scroll
        // while the additional lines make the mouse increase scrolling
        int nMouseSensitivity = pDSplugin->GetMouseDragSensitivity();
        int scrollingSpeed = GetScrollingSpeed(abs(deltaX + deltaY), nMouseSensitivity);
        int scrollAddLines = scrollingSpeed;
        LOGIT("Speed Adjustment %d", scrollAddLines);

        // Skipping Events allows us to vary the scrolling speed relative to mouse speed.
        // The nummber of events to skip comes from GetMouseDragSensitivity();
        // adjust skip event to honor the '1' value, else it gets ignored
        //  because of the decrement before the test (--m_skipEventscount)
        if ((not m_skipEventsCount) and (scrollAddLines < 0) )
            m_skipEventsCount = abs(scrollAddLines) +1;
        if ((not m_skipEventsCount) and (scrollAddLines >= 0) )
            m_skipEventsCount += 1; // keep skip count from going negative
        if (m_skipEventsCount > 1) LOGIT("Event Skipped");
        // decrement the skip events count
        if (--m_skipEventsCount)
            // if skipping events, say we scrolled in order to own the event.
            { m_didScroll = true; return; }

        //// skip count is exhausted. 0 or more lines need to be scrolled
        if ((abs(deltaX)>0 ) or (abs(deltaY) > 0 ) )
        {
            if (scrollAddLines > 0)
            {
                deltaX += (deltaX < 0) ? -scrollAddLines : scrollAddLines;
                deltaY += (deltaY < 0) ? -scrollAddLines : scrollAddLines;
            }
            // Don't allow skewed movement, ie., both x and y scrolling at same time.
            // Scroll the larger of X or Y
            if (deltaX and deltaY)
            {
                if (abs(deltaY) >= abs(deltaX)) deltaX = 0;
                if (abs(deltaX) >= abs(deltaY)) deltaY = 0;
            }

            m_didScroll = true; //say we scrolled (in order to own the event)
        }
        /// ------- Speed and direction adjustment code end --------------------

        if ((deltaX) or (deltaY) )
        {
            LOGIT("Scrolling:Cols/Lines %d : %d", deltaX,deltaY);
            int mouseDirection = pDSplugin->GetMouseDragDirection();
            // Translate the mouseDirection to user specification
            if (not mouseDirection) mouseDirection = -1;
            deltaX *= mouseDirection;
            deltaY *= mouseDirection;

            // If editor window, use scintilla scroll
            if (m_pStyledTextCtrl and (not m_skipEventsCount) )
            {
                m_pStyledTextCtrl->LineScroll (deltaX,deltaY);
                m_didScroll = true;
            }
            else if (pWindow) //use wxControl scrolling
            {
                //use wxTextCtrl scroll for y scrolling
                if ( deltaY)
                    pWindow->ScrollLines(deltaY);
                // use listCtrl for x scrolling
                else if (pWindow->IsKindOf(wxCLASSINFO(wxListCtrl)))
                        ((wxListCtrl*)pWindow)->ScrollList(deltaX<<2,deltaY);
                // Use Scroll for html windows
                else if (pWindow->IsKindOf(wxCLASSINFO(wxHtmlWindow)))
                {
                    // This is a horizontal scroll only. //(ph 2024/09/04)
                    // This code violates the documentation (which is hopeless).
                    // The code is adding pixels to "scroll units"; but converting
                    // pixels to scroll units always results in zero.
                    wxHtmlWindow* htmlWindow = dynamic_cast<wxHtmlWindow*>(pWindow);
                    int xStart, yStart;
                    htmlWindow->GetViewStart(&xStart,&yStart);
                    htmlWindow->Scroll(xStart+deltaX, yStart);
                }
            }

            m_didScroll = true;

        }//endIf deltaX or deltaY
        else
        {   LOGIT("NO Scrolling:Cols/Lines %d : %d", deltaX,deltaY) ;
            m_didScroll = false;
        }
        LOGIT("-------------------------------------");
    }

    if (not m_didScroll)
        // pass the event onward
        event.Skip();

    return;
}//OnMouseMotion

// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseEnterWindow(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
    //remember event window pointer
    wxObject* pEvtObject = event.GetEventObject();
    wxWindow* pWindow = nullptr; wxUnusedVar(pWindow);
    if (pEvtObject && pEvtObject->IsKindOf(wxCLASSINFO(wxWindow)))
        pWindow = dynamic_cast<wxWindow*>(pEvtObject);

    m_pLastEnteredWindow = pWindow;
    wxFrame* frame = dynamic_cast<wxFrame*>(pWindow);
    if (frame)
        m_LastEnteredWindowTitle = frame->GetTitle();
    else
        m_LastEnteredWindowTitle = pWindow->GetLabel();

    LOGIT("%s entered name %p %s", __FUNCTION__, event.GetEventObject(), pWindow?pWindow->GetName():"");
    LOGIT("%s entered title %p %s", __FUNCTION__, event.GetEventObject(), m_LastEnteredWindowTitle);

    // if "focus follows mouse" enabled, set focus to window
    if (pDSplugin->GetMouseFocusEnabled() )
    {   // use EVT_ENTER_WINDOW instead of EVT_MOTION so that double
        // clicking a search window item allows activating the editor cursor
        // while mouse is still in the search window
        LOGIT("%s SetFocus %s", __FUNCTION__, pWindow ? pWindow->GetName() :"unknown" );
        if (pEvtObject) ((wxWindow*)pEvtObject)->SetFocus();
    }
    event.Skip();
}
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseLeaveWindow(wxMouseEvent& event)
// ----------------------------------------------------------------------------
{
    //remember event window pointer
    wxObject* pEvtObject = event.GetEventObject();
    wxWindow* pWindow = nullptr; wxUnusedVar(pWindow);
    if (pEvtObject && pEvtObject->IsKindOf(wxCLASSINFO(wxWindow)))
        pWindow = dynamic_cast<wxWindow*>(pEvtObject);

    m_pLastExitedWindow = pWindow;
    wxFrame* frame = dynamic_cast<wxFrame*>(pWindow);
    if (frame)
        m_LastExitedWindowTitle = frame->GetTitle();
    else
        m_LastExitedWindowTitle = pWindow->GetLabel();

    LOGIT("%s %p %s", __FUNCTION__, event.GetEventObject(), pWindow?pWindow->GetName():"");

    m_isScrollKeyValid = false; //we left the window
    event.Skip();
}
// ----------------------------------------------------------------------------
wxString MouseEventsHandler::GetTopWindowTitle()
// ----------------------------------------------------------------------------
{
    wxWindowList topLevelWindows = wxTopLevelWindows;
    wxWindow* topMostWindow = nullptr;

    // Iterate through the list of top-level windows to find the topmost (active) window
    for (wxWindowList::iterator it = topLevelWindows.begin(); it != topLevelWindows.end(); ++it)
    {
        wxTopLevelWindow* tlw = dynamic_cast<wxTopLevelWindow*>(*it);
        if (tlw) LOGIT("tlwWindow: %s", tlw->GetTitle());
        if (tlw && tlw->IsActive())
        {
            topMostWindow = tlw;
            break;
        }
    }

    if (topMostWindow)
    {
        wxString title;
        wxTopLevelWindow* topFrame = dynamic_cast<wxTopLevelWindow*>(topMostWindow);
        if (topFrame)
        {
            title = topFrame->GetTitle();
        }
        else
        {
            title = topMostWindow->GetLabel();
        }
        LOGIT("Top Window Title: " + title);
        return title;
    }
    else
    {
        LOGIT("No active top-level window found");
        return wxString();
    }
}
//// ----------------------------------------------------------------------------
//double MouseEventsHandler::GetScrollingSpeed(int linesPassed, int mouseSensitivity)
//// ----------------------------------------------------------------------------
//{
//    if (linesPassed < 2)
//        return 0.0; // Add no additional lines if fewer than 2 lines passed
//
//    double S_min = 0.0;
//    double S_max = 10.0;
//    //double k = 0.01;
//    double k = double(mouseSensitivity) * 0.01;
//
//    double result = S_min + (S_max - S_min) * (1 - exp(-k * abs(linesPassed)));
//    return result;
//
//
//    // Formula Where:
//    // S is the scrolling speed.
//    // L is the number of lines the mouse has passed over.
//    // S_min is the minimum scrolling speed, set to 0 for no additional lines when
//    // L=1.
//    // S max is the maximum scrolling speed.
//    // k is a constant that controls how quickly the speed increases
//    //     (higher k means the speed increases faster with the number of lines).
//    // When L=1, S should be 0.As L increases, S approaches S_max
//}
// ----------------------------------------------------------------------------
int MouseEventsHandler::GetScrollingSpeed(int linesPassed, int mouseSensitivity)
// ----------------------------------------------------------------------------
{
    if (mouseSensitivity == 0) return 0;

    // when user is scrolling extreemly slowly, allow a one line scroll
    // no matter what the mouse sensitivity.
    if (linesPassed == 0) return 0;

    // on A scale of 1 to 10 a negative means skip scrolling that number of motion  events
    // A positive means to add that number of lines to scroll.
    switch (mouseSensitivity)
    {
        case 1: return -5;  // |
        case 2: return -4;  // |
        case 3: return -3;  // | Skip this number of events
        case 4: return -2;  // |
        case 5: return -1;  // |
        case 6: return  0;
        case 7: return  1;  // |
        case 8: return  2;  // |
        case 9: return  3;  // | Add this number of lines to scroll
        case 10:return  4;  // |
        default: return 0;
    }
}
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
// ----------------------------------------------------------------------------
{
    /// MouseCapture is deprecated, too problematic
    //remember event window pointer
    wxObject* pEvtObject = event.GetEventObject();
    wxWindow* pWindow = nullptr;
    if (pEvtObject && pEvtObject->IsKindOf(wxCLASSINFO(wxWindow)))
        pWindow = dynamic_cast<wxWindow*>(pEvtObject);

    LOGIT("%s entered", __FUNCTION__, pWindow?pWindow->GetName():"");

    // If we're here, we have to accept the lost mouse
    if (m_dragging)
        m_dragging = false;

    if (pWindow and pWindow->HasCapture())
        pWindow->ReleaseMouse();
}
