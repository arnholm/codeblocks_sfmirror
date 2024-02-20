
#ifndef PARSEMANAGER_TEST_H
#define PARSEMANAGER_TEST_H

#include "parser_base.h"
#include "parsemanager_base.h"

class ParseManagerTest : public ParseManagerBase
{
public:
    ParseManagerTest();
    ~ParseManagerTest();

    bool TestExpression(wxString&          expression,
                        const TokenIdxSet& searchScope,
                        TokenIdxSet&       result);

    bool Parse(wxString& file, bool isLocalFile);

    void PrintList();

    wxString SerializeTree();

    void PrintTokenTree(Token* token);

    void PrintTree();

    /** clear the token tree */
    void Clear();

    /** set the include search paths and the macro replacement rules of the parser */
    void Init();

    /** parse and run test on the file
     * @param file this can be either a file name, which is a file name in hard disk or a file
     * contents.
     * @param isLocalFile true if is is a file name otherwise it is a file contents (buffer).
     */
    bool ParseAndCodeCompletion(wxString file, bool isLocalFile = true);

    ParserBase m_Parser;
};

#endif //PARSEMANAGER_TEST_H
