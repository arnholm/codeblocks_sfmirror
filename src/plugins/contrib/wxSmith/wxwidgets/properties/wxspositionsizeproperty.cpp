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

#include "wxspositionsizeproperty.h"

#include <globals.h>

// Helper macro for fetching variable
#define DEFVALUE   wxsVARIABLE(Object,Offset,wxsPositionSizeData).IsDefault
#define XVALUE     wxsVARIABLE(Object,Offset,wxsPositionSizeData).X
#define YVALUE     wxsVARIABLE(Object,Offset,wxsPositionSizeData).Y
#define DUVALUE    wxsVARIABLE(Object,Offset,wxsPositionSizeData).DialogUnits

namespace
{
    // Helper values for compound property
    enum
    {
        DEFIND = 1,
        XIND,
        YIND,
        DUIND
    };
}

wxString wxsPositionSizeData::GetPositionCode(wxsCoderContext* Context)
{
    switch ( Context->m_Language )
    {
        case wxsCPP:
        {
            return IsDefault ?
                _T("wxDefaultPosition") :
                DialogUnits ?
                    wxString::Format(_T("wxDLG_UNIT(%s,wxPoint(%ld,%ld))"),Context->m_WindowParent.c_str(),X,Y) :
                    wxString::Format(_T("wxPoint(%ld,%ld)"),X,Y);
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsPositionSizeData::GetPositionCode"),Context->m_Language);
        }
    }

    return wxEmptyString;
}

wxString wxsPositionSizeData::GetSizeCode(wxsCoderContext* Context)
{
    switch ( Context->m_Language )
    {
        case wxsCPP:
        {
            return IsDefault ?
                _T("wxDefaultSize") :
                DialogUnits ?
                    wxString::Format(_T("wxDLG_UNIT(%s,wxSize(%ld,%ld))"),Context->m_WindowParent.c_str(),X,Y) :
                    wxString::Format(_T("wxSize(%ld,%ld)"),X,Y);
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsPositionSizeData::GetSizeCode"),Context->m_Language);
        }
    }

    return wxEmptyString;
}


wxsPositionSizeProperty::wxsPositionSizeProperty(
    const wxString& PGUseDefName,
    const wxString& _PGXName,
    const wxString& _PGYName,
    const wxString& _PGDUName,
    const wxString& _DataName,
    long _Offset,
    int _Priority):
        wxsProperty(PGUseDefName,_DataName,_Priority),
        PGXName(_PGXName),
        PGYName(_PGYName),
        PGDUName(_PGDUName),
        Offset(_Offset)
{}


void wxsPositionSizeProperty::PGCreate(wxsPropertyContainer* Object,wxPropertyGridManager* Grid,wxPGId Parent)
{
    wxPGId DefId = Grid->AppendIn(Parent,new wxBoolProperty(GetPGName(),wxPG_LABEL,DEFVALUE));
    wxPGId XId = Grid->AppendIn(Parent,new wxIntProperty(PGXName,wxPG_LABEL,XVALUE));
    wxPGId YId = Grid->AppendIn(Parent,new wxIntProperty(PGYName,wxPG_LABEL,YVALUE));
    wxPGId DUId = Grid->AppendIn(Parent,new wxBoolProperty(PGDUName,wxPG_LABEL,DUVALUE));

#if wxCHECK_VERSION(3, 3, 0)
    Grid->SetPropertyAttribute(DefId,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(DUId,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
#else
    Grid->SetPropertyAttribute(DefId,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(DUId,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
#endif

    PGRegister(Object,Grid,DefId,DEFIND);
    PGRegister(Object,Grid,XId,XIND);
    PGRegister(Object,Grid,YId,YIND);
    PGRegister(Object,Grid,DUId,DUIND);

    if ( DEFVALUE )
    {
        Grid->DisableProperty(XId);
        Grid->DisableProperty(YId);
        Grid->DisableProperty(DUId);
    }
}

bool wxsPositionSizeProperty::PGRead(wxsPropertyContainer* Object,wxPropertyGridManager* Grid,wxPGId Id,long Index)
{
    switch ( Index )
    {
        case DEFIND:
            DEFVALUE = Grid->GetPropertyValue(Id).GetBool();
            break;

        case XIND:
            XVALUE = Grid->GetPropertyValue(Id).GetLong();
            break;

        case YIND:
            YVALUE = Grid->GetPropertyValue(Id).GetLong();
            break;

        case DUIND:
            DUVALUE = Grid->GetPropertyValue(Id).GetBool();
            break;

        default:
            break;
    }

    return true;
}

bool wxsPositionSizeProperty::PGWrite(wxsPropertyContainer* Object,wxPropertyGridManager* Grid,wxPGId Id,long Index)
{
    switch ( Index )
    {
        case DEFIND:
            Grid->SetPropertyValue(Id,DEFVALUE);
            break;

        case XIND:
            if ( DEFVALUE )
            {
                Grid->DisableProperty(Id);
            }
            else
            {
                Grid->EnableProperty(Id);
            }
            Grid->SetPropertyValue(Id,XVALUE);
            break;

        case YIND:
            if ( DEFVALUE )
            {
                Grid->DisableProperty(Id);
            }
            else
            {
                Grid->EnableProperty(Id);
            }
            Grid->SetPropertyValue(Id,YVALUE);
            break;

        case DUIND:
            if ( DEFVALUE )
            {
                Grid->DisableProperty(Id);
            }
            else
            {
                Grid->EnableProperty(Id);
            }
            Grid->SetPropertyValue(Id,DUVALUE);
            break;

        default:
            break;
    }
    return true;
}

bool wxsPositionSizeProperty::XmlRead(wxsPropertyContainer* Object,TiXmlElement* Element)
{
    if ( !Element )
    {
        DEFVALUE = true;
        XVALUE = -1;
        YVALUE = -1;
        DUVALUE = false;
        return false;
    }

    const char* Text = Element->GetText();

    // If no node or empty text, using default values
    if ( !Text || !Text[0] )
    {
        DEFVALUE = true;
        XVALUE = -1;
        YVALUE = -1;
        DUVALUE = false;
        return false;
    }
    wxString Str = cbC2U(Text);

    if ( Str[Str.Length()-1] == _T('d') )
    {
        DUVALUE = true;
        Str.RemoveLast();
    }
    else
    {
        DUVALUE = false;
    }

    if ( !Str.BeforeFirst(_T(',')).ToLong(&XVALUE) ||
         !Str.AfterLast(_T(',')).ToLong(&YVALUE) )
    {
        DEFVALUE = true;
        XVALUE = -1;
        YVALUE = -1;
        DUVALUE = false;
        return false;
    }
    DEFVALUE = false;

    return true;
}

bool wxsPositionSizeProperty::XmlWrite(wxsPropertyContainer* Object,TiXmlElement* Element)
{
    if ( !DEFVALUE )
    {
        wxString Str;
        Str.Printf(_T("%ld,%ld%s"),XVALUE,YVALUE,DUVALUE ? _T("d") : wxEmptyString);
        Element->InsertEndChild(TiXmlText(cbU2C(Str)));
        return true;
    }
    return false;
}

bool wxsPositionSizeProperty::PropStreamRead(wxsPropertyContainer* Object,wxsPropertyStream* Stream)
{
    bool Ret = true;
    Stream->SubCategory(GetDataName());
    if ( !Stream->GetBool(_T("default"),DEFVALUE,true) ) Ret = false;
    if ( !DEFVALUE )
    {
        if ( !Stream->GetLong(_T("x"),XVALUE,-1) ) Ret = false;
        if ( !Stream->GetLong(_T("y"),YVALUE,-1) ) Ret = false;
        if ( !Stream->GetBool(_T("dialog_units"),DUVALUE,false) ) Ret = false;
    }
    Stream->PopCategory();
    return Ret;
}

bool wxsPositionSizeProperty::PropStreamWrite(wxsPropertyContainer* Object,wxsPropertyStream* Stream)
{
    bool Ret = true;
    Stream->SubCategory(GetDataName());
    if ( !Stream->PutBool(_T("default"),DEFVALUE,true) ) Ret = false;
    if ( !DEFVALUE )
    {
        if ( !Stream->PutLong(_T("x"),XVALUE,-1) ) Ret = false;
        if ( !Stream->PutLong(_T("y"),YVALUE,-1) ) Ret = false;
        if ( !Stream->PutBool(_T("dialog_units"),DUVALUE,false) ) Ret = false;
    }
    Stream->PopCategory();
    return Ret;
}
