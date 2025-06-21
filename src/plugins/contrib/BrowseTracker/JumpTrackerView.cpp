/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

//#include "sdk_precomp.h" gets not used because `EXPORT_LIB' not defined [-Winvalid-pch] error
#include "sdk.h" // needed even though clangd: says it's not

#ifndef CB_PRECOMP
    #include <wx/arrstr.h>
    #include <wx/filename.h>
    #include <wx/listctrl.h>
    #include "manager.h"
    #include "editormanager.h"
    #include "cbeditor.h"
#endif
#include <wx/menu.h>
#include <wx/checklst.h>
#include "wx/xrc/xmlres.h"

#include <multiselectdlg.h>
#include <annoyingdialog.h>
#include <configmanager.h>

#include "cbstyledtextctrl.h"
#include "JumpTrackerView.h"

namespace
{
}

BEGIN_EVENT_TABLE(JumpTrackerView, wxEvtHandler)
//
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
JumpTrackerView::JumpTrackerView(const wxArrayString& titles_in, wxArrayInt& widths_in)
// ----------------------------------------------------------------------------
    : ListCtrlLogger(titles_in, widths_in)

{
    m_ID_List = wxNewId();
    m_pListCtrl =  dynamic_cast<wxListCtrl*>(CreateControl(Manager::Get()->GetAppFrame()));

}
// ----------------------------------------------------------------------------
wxEvtHandler* JumpTrackerView::FindEventHandler(wxEvtHandler* pEvtHdlr)
// ----------------------------------------------------------------------------
{
    wxEvtHandler* pFoundEvtHdlr =  Manager::Get()->GetAppWindow()->GetEventHandler();

    while (pFoundEvtHdlr != nullptr)
    {
        if (pFoundEvtHdlr == pEvtHdlr)
            return pFoundEvtHdlr;
        pFoundEvtHdlr = pFoundEvtHdlr->GetNextHandler();
    }
    return nullptr;
}
// ----------------------------------------------------------------------------
JumpTrackerView::~JumpTrackerView()
// ----------------------------------------------------------------------------
{
    //dtor
    if (FindEventHandler(this))
        Manager::Get()->GetAppWindow()->RemoveEventHandler(this);
}
// ----------------------------------------------------------------------------
wxWindow* JumpTrackerView::CreateControl(wxWindow* parent)
// ----------------------------------------------------------------------------
{
    ListCtrlLogger::CreateControl(parent);
    control->SetId(m_ID_List);
    Connect(m_ID_List, -1, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &JumpTrackerView::OnDoubleClick);
    Manager::Get()->GetAppWindow()->PushEventHandler(this);
    m_pControl = control;

    return m_pControl;
}
// ----------------------------------------------------------------------------
bool JumpTrackerView::HasFeature(Feature::Enum feature) const
// ----------------------------------------------------------------------------
{
    if (feature == Feature::Additional)
        return true;
    else
        return ListCtrlLogger::HasFeature(feature);
}

// ----------------------------------------------------------------------------
void JumpTrackerView::AppendAdditionalMenuItems(cb_unused wxMenu &menu)
// ----------------------------------------------------------------------------
{
}

// ----------------------------------------------------------------------------
void JumpTrackerView::FocusEntry(size_t index)
// ----------------------------------------------------------------------------
{
    if (not control->GetItemCount()) return;
    if (index < (size_t)control->GetItemCount())
    {
        control->SetItemState(index, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED);
        control->EnsureVisible(index);
    }
}

// ----------------------------------------------------------------------------
void JumpTrackerView::SyncEditor(int selIndex)
// ----------------------------------------------------------------------------
{
    wxFileName filename(control->GetItemText(selIndex));
    wxString file;
    if (not filename.Exists()) return;
    if (!filename.IsAbsolute())
        filename.MakeAbsolute(m_Base);
    file = filename.GetFullPath();

    wxListItem li;
    li.m_itemId = selIndex;
    li.m_col = 1;
    li.m_mask = wxLIST_MASK_TEXT;
    control->GetItem(li);
    long line = 0;
    li.m_text.ToLong(&line);
    cbEditor* ed = Manager::Get()->GetEditorManager()->Open(file);
    if (!line || !ed)
        return;

    line -= 1;
    ed->Activate();
    ed->GotoLine(line);

    if (cbStyledTextCtrl* ctrl = ed->GetControl())
        ctrl->EnsureVisible(line);
}

// ----------------------------------------------------------------------------
void JumpTrackerView::OnDoubleClick(cb_unused wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // go to the relevant file/line
    if (control->GetSelectedItemCount() == 0)
        return;

    // find selected item index
    int index = control->GetNextItem(-1,
                                     wxLIST_NEXT_ALL,
                                     wxLIST_STATE_SELECTED);
    //-m_lastJTViewIndex = index;

    m_bJumpInProgress = true;
    SyncEditor(index);
    FocusEntry(index);
    m_bJumpInProgress = false;
    return;

} // end of OnDoubleClick
