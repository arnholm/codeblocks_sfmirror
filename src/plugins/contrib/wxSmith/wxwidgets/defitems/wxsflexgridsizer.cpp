/** \file wxsflexgridsizer.cpp
*
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

#include "wxsflexgridsizer.h"

namespace
{
    wxArrayInt GetArray(const wxString& String, bool* Valid = nullptr)
    {
        bool ok = true;
        wxArrayInt Array;

        wxStringTokenizer Tokens(String, ",");
        while (Tokens.HasMoreTokens())
        {
            long Value;
            wxString Token = Tokens.GetNextToken();
            Token.Trim(true);
            Token.Trim(false);
            if (!Token.ToLong(&Value) || (Value < 0))
            {
                // Reject invalid values and signal error
                ok = false;
            }
            else
            {
                // Reject repeated values
                // AddGrowable[Col|Row]() cannot be called twice with the same index
                if (Array.Index((int)Value) == wxNOT_FOUND)
                    Array.Add((int)Value);
            }
        }

        if (Valid)
            *Valid = ok;

        return Array;
    }

    bool FixupList(wxString& List)
    {
        bool Ret;
        wxArrayInt Array = GetArray(List, &Ret);
        List.Clear();
        for ( size_t i=0; i<Array.Count(); i++ )
        {
            List << Array[i];
            if ( i < Array.Count() - 1 )
            {
                List << ',';
            }
        }

        return Ret;
    }

    wxsRegisterItem<wxsFlexGridSizer> Reg(_T("FlexGridSizer"),wxsTSizer,_T("Layout"),80);
}

wxsFlexGridSizer::wxsFlexGridSizer(wxsItemResData* Data):
    wxsSizer(Data,&Reg.Info),
    Cols(3),
    Rows(0)
{
}

wxSizer* wxsFlexGridSizer::OnBuildSizerPreview(wxWindow* Parent)
{
    wxFlexGridSizer* Sizer = new wxFlexGridSizer(Rows, Cols,
        VGap.GetPixels(Parent), HGap.GetPixels(Parent));

    wxArrayInt _Cols = GetArray(GrowableCols);
    for ( size_t i=0; i<_Cols.Count(); i++ )
    {
        // Do not call the method with an out-of-range index
        if (_Cols[i] < Cols)
            Sizer->AddGrowableCol(_Cols[i]);
    }

    wxArrayInt _Rows = GetArray(GrowableRows);
    for ( size_t i=0; i<_Rows.Count(); i++ )
    {
        // Do not call the method with an out-of-range index
        if (_Rows[i] < Rows)
            Sizer->AddGrowableRow(_Rows[i]);
    }

    return Sizer;
}

void wxsFlexGridSizer::OnBuildSizerCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/sizer.h>"),GetInfo().ClassName,hfInPCH);
            Codef(_T("%C(%d, %d, %s, %s);\n"),Rows,Cols,
                  VGap.GetPixelsCode(GetCoderContext()).wx_str(),
                  HGap.GetPixelsCode(GetCoderContext()).wx_str());

            wxArrayInt _Cols = GetArray(GrowableCols);
            for ( size_t i=0; i<_Cols.Count(); i++ )
            {
                // Do not check range here, let the runtime assert
                Codef(_T("%AAddGrowableCol(%d);\n"),_Cols[i]);
            }

            wxArrayInt _Rows = GetArray(GrowableRows);
            for ( size_t i=0; i<_Rows.Count(); i++ )
            {
                // Do not check range here, let the runtime assert
                Codef(_T("%AAddGrowableRow(%d);\n"),_Rows[i]);
            }

            return;
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsFlexGridSizer::OnBuildSizerCreatingCode"),GetLanguage());
        }
    }
}

void wxsFlexGridSizer::OnEnumSizerProperties(cb_unused long Flags)
{
    FixupList(GrowableCols);
    FixupList(GrowableRows);
    WXS_LONG(wxsFlexGridSizer,Cols,_("Cols"),_T("cols"),0);
    WXS_LONG(wxsFlexGridSizer,Rows,_("Rows"),_T("rows"),0);
    WXS_DIMENSION(wxsFlexGridSizer,VGap,_("V-Gap"),_("V-Gap in dialog units"),_T("vgap"),0,false);
    WXS_DIMENSION(wxsFlexGridSizer,HGap,_("H-Gap"),_("H-Gap in dialog units"),_T("hgap"),0,false);
    WXS_SHORT_STRING_T(wxsFlexGridSizer, GrowableCols, _("Growable cols"), _T("growablecols"), _T(""), false, _("Comma-separated list of growable columns"));
    WXS_SHORT_STRING_T(wxsFlexGridSizer, GrowableRows, _("Growable rows"), _T("growablerows"), _T(""), false, _("Comma-separated list of growable rows"));
    FixupList(GrowableCols);
    FixupList(GrowableRows);
}
