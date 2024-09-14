/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERPANEL_H
#define COMPILERPANEL_H

#include <wx/string.h>

//(*HeadersPCH(CompilerPanel)
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

class CompilerPanel: public wxPanel
{
	public:

		CompilerPanel(wxWindow* parent, wxWindow* parentDialog);
		virtual ~CompilerPanel();

        wxComboBox* GetCompilerCombo(){ return cmbCompiler; }
        void EnableConfigurationTargets(bool en);

        void SetWantDebug(bool want){ chkConfDebug->SetValue(want); }
        bool GetWantDebug() const { return chkConfDebug->IsChecked() && chkConfDebug->IsShown(); }
        void SetDebugName(const wxString& name){ txtDbgName->SetValue(name); }
        wxString GetDebugName() const { return txtDbgName->GetValue(); }
        void SetDebugOutputDir(const wxString& dir){ txtDbgOut->SetValue(dir); }
        wxString GetDebugOutputDir() const { return txtDbgOut->GetValue(); }
        void SetDebugObjectOutputDir(const wxString& dir){ txtDbgObjOut->SetValue(dir); }
        wxString GetDebugObjectOutputDir() const { return txtDbgObjOut->GetValue(); }

        void SetWantRelease(bool want){ chkConfRelease->SetValue(want); }
        bool GetWantRelease() const { return chkConfRelease->IsChecked() && chkConfRelease->IsShown(); }
        void SetReleaseName(const wxString& name){ txtRelName->SetValue(name); }
        wxString GetReleaseName() const { return txtRelName->GetValue(); }
        void SetReleaseOutputDir(const wxString& dir){ txtRelOut->SetValue(dir); }
        wxString GetReleaseOutputDir() const { return txtRelOut->GetValue(); }
        void SetReleaseObjectOutputDir(const wxString& dir){ txtRelObjOut->SetValue(dir); }
        wxString GetReleaseObjectOutputDir() const { return txtRelObjOut->GetValue(); }

		//(*Identifiers(CompilerPanel)
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_STATICTEXT2;
		static const wxWindowID ID_COMBOBOX1;
		static const wxWindowID ID_CHECKBOX1;
		static const wxWindowID ID_TEXTCTRL3;
		static const wxWindowID ID_STATICTEXT3;
		static const wxWindowID ID_TEXTCTRL1;
		static const wxWindowID ID_STATICTEXT4;
		static const wxWindowID ID_TEXTCTRL2;
		static const wxWindowID ID_CHECKBOX3;
		static const wxWindowID ID_TEXTCTRL4;
		static const wxWindowID ID_STATICTEXT7;
		static const wxWindowID ID_TEXTCTRL5;
		static const wxWindowID ID_STATICTEXT8;
		static const wxWindowID ID_TEXTCTRL6;
		//*)

	private:

		//(*Handlers(CompilerPanel)
		void OnDebugChange(wxCommandEvent& event);
		void OnReleaseChange(wxCommandEvent& event);
		//*)

		//(*Declarations(CompilerPanel)
		wxBoxSizer* BoxSizer4;
		wxBoxSizer* BoxSizer5;
		wxCheckBox* chkConfDebug;
		wxCheckBox* chkConfRelease;
		wxComboBox* cmbCompiler;
		wxStaticBoxSizer* StaticBoxSizer1;
		wxStaticBoxSizer* StaticBoxSizer2;
		wxStaticText* StaticText1;
		wxTextCtrl* txtDbgName;
		wxTextCtrl* txtDbgObjOut;
		wxTextCtrl* txtDbgOut;
		wxTextCtrl* txtRelName;
		wxTextCtrl* txtRelObjOut;
		wxTextCtrl* txtRelOut;
		//*)

        wxWindow* m_parentDialog;

		DECLARE_EVENT_TABLE()
};

#endif
