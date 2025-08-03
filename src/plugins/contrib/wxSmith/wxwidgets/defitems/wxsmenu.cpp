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

#include "wxsmenu.h"
#include "wxsmenuitem.h"
#include "wxsmenueditor.h"
#include "../wxsitemresdata.h"
#include <globals.h>
#include <prep.h>
#include "scrollingdialog.h"

namespace
{
    wxsRegisterItem<wxsMenu> Reg(_T("Menu"),wxsTTool,_T("Tools"),70,false);

    class MenuEditorDialog: public wxScrollingDialog
    {
        public:

            wxsMenuEditor* Editor;

            MenuEditorDialog(wxsMenu* Menu):
                wxScrollingDialog(nullptr,-1,_("Menu editor"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
            {
                wxBoxSizer* Sizer = new wxBoxSizer(wxVERTICAL);
                Sizer->Add(Editor = new wxsMenuEditor(this,Menu),1,wxEXPAND,0);
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

wxsMenu::wxsMenu(wxsItemResData* Data):
    wxsTool(Data,&Reg.Info,nullptr,nullptr,flVariable|flSubclass|flExtraCode)
{
}

//wxSize wxsMenu::CalculateSize(wxArrayInt* Cols)
//{
//    if ( m_Type != Menu )
//    {
//        return wxSize(0,0);
//    }
//
//    wxBitmap EmptyBitmap(10,10);
//    wxMemoryDC DC;
//    DC.SelectObject(EmptyBitmap);
//
//    wxSize TotalSize(0,0);
//    wxSize ColumnSize(0,0);
//    for ( int i=0; i<GetChildCount(); i++ )
//    {
//        wxsMenu* Child = (wxsMenu*)GetChild(i);
//        switch ( Child->m_Type )
//        {
//            case Break:
//            {
//                if ( Cols )
//                {
//                    Cols->Add(ColumnSize.GetWidth());
//                }
//
//                TotalSize.SetWidth(TotalSize.GetWidth()+ColumnSize.GetWidth()+8);
//                if ( TotalSize.GetHeight() < ColumnSize.GetHeight() )
//                {
//                    TotalSize.SetHeight(ColumnSize.GetHeight());
//                }
//                ColumnSize.Set(0,0);
//                break;
//            }
//
//            case Separator:
//            {
//                ColumnSize.SetHeight(ColumnSize.GetHeight() + 8);
//                break;
//            }
//
//            case Menu:
//            case Normal:
//            case Radio:
//            case Check:
//            {
//                wxCoord W, H;
//                wxString Text = m_Label;
//                if ( !m_Accelerator.IsEmpty() ) Text << _T(' ') << m_Accelerator;
//                DC.GetTextExtent(Text,&W,&H);
//                W += 36;
//                ColumnSize.SetHeight(ColumnSize.GetHeight() + H);
//                if ( ColumnSize.GetWidth() < W )
//                {
//                    ColumnSize.SetWidth(W);
//                }
//                break;
//            }
//        }
//    }
//
//    if ( Cols )
//    {
//        Cols->Add(ColumnSize.GetWidth());
//    }
//
//    return wxSize(TotalSize.GetWidth() + ColumnSize.GetWidth() + 4,
//                  TotalSize.GetHeight() + ColumnSize.GetHeight() + 4);
//}
//
//void wxsMenu::Draw(wxDC* DC,int BegX,int BegY)
//{
//    if ( m_Type != Menu )
//    {
//        return;
//    }
//    wxColour Back(0x80,0x80,0x80);
//    wxColour Light(0xC0,0xC0,0xC0);
//    wxColour Dark(0x40,0x40,0x40);
//    wxArrayInt Cols;
//    wxSize Size = CalculateSize(&Cols);
//    DC->SetBrush(Back);
//    DC->SetPen(Light);
//    DC->DrawRectangle(BegX,BegY,Size.GetWidth(),Size.GetHeight());
//    DC->SetPen(Dark);
//    DC->DrawLine(BegX+Size.GetWidth(),BegY, BegX+Size.GetWidth(),BegY+Size.GetHeight());
//    DC->DrawLine(BegX,BegY+Size.GetHeight(),BegX+Size.GetWidth(),BegY+Size.GetHeight());
//    int PosX = BegX + 2;
//    int PosY = BegY + 2;
//    int Column = 0;
//    for ( int i=0; i<GetChildCount(); i++ )
//    {
//        wxsMenu* Child = (wxsMenu*)GetChild(i);
//        switch ( Child->m_Type )
//        {
//            case Break:
//            {
//                PosX += Cols[Column++];
//                PosY = BegY + 2;
//                DC->SetPen(Dark);
//                DC->DrawLine(PosX+3,PosY,PosX+3,PosY+Size.GetHeight()-4);
//                DC->SetPen(Light);
//                DC->DrawLine(PosX+4,PosY,PosX+4,PosY+Size.GetHeight()-4);
//                PosX += 8;
//                break;
//            }
//
//            case Separator:
//            {
//                DC->SetPen(Dark);
//                DC->DrawLine(PosX,PosY+3,PosX+Cols[Column],PosY+3);
//                DC->SetPen(Light);
//                DC->DrawLine(PosX,PosY+4,PosX+Cols[Column],PosY+4);
//                PosY += 8;
//                break;
//            }
//
//            case Menu:
//            case Normal:
//            case Radio:
//            case Check:
//            {
//                DC->SetPen(*wxBLACK);
//                DC->DrawText(m_Label,PosX,PosY);
//                wxCoord W, H;
//                DC->GetTextExtent(m_Label,&W,&H);
//                PosY += H;
//                break;
//            }
//        }
//    }
//}

void wxsMenu::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
            AddHeader(_T("<wx/menu.h>"),GetInfo().ClassName,hfInPCH);
            if ( IsPointer() )
            {
                // There's no Create() method for wxMenu so we call ctor only when creating pointer
                Codef(_T("%C();\n"));
            }
            for ( int i=0; i<GetChildCount(); i++ )
            {
                GetChild(i)->BuildCode(GetCoderContext());
            }
            if ( GetParent() && GetParent()->GetClassName()==_T("wxMenuBar") )
            {
                Codef(_T("%MAppend(%O, %t);\n"),m_Label.wx_str());
            }
            BuildSetupWindowCode();
            break;

        case wxsUnknownLanguage: // fall-through
        default:
            wxsCodeMarks::Unknown(_T("wxsMenu::OnBuildCreatingCode"),GetLanguage());
    }
}

void wxsMenu::OnEnumToolProperties(cb_unused long _Flags)
{
    if ( GetParent() )
    {
        // If there's parent we got label for this menu
        WXS_SHORT_STRING(wxsMenu,m_Label,_("Title"),_T("label"),_T(""),true);
    }
}

bool wxsMenu::OnMouseDClick(cb_unused wxWindow* Preview,cb_unused int PosX,cb_unused int PosY)
{
    MenuEditorDialog Dlg(this);
    Dlg.ShowModal();
    return false;
}

bool wxsMenu::OnIsPointer()
{
    // Must be object if parentless (tool)
    if ( !GetParent() ) return false;

    // Otherwise must be a pointer (child of wxMenuBar)
    return true;
}

bool wxsMenu::OnCanAddChild(wxsItem* Item,bool ShowMessage)
{
    if ( Item->GetInfo().ClassName != _T("wxMenuItem") )
    {
        if ( ShowMessage )
        {
            cbMessageBox(_("Only wxMenuItem classes can be added into wxMenu"));
        }
        return false;
    }
    return true;
}

bool wxsMenu::OnCanAddToParent(wxsParent* Parent,bool ShowMessage)
{
    if ( Parent->GetClassName() != _T("wxMenuBar") )
    {
        if ( ShowMessage )
        {
            cbMessageBox(_("wxMenu can be added into wxMenuBar only"));
        }
        return false;
    }
    return true;
}

bool wxsMenu::OnXmlReadChild(TiXmlElement* Elem,bool IsXRC,bool IsExtra)
{
    if ( IsXRC )
    {
        wxString ClassName = cbC2U(Elem->Attribute("class"));
        if ( ClassName == _T("separator") || ClassName == _T("break") )
        {
            wxsMenuItem* Child = new wxsMenuItem(GetResourceData(),true);
            AddChild(Child);
            return Child->XmlRead(Elem,IsXRC,IsExtra);
        }

        if ( ClassName == _T("wxMenu") || ClassName == _T("wxMenuItem") )
        {
            wxsMenuItem* Child = new wxsMenuItem(GetResourceData(),false);
            AddChild(Child);
            return Child->XmlRead(Elem,IsXRC,IsExtra);
        }
    }

    return true;
}

wxString wxsMenu::OnGetTreeLabel(int& Image)
{
    if ( GetParent() )
    {
        // This is internal item of wxMenuBar,
        // we give the description like in wxMenuItem
        return m_Label;
    }

    // When this is tool, we return usual description
    return wxsItem::OnGetTreeLabel(Image);
}
