/** \file wxsimagelisteditordlg.h
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

#ifndef WXSIMAGELISTEDITORDLG_H
#define WXSIMAGELISTEDITORDLG_H

#include <cbplugin.h>

//(*Headers(wxsImageListEditorDlg)
#include "scrollingdialog.h"
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/colordlg.h>
#include <wx/filedlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
//*)

#include "wxsimagelistproperty.h"
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/imaglist.h>
#include "wxsbitmapiconeditordlg.h"

class wxsItem;

/*! \brief Class for wxsImageListEditorDlg. */
class PLUGIN_EXPORT wxsImageListEditorDlg: public wxScrollingDialog
{
    public:

        wxsImageListEditorDlg(wxWindow* parent);
        virtual ~wxsImageListEditorDlg();
        bool                        Execute(wxString &inName, wxArrayString &aImageData);
        static  void            ImageToArray(wxImage &inImage, wxArrayString &outArray);
        static  void            ArrayToImage(wxArrayString &inArray, wxImage &outImage);
        static  void            BitmapToArray(wxBitmap &inBitmap, wxArrayString &outArray);
        static  void            ArrayToBitmap(wxArrayString &inArray, wxBitmap &outBitmap);
        static  void            ImageListToArray(wxImageList &inList, wxArrayString &outArray);
        static  void            ArrayToImageList(wxArrayString &inArray, wxImageList &outList);
        static  void            CopyImageList(wxImageList &inList, wxImageList &outList);
        static  wxsItem     *FindTool(wxsItem *inItem, wxString inName);
        static  int               CalcArraySize(wxArrayString &inArray);
        static  bool            SaveXPM(wxImage * image, wxOutputStream& stream);

        //(*Declarations(wxsImageListEditorDlg)
        wxBitmapButton* bAdd;
        wxBitmapButton* bClear;
        wxBitmapButton* bDel;
        wxBitmapButton* bLeft;
        wxBitmapButton* bRead;
        wxBitmapButton* bRight;
        wxBitmapButton* bSave;
        wxBitmapButton* bSaveList;
        wxButton* bCancel;
        wxButton* bColor;
        wxButton* bOK;
        wxCheckBox* cxTransparent;
        wxColourDialog* ColourDialog1;
        wxFileDialog* FileDialog1;
        wxPanel* Panel10;
        wxPanel* Panel11;
        wxPanel* Panel12;
        wxPanel* Panel1;
        wxPanel* Panel2;
        wxPanel* Panel3;
        wxPanel* Panel4;
        wxPanel* Panel5;
        wxPanel* Panel6;
        wxPanel* Panel7;
        wxPanel* Panel8;
        wxPanel* Panel9;
        wxStaticText* StaticText10;
        wxStaticText* StaticText11;
        wxStaticText* StaticText12;
        wxStaticText* StaticText13;
        wxStaticText* StaticText14;
        wxStaticText* StaticText15;
        wxStaticText* StaticText16;
        wxStaticText* StaticText17;
        wxStaticText* StaticText18;
        wxStaticText* StaticText19;
        wxStaticText* StaticText1;
        wxStaticText* StaticText20;
        wxStaticText* StaticText21;
        wxStaticText* StaticText22;
        wxStaticText* StaticText23;
        wxStaticText* StaticText24;
        wxStaticText* StaticText25;
        wxStaticText* StaticText26;
        wxStaticText* StaticText27;
        wxStaticText* StaticText28;
        wxStaticText* StaticText29;
        wxStaticText* StaticText2;
        wxStaticText* StaticText3;
        wxStaticText* StaticText4;
        wxStaticText* StaticText5;
        wxStaticText* StaticText6;
        wxStaticText* StaticText7;
        wxStaticText* StaticText8;
        wxStaticText* StaticText9;
        //*)

    protected:

        //(*Identifiers(wxsImageListEditorDlg)
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_STATICTEXT15;
        static const wxWindowID ID_STATICTEXT16;
        static const wxWindowID ID_STATICTEXT17;
        static const wxWindowID ID_STATICTEXT18;
        static const wxWindowID ID_STATICTEXT26;
        static const wxWindowID ID_STATICTEXT19;
        static const wxWindowID ID_STATICTEXT27;
        static const wxWindowID ID_BITMAPBUTTON1;
        static const wxWindowID ID_PANEL2;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_PANEL3;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_PANEL4;
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_PANEL5;
        static const wxWindowID ID_STATICTEXT5;
        static const wxWindowID ID_PANEL8;
        static const wxWindowID ID_STATICTEXT6;
        static const wxWindowID ID_PANEL6;
        static const wxWindowID ID_STATICTEXT7;
        static const wxWindowID ID_PANEL7;
        static const wxWindowID ID_STATICTEXT8;
        static const wxWindowID ID_PANEL9;
        static const wxWindowID ID_STATICTEXT9;
        static const wxWindowID ID_PANEL10;
        static const wxWindowID ID_STATICTEXT10;
        static const wxWindowID ID_PANEL11;
        static const wxWindowID ID_STATICTEXT12;
        static const wxWindowID ID_BITMAPBUTTON2;
        static const wxWindowID ID_STATICTEXT11;
        static const wxWindowID ID_PANEL1;
        static const wxWindowID ID_STATICTEXT23;
        static const wxWindowID ID_BITMAPBUTTON5;
        static const wxWindowID ID_STATICTEXT20;
        static const wxWindowID ID_CHECKBOX1;
        static const wxWindowID ID_BUTTON2;
        static const wxWindowID ID_STATICTEXT21;
        static const wxWindowID ID_BITMAPBUTTON3;
        static const wxWindowID ID_STATICTEXT13;
        static const wxWindowID ID_STATICTEXT22;
        static const wxWindowID ID_PANEL12;
        static const wxWindowID ID_STATICTEXT28;
        static const wxWindowID ID_BITMAPBUTTON4;
        static const wxWindowID ID_STATICTEXT14;
        static const wxWindowID ID_BITMAPBUTTON6;
        static const wxWindowID ID_STATICTEXT24;
        static const wxWindowID ID_BITMAPBUTTON7;
        static const wxWindowID ID_STATICTEXT25;
        static const wxWindowID ID_BITMAPBUTTON8;
        static const wxWindowID ID_STATICTEXT29;
        static const wxWindowID ID_BUTTON1;
        static const wxWindowID ID_BUTTON4;
        //*)

    private:

        //(*Handlers(wxsImageListEditorDlg)
        void OnbAddClick(wxCommandEvent& event);
        void OnbReadClick(wxCommandEvent& event);
        void OnPanel1Paint(wxPaintEvent& event);
        void OnbColorClick(wxCommandEvent& event);
        void OnPanel1LeftUp(wxMouseEvent& event);
        void OncxTransparentClick(wxCommandEvent& event);
        void OnbOKClick(wxCommandEvent& event);
        void OnbCancelClick(wxCommandEvent& event);
        void OnbLeftClick(wxCommandEvent& event);
        void OnbRightClick(wxCommandEvent& event);
        void OnPanel2LeftUp(wxMouseEvent& event);
        void OnPanel3LeftUp(wxMouseEvent& event);
        void OnPanel4LeftUp(wxMouseEvent& event);
        void OnPanel5LeftUp(wxMouseEvent& event);
        void OnPanel8LeftUp(wxMouseEvent& event);
        void OnPanel6LeftUp(wxMouseEvent& event);
        void OnPanel7LeftUp(wxMouseEvent& event);
        void OnPanel9LeftUp(wxMouseEvent& event);
        void OnPanel10LeftUp(wxMouseEvent& event);
        void OnPanel11LeftUp(wxMouseEvent& event);
        void OnPanel12Paint(wxPaintEvent& event);
        void OnbDelClick(wxCommandEvent& event);
        void OnbClearClick(wxCommandEvent& event);
        void OnbSaveClick(wxCommandEvent& event);
        void OnbSaveListClick(wxCommandEvent& event);
        void PaintPreviewPanel(wxPaintEvent& event);
        //*)

        void                PreviewImport(void);
        void                PreviewList(void);
        void                PreviewSelected(void);
        void                UpdateEnabled(void);
        void                SelectPreviewPanel(int aIndex);
        void                PaintPanel(wxPaintDC &aDC, wxPanel *aPanel, wxBitmap &aBitmap, bool aHot = false);

        wxImageList                         m_ImageList;                     //!< Working copy of image list.
        wxImage                                 m_ImportImage;                //!< The thing that gets displayed.
        wxColour                                m_ImportMask;                  //!< Mask color.
        wxsBitmapIconEditorDlg     *m_ImageDialog;            //!< Dialog to import external images.
        wxsBitmapIconData           m_ImageData;                   //!< Image data for the dialog.
        int                                             m_FirstImage;                    //!< Left-most image displayed in preview.
        wxPanel                                *m_PreviewPanel[10];      //!< The list of preview images.
        wxStaticText                           *m_PreviewLabel[10];      //!< The preview list label.
        int                                             m_PreviewSelect;             //!< The selected preview image.

        DECLARE_EVENT_TABLE()
};

#endif
