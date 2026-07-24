/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 13106 $
 * $Id: parsewatchvalue.cpp 13106 2022-12-10 08:21:40Z wh11204 $
 * $HeadURL: https://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/debuggergdb/parsewatchvalue.cpp $
 */

#include <sdk.h>

#ifndef CB_PRECOMP
#   include <wx/regex.h>
#endif

#include "projectmanager.h"
#include "logmanager.h"

#include "definitions.h"
#include "cmd_result_parser.h" // (ph 26/06/04)
#include "parsewatchvalue.h"

// ----------------------------------------------------------------------------
struct Token
// ----------------------------------------------------------------------------
{
    enum Type
    {
        Undefined,
        OpenBrace,
        CloseBrace,
        Equal,
        String,
        Comma
    };

    Token() :
        start(0),
        end(0),
        type(Undefined),
        hasRepeatedChar(false)
    {
    }
    Token(int start_, int end_, Type type_) :
        start(start_),
        end(end_),
        type(type_),
        hasRepeatedChar(false)
    {
    }

    bool operator == (Token const &t) const
    {
        return start == t.start && end == t.end && type == t.type;
    }
    wxString ExtractString(wxString const &s) const
    {
        assert(end <= static_cast<int>(s.length()));
        return s.substr(start, end - start);
    }

    void Trim(wxString const &s)
    {
        while (start < static_cast<int>(s.length())
               && (s[start] == wxT(' ') || s[start] == wxT('\t') || s[start] == wxT('\n')))
            start++;
        while (end > 0
               && (s[end - 1] == wxT(' ') || s[end - 1] == wxT('\t') || s[end - 1] == wxT('\n')))
            end--;
    }

    int start, end;
    Type type;
    bool hasRepeatedChar;
};

wxRegEx regexRepeatedChars(wxT("^((\\\\'.{1,6}\\\\')|('.{1,6}'))[[:blank:]](<repeats[[:blank:]][0-9]+[[:blank:]]times>)"),
#ifndef __WXMAC__
                           wxRE_ADVANCED);
#else
                           wxRE_EXTENDED);
#endif

/// GDB can shorten the string. Such strings are printed as '"value"...'.
/// This function moves position, so that the dot characters become part of the token.
/// @return The new position if there are dots or the old position.
// ----------------------------------------------------------------------------
inline int SkipShortenedString(wxString const &str, int pos)
// ----------------------------------------------------------------------------
{
    while (pos < static_cast<int>(str.length()) && str[pos]==wxT('.'))
        ++pos;
    return pos;
}

// ----------------------------------------------------------------------------
inline int DetectRepeatingSymbols(wxString const &str, int pos)
// ----------------------------------------------------------------------------
{
    int newPos = -1, currPos = pos;
    while (1)
    {
        if (currPos + 4 >= static_cast<int>(str.length()))
            break;
        if (str[currPos + 1] != wxT(','))
            break;
        if (str[currPos + 3] == wxT('\''))
        {
            const wxString &s = str.substr(currPos + 3, str.length() - (currPos + 3));
            if (regexRepeatedChars.Matches(s))
            {
                size_t start, length;
                regexRepeatedChars.GetMatch(&start, &length, 0);
                newPos = currPos + 3 + length;
                if ((newPos + 4 < static_cast<int>(str.length()))
                    && str[newPos] == wxT(',') && str[newPos + 2] == wxT('"'))
                {
                    newPos += 3;
                    while (newPos < static_cast<int>(str.length()) && str[newPos] != wxT('"'))
                        ++newPos;
                    if (newPos + 1 < static_cast<int>(str.length()) && str[newPos] == wxT('"'))
                        ++newPos;
                }
                currPos = newPos;
            }
            else
                break;
        }
        else
            break;

        // move the current position to point at the '"' character
        currPos--;
    }
    return newPos;
}

// ----------------------------------------------------------------------------
inline bool GetNextToken(wxString const &str, int pos, Token &token)
// ----------------------------------------------------------------------------
{
    token.hasRepeatedChar = false;
    while (pos < static_cast<int>(str.length())
           && (str[pos] == _T(' ') || str[pos] == _T('\t') || str[pos] == _T('\n')))
        ++pos;

    if (pos >= static_cast<int>(str.length()))
        return false;

    token.start = -1;
    bool in_quote = false, in_char = false;
    int open_braces = 0;
    struct BraceType { enum Enum { None, Angle, Square, Normal }; };
    BraceType::Enum brace_type = BraceType::None;

    switch (static_cast<wxChar>(str[pos]))
    {
    case _T('='):
        token = Token(pos, pos + 1, Token::Equal);
        return true;
    case _T(','):
        token = Token(pos, pos + 1, Token::Comma);
        return true;
    case _T('{'):
        token = Token(pos, pos + 1, Token::OpenBrace);
        return true;
    case _T('}'):
        token = Token(pos, pos + 1, Token::CloseBrace);
        return true;

    case _T('"'):
        in_quote = true;
        token.type = Token::String;
        token.start = pos;
        break;
    case _T('\''):
        in_char = true;
        token.type = Token::String;
        token.start = pos;
        break;
    case _T('<'):
        token.type = Token::String;
        token.start = pos;
        open_braces = 1;
        brace_type = BraceType::Angle;
        break;
    case wxT('['):
        token.type = Token::String;
        token.start = pos;
        open_braces = 1;
        brace_type = BraceType::Square;
        break;
    case wxT('('):
        token.type = Token::String;
        open_braces = 1;
        brace_type = BraceType::Normal;
        token.start = pos;
        break;
    default:
        token.type = Token::String;
        token.start = pos;
    }
    ++pos;

    bool escape_next = false;
    while (pos < static_cast<int>(str.length()))
    {
        if (open_braces == 0)
        {
            if (str[pos] == _T(',') && !in_quote)
            {
                token.end = pos;
                return true;
            }
            else if ((str[pos] == _T('=') || str[pos] == _T('{') || str[pos] == _T('}')) && !in_quote && !in_char)
            {
                token.end = pos;
                return true;
            }
            else if (str[pos] == _T('"'))
            {
                if (in_quote)
                {
                    if (!escape_next)
                    {
                        int newPos = DetectRepeatingSymbols(str, pos);
                        if (newPos != -1)
                        {
                            token.hasRepeatedChar = true;
                            token.end = SkipShortenedString(str, newPos);
                            return true;
                        }
                        else
                        {
                            token.end = SkipShortenedString(str, pos + 1);
                            return true;
                        }
                    }
                    else
                        escape_next = false;
                }
                else
                {
                    if (escape_next)
                        return false;
                    in_quote = true;
                }
            }
            else if (str[pos] == _T('\''))
            {
                if (!escape_next)
                    in_char = !in_char;
                escape_next = false;
            }
            else if (str[pos] == _T('\\'))
                escape_next = true;
            else
                escape_next = false;

            switch (brace_type)
            {
                case BraceType::Angle:
                    if (str[pos] == wxT('<'))
                        open_braces++;
                    break;
                case BraceType::Square:
                    if (str[pos] == wxT('['))
                        open_braces++;
                    break;
                case BraceType::None:
                default:
                    break;
            }
        }
        else
        {
            switch (brace_type)
            {
                case BraceType::Angle:
                    if (str[pos] == wxT('<'))
                        open_braces++;
                    else if (str[pos] == wxT('>'))
                        --open_braces;
                    break;
                case BraceType::Square:
                    if (str[pos] == wxT('['))
                        open_braces++;
                    else if (str[pos] == wxT(']'))
                        --open_braces;
                    break;
                case BraceType::Normal:
                    if (str[pos] == wxT('('))
                        open_braces++;
                    else if (str[pos] == wxT(')'))
                        --open_braces;
                    break;
                case BraceType::None:
                default:
                    break;
            }
        }
        ++pos;
    }

    if (in_quote)
    {
        token.end = -1;
        return false;
    }
    else
    {
        token.end = pos;
        return true;
    }
}

// ----------------------------------------------------------------------------
inline cb::shared_ptr<dbg_mi::Watch> AddChild(cb::shared_ptr<dbg_mi::Watch> parent, wxString const &full_value, Token &name)
// ----------------------------------------------------------------------------
{
    wxString const &str_name = name.ExtractString(full_value);
    cb::shared_ptr<dbg_mi::Watch>old_child = std::dynamic_pointer_cast<dbg_mi::Watch>(parent->FindChild(str_name));
    cb::shared_ptr<dbg_mi::Watch> child;
    if (old_child)
        child = cb::static_pointer_cast<dbg_mi::Watch>(old_child);
    else
    {
        cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        child = cb::shared_ptr<dbg_mi::Watch>(new dbg_mi::Watch(str_name,false, pProject));
        dbg_mi::Watch::AddChild(parent, child);
    }
    child->MarkAsRemoved(false);
    return child;
}

// ----------------------------------------------------------------------------
inline cb::shared_ptr<dbg_mi::Watch> AddChild(cb::shared_ptr<dbg_mi::Watch> parent, wxString const &str_name)
// ----------------------------------------------------------------------------
{
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    int index = parent->FindChildIndex(str_name);
    cb::shared_ptr<dbg_mi::Watch> child;
    if (index != -1)
        child = cb::static_pointer_cast<dbg_mi::Watch>(parent->GetChild(index));
    else
    {
        child = cb::shared_ptr<dbg_mi::Watch>(new dbg_mi::Watch(str_name, false, pProject));
        dbg_mi::Watch::AddChild(parent, child);
    }
    child->MarkAsRemoved(false);
    return child;
}
// ----------------------------------------------------------------------------
// Rewrite the parse code assuming I'll use the command -stack-list-variables 1 .
// ----------------------------------------------------------------------------
//Since you are using -stack-list-variables 1, GDB will return a structured list where each entry is guaranteed to have a name, a type, and a value.
//
// This rewritten parser specifically targets that structure. It uses a robust
// "search and extract" method that handles the attributes in any order (GDB
// sometimes shifts the order of type and arg). It also includes the findClosingQuote
// helper to ensure that complex C++ values (like strings with escaped quotes)
// don't break the logic.
//Refined C++ Parser
//C++

#include <iostream>
#include <string>
#include <vector>

//struct GdbVar {
//    std::string name;
//    std::string type;
//    std::string value;
//    bool isArgument;
//};

// Helper to extract a value regardless of quotes
// ----------------------------------------------------------------------------
std::string ExtractValue(const std::string& tuple, const std::string& key)
// ----------------------------------------------------------------------------
{
    std::string searchKey = key + "=";
    size_t keyPos = tuple.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t valStart = keyPos + searchKey.length();
    if (valStart >= tuple.length()) return "";

    // Case 1: Quoted value (e.g., name="mbar")
    if (tuple[valStart] == '"') {
        valStart++; // Skip opening quote
        size_t valEnd = tuple.find('"', valStart);
        if (valEnd == std::string::npos) return "";
        return tuple.substr(valStart, valEnd - valStart);
    }
    // Case 2: Unquoted value (e.g., name=mbar)
    else {
        // Value ends at a comma or the closing brace of the tuple
        size_t valEnd = tuple.find_first_of(",}", valStart);
        return tuple.substr(valStart, valEnd - valStart);
    }
}
// Helper to find the end of a quoted string, skipping escaped quotes
// ----------------------------------------------------------------------------
size_t findClosingQuote(const std::string& str, size_t start)
// ----------------------------------------------------------------------------
{
    for (size_t i = start; i < str.length(); ++i) {
        if (str[i] == '\"') {
            // If the quote is escaped by a backslash, it's part of the data, not the end
            if (i > 0 && str[i - 1] == '\\') continue;
            return i;
        }
    }
    return std::string::npos;
}

// ----------------------------------------------------------------------------
std::vector<GdbVar> ParseGDBMIWatchValues(const std::string& input)
// ----------------------------------------------------------------------------
{
    LogManager* pLog = Manager::Get()->GetLogManager();
    pLog->DebugLog("Input length: " + std::to_string(input.length()));
    pLog->DebugLog("Found 'variables=' at: " + std::to_string(input.find("variables=[")));
    pLog->DebugLog("input arg: " + input);
    std::vector<GdbVar> vars;

    // Find the start of the variables list
    size_t pos = input.find("variables=[");
    if (pos == std::string::npos) return vars;

    pos += 11; // Move past "variables=["

    // Continue until we hit the end of the variables list ']'
    while (pos < input.length() && input[pos] != ']') {
        // Find the bounds of the current variable tuple { ... }
        size_t tupleStart = input.find('{', pos);
        if (tupleStart == std::string::npos) break;

        size_t tupleEnd = input.find('}', tupleStart);
        if (tupleEnd == std::string::npos) break;

        // Isolate the tuple string
        std::string tuple = input.substr(tupleStart, tupleEnd - tupleStart + 1);

        GdbVar v;

        // 1. Extract Fields using the helper
        v.name  = ExtractValue(tuple, "name");
        v.type  = ExtractValue(tuple, "type");
        v.value = ExtractValue(tuple, "value");

        // 2. Check if it is an Argument
        // GDB/MI usually represents this as arg="1" or sometimes arg=1
        std::string argVal = ExtractValue(tuple, "arg");
        v.isArgument = (argVal == "1");

        vars.push_back(v);

        // Move pos forward for the next iteration
        pos = tupleEnd + 1;
    }

    return vars;
}

//Key Improvements in this Version
//
//    Tuple Isolation: By using input.substr to isolate the text between { and }, the parser is much safer. It won't accidentally find the type of "Variable B" while it's still trying to process "Variable A."
//
//    Order Independence: GDB/MI output isn't always consistent with the order of fields (e.g., arg might come before or after type). This version finds each key independently within the local tuple.
//
//    Robust Escaping: Since you are working with wxString, the values will often contain literal quotes. findClosingQuote ensures that a value like L"Click \"OK\" to continue" is captured entirely as one string.
//
//    No Trailing Comma Issues: GDB puts commas between tuples },{. By explicitly searching for the next { or using the tupleEnd + 1 logic, this code skips over those separators smoothly.
//
//How to use the results
//
//Once you have the std::vector<GdbVar>, you can easily populate a UI table or tree:
//Name	Type	Value	Scope
//this	HelloWxWorldFrame*	0x103d070	Argument
//title	const wxString&	L"wxWidgets App"	Argument
//mbar	wxMenuBar*	0x1044c50	Local

wxRegEx regexRepeatedChar(wxT(".+[[:blank:]](<repeats[[:blank:]][0-9]+[[:blank:]]times>)$"));
wxRegEx regexFortranArray(wxT("^\\([0-9,]+\\)$"));


    #define RAWB_DEBUG // <-- Turn it on
#include "wx/ffile.h"
// ----------------------------------------------------------------------------
inline bool ParseGDBWatchValue(cb::shared_ptr<dbg_mi::Watch> watch, wxString const &value, int &start, int length)
// ----------------------------------------------------------------------------
{

// TEMPORARY DEBUG PRINT:
    #ifdef RAWB_DEBUG
    if (value.Contains("Vector"))
    {
        wxFFile file(wxT("C:\\temp\\gdb_raw_value.txt"), wxT("a"));
        if (file.IsOpened()) {
            file.Write(wxT("--- NEW VALUE PAYLOAD ---\n"));
            file.Write(value + wxT("\n"));
            file.Close();
        }
    }
    #endif
    #undef RAWB_DEBUG // <-- Turn it off


    watch->SetDebugValue(value);
    watch->MarkChildsAsRemoved();

    wxString watchSymbol;
    watch->GetSymbol(watchSymbol);
    bool isFortranArray = false;
    if (g_DebugLanguage == dl_Fortran)
        isFortranArray = regexFortranArray.Matches(watchSymbol);
    wxString watchSymbolNew;
    bool hasNewWatchSymbol = false;

    int position = start;
    Token token, token_name, token_value;
    wxString pythonToStringValue;
    bool skip_comma = false;
    bool last_was_closing_brace = false;
    int added_children = 0;
    int token_real_end = 0;
    while (GetNextToken(value, position, token))
    {
        token_real_end = token.end;
        token.Trim(value);
        const wxString &str = token.ExtractString(value);
        if (str.StartsWith(wxT("members of ")))
        {
            wxString::size_type pos = str.find(wxT('\n'));
            if (pos == wxString::npos)
            {
                // If the token has no '\n' character, then we have to search the whole value
                // for the token and then we skip this token completely.
                wxString::size_type pos_val = value.find(wxT('\n'), token_real_end);
                if (pos_val == wxString::npos)
                    return false;
                position = pos_val+1;
                if (length > 0 && position >= start + length)
                    break;
                continue;
            }
            else
            {
                // If we have the '\n' in the token, then we have the next valid token, too,
                // so we correct the current token to be the correct one.
                if (str.find_last_of(wxT(':'), pos) == wxString::npos)
                    return false;
                token.start += pos + 2;
                token.Trim(value);
            }
        }

        if (!token.hasRepeatedChar && regexRepeatedChar.Matches(str) && g_DebugLanguage != dl_Fortran)
        {
            Token expanded_token = token;
            while (1)
            {
                if (value[expanded_token.end] == wxT(','))
                {
                    position = token.end + 1;
                    token_real_end = position;
                    int comma_end = expanded_token.end;
                    if (GetNextToken(value, position, expanded_token))
                    {
                        const wxString &expanded_str = expanded_token.ExtractString(value);
                        if (!expanded_str.empty() && (expanded_str[0] != wxT('"') && expanded_str[0] != wxT('\'')))
                        {
                            token.end = comma_end;
                            position = comma_end;
                            token_real_end = comma_end;
                            break;
                        }
                        token.end = expanded_token.end;
                        if (regexRepeatedChar.Matches(expanded_str))
                            continue;
                        token_real_end = expanded_token.end;
                    }
                }
                else if (expanded_token.end == static_cast<int>(value.length()) || value[expanded_token.end] == wxT('}'))
                {
                    token.end = expanded_token.end;
                    token_real_end = expanded_token.end;
                }
                break;
            }
        }

        switch (token.type)
        {
        case Token::String:
            if (token_name.type == Token::Undefined)
                token_name = token;
            else if (token_value.type == Token::Undefined)
            {
                if (   wxIsdigit(str[0])
                    || str[0]==wxT('\'')
                    || str[0]==wxT('"')
                    || str[0]==wxT('<')
                    || str[0]==wxT('-')
                    || str.StartsWith(wxT("L\""))
                    || str.StartsWith(wxT("L'")) )
                {
                    token_value = token;
                }
                else
                {
                    // Detect strings generated by python pretty printing to_string() method.
                    Token expanded_token = token;
                    int firstCloseBrace = -1;
                    for (; expanded_token.end < static_cast<int>(value.length()); ++expanded_token.end)
                    {
                        if (value[expanded_token.end] == wxT('='))
                        {
                            bool foundBrace = false;
                            for (int ii = expanded_token.end + 1; ii < static_cast<int>(value.length()); ++ii)
                            {
                                if (value[ii] == wxT('{'))
                                {
                                    foundBrace = true;
                                    break;
                                }
                                else if (value[ii] != wxT(' ') && value[ii] != wxT('\t')
                                         && value[ii] != wxT('\n') && value[ii] != wxT(' '))
                                {
                                    break;
                                }

                            }
                            if (foundBrace)
                            {
                                token.end = token_real_end = expanded_token.end;
                                token_value = token;
                                token_value.end--;
                                pythonToStringValue = token_value.ExtractString(value);
                            }
                            else
                            {
                                while (expanded_token.end >= 0)
                                {
                                    if (value[expanded_token.end] == wxT(','))
                                    {
                                        token.end = token_real_end = expanded_token.end;
                                        token_value = token;
                                        pythonToStringValue = token_value.ExtractString(value);
                                        break;
                                    }
                                    expanded_token.end--;
                                }
                            }
                            break;
                        }
                        else if (firstCloseBrace == -1 && value[expanded_token.end] == wxT('}'))
                        {
                            firstCloseBrace=expanded_token.end;
                            break;
                        }
                    }

                    if (pythonToStringValue.empty())
                    {
                        if (firstCloseBrace == -1)
                            return false;
                        token.end = token_real_end = firstCloseBrace;
                        token_value = token;
                        pythonToStringValue = token_value.ExtractString(value);
                        if (pythonToStringValue.empty())
                            return false;
                    }
                }
            }
            else
                return false;
            last_was_closing_brace = false;
            break;
        case Token::Equal:
            last_was_closing_brace = false;
            break;
        case Token::Comma:
            pythonToStringValue = wxEmptyString;
            last_was_closing_brace = false;
            if (skip_comma)
                skip_comma = false;
            else
            {
                if (token_name.type != Token::Undefined)
                {
                    if (token_value.type != Token::Undefined)
                    {
                        cb::shared_ptr<dbg_mi::Watch> child = AddChild(watch, value, token_name);
                        child->SetValue(token_value.ExtractString(value));
                    }
                    else
                    {
                        cb::shared_ptr<dbg_mi::Watch> child;
                        if (g_DebugLanguage == dl_Cpp)
                        {
                            int start_arr = watch->IsArray() ? watch->GetArrayStart() : 0;
                            child = AddChild(watch, wxString::Format(wxT("[%d]"), start_arr + added_children));
                        }
                        else // g_DebugLanguage == dl_Fortran
                        {
                            int start_arr = watch->IsArray() ? watch->GetArrayStart() : 1;
                            wxString childSymbol;
                            if (isFortranArray)
                            {
                                childSymbol = wxString::Format(wxT("(%d,"), start_arr + added_children);
                                childSymbol << watchSymbol.Mid(1);
                            }
                            else
                            {
                                childSymbol = wxString::Format(wxT("(%d)"), start_arr + added_children);
                            }
                            child = AddChild(watch, childSymbol);
                        }
                        child->SetValue(token_name.ExtractString(value));
                    }
                    token_name.type = token_value.type = Token::Undefined;
                    added_children++;
                }
                else
                    return false;
            }
            break;
        case Token::OpenBrace:
            {
                cb::shared_ptr<dbg_mi::Watch> child;
                if(token_name.type == Token::Undefined)
                {
                    if (g_DebugLanguage == dl_Cpp)
                    {
                        int start_arr = watch->IsArray() ? watch->GetArrayStart() : 0;
                        child = AddChild(watch, wxString::Format(wxT("[%d]"), start_arr + added_children));
                    }
                    else // g_DebugLanguage == dl_Fortran
                    {
                        int start_arr = watch->IsArray() ? watch->GetArrayStart() : 1;
                        wxString childSymbol;
                        if (isFortranArray)
                        {
                            childSymbol = wxString::Format(wxT("(%d,"), start_arr + added_children);
                            childSymbol << watchSymbol.Mid(1);
                        }
                        else
                        {
                            childSymbol = wxString::Format(wxT("(%d)"), start_arr + added_children);
                        }
                        child = AddChild(watch, childSymbol);
                    }
                }
                else if (g_DebugLanguage == dl_Fortran)
                    child = AddChild(watch, token_name.ExtractString(value));
                else
                    child = AddChild(watch, value, token_name);

                if (g_DebugLanguage == dl_Fortran)
                    child->SetValue(wxEmptyString);
                else if (!pythonToStringValue.empty())
                    child->SetValue(pythonToStringValue);
                position = token_real_end;
                added_children++;

                if(!ParseGDBWatchValue(child, value, position, 0)) //recursion
                    return false;

                if(g_DebugLanguage == dl_Fortran && isFortranArray && !hasNewWatchSymbol && token_name.type == Token::Undefined)
                {
                    // Change watch symbol on the way back for Fortran multidimensional arrays
                    wxString childSymbol;
                    child->GetSymbol(childSymbol);
                    wxString::size_type pos = childSymbol.find_last_of(wxT(':'));
                    if (pos != wxString::npos)
                    {
                        wxString::size_type pos_com = childSymbol.find(wxT(','), pos+2);
                        if (pos_com != wxString::npos)
                        {
                            watchSymbolNew = childSymbol;
                            watchSymbolNew.replace(pos+2, pos_com-(pos+2), wxT(':'));
                            hasNewWatchSymbol = true;
                        }
                    }
                    else
                    {
                        watchSymbolNew << wxT("(:,") << watchSymbol.Mid(1);
                        hasNewWatchSymbol = true;
                    }
                }

                token_real_end = position;
                token_name.type = token_value.type = Token::Undefined;
                skip_comma = true;
                last_was_closing_brace = true;
            }
            break;
        case Token::CloseBrace:
            if (!last_was_closing_brace)
            {
                if (token_name.type != Token::Undefined)
                {
                    if (token_value.type != Token::Undefined)
                    {
                        cb::shared_ptr<dbg_mi::Watch> child = AddChild(watch, value, token_name);
                        child->SetValue(token_value.ExtractString(value));
                    }
                    else
                    {
                        cb::shared_ptr<dbg_mi::Watch> child;
                        if (g_DebugLanguage == dl_Cpp)
                        {
                            int start_arr = watch->IsArray() ? watch->GetArrayStart() : 0;
                            child = AddChild(watch, wxString::Format(wxT("[%d]"), start_arr + added_children));
                        }
                        else // g_DebugLanguage == dl_Fortran
                        {
                            int start_arr = watch->IsArray() ? watch->GetArrayStart() : 1;
                            wxString childSymbol;
                            if (isFortranArray)
                            {
                                childSymbol = wxString::Format(wxT("(%d,"), start_arr + added_children);
                                childSymbol << watchSymbol.Mid(1);
                                if (!hasNewWatchSymbol)
                                {
                                    watchSymbolNew << wxT("(:,") << watchSymbol.Mid(1);
                                    hasNewWatchSymbol = true;
                                }
                            }
                            else
                            {
                                childSymbol = wxString::Format(wxT("(%d)"), start_arr + added_children);
                            }
                            child = AddChild(watch, childSymbol);
                        }
                        child->SetValue(token_name.ExtractString(value));
                    }
                    token_name.type = token_value.type = Token::Undefined;
                    added_children++;
                }
                else
                    watch->SetValue(wxT(""));
            }

            if (hasNewWatchSymbol)
                watch->SetSymbol(watchSymbolNew);

            start = token_real_end;
            return true;
        case Token::Undefined:
        default:
            return false;
        }

        position = token_real_end;
        if (length > 0 && position >= start + length)
            break;
    }

    start = position + 1;
    if (token_name.type != Token::Undefined)
    {
        if (token_value.type != Token::Undefined)
        {
            cb::shared_ptr<dbg_mi::Watch> child = AddChild(watch, value, token_name);
            child->SetValue(token_value.ExtractString(value));
        }
        else
        {
            cb::shared_ptr<dbg_mi::Watch> child;
            if (g_DebugLanguage == dl_Cpp)
            {
                int start_arr = watch->IsArray() ? watch->GetArrayStart() : 0;
                child = AddChild(watch, wxString::Format(wxT("[%d]"), start_arr + added_children));
            }
            else // g_DebugLanguage == dl_Fortran
            {
                int start_arr = watch->IsArray() ? watch->GetArrayStart() : 1;
                child = AddChild(watch, wxString::Format(wxT("(%d)"), start_arr + added_children));
            }
            child->SetValue(token_name.ExtractString(value));
        }
    }



    return true;
}

// ----------------------------------------------------------------------------
inline wxString RemoveWarnings(wxString const &input)
// ----------------------------------------------------------------------------
{
    wxString::size_type pos = input.find(wxT('\n'));

    if (pos == wxString::npos)
        return input;

    wxString::size_type lastPos = 0;
    wxString result;

    while (pos != wxString::npos)
    {
        wxString const &line = input.substr(lastPos, pos - lastPos);

        if (!line.StartsWith(wxT("warning:")))
        {
            result += line;
            result += wxT('\n');
        }

        lastPos = pos + 1;
        pos = input.find(wxT('\n'), lastPos);
    }

    if (lastPos < input.length())
        result += input.substr(lastPos, input.length() - lastPos);

    return result;
}

// ----------------------------------------------------------------------------
inline void RemoveBefore(wxString &str, const wxString &s)
// ----------------------------------------------------------------------------
{
    wxString::size_type pos = str.find(s);
    if (pos != wxString::npos)
    {
        str.Remove(0, pos+s.length());
        str.Trim(false);
    }
}

// ----------------------------------------------------------------------------
void PrepareFortranOutput(wxString& outStr)
// ----------------------------------------------------------------------------
{
    static wxRegEx nan_line(wxT("nan\\([a-zA-Z0-9]*\\)"));
    nan_line.Replace(&outStr, wxT("nan"));
    outStr.Replace(wxT("("),wxT("{"));
    outStr.Replace(wxT(")"),wxT("}"));
}

// ----------------------------------------------------------------------------
bool ParseGDBWatchValue(cb::shared_ptr<dbg_mi::Watch> watch, wxString const &inputValue)
// ----------------------------------------------------------------------------
{
    //Gemini: // (ph 26/06/02)

    if(inputValue.empty())
    {
        watch->SetValue(inputValue);
        return true;
    }

    wxString OldValue; watch->GetValue(OldValue); // (ph 26/06/03)
    wxString value = RemoveWarnings(inputValue);

    if (g_DebugLanguage == dl_Fortran)
        PrepareFortranOutput(value);

    // 1. Sanitize GDB Wide-Struct Formatting Envelopes (e.g., L"{...}" or L"...")
    bool hasOuterWideWrapper = false;
    if (value.StartsWith(wxT("L\"")) && value.EndsWith(wxT("\"")))
    {
        value = value.substr(2, value.length() - 3);
        hasOuterWideWrapper = true;
    }
    else if (value.StartsWith(wxT("L{")) && value.EndsWith(wxT("}")))
    {
        value = value.substr(1);
    }

    // 2. Identify structural brace groupings
    wxString::size_type start = value.find(wxT('{'));

    if (start != wxString::npos && value[value.length() - 1] == wxT('}'))
    {
        wxString primaryValue = wxEmptyString;

        // Normalize GDB text escape layouts for safe searching
        wxString cleanValue = value;
        cleanValue.Replace(wxT("\\\""), wxT("\""));

        // 3. Pinpoint the string value payload.
        // We look specifically for data markers like "_M_p =" to avoid matching outer structural wrappers
        wxString::size_type targetPos = cleanValue.find(wxT("_M_p ="));
        if (targetPos == wxString::npos)
        {
            targetPos = cleanValue.find(wxT("="));
        }

        if (targetPos != wxString::npos)
        {
            // Scan forward from our assignment operator for the contents inside quotes
            wxString::size_type quoteStart = cleanValue.find(wxT('"'), targetPos);
            if (quoteStart != wxString::npos)
            {
                wxString::size_type quoteEnd = cleanValue.find(wxT('"'), quoteStart + 1);
                if (quoteEnd != wxString::npos)
                {
                    primaryValue = cleanValue.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

                    // Clean up trailing/leading redundant literal markers if GDB left them behind
                    if (primaryValue.StartsWith(wxT("L\"")) && primaryValue.EndsWith(wxT("\"")))
                        primaryValue = primaryValue.substr(2, primaryValue.length() - 3);
                    else if (primaryValue.StartsWith(wxT("L\\\"")) && primaryValue.EndsWith(wxT("\\\"")))
                        primaryValue = primaryValue.substr(3, primaryValue.length() - 5);
                    else if (primaryValue.StartsWith(wxT("L")))
                        primaryValue = primaryValue.substr(1);
                }
            }
        }

        // Re-wrap cleanly into a uniform IDE string presentation format
        if (not primaryValue.empty())
        {
            //-primaryValue = wxT("L\"") + primaryValue + wxT("\"");
            // Leave it as is: // (ph 26/06/02)

        }

        // Apply our isolated string preview directly to the parent watch node
        watch->SetValue(primaryValue);
        if (primaryValue != OldValue)
            watch->MarkAsChanged(true); // (ph 26/06/03)

        int t_start = start + 1;
        bool result = ParseGDBWatchValue(watch, value, t_start, value.length() - 2);
        if (result)
        {
            if (start > 0)
            {
                wxString referenceValue = value.substr(0, start);
                referenceValue.Trim(true);
                referenceValue.Trim(false);
                if (referenceValue.EndsWith(wxT("=")))
                {
                    referenceValue.RemoveLast(1);
                    referenceValue.Trim(true);
                }

                if (!primaryValue.empty())
                    watch->SetValue(referenceValue + wxT(" ") + primaryValue);
                else
                    watch->SetValue(referenceValue);
            }
            watch->RemoveMarkedChildren();
        }
        return result;
    }
    else
    {
        if (hasOuterWideWrapper && !inputValue.StartsWith(value))
            watch->SetValue(inputValue);
        else
            watch->SetValue(value);

        watch->RemoveChildren();
        return true;
    }
    return false;
}

//
//    struct HWND__ * 0x7ffd8000
//
//    struct tagWNDCLASSEXA
//       +0x000 cbSize           : 0x7c8021b5
//       +0x004 style            : 0x7c802011
//       +0x008 lpfnWndProc      : 0x7c80b529     kernel32!GetModuleHandleA+0
//       +0x00c cbClsExtra       : 0
//       +0x010 cbWndExtra       : 2147319808
//       +0x014 hInstance        : 0x00400000
//       +0x018 hIcon            : 0x0012fe88
//       +0x01c hCursor          : 0x0040a104
//       +0x020 hbrBackground    : 0x689fa962
//       +0x024 lpszMenuName     : 0x004028ae  "???"
//       +0x028 lpszClassName    : 0x0040aa30  "CodeBlocksWindowsApp"
//       +0x02c hIconSm          : (null)
//
//    char * 0x0040aa30
//     "CodeBlocksWindowsApp"
//
//    char [16] 0x0012fef8
//    116 't'
//
//    int [10] 0x0012fee8
//    0

// ----------------------------------------------------------------------------
bool ParseCDBWatchValue(cb::shared_ptr<dbg_mi::Watch> watch, wxString const &value)
// ----------------------------------------------------------------------------
{
    wxArrayString lines = GetArrayFromString(value, wxT('\n'));
    watch->SetDebugValue(value);
    watch->MarkChildsAsRemoved();

    if (lines.GetCount() == 0)
        return false;

    static wxRegEx unexpected_error(wxT("^Unexpected token '.+'$"));
    static wxRegEx resolve_error(wxT("^Couldn't resolve error at '.+'$"));

    // search for errors
    for (unsigned ii = 0; ii < lines.GetCount(); ++ii)
    {
        if (unexpected_error.Matches(lines[ii])
            || resolve_error.Matches(lines[ii])
            || lines[ii] == wxT("No pointer for operator* '<EOL>'"))
        {
            watch->SetValue(lines[ii]);
            return true;
        }
    }

    if (lines.GetCount() == 1)
    {
        wxArrayString tokens = GetArrayFromString(lines[0], wxT(' '));
        if (tokens.GetCount() < 2)
            return false;

        int type_token = 0;
        if (tokens[0] == wxT("class") || tokens[0] == wxT("struct"))
            type_token = 1;

        if (static_cast<int>(tokens.GetCount()) < type_token + 2)
            return false;

        int value_start = type_token + 1;
        if (tokens[type_token + 1] == wxT('*'))
        {
            watch->SetType(tokens[type_token] + tokens[type_token + 1]);
            value_start++;
        }
        else
            watch->SetType(tokens[type_token]);

        if(value_start >= static_cast<int>(tokens.GetCount()))
            return false;

        watch->SetValue(tokens[value_start]);
        watch->RemoveMarkedChildren();
        return true;
    }
    else
    {
        wxArrayString tokens = GetArrayFromString(lines[0], wxT(' '));

        if (tokens.GetCount() < 2)
            return false;

        bool set_type = true;
        if (tokens.GetCount() > 2)
        {
            if (tokens[0] == wxT("struct") || tokens[0] == wxT("class"))
            {
                if (tokens[2] == wxT('*') || tokens[2].StartsWith(wxT("[")))
                {
                    watch->SetType(tokens[1] + tokens[2]);
                    set_type = false;
                }
            }
            else
            {
                if (tokens[1] == wxT('*') || tokens[1].StartsWith(wxT("[")))
                {

                    watch->SetType(tokens[0] + tokens[1]);
                    watch->SetValue(lines[1]);
                    return true;
                }
            }
        }

        if (set_type)
            watch->SetType(tokens[1]);

        static wxRegEx class_line(wxT("[[:blank:]]*\\+(0x[0-9a-f]+)[[:blank:]]([a-zA-Z0-9_]+)[[:blank:]]+:[[:blank:]]+(.+)"));
        if (!class_line.IsValid())
        {
            int *p = NULL;
            *p = 0;
        }
        else
        {
            if (!class_line.Matches(wxT("   +0x000 a                : 10")))
            {
                int *p = NULL;
                *p = 0;
            }
        }

        for (unsigned ii = 1; ii < lines.GetCount(); ++ii)
        {
            if (class_line.Matches(lines[ii]))
            {
                cb::shared_ptr<dbg_mi::Watch> w = AddChild(watch, class_line.GetMatch(lines[ii], 2));
                w->SetValue(class_line.GetMatch(lines[ii], 3));
                w->SetDebugValue(lines[ii]);
            }
        }
        watch->RemoveMarkedChildren();
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Helper utility to slice out a clean text preview from raw GCC string internals
wxString ExtractStringPreview(wxString const &raw_val)
// ----------------------------------------------------------------------------
{
    if (raw_val.Contains(wxT("_M_string_length")) || raw_val.Contains(wxT("_M_p =")))
    {
        size_t content_start = raw_val.find(wxT("\""));
        if (content_start != wxString::npos)
        {
            content_start += 1;
            size_t content_end = raw_val.find(wxT("\\\"}"), content_start);
            if (content_end == wxString::npos)
                content_end = raw_val.find(wxT("\"},"), content_start);
            if (content_end == wxString::npos)
                content_end = raw_val.find_last_of(wxT("\""));

            if (content_end != wxString::npos && content_end > content_start)
            {
                wxString extracted = raw_val.Mid(content_start, content_end - content_start);
                extracted.Replace(wxT("\\\""), wxT("\""));
                extracted.Replace(wxT("\\\\"), wxT("\\"));
                return extracted;
            }
        }
    }
    return wxEmptyString;
}

// ----------------------------------------------------------------------------
// Unified GDB/MI Watch Parser: Handles all data structures cleanly
// (ph 26/06/04) Gemini - Fixed missing expansion boxes for collections
bool ParseGDBWatchValueMI(cb::shared_ptr<dbg_mi::Watch> watch, dbg_mi::ResultValue const *val_node)
// ----------------------------------------------------------------------------
{
    if (!watch)
        return false;

    watch->MarkChildsAsRemoved();

    if (!val_node)
    {
        watch->SetValue(wxT("<No data>"));
        return true;
    }

    // Safely extract the raw string representation depending on the node variant type
    wxString raw_layout = (val_node->GetType() == dbg_mi::ResultValue::Simple)
                          ? val_node->GetSimpleValue()
                          : val_node->MakeDebugString();
    watch->SetDebugValue(raw_layout);

    if (val_node->GetType() == dbg_mi::ResultValue::Simple)
    {
        // --------------------------------------------------------------------
        // BRANCH A: NATIVE SIMPLE LEAF NODES (GDB treats them as raw text)
        // --------------------------------------------------------------------

        // 1. Handle raw wxString/std::string masquerading as a Simple type
        if (raw_layout.Contains(wxT("_M_string_length")) || raw_layout.Contains(wxT("_M_p =")))
        {
            size_t content_start = raw_layout.find(wxT("\""));
            if (content_start != wxString::npos)
            {
                content_start += 1;

                size_t content_end = raw_layout.find(wxT("\\\"}"), content_start);
                if (content_end == wxString::npos)
                    content_end = raw_layout.find(wxT("\"},"), content_start);
                if (content_end == wxString::npos)
                    content_end = raw_layout.find_last_of(wxT("\""));

                if (content_end != wxString::npos && content_end > content_start)
                {
                    wxString extracted_text = raw_layout.Mid(content_start, content_end - content_start);
                    extracted_text.Replace(wxT("\\\""), wxT("\""));
                    extracted_text.Replace(wxT("\\\\"), wxT("\\"));

                    watch->SetValue(extracted_text);

                    // Strings are leaves but we can explicitly force a child row if desired
                    if (raw_layout.Contains(wxT("_M_string_length =")))
                    {
                        size_t len_pos = raw_layout.find(wxT("_M_string_length ="));
                        if (len_pos != wxString::npos)
                        {
                            wxString len_sub = raw_layout.Mid(len_pos + 18);
                            size_t comma_pos = len_sub.find(wxT(","));
                            if (comma_pos != wxString::npos)
                                len_sub = len_sub.Left(comma_pos);

                            cb::shared_ptr<dbg_mi::Watch> ch_len = AddChild(watch, wxT("_M_string_length"));
                            ch_len->SetValue(len_sub.Trim(true).Trim(false));
                            ch_len->SetDebugValue(len_sub);
                        }
                    }
                    return true;
                }
            }
        }
        // 2. Handle std::vector, wxArray, and custom buffer allocations
        else if (raw_layout.Contains(wxT("_M_start")) ||
                 raw_layout.Contains(wxT("_M_finish")) ||
                 raw_layout.Contains(wxT("wxBaseArray"))) // <-- Added lookahead for wxArrayInt
        {
            watch->SetValue(wxT("{...}"));

            // Tell Code::Blocks UI to show the [+] expander icon box!
            // Note: Check the dbg_mi::Watch header file. If SetHasChildren doesn't exist,
            // look for watch->SetType(Watch::TypeContainer) or watch->SetExpandable(true);
            watch->SetArray(true); //show the expansion box
            return true;
        }

        // Standard plain primitive literal data handler (int, float, pointers)
        watch->SetValue(raw_layout);
        return true;
    }
    else
    {
        // --------------------------------------------------------------------
        // BRANCH B: NATIVE TUPLE / ARRAY CONTAINERS
        // --------------------------------------------------------------------
        int child_count = val_node->GetTupleSize();
        watch->SetValue(wxString::Format(wxT("{...} (%d elements)"), child_count));

        for (int i = 0; i < child_count; ++i)
        {
            dbg_mi::ResultValue const *mi_child = val_node->GetTupleValueByIndex(i);
            if (mi_child)
            {
                wxString child_name = mi_child->GetName();
                if (child_name.empty())
                {
                    child_name = (val_node->GetType() == dbg_mi::ResultValue::Array)
                                 ? wxString::Format(wxT("[%d]"), i)
                                 : wxString::Format(wxT("child_%d"), i);
                }

                cb::shared_ptr<dbg_mi::Watch> ui_child = AddChild(watch, child_name);
                ParseGDBWatchValueMI(ui_child, mi_child);
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
GDBLocalVariable::GDBLocalVariable(wxString const &nameValue, size_t start, size_t length)
// ----------------------------------------------------------------------------
{
    for (size_t ii = 0; ii < length; ++ii)
    {
        if (nameValue[start + ii] == wxT('='))
        {
            name = nameValue.substr(start, ii);
            name.Trim();
            value = nameValue.substr(start + ii + 1, length - ii - 1);
            value.Trim(false);
            error = false;
            return;
        }
    }
    error = true;
}

// ----------------------------------------------------------------------------
void TokenizeGDBLocals(std::vector<GDBLocalVariable> &results, wxString const &value)
// ----------------------------------------------------------------------------
{
    size_t count = value.length();
    size_t start = 0;
    int curlyBraces = 0;
    bool inString = false, inChar = false;
    bool escaped = false;

    for (size_t ii = 0; ii < count; ++ii)
    {
        wxChar ch = value[ii];
        switch (ch)
        {
        case wxT('\n'):
            if (!inString && !inChar && curlyBraces == 0)
            {
                results.push_back(GDBLocalVariable(value, start, ii - start));
                start = ii + 1;
            }
            break;
        case wxT('{'):
            if (!inString && !inChar)
                curlyBraces++;
            break;
        case wxT('}'):
            if (!inString && !inChar)
                curlyBraces--;
            break;
        case wxT('"'):
            if (!inChar && !escaped)
                inString = !inString;
            break;
        case wxT('\''):
            if (!inString && !escaped)
                inChar = !inChar;
            break;
        default:
            break;
        }

        escaped = (ch == wxT('\\') && !escaped);
    }
    results.push_back(GDBLocalVariable(value, start, value.length() - start));
}

const wxRegEx reExamineMemoryLine(wxT("[[:blank:]]*(0x[0-9a-f]+)[[:blank:]]<.+>:[[:blank:]]+(.+)"));

// ----------------------------------------------------------------------------
bool ParseGDBExamineMemoryLine(wxString &resultAddr, std::vector<uint8_t> &resultValues,
                               const wxString &outputLine)
// ----------------------------------------------------------------------------
{
    // output is a series of:
    //
    // 0x22ffc0:       0xf0    0xff    0x22    0x00    0x4f    0x6d    0x81    0x7c
    // or
    // 0x85267a0 <RS485TxTask::taskProc()::rcptBuf>:   0x00   0x00   0x00   0x00   0x00   0x00   0x00   0x00
    // or
    // > x /108xb 0xa0000000
    // Cannot access memory at address 0xa0000000
    // 0xa0000000:

    /// TODO: Detect errors like cannot access memory at address XXX!!!!
    resultValues.clear();
    resultAddr.clear();

    if (outputLine.StartsWith(wxT("Cannot access memory at address ")))
        return false;

    wxString memory;
    if (reExamineMemoryLine.Matches(outputLine))
    {
        resultAddr = reExamineMemoryLine.GetMatch(outputLine, 1);
        memory = reExamineMemoryLine.GetMatch(outputLine, 2);
    }
    else
    {
        if (outputLine.First(wxT(':')) == -1)
        {
            return false;
        }
        resultAddr = outputLine.BeforeFirst(wxT(':'));
        memory = outputLine.AfterFirst(wxT(':'));
    }

    size_t pos = memory.find(wxT('x'));
    wxString hexbyte;
    while (pos != wxString::npos)
    {
        hexbyte.clear();
        hexbyte << memory[pos + 1];
        hexbyte << memory[pos + 2];
        unsigned long value;
        hexbyte.ToULong(&value,16);
        resultValues.push_back(static_cast<uint8_t>(value));
        pos = memory.find(wxT('x'), pos + 1); // skip current 'x'
    }

    return true;
}
