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
* $Revision: 13547 $
* $Id: wxsbitmapbutton.cpp 13547 2024-09-14 04:35:04Z mortenmacfly $
* $HeadURL: svn://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/contrib/wxSmith/wxwidgets/defitems/wxsbitmapbutton.cpp $
*/

#include <wx/tglbtn.h>
#include "wxsbitmaptogglebutton.h"

#include <prep.h>

namespace
{
    wxsRegisterItem<wxsBitmapToggleButton> Reg(_T("BitmapToggleButton"),wxsTWidget,_T("Standard"),350);

    WXS_ST_BEGIN(wxsBitmapToggleButtonStyles,_T("wxBU_AUTODRAW"))
        WXS_ST_CATEGORY("wxBitmapToggleButton")
        WXS_ST(wxBU_LEFT)
        WXS_ST(wxBU_TOP)
        WXS_ST(wxBU_RIGHT)
        WXS_ST(wxBU_BOTTOM)
        WXS_ST(wxBU_AUTODRAW)
        // cyberkoa: "The help mentions that wxBU_EXACTFIX is not used but the XRC code yes
        //  WXS_ST(wxBU_EXACTFIX)
        WXS_ST_DEFAULTS()
    WXS_ST_END()

    WXS_EV_BEGIN(wxsBitmapToggleButtonEvents)
        WXS_EVI(EVT_TOGGLEBUTTON,wxEVT_COMMAND_TOGGLEBUTTON_CLICKED,wxCommandEvent,Toggle)
    WXS_EV_END()
}

wxsBitmapToggleButton::wxsBitmapToggleButton(wxsItemResData* Data):
    wxsWidget(
        Data,
        &Reg.Info,
        wxsBitmapToggleButtonEvents,
        wxsBitmapToggleButtonStyles),
    IsChecked(false)
{}

void wxsBitmapToggleButton::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/tglbtn.h>"),GetInfo().ClassName,hfInPCH);

            Codef(_T("%C(%W, %I, %i, %P, %S, %T, %V, %N);\n"), &BitmapLabel, _T("wxART_BUTTON"));
            if ( !BitmapDisabled.IsEmpty() )
            {
                Codef(_T("%ASetBitmapDisabled(%i);\n"), &BitmapDisabled, _T("wxART_BUTTON"));
            }

            if ( !BitmapSelected.IsEmpty() )
            {
                Codef(_T("%ASetBitmapSelected(%i);\n"), &BitmapSelected, _T("wxART_BUTTON"));
            }

            if ( !BitmapFocus.IsEmpty() )
            {
                Codef(_T("%ASetBitmapFocus(%i);\n"), &BitmapFocus, _T("wxART_BUTTON"));
            }

            if ( IsChecked )
                Codef(_T("%ASetValue(%b);\n"),true);

            BuildSetupWindowCode();
            return;
        }

        case wxsUnknownLanguage: // fall through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsBitmapToggleButton::OnBuildCreatingCode"),GetLanguage());
        }
    }
}


wxObject* wxsBitmapToggleButton::OnBuildPreview(wxWindow* Parent,long _Flags)
{
    wxBitmapToggleButton* Preview = new wxBitmapToggleButton(Parent,GetId(),BitmapLabel.GetPreview(wxDefaultSize),Pos(Parent),Size(Parent),Style());

    if ( !BitmapDisabled.IsEmpty() )
    {
        Preview->SetBitmapDisabled(BitmapDisabled.GetPreview(wxDefaultSize));
    }

    if ( !BitmapSelected.IsEmpty() )
    {
        Preview->SetBitmapSelected(BitmapSelected.GetPreview(wxDefaultSize));
    }

    if ( !BitmapFocus.IsEmpty() )
    {
        Preview->SetBitmapFocus(BitmapFocus.GetPreview(wxDefaultSize));
    }

    if ( IsChecked )
    {
        Preview->SetValue(true);
    }

    return SetupWindow(Preview,_Flags);
}


void wxsBitmapToggleButton::OnEnumWidgetProperties(cb_unused long _Flags)
{
    WXS_BITMAP(wxsBitmapToggleButton,BitmapLabel,_("Bitmap"),_T("bitmap"),_T("wxART_OTHER"))
    WXS_BITMAP(wxsBitmapToggleButton,BitmapDisabled,_("Disabled bmp."),_T("disabled"),_T("wxART_OTHER"))
    WXS_BITMAP(wxsBitmapToggleButton,BitmapSelected,_("Pressed bmp."),_T("selected"),_T("wxART_OTHER"))
    WXS_BITMAP(wxsBitmapToggleButton,BitmapFocus,_("Focused bmp."),_T("focus"),_T("wxART_OTHER"))
    WXS_BOOL  (wxsBitmapToggleButton,IsChecked,_("Is checked"),_T("checked"),false)
}
