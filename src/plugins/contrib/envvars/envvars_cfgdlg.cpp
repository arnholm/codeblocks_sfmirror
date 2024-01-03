/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/arrstr.h>
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/checklst.h>
    #include <wx/choice.h>
    #include <wx/panel.h>
    #include <wx/xrc/xmlres.h>

    #include "globals.h"
    #include "manager.h"
    #include "configmanager.h"
    #include "logmanager.h"
    #include "projectmanager.h"
#endif

#include "editpairdlg.h"

#include "envvars_common.h"
#include "envvars_cfgdlg.h"

// TODO (morten#1#): Save changes if another set is selected (more convenient).

// Uncomment this for tracing of method calls in C::B's DebugLog:
//#define TRACE_ENVVARS


// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

BEGIN_EVENT_TABLE(EnvVarsConfigDlg, wxPanel)
    EVT_CHOICE        (XRCID("choSet"),          EnvVarsConfigDlg::OnSetClick)
    EVT_BUTTON        (XRCID("btnCreateSet"),    EnvVarsConfigDlg::OnCreateSetClick)
    EVT_BUTTON        (XRCID("btnCloneSet"),     EnvVarsConfigDlg::OnCloneSetClick)
    EVT_BUTTON        (XRCID("btnRemoveSet"),    EnvVarsConfigDlg::OnRemoveSetClick)
    EVT_UPDATE_UI     (XRCID("btnRemoveSet"),    EnvVarsConfigDlg::OnUpdateUI)

    EVT_LISTBOX_DCLICK(XRCID("lstEnvVars"),      EnvVarsConfigDlg::OnEditEnvVarClick)
    EVT_CHECKLISTBOX  (XRCID("lstEnvVars"),      EnvVarsConfigDlg::OnToggleEnvVarClick)

    EVT_BUTTON        (XRCID("btnAddEnvVar"),    EnvVarsConfigDlg::OnAddEnvVarClick)
    EVT_BUTTON        (XRCID("btnEditEnvVar"),   EnvVarsConfigDlg::OnEditEnvVarClick)
    EVT_BUTTON        (XRCID("btnDeleteEnvVar"), EnvVarsConfigDlg::OnDeleteEnvVarClick)
    EVT_BUTTON        (XRCID("btnClearEnvVars"), EnvVarsConfigDlg::OnClearEnvVarsClick)
    EVT_BUTTON        (XRCID("btnSetEnvVars"),   EnvVarsConfigDlg::OnSetEnvVarsClick)
    EVT_UPDATE_UI     (XRCID("btnAddEnvVar"),    EnvVarsConfigDlg::OnUpdateUI)
    EVT_UPDATE_UI     (XRCID("btnEditEnvVar"),   EnvVarsConfigDlg::OnUpdateUI)
    EVT_UPDATE_UI     (XRCID("btnDeleteEnvVar"), EnvVarsConfigDlg::OnUpdateUI)
    EVT_UPDATE_UI     (XRCID("btnClearEnvVars"), EnvVarsConfigDlg::OnUpdateUI)
END_EVENT_TABLE()

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

EnvVarsConfigDlg::EnvVarsConfigDlg(wxWindow* parent, EnvVars* plugin) : m_plugin(plugin)
{
    wxXmlResource::Get()->LoadPanel(this, parent, "dlgEnvVars");
    LoadSettings();
}// EnvVarsConfigDlg

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

EnvVarsConfigDlg::~EnvVarsConfigDlg()
{
    if (m_plugin->IsAttached())
    {
        ProjectManager* ProjMan = Manager::Get()->GetProjectManager();
        //set active project environnement variables set
        m_plugin->DoProjectActivate(ProjMan->GetActiveProject());
    }
}

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnUpdateUI(wxUpdateUIEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnUpdateUI");
#endif

    // toggle remove envvar set button
    wxChoice* choSet = XRCCTRL(*this, "choSet", wxChoice);
    if (!choSet)
        return;

    bool en = (choSet->GetCount() > 1);
    XRCCTRL(*this, "btnRemoveSet",    wxButton)->Enable(en);

    // toggle edit/delete/clear/set env vars buttons
    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars || lstEnvVars->IsEmpty())
        return;

    en = (lstEnvVars->GetSelection() >= 0);
    XRCCTRL(*this, "btnEditEnvVar",   wxButton)->Enable(en);
    XRCCTRL(*this, "btnDeleteEnvVar", wxButton)->Enable(en);

    en = (lstEnvVars->GetCount() != 0);
    XRCCTRL(*this, "btnClearEnvVars", wxButton)->Enable(en);
    XRCCTRL(*this, "btnSetEnvVars",   wxButton)->Enable(en);
}// OnUpdateUI

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::LoadSettings()
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("LoadSettings");
#endif

    wxChoice* choSet = XRCCTRL(*this, "choSet", wxChoice);
    if (!choSet)
        return;

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    wxCheckBox* chkDebugLog = XRCCTRL(*this, "chkDebugLog", wxCheckBox);
    if (!chkDebugLog)
        return;

    // load and apply configuration (to application and GUI)
    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    // Load all envvar sets available
    choSet->Set(nsEnvVars::GetEnvvarSetNames());
    EV_DBGLOG("Found %u envvar sets in config.", choSet->GetCount());

    // Read the currently active envvar set and select it
    m_active_set = nsEnvVars::GetActiveSetName();
    const int active_set_idx = choSet->FindString(m_active_set);
    choSet->SetSelection(active_set_idx);

    lstEnvVars->Clear();
    chkDebugLog->SetValue(cfg->ReadBool("/debug_log"));

    // Show currently activated set in debug log (for reference)
    const wxString active_set_path(nsEnvVars::GetSetPathByName(m_active_set));
    EV_DBGLOG("Active envvar set is '%s' at index %d, config path '%s'.",
              m_active_set,
              active_set_idx,
              active_set_path);

    // NOTE: Keep this in sync with nsEnvVars::EnvvarSetApply
    // Read and show all envvars from currently active set in listbox
    const wxArrayString vars(nsEnvVars::GetEnvvarsBySetPath(active_set_path));
    const unsigned long envvars_total = vars.GetCount();
    unsigned long envvars_applied = 0;
    for (unsigned long i = 0; i < envvars_total; ++i)
    {
        // Format: [checked?]|[key]|[value]
        const wxArrayString var_array(nsEnvVars::EnvvarStringTokeniser(vars[i]));
        if (nsEnvVars::EnvvarArrayApply(var_array, lstEnvVars))
            ++envvars_applied;
        else
            EV_DBGLOG("Invalid envvar in '%s' at position #%lu.", active_set_path, i);
    }// for

    if (envvars_total)
        EV_DBGLOG("%lu/%lu envvars applied within C::B focus.", envvars_applied, envvars_total);
}// LoadSettings

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::SaveSettings()
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("SaveSettings");
#endif

    wxChoice* choSet = XRCCTRL(*this, "choSet", wxChoice);
    if (!choSet)
        return;

    wxString active_set(choSet->GetStringSelection());
    if (active_set.empty())
      active_set = nsEnvVars::EnvVarsDefault;

    SaveSettingsActiveSet(active_set);
    SaveSettings(active_set);
}

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::SaveSettings(const wxString& active_set)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("SaveSettings(wxString)");
#endif

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    wxCheckBox* chkDebugLog = XRCCTRL(*this, "chkDebugLog", wxCheckBox);
    if (!chkDebugLog)
        return;

    const wxString active_set_path(nsEnvVars::GetSetPathByName(active_set, false));
    EV_DBGLOG("Removing (old) envvar set '%s' at path '%s' from config.",
              active_set, active_set_path);

    cfg->DeleteSubPath(active_set_path);

    EV_DBGLOG("Saving (new) envvar set '%s'.", active_set);
    cfg->SetPath(active_set_path);

    const unsigned long envvars_count = lstEnvVars->GetCount();
    for (unsigned long i = 0; i < envvars_count; ++i)
    {
        // Format: [checked?]|[key]|[value]
        const wxUniChar check = lstEnvVars->IsChecked(i) ? '1' : '0';
        const wxString key(lstEnvVars->GetString(i).BeforeFirst('=').Trim(true).Trim(false));
        const wxString value(lstEnvVars->GetString(i).AfterFirst('=').Trim(true).Trim(false));

        wxString txt;
        txt << check << nsEnvVars::EnvVarsSep << key
                     << nsEnvVars::EnvVarsSep << value;

        cfg->Write(wxString::Format("EnvVar%lu", i), txt);
    }// for

    cfg->Write("/debug_log", chkDebugLog->GetValue());
}// SaveSettings

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::SaveSettingsActiveSet(wxString active_set)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("SaveSettingsActiveSet");
#endif

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    if (active_set.empty())
        active_set = nsEnvVars::EnvVarsDefault;

    EV_DBGLOG("Saving '%s' as active envvar set to config.", active_set);
    cfg->Write("/active_set", active_set);
}// SaveSettingsActiveSet

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnSetClick(wxCommandEvent& event)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnSetClick");
#endif

    SaveSettings(m_active_set);
    SaveSettingsActiveSet(event.GetString());
    LoadSettings();
}// OnSetClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnCreateSetClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnCreateSetClick");
#endif

    wxChoice* choSet = XRCCTRL(*this, "choSet", wxChoice);
    if (!choSet)
        return;

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    wxString set(cbGetTextFromUser(_("Enter (lower case) name for new environment variables set:"),
                                   _("Input Set"), nsEnvVars::EnvVarsDefault, this));

    if (set.empty())
        return;

    set.MakeLower();
    if (!VerifySetUnique(choSet, set))
        return;

    EV_DBGLOG("Unsetting variables of envvar set '%s'.",
              choSet->GetString(choSet->GetCurrentSelection()));

    nsEnvVars::EnvvarsClearUI(lstEnvVars); // Don't care about return value
    lstEnvVars->Clear();

    choSet->SetSelection(choSet->Append(set));

    SaveSettings();
    LoadSettings();
}// OnCreateSetClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnCloneSetClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnCloneSetClick");
#endif

    wxChoice* choSet = XRCCTRL(*this, "choSet", wxChoice);
    if (!choSet)
        return;

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    wxString set(cbGetTextFromUser(_("Enter (lower case) name for cloned environment variables set:"),
                                   _("Input Set"), nsEnvVars::EnvVarsDefault, this));

    if (set.empty())
        return;

    set.MakeLower();
    if (!VerifySetUnique(choSet, set))
        return;

    choSet->SetSelection(choSet->Append(set));

    // Clone envvars set in config
    SaveSettings();
    LoadSettings();
}// OnCloneSetClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnRemoveSetClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnRemoveSetClick");
#endif

    wxChoice* choSet = XRCCTRL(*this, "choSet", wxChoice);
    if (!choSet)
        return;

    if (choSet->GetCount()<2)
    {
        cbMessageBox(_("Must have at least one set active (can be empty)."),
                     _("Information"), wxICON_INFORMATION);
        return;
    }

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    if (cbMessageBox(_("Are you sure you want to delete the set?"),
                     _("Confirmation"),
                     wxYES | wxNO | wxICON_QUESTION) == wxID_YES)
    {
        // Obtain active set
        int active_set_idx = choSet->GetCurrentSelection();
        const wxString active_set(choSet->GetString(active_set_idx));

        // Remove envvars from C::B focus (and listbox)
        EV_DBGLOG("Unsetting variables of envvar set '%s'.", active_set);
        nsEnvVars::EnvvarsClearUI(lstEnvVars); // Don't care about return value

        // Remove envvars set from config
        const wxString active_set_path(nsEnvVars::GetSetPathByName(active_set, false));
        EV_DBGLOG("Removing envvar set '%s' at path '%s' from config.",
                  active_set, active_set_path);

        cfg->DeleteSubPath(active_set_path);

        // Remove envvars set from choicebox
        choSet->Delete(active_set_idx);
        // Select next item if available, previous if not
        if (active_set_idx == (int)choSet->GetCount())
          active_set_idx--;

        choSet->SetSelection(active_set_idx);
    }// if

    SaveSettingsActiveSet(choSet->GetStringSelection());
    LoadSettings();
}// OnRemoveSetClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnToggleEnvVarClick(wxCommandEvent& event)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnToggleEnvVarClick");
#endif

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    const int sel = event.GetInt();
    if (sel < 0)
        return;

    const wxString key(lstEnvVars->GetString(sel).BeforeFirst('=').Trim(true).Trim(false));
    if (key.empty())
        return;

    if (lstEnvVars->IsChecked(sel))
    {
        // Is has been toggled ON -> set envvar now
        const wxString value(lstEnvVars->GetString(sel).AfterFirst('=').Trim(true).Trim(false));
        if (!nsEnvVars::EnvvarApply(key, value))
            lstEnvVars->Check(sel, false); // Unset on UI to mark it's NOT set
    }
    else
    {
        // Is has been toggled OFF -> unset envvar now
        nsEnvVars::EnvvarDiscard(key); // Don't care about return value
    }
}// OnToggleEnvVarClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnAddEnvVarClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnAddEnvVarClick");
#endif

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    wxString key;
    wxString value;
    EditPairDlg dlg(this, key, value, _("Add new variable"), EditPairDlg::bmBrowseForDirectory);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        key.Trim(true).Trim(false);
        value.Trim(true).Trim(false);

        if (nsEnvVars::EnvvarVetoUI(key, NULL, -1))
            return;

        const int sel = lstEnvVars->Append(key + " = " + value, new nsEnvVars::EnvVariableListClientData(key, value));
        const bool success = nsEnvVars::EnvvarApply(key, value);
        if (sel >= 0)
            lstEnvVars->Check(sel, success);
    }
}// OnAddEnvVarClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnEditEnvVarClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnEditEnvVarClick");
#endif

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    const int sel = lstEnvVars->GetSelection();
    if (sel == wxNOT_FOUND)
        return;

    nsEnvVars::EnvVariableListClientData *data;
    data = static_cast <nsEnvVars::EnvVariableListClientData*> (lstEnvVars->GetClientObject(sel));
    wxString key(data->key);
    if (key.empty())
        return;

    wxString value(data->value);
    const wxString old_key(key);
    const wxString old_value(value);

    EditPairDlg dlg(this, key, value, _("Edit variable"), EditPairDlg::bmBrowseForDirectory);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() != wxID_OK)
        return;

    key.Trim(true).Trim(false);
    value.Trim(true).Trim(false);

    // filter illegal envvars with no key
    if (key.empty())
    {
        cbMessageBox(_("Cannot set an empty environment variable key."),
                     _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
        return;
    }

    // is this envvar to be set?
    bool was_checked = lstEnvVars->IsChecked(sel);
    const bool bDoSet = (((key != old_key) || (value != old_value)) && was_checked);
    if (bDoSet)
    {
        // unset the old envvar if it's key name has changed
        if (key != old_key)
        {
            nsEnvVars::EnvvarDiscard(old_key); // Don't care about return value
            if (nsEnvVars::EnvvarVetoUI(key, lstEnvVars, sel))
                return;
        }

        // set the new envvar
        if (!nsEnvVars::EnvvarApply(key, value))
        {
            lstEnvVars->Check(sel, false); // Unset on UI to mark it's NOT set
            was_checked = false;
        }
    }

    // update the GUI to the (new/updated/same) key/value pair anyway
    lstEnvVars->SetString(sel, key + " = " + value);
    lstEnvVars->Check(sel, was_checked);
    data->key = key;
    data->value = value;
}// OnEditEnvVarClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnDeleteEnvVarClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnDeleteEnvVarClick");
#endif

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars)
        return;

    const int sel = lstEnvVars->GetSelection();
    if (sel == wxNOT_FOUND)
        return;

    const wxString key(static_cast<nsEnvVars::EnvVariableListClientData*>(lstEnvVars->GetClientObject(sel))->key);
    if (key.empty())
        return;

    if (cbMessageBox(_("Are you sure you want to delete this variable?"),
                     _("Confirmation"),
                     wxYES_NO | wxICON_QUESTION) == wxID_YES)
    {
        nsEnvVars::EnvvarDiscard(key); // Don't care about return value
        lstEnvVars->Delete(sel);
    }
}// OnDeleteEnvVarClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnClearEnvVarsClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnClearEnvVarsClick");
#endif

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars || lstEnvVars->IsEmpty())
        return;

    if (cbMessageBox(_("Are you sure you want to clear and unset all variables?"),
                     _("Confirmation"),
                     wxYES | wxNO | wxICON_QUESTION) == wxID_YES)
        nsEnvVars::EnvvarsClearUI(lstEnvVars); // Don't care about return value
}// OnClearEnvVarsClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsConfigDlg::OnSetEnvVarsClick(wxCommandEvent& WXUNUSED(event))
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnSetEnvVarsClick");
#endif

    wxCheckListBox* lstEnvVars = XRCCTRL(*this, "lstEnvVars", wxCheckListBox);
    if (!lstEnvVars || lstEnvVars->IsEmpty())
        return;

    if (cbMessageBox(_("Are you sure you want to set all variables?"),
                     _("Confirmation"),
                     wxYES | wxNO | wxICON_QUESTION) != wxID_YES)
        return;

    wxString envsNotSet;

    // Set all (checked) variables of lstEnvVars
    const int envvars_count = (int)lstEnvVars->GetCount();
    for (int i = 0; i < envvars_count; ++i)
    {
        if (lstEnvVars->IsChecked(i))
        {
            nsEnvVars::EnvVariableListClientData *data;
            data = static_cast <nsEnvVars::EnvVariableListClientData*> (lstEnvVars->GetClientObject(i));
            if (!data->key.empty())
            {
                if (!nsEnvVars::EnvvarApply(data->key, data->value))
                {
                    lstEnvVars->Check(i, false); // Unset on UI to mark it's NOT set

                    // Setting envvar failed. Remember this key to report later.
                    if (envsNotSet.empty())
                        envsNotSet << data->key;
                    else
                        envsNotSet << ", " << data->key;
                }
            }
        }
    }// for

    if (!envsNotSet.empty())
    {
        wxString msg;
        msg.Printf(_("There was an error setting the following environment variables:\n%s"),
                   envsNotSet.wx_str() );

        cbMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
    }
}// OnSetEnvVarsClick

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool EnvVarsConfigDlg::VerifySetUnique(const wxChoice* choSet, wxString set)
{
    const unsigned int set_count = choSet->GetCount();
    set.MakeLower();
    for (unsigned int i = 0; i < set_count; ++i)
    {
        if (set.IsSameAs(choSet->GetString(i).MakeLower()))
        {
            cbMessageBox(_("This set already exists."), _("Error"),
                         wxOK | wxCENTRE | wxICON_EXCLAMATION);

            return false;
        }
    }

    return true;
}// VerifySetUnique
