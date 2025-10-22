#ifndef JUMPTRACKERVIEW_H_INCLUDED
#define JUMPTRACKERVIEW_H_INCLUDED

/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#include <wx/string.h>
#include <wx/panel.h>
#include <wx/listctrl.h>

#include "loggers.h"

class wxArrayString;
class wxCommandEvent;

class JumpTrackerView : public ListCtrlLogger, public wxEvtHandler
{
	public:
		JumpTrackerView(const wxArrayString& titles, wxArrayInt& widths);
		~JumpTrackerView() override;
		void FocusEntry(size_t index);
		void SetBasePath(const wxString base){ m_Base = base; }

		wxWindow* CreateControl(wxWindow* parent) override;
        bool      HasFeature(Feature::Enum feature) const override;
        void      AppendAdditionalMenuItems(wxMenu &menu) override;

        int m_ID_List = wxNewId();
        wxListCtrl* m_pListCtrl = nullptr;
        wxListCtrl* m_pControl = nullptr;
        //-int         m_lastJTViewIndex = 0;
        bool        m_bJumpInProgress = false;

        int GetJumpTrackerViewIndex()
        {
            long selected = m_pListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if (selected == -1)
            {
                if (m_pListCtrl->GetItemCount() > 0)
                    selected = 0; // No selection, fallback to first item
                else
                    selected = -1; // List is empty, nothing to select
            }
            // Now 'selected' holds the desired item index, or -1 if the list is empty
            if (selected == -1)
                return 0;

            return selected;
        }

        void SetJumpTrackerViewIndex(int itemIndex)
        {
            int knt = m_pListCtrl->GetItemCount();
            if ( (itemIndex < 0) or (knt < 0)) return;  // (Letartare 25/10/22)
            if ( (not knt)
                or (itemIndex >= knt) )                 // (christo 25/10/22)
                return; //avoid assert bec. setting index before adding the item
                        // https://forums.codeblocks.org/index.php?topic=26149.msg177897#msg177897
            m_pListCtrl->SetItemState(itemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        }

	protected:
        void OnDoubleClick(wxCommandEvent& event);
        void SyncEditor(int selIndex);
        wxEvtHandler* FindEventHandler(wxEvtHandler* pEvtHdlr);

        wxString m_Base;
	private:
        DECLARE_EVENT_TABLE()
};

#endif // JUMPTRACKERVIEW_H_INCLUDED
