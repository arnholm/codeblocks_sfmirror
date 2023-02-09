

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
const long EditProjectGlobsDlg::ID_TEXTPATH = wxNewId();
const long EditProjectGlobsDlg::ID_BTN_BROWSE = wxNewId();
const long EditProjectGlobsDlg::ID_BTN_OTHER = wxNewId();
const long EditProjectGlobsDlg::ID_CHECK_RECURSIVE = wxNewId();
const long EditProjectGlobsDlg::ID_TXT_WILDCART = wxNewId();
//*)

BEGIN_EVENT_TABLE(EditProjectGlobsDlg,wxDialog)
	//(*EventTable(EditProjectGlobsDlg)
	//*)
END_EVENT_TABLE()

EditProjectGlobsDlg::EditProjectGlobsDlg(const ProjectGlob& glob, wxWindow* parent,wxWindowID id,const wxPoint& pos,const wxSize& size) : m_GlobObj(glob)
{
	//(*Initialize(EditProjectGlobsDlg)
	wxBoxSizer* BoxSizer1;
	wxBoxSizer* BoxSizer2;
	wxBoxSizer* BoxSizer3;
	wxStaticText* StaticText1;
	wxStaticText* StaticText2;

	Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER, _T("wxID_ANY"));
	BoxSizer3 = new wxBoxSizer(wxVERTICAL);
	BoxSizer1 = new wxBoxSizer(wxHORIZONTAL);
	StaticText1 = new wxStaticText(this, wxID_ANY, _("Path:"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	BoxSizer1->Add(StaticText1, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	txtPath = new wxTextCtrl(this, ID_TEXTPATH, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TEXTPATH"));
	BoxSizer1->Add(txtPath, 1, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnBrowse = new wxBitmapButton(this, ID_BTN_BROWSE, wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_FILE_OPEN")),wxART_BUTTON), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, _T("ID_BTN_BROWSE"));
	BoxSizer1->Add(btnBrowse, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnOther = new wxBitmapButton(this, ID_BTN_OTHER, wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(_T("wxART_EXECUTABLE_FILE")),wxART_BUTTON), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, _T("ID_BTN_OTHER"));
	BoxSizer1->Add(btnOther, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	BoxSizer3->Add(BoxSizer1, 1, wxLEFT|wxRIGHT|wxEXPAND, 5);
	BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
	chkRecursive = new wxCheckBox(this, ID_CHECK_RECURSIVE, _("Recursive"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECK_RECURSIVE"));
	chkRecursive->SetValue(false);
	BoxSizer2->Add(chkRecursive, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 10);
	StaticText2 = new wxStaticText(this, wxID_ANY, _("Wildcard: "), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	BoxSizer2->Add(StaticText2, 0, wxALIGN_CENTER_VERTICAL, 5);
	txtWildcart = new wxTextCtrl(this, ID_TXT_WILDCART, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TXT_WILDCART"));
	BoxSizer2->Add(txtWildcart, 1, wxALIGN_CENTER_VERTICAL, 5);
	BoxSizer3->Add(BoxSizer2, 1, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	StdDialogButtonSizer1 = new wxStdDialogButtonSizer();
	StdDialogButtonSizer1->AddButton(new wxButton(this, wxID_OK, wxEmptyString));
	StdDialogButtonSizer1->AddButton(new wxButton(this, wxID_CANCEL, wxEmptyString));
	StdDialogButtonSizer1->Realize();
	BoxSizer3->Add(StdDialogButtonSizer1, 1, wxALL|wxEXPAND, 5);
	SetSizer(BoxSizer3);
	BoxSizer3->SetSizeHints(this);

	Connect(ID_TEXTPATH,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&EditProjectGlobsDlg::OntxtPathText);
	Connect(ID_BTN_BROWSE,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&EditProjectGlobsDlg::OnBrowseClick);
	Connect(ID_BTN_OTHER,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&EditProjectGlobsDlg::OnOtherClick);
	//*)

	SetMaxSize(wxSize(-1, GetMinHeight()));
    Fit();

    StdDialogButtonSizer1->GetAffirmativeButton()->Disable();

    txtWildcart->SetToolTip(_("Use ; as separator"));

    if (m_GlobObj.IsValid())
    {
        txtPath->SetValue(m_GlobObj.GetPath());
        txtWildcart->SetValue(m_GlobObj.GetWildCard());
        chkRecursive->SetValue(m_GlobObj.GetRecursive());
    }

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
