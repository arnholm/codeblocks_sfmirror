/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 0 $
 * $Id: editbreakpointdlg.cpp 7920 2024-03-07 09:15:28Z codeblocks $
 * $HeadURL: https://svn.berlios.de/svnroot/repos/codeblocks/trunk/src/plugins/debugger_gdbmi/editbreakpointdlg.cpp $
 */

#include <sdk.h>
#ifndef CB_PRECOMP
    #include <wx/checkbox.h>
    #include <wx/intl.h>
    #include <wx/button.h>
    #include <wx/listbox.h>
    #include <wx/spinctrl.h>
    #include <wx/textctrl.h>
    #include <wx/xrc/xmlres.h>

    #include <debuggermanager.h>
#endif

#include "editbreakpointdlg.h"
namespace dbg_mi
{


BEGIN_EVENT_TABLE(dbg_mi::EditBreakpointDlg, wxScrollingDialog)
    EVT_UPDATE_UI(-1, dbg_mi::EditBreakpointDlg::OnUpdateUI)
END_EVENT_TABLE()

EditBreakpointDlg::EditBreakpointDlg(Breakpoint& breakpoint, wxWindow* parent)
    : m_breakpoint(breakpoint)
{
    //ctor
    wxXmlResource::Get()->LoadObject(this, parent, _T("dlgEditBreakpoint"),_T("wxScrollingDialog"));

    XRCCTRL(*this, "chkEnabled", wxCheckBox)->SetValue(m_breakpoint.IsEnabled());
    XRCCTRL(*this, "chkIgnore", wxCheckBox)->SetValue(m_breakpoint.HasIgnoreCount());
    XRCCTRL(*this, "spnIgnoreCount", wxSpinCtrl)->SetValue(m_breakpoint.GetIgnoreCount());
    XRCCTRL(*this, "chkExpr", wxCheckBox)->SetValue(m_breakpoint.HasCondition());
    XRCCTRL(*this, "txtExpr", wxTextCtrl)->SetValue(m_breakpoint.GetCondition());
}

EditBreakpointDlg::~EditBreakpointDlg()
{
    //dtor
}

void EditBreakpointDlg::EndModal(int retCode)
{
    if (retCode == wxID_OK)
    {
        m_breakpoint.SetEnabled( XRCCTRL(*this, "chkEnabled", wxCheckBox)->GetValue());
        m_breakpoint.SetUseIgnoreCount( XRCCTRL(*this, "chkIgnore", wxCheckBox)->IsChecked());
        m_breakpoint.SetIgnoreCount( XRCCTRL(*this, "spnIgnoreCount", wxSpinCtrl)->GetValue());
        m_breakpoint.SetUseCondition( XRCCTRL(*this, "chkExpr", wxCheckBox)->IsChecked());
        m_breakpoint.SetCondition( XRCCTRL(*this, "txtExpr", wxTextCtrl)->GetValue());
    }
    wxScrollingDialog::EndModal(retCode);
}

void EditBreakpointDlg::OnUpdateUI(wxUpdateUIEvent& WXUNUSED(event))
{
    bool en = XRCCTRL(*this, "chkEnabled", wxCheckBox)->IsChecked();
    XRCCTRL(*this, "chkIgnore", wxCheckBox)->Enable(en && !XRCCTRL(*this, "chkExpr", wxCheckBox)->IsChecked());
    XRCCTRL(*this, "spnIgnoreCount", wxSpinCtrl)->Enable(en && XRCCTRL(*this, "chkIgnore", wxCheckBox)->IsChecked());
    XRCCTRL(*this, "chkExpr", wxCheckBox)->Enable(en && !XRCCTRL(*this, "chkIgnore", wxCheckBox)->IsChecked());
    XRCCTRL(*this, "txtExpr", wxTextCtrl)->Enable(en && XRCCTRL(*this, "chkExpr", wxCheckBox)->IsChecked());
}
} //namespace dbg_mi

