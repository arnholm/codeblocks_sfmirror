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

#include "wxstoolbaritem.h"

namespace
{
    class InfoHandler: public wxsItemInfo
    {
        public:

            InfoHandler(): m_TreeImage(_T("images/wxsmith/wxToolBarItem16.png"),true)
            {
                ClassName      = _T("wxToolBarToolBase");
                Type           = wxsTTool;
                License        = _("wxWidgets license");
                Author         = _("wxWidgets team");
                Email          = _T("");
                Site           = _T("www.wxwidgets.org");
                Category       = _T("");
                Priority       = 0;
                DefaultVarName = _T("ToolBarItem");
                Languages      = wxsCPP;
                VerHi          = 2;
                VerLo          = 8;
                AllowInXRC     = true;

                // TODO: This code should be more generic since it may quickly
                //       become invalid
                wxString DataPath = ConfigManager::GetDataFolder() + _T("/images/wxsmith/");
                Icon32.LoadFile(DataPath+_T("wxToolBarItem32.png"),wxBITMAP_TYPE_PNG);
                Icon16.LoadFile(DataPath+_T("wxToolBarItem16.png"),wxBITMAP_TYPE_PNG);
                TreeIconId = m_TreeImage.GetIndex();
            };

            wxsAutoResourceTreeImage m_TreeImage;
    } Info;

    WXS_EV_BEGIN(wxsToolBarItemEvents)
        WXS_EVI(EVT_TOOL,wxEVT_COMMAND_TOOL_CLICKED,wxCommandEvent,Clicked)
        WXS_EVI(EVT_TOOL_RCLICKED,wxEVT_COMMAND_TOOL_RCLICKED,wxCommandEvent,RClicked)
    WXS_EV_END()
}


wxsToolBarItem::wxsToolBarItem(wxsItemResData* Data, ToolType Tool):
    wxsTool(
        Data,
        &Info,
        (Tool == Separator || Tool == Stretchable) ? nullptr : wxsToolBarItemEvents,
        nullptr,
        (Tool == Separator || Tool == Stretchable) ? 0 : (flVariable|flId)
    ),
    m_Type(Tool)
{
}

void wxsToolBarItem::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:

            switch ( m_Type )
            {
                case Control:
                case Normal:
                case Radio:
                case Check:
                {
                    const wxChar* ItemType;
                    switch ( m_Type )
                    {
                        case Control:
                        case Normal: // fall-through
                            ItemType = _T("wxITEM_NORMAL");
                            break;
                        case Radio:
                            ItemType = _T("wxITEM_RADIO");
                            break;
                        case Check:
                        case Separator:
                        case Stretchable: // fall-through
                        default:
                            ItemType = _T("wxITEM_CHECK");
                    }

                    wxString BitmapCode  = m_Bitmap.BuildCode(true,_T(""),GetCoderContext(),_T("wxART_TOOLBAR"));
                    wxString Bitmap2Code = m_Bitmap2.BuildCode(true,_T(""),GetCoderContext(),_T("wxART_TOOLBAR"));
                    if ( BitmapCode.IsEmpty() )  BitmapCode  = _T("wxNullBitmap");
                    if ( Bitmap2Code.IsEmpty() ) Bitmap2Code = _T("wxNullBitmap");

                    Codef(_T("%v = %MAddTool(%I, %t, %i, %i, %s, %t, %t);\n"),
                          GetVarName().wx_str(),
                          m_Label.wx_str(),
                          &m_Bitmap,_T("wxART_TOOLBAR"),
                          &m_Bitmap2,_T("wxART_TOOLBAR"),
                          ItemType,
                          m_ToolTip.wx_str(),
                          m_HelpText.wx_str());
                    break;
                }

                case Separator:
                {
                    Codef(_T("%MAddSeparator();\n"));
                    break;
                }

                case Stretchable:
                {
                    Codef(_T("%MAddStretchableSpace();\n"));
                    break;
                }

                default:
                    break;

            }
            break;

        case wxsUnknownLanguage: // fall-through
        default:
            wxsCodeMarks::Unknown(_T("wxsToolBarItem::OnBuildCreatingCode"),GetLanguage());
    }
}

void wxsToolBarItem::OnEnumToolProperties(cb_unused long _Flags)
{
    switch ( m_Type )
    {
        case Normal:
        case Radio:
        case Check:
            WXS_SHORT_STRING(wxsToolBarItem,m_Label,_("Label"),_T("label"),_T(""),true);
            WXS_BITMAP(wxsToolBarItem,m_Bitmap,_("Bitmap"),_T("bitmap"),_T("wxART_TOOLBAR"));
            WXS_BITMAP(wxsToolBarItem,m_Bitmap2,_("Disabled bitmap"),_T("bitmap2"),_T("wxART_TOOLBAR"));
            WXS_STRING(wxsToolBarItem,m_ToolTip,_("Tooltip"),_T("tooltip"),_T(""),false);
            WXS_STRING(wxsToolBarItem,m_HelpText,_("Help text"),_T("longhelp"),_T(""),false);
            break;

        case Separator: // fall-through
        case Stretchable: // fall-through
        default:
            break;
    }
}

bool wxsToolBarItem::OnXmlWrite(TiXmlElement* Element,bool IsXRC,bool IsExtra)
{
    bool Ret = wxsParent::OnXmlWrite(Element,IsXRC,IsExtra);

    if ( IsXRC )
    {
        Element->SetAttribute("class", "tool");

        switch ( m_Type )
        {
            case Separator:
                Element->SetAttribute("class", "separator");
                break;

            case Stretchable:
                Element->SetAttribute("class", "stretchable");
                break;

            case Radio:
                Element->InsertEndChild(TiXmlElement("radio"))->ToElement()->InsertEndChild(TiXmlText("1"));
                break;

            case Check:
                Element->InsertEndChild(TiXmlElement("check"))->ToElement()->InsertEndChild(TiXmlText("1"));
                break;

            case Normal: // fall-through
            default:
                break;
        }
    }

    return Ret;
}

bool wxsToolBarItem::OnXmlRead(TiXmlElement* Element,bool IsXRC,bool IsExtra)
{
    bool Ret = wxsParent::OnXmlRead(Element,IsXRC,IsExtra);

    if ( IsXRC )
    {
        wxString Class = cbC2U(Element->Attribute("class"));
        if ( Class == _T("separator") )
        {
            m_Type = Separator;
        }
        else if ( Class == _T("stretchable") )
        {
            m_Type = Stretchable;
        }
        else
        {
            // This will handle both wxMenu and wxToolBarItem
            TiXmlElement* Node = Element->FirstChildElement("radio");
            if ( Node && (cbC2U(Node->GetText())==_T("1")) )
            {
                m_Type = Radio;
            }
            else if ( (Node = Element->FirstChildElement("check")) &&
                      (cbC2U(Node->GetText())==_T("1")) )
            {
                m_Type = Check;
            }
            else
            {
                m_Type = Normal;
            }
        }
    }

    return Ret;
}

bool wxsToolBarItem::OnCanAddToParent(wxsParent* Parent,bool ShowMessage)
{
    if ( Parent->GetClassName() != _T("wxToolBar") )
    {
        if ( ShowMessage )
        {
            cbMessageBox(_("Toolbar items can be used inside wxToolBar only"));
        }
        return false;
    }
    return true;
}

wxString wxsToolBarItem::OnGetTreeLabel(cb_unused int& Image)
{
    switch ( m_Type )
    {
        case Separator:
            return _T("--------");

        case Stretchable:
            return _T("<------>");

        case Radio:  // fall-through
        case Check:  // fall-through
        case Normal: // fall-through
        default:
            return _("Item: ") + m_Label;
    }
}

void wxsToolBarItem::OnBuildDeclarationsCode()
{
    if (m_Type == Separator || m_Type == Stretchable)
        return;
    wxsItem::OnBuildDeclarationsCode();
}
