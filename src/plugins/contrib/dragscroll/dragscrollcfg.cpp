#include "configmanager.h"
#include "dragscrollcfg.h"

BEGIN_EVENT_TABLE(cbDragScrollCfg,cbConfigurationPanel)
//	//(*EventTable(cbDragScrollCfg)
//	EVT_BUTTON(ID_DONEBUTTON,cbDragScrollCfg::OnDoneButtonClick)
//	//*)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
cbDragScrollCfg::cbDragScrollCfg(wxWindow* parent, cbDragScroll* pOwner, wxWindowID /*id*/)
// ----------------------------------------------------------------------------
    :pOwnerClass(pOwner)
{
    cbConfigurationPanel::Create(parent, -1, wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxVERTICAL );

	bSizer2->SetMinSize(wxSize( -1,50 ));
	StaticText1 = new wxStaticText( this, wxID_ANY, _("Mouse Drag Scrolling Configuration"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer2->Add( StaticText1, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	bSizer2->Add( 0, 0, 1, wxEXPAND, 0 );

	bSizer1->Add( bSizer2, 0, wxEXPAND, 5 );

	wxBoxSizer* bSizer5;
	bSizer5 = new wxBoxSizer( wxHORIZONTAL );
	ScrollEnabled = new wxCheckBox( this, wxID_ANY, _("Scrolling Enabled"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer5->Add( ScrollEnabled, 0, wxALL, 5 );
	bSizer1->Add( bSizer5, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

	wxBoxSizer* bSizer6;
	bSizer6 = new wxBoxSizer( wxHORIZONTAL );
	EditorFocusEnabled = new wxCheckBox( this, wxID_ANY, _("Auto Focus Editors"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer6->Add( EditorFocusEnabled, 0, wxALL, 5 );

	MouseFocusEnabled = new wxCheckBox( this, wxID_ANY, _("Focus follows Mouse"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer6->Add( MouseFocusEnabled, 0, wxALL, 5 );
	bSizer1->Add( bSizer6, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

	wxBoxSizer* MouseWheelSizer1;
	MouseWheelSizer1 = new wxBoxSizer( wxHORIZONTAL );
	MouseWheelZoom = new wxCheckBox( this, wxID_ANY, _("Log MouseWheelZoom"), wxDefaultPosition, wxDefaultSize, 0 );
	MouseWheelSizer1->Add( MouseWheelZoom, 0, wxALL, 5 );
	PropagateLogZoomSize = new wxCheckBox( this, wxID_ANY, _("Propagate Log Zooms"), wxDefaultPosition, wxDefaultSize, 0 );
	MouseWheelSizer1->Add( PropagateLogZoomSize, 0, wxALL, 5 );
    bSizer1->Add( MouseWheelSizer1, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

	//2019/03/30
	wxBoxSizer* MouseWheelZoomReverseSizer1;
	MouseWheelZoomReverseSizer1 = new wxBoxSizer( wxHORIZONTAL );
	MouseWheelZoomReverse = new wxCheckBox( this, wxID_ANY, _("Reverse mouse Zoom direction"), wxDefaultPosition, wxDefaultSize, 0 );
	MouseWheelZoomReverseSizer1->Add( MouseWheelZoomReverse, 0, wxALL, 5 );
	bSizer1->Add( MouseWheelZoomReverseSizer1, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );

	wxString ScrollDirectionChoices[] = { _("With Mouse"), _("Opposite Mouse") };
	int ScrollDirectionNChoices = sizeof( ScrollDirectionChoices ) / sizeof( wxString );
	ScrollDirection = new wxRadioBox( this, wxID_ANY, _("Scroll Direction"), wxDefaultPosition, wxDefaultSize, ScrollDirectionNChoices, ScrollDirectionChoices, 2, wxRA_SPECIFY_COLS );
	bSizer3->Add( ScrollDirection, 0, wxALL, 5 );

	bSizer1->Add( bSizer3, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxHORIZONTAL );

	StaticText2 = new wxStaticText( this, wxID_ANY, _("Mouse Button To Use:"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer4->Add( StaticText2, 0, wxALL, 5 );

	wxString MouseKeyChoiceChoices[] = { _("Right Mouse"), _("Alt+Right Mouse"), _("Shift+Right Mouse"), _("Middle Mouse"), _("Alt+Middle Mouse"), _("Shift+MIddle Mouse")};

	int MouseKeyChoiceNChoices = sizeof( MouseKeyChoiceChoices ) / sizeof( wxString );
	MouseKeyChoice = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, MouseKeyChoiceNChoices, MouseKeyChoiceChoices, 0 );
	bSizer4->Add( MouseKeyChoice, 0, wxALL, 5 );

	bSizer1->Add( bSizer4, 0, wxALIGN_CENTER_HORIZONTAL, 5 );

	wxBoxSizer* bSizer7;
	bSizer7 = new wxBoxSizer( wxVERTICAL );

	bSizer7->SetMinSize(wxSize( 300,-1 ));
	//bSizer7->Add( 0, 0, 1, wxEXPAND, 0 );
	bSizer7->Add( 0, 0, 0, wxEXPAND, 0 );

	StaticText3 = new wxStaticText( this, wxID_ANY, _("-- Adaptive Mouse Speed Sensitivity --"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer7->Add( StaticText3, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

	Sensitivity = new wxSlider( this, wxID_ANY, 8, 1, 10, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS|wxSL_HORIZONTAL|wxSL_LABELS|wxSL_TOP );
	if ( 1 ) Sensitivity->SetTickFreq(1);
	if ( 1 ) Sensitivity->SetPageSize(1);
	if ( 0 ) Sensitivity->SetLineSize(0);
	if ( 0 ) Sensitivity->SetThumbLength(0);
	if ( 1 ) Sensitivity->SetTick(1);
	if ( 1 ) Sensitivity->SetSelection(1,10);
	bSizer7->Add( Sensitivity, 0, wxALL|wxEXPAND, 5 );

	bSizer1->Add( bSizer7, 1, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );
	this->SetSizer( bSizer1 );
	this->Layout();

}
// ----------------------------------------------------------------------------
void cbDragScrollCfg::OnApply()
// ----------------------------------------------------------------------------
{
    pOwnerClass->OnDialogDone(this);
}
// ----------------------------------------------------------------------------
cbDragScrollCfg::~cbDragScrollCfg()
// ----------------------------------------------------------------------------
{
}
// ----------------------------------------------------------------------------
void cbDragScrollCfg::OnDoneButtonClick(wxCommandEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    //EndModal(0);
    #if defined(LOGGING)
    LOGIT( _T("cbDragScrollCfg::OnDoneButtonClick erroniously called") );
    #endif
}
// ----------------------------------------------------------------------------
wxString cbDragScrollCfg::GetBitmapBaseName() const
// ----------------------------------------------------------------------------
{
    //probing
    //LOGIT( _T("Config:%s"),ConfigManager::GetConfigFolder().GetData()  );
    //LOGIT( _T("Plugins:%s"),ConfigManager::GetPluginsFolder().GetData() );
    //LOGIT( _T("Data:%s"),ConfigManager::GetDataFolder().GetData() );
    //LOGIT( _T("Executable:%s"),ConfigManager::GetExecutableFolder().GetData() );

    wxString pngName = _T("generic-plugin");
    //if file exist "./share/codeblocks/images/settings/cbdragscroll.png";
    #ifdef __WXGTK__
     if ( ::wxFileExists(ConfigManager::GetDataFolder() + _T("/images/settings/dragscroll.png")) )
    #else
     if ( ::wxFileExists(ConfigManager::GetDataFolder() + _T("\\images\\settings\\dragscroll.png")) )
    #endif
    	pngName = _T("dragscroll") ;
    // else return "generic-plugin"
    return pngName;
}
// ----------------------------------------------------------------------------
//
