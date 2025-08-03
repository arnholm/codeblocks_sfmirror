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

#include "wxsbitmapiconproperty.h"
#include "wxsbitmapiconeditordlg.h"

#include <logmanager.h>
#include <wx/artprov.h>
#include <wx/image.h>
#include <globals.h>
#include "../wxsflags.h"

using namespace wxsFlags;

wxBitmap wxsBitmapIconData::GetPreview(const wxSize& Size,const wxString& DefaultClient)
{
    if ( Id.empty() )
    {
        if ( FileName.empty() )
        {
            return wxNullBitmap;
        }

        wxImage Img(FileName);
        if ( !Img.Ok() ) return wxNullBitmap;
        if ( Size != wxDefaultSize )
        {
            Img.Rescale(Size.GetWidth(),Size.GetHeight());
        }
        return wxBitmap(Img);
    }

    wxString TempClient = Client.empty() ? DefaultClient : Client;
    return wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(Id),wxART_MAKE_CLIENT_ID_FROM_STR(TempClient),Size);
}

wxString wxsBitmapIconData::BuildCode(bool NoResize,const wxString& SizeCode,wxsCoderContext* Ctx,const wxString& DefaultClient)
{
    switch ( Ctx->m_Language )
    {
        case wxsCPP:
        {
            Ctx->AddHeader(_T("<wx/bitmap.h>"),_T(""),hfLocal);
            Ctx->AddHeader(_T("<wx/image.h>"),_T(""),hfLocal);
            wxString Code;
            if ( Id.empty() )
            {
                if ( FileName.empty() )
                {
                    if ( CodeText.empty() )
                    {
                        return wxEmptyString;
                    }
                    else
                    {
                        Code << CodeText;
                    }
                }
                else{
                    if ( NoResize )
                    {
                        Code << _T("wxBitmap(wxImage(") << wxsCodeMarks::WxString(wxsCPP,FileName,false) << _T("))");
                    }
                    else
                    {
                        Code << _T("wxBitmap(wxImage(") << wxsCodeMarks::WxString(wxsCPP,FileName,false) << _T(").Rescale(")
                             << SizeCode << _T(".GetWidth(),") << SizeCode << _T(".GetHeight()))");
                    }
                }
            }
            else
            {
                Ctx->AddHeader(_T("<wx/artprov.h>"),_T(""),hfLocal);
                Code << _T("wxArtProvider::GetBitmap(wxART_MAKE_ART_ID_FROM_STR(") << wxsCodeMarks::WxString(wxsCPP,Id,false) << _T("),");
                wxString UsedClient = Client.empty() ? DefaultClient : Client;

                if ( UsedClient == _T("wxART_TOOLBAR") ||
                     UsedClient == _T("wxART_MENU") ||
                     UsedClient == _T("wxART_FRAME_ICON") ||
                     UsedClient == _T("wxART_CMN_DIALOG") ||
                     UsedClient == _T("wxART_HELP_BROWSER") ||
                     UsedClient == _T("wxART_MESSAGE_BO") ||
                     UsedClient == _T("wxART_BUTTON") ||
                     UsedClient == _T("wxART_OTHER") )
                {
                    // One of predefined client ids so we can use name directly
                    Code << UsedClient;
                }
                else
                {
                    // Not standard client id, we have to use macro
                    // but because wxART_MAKE_CLIENT_ID_FROM_STR uses + operator
                    // we additionally have to convert text to wxString
                    Code << _T("wxART_MAKE_CLIENT_ID_FROM_STR(wxString(");
                    Code << wxsCodeMarks::WxString(wxsCPP,Client,false);
                    Code << _T("))");
                }

                if ( !NoResize )
                {
                    Code << _T(",") << SizeCode;
                }

                Code << _T(")");
            }

            return Code;
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsBitmapIconData::BuildCode"),Ctx->m_Language);
        }
    }

    return wxEmptyString;
}

bool wxsBitmapIconData::IsEmpty()
{
    return Id.IsEmpty() && FileName.IsEmpty() && CodeText.IsEmpty();
}


// Helper macros for fetching variables
#define VALUE   wxsVARIABLE(Object,Offset,wxsBitmapIconData)

wxsBitmapIconProperty::wxsBitmapIconProperty(const wxString& PGName,const wxString& DataName,long _Offset,const wxString& _DefaultClient,int Priority):
    wxsCustomEditorProperty(PGName,DataName,Priority),
    Offset(_Offset),
    DefaultClient(_DefaultClient)
{}

wxString wxsBitmapIconProperty::GetStr(wxsPropertyContainer* Object)
{
    wxString res = VALUE.Id;
    if(res.IsEmpty())
        res = VALUE.FileName;
    return res.IsEmpty()?wxString(_("Click to add")):res;
}

bool wxsBitmapIconProperty::ShowEditor(wxsPropertyContainer* Object)
{
    wxsBitmapIconEditorDlg Dlg(nullptr,VALUE,DefaultClient);
    PlaceWindow(&Dlg);
    return Dlg.ShowModal() == wxID_OK;
}

bool wxsBitmapIconProperty::XmlRead(wxsPropertyContainer* Object,TiXmlElement* Element)
{
    if ( !Element )
    {
        return false;
    }

    // Trying to read from element's attributes
    VALUE.Id = cbC2U(Element->Attribute("stock_id"));
    VALUE.Client = cbC2U(Element->Attribute("stock_client"));

    // If failed, trying to read from child elements
    // (previous buggy style)
    if ( VALUE.Id.IsEmpty() )
    {
        XmlGetString(Element,VALUE.Id,_T("stock_id"));
    }
    if ( VALUE.Client.IsEmpty() )
    {
        XmlGetString(Element,VALUE.Client,_T("stock_client"));
    }

    if ( VALUE.Id.IsEmpty() )
    {
        // No wxART_PROVIDER Id, so it must be a filename or code
        VALUE.Id.Clear();
        VALUE.Client.Clear();

        VALUE.CodeText = cbC2U(Element->Attribute("code"));
        if ( VALUE.CodeText.IsEmpty() )
        {
            // It's a filename
            VALUE.CodeText.Clear();
            return XmlGetString(Element,VALUE.FileName);
        }
    }

    VALUE.FileName.Clear();
    return true;
}

bool wxsBitmapIconProperty::XmlWrite(wxsPropertyContainer* Object,TiXmlElement* Element)
{
    if ( !VALUE.Id.empty() )
    {
        Element->SetAttribute("stock_id",cbU2C(VALUE.Id));
        if ( !VALUE.Client.empty() )
        {
            Element->SetAttribute("stock_client",cbU2C(VALUE.Client));
        }
        return true;
    }

    if ( !VALUE.FileName.empty() )
    {
        XmlSetString(Element,VALUE.FileName);
        return true;
    }

    if ( !VALUE.CodeText.empty() )
    {
        Element->SetAttribute("code",cbU2C(VALUE.CodeText));
        return true;
    }

    return false;
}

bool wxsBitmapIconProperty::PropStreamRead(wxsPropertyContainer* Object,wxsPropertyStream* Stream)
{
    bool Ret = true;
    Stream->SubCategory(GetDataName());
    if ( Stream->GetString(_T("id"),VALUE.Id,wxEmptyString) )
    {
        Stream->GetString(_T("client"),VALUE.Client,wxEmptyString);
    }
    else
    {
        if ( !Stream->GetString(_T("file_name"),VALUE.FileName,wxEmptyString) ) Ret = false;
    }
    Stream->PopCategory();
    return Ret;
}

bool wxsBitmapIconProperty::PropStreamWrite(wxsPropertyContainer* Object,wxsPropertyStream* Stream)
{
    bool Ret = true;
    Stream->SubCategory(GetDataName());
    if ( VALUE.Id.empty() )
    {
        if ( !Stream->PutString(_T("file_name"),VALUE.FileName,wxEmptyString) ) Ret = false;
    }
    else
    {

        if ( !Stream->PutString(_T("id"),VALUE.Id,wxEmptyString) ) Ret = false;
        if ( !Stream->PutString(_T("client"),VALUE.Client,wxEmptyString) ) Ret = false;
    }
    Stream->PopCategory();
    return Ret;
}
