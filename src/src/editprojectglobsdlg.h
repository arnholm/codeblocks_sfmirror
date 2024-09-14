#ifndef DLGEDITPROJECTGLOB_H
#define DLGEDITPROJECTGLOB_H

//(*Headers(EditProjectGlobsDlg)
#include <wx/bmpbuttn.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include "cbproject.h"


class EditProjectGlobsDlg: public wxDialog
{
	public:

		EditProjectGlobsDlg(const cbProject* prj, const ProjectGlob& glob, wxWindow* parent,wxWindowID id=wxID_ANY,const wxPoint& pos=wxDefaultPosition,const wxSize& size=wxDefaultSize);
		virtual ~EditProjectGlobsDlg();

		//(*Declarations(EditProjectGlobsDlg)
		wxBitmapButton* btnBrowse;
		wxBitmapButton* btnOther;
		wxCheckBox* chkAddToProject;
		wxCheckBox* chkAllNone;
		wxCheckBox* chkRecursive;
		wxCheckListBox* lstTargets;
		wxStaticText* StaticText3;
		wxStdDialogButtonSizer* StdDialogButtonSizer1;
		wxTextCtrl* txtPath;
		wxTextCtrl* txtWildcart;
		//*)

		ProjectGlob WriteGlob();

	protected:

		//(*Identifiers(EditProjectGlobsDlg)
		static const wxWindowID ID_TEXTPATH;
		static const wxWindowID ID_BTN_BROWSE;
		static const wxWindowID ID_BTN_OTHER;
		static const wxWindowID ID_TXT_WILDCART;
		static const wxWindowID ID_CHECK_RECURSIVE;
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_CHK_ALL_NONE;
		static const wxWindowID ID_LST_TARGETS;
		static const wxWindowID ID_CHECK_ADD_TO_PROJECT;
		//*)

	private:

		//(*Handlers(EditProjectGlobsDlg)
		void OnBrowseClick(wxCommandEvent& event);
		void OnOtherClick(wxCommandEvent& event);
		void OntxtPathText(wxCommandEvent& event);
		void OnTargetsToggled(wxCommandEvent& event);
		void OnAllNoneClick(wxCommandEvent& event);
		//*)

		void UpdateTargetCheckBox();

		ProjectGlob m_GlobObj;
		const cbProject* m_Prj;
		wxArrayString m_targets;

		DECLARE_EVENT_TABLE()
};

#endif
