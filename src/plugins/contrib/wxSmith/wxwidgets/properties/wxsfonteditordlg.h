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

#ifndef WXSFONTEDITORDLG_H
#define WXSFONTEDITORDLG_H

#include "wxsfontproperty.h"

//(*Headers(wxsFontEditorDlg)
#include "scrollingdialog.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

class wxsFontEditorDlg: public wxScrollingDialog
{
    public:

        wxsFontEditorDlg(wxWindow* parent,wxsFontData& Data,wxWindowID id = -1);
        virtual ~wxsFontEditorDlg();

        //(*Identifiers(wxsFontEditorDlg)
        static const wxWindowID ID_CHOICE1;
        static const wxWindowID ID_CHECKBOX8;
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_CHOICE2;
        static const wxWindowID ID_CHECKBOX2;
        static const wxWindowID ID_COMBOBOX1;
        static const wxWindowID ID_CHECKBOX1;
        static const wxWindowID ID_CHOICE3;
        static const wxWindowID ID_CHECKBOX7;
        static const wxWindowID ID_SPINCTRL1;
        static const wxWindowID ID_CHECKBOX6;
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID ID_CHECKBOX3;
        static const wxWindowID ID_RADIOBUTTON1;
        static const wxWindowID ID_RADIOBUTTON2;
        static const wxWindowID ID_RADIOBUTTON3;
        static const wxWindowID ID_CHECKBOX4;
        static const wxWindowID ID_RADIOBUTTON4;
        static const wxWindowID ID_RADIOBUTTON5;
        static const wxWindowID ID_RADIOBUTTON6;
        static const wxWindowID ID_CHECKBOX5;
        static const wxWindowID ID_RADIOBUTTON7;
        static const wxWindowID ID_RADIOBUTTON8;
        static const wxWindowID ID_LISTBOX1;
        static const wxWindowID ID_BUTTON4;
        static const wxWindowID ID_BUTTON5;
        static const wxWindowID ID_BUTTON6;
        static const wxWindowID ID_BUTTON8;
        static const wxWindowID ID_BUTTON7;
        static const wxWindowID ID_TEXTCTRL2;
        //*)

    protected:

        //(*Handlers(wxsFontEditorDlg)
        void OnButton1Click(wxCommandEvent& event);
        void OnUpdateContent(wxCommandEvent& event);
        void OnButton2Click(wxCommandEvent& event);
        void OnButton1Click1(wxCommandEvent& event);
        void OnFaceAddClick(wxCommandEvent& event);
        void OnFaceDelClick(wxCommandEvent& event);
        void OnFaceEditClick(wxCommandEvent& event);
        void OnFaceUpClick(wxCommandEvent& event);
        void OnFaceDownClick(wxCommandEvent& event);
        void OnUpdatePreview(wxCommandEvent& event);
        void OnSizeValChange(wxSpinEvent& event);
        void OnBaseFontUseChange(wxCommandEvent& event);
        //*)

        //(*Declarations(wxsFontEditorDlg)
        wxBoxSizer* BaseFontSizer;
        wxBoxSizer* BoxSizer2;
        wxBoxSizer* BoxSizer3;
        wxButton* FaceAdd;
        wxButton* FaceDel;
        wxButton* FaceDown;
        wxButton* FaceEdit;
        wxButton* FaceUp;
        wxCheckBox* BaseFontUse;
        wxCheckBox* EncodUse;
        wxCheckBox* FamUse;
        wxCheckBox* RelSizeUse;
        wxCheckBox* SizeUse;
        wxCheckBox* StyleUse;
        wxCheckBox* UnderUse;
        wxCheckBox* WeightUse;
        wxChoice* BaseFontVal;
        wxChoice* EncodVal;
        wxChoice* FontType;
        wxComboBox* FamVal;
        wxFlexGridSizer* FlexGridSizer2;
        wxListBox* FaceList;
        wxRadioButton* StyleItal;
        wxRadioButton* StyleNorm;
        wxRadioButton* StyleSlant;
        wxRadioButton* UnderNo;
        wxRadioButton* UnderYes;
        wxRadioButton* WeightBold;
        wxRadioButton* WeightLight;
        wxRadioButton* WeightNorm;
        wxSpinCtrl* SizeVal;
        wxStaticBoxSizer* StaticBoxSizer1;
        wxStaticBoxSizer* StaticBoxSizer2;
        wxStaticBoxSizer* StaticBoxSizer3;
        wxStaticBoxSizer* StaticBoxSizer4;
        wxStaticBoxSizer* StaticBoxSizer5;
        wxStaticBoxSizer* StaticBoxSizer6;
        wxStaticText* BaseFontTxt;
        wxTextCtrl* RelSizeVal;
        wxTextCtrl* TestArea;
        //*)

    private:

        void UpdateContent();
        void UpdatePreview();
        void ReadData(wxsFontData& _Data);
        void StoreData(wxsFontData& _Data);

        wxsFontData& Data;
        wxArrayString Encodings;
        bool Initialized;

        DECLARE_EVENT_TABLE()
};

#endif
