/***************************************************************
 * Name:      TextFileSearcherRegEx
 * Purpose:   TextFileSearcherRegEx is used to search text files
 *            for regular expressions.
 * Author:    Jerome ANTOINE
 * Created:   2007-04-07
 * Copyright: Jerome ANTOINE
 * License:   GPL
 **************************************************************/

#include "sdk.h"
#ifndef CB_PRECOMP
    // Required extra includes
    #include <wx/intl.h>
    #include <wx/string.h>
#endif

#include "TextFileSearcherRegEx.h"


TextFileSearcherRegEx::TextFileSearcherRegEx(const wxString& searchText, bool matchCase,
        bool matchWordBegin, bool matchWord, bool matchInComments)
    : TextFileSearcher(searchText, matchCase, matchWordBegin, matchWord, matchInComments)
{
#ifdef wxHAS_REGEX_ADVANCED
    int flags = wxRE_ADVANCED;
#else
    int flags = wxRE_EXTENDED;
#endif
    if (matchCase == false)
    {
        flags |= wxRE_ICASE;
    }

    wxString pattern;
    if (matchWord == true)
    {
        pattern = _T("([^[:alnum:]_]|^)(") + searchText + _T(")([^[:alnum:]_]|$)");
        m_IndexToMatch = 2;
    }
    else if (matchWordBegin == true)
    {
        pattern = _T("([^[:alnum:]_]|^)(") + searchText + wxT(")");
        m_IndexToMatch = 2;
    }
    else
    {
        m_IndexToMatch = 0;
        pattern = searchText;
    }

    m_RegEx.Compile(pattern, flags);
}

bool TextFileSearcherRegEx::MatchLine(std::vector<int>* outMatchedPositions, const wxString& line)
{
    wxString lineCopy = line;
    if(!m_RegEx.IsValid())
        return false;
    // skip the C++ comments
    if (!m_MatchInComments)
    {
        lineCopy = lineCopy.Trim (false);
        if(lineCopy.Left(2) == "//")                // pure comment line
            return false;                           // simply skip the whole comment line
        // such as we have a line: int xxx; // xxx
        // we have to remove the C++ comments, so "// xxx" got removed
        if (lineCopy.Contains("//"))
        {
            // remove EOL comment
            if (lineCopy.Contains("\"") || lineCopy.Contains("'"))
            {
                // remove all "" or '' strings with space (preserve length)
                bool bInString1 = false;
                bool bInString2 = false;
                wxChar cChar;
                for (int i = 0; i < (int)lineCopy.Length(); i++)
                {
                    cChar = lineCopy.at(i);
                    if (cChar == '"'  && !bInString1)
                    {
                        lineCopy.at(i) = ' ';
                        bInString1 = true;
                        continue;
                    }
                    if (cChar == '"'  &&  bInString1)
                    {
                        lineCopy.at(i) = ' ';
                        bInString1 = false;
                        continue;
                    }
                    if (cChar == '\'' && !bInString2)
                    {
                        lineCopy.at(i) = ' ';
                        bInString2 = true;
                        continue;
                    }
                    if (cChar == '\'' &&  bInString2)
                    {
                        lineCopy.at(i) = ' ';
                        bInString2 = false;
                        continue;
                    }
                    if (bInString1 || bInString2)
                        lineCopy.at(i) = ' ';
                }
            }
            int nPos = lineCopy.Find("//");     // simple case first "//" will be the comment
            lineCopy = line.Left(nPos);
        }
    }
    const wxChar* lineBuffer = lineCopy.wx_str();
    const bool match = m_RegEx.Matches(lineBuffer, 0, lineCopy.length());
    if (!match)
        return false;

    const std::vector<int>::size_type countIdx = outMatchedPositions->size();
    outMatchedPositions->push_back(0);

    int count = 0;

    size_t start, length;

    // GetMatch returns the values relative to the start of the string, so for the second and later
    // matches we need to know the position of the string relative to the full string.
    int offset = 0;
    while (m_RegEx.GetMatch(&start, &length, m_IndexToMatch))
    {
        count++;
        outMatchedPositions->push_back(start + offset);
        outMatchedPositions->push_back(length);

        offset += start + length;

        if (!m_RegEx.Matches(lineBuffer + offset, 0, lineCopy.length() - offset))
            break;
    }

    (*outMatchedPositions)[countIdx] = count;
    return true;
}

bool TextFileSearcherRegEx::IsOk(wxString* pErrorMessage)
{
    bool ok = m_RegEx.IsValid();
    if ( !ok && pErrorMessage )
    {
        *pErrorMessage = _("Bad regular expression.");
    }
    return ok;
}
