#ifndef BYOCONF_H
#define BYOCONF_H

#include <wx/intl.h>
#include "configurationpanel.h"

//(*Headers(byoConf)
#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
//*)

class byoConf: public cbConfigurationPanel
{
	public:

		byoConf(wxWindow* parent,wxWindowID id = -1);
		virtual ~byoConf();

        wxString GetTitle() const { return _("C::B games"); }
        wxString GetBitmapBaseName() const { return "generic-plugin"; }
        void OnApply();
        void OnCancel(){}

	protected:

		//(*Identifiers(byoConf)
		static const long ID_CHECKBOX1;
		static const long ID_SPINCTRL1;
		static const long ID_CHECKBOX2;
		static const long ID_SPINCTRL2;
		static const long ID_CHECKBOX3;
		static const long ID_SPINCTRL3;
		static const long ID_STATICTEXT1;
		static const long ID_COLOURPICKERCTRL1;
		static const long ID_STATICTEXT2;
		static const long ID_COLOURPICKERCTRL2;
		static const long ID_STATICTEXT3;
		static const long ID_COLOURPICKERCTRL3;
		static const long ID_STATICTEXT4;
		static const long ID_COLOURPICKERCTRL4;
		static const long ID_STATICTEXT5;
		static const long ID_COLOURPICKERCTRL5;
		static const long ID_STATICTEXT6;
		static const long ID_COLOURPICKERCTRL6;
		//*)
		//(*Handlers(byoConf)
		void BTWSRefresh(wxCommandEvent& event);
		//*)
		//(*Declarations(byoConf)
		wxCheckBox* m_MaxPlaytimeChk;
		wxCheckBox* m_MinWorkChk;
		wxCheckBox* m_OverworkChk;
		wxColourPickerCtrl* m_Col1;
		wxColourPickerCtrl* m_Col2;
		wxColourPickerCtrl* m_Col3;
		wxColourPickerCtrl* m_Col4;
		wxColourPickerCtrl* m_Col5;
		wxColourPickerCtrl* m_Col6;
		wxFlexGridSizer* FlexGridSizer1;
		wxFlexGridSizer* FlexGridSizer2;
		wxFlexGridSizer* FlexGridSizer3;
		wxSpinCtrl* m_MaxPlaytimeSpn;
		wxSpinCtrl* m_MinWorkSpn;
		wxSpinCtrl* m_OverworkSpn;
		wxStaticBoxSizer* StaticBoxSizer1;
		wxStaticBoxSizer* StaticBoxSizer2;
		wxStaticText* StaticText1;
		wxStaticText* StaticText2;
		wxStaticText* StaticText3;
		wxStaticText* StaticText4;
		wxStaticText* StaticText5;
		wxStaticText* StaticText6;
		//*)

	private:

		DECLARE_EVENT_TABLE()
};

#endif // BYOCONF_H
