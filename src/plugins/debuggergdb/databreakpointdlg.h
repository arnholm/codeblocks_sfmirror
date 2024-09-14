/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef DATABREAKPOINTDLG_H
#define DATABREAKPOINTDLG_H

#include <wx/wxprec.h>

//(*Headers(DataBreakpointDlg)
#include "scrollingdialog.h"
#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
//*)

class DataBreakpointDlg: public wxScrollingDialog
{
    public:

        DataBreakpointDlg(wxWindow *parent, const wxString& dataExpression, bool enabled, int selection);
        virtual ~DataBreakpointDlg();

        //(*Identifiers(DataBreakpointDlg)
        static const wxWindowID ID_CHK_ENABLED;
        static const wxWindowID ID_TXT_DATA_EXPRESION;
        static const wxWindowID ID_RDO_CONDITION;
        //*)

        bool IsBreakpointEnabled();
        int GetSelection();
        wxString GetDataExpression() const;

    protected:

        //(*Handlers(DataBreakpointDlg)
        //*)

        //(*Declarations(DataBreakpointDlg)
        wxCheckBox* m_enabled;
        wxRadioBox* m_condition;
        wxStdDialogButtonSizer* StdDialogButtonSizer1;
        wxTextCtrl* m_dataExpression;
        //*)

    private:

        DECLARE_EVENT_TABLE()
};

#endif
