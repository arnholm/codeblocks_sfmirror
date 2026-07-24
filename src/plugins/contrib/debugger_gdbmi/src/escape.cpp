#include "escape.h"

namespace dbg_mi
{

// ----------------------------------------------------------------------------
wxString EscapePath(wxString const &path)
// ----------------------------------------------------------------------------
{
    if (path.empty())
        return path;

    bool escape = false, escapeDoubleQuotes = false;
    for (size_t ii = 0; ii < path.length(); ++ii)
    {
        switch (static_cast<int>(path[ii]))
        {
        case _T(' '):
            escape = true;
            break;
        case _T('"'):
            if (ii > 0 && ii < path.length() - 1)
                escapeDoubleQuotes = true;
            break;
        }
    }
    if (path[0] == _T('"') && path[path.length()-1] == _T('"'))
        escape = false;
    if (not escape && !escapeDoubleQuotes)
        return path;

    wxString result;
    if (path[0] == _T('"') && path[path.length()-1] == _T('"'))
        result = path.substr(1, path.length() - 2);
    else
        result = path;
    result.Replace(_T("\""), _T("\\\""));
    return _T('"') + result + _T('"');
}

// ----------------------------------------------------------------------------
void ConvertDirectory(wxString& str, wxString base, bool relative)
// ----------------------------------------------------------------------------
{
    if (relative) {;} //hush warning
    if (not base.empty())
        str = base + _T("/") + str;
    str = EscapePath(str);
}

} // namespace dbg_mi
