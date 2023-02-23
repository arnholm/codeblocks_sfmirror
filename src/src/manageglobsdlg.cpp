#include "sdk.h"
#ifndef CB_PRECOMP

    #include "cbproject.h"
    #include "configmanager.h"
    #include "editorcolourset.h"
    #include "editormanager.h"
    #include "globals.h"
    #include "manager.h"
    #include "projectmanager.h"
    #include "projectloader.h"
#endif
#include "manageglobsdlg.h"
#include "editprojectglobsdlg.h"

//(*InternalHeaders(ManageGlobsDlg)
#include <wx/intl.h>
#include <wx/string.h>
//*)

//(*IdInit(ManageGlobsDlg)
const long ManageGlobsDlg::ID_LISTCTRL = wxNewId();
const long ManageGlobsDlg::ID_BUTTON_ADD = wxNewId();
const long ManageGlobsDlg::ID_BUTTON_DELETE = wxNewId();
const long ManageGlobsDlg::ID_BUTTON_EDIT = wxNewId();
//*)

BEGIN_EVENT_TABLE(ManageGlobsDlg,wxDialog)
	//(*EventTable(ManageGlobsDlg)
	//*)
END_EVENT_TABLE()

ManageGlobsDlg::ManageGlobsDlg(cbProject* prj, wxWindow* parent,wxWindowID id,const wxPoint& pos,const wxSize& size)
{
	//(*Initialize(ManageGlobsDlg)
	wxButton* btnCancel;
	wxGridBagSizer* GridBagSizer1;
	wxGridBagSizer* GridBagSizer2;
	wxStaticLine* StaticLine1;

	Create(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER, _T("id"));
	SetClientSize(wxDefaultSize);
	Move(wxDefaultPosition);
	GridBagSizer1 = new wxGridBagSizer(0, 0);
	m_ListGlobs = new wxListCtrl(this, ID_LISTCTRL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT, wxDefaultValidator, _T("ID_LISTCTRL"));
	GridBagSizer1->Add(m_ListGlobs, wxGBPosition(0, 0), wxDefaultSpan, wxALL|wxEXPAND, 5);
	GridBagSizer2 = new wxGridBagSizer(0, 0);
	btnAdd = new wxButton(this, ID_BUTTON_ADD, _("Add"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON_ADD"));
	GridBagSizer2->Add(btnAdd, wxGBPosition(0, 0), wxDefaultSpan, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnDelete = new wxButton(this, ID_BUTTON_DELETE, _("Delete"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON_DELETE"));
	GridBagSizer2->Add(btnDelete, wxGBPosition(1, 0), wxDefaultSpan, wxLEFT|wxRIGHT|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnEdit = new wxButton(this, ID_BUTTON_EDIT, _("Edit"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON_EDIT"));
	GridBagSizer2->Add(btnEdit, wxGBPosition(2, 0), wxDefaultSpan, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	StaticLine1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(10,-1), wxLI_HORIZONTAL, _T("wxID_ANY"));
	GridBagSizer2->Add(StaticLine1, wxGBPosition(3, 0), wxDefaultSpan, wxALL|wxEXPAND, 5);
	btnCancel = new wxButton(this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("wxID_CANCEL"));
	GridBagSizer2->Add(btnCancel, wxGBPosition(4, 0), wxDefaultSpan, wxTOP|wxLEFT|wxRIGHT|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	btnOk = new wxButton(this, wxID_OK, _("Ok"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("wxID_OK"));
	btnOk->SetDefault();
	GridBagSizer2->Add(btnOk, wxGBPosition(5, 0), wxDefaultSpan, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	GridBagSizer1->Add(GridBagSizer2, wxGBPosition(0, 1), wxDefaultSpan, wxALL|wxALIGN_TOP|wxALIGN_CENTER_HORIZONTAL, 5);
	GridBagSizer1->AddGrowableCol(0);
	GridBagSizer1->AddGrowableRow(0);
	SetSizer(GridBagSizer1);
	GridBagSizer1->SetSizeHints(this);

	Connect(ID_LISTCTRL,wxEVT_COMMAND_LIST_ITEM_SELECTED,(wxObjectEventFunction)&ManageGlobsDlg::OnlstGlobsListItemSelect);
	Connect(ID_LISTCTRL,wxEVT_COMMAND_LIST_ITEM_DESELECTED,(wxObjectEventFunction)&ManageGlobsDlg::OnlstGlobsListItemDeselect);
	Connect(ID_LISTCTRL,wxEVT_COMMAND_LIST_ITEM_ACTIVATED,(wxObjectEventFunction)&ManageGlobsDlg::OnlstGlobsListItemActivated);
	Connect(ID_BUTTON_ADD,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ManageGlobsDlg::OnAddClick);
	Connect(ID_BUTTON_DELETE,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ManageGlobsDlg::OnDeleteClick);
	Connect(ID_BUTTON_EDIT,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ManageGlobsDlg::OnEditClick);
	Connect(wxID_OK,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ManageGlobsDlg::OnOkClick);
	//*)

    if (prj != nullptr)
    {
        m_Prj = prj;
        for (const ProjectGlob& g : m_Prj->GetGlobs())
            m_GlobList.push_back(TemporaryGlobHolder(g));

        wxString title = m_Prj->GetTitle();
        this->SetTitle(wxString::Format(_("Edit automatic source paths of %s"), title.wx_str()));
    }

    m_OldGlobList = m_GlobList;

    btnEdit->Disable();
    btnDelete->Disable();

    m_ListGlobs->InsertColumn(0, _("Path"));
    m_ListGlobs->InsertColumn(1, _("Recursive"), wxLIST_FORMAT_CENTRE);
    m_ListGlobs->InsertColumn(2, _("Wildcard"));
    m_ListGlobs->InsertColumn(3, _("Targets"));

    PopulateList();

    Fit();
}

ManageGlobsDlg::~ManageGlobsDlg()
{
	//(*Destroy(ManageGlobsDlg)
	//*)
}

void ManageGlobsDlg::PopulateList()
{
    m_ListGlobs->DeleteAllItems();
    int i = 0;
    for (const TemporaryGlobHolder& tmpGlobObj : m_GlobList)
    {
        const ProjectGlob& globObj = tmpGlobObj.glob;
        m_ListGlobs->InsertItem(i, globObj.GetPath());
        wxString rec = wxString::Format(wxT("%i"), globObj.GetRecursive());
        m_ListGlobs->SetItem(i, 1, rec);
        m_ListGlobs->SetItem(i, 2, globObj.GetWildCard());
        m_ListGlobs->SetItem(i, 3, wxJoin(globObj.GetTargets(), ','));
        ++i;
    }
}

void ManageGlobsDlg::OnAddClick(wxCommandEvent& event)
{
    ProjectGlob tmpGlob = ProjectGlob();
    EditProjectGlobsDlg dlg(m_Prj, tmpGlob, nullptr);
    if (dlg.ShowModal() == wxID_OK)
    {
        tmpGlob = dlg.WriteGlob();
        if (std::find_if(m_GlobList.begin(), m_GlobList.end(), [&tmpGlob](const TemporaryGlobHolder& a){ return tmpGlob.GetId() == a.glob.GetId(); }) == m_GlobList.end())
        {
            m_GlobList.push_back(TemporaryGlobHolder(tmpGlob));
            PopulateList();
        }
        else
            cbMessageBox("Path already in list", "Info");
    }
}

void ManageGlobsDlg::OnDeleteClick(wxCommandEvent& event)
{
    int item = -1;
    std::vector<TemporaryGlobHolder> itemsToDelete;
    for ( ;; )
    {
        item = m_ListGlobs->GetNextItem(item,
                                         wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED);
        if ( item == -1 )
            break;
        itemsToDelete.push_back(m_GlobList[item]);
    }

    for (std::vector<TemporaryGlobHolder>::iterator itr = m_GlobList.begin(); itr != m_GlobList.end(); )
    {
        if (std::find(itemsToDelete.begin(),itemsToDelete.end(), *itr ) != itemsToDelete.end())
            itr = m_GlobList.erase(itr);
        else
            ++itr;
    }

    PopulateList();
}

void ManageGlobsDlg::OnEditClick(wxCommandEvent& event)
{
    EditSelectedItem();
}

void  ManageGlobsDlg::EditSelectedItem()
{
    int item = m_ListGlobs->GetNextItem(-1,
                                         wxLIST_NEXT_ALL,
                                         wxLIST_STATE_SELECTED);
    if ( item == -1 )
        return;

    const GlobId oldId = m_GlobList[item].glob.GetId();
    const wxArrayString oldTargets = m_GlobList[item].glob.GetTargets();
    EditProjectGlobsDlg dlg(m_Prj, m_GlobList[item].glob, this);
    if (dlg.ShowModal() == wxID_OK)
    {
        m_GlobList[item].glob            =  dlg.WriteGlob();
        // We mark targets only modified if the have changed AND the path is the same as the old.
        // If the path changed we have to remove/add files anyway, so it is like a new glob and we override the old
        m_GlobList[item].targetsModified =  oldTargets !=  m_GlobList[item].glob.GetTargets() && oldId == m_GlobList[item].glob.GetId();
        PopulateList();
    }
}

void ManageGlobsDlg::ReassignTargets(const ProjectGlob& glob)
{
    for (ProjectFile* file : m_Prj->GetFilesList())
    {
        const wxArrayString newTargets = glob.GetTargets();
        for (const wxString& target : newTargets)
        {
            file->AddBuildTarget(target);
        }
        for (const wxString& target : wxArrayString(file->GetBuildTargets()))
        {
            if (newTargets.Index(target) == wxNOT_FOUND)
                file->RemoveBuildTarget(target);
        }
    }
}

void ManageGlobsDlg::OnOkClick(wxCommandEvent& event)
{
    if (m_Prj != nullptr && GlobsChanged())
    {
        m_Prj->SetModified(true);
        std::vector<ProjectGlob> globList;
        for (const TemporaryGlobHolder& tmpGlobObj : m_GlobList)
        {
            globList.push_back(tmpGlobObj.glob);
            if (tmpGlobObj.targetsModified)
            {
                int ret = cbMessageBox(
                             wxString::Format(_("Targets for path %s have changed.\nDo you want to change all existing source files to match the new target?\n\nYes: All existing project files for this path will get assigned to the new target\nNo: Only newly added files will be added to the new target. Old files will not be changed."),
                                              tmpGlobObj.glob.GetPath()),
                             _("Reassign targets for current files?"),
                             wxYES_NO | wxICON_QUESTION);
                if (ret == wxID_YES)
                    ReassignTargets(tmpGlobObj.glob);
            }
        }


        m_Prj->SetGlobs(globList);
        m_Prj->Touch();

        Manager::Get()->GetProjectManager()->GetUI().ReloadFileSystemWatcher(m_Prj);
        Manager::Get()->GetProjectManager()->GetUI().RebuildTree();
    }
    this->EndModal(wxID_OK);
}

bool ManageGlobsDlg::GlobsChanged()
{
    return m_OldGlobList != m_GlobList;
}

void ManageGlobsDlg::OnlstGlobsListItemSelect(wxListEvent& event)
{
    btnEdit->Enable();
    btnDelete->Enable();
}

void ManageGlobsDlg::OnlstGlobsListItemDeselect(wxListEvent& event)
{
    btnEdit->Disable();
    btnDelete->Disable();
}

void ManageGlobsDlg::OnlstGlobsListItemActivated(wxListEvent& event)
{
    EditSelectedItem();
}
