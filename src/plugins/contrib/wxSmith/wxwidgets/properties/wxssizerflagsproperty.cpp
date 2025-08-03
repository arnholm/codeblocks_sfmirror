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

#include "wxssizerflagsproperty.h"

#include <wx/tokenzr.h>
#include <globals.h>

// Helper macro for fetching variables
#define FLAGS   wxsVARIABLE(Object,Offset,long)

#define BORDERIND   0x01
#define ALIGNHIND   0x02
#define ALIGNVIND   0x03
#define ALIGNCIND   0x04
#define EXPANDIND   0x05
#define SHAPEDIND   0x06
#define FIXEDIND    0x07

wxsSizerFlagsProperty::wxsSizerFlagsProperty(long _Offset,int Priority):
        wxsProperty(_("Borders"),_T("flag"),Priority),
        Offset(_Offset)
{
}

void wxsSizerFlagsProperty::PGCreate(wxsPropertyContainer* Object,wxPropertyGridManager* Grid,wxPGId Parent)
{
    // Creating border flags

    if ( (FLAGS & BorderMask) == BorderMask )
    {
        FLAGS |= BorderAll | BorderPrevAll;
    }
    else
    {
        FLAGS &= ~BorderAll & ~BorderPrevAll;
    }

    wxPGChoices PGC;
    PGC.Add(_("Top"),BorderTop);
    PGC.Add(_("Bottom"),BorderBottom);
    PGC.Add(_("Left"),BorderLeft);
    PGC.Add(_("Right"),BorderRight);
    PGC.Add(_("All"),BorderAll);
    wxPGId ID1 = Grid->AppendIn(Parent,new wxFlagsProperty(_("Border"),wxPG_LABEL,PGC,FLAGS&(BorderMask|BorderAll)));
    PGRegister(Object,Grid,ID1,BORDERIND);

    wxPGChoices PGC2;
    PGC2.Add(wxString(),AlignNot);
    PGC2.Add(_("Center"),AlignCMask);
    wxPGId ID2 = Grid->AppendIn(Parent,new wxEnumProperty(_("Center align"),wxPG_LABEL,PGC2,FLAGS&AlignCMask));
    PGRegister(Object,Grid,ID2,ALIGNCIND);

    wxPGChoices PGC3;
    PGC3.Add(wxString(),AlignNot);
    PGC3.Add(_("Left"),AlignLeft);
    PGC3.Add(_("Center"),AlignCenterHorizontal);
    PGC3.Add(_("Right"),AlignRight);
    wxPGId ID3 = Grid->AppendIn(Parent,new wxEnumProperty(_("Horizontal align"),wxPG_LABEL,PGC3,FLAGS&AlignHMask));
    PGRegister(Object,Grid,ID3,ALIGNHIND);

    wxPGChoices PGC4;
    PGC4.Add(wxString(),AlignNot);
    PGC4.Add(_("Top"),AlignTop);
    PGC4.Add(_("Center"),AlignCenterVertical);
    PGC4.Add(_("Bottom"),AlignBottom);
    wxPGId ID4 = Grid->AppendIn(Parent,new wxEnumProperty(_("Vertical align"),wxPG_LABEL,PGC4,FLAGS&AlignVMask));
    PGRegister(Object,Grid,ID4,ALIGNVIND);

    wxPGId ID5 = Grid->AppendIn(Parent,new wxBoolProperty(_("Expand"),wxPG_LABEL,(FLAGS&Expand)!=0));
    wxPGId ID6 = Grid->AppendIn(Parent,new wxBoolProperty(_("Shaped"),wxPG_LABEL,(FLAGS&Shaped)!=0));
    wxPGId ID7 = Grid->AppendIn(Parent,new wxBoolProperty(_("Fixed min size"),wxPG_LABEL,(FLAGS&FixedMinSize)!=0));
    PGRegister(Object,Grid,ID5,EXPANDIND);
    PGRegister(Object,Grid,ID6,SHAPEDIND);
    PGRegister(Object,Grid,ID7,FIXEDIND);

#if wxCHECK_VERSION(3, 3, 0)
    Grid->SetPropertyAttribute(ID1,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(ID2,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(ID3,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(ID4,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(ID5,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(ID6,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
    Grid->SetPropertyAttribute(ID7,wxPG_BOOL_USE_CHECKBOX,1L,wxPGPropertyValuesFlags::Recurse);
#else
    Grid->SetPropertyAttribute(ID1,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(ID2,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(ID3,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(ID4,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(ID5,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(ID6,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
    Grid->SetPropertyAttribute(ID7,wxPG_BOOL_USE_CHECKBOX,1L,wxPG_RECURSE);
#endif
}

bool wxsSizerFlagsProperty::PGRead(wxsPropertyContainer* Object,wxPropertyGridManager* Grid,wxPGId Id,long Index)
{
    switch ( Index )
    {
        case BORDERIND:
            {
                long NewVal = Grid->GetPropertyValue(Id).GetLong();
                bool ThisAll = (NewVal&BorderAll) != 0;
                bool PrevAll = (FLAGS&BorderPrevAll) != 0;
                // Checking if "all" flag has changed
                if ( ThisAll != PrevAll )
                {
                    if ( ThisAll )
                    {
                        FLAGS |= BorderMask | BorderAll | BorderPrevAll;
                    }
                    else
                    {
                        FLAGS &= ~BorderMask & ~BorderAll & ~BorderPrevAll;
                    }
                }
                else
                {
                    NewVal &= BorderMask;
                    FLAGS &= ~BorderMask;
                    FLAGS |= NewVal;
                    if ( NewVal == BorderMask )
                    {
                        FLAGS |= BorderAll | BorderPrevAll;
                    }
                    else
                    {
                        FLAGS &= ~BorderAll & ~BorderPrevAll;
                    }
                }
            }
            break;

        case ALIGNHIND:
            FLAGS &= ~AlignHMask;
            FLAGS |= (Grid->GetPropertyValue(Id).GetLong() & AlignHMask);
            break;

        case ALIGNVIND:
            FLAGS &= ~AlignVMask;
            FLAGS |= (Grid->GetPropertyValue(Id).GetLong() & AlignVMask);
            break;

        case ALIGNCIND:
            FLAGS &= ~AlignCMask;
            FLAGS |= (Grid->GetPropertyValue(Id).GetLong() & AlignCMask);
            break;

        case EXPANDIND:
            if ( Grid->GetPropertyValue(Id).GetBool() )
            {
                FLAGS |= Expand;
            }
            else
            {
                FLAGS &= ~Expand;
            }
            break;

        case SHAPEDIND:
            if ( Grid->GetPropertyValue(Id).GetBool() )
            {
                FLAGS |= Shaped;
            }
            else
            {
                FLAGS &= ~Shaped;
            }
            break;

        case FIXEDIND:
            if ( Grid->GetPropertyValue(Id).GetBool() )
            {
                FLAGS |= FixedMinSize;
            }
            else
            {
                FLAGS &= ~FixedMinSize;
            }
            break;

        default:
            return false;
    }

    return true;
}

bool wxsSizerFlagsProperty::PGWrite(wxsPropertyContainer* Object,wxPropertyGridManager* Grid,wxPGId Id,long Index)
{
    FixFlags(FLAGS);
    switch ( Index )
    {
        case BORDERIND:
            if ( (FLAGS & BorderMask) == BorderMask )
            {
                FLAGS |= BorderAll | BorderPrevAll;
            }
            else
            {
                FLAGS &= ~BorderAll & ~BorderPrevAll;
            }
            Grid->SetPropertyValue(Id,FLAGS&(BorderMask|BorderAll));
            break;

        case ALIGNHIND:
            Grid->SetPropertyValue(Id,FLAGS&AlignHMask);
            break;

        case ALIGNVIND:
            Grid->SetPropertyValue(Id,FLAGS&AlignVMask);
            break;

        case ALIGNCIND:
            Grid->SetPropertyValue(Id,FLAGS&AlignCMask);
            break;

        case EXPANDIND:
            Grid->SetPropertyValue(Id,(FLAGS&Expand)!=0);
            break;

        case SHAPEDIND:
            Grid->SetPropertyValue(Id,(FLAGS&Shaped)!=0);
            break;

        case FIXEDIND:
            Grid->SetPropertyValue(Id,(FLAGS&FixedMinSize)!=0);
            break;

        default:
            return false;
    }
    return true;
}

long wxsSizerFlagsProperty::GetParentOrientation(TiXmlElement* Element)
{
    if ( Element->Parent() && Element->Parent()->Parent() )
    {
        TiXmlNode* p = Element->Parent()->Parent();
        TiXmlElement* e = p->ToElement();
        if ( e &&( !strcmp(e->Attribute("class"), "wxBoxSizer") || !strcmp(e->Attribute("class"), "wxStaticBoxSizer") ) )
        {
            if ( p->FirstChild("orient") && p->FirstChild("orient")->ToElement() )
            {
                const char* value = p->FirstChild("orient")->ToElement()->GetText();
                if ( !strcmp(value, "wxVERTICAL") )
                    return ParentAlignVertical;
                else if ( !strcmp(value, "wxHORIZONTAL") )
                    return ParentAlignHorizontal;
                else return 0;
            }
            else
                return ParentAlignHorizontal;
        }
    }
    return 0;
}

bool wxsSizerFlagsProperty::XmlRead(wxsPropertyContainer* Object,TiXmlElement* Element)
{
    if ( !Element )
    {
        FLAGS = AlignNot;
        return false;
    }

    FLAGS &= ~ParentAlignMask;
    FLAGS |= GetParentOrientation(Element);

    const char* Text = Element->GetText();
    if ( !Text )
    {
        FLAGS = AlignNot;
        return false;
    }
    FLAGS = ParseString(cbC2U(Text));
    return true;
}

bool wxsSizerFlagsProperty::XmlWrite(wxsPropertyContainer* Object,TiXmlElement* Element)
{
    if (Element!=nullptr)
    {
        FLAGS &= ~ParentAlignMask;
        FLAGS |= GetParentOrientation(Element);

        FixFlags(FLAGS);

        Element->InsertEndChild(TiXmlText(cbU2C(GetString(FLAGS))));
    }

    return true;
}

bool wxsSizerFlagsProperty::PropStreamRead(wxsPropertyContainer* Object,wxsPropertyStream* Stream)
{
    if ( Stream->GetLong(GetDataName(),FLAGS,AlignNot) )
    {
        FixFlags(FLAGS);
        return true;
    }
    return false;
}

bool wxsSizerFlagsProperty::PropStreamWrite(wxsPropertyContainer* Object,wxsPropertyStream* Stream)
{
    return Stream->PutLong(GetDataName(),FLAGS,AlignNot);
}

long wxsSizerFlagsProperty::ParseString(const wxString& String)
{
    long Flags = 0;
    wxStringTokenizer Tkn(String, _T("| \t\n"), wxTOKEN_STRTOK);
    while ( Tkn.HasMoreTokens() )
    {
        wxString Flag = Tkn.GetNextToken();
             if ( Flag == _T("wxTOP")           ) Flags |= BorderTop;
        else if ( Flag == _T("wxNORTH")         ) Flags |= BorderTop;
        else if ( Flag == _T("wxBOTTOM")        ) Flags |= BorderBottom;
        else if ( Flag == _T("wxSOUTH")         ) Flags |= BorderBottom;
        else if ( Flag == _T("wxLEFT")          ) Flags |= BorderLeft;
        else if ( Flag == _T("wxWEST")          ) Flags |= BorderLeft;
        else if ( Flag == _T("wxRIGHT")         ) Flags |= BorderRight;
        else if ( Flag == _T("wxEAST")          ) Flags |= BorderLeft;
        else if ( Flag == _T("wxALL")           ) Flags |= BorderMask;
        else if ( Flag == _T("wxEXPAND")        ) Flags |= Expand;
        else if ( Flag == _T("wxGROW")          ) Flags |= Expand;
        else if ( Flag == _T("wxSHAPED")        ) Flags |= Shaped;
        else if ( Flag == _T("wxFIXED_MINSIZE") ) Flags |= FixedMinSize;
        else if ( Flag == _T("wxALIGN_CENTER")  ) Flags |= AlignCenterHorizontal | AlignCenterVertical;
        else if ( Flag == _T("wxALIGN_CENTRE")  ) Flags |= AlignCenterHorizontal | AlignCenterVertical;
        else if ( Flag == _T("wxALIGN_LEFT")    ) Flags |= AlignLeft;
        else if ( Flag == _T("wxALIGN_RIGHT")   ) Flags |= AlignRight;
        else if ( Flag == _T("wxALIGN_TOP")     ) Flags |= AlignTop;
        else if ( Flag == _T("wxALIGN_BOTTOM")  ) Flags |= AlignBottom;
        else if ( Flag == _T("wxALIGN_CENTER_HORIZONTAL") ) Flags |= AlignCenterHorizontal;
        else if ( Flag == _T("wxALIGN_CENTRE_HORIZONTAL") ) Flags |= AlignCenterHorizontal;
        else if ( Flag == _T("wxALIGN_CENTER_VERTICAL")   ) Flags |= AlignCenterVertical;
        else if ( Flag == _T("wxALIGN_CENTRE_VERTICAL")   ) Flags |= AlignCenterVertical;
    }
    FixFlags(Flags);
    return Flags;
}

wxString wxsSizerFlagsProperty::GetString(long Flags)
{
    wxString Result;

    if ( (Flags & BorderMask) == BorderMask )
    {
        Result = _T("wxALL|");
    }
    else
    {
        if ( Flags & BorderTop    ) Result.Append(_T("wxTOP|"));
        if ( Flags & BorderBottom ) Result.Append(_T("wxBOTTOM|"));
        if ( Flags & BorderLeft   ) Result.Append(_T("wxLEFT|"));
        if ( Flags & BorderRight  ) Result.Append(_T("wxRIGHT|"));
    }

    if ( Flags & Expand )
    {
        Result.Append(_T("wxEXPAND|"));
    }
    else
    {
        if ( Flags & AlignLeft              ) Result.Append(_T("wxALIGN_LEFT|"));
        if ( Flags & AlignRight             ) Result.Append(_T("wxALIGN_RIGHT|"));
        if ( Flags & AlignTop               ) Result.Append(_T("wxALIGN_TOP|"));
        if ( Flags & AlignBottom            ) Result.Append(_T("wxALIGN_BOTTOM|"));
        if ( Flags & AlignCenterHorizontal  ) Result.Append(_T("wxALIGN_CENTER_HORIZONTAL|"));
        if ( Flags & AlignCenterVertical    ) Result.Append(_T("wxALIGN_CENTER_VERTICAL|"));
    }
    if ( Flags & Shaped                 ) Result.Append(_T("wxSHAPED|"));
    if ( Flags & FixedMinSize           ) Result.Append(_T("wxFIXED_MINSIZE|"));

    if ( Result.empty() )
    {
        // returning empty-string breaks .wxs-files, returning 0 breaks .xrc
        // wxALIGN_NOT is not parsed by xrc-handler either, so we return wxALIGN_LEFT,
        // which is 0 (or wxALIGN_NOT) internally
        return _T("wxALIGN_LEFT");
    }

    Result.RemoveLast();
    return Result;
}

long wxsSizerFlagsProperty::GetWxFlags(long Flags)
{
    long Result = 0;

    if ( Flags & BorderTop             ) Result |= wxTOP;
    if ( Flags & BorderBottom          ) Result |= wxBOTTOM;
    if ( Flags & BorderLeft            ) Result |= wxLEFT;
    if ( Flags & BorderRight           ) Result |= wxRIGHT;
    if ( Flags & Expand                ) Result |= wxEXPAND;
    if ( Flags & Shaped                ) Result |= wxSHAPED;
    if ( Flags & FixedMinSize          ) Result |= wxFIXED_MINSIZE;
    if ( Flags & AlignLeft             ) Result |= wxALIGN_LEFT;
    if ( Flags & AlignRight            ) Result |= wxALIGN_RIGHT;
    if ( Flags & AlignTop              ) Result |= wxALIGN_TOP;
    if ( Flags & AlignBottom           ) Result |= wxALIGN_BOTTOM;
    if ( Flags & AlignCenterHorizontal ) Result |= wxALIGN_CENTER_HORIZONTAL;
    if ( Flags & AlignCenterVertical   ) Result |= wxALIGN_CENTER_VERTICAL;

    return Result;
}

void wxsSizerFlagsProperty::FixFlags(long& Flags)
{
    // expanded elements can not be aligned in any direction
    if ( Flags & Expand )
    {
        Flags &= ~(AlignHMask|AlignVMask);
        return;
    }
    // center-alignment in both directions) is kept
    if ( (Flags & AlignCMask) == AlignCMask)
        return;

    if ( Flags & ParentAlignVertical )
    {
        Flags &= ~AlignVMask;
    }

    if ( Flags & ParentAlignHorizontal )
    {
        Flags &= ~AlignHMask;
    }

    if ( Flags & AlignLeft )
    {
        Flags &= ~(AlignCenterHorizontal|AlignRight);
    }
    else if ( Flags & AlignCenterHorizontal )
    {
        Flags &= ~AlignRight;
    }

    if ( Flags & AlignTop )
    {
        Flags &= ~(AlignCenterVertical|AlignBottom);
    }
    else if ( Flags & AlignCenterVertical )
    {
        Flags &= ~AlignBottom;
    }
}
