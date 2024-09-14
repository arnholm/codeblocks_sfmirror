#ifndef DEBUGGERSETTINGSCOMMONPANEL_H
#define DEBUGGERSETTINGSCOMMONPANEL_H

#ifndef CB_PRECOMP
	//(*HeadersPCH(DebuggerSettingsCommonPanel)
	#include <wx/button.h>
	#include <wx/checkbox.h>
	#include <wx/choice.h>
	#include <wx/panel.h>
	#include <wx/sizer.h>
	#include <wx/stattext.h>
	//*)
#endif
//(*Headers(DebuggerSettingsCommonPanel)
//*)

class DebuggerSettingsCommonPanel: public wxPanel
{
	public:

		DebuggerSettingsCommonPanel(wxWindow* parent);
		virtual ~DebuggerSettingsCommonPanel();

		void SaveChanges();

	private:
	    void UpdateValueTooltipFont();
	private:

		//(*Declarations(DebuggerSettingsCommonPanel)
		wxCheckBox* m_autoBuild;
		wxCheckBox* m_autoSwitch;
		wxCheckBox* m_debuggersLog;
		wxCheckBox* m_jumpOnDoubleClick;
		wxCheckBox* m_requireCtrlForTooltips;
		wxChoice* m_perspective;
		wxStaticText* m_valueTooltipLabel;
		//*)
    private:
        wxString m_valueTooltipFontInfo;
    private:

		//(*Identifiers(DebuggerSettingsCommonPanel)
		static const wxWindowID ID_AUTOBUILD;
		static const wxWindowID ID_AUTOSWITCH;
		static const wxWindowID ID_DEBUGGERS_LOG;
		static const wxWindowID ID_JUMP_ON_DOUBLE_CLICK;
		static const wxWindowID ID_REQUIRE_CTRL_FOR_TOOLTIPS;
		static const wxWindowID ID_VALUE_TOOLTIP_LABEL;
		static const wxWindowID ID_BUTTON_CHOOSE_FONT;
		static const wxWindowID ID_CHOICE_PERSPECTIVE;
		//*)

		//(*Handlers(DebuggerSettingsCommonPanel)
		void OnChooseFontClick(wxCommandEvent& event);
		//*)

		DECLARE_EVENT_TABLE()
};

#endif
