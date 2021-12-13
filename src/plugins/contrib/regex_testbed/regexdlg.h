#ifndef REGEXDLG_H
#define REGEXDLG_H

#include <wx/wxprec.h>

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include <set>

//(*Headers(RegExDlg)
#include "scrollingdialog.h"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/html/htmlwin.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include <wx/html/htmlwin.h>

#ifndef CB_PRECOMP
  #include <wx/listctrl.h> // wxListEvent
  #include <wx/choice.h>
  #include <wx/checkbox.h>
  #include <wx/regex.h>
#endif

class RegExDlg: public wxScrollingDialog
{
	public:

		RegExDlg(wxWindow* parent,wxWindowID id = -1);
		virtual ~RegExDlg();

		//(*Identifiers(RegExDlg)
		//*)

		void EndModal(int retCode);

		static void ReleaseAll();

	protected:

		//(*Handlers(RegExDlg)
		void OnClose(wxCloseEvent& event);
		void OnValueChanged(wxCommandEvent& event);
		void OnQuoteChanged(wxCommandEvent& event);
		void OnOptionChanged(wxCommandEvent& event);
		void OnSyntaxSelect(wxCommandEvent& event);
		//*)

	private:

        wxArrayString GetBuiltinMatches(const wxString& text);
        void Reevaluate();
        void ShowError(bool Error);

        //(*Declarations(RegExDlg)
        wxCheckBox* m_newlines;
        wxCheckBox* m_nocase;
        wxChoice* m_syntax;
        wxHtmlWindow* m_output;
        wxStaticText* StaticText4;
        wxTextCtrl* m_quoted;
        wxTextCtrl* m_regex;
        wxTextCtrl* m_text;
        //*)

        typedef std::set<RegExDlg*> VisibleDialogs;
        static VisibleDialogs m_visible_dialogs;

		DECLARE_EVENT_TABLE()
};

#endif
