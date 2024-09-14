#ifndef CONFIGPANEL_H
#define CONFIGPANEL_H

//(*Headers(ConfigPanel)
#include <wx/panel.h>
class wxBoxSizer;
class wxButton;
class wxChoice;
class wxFlexGridSizer;
class wxHyperlinkCtrl;
class wxStaticLine;
class wxStaticText;
class wxTextCtrl;
//*)

#include <wx/string.h>

#include <configurationpanel.h>

class CppCheck;

class ConfigPanel : public cbConfigurationPanel
{
public:

    ConfigPanel(wxWindow* parent);
    virtual ~ConfigPanel();

    //(*Declarations(ConfigPanel)
    wxChoice* choOperation;
    wxStaticLine* StaticLine1;
    wxStaticLine* StaticLine2;
    wxTextCtrl* txtCppCheckApp;
    wxTextCtrl* txtCppCheckArgs;
    wxTextCtrl* txtVeraApp;
    wxTextCtrl* txtVeraArgs;
    //*)

    /// @return the panel's title.
    virtual wxString GetTitle() const { return _("CppCheck/Vera++"); }
    /// @return the panel's bitmap base name. You must supply two bitmaps: \<basename\>.png and \<basename\>-off.png...
    virtual wxString GetBitmapBaseName() const { return wxT("CppCheck"); }
    /// Called when the user chooses to apply the configuration.
    virtual void OnApply();
    /// Called when the user chooses to cancel the configuration.
    virtual void OnCancel() { ; }

    static wxString GetDefaultCppCheckExecutableName();
    static wxString GetDefaultVeraExecutableName();
protected:

    //(*Identifiers(ConfigPanel)
    static const wxWindowID ID_TXT_CPP_CHECK_APP;
    static const wxWindowID ID_BTN_CPPCHECK_APP;
    static const wxWindowID ID_TXT_CPP_CHECK_ARGS;
    static const wxWindowID ID_HYC_CPP_CHECK_WWW;
    static const wxWindowID ID_TXT_VERA_APP;
    static const wxWindowID ID_BTN_VERA;
    static const wxWindowID ID_TXT_VERA_ARGS;
    static const wxWindowID ID_HYC_VERA_WWW;
    static const wxWindowID ID_STATICLINE1;
    static const wxWindowID ID_STATICLINE2;
    static const wxWindowID ID_CHO_OPERATION;
    //*)

private:

    //(*Handlers(ConfigPanel)
    void OnCppCheckApp(wxCommandEvent& event);
    void OnVeraApp(wxCommandEvent& event);
    //*)

    DECLARE_EVENT_TABLE()
};

#endif
