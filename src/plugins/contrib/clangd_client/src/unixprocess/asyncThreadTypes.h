#ifndef ASYNCTHREADTYPES_H_INCLUDED
#define ASYNCTHREADTYPES_H_INCLUDED

#include <wx/event.h>

class AsyncThreadTypes
{
    AsyncThreadTypes();
    ~AsyncThreadTypes();
};

wxDECLARE_EXPORTED_EVENT(, wxEVT_ASYNC_PROCESS_OUTPUT, wxThreadEvent);
wxDECLARE_EXPORTED_EVENT(, wxEVT_ASYNC_PROCESS_STDERR, wxThreadEvent);
wxDECLARE_EXPORTED_EVENT(, wxEVT_ASYNC_PROCESS_STDERR, wxThreadEvent);
wxDECLARE_EXPORTED_EVENT(, wxEVT_ASYNC_PROCESS_TERMINATED, wxThreadEvent);


#endif // ASYNCTHREADTYPES_H_INCLUDED
