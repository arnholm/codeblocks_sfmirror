#include "asyncThreadTypes.h"


 AsyncThreadTypes::AsyncThreadTypes(){}
 AsyncThreadTypes::~AsyncThreadTypes(){}

wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_OUTPUT, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_STDERR, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_TERMINATED, wxThreadEvent);

