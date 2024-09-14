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

#ifndef WXSSIZERPARENTQP_H
#define WXSSIZERPARENTQP_H

//(*Headers(wxsSizerParentQP)
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
//*)

#include "wxssizer.h"
#include "../wxsadvqppchild.h"

class wxsSizerParentQP: public wxsAdvQPPChild
{
    public:

        wxsSizerParentQP(wxsAdvQPP* parent,wxsSizerExtra* Extra);
        virtual ~wxsSizerParentQP();

        //(*Identifiers(wxsSizerParentQP)
        static const wxWindowID ID_CHECKBOX1;
        static const wxWindowID ID_CHECKBOX2;
        static const wxWindowID ID_CHECKBOX8;
        static const wxWindowID ID_CHECKBOX3;
        static const wxWindowID ID_CHECKBOX4;
        static const wxWindowID ID_SPINCTRL1;
        static const wxWindowID ID_CHECKBOX7;
        static const wxWindowID ID_RADIOBUTTON4;
        static const wxWindowID ID_RADIOBUTTON5;
        static const wxWindowID ID_RADIOBUTTON6;
        static const wxWindowID ID_RADIOBUTTON7;
        static const wxWindowID ID_RADIOBUTTON8;
        static const wxWindowID ID_RADIOBUTTON9;
        static const wxWindowID ID_RADIOBUTTON10;
        static const wxWindowID ID_RADIOBUTTON11;
        static const wxWindowID ID_RADIOBUTTON12;
        static const wxWindowID ID_STATICLINE1;
        static const wxWindowID ID_CHECKBOX6;
        static const wxWindowID ID_CHECKBOX5;
        static const wxWindowID ID_SPINCTRL2;
        //*)

    protected:

        //(*Handlers(wxsSizerParentQP)
        void OnBrdChange(wxCommandEvent& event);
        void OnBrdSizeChange(wxSpinEvent& event);
        void OnPlaceChange(wxCommandEvent& event);
        void OnProportionChange(wxSpinEvent& event);
        void OnBrdDlgChange(wxCommandEvent& event);
        void OnBrdAll(wxCommandEvent& event);
        //*)

        //(*Declarations(wxsSizerParentQP)
        wxCheckBox* BrdAll;
        wxCheckBox* BrdBottom;
        wxCheckBox* BrdDlg;
        wxCheckBox* BrdLeft;
        wxCheckBox* BrdRight;
        wxCheckBox* BrdTop;
        wxCheckBox* PlaceExp;
        wxCheckBox* PlaceShp;
        wxRadioButton* PlaceCB;
        wxRadioButton* PlaceCC;
        wxRadioButton* PlaceCT;
        wxRadioButton* PlaceLB;
        wxRadioButton* PlaceLC;
        wxRadioButton* PlaceLT;
        wxRadioButton* PlaceRB;
        wxRadioButton* PlaceRC;
        wxRadioButton* PlaceRT;
        wxSpinCtrl* BrdSize;
        wxSpinCtrl* Proportion;
        wxStaticLine* StaticLine1;
        //*)

        virtual void Update();

    private:

        void ReadData();
        void SaveData();

        wxsSizerExtra* m_Extra;
        long m_ParentOrientation;

        DECLARE_EVENT_TABLE()
};

#endif
