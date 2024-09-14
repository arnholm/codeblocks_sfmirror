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

#ifndef WXSARRAYSTRINGCHECKEDITORDLG_H
#define WXSARRAYSTRINGCHECKEDITORDLG_H

#include "wxsarraystringcheckproperty.h"

//(*Headers(wxsArrayStringCheckEditorDlg)
#include "scrollingdialog.h"
#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
//*)


class wxsArrayStringCheckEditorDlg: public wxScrollingDialog
{
    public:

        wxsArrayStringCheckEditorDlg(wxWindow* parent,wxArrayString& Strings,wxArrayBool& Bools,wxWindowID id = -1);
        virtual ~wxsArrayStringCheckEditorDlg();

        //(*Identifiers(wxsArrayStringCheckEditorDlg)
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID ID_BUTTON1;
        static const wxWindowID ID_CHECKLISTBOX1;
        static const wxWindowID ID_BUTTON2;
        static const wxWindowID ID_BUTTON4;
        static const wxWindowID ID_BUTTON3;
        static const wxWindowID ID_BUTTON5;
        static const wxWindowID ID_BUTTON6;
        static const wxWindowID ID_BUTTON7;
        //*)

    protected:

        //(*Handlers(wxsArrayStringCheckEditorDlg)
        void OnButton1Click(wxCommandEvent& event);
        void OnButton2Click(wxCommandEvent& event);
        void OnButton4Click(wxCommandEvent& event);
        void OnButton3Click(wxCommandEvent& event);
        void OnButton5Click(wxCommandEvent& event);
        void OnButton6Click(wxCommandEvent& event);
        void OnButton7Click(wxCommandEvent& event);
        void OnStringListToggled(wxCommandEvent& event);
        //*)

        //(*Declarations(wxsArrayStringCheckEditorDlg)
        wxBoxSizer* BoxSizer4;
        wxButton* Button1;
        wxButton* Button2;
        wxButton* Button3;
        wxButton* Button4;
        wxButton* Button5;
        wxButton* Button6;
        wxButton* Button7;
        wxCheckListBox* StringList;
        wxTextCtrl* EditArea;
        //*)

    private:

        wxArrayString& Strings;
        wxArrayBool& Bools;

        DECLARE_EVENT_TABLE()
};

#endif
