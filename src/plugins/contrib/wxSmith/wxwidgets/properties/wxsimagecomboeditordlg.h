/** \file wxsimagecomboeditordlg.h
*
* This file is part of wxSmith plugin for Code::Blocks Studio
* Copyright (C) 2010 Gary Harris
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
* This code was taken from the wxSmithImage plug-in, copyright Ron Collins
* and released under the GPL.
*
*/

#ifndef WXSIMAGECOMBOEDITORDLG_H
#define WXSIMAGECOMBOEDITORDLG_H

//(*Headers(wxsImageComboEditorDlg)
#include "scrollingdialog.h"
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>
//*)

#include "../defitems/wxsimagelist.h"
#include <wx/bmpcbox.h>
#include <wx/msgdlg.h>
#include <wx/imaglist.h>
#include <wx/bitmap.h>

class wxsItem;

///*! \brief wxBitmapComboBox item editor dialogue. */
class wxsImageComboEditorDlg: public wxScrollingDialog
{
    public:

        /*! \brief Constructor.
         *
         * \param parent wxWindow*
         * \param id wxWindowID
         * \param pos const wxPoint&
         * \param size const wxSize&
         */
        wxsImageComboEditorDlg(wxWindow* parent);
        /** \brief Destructor. */
        virtual ~wxsImageComboEditorDlg();

        bool    Execute(wxArrayString &aItems);


        //(*Declarations(wxsImageComboEditorDlg)
        wxBitmapButton* m_pBtnAddItem;
        wxBitmapButton* m_pBtnDeleteAll;
        wxBitmapButton* m_pBtnDeleteItem;
        wxBitmapButton* m_pBtnEditItem;
        wxBitmapButton* m_pBtnMoveDown;
        wxBitmapButton* m_pBtnMoveUp;
        wxBitmapComboBox* m_pCmbImage;
        wxButton* m_pBtnCancel;
        wxButton* m_pBtnOK;
        wxStaticText* StaticText10;
        wxStaticText* StaticText1;
        wxStaticText* StaticText2;
        wxStaticText* StaticText3;
        wxStaticText* StaticText4;
        wxStaticText* StaticText5;
        wxStaticText* StaticText6;
        wxStaticText* StaticText7;
        wxStaticText* StaticText8;
        wxStaticText* StaticText9;
        wxTreeCtrl* m_pTree;
        //*)

    protected:

        //(*Identifiers(wxsImageComboEditorDlg)
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_TREECTRL1;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_BITMAPBUTTON1;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_BITMAPBUTTON2;
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_BITMAPBUTTON3;
        static const wxWindowID ID_STATICTEXT5;
        static const wxWindowID ID_BITMAPBUTTON4;
        static const wxWindowID ID_STATICTEXT6;
        static const wxWindowID ID_BITMAPBUTTON5;
        static const wxWindowID ID_STATICTEXT7;
        static const wxWindowID ID_BITMAPBUTTON6;
        static const wxWindowID ID_STATICTEXT8;
        static const wxWindowID ID_STATICTEXT9;
        static const wxWindowID ID_COMBOBOX1;
        static const wxWindowID ID_STATICTEXT10;
        static const wxWindowID ID_BUTTON1;
        static const wxWindowID ID_BUTTON2;
        //*)

    private:

        //(*Handlers(wxsImageComboEditorDlg)
        void OnBtnDeleteAllClick(wxCommandEvent& event);
        void OnBtnAddItemClick(wxCommandEvent& event);
        void OnBtnMoveUpClick(wxCommandEvent& event);
        void OnBtnMoveDownClick(wxCommandEvent& event);
        void OnBtnDeleteItemClick(wxCommandEvent& event);
        void OnBtnEditItemClick(wxCommandEvent& event);
        void OnBtnOKClick(wxCommandEvent& event);
        void OnBtnCancelClick(wxCommandEvent& event);
        void OnCmbImageSelect(wxCommandEvent& event);
        void OnTreeSelectionChanged(wxTreeEvent& event);
        //*)

        wxString            m_ComboName;        //!< The name of the base combobox.
        wxString            m_ImageName;        //!< The name of the image list.
        wxImageList        m_ImageList;                 //!< Local copy of an image list

        DECLARE_EVENT_TABLE()
};

#endif
