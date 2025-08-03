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

#include "wxsmenubar.h"
#include "wxsmenueditor.h"
#include "wxsmenu.h"
#include "../wxsitemresdata.h"
#include <wx/menu.h>
#include "scrollingdialog.h"

#include <prep.h>

namespace
{
    wxsRegisterItem<wxsMenuBar> Reg(_T("MenuBar"),wxsTTool,_T("Tools"),60);

    class MenuEditorDialog: public wxScrollingDialog
    {
        public:

            wxsMenuEditor* Editor;

            MenuEditorDialog(wxsMenuBar* MenuBar):
                wxScrollingDialog(nullptr,-1,_("MenuBar editor"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
            {
                wxBoxSizer* Sizer = new wxBoxSizer(wxVERTICAL);
                Sizer->Add(Editor = new wxsMenuEditor(this,MenuBar),1,wxEXPAND,0);
                Sizer->Add(CreateButtonSizer(wxOK|wxCANCEL),0,wxEXPAND,15);
                SetSizer(Sizer);
                Sizer->SetSizeHints(this);
                PlaceWindow(this,pdlCentre,true);
            }

            void OnOK(cb_unused wxCommandEvent& event)
            {
                Editor->ApplyChanges();
                EndModal(wxID_OK);
            }

            DECLARE_EVENT_TABLE()
    };

    BEGIN_EVENT_TABLE(MenuEditorDialog,wxScrollingDialog)
        EVT_BUTTON(wxID_OK,MenuEditorDialog::OnOK)
    END_EVENT_TABLE()
}

wxsMenuBar::wxsMenuBar(wxsItemResData* Data):
    wxsTool(
        Data,
        &Reg.Info,
        nullptr,
        nullptr,
        flVariable|flSubclass|flExtraCode)
{
}

void wxsMenuBar::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
            AddHeader(_T("<wx/menu.h>"),GetInfo().ClassName,hfInPCH);
            Codef(_T("%C();\n"));
            for ( int i=0; i<GetChildCount(); i++ )
            {
                GetChild(i)->BuildCode(GetCoderContext());
            }
            Codef(_T("%MSetMenuBar(%O);\n"));
            BuildSetupWindowCode();
            break;

        case wxsUnknownLanguage: // fall-through
        default:
            wxsCodeMarks::Unknown(_T("wxsMenuBar::OnBuildCreatingCode"),GetLanguage());
    }
}

void wxsMenuBar::OnEnumToolProperties(cb_unused long _Flags)
{
}

bool wxsMenuBar::OnCanAddToResource(wxsItemResData* Data,bool ShowMessage)
{
    if ( Data->GetClassType() != _T("wxFrame") )
    {
        if ( ShowMessage )
        {
            cbMessageBox(_("wxMenuBar can be added to wxFrame only"));
        }
        return false;
    }

    for ( int i=0; i<Data->GetToolsCount(); i++ )
    {
        if ( Data->GetTool(i)->GetClassName() == _T("wxMenuBar") )
        {
            if ( ShowMessage )
            {
                cbMessageBox(_("Can not add two or more wxMenuBar classes\ninto one wxFrame"));
            }
            return false;
        }
    }

    return true;
}

bool wxsMenuBar::OnXmlReadChild(TiXmlElement* Elem,bool IsXRC,bool IsExtra)
{
    if ( IsXRC )
    {
        wxString ClassName = cbC2U(Elem->Attribute("class"));
        if ( ClassName == _T("wxMenu") )
        {
            wxsMenu* Child = new wxsMenu(GetResourceData());
            if ( !AddChild(Child) )
            {
                delete Child;
                return false;
            }
            return Child->XmlRead(Elem,IsXRC,IsExtra);
        }
    }

    return true;
}

bool wxsMenuBar::OnCanAddChild(wxsItem* Item,bool ShowMessage)
{
    if ( Item->GetInfo().ClassName != _T("wxMenu") )
    {
        if ( ShowMessage )
        {
            cbMessageBox(_("Only wxMenu items can be added into wxMenuBar"));
        }
        return false;
    }
    return true;
}

bool wxsMenuBar::OnMouseDClick(cb_unused wxWindow* Preview,cb_unused int PosX,cb_unused int PosY)
{
    MenuEditorDialog Dlg(this);
    Dlg.ShowModal();
    return false;
}
