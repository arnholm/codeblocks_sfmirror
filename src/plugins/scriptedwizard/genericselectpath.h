/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef GENERICSELECTPATH_H
#define GENERICSELECTPATH_H


//(*HeadersPCH(GenericSelectPath)
#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

class wxString;

class GenericSelectPath: public wxPanel
{
	public:

		GenericSelectPath(wxWindow* parent,wxWindowID id = -1);
		virtual ~GenericSelectPath();

		// use this because it adjusts the sizer too
		void SetDescription(const wxString& descr)
		{
		    lblDescr->SetLabel(descr);

            GetSizer()->Fit(this);
            GetSizer()->SetSizeHints(this);
		}

		//(*Identifiers(GenericSelectPath)
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_STATICTEXT2;
		static const wxWindowID ID_TEXTCTRL1;
		static const wxWindowID ID_BUTTON1;
		//*)

		//(*Handlers(GenericSelectPath)
		//*)

		//(*Declarations(GenericSelectPath)
		wxBoxSizer* BoxSizer1;
		wxBoxSizer* BoxSizer2;
		wxButton* btnBrowse;
		wxStaticText* lblDescr;
		wxStaticText* lblLabel;
		wxTextCtrl* txtFolder;
		//*)

	private:

		DECLARE_EVENT_TABLE()
};

#endif
