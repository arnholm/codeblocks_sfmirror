/////////////////////////////////////////////////////////////////////////////
// Name:        BmpCheckBox.h
// Purpose:     wxIndustrialControls Library
// Author:      Marco Cavallini <m.cavallini AT koansoftware.com>
// Modified by:
// Copyright:   (C)2004-2006 Copyright by Koan s.a.s. - www.koansoftware.com
// Licence:     KWIC License http://www.koansoftware.com/kwic/kwic-license.htm
/////////////////////////////////////////////////////////////////////////////
//
//	Cleaned up and modified by Gary Harris for wxSmithKWIC, 2010.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BMPCHECKBOX_H
#define BMPCHECKBOX_H

#ifdef __WXMSW__
#ifndef DLLEXPORT
#define DLLEXPORT __declspec (dllexport)
#endif
#else
#define DLLEXPORT
#endif

////////////////// Custom events /////////////////////////

const int kwxEVT_BITBUTTON_FIRST = wxEVT_FIRST + 5400;

const wxEventType kwxEVT_BITBUTTON_CLICK	= kwxEVT_BITBUTTON_FIRST + 1;

#define EVT_BITBUTTON_CLICK(id, fn)	\
		DECLARE_EVENT_TABLE_ENTRY(kwxEVT_BITBUTTON_CLICK, id, -1, \
		(wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&fn, \
		(wxObject*)NULL ),


//////////////////////////////////////////////////////////

class DLLEXPORT kwxBmpCheckBox : public wxControl
{
public:
    // ctor(s)
    kwxBmpCheckBox() = default;

    kwxBmpCheckBox(wxWindow *parent,
                   const wxWindowID id          = wxID_ANY,
                   const wxBitmap& OnBitmap	    = wxNullBitmap,
                   const wxBitmap& OffBitmap	= wxNullBitmap,
                   const wxBitmap& OnSelBitmap	= wxNullBitmap,
                   const wxBitmap& OffSelBitmap	= wxNullBitmap,
                   const wxPoint&  pos          = wxDefaultPosition,
                   const wxSize&   size         = wxDefaultSize,
                   long int        style        = 0);

    virtual ~kwxBmpCheckBox() = default;

    void SetBorder(bool bord, wxPenStyle style)
    {
        m_bBord = bord;
        m_nStyle = style;
    }

    bool GetState() const
    {
        return m_bPress;
    }

    void SetState(bool newstate);

protected:
    wxSize DoGetBestClientSize() const override;

private:
    DECLARE_DYNAMIC_CLASS(kwxBmpCheckBox)
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()

    void OnPaint(wxPaintEvent& event);
    void OnMouse(wxMouseEvent& event);

    int m_stato;
    int m_oldstato;
    bool m_bPress;
    bool m_bBord;
    wxPenStyle m_nStyle;

    wxBitmap mOffBitmap;
    wxBitmap mOnBitmap;
    wxBitmap mOffSelBitmap;
    wxBitmap mOnSelBitmap;
};

#endif
