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

#include "wxsitemrestreedata.h"
#include "wxsitemresdata.h"
#include "wxsitem.h"
#include "wxsitemeditor.h" // For context menu functions

namespace
{
const int MenuCutId         = wxNewId();
const int MenuCopyId        = wxNewId();
const int MenuPasteBeforeId = wxNewId();
const int MenuPasteIntoId   = wxNewId();
const int MenuPasteAfterId  = wxNewId();
}

wxsItemResTreeData::wxsItemResTreeData(wxsItem* Item): m_Item(Item)
{
}

wxsItemResTreeData::~wxsItemResTreeData()
{
}

void wxsItemResTreeData::OnSelect()
{
    if ( m_Item )
    {
        m_Item->GetResourceData()->SelectItem(m_Item,true);
    }
}

// Provides context menu functions for all tree item controls.
// The context menu performs the same functions as
//   context/copy, context/PasteAfter
//   select button "InsertAfter", Menu/Edit/Copy, Menu/Edit/Paste
void wxsItemResTreeData::OnRightClick()
{
    if ( !m_Item || !m_Item->GetResourceData() )
        return;
    m_Item->GetResourceData()->SelectItem(m_Item,true); // The clicked item could not have been selected before
    wxMenu Popup;
    Popup.Append( MenuCutId,_("Cut"));
    Popup.Append( MenuCopyId,_("Copy"));
    Popup.Append( MenuPasteBeforeId,_("Paste Before Selected"));
    Popup.Append( MenuPasteIntoId,_("Paste Inside Selected"));
    Popup.Append( MenuPasteAfterId,_("Paste After Selected"));
    if ( !m_Item->GetResourceData()->CanPaste() )
    {
        Popup.Enable( MenuPasteBeforeId, false );
        Popup.Enable( MenuPasteIntoId, false );
        Popup.Enable( MenuPasteAfterId, false );
    }
    PopupMenu(&Popup); // Base class call, evt. loop is in base class
}

bool wxsItemResTreeData::OnPopup( long Id )
{
    wxsItemEditor* Editor = m_Item->GetResourceData()->GetEditor(); // Select
    if ( !Editor )
        return false;

    if      ( Id == MenuCutId )         { Editor->Cut();         }
    else if ( Id == MenuCopyId )        { Editor->Copy();        }
    else if ( Id == MenuPasteBeforeId ) { Editor->PasteBefore(); }
    else if ( Id == MenuPasteIntoId )   { Editor->PasteInto();   }
    else if ( Id == MenuPasteAfterId )  { Editor->PasteAfter();  }
    else                                { return false;          }
    return true;
}
