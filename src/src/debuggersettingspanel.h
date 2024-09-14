#ifndef DEBUGGERSETTINGSPANEL_H
#define DEBUGGERSETTINGSPANEL_H

#ifndef CB_PRECOMP
	//(*HeadersPCH(DebuggerSettingsPanel)
	#include <wx/button.h>
	#include <wx/panel.h>
	#include <wx/sizer.h>
	#include <wx/textctrl.h>
	//*)
#endif
//(*Headers(DebuggerSettingsPanel)
//*)

class cbDebuggerPlugin;
class DebuggerSettingsDlg;

class DebuggerSettingsPanel: public wxPanel
{
	public:

		DebuggerSettingsPanel(wxWindow* parent, DebuggerSettingsDlg *dialog, cbDebuggerPlugin *plugin);
		virtual ~DebuggerSettingsPanel();

	private:

		//(*Declarations(DebuggerSettingsPanel)
		//*)

		//(*Identifiers(DebuggerSettingsPanel)
		static const wxWindowID ID_BUTTON_CREATE;
		static const wxWindowID ID_BUTTON_DELETE;
		static const wxWindowID ID_BUTTON_RESET;
		static const wxWindowID ID_TEXTCTRL_INFO;
		//*)
    private:

		//(*Handlers(DebuggerSettingsPanel)
		void OnButtonCreate(wxCommandEvent& event);
		void OnButtonDelete(wxCommandEvent& event);
		void OnButtonReset(wxCommandEvent& event);
		//*)

    private:
        DebuggerSettingsDlg *m_dialog;
        cbDebuggerPlugin *m_plugin;
    private:
		DECLARE_EVENT_TABLE()
};

#endif // DEBUGGERSETTINGSPANEL_H
