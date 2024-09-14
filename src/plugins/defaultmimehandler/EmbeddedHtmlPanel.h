/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef EMBEDDEDHTMLPANEL_H
#define EMBEDDEDHTMLPANEL_H

#ifndef CB_PRECOMP
    //(*HeadersPCH(EmbeddedHtmlPanel)
    #include <wx/panel.h>
    class wxBitmapButton;
    class wxBoxSizer;
    class wxStaticText;
    //*)
#endif
//(*Headers(EmbeddedHtmlPanel)
class wxHtmlWindow;
//*)

class wxHtmlLinkEvent;

class EmbeddedHtmlPanel: public wxPanel
{
    public:

        EmbeddedHtmlPanel(wxWindow* parent);
        virtual ~EmbeddedHtmlPanel();

        void Open(const wxString& url);

        //(*Declarations(EmbeddedHtmlPanel)
        wxBitmapButton* btnBack;
        wxBitmapButton* btnForward;
        wxHtmlWindow* winHtml;
        wxPanel* Panel1;
        wxStaticText* lblStatus;
        //*)

    protected:

        //(*Identifiers(EmbeddedHtmlPanel)
        static const wxWindowID ID_BITMAPBUTTON2;
        static const wxWindowID ID_BITMAPBUTTON3;
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_PANEL1;
        static const wxWindowID ID_HTMLWINDOW1;
        //*)

    private:
        void OnUpdateUI(wxUpdateUIEvent& event);
        void OnLinkClicked(wxHtmlLinkEvent &event);

        //(*Handlers(EmbeddedHtmlPanel)
        void OnbtnBackClick(wxCommandEvent& event);
        void OnbtnForwardClick(wxCommandEvent& event);
        //*)

        DECLARE_EVENT_TABLE()
};

#endif
