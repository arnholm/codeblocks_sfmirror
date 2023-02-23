#ifndef JUMPTRACKERVIEW_H_INCLUDED
#define JUMPTRACKERVIEW_H_INCLUDED

/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#include <wx/string.h>
#include <wx/panel.h>

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
        int         m_lastDoubleClickIndex = 0;
        bool        m_bJumpInProgress = false;

	protected:
        void OnDoubleClick(wxCommandEvent& event);
        void SyncEditor(int selIndex);
        wxEvtHandler* FindEventHandler(wxEvtHandler* pEvtHdlr);

        wxString m_Base;
	private:
        DECLARE_EVENT_TABLE()
};

#endif // JUMPTRACKERVIEW_H_INCLUDED
