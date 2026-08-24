/***************************************************************
 * Name:      DragScrollEvent
 *
 * Purpose:   This class implements the events sent by/for a
 *            DragScroll request to the
 *            DragScroll plugin to request services such as
 *            start/end scroll monitoring of an open window.
 *            wxCommandEvent m_id contains a window id.
 *
 * Author:    Pecan
 * Created:   2008/4/01
 * Copyright: Pecan
 * License:   GPL
 **************************************************************/
#ifndef DRAGSCROLL_EVENT_H
#define DRAGSCROLL_EVENT_H

#include <wx/event.h>
#include "wx/xrc/xmlres.h"

class cbPlugin;

    const int idDragScrollAddWindow    = XRCID("idDragScrollAddWindow");
    const int idDragScrollRemoveWindow = XRCID("idDragScrollRemoveWindow");
    const int idDragScrollRescan       = XRCID("idDragScrollRescan");
    const int idDragScrollReadConfig   = XRCID("idDragScrollReadConfig");
    const int idDragScrollInvokeConfig = XRCID("idDragScrollInvokeConfig");
    const int idDragScrollDidScroll    = XRCID("idDragScrollDidScroll");

// ----------------------------------------------------------------------------
class DragScrollEvent : public wxCommandEvent
// ----------------------------------------------------------------------------
{
public:
	/** Constructor. */
	DragScrollEvent(wxEventType commandType = wxEVT_NULL, int id = 0);

	/** Copy constructor. */
	DragScrollEvent( const DragScrollEvent& event);

	/** Destructor. */
	~DragScrollEvent();

	virtual wxEvent* Clone() const { return new DragScrollEvent(*this);}

	DECLARE_DYNAMIC_CLASS(DragScrollEvent);

	wxString  GetEventTypeLabel() const {return m_EventTypeLabel;}

    bool      PostDragScrollEvent(const cbPlugin* targetWin);
    bool      ProcessDragScrollEvent(const cbPlugin* targetWin);

private:
	//-int        m_WindowID;
	//-wxWindow*  m_pWindow;
	wxString   m_EventTypeLabel;
};

typedef void (wxEvtHandler::*DragScrollEventFunction)(DragScrollEvent&);

wxDECLARE_EVENT(wxEVT_DRAGSCROLL_EVENT, DragScrollEvent); // (ph 26/04/14)

// Optional: If you still want to support the old Event Table style (BEGIN_EVENT_TABLE)
//#define EVT_DRAGSCROLL_EVENT(id, fn) \ <-- continuation
//    wx__DECLARE_EVT1(wxEVT_DRAGSCROLL_EVENT, id, DragScrollEventHandler(fn))

#endif // DRAGSCROLL_EVENT_H


