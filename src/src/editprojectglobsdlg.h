#ifndef DLGEDITPROJECTGLOB_H
#define DLGEDITPROJECTGLOB_H

//(*Headers(EditProjectGlobsDlg)
#include <wx/bmpbuttn.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include "cbproject.h"


class EditProjectGlobsDlg: public wxDialog
{
	public:

		EditProjectGlobsDlg(const ProjectGlob& glob, wxWindow* parent,wxWindowID id=wxID_ANY,const wxPoint& pos=wxDefaultPosition,const wxSize& size=wxDefaultSize);
		virtual ~EditProjectGlobsDlg();

		//(*Declarations(EditProjectGlobsDlg)
		wxBitmapButton* btnBrowse;
		wxBitmapButton* btnOther;
		wxCheckBox* chkRecursive;
		wxStdDialogButtonSizer* StdDialogButtonSizer1;
		wxTextCtrl* txtPath;
		wxTextCtrl* txtWildcart;
		//*)

		ProjectGlob WriteGlob();

	protected:

		//(*Identifiers(EditProjectGlobsDlg)
		static const long ID_TEXTPATH;
		static const long ID_BTN_BROWSE;
		static const long ID_BTN_OTHER;
		static const long ID_CHECK_RECURSIVE;
		static const long ID_TXT_WILDCART;
		//*)

	private:

		//(*Handlers(EditProjectGlobsDlg)
		void OnBrowseClick(wxCommandEvent& event);
		void OnOtherClick(wxCommandEvent& event);
		void OntxtPathText(wxCommandEvent& event);
		//*)

		ProjectGlob m_GlobObj;

		DECLARE_EVENT_TABLE()
};

#endif
