#ifndef DEFSDOWNLOADDLG_H
#define DEFSDOWNLOADDLG_H

//(*Headers(DefsDownloadDlg)
#include "scrollingdialog.h"
class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxListBox;
class wxStaticBoxSizer;
class wxStaticText;
class wxTextCtrl;
class wxTreeCtrl;
class wxTreeEvent;
//*)


class DefsDownloadDlg: public wxScrollingDialog
{
	public:

		DefsDownloadDlg(wxWindow* parent);
		virtual ~DefsDownloadDlg();

	private:

		//(*Declarations(DefsDownloadDlg)
		wxButton* Button1;
		wxButton* m_Add;
		wxButton* m_Remove;
		wxCheckBox* m_Tree;
		wxListBox* m_UsedLibraries;
		wxStaticText* StaticText1;
		wxTextCtrl* m_Filter;
		wxTreeCtrl* m_KnownLibrariesTree;
		//*)

		//(*Identifiers(DefsDownloadDlg)
		static const wxWindowID ID_LISTBOX1;
		static const wxWindowID ID_BUTTON1;
		static const wxWindowID ID_BUTTON2;
		static const wxWindowID ID_TREECTRL1;
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_TEXTCTRL2;
		static const wxWindowID ID_CHECKBOX1;
		static const wxWindowID ID_BUTTON3;
		//*)

		//(*Handlers(DefsDownloadDlg)
		//*)

		void FetchList();

		DECLARE_EVENT_TABLE()
};

#endif
