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
* $Id: wxscommandlinkbutton.cpp 13547 2024-09-14 04:35:04Z mortenmacfly $
* $HeadURL: svn://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/contrib/wxSmith/wxwidgets/defitems/wxscommandlinkbutton.cpp $
*/

#include "wxscommandlinkbutton.h"

#include <prep.h>

#include <wx/commandlinkbutton.h>

namespace
{
    wxsRegisterItem <wxsCommandLinkButton> Reg(_T("CommandLinkButton"),wxsTWidget,_T("Standard"), 285);

    WXS_ST_BEGIN(wxsCommandLinkButtonStyles, _T(""))
        WXS_ST_DEFAULTS()
    WXS_ST_END()

    WXS_EV_BEGIN(wxsCommandLinkButtonEvents)
        WXS_EVI(EVT_BUTTON, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEvent, Click)
    WXS_EV_END()
}

wxsCommandLinkButton::wxsCommandLinkButton(wxsItemResData* Data):
    wxsWidget(
        Data,
        &Reg.Info,
        wxsCommandLinkButtonEvents,
        wxsCommandLinkButtonStyles),
    Label(_("Label")),
    Note(_("Note")),
    IsDefault(false)
{}

void wxsCommandLinkButton::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/commandlinkbutton.h>"), GetInfo().ClassName, hfInPCH);
            Codef(_T("%C(%W, %I, %t, %t, %P, %S, %T, %V, %N);\n"), Label.wx_str(), Note.wx_str());
            if (IsDefault)
                Codef(_T("%ASetDefault();\n"));

            BuildSetupWindowCode();
            return;
        }

        case wxsUnknownLanguage: // fall through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsCommandLinkButton::OnBuildCreatingCode"), GetLanguage());
        }
    }
}

wxObject* wxsCommandLinkButton::OnBuildPreview(wxWindow* Parent, long _Flags)
{
    wxCommandLinkButton* Preview = new wxCommandLinkButton(Parent, GetId(), Label, Note, Pos(Parent), Size(Parent), Style());
    if (IsDefault)
      Preview->SetDefault();

    return SetupWindow(Preview, _Flags);
}

void wxsCommandLinkButton::OnEnumWidgetProperties(cb_unused long _Flags)
{
    WXS_STRING(wxsCommandLinkButton, Label, _("Label"), _T("label"), _T(""), false)
    WXS_STRING(wxsCommandLinkButton, Note,  _("Note"),  _T("label"), _T(""), false)
    WXS_BOOL(wxsCommandLinkButton, IsDefault, _("Is default"), _T("default"), false)
}
