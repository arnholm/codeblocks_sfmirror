
//=============================================================================
//    _      _                        ___          _
//   | |    (_) _ __   _   _ __  __  / _ \  _ __  | | _   _
//   | |    | || '_ \ | | | |\ \/ / | | | || '_ \ | || | | |
//   | |___ | || | | || |_| | >  <  | |_| || | | || || |_| |
//   |_____||_||_| |_| \__,_|/_/\_\  \___/ |_| |_||_| \__, |
//                                                    |___/
//==============================================================================

#if not defined(__WXMSW__) /// Linux

#include "wx/wx.h"
#include "wx/treebase.h"
#include <wx/listctrl.h>
#include <wx/weakref.h>

#include "cbplugin.h"  // Assuming cbDragScroll is defined here
#include "linux_functions.h"
#include "dragscroll.h"  // To access MouseEventsHandler class members

/// Linux functions Linux functions Linux functions Linux functions Linux functions
// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{

    enum class ScrollMouseMode
    {
        Idle,
        WaitingForMoveOrMenu,
        Scrolling,
        MenuOpen
    };

    ScrollMouseMode m_scrollMouseMode = ScrollMouseMode::Idle;

    wxWeakRef<wxWindow> m_DragScrolWindow;
    wxPoint m_dragKeyDownPoint;

    bool m_rightMouseCaptured = false;
    bool m_timerInitialized = false;
}
//---------------------------------------------------------------------------------
bool MouseEventsHandler::HasExceededRightDragThreshold( const wxPoint& currentPoint) const
//---------------------------------------------------------------------------------
{
    constexpr int dragThresholdPixels = 4;
    const int dx = currentPoint.x - m_dragKeyDownPoint.x;
    const int dy = currentPoint.y - m_dragKeyDownPoint.y;

    return (abs(dx) >= dragThresholdPixels) ||
                (abs(dy) >= dragThresholdPixels);

}

//---------------------------------------------------------------------------------
void MouseEventsHandler::ReleaseRightMouseCapture()
//---------------------------------------------------------------------------------
{
    wxWindow* const window = m_DragScrolWindow.get();

    if (m_rightMouseCaptured && window && window->HasCapture())
        window->ReleaseMouse();

    m_rightMouseCaptured = false;
}

//---------------------------------------------------------------------------------
void MouseEventsHandler::ResetScrollMouseGesture()
//---------------------------------------------------------------------------------
{
    m_WaitTimer.Stop();
    ReleaseRightMouseCapture();

    m_scrollMouseMode = ScrollMouseMode::Idle;

    m_didScroll = false;
    m_dragging = false;
    m_isScrollKeyValid = false;

    m_scrollAxis = ScrollAxis::Undecided;

    m_DragScrolWindow = nullptr;
}
//---------------------------------------------------------------------------------
void MouseEventsHandler::OpenRightClickContextMenu(wxWindow* window)
//---------------------------------------------------------------------------------
{
     if (!window)
     return;

    if (!pDSplugin->IsAttachedTo(window))
        return;

    const wxPoint clientPos(m_firstMouseX, m_firstMouseY);

    if (auto* pTreeCtrl = dynamic_cast<wxTreeCtrl*>(window))
    {
        pTreeCtrl->UnselectAll();

        int flags = 0;
        wxTreeItemId itemId = pTreeCtrl->HitTest(clientPos, flags);

        if (!itemId.IsOk())
            return;

        pTreeCtrl->SelectItem(itemId);

        wxTreeEvent treeEvent(
            wxEVT_TREE_ITEM_RIGHT_CLICK,
            pTreeCtrl,
            itemId);

        treeEvent.SetEventObject(pTreeCtrl);
        treeEvent.SetPoint(clientPos);

        wxKeyEvent keyEvent(wxEVT_KEY_DOWN);
        keyEvent.m_keyCode = WXK_RBUTTON;

        treeEvent.SetKeyEvent(keyEvent);

        // Synchronous dispatch: menu handling begins now, not later.
        pTreeCtrl->GetEventHandler()->ProcessEvent(treeEvent);

        return;
    }//endif pTreeCtrl

    if (auto* pListCtrl = dynamic_cast<wxListCtrl*>(window))
    {
        const long itemIndex = GetItemIndexUnderMouse(pListCtrl);

        if (itemIndex == wxNOT_FOUND)
            return;

        // Match normal right-click selection behavior:
        // clear current selections and select the item below the mouse.
        for (long i = 0; i < pListCtrl->GetItemCount(); ++i)
        {
            pListCtrl->SetItemState(
                i,
                0,
                wxLIST_STATE_SELECTED);
        }

        pListCtrl->SetItemState(
            itemIndex,
            wxLIST_STATE_SELECTED,
            wxLIST_STATE_SELECTED);

        wxListEvent listEvent(
            wxEVT_LIST_ITEM_RIGHT_CLICK,
            pListCtrl->GetId());

        listEvent.SetEventObject(pListCtrl);
        listEvent.m_itemIndex = itemIndex;

        // Synchronous dispatch: do not queue it with AddPendingEvent().
        pListCtrl->GetEventHandler()->ProcessEvent(listEvent);

        return;
    }//endif pListCtrl

    const wxPoint screenPos = window->ClientToScreen(clientPos);

    wxContextMenuEvent contextEvent(
        wxEVT_CONTEXT_MENU,
        window->GetId(),
        screenPos);

    contextEvent.SetEventObject(window);

    // This asks the target window/application to show its normal
    // context menu immediately.
    window->GetEventHandler()->ProcessEvent(contextEvent);
}

//---------------------------------------------------------------------------------
void MouseEventsHandler::OnLinuxMouseMotion(wxMouseEvent& event)
//---------------------------------------------------------------------------------
{
     if (m_scrollMouseMode == ScrollMouseMode::Idle)
     {
        event.Skip();
        return;
     }

    // If we missed a normal right-up event because focus changed or another
    // control acquired capture, abandon the mouse gesture safely.
    if (not (event.MiddleIsDown() or event.RightIsDown())) //(ph 26/09/02)
    {
        ResetScrollMouseGesture();
        event.Skip();
        return;
    }

    if (m_scrollMouseMode == ScrollMouseMode::MenuOpen)
    {
        // The timer already issued the context-menu event.
        // Do not begin scrolling after the menu path has won.
        return;
        //#warning timer should not issue context menu on MiddleMouse
    }

    const wxPoint currentPoint = event.GetPosition();

    if (m_scrollMouseMode == ScrollMouseMode::WaitingForMoveOrMenu)
    {
        if (not HasExceededRightDragThreshold(currentPoint))
        {
            // Still waiting for either:
            // - significant pointer movement, or
            // - the timer to expire and open the menu.
            //
            // Do not Skip: native right-drag/menu processing must remain
            // suppressed until we make that decision.
            return;
        }

        // Motion happened before the menu timer fired.
        // This is a scrolling gesture, not a context-menu request.
        m_WaitTimer.Stop();

        m_scrollMouseMode = ScrollMouseMode::Scrolling;

        m_didScroll = true;
        m_dragging = true;
    }

    if (m_scrollMouseMode == ScrollMouseMode::Scrolling)
    {
        OnMouseMotion(event);

        // Own the motion event.
        // No event.Skip(): this suppresses ordinary right-drag behavior.
        return;
    }

    return;
}//end OnMouseMotion()

// ----------------------------------------------------------------------------
void MouseEventsHandler::OnTimerEvent(wxTimerEvent& event)
// ----------------------------------------------------------------------------
{
     if (m_scrollMouseMode != ScrollMouseMode::WaitingForMoveOrMenu)
     return;

    wxWindow* const window = m_DragScrolWindow.get();

    if (not window)
    {
        ResetScrollMouseGesture();
        return;
    }

    if (not pDSplugin->IsAttachedTo(window))
    {
        ResetScrollMouseGesture();
        return;
    }

    LOGIT("%s: no right-drag movement; opening context menu for %p",
        __FUNCTION__, window);

    // The normal GTK menu must take over mouse handling now.
    // Release our capture before showing/dispatching the menu.
    ReleaseRightMouseCapture();

    m_scrollMouseMode = ScrollMouseMode::MenuOpen;

    // This synchronously causes your tree/list/context-menu handling to run.
    // It is not event.Skip() on the already-finished mouse-down event.
    const int chosenDragKey = pDSplugin->GetchosenDragKey();
    if (chosenDragKey == pDSplugin->dragKeyType::Right_Mouse)
        OpenRightClickContextMenu(window);

    // Do not reset here. The button is still down, and OnMouseRightUp()
    // will clean up when the user releases it.

}//endif onTimerEvent()

// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseRightDown(wxMouseEvent& event) /// Linux
// ---------------------------------------------------------------------------
{
    LOGIT("\n%s entered %p", __FUNCTION__, event.GetEventObject());

    auto* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());

    if (!pWindow || !pDSplugin->IsAttachedTo(pWindow))
    {
        event.Skip();
        return;
    }

    const int chosenDragKey = pDSplugin->GetchosenDragKey();

    // A middle-mouse drag key does not use this handler.
    if (chosenDragKey >= pDSplugin->dragKeyType::Middle_Mouse)
    {
        event.Skip();
        return;
    }

    const bool isAltDown   = wxGetKeyState(WXK_ALT);
    const bool isShiftDown = wxGetKeyState(WXK_SHIFT);

    // Right mouse alone: neither modifier may be held.
    if (chosenDragKey == pDSplugin->dragKeyType::Right_Mouse &&
        (isAltDown || isShiftDown))
    {
        event.Skip();
        return;
    }

    // Alt + right mouse: Alt only.
    if (chosenDragKey == pDSplugin->dragKeyType::Alt_Right_Mouse &&
        (!isAltDown || isShiftDown))
    {
        event.Skip();
        return;
    }

    // Shift + right mouse: Shift only.
    if (chosenDragKey == pDSplugin->dragKeyType::Shift_Right_Mouse &&
        (!isShiftDown || isAltDown))
    {
        event.Skip();
        return;
    }

    if (!m_timerInitialized)
    {
        m_WaitTimer.Bind(
            wxEVT_TIMER,
            &MouseEventsHandler::OnTimerEvent,
            this);

        m_timerInitialized = true;
    }

    // Defensive cleanup in case a prior gesture was interrupted.
    ResetScrollMouseGesture();

    // if StyledTextCtrl, remember for later scrolling
    m_pStyledTextCtrl = 0;
    if ( ((wxWindow*)event.GetEventObject())->GetName() == _T("SCIwindow"))
        m_pStyledTextCtrl = (wxScintilla*)event.GetEventObject();

    m_scrollMouseMode = ScrollMouseMode::WaitingForMoveOrMenu;

    m_DragScrolWindow = pWindow;
    m_dragKeyDownPoint = event.GetPosition();

    m_firstMouseX = event.GetX();
    m_firstMouseY = event.GetY();

    m_lastMouseX = event.GetX();
    m_lastMouseY = event.GetY();

    m_startPoint = wxPoint(m_firstMouseX, m_firstMouseY);   //(ph 2026-08-31)

    #if defined(LOGGING)
    wxPoint mousePos = pWindow->ScreenToClient(wxGetMousePosition()); //(ph 2026-08-31)
    LOGIT("OnMouseRightDOWN ------------------------------------");
    LOGIT("m_rightDownPoint %d, %d", m_dragKeyDownPoint.x, m_dragKeyDownPoint.y);
    LOGIT("m_startPoint %d, %d", m_firstMouseX, m_firstMouseY);
    LOGIT("ScreenClientXY %d, %d", mousePos.x, mousePos.y);
    LOGIT("EventMouseXY %d, %d", event.GetX(), event.GetY());
    #endif

    m_didScroll = false;
    m_dragging = false;
    m_isScrollKeyValid = true;

    m_scrollAxis = ScrollAxis::Undecided;

    // Capture now so that a right-button drag leaving the window still
    // produces wxEVT_MOTION and wxEVT_RIGHT_UP for this handler.
    if (!pWindow->HasCapture())
    {
        // Capture the mouse if not debugging this code
        // Not capturing seems to work ok also in Release target
        #if !defined(DEBUG_ON) //can't debug if mouse is captured
        pWindow->CaptureMouse();
        m_rightMouseCaptured = true;
        #endif
    }

    // Your menu delay. Adjust as desired.
    m_WaitTimer.Start(250, wxTIMER_ONE_SHOT);

    // Intentionally do NOT event.Skip().
    //
    // On Ubuntu/GTK, allowing the real right-down through here can open
    // the native menu immediately, before we know whether the user is
    // beginning a scrolling gesture.
    return;

}//end OnMouseRightDown

// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseRightUp(wxMouseEvent& event) /// Linux
// ----------------------------------------------------------------------------
{
    LOGIT("%s entered", __FUNCTION__);

    const ScrollMouseMode completedMode = m_scrollMouseMode;
    wxWindow* const window = m_DragScrolWindow.get();

    // This is the timer cancellation you asked for.
    //
    // It prevents OnTimerEvent() from running after a quick button release.
    m_WaitTimer.Stop();

    ReleaseRightMouseCapture();

    if (completedMode == ScrollMouseMode::Scrolling)
    {
        // The right-button gesture was used for scrolling.
        //
        // Own the right-up event so normal GTK processing cannot create
        // a context menu after a scroll gesture.
        ResetScrollMouseGesture();
        return;
    }

    if (completedMode == ScrollMouseMode::MenuOpen)
    {
        // The timer already opened/requested the context menu while the
        // physical button was held down.
        //
        // Own this up event. The native menu interaction gets the release.
        ResetScrollMouseGesture();
        return;
    }

    if (completedMode == ScrollMouseMode::WaitingForMoveOrMenu)
    {
        // The user released before the timeout and did not drag.
        //
        // There was no prior native right-down, because we correctly owned
        // it to prevent a premature menu. Open the logical context menu now.
        if (window && pDSplugin->IsAttachedTo(window))
            { ; } //?OpenRightClickContextMenu(window); not for middle mouse

        ResetScrollMouseGesture();
        return;
    }

    // This right-up did not belong to a gesture owned by this handler.
    ResetScrollMouseGesture();

    event.Skip();

}//end OnMouseRightUp
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseMiddleDown(wxMouseEvent& event) /// Linux
// ---------------------------------------------------------------------------
{
    LOGIT("\n%s entered %p", __FUNCTION__, event.GetEventObject());

    auto* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());

    if (!pWindow || !pDSplugin->IsAttachedTo(pWindow))
    {
        event.Skip();
        return;
    }

    const int chosenDragKey = pDSplugin->GetchosenDragKey();

    // if chosen drag key does not contain middle mouse, ignore this mouse event
    if ( chosenDragKey < pDSplugin->dragKeyType::Middle_Mouse)
        { event.Skip(); return;}

    const bool isAltDown   = wxGetKeyState(WXK_ALT);
    const bool isShiftDown = wxGetKeyState(WXK_SHIFT);

    // Right mouse alone: neither modifier may be held.
    if (chosenDragKey == pDSplugin->dragKeyType::Middle_Mouse &&
        (isAltDown || isShiftDown))
    {
        event.Skip();
        return;
    }

    // Alt + right mouse: Alt only.
    if (chosenDragKey == pDSplugin->dragKeyType::Alt_Middle_Mouse &&
        (!isAltDown || isShiftDown))
    {
        event.Skip();
        return;
    }

    // Shift + middle mouse: Shift only.
    if (chosenDragKey == pDSplugin->dragKeyType::Shift_Middle_Mouse &&
        (!isShiftDown || isAltDown))
    {
        event.Skip();
        return;
    }

    if (not m_timerInitialized)
    {
        m_WaitTimer.Bind(
            wxEVT_TIMER,
            &MouseEventsHandler::OnTimerEvent,
            this);

        m_timerInitialized = true;
    }

    // Defensive cleanup in case a prior gesture was interrupted.
    ResetScrollMouseGesture();

    // if StyledTextCtrl, remember for later scrolling
    m_pStyledTextCtrl = 0;
    if ( ((wxWindow*)event.GetEventObject())->GetName() == _T("SCIwindow"))
        m_pStyledTextCtrl = (wxScintilla*)event.GetEventObject();

    m_scrollMouseMode = ScrollMouseMode::WaitingForMoveOrMenu;

    m_DragScrolWindow = pWindow;
    m_dragKeyDownPoint = event.GetPosition();

    m_firstMouseX = event.GetX();
    m_firstMouseY = event.GetY();

    m_lastMouseX = event.GetX();
    m_lastMouseY = event.GetY();

    m_startPoint = wxPoint(m_firstMouseX, m_firstMouseY);   //(ph 2026-08-31)

    #if defined(LOGGING)
    wxPoint mousePos = pWindow->ScreenToClient(wxGetMousePosition()); //(ph 2026-08-31)
    LOGIT("OnMouseRightDOWN ------------------------------------");
    LOGIT("m_rightDownPoint %d, %d", m_dragKeyDownPoint.x, m_dragKeyDownPoint.y);
    LOGIT("m_startPoint %d, %d", m_firstMouseX, m_firstMouseY);
    LOGIT("ScreenClientXY %d, %d", mousePos.x, mousePos.y);
    LOGIT("EventMouseXY %d, %d", event.GetX(), event.GetY());
    #endif

    m_didScroll = false;
    m_dragging = false;
    m_isScrollKeyValid = true;

    m_scrollAxis = ScrollAxis::Undecided;

    // Capture now so that a right-button drag leaving the window still
    // produces wxEVT_MOTION and wxEVT_RIGHT_UP for this handler.
    if (!pWindow->HasCapture())
    {
        // Capture the mouse if not debugging this code
        // Not capturing seems to work ok also in Release target
        #if !defined(DEBUG_ON) //can't debug if mouse is captured
        pWindow->CaptureMouse();
        m_rightMouseCaptured = true;
        #endif
        #warning generalize rigthMouseCaptured
    }

    // Your menu delay. Adjust as desired.
    m_WaitTimer.Start(250, wxTIMER_ONE_SHOT);

    // Intentionally do NOT event.Skip().
    //
    // On Ubuntu/GTK, allowing the real right-down through here can open
    // the native menu immediately, before we know whether the user is
    // beginning a scrolling gesture.
    return;

}//end OnMouseMiddleDown

// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseMiddleUp(wxMouseEvent& event) /// Linux
// ----------------------------------------------------------------------------
{
    LOGIT("%s entered", __FUNCTION__);

    const ScrollMouseMode completedMode = m_scrollMouseMode;
    wxWindow* const window = m_DragScrolWindow.get();

    // Timer cancel prevents OnTimerEvent() from running after a quick button release.
    m_WaitTimer.Stop();

    ReleaseRightMouseCapture();

    if (completedMode == ScrollMouseMode::Scrolling)
    {
        // The right-button gesture was used for scrolling.
        // Own the right-up event so normal GTK processing cannot create
        // a context menu after a scroll gesture.
        ResetScrollMouseGesture();
        return;
    }

    if (completedMode == ScrollMouseMode::MenuOpen)
    {
        // The timer already opened/requested the context menu while the
        // physical button was held down.
        //
        // Don't own this up event. The MenuOpen on a middle-mouse is meaningless.
        // Allow normal middle mouse paste //(ph 26/09/02)
        ResetScrollMouseGesture();
        event.Skip();
        return;
    }

    if (completedMode == ScrollMouseMode::WaitingForMoveOrMenu)
    {
        // The user released before the timeout and did not drag.
        //
        // There was no prior native mouse-down, because we correctly owned
        // it to prevent a premature menu. Open the logical context menu now.
        if (window && pDSplugin->IsAttachedTo(window))
            {;} //OpenRightClickContextMenu(window); // don't open if middle mouse key
        ResetScrollMouseGesture();
        event.Skip(); // Allow normal mouse paste //(ph 26/09/02))
        return;
    }

    // This right-up did not belong to a gesture owned by this handler.
    ResetScrollMouseGesture();

    event.Skip();

}//end OnMouseRightUp

#endif /// NOT __WXMSW__ Linux
