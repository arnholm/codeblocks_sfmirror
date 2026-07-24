#include "cmd_result_tokens.h"

namespace dbg_mi
{


bool GetNextToken(wxString const &str, int pos, Token &token)
{
    while(pos < static_cast<int>(str.length()) && (str[pos] == _T(' ') || str[pos] == _T('\t')))
        ++pos;

    if(pos >= static_cast<int>(str.length()))
        return false;

    token.start = -1;
    bool in_quote = false;

    switch(static_cast<int>(str[pos]))
    {
    case _T('='):
        token = Token(pos, pos + 1, Token::Equal);
        return true;

    case _T(','):
        token = Token(pos, pos + 1, Token::Comma);
        return true;
    case _T('['):
        token = Token(pos, pos + 1, Token::ListStart);
        return true;
    case _T(']'):
        token = Token(pos, pos + 1, Token::ListEnd);
        return true;
    case _T('{'):
        token = Token(pos, pos + 1, Token::TupleStart);
        return true;
    case _T('}'):
        token = Token(pos, pos + 1, Token::TupleEnd);
        return true;

    case _T('"'):
        in_quote = true;
        /* FALLTHRU */
    default:
        token.type = Token::String;
        token.start = pos;
    }
    ++pos;

    bool escape_next = false;
    while(pos < static_cast<int>(str.length()))
    {
        if((str[pos] == _T(' ') || str[pos] == _T('\t') || str[pos] == _T(',')
            || str[pos] == _T('=') || str[pos] == _T('{') || str[pos] == _T('}')
            || str[pos] == _T('[') || str[pos] == _T(']')) && !in_quote)
        {
            token.end = pos;
            return true;
        }
        else if(str[pos] == _T('"') && in_quote && !escape_next)
        {
            token.end = pos + 1;
            return true;
        }
        else if(str[pos] == _T('\\'))
        {
            escape_next = true;
        }
        else
        {
            escape_next = false;
        }


        ++pos;
    }

    if(in_quote)
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


} // namespace dbg_mi

