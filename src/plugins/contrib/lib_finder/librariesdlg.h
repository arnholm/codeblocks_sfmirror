#ifndef LIBRARIESDLG_H
#define LIBRARIESDLG_H

//(*Headers(LibrariesDlg)
#include "scrollingdialog.h"
class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxFlatNotebook;
class wxFlexGridSizer;
class wxListBox;
class wxPanel;
class wxStaticBoxSizer;
class wxStaticText;
class wxStdDialogButtonSizer;
class wxTextCtrl;
//*)

#include "resultmap.h"
#include "librarydetectionmanager.h"

class LibrariesDlg: public wxScrollingDialog
{
    public:

        LibrariesDlg(wxWindow* parent,TypedResults& knownLibraries);
        virtual ~LibrariesDlg();

    private:

        //(*Declarations(LibrariesDlg)
        wxButton* Button1;
        wxButton* Button2;
        wxButton* Button3;
        wxButton* Button5;
        wxButton* Button6;
        wxButton* Button7;
        wxButton* Button8;
        wxButton* m_ConfDelete;
        wxButton* m_ConfDuplicate;
        wxButton* m_ConfigDown;
        wxButton* m_ConfigUp;
        wxCheckBox* m_ShowPkgConfig;
        wxCheckBox* m_ShowPredefined;
        wxFlatNotebook* FlatNotebook1;
        wxListBox* m_Configurations;
        wxListBox* m_Libraries;
        wxPanel* Panel10;
        wxPanel* Panel1;
        wxPanel* Panel2;
        wxPanel* Panel3;
        wxPanel* Panel4;
        wxPanel* Panel5;
        wxPanel* Panel6;
        wxPanel* Panel7;
        wxPanel* Panel8;
        wxPanel* Panel9;
        wxStaticText* StaticText10;
        wxStaticText* StaticText11;
        wxStaticText* StaticText1;
        wxStaticText* StaticText2;
        wxStaticText* StaticText3;
        wxStaticText* StaticText4;
        wxStaticText* StaticText5;
        wxStaticText* StaticText6;
        wxStaticText* StaticText7;
        wxStaticText* StaticText8;
        wxStaticText* StaticText9;
        wxStaticText* m_Type;
        wxTextCtrl* m_BasePath;
        wxTextCtrl* m_CFlags;
        wxTextCtrl* m_Categories;
        wxTextCtrl* m_CompilerDirs;
        wxTextCtrl* m_Compilers;
        wxTextCtrl* m_Defines;
        wxTextCtrl* m_Description;
        wxTextCtrl* m_Headers;
        wxTextCtrl* m_LFlags;
        wxTextCtrl* m_Libs;
        wxTextCtrl* m_LinkerDir;
        wxTextCtrl* m_Name;
        wxTextCtrl* m_ObjectsDir;
        wxTextCtrl* m_PkgConfigName;
        wxTextCtrl* m_Required;
        //*)

        //(*Identifiers(LibrariesDlg)
        static const wxWindowID ID_LISTBOX1;
        static const wxWindowID ID_CHECKBOX1;
        static const wxWindowID ID_CHECKBOX2;
        static const wxWindowID ID_BUTTON1;
        static const wxWindowID ID_BUTTON2;
        static const wxWindowID ID_BUTTON11;
        static const wxWindowID ID_BUTTON8;
        static const wxWindowID ID_LISTBOX2;
        static const wxWindowID ID_BUTTON9;
        static const wxWindowID ID_BUTTON10;
        static const wxWindowID ID_BUTTON3;
        static const wxWindowID ID_BUTTON4;
        static const wxWindowID ID_STATICTEXT10;
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_STATICTEXT9;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID ID_STATICTEXT5;
        static const wxWindowID ID_TEXTCTRL4;
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_TEXTCTRL3;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_TEXTCTRL2;
        static const wxWindowID ID_PANEL1;
        static const wxWindowID ID_TEXTCTRL13;
        static const wxWindowID ID_PANEL8;
        static const wxWindowID ID_TEXTCTRL5;
        static const wxWindowID ID_PANEL6;
        static const wxWindowID ID_TEXTCTRL8;
        static const wxWindowID ID_PANEL3;
        static const wxWindowID ID_TEXTCTRL12;
        static const wxWindowID ID_PANEL5;
        static const wxWindowID ID_STATICTEXT6;
        static const wxWindowID ID_TEXTCTRL9;
        static const wxWindowID ID_BUTTON5;
        static const wxWindowID ID_STATICTEXT7;
        static const wxWindowID ID_TEXTCTRL10;
        static const wxWindowID ID_BUTTON6;
        static const wxWindowID ID_STATICTEXT8;
        static const wxWindowID ID_TEXTCTRL11;
        static const wxWindowID ID_BUTTON7;
        static const wxWindowID ID_PANEL4;
        static const wxWindowID ID_TEXTCTRL6;
        static const wxWindowID ID_PANEL7;
        static const wxWindowID ID_TEXTCTRL7;
        static const wxWindowID ID_PANEL2;
        static const wxWindowID ID_STATICTEXT11;
        static const wxWindowID ID_TEXTCTRL14;
        static const wxWindowID ID_PANEL9;
        static const wxWindowID ID_STATICTEXT12;
        static const wxWindowID ID_TEXTCTRL15;
        static const wxWindowID ID_PANEL10;
        static const wxWindowID ID_FLATNOTEBOOK1;
        //*)

        //(*Handlers(LibrariesDlg)
        void OnInit(wxInitDialogEvent& event);
        void OnButton8Click(wxCommandEvent& event);
        void Onm_ShowPredefinedClick(wxCommandEvent& event);
        void Onm_ShowPkgConfigClick(wxCommandEvent& event);
        void Onm_LibrariesSelect(wxCommandEvent& event);
        void Onm_ConfDeleteClick(wxCommandEvent& event);
        void OnWrite(wxCommandEvent& event);
        void Onm_ConfigurationsSelect(wxCommandEvent& event);
        void Onm_ConfDuplicateClick(wxCommandEvent& event);
        void Onm_NameText(wxCommandEvent& event);
        void Onm_CompilersText(wxCommandEvent& event);
        void OnButton1Click(wxCommandEvent& event);
        void OnButton2Click(wxCommandEvent& event);
        void Onm_ConfigPosChangeDown(wxCommandEvent& event);
        void Onm_ConfigPosChangeUp(wxCommandEvent& event);
        void OnButton3Click(wxCommandEvent& event);
        //*)

        TypedResults& m_KnownLibraries;
        TypedResults m_WorkingCopy;
        wxString m_SelectedShortcut;
        LibraryResult* m_SelectedConfig;
        bool m_WhileUpdating;

        bool LoadSearchFilters(LibraryDetectionManager* CfgManager);
        void RecreateLibrariesList(const wxString& Selection);
        void RecreateLibrariesListForceRefresh();
        void SelectLibrary(const wxString& Shortcut);
        void SelectConfiguration(LibraryResult* Configuration);
        void CopyFromKnown();
        void CopyToKnown();
        wxString GetDesc(LibraryResult* Configuration);
        void StoreConfiguration();
        void RefreshConfigurationName();

        DECLARE_EVENT_TABLE()
};

#endif
