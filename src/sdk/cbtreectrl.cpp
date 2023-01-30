/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include <wx/event.h>
    #include <wx/filename.h>
    #include <wx/gdicmn.h> // wxPoint
    #include <wx/string.h>
    #include <wx/treebase.h> // wxTreeItemId

    #include "cbproject.h" // FileTreeData
    #include "projectfile.h"
#endif

IMPLEMENT_DYNAMIC_CLASS(cbTreeCtrl, wxTreeCtrl)

BEGIN_EVENT_TABLE(cbTreeCtrl, wxTreeCtrl)
#ifndef __WXMSW__
    EVT_RIGHT_DOWN(cbTreeCtrl::OnRightClick)
#endif // !__WXMSW__
    EVT_KEY_DOWN(cbTreeCtrl::OnKeyDown)

    EVT_MOUSE_EVENTS(cbTreeCtrl::OnMouseMove)
    EVT_LEFT_UP(cbTreeCtrl::OnMouseMove)
    EVT_TREE_BEGIN_DRAG(wxID_ANY, cbTreeCtrl::OnBeginDrag)
    EVT_TREE_END_DRAG(wxID_ANY, cbTreeCtrl::OnEndDrag)
END_EVENT_TABLE()

cbTreeCtrl::cbTreeCtrl() : wxTreeCtrl()
{
    Compare = &filesSort;
    m_dragging = false;
}

cbTreeCtrl::cbTreeCtrl(wxWindow* parent, int id) :
    wxTreeCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
               wxTR_EDIT_LABELS | wxTR_HAS_BUTTONS | wxTR_MULTIPLE | wxBORDER_NONE)
{
    Compare = &filesSort;
}

void cbTreeCtrl::CalculateScrollingAfterMove(int x, int y)
{
    // implement custom scrolling during drag and drop
    if ( m_dragging )
    {
        int iWidth = 0;
        int iHeight = 0;
        // Get the width and height of the treectrl
        GetSize( &iWidth, &iHeight );
        // Find where the mouse is in relation to the (exited) pane

        if ( x < 0 || x > iWidth )
            return;     // If mouse outside x coordinates of window we do not scroll
        // scrolling if the mouse is move at the top/bottom 1/5 of the tree ctrl
        if ( y <= (iHeight / 5) )
        {
            Scroll( true ); // Scroll Up
        }
        else if ( y >= iHeight - (iHeight / 5) )
        {
            Scroll( false ); // Scroll Down
        }
    }
}

void cbTreeCtrl::OnMouseMove( wxMouseEvent & event )
{
    // To implement scrolling during dragging we need to track the
    // mouse movement, to terminate dragging when the user
    // drops outside the tree ctrl. When left mouse up then
    // terminate dragging
    // when mouse down -> dragging -> update scroll position
    event.Skip();
    if (event.LeftUp() || !event.LeftIsDown())
    {
        OnEndDrag();
        return;
    }

    wxPoint mousePt = ScreenToClient( wxGetMousePosition( ) );
    CalculateScrollingAfterMove(mousePt.x, mousePt.y);
}

void cbTreeCtrl::Scroll( bool bScrollUp )
{

    // Scrolling by dragging is currently not natively supported
    // by wxWidgets. We have to implement it by hand
    // i encoutered a strange behaviour on linux that LineUp and LineDow
    // only move the scroll bar but do not redraw the tree at the
    // current scroll position.

#ifdef __WXMSW__
    bool refresh = false;
    if ( bScrollUp )
        refresh = LineUp( );
    else
        refresh = LineDown( );

    if (refresh)
        Refresh( true );
#else
    // On linux we scroll by getting the current window position
    // and add some pixel to it. The User has to move the mouse
    // to actively scroll.
    int x = 0, y = 0;
    GetViewStart(&y, &y);
    if ( bScrollUp )
        wxTreeCtrl::Scroll(x, y - 5);
    else
        wxTreeCtrl::Scroll(x, y + 5);
#endif // __WXMSW__
}

void cbTreeCtrl::OnBeginDrag(wxTreeEvent& event)
{
    m_dragging = true;
    event.Skip();   // process the event future up
}

void cbTreeCtrl::OnEndDrag()
{
    m_dragging = false;
}

void cbTreeCtrl::OnEndDrag(wxTreeEvent &event)
{
    OnEndDrag();
    event.Skip(); // process the event future up
}

void cbTreeCtrl::SetCompareFunction(const int ptvs)
{
    // sort list of files
    if ( !(ptvs & ptvsUseFolders) && (ptvs & ptvsHideFolderName) )
        Compare = &filesSortNameOnly;
    else
        Compare = &filesSort;
}

wxTreeItemId cbTreeCtrl::GetPrevVisible(const wxTreeItemId& item) const
{
    wxTreeItemId previous = GetPrevSibling(item);
    if (previous.IsOk())
    {
        while (ItemHasChildren(previous) && IsExpanded(previous))
        {
            wxTreeItemId child = GetLastChild(previous);
            if (!child.IsOk())
                break;
            else
                previous = child;
        }
    }
    else
        previous = GetItemParent(item);
    return previous;
}

#ifndef __WXMSW__
/*
    Under wxGTK, wxTreeCtrl is not sending an EVT_COMMAND_RIGHT_CLICK
    event when right-clicking on the client area.
*/
void cbTreeCtrl::OnRightClick(wxMouseEvent& event)
{
    int flags;
    HitTest(wxPoint(event.GetX(), event.GetY()), flags);
    if (flags & (wxTREE_HITTEST_ABOVE | wxTREE_HITTEST_BELOW | wxTREE_HITTEST_NOWHERE))
    {
        // "proxy" the call
        wxCommandEvent e(wxEVT_COMMAND_RIGHT_CLICK, GetID());
        wxPostEvent(GetParent(), e);
    }
    else
        event.Skip();
}
#endif // !__WXMSW__
/*
    Under all platforms there is no reaction when pressing "ENTER".
    Expected would be e.g. to open the file in an editor.
*/
void cbTreeCtrl::OnKeyDown(wxKeyEvent& event)
{
    // Don't care about special key combinations
    if (event.GetModifiers()!=wxMOD_NONE)
    {
        event.Skip();
        return;
    }

    wxArrayTreeItemIds selections;
    // Don't care if no selection has been made
    if ( GetSelections(selections)<1 )
    {
        event.Skip();
        return;
    }

    long         keycode = event.GetKeyCode();
    wxTreeItemId itemId  = selections[0];
    // Don't care if item is invalid
    if ( !itemId.IsOk() )
    {
        event.Skip();
        return;
    }

    switch (keycode)
    {
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
        {
            wxTreeEvent te = wxTreeEvent(wxEVT_COMMAND_TREE_ITEM_ACTIVATED, this, itemId);
            wxPostEvent(this, te);
            break;
        }
        default:
            event.Skip();
    }
}

/*static*/ int cbTreeCtrl::filesSort(const ProjectFile* arg1, const ProjectFile* arg2)
{
    if (arg1 && arg2)
        return wxStricmp(arg1->file.GetFullPath(), arg2->file.GetFullPath());
    return 0;
}

/*static*/ int cbTreeCtrl::filesSortNameOnly(const ProjectFile* arg1, const ProjectFile* arg2)
{
    if (arg1 && arg2)
        return wxStricmp(arg1->file.GetFullName(), arg2->file.GetFullName());
    return 0;
}

int cbTreeCtrl::OnCompareItems(const wxTreeItemId& item1, const wxTreeItemId& item2)
{
    return Compare(((FileTreeData*)GetItemData(item1))->GetProjectFile(), ((FileTreeData*)GetItemData(item2))->GetProjectFile());
}
