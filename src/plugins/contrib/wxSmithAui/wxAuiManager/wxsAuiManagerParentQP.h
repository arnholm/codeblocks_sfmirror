/*
* This file is part of wxSmithAui plugin for Code::Blocks Studio
* Copyright (C) 2008-2009  César Fernández Domínguez
*
* wxSmithAui is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* wxSmithAui is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with wxSmithAui. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WXSAUIMANAGERPARENTQP_H
#define WXSAUIMANAGERPARENTQP_H

//(*Headers(wxsAuiManagerParentQP)
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include "wxsAuiManager.h"
#include <wxsadvqppchild.h>

class wxsAuiManagerParentQP: public wxsAdvQPPChild
{
    public:

        wxsAuiManagerParentQP(wxsAdvQPP* parent,wxsAuiPaneInfoExtra* Extra, wxWindowID id = -1);
        virtual ~wxsAuiManagerParentQP();

        //(*Declarations(wxsAuiManagerParentQP)
        wxCheckBox* BottomDockable;
        wxCheckBox* CaptionVisible;
        wxCheckBox* CloseButton;
        wxCheckBox* DestroyOnClose;
        wxCheckBox* DockBottom;
        wxCheckBox* DockCenter;
        wxCheckBox* DockFixed;
        wxCheckBox* DockLeft;
        wxCheckBox* DockRight;
        wxCheckBox* DockTop;
        wxCheckBox* Docked;
        wxCheckBox* Floatable;
        wxCheckBox* LeftDockable;
        wxCheckBox* MaximizeButton;
        wxCheckBox* MinimizeButton;
        wxCheckBox* Movable;
        wxCheckBox* PaneBorder;
        wxCheckBox* PinButton;
        wxCheckBox* Resizable;
        wxCheckBox* RightDockable;
        wxCheckBox* TopDockable;
        wxCheckBox* Visible;
        wxChoice* StandardPane;
        wxFlexGridSizer* FlexGridSizer1;
        wxRadioBox* Gripper;
        wxSpinCtrl* Layer;
        wxSpinCtrl* Position;
        wxSpinCtrl* Row;
        wxStaticLine* StaticLine1;
        wxStaticLine* StaticLine2;
        wxStaticText* StaticText1;
        wxStaticText* StaticText2;
        wxStaticText* StaticText3;
        wxStaticText* StaticText4;
        wxTextCtrl* Caption;
        wxTextCtrl* Name;
        //*)

    protected:

        //(*Identifiers(wxsAuiManagerParentQP)
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID ID_CHECKBOX18;
        static const wxWindowID ID_CHECKBOX15;
        static const wxWindowID ID_CHECKBOX20;
        static const wxWindowID ID_CHECKBOX21;
        static const wxWindowID ID_CHECKBOX19;
        static const wxWindowID ID_CHECKBOX22;
        static const wxWindowID ID_CHOICE1;
        static const wxWindowID ID_TEXTCTRL2;
        static const wxWindowID ID_CHECKBOX7;
        static const wxWindowID ID_CHECKBOX9;
        static const wxWindowID ID_CHECKBOX11;
        static const wxWindowID ID_CHECKBOX10;
        static const wxWindowID ID_CHECKBOX12;
        static const wxWindowID ID_CHECKBOX6;
        static const wxWindowID ID_CHECKBOX8;
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_SPINCTRL1;
        static const wxWindowID ID_STATICLINE1;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_SPINCTRL2;
        static const wxWindowID ID_STATICLINE2;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_SPINCTRL3;
        static const wxWindowID ID_CHECKBOX1;
        static const wxWindowID ID_CHECKBOX2;
        static const wxWindowID ID_CHECKBOX5;
        static const wxWindowID ID_CHECKBOX3;
        static const wxWindowID ID_CHECKBOX4;
        static const wxWindowID ID_CHECKBOX13;
        static const wxWindowID ID_CHECKBOX14;
        static const wxWindowID ID_CHECKBOX16;
        static const wxWindowID ID_CHECKBOX17;
        static const wxWindowID ID_RADIOBOX1;
        //*)

        virtual void Update();

    private:

        //(*Handlers(wxsAuiManagerParentQP)
        void OnDockChange(wxCommandEvent& event);
        void OnDockDirectionChange(wxCommandEvent& event);
        void OnDockSiteChange(wxSpinEvent& event);
        void OnNameChange(wxCommandEvent& event);
        void OnCaptionChange(wxCommandEvent& event);
        void OnCaptionButtonClick(wxCommandEvent& event);
        void OnDockableChange(wxCommandEvent& event);
        void OnGripperSelect(wxCommandEvent& event);
        void OnGeneralChange(wxCommandEvent& event);
        void OnCaptionVisibleClick(wxCommandEvent& event);
        void OnStandardPaneChange(wxCommandEvent& event);
        //*)

        void ReadData();

        wxsAuiPaneInfoExtra* m_Extra;

        DECLARE_EVENT_TABLE()
};

#endif
