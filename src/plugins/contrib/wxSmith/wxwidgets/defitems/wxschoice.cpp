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

#include <wx/choice.h>
#include "wxschoice.h"

namespace
{
    wxsRegisterItem<wxsChoice> Reg(_T("Choice"),wxsTWidget,_T("Standard"),310);

    WXS_ST_BEGIN(wxsChoiceStyles,_T(""))
        WXS_ST_CATEGORY("wxChoice")
        WXS_ST(wxCB_SORT)
        WXS_ST_DEFAULTS()
    WXS_ST_END()


    WXS_EV_BEGIN(wxsChoiceEvents)
        WXS_EVI(EVT_CHOICE,wxEVT_COMMAND_CHOICE_SELECTED,wxCommandEvent,Select)
    WXS_EV_END()
}

wxsChoice::wxsChoice(wxsItemResData* Data):
    wxsWidget(
        Data,
        &Reg.Info,
        wxsChoiceEvents,
        wxsChoiceStyles),
        DefaultSelection(-1),
        UseItemsArray(false)
{}


void wxsChoice::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/choice.h>"),GetInfo().ClassName,hfInPCH);

            if (UseItemsArray)
            {
              size_t count = ArrayChoices.GetCount();

              if (count != 0)
              {
                // Create const array of items
                Codef(_T("const wxString %O_choices[] = {\n"));
                for ( size_t i = 0; i <  count; ++i )
                {
                  Codef(_T("%t,\n"),ArrayChoices[i].wx_str());
                }
                Codef(_T("};\n"));

                wxString code = wxString::Format(_T("%%C(%%W, %%I, %%P, %%S, %zu, %%O_choices, %%T, %%V, %%N);\n"), count);
                Codef(code);
                if ( DefaultSelection != -1 )
                  Codef(wxString::Format(_T("%%ASetSelection(%ld);\n"), DefaultSelection));
              }
              else
              {
                Codef(_T("%C(%W, %I, %P, %S, 0, 0, %T, %V, %N);\n"));
              }
            }
            else
            {
                Codef(_T("%C(%W, %I, %P, %S, 0, 0, %T, %V, %N);\n"));

                for ( size_t i = 0; i <  ArrayChoices.GetCount(); ++i )
                {
                    if ( DefaultSelection == (int)i )
                    {
                        Codef(_T("%ASetSelection( "));
                    }
                    Codef(_T("%AAppend(%t)"),ArrayChoices[i].wx_str());
                    if ( DefaultSelection == (int)i )
                    {
                        Codef(_T(" )"));
                    }
                    Codef(_T(";\n"));
                }
            }
            BuildSetupWindowCode();
            return;
        }

        case wxsUnknownLanguage: // fall through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsChoice::OnBuildCreatingCode"),GetLanguage());
        }
    }
}

wxObject* wxsChoice::OnBuildPreview(wxWindow* Parent,long _Flags)
{
    wxChoice* Preview = new wxChoice(Parent,GetId(),Pos(Parent),Size(Parent),0,nullptr,Style());

    for ( size_t i = 0; i <  ArrayChoices.GetCount(); ++i )
    {
        int Val = Preview->Append(ArrayChoices[i]);
        if ( (int)i == DefaultSelection )
        {
            Preview->SetSelection(Val);
        }
    }
    return SetupWindow(Preview,_Flags);
}

void wxsChoice::OnEnumWidgetProperties(cb_unused long _Flags)
{
    WXS_ARRAYSTRING(wxsChoice,ArrayChoices, _("Choices"), "content", "item")
    WXS_BOOL(wxsChoice,UseItemsArray, _("Use Items Array"), "use_items_array", false)
    WXS_LONG(wxsChoice,DefaultSelection, _("Selection"), "selection", -1)
}
