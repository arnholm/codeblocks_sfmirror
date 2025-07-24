/***************************************************************
 * Name:      dragscroll.h
 * Purpose:   Code::Blocks plugin
 * Author:    Pecan<>
 * Copyright: (c) Pecan
 * License:   GPL
 **************************************************************/

#ifndef DRAGSCROLL_H
#define DRAGSCROLL_H

#if defined(__GNUG__) && !defined(__APPLE__) && !defined(__MINGW32__) && !defined(__MINGW64__)
#pragma interface "dragscroll.h"
#endif

#include <wx/arrstr.h>
#include <wx/dynarray.h>
#include <wx/event.h> // wxEvtHandler
#include <wx/gdicmn.h> //wxPoint
#include <wx/log.h>
#include <wx/timer.h>

#include <cbplugin.h> // the base class we 're inheriting

#include "dragscrollevent.h"
#include "cbstyledtextctrl.h"

// ---------------------------------------------------------------------------
//  Logging / debugging
// ---------------------------------------------------------------------------

//-#undef LOGGING
//-#define LOGIT wxLogDebug
#if defined(LOGGING)
#define LOGIT wxLogMessage
#else
#define LOGIT(...) // Define LOGIT as an empty macro
#endif

// anchor to one and only DragScroll object
class MouseEventsHandler;
class cbDragScrollCfg;
class wxLogWindow;
class wxObject;
class dsTextCtrlLogger;

// ----------------------------------------------------------------------------
//  cbDragScroll class declaration
// ----------------------------------------------------------------------------
class cbDragScroll : public cbPlugin
{
public:
    cbDragScroll();
    ~cbDragScroll();
    int GetConfigurationGroup() const
    {
        return cgEditor;
    }
    void BuildMenu(wxMenuBar* /*menuBar*/)
    {
        return;
    }
    void BuildModuleMenu(const ModuleType /*type*/, wxMenu* /*menu*/, const FileTreeData* /*data*/)
    {
        return;
    }
    bool BuildToolBar(wxToolBar* /*toolBar*/)
    {
        return false;
    }
    void OnAttach(); // fires when the plugin is attached to the application
    void OnRelease(bool appShutDown); // fires when the plugin is released from the application
    virtual cbConfigurationPanel* GetConfigurationPanel(wxWindow* parent);
    virtual int Configure(wxWindow* parent = 0);

    static cbDragScroll* pDragScroll;
protected:
    cbConfigurationPanel* CreatecbCfgPanel(wxWindow* parent);

public:
    void SearchForScrollableWindows()
    {
        OnAppStartupDoneInit();
    }
    void OnDialogDone(cbDragScrollCfg* pdlg);

    bool GetMouseDragScrollEnabled() const
    {
        return MouseDragScrollEnabled;
    }
    bool GetMouseEditorFocusEnabled() const
    {
        return MouseEditorFocusEnabled;
    }
    bool GetMouseFocusEnabled()      const
    {
        return MouseFocusEnabled;
    }
    int  GetMouseDragDirection()     const
    {
        return MouseDragDirection;
    }
    enum dragKeyType
    {
        Right_Mouse = 0,    //windows only
        Alt_Right_Mouse,
        Shift_Right_Mouse,
        Middle_Mouse,
        Alt_Middle_Mouse,
        Shift_Middle_Mouse
    };

    int  GetchosenDragKey()           const
    {
        return MouseDragKey;
    }
    int  GetMouseDragSensitivity()   const
    {
        return MouseDragSensitivity;
    }
    int  GetMouseWheelZoom()         const
    {
        return MouseWheelZoom;
    }
    int  IsLogZoomSizePropagated()   const
    {
        return PropagateLogZoomSize;
    }
    int  GetMouseHtmlFontSize()      const
    {
        return m_MouseHtmlFontSize;
    }
    bool GetMouseWheelZoomReverse()  const
    {
        return m_MouseWheelZoomReverse;    //2019/03/30
    }

    bool IsAttachedTo(wxWindow* p);

    wxWindow* m_pCB_AppWindow;
    //-wxWindow* m_pSearchResultsWindow;

private:
    void OnAppStartupDone(CodeBlocksEvent& event);
    void OnAppStartupDoneInit();
    void OnDoConfigRequests(wxUpdateUIEvent& event);

    void AttachRecursively(wxWindow *p);
    void Detach(wxWindow* thisEditor);
    void DetachAll();
    void Attach(wxWindow *p);
    void DisconnectEvtHandler(MouseEventsHandler* thisEvtHandler);
    void CenterChildOnParent(wxWindow* parent, wxWindow* child);
    dsTextCtrlLogger* IsLoggerControl(const wxTextCtrl* pControl);
    bool OnMouseWheelInHtmlWindowEvent(wxMouseEvent& event);
    void OnProjectClose(CodeBlocksEvent& event);
    void OnStartShutdown(CodeBlocksEvent& event);
    //-void UpdateAllLoggerWindowFonts(const int pointSize);

    void OnDragScrollEvent_Dispatcher(wxCommandEvent& event );
    void OnDragScrollEventAddWindow(wxCommandEvent& event );
    void OnDragScrollEventRemoveWindow(wxCommandEvent& event );
    void OnDragScrollEventRescan(wxCommandEvent& event );
    void OnDragScrollEvent_RereadConfig(wxCommandEvent& event );
    void OnDragScrollEvent_InvokeConfig(wxCommandEvent& event );
    void OnMouseWheel(wxMouseEvent& event);

    void OnDragScrollTestRescan(DragScrollEvent& event );

    wxWindow* winExists(wxWindow *parent);
    wxWindow* FindWindowRecursively(const wxWindow* parent, const wxWindow* handle);
    wxString  FindAppPath(const wxString& argv0, const wxString& cwd, const wxString& appVariableName);
    void      OnWindowOpen(wxEvent& event);
    void      OnWindowClose(wxEvent& event);

    MouseEventsHandler* GetMouseEventsHandler();
    void      CleanUpWindowPointerArray();
    void      SetZoomWindowsStrings(wxString zoomWindowIds, wxString zoomFontSizes)
    {
        m_ZoomWindowIds = zoomWindowIds;
        m_ZoomFontSizes = zoomFontSizes;
    }
    int       GetZoomWindowsArraysFrom(wxString zoomWindowIds, wxString zoomFontSizes);
    void      UpdateConfigFile();

    wxString        m_ConfigFolder;
    wxString        m_ExecuteFolder;
    wxString        m_DataFolder;
    wxString        m_CfgFilenameStr;

    wxArrayString   m_UsableWindows;
    wxArrayPtrVoid  m_WindowPtrs;
    wxLogWindow*    pMyLog;
    bool            m_bNotebooksAttached;

    MouseEventsHandler* m_pMouseEventsHandler; //one and only
    wxString            m_DragScrollFirstId;
    wxString            m_ZoomWindowIds;
    wxString            m_ZoomFontSizes;
    wxArrayInt          m_ZoomWindowIdsArray;
    wxArrayInt          m_ZoomFontSizesArray;

    int  MouseDragKey           ;   //Mouse key used to drag
    bool MouseDragScrollEnabled ;   //Enable/Disable mouse event handler
    bool MouseEditorFocusEnabled;   //Enable/Disable mouse focus() editor
    bool MouseFocusEnabled      ;   //Enable/Disable focus follows mouse
    int  MouseDragDirection     ;   //Move with or opposite mouse
    int  MouseDragSensitivity   ;   //Adaptive speed sensitivity
    int  MouseWheelZoom         ;   //MouseWheel zooms tree, text, list controls
    int  PropagateLogZoomSize   ;   //Propagate Zoom Font size for all logs
    int  m_MouseHtmlFontSize    ;   //Ctrl-MouseWheel zoomed htmlWindow font size
    bool m_MouseWheelZoomReverse;   //ctrl-MouseWheel zoom reverse default direction //2019/03/30

private:
    DECLARE_EVENT_TABLE()

};//cbDragScroll

// ----------------------------------------------------------------------------
//      MOUSE DRAG and SCROLL CLASS
// ----------------------------------------------------------------------------
#include "linux_functions.h"
// ----------------------------------------------------------------------------
class MouseEventsHandler : public wxEvtHandler
// ----------------------------------------------------------------------------
{

public:
    MouseEventsHandler();

    ~MouseEventsHandler();

    void OnMouseMiddleDown(wxMouseEvent& event);
    void OnMouseMiddleUp(wxMouseEvent& event);
    void OnMouseRightDown(wxMouseEvent& event);
    void OnMouseRightUp(wxMouseEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseEnterWindow(wxMouseEvent& event);
    void OnMouseLeaveWindow(wxMouseEvent& event);
    void OnCallAfter(wxWindow* pWindow, wxPoint posn);

    int  GetScrollingSpeed(int linesPassed, int mouseSensitivity);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
    long GetItemIndexUnderMouse(wxListCtrl* pListCtrl);
    bool SelectItemUnderCursor(wxTreeCtrl* pTreeCtrl);
    void SimulateRightMouseDown(wxWindow* pWindow, const wxPoint& pos);

private:
    wxWindow* m_Window;
    //?bool m_rightIsDown = false;  // Linux
    bool m_middleIsDown = false; //  Linux
    bool m_dragging;
    bool m_didScroll;
    int  m_skipEventsCount = 0;
    int  m_firstMouseX;
    int  m_firstMouseY;
    int  m_lastMouseX;
    int  m_lastMouseY;
    int  m_scrollOffsetX;
    int  m_scrollOffsetY;
    int  m_skipOrAddCount;
    bool m_popupActive = false;

    int m_draggingX; //(ph 2024/09/03)
    int m_draggingY;

    wxWindow* m_pLastEnteredWindow = nullptr;
    wxWindow* m_pLastExitedWindow  = nullptr;
    wxString  m_LastEnteredWindowTitle;
    wxString  m_LastExitedWindowTitle;
    int       m_lastCaretPosition = -1;
    bool      m_isScrollKeyValid  = false; //(ph 2024/07/17)

    wxScintilla* m_pStyledTextCtrl = 0;
    cbDragScroll* pDSplugin = cbDragScroll::pDragScroll;

    // Scroll Direction move -1(mouse direction) +1(reverse mouse direction)
    int         m_Direction;
    unsigned    m_gtkContextDelay;
    wxTimer     m_WaitTimer;                //used in Linus functon
    wxObject*   m_pEventObject   = nullptr; //used in Linux function
    wxWindow*   m_pEventWindow   = nullptr; //used in Linux function

    void OnTimerEvent(wxTimerEvent& event);
    int GetUserDragKey()
    {
        return ( cbDragScroll::pDragScroll->GetchosenDragKey() );
    }
    int m_ignoreThisEvent = 0;

    wxString GetTopWindowTitle();

    DECLARE_EVENT_TABLE()
};
//----------------------------------------
#define VERSION "1.4.13 25/01/16"
//----------------------------------------
//versions
// ----------------------------------------------------------------------------
//  Modification History
// ----------------------------------------------------------------------------
// 1/4/13   2025/01/16 Remove tree ctrl scrolling from linux causing stalls.
// 1/4/12   2024/09/6 Optimization of OnRightMouseUp() events.
// 1/4/11   2024/09/5 Allow 3 pixel slop before deciding this is a scroll.
// 1/4/10   2024/09/4 Allow 3 pixel slop before deciding this is a scroll.
//          This makes it easier to place the mouse to request context menu.
//          Add ability to scroll html window horizontally.
// 1/4/09   2024/08/26 For wxTreeCtrl, select the item under the cursor.
// 1/4/08   2024/08/22 Issue wxEVT_CONTEXT_MENU for wxListCtrl with style wxLC_REPORT
// 1/4/07   2024/08/17 Changes to support Linux re: selected item index
// 1/4/06   2024/08/14 OnMouseRightUp(): For wxListCtrl verify an istem is selected
// 1/4/05   2024/08/2  Rework the mouse sensitivity
// 1/4/04   2024/07/29 On Linux issue wxContextMenuEvent for non-wxTree RightMouse simulation
// 1/4/03   2024/07/24 Separated Linux funcs from windows, use wxTimer to determine movement
// 1/4/02   2024/07/20 Fix Linux and add cursors to show scroll direction
// 1.4.01   2024/06/27
//          Simplify mouse handler and resolve Linux inability to scroll with right mouse button
//          Add keys Alt-RightMouset, Shift-RightMouse for Linux and Windows.
// 1.3.32   2023/10/23
//          Comments and debug Logging text updates, no functional changes
// 1.3.31   2022/11/12
//          For focus-follows-mouse, give focus to windows with mouse movement and no clicks
//          Allows log windows to re-click/scroll just by moving mouse.
// 1.3.30  2021/06/25
//          Make it easier for external processes to use DragScroll support
//          Convert event ids to XRCIDs for external use.
//          Add Connect() for wxEVT_COMMAND_MENU_SELECTED for DragScroll events
// 1.3.29 2019/03/30
//          Option to reverse mouse wheel zoom direction
//  Commit  1.3.28 2018/07/9
//          Dont event Skip at dragscroll.cpp 1519. With new wxScintilla, skipping here
//          will annoyingly set the editor caret at the right-click position.
// ----------------------------------------------------------------------------
//  Commit 1.3.27 2015/08/21
//          27 Fixes for wxWidgets 3.0 SIGFPE during mouse wheel scroll
//  Commit 1.3.26 2011/01/2
//          26) Linux: check ClassInfo before scrolling wxListCtrl(s)
// ----------------------------------------------------------------------------
//  Commit 1.3.25 2009/09/6
//          25) re-instate wxCHECK_VERSION(3, 0, 0) accidently removed.
// ----------------------------------------------------------------------------
//  Commit 1.3.24 2009/09/4
//          24) Fix crash when loading .cbp via dde. OnAppStartupDoneInit()
// ----------------------------------------------------------------------------
//  Commit  1.3.23 2008/08/29
//          20) Fixed: font sizes increasing across sessions in OnMouseWheelEvent.
//          21) Save/restore users ctrl-MouseWheel font changes across sessions.
//          22) Fixed: crash caused by failure in CleanUpWindowPointerArray()
//          23) Changed option label "MouseWheelZoom" to "Log MouseWheelZoom" to
//              avoid confusion; even though it applies to other tree and list controls.
// ----------------------------------------------------------------------------
//  Commit  1.3.18 2008/08/23
//          16) Implement Ctrl-MouseWheel zoom for CB list & tree ctrls
//          17) Add config options "Ctrl-WheelMouse Zooms" and "Remember Log Zoom"
//          18) Allow user to ctrl-mouse zoom htmlWindows (eg, Start Here page)
//          19) Fixed: missing events bec.StartHere htmlWindow never issues wxEVT_DESTROY
// ----------------------------------------------------------------------------
//  Commit  1.2.15 2008/05/22
//          08) Allow multiple invocations of OnAppStartupDoneInit() in order
//              to catch windows that open after we intialize. (2008/03/4)
//          09) Conversion to use only one event handler (2008/04/22)
//          10) Optimizations in MouseEventsHandler
//          11) SearchForScrollableWindows() as service to external callers
//          12) Added DragScroll events for rescanning/adding/removing windows
//          13) Optimized/cleaned up MouseEventHandler
//          14) Removed OnWindowOpen EditorManager dependencies
//          15) Add Configure() and event to invoke it. 2008/04/29
// ----------------------------------------------------------------------------
//  Commit  1.2.07 2008/02/2
//          06) Fixed: On some Linux's context menu missing in loggers bec mouse
//              events always reported right-mouse was dragged. (Jens fix)
// ----------------------------------------------------------------------------
//  Commit  1.2.05 2008/01/29
//          05) Killerbot header and const(ipation) changes (2007/12/28)
//          06) Fixed: Middle-mouse conflict with msWindows paste (2008/01/29)
// ----------------------------------------------------------------------------
//  commit  1.2.04 2007/11/29
//          1) add non-pch logmanager header
//          2) correct "focus follows mouse" event on long compilations
//          3) correct editor focus event on long compilations
//          4) remove unused MouseRightKeyCtrl code
// ----------------------------------------------------------------------------
//  Commit  1.1.05
//          05) mandrav changes for sdk RegisterEventSink
// ----------------------------------------------------------------------------
//  Commit  1.1.04 2007/06/27
//          03) Reduce minimum Unix Context menu sentry delay
//          04) Clean up configuration panel
// ----------------------------------------------------------------------------
//  Commit  1.1.02 2007/06/7
//          01) Prepend --personality arg to .ini filename
// ----------------------------------------------------------------------------
//  Commit  1.1.01 2007/04/28
//          01) Removed wx2.6.3 workarounds fixed by wx2.8.3
// ----------------------------------------------------------------------------
//  Commit  1.0.39 2007/02/28
//          39) If exists executable folder .ini file, use it.
// 1.3.34   2024/06/27
//          Rework mouse handler to fix Linux inability to scroll with right mouse button
// 1.3.32   2023/10/23
//          Comments and debug Logging text updates, no functional changes
// 1.3.31   2022/11/12
//          For focus-follows-mouse, give focus to windows with mouse movement and no clicks
//          Allows log windows to re-click/scroll just by moving mouse.
// ----------------------------------------------------------------------------
//  Commit  1.0.38
//          37) Re-instated GTK wxTextCtrl y-axis scrolling
//              GTK cannot scroll a wxListCtrl
//          38) Corrected GTK dialog layout
// ----------------------------------------------------------------------------
//  Commit  1.0.36 2006/12/19 - 2006/12/22
//          34) Added focus follows mouse for wxGTK
//          35) Fixed GTK RightMouse scrolling (avoiding Context Menu conflict)
//          36) Added slider allowing user to set GTK RightMouse scrolling/context menu sensitivity
// ----------------------------------------------------------------------------
//  Commit  1.0.33 2006/12/19
//          33) Removed dependency on EVT_APP_STARTUP_DONE
// ----------------------------------------------------------------------------
//  Commit  1.0.32
//          Determine RTTI GetClassName() in mouse event
// ----------------------------------------------------------------------------
//  Commit  1.0.31 2006/10/18
//          Default Auto focus editor to OFF
// ----------------------------------------------------------------------------
//  Commit  1.0.30 2006/10/16
//          Add focus follow mouse option for MSW
// ----------------------------------------------------------------------------
//  Commit  v0.29 2006/09/22
//          Edited manifest.xml requirement for codeblocks plugins
//          Set displayed Menu About version dynamically.
//          Removed all "eq". Conflicted with wxWidgest hash equates
//          Added (__WXMAC__) to (_WXGTK_)defines to support mac.
// ----------------------------------------------------------------------------
//  commit  v0.28 2006/09/11
// ----------------------------------------------------------------------------
//  open    2006/09/11
//          Complaints that config font was too small on some linux systems
//          and that the background color was incorrect.
//  closed  2006/09/11
//          Removed all Setfont()'s from config. Removed SetBackGroundColorColor()
// ----------------------------------------------------------------------------
//  closed  2006/09/11 open    2006/07/2
//          Clean up code after conversion to Connec/Disconnect event handlers
// ----------------------------------------------------------------------------
//  commit  v0.26 2006/06/29
// ----------------------------------------------------------------------------
//  fixed   v 0.26 2006/06/29
//          Broken by change in plugin interface 1.80
//          Had to add the following to the project file
//
//          Compile Options         #defines
//          -Winvalid-pch           BUILDING_PLUGIN
//          -pipe                   CB_PRECOMP
//          -mthreads               WX_PRECOMP
//          -fmessage-length=0      HAVE_W32API_H
//          -fexceptions            __WXMSW__
//          -include "sdk.h"        WXUSINGDLL
//                                  cbDEBUG
//                                  TIXML_USE_STL
//                                  wxUSE_UNICODE
// ----------------------------------------------------------------------------
//  closed  2006/06/16 open    2006/06/15
//          MouseEventsHandler are being leaked because of split windows. When the
//          windows close, no event is sent to allow cleanup before Destroy()
//          Deleting an eventHandler during Destroy() causes wxWidgets to crash.
//
//          Switched to runtime Connect/Disconnect/EVT_CREATE/EVT_DESTROY
//          in order to stop leaks on split windows & pushed event handlers.
// ----------------------------------------------------------------------------
//  commit  v0.24   2006/06/14
// ----------------------------------------------------------------------------
//  closed  opened    2006/06/11
//          split windows are unrecognized because no event is issued
//          that a split has taken place
//          Had to add wxEVT_CREATE and wxEVT_DESTROY event sinks to catch
//          split window open/close. wxWindows nor CodeBlocks has events
//          usable for the purpose.
// ----------------------------------------------------------------------------
//  commit  v0.23 2006/04/25
// ----------------------------------------------------------------------------
//  fix     v0.23 2006/04/25
//          Added MS windows test for main window because events were getting
//          to mouse handler even though main window didnt have focus
// ----------------------------------------------------------------------------
//  testing v0.22 2006/04/8 Capture ListCtrl Right Mouse Keydown
// ----------------------------------------------------------------------------
//  open    2006/04/8
//          listCtrl windows activate on right mouse click. eg, Search and compiler
//          error windows move the editor window on "right click". Very annoying.
//          Suggest option to hide right mouse keydown from listCtrls
//          Added config option []"Smooth Message List Scrolling"
//                              "(Conflicts with some Context Menus)"
//          Set the Editor focus and Smooth Scrolling to default=false
// ----------------------------------------------------------------------------
//  closed  2006/04/6
//          Resolution of above: event.Skip() on Right mouse Key down.
//          Put back events for listctrl windows
//          Catch address of Search Results window for scrolling.
// ----------------------------------------------------------------------------
//  commit  v0.21 2006/04/6
// ----------------------------------------------------------------------------
//  commit  v0.20 2006/04/5
// ----------------------------------------------------------------------------
//  closed  2006/04/6 open    2006/04/5
//          Conflict with Build messages context menu
//          Removed events for ListCtrl windows
// ----------------------------------------------------------------------------
//  commit  v0.19 committed 2006/03/21
// ----------------------------------------------------------------------------
//  mod     2/25/2006
//          1.Added "Mouse sets Editor Focus" for GTK
//          2.Capture GTK middle mouse key immediately when used for scrolling
// ----------------------------------------------------------------------------
//  commit  v0.18 2/14/2006
// ----------------------------------------------------------------------------
//  mod     2/14/2006 3
//          Added "Mouse sets Editor Focus" configuration item
// ----------------------------------------------------------------------------
//  commit  v0.17 2/13/2006
// ----------------------------------------------------------------------------
//  mod     2/11/2006 7
//          Set focus to editor when mouse in editor window.
//  open    2/11/2006 8
//          mod middle mouse key to avoid waits/delays
//  mod     2/13/2006 9
//          CB_IMPLEMENT_PLUGIN(cbDragScroll, "DragScroll" );
//// ----------------------------------------------------------------------------
//  commit  v0.16 2/4/2006
// ----------------------------------------------------------------------------
//  mod     fixes for unix/GTK
// ----------------------------------------------------------------------------
//  commit  v0.15 2/3/2006 1:03 PM
// ----------------------------------------------------------------------------
//
//  closed  v0.15 2/3/2006 12 open 1/26/2006 5:41 PM
//          Need to stow/read user configuration settings
//          Make MSW changes for GTK
//          Removed-Test on ubuntu
//          Linux version of C::B doesnt compile; will wait for Linux nightly builds(if ever provided)
//// v0.11 1/19/2006
// CodeBlocks SDK version changed. Modified BuildModuleMenu().
// void BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data = 0){ return; }
//
//  mod     v0.13 1/22/2006
//          Added GTK scrolling
//
//  mod     v0.14 1/25/2006 12
//          Added cbConfiguratonDialog for configuration dialog
// V0.7 12/31/2005
//  ---------------------------------------------------------------------------
// Removed event.Skip() when first Right mouse key down to avoid in text/textctrl
// the event.Skip() was activating text/textctrl mouse activity in codeblocks
// eg., mouse RightKeydown in "Search results" caused loading of editors etc.
//  ---------------------------------------------------------------------------
// v0.6 12/30/2005
// ----------------------------------------------------------------------------
//  tiwag conversion to unicode
//	Re: Right Mouse Drag and Scroll Plugin
//  Reply #4 on: December 29, 2005, 08:05:40 PM
//  thanks for this fine plugin,
//  in the course of switching CodeBlocks to unicode build as standard development version
//  i've done some necessary modifications to your code and also updated the project file to
//  the NewBuild global variable system.
//  attached DragScroll 0.4 NewBuild unicode
//  * DragScroll-NewBuild-u.zip (5.77 KB - downloaded 0 times.)
//  http://forums.codeblocks.org/index.php?topic=1594.msg13387#msg13387
//
//  Added scrolling in text and textCtrl windows

#endif // DRAGSCROLL_H
