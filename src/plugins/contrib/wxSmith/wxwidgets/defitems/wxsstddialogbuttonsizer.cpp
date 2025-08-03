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

#include "wxsstddialogbuttonsizer.h"
#include "../wxsitemresdata.h"
#include "../wxsflags.h"

#include <wx/sizer.h>
#include <wx/panel.h>

using namespace wxsFlags;

namespace
{
    wxsRegisterItem<wxsStdDialogButtonSizer> Reg(_T("StdDialogButtonSizer"),wxsTSizer,_T("Layout"),10);

    class wxsSizerPreview: public wxPanel
    {
        public:
            wxsSizerPreview(wxWindow* Parent): wxPanel(Parent,-1,wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL)
            {
                InheritAttributes();
                Connect(wxID_ANY,wxEVT_PAINT,(wxObjectEventFunction)&wxsSizerPreview::OnPaint);
            }

        private:

            void OnPaint(cb_unused wxPaintEvent& event)
            {
                // Drawing additional border around te panel
                wxPaintDC DC(this);
                int W, H;
                GetSize(&W,&H);
                DC.SetBrush(*wxTRANSPARENT_BRUSH);
                DC.SetPen(*wxRED_PEN);
                DC.DrawRectangle(0,0,W,H);
            }
    };

    enum ButtonType
    {
        Ok = 0, Yes, No, Cancel, Aply, Save, Help, ContextHelp
    };

    const wxChar* IdNames[] =
    {
        _T("wxID_OK"),
        _T("wxID_YES"),
        _T("wxID_NO"),
        _T("wxID_CANCEL"),
        _T("wxID_APPLY"),
        _T("wxID_SAVE"),
        _T("wxID_HELP"),
        _T("wxID_CONTEXT_HELP")
    };
    const wxChar* IdLabels[] =
    {
        _T("OK Label:"),
        _T("YES Label:"),
        _T("NO Label:"),
        _T("CANCEL Label:"),
        _T("APPLY Label:"),
        _T("SAVE Label:"),
        _T("HELP Label:"),
        _T("CONTEXT_HELP Label:")
    };

    const wxWindowID IdValues[] =
    {
        wxID_OK,
        wxID_YES,
        wxID_NO,
        wxID_CANCEL,
        wxID_APPLY,
        wxID_SAVE,
        wxID_HELP,
        wxID_CONTEXT_HELP
    };

}

wxsStdDialogButtonSizer::wxsStdDialogButtonSizer(wxsItemResData* Data):
    wxsItem(Data,&Reg.Info,flVariable|flSubclass,nullptr,nullptr)
{
    GetBaseProps()->m_IsMember = false;

    for ( int i=0; i<NumButtons; i++ )
    {
        m_Use[i] = false;
        m_Label[i] = _T("");
    }

    m_Use[Ok] = true;
    m_Use[Cancel] = true;
}

long wxsStdDialogButtonSizer::OnGetPropertiesFlags()
{
    if ( !(wxsItem::OnGetPropertiesFlags() & flSource) )
    {
        return wxsItem::OnGetPropertiesFlags() & ~flVariable;
    }

    return wxsItem::OnGetPropertiesFlags();
}

void wxsStdDialogButtonSizer::OnEnumItemProperties(cb_unused long _Flags)
{
}

wxObject* wxsStdDialogButtonSizer::OnBuildPreview(wxWindow* Parent,long _Flags)
{
    wxWindow* NewParent = Parent;

    if ( !(_Flags & pfExact) )
    {
        NewParent = new wxsSizerPreview(Parent);
    }

    wxStdDialogButtonSizer* Sizer = new wxStdDialogButtonSizer();

    for ( int i=0; i<NumButtons; i++ )
    {
        if ( m_Use[i] )
        {
            wxButton* Button = new wxButton(NewParent,IdValues[i],m_Label[i]);
            Sizer->AddButton(Button);
        }
    }

    Sizer->Realize();

    if ( !(_Flags & pfExact) )
    {
        NewParent->SetSizer(Sizer);
        Sizer->Fit(NewParent);
        Sizer->SetSizeHints(NewParent);
        wxSizer* OutSizer = new wxBoxSizer(wxHORIZONTAL);
        Parent->SetSizer(OutSizer);
        OutSizer->SetSizeHints(Parent);
        return NewParent;
    }

    if ( GetParent() && GetParent()->GetType()!=wxsTSizer )
    {
        Parent->SetSizer(Sizer);
        Sizer->SetSizeHints(Parent);
    }
    return Sizer;
}

void wxsStdDialogButtonSizer::OnBuildCreatingCode()
{
    switch ( GetLanguage() )
    {
        case wxsCPP:
        {
            AddHeader(_T("<wx/sizer.h>"),GetInfo().ClassName,hfInPCH);
            AddHeader(_T("<wx/button.h>"),GetInfo().ClassName,hfLocal);

            if ( IsPointer() )
                Codef(_T("%C();\n"));

            for ( int i=0; i<NumButtons; i++ )
            {
                if ( m_Use[i] )
                {
                    Codef(_T("%AAddButton(new wxButton(%W, %v, %t));\n"),IdNames[i],m_Label[i].wx_str());
                }
            }

            Codef(_T("%ARealize();\n"));
            if ( m_Use[0] )
                Codef(_T("dynamic_cast <wxButton *> (%W->FindWindow(%v))->SetDefault();\n"), IdNames[0]);

            break;

        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsStdDialogButtonSizer::OnBuildCreatingCode"),GetLanguage());
        }
    }
}

bool wxsStdDialogButtonSizer::OnXmlRead(TiXmlElement* Element,bool IsXRC,bool IsExtra)
{
    if ( IsXRC )
    {

        for ( int i=0; i<NumButtons; i++ )
        {
            m_Use[i] = false;
            m_Label[i] = wxEmptyString;
        }

        for ( TiXmlElement* Object = Element->FirstChildElement("object"); Object; Object = Object->NextSiblingElement("object") )
        {
            TiXmlElement* Button = Object->FirstChildElement("object");
            if ( !Button ) continue;
            if ( cbC2U(Button->Attribute("class")) != _T("wxButton") ) continue;

            wxString Id = cbC2U(Button->Attribute("name"));
            for ( int i=0; i<NumButtons; i++ )
            {
                if ( Id == IdNames[i] )
                {
                    m_Use[i] = true;
                    TiXmlElement* Label = Button->FirstChildElement("label");
                    if ( Label )
                    {
                        m_Label[i] = cbC2U(Label->GetText());
                    }
                    break;
                }
            }
        }
    }

    return wxsItem::OnXmlRead(Element,IsXRC,IsExtra);
}

bool wxsStdDialogButtonSizer::OnXmlWrite(TiXmlElement* Element,bool IsXRC,bool IsExtra)
{
    if ( IsXRC )
    {
        for ( int i=0; i<NumButtons; i++ )
        {
            if ( m_Use[i] )
            {
                TiXmlElement* Object = Element->InsertEndChild(TiXmlElement("object"))->ToElement();
                Object->SetAttribute("class","button");
                TiXmlElement* Button = Object->InsertEndChild(TiXmlElement("object"))->ToElement();
                Button->SetAttribute("class","wxButton");
                Button->SetAttribute("name",cbU2C(IdNames[i]));
                Button->InsertEndChild(TiXmlElement("label"))->InsertEndChild(TiXmlText(cbU2C(m_Label[i])));
            }
        }
    }

    return wxsItem::OnXmlWrite(Element,IsXRC,IsExtra);
}

void wxsStdDialogButtonSizer::OnAddExtraProperties(wxsPropertyGridManager* Grid )
{
    for ( int i=0; i<NumButtons; i++ )
    {
        m_UseId[i] = Grid->Append(new wxBoolProperty(IdNames[i],wxPG_LABEL,m_Use[i]));
#if wxCHECK_VERSION(3, 3, 0)
        Grid->SetPropertyAttribute(m_UseId[i],wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
#else
        Grid->SetPropertyAttribute(m_UseId[i],wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
#endif
        m_LabelId[i] = Grid->Append(new wxStringProperty(IdLabels[i],wxPG_LABEL,m_Label[i]));
    }
    wxsItem::OnAddExtraProperties(Grid);
}

void wxsStdDialogButtonSizer::OnExtraPropertyChanged(wxsPropertyGridManager* Grid,wxPGId Id)
{
    for ( int i=0; i<NumButtons; i++ )
    {
        if ( Id == m_UseId[i] )
        {
            m_Use[i] = Grid->GetPropertyValueAsBool(Id);
            NotifyPropertyChange(true);
            return;
        }

        if ( Id == m_LabelId[i] )
        {
            m_Label[i] = Grid->GetPropertyValueAsString(Id);
            NotifyPropertyChange(true);
            return;
        }
    }

    wxsItem::OnExtraPropertyChanged(Grid,Id);
}

void wxsStdDialogButtonSizer::OnBuildDeclarationsCode()
{
    // Add declaration only when not using XRC file
    if ( GetCoderFlags() & flSource )
    {
        wxsItem::OnBuildDeclarationsCode();
    }
}


