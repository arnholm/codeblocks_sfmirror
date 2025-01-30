///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif //WX_PRECOMP

#include "ConfigPanel.h"

///////////////////////////////////////////////////////////////////////////

ConfigPanel::ConfigPanel( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxVERTICAL );

	m_staticText2 = new wxStaticText( this, wxID_ANY, _("BrowseTracker Options"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
	m_staticText2->Wrap( -1 );
//-	m_staticText2->SetFont( wxFont( 9, 74, 90, 92, false, wxT("Tahoma") ) );

	bSizer3->Add( m_staticText2, 0, wxALL|wxEXPAND, 5 );


	bSizer3->Add( 0, 10, 0, 0, 5 );

	wxBoxSizer* bSizer71;
	bSizer71 = new wxBoxSizer( wxHORIZONTAL );

	Cfg_BrowseMarksEnabled = new wxCheckBox( this, wxID_ANY, _("Enable BookMark Tracking"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer71->Add( Cfg_BrowseMarksEnabled, 1, wxALL, 5 );

	Cfg_WrapJumpEntries = new wxCheckBox( this, wxID_ANY, _("Wrap Jump Entries"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer71->Add( Cfg_WrapJumpEntries, 1, wxALL, 5 );

	Cfg_ShowToolbar = new wxCheckBox( this, wxID_ANY, _("Show Toolbar always"), wxDefaultPosition, wxDefaultSize, 0 );
	Cfg_ShowToolbar->SetValue(true);
	bSizer71->Add( Cfg_ShowToolbar, 0, wxALL, 5 );

	bSizer3->Add( bSizer71, 0, wxEXPAND|wxSHAPED, 5 );

	wxBoxSizer* bSizer711;
	bSizer711 = new wxBoxSizer( wxHORIZONTAL );
	Cfg_ActivatePrevEd = new wxCheckBox( this, wxID_ANY, _("On editor close, activate previously active editor"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer711->Add( Cfg_ActivatePrevEd, 0, wxALL, 5 );
	bSizer711->Add( 0, 50, 1, wxEXPAND, 5 );
	bSizer3->Add( bSizer711, 0, wxEXPAND, 5 );

    // JumpTracker max rows
	wxBoxSizer* bSizer712;
	bSizer712 = new wxBoxSizer( wxHORIZONTAL );
	m_staticText712 = new wxStaticText( this, wxID_ANY, _("Max number of Browse marks and Jump entries\n  ( Requires a restart of CodeBlocks when changed )."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText712->Wrap( -1 );
	bSizer712->Add( m_staticText712, 0, wxALL|wxEXPAND, 5 );
	Cfg_JumpTrackerSpinCtrl = new wxSpinCtrl( this, wxID_ANY,"20", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 20, 100, 20);
	bSizer712->Add( Cfg_JumpTrackerSpinCtrl, 0, wxALL, 5 );
	bSizer712->Add( 0, 50, 1, wxEXPAND, 5 );
	bSizer3->Add( bSizer712, 0, wxEXPAND, 5 );


	wxBoxSizer* bSizer8;
	bSizer8 = new wxBoxSizer( wxVERTICAL );

	wxString Cfg_ToggleKeyChoices[] = { _("Left_Mouse"), _("Ctrl-Left_Mouse") };
	int Cfg_ToggleKeyNChoices = sizeof( Cfg_ToggleKeyChoices ) / sizeof( wxString );
	Cfg_ToggleKey = new wxRadioBox( this, wxID_ANY, _("Toggle BookMark Key"), wxDefaultPosition, wxDefaultSize, Cfg_ToggleKeyNChoices, Cfg_ToggleKeyChoices, 3, wxRA_SPECIFY_COLS );
	Cfg_ToggleKey->SetSelection( 0 );
	bSizer8->Add( Cfg_ToggleKey, 0, wxALL|wxEXPAND, 5 );

	bSizer3->Add( bSizer8, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer9;
	bSizer9 = new wxBoxSizer( wxVERTICAL );

	m_staticText4 = new wxStaticText( this, wxID_ANY, _(" Left_Mouse delay before BookMark Toggle (Milliseconds)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText4->Wrap( -1 );
	bSizer9->Add( m_staticText4, 0, wxALL|wxEXPAND, 5 );

	Cfg_LeftMouseDelay = new wxSlider( this, wxID_ANY, 200, 0, 1000, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS|wxSL_HORIZONTAL|wxSL_LABELS );
	bSizer9->Add( Cfg_LeftMouseDelay, 1, wxALL|wxEXPAND, 5 );

	bSizer3->Add( bSizer9, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer10;
	bSizer10 = new wxBoxSizer( wxVERTICAL );

	wxString Cfg_ClearAllKeyChoices[] = { _("Ctrl-Left_Mouse"), _("Ctrl-Left_DblClick") };
	int Cfg_ClearAllKeyNChoices = sizeof( Cfg_ClearAllKeyChoices ) / sizeof( wxString );
	Cfg_ClearAllKey = new wxRadioBox( this, wxID_ANY, _("Clear All BookMarks"), wxDefaultPosition, wxDefaultSize, Cfg_ClearAllKeyNChoices, Cfg_ClearAllKeyChoices, 2, wxRA_SPECIFY_COLS );
	Cfg_ClearAllKey->SetSelection( 0 );
	bSizer10->Add( Cfg_ClearAllKey, 0, wxALL|wxEXPAND, 5 );

	bSizer3->Add( bSizer10, 1, wxEXPAND, 5 );

	m_staticText3 = new wxStaticText( this, wxID_ANY, _("Note: The Ctrl-Left_Mouse key options are disabled when\nthe editors multi-selection option is enabled at:\nSettings/Editor/Margins/Allow Multiple Selections\n\nMenu items can be used for additional BrowseTracker functions.\n(MainMenu/View/BrowseTracker)\n"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3->Wrap( -1 );
	bSizer3->Add( m_staticText3, 0, wxALL, 5 );

	this->SetSizer( bSizer3 );
	this->Layout();

	// Connect Events
	Cfg_BrowseMarksEnabled->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnEnableBrowseMarks ), NULL, this );
	Cfg_WrapJumpEntries->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnWrapJumpEntries ), NULL, this );
	Cfg_ShowToolbar->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnShowToolbar ), NULL, this );
	Cfg_JumpTrackerSpinCtrl->Connect( wxEVT_SPINCTRL, wxSpinEventHandler( ConfigPanel::OnJumpTrackerSpinCtrl ), NULL, this );
	Cfg_ActivatePrevEd->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnEnableBrowseMarks ), NULL, this );
	Cfg_ToggleKey->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( ConfigPanel::OnToggleBrowseMarkKey ), NULL, this );
	Cfg_ClearAllKey->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( ConfigPanel::OnClearAllBrowseMarksKey ), NULL, this );
}

ConfigPanel::~ConfigPanel()
{
	// Disconnect Events
	Cfg_BrowseMarksEnabled->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnEnableBrowseMarks ), NULL, this );
	Cfg_WrapJumpEntries->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnWrapJumpEntries ), NULL, this );
	Cfg_ShowToolbar->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnShowToolbar ), NULL, this );
	Cfg_ActivatePrevEd->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( ConfigPanel::OnEnableBrowseMarks ), NULL, this );
	Cfg_JumpTrackerSpinCtrl->Disconnect( wxEVT_SPINCTRL, wxSpinEventHandler( ConfigPanel::OnJumpTrackerSpinCtrl ), NULL, this );
	Cfg_ToggleKey->Disconnect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( ConfigPanel::OnToggleBrowseMarkKey ), NULL, this );
	Cfg_ClearAllKey->Disconnect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( ConfigPanel::OnClearAllBrowseMarksKey ), NULL, this );

}
