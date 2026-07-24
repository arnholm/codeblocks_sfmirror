#include "definitions.h"
#include "helpers.h"

namespace dbg_mi
{

const int DEBUGGER_CURSOR_CHANGED = wxNewId();
const int DEBUGGER_SHOW_FILE_LINE = wxNewId();

// ----------------------------------------------------------------------------
void Breakpoint::SetEnabled(bool flag)
// ----------------------------------------------------------------------------
{
    m_enabled = flag; //(pecan 2012/07/26)
}

// ----------------------------------------------------------------------------
wxString Breakpoint::GetLocation() const
// ----------------------------------------------------------------------------
{
    return m_filename;
}

int Breakpoint::GetLine() const
{
    return m_line;
}

// ----------------------------------------------------------------------------
wxString Breakpoint::GetLineString() const
// ----------------------------------------------------------------------------
{
    return wxString::Format(_T("%d"), m_line);
}

// ----------------------------------------------------------------------------
wxString Breakpoint::GetType() const
// ----------------------------------------------------------------------------
{
    //return _("Code");
    return m_type;
}

// ----------------------------------------------------------------------------
wxString Breakpoint::GetInfo() const
// ----------------------------------------------------------------------------
{
        if (GetType() == _T("Data"))
        {
            if (GetBreakOnDataRead() && GetBreakOnDataWrite())
                return  _("type: read-write");
            else if (GetBreakOnDataRead())
                return _("type: read");
            else if (GetBreakOnDataWrite())
                return _("type: write");
            else
                return _("type: unknown");
        }
        if (GetType() == _T("Code"))
        {
            wxString s;
            if (HasCondition())
                s += _("condition: ") + GetCondition();
            if (HasIgnoreCount())
            {
                if (not s.empty())
                    s += _T(" ");
                s += wxString::Format(_T("%s: %d"), _("ignore count"), GetIgnoreCount());
            }
            if (IsTemporary())
            {
                if (not s.empty())
                    s += _T(" ");
                s += _("temporary");
            }
            s += wxString::Format(_T(" (%s: %d)"), _T("index"), GetIndex());
            return s;
        }
        //case bptFunction:
        //default:
        return wxEmptyString;

}

// ----------------------------------------------------------------------------
bool Breakpoint::IsEnabled() const
// ----------------------------------------------------------------------------
{
    return m_enabled;
}

// ----------------------------------------------------------------------------
bool Breakpoint::IsVisibleInEditor() const
// ----------------------------------------------------------------------------
{
    return true;
}

// ----------------------------------------------------------------------------
bool Breakpoint::IsTemporary() const
// ----------------------------------------------------------------------------
{
    return m_temporary;
}

// ----------------------------------------------------------------------------
Watch::Pointer FindWatch(wxString const &expression, WatchesContainer &watches)
// ----------------------------------------------------------------------------
{
    for(WatchesContainer::iterator it = watches.begin(); it != watches.end(); ++it)
    {
        if(expression.StartsWith(it->get()->GetID()))
        {
            if(expression.length() == it->get()->GetID().length())
                return *it;
            else
            {
                Watch::Pointer curr = *it;
                while(curr)
                {
                    Watch::Pointer temp = curr;
                    curr = Watch::Pointer();

                    for(int child = 0; child < temp->GetChildCount(); ++child)
                    {
                        Watch::Pointer p = cb::static_pointer_cast<Watch>(temp->GetChild(child));
                        if(expression.StartsWith(p->GetID()))
                        {
                            if(expression.length() == p->GetID().length())
                                return p;
                            else
                            {
                                curr = p;
                                break;
                            }
                        }
                    }
                }
                if(curr)
                    return curr;
            }
        }
    }
    return Watch::Pointer();
}
// ----------------------------------------------------------------------------
void Watch::SetSymbol(const wxString& symbol)
// ----------------------------------------------------------------------------
{
    m_symbol = symbol;
}

// ----------------------------------------------------------------------------
bool Watch::SetValue(const wxString &value)
// ----------------------------------------------------------------------------
{
    m_value = value;
    ConvertValueToUserFormat();
    return true;
}

// ----------------------------------------------------------------------------
void Watch::SetFormat(WatchFormat format)
// ----------------------------------------------------------------------------
{
    m_format = format;
}
// ----------------------------------------------------------------------------
WatchFormat Watch::GetFormat() const
// ----------------------------------------------------------------------------
{
    return m_format;
}
// ----------------------------------------------------------------------------
void Watch::SetArray(bool flag)
// ----------------------------------------------------------------------------
{
    m_is_array = flag;
}

// ----------------------------------------------------------------------------
bool Watch::IsArray() const
// ----------------------------------------------------------------------------
{
    return m_is_array;
}

// ----------------------------------------------------------------------------
void Watch::SetArrayParams(int start, int count)
// ----------------------------------------------------------------------------
{
    m_array_start = start;
    m_array_count = count;
}

// ----------------------------------------------------------------------------
int Watch::GetArrayStart() const
// ----------------------------------------------------------------------------
{
    return m_array_start;
}

// ----------------------------------------------------------------------------
int Watch::GetArrayCount() const
// ----------------------------------------------------------------------------
{
    return m_array_count;
}
// ----------------------------------------------------------------------------
void Watch::ConvertValueToUserFormat() // EditWatches support
// ----------------------------------------------------------------------------
{
    // User format requests can be: (from definitions.h)
    //  Undefined = 0,  ///< Format is undefined (whatever the debugger uses by default).
    //  Decimal,        ///< Variable should be displayed as decimal.
    //  Unsigned,       ///< Variable should be displayed as unsigned.
    //  Hex,            ///< Variable should be displayed as hexadecimal (e.g. 0xFFFFFFFF).
    //  Binary,         ///< Variable should be displayed as binary (e.g. 00011001).
    //  Char,           ///< Variable should be displayed as a single character (e.g. 'x').
    //  Float           ///< Variable should be displayed as floating point number (e.g. 14.35)

    wxString newStr;
    wxString type;
    wxString value;

    dbg_mi::WatchFormat format = GetFormat();
    if (GetParent())
    {
        cbWatch::Pointer p = GetParent();
        dbg_mi::Watch::Pointer wp = cb::static_pointer_cast<dbg_mi::Watch>(p);
        format = wp->GetFormat();
    }
    GetType(type);
    GetValue(value);

    if ((format == Undefined) or (value.Contains(_T("..."))) )
        return;

    do {
        if ( format == Decimal )
        {
                newStr = AnyToIntStr(value);
                break;
        }
        if ( format == Unsigned)
        {
                newStr = AnyToUIntStr(value);
                break;
        }
        if ( (format == Hex) and (not value.StartsWith(_T("0x"))))
        {
            if (type == _T("int")) newStr = IntToHexStr(value);
            if (type == _T("unsigned int")) newStr = UIntToHexStr(value);
            if (type == _T("unsigned char")) newStr = CharToHexStr(value);
            if (type == _T("string"))
            {
                std::string stdstr = std::string(value.mb_str());
                newStr = StdStrToHexStr(stdstr);
            }
            if (type == _T("wxString")) newStr = wxStringToHexStr(value);
                break;

        }
        if ( format == Binary)
        {
            newStr = UIntToBinStr(value);
            break;
        }
        if ( format == Char)
        {
                newStr = IntToCharStr(value);
                break;
        }
        if ( (format == Float) and (type not_eq _T("float")))
        {
                break;
        }
    }while(0);

    if (not newStr.empty())
        m_value = (newStr);
}
void Watch::SetDebugValue(wxString const &value)
{
    m_debug_value = value;
}
void Watch::GetDebugValue(wxString& debugValue)  // (ph 26/05/28)
{
    debugValue = m_debug_value;
}

// ----------------------------------------------------------------------------
struct PtrTypeToken //from GDB debugger_defs.cpp //(ph 2024/03/08)
// ----------------------------------------------------------------------------
{
    bool Match(const wxString &base, const char *term, size_t count) const
    {
        return base.compare(start, end - start, term, count) == 0;
    }

    size_t start, end;
};

/// Extract a token from a C/C++ type definition.
/// Tokens are things separated by white space.
/// '*' characters start or end tokens.
/// C++ template parameters are parsed as a single token.
// ----------------------------------------------------------------------------
static PtrTypeToken ConsumeToken(const wxString &s, size_t pos) //DebuggerGDB from GDB debugger_defs.cpp
// ----------------------------------------------------------------------------
{
    while (pos > 0 && (s[pos - 1] == ' ' || s[pos - 1] == '\t'))
    {
        --pos;
    }

    PtrTypeToken result;
    result.end = pos;

    int openedAngleBrackets = 0;

    while (pos > 0)
    {
        const char ch = s[pos - 1];
        // We want to ignore any '*' inside the angle brackets of a C++ template declaration.
        // To do so we expand the token until the end of the angle brackets.
        if (ch == '>')
            openedAngleBrackets++;
        else if (ch == '<')
            openedAngleBrackets--;
        else if (openedAngleBrackets == 0)
        {
            if (ch == ' ' || ch == '\t')
                break;
            if (ch == '*')
            {
                // If this is the start of the token consume the star and make a single character
                // token. If this is not the start of the token, just end the token, so the star
                // could be parsed at a separate token on the next iteration.
                if (pos == result.end)
                    --pos;
                break;
            }
        }
        --pos;
    }

    result.start = pos;
    return result;
}


/// The function expects valid type C/C++ type declarations, so there is no code to detect invalid
/// ones.
// ----------------------------------------------------------------------------
bool IsPointerType(const wxString &type) //from DebuggerGDB::debugger_def.cpp //(ph 2024/03/08)
// ----------------------------------------------------------------------------
{
    if (type.empty())
        return false;

    int numberOfStars = 0;
    size_t pos = type.length();
    do
    {
        const PtrTypeToken token = ConsumeToken(type, pos);
        if (token.start == token.end)
            return numberOfStars > 0;

        pos = token.start;

        if (token.end - token.start == 1)
        {
            if (type[token.start] == '*')
                numberOfStars++;
            else if (type[token.start] == '&')
                return false;
        }
        else
        {
            // char*, const char*, wchar_t and const wchar* should be treated as string and not as
            // array. This is some kind of arbitrary decision, I'm not 100% sure that it is useful.
            if (token.Match(type, "char", cbCountOf("char") - 1))
                return numberOfStars > 1;
            else if (token.Match(type, "wchar_t", cbCountOf("wchar_t") - 1))
                return numberOfStars > 1;
        }

    } while (true);

    // Should be unreachable
    return false;
}


////// ----------------------------------------------------------------------------
////GDBMemoryRangeWatch::GDBMemoryRangeWatch(uint64_t address, uint64_t size, const wxString& symbol) :
////// ----------------------------------------------------------------------------
////    m_address(address),
////    m_size(size),
////    m_symbol(symbol)
////{
////}

} // namespace dbg_mi

// ----------------------------------------------------------------------------
//  Globals
// ----------------------------------------------------------------------------
DebuggerLanguage g_DebugLanguage = dl_Cpp;
