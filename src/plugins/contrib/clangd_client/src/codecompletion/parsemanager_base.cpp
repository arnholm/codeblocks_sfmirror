/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
#endif

#include "parsemanager_base.h"
#include "parser/tokenizer.h"

#include "parser/cclogger.h"

#define CC_ParseManagerBASE_DEBUG_OUTPUT 0

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_ParseManagerBASE_DEBUG_OUTPUT
        #define CC_ParseManagerBASE_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_ParseManagerBASE_DEBUG_OUTPUT
        #define CC_ParseManagerBASE_DEBUG_OUTPUT 2
    #endif
#endif

#ifdef CC_PARSER_TEST
    #define ADDTOKEN(format, args...) \
            CCLogger::Get()->AddToken(F(format, ##args))
    #define TRACE(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
#else
    #if CC_ParseManagerBASE_DEBUG_OUTPUT == 1
        #define ADDTOKEN(format, args...) \
                CCLogger::Get()->AddToken(F(format, ##args))
        #define TRACE(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
        #define TRACE2(format, args...)
    #elif CC_ParseManagerBASE_DEBUG_OUTPUT == 2
        #define ADDTOKEN(format, args...) \
                CCLogger::Get()->AddToken(F(format, ##args))
        #define TRACE(format, args...)                                              \
            do                                                                      \
            {                                                                       \
                if (g_EnableDebugTrace)                                             \
                    CCLogger::Get()->DebugLog(wxString::Format(format, ##args));                   \
            }                                                                       \
            while (false)
        #define TRACE2(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
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


// ----------------------------------------------------------------------------
void ParseManagerBase::RemoveLastFunctionChildren(TokenTree* tree,
                                                  int& lastFuncTokenIdx)
// ----------------------------------------------------------------------------
{
    // a new parser activated, so remove the old parser's local variable tokens.

    /** Remove the last function's children. When doing codecompletion in a function body, the
     *  function body up to the caret position was parsed, and the local variables defined in
     *  the function were recorded as the function's children.
     *  Note that these tokens are marked as temporary tokens, so if the edit caret moves to another
     *  function body, these temporary tokens should be removed.
    */
    // FIXME (ph#): Is this function necessary with clangd ??
    // Removing function variables with Clangd may not be necessary since on every 'save file'
    // and any changes just before a code completion request
    // clangd re-parses the file and sends back all current variables which are then
    // (re)inserted by Parser::OnLSP_DocumentSymbols();

    /// Experiment, don't remove variables. This routine may not be needed for clangd
    // So far this has been working ok since 21/10/05
    return ;
    // 2023/02/26 a crash occured using token ptr may have been caused by this optimization
    // I could not recreate the situation, so I added if(token) checks to every use of token ptr
    //-return ;
    // FIXME (ph#): Use a TryLock() and if failure, try to reschedule this function like other locks.
    // -------------------------------------------
    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)   //TokenTree lock
    // -------------------------------------------

    Token* token = tree->at(lastFuncTokenIdx);
    if (token)
    {
        lastFuncTokenIdx = -1;
        if (token->m_TokenKind & tkAnyFunction)
            token->DeleteAllChildren();
    }

    // -----------------------------------------------
    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)     //TokenTree UnLock
    // -----------------------------------------------
    s_TokenTreeMutex_Owner = wxString();
}


// No critical section needed in this recursive function!
// All functions that call this recursive function, should already have entered a critical section.
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
    if (s_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format(_("GenerateResultSet() search '%s', parent='%s (id:%d, type:%s), isPrefix=%d'"),
                                    target.wx_str(),
                                    parent ? parent->m_Name.wx_str() : wxString(_T("Global namespace")).wx_str(),
                                    parent ? parent->m_Index : 0,
                                    parent ? parent->GetTokenKindString().wx_str() : 0,
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

        TRACE(_T("GetTokenFromCurrentLine() Function out of bounds: tN='%s', tF='%s', tStart=%u, ")
              _T("tEnd=%u, line=%lu (size_t)line=%lu"), token->DisplayName().wx_str(),
              token->GetFilename().wx_str(), token->m_ImplLineStart, token->m_ImplLineEnd,
              static_cast<unsigned long>(curLine), static_cast<unsigned long>(curLine));
    }

    if (classToken)
        result = classToken->m_Index;

    return result;
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
