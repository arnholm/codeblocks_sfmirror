
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
#include "cbplugin.h"  // Assuming cbDragScroll is defined here
#include "linux_functions.h"
#include "dragscroll.h"  // To access MouseEventsHandler class members

/// Linux functions Linux functions Linux functions Linux functions Linux functions
// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{
    bool timerUninialized = true;
}
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnTimerEvent(wxTimerEvent& event)
// ----------------------------------------------------------------------------
{
    // This timer was activated in OnMouseRightDown() to wait for any mouse motion,
    // else re-issue the RightMouseDown invocation to allow the contex menu popup.

    // If no scrolling took place allow the context popup to take place.
    if ( not m_didScroll )
    {
        LOGIT("%s RightMouse did NOT scroll %p", __FUNCTION__, event.GetEventObject());

        //-unused- wxWindow* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());
        wxListCtrl* pListCtrl = dynamic_cast<wxListCtrl*>(m_pEventObject);
        wxTreeCtrl* pTreeCtrl = dynamic_cast<wxTreeCtrl*>(m_pEventObject);

        // If the mouse window is a cb wxTreeCtrl, it needs a RightButton-Down click
        // For example the Open files list window needs this special treatment.
        if (pTreeCtrl)
        {
            pTreeCtrl->UnselectAll();
            wxPoint posn = wxPoint(m_firstMouseX, m_firstMouseY);
            int flags = 0;
             wxTreeItemId itemId = pTreeCtrl->HitTest(posn, flags);
             if (itemId.IsOk())
                pTreeCtrl->SelectItem(itemId);
            if (itemId.IsOk())
            {
                wxTreeEvent treeEvent(wxEVT_TREE_ITEM_RIGHT_CLICK, pTreeCtrl, itemId);
                treeEvent.SetEventObject(pTreeCtrl);
                wxKeyEvent keyEvent(wxEVT_KEY_DOWN);
                keyEvent.m_keyCode = WXK_RBUTTON; // Example key code
                treeEvent.SetKeyEvent(keyEvent);
                treeEvent.SetPoint(wxPoint(m_firstMouseX, m_firstMouseY));
                pTreeCtrl->GetEventHandler()->AddPendingEvent(treeEvent);
            }
        }//endif m_pEventTreeCtrl

        if (pListCtrl)
        {
            long itemIndex = GetItemIndexUnderMouse(pListCtrl);
            if (itemIndex != wxNOT_FOUND)
            {
                wxListEvent listEvent(wxEVT_LIST_ITEM_RIGHT_CLICK, pListCtrl->GetId());
                listEvent.SetEventObject(pListCtrl);
                listEvent.m_itemIndex = itemIndex;

                int selectedItemCount = pListCtrl->GetSelectedItemCount();
                // unselect all selected items
                if (selectedItemCount)
                    for (int i = 0; i < pListCtrl->GetItemCount(); i++)
                        pListCtrl->SetItemState(i, 0, wxLIST_STATE_SELECTED);

                // select the item under the mouse.
                if (not pListCtrl->GetSelectedItemCount())
                    pListCtrl->SetItemState(itemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                // Send the event to the list control
                pListCtrl->GetEventHandler()->AddPendingEvent(listEvent);
            }
        }//endif pListCtrl

        if (not (pTreeCtrl or pListCtrl)) //not TreeCtrl and not ListCtrl
        {
            // Simulate a Linux right-mouse-down/up via a context popup
            wxPoint screenPos = m_pEventWindow->ClientToScreen(wxPoint(m_firstMouseX, m_firstMouseY));
            wxContextMenuEvent contextEvt(wxEVT_CONTEXT_MENU,
                                m_pEventWindow->GetId(),
                                screenPos );
            contextEvt.SetEventObject(event.GetEventObject());
            m_pEventWindow->GetEventHandler()->AddPendingEvent(contextEvt);
        }

        m_isScrollKeyValid = false;
        m_dragging = false;
        return; //own the event
    }//endif not scrolling
}
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseRightDown(wxMouseEvent& event) /// Linux
// ---------------------------------------------------------------------------
{
     LOGIT("\n%s entered %p", __FUNCTION__, event.GetEventObject());

    if ( (not event.GetEventObject()->IsKindOf(CLASSINFO(wxWindow)))
        or (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject())) )
        {event.Skip(); return;}

    //Note: We know the Right_mouse_Button is down (because we're here).

    int chosenDragKey = pDSplugin->GetchosenDragKey();
    // if chosen drag key contains middle mouse, ignore this Right mouse event
    if ( chosenDragKey >= pDSplugin->dragKeyType::Middle_Mouse)
        { event.Skip(); return;}

    m_pEventObject = event.GetEventObject(); // save event object for timer event
    wxWindow* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());
    m_pEventWindow = pWindow;

    if (timerUninialized) // one-time execution
    {
        m_WaitTimer.Bind(wxEVT_TIMER, &MouseEventsHandler::OnTimerEvent, this);
        timerUninialized = false;
    }

    m_isScrollKeyValid = false;

    bool isAltDown = wxGetKeyState(WXK_ALT);
    bool isShiftDown = wxGetKeyState(WXK_SHIFT);

    // if chosenDragKey is ONLY Right_Mouse, there should be no modifier keys down
    if ( (chosenDragKey == pDSplugin->dragKeyType::Right_Mouse)
        and (isAltDown or isShiftDown))
        {event.Skip(); return;}

    // verify modifier keys if chosenDragKey is [alt|shift] Right_mouse
    if (chosenDragKey == pDSplugin->dragKeyType::Alt_Right_Mouse
        and ((not isAltDown) or (isShiftDown)) )
            {event.Skip(); return;}
    if (chosenDragKey == pDSplugin->dragKeyType::Shift_Right_Mouse
        and ((not isShiftDown) or (isAltDown)))
            {event.Skip(); return;}

    // mark the current scroll key invalid to indicate we've processed this key sequence
    m_isScrollKeyValid = true;

    ////m_rightIsDown = false;  //Liinux
    m_didScroll =   false;
    m_firstMouseY = event.GetY();
    m_firstMouseX = event.GetX();
    m_lastMouseY = event.GetY();
    m_lastMouseX = event.GetX();

    wxObject* pEvtObject = event.GetEventObject();

    // if StyledTextCtrl, remember for later scrolling
    m_pStyledTextCtrl = 0;
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

    // Wait to see if the mouse moves when using only Linux Right_Mouse button
    // If the mouse moves, we'll override the context popup process
    if ( chosenDragKey == pDSplugin->dragKeyType::Right_Mouse)
        m_WaitTimer.Start(250, wxTIMER_ONE_SHOT); //one shot wait for 1/4th second

    return;

}//end OnMouseRightDown
// ----------------------------------------------------------------------------
void MouseEventsHandler::OnMouseRightUp(wxMouseEvent& event) /// Linux
// ----------------------------------------------------------------------------
{
    LOGIT("%s entered", __FUNCTION__);

    // cleanup any status for a previous mouse RightDown

    wxObject* pEvtObject = event.GetEventObject();
    if ( (not pEvtObject->IsKindOf(CLASSINFO(wxWindow)))
        or (not pDSplugin->IsAttachedTo((wxWindow*)event.GetEventObject())) )
        { event.Skip(); return; }

    //-unused- wxWindow* pWindow = dynamic_cast<wxWindow*>(event.GetEventObject());
    //-unused- wxTreeCtrl* m_pEventTreeCtrl = dynamic_cast<wxTreeCtrl*>(event.GetEventObject());

    m_isScrollKeyValid = false;
    m_dragging = false;

    return;
}//end OnMouseRightUp
#endif /// NOT __WXMSW__ Linux
