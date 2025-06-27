/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef ABBREVIATIONSCONFIGPANEL_H
#define ABBREVIATIONSCONFIGPANEL_H

#include "abbreviations.h"
#include "cbstyledtextctrl.h"

#include "configurationpanel.h"

#include <wx/stattext.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/button.h>

class AbbreviationsConfigPanel: public cbConfigurationPanel
{
public:
    AbbreviationsConfigPanel(wxWindow* parent, Abbreviations* plugin);
    ~AbbreviationsConfigPanel() override;

    /// @return the panel's title.
    wxString GetTitle() const override;
    /// @return the panel's bitmap base name. You must supply two bitmaps: \<basename\>.png and \<basename\>-off.png...
    wxString GetBitmapBaseName() const override;
    /// Called when the user chooses to apply the configuration.
    void OnApply() override;
    /// Called when the user chooses to cancel the configuration.
    void OnCancel() override;

private:
    void InitCompText();
    void AutoCompUpdate(const wxString& key, const wxString& lang);
    void ApplyColours();

    void OnAutoCompKeyword(wxCommandEvent& event);
    void OnAutoCompAdd(wxCommandEvent& event);
    void OnAutoCompDelete(wxCommandEvent& event);
    void OnLanguageSelect(wxCommandEvent& event);
    void OnLanguageAdd(wxCommandEvent& event);
    void OnLanguageCopy(wxCommandEvent& event);
    void OnLanguageDelete(wxCommandEvent& event);

    void FillLangugages();
    void FillKeywords();
    void LanguageSelected();
    int  LanguageAdd();

private:
    cbStyledTextCtrl* m_AutoCompTextControl;
    wxListBox*        m_Keyword;
    wxString          m_LastAutoCompKeyword;
    wxString          m_LastAutoCompLanguage;
    AutoCompleteMap*  m_pCurrentAutoCompMap;
    Abbreviations*    m_Plugin;

    wxComboBox*       m_LanguageCmb;

    DECLARE_EVENT_TABLE()
};

#endif
