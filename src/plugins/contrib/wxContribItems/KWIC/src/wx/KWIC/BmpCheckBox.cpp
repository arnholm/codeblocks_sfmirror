/////////////////////////////////////////////////////////////////////////////
// Name:        BmpCheckBox.cpp
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


// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
//#include "kprec.h"		//#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#include <wx/image.h>
#include <wx/log.h>

#include "wx/KWIC/BmpCheckBox.h"
#include <wx/event.h>



IMPLEMENT_DYNAMIC_CLASS(kwxBmpCheckBox, wxControl)


BEGIN_EVENT_TABLE(kwxBmpCheckBox,wxControl)
    EVT_PAINT(kwxBmpCheckBox::OnPaint)
    EVT_MOUSE_EVENTS(kwxBmpCheckBox::OnMouse)
END_EVENT_TABLE()

kwxBmpCheckBox::kwxBmpCheckBox(wxWindow* parent,
                               const wxWindowID id,
                               const wxBitmap& OnBitmap,
                               const wxBitmap& OffBitmap,
                               const wxBitmap& OnSelBitmap,
                               const wxBitmap& OffSelBitmap,
                               const wxPoint& pos,
                               const wxSize& size,
                               long int style)
    : wxControl(parent, id, pos, size, style)
{
    SetBackgroundColour(parent ? parent->GetBackgroundColour() : *wxLIGHT_GREY);

    mOnBitmap     = OnBitmap;
    mOffBitmap    = OffBitmap;
    mOnSelBitmap  = OnSelBitmap;
    mOffSelBitmap = OffSelBitmap;

    m_stato = 0;
    m_oldstato = 0;
    m_bPress = false;
    m_bBord = true;
    m_nStyle = wxPENSTYLE_DOT;

    SetAutoLayout(true);
    Refresh();
}

void kwxBmpCheckBox::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC paintdc(this);

    // Create a memory DC
    wxMemoryDC dc;
    wxBitmap membitmap(paintdc.GetSize());
    dc.SelectObject(membitmap);

    // Erase the background
    dc.SetBackground(*wxTheBrushList->FindOrCreateBrush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID));
    dc.Clear();

    // Draw the bitmap if available
    wxBitmap bmp;
    bool bdraw = false;
    switch (m_stato)
    {
    case 0:
        bmp = m_bPress ? mOnBitmap : mOffBitmap;
        bdraw = false;
        break;
    case 1:
        bmp = m_bPress ? mOnSelBitmap : mOffSelBitmap;
        bdraw = true;
        break;
    case 2:
        bmp = m_bPress ? mOffSelBitmap : mOnSelBitmap;
        bdraw = true;
    }

    wxCoord w, h;
    paintdc.GetSize(&w, &h);

    // Draw the bitmap in the center of the button
    if (bmp.IsOk())
    {
        const wxCoord left = (w-bmp.GetWidth()) / 2;
        const wxCoord top  = (h-bmp.GetHeight()) / 2;
        dc.DrawBitmap(bmp, std::max(left, 1), std::max(top, 1), true);
    }

    if (m_bBord && bdraw)
    {
        // Cornice intorno
        dc.SetPen(*wxThePenList->FindOrCreatePen(*wxRED, 1, m_nStyle));
        dc.DrawLine(0, 0, w-1, 0);
        dc.DrawLine(w-1, 0, w-1, h-1);
        dc.DrawLine(w-1, h-1, 0, h-1);
        dc.DrawLine(0, h-1, 0, 0);
    }

    dc.SelectObject(wxNullBitmap);
    paintdc.DrawBitmap(membitmap, 0, 0);
}

void kwxBmpCheckBox::OnMouse(wxMouseEvent& event)
{
    if (m_stato == 0 && event.Entering())
    {
        m_stato = 1;	// mouse sul bottone
        wxCommandEvent ev(event.GetEventType(), GetId());
        ev.SetEventType(wxEVT_ENTER_WINDOW);
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(ev);
    }
    else if (m_stato == 1 && event.LeftDown())
    {
        m_stato = 2;	// uscita click sul bottone
    }
    else if (m_stato >= 1 && event.Leaving())
    {
        m_stato = 0;	// uscita mouse dal bottone
        wxCommandEvent ev(event.GetEventType(), GetId());
        ev.SetEventType(wxEVT_LEAVE_WINDOW);
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(ev);
    }
    else if (m_stato == 2 && event.LeftUp())
    {
        m_bPress = !m_bPress;
        wxCommandEvent event(kwxEVT_BITBUTTON_CLICK, GetId());
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(event);
        m_stato = 1;
    }

    if (m_oldstato != m_stato)
    {
        m_oldstato = m_stato;
        Refresh();
    }

    event.Skip();
}

void kwxBmpCheckBox::SetState(bool newstate)
{
    if (m_bPress != newstate)
    {
        m_bPress = newstate;
        Refresh();
    }
}

wxSize kwxBmpCheckBox::DoGetBestClientSize() const
{
    wxSize Size(16, 16);
    if (mOffBitmap.IsOk())
        Size.IncTo(mOffBitmap.GetSize());

    if (mOnBitmap.IsOk())
        Size.IncTo(mOnBitmap.GetSize());

    if (mOffSelBitmap.IsOk())
        Size.IncTo(mOffSelBitmap.GetSize());

    if (mOnSelBitmap.IsOk())
        Size.IncTo(mOnSelBitmap.GetSize());

    // Add space for the border
    Size.IncBy(2);
    return Size;
}
