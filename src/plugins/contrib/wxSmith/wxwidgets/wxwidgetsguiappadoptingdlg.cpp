/*
* This file is part of wxSmith plugin for Code::Blocks Studio
* Copyright (C) 2006-2007  Bartlomiej Swiecki
*
* wxSmith is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* wxSmith is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with wxSmith. If not, see <http://www.gnu.org/licenses/>.
*
* $Revision$
* $Id$
* $HeadURL$
*/

#include <prep.h>
#include "wxwidgetsguiappadoptingdlg.h"
#include "wxwidgetsgui.h"

#include <wx/filedlg.h>
#include <projectmanager.h>

//(*InternalHeaders(wxWidgetsGUIAppAdoptingDlg)
#include <wx/intl.h>
#include <wx/string.h>
//*)

//(*IdInit(wxWidgetsGUIAppAdoptingDlg)
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_LISTBOX1 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_GAUGE1 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_STATICTEXT1 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_STATICTEXT2 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_BUTTON5 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_BUTTON2 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_BUTTON3 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_STATICLINE2 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_BUTTON4 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_STATICLINE1 = wxNewId();
const wxWindowID wxWidgetsGUIAppAdoptingDlg::ID_BUTTON6 = wxNewId();
//*)

BEGIN_EVENT_TABLE(wxWidgetsGUIAppAdoptingDlg,wxScrollingDialog)
    //(*EventTable(wxWidgetsGUIAppAdoptingDlg)
    //*)
    EVT_CLOSE(wxWidgetsGUIAppAdoptingDlg::OnClose)
    EVT_TIMER(1,wxWidgetsGUIAppAdoptingDlg::OnTimer)
END_EVENT_TABLE()

wxWidgetsGUIAppAdoptingDlg::wxWidgetsGUIAppAdoptingDlg(wxWindow* parent,wxWidgetsGUI* GUI,wxWindowID id):
    m_Project(GUI->GetProject()->GetCBProject()),
    m_GUI(GUI),
    m_Timer(this,1),
    m_Run(true)
{
    //(*Initialize(wxWidgetsGUIAppAdoptingDlg)
    wxBoxSizer* BoxSizer1;
    wxBoxSizer* BoxSizer3;
    wxStaticBoxSizer* StaticBoxSizer1;

    Create(parent, id, _("Integrating application class with wxSmith"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER, _T("id"));
    BoxSizer1 = new wxBoxSizer(wxHORIZONTAL);
    StaticBoxSizer1 = new wxStaticBoxSizer(wxVERTICAL, this, _("Files with application class:"));
    FoundFiles = new wxListBox(this, ID_LISTBOX1, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_LISTBOX1"));
    StaticBoxSizer1->Add(FoundFiles, 1, wxEXPAND, 4);
    Progress = new wxGauge(this, ID_GAUGE1, 100, wxDefaultPosition, wxDefaultSize, wxGA_SMOOTH, wxDefaultValidator, _T("ID_GAUGE1"));
    StaticBoxSizer1->Add(Progress, 0, wxTOP|wxEXPAND, 4);
    BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    ScanningTxt = new wxStaticText(this, ID_STATICTEXT1, _("Scanning:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
    BoxSizer2->Add(ScanningTxt, 0, wxEXPAND, 4);
    ScanningFile = new wxStaticText(this, ID_STATICTEXT2, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
    BoxSizer2->Add(ScanningFile, 1, wxLEFT|wxEXPAND, 5);
    StaticBoxSizer1->Add(BoxSizer2, 0, wxTOP|wxEXPAND, 4);
    BoxSizer1->Add(StaticBoxSizer1, 1, wxALIGN_CENTER_VERTICAL, 5);
    BoxSizer3 = new wxBoxSizer(wxVERTICAL);
    UseFileBtn = new wxButton(this, ID_BUTTON5, _("Use selected file"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON5"));
    UseFileBtn->Disable();
    BoxSizer3->Add(UseFileBtn, 0, wxEXPAND, 5);
    SelectBtn = new wxButton(this, ID_BUTTON2, _("Select file manually"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON2"));
    BoxSizer3->Add(SelectBtn, 0, wxTOP|wxEXPAND, 5);
    CreateBtn = new wxButton(this, ID_BUTTON3, _("Create new file"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON3"));
    BoxSizer3->Add(CreateBtn, 0, wxTOP|wxEXPAND, 5);
    StaticLine2 = new wxStaticLine(this, ID_STATICLINE2, wxDefaultPosition, wxSize(50,-1), wxLI_HORIZONTAL, _T("ID_STATICLINE2"));
    BoxSizer3->Add(StaticLine2, 0, wxTOP|wxEXPAND, 5);
    Button4 = new wxButton(this, ID_BUTTON4, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON4"));
    Button4->SetDefault();
    BoxSizer3->Add(Button4, 0, wxTOP|wxEXPAND, 5);
    StaticLine1 = new wxStaticLine(this, ID_STATICLINE1, wxDefaultPosition, wxSize(50,-1), wxLI_HORIZONTAL, _T("ID_STATICLINE1"));
    BoxSizer3->Add(StaticLine1, 0, wxTOP|wxEXPAND, 5);
    Button6 = new wxButton(this, ID_BUTTON6, _("What\'s this for \?"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON6"));
    BoxSizer3->Add(Button6, 0, wxTOP|wxEXPAND, 5);
    BoxSizer1->Add(BoxSizer3, 0, wxTOP|wxLEFT|wxALIGN_TOP, 5);
    SetSizer(BoxSizer1);
    BoxSizer1->SetSizeHints(this);
    Center();

    Connect(ID_BUTTON5,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(wxWidgetsGUIAppAdoptingDlg::OnUseFileBtnClick));
    Connect(ID_BUTTON2,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(wxWidgetsGUIAppAdoptingDlg::OnSelectBtnClick));
    Connect(ID_BUTTON3,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(wxWidgetsGUIAppAdoptingDlg::OnCreateBtnClick));
    Connect(ID_BUTTON4,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(wxWidgetsGUIAppAdoptingDlg::OnButton4Click));
    Connect(ID_BUTTON6,wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(wxWidgetsGUIAppAdoptingDlg::OnButton6Click));
    //*)

    m_Timer.Start(100,true);
}

wxWidgetsGUIAppAdoptingDlg::~wxWidgetsGUIAppAdoptingDlg()
{
    //(*Destroy(wxWidgetsGUIAppAdoptingDlg)
    //*)
}

void wxWidgetsGUIAppAdoptingDlg::OnButton6Click(cb_unused wxCommandEvent& event)
{
    wxMessageBox(
        _("In order to fully integrate wxSmith with wxWidgets application\n"
          "it's required to do some modifications to source files with\n"
          "application class. This will allow wxSmith to automatically\n"
          "generate application initializing code.\n"
          "By default wxSmith scans for sources automatically\n"
          "and is able to properly modify required sources.\n"
          "You can also specify source file with application\n"
          "class manually or request to create new one.\n"),
        _("Integrating application class with wxSmith"),
        wxICON_ASTERISK|wxOK);
}

void wxWidgetsGUIAppAdoptingDlg::OnButton4Click(cb_unused wxCommandEvent& event)
{
    EndModal(wxID_CANCEL);
}

void wxWidgetsGUIAppAdoptingDlg::OnClose(wxCloseEvent& event)
{
    m_Run = false;
    event.Skip();
}

void wxWidgetsGUIAppAdoptingDlg::OnTimer(cb_unused wxTimerEvent& event)
{
    Run();
}

void wxWidgetsGUIAppAdoptingDlg::OnUseFileBtnClick(cb_unused wxCommandEvent& event)
{
    int Index = FoundFiles->GetSelection();
    if ( Index < 0 ) return;
    AddSmith(FoundFiles->GetStringSelection());
}

void wxWidgetsGUIAppAdoptingDlg::Run()
{
    Progress->SetRange(m_Project->GetFilesCount());
    int i = 0;
    for (FilesList::iterator it = m_Project->GetFilesList().begin(); it != m_Project->GetFilesList().end(); ++it)
    {
        Manager::Yield();
        ProjectFile* File = *it;
        ScanningFile->SetLabel(File->relativeFilename);
        Progress->SetValue(++i);
        if ( ScanFile(File) )
        {
            FoundFiles->Append(File->relativeFilename);
            UseFileBtn->Enable();
        }
    }
    ScanningFile->SetLabel(_("*** Done ***"));
}

void wxWidgetsGUIAppAdoptingDlg::AddSmith(wxString RelativeFileName)
{
    wxsCodingLang Lang = wxsCodeMarks::IdFromExt(wxFileName(RelativeFileName).GetExt());
    if ( Lang == wxsUnknownLanguage ) return;

    if ( m_GUI->AddSmithToApp(RelativeFileName,Lang) )
    {
        wxMessageBox(_("Application class has been adopted. Please check if it\n"
                       "works fine (some application initializing code could\n"
                       "be skipped)."));
        m_Run = false;
        EndModal(wxID_OK);
    }
}

bool wxWidgetsGUIAppAdoptingDlg::ScanFile(ProjectFile* File)
{
    return m_GUI->ScanForApp(File);
}

void wxWidgetsGUIAppAdoptingDlg::OnSelectBtnClick(cb_unused wxCommandEvent& event)
{
    wxString FileName = ::wxFileSelector(
        _("Select file with implementation of application class"),
        _T(""),
        _T(""),
        _T(""),
        _("C++ sources (*.cpp)|*.cpp|All files|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST | compatibility::wxHideReadonly);
    if ( FileName.empty() )
    {
        return;
    }
    ProjectFile* PF = m_Project->GetFileByFilename(FileName,false,false);
    if ( !PF )
    {
        wxMessageBox(_("This file is not included in project.\n"
                       "Please add this file to project first."),
                     _("File outside project"));
        return;
    }

    if ( !ScanFile(PF) )
    {
        wxMessageBox(_("wxSmith is not able to adopt this file\n"
                       "(Please check if it contains implementation\n"
                       "of application class)"));
        return;
    }

    AddSmith(PF->relativeFilename);
}

void wxWidgetsGUIAppAdoptingDlg::OnCreateBtnClick(cb_unused wxCommandEvent& event)
{
    wxString FileName = ::wxFileSelector(
        _("Please select cpp file where application class should be created"),
        m_GUI->GetProjectPath(),
        _T("myapp.cpp"),
        _T("cpp"),
        _T("C++ source files|*.cpp|All files|*"),
        wxFD_SAVE|wxFD_OVERWRITE_PROMPT);

    if ( FileName.empty() ) return;

    if ( m_Project->GetFileByFilename(FileName,false) == nullptr )
    {
        // Adding new file to project
        wxArrayInt targets;
        Manager::Get()->GetProjectManager()->AddFileToProject(FileName,m_Project, targets);
        Manager::Get()->GetProjectManager()->GetUI().RebuildTree();
    }

    if ( m_GUI->CreateNewApp(FileName) )
    {
        wxMessageBox(_("New application class created"));
        m_Run = false;
        EndModal(wxID_OK);
    }
    else
    {
        wxMessageBox(_("Error occured while creating new files"));
    }
}
