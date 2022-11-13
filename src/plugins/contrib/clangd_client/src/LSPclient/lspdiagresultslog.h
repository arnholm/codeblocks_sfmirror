/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef LSP_DIAGRESULTSLOG_H
#define LSP_DIAGRESULTSLOG_H

#include <wx/string.h>
#include "loggers.h"

class wxArrayString;
class wxCommandEvent;

class LSPDiagnosticsResultsLog : public ListCtrlLogger, public wxEvtHandler
{
	public:
		LSPDiagnosticsResultsLog(const wxArrayString& titles, wxArrayInt& widths, wxArrayString& aIgnoredMsgs);
		~LSPDiagnosticsResultsLog() override;
		void FocusEntry(size_t index);
		void SetBasePath(const wxString base){ m_Base = base; }
        wxWindow* m_pControl = nullptr;

        wxArrayString& rUsrIgnoredDiagnostics;

		wxWindow* CreateControl(wxWindow* parent) override;
        bool      HasFeature(Feature::Enum feature) const override;
        void      AppendAdditionalMenuItems(wxMenu &menu) override;

	protected:
        void OnDoubleClick(wxCommandEvent& event);
        void SyncEditor(int selIndex);
        void OnSetIgnoredMsgs(wxCommandEvent& event);
        wxEvtHandler* FindEventHandler(wxEvtHandler* pEvtHdlr);

        wxString m_Base;
	private:
        DECLARE_EVENT_TABLE()
};

#endif // CB_SEARCHRESULTSLOG_H
