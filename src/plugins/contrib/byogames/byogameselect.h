#ifndef BYOGAMESELECT_H
#define BYOGAMESELECT_H

#include "scrollingdialog.h"

class wxBoxSizer;
class wxStaticText;
class wxPanel;
class wxStaticBoxSizer;
class wxListBox;
class wxButton;
class wxStaticLine;
class wxCommandEvent;

class byoGameSelect: public wxScrollingDialog
{
	public:

		byoGameSelect(wxWindow* parent,wxWindowID id = -1);
		virtual ~byoGameSelect();

		//(*Identifiers(byoGameSelect)
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_PANEL1;
		static const wxWindowID ID_LISTBOX1;
		static const wxWindowID ID_STATICLINE1;
		//*)

	protected:

		//(*Handlers(byoGameSelect)
		void OnCancel(wxCommandEvent& event);
		void OnPlay(wxCommandEvent& event);
		//*)

		//(*Declarations(byoGameSelect)
		wxBoxSizer* BoxSizer1;
		wxBoxSizer* BoxSizer2;
		wxBoxSizer* BoxSizer3;
		wxBoxSizer* BoxSizer4;
		wxButton* Button1;
		wxButton* Button2;
		wxListBox* m_GamesList;
		wxPanel* Panel1;
		wxStaticBoxSizer* StaticBoxSizer1;
		wxStaticLine* StaticLine1;
		wxStaticText* StaticText1;
		//*)

	private:

		DECLARE_EVENT_TABLE()
};

#endif // BYOGAMESELECT_H
