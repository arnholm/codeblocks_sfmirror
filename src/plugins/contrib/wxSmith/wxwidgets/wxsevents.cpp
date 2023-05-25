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

#include "wxsevents.h"
#include "wxsitem.h"
#include "wxsitemresdata.h"
#include "wxsflags.h"

#include <logmanager.h>

#define HandlerXmlElementName   "handler"
#define HandlerXmlEntryName     "entry"
#define HandlerXmlTypeName      "type"
#define HandlerXmlFunctionName  "function"

using namespace wxsFlags;

wxsEvents::wxsEvents(const wxsEventDesc* Events,wxsItem* Item):
    m_Item(Item),
    m_EventArray(Events),
    m_Count(0)
{
    // Counting number of events
    if ( m_EventArray )
    {
        for ( const wxsEventDesc* Event = m_EventArray;
              Event->ET != wxsEventDesc::EndOfList;
              Event++ )
        {
            m_Count++;
        }
    }

    m_Functions.SetCount(m_Count);
}

void wxsEvents::XmlLoadFunctions(TiXmlElement* Element)
{
    for ( TiXmlElement* Handler = Element->FirstChildElement(HandlerXmlElementName);
          Handler;
          Handler = Handler->NextSiblingElement(HandlerXmlElementName) )
    {
        const char* EntryName    = Handler->Attribute(HandlerXmlEntryName);
        const char* TypeName     = Handler->Attribute(HandlerXmlTypeName);
        const char* FunctionName = Handler->Attribute(HandlerXmlFunctionName);

        // There must be function name, otherwise this handler definition is useless
        if ( !FunctionName ) continue;

        if ( EntryName )
        {
            // Function given by event entry
            wxString Name = cbC2U(EntryName);
            for ( int i=0; i<m_Count; i++ )
            {
                if ( (m_EventArray[i].Entry == Name) && (m_EventArray[i].ET != wxsEventDesc::Category) )
                {
                    m_Functions[i] = cbC2U(FunctionName);
                    break;
                }
            }
        }
        else
        {
            // Function given by event type
            wxString Name = cbC2U(TypeName);
            for ( int i=0; i<m_Count; i++ )
            {
                if ( (m_EventArray[i].Type == Name) && (m_EventArray[i].ET != wxsEventDesc::Category) )
                {
                    m_Functions[i] = cbC2U(FunctionName);
                }
            }
        }
    }
}

void wxsEvents::XmlSaveFunctions(TiXmlElement* Element)
{
    for ( int i=0; i<m_Count; i++ )
    {
        if ( !m_Functions[i].empty() && (m_EventArray[i].ET != wxsEventDesc::Category) )
        {
            TiXmlElement* Handler = Element->InsertEndChild( TiXmlElement(HandlerXmlElementName) ) -> ToElement();
            Handler->SetAttribute(HandlerXmlFunctionName,cbU2C(m_Functions[i]));
            if ( !m_EventArray[i].Entry.empty() )
            {
                Handler->SetAttribute(HandlerXmlEntryName,cbU2C(m_EventArray[i].Entry));
            }
            else
            {
                Handler->SetAttribute(HandlerXmlTypeName,cbU2C(m_EventArray[i].Type));
            }
        }
    }
}

void wxsEvents::GenerateBindingCode(wxsCoderContext* Context,const wxString& IdString,const wxString& VarNameString)
{
    const wxString ClassName(m_Item->GetResourceData()->GetClassName());
    ConfigManager* cfg = Manager::Get()->GetConfigManager("wxsmith");
    const bool UseBind = cfg->ReadBool("/usebind", false);
    switch ( Context->m_Language )
    {
        case wxsCPP:
        {
            for ( int i=0; i<m_Count; i++ )
            {
                if ( !m_Functions[i].empty() )
                {
                    const wxString Method("&" + ClassName + "::" + m_Functions[i]);
                    const wxString Type(m_EventArray[i].Type);
                    switch ( m_EventArray[i].ET )
                    {
                        case wxsEventDesc::Id:
                            if (UseBind)
                                Context->m_EventsConnectingCode << "Bind(" << Type << ", " << Method << ", this, " << IdString << ");\n";
                            else
                                Context->m_EventsConnectingCode << "Connect(" << IdString << ", " << Type << ", (wxObjectEventFunction)" << Method << ");\n";
                            break;

                        case wxsEventDesc::NoId:

                            if ( Context->m_Flags & flRoot )
                            {
                                if (UseBind)  // If this is root item, it's threaded as Id one
                                    Context->m_EventsConnectingCode << "Bind(" << Type << ", " << Method << ", this);\n";
                                else
                                    Context->m_EventsConnectingCode << "Connect(" << Type << ", (wxObjectEventFunction)" << Method << ");\n";
                            }
                            else
                            {
                                if (UseBind)
                                    Context->m_EventsConnectingCode << VarNameString << "->Bind(" << Type << ", " << Method << ", this);\n";
                                else
                                    Context->m_EventsConnectingCode << VarNameString << "->Connect(" << Type << ", (wxObjectEventFunction)" << Method << ", NULL, this);\n";
                            }
                            break;

                        case wxsEventDesc::IdRange:   // fall-through
                        case wxsEventDesc::Category:  // fall-through
                        case wxsEventDesc::EndOfList: // fall-through
                        default:
                            break;
                    }
                }
            }
            return;
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsEvents::GenerateBindingCode"),Context->m_Language);
        }
    }
}
