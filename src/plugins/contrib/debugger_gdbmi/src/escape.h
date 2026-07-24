#ifndef _DEBUGGER_MI_ESCAPE_H_
#define _DEBUGGER_MI_ESCAPE_H_

#include <wx/string.h>

namespace dbg_mi
{

wxString EscapePath(wxString const &path);
void ConvertDirectory(wxString& str, wxString base, bool relative);

} // namespace dbg_mi

#endif // _DEBUGGER_MI_ESCAPE_H_
