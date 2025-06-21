/***************************************************************
 * Name:      BrowseTrackerConfPanel
 * Purpose:   This class implements the configuration panel used
 *            in modal dialog called on settings menu click
 *            and by C::B on "Environment" settings window.
 * Author:    Pecan
 * Created:   2008/03/13
 * Copyright: Pecan
 * License:   GPL
 **************************************************************/

#include <sdk.h> // Code::Blocks SDK
#ifndef CB_PRECOMP
#include "configmanager.h"
#endif

#include "manager.h"
#include "Version.h"

#include "BrowseTracker.h"
#include "BrowseTrackerDefs.h"
#include "BrowseTrackerConfPanel.h"
#include "ConfigPanel.h"

// ----------------------------------------------------------------------------
//  Events table
// ----------------------------------------------------------------------------
BEGIN_EVENT_TABLE(BrowseTrackerConfPanel, wxPanel)

END_EVENT_TABLE();

// ----------------------------------------------------------------------------
BrowseTrackerConfPanel::BrowseTrackerConfPanel(BrowseTracker& browseTrackerPlugin, wxWindow* parent,wxWindowID id)
// ----------------------------------------------------------------------------
    :m_BrowseTrackerPlugin(browseTrackerPlugin)
    ,m_pConfigPanel(0 )
{
    //ctor

    // wxPanel creation
    Create(parent,id,wxDefaultPosition,wxDefaultSize,wxTAB_TRAVERSAL);
    m_pConfigPanel =  new ConfigPanel( this, id );
    wxBoxSizer* pMainSizer = new wxBoxSizer( wxVERTICAL );
    this->SetSizer( pMainSizer );
    pMainSizer->Add(m_pConfigPanel, 1 , wxEXPAND );
    pMainSizer->Layout();

	// Connect Events for choice validation
	m_pConfigPanel->Cfg_BrowseMarksEnabled->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BrowseTrackerConfPanel::OnEnableBrowseMarks ), NULL, this );
	m_pConfigPanel->Cfg_WrapJumpEntries->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BrowseTrackerConfPanel::OnWrapJumpEntries ), NULL, this );
	m_pConfigPanel->Cfg_ShowToolbar->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BrowseTrackerConfPanel::OnShowToolbar ), NULL, this );
	m_pConfigPanel->Cfg_ActivatePrevEd->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( BrowseTrackerConfPanel::OnActivatePrevEd ), NULL, this ); //2020/06/15
	m_pConfigPanel->Cfg_JumpTrackerSpinCtrl->Connect( wxEVT_SPINCTRL, wxSpinEventHandler( BrowseTrackerConfPanel::OnJumpTrackerSpinCtrl ), NULL, this );
	m_pConfigPanel->Cfg_ToggleKey->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( BrowseTrackerConfPanel::OnToggleBrowseMarkKey ), NULL, this );
	m_pConfigPanel->Cfg_ClearAllKey->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( BrowseTrackerConfPanel::OnClearAllBrowseMarksKey ), NULL, this );

    // FIXME (ph#): Something fishy here. On the first use of View/BrowseTracker/Settings
    // the BrowseMark is not set to the BookMark style when selected.
    // It does work when the Editor/Config BrowseTracker settings is used.
    // save some old data for later comparison
    m_BrowseTrackerPlugin.m_OldUserMarksStyle = m_BrowseTrackerPlugin.m_UserMarksStyle;
    m_BrowseTrackerPlugin.m_OldBrowseMarksEnabled = m_BrowseTrackerPlugin.m_BrowseMarksEnabled;

    // read current user options
    GetUserOptions( m_BrowseTrackerPlugin.GetBrowseTrackerCfgFilename() );
    // get ctrl-key ownership if editor multi-selection is enabled
    bEdMultiSelOn = Manager::Get()->GetConfigManager(_T("editor"))->ReadBool(_T("/selection/multi_select"), false);

    // enable/disable dialog options mapped to user options
    wxCommandEvent evt;
    OnEnableBrowseMarks( evt );
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnApply()
// ----------------------------------------------------------------------------
{
    // get any new user values
    m_BrowseTrackerPlugin.m_BrowseMarksEnabled  = m_pConfigPanel->Cfg_BrowseMarksEnabled->GetValue();
    m_BrowseTrackerPlugin.m_WrapJumpEntries     = m_pConfigPanel->Cfg_WrapJumpEntries->GetValue();
    //-m_BrowseTrackerPlugin.m_UserMarksStyle   = m_pConfigPanel->Cfg_MarkStyle->GetSelection();
    m_BrowseTrackerPlugin.m_UserMarksStyle      = BookMarksStyle;

    m_BrowseTrackerPlugin.m_ToggleKey           = m_pConfigPanel->Cfg_ToggleKey->GetSelection();
	m_BrowseTrackerPlugin.m_LeftMouseDelay      = m_pConfigPanel->Cfg_LeftMouseDelay->GetValue();
	m_BrowseTrackerPlugin.m_ClearAllKey         = m_pConfigPanel->Cfg_ClearAllKey->GetSelection();

    m_BrowseTrackerPlugin.m_ConfigShowToolbar   = m_pConfigPanel->Cfg_ShowToolbar->GetValue();
    m_BrowseTrackerPlugin.ShowBrowseTrackerToolBar(m_BrowseTrackerPlugin.m_ConfigShowToolbar);

    m_BrowseTrackerPlugin.m_CfgActivatePrevEd   = m_pConfigPanel->Cfg_ActivatePrevEd->GetValue(); //2020/06/15

    m_BrowseTrackerPlugin.m_CfgJumpViewRowCount = m_pConfigPanel->Cfg_JumpTrackerSpinCtrl->GetValue();

    // write user options to config file
	m_BrowseTrackerPlugin.SaveUserOptions( m_BrowseTrackerPlugin.GetBrowseTrackerCfgFilename() );
	// call validation/update routine
	m_BrowseTrackerPlugin.OnConfigApply();
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::GetUserOptions(wxString configFullPath)
// ----------------------------------------------------------------------------
{
    // Read user options from storage file
    wxString m_ConfigFullPath = configFullPath;

    m_BrowseTrackerPlugin.ReadUserOptions( configFullPath );

    #if defined(__WXMSW__)
    // The following causes bad color combination in Windows 10
    //-m_pConfigPanel->m_staticText4->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_ACTIVECAPTION ) );
    #endif

    // set the current values
    m_pConfigPanel->Cfg_BrowseMarksEnabled->SetValue( m_BrowseTrackerPlugin.m_BrowseMarksEnabled);
    m_pConfigPanel->Cfg_WrapJumpEntries->SetValue( m_BrowseTrackerPlugin.m_WrapJumpEntries);
    m_pConfigPanel->Cfg_ToggleKey->SetSelection( m_BrowseTrackerPlugin.m_ToggleKey );
	m_pConfigPanel->Cfg_LeftMouseDelay->SetValue( m_BrowseTrackerPlugin.m_LeftMouseDelay ) ;
	m_pConfigPanel->Cfg_ClearAllKey->SetSelection( m_BrowseTrackerPlugin.m_ClearAllKey ) ;

    m_pConfigPanel->Cfg_ActivatePrevEd->SetValue(m_BrowseTrackerPlugin.m_CfgActivatePrevEd); //2020/06/15
    m_pConfigPanel->Cfg_JumpTrackerSpinCtrl->SetValue(m_BrowseTrackerPlugin.m_CfgJumpViewRowCount);

//-m_pConfigPanel->Cfg_ShowToolbar->SetValue(m_BrowseTrackerPlugin.IsViewToolbarEnabled());
    m_pConfigPanel->Cfg_ShowToolbar->SetValue(m_BrowseTrackerPlugin.m_ConfigShowToolbar);

}//Init

//// ----------------------------------------------------------------------------
//BrowseTrackerConfPanel::~BrowseTrackerConfPanel()
//// ----------------------------------------------------------------------------
//{
//    //dtor
//}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnEnableBrowseMarks( wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Enable BrowseMarks options if "Enable BrowseMarks" is checked
    if ( not m_pConfigPanel->Cfg_BrowseMarksEnabled->IsChecked() )
    {
        m_pConfigPanel->Cfg_ToggleKey->Enable(false);
        m_pConfigPanel->Cfg_LeftMouseDelay->Enable(false); ;
        m_pConfigPanel->Cfg_ClearAllKey->Enable(false); ;

    }
    if ( m_pConfigPanel->Cfg_BrowseMarksEnabled->IsChecked() )
    {
        m_pConfigPanel->Cfg_ToggleKey->Enable(true);
        m_pConfigPanel->Cfg_LeftMouseDelay->Enable(true);
        m_pConfigPanel->Cfg_ClearAllKey->Enable(true); ;
        // if Ctrl-key belongs to editor multi-selection, disable here
        if (bEdMultiSelOn)
        {
            m_pConfigPanel->Cfg_ToggleKey->Enable(false);
            m_pConfigPanel->Cfg_ClearAllKey->Enable(false); ;

        }
    }
    event.Skip();
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnWrapJumpEntries( wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Enable Jump entry wraps if "Wrap Jum0 Entries" is checked
    if ( not m_pConfigPanel->Cfg_WrapJumpEntries->IsChecked() )
    {
        //-?m_pConfigPanel->Cfg_WrapJumpEntries->Enable(false); //dont disable 2020/12/22
    }

    if ( m_pConfigPanel->Cfg_WrapJumpEntries->IsChecked() )
    {
        //_m_pConfigPanel->Cfg_WrapJumpEntries->Enable(true);   //dont disable 2020/12/22
    }
    event.Skip();
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnActivatePrevEd( wxCommandEvent& event ) //2020/12/22
// ----------------------------------------------------------------------------
{
    // Enable switching to previous editor when current closed
    if ( not m_pConfigPanel->Cfg_ActivatePrevEd->IsChecked() )
    {
       //- m_pConfigPanel->Cfg_ActivatePrevEd->Enable(false); dont do this
    }

    if ( m_pConfigPanel->Cfg_ActivatePrevEd->IsChecked() )
    {
        //-m_pConfigPanel->Cfg_ActivatePrevEd->Enable(true); dont do this
    }
    event.Skip();
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnJumpTrackerSpinCtrl( wxSpinEvent& event )
// ----------------------------------------------------------------------------
{

    //int debugLook = m_pConfigPanel->Cfg_JumpTrackerSpinCtrl->GetValue();

}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnShowToolbar( wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Enable/Disable BrowseTracker tool bar
    if ( not m_pConfigPanel->Cfg_ShowToolbar->IsChecked() )
    {
        m_pConfigPanel->Cfg_ShowToolbar->Enable(false);
    }

    if ( m_pConfigPanel->Cfg_ShowToolbar->IsChecked() )
    {
        m_pConfigPanel->Cfg_ShowToolbar->Enable(true);
    }
    event.Skip();
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnToggleBrowseMarkKey( wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Dont allow both Toggle/Ctrl-Left-Mouse and Clear/Ctrl-Left-Mouse
    if ( m_pConfigPanel->Cfg_ToggleKey->GetSelection() == 1 )
        m_pConfigPanel->Cfg_ClearAllKey->SetSelection( 1 ) ;

    event.Skip();
}
// ----------------------------------------------------------------------------
void BrowseTrackerConfPanel::OnClearAllBrowseMarksKey( wxCommandEvent& event )
// ----------------------------------------------------------------------------
{
    // Dont allow both Toggle/Ctrl-Left-Mouse and Clear/Ctrl-Left-Mouse
    if ( m_pConfigPanel->Cfg_ClearAllKey->GetSelection() == 0 )
        m_pConfigPanel->Cfg_ToggleKey->SetSelection(0);

    event.Skip();
}
// ----------------------------------------------------------------------------

