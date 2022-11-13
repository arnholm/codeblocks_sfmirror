/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CCOPTIONSPRJDLG_H
#define CCOPTIONSPRJDLG_H

#include <wx/intl.h>
#include "configurationpanel.h"
#include <settings.h>
#include "parsemanager.h"
#include "parser/parser.h"

class cbProject;

class CCOptionsProjectDlg : public cbConfigurationPanel
{
public:
    CCOptionsProjectDlg(wxWindow* parent, cbProject* project, ParseManager* np);
    ~CCOptionsProjectDlg() override;

    wxString GetTitle() const override { return _("C/C++ parser options"); }
    wxString GetBitmapBaseName() const override { return _T("generic-plugin"); }
    void OnApply() override;
    void OnCancel() override {}

protected:
    void OnAdd(wxCommandEvent& event);
    void OnEdit(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnUpdateUI(wxUpdateUIEvent& event);

private:
    cbProject*    m_Project;
    ParseManager* m_ParseManager;
    ParserBase*   m_Parser;
    wxArrayString m_OldPaths;

    DECLARE_EVENT_TABLE()
};

#endif // CCOPTIONSPRJDLG_H
