/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef ParseManagerBASE_H
#define ParseManagerBASE_H

#include "wx/defs.h" //necessary to avoid error 'unknown type name 'WXDLLIMPEXP_BASE' from wxcrt.h
#include <wx/wxcrt.h> // wxIsalnum

#include <map>
#include <queue>
#include <chrono>

#include "parser/token.h"
#include "parser/tokentree.h"

/** debug only variable, used to print the AI match related log message */
extern bool s_DebugSmartSense;

// ----------------------------------------------------------------------------
class ParseManagerBase
// ----------------------------------------------------------------------------
{
public:
    /** divide a statement to several pieces(parser component), each component has a type member */
    enum ParserTokenType
    {
        pttUndefined = 0,
        pttSearchText,
        pttClass,
        pttNamespace,
        pttFunction
    };

    /** the delimiter separating two Parser Component, See ParserComponent struct for more details */
    enum OperatorType
    {
        otOperatorUndefined = 0,
        otOperatorSquare,
        otOperatorParentheses,
        otOperatorPointer,
        otOperatorStar
    };

    /** @brief a long statement can be divided to a ParserComponent chain.
     *
     * e.g. for a statement like below:
     * @code
     * Ogre::Root::getSingleton().|
     * @endcode
     *
     *  a chains of four ParserComponents will be generated and list below:
     * @code
     *  Ogre             [pttNamespace]
     *  Root             [pttClass]
     *  getSingleton     [pttFunction]
     *  (empty space)    [pttSearchText]
     * @endcode
     *
     * a ParserComponent is mainly an identifier, which we use this identifier name to query on the
     * Symbol tree, and try to get the matched symbols.
     * For this example, we first try to search the Symbol tree by the keyword "Ogre", this will
     * match a symbol which has type "Namespace".
     * The next step, we search the "Root" under the previous returned Symbols.
     * In some special cases, such as "array[7]->b", we gives such result
     * @code
     *  array            [pttNamespace] (variable name?)
     *  []               [pttFunction, which is operator[]]
     *  b                [pttSearchText]
     * @endcode
     */
    struct ParserComponent
    {
        wxString        component;          /// name
        ParserTokenType tokenType;          /// type
        OperatorType    tokenOperatorType;  /// operator type

        ParserComponent() { Clear(); }
        void Clear()
        {
            component         = wxEmptyString;
            tokenType         = pttUndefined;
            tokenOperatorType = otOperatorUndefined;
        }
    };

    /** Constructor */
    ParseManagerBase();

    /** Destructor */
    virtual ~ParseManagerBase();

protected:

    /** Init cc search member variables */
    void Reset();

    /** Remove the last function's children, when doing codecompletion in a function body, the
     *  function body up to the caret position was parsed, and the local variables defined in
     *  the function were recorded as the function's children.
     *  Note that these tokens are marked as temporary tokens, so if the edit caret moves to another
     *  function body, these temporary tokens should be removed.
     */
    void RemoveLastFunctionChildren(TokenTree* tree, int& lastFuncTokenIdx);

    /** Generate the matching results under the Parent Token index set
     *
     * All functions that call this recursive function, should already have entered a critical section.
     *
     * @param tree TokenTree pointer
     * @param target Scope (defined in TokenIdxSet)
     * @param result result token index set
     * @param isPrefix whether a full match is used or only do a prefix match
     * @param kindMask define the result tokens filter, such as only class type is OK
     * @return result token set number
     */
    size_t GenerateResultSet(TokenTree*      tree,
                             const wxString& target,
                             int             parentIdx,
                             TokenIdxSet&    result,
                             bool            caseSens = true,
                             bool            isPrefix = false,
                             short int       kindMask = 0xFFFF);

    /** Test if token with this id depends on allocator class.
     * Currently, this function only identifies STL containers dependent on allocator.
     *
     * All functions that call this recursive function, should already entered a critical section.
     *
     * @param tree TokenTree pointer
     * @param id token idx
     */
    bool DependsOnAllocator(TokenTree*    tree,
                            const int&    id);

    /** used to get the correct token index in current line, e.g.
     * @code
     * class A
     * {
     *    void test()
     *    {               // start of the function body
     *       |
     *    };              // end of the function body
     * };
     * @endcode
     * @param tokens all current file's function and class, which cover the current line
     * @param curLine the line of the current caret position
     * @param file editor file name
     */
    int GetTokenFromCurrentLine(TokenTree*         tree,
                                const TokenIdxSet& tokens,
                                size_t             curLine,
                                const wxString&    file);

    /** For ComputeCallTip()
     * No critical section needed in this recursive function!
     * All functions that call this recursive function, should already entered a critical section. */
    bool PrettyPrintToken(TokenTree*   tree,
                          const Token* token,
                          wxString&    result,
                          bool         isRoot = true);

    // convenient static funcs for fast access and improved readability

    // count commas in lineText (nesting parentheses)
    static int CountCommas(const wxString& lineText, int start)
    {
        int commas = 0;
        int nest = 0;
        while (true)
        {
            wxChar c = lineText.GetChar(start++);
            if (c == '\0')
                break;
            else if (c == '(')
                ++nest;
            else if (c == ')')
                --nest;
            else if (c == ',' && nest == 1)
                ++commas;
        }
        return commas;
    }

    /** check whether the line[startAt] point to the identifier
     * @code
     *  SomeMethod(arg1, arg2)->Method2()
     *  ^^^^^^^^^^ those index will return true
     * @endcode
     */
    static bool InsideToken(int startAt, const wxString& line)
    {
        return (   (startAt >= 0)
                && ((size_t)startAt < line.Len())
                && (   (wxIsalnum(line.GetChar(startAt)))
                    || (line.GetChar(startAt) == '_') ) );
    }

    /** go to the first character of the identifier, e.g
     * @code
     * "    f(SomeNameSpace::SomeClass.SomeMethod"
     *                    return value^         ^begin
     * @endcode
     *   this is the index before the first character of the identifier
     */
    static int BeginOfToken(int startAt, const wxString& line)
    {
        while (   (startAt >= 0)
               && ((size_t)startAt < line.Len())
               && (   (wxIsalnum(line.GetChar(startAt)))
                   || (line.GetChar(startAt) == '_') ) )
            --startAt;
        return startAt;
    }
    static int BeforeToken(int startAt, const wxString& line)
    {
        if (   (startAt > 0)
            && ((size_t)startAt < line.Len() + 1)
            && (   (wxIsalnum(line.GetChar(startAt - 1)))
                || (line.GetChar(startAt - 1) == '_') ) )
            --startAt;
        return startAt;
    }

    /** check startAt is at some character like:
     * @code
     *  SomeNameSpace::SomeClass
     *                ^ here is a double colon
     *  SomeObject->SomeMethod
     *             ^ here is a pointer member access operator
     * @endcode
     */
    static bool IsOperatorEnd(int startAt, const wxString& line)
    {
        return (   (startAt > 0)
                && ((size_t)startAt < line.Len())
                && (   (   (line.GetChar(startAt) == '>')
                        && (line.GetChar(startAt - 1) == '-') )
                    || (   (line.GetChar(startAt) == ':')
                        && (line.GetChar(startAt - 1) == ':') ) ) );
    }
    static bool IsOperatorPointer(int startAt, const wxString& line)
    {
        return (   (startAt > 0)
            && ((size_t)startAt < line.Len())
            && (   (   (line.GetChar(startAt) == '>')
                    && (line.GetChar(startAt - 1) == '-') )));
    }

    /** check if startAt point to "->" or "::" operator */
     // FIXME (ollydbg#1#): should be startAt+1 < line.Len()?
    static bool IsOperatorBegin(int startAt, const wxString& line)
    {
        return (   (startAt >= 0)
                && ((size_t)startAt < line.Len())
                && (   (   (line.GetChar(startAt ) == '-')
                        && (line.GetChar(startAt + 1) == '>') )
                    || (   (line.GetChar(startAt) == ':')
                        && (line.GetChar(startAt + 1) == ':') ) ) );
    }

    /** check whether line[startAt] is a dot character */
    static bool IsOperatorDot(int startAt, const wxString& line)
    {
        return (   (startAt >= 0)
                && ((size_t)startAt < line.Len())
                && (line.GetChar(startAt) == '.') );
    }

    /** move to the char before whitespace and tabs, e.g.
     * @code
     *  SomeNameSpace       ::  SomeClass
     *              ^end   ^begin
     * note if there some spaces in the beginning like below
     *      "       f::"
     *     ^end    ^begin
     * @endcode
     * the returned index is -1.
     * @return the cursor after the operation
     */
    static int BeforeWhitespace(int startAt, const wxString& line)
    {
        while (   (startAt >= 0)
               && ((size_t)startAt < line.Len())
               && (   (line.GetChar(startAt) == ' ')
                   || (line.GetChar(startAt) == '\t') ) )
            --startAt;
        return startAt;
    }

    /** search from left to right, move the cursor to the first character after the space
     * @code
     *  "       ::   f"
     *   ^begin ^end
     * @endcode
     * @param[in] startAt the begin of the cursor
     * @param[in] line the string buffer
     * @return the location of the cursor
     */
    static int AfterWhitespace(int startAt, const wxString& line)
    {
        if (startAt < 0)
            startAt = 0;
        while (   ((size_t)startAt < line.Len())
               && (   (line.GetChar(startAt) == ' ')
                   || (line.GetChar(startAt) == '\t') ) )
            ++startAt;
        return startAt;
    }

    /** Test whether the current character is a '(' or '['
     * @param startAt the current cursor on the buffer
     * @return true if test is OK
     */
    static bool IsOpeningBracket(int startAt, const wxString& line)
    {
        return (   ((size_t)startAt < line.Len())
                && (   (line.GetChar(startAt) == '(')
                    || (line.GetChar(startAt) == '[') ) );
    }

    /** check the current char (line[startAt]) is either ')' or ']'  */
    static bool IsClosingBracket(int startAt, const wxString& line)
    {
        return (   (startAt >= 0)
                && (   (line.GetChar(startAt) == ')')
                    || (line.GetChar(startAt) == ']') ) );
    }

protected:

private:
    // Helper utilities called only by GenerateResultSet!
    // No critical section needed! All functions that call these functions,
    // should already entered a critical section.

    /** @brief collect child tokens of the specified token, the specified token must be unnamed.
     *
     * used for GenerateResultSet() function
     * @param tree TokenTree pointer
     * @param parent we need to collect the children of this token
     * @param result collected tokens
     * @return bool true if parent is an unnamed class or enum
     */
    bool AddChildrenOfUnnamed(TokenTree* tree, const Token* parent, TokenIdxSet& result)
    {
        if (  ( (parent->m_TokenKind & (tkClass | tkEnum)) != 0 )
            && parent->m_IsAnonymous == true )
        {
            // add all its children
            for (TokenIdxSet::const_iterator it = parent->m_Children.begin();
                                             it != parent->m_Children.end(); ++it)
            {
                Token* tokenChild = tree->at(*it);
                if (    tokenChild
                    && (parent->m_TokenKind == tkClass || tokenChild->m_Scope != tsPrivate) )
                {
                    // NOTE: recurse (eg: class A contains struct contains union or enum)
                    if ( !AddChildrenOfUnnamed(tree, tokenChild, result) )
                    {
                        result.insert(*it);
                        AddChildrenOfEnum(tree, tokenChild, result);
                    }
                }
            }
            return true;
        }
        return false;
    }

    bool AddChildrenOfEnum(TokenTree* tree, const Token* parent, TokenIdxSet& result)
    {
        if (parent->m_TokenKind == tkEnum)
        {
            // add all its children
            for (TokenIdxSet::const_iterator it = parent->m_Children.begin();
                                             it != parent->m_Children.end(); ++it)
            {
                Token* tokenChild = tree->at(*it);
                if (tokenChild && tokenChild->m_Scope != tsPrivate)
                    result.insert(*it);
            }

            return true;
        }
        return false;
    }

    /** @brief check to see if the token is an unnamed class or enum under the parent token
     *
     * This function will internally recursive call itself.
     * @param tree pointer to the TokenTree
     * @param targetIdx the checked token index
     * @param parentIdx the search scope
     * @return bool true if success
     */
    bool IsChildOfUnnamedOrEnum(TokenTree* tree, const int targetIdx, const int parentIdx)
    {
        if (targetIdx == parentIdx)
            return true;
        if (parentIdx == -1)
            return false;

        Token* parent = tree->at(parentIdx);
        if (parent && (parent->m_TokenKind & tkClass))
        {
            // loop all children of the parent token
            for (TokenIdxSet::const_iterator it = parent->m_Children.begin();
                                             it != parent->m_Children.end(); ++it)
            {
                Token* token = tree->at(*it);
                // an unnamed class is much similar like the enum
                if (token && (((token->m_TokenKind & tkClass)
                                && (token->m_IsAnonymous == true))
                             || (token->m_TokenKind & tkEnum)))
                {
                    // if target token matches on child, we can return success
                    // other wise, we try to see the target token matches child's child.
                    if ((targetIdx == (*it)) || IsChildOfUnnamedOrEnum(tree, targetIdx, (*it)))
                        return true;
                }
            }
        }
        return false;
    }

    // for GenerateResultSet()
    bool MatchText(const wxString& text, const wxString& target, bool caseSens, bool isPrefix)
    {
        if (isPrefix && target.IsEmpty())
            return true;
        if (!isPrefix)
            return text.CompareTo(target.wx_str(), caseSens ? wxString::exact : wxString::ignoreCase) == 0;
        // isPrefix == true
        if (caseSens)
            return text.StartsWith(target);
        return text.Upper().StartsWith(target.Upper());
    }

    // for GenerateResultSet()
    bool MatchType(TokenKind kind, short int kindMask)
    {
        return kind & kindMask;
    }

private:
    ParserComponent              m_LastComponent;
    std::map<wxString, wxString> m_TemplateMap;
};

#endif // ParseManagerBASE_H
