/***************************************************************
 * Name:      SearchInPanel
 *
 * Purpose:   This class is a panel that allows the user to
 *            define the search scope : open files, project,
 *            worspace or directory.
 *            It is used in the ThreadSearchView and the
 *            ThreadSearchConfPanel.
 *            It does nothing but forwarding events to the
 *            parent window.
 *
 * Author:    Jerome ANTOINE
 * Created:   2007-10-08
 * Copyright: Jerome ANTOINE
 * License:   GPL
 **************************************************************/

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/bitmap.h>
    #include <wx/bmpbuttn.h>
    #include <wx/checkbox.h>
    #include <wx/sizer.h>
    #include "configmanager.h"
#endif

#include "SearchInPanel.h"
#include "ThreadSearchCommon.h"
#include "ThreadSearchControlIds.h"

SearchInPanel::SearchInPanel(wxWindow* parent, int id, const wxPoint& pos, const wxSize& size, long WXUNUSED(style)):
    wxPanel(parent, id, pos, size, wxTAB_TRAVERSAL)
{
    // Create icons 3 to 7 in the message pane and all icons of the options dialog

#if wxCHECK_VERSION(3, 1, 6)
    const wxString prefix(ConfigManager::GetDataFolder()+"/ThreadSearch.zip#zip:images/svg/");
#else
    const wxString prefix(GetImagePrefix(false, parent));
#endif

    wxBitmapToggleButton dummy(this, wxID_ANY, wxBitmap(16, 16));
    const int height = dummy.GetSize().GetHeight();
    const wxSize butSize(height, height);
    m_pBtnSearchOpenFiles = CreateButton(controlIDs.Get(ControlIDs::idBtnSearchOpenFiles), butSize, prefix, "openfiles");
    m_pBtnSearchTargetFiles = CreateButton(controlIDs.Get(ControlIDs::idBtnSearchTargetFiles), butSize, prefix, "target");
    m_pBtnSearchProjectFiles = CreateButton(controlIDs.Get(ControlIDs::idBtnSearchProjectFiles), butSize, prefix, "project");
    m_pBtnSearchWorkspaceFiles = CreateButton(controlIDs.Get(ControlIDs::idBtnSearchWorkspaceFiles), butSize, prefix, "workspace");
    m_pBtnSearchDir = CreateButton(controlIDs.Get(ControlIDs::idBtnSearchDirectoryFiles), butSize, prefix, "folder");
    set_properties();
    do_layout();
    // end wxGlade
}

wxBitmapToggleButton* SearchInPanel::CreateButton(wxWindowID id, const wxSize& size, const wxString& prefix, const wxString& name)
{
#if wxCHECK_VERSION(3, 1, 6)
    const wxSize bmpSize(16, 16);
    wxBitmapToggleButton *button = new wxBitmapToggleButton(this, id, cbLoadBitmapBundleFromSVG(prefix+name+".svg", bmpSize), wxDefaultPosition, size);
    button->SetBitmapDisabled(cbLoadBitmapBundleFromSVG(prefix+name+"disabled.svg", bmpSize));
    button->SetBitmapPressed(cbLoadBitmapBundleFromSVG(prefix+name+"selected.svg", bmpSize));
#else
    wxBitmapToggleButton *button = new wxBitmapToggleButton(this, id, cbLoadBitmap(prefix+name+".png"), wxDefaultPosition, size);
    button->SetBitmapDisabled(cbLoadBitmap(prefix+name+"disabled.png"));
    button->SetBitmapPressed(cbLoadBitmap(prefix+name+"selected.png"));
#endif
    return button;
}

BEGIN_EVENT_TABLE(SearchInPanel, wxPanel)
    // begin wxGlade: SearchInPanel::event_table
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchOpenFiles), SearchInPanel::OnBtnClickEvent)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchTargetFiles), SearchInPanel::OnBtnSearchTargetFilesClick)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchProjectFiles), SearchInPanel::OnBtnSearchProjectFilesClick)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchWorkspaceFiles), SearchInPanel::OnBtnSearchWorkspaceFilesClick)
    EVT_TOGGLEBUTTON(controlIDs.Get(ControlIDs::idBtnSearchDirectoryFiles), SearchInPanel::OnBtnClickEvent)
    // end wxGlade
END_EVENT_TABLE();

void SearchInPanel::OnBtnClickEvent(wxCommandEvent &event)
{
    event.Skip();
}

void SearchInPanel::OnBtnSearchTargetFilesClick(wxCommandEvent &event)
{
    // If target scope becomes checked, we uncheck if necessary project
    // and workspace.
    if (event.GetInt())
    {
        SetSearchInProjectFiles(false);
        SetSearchInWorkspaceFiles(false);
    }

    event.Skip();
}

void SearchInPanel::OnBtnSearchProjectFilesClick(wxCommandEvent &event)
{
    // If project scope becomes checked, we uncheck if necessary target
    // and workspace.
    if (event.GetInt())
    {
        SetSearchInTargetFiles(false);
        SetSearchInWorkspaceFiles(false);
    }

    event.Skip();
}

void SearchInPanel::OnBtnSearchWorkspaceFilesClick(wxCommandEvent &event)
{
    // If workspace scope becomes checked, we uncheck if necessary target
    // and project.
    if (event.GetInt())
    {
        SetSearchInTargetFiles(false);
        SetSearchInProjectFiles(false);
    }

    event.Skip();
}

// wxGlade: add SearchInPanel event handlers

void SearchInPanel::set_properties()
{
    // begin wxGlade: SearchInPanel::set_properties
    m_pBtnSearchOpenFiles->SetToolTip(_("Search in open files"));
    m_pBtnSearchTargetFiles->SetToolTip(_("Search in target files"));
    m_pBtnSearchProjectFiles->SetToolTip(_("Search in project files"));
    m_pBtnSearchWorkspaceFiles->SetToolTip(_("Search in workspace files"));
    m_pBtnSearchDir->SetToolTip(_("Search in directory files"));
    // end wxGlade
}

void SearchInPanel::do_layout()
{
    // begin wxGlade: SearchInPanel::do_layout
    wxBoxSizer* SizerTop = new wxBoxSizer(wxHORIZONTAL);
    SizerTop->Add(m_pBtnSearchOpenFiles, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
    SizerTop->Add(m_pBtnSearchTargetFiles, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
    SizerTop->Add(m_pBtnSearchProjectFiles, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
    SizerTop->Add(m_pBtnSearchWorkspaceFiles, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
    SizerTop->Add(m_pBtnSearchDir, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
    SetAutoLayout(true);
    SetSizer(SizerTop);
    SizerTop->Fit(this);
    SizerTop->SetSizeHints(this);
    // end wxGlade
}

//{ Getters
bool SearchInPanel::GetSearchInOpenFiles() const
{
    return m_pBtnSearchOpenFiles->GetValue();
}

bool SearchInPanel::GetSearchInTargetFiles() const
{
    return m_pBtnSearchTargetFiles->GetValue();
}

bool SearchInPanel::GetSearchInProjectFiles() const
{
    return m_pBtnSearchProjectFiles->GetValue();
}

bool SearchInPanel::GetSearchInWorkspaceFiles() const
{
    return m_pBtnSearchWorkspaceFiles->GetValue();
}

bool SearchInPanel::GetSearchInDirectory() const
{
    return m_pBtnSearchDir->GetValue();
}
//}

//{ Setters
void SearchInPanel::SetSearchInOpenFiles(bool bSearchInOpenFiles)
{
    m_pBtnSearchOpenFiles->SetValue(bSearchInOpenFiles);
}

void SearchInPanel::SetSearchInTargetFiles(bool bSearchInTargetFiles)
{
    m_pBtnSearchTargetFiles->SetValue(bSearchInTargetFiles);
}

void SearchInPanel::SetSearchInProjectFiles(bool bSearchInProjectFiles)
{
    m_pBtnSearchProjectFiles->SetValue(bSearchInProjectFiles);
}

void SearchInPanel::SetSearchInWorkspaceFiles(bool bSearchInWorkspaceFiles)
{
    m_pBtnSearchWorkspaceFiles->SetValue(bSearchInWorkspaceFiles);
}

void SearchInPanel::SetSearchInDirectory(bool bSearchInDirectoryFiles)
{
    m_pBtnSearchDir->SetValue(bSearchInDirectoryFiles);
}
//}
