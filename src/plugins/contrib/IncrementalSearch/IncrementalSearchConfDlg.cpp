/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * Copyright: 2008 Jens Lody
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#ifndef CB_PRECOMP
    #include <wx/xrc/xmlres.h>
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/choice.h>
    #include <configmanager.h>
#endif

#include <wx/spinctrl.h>
#include <wx/clrpicker.h>

#include "IncrementalSearchConfDlg.h"
#include "IncrementalSearch.h"

BEGIN_EVENT_TABLE(IncrementalSearchConfDlg,wxPanel)
END_EVENT_TABLE()

IncrementalSearchConfDlg::IncrementalSearchConfDlg(wxWindow* parent)
{
    wxXmlResource::Get()->LoadObject(this,parent,_T("IncrementalSearchConfDlg"),_T("wxPanel"));

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));

    // initialise colour-values
    XRCCTRL(*this, "cpIncSearchConfColourFound", wxColourPickerCtrl)->SetColour(cfg->ReadColour(_T("/incremental_search/text_found_colour"), wxColour(160, 32, 240)));
    XRCCTRL(*this, "cpIncSearchConfColourHighlight", wxColourPickerCtrl)->SetColour(cfg->ReadColour(_T("/incremental_search/highlight_colour"), wxColour(255, 165, 0)));
    XRCCTRL(*this, "cpIncSearchConfNotFoundBG", wxColourPickerCtrl)->SetColour(cfg->ReadColour(_T("/incremental_search/text_not_found_colour"), wxColour(255, 127, 127)));
    XRCCTRL(*this, "cpIncSearchConfWrappedBG", wxColourPickerCtrl)->SetColour(cfg->ReadColour(_T("/incremental_search/wrapped_colour"), wxColour(127, 127, 255)));

    // get value from conf-file or predefine them with default value
    XRCCTRL(*this, "chkIncSearchConfCenterText", wxCheckBox)->SetValue(cfg->ReadBool(_T("/incremental_search/center_found_text_on_screen"),true));
    XRCCTRL(*this, "idIncSearchSelectOnEscape", wxCheckBox)->SetValue(cfg->ReadBool(_T("/incremental_search/select_found_text_on_escape"),false));
    XRCCTRL(*this, "idIncSearchSelectOnFocus", wxCheckBox)->SetValue(cfg->ReadBool(_T("/incremental_search/select_text_on_focus"),false));
    XRCCTRL(*this, "idIncSearchHighlightDefault", wxChoice)->SetSelection(cfg->ReadInt(_T("/incremental_search/highlight_default_state"),0));
    XRCCTRL(*this, "idIncSearchSelectedDefault", wxChoice)->SetSelection(cfg->ReadInt(_T("/incremental_search/selected_default_state"),0));
    XRCCTRL(*this, "idIncSearchMatchCaseDefault", wxChoice)->SetSelection(cfg->ReadInt(_T("/incremental_search/match_case_default_state"),0));
    XRCCTRL(*this, "idIncSearchRegExDefault", wxChoice)->SetSelection(cfg->ReadInt(_T("/incremental_search/regex_default_state"),0));
    XRCCTRL(*this, "idIncSearchComboMaxItems", wxSpinCtrl)->SetValue(cfg->ReadInt(_T("/incremental_search/max_items_in_history"),20));
}

IncrementalSearchConfDlg::~IncrementalSearchConfDlg()
{
}

void IncrementalSearchConfDlg::SaveSettings()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));
    // save checkbox/choice-settings
    cfg->Write(_T("/incremental_search/center_found_text_on_screen"),   XRCCTRL(*this, "chkIncSearchConfCenterText", wxCheckBox)->IsChecked());
    cfg->Write(_T("/incremental_search/select_found_text_on_escape"),   XRCCTRL(*this, "idIncSearchSelectOnEscape", wxCheckBox)->IsChecked());
    cfg->Write(_T("/incremental_search/select_text_on_focus"),          XRCCTRL(*this, "idIncSearchSelectOnFocus", wxCheckBox)->IsChecked());
    cfg->Write(_T("/incremental_search/highlight_default_state"),       XRCCTRL(*this, "idIncSearchHighlightDefault", wxChoice)->GetSelection());
    cfg->Write(_T("/incremental_search/selected_default_state"),        XRCCTRL(*this, "idIncSearchSelectedDefault", wxChoice)->GetSelection());
    cfg->Write(_T("/incremental_search/match_case_default_state"),      XRCCTRL(*this, "idIncSearchMatchCaseDefault", wxChoice)->GetSelection());
    cfg->Write(_T("/incremental_search/regex_default_state"),           XRCCTRL(*this, "idIncSearchRegExDefault", wxChoice)->GetSelection());
    int maxItemsInHistory = XRCCTRL(*this, "idIncSearchComboMaxItems", wxSpinCtrl)->GetValue();
    cfg->Write(_T("/incremental_search/max_items_in_history"),          maxItemsInHistory);
    IncrementalSearch* plugin = wxStaticCast(Manager::Get()->GetPluginManager()->FindPluginByName(_T("IncrementalSearch")), IncrementalSearch);
    plugin->SetMaxHistoryLen(maxItemsInHistory);


    // save colour-values
    cfg->Write(_T("/incremental_search/text_found_colour"),             XRCCTRL(*this, "cpIncSearchConfColourFound", wxColourPickerCtrl)->GetColour());
    cfg->Write(_T("/incremental_search/highlight_colour"),              XRCCTRL(*this, "cpIncSearchConfColourHighlight", wxColourPickerCtrl)->GetColour());
    cfg->Write(_T("/incremental_search/text_not_found_colour"),         XRCCTRL(*this, "cpIncSearchConfNotFoundBG", wxColourPickerCtrl)->GetColour());
    cfg->Write(_T("/incremental_search/wrapped_colour"),                XRCCTRL(*this, "cpIncSearchConfWrappedBG", wxColourPickerCtrl)->GetColour());
}

