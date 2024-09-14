

#include <sdk.h>

#ifndef CB_PRECOMP
    #include "macrosmanager.h"
    #include "uservarmanager.h"
#endif

#include "editprojectglobsdlg.h"

#include <wx/filefn.h>


//(*InternalHeaders(EditProjectGlobsDlg)
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/string.h>
//*)

//(*IdInit(EditProjectGlobsDlg)
const wxWindowID EditProjectGlobsDlg::ID_TEXTPATH = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_BTN_BROWSE = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_BTN_OTHER = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_TXT_WILDCART = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_CHECK_RECURSIVE = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_STATICTEXT1 = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_CHK_ALL_NONE = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_LST_TARGETS = wxNewId();
const wxWindowID EditProjectGlobsDlg::ID_CHECK_ADD_TO_PROJECT = wxNewId();
//*)

BEGIN_EVENT_TABLE(EditProjectGlobsDlg,wxDialog)
	//(*EventTable(EditProjectGlobsDlg)
	//*)
END_EVENT_TABLE()

EditProjectGlobsDlg::EditProjectGlobsDlg(const cbProject* prj, const ProjectGlob& glob, wxWindow* parent,wxWindowID id,const wxPoint& pos,const wxSize& size) : m_GlobObj(glob)
{
	//(*Initialize(EditProjectGlobsDlg)
	wxBoxSizer* BoxSizer1;
	wxBoxSizer* BoxSizer2;
	wxBoxSizer* BoxSizer3;
	wxBoxSizer* BoxSizer4;
	wxBoxSizer* BoxSizer6;
	wxBoxSizer* BoxSizer7;
	wxStaticBoxSizer* StaticBoxSizer1;
	wxStaticBoxSizer* StaticBoxSizer2;
	wxStaticText* StaticText1;
	wxStaticText* StaticText2;

	Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER, _T("wxID_ANY"));
	BoxSizer3 = new wxBoxSizer(wxVERTICAL);
	StaticBoxSizer1 = new wxStaticBoxSizer(wxVERTICAL, this, _("Path"));
	BoxSizer1 = new wxBoxSizer(wxHORIZONTAL);
	StaticText1 = new wxStaticText(this, wxID_ANY, _("Path:"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	BoxSizer1->Add(StaticText1, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	txtPath = new wxTextCtrl(this, ID_TEXTPATH, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TEXTPATH"));
	BoxSizer1->Add(txtPath, 1, wxLEFT|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnBrowse = new wxBitmapButton(this, ID_BTN_BROWSE, wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_FILE_OPEN")),wxART_BUTTON), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, _T("ID_BTN_BROWSE"));
	BoxSizer1->Add(btnBrowse, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnOther = new wxBitmapButton(this, ID_BTN_OTHER, wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_EXECUTABLE_FILE")),wxART_BUTTON), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, _T("ID_BTN_OTHER"));
	BoxSizer1->Add(btnOther, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	StaticBoxSizer1->Add(BoxSizer1, 1, wxLEFT|wxRIGHT|wxEXPAND, 5);
	BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
	StaticText2 = new wxStaticText(this, wxID_ANY, _("Wildcard: "), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	BoxSizer2->Add(StaticText2, 0, wxALIGN_CENTER_VERTICAL, 5);
	txtWildcart = new wxTextCtrl(this, ID_TXT_WILDCART, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TXT_WILDCART"));
	BoxSizer2->Add(txtWildcart, 1, wxALIGN_CENTER_VERTICAL, 5);
	chkRecursive = new wxCheckBox(this, ID_CHECK_RECURSIVE, _("Recursive"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECK_RECURSIVE"));
	chkRecursive->SetValue(false);
	BoxSizer2->Add(chkRecursive, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 10);
	StaticBoxSizer1->Add(BoxSizer2, 1, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	BoxSizer3->Add(StaticBoxSizer1, 0, wxALL|wxEXPAND, 5);
	StaticBoxSizer2 = new wxStaticBoxSizer(wxVERTICAL, this, _("Project settings"));
	BoxSizer4 = new wxBoxSizer(wxVERTICAL);
	BoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
	StaticText3 = new wxStaticText(this, ID_STATICTEXT1, _("Targets:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
	BoxSizer7->Add(StaticText3, 0, wxALIGN_LEFT, 5);
	BoxSizer7->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	chkAllNone = new wxCheckBox(this, ID_CHK_ALL_NONE, _("All/None"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE, wxDefaultValidator, _T("ID_CHK_ALL_NONE"));
	chkAllNone->SetValue(false);
	BoxSizer7->Add(chkAllNone, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	BoxSizer4->Add(BoxSizer7, 0, wxEXPAND, 5);
	lstTargets = new wxCheckListBox(this, ID_LST_TARGETS, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_LST_TARGETS"));
	BoxSizer4->Add(lstTargets, 1, wxEXPAND, 5);
	StaticBoxSizer2->Add(BoxSizer4, 1, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	BoxSizer6 = new wxBoxSizer(wxHORIZONTAL);
	chkAddToProject = new wxCheckBox(this, ID_CHECK_ADD_TO_PROJECT, _("Add files to project"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECK_ADD_TO_PROJECT"));
	chkAddToProject->SetValue(false);
	BoxSizer6->Add(chkAddToProject, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	StaticBoxSizer2->Add(BoxSizer6, 0, wxTOP|wxRIGHT|wxEXPAND, 5);
	BoxSizer3->Add(StaticBoxSizer2, 1, wxALL|wxEXPAND, 5);
	StdDialogButtonSizer1 = new wxStdDialogButtonSizer();
	StdDialogButtonSizer1->AddButton(new wxButton(this, wxID_OK, wxEmptyString));
	StdDialogButtonSizer1->AddButton(new wxButton(this, wxID_CANCEL, wxEmptyString));
	StdDialogButtonSizer1->Realize();
	BoxSizer3->Add(StdDialogButtonSizer1, 0, wxALL|wxEXPAND, 5);
	SetSizer(BoxSizer3);
	BoxSizer3->SetSizeHints(this);
	Center();

	Connect(ID_TEXTPATH,wxEVT_COMMAND_TEXT_UPDATED,wxCommandEventHandler(EditProjectGlobsDlg::OntxtPathText));
	Connect(ID_BTN_BROWSE,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(EditProjectGlobsDlg::OnBrowseClick));
	Connect(ID_BTN_OTHER,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(EditProjectGlobsDlg::OnOtherClick));
	Connect(ID_CHK_ALL_NONE,wxEVT_COMMAND_CHECKBOX_CLICKED,wxCommandEventHandler(EditProjectGlobsDlg::OnAllNoneClick));
	Connect(ID_LST_TARGETS,wxEVT_COMMAND_CHECKLISTBOX_TOGGLED,wxCommandEventHandler(EditProjectGlobsDlg::OnTargetsToggled));
	//*)

    StdDialogButtonSizer1->GetAffirmativeButton()->Disable();

    txtWildcart->SetToolTip(_("Use ; as separator"));

    m_Prj = prj;

    for (int i = 0; i < m_Prj->GetBuildTargetsCount(); ++i)
    {
        const wxString& targetName = prj->GetBuildTarget(i)->GetTitle();
        lstTargets->Append(targetName);
    }

    lstTargets->SetMinSize(wxSize(50,50));

    if (m_GlobObj.IsValid())
    {
        txtPath->SetValue(m_GlobObj.GetPath());
        txtWildcart->SetValue(m_GlobObj.GetWildCard());
        chkRecursive->SetValue(m_GlobObj.GetRecursive());
        chkAddToProject->SetValue(m_GlobObj.GetAddToProject());

        const wxArrayString& targetArray = lstTargets->GetStrings();
        for (const wxString& target : m_GlobObj.GetTargets() )
        {
            int idx = targetArray.Index(target);
            if (idx != wxNOT_FOUND)
                lstTargets->Check(idx);
        }
    }
    else
    {
        // If glob is not valid, so it is new
        // for new globs we add them by default to project
        chkAddToProject->SetValue(true);
    }

    UpdateTargetCheckBox();
    Fit();
}

EditProjectGlobsDlg::~EditProjectGlobsDlg()
{
	//(*Destroy(EditProjectGlobsDlg)
	//*)
}


void EditProjectGlobsDlg::OnBrowseClick(wxCommandEvent& event)
{
    wxFileName path;
    const wxString basePath = Manager::Get()->GetProjectManager()->GetActiveProject()->GetBasePath();

    wxString val = txtPath->GetValue();
    int idx = val.Find(DEFAULT_ARRAY_SEP);
    if (idx != -1)
        val.Remove(idx);


    // try to "decode" custom var
    wxString initialVal = val;
    // try to resolve the current path for macros
    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(val);
    // Make the resolved path absolute
    wxFileName fname(val);
    fname.MakeAbsolute(basePath);
    wxString fullPath = fname.GetFullPath();

    // Get the new path from user
    path = ChooseDirectory(this, _("Get project glob path"),
                           fullPath,
                           basePath, true, true);

    if (path.GetFullPath().empty())
        return;

    // if it was a custom var, see if we can re-insert it
    if (initialVal != val)
    {
        wxString tmp = path.GetFullPath();
        if (tmp.Replace(val, initialVal) != 0)
        {
            // replace the part we expressed with a custom variable again with
            // the custom variable name
            txtPath->SetValue(tmp);
            return;
        }
    }

    wxString result;
    result = path.GetFullPath();
    txtPath->SetValue(result);
}

ProjectGlob EditProjectGlobsDlg::WriteGlob()
{
    m_GlobObj.Set(txtPath->GetValue(), txtWildcart->GetValue(), chkRecursive->GetValue());
    m_GlobObj.SetAddToProject(chkAddToProject->GetValue());

    wxArrayInt checkedItems;
    lstTargets->GetCheckedItems(checkedItems);
    const wxArrayString& targetItems = lstTargets->GetStrings();
    wxArrayString targets;

    for (int item : checkedItems)
        targets.Add(targetItems.Item(item));

    m_GlobObj.SetTargets(targets);
    return m_GlobObj;
}

void EditProjectGlobsDlg::OnOtherClick(wxCommandEvent& event)
{
    UserVariableManager *userMgr = Manager::Get()->GetUserVariableManager();

    const wxString &userVar = userMgr->GetVariable(this, txtPath->GetValue());
    if ( !userVar.empty() )
    {
        txtPath->SetValue(userVar);
    }
}

void EditProjectGlobsDlg::OntxtPathText(wxCommandEvent& event)
{
    if (!event.GetString().IsEmpty())
        StdDialogButtonSizer1->GetAffirmativeButton()->Enable();
    else
        StdDialogButtonSizer1->GetAffirmativeButton()->Disable();
}

void EditProjectGlobsDlg::UpdateTargetCheckBox()
{
    wxArrayInt checkedItems;
    size_t checkedItemsCnt = lstTargets->GetCheckedItems(checkedItems);
    size_t totalCnt = lstTargets->GetStrings().size();
    if (checkedItemsCnt == totalCnt)
    {
        chkAllNone->Set3StateValue(wxCHK_CHECKED );
    }
    else if (checkedItemsCnt == 0)
    {
        chkAllNone->Set3StateValue(wxCHK_UNCHECKED );
    }
    else
    {
        chkAllNone->Set3StateValue(wxCHK_UNDETERMINED);
    }
}

void EditProjectGlobsDlg::OnTargetsToggled(wxCommandEvent& event)
{
    UpdateTargetCheckBox();
}

void EditProjectGlobsDlg::OnAllNoneClick(wxCommandEvent& event)
{
    wxCheckBoxState state = chkAllNone->Get3StateValue();
    if (state == wxCHK_CHECKED)
    {
        for (size_t i = 0; i < lstTargets->GetStrings().size(); i++)
            lstTargets->Check(i);
    }
    else if (state == wxCHK_UNCHECKED)
    {
        for (size_t i = 0; i < lstTargets->GetStrings().size(); i++)
            lstTargets->Check(i, false);
    }
}
