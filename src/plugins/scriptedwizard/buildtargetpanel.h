/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef BUILDTARGETPANEL_H
#define BUILDTARGETPANEL_H


//(*HeadersPCH(BuildTargetPanel)
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include <wx/string.h>

class BuildTargetPanel: public wxPanel
{
    public:

        BuildTargetPanel(wxWindow* parent,wxWindowID id = -1);
        virtual ~BuildTargetPanel();

        void ShowCompiler(bool show)
        {
            lblCompiler->Show(show);
            cmbCompiler->Show(show);
        }
        void SetTargetName(const wxString& name)
        {
            txtName->SetValue(name);
        }
        void SetEnableDebug(bool debug)
        {
            chkEnableDebug->SetValue(debug);
        }

        wxComboBox* GetCompilerCombo(){ return cmbCompiler; }
        bool GetEnableDebug() const { return chkEnableDebug->IsChecked(); }
        wxString GetTargetName() const { return txtName->GetValue(); }
        wxString GetOutputDir() const { return txtOut->GetValue(); }
        wxString GetObjectOutputDir() const { return txtObjOut->GetValue(); }


        //(*Identifiers(BuildTargetPanel)
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_COMBOBOX1;
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_TEXTCTRL2;
        static const wxWindowID ID_STATICTEXT5;
        static const wxWindowID ID_TEXTCTRL3;
        static const wxWindowID ID_CHECKBOX1;
        //*)

    private:

        //(*Handlers(BuildTargetPanel)
        void OntxtNameText(wxCommandEvent& event);
        //*)

        //(*Declarations(BuildTargetPanel)
        wxBoxSizer* BoxSizer1;
        wxCheckBox* chkEnableDebug;
        wxComboBox* cmbCompiler;
        wxStaticText* StaticText3;
        wxStaticText* lblCompiler;
        wxTextCtrl* txtName;
        wxTextCtrl* txtObjOut;
        wxTextCtrl* txtOut;
        //*)

        DECLARE_EVENT_TABLE()
};

#endif
