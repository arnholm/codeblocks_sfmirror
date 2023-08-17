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
#include "prep.h"
#ifndef CB_PRECOMP
  #include <wx/checklst.h>
  #include <wx/utils.h>

  #include "configmanager.h"
  #include "globals.h"
  #include "manager.h"
  #include "macrosmanager.h"
  #include "logmanager.h"
#endif

#include <map>
#include <utility> // std::pair

#include "envvars_common.h"

// Uncomment this for tracing of method calls in C::B's DebugLog:
//#define TRACE_ENVVARS

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

namespace nsEnvVars
{
    const wxUniChar             EnvVarsSep     = '|';
    const wxString              EnvVarsDefault = "default";
    std::map<wxString,wxString> EnvVarsStack;
}

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvVarsDebugLog()
{
    // load and apply configuration (to application only)
    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return false;

    // get whether to print debug message to debug log or not
    return cfg->ReadBool("/debug_log");
}// EnvVarsDebugLog

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

wxArrayString nsEnvVars::EnvvarStringTokeniser(const wxString& str)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarStringTokeniser");
#endif
    // tokenise string like:
    // C:\Path;"D:\Other Path"

    wxArrayString out;

    wxString search(str);
    search.Trim(true).Trim(false);

    // trivial case: string is empty or consists of blanks only
    if (search.empty())
        return out;

    wxString token;
    bool     inside_quot = false;
    size_t   pos         = 0;
    const size_t length  = search.Length();
    while (pos < length)
    {
        const wxUniChar current_char = search.GetChar(pos);

        // for e.g. /libpath:"C:\My Folder"
        if (current_char == '"')
          inside_quot = !inside_quot;

        if ((current_char == nsEnvVars::EnvVarsSep) && !inside_quot)
        {
            if (!token.empty())
            {
                out.Add(token);
                token.Clear();
            }
        }
        else
            token.Append(current_char);

        pos++;
        // Append final token
        if ((pos == length) && !inside_quot && !token.empty())
            out.Add(token);
    }// while

    return out;
}// EnvvarStringTokeniser

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

wxArrayString nsEnvVars::GetEnvvarSetNames()
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("GetEnvvarSetNames");
#endif

    wxArrayString set_names;

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
    {
        set_names.Add(nsEnvVars::EnvVarsDefault);
        return set_names;
    }

    // Read all envvar sets available
    const wxArrayString sets(cfg->EnumerateSubPaths("/sets"));
    const unsigned long num_sets = sets.GetCount();
    EV_DBGLOG("Found %lu envvar sets in config.", num_sets);

    if (num_sets == 0)
    {
        set_names.Add(nsEnvVars::EnvVarsDefault);
    }
    else
    {
        for (unsigned long i = 0; i < num_sets; ++i)
        {
            wxString set_name(sets[i]);
            if (set_name.empty())
                set_name.Printf("Set%lu", i);

            set_names.Add(set_name);
        }// for
    }// if

    set_names.Sort();
    return set_names;
}// GetEnvvarSetNames

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

wxString nsEnvVars::GetActiveSetName()
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("GetActiveSetName");
#endif

    wxString active_set(nsEnvVars::EnvVarsDefault);

    // load and apply configuration (to application only)
    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return active_set;

    // try to get the envvar set name of the currently active global envvar set
    const wxString active_set_cfg(cfg->Read("/active_set"));
    if (!active_set_cfg.empty())
        active_set = active_set_cfg;

    EV_DBGLOG("Obtained '%s' as active envvar set from config.", active_set);
    return active_set;
}// GetActiveSetName

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

wxString nsEnvVars::GetSetPathByName(const wxString& set_name, bool check_exists,
                                     bool return_default)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("GetSetPathByName");
#endif

    if (set_name.empty())
        return wxString();

    const wxString set_path("/sets/"+set_name);
    if (!check_exists)
        return set_path;

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (cfg && (cfg->EnumerateSubPaths("/sets").Index(set_name) != wxNOT_FOUND))
        return set_path;

    if (!return_default)
        return wxString();

    return "/sets/"+nsEnvVars::EnvVarsDefault; // fall back solution
}// GetSetPathByName

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

wxArrayString nsEnvVars::GetEnvvarsBySetPath(const wxString& set_path)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("GetEnvvarsBySetPath");
#endif

    wxArrayString envvars;

    EV_DBGLOG("Searching for envvars in path '%s'.", set_path);

    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg || set_path.empty())
      return envvars;

    const wxArrayString envvars_keys(cfg->EnumerateKeys(set_path));
    const size_t num_envvars = envvars_keys.GetCount();
    for (size_t i = 0; i < num_envvars; ++i)
    {
        const wxString envvar(cfg->Read(set_path+'/'+envvars_keys[i]));
        if (!envvar.empty())
            envvars.Add(envvar);
        else
            EV_DBGLOG("Warning: empty envvar '%s' detected and skipped.", envvars_keys[i]);
    }

    EV_DBGLOG("Read %zu/%zu envvars in path '%s'.", envvars.GetCount(), num_envvars, set_path);

    return envvars;
}// GetEnvvarsBySetPath

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarSetExists(const wxString& set_name)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarSetExists");
#endif

    if (set_name.empty())
        return false;

    const wxString set_path(nsEnvVars::GetSetPathByName(set_name, true, false));
    if (set_path.empty())
        return false;

    return true;
}// EnvvarSetExists

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarVetoUI(const wxString& key, wxCheckListBox* lstEnvVars, int sel)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarVetoUI");
#endif

    if (wxGetEnv(key, NULL))
    {
        const wxString recursion(platform::windows ? "PATH=%PATH%;C:\\NewPath" : "PATH=$PATH:/new_path");

        wxString warn_exist;
        warn_exist.Printf(_("Warning: Environment variable '%s' is already set.\n"
                            "Continue with updating it's value?\n"
                            "(Recursions like '%s' will be considered.)"),
                            key, recursion);

        if (cbMessageBox(warn_exist, _("Confirmation"),
                         wxYES_NO | wxICON_QUESTION) == wxID_NO)
        {
            if (lstEnvVars && (sel >= 0))
                lstEnvVars->Check(sel, false); // Unset to visualise it's NOT set

            return true; // User has vetoed the operation
        }
    }// if

    return false;
}// EnvvarVetoUI

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarsClearUI(wxCheckListBox* lstEnvVars)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarsClearUI");
#endif

    if (!lstEnvVars)
        return false;

    wxString envsNotUnSet;

    // Unset all (checked) variables of lstEnvVars
    const unsigned long lst_count = lstEnvVars->GetCount();
    for (unsigned long i = 0; i < lst_count; ++i)
    {
        // Note: It's better not to just clear all because wxUnsetEnv would
        //       fail in case an envvar is not set (not checked).
        if (lstEnvVars->IsChecked(i))
        {
            const wxString key(lstEnvVars->GetString(i).BeforeFirst('=').Trim(true).Trim(false));
            if (!key.empty())
            {
                if (!nsEnvVars::EnvvarDiscard(key))
                {
                    // Setting env.-variable failed. Remember this key to report later.
                    if (!envsNotUnSet.empty())
                        envsNotUnSet << ", ";

                    envsNotUnSet << key;
                }
            }
        }
    }// for

    lstEnvVars->Clear();

    if (!envsNotUnSet.empty())
    {
        wxString msg;
        msg.Printf(_("There was an error unsetting the following environment variables:\n%s"),
                   envsNotUnSet);

        cbMessageBox(msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
        return false;
    }

    return true;
}// EnvvarsClearUI

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarIsRecursive(const wxString& key, const wxString& value)
{
    // Replace all macros the user might have setup for the key
    wxString the_key(key);
    Manager::Get()->GetMacrosManager()->ReplaceMacros(the_key);
    const wxString recursion(platform::windows ? "%"+the_key+"%" : "$"+the_key);
    return value.Contains(recursion);
}// EnvvarIsRecursive

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarDiscard(const wxString &key)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarDiscard");
#endif

    // Replace all macros the user might have setup for the key
    wxString the_key(key);
    Manager::Get()->GetMacrosManager()->ReplaceMacros(the_key);
    if (the_key.Trim().empty())
        return false;

    if (!wxGetEnv(the_key, NULL))
        return false; // envvar was not set - nothing to do.

    std::map<wxString,wxString>::iterator it = nsEnvVars::EnvVarsStack.find(the_key);
    if (it != nsEnvVars::EnvVarsStack.end()) // found an old envvar on the stack
        return nsEnvVars::EnvvarApply(the_key, it->second); // restore old value

    if (!wxUnsetEnv(the_key))
    {
        Manager::Get()->GetLogManager()->Log(wxString::Format(_("Unsetting environment variable '%s' failed."), the_key));
        EV_DBGLOG("Unsetting environment variable '%s' failed.", the_key);
        return false;
    }

    return true;
}// EnvvarDiscard

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarApply(const wxString& key, const wxString& value)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarApply");
#endif

    // Replace all macros the user might have setup for the key
    wxString the_key(key);
    Manager::Get()->GetMacrosManager()->ReplaceMacros(the_key);
    if (the_key.Trim().empty())
        return false;

    // Value: First, expand stuff like:
    //        set PATH=%PATH%;C:\NewPath OR export PATH=$PATH:/new_path
    //        After, replace all macros the user might have used in addition
    wxString the_value(value);
    wxString value_set;
    const bool is_set = wxGetEnv(the_key, &value_set);
    if (is_set)
    {
      std::map <wxString, wxString>::iterator it = nsEnvVars::EnvVarsStack.find(the_key);
      if (it == nsEnvVars::EnvVarsStack.end()) // envvar not already on the stack
          nsEnvVars::EnvVarsStack[the_key] = value_set; // remember the old value

      // Avoid endless recursion if the value set contains e.g. $PATH, too
      if (nsEnvVars::EnvvarIsRecursive(the_key, the_value))
      {
          if (nsEnvVars::EnvvarIsRecursive(the_key, value_set))
          {
              EV_DBGLOG("Setting environment variable '%s' failed "
                        "due to unresolvable recursion.", the_key);
              return false;
          }

          // Restore original value in case of recursion before
          if (it != nsEnvVars::EnvVarsStack.end())
              value_set = nsEnvVars::EnvVarsStack[the_key];

          // Resolve recursion now (if any)
          const wxString recursion(platform::windows ? "%"+the_key+"%" : "$"+the_key);
          the_value.Replace(recursion, value_set);
        }
    }

    // Replace all macros the user might have setup for the value
    Manager::Get()->GetMacrosManager()->ReplaceMacros(the_value);

    EV_DBGLOG("Trying to set environment variable '%s' to value '%s'...",
              the_key, the_value);

    if (!wxSetEnv(the_key, the_value)) // set the envvar as computed
    {
        EV_DBGLOG("Setting environment variable '%s' failed.", the_key);
        return false;
    }

    return true;
}// EnvvarApply

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

bool nsEnvVars::EnvvarArrayApply(const wxArrayString& envvar,
                                 wxCheckListBox*      lstEnvVars)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarArrayApply");
#endif

    if (envvar.GetCount() != 3)
        return false;

    wxString check = envvar[0];
    wxString key   = envvar[1];
    wxString value = envvar[2];

    const bool bCheck = check.Trim(true).Trim(false).IsSameAs("1");
    key.Trim(true).Trim(false);
    value.Trim(true).Trim(false);

    int sel = -1;
    if (lstEnvVars)
    {
        sel = lstEnvVars->Append(key + " = " + value, new EnvVariableListClientData(key, value));
        lstEnvVars->Check(sel, bCheck);
    }

    if (!bCheck)
        return true; // No need to apply -> success, too.

    const bool success = EnvvarApply(key, value);
    if (!success && lstEnvVars && (sel >= 0))
        lstEnvVars->Check(sel, false); // Unset on UI to mark it's NOT set

    return success;
}// EnvvarArrayApply

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void nsEnvVars::EnvvarSetApply(const wxString& set_name, bool even_if_active)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarSetApply");
#endif

    // Load and apply envvar set from config (to application only)
    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    // Stores the currently active envar set that has been successfully applied at last
    static wxString last_set_applied = wxEmptyString;

    wxString set_to_apply(set_name);
    if (set_to_apply.empty())
        set_to_apply = nsEnvVars::GetActiveSetName();

    // Early exit for a special case requested by even_if_active parameter
    if (!even_if_active && set_to_apply.IsSameAs(last_set_applied))
    {
        EV_DBGLOG("Set '%s' will not be applied (already active).", set_to_apply);
        return;
    }

    // Show currently activated set in debug log (for reference)
    const wxString set_path(nsEnvVars::GetSetPathByName(set_to_apply));
    EV_DBGLOG("Active envvar set is '%s', config path '%s'.", set_to_apply, set_path);

    // NOTE: Keep this in sync with EnvVarsConfigDlg::LoadSettings
    // Read and apply all envvars from currently active set in config
    const wxArrayString vars(nsEnvVars::GetEnvvarsBySetPath(set_path));
    const unsigned long envvars_total = vars.GetCount();
    unsigned long envvars_applied = 0;
    for (unsigned long i = 0; i < envvars_total; ++i)
    {
        // Format: [checked?]|[key]|[value]
        const wxArrayString var_array(nsEnvVars::EnvvarStringTokeniser(vars[i]));
        if (nsEnvVars::EnvvarArrayApply(var_array))
            envvars_applied++;
        else
            EV_DBGLOG("Invalid envvar in '%s' at position #%lu.", set_path, i);
    }// for

    if (envvars_total > 0)
    {
        last_set_applied = set_to_apply;
        EV_DBGLOG("%lu/%lu envvars applied within C::B focus.", envvars_applied, envvars_total);
    }
}// EnvvarSetApply

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----

void nsEnvVars::EnvvarSetDiscard(const wxString& set_name)
{
#if defined(TRACE_ENVVARS)
    Manager::Get()->GetLogManager()->DebugLog("EnvvarSetDiscard");
#endif

    // load and apply envvar set from config (to application only)
    ConfigManager *cfg = Manager::Get()->GetConfigManager("envvars");
    if (!cfg)
        return;

    wxString set_to_discard(set_name);
    if (set_to_discard.empty())
        set_to_discard = nsEnvVars::GetActiveSetName();

    // Show currently activated set in debug log (for reference)
    const wxString set_path(nsEnvVars::GetSetPathByName(set_to_discard));
    EV_DBGLOG("Active envvar set is '%s', config path '%s'.", set_to_discard, set_path);

    // Read and apply all envvars from currently active set in config
    const wxArrayString vars(nsEnvVars::GetEnvvarsBySetPath(set_path));
    const unsigned long envvars_total = vars.GetCount();
    unsigned long envvars_discarded = 0;
    for (unsigned long i = 0; i < envvars_total; ++i)
    {
        // Format: [checked?]|[key]|[value]
        const wxArrayString var_array(nsEnvVars::EnvvarStringTokeniser(vars[i]));
        if (var_array.GetCount() == 3)
        {
            wxString check(var_array[0]);
            const bool bCheck = check.Trim(true).Trim(false).IsSameAs("1");
            // Do not unset envvars that are not activated (checked)
            if (!bCheck)
                continue; // next for-loop

            // unset the old envvar
            if (nsEnvVars::EnvvarDiscard(var_array[1]))
                envvars_discarded++;
        }
        else
            EV_DBGLOG("Invalid envvar in '%s' at position #%lu.", set_path, i);
    }// for

    if (envvars_total > 0)
        EV_DBGLOG("%lu/%lu envvars discarded within C::B focus.", envvars_discarded, envvars_total);
}// EnvvarSetDiscard
