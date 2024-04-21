/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include "uservarmanager.h"
    #include "configmanager.h"
    #include "logmanager.h"
    #include "projectmanager.h"
    #include "macrosmanager.h"
    #include "manager.h"
    #include "cbexception.h"
    #include "infowindow.h"

    #include <wx/button.h>
    #include "scrollingdialog.h"
    #include <wx/intl.h>
    #include <wx/xrc/xmlres.h>
    #include <wx/textctrl.h>
    #include <wx/textdlg.h>
    #include <wx/splitter.h>
    #include <wx/choice.h>
    #include <wx/listbox.h>
    #include <wx/app.h>
#endif

#include "annoyingdialog.h"


#if wxCHECK_VERSION(3, 0, 0)
#include <wx/unichar.h>
#endif

#include <ctype.h>

namespace
{
    bool userVariableMgrIsBusy = false; //non-blocking mutex
}

template<> UserVariableManager* Mgr<UserVariableManager>::instance   = nullptr;
template<> bool                 Mgr<UserVariableManager>::isShutdown = false;

class UserVarManagerNoGuiUI : public UserVarManagerUI
{
public:
    ~UserVarManagerNoGuiUI()    {};

    void DisplayInfoWindow(cb_unused const wxString &title,const wxString &msg) override
    {
        Manager::Get()->GetLogManager()->LogWarning(msg);
    }

    void OpenEditWindow(cb_unused const std::set<wxString> &var) override { };
    wxString GetVariable(cb_unused wxWindow* parent, cb_unused const wxString &old) override
    {
        return wxString();
    };
};

void UserVariableManager::SetUI(std::unique_ptr<UserVarManagerUI> ui)
{
    m_ui = std::move(ui);
}


void UserVariableManager::Configure()
{
    m_ui->OpenEditWindow();

}

void UserVariableManager::Reload()
{
    m_ActiveSet = Manager::Get()->GetConfigManager(_T("gcv"))->Read(_T("/active"), UserVariableManagerConsts::defaultSetName);
    m_VariableSetMap.clear();
    wxArrayString sets = m_CfgMan->EnumerateSubPaths(UserVariableManagerConsts::cSets);
    for (const wxString& set : sets)
    {
        wxArrayString variables = m_CfgMan->EnumerateSubPaths(UserVariableManagerConsts::cSets + set);
        VariableMap variableMap;
        const wxString varPath(UserVariableManagerConsts::cSets + set + "/");
        for (const wxString& name : variables)
        {
            wxArrayString values = m_CfgMan->EnumerateKeys(varPath + name);
            UserVariable variable(name);
            for (const wxString& value : values)
            {
                const wxString str =  m_CfgMan->Read(varPath + name + "/" + value);
                variable.SetValue(value.Lower(), str);
            }
            variableMap.emplace(name, std::move(variable));
        }
        m_VariableSetMap.emplace(set, std::move(variableMap));
    }

    if (m_VariableSetMap.find(UserVariableManagerConsts::defaultSetName) == m_VariableSetMap.end())
    {
        // There is no default set, so lets create one...
        m_VariableSetMap[UserVariableManagerConsts::defaultSetName] = VariableMap();
    }
}

void UserVariableManager::Save()
{
    ConfigManager* manager = Manager::Get()->GetConfigManager(_T("gcv"));
    // Delete all sets, we write them new
    manager->DeleteSubPath(UserVariableManagerConsts::cSets);
    for (VariableSetMap::const_iterator itr = m_VariableSetMap.cbegin(); itr != m_VariableSetMap.cend(); ++itr)
    {
        const wxString& setName = itr->first;
        const VariableMap& variableMap = itr->second;
        for (VariableMap::const_iterator varItr = variableMap.begin(); varItr != variableMap.end(); ++varItr)
        {
            const wxString& varName = varItr->first;
            const UserVariable& var = varItr->second;
            std::vector<wxString> members = var.GetMembers();
            const wxString basePath = UserVariableManagerConsts::cSets + "/" + setName + "/" + varName + "/";
            for (const wxString& member : members)
            {
                wxString value;
                var.GetValue(member, value);
                manager->Write(basePath + member, value);
            }
        }
    }

    if (!setNameOverwrittenInParameter)
        manager->Write("/active", m_ActiveSet);
}

VariableSetMap UserVariableManager::GetVariableMap()
{
    return m_VariableSetMap;
}


void UserVariableManager::UpdateFromVariableMap(VariableSetMap otherSet)
{
    m_VariableSetMap = otherSet;
}

wxString UserVariableManager::Replace(const wxString& variable, std::vector<wxString>* errorMessages)
{

    wxString varName, memberName;
    if (!ParseVariableName(variable, varName, memberName))
    {
        if (errorMessages != nullptr)
            errorMessages->push_back(wxString::Format(_("Variable %s has not the right syntax"), variable));

        return wxString();
    }

    wxString value;
    if (!GetMemberValue(value, varName, memberName))
    {
        if (Manager::Get()->GetProjectManager()->IsLoading())
        {
            // a project/workspace is being loaded.
            // no need to bug the user now about global vars.
            // just preempt it; ProjectManager will call Arrogate() when it's done.
            Preempt(variable);
            return variable;
        }
        else
        {
            // Guard against an infinite loop here because this code is rooted in the wxAppBase::DoIdle()/OnUpdateUI() event
            // and can be called from the idle loop until the "Global Variable Editor dialog" is dismissed.
            if (userVariableMgrIsBusy)
                return value;

            // Instantiation/deallocation of this struct acts as a non-blocking code guard.
            // Any return from this point forward will set the guard to false.
            struct usrVarMgr_t
            {
                usrVarMgr_t()  { userVariableMgrIsBusy = true; }
                ~usrVarMgr_t() { userVariableMgrIsBusy = false; }
            } UsrVarMgr;

            wxString msg;
            msg.Printf(_("In the currently active set, Code::Blocks does not know\n"
                         "the global compiler variable \"%s\".\n\n"
                         "Please define it."), varName.wx_str());
            if (errorMessages != nullptr)
                errorMessages->push_back(msg);
            m_ui->DisplayInfoWindow(_("Global Compiler Variables"), msg);

            std::set<wxString> ar = {varName};
            m_ui->OpenEditWindow(ar);
            Reload();
        }
    }
    return value;
}

std::vector<wxString> UserVariableManager::GetVariableSetNames() const
{
    std::vector<wxString> names;
    names.reserve(m_VariableSetMap.size());
    for (VariableSetMap::const_iterator itr = m_VariableSetMap.cbegin(); itr != m_VariableSetMap.cend(); ++itr)
    {
        names.push_back(itr->first);
    }
    return names;
}

void UserVariableManager::CreateVariableSet(const wxString& setName)
{
    if (m_VariableSetMap.find(setName) != m_VariableSetMap.end())
        return;
    m_VariableSetMap[setName] = VariableMap();
}

std::vector<wxString> UserVariableManager::GetVariableNames(const wxString& setName) const
{
    std::vector<wxString> names;
    if (m_VariableSetMap.find(setName) == m_VariableSetMap.end())
        return names;
    const VariableMap& varMap = m_VariableSetMap.at(setName) ;
    names.reserve(varMap.size());
    for (VariableMap::const_iterator itr = varMap.cbegin(); itr != varMap.cend(); ++itr)
    {
        names.push_back(itr->first);
    }
    return names;
}

std::vector<wxString> UserVariableManager::GetMemberNames(const wxString& setName, const wxString& varName) const
{
    std::vector<wxString> names;
    const VariableSetMap::const_iterator itrSet = m_VariableSetMap.find(setName);
    if (itrSet == m_VariableSetMap.end())
        return names;

    const VariableMap& varMap = itrSet->second;
    const VariableMap::const_iterator itrVar = varMap.find(varName);
    if (itrVar == varMap.end())
        return names;

    const UserVariable& var = itrVar->second;
    return var.GetMembers();
}

bool UserVariableManager::GetMemberValue(wxString& value, const wxString& varName, const wxString& memberName) const
{
    if (GetMemberValue(m_ParameterVariableSetMap, value, m_ActiveSet, varName, memberName))
        return true;
    return GetMemberValue(m_VariableSetMap, value, m_ActiveSet, varName, memberName);
}

bool UserVariableManager::GetMemberValue(const VariableSetMap& setMap, wxString& value, const wxString& setName, const wxString& varName, const wxString& memberName) const
{
    const VariableSetMap::const_iterator itrSet = setMap.find(setName);
    if (itrSet == setMap.end())
        return false;

    const VariableMap& varMap = itrSet->second;
    const VariableMap::const_iterator itrVar = varMap.find(varName);
    if (itrVar == varMap.end())
        return false;

    const UserVariable& var = itrVar->second;
    return var.GetValue(memberName, value);
}


void UserVariableManager::Preempt(const wxString& variable)
{
    wxString varName, memberName;
    if (!ParseVariableName(variable, varName, memberName))
        return;

    if (!Exists(variable))
    {
        m_Preempted.insert(memberName);
    }
}

bool UserVariableManager::SetActiveSetName(const wxString& setName)
{
    if (m_VariableSetMap.find(setName) == m_VariableSetMap.end())
        return false;

    setNameOverwrittenInParameter = false;
    m_ActiveSet = setName;
    return true;
}

bool UserVariableManager::ParseVariableName(const wxString& variable, wxString& varName, wxString& memberName) const
{
    if (variable.find(_T('#')) == wxString::npos)
        return false;
    varName = variable.AfterLast(wxT('#')).BeforeFirst(wxT('.')).BeforeFirst(wxT(')')).MakeLower();
    if (variable.Contains("."))
        memberName = variable.AfterLast(wxT('.')).BeforeFirst(wxT(')')).MakeLower();
    else
        memberName = UserVariableManagerConsts::cBase;
    return true;
}

bool UserVariableManager::Exists(const VariableSetMap& setMap, const wxString& varName, const wxString& memberName) const
{
    wxString tmp;
    return GetMemberValue(setMap, tmp, m_ActiveSet, varName, memberName);
}

bool UserVariableManager::Exists(const VariableSetMap& setMap, const wxString& variable) const
{
    wxString varName, memberName;
    if (!ParseVariableName(variable, varName, memberName))
        return false;
    return Exists(setMap, varName, memberName);
}

bool UserVariableManager::Exists(const wxString& variable) const
{
    return Exists(m_ParameterVariableSetMap, variable) || Exists(m_VariableSetMap, variable);
}

bool UserVariableManager::IsOverridden(const wxString& variable) const
{
    return Exists(m_ParameterVariableSetMap, variable);
}

bool UserVariableManager::IsOverridden(const wxString& varName, const wxString& member) const
{
    return Exists(m_ParameterVariableSetMap, varName, member);
}

void UserVariableManager::Arrogate()
{
    if (m_Preempted.size() == 0)
        return;

    wxString peList;

    for (const wxString& var : m_Preempted)
    {
        peList << var << _T('\n');
    }
    peList = peList.BeforeLast('\n'); // remove trailing newline

    wxString msg;
    if (m_Preempted.size() == 1)
        msg.Printf(_("In the currently active set, Code::Blocks does not know\n"
                     "the global compiler variable \"%s\".\n\n"
                     "Please define it."), peList.wx_str());
    else
        msg.Printf(_("In the currently active set, Code::Blocks does not know\n"
                     "the following global compiler variables:\n"
                     "%s\n\n"
                     "Please define them."), peList.wx_str());

    m_ui->DisplayInfoWindow(_("Global Compiler Variables"), msg);
    m_ui->OpenEditWindow(m_Preempted);
    m_Preempted.clear();
}

UserVariableManager::UserVariableManager()
{
    m_CfgMan = Manager::Get()->GetConfigManager(_T("gcv"));
    m_ui = std::unique_ptr<UserVarManagerUI>(new UserVarManagerNoGuiUI());

    Migrate();
    Reload();
}

UserVariableManager::~UserVariableManager()
{

}

void UserVariableManager::ParseCommandLine(wxCmdLineParser& parser)
{
    // Parse command line
    // We search for "-D set.variable.member=value" or "-D variable.member=value"
    // or -D variablename=value
    // We also search for -S setName to set the current active set
    // we use the same code as wxWidgets uses to parse command line arguments

    // We can not use the wxCmdLineParser because it supports only one parameter but we want
    // to be able to parse multiple -D

    const int argc = wxApp::GetInstance()->argc;
    const wxCmdLineArgsArray& argv = wxApp::GetInstance()->argv;

    for (int i = 0; i < argc; ++i)
    {
        wxString arg(argv[i]);
        if (arg == "-D")    // if it is not -D, or -S we continue...
        {
            // the argument is -D so the next argument is our variable string
            ++i;
            if ( i >= argc)
            {
                Manager::Get()->GetLogManager()->LogError(_("Incomplete command line argument -D: missing variable name and value"));
                break;
            }
            arg = argv[i];
            arg.Trim().Trim(false);
            if (arg.StartsWith('-'))
            {
                Manager::Get()->GetLogManager()->LogError(_("Incomplete command line argument -D: missing variable name and value"));
                continue;
            }

            wxString variableName = arg.BeforeFirst('=');
            wxString value = arg.AfterFirst('=');
            if (variableName.IsEmpty())
            {
                Manager::Get()->GetLogManager()->LogError(_("Incomplete command line argument -D: missing variable name"));
                continue;
            }
            if (value.IsEmpty())
            {
                Manager::Get()->GetLogManager()->LogError(_("Incomplete command line argument -D: missing value"));
                continue;
            }

            wxArrayString variableArray = wxSplit(variableName,'.');
            if (variableArray.size() < 2 || variableArray.size() > 3)
            {
                Manager::Get()->GetLogManager()->LogError(_("Incomplete command line argument -D: variable name is not correct"));
                continue;
            }
            wxString setName = m_ActiveSet;
            wxString varName = variableArray[variableArray.size() - 2];
            wxString memberName = variableArray[variableArray.size() -1];
            if (variableArray.size() == 3)
                setName = variableArray[0];

            VariableMap& varMap = m_ParameterVariableSetMap[setName];
            const std::pair<VariableMap::iterator, bool> result = varMap.emplace(varName, varName);
            UserVariable& var = result.first->second;
            var.SetValue(memberName, value);

            Manager::Get()->GetLogManager()->LogError("Global Variables: Value for variable " + setName + "." + varName + "." + memberName + " set to " + value);
        }
    }

    wxString active;
    if ( parser.Found("S", &active))
    {
        if (!SetActiveSetName(active))
            Manager::Get()->GetLogManager()->LogError(_("Set " + active +" not found"));
    }
}

void UserVariableManager::Migrate()
{
    ConfigManager *cfgman_gcv = Manager::Get()->GetConfigManager(_T("gcv"));

    m_ActiveSet = cfgman_gcv->Read(_T("/active"));

    if (!m_ActiveSet.IsEmpty())
        return;

    m_ActiveSet = UserVariableManagerConsts::defaultSetName;
    cfgman_gcv->Exists("/sets/default/foo"); // assert /sets/default
    cfgman_gcv->Write("/active", m_ActiveSet);
    wxString oldpath;
    wxString newpath;

    ConfigManager *cfgman_old = Manager::Get()->GetConfigManager(_T("global_uservars"));
    wxArrayString vars = cfgman_old->EnumerateSubPaths(_T("/"));

    for (unsigned int i = 0; i < vars.GetCount(); ++i)
    {
        vars[i].Prepend(_T('/'));
        wxArrayString members = cfgman_old->EnumerateKeys(vars[i]);

        for (unsigned j = 0; j < members.GetCount(); ++j)
        {
            oldpath.assign(vars[i] + _T("/") + members[j]);
            newpath.assign(_T("/sets/default") + vars[i] + _T("/") + members[j]);

            cfgman_gcv->Write(newpath, cfgman_old->Read(oldpath));
        }
    }
    cfgman_old->Delete();
}

wxString UserVariableManager::GetVariable(wxWindow *parent, const wxString &old)
{
    return m_ui->GetVariable(parent, old);
}

bool UserVariableManager::SetVariable(const wxString& varName, const wxString& memberName, const wxString& value, bool createIfNotPresent)
{
    return SetVariable(GetActiveSetName(), varName, memberName, value, createIfNotPresent);
}

bool UserVariableManager::SetVariable(const wxString& setName, const wxString& varName, const wxString& memberName, const wxString& value, bool createIfNotPresent)
{
    const VariableSetMap::iterator itrSet = m_VariableSetMap.find(setName);
    if (itrSet == m_VariableSetMap.end())
        return false;

    VariableMap& varMap = itrSet->second;
    VariableMap::iterator itrVar = varMap.find(varName);
    if (itrVar == varMap.end())
    {
        if (!createIfNotPresent)
            return false;
        varMap.emplace(varName, varName);
        itrVar = varMap.find(varName);
    }

    UserVariable& var = itrVar->second;
    if (!var.HasMember(memberName) && !createIfNotPresent)
        return false;

    var.SetValue(memberName, value);
    return true;
}
