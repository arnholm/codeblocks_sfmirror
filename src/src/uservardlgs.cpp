#include "uservardlgs.h"
#include "annoyingdialog.h"
#include <wx/textfile.h>

void UserVarManagerGUI::DisplayInfoWindow(const wxString &title, const wxString &msg)
{
    InfoWindow::Display(title, msg, 8000, 1000);
}

void UserVarManagerGUI::OpenEditWindow(const std::set<wxString> &var)
{
    UsrGlblMgrEditDialog d;
    for (const wxString& item : var)
    {
        d.AddVar(item);
    }
    PlaceWindow(&d);
    d.ShowModal();
}

wxString UserVarManagerGUI::GetVariable(wxWindow* parent, const wxString &old)
{
    GetUserVariableDialog dlg(parent, old);
    PlaceWindow(&dlg);
    dlg.ShowModal();
    return dlg.GetVariable();
}


BEGIN_EVENT_TABLE(GetUserVariableDialog, wxScrollingDialog)
    EVT_BUTTON(XRCID("ID_CONFIG"), GetUserVariableDialog::OnConfig)
    EVT_BUTTON(XRCID("wxID_OK"), GetUserVariableDialog::OnOK)
    EVT_BUTTON(XRCID("wxID_CANCEL"), GetUserVariableDialog::OnCancel)
    EVT_TREE_ITEM_ACTIVATED(XRCID("ID_GET_USER_VAR_TREE"), GetUserVariableDialog::OnActivated)
END_EVENT_TABLE()

GetUserVariableDialog::GetUserVariableDialog(wxWindow *parent, const wxString &old) :
    m_old(old)
{
    wxXmlResource::Get()->LoadObject(this, parent, wxT("dlgGetGlobalUsrVar"), wxT("wxScrollingDialog"));
    m_treectrl = XRCCTRL(*this, "ID_GET_USER_VAR_TREE", wxTreeCtrl);

    if (m_treectrl == nullptr)
        Manager::Get()->GetLogManager()->LogError(_("Failed to load dlgGetGlobalUsrVar"));

    Load();

    // Try to open the old variable
    if (m_old != wxEmptyString && m_old.StartsWith(wxT("$(#")))
    {
        // Remove "$(#"
        wxString tmp = m_old.AfterFirst('#');
        // Remove the last ")"
        tmp = tmp.BeforeFirst(')');
        // In tmp is now "var.subVar". subVar is optional
        wxString var[2];
        var[0] = tmp.Before('.');
        var[1] = tmp.After('.');
        wxTreeItemId root = m_treectrl->GetRootItem();
        wxTreeItemIdValue cookie;
        wxTreeItemId child = m_treectrl->GetFirstChild(root, cookie);
        unsigned int i = 0;
        while (child.IsOk())
        {
            if (m_treectrl->GetItemText(child) == var[i])
            {
                m_treectrl->EnsureVisible(child);
                m_treectrl->SelectItem(child);
                i++;
                if (i >= 2 || var[i] == wxEmptyString)
                    break;

                root = child;
                child = m_treectrl->GetFirstChild(root, cookie);
            }
            else
                child = m_treectrl->GetNextChild(root, cookie);
        }
    }

    Fit();
    SetMinSize(GetSize());
}

void GetUserVariableDialog::Load()
{
    if (m_treectrl == nullptr)
        return;

    m_treectrl->DeleteAllItems();

    UserVariableManager* userVarMan = Manager::Get()->GetUserVariableManager();
    wxString activeSetName = userVarMan->GetActiveSetName();
    std::vector<wxString> vars = userVarMan->GetVariableNames(activeSetName);
    std::sort(vars.begin(), vars.end());

    wxTreeItemId root = m_treectrl->AddRoot(activeSetName);
    for (std::vector<wxString>::const_iterator var_itr = vars.cbegin(); var_itr != vars.cend() ; ++var_itr)
    {
        wxTreeItemId varId = m_treectrl->AppendItem(root, *var_itr);
        const std::vector<wxString> members = userVarMan->GetMemberNames(activeSetName, *var_itr);
        for (const wxString& memberName : members)
            m_treectrl->AppendItem(varId, memberName);
    }
    m_treectrl->Expand(root);
}

void GetUserVariableDialog::OnOK(cb_unused wxCommandEvent& evt)
{
    m_SelectedVar = GetSelectedVariable();
    EndModal(wxID_OK);
}

void GetUserVariableDialog::OnActivated(cb_unused wxTreeEvent& event)
{
    m_SelectedVar = GetSelectedVariable();
    EndModal(wxID_OK);
}

void GetUserVariableDialog::OnCancel(cb_unused wxCommandEvent& evt)
{
    m_SelectedVar = wxEmptyString;
    EndModal(wxID_CANCEL);
}

void GetUserVariableDialog::OnConfig(cb_unused wxCommandEvent& evt)
{
    Manager::Get()->GetUserVariableManager()->Configure();
    Load();
}

wxString GetUserVariableDialog::GetSelectedVariable()
{
    wxTreeItemId subVar = m_treectrl->GetSelection();
    wxTreeItemId var = m_treectrl->GetItemParent(subVar);

    if (subVar == m_treectrl->GetRootItem() || !subVar.IsOk())
        return wxEmptyString;

    wxString ret;
    ret << wxT("$(#");
    if (var == m_treectrl->GetRootItem()) // It is only a variable
        ret << m_treectrl->GetItemText(subVar) << wxT(")");
    else // var with subitem
        ret << m_treectrl->GetItemText(var) << wxT(".") <<  m_treectrl->GetItemText(subVar) << wxT(")");

    return ret;
}

BEGIN_EVENT_TABLE(UsrGlblMgrEditDialog, wxScrollingDialog)
    EVT_BUTTON(XRCID("cloneVar"), UsrGlblMgrEditDialog::CloneVar)
    EVT_BUTTON(XRCID("newVar"), UsrGlblMgrEditDialog::NewVar)
    EVT_BUTTON(XRCID("deleteVar"), UsrGlblMgrEditDialog::DeleteVar)
    EVT_BUTTON(XRCID("cloneSet"), UsrGlblMgrEditDialog::CloneSet)
    EVT_BUTTON(XRCID("newSet"), UsrGlblMgrEditDialog::NewSet)
    EVT_BUTTON(XRCID("deleteSet"), UsrGlblMgrEditDialog::DeleteSet)
    EVT_BUTTON(XRCID("exportSet"), UsrGlblMgrEditDialog::ExportSet)
    EVT_BUTTON(XRCID("importSet"), UsrGlblMgrEditDialog::ImportSet)
    EVT_BUTTON(XRCID("saveSet"), UsrGlblMgrEditDialog::SaveSet)
    EVT_BUTTON(XRCID("help"), UsrGlblMgrEditDialog::Help)
    EVT_BUTTON(wxID_OK, UsrGlblMgrEditDialog::OnOK)
    EVT_CLOSE(UsrGlblMgrEditDialog::CloseHandler)
    EVT_BUTTON(XRCID("fs1"), UsrGlblMgrEditDialog::OnFS)
    EVT_BUTTON(XRCID("fs2"), UsrGlblMgrEditDialog::OnFS)
    EVT_BUTTON(XRCID("fs3"), UsrGlblMgrEditDialog::OnFS)
    EVT_BUTTON(XRCID("fs4"), UsrGlblMgrEditDialog::OnFS)
    EVT_BUTTON(XRCID("fs5"), UsrGlblMgrEditDialog::OnFS)

    EVT_CHOICE(XRCID("selSet"), UsrGlblMgrEditDialog::SelectSet)
    EVT_LISTBOX(XRCID("selVar"), UsrGlblMgrEditDialog::SelectVar)
END_EVENT_TABLE()

UsrGlblMgrEditDialog::UsrGlblMgrEditDialog(const wxString& var) :
    m_CurrentSetName(Manager::Get()->GetUserVariableManager()->GetActiveSetName()),
    m_CurrentVar(var)
{
    wxXmlResource::Get()->LoadObject(this, Manager::Get()->GetAppWindow(), _T("dlgGlobalUservars"),_T("wxScrollingDialog"));
    m_SelSet    = XRCCTRL(*this, "selSet",   wxChoice);
    m_SelVar    = XRCCTRL(*this, "selVar",   wxListBox);
    m_DeleteSet = XRCCTRL(*this, "deleteSet",wxButton);

    m_Base    = XRCCTRL(*this, "base",    wxTextCtrl);
    m_Include = XRCCTRL(*this, "include", wxTextCtrl);
    m_Lib     = XRCCTRL(*this, "lib",     wxTextCtrl);
    m_Obj     = XRCCTRL(*this, "obj",     wxTextCtrl);
    m_Bin     = XRCCTRL(*this, "bin",     wxTextCtrl);

    wxSplitterWindow *splitter = XRCCTRL(*this, "splitter", wxSplitterWindow);
    if (splitter)
        splitter->SetSashGravity(0.7);

    wxString n;
    m_Name.resize(7);
    m_Value.resize(7);
    for (unsigned int i = 0; i < 7; ++i)
    {
        n.Printf(_T("n%d"), i);
        m_Name[i]  = (wxTextCtrl*) FindWindow(n);

        n.Printf(_T("v%d"), i);
        m_Value[i] = (wxTextCtrl*) FindWindow(n);
    }

    m_UserVarMgr = Manager::Get()->GetUserVariableManager();
    m_varMap = m_UserVarMgr->GetVariableMap();

    UpdateChoices();
    Load();
    PlaceWindow(this);
}

void UsrGlblMgrEditDialog::DoClose()
{
    EndModal(wxID_OK);
}


void UsrGlblMgrEditDialog::CloneVar(cb_unused wxCommandEvent& event)
{
    wxTextEntryDialog d(this, _("Please specify a name for the new clone:"), _("Clone Variable"));
    PlaceWindow(&d);
    if (d.ShowModal() == wxID_OK)
    {
        wxString clone = d.GetValue();

        if (clone.IsEmpty())
            return;

        Sanitise(clone);

        auto& curSet = m_varMap.at(m_CurrentSetName);
        if ( curSet.find(clone) != curSet.end())
        {
            wxString msg;
            msg.Printf(_("Cowardly refusing to overwrite existing variable \"%s\"."), clone.wx_str());
            InfoWindow::Display(_("Clone Set"), msg);
            return;
        }

        curSet.emplace(clone, UserVariable(clone, m_varMap.at(m_CurrentSetName).at(m_CurrentVar)));
        m_CurrentVar = clone;
        UpdateChoices();
        Load();
    }
}

void UsrGlblMgrEditDialog::CloneSet(cb_unused wxCommandEvent& event)
{
    wxTextEntryDialog d(this, _("Please specify a name for the new clone:"), _("Clone Set"));
    PlaceWindow(&d);
    if (d.ShowModal() == wxID_OK)
    {
        wxString clone = d.GetValue();
        Sanitise(clone);

        if (clone.IsEmpty())
            return;

        if (m_varMap.find(clone) != m_varMap.end() )
        {
            wxString msg;
            msg.Printf(_("Cowardly refusing overwrite existing set \"%s\"."), clone.wx_str());
            InfoWindow::Display(_("Clone Set"), msg);
            return;
        }

        m_varMap[clone] = m_varMap.at(m_CurrentSetName);
        m_CurrentSetName = clone;
        UpdateChoices();
        Load();
    }
}

void UsrGlblMgrEditDialog::DeleteVar(cb_unused wxCommandEvent& event)
{
    wxString msg;
    msg.Printf(_("Delete the global compiler variable \"%s\" from this set?"), m_CurrentVar.wx_str());
    AnnoyingDialog d(_("Delete Global Variable"), msg, wxART_QUESTION);
    PlaceWindow(&d);
    if (d.ShowModal() == AnnoyingDialog::rtYES)
    {
        m_varMap.at(m_CurrentSetName).erase(m_varMap.at(m_CurrentSetName).find(m_CurrentVar));
        m_CurrentVar = wxEmptyString;
        UpdateChoices();
        Load();
    }
}

void UsrGlblMgrEditDialog::DeleteSet(cb_unused wxCommandEvent& event)
{
    wxString msg;
    msg.Printf(_("Do you really want to delete the entire\n"
                 "global compiler variable set \"%s\"?\n\n"
                 "This cannot be undone."), m_CurrentSetName.wx_str());
    AnnoyingDialog d(_("Delete Global Variable Set"), msg, wxART_QUESTION);
    PlaceWindow(&d);
    if (d.ShowModal() == AnnoyingDialog::rtYES)
    {
        m_varMap.erase(m_varMap.find(m_CurrentSetName));
        m_CurrentSetName = wxEmptyString;
        m_CurrentVar = wxEmptyString;
        UpdateChoices();
        Load();
    }
}

/** \brief Export variables to text file
 *
 * Create text file with one line per variable with the format
 * `varName.memberName=value\r\n`
 *
 */
void UsrGlblMgrEditDialog::ExportSet(cb_unused wxCommandEvent& event)
{
    wxFileDialog fileDlg(this, _("Export set to file"), wxEmptyString, m_CurrentSetName+".set", _("Exported sets (*.set)|*.set|All files (*.*)|*.*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    PlaceWindow(&fileDlg);
    if (fileDlg.ShowModal() == wxID_OK)
    {
        wxArrayString result;
        result.Add('[' + m_CurrentSetName + ']');

        const VariableSetMap::iterator itrSet = m_varMap.find(m_CurrentSetName);
        if (itrSet == m_varMap.end())
        {
            InfoWindow::Display(_("Internal error during export variables"), _("Internal error"));
            return;
        }
        const VariableMap& varMap = itrSet->second;
        // sort members by using a ordinary map
        const std::map<wxString, UserVariable> orderedVarMap(varMap.begin(), varMap.end());
        for ( const auto& varEntry :  orderedVarMap )
        {
            const wxString& varName = varEntry.first;
            const UserVariable& var = varEntry.second;

            std::vector<wxString> members = var.GetMembers();
            std::sort(members.begin(), members.end());
            for (const wxString& member : members)
            {
                wxString memberVal;
                if (var.GetValue(member, memberVal))
                    result.Add(varName + '.' + member + '=' + memberVal);
            }
        }

        wxTextFile textFile(fileDlg.GetPath());
        bool success;
        if (textFile.Exists())
        {
            success = textFile.Open();
            if (success)
                textFile.Clear();
        }
        else
        {
            success = textFile.Create();
        }

        if (success)
        {
            for (const wxString& line : result)
                textFile.AddLine(line);

            textFile.Write();
            textFile.Close();
            InfoWindow::Display(_("Export Set"), _("File exported succesfully"));
        }
        else
        {
            InfoWindow::Display(_("Export Set"), _("Cannot open the file for writing"));
        }
     }
 }

/** \brief Ask user to import a file previously exported with UsrGlblMgrEditDialog::ExportSet
 *
 */
void UsrGlblMgrEditDialog::ImportSet(cb_unused wxCommandEvent& event)
{
    wxFileDialog fileDlg(this, _("Import set from file"), wxEmptyString, wxEmptyString, _("Exported sets (*.set)|*.set|All files (*.*)|*.*"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    PlaceWindow(&fileDlg);
    if (fileDlg.ShowModal() == wxID_OK)
    {
        wxTextFile textFile(fileDlg.GetPath());
        if (!textFile.Open())
        {
            InfoWindow::Display(_("Import Set"), _("Cannot open the file for reading"));
            return;
        }

        wxString setName = textFile.GetFirstLine();
        if (setName.empty())
        {
            textFile.Close();
            InfoWindow::Display(_("Import Set"), _("The file is empty"));
            return;
        }

        if (!setName.StartsWith("[") || !setName.EndsWith("]"))
        {
            textFile.Close();
            InfoWindow::Display(_("Import Set"), _("Set name unknown"));
            return;
        }

        setName = setName.Mid(1, setName.length()-2);
        Sanitise(setName);

        VariableSetMap::iterator itrSet = m_varMap.find(setName);
        while (setName.empty() || itrSet != m_varMap.end())
        {
            const wxString msg(setName.empty() ? _("Please input a valid set name")
                                               : _("This set already exists, please rename..."));
            wxTextEntryDialog entryDlg(this, msg, _("Edit set name"), setName);
            PlaceWindow(&entryDlg);
            if (entryDlg.ShowModal() != wxID_OK)
            {
                textFile.Close();
                return;
            }

            setName = entryDlg.GetValue();
            Sanitise(setName);
            itrSet = m_varMap.find(setName);
        }

        int errors = 0, vars = 0;
        VariableMap varMap;

        while (!textFile.Eof())
        {
            const wxString str(textFile.GetNextLine());
            if (!str.empty())
            {
                wxString varValue;
                const wxString variable(str.BeforeFirst('=', &varValue));
                if (variable.empty() || varValue.empty())
                {
                    errors++;
                    continue;
                }

                wxString varMember;
                const wxString varName(variable.BeforeFirst('.', &varMember));
                if (varName.empty() || varMember.empty())
                {
                    errors++;
                    continue;
                }

                VariableMap::iterator itr = varMap.find(varName);
                if (itr == varMap.end())
                    itr = varMap.emplace(varName, varName).first;
                itr->second.SetValue(varMember, varValue);
                vars++;
            }
        }
        m_varMap[setName] = varMap;

        textFile.Close();
        UpdateChoices();
        if (errors)
            cbMessageBox(wxString::Format(_("Created set '%s' with %i values (%i errors)"), setName, vars, errors), _("Import Set"), wxOK | wxICON_WARNING, this);
        else
            cbMessageBox(wxString::Format(_("Created set '%s' with %i values (no errors)"), setName, vars), _("Import Set"), wxOK | wxICON_INFORMATION, this);
    }
}

void UsrGlblMgrEditDialog::SaveSet(cb_unused wxCommandEvent& event)
{
    Save();
    m_UserVarMgr->UpdateFromVariableMap(m_varMap);
    m_UserVarMgr->Save();
    m_UserVarMgr->SetActiveSetName(m_CurrentSetName);
    EndModal(wxID_OK);
}

void UsrGlblMgrEditDialog::AddVar(const wxString& name)
{
    if (name.IsEmpty())
        return;
    Save();
    m_varMap.at(m_CurrentSetName).emplace(name, name);
    Save();
    m_CurrentVar = name;
    UpdateChoices();
    Load();
}

void UsrGlblMgrEditDialog::Sanitise(wxString& s)
{
    s.Trim().Trim(true);

    if (s.IsEmpty())
    {
        s = _T("[?empty?]");
        return;
    }

    for (unsigned int i = 0; i < s.length(); ++i)
#if wxCHECK_VERSION(3, 0, 0)
        s[i] = wxIsalnum(s.GetChar(i)) ? s.GetChar(i) : wxUniChar('_');
#else
        s[i] = wxIsalnum(s.GetChar(i)) ? s.GetChar(i) : _T('_');
#endif

    if (s.GetChar(0) == _T('_'))
        s.Prepend(_T("set"));

    if (s.GetChar(0) >= _T('0') && s.GetChar(0) <= _T('9'))
        s.Prepend(_T("set_"));
}

void UsrGlblMgrEditDialog::NewVar(cb_unused wxCommandEvent& event)
{
    Save();
    wxTextEntryDialog d(this, _("Please specify a name for the new variable:"), _("New Variable"));
    PlaceWindow(&d);
    if (d.ShowModal() == wxID_OK)
    {
        wxString name = d.GetValue();
        Sanitise(name);
        AddVar(name);
    }
}

void UsrGlblMgrEditDialog::NewSet(cb_unused wxCommandEvent& event)
{
    Save();
    wxTextEntryDialog d(this, _("Please specify a name for the new set:"), _("New Set"));
    PlaceWindow(&d);
    if (d.ShowModal() == wxID_OK)
    {
        wxString name = d.GetValue();
        Sanitise(name);

        if (name.IsEmpty())
            return;
        if (m_varMap.find(name) != m_varMap.end())
        {
            cbMessageBox(_("Set already exists. Please choose a other name."), _("Set already exists!"), wxICON_EXCLAMATION);
            return;
        }
        m_varMap[name] = VariableMap();
        m_CurrentSetName = name;
        UpdateChoices();
        Load();
    }
}

void UsrGlblMgrEditDialog::SelectVar(cb_unused wxCommandEvent& event)
{
    Save();
    m_CurrentVar = m_SelVar->GetStringSelection();
    Load();
}

void UsrGlblMgrEditDialog::SelectSet(cb_unused wxCommandEvent& event)
{
    Save();
    m_CurrentSetName = m_SelSet->GetStringSelection();
    UpdateChoices();
    Load();
}


void UsrGlblMgrEditDialog::Load()
{
    m_DeleteSet->Enable(!m_CurrentSetName.IsSameAs(UserVariableManagerConsts::defaultSetName));
    std::vector<wxString> knownMembers = UserVariableManagerConsts::cBuiltinMembers;

    // Clear all controlls first
    for (const wxString& buildInVar : knownMembers)
    {
        ((wxTextCtrl*) FindWindow(buildInVar))->SetValue(wxString());
    }
    for (unsigned int i = 0; i < 7; ++i)
    {
        m_Name[i]->SetValue(wxEmptyString);
        m_Value[i]->SetValue(wxEmptyString);
    }


    const auto itrSet = m_varMap.find(m_CurrentSetName);
    if (itrSet == m_varMap.end())
        return;

    const auto itrVar = m_varMap.at(m_CurrentSetName).find(m_CurrentVar);
    if (itrVar == m_varMap.at(m_CurrentSetName).end())
        return;

    const UserVariable& var = itrVar->second;
    std::vector<wxString> members;
    members = var.GetMembers();

    for (const wxString& buildInVar : knownMembers)
    {
        wxString value;
        var.GetValue(buildInVar, value, true);
        wxTextCtrl* ctrl = ((wxTextCtrl*) FindWindow(buildInVar));
        ctrl->SetValue(value);
        if (m_UserVarMgr->IsOverridden(var.GetName(), buildInVar))
            ctrl->SetEditable(false);

        const auto itr = std::find(members.begin(), members.end(), buildInVar);
        if (itr != members.end())
            members.erase(itr);
    }

    size_t i = 0;
    for (const wxString& member : members)
    {
        if (member.IsEmpty())
            continue;

        const wxString name = member.Lower();
        wxString value;
        var.GetValue(member, value, true);

        m_Name[i]->SetValue(name);
        m_Value[i]->SetValue(value);

        if (m_UserVarMgr->IsOverridden(var.GetName(), name))
        {
            m_Value[i]->SetEditable(false);
            m_Name[i]->SetEditable(false);
        }
        ++i;
        if (i >= m_Name.size())
            break;
    }
}

void UsrGlblMgrEditDialog::Save()
{
    std::vector<wxString> knownMembers = UserVariableManagerConsts::cBuiltinMembers;

    const auto itrSet = m_varMap.find(m_CurrentSetName);
    if (itrSet == m_varMap.end())
        return;
    const auto itrVar = m_varMap.at(m_CurrentSetName).find(m_CurrentVar);
    if (itrVar == m_varMap.at(m_CurrentSetName).end())
        return;

    UserVariable& var = itrVar->second;
    for (const wxString& buildInVar : knownMembers)
    {
        wxString value = ((wxTextCtrl*) FindWindow(buildInVar))->GetValue();
        var.SetValue(buildInVar, value);
    }

    size_t i = 0;
    for (i = 0; i < m_Name.size() ; ++i)
    {
        const wxString name  = m_Name[i]->GetValue();
        if (name.IsEmpty())
            continue;
        const wxString value = m_Value[i]->GetValue();
        var.SetValue(name, value);
    }
}

void UsrGlblMgrEditDialog::UpdateChoices()
{
    if (m_CurrentSetName.IsEmpty())
        m_CurrentSetName = UserVariableManagerConsts::defaultSetName;

    std::vector<wxString> sets;
    sets.reserve(m_varMap.size());
    for (const auto& setName : m_varMap)
        sets.emplace_back(setName.first);

    std::vector<wxString> vars;
    vars.reserve(m_varMap.at(m_CurrentSetName).size());
    for (const auto& var : m_varMap.at(m_CurrentSetName))
        vars.emplace_back(var.first);

    std::sort(sets.begin(), sets.end());
    std::sort(vars.begin(), vars.end());

    m_SelSet->Clear();
    for (const wxString& s : sets)
        m_SelSet->Append(s);
    m_SelVar->Clear();
    for (const wxString& s : vars)
        m_SelVar->Append(s);

    if (m_CurrentVar.IsEmpty() && m_SelVar->GetCount() > 0)
        m_CurrentVar = m_SelVar->GetString(0);

    m_SelSet->SetStringSelection(m_CurrentSetName);
    m_SelVar->SetStringSelection(m_CurrentVar);
}


void UsrGlblMgrEditDialog::OnFS(wxCommandEvent& event)
{
    wxTextCtrl* c = nullptr;
    int id = event.GetId();

    if      (id == XRCID("fs1"))
        c = m_Base;
    else if (id == XRCID("fs2"))
        c = m_Include;
    else if (id == XRCID("fs3"))
        c = m_Lib;
    else if (id == XRCID("fs4"))
        c = m_Obj;
    else if (id == XRCID("fs5"))
        c = m_Bin;
    else
        cbThrow(_T("Encountered invalid button ID"));

    wxString path = ChooseDirectory(this, _("Choose a location"), c->GetValue());
    if (!path.IsEmpty())
        c->SetValue(path);
}

void UsrGlblMgrEditDialog::Help(cb_unused wxCommandEvent& event)
{
    wxLaunchDefaultBrowser(_T("http://wiki.codeblocks.org/index.php?title=Global_compiler_variables"));
}
