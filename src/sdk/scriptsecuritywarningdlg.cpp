/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"
#include "scriptsecuritywarningdlg.h"

#ifndef CB_PRECOMP
    #include <wx/button.h>
    #include <wx/combobox.h>
    #include <wx/intl.h>
    #include <wx/settings.h>
    #include <wx/stattext.h>
    #include <wx/textctrl.h>
    #include <wx/xrc/xmlres.h>
#endif // CB_PRECOMP

ScriptSecurityWarningDlg::ScriptSecurityWarningDlg(wxWindow* parent, const wxString& operation, const wxString& command, bool hasPath)
{
    //ctor
    m_hasPath = hasPath;

    wxXmlResource::Get()->LoadObject(this, parent, "ScriptingSecurityDlg", "wxScrollingDialog");
    XRCCTRL(*this, "wxID_OK", wxButton)->SetDefault();

    wxColour c = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
    XRCCTRL(*this, "txtCommand", wxTextCtrl)->SetBackgroundColour(c);

    XRCCTRL(*this, "lblOperation", wxStaticText)->SetLabel(_("Operation: ") + operation);
    XRCCTRL(*this, "txtCommand", wxTextCtrl)->SetValue(command);

    wxChoice* cmbAnswer = XRCCTRL(*this, "cmbAnswer", wxChoice);
    if (m_hasPath)
    {
        // The operation to ask has its origin in a valid file, so we can ask the user if he wants to trust this file
        cmbAnswer->Append(_("ALLOW execution of this command"));
        cmbAnswer->Append(_("ALLOW execution of this command for all scripts from now on"));
        cmbAnswer->Append(_("DENY execution of this command"));
        cmbAnswer->Append(_("Mark this script as TRUSTED for this session"));
        cmbAnswer->Append(_("Mark this script as TRUSTED permanently"));
        cmbAnswer->SetSelection(2);
    }
    else
    {
        // The  operation is not called from a file, so we can ask only for current permission
        cmbAnswer->Append(_("ALLOW execution of this command"));
        cmbAnswer->Append(_("DENY execution of this command"));
        cmbAnswer->SetSelection(1);
    }
}

ScriptSecurityWarningDlg::~ScriptSecurityWarningDlg()
{
    //dtor
}

ScriptSecurityResponse ScriptSecurityWarningDlg::GetResponse()
{
    int selection = XRCCTRL(*this, "cmbAnswer", wxChoice)->GetSelection();
    if (m_hasPath)
        return (ScriptSecurityResponse) selection;
    else
    {
        // We have to map the response, because there are only two selections...
        switch(selection)
        {
        case 0: return ScriptSecurityResponse::ssrAllow;
        case 1: return ScriptSecurityResponse::ssrDeny;
        }
        return ScriptSecurityResponse::ssrDeny;
    }
}

void ScriptSecurityWarningDlg::EndModal(int retCode)
{
    wxScrollingDialog::EndModal(retCode);
}
