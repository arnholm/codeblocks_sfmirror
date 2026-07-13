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

kwxBmpCheckBox::~kwxBmpCheckBox()
{
	if (membitmap)
		delete membitmap ;
}

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
	m_id = id;

    SetAutoLayout(true);
	Refresh();
	m_stato = 0;
	m_oldstato = 0;
	m_bPress = false ;
	m_bBord = true ;
	m_nStyle = wxPENSTYLE_DOT;

	const wxSize Size(GetSize());
	membitmap = new wxBitmap(Size.GetWidth(), Size.GetHeight());
}

void kwxBmpCheckBox::SetLabel(const wxString& label)
{
	mLabelStr = label;
}

void kwxBmpCheckBox::OnPaint(wxPaintEvent& WXUNUSED(event))
{
	wxPaintDC old_dc(this);

	int w,h;
	bool bdraw = false;

	GetClientSize(&w,&h);

	/////////////////

	// Create a memory DC
	wxMemoryDC dc;
	dc.SelectObject(*membitmap);

	dc.SetBackground(*wxTheBrushList->FindOrCreateBrush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID));
	dc.Clear();

	///////////////////

	// se impostato n bitmap lo disegno
    //if (mOffBitmap)
    switch (m_stato)
    {
        case 0:
            if (m_bPress)
            {
                if (mOnBitmap.IsOk())
                    dc.DrawBitmap(mOnBitmap, 0, 0, true);
            }
            else
            {
                if (mOffBitmap.IsOk())
                    dc.DrawBitmap(mOffBitmap, 0, 0, true);
            }

            bdraw = false;
            break;
        case 1:
            if (m_bPress)
            {
                if (mOnSelBitmap.IsOk())
                    dc.DrawBitmap(mOnSelBitmap, 0, 0, true);
            }
            else
            {
                if (mOffSelBitmap.IsOk())
                    dc.DrawBitmap(mOffSelBitmap, 0, 0, true);
            }

            bdraw = true;
            break;
        case 2:
            if (m_bPress)
            {
                if (mOffSelBitmap.IsOk())
                    dc.DrawBitmap(mOffSelBitmap, 0, 0, true);
            }
            else
            {
                if (mOnSelBitmap.IsOk())
                    dc.DrawBitmap(mOnSelBitmap, 0, 0, true);
            }

            bdraw = true;
    }

	if (m_bBord && bdraw)
    {
        // Cornice intorno
        dc.SetPen(*wxThePenList->FindOrCreatePen(*wxRED, 1, m_nStyle));
        dc.DrawLine(0, 0, 0, h - 1);
        dc.DrawLine(0, 0, w, 0);
        dc.DrawLine(0, h - 1, w, h - 1);
        dc.DrawLine(w - 1, 0, w - 1, h - 1);
    }

	// We can now draw into the memory DC...
	// Copy from this DC to another DC.
	old_dc.Blit(0, 0, w, h, &dc, 0, 0);
}

void kwxBmpCheckBox::OnMouse(wxMouseEvent& event)
{
	if (m_stato == 0 && event.Entering())
	{
		m_stato = 1;	// mouse sul bottone
		wxCommandEvent ev(event.GetEventType(),GetId());
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
		wxCommandEvent ev(event.GetEventType(),GetId());
		ev.SetEventType(wxEVT_LEAVE_WINDOW);
		event.SetEventObject(this);
		GetEventHandler()->ProcessEvent(ev);
	}
	else if (m_stato == 2 && event.LeftUp())
	{
		m_bPress = !m_bPress ;
		Click();		// rilascio sul bottone genera evento
		m_stato = 1;
	}

	if (m_oldstato != m_stato)
		Refresh();

	m_oldstato = m_stato;
	event.Skip();
}

void kwxBmpCheckBox::Click()
{
	wxCommandEvent event(kwxEVT_BITBUTTON_CLICK, GetId());
	event.SetEventObject(this);
//	ProcessCommand(event);
    GetEventHandler()->ProcessEvent(event);
}

void kwxBmpCheckBox::SetState(bool newstate)
{
	m_bPress = newstate ;
	Refresh();
}

