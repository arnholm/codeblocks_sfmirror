#include "sdk.h"

#ifndef CB_PRECOMP
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include "configmanager.h"
#endif

#include <wx/settings.h>
#include "byoconf.h"
#include "byogamebase.h"

//(*InternalHeaders(byoConf)
#include <wx/intl.h>
#include <wx/string.h>
//*)

//(*IdInit(byoConf)
const long byoConf::ID_CHECKBOX1 = wxNewId();
const long byoConf::ID_SPINCTRL1 = wxNewId();
const long byoConf::ID_CHECKBOX2 = wxNewId();
const long byoConf::ID_SPINCTRL2 = wxNewId();
const long byoConf::ID_CHECKBOX3 = wxNewId();
const long byoConf::ID_SPINCTRL3 = wxNewId();
const long byoConf::ID_STATICTEXT1 = wxNewId();
const long byoConf::ID_COLOURPICKERCTRL1 = wxNewId();
const long byoConf::ID_STATICTEXT2 = wxNewId();
const long byoConf::ID_COLOURPICKERCTRL2 = wxNewId();
const long byoConf::ID_STATICTEXT3 = wxNewId();
const long byoConf::ID_COLOURPICKERCTRL3 = wxNewId();
const long byoConf::ID_STATICTEXT4 = wxNewId();
const long byoConf::ID_COLOURPICKERCTRL4 = wxNewId();
const long byoConf::ID_STATICTEXT5 = wxNewId();
const long byoConf::ID_COLOURPICKERCTRL5 = wxNewId();
const long byoConf::ID_STATICTEXT6 = wxNewId();
const long byoConf::ID_COLOURPICKERCTRL6 = wxNewId();
//*)

BEGIN_EVENT_TABLE(byoConf,wxPanel)
	//(*EventTable(byoConf)
	//*)
END_EVENT_TABLE()

byoConf::byoConf(wxWindow* parent,wxWindowID id)
{
	//(*Initialize(byoConf)
	Create(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("id"));
	FlexGridSizer1 = new wxFlexGridSizer(0, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	StaticBoxSizer1 = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Back-To-Work settings"));
	FlexGridSizer2 = new wxFlexGridSizer(0, 2, 0, 0);
	FlexGridSizer2->AddGrowableCol(1);
	m_MaxPlaytimeChk = new wxCheckBox(this, ID_CHECKBOX1, _("Maximum play time (sec)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX1"));
	m_MaxPlaytimeChk->SetValue(true);
	FlexGridSizer2->Add(m_MaxPlaytimeChk, 0, wxBOTTOM|wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
	m_MaxPlaytimeSpn = new wxSpinCtrl(this, ID_SPINCTRL1, _T("1"), wxDefaultPosition, wxDefaultSize, 0, 1, 1000000, 1, _T("ID_SPINCTRL1"));
	m_MaxPlaytimeSpn->SetValue(_T("1"));
	FlexGridSizer2->Add(m_MaxPlaytimeSpn, 1, wxBOTTOM|wxLEFT|wxRIGHT|wxEXPAND, 4);
	m_MinWorkChk = new wxCheckBox(this, ID_CHECKBOX2, _("Minimum work time (sec)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX2"));
	m_MinWorkChk->SetValue(true);
	FlexGridSizer2->Add(m_MinWorkChk, 0, wxBOTTOM|wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
	m_MinWorkSpn = new wxSpinCtrl(this, ID_SPINCTRL2, _T("1"), wxDefaultPosition, wxDefaultSize, 0, 1, 1000000, 1, _T("ID_SPINCTRL2"));
	m_MinWorkSpn->SetValue(_T("1"));
	FlexGridSizer2->Add(m_MinWorkSpn, 1, wxBOTTOM|wxLEFT|wxRIGHT|wxEXPAND, 4);
	m_OverworkChk = new wxCheckBox(this, ID_CHECKBOX3, _("Overwork time (sec)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX3"));
	m_OverworkChk->SetValue(true);
	FlexGridSizer2->Add(m_OverworkChk, 0, wxBOTTOM|wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 4);
	m_OverworkSpn = new wxSpinCtrl(this, ID_SPINCTRL3, _T("1"), wxDefaultPosition, wxDefaultSize, 0, 1, 1000000, 1, _T("ID_SPINCTRL3"));
	m_OverworkSpn->SetValue(_T("1"));
	FlexGridSizer2->Add(m_OverworkSpn, 1, wxBOTTOM|wxLEFT|wxRIGHT|wxEXPAND, 4);
	StaticBoxSizer1->Add(FlexGridSizer2, 1, wxEXPAND, 4);
	FlexGridSizer1->Add(StaticBoxSizer1, 1, wxBOTTOM|wxLEFT|wxRIGHT|wxEXPAND, 4);
	StaticBoxSizer2 = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Default colours"));
	FlexGridSizer3 = new wxFlexGridSizer(0, 8, 0, 0);
	FlexGridSizer3->AddGrowableCol(1);
	FlexGridSizer3->AddGrowableCol(4);
	FlexGridSizer3->AddGrowableCol(7);
	FlexGridSizer3->AddGrowableRow(0);
	FlexGridSizer3->AddGrowableRow(1);
	StaticText1 = new wxStaticText(this, ID_STATICTEXT1, _("1"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
	FlexGridSizer3->Add(StaticText1, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	m_Col1 = new wxColourPickerCtrl(this, ID_COLOURPICKERCTRL1, wxColour(0,0,0), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_COLOURPICKERCTRL1"));
	FlexGridSizer3->Add(m_Col1, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 4);
	FlexGridSizer3->Add(-1,-1,1, wxALL|wxEXPAND, 4);
	StaticText2 = new wxStaticText(this, ID_STATICTEXT2, _("3"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
	FlexGridSizer3->Add(StaticText2, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	m_Col3 = new wxColourPickerCtrl(this, ID_COLOURPICKERCTRL2, wxColour(0,0,0), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_COLOURPICKERCTRL2"));
	FlexGridSizer3->Add(m_Col3, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 4);
	FlexGridSizer3->Add(-1,-1,1, wxALL|wxEXPAND, 4);
	StaticText3 = new wxStaticText(this, ID_STATICTEXT3, _("5"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT3"));
	FlexGridSizer3->Add(StaticText3, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	m_Col5 = new wxColourPickerCtrl(this, ID_COLOURPICKERCTRL3, wxColour(0,0,0), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_COLOURPICKERCTRL3"));
	FlexGridSizer3->Add(m_Col5, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 4);
	StaticText4 = new wxStaticText(this, ID_STATICTEXT4, _("2"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT4"));
	FlexGridSizer3->Add(StaticText4, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	m_Col2 = new wxColourPickerCtrl(this, ID_COLOURPICKERCTRL4, wxColour(0,0,0), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_COLOURPICKERCTRL4"));
	FlexGridSizer3->Add(m_Col2, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 4);
	FlexGridSizer3->Add(0,0,1, wxALL|wxEXPAND, 4);
	StaticText5 = new wxStaticText(this, ID_STATICTEXT5, _("4"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT5"));
	FlexGridSizer3->Add(StaticText5, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	m_Col4 = new wxColourPickerCtrl(this, ID_COLOURPICKERCTRL5, wxColour(0,0,0), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_COLOURPICKERCTRL5"));
	FlexGridSizer3->Add(m_Col4, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 4);
	FlexGridSizer3->Add(-1,-1,1, wxALL|wxEXPAND, 4);
	StaticText6 = new wxStaticText(this, ID_STATICTEXT6, _("6"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT6"));
	FlexGridSizer3->Add(StaticText6, 1, wxTOP|wxBOTTOM|wxLEFT|wxALIGN_CENTER_VERTICAL, 4);
	m_Col6 = new wxColourPickerCtrl(this, ID_COLOURPICKERCTRL6, wxColour(0,0,0), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_COLOURPICKERCTRL6"));
	FlexGridSizer3->Add(m_Col6, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 4);
	StaticBoxSizer2->Add(FlexGridSizer3, 1, wxEXPAND, 4);
	FlexGridSizer1->Add(StaticBoxSizer2, 1, wxBOTTOM|wxLEFT|wxRIGHT|wxEXPAND, 4);
	SetSizer(FlexGridSizer1);

	Connect(ID_CHECKBOX1,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&byoConf::BTWSRefresh);
	Connect(ID_CHECKBOX2,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&byoConf::BTWSRefresh);
	Connect(ID_CHECKBOX3,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&byoConf::BTWSRefresh);
	//*)

    ConfigManager* cfg = Manager::Get()->GetConfigManager("byogames");
    m_MaxPlaytimeChk->SetValue(cfg->ReadBool("/ismaxplaytime", true));
    m_MaxPlaytimeSpn->SetValue(cfg->ReadInt("/maxplaytime", 60*10));
    m_MinWorkChk->SetValue(cfg->ReadBool("/isminworktime", true));
    m_MinWorkSpn->SetValue(cfg->ReadInt("/minworktime", 60*60));
    m_OverworkChk->SetValue(cfg->ReadBool("/isoverworktime", false));
    m_OverworkSpn->SetValue(cfg->ReadInt("/overworktime", 3*60*60));

    m_Col1->SetColour(cfg->ReadColour("/col01", wxColour(0xFF, 0x00, 0x00)));
    m_Col2->SetColour(cfg->ReadColour("/col02", wxColour(0x00, 0xFF, 0x00)));
    m_Col3->SetColour(cfg->ReadColour("/col03", wxColour(0x00, 0x00, 0xFF)));
    m_Col4->SetColour(cfg->ReadColour("/col04", wxColour(0xFF, 0xFF, 0x00)));
    m_Col5->SetColour(cfg->ReadColour("/col05", wxColour(0xFF, 0x00, 0xFF)));
    m_Col6->SetColour(cfg->ReadColour("/col06", wxColour(0x00, 0xFF, 0xFF)));

    SetSize(500,500);
}

byoConf::~byoConf()
{
}

void byoConf::OnApply()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("byogames");
    cfg->Write(_T("/ismaxplaytime"), (bool)m_MaxPlaytimeChk->GetValue());
    cfg->Write(_T("/maxplaytime"), (int)m_MaxPlaytimeSpn->GetValue());
    cfg->Write(_T("/isminworktime"), (bool)m_MinWorkChk->GetValue());
    cfg->Write(_T("/minworktime"), (int)m_MinWorkSpn->GetValue());
    cfg->Write(_T("/isoverworktime"), (bool)m_OverworkChk->GetValue());
    cfg->Write(_T("/overworktime"), (int)m_OverworkSpn->GetValue());
    cfg->Write(_T("/col01"), m_Col1->GetColour());
    cfg->Write(_T("/col02"), m_Col2->GetColour());
    cfg->Write(_T("/col03"), m_Col3->GetColour());
    cfg->Write(_T("/col04"), m_Col4->GetColour());
    cfg->Write(_T("/col05"), m_Col5->GetColour());
    cfg->Write(_T("/col06"), m_Col6->GetColour());

    byoGameBase::ReloadFromConfig();
}

void byoConf::BTWSRefresh(wxCommandEvent& /*event*/)
{
    if ( m_MaxPlaytimeChk->GetValue() )
    {
        m_MaxPlaytimeSpn->Enable();
        m_MinWorkChk->Enable();
        m_MinWorkSpn->Enable(m_MinWorkChk->GetValue());
    }
    else
    {
        m_MaxPlaytimeSpn->Disable();
        m_MinWorkChk->Disable();
        m_MinWorkSpn->Disable();
    }
    m_OverworkSpn->Enable(m_OverworkChk->GetValue());
}
