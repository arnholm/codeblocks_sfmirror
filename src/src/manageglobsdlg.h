#ifndef DLGMANAGEBLOB_H
#define DLGMANAGEBLOB_H

//(*Headers(dlgmanageblob)
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
//*)

class ManageGlobsDlg: public wxDialog
{
	public:

		ManageGlobsDlg(cbProject* prj, wxWindow* parent,wxWindowID id=wxID_ANY,const wxPoint& pos=wxDefaultPosition,const wxSize& size=wxDefaultSize);
		virtual ~ManageGlobsDlg();

		//(*Declarations(ManageGlobsDlg)
		wxButton* btnAdd;
		wxButton* btnDelete;
		wxButton* btnEdit;
		wxButton* btnOk;
		wxListCtrl* m_ListGlobs;
		//*)

	protected:

		//(*Identifiers(ManageGlobsDlg)
		static const long ID_LISTCTRL;
		static const long ID_BUTTON_ADD;
		static const long ID_BUTTON_DELETE;
		static const long ID_BUTTON_EDIT;
		//*)

	private:

		//(*Handlers(ManageGlobsDlg)
		void OnAddClick(wxCommandEvent& event);
		void OnDeleteClick(wxCommandEvent& event);
		void OnEditClick(wxCommandEvent& event);
		void OnOkClick(wxCommandEvent& event);
		void OnlstGlobsListItemSelect(wxListEvent& event);
		void OnlstGlobsListItemDeselect(wxListEvent& event);
		void OnlstGlobsListItemDeselect1(wxListEvent& event);
		void OnlstGlobsListItemActivated(wxListEvent& event);
		//*)

		void PopulateList();
		bool GlobsChanged();
		void EditSelectedItem();

		std::vector<ProjectGlob> m_GlobList;

		DECLARE_EVENT_TABLE()
		cbProject* m_Prj;
		bool m_GlobsChanged;
};

#endif
