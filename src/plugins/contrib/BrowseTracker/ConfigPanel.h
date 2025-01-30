///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __ConfigPanel__
#define __ConfigPanel__

#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/radiobox.h>
#include <wx/slider.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class ConfigPanel
///////////////////////////////////////////////////////////////////////////////
class ConfigPanel : public wxPanel
{
	private:

	protected:
		wxStaticText* m_staticText2;
		wxStaticText* m_staticText3;
		wxStaticText* m_staticText712;  // JumpView rows spin ctrl

		// Virtual event handlers, overide them in your derived class
		virtual void OnEnableBrowseMarks( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnWrapJumpEntries( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnShowToolbar( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnToggleBrowseMarkKey( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnClearAllBrowseMarksKey( wxCommandEvent& event ) { event.Skip(); }
        virtual void OnJumpTrackerSpinCtrl(wxSpinEvent& event) {event.Skip();}

	public:
		wxCheckBox* Cfg_BrowseMarksEnabled;
		wxCheckBox* Cfg_WrapJumpEntries;
		wxCheckBox* Cfg_ShowToolbar;
		wxCheckBox* Cfg_ActivatePrevEd;
        wxSpinCtrl* Cfg_JumpTrackerSpinCtrl;
		wxRadioBox* Cfg_ToggleKey;
		wxStaticText* m_staticText4;
		wxSlider* Cfg_LeftMouseDelay;
		wxRadioBox* Cfg_ClearAllKey;

		ConfigPanel( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 554,569 ), long style = wxTAB_TRAVERSAL );
		~ConfigPanel();

};

#endif //__ConfigPanel__
