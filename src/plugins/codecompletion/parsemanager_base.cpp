/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#ifndef CB_PRECOMP
#endif

#include "parsemanager_base.h"
#include "parser/tokenizer.h"

#include "parser/cclogger.h"

#define CC_PARSEMANAGERBASE_DEBUG_OUTPUT 0

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_PARSEMANAGERBASE_DEBUG_OUTPUT
        #define CC_PARSEMANAGERBASE_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_PARSEMANAGERBASE_DEBUG_OUTPUT
        #define CC_PARSEMANAGERBASE_DEBUG_OUTPUT 2
    #endif
#endif

#ifdef CC_PARSER_TEST
    #define ADDTOKEN(format, args...) \
            CCLogger::Get()->AddToken(F(format, ##args))
    #define TRACE(format, args...) \
            CCLogger::Get()->DebugLog(F(format, ##args))
    #define TRACE2(format, args...) \
            CCLogger::Get()->DebugLog(F(format, ##args))
#else
    #if CC_PARSEMANAGERBASE_DEBUG_OUTPUT == 1
        #define ADDTOKEN(format, args...) \
                CCLogger::Get()->AddToken(F(format, ##args))
        #define TRACE(format, args...) \
            CCLogger::Get()->DebugLog(F(format, ##args))
        #define TRACE2(format, args...)
    #elif CC_PARSEMANAGERBASE_DEBUG_OUTPUT == 2
        #define ADDTOKEN(format, args...) \
                CCLogger::Get()->AddToken(F(format, ##args))
        #define TRACE(format, args...)                            \
            do                                                    \
            {                                                     \
                if (g_EnableDebugTrace)                           \
                    CCLogger::Get()->DebugLog(F(format, ##args)); \
            }                                                     \
            while (false)
        #define TRACE2(format, args...) \
            CCLogger::Get()->DebugLog(F(format, ##args))
    #else
        #define ADDTOKEN(format, args...)
        #define TRACE(format, args...)
        #define TRACE2(format, args...)
    #endif
#endif

ParseManagerBase::ParseManagerBase()
{
}

ParseManagerBase::~ParseManagerBase()
{
}

void ParseManagerBase::Reset()
{
    m_LastComponent.Clear();
}

// Here's the meat of code-completion :)
// This function decides most of what gets included in the auto-completion
// list presented to the user.
// It's called recursively for each component of the std::queue argument.
// for example: objA.objB.function()
// The queue is like: 'objA' 'objB' 'function'. We deal with objA first.
//
// No critical section needed in this recursive function!
// All functions that call this recursive function, should already entered a critical section.
size_t ParseManagerBase::FindAIMatches(TokenTree*                  tree,
                                       std::queue<ParserComponent> components,
                                       TokenIdxSet&                result,
                                       int                         parentTokenIdx,
                                       bool                        isPrefix,
                                       bool                        caseSensitive,
                                       bool                        use_inheritance,
                                       short int                   kindMask,
                                       TokenIdxSet*                search_scope)
{
    if (components.empty())
        return 0;

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(_T("FindAIMatches() ----- FindAIMatches - enter -----"));

    TRACE(_T("ParseManager::FindAIMatches()"));

    // pop top component
    ParserComponent parser_component = components.front();
    components.pop();

    // handle the special keyword "this".
    if ((parentTokenIdx != -1) && (parser_component.component == _T("this")))
    {
        // this will make the AI behave like it's the previous scope (or the current if no previous scope)

        // move on please, nothing to see here...
        // All functions that call the recursive FindAIMatches should already entered a critical section.
        return FindAIMatches(tree, components, result, parentTokenIdx,
                             isPrefix, caseSensitive, use_inheritance,
                             kindMask, search_scope);
    }

    // we 'll only add tokens in the result set if we get matches for the last token
    bool isLastComponent = components.empty();
    wxString searchtext = parser_component.component;

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Search for %s, isLast = %d",
                                                   searchtext, isLastComponent ? 1 : 0));

    // get a set of matches for the current token
    TokenIdxSet local_result;
    // All functions that call the recursive GenerateResultSet should already entered a critical section.
    GenerateResultSet(tree, searchtext, parentTokenIdx, local_result,
                      (caseSensitive || !isLastComponent),
                      (isLastComponent && !isPrefix), kindMask);

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Looping %zu results", local_result.size()));

    // loop all matches, and recurse
    for (TokenIdxSet::const_iterator it = local_result.begin(); it != local_result.end(); it++)
    {
        int id = *it;
        const Token* token = tree->at(id);

        // sanity check
        if (!token)
        {
            if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(_T("FindAIMatches() Token is NULL?!"));
            continue;
        }

        // ignore operators
        if (token->m_IsOperator)
            continue;

        // enums children (enumerators), are added by default
        if (token->m_TokenKind == tkEnum)
        {
            // insert enum type
            result.insert(id);

            // insert enumerators
            for (TokenIdxSet::const_iterator tis_it = token->m_Children.begin();
                 tis_it != token->m_Children.end();
                 tis_it++)
                result.insert(*tis_it);

            continue; // done with this token
        }

        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Match: '%s' (ID='%d') : type='%s'",
                                                       token->m_Name, id, token->m_BaseType));

        // is the token a function or variable (i.e. is not a type)
        if (    !searchtext.IsEmpty()
             && (parser_component.tokenType != pttSearchText)
             && !token->m_BaseType.IsEmpty() )
        {
            // the token is not a type
            // find its type's ID and use this as parent instead of (*it)
            TokenIdxSet type_result;
            std::queue<ParserComponent> type_components;
            wxString actual = token->m_BaseType;

            // TODO: ignore builtin types (void, int, etc)
            BreakUpComponents(actual, type_components);
            // the parent to search under is a bit troubling, because of namespaces
            // what we 'll do is search under current parent and traverse up the parentship
            // until we find a result, or reach -1...

            if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Looking for type: '%s' (%zu components)",
                                                           actual.wx_str(), type_components.size()));

            // search under all search-scope namespaces too
            TokenIdxSet temp_search_scope;
            if (search_scope)
                temp_search_scope = *search_scope;

            // add grand-parent as search scope (if none defined)
            // this helps with namespaces when the token's type doesn't contain
            // namespace info. In that case (with the code here) we 're searching in
            // the parent's namespace too
            if (parentTokenIdx != -1)
            {
                const Token* parentToken = tree->at(parentTokenIdx);
                if (parentToken)
                {
                    const Token* parent = tree->at(parentToken->m_ParentIndex);
                    if (parent)
                    {
                        temp_search_scope.insert(parent->m_Index);
                        if (g_DebugSmartSense)
                            CCLogger::Get()->DebugLog(_T("FindAIMatches() Implicit search scope added:") + parent->m_Name);
                    }
                }
            }

            TokenIdxSet::const_iterator itsearch;
            itsearch = temp_search_scope.begin();
            while (!search_scope || itsearch != temp_search_scope.end())
            {
                const Token* parent = tree->at(*itsearch);

                if (g_DebugSmartSense)
                    CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Now looking under '%s'",
                                                               parent ? parent->m_Name : wxString("Global namespace")));

                do
                {
                    // types are searched as whole words, case sensitive and only classes/namespaces
                    // All functions that call the recursive FindAIMatches should already entered a critical section.
                    if (FindAIMatches(tree,
                                      type_components,
                                      type_result,
                                      parent ? parent->m_Index : -1,
                                      true,
                                      false,
                                      false,
                                      tkClass | tkNamespace | tkTypedef | tkEnum,
                                      &temp_search_scope) != 0)
                        break;
                    if (!parent)
                        break;
                    parent = tree->at(parent->m_ParentIndex);
                } while (true);
                ++itsearch;
            }

            // we got all possible types (hopefully should be just one)
            if (!type_result.empty())
            {
                // this is the first result
                id = *(type_result.begin());
                if (type_result.size() > 1)
                {
                    // if we have more than one result, recurse for all of them
                    TokenIdxSet::const_iterator tis_it = type_result.begin();
                    ++tis_it;
                    while (tis_it != type_result.end())
                    {
                        std::queue<ParserComponent> lcomp = components;
                        // All functions that call the recursive FindAIMatches should already entered a critical section.
                        FindAIMatches(tree, lcomp, result, *tis_it, isPrefix,
                                      caseSensitive, use_inheritance,
                                      kindMask, search_scope);
                        ++tis_it;
                    }
                }

                if (g_DebugSmartSense)
                {
                    CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Type: '%s' (%d)", tree->at(id)->m_Name, id));
                    if (type_result.size() > 1)
                        CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() Multiple types matched for '%s': %zu results",
                                                                   token->m_BaseType, type_result.size()));
                }
            }
            else if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(wxString::Format("FindAIMatches() No types matched '%s'.", token->m_BaseType));
        }

        // if no more components, add to result set
        if (isLastComponent)
            result.insert(id);
        // else recurse this function using id as a parent
        else
            // All functions that call the recursive FindAIMatches should already entered a critical section.
            FindAIMatches(tree, components, result, id, isPrefix,
                          caseSensitive, use_inheritance, kindMask,
                          search_scope);
    }

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(_T("FindAIMatches() ----- FindAIMatches - leave -----"));

    return result.size();
}

void ParseManagerBase::FindCurrentFunctionScope(TokenTree*        tree,
                                                const TokenIdxSet& procResult,
                                                TokenIdxSet&       scopeResult)
{
    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // loop on the input parameter procResult, and only record some container tokens, such as
    // class token, function token.
    for (TokenIdxSet::const_iterator it = procResult.begin(); it != procResult.end(); ++it)
    {
        const Token* token = tree->at(*it);
        if (!token)
            continue;

        if (token->m_TokenKind == tkClass)
            scopeResult.insert(*it);
        else
        {
            if (token->m_TokenKind & tkAnyFunction && token->HasChildren()) // for local variable
                scopeResult.insert(*it);
            scopeResult.insert(token->m_ParentIndex);
        }

        if (g_DebugSmartSense)
        {
            const Token* parent = tree->at(token->m_ParentIndex);
            CCLogger::Get()->DebugLog(_T("AI() Adding search namespace: ") +
                                      (parent ? parent->m_Name : _T("Global namespace")));
        }
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
}

void ParseManagerBase::CleanupSearchScope(TokenTree*   tree,
                                          TokenIdxSet* searchScope)
{
    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // remove all the container tokens in the token index set
    for (TokenIdxSet::const_iterator it = searchScope->begin(); it != searchScope->end();)
    {
        const Token* token = tree->at(*it);
        if (!token || !(token->m_TokenKind & (tkNamespace | tkClass | tkTypedef | tkAnyFunction)))
            searchScope->erase(it++);
        else
            ++it;
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    // ...but always search the global scope.
    searchScope->insert(-1);
}

// Set start and end for the calltip highlight region.
void ParseManagerBase::GetCallTipHighlight(const wxString& calltip,
                                           int*            start,
                                           int*            end,
                                           int             typedCommas)
{
    TRACE(_T("ParseManagerBase::GetCallTipHighlight()"));

    int pos = 0;
    int paramsCloseBracket = calltip.length() - 1;
    int nest = 0;
    int commas = 0;
    *start = FindFunctionOpenParenthesis(calltip) + 1;
    *end = 0;

    // for a function call tip string like below
    // void f(int a, map<int, float>b)
    // we have to take care the <> pair
    while (true)
    {
        wxChar c = calltip.GetChar(pos++);
        if (c == '\0')
            break;
        else if (c == '(')
            ++nest;
        else if (c == ')')
        {
            --nest;
            if (nest == 0)
                paramsCloseBracket = pos - 1;
        }
        else if (c == ',' && nest == 1)
        {
            ++commas;
            if (commas == typedCommas + 1)
            {
                *end = pos - 1;
                return;
            }
            *start = pos;
        }
        else if (c =='<')
            ++nest;
        else if (c == '>')
            --nest;
    }
    if (*end == 0)
        *end = paramsCloseBracket;
}

int ParseManagerBase::FindFunctionOpenParenthesis(const wxString& calltip)
{
    int nest = 0;
    for (size_t i = calltip.length(); i > 0; --i)
    {
        wxChar c = calltip[i - 1];
        if (c == wxT('('))
        {
            --nest;
            if (nest == 0)
            return i - 1;
        }
        else if (c == wxT(')'))
            ++nest;
    }
    return -1;
}

wxString ParseManagerBase::GetCCToken(wxString&        line,
                                      ParserTokenType& tokenType,
                                      OperatorType&    tokenOperatorType)
{
    tokenType         = pttSearchText;
    tokenOperatorType = otOperatorUndefined;
    if (line.IsEmpty())
        return wxEmptyString;

    tokenOperatorType = otOperatorUndefined;
    unsigned int startAt = FindCCTokenStart(line);
    wxString res = GetNextCCToken(line, startAt, tokenOperatorType);

    TRACE(_T("GetCCToken() : FindCCTokenStart returned %u \"%s\""), startAt, line.wx_str());
    TRACE(_T("GetCCToken() : GetNextCCToken returned %u \"%s\""), startAt, res.wx_str());


    if (startAt == line.Len())
        line.Clear();
    else
    {
        // skip whitespace
        startAt = AfterWhitespace(startAt, line);

        // Check for [Class]. ('.' pressed)
        if (IsOperatorDot(startAt, line))
        {
            tokenType = pttClass;
            line.Remove(0, startAt + 1);
        }
        // Check for
        // (1) "AAA->" ('>' pressed)
        // (2) "AAA::" (':' pressed)
        // If (1) and tokenOperatorType == otOperatorSquare, then we have
        // special case "AAA[]->" and should not change tokenOperatorType.
        else if (IsOperatorEnd(startAt, line))
        {
            if (    IsOperatorPointer(startAt, line)
                 && !res.IsEmpty()
                 && tokenOperatorType != otOperatorSquare)
                tokenOperatorType = otOperatorPointer;
            if (line.GetChar(startAt) == ':')
                tokenType = pttNamespace;
            else
                tokenType = pttClass;
            line.Remove(0, startAt + 1);
        }
        else
            line.Clear();
    }

    TRACE(_T("GetCCToken() : Left \"%s\""), line.wx_str());

    if (tokenOperatorType == otOperatorParentheses)
        tokenType = pttFunction;

    return res;
}

// skip nest braces in the expression, e.g.
//  SomeObject->SomeMethod(arg1, arg2)->Method2()
//              ^end                 ^begin
// note we skip the nest brace (arg1, arg2).
unsigned int ParseManagerBase::FindCCTokenStart(const wxString& line)
{
    // Careful: startAt can become negative, so it's defined as integer here!
    int startAt = line.Len() - 1;
    int nest    = 0;

    bool repeat = true;
    while (repeat)
    {
        repeat = false;
        // Go back to the beginning of the function/variable (token)
        startAt = BeginOfToken(startAt, line);

        // Check for [Class]. ('.' pressed)
        if (IsOperatorDot(startAt, line))
        {
            --startAt;
            repeat = true; // yes -> repeat.
        }
        // Check for [Class]-> ('>' pressed)
        // Check for [Class]:: (':' pressed)
        else if (IsOperatorEnd(startAt, line))
        {
            startAt -= 2;
            repeat = true; // yes -> repeat.
        }

        if (repeat)
        {
            // now we're just before the "." or "->" or "::"
            // skip any whitespace
            startAt = BeforeWhitespace(startAt, line);

            // check for function/array/cast ()
            if (IsClosingBracket(startAt, line))
            {
                ++nest;
                while (   (--startAt >= 0)
                       && (nest != 0) )
                {
                    switch (line.GetChar(startAt).GetValue())
                    {
                        case ']':
                        case ')': ++nest; --startAt; break;

                        case '[':
                        case '(': --nest; --startAt; break;
                        default:
                            break;
                    }

                    startAt = BeforeWhitespace(startAt, line);

                    if (IsClosingBracket(startAt, line))
                        ++nest;
                    if (IsOpeningBracket(startAt, line))
                        --nest;
                }

                startAt = BeforeToken(startAt, line);
            }
        }
    }
    ++startAt;

    startAt = AfterWhitespace(startAt, line);

    TRACE(_T("FindCCTokenStart() : Starting at %u \"%s\""), startAt, line.Mid(startAt).wx_str());

    return startAt;
}

wxString ParseManagerBase::GetNextCCToken(const wxString& line,
                                          unsigned int&   startAt,
                                          OperatorType&   tokenOperatorType)
{
    wxString res;
    int nest = 0;

    if (   (startAt < line.Len())
        && (line.GetChar(startAt) == '(') )
    {
        while (   (startAt < line.Len())
               && (   (line.GetChar(startAt) == '*')
                   || (line.GetChar(startAt) == '&')
                   || (line.GetChar(startAt) == '(') ) )
        {
            if (line.GetChar(startAt) == '(')
                ++nest;
            if (line.GetChar(startAt) == _T('*'))
                tokenOperatorType = otOperatorStar;
            ++startAt;
        }
    }

    TRACE(_T("GetNextCCToken() : at %u (%c): res=%s"), startAt, line.GetChar(startAt), res.wx_str());

    while (InsideToken(startAt, line))
    {
        res << line.GetChar(startAt);
        ++startAt;
    }
    while (   (nest > 0)
           && (startAt < line.Len()) )
    {
        // TODO: handle nested scope resolution (A::getC()).|
        if (line.GetChar(startAt) == '(')
            ++nest;
        else if (line.GetChar(startAt) == ')')
            --nest;
        ++startAt;
    }

    TRACE(_T("GetNextCCToken() : Done nest: at %u (%c): res=%s"), startAt, line.GetChar(startAt), res.wx_str());

    startAt = AfterWhitespace(startAt, line);
    if (IsOpeningBracket(startAt, line))
    {
        if (line.GetChar(startAt) == _T('('))
            tokenOperatorType = otOperatorParentheses;
        else if (line.GetChar(startAt) == _T('['))
            tokenOperatorType = otOperatorSquare;
        ++nest;
        while (   (startAt < line.Len()-1)
               && (nest != 0) )
        {
            ++startAt;
            switch (line.GetChar(startAt).GetValue())
            {
                case ']':
                case ')': --nest; ++startAt; break;

                case '[': tokenOperatorType = otOperatorSquare;
                case '(': ++nest; ++startAt; break;
                default:
                    break;
            }

            startAt = AfterWhitespace(startAt, line);

            if (IsOpeningBracket(startAt, line))
                ++nest;
            //NOTE: do not skip successive closing brackets. Eg,
            // "GetConfigManager(_T("code_completion"))->ReadBool"
            //                                        ^
            if (IsClosingBracket(startAt, line))
            {
                --nest;
                if (nest == 0)
                    ++startAt;
            }
        }
    }
    if (IsOperatorBegin(startAt, line))
        ++startAt;

    TRACE(_T("GetNextCCToken() : Return at %u (%c): res=%s"), startAt, line.GetChar(startAt), res.wx_str());

    return res;
}

void ParseManagerBase::RemoveLastFunctionChildren(TokenTree* tree,
                                                  int&       lastFuncTokenIdx)
{
    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    Token* token = tree->at(lastFuncTokenIdx);
    if (token)
    {
        lastFuncTokenIdx = -1;
        if (token->m_TokenKind & tkAnyFunction)
            token->DeleteAllChildren();
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
}

// Breaks up the phrase for code-completion.
// Suppose the user has invoked code-completion in this piece of code:
//
//   Ogre::Root::getSingleton().|
//
// This function will break this up into an std::queue (FIFO) containing
// the following items (top is first-out):
//
// Ogre             [pttNamespace]
// Root             [pttClass]
// getSingleton     [pttFunction]
// (empty space)    [pttSearchText]
//
// It also classifies each component as a pttClass, pttNamespace, pttFunction, pttSearchText
size_t ParseManagerBase::BreakUpComponents(const wxString&              actual,
                                           std::queue<ParserComponent>& components)
{
    ParserTokenType tokenType;
    wxString statement = actual;
    OperatorType tokenOperatorType;
    // break up components of phrase
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format("BreakUpComponents() Breaking up '%s'", statement));
    TRACE(_T("ParseManagerBase::BreakUpComponents()"));

    while (true)
    {
        wxString tok = GetCCToken(statement, tokenType, tokenOperatorType);

        ParserComponent pc;
        pc.component         = tok;
        pc.tokenType         = tokenType;
        pc.tokenOperatorType = tokenOperatorType;
        // Debug smart sense: output the component's name and type.
        if (g_DebugSmartSense)
        {
            wxString tokenTypeString;
            switch (tokenType)
            {
                case (pttFunction):
                {   tokenTypeString = _T("Function");   break; }
                case (pttClass):
                {   tokenTypeString = _T("Class");      break; }
                case (pttNamespace):
                {   tokenTypeString = _T("Namespace");  break; }
                case (pttSearchText):
                {   tokenTypeString = _T("SearchText"); break; }
                case (pttUndefined):
                default:
                {   tokenTypeString = _T("Undefined");         }
            }
            CCLogger::Get()->DebugLog(wxString::Format("BreakUpComponents() Found component: '%s' (%s)",
                                                       tok, tokenTypeString));
        }

        // Support global namespace like ::MessageBoxA
        // Break up into "", type is pttNameSpace and "MessageBoxA", type is pttSearchText.
        // for pttNameSpace  type, if its text (tok) is empty -> ignore this component.
        // for pttSearchText type, don't do this because for ss:: we need this, too.
        if (!tok.IsEmpty() || (tokenType == pttSearchText && !components.empty()))
        {
            if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(wxString::Format("BreakUpComponents() Adding component: '%s'.", tok));
            components.push(pc);
        }

        if (tokenType == pttSearchText)
            break;
    }

    return 0;
}

size_t ParseManagerBase::ResolveExpression(TokenTree*                  tree,
                                           std::queue<ParserComponent> components,
                                           const TokenIdxSet&          searchScope,
                                           TokenIdxSet&                result,
                                           bool                        caseSense,
                                           bool                        isPrefix)
{
    m_TemplateMap.clear();
    if (components.empty())
        return 0;

    TokenIdxSet initialScope;
    if (!searchScope.empty())
        initialScope = searchScope;
    else
        initialScope.insert(-1);

    while (!components.empty())
    {
        TokenIdxSet initialResult;
        ParserComponent subComponent = components.front();
        components.pop();
        wxString searchText = subComponent.component;
        if (searchText == _T("this"))
        {
            initialScope.erase(-1);
            TokenIdxSet tempInitialScope = initialScope;

            CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

            for (TokenIdxSet::const_iterator it = tempInitialScope.begin();
                 it != tempInitialScope.end(); ++it)
            {
                const Token* token = tree->at(*it);
                if (token && (token->m_TokenKind != tkClass))
                    initialScope.erase(*it);
            }

            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

            if (!initialScope.empty())
                continue;
            else
            {
                CCLogger::Get()->DebugLog("ResolveExpression() Error to find initial search scope.");
                break; // error happened.
            }
        }

        if (g_DebugSmartSense)
        {
            CCLogger::Get()->DebugLog(wxString::Format("ResolveExpression() Search scope with %zu result:",
                                                       initialScope.size()));
            for (TokenIdxSet::const_iterator tt = initialScope.begin(); tt != initialScope.end(); ++tt)
                CCLogger::Get()->DebugLog(wxString::Format("- Search scope: %d", *tt));
        }

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        // All functions that call the recursive GenerateResultSet should already entered a critical section.

        // e.g. A.BB.CCC.DDDD|
        if (components.empty()) // is the last component (DDDD)
            GenerateResultSet(tree, searchText, initialScope, initialResult, caseSense, isPrefix);
        else // case sensitive and full-match always (A / BB / CCC)
            GenerateResultSet(tree, searchText, initialScope, initialResult, true, false);

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        // now we should clear the initialScope.
        initialScope.clear();

        //-------------------------------------

        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("ResolveExpression() Looping %zu result.",
                                                       initialResult.size()));

        //------------------------------------
        if (!initialResult.empty())
        {
            bool locked = false;

            // loop all matches.
            for (TokenIdxSet::const_iterator it = initialResult.begin(); it != initialResult.end(); ++it)
            {
                const size_t id = (*it);
                wxString actualTypeStr;
                int parentIndex = -1;
                bool isFuncOrVar = false;

                if (locked)
                    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
                CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
                locked = true;

                const Token* token = tree->at(id);
                if (!token)
                {
                    if (g_DebugSmartSense)
                        CCLogger::Get()->DebugLog("ResolveExpression() token is NULL?!");
                    continue;
                }

                // TODO: we should deal with operators carefully.
                // it should work for class::/namespace::
                if (token->m_IsOperator && (m_LastComponent.tokenType != pttNamespace))
                    continue;

                if (g_DebugSmartSense)
                    CCLogger::Get()->DebugLog(wxString::Format("ResolvExpression() Match:'%s(ID=%zu) : type='%s'",
                                                               token->m_Name, id, token->m_BaseType));

                // recond the template map message here. hope it will work.
                // wxString tkname = token->m_Name;
                // wxArrayString tks = token->m_TemplateType;
                if (!token->m_TemplateMap.empty())
                    m_TemplateMap = token->m_TemplateMap;

                // if the token is a function/variable(i.e. is not a type)
                isFuncOrVar =   !searchText.IsEmpty()
                             && (subComponent.tokenType != pttSearchText)
                             && !token->m_BaseType.IsEmpty();
                if (isFuncOrVar)
                {
                    actualTypeStr = token->m_BaseType;
                    parentIndex = token->m_Index;
                }

                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
                locked = false;

                // handle it if the token is a function/variable(i.e. is not a type)
                if (isFuncOrVar)
                {
                    TokenIdxSet actualTypeScope;
                    if (searchScope.empty())
                        actualTypeScope.insert(-1);
                    else
                    {
                        // now collect the search scope for actual type of function/variable.
                        CollectSearchScopes(searchScope, actualTypeScope, tree);

                        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

                        // now add the current token's parent scope;
                        const Token* currentTokenParent = tree->at(parentIndex);
                        while (true)
                        {
                            if (!currentTokenParent)
                                break;
                            actualTypeScope.insert(currentTokenParent->m_Index);
                            currentTokenParent = tree->at(currentTokenParent->m_ParentIndex);
                        }

                        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
                    }

                    // now get the tokens of variable/function.
                    TokenIdxSet actualTypeResult;
                    ResolveActualType(tree, actualTypeStr, actualTypeScope, actualTypeResult);
                    if (!actualTypeResult.empty())
                    {
                        for (TokenIdxSet::const_iterator it2 = actualTypeResult.begin();
                             it2 != actualTypeResult.end();
                             ++it2)
                        {
                            initialScope.insert(*it2);

                            CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

                            const Token* typeToken = tree->at(*it2);
                            if (typeToken && !typeToken->m_TemplateMap.empty())
                                m_TemplateMap = typeToken->m_TemplateMap;

                            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

                            // and we need to add the template argument alias too.
                            AddTemplateAlias(tree, *it2, actualTypeScope, initialScope);
                        }
                    }
                    else // ok ,we search template container to check if type is template formal.
                        ResolveTemplateMap(tree, actualTypeStr, actualTypeScope, initialScope);

                    continue;
                }

                initialScope.insert(id);
            }// for

            if (locked)
                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        }
        else
        {
            initialScope.clear();
            break;
        }

        if (subComponent.tokenOperatorType != otOperatorUndefined)
        {
            TokenIdxSet operatorResult;
            ResolveOperator(tree, subComponent.tokenOperatorType, initialScope, searchScope, operatorResult);
            if (!operatorResult.empty())
                initialScope = operatorResult;
        }
        if (subComponent.tokenType != pttSearchText)
            m_LastComponent = subComponent;
    }// while

    // initialScope contains all the matching tokens after the cascade matching algorithm
    if (!initialScope.empty())
    {
        // normally, tokens have hierarchies. E.g. a constructor token is a child of a class token.
        // but here we promote (expose) the constructor tokens to the user. if a Token in initialScope
        // is a class, we add all its public constructors to the results, this give us a chance to let
        // CC jump to the declaration of a constructor, see
        // http://forums.codeblocks.org/index.php/topic,13753.msg92654.html#msg92654
        AddConstructors(tree, initialScope, result);
    }

    return result.size();
}

void ParseManagerBase::AddConstructors(TokenTree *tree, const TokenIdxSet& source, TokenIdxSet& dest)
{
    for (TokenIdxSet::iterator It = source.begin(); It != source.end(); ++It)
    {
        const Token* token = tree->at(*It);
        if (!token)
            continue;
        dest.insert(*It);

        // add constructors of the class type token
        if (token->m_TokenKind == tkClass)
        {
            // loop on its children, add its public constructors
            for (TokenIdxSet::iterator chIt = token->m_Children.begin();
                 chIt != token->m_Children.end();
                 ++chIt)
            {
                const Token* tk = tree->at(*chIt);
                if (   tk && (   tk->m_TokenKind == tkConstructor
                              || (tk->m_IsOperator && tk->m_Name.EndsWith(wxT("()"))) )
                    && (tk->m_Scope == tsPublic || tk->m_Scope == tsUndefined) )
                {
                    dest.insert(*chIt);
                }
            }
        }
    }
}

void ParseManagerBase::ResolveOperator(TokenTree*          tree,
                                       const OperatorType& tokenOperatorType,
                                       const TokenIdxSet&  tokens,
                                       const TokenIdxSet&  searchScope,
                                       TokenIdxSet&        result)
{
    if (!tree || searchScope.empty())
        return;

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    // first,we need to eliminate the tokens which are not tokens.
    TokenIdxSet opInitialScope;
    for (TokenIdxSet::const_iterator it=tokens.begin(); it!=tokens.end(); ++it)
    {
        int id = (*it);
        const Token* token = tree->at(id);
        if (token && (token->m_TokenKind == tkClass || token->m_TokenKind == tkTypedef))
            opInitialScope.insert(id);
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    // if we get nothing, just return.
    if (opInitialScope.empty())
        return;

    wxString operatorStr;
    switch(tokenOperatorType)
    {
        case otOperatorParentheses:
            operatorStr = _T("operator()"); break;
        case otOperatorSquare:
            operatorStr = _T("operator[]"); break;
        case otOperatorPointer:
            operatorStr = _T("operator->"); break;
        case otOperatorStar:
            operatorStr = _T("operator*"); break;
        case otOperatorUndefined:
        default:
            break;
    }
    if (operatorStr.IsEmpty())
        return;

    //s tart to parse the operator overload actual type.
    TokenIdxSet opInitialResult;

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    // All functions that call the recursive GenerateResultSet should already entered a critical section.
    GenerateResultSet(tree, operatorStr, opInitialScope, opInitialResult);

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    CollectSearchScopes(searchScope, opInitialScope, tree);

    if (opInitialResult.empty())
        return;

    for (TokenIdxSet::const_iterator it=opInitialResult.begin(); it!=opInitialResult.end(); ++it)
    {
        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        wxString type;
        const Token* token = tree->at((*it));
        if (token)
            type = token->m_BaseType;

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        if (type.IsEmpty())
            continue;

        TokenIdxSet typeResult;
        ResolveActualType(tree, type, opInitialScope, typeResult);
        if (!typeResult.empty())
        {
            for (TokenIdxSet::const_iterator pTypeResult = typeResult.begin();
                 pTypeResult!=typeResult.end();
                 ++pTypeResult)
            {
                result.insert(*pTypeResult);
                AddTemplateAlias(tree, *pTypeResult, opInitialScope, result);
            }
        }
        else
            ResolveTemplateMap(tree, type, opInitialScope, result);
    }
}

size_t ParseManagerBase::ResolveActualType(TokenTree*         tree,
                                           wxString           searchText,
                                           const TokenIdxSet& searchScope,
                                           TokenIdxSet&       result)
{
    // break up the search text for next analysis.
    std::queue<ParserComponent> typeComponents;
    BreakUpComponents(searchText, typeComponents);
    if (!typeComponents.empty())
    {
        TokenIdxSet initialScope;
        if (!searchScope.empty())
            initialScope = searchScope;
        else
            initialScope.insert(-1);

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        while (!typeComponents.empty())
        {
            TokenIdxSet initialResult;
            ParserComponent component = typeComponents.front();
            typeComponents.pop();
            wxString actualTypeStr = component.component;

            // All functions that call the recursive GenerateResultSet should already entered a critical section.
            GenerateResultSet(tree, actualTypeStr, initialScope, initialResult, true, false, 0xFFFF);

            if (!initialResult.empty())
            {
                initialScope.clear();
                for (TokenIdxSet::const_iterator it = initialResult.begin(); it != initialResult.end(); ++it)
                    // TODO (Morten#1#): eliminate the variable/function
                    initialScope.insert(*it);
            }
            else
            {
                initialScope.clear();
                break;
            }
        }

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        if (!initialScope.empty())
            result = initialScope;
    }

    return result.size();
}

void ParseManagerBase::ResolveTemplateMap(TokenTree*         tree,
                                          const wxString&    searchStr,
                                          const TokenIdxSet& actualTypeScope,
                                          TokenIdxSet&       initialScope)
{
    if (actualTypeScope.empty())
        return;

    wxString actualTypeStr = searchStr;
    std::map<wxString, wxString>::const_iterator it = m_TemplateMap.find(actualTypeStr);
    if (it != m_TemplateMap.end())
    {
        actualTypeStr = it->second;
        TokenIdxSet actualTypeResult;
        ResolveActualType(tree, actualTypeStr, actualTypeScope, actualTypeResult);
        if (!actualTypeResult.empty())
        {
            for (TokenIdxSet::const_iterator it2=actualTypeResult.begin(); it2!=actualTypeResult.end(); ++it2)
                initialScope.insert(*it2);
        }
    }
}

void ParseManagerBase::AddTemplateAlias(TokenTree*         tree,
                                        const int&         id,
                                        const TokenIdxSet& actualTypeScope,
                                        TokenIdxSet&       initialScope)
{
    if (!tree || actualTypeScope.empty())
        return;

    // and we need to add the template argument alias too.
    wxString actualTypeStr;

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    const Token* typeToken = tree->at(id);
    if (typeToken &&  typeToken->m_TokenKind == tkTypedef
                  && !typeToken->m_TemplateAlias.IsEmpty() )
        actualTypeStr = typeToken->m_TemplateAlias;

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    std::map<wxString, wxString>::const_iterator it = m_TemplateMap.find(actualTypeStr);
    if (it != m_TemplateMap.end())
    {
        actualTypeStr = it->second;

        if (actualTypeStr.Last() == _T('&') || actualTypeStr.Last() == _T('*'))
            actualTypeStr.RemoveLast();

        TokenIdxSet actualTypeResult;
        ResolveActualType(tree, actualTypeStr, actualTypeScope, actualTypeResult);
        if (!actualTypeResult.empty())
        {
            for (TokenIdxSet::const_iterator it2 = actualTypeResult.begin(); it2 != actualTypeResult.end(); ++it2)
                initialScope.insert(*it2);
        }
    }
}

// No critical section needed in this recursive function!
// All functions that call this recursive function, should already entered a critical section.
//
// Here are sample algorithm, if we have code snippet
//
// class AAA { public: void f1(); int m_aaa;};
// class BBB : public AAA {public: void f2(); int m_bbb;};
// BBB obj;
// obj.f|-------CC Here, we expect "f1" and "f2" be prompt
//
// Here, we do a search with the following states:
// the search text target = "f", and isPrefix = true
// parentIdx points to the class type Token "BBB"
// caseSens could be true, kindMask could be to list any function kind tokens.
//
// First, we first list the children of BBB, there are two "f1", and "m_aaa", and text match should
// only find one "f1".
// Second, we try to calculate all the ancestors of BBB, which we see a class type Token "AAA"
// list the children of AAA, and do a text match, we find "f2"
// Last, we return a Token set which contains two elements ["f1", "f2"]
//
// Note that parentIdx could be the global namespace
// Some kinds of Token types, such as Enum or Namespaces could be handled specially
size_t ParseManagerBase::GenerateResultSet(TokenTree*      tree,
                                           const wxString& target,
                                           int             parentIdx,
                                           TokenIdxSet&    result,
                                           bool            caseSens,
                                           bool            isPrefix,
                                           short int       kindMask)
{
    TRACE(_T("ParseManagerBase::GenerateResultSet_1()"));

    Token* parent = tree->at(parentIdx);
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format(_("GenerateResultSet() search '%s', parent='%s (id:%d, type:%s), isPrefix=%d'"),
                                                   target,
                                                   parent ? parent->m_Name : wxString("Global namespace"),
                                                   parent ? parent->m_Index : 0,
                                                   parent ? parent->GetTokenKindString() : wxString("?"),
                                                   isPrefix ? 1 : 0));

    // parent == null means we are searching in the global scope
    if (parent)
    {
        // we got a parent; add its children
        for (TokenIdxSet::const_iterator it = parent->m_Children.begin(); it != parent->m_Children.end(); ++it)
        {
            const Token* token = tree->at(*it);
            if (token && MatchType(token->m_TokenKind, kindMask))
            {
                if (MatchText(token->m_Name, target, caseSens, isPrefix))
                    result.insert(*it);
                else if (token && token->m_TokenKind == tkNamespace && token->m_Aliases.size()) // handle namespace aliases
                {
                    for (size_t i = 0; i < token->m_Aliases.size(); ++i)
                    {
                        if (MatchText(token->m_Aliases[i], target, caseSens, isPrefix))
                        {
                            result.insert(*it);
                            // break; ?
                        }
                    }
                }
                else if (token && token->m_TokenKind == tkEnum) // check enumerators for match too
                    // All functions that call the recursive GenerateResultSet should already entered a critical section.
                    GenerateResultSet(tree, target, *it, result, caseSens, isPrefix, kindMask);
            }
        }
        // now go up the inheritance chain and add all ancestors' children too
        tree->RecalcInheritanceChain(parent);
        for (TokenIdxSet::const_iterator it = parent->m_Ancestors.begin(); it != parent->m_Ancestors.end(); ++it)
        {
            const Token* ancestor = tree->at(*it);
            if (!ancestor)
                continue;
            for (TokenIdxSet::const_iterator it2 = ancestor->m_Children.begin(); it2 != ancestor->m_Children.end(); ++it2)
            {
                const Token* token = tree->at(*it2);
                if (token && MatchType(token->m_TokenKind, kindMask))
                {
                    if (MatchText(token->m_Name, target, caseSens, isPrefix))
                        result.insert(*it2);
                    else if (token && token->m_TokenKind == tkNamespace && token->m_Aliases.size()) // handle namespace aliases
                    {
                        for (size_t i = 0; i < token->m_Aliases.size(); ++i)
                        {
                            if (MatchText(token->m_Aliases[i], target, caseSens, isPrefix))
                            {
                                result.insert(*it2);
                                // break; ?
                            }
                        }
                    }
                    else if (token && token->m_TokenKind == tkEnum) // check enumerators for match too
                        // All functions that call the recursive GenerateResultSet should already entered a critical section.
                        GenerateResultSet(tree, target, *it2, result, caseSens, isPrefix, kindMask);
                }
            }
        }
    }
    else
    {
        // all global tokens
        const TokenList* tl = tree->GetTokens();
        for (TokenList::const_iterator it = tl->begin(); it != tl->end(); ++it)
        {
            const Token* token = *it;
            if (token && token->m_ParentIndex == -1)
            {
                if (token && MatchType(token->m_TokenKind, kindMask))
                {
                    if (MatchText(token->m_Name, target, caseSens, isPrefix))
                        result.insert(token->m_Index);
                    else if (token && token->m_TokenKind == tkNamespace && token->m_Aliases.size()) // handle namespace aliases
                    {
                        for (size_t i = 0; i < token->m_Aliases.size(); ++i)
                        {
                            if (MatchText(token->m_Aliases[i], target, caseSens, isPrefix))
                            {
                                result.insert(token->m_Index);
                                // break; ?
                            }
                        }
                    }
                    else if (token && token->m_TokenKind == tkEnum) // check enumerators for match too
                        // All functions that call the recursive GenerateResultSet should already entered a critical section.
                        GenerateResultSet(tree, target, token->m_Index, result, caseSens, isPrefix, kindMask);
                }
            }
        }
    }

    // done
    return result.size();
}

// No critical section needed in this recursive function!
// All functions that call this recursive function, should already entered a critical section.
/** detailed description
 * we have special handling of the c++ stl container with some template related code:
 * example:
 *
 * vector<string> AAA;
 * AAA.back().     // should display tokens of string members
 *
 * Cause of problem:
 *
 * The root of the problem is that CC can't parse some pieces of the C++ library. STL containers have
 * return types like "const_reference", which users of the library can treat as typedef aliases of
 * their template arguments.
 *
 * E.g. "back()" returns "const_reference", which we can assume to
 *     be a typedef alias of "const string&"
 *
 * However, these return types are actually defined through a complicated chain of typedefs,
 * templates, and inheritance. For example, the C++ library defines "const_reference" in vector
 * as a typedef alias of "_Alloc_traits::const_reference", which is a typedef alias of
 * "__gnu_cxx::_alloc_traits<_Tp_alloc_type>::const_reference". This chain actually continues, but
 * it would be too much to list here. The main thing to understand is that CC will not be able to
 * figure out what "const_reference" is supposed to be.
 *
 * Solution:
 *
 * Trying get CC to understand this chain would have made the template code even more complicated
 * and error-prone, so I used a trick. STL containers are based on the allocator class. And the
 * allocator class contains all the simple typedefs we need. For example, in the allocator class we
 * find the definition of const_reference:
 *
 * typedef const _Tp&   const_reference
 *
 * Where _Tp is a template parameter. Here's another trick - because most STL containers use the
 * name "_Tp" for their template parameter, the above definition can be directly applied to these
 * containers.
 *
 * E.g. vector is defined as:
 *         template <typename _Tp>
 *         vector { ... }
 * So AAA's template map will connect "_Tp" to "string".
 *
 * So we can look up "const_reference" in allocator, see that it returns "_Tp", look up "_Tp" in
 * the template map and add its actual value to the search scope.
 *
 * Walking through the example:
 *
 * [in ParseManagerBase::GenerateResultSet()]
 * CC sees that back() returns const_reference. It searches the TokenTree for const_reference and
 * finds the typedef belonging to allocator:
 *
 * "typedef const _Tp&   const_reference"
 *
 * CC then checks that back()'s parent, AAA, is an STL container which relies on allocator. Since it
 * is, this typedef is added to the search scope.
 *
 * [in ParseManagerBase::AddTemplateAlias()]
 * CC sees the typedef in the search scope. It searches AAA's template map for the actual type of
 * "_Tp" and finds "string". So "string" is added to the scope.
 *
 * That's the big picture. The negative of this patch is that it relies on how the C++ STL library
 * is written. If the library is ever changed significantly, then this patch will need to be updated.
 * It was an ok sacrifice to make for cleaner, maintainable code.
 */
size_t ParseManagerBase::GenerateResultSet(TokenTree*          tree,
                                           const wxString&     target,
                                           const TokenIdxSet&  parentSet,
                                           TokenIdxSet&        result,
                                           bool                caseSens,
                                           bool                isPrefix,
                                           cb_unused short int kindMask)
{
    if (!tree) return 0;

    TRACE(_T("ParseManagerBase::GenerateResultSet_2()"));

    if (target.IsEmpty())
    {
        for (TokenIdxSet::const_iterator ptr = parentSet.begin(); ptr != parentSet.end(); ++ptr)
        {
            size_t parentIdx = (*ptr);
            Token* parent = tree->at(parentIdx);
            if (!parent)
                continue;

            for (TokenIdxSet::const_iterator it = parent->m_Children.begin();
                 it != parent->m_Children.end();
                 ++it)
            {
                const Token* token = tree->at(*it);
                if (!token)
                    continue;
                if ( !AddChildrenOfUnnamed(tree, token, result) )
                {
                    result.insert(*it);
                    AddChildrenOfEnum(tree, token, result);
                }
            }

            tree->RecalcInheritanceChain(parent);

            for (TokenIdxSet::const_iterator it = parent->m_Ancestors.begin();
                 it != parent->m_Ancestors.end();
                 ++it)
            {
                const Token* ancestor = tree->at(*it);
                if (!ancestor)
                    continue;
                for (TokenIdxSet::const_iterator it2 = ancestor->m_Children.begin();
                     it2 != ancestor->m_Children.end();
                     ++it2)
                {
                    const Token* token = tree->at(*it2);
                    if (!token)
                        continue;
                    if ( !AddChildrenOfUnnamed(tree, token, result) )
                    {
                        result.insert(*it2);
                        AddChildrenOfEnum(tree, token, result);
                    }
                }
            }
        }
    }
    else
    {
        // we use FindMatches to get the items from tree directly and eliminate the
        // items which are not under the search scope.
        TokenIdxSet textMatchSet, tmpMatches;
        if (tree->FindMatches(target, tmpMatches, caseSens, isPrefix))
        {
            TokenIdxSet::const_iterator it;
            for (it = tmpMatches.begin(); it != tmpMatches.end(); ++it)
            {
                const Token* token = tree->at(*it);
                if (token)
                    textMatchSet.insert(*it);
            }
        }
        // eliminate the tokens.
        if (!textMatchSet.empty())
        {
            TRACE(wxString::Format("Find %zu valid text matched tokens from the tree.", textMatchSet.size()));

            // get the tokens under the search scope. Note: tokens can have the same names, but we are
            // only interests those under the search scope, here the search scope is the parentSet,
            // So, the outer loop is the parentSet, which is the search scope
            for (TokenIdxSet::const_iterator parentIterator = parentSet.begin();
                 parentIterator != parentSet.end();
                 ++parentIterator)
            {
                // to make it clear, parentIdx stands for search scope. (Token Idx)
                // (*it) stand for matched item id.
                int parentIdx = (*parentIterator);

                // The inner loop is the textMatchSet
                for (TokenIdxSet::const_iterator it = textMatchSet.begin();
                     it != textMatchSet.end();
                     ++it)
                {
                    const Token* token = tree->at(*it);
                    // check whether its under the parentIdx
                    // NOTE: check for unnamed or enum inside class.
                    // eg, 'ParserCommon::ParserState::ptCreateParser' should be accessed as
                    // 'ParserCommon::ptCreateParser'.
                    // Here, token is ptCreateParser and parentIdx is ParserCommon, so
                    // 'token->m_ParentIndex == parentIdx' is false. Now, we iterate over the
                    // children of parentIdx and check if any of them is unnamed or enum
                    // and match with token->m_ParentIndex. Thus if we confirm that 'token' is a
                    // child of unnamed or enum(i.e., m_ParentIndex), we add the token to result.
                    if (token && ((token->m_ParentIndex == parentIdx)
                              || IsChildOfUnnamedOrEnum(tree, token->m_ParentIndex, parentIdx)))
                        result.insert(*it);

                    // "result" will become the search scope for the next loop, so
                    // if the parentIdx has ancestors, we need to add them too.
                    if (parentIdx != -1) //global namespace does not have ancestors
                    {
                        Token* tokenParent = tree->at(parentIdx);
                        if (tokenParent)
                        {
                            // Here, we are going to calculate all tk's ancestors
                            // Finally we will add them in the "result".
                            tree->RecalcInheritanceChain(tokenParent);

                            // This is somewhat tricky, for example, we have one tk, which has the
                            // tk->m_AncestorsString == wxNavigationEnabled<wxWindow>
                            // Normally, the wxNavigationEnabled will be added, but if we have:
                            // template <class W>
                            // class wxNavigationEnabled : public W
                            // Shall we add the "W" as tk's ancestors? W is a formalTemplateArgument

                            // Add tk's ancestors
                            for ( TokenIdxSet::const_iterator ancestorIterator = tokenParent->m_Ancestors.begin();
                                  ancestorIterator != tokenParent->m_Ancestors.end();
                                  ++ancestorIterator )
                            {
                                // NOTE: check for unnamed or enum inside class (see note above).
                                if (token && ((token->m_ParentIndex == (*ancestorIterator)) //matched
                                          || IsChildOfUnnamedOrEnum(tree, token->m_ParentIndex, (*ancestorIterator))))
                                    result.insert(*it);
                            }
                        }
                    }
                    else if (-1 == parentIdx)
                    {
                        //if the search scope is global,and the token's parent token kind is tkEnum ,we add them too.
                        const Token* parentToken = tree->at(token->m_ParentIndex);
                        if (parentToken && parentToken->m_TokenKind == tkEnum)
                            result.insert(*it);
                    }

                    // Check if allocator class tokens should be added to the search scope.
                    // allocator holds the typedefs that CC needs to handle STL containers.
                    // An allocator token will be added if parentIdx is a child of an allocator dependent class.
                    // Most STL containers are dependent on allocator.
                    //
                    // For example, suppose we are completing:
                    //     vector<string> AAA;
                    //     AAA.back().
                    //
                    // Here, parentIdx == "back()", which is a child of the allocator dependent class, vector.
                    // So we add (*it) to the search scope if it is an allocator token.
                    if (token && IsAllocator(tree, token->m_ParentIndex)
                              && DependsOnAllocator(tree, parentIdx))
                        result.insert(*it);
                }
            }
        }
        else
        {
            // We need to handle namespace aliases too. I hope we can find a good way to do this.
            // TODO: Handle template class here.
            if (parentSet.count(-1))
            {
                const TokenList* tl = tree->GetTokens();
                for (TokenList::const_iterator it = tl->begin(); it != tl->end(); ++it)
                {
                    const Token* token = (*it);
                    if (token && token->m_TokenKind == tkNamespace && token->m_Aliases.size())
                    {
                        for (size_t i = 0; i < token->m_Aliases.size(); ++i)
                        {
                            if (token->m_Aliases[i] == target)
                            {
                                result.insert(token->m_Index);
                                // break; ?
                            }
                        }
                    }
                }
            }
        }
    }

    return result.size();
}

// No critical section needed in this function!
// All functions that call this function, should already entered a critical section.
bool ParseManagerBase::IsAllocator(TokenTree*   tree,
                                   const int&   id)
{
    if (!tree)
        return false;

    const Token* token = tree->at(id);
    return (token && token->m_Name.IsSameAs(_T("allocator")));
}

// No critical section needed in this recursive function!
// All functions that call this recursive function, should already entered a critical section.
//
// Currently, this function only identifies STL containers dependent on allocator.
bool ParseManagerBase::DependsOnAllocator(TokenTree*    tree,
                                          const int&    id)
{
    if (!tree)
        return false;

    const Token* token = tree->at(id);
    if (!token)
        return false;

    // If the STL class depends on allocator, it will have the form:
    // template <typename T, typename _Alloc = std::allocator<T> > class AAA { ... };
    if (token->m_TemplateArgument.Find(_T("_Alloc")) != wxNOT_FOUND)
        return true;

    // The STL class could also be a container adapter:
    // template <typename T, typename _Sequence = AAA<T> > class BBB { ... };
    // where AAA depends on allocator.
    if (token->m_TemplateArgument.Find(_T("_Sequence")) != wxNOT_FOUND)
        return true;

    return DependsOnAllocator(tree, token->m_ParentIndex);
}

void ParseManagerBase::CollectSearchScopes(const TokenIdxSet& searchScope,
                                           TokenIdxSet&       actualTypeScope,
                                           TokenTree*         tree)
{
    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    for (TokenIdxSet::const_iterator pScope=searchScope.begin(); pScope!=searchScope.end(); ++pScope)
    {
        actualTypeScope.insert(*pScope);
        // we need to pScope's parent scope too.
        if ((*pScope) != -1)
        {
            const Token* token = tree->at(*pScope);
            if (!token)
                continue;
            const Token* parent = tree->at(token->m_ParentIndex);
            while (true)
            {
                if (!parent)
                    break;
                actualTypeScope.insert(parent->m_Index);
                parent = tree->at(parent->m_ParentIndex);
            }
        }
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
}

// No critical section needed in this recursive function!
// All functions that call this function, should already entered a critical section.
int ParseManagerBase::GetTokenFromCurrentLine(TokenTree*         tree,
                                              const TokenIdxSet& tokens,
                                              size_t             curLine,
                                              const wxString&    file)
{
    TRACE(_T("ParseManagerBase::GetTokenFromCurrentLine()"));

    int result = -1;
    bool found = false;
    if (!tree)
        return result;

    const size_t fileIdx = tree->InsertFileOrGetIndex(file);
    const Token* classToken = nullptr;
    for (TokenIdxSet::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        const Token* token = tree->at(*it);
        if (!token)
            continue;

        TRACE(_T("GetTokenFromCurrentLine() Iterating: tN='%s', tF='%s', tStart=%u, tEnd=%u"),
              token->DisplayName().wx_str(), token->GetFilename().wx_str(),
              token->m_ImplLineStart, token->m_ImplLineEnd);

        if (   token->m_TokenKind & tkAnyFunction
            && token->m_ImplFileIdx == fileIdx
            && token->m_ImplLine    <= curLine
            && token->m_ImplLineEnd >= curLine)
        {
            TRACE(_T("GetTokenFromCurrentLine() tkAnyFunction : tN='%s', tF='%s', tStart=%u, tEnd=%u"),
                   token->DisplayName().wx_str(), token->GetFilename().wx_str(),
                   token->m_ImplLineStart, token->m_ImplLineEnd);
            result = token->m_Index;
            found = true;
        }
        else if (   token->m_TokenKind == tkConstructor
                 && token->m_ImplFileIdx == fileIdx
                 && token->m_ImplLine <= curLine
                 && token->m_ImplLineStart >= curLine)
        {
            TRACE(_T("GetTokenFromCurrentLine() tkConstructor : tN='%s', tF='%s', tStart=%u, tEnd=%u"),
                  token->DisplayName().wx_str(), token->GetFilename().wx_str(),
                  token->m_ImplLineStart, token->m_ImplLineEnd);
            result = token->m_Index;
            found = true;
        }
        else if (   token->m_TokenKind == tkClass
                 && token->m_ImplLineStart <= curLine
                 && token->m_ImplLineEnd >= curLine)
        {
            TRACE(_T("GetTokenFromCurrentLine() tkClass : tN='%s', tF='%s', tStart=%u, tEnd=%u"),
                  token->DisplayName().wx_str(), token->GetFilename().wx_str(),
                  token->m_ImplLineStart, token->m_ImplLineEnd);
            classToken = token;
            continue;
        }

        if (found) break; // exit for-loop

        TRACE(wxString::Format("GetTokenFromCurrentLine() Function out of bounds: tN='%s', tF='%s', tStart=%u, "
                               "tEnd=%u, line=%zu (size_t)line=%zu"), token->DisplayName(),
                               token->GetFilename(), token->m_ImplLineStart, token->m_ImplLineEnd,
                               curLine, curLine);
    }

    if (classToken)
        result = classToken->m_Index;

    return result;
}

void ParseManagerBase::ComputeCallTip(TokenTree*         tree,
                                      const TokenIdxSet& tokens,
                                      wxArrayString&     items)
{
    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    for (TokenIdxSet::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        const Token* token = tree->at(*it);
        if (!token)
            continue;

        // support constructor call tips
        if (token->m_TokenKind == tkVariable)
        {
            TokenIdxSet classes;
            tree->FindMatches(token->m_BaseType, classes, true, false, tkClass);
            for (TokenIdxSet::const_iterator clIt = classes.begin(); clIt != classes.end(); ++clIt)
            {
                const Token* tk = tree->at(*clIt);
                if (tk)
                {
                    token = tk;
                    break;
                }
            }
        }
        if (token->m_TokenKind == tkClass)
        {
            for (TokenIdxSet::iterator chIt = token->m_Children.begin();
                 chIt != token->m_Children.end();
                 ++chIt)
            {
                const Token* tk = tree->at(*chIt);
                if (   tk && (   tk->m_TokenKind == tkConstructor
                              || (tk->m_IsOperator && tk->m_Name.EndsWith(wxT("()"))) )
                    && (tk->m_Scope == tsPublic || tk->m_Scope == tsUndefined) )
                {
                    wxString tkTip;
                    if (PrettyPrintToken(tree, tk, tkTip))
                        items.Add(tkTip);
                }
            }
            continue;
        }

        // support macro call tips
        // NOTE: improved to support more advanced cases: preprocessor token mapped to
        // function / macro name or variable name (for typedef'd function ptr). Eg,
        // #define __MINGW_NAME_AW(func) func##A
        // #define MessageBox __MINGW_NAME_AW(MessageBox)
        // MessageBox(  // --> Use calltip for MessageBoxA().
        // see details in
        // http://forums.codeblocks.org/index.php/topic,19278.msg133989.html#msg133989

        // only handle variable like macro definitions
        if (token->m_TokenKind == tkMacroDef && token->m_Args.empty())
        {
            // NOTE: we use m_FullType for our search so that we accept function / macro NAMES only,
            // any actual calls will be rejected (i.e., allow "#define MessageBox MessageBoxA", but
            // not "#define MessageBox MessageBoxA(...)"
            const Token* tk = tree->at(tree->TokenExists(token->m_FullType, -1,
                                       tkFunction|tkMacroDef|tkVariable));

            // either a function or a variable, but it is OK if a macro with not empty m_Args.
            if (tk && ((tk->m_TokenKind ^ tkMacroDef) || !tk->m_Args.empty()))
                token = tk; // tkVariable could be a typedef'd function ptr (checked in PrettyPrintToken())
            else
            {
                // a variable like macro, this token don't have m_Args(call tip information), but
                // if we try to expand the token, and finally find some one who do have m_Args, then
                // the expanded token's m_Args can used as call tips.
                Tokenizer smallTokenizer(tree);
                smallTokenizer.InitFromBuffer(token->m_FullType + _T('\n'));
                tk = tree->at(tree->TokenExists(smallTokenizer.GetToken(), -1, tkFunction|tkMacroDef|tkVariable));
                // only if the expanded result is a single token
                if (tk && smallTokenizer.PeekToken().empty())
                    token = tk;
            }
        }

        wxString tkTip;
        if ( !PrettyPrintToken(tree, token, tkTip) )
            tkTip = wxT("Error while pretty printing token!");
        items.Add(tkTip);

    }// for

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
}

bool ParseManagerBase::PrettyPrintToken(TokenTree*   tree,
                                        const Token* token,
                                        wxString&    result,
                                        bool         isRoot)
{
    wxString name = token->m_Name;
    // a variable basically don't have call tips, but if it's type is a typedef'd function
    // pointer, we can still have call tips (which is the typedef function's arguments)
    if (token->m_TokenKind == tkVariable)
    {
        const Token* tk = tree->at(tree->TokenExists(token->m_BaseType, token->m_ParentIndex, tkTypedef));
        if (!tk && token->m_ParentIndex != -1)
            tk = tree->at(tree->TokenExists(token->m_BaseType, -1, tkTypedef));
        if (tk && !tk->m_Args.empty()) // typedef'd function pointer
        {
            name = token->m_Name;
            token = tk;
        }
    }

    // if the token has parents and the token is a container or a function,
    // then pretty print the parent of the token->
    if (   (token->m_ParentIndex != -1)
        && (token->m_TokenKind & (tkAnyContainer | tkAnyFunction)) )
    {
        const Token* parentToken = tree->at(token->m_ParentIndex);
        if (!parentToken || !PrettyPrintToken(tree, parentToken, result, false))
            return false;
    }

    switch (token->m_TokenKind)
    {
        case tkConstructor:
            result = result + token->m_Name + token->GetFormattedArgs();
            return true;

        case tkFunction:
            result = token->m_FullType + wxT(" ") + result + token->m_Name + token->GetFormattedArgs();
            if (token->m_IsConst)
                result += wxT(" const");
            if (token->m_IsNoExcept)
                result += wxT(" noexcept");
            return true;

        case tkClass:
        case tkNamespace:
            if (isRoot)
                result += token->m_Name;
            else
                result += token->m_Name + wxT("::");
            return true;

        case tkMacroDef:
            if (!token->GetFormattedArgs().IsEmpty())
                result = wxT("#define ") + token->m_Name + token->GetFormattedArgs();
            return true;

        case tkTypedef:
            result = token->m_BaseType + wxT(" ") + result + name + token->GetFormattedArgs();
            return true;

        case tkEnum:
        case tkDestructor:
        case tkVariable:
        case tkEnumerator:
        case tkMacroUse:
        case tkAnyContainer:
        case tkAnyFunction:
        case tkUndefined:
        default:
            break;
    }
    return true;
}
