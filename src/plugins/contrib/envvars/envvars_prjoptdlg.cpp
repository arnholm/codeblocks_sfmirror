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
    #include <wx/xrc/xmlres.h>
    #include <wx/checkbox.h>
    #include <wx/choice.h>

    #include "cbproject.h"
    #include "logmanager.h"
#endif

#include "envvars.h"
#include "envvars_common.h"
#include "envvars_prjoptdlg.h"

// Uncomment this for tracing of method calls in C::B's DebugLog:
//#define TRACE_ENVVARS

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

BEGIN_EVENT_TABLE(EnvVarsProjectOptionsDlg, wxPanel)
    EVT_UPDATE_UI(-1, EnvVarsProjectOptionsDlg::OnUpdateUI)
END_EVENT_TABLE()

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

EnvVarsProjectOptionsDlg::EnvVarsProjectOptionsDlg(wxWindow* parent, cbProject* project) :
  m_pProject(project)
{
    wxXmlResource::Get()->LoadPanel(this, parent, "pnlProjectEnvVarsOptions");

    wxChoice* choice_control = XRCCTRL(*this, "choEnvvarSets", wxChoice);
    if (!choice_control)
        return;

    wxCheckBox* checkbox_control = XRCCTRL(*this, "chkEnvvarSet", wxCheckBox);
    if (!checkbox_control)
        return;

    choice_control->Set(nsEnvVars::GetEnvvarSetNames());
    if (!choice_control->IsEmpty())
    {
        const wxString envvar_set(EnvVars::ParseProjectEnvvarSet(project));
        if (envvar_set.empty())
        {
            checkbox_control->SetValue(false);
            choice_control->SetSelection(0);
            choice_control->Disable();
        }
        else
        {
            checkbox_control->SetValue(true);
            choice_control->SetStringSelection(envvar_set);
            choice_control->Enable();
        }
    }
}// EnvVarsProjectOptionsDlg

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

EnvVarsProjectOptionsDlg::~EnvVarsProjectOptionsDlg()
{
}// ~EnvVarsProjectOptionsDlg

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsProjectOptionsDlg::OnUpdateUI(wxUpdateUIEvent& event)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnUpdateUI");
#endif

    wxCheckBox* checkbox_control = XRCCTRL(*this, "chkEnvvarSet", wxCheckBox);
    if (checkbox_control)
    {
        wxChoice* choice_control = XRCCTRL(*this, "choEnvvarSets", wxChoice);
        if (choice_control)
            choice_control->Enable(checkbox_control->IsChecked());
    }

    event.Skip();
}// OnUpdateUI

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void EnvVarsProjectOptionsDlg::OnApply()
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("OnApply");
#endif

    const wxString current_envvar_set(EnvVars::ParseProjectEnvvarSet(m_pProject));

    wxString new_envvar_set;
    wxCheckBox* checkbox_control = XRCCTRL(*this, "chkEnvvarSet", wxCheckBox);
    if (checkbox_control && checkbox_control->IsChecked())
    {
        wxChoice* choice_control = XRCCTRL(*this, "choEnvvarSets", wxChoice);
        if (choice_control)
            new_envvar_set = choice_control->GetStringSelection();
    }

    if (current_envvar_set != new_envvar_set)
    {
        EV_DBGLOG("Changing envvars set from '%s' to '%s'.", current_envvar_set, new_envvar_set);
        nsEnvVars::EnvvarSetDiscard(current_envvar_set);
        nsEnvVars::EnvvarSetApply(new_envvar_set, true);
        EnvVars::SaveProjectEnvvarSet(m_pProject, new_envvar_set);
    }
}// OnApply
