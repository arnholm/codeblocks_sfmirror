#include "cmd_result_parser.h"

#include "globals.h"
#include "cmd_result_tokens.h"

namespace dbg_mi
{

void find_and_replace(wxString &source, const wxString &find, wxString const &replace)
{
    for (size_t j; (j = source.find(find)) != wxString::npos; )
    {
        source.replace(j, find.length(), replace);
    }
}
bool StripEnclosingQuotes(wxString &str)
{
    if(str.length() == 0)
        return true;
    if(str[0] == _T('"') && str[str.length() - 1] == _T('"'))
    {
        if(str.length() >= 2 && str[str.length() - 2] == _T('\\'))
            return false;
        str = str.substr(1, str.length() - 2);
    }
    else if(str[0] == _T('"'))
        return false;
    else if(str[str.length() - 1] == _T('"'))
    {
        if(str.length() >= 2 && str[str.length() - 2] != _T('\\'))
            return false;
    }

    find_and_replace(str, _T("\\\""), _T("\""));
    return true;
}

bool ParseTuple(wxString const &str, int &start, ResultValue &tuple, bool want_closing_brace)
{
    Token token;
    int pos = start;
    ResultValue *curr_value = NULL;

    enum Step
    {
        Nothing,
        Name,
        Equal,
        Value
    };
    Step step = Nothing;

    while(pos < static_cast<int>(str.length()))
    {
        if(not GetNextToken(str, pos, token))
            return false;

        switch(token.type)
        {
        case Token::String:
            // set the value
            if(curr_value)
            {
                if(step != Equal)
                    return false;
                step = Value;

                curr_value->SetType(ResultValue::Simple);
                wxString value = token.ExtractString(str);

                if(not StripEnclosingQuotes(value))
                    return false;
                curr_value->SetSimpleValue(value);
            }
            else
            {
                if(step != Nothing)
                    return false;
                step = Name;

                curr_value = new ResultValue;
                curr_value->SetName(token.ExtractString(str));
            }
            break;

        case Token::Equal:
            if(not curr_value)
                return false;
            if(step != Name)
            {
                delete curr_value;
                return false;
            }
            step = Equal;
            break;

        case Token::Comma:
            if(not curr_value)
                return false;
            if(tuple.GetType() == ResultValue::Array)
            {
                if(step == Name)
                {
                    curr_value->SetType(ResultValue::Simple);
                    curr_value->SetSimpleValue(curr_value->GetName());
                    curr_value->SetName(_T(""));
                }
                else if(step != Value)
                {
                    delete curr_value;
                    return false;
                }
            }
            else
            {
                if(step != Value)
                {
                    delete curr_value;
                    return false;
                }
            }
            tuple.SetTupleValue(curr_value);
            curr_value = NULL;
            step = Nothing;
            break;

        case Token::TupleStart:
            if(not curr_value)
            {
                if(tuple.GetType() != ResultValue::Array)
                    return false;
                else
                {
                    curr_value = new ResultValue;
                    step = Equal;
                }
            }
            if(step != Equal)
            {
                delete curr_value;
                return false;
            }

            curr_value->SetType(ResultValue::Tuple);
            pos = token.end;
            if(not ParseTuple(str, pos, *curr_value, true))
            {
                delete curr_value;
                return false;
            }
            else
            {
                token.end = pos;
                token.type = Token::TupleEnd;
                step = Value;
            }
            break;

        case Token::ListStart:
            if(not curr_value)
                return false;
            if(step != Equal)
            {
                delete curr_value;
                return false;
            }
            curr_value->SetType(ResultValue::Array);
            pos = token.end;
            if(not ParseTuple(str, pos, *curr_value, true))
            {
                delete curr_value;
                return false;
            }
            else
            {
                token.end = pos;
                token.type = Token::ListEnd;
                step = Value;
            }
            break;

        case Token::TupleEnd:
            if(not curr_value)
                return false;
            if(tuple.GetType() != ResultValue::Tuple || !want_closing_brace)
            {
                delete curr_value;
                return false;
            }
            if(step != Value)
            {
                delete curr_value;
                return false;
            }
            start = pos + 1;
            tuple.SetTupleValue(curr_value);
            return true;

        case Token::ListEnd:
//            if(not curr_value)
//                return false;
            if(tuple.GetType() != ResultValue::Array || !want_closing_brace)
            {
                delete curr_value;
                return false;
            }

            if(step == Name)
            {
                curr_value->SetType(ResultValue::Simple);
                curr_value->SetSimpleValue(curr_value->GetName());
                curr_value->SetName(_T(""));
                tuple.SetTupleValue(curr_value);
            }
            else if(step == Value)
            {
                tuple.SetTupleValue(curr_value);
            }
            else if(step != Nothing || curr_value)
            {
                delete curr_value;
                return false;
            }

            start = pos + 1;
            return true;
        default:
            assert(false);
        }

        pos = token.end;
    }

    if(curr_value)
    {
        if(step != Value)
        {
            delete curr_value;
            return false;
        }
        else
            tuple.SetTupleValue(curr_value);
    }
    if(token.type == Token::Comma || token.type == Token::ListStart || token.type == Token::TupleStart)
        return false;

    // if we are here the closing brace was not found, so we exit with error
    if(want_closing_brace)
        return false;

    start = pos;
    return true;
}


bool ParseValue(wxString const &str, ResultValue &results, int start)
{
    results.SetType(ResultValue::Tuple);
    return ParseTuple(str, start, results, false);
}

void ResultValue::SetType(Type type)
{
    m_type = type;
}

void ResultValue::SetTupleValue(ResultValue *value)
{
    assert(value);
    m_value.tuple.push_back(value);
}

ResultValue const * ResultValue::GetTupleValue(wxString const &key) const
{
    if (m_type != Tuple) {
        wxString msg = wxString::Format("Assert: (m_type == Tuple)\n Check the logs and CallStack."
                       //? "\nIf this code is the debuggee, the var is probably not initialized yet"
                                    "\n %s:%d", __FUNCTION__, __LINE__);
        cbMessageBox(msg, "Assert");
    }
    //    //#include <intrin.h> // Required for __debugbreak() in MinGW // (ph 26/05/28)
    //    if (m_type != Tuple) {
    //        __debugbreak(); // MinGW will compile this down to an 'int 3' assembly instruction
    //    }
    //    assert(m_type == Tuple);

    wxString::size_type pos = key.find(_T('.'));
    if(pos != wxString::npos)
    {
        wxString const &subkey = key.substr(0, pos);
        Container::const_iterator it = FindTupleValue(subkey);
        if(it != m_value.tuple.end() && (*it)->GetType() == Tuple)
            return (*it)->GetTupleValue(key.substr(pos + 1, key.length() - pos - 1));
    }
    else
    {
        Container::const_iterator it = FindTupleValue(key);
        if(it != m_value.tuple.end())
            return *it;
    }
    return NULL;
}

ResultValue const* ResultValue::GetTupleValueByIndex(int index) const
{
    Container::const_iterator it = m_value.tuple.begin();

    std::advance(it, index);
    if(it != m_value.tuple.end())
        return *it;
    else
        return NULL;
}

wxString ResultValue::MakeDebugString() const
{
    switch(m_type)
    {
    case Simple:
        if(m_name.empty())
            return m_value.simple;
        else
            return m_name + _T("=") + m_value.simple;
        break;
    case Tuple:
        {
            wxString s;
            if(m_name.empty())
                s = _T("{");
            else
                s = m_name + _T("={");

            bool first = true;
            for(Container::const_iterator it = m_value.tuple.begin(); it != m_value.tuple.end(); ++it)
            {
                if(first)
                    first = false;
                else
                    s += _T(",");

                s += (*it)->MakeDebugString();
            }

            s += _T("}");
            return s;
        }
    case Array:
        {
            wxString s;
            if(m_name.empty())
                s = _T("[");
            else
                s = m_name + _T("=[");
            bool first = true;
            for(Container::const_iterator it = m_value.tuple.begin(); it != m_value.tuple.end(); ++it)
            {
                if(first)
                    first = false;
                else
                    s += _T(",");

                s += (*it)->MakeDebugString();
            }

            s += _T("]");
            return s;
        }
    default:
        return _T("not_initialized");
    }
}

bool ResultParser::Parse(wxString const &s)
{
    m_type = ParseType(s);
    m_class = ClassUnknown;
    m_parseError = false;
    wxString str = s.substr(1, s.length() - 1);
    if (str.length() == 0)
        return false;

    wxString::size_type after_class_index = 0;

    if (m_type == NotifyAsyncOutput)
    {
        after_class_index = str.find(_T(','));
        if (after_class_index == wxString::npos)
        {
            m_async_type = str;
            return true;
        }
        else
            m_async_type = str.substr(0, after_class_index);
    }
    else
    {
        if(str.StartsWith(_T("done")))
        {
            m_class = ClassDone;
            after_class_index = 4;
        }
        else if(str.StartsWith(_T("stopped")))
        {
            m_class = ClassStopped;
            after_class_index = 7;
        }
        else if(str.StartsWith(_T("running")))
        {
            m_class = ClassRunning;
            after_class_index = 7;
        }
        else if(str.StartsWith(_T("connected")))
        {
            m_class = ClassConnected;
            after_class_index = 9;
        }
        else if(str.StartsWith(_T("error")))
        {
            m_class = ClassError;
            after_class_index = 5;
        }
        else if(str.StartsWith(_T("exit")))
        {
            m_class = ClassExit;
            after_class_index = 4;
        }
        else
            return false;
    }

    if(str[after_class_index] == _T(','))
    {   //-return ParseValue(str, m_value, after_class_index + 1);
        // Avoid MI stall if false is passed back
        if (not ParseValue(str, m_value, after_class_index + 1))
            m_parseError = true;                                    // malformed response
        return true;
    }
    else if(after_class_index != str.length())
    {   //-return false;        // avoid MI stall
        m_parseError = true;    // on malformed responses
        return true;
    }
    else
        return true;
}

ResultParser::Type ResultParser::ParseType(wxString const &str)
{
    if(str.empty())
        return TypeUnknown;

    switch(static_cast<int>(str[0]))
    {
    case _T('^'): // result record
        return ResultParser::Result;
    case _T('*'):
        return ResultParser::ExecAsyncOutput;

    case _T('+'):
        return ResultParser::StatusAsyncOutput;

    case _T('='):
        return ResultParser::NotifyAsyncOutput;
    default:
        return TypeUnknown;
    }
}

wxString ResultParser::MakeDebugString() const
{
    wxString s(_T("type: "));
    switch(m_type)
    {
    case Result:
        s += _T("result");
        break;
    case ExecAsyncOutput:
        s += _T("exec-async-ouput");
        break;
    case StatusAsyncOutput:
        s += _T("status-async-ouput");
        break;
    case NotifyAsyncOutput:
        s += _T("notify-async-ouput");
        break;
    default:
        s += _T("unknown");
    }
    s += _T("\nclass: ");
    switch(m_class)
    {
    case ClassDone:
        s += _T("done");
        break;
    case ClassRunning:
        s += _T("running");
        break;
    case ClassConnected:
        s += _T("connected");
        break;
    case ClassError:
        s += _T("error");
        break;
    case ClassExit:
        s += _T("exit");
        break;
    case ClassStopped:
        s += _T("stopped");
        break;
    default:
        s += _T("unknown");
    }

    s += _T("\nresults:\n");
    s += m_value.MakeDebugString();


    return s;
}

} // namespace dbg_mi
