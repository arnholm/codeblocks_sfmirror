/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>
#include "occurrenceshighlightingconfigurationpanel.h"

#ifndef CB_PRECOMP
    #include <algorithm>
    #include <wx/xrc/xmlres.h>
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/stattext.h>
    #include <wx/spinctrl.h>

    #include <configmanager.h>
    #include <editormanager.h>
    #include <logmanager.h>
#endif

#include <wx/clrpicker.h>

#include "cbcolourmanager.h"

BEGIN_EVENT_TABLE(OccurrencesHighlightingConfigurationPanel, cbConfigurationPanel)
    EVT_CHECKBOX(XRCID("chkHighlightOccurrences"),     OccurrencesHighlightingConfigurationPanel::OnCheck)
    EVT_CHECKBOX(XRCID("chkHighlightOccurrencesOverrideText"), OccurrencesHighlightingConfigurationPanel::OnCheck)
    EVT_CHECKBOX(XRCID("chkHighlightPermanentlyOccurrencesOverrideText"), OccurrencesHighlightingConfigurationPanel::OnCheck)
END_EVENT_TABLE()

OccurrencesHighlightingConfigurationPanel::OccurrencesHighlightingConfigurationPanel(wxWindow* parent)
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));
    if (!cfg)
        return;

    if (!wxXmlResource::Get()->LoadObject(this, parent, _T("OccurrencesHighlightingConfigurationPanel"), _T("wxPanel")))
    {
        Manager::Get()->GetLogManager()->DebugLog(_T("Could not load occurrences highlighting config panel!"));
        return;
    }

    ColourManager *colourManager = Manager::Get()->GetColourManager();

    // Highlight Occurrence
    bool highlightEnabled = cfg->ReadBool(_T("/highlight_occurrence/enabled"), true);
    XRCCTRL(*this, "chkHighlightOccurrences",              wxCheckBox)->SetValue(highlightEnabled);
    XRCCTRL(*this, "chkHighlightOccurrencesCaseSensitive", wxCheckBox)->SetValue(cfg->ReadBool(_T("/highlight_occurrence/case_sensitive"), true));
    XRCCTRL(*this, "chkHighlightOccurrencesWholeWord",     wxCheckBox)->SetValue(cfg->ReadBool(_T("/highlight_occurrence/whole_word"), true));
    XRCCTRL(*this, "chkHighlightOccurrencesOverrideText",  wxCheckBox)->SetValue(cfg->ReadBool(_T("/highlight_occurrence/override_text"), false));

    wxColour highlightColour = colourManager->GetColour(wxT("editor_highlight_occurrence"));
    XRCCTRL(*this, "cpHighlightColour", wxColourPickerCtrl)->SetColour(highlightColour);

    XRCCTRL(*this, "spnHighlightAlpha", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/highlight_occurrence/alpha"), 100));
    XRCCTRL(*this, "spnHighlightBorderAlpha", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/highlight_occurrence/border_alpha"), 255));

    highlightColour = colourManager->GetColour(wxT("editor_highlight_occurrence_text"));
    XRCCTRL(*this, "cpHighlightTextColour", wxColourPickerCtrl)->SetColour(highlightColour);

    wxSpinCtrl *minLength = XRCCTRL(*this, "spnHighlightLength", wxSpinCtrl);
    minLength->SetValue(cfg->ReadInt(_T("/highlight_occurrence/min_length"), 3));
    minLength->Enable(highlightEnabled);

    XRCCTRL(*this, "chkHighlightOccurrencesPermanentlyCaseSensitive", wxCheckBox)->SetValue(cfg->ReadBool(_T("/highlight_occurrence/case_sensitive_permanently"), true));
    //XRCCTRL(*this, "chkHighlightOccurrencesPermanentlyCaseSensitive", wxCheckBox)->Enable(true);
    XRCCTRL(*this, "chkHighlightOccurrencesPermanentlyWholeWord",     wxCheckBox)->SetValue(cfg->ReadBool(_T("/highlight_occurrence/whole_word_permanently"), true));
    XRCCTRL(*this, "chkHighlightPermanentlyOccurrencesOverrideText",  wxCheckBox)->SetValue(cfg->ReadBool(_T("/highlight_occurrence/override_text_permanently"), false));

    //XRCCTRL(*this, "chkHighlightOccurrencesPermanentlyWholeWord",     wxCheckBox)->Enable(true);
    highlightColour = colourManager->GetColour(wxT("editor_highlight_occurrence_permanently"));
    XRCCTRL(*this, "cpHighlightPermanentlyColour", wxColourPickerCtrl)->SetColour(highlightColour);
    XRCCTRL(*this, "spnHighlightPermanentlyAlpha", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/highlight_occurrence/alpha_permanently"), 100));
    XRCCTRL(*this, "spnHighlightPermanentlyBorderAlpha", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/highlight_occurrence/border_alpha_permanently"), 255));

    highlightColour = colourManager->GetColour(wxT("editor_highlight_occurrence_permanently_text"));
    XRCCTRL(*this, "cpHighlightPermanentlyTextColour", wxColourPickerCtrl)->SetColour(highlightColour);
    //XRCCTRL(*this, "stHighlightPermanentlyColour",                    wxStaticText)->Enable(permanentlyHighlightEnabled);
    //XRCCTRL(*this, "cpHighlightPermanentlyColour",                    wxColourPickerCtrl)->Enable(permanentlyHighlightEnabled);

    UpdateUI();
}

OccurrencesHighlightingConfigurationPanel::~OccurrencesHighlightingConfigurationPanel()
{
}

void OccurrencesHighlightingConfigurationPanel::OnApply()
{
    // save any changes
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));
    ColourManager *colourManager = Manager::Get()->GetColourManager();

    cfg->Write(_T("/highlight_occurrence/enabled"),        XRCCTRL(*this, "chkHighlightOccurrences",              wxCheckBox)->GetValue());
    cfg->Write(_T("/highlight_occurrence/case_sensitive"), XRCCTRL(*this, "chkHighlightOccurrencesCaseSensitive", wxCheckBox)->GetValue());
    cfg->Write(_T("/highlight_occurrence/whole_word"),     XRCCTRL(*this, "chkHighlightOccurrencesWholeWord",     wxCheckBox)->GetValue());
    cfg->Write(_T("/highlight_occurrence/override_text"),     XRCCTRL(*this, "chkHighlightOccurrencesOverrideText",  wxCheckBox)->GetValue());
    wxColour highlightColour = XRCCTRL(*this, "cpHighlightColour", wxColourPickerCtrl)->GetColour();
    colourManager->SetColour(wxT("editor_highlight_occurrence"), highlightColour);
    cfg->Write(_T("/highlight_occurrence/min_length"),     XRCCTRL(*this, "spnHighlightLength",                   wxSpinCtrl)->GetValue());

    cfg->Write(_T("/highlight_occurrence/alpha"), XRCCTRL(*this, "spnHighlightAlpha", wxSpinCtrl)->GetValue());
    cfg->Write(_T("/highlight_occurrence/border_alpha"), XRCCTRL(*this, "spnHighlightBorderAlpha", wxSpinCtrl)->GetValue());

    highlightColour = XRCCTRL(*this, "cpHighlightTextColour", wxColourPickerCtrl)->GetColour();
    colourManager->SetColour(wxT("editor_highlight_occurrence_text"), highlightColour);

    cfg->Write(_T("/highlight_occurrence/case_sensitive_permanently"), XRCCTRL(*this, "chkHighlightOccurrencesPermanentlyCaseSensitive", wxCheckBox)->GetValue());
    cfg->Write(_T("/highlight_occurrence/whole_word_permanently"),     XRCCTRL(*this, "chkHighlightOccurrencesPermanentlyWholeWord",     wxCheckBox)->GetValue());
    cfg->Write(_T("/highlight_occurrence/override_text_permanently"),  XRCCTRL(*this, "chkHighlightPermanentlyOccurrencesOverrideText",  wxCheckBox)->GetValue());
    highlightColour = XRCCTRL(*this, "cpHighlightPermanentlyColour", wxColourPickerCtrl)->GetColour();
    colourManager->SetColour(wxT("editor_highlight_occurrence_permanently"), highlightColour);

    cfg->Write(_T("/highlight_occurrence/alpha_permanently"), XRCCTRL(*this, "spnHighlightPermanentlyAlpha", wxSpinCtrl)->GetValue());
    cfg->Write(_T("/highlight_occurrence/border_alpha_permanently"), XRCCTRL(*this, "spnHighlightPermanentlyBorderAlpha", wxSpinCtrl)->GetValue());

    highlightColour = XRCCTRL(*this, "cpHighlightPermanentlyTextColour", wxColourPickerCtrl)->GetColour();
    colourManager->SetColour(wxT("editor_highlight_occurrence_permanently_text"), highlightColour);
}

void OccurrencesHighlightingConfigurationPanel::OnCancel()
{
}

wxString OccurrencesHighlightingConfigurationPanel::GetTitle() const
{
    return _("Occurrences Highlighting");
}

wxString OccurrencesHighlightingConfigurationPanel::GetBitmapBaseName() const
{
    return _T("occurrenceshighlighting");
}

void OccurrencesHighlightingConfigurationPanel::OnCheck(cb_unused wxCommandEvent& event)
{
    UpdateUI();
}

void OccurrencesHighlightingConfigurationPanel::UpdateUI()
{
    const bool enabled = XRCCTRL(*this, "chkHighlightOccurrences", wxCheckBox)->GetValue();
    wxCheckBox *overrideTextColour = XRCCTRL(*this, "chkHighlightOccurrencesOverrideText",  wxCheckBox);
    const bool overrideText = overrideTextColour->GetValue();

    XRCCTRL(*this, "chkHighlightOccurrencesCaseSensitive", wxCheckBox)->Enable(enabled);
    XRCCTRL(*this, "chkHighlightOccurrencesWholeWord",     wxCheckBox)->Enable(enabled);
    overrideTextColour->Enable(enabled);
    XRCCTRL(*this, "stHighlightColour",  wxStaticText)->Enable(enabled);
    XRCCTRL(*this, "cpHighlightColour", wxColourPickerCtrl)->Enable(enabled);
    XRCCTRL(*this, "stHighlightAlpha", wxStaticText)->Enable(enabled);
    XRCCTRL(*this, "spnHighlightAlpha", wxSpinCtrl)->Enable(enabled);
    XRCCTRL(*this, "stHighlightBorderAlpha", wxStaticText)->Enable(enabled);
    XRCCTRL(*this, "spnHighlightBorderAlpha", wxSpinCtrl)->Enable(enabled);
    XRCCTRL(*this, "stHighlightColourText", wxStaticText)->Enable(enabled && overrideText);
    XRCCTRL(*this, "cpHighlightTextColour", wxColourPickerCtrl)->Enable(enabled && overrideText);
    XRCCTRL(*this, "spnHighlightLength", wxSpinCtrl)->Enable(enabled);
    XRCCTRL(*this, "stHighlightLength", wxStaticText)->Enable(enabled);

    wxCheckBox *permOverrideTextColour = XRCCTRL(*this, "chkHighlightPermanentlyOccurrencesOverrideText",  wxCheckBox);
    const bool permOverrideText = permOverrideTextColour->GetValue();

    XRCCTRL(*this, "stHighlightPermanentlyColourText", wxStaticText)->Enable(permOverrideText);
    XRCCTRL(*this, "cpHighlightPermanentlyTextColour", wxColourPickerCtrl)->Enable(permOverrideText);
}

