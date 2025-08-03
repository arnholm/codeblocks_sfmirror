/*
* This file is part of wxSmith plugin for Code::Blocks Studio
* Copyright (C) 2006-2007  Bartlomiej Swiecki
*
* wxSmith is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* wxSmith is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with wxSmith. If not, see <http://www.gnu.org/licenses/>.
*
* $Revision$
* $Id$
* $HeadURL$
*/

#include "wxschoicebook.h"
#include "../../wxsadvqppchild.h"
#include "../wxsitemresdata.h"
#include <wx/choicebk.h>

#include <prep.h>

//(*Headers(wxsChoicebookParentQP)
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
//*)

//(*InternalHeaders(wxsChoicebookParentQP)
#include <wx/intl.h>
#include <wx/string.h>
//*)

namespace
{
    wxsRegisterItem<wxsChoicebook> Reg(_T("Choicebook"),wxsTContainer,_T("Standard"),300);

    /** \brief Extra parameters for notebook's children */
    class wxsChoicebookExtra: public wxsPropertyContainer
    {
        public:

            wxsChoicebookExtra():
                m_Label(_("Page name")),
                m_Selected(false)
            {}

            wxString m_Label;
            bool m_Selected;

        protected:

            virtual void OnEnumProperties(cb_unused long _Flags)
            {
                WXS_SHORT_STRING(wxsChoicebookExtra,m_Label,_("Page name"),_T("label"),_T(""),false);
                WXS_BOOL(wxsChoicebookExtra,m_Selected,_("Page selected"),_T("selected"),false);
            }
    };

    /** \brief Inernal Quick properties panel */
    class wxsChoicebookParentQP: public wxsAdvQPPChild
    {
        public:

            wxsChoicebookParentQP(wxsAdvQPP* parent,wxsChoicebookExtra* Extra,wxWindowID id = -1):
                wxsAdvQPPChild(parent,_("Choicebook")),
                m_Extra(Extra)
            {
                //(*Initialize(wxsChoicebookParentQP)
                wxFlexGridSizer* FlexGridSizer1;
                wxStaticBoxSizer* StaticBoxSizer1;
                wxStaticBoxSizer* StaticBoxSizer2;

                Create(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("id"));
                FlexGridSizer1 = new wxFlexGridSizer(0, 1, 0, 0);
                StaticBoxSizer1 = new wxStaticBoxSizer(wxVERTICAL, this, _("Label"));
                Label = new wxTextCtrl(this, ID_TEXTCTRL1, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TEXTCTRL1"));
                StaticBoxSizer1->Add(Label, 0, wxEXPAND, 5);
                FlexGridSizer1->Add(StaticBoxSizer1, 1, wxEXPAND, 5);
                StaticBoxSizer2 = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Selection"));
                Selected = new wxCheckBox(this, ID_CHECKBOX1, _("Selected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX1"));
                Selected->SetValue(false);
                StaticBoxSizer2->Add(Selected, 1, wxEXPAND, 5);
                FlexGridSizer1->Add(StaticBoxSizer2, 1, wxEXPAND, 5);
                SetSizer(FlexGridSizer1);
                FlexGridSizer1->SetSizeHints(this);

                Connect(ID_TEXTCTRL1,wxEVT_COMMAND_TEXT_ENTER,wxCommandEventHandler(wxsChoicebookParentQP::OnLabelText));
                Connect(ID_CHECKBOX1,wxEVT_COMMAND_CHECKBOX_CLICKED,wxCommandEventHandler(wxsChoicebookParentQP::OnSelectionChange));
                //*)
                ReadData();

                Label->Connect(-1,wxEVT_KILL_FOCUS,(wxObjectEventFunction)&wxsChoicebookParentQP::OnLabelKillFocus,nullptr,this);
            }

            virtual ~wxsChoicebookParentQP()
            {
                //(*Destroy(wxsChoicebookParentQP)
                //*)
            }

        private:

            virtual void Update()
            {
                ReadData();
            }

            void ReadData()
            {
                if ( !GetPropertyContainer() || !m_Extra ) return;
                Label->SetValue(m_Extra->m_Label);
                Selected->SetValue(m_Extra->m_Selected);
            }

            void SaveData()
            {
                if ( !GetPropertyContainer() || !m_Extra ) return;
                m_Extra->m_Label = Label->GetValue();
                m_Extra->m_Selected = Selected->GetValue();
                NotifyChange();
            }

            //(*Identifiers(wxsChoicebookParentQP)
            static const wxWindowID ID_TEXTCTRL1;
            static const wxWindowID ID_CHECKBOX1;
            //*)

            //(*Handlers(wxsChoicebookParentQP)
            void OnLabelText(wxCommandEvent& event);
            void OnLabelKillFocus(wxFocusEvent& event);
            void OnSelectionChange(wxCommandEvent& event);
            //*)

            //(*Declarations(wxsChoicebookParentQP)
            wxCheckBox* Selected;
            wxTextCtrl* Label;
            //*)

            wxsChoicebookExtra* m_Extra;

            DECLARE_EVENT_TABLE()
    };

    //(*IdInit(wxsChoicebookParentQP)
    const wxWindowID wxsChoicebookParentQP::ID_TEXTCTRL1 = wxNewId();
    const wxWindowID wxsChoicebookParentQP::ID_CHECKBOX1 = wxNewId();
    //*)

    BEGIN_EVENT_TABLE(wxsChoicebookParentQP,wxPanel)
        //(*EventTable(wxsChoicebookParentQP)
        //*)
    END_EVENT_TABLE()

    void wxsChoicebookParentQP::OnLabelText(cb_unused wxCommandEvent& event)       { SaveData(); }
    void wxsChoicebookParentQP::OnLabelKillFocus(cb_unused wxFocusEvent& event)    { SaveData(); event.Skip(); }
    void wxsChoicebookParentQP::OnSelectionChange(cb_unused wxCommandEvent& event) { SaveData(); }

    WXS_ST_BEGIN(wxsChoicebookStyles,_T(""))
        WXS_ST_CATEGORY("wxChoicebook")
        // TODO: XRC in wx 2.8 uses wxBK_ prefix, docs stay at wxCHB_
        WXS_ST(wxCHB_DEFAULT)
        WXS_ST(wxCHB_LEFT)
        WXS_ST(wxCHB_RIGHT)
        WXS_ST(wxCHB_TOP)
        WXS_ST(wxCHB_BOTTOM)
        WXS_ST_DEFAULTS()
    WXS_ST_END()

    WXS_EV_BEGIN(wxsChoicebookEvents)
        WXS_EVI(EVT_CHOICEBOOK_PAGE_CHANGED,wxEVT_COMMAND_CHOICEBOOK_PAGE_CHANGED,wxChoicebookEvent,PageChanged)
        WXS_EVI(EVT_CHOICEBOOK_PAGE_CHANGING,wxEVT_COMMAND_CHOICEBOOK_PAGE_CHANGING,wxChoicebookEvent,PageChanging)
    WXS_EV_END()

}


wxsChoicebook::wxsChoicebook(wxsItemResData* Data):
    wxsContainer(
        Data,
        &Reg.Info,
        wxsChoicebookEvents,
        wxsChoicebookStyles),
    m_CurrentSelection(nullptr)
{
}

void wxsChoicebook::OnEnumContainerProperties(cb_unused long _Flags)
{
}

bool wxsChoicebook::OnCanAddChild(wxsItem* Item,bool ShowMessage)
{
    if ( Item->GetType() == wxsTSizer )
    {
        if ( ShowMessage )
        {
            wxMessageBox(_("Can not add sizer into Choicebook.\nAdd panels first"));
        }
        return false;
    }

    return wxsContainer::OnCanAddChild(Item,ShowMessage);
}

wxsPropertyContainer* wxsChoicebook::OnBuildExtra()
{
    return new wxsChoicebookExtra();
}

wxString wxsChoicebook::OnXmlGetExtraObjectClass()
{
    return _T("choicebookpage");
}

void wxsChoicebook::OnAddChildQPP(wxsItem* Child,wxsAdvQPP* QPP)
{
    wxsChoicebookExtra* CBExtra = (wxsChoicebookExtra*)GetChildExtra(GetChildIndex(Child));
    if ( CBExtra )
    {
        QPP->Register(new wxsChoicebookParentQP(QPP,CBExtra),_("Choicebook"));
    }
}

wxObject* wxsChoicebook::OnBuildPreview(wxWindow* Parent,long _Flags)
{
    UpdateCurrentSelection();
    wxChoicebook* Choicebook = new wxChoicebook(Parent,-1,Pos(Parent),Size(Parent),Style());

    if ( !GetChildCount() && !(_Flags & pfExact) )
    {
        // Adding additional empty notebook to prevent from having zero-sized notebook
        Choicebook->AddPage(
            new wxPanel(Choicebook,-1,wxDefaultPosition,wxSize(50,50)),
            _("No pages"));
    }

    AddChildrenPreview(Choicebook,_Flags);

    for ( int i=0; i<GetChildCount(); i++ )
    {
        wxsItem* Child = GetChild(i);
        wxsChoicebookExtra* CBExtra = (wxsChoicebookExtra*)GetChildExtra(i);

        wxWindow* ChildPreview = wxDynamicCast(GetChild(i)->GetLastPreview(),wxWindow);
        if ( !ChildPreview ) continue;

        bool Selected = (Child == m_CurrentSelection);
        if ( _Flags & pfExact ) Selected = CBExtra->m_Selected;

        Choicebook->AddPage(ChildPreview,CBExtra->m_Label,Selected);
    }

    return Choicebook;
}

void wxsChoicebook::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/choicebk.h>"),GetInfo().ClassName,0);
            AddHeader(_T("<wx/notebook.h>"),_T("wxNotebookEvent"),0);
            Codef(_T("%C(%W, %I, %P, %S, %T, %N);\n"));
            BuildSetupWindowCode();
            AddChildrenCode();

            for ( int i=0; i<GetChildCount(); i++ )
            {
                wxsChoicebookExtra* CBExtra = (wxsChoicebookExtra*)GetChildExtra(i);
                Codef(_T("%AAddPage(%o, %t, %b);\n"),i,CBExtra->m_Label.wx_str(),CBExtra->m_Selected);
            }

            break;
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsChoicebook::OnBuildCreatingCode"),GetLanguage());
        }
    }
}

bool wxsChoicebook::OnMouseClick(cb_unused wxWindow* Preview,cb_unused int PosX,cb_unused int PosY)
{
    UpdateCurrentSelection();
    if ( GetChildCount()<2 ) return false;
    int NewIndex = GetChildIndex(m_CurrentSelection)+1;
    if ( NewIndex >= GetChildCount() ) NewIndex = 0;
    m_CurrentSelection = GetChild(NewIndex);
    GetResourceData()->SelectItem(m_CurrentSelection,true);
    return true;
}

bool wxsChoicebook::OnIsChildPreviewVisible(wxsItem* Child)
{
    UpdateCurrentSelection();
    return Child == m_CurrentSelection;
}

bool wxsChoicebook::OnEnsureChildPreviewVisible(wxsItem* Child)
{
    if ( IsChildPreviewVisible(Child) ) return false;
    m_CurrentSelection = Child;
    UpdateCurrentSelection();
    return true;
}

void wxsChoicebook::UpdateCurrentSelection()
{
    wxsItem* NewCurrentSelection = nullptr;
    for ( int i=0; i<GetChildCount(); i++ )
    {
        if ( m_CurrentSelection == GetChild(i) ) return;
        wxsChoicebookExtra* CBExtra = (wxsChoicebookExtra*)GetChildExtra(i);
        if ( (i==0) || CBExtra->m_Selected )
        {
            NewCurrentSelection = GetChild(i);
        }
    }
    m_CurrentSelection = NewCurrentSelection;
}
