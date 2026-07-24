#ifndef _Debugger_GDB_MI_EVENTS_H_
#define _Debugger_GDB_MI_EVENTS_H_

#include <wx/event.h>

namespace dbg_mi
{

extern const wxEventType NotificationEventType;

class ResultParser;
// TODO (pecan#): remove this class, not needed
class NotificationEvent : public wxEvent
{
public:
    NotificationEvent(ResultParser *parser, wxEventType command_type = NotificationEventType) :
        wxEvent(wxID_ANY, command_type),
        m_result_parser(parser)
    {
    }

	// You *must* copy here the data to be transported
	NotificationEvent(const NotificationEvent &event) :
        wxEvent(event)
    {
    }

	// Required for sending with wxPostEvent()
	wxEvent* Clone() const { return new NotificationEvent(*this); }
public:
    ResultParser* GetResultParser() { return m_result_parser; }
private:
    ResultParser *m_result_parser;
};

typedef void (wxEvtHandler::*NotificationEventFunction)(NotificationEvent &);


} // namespace dbg_mi


// This #define simplifies the one below, and makes the syntax less
// ugly if you want to use Connect() instead of an event table.
#define NotificationEventHandler(func)                                         \
	(wxObjectEventFunction)(wxEventFunction)                                    \
	wxStaticCastEvent(dbg_mi::NotificationEventFunction, &func)

#define EVT_DBGMI_NOTIFICATION(fn)                                              \
    DECLARE_EVENT_TABLE_ENTRY(dbg_mi::NotificationEventType, -1, wxID_ANY,              \
                              (wxObjectEventFunction)(wxEventFunction)(dbg_mi::NotificationEventFunction)&fn,     \
                              (wxObject*) NULL),


#endif // _Debugger_GDB_MI_EVENTS_H_
