/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef PROJECTPATHPANEL_H
#define PROJECTPATHPANEL_H


//(*HeadersPCH(ProjectPathPanel)
#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include <wx/string.h>

class ProjectPathPanel: public wxPanel
{
    public:

        ProjectPathPanel(wxWindow* parent,wxWindowID id = -1);
        ~ProjectPathPanel() override;

        void SetPath(const wxString& path)
        {
            txtPrjPath->SetValue(path);
//            if (!path.IsEmpty())
//                txtPrjName->SetFocus();
            Update();
        }
        wxString GetPath() const { return txtPrjPath->GetValue(); }
        //wxString GetName(){ return txtPrjName->GetValue(); }
// NOTE (Biplab#1#): This is a temporary fix. This function
// need to be renamed according to it's visual representation
        wxString GetName() const override { return txtPrjTitle->GetValue(); }
        wxString GetFullFileName() const { return txtFinalDir->GetValue(); }
        wxString GetTitle() const { return txtPrjTitle->GetValue(); }

        //(*Identifiers(ProjectPathPanel)
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_TEXTCTRL3;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID ID_BUTTON1;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_TEXTCTRL2;
        static const wxWindowID ID_STATICTEXT5;
        static const wxWindowID ID_TEXTCTRL4;
        //*)

    private:

        void Update() override;
        void UpdateFromResulting();
        bool m_LockUpdates;

        //(*Handlers(ProjectPathPanel)
        void OnFullPathChanged(wxCommandEvent& event);
        void OntxtFinalDirText(wxCommandEvent& event);
        void OntxtPrjTitleText(wxCommandEvent& event);
        //*)

        //(*Declarations(ProjectPathPanel)
        wxBoxSizer* BoxSizer1;
        wxBoxSizer* BoxSizer2;
        wxButton* btnPrjPathBrowse;
        wxTextCtrl* txtFinalDir;
        wxTextCtrl* txtPrjName;
        wxTextCtrl* txtPrjPath;
        wxTextCtrl* txtPrjTitle;
        //*)

        DECLARE_EVENT_TABLE()
};

#endif
