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

#include "wxsitemfactory.h"
#include "wxsitem.h"
#include "../wxsresourcetree.h"

wxsItem* wxsItemFactory::Build(const wxString& Name,wxsItemResData* Data)
{
    ItemMapT::iterator it = ItemMap().find(Name);
    if ( it == ItemMap().end() ) return nullptr;
    wxsItem* Item = it->second->OnBuild(Data);

    // Checking few things in item's info
    switch ( Item->GetInfo().Type )
    {
        case wxsTTool:
            if ( !Item->ConvertToTool() )
            {
                // Fake item
                delete Item;
                return nullptr;
            }
            break;

        case wxsTContainer:
            if ( !Item->ConvertToParent() )
            {
                // Fake item
                delete Item;
                return nullptr;
            }
            break;

        case wxsTSizer:
        case wxsTSpacer:
        case wxsTWidget:
            break;

        case wxsTInvalid:
        default:
            delete Item;
            return nullptr;
    }

    return Item;
}

wxsItemInfo* wxsItemFactory::GetInfo(const wxString& Name)
{
    ItemMapT::iterator it = ItemMap().find(Name);
    if ( it == ItemMap().end() ) return nullptr;
    return it->second->m_Info;
}

wxsItemInfo* wxsItemFactory::GetFirstInfo()
{
    m_Iter = ItemMap().begin();
    return (m_Iter==ItemMap().end()) ? nullptr : m_Iter->second->m_Info;
}

wxsItemInfo* wxsItemFactory::GetNextInfo()
{
    if ( m_Iter==ItemMap().end() ) return nullptr;
    ++m_Iter;
    return (m_Iter==ItemMap().end()) ? nullptr : m_Iter->second->m_Info;
}

wxImageList& wxsItemFactory::GetImageList()
{
    return wxsResourceTree::GetGlobalImageList();
}

int wxsItemFactory::LoadImage(const wxString& FileName)
{
    return wxsResourceTree::LoadImage(FileName);
}

wxsItemFactory::wxsItemFactory(wxsItemInfo* Info) :
    m_Info(Info)
{
    m_Name = Info->ClassName;
    if ( Info==nullptr ) return;
    ItemMap()[m_Name] = this;
}

wxsItemFactory::wxsItemFactory(wxsItemInfo* Info, wxString ClassName) :
    m_Info(Info)
{
    m_Name = ClassName;
    if ( Info==nullptr ) return;
    ItemMap()[m_Name] = this;
}

wxsItemFactory::~wxsItemFactory()
{
    if ( !m_Info ) return;
    ItemMapT::iterator it = ItemMap().find(m_Name);
    if ( it == ItemMap().end() ) return;
    if ( it->second!=this ) return;
    ItemMap().erase(it);
}

wxsItemFactory::ItemMapT& wxsItemFactory::ItemMap()
{
    static ItemMapT Map;
    return Map;
}

wxsItemFactory::ItemMapT::iterator wxsItemFactory::m_Iter(wxsItemFactory::ItemMap().end());
