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
* $Revision: 13235 $
* $Id: wxslistbox.cpp 13235 2023-03-15 14:00:15Z wh11204 $
* $HeadURL: svn://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/contrib/wxSmith/wxwidgets/defitems/wxslistbox.cpp $
*/

#include "wxseditablelistbox.h"

#include <prep.h>
#include <wx/editlbox.h>

namespace
{
    wxsRegisterItem<wxsEditableListBox> Reg(_T("EditableListBox"), wxsTWidget ,_T("Standard"), 274);

    WXS_ST_BEGIN(wxsEditableListBoxStyles,_T(""))
        WXS_ST_CATEGORY("wxEditableListBox")
        WXS_ST(wxEL_ALLOW_NEW)
        WXS_ST(wxEL_ALLOW_EDIT)
        WXS_ST(wxEL_ALLOW_DELETE)
        WXS_ST(wxEL_NO_REORDER)
        WXS_ST(wxEL_DEFAULT_STYLE)
        WXS_ST_DEFAULTS()
    WXS_ST_END()

    WXS_EV_BEGIN(wxsEditableListBoxEvents)
        WXS_EVI(EVT_LIST_BEGIN_DRAG,wxEVT_COMMAND_LIST_BEGIN_DRAG, wxListEvent, BeginDrag)
        WXS_EVI(EVT_LIST_BEGIN_RDRAG,wxEVT_COMMAND_LIST_BEGIN_RDRAG, wxListEvent, BeginRDrag)
        WXS_EVI(EVT_LIST_BEGIN_LABEL_EDIT,wxEVT_COMMAND_LIST_BEGIN_LABEL_EDIT, wxListEvent, BeginLabelEdit)
        WXS_EVI(EVT_LIST_END_LABEL_EDIT,wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEvent, EndLabelEdit)
        WXS_EVI(EVT_LIST_DELETE_ITEM,wxEVT_COMMAND_LIST_DELETE_ITEM, wxListEvent, DeleteItem)
        WXS_EVI(EVT_LIST_DELETE_ALL_ITEMS,wxEVT_COMMAND_LIST_DELETE_ALL_ITEMS, wxListEvent, DeleteAllItems)
        WXS_EVI(EVT_LIST_ITEM_SELECTED,wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEvent, ItemSelect)
        WXS_EVI(EVT_LIST_ITEM_DESELECTED,wxEVT_COMMAND_LIST_ITEM_DESELECTED, wxListEvent, ItemDeselect)
        WXS_EVI(EVT_LIST_ITEM_ACTIVATED,wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEvent, ItemActivated)
        WXS_EVI(EVT_LIST_ITEM_FOCUSED,wxEVT_COMMAND_LIST_ITEM_FOCUSED, wxListEvent, ItemFocused)
        WXS_EVI(EVT_LIST_ITEM_MIDDLE_CLICK,wxEVT_COMMAND_LIST_ITEM_MIDDLE_CLICK, wxListEvent, ItemMClick)
        WXS_EVI(EVT_LIST_ITEM_RIGHT_CLICK,wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK, wxListEvent, ItemRClick)
        WXS_EVI(EVT_LIST_KEY_DOWN,wxEVT_COMMAND_LIST_KEY_DOWN, wxListEvent, KeyDown)
        WXS_EVI(EVT_LIST_INSERT_ITEM,wxEVT_COMMAND_LIST_INSERT_ITEM, wxListEvent, InsertItem)
        WXS_EVI(EVT_LIST_COL_CLICK,wxEVT_COMMAND_LIST_COL_CLICK, wxListEvent, ColumnClick)
        WXS_EVI(EVT_LIST_COL_RIGHT_CLICK,wxEVT_COMMAND_LIST_COL_RIGHT_CLICK, wxListEvent, ColumnRClick)
        WXS_EVI(EVT_LIST_COL_BEGIN_DRAG,wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEvent, ColumnBeginDrag)
        WXS_EVI(EVT_LIST_COL_DRAGGING,wxEVT_COMMAND_LIST_COL_DRAGGING, wxListEvent, ColumnDragging)
        WXS_EVI(EVT_LIST_COL_END_DRAG,wxEVT_COMMAND_LIST_COL_END_DRAG, wxListEvent, ColumnEndDrag)
        WXS_EVI(EVT_LIST_CACHE_HINT,wxEVT_COMMAND_LIST_CACHE_HINT, wxListEvent, CacheHint)
    WXS_EV_END()
}

wxsEditableListBox::wxsEditableListBox(wxsItemResData* Data):
    wxsWidget(
        Data,
        &Reg.Info,
        wxsEditableListBoxEvents,
        wxsEditableListBoxStyles)
{}

void wxsEditableListBox::OnBuildCreatingCode()
{
    switch (GetLanguage())
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/editlbox.h>"), GetInfo().ClassName, hfInPCH);
            AddHeader(_T("<wx/arrstr.h>"), "wxArrayString", hfInPCH);
            Codef(_T("%C(%W, %I, %t, %P, %S, %T, %N);\n"), Label.wx_str());
            if (!ArrayChoices.IsEmpty())
            {
                const wxString arrayName(GetVarName()+"_array");
                Codef(_T("wxArrayString %s;\n"), arrayName.wx_str());
                for (size_t i = 0; i < ArrayChoices.GetCount(); ++i)
                    Codef(_T("%s.Add(%t);\n"), arrayName.wx_str(), ArrayChoices[i].wx_str());

                Codef(_T("%ASetStrings(%s);\n"), arrayName.wx_str());
            }

            BuildSetupWindowCode();
            return;
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsEditableListBox::OnBuildCreatingCode"),GetLanguage());
        }
    }
}

wxObject* wxsEditableListBox::OnBuildPreview(wxWindow* Parent,long _Flags)
{
    wxEditableListBox* Preview = new wxEditableListBox(Parent, GetId(), Label, Pos(Parent), Size(Parent), Style());
    Preview->SetStrings(ArrayChoices);
    return SetupWindow(Preview,_Flags);
}

void wxsEditableListBox::OnEnumWidgetProperties(cb_unused long _Flags)
{
      WXS_SHORT_STRING(wxsEditableListBox, Label, _("Label"), "label", "", false)
      WXS_ARRAYSTRING(wxsEditableListBox, ArrayChoices, _("Choices"), "content", "item")
}
