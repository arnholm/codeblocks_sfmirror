/***************************************************************
 * Name:      TextFileSearcherText
 * Purpose:   TextFileSearcherText implements the TextFileSearcher
 *            interface is used to search text files for a text
 *            expression ( != regular expression).
 * Author:    Jerome ANTOINE
 * Created:   2007-04-07
 * Copyright: Jerome ANTOINE
 * License:   GPL
 **************************************************************/

#ifndef TEXT_FILE_SEARCHER_TEXT_H
#define TEXT_FILE_SEARCHER_TEXT_H

#include <wx/string.h>

#include "TextFileSearcher.h"

class TextFileSearcherText : public TextFileSearcher
{
public:
    /** Constructor TextFileSearcherText
     * \brief We don't use ThreadSearchFindData to limit coupling
      * @param searchText : text to search
      * @param matchCase : with character case
      * @param matchWordBegin : with begin word
      * @param matchWord : all words
      * @param matchInComments : in comments
	*/
    TextFileSearcherText(const wxString& searchText, bool matchCase, bool matchWordBegin,
                         bool matchWord, bool matchInComments);

    /** Return true if Line matches search text.
      * This method is inherited from TextFileSearcher and is used to implement
      * different search strategies. In TextFileSearcherText, the basic text search
      * is implemented.
      * @param outMatchedPositions : positions text matched
      * @param line : the text line to match.
      * @return true if line matches search text.
      */
    bool MatchLine(std::vector<int> *outMatchedPositions, const wxString &line) override;
};

#endif // TEXT_FILE_SEARCHER_TEXT_H
