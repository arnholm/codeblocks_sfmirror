/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CCOPTIONSDLG_H
#define CCOPTIONSDLG_H

#include <wx/intl.h>
#include "configurationpanel.h"
#include <settings.h>
#include "parsemanager.h"
#include "parser/parser.h"

class ClgdCompletion;
class DocumentationHelper;

class CCOptionsDlg : public cbConfigurationPanel
{
public:
    CCOptionsDlg(wxWindow* parent, ParseManager* np, ClgdCompletion* cc, DocumentationHelper* dh);
    ~CCOptionsDlg() override;

    wxString GetTitle() const override { return _("clangd_client"); }
    // Get png for the main settings/Editor/codecompletion image residing at
    // ...\share\CodeBlocks\images\settings\codecompletion.png (main CB images)
    wxString GetBitmapBaseName() const override { return _T("codecompletion"); }
    void OnApply() override;
    void OnCancel() override {}
    void OnPageChanging() override;

    void OnFindDirClangd_Dlg(wxCommandEvent& event);
    void OnClangd_AutoDetect(cb_unused wxCommandEvent& event);

protected:
    void OnChooseColour(wxCommandEvent& event);
    void OnCCDelayScroll(wxScrollEvent& event);

    void OnUpdateUI(wxUpdateUIEvent& event);

private:
    void UpdateCCDelayLabel();
    bool ValidateReplacementToken(wxString& from, wxString& to);

    ParseManager*        m_ParseManager;
    ClgdCompletion*      m_CodeCompletion;
    ParserBase&          m_Parser;
    DocumentationHelper* m_Documentation;
    wxString             m_Old_LLVM_MasterPath = wxString();

    DECLARE_EVENT_TABLE()
};

#endif // CCOPTIONSDLG_H
