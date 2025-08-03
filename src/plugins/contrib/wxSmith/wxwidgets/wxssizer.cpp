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

#include "wxssizer.h"
#include "wxsitemresdata.h"
#include "wxssizerparentqp.h"
#include "wxsflags.h"

#include <wx/dcclient.h>
#include <logmanager.h>

using namespace wxsFlags;

namespace
{
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
}

void wxsSizerExtra::OnEnumProperties(long _Flags)
{
    static const int Priority = 20;
    WXS_SIZERFLAGS_P(wxsSizerExtra,Flags,Priority);
    WXS_DIMENSION_P(wxsSizerExtra,Border,_("Border width"),_("  Dialog Units"),_T("border"),0,false,Priority);
    // We only need to add "Default Min Size", if flMinMaxSize is not set.
    // If it is set, it should already be there.
    if ( !( _Flags & flMinMaxSize ) )
        WXS_SIZE_P(wxsSizerExtra,MinSize,_("Default Min size"),_("Min Width"),_("Min Height"),_("Min size in dialog units"), _T("minsize"),Priority);
    WXS_LONG_P(wxsSizerExtra,Proportion,_("Proportion"),_T("option"),0,Priority);
}

wxString wxsSizerExtra::AllParamsCode(wxsCoderContext* Ctx)
{
    switch ( Ctx->m_Language )
    {
        case wxsCPP:
            return wxString::Format(_T("%ld, "),Proportion) +
                   wxsSizerFlagsProperty::GetString(Flags) +
                   _T(", ") << Border.GetPixelsCode(Ctx);

        case wxsUnknownLanguage: // fall-through
        default:
            wxsCodeMarks::Unknown(_T("wxsSizerExtra::AllParamsCode"),Ctx->m_Language);
    }
    return wxEmptyString;
}

wxsSizer::wxsSizer(wxsItemResData* Data,const wxsItemInfo* Info):
    wxsParent(Data,Info,flVariable|flSubclass,nullptr,nullptr)
{
    GetBaseProps()->m_IsMember = false;
}

long wxsSizer::OnGetPropertiesFlags()
{
    if ( !(wxsParent::OnGetPropertiesFlags() & flSource) )
    {
        return wxsParent::OnGetPropertiesFlags() & ~flVariable;
    }

    return wxsParent::OnGetPropertiesFlags();
}

void wxsSizer::OnBuildCreatingCode()
{
    OnBuildSizerCreatingCode();

    bool UnknownLang = false;
    int Count = GetChildCount();
    for ( int i=0; i<Count; i++ )
    {
        wxsItem* Child = GetChild(i);
        wxsSizerExtra* SizerExtra = (wxsSizerExtra*)GetChildExtra(i);

        // Using same parent as we got, sizer is not a parent window
        Child->BuildCode(GetCoderContext());

        switch ( Child->GetType() )
        {
            case wxsTWidget:
            case wxsTContainer:
            case wxsTSizer:
                switch ( GetLanguage() )
                {
                    case wxsCPP:
                    {
                        Codef(_T("%AAdd(%o, %s);\n"),i,SizerExtra->AllParamsCode(GetCoderContext()).wx_str());
                        break;
                    }

                    case wxsUnknownLanguage: // fall-through
                    default:
                    {
                        UnknownLang = true;
                    }
                }
                break;

            case wxsTSpacer:
                // Spacer is responsible for adding itself into sizer
                break;

            case wxsTTool:           // fall-through
            case wxsTInvalid:        // fall-through
            default:
                break;
        }
    }

    if ( UnknownLang )
    {
        wxsCodeMarks::Unknown(_T("wxsSizer::OnBuildCreatingCode"),GetLanguage());
    }
}

void wxsSizer::OnBuildDeclarationsCode()
{
    if ( GetCoderFlags() & flSource )
    {
        wxsItem::OnBuildDeclarationsCode();
    }
}

wxObject* wxsSizer::OnBuildPreview(wxWindow* Parent,long _Flags)
{
    wxWindow* NewParent = Parent;

    if ( !(_Flags & pfExact) )
    {
        NewParent = new wxsSizerPreview(Parent);
    }

    wxSizer* Sizer = OnBuildSizerPreview(NewParent);
    int Count = GetChildCount();
    for ( int i=0; i<Count; i++ )
    {
        wxsItem* Child = GetChild(i);
        wxsSizerExtra* SizerExtra = (wxsSizerExtra*)GetChildExtra(i);

        // We pass either Parent passed to current BuildPreview function
        // or pointer to additional parent currently created
        wxObject* ChildPreview = Child->BuildPreview(NewParent,_Flags);
        if (!ChildPreview)
            continue;

        const long SizerFlags = wxsSizerFlagsProperty::GetWxFlags(SizerExtra->Flags);
        const long Proportion = SizerExtra->Proportion;
        const long Pixels = SizerExtra->Border.GetPixels(Parent);
        wxSizer* ChildAsSizer = wxDynamicCast(ChildPreview, wxSizer);
        wxWindow* ChildAsWindow = wxDynamicCast(ChildPreview, wxWindow);
        wxSizerItem* ChildAsItem = wxDynamicCast(ChildPreview, wxSizerItem);
        if ( ChildAsSizer )
        {
            Sizer->Add(ChildAsSizer, Proportion, SizerFlags, Pixels);
        }
        else if ( ChildAsWindow )
        {
            Sizer->Add(ChildAsWindow, (SizerFlags & wxSHAPED) ? 0 : Proportion, SizerFlags, Pixels);
        }
        else if ( ChildAsItem )
        {
            ChildAsItem->SetProportion(Proportion);
            ChildAsItem->SetFlag(SizerFlags);
            ChildAsItem->SetBorder(Pixels);
            Sizer->Add(ChildAsItem);
        }
    }

    if ( !(_Flags & pfExact) )
    {
        NewParent->SetSizer(Sizer);
        if ( !GetChildCount() )
        {
            // Setting custom size for childless sizer to prevent
            // zero-size items
            NewParent->SetInitialSize(wxSize(20,20));
            NewParent->SetSizeHints(20,20);
            NewParent->SetSize(wxSize(20,20));
        }
        else
        {
            Sizer->Fit(NewParent);
            Sizer->SetSizeHints(NewParent);
        }

        return NewParent;
    }

    return Sizer;
}

wxsPropertyContainer* wxsSizer::OnBuildExtra()
{
    return new wxsSizerExtra();
}

void wxsSizer::OnAddChildQPP(wxsItem* Child,wxsAdvQPP* QPP)
{
    wxsParent::OnAddChildQPP(Child,QPP);

    int Index = GetChildIndex(Child);
    if ( Index >= 0 )
    {
        QPP->Register(new wxsSizerParentQP(QPP,(wxsSizerExtra*)GetChildExtra(Index)),_("Sizer"));
    }
}

bool wxsSizer::OnXmlReadChild(TiXmlElement* Elem,bool IsXRC,bool IsExtra)
{
    if ( cbC2U(Elem->Attribute("class")) == _T("spacer") )
    {
        wxsItem* Item = wxsItemFactory::Build(_T("Spacer"),GetResourceData());
        if ( !AddChild(Item) )
        {
            delete Item;
            return false;
        }
        RestoreExtraData(GetChildCount()-1,Elem);
        return Item->XmlRead(Elem,IsXRC,IsExtra);
    }

    return wxsParent::OnXmlReadChild(Elem,IsXRC,IsExtra);
}

bool wxsSizer::OnXmlWriteChild(int Index,TiXmlElement* Elem,bool IsXRC,bool IsExtra)
{
    wxsItem* Child = GetChild(Index);
    if ( Child->GetType() == wxsTSpacer )
    {
        StoreExtraData(Index,Elem);
        bool Ret = Child->XmlWrite(Elem,IsXRC,IsExtra);
        Elem->SetAttribute("class","spacer");
        return Ret;
    }

    return wxsParent::OnXmlWriteChild(Index,Elem,IsXRC,IsExtra);
}

wxString wxsSizer::OnXmlGetExtraObjectClass()
{
    return _T("sizeritem");
}

void wxsSizer::OnEnumItemProperties(long _Flags)
{
    OnEnumSizerProperties(_Flags);
}

void wxsSizer::OnAddItemQPP(wxsAdvQPP* QPP)
{
    OnAddSizerQPP(QPP);
}
