/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "tokentree.h"

#include <wx/tokenzr.h>

#include <set>

#include "cclogger.h"

#define CC_TOKENTREE_DEBUG_OUTPUT 0

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_TOKENTREE_DEBUG_OUTPUT
        #define CC_TOKENTREE_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_TOKENTREE_DEBUG_OUTPUT
        #define CC_TOKENTREE_DEBUG_OUTPUT 2
    #endif
#endif

#if CC_TOKENTREE_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...)
#elif CC_TOKENTREE_DEBUG_OUTPUT == 2
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
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif


/** a global mutex to protect multiply access to the TokenTree
 * This mutex is tricky. if we have two tokentree instances, such as treeA and treeB,
 * When accessing treeA, we should use the mutex to forbid another thread from accessing
 * either treeA or treeB. This is because the treeA and treeB may not be thread safe; and
 * when we are using wx2.8, the wxString is not using the atomic reference counter
 * to protect the multiply thread access; and treeA and treeB may accidentally access some wxStrings.
 * The wxString in wx3.0+ doesn't have such issue. So we should remove it after the migration
 * to wx3.0+
 */

//wxMutex s_TokenTreeMutex(wxMUTEX_DEFAULT);

#include <mutex>
#include <chrono>
std::timed_mutex s_TokenTreeMutex; //(ph 250526)


TokenTree::TokenTree() :
    m_TokenTicketCount(255) // Reserve some space for the class browser
{
    m_Tokens.clear();
    m_Tree.clear();

    m_FilenameMap.clear();
    m_FileMap.clear();
    m_FilesToBeReparsed.clear();
    m_FreeTokens.clear();

    m_TopNameSpaces.clear();
    m_GlobalNameSpaces.clear();

    m_FileStatusMap.clear();
}


TokenTree::~TokenTree()
{
    clear();
}

void TokenTree::clear()
{
    m_Tree.clear();

    m_FilenameMap.clear();
    m_FileMap.clear();
    m_FilesToBeReparsed.clear();
    m_FreeTokens.clear();

    m_TopNameSpaces.clear();
    m_GlobalNameSpaces.clear();

    m_FileStatusMap.clear();

    size_t i;
    for (i = 0;i < m_Tokens.size(); ++i)
    {
        Token* token = m_Tokens[i];
        if (token)
            delete token;
    }
    m_Tokens.clear();
}

size_t TokenTree::size()
{
    return m_Tokens.size();
}

size_t TokenTree::realsize()
{
    if (m_Tokens.size() <= m_FreeTokens.size())
        return 0;

    return m_Tokens.size() - m_FreeTokens.size();
}

int TokenTree::insert(Token* newToken)
{
    if (!newToken)
        return -1;

    return AddToken(newToken, -1); // -1 means we add a new slot to the Token list (vector)
}

int TokenTree::insert(int loc, Token* newToken)
{
    if (!newToken)
        return -1;

    return AddToken(newToken, loc);
}

int TokenTree::erase(int loc)
{
    if (!m_Tokens[loc])
        return 0;

    RemoveToken(loc);
    return 1;
}

void TokenTree::erase(Token* oldToken)
{
    RemoveToken(oldToken);
}

int TokenTree::TokenExists(const wxString& name, int parent, short int kindMask)
{
    int idx = m_Tree.GetItemNo(name);
    if (!idx)
        return -1;

    TokenIdxSet& curList = m_Tree.GetItemAtPos(idx);
    for (TokenIdxSet::const_iterator it = curList.begin(); it != curList.end(); ++it)
    {
        int result = *it;
        if (result < 0 || (size_t)result >= m_Tokens.size())
            continue;

        const Token* curToken = m_Tokens[result];
        if (!curToken)
            continue;

        if ((curToken->m_ParentIndex == parent) && (curToken->m_TokenKind & kindMask))
        {
            return result;
        }
    }

    return -1;
}

int TokenTree::TokenExists(const wxString& name, const wxString& baseArgs, int parent, TokenKind kind)
{
    int idx = m_Tree.GetItemNo(name);
    if (!idx)
        return -1;

    TokenIdxSet::const_iterator it;
    TokenIdxSet& curList = m_Tree.GetItemAtPos(idx);
    for (it = curList.begin(); it != curList.end(); ++it)
    {
        int result = *it;
        if (result < 0 || (size_t)result >= m_Tokens.size())
            continue;

        const Token* curToken = m_Tokens[result];
        if (!curToken)
            continue;

        // for a container token, its args member variable is used to store inheritance information
        // so, don't compare args for tkAnyContainer
        if (   (curToken->m_ParentIndex == parent)
            && (curToken->m_TokenKind   == kind)
            && (curToken->m_BaseArgs == baseArgs || kind & tkAnyContainer) )
        {
            return result;
        }
    }

    return -1;
}

int TokenTree::TokenExists(const wxString& name, const TokenIdxSet& parents, short int kindMask)
{
    int idx = m_Tree.GetItemNo(name);
    if (!idx)
        return -1;

    TokenIdxSet::const_iterator it;
    TokenIdxSet& curList = m_Tree.GetItemAtPos(idx);
    for (it = curList.begin(); it != curList.end(); ++it)
    {
        int result = *it;
        if (result < 0 || (size_t)result >= m_Tokens.size())
            continue;

        const Token* curToken = m_Tokens[result];
        if (!curToken)
            continue;

        if (curToken->m_TokenKind & kindMask)
        {
            for ( TokenIdxSet::const_iterator pIt = parents.begin();
                  pIt != parents.end(); ++pIt )
            {
                if (curToken->m_ParentIndex == *pIt)
                    return result;
            }
        }
    }

    return -1;
}

int TokenTree::TokenExists(const wxString& name, const wxString& baseArgs, const TokenIdxSet& parents, TokenKind kind)
{
    int idx = m_Tree.GetItemNo(name);
    if (!idx)
        return -1;

    TokenIdxSet::const_iterator it;
    TokenIdxSet& curList = m_Tree.GetItemAtPos(idx);
    for (it = curList.begin(); it != curList.end(); ++it)
    {
        int result = *it;
        if (result < 0 || (size_t)result >= m_Tokens.size())
            continue;

        const Token* curToken = m_Tokens[result];
        if (!curToken)
            continue;

        // for a container token, their args member variable is used to store inheritance information
        // so, don't compare args for tkAnyContainer
        if (  curToken->m_TokenKind == kind
            && (   curToken->m_BaseArgs == baseArgs
                || kind & tkAnyContainer ))
        {
            for ( TokenIdxSet::const_iterator pIt = parents.begin();
                  pIt != parents.end(); ++pIt )
            {
                if (curToken->m_ParentIndex == *pIt)
                    return result;
            }
        }
    }

    return -1;
}

size_t TokenTree::FindMatches(const wxString& query, TokenIdxSet& result, bool caseSensitive,
                              bool is_prefix, TokenKind kindMask)
{
    result.clear();

    std::set<size_t> lists;
    int numitems = m_Tree.FindMatches(query, lists, caseSensitive, is_prefix);
    if (!numitems)
        return 0;

    // now the lists contains indexes to all the matching keywords
    // first loop will find all the keywords
    for (std::set<size_t>::const_iterator it = lists.begin(); it != lists.end(); ++it)
    {
        const TokenIdxSet* curset = &(m_Tree.GetItemAtPos(*it));
        // second loop will get all the items mapped by the same keyword,
        // for example, we have ClassA::foo, ClassB::foo ...
        if (curset)
        {
            for (TokenIdxSet::const_iterator it2 = curset->begin(); it2 != curset->end(); ++it2)
            {
                const Token* token = at(*it2);
                if (   token
                    && (   (kindMask == tkUndefined)
                        || (token->m_TokenKind & kindMask) ) )
                    result.insert(*it2);
            }
        }
    }
    return result.size();
}
// ----------------------------------------------------------------------------
size_t TokenTree::FindTokensInFile(const wxString& filename, TokenIdxSet& result, short int kindMask)
// ----------------------------------------------------------------------------
{
    result.clear();

    // get file idx
    wxString f(filename);
    // convert the UNIX path style
    while (f.Replace(_T("\\"),_T("/")))
        { ; }

    if ( !m_FilenameMap.HasItem(f) )
    {
        TRACE(_T("TokenTree::FindTokensInFile() : File '%s' not found in file names map."), f.wx_str());
        return 0;
    }

    int idx = m_FilenameMap.GetItemNo(f);

    // now get the tokens set matching this file idx
    TokenFileMap::iterator itf = m_FileMap.find(idx);
    if (itf == m_FileMap.end())
    {
        TRACE(_T("TokenTree::FindTokensInFile() : No tokens found for file '%s' (index %d)."), f.wx_str(), idx);
        return 0;
    }

    // loop all results and add to final result set after filtering on token kind
    TokenIdxSet& tokens = itf->second;
    for (TokenIdxSet::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        const Token* token = at(*it);
        if (token && (kindMask & token->m_TokenKind))
            result.insert(*it);
    }

    TRACE(_T("TokenTree::FindTokensInFile() : Found %lu results for file '%s'."),
          static_cast<unsigned long>(result.size()), f.wx_str());
    return result.size();
}

void TokenTree::RenameToken(Token* token, const wxString& newName)
{
    if (!token)
        return;
    // remove the old token index from the TokenIdxSet mapped by old name.
    int slotNo = m_Tree.GetItemNo(token->m_Name);
    if (slotNo)
    {
        TokenIdxSet& curList = m_Tree.GetItemAtPos(slotNo);
    // Note: As we have no way to actually delete keys in the TokenSearchTree,
    // the previous name index path of the token will still exist, as well as its TokenIdxSet slot,
    // but this slot will be empty and as result will lead to nothing.
    // This is the same thing the RemoveToken procedure does.
        curList.erase(token->m_Index);
    };
    token->m_Name = newName;

    static TokenIdxSet tmpTokens = TokenIdxSet();

    size_t tokenIdx = m_Tree.AddItem(newName, tmpTokens);
    TokenIdxSet& curList = m_Tree.GetItemAtPos(tokenIdx);

    // add the old token index to the TokenIdxSet mapped by new name, note Token index is not changed
    curList.insert(token->m_Index);
}

int TokenTree::AddToken(Token* newToken, int forceIdx)
{
    if (!newToken)
        return -1;

    const wxString & name = newToken->m_Name;

    static TokenIdxSet tmpTokens = TokenIdxSet();

    // Insert the token's name and the token in the (inserted?) list
    size_t tokenIdx = m_Tree.AddItem(name, tmpTokens);
    TokenIdxSet& curList = m_Tree.GetItemAtPos(tokenIdx);

    int newItem = AddTokenToList(newToken, forceIdx);
    curList.insert(newItem);

    size_t fIdx = newToken->m_FileIdx;
    m_FileMap[fIdx].insert(newItem);

    // Add Token (if applicable) to the namespaces indexes
    if (newToken->m_ParentIndex < 0)
    {
        newToken->m_ParentIndex = -1;
        m_GlobalNameSpaces.insert(newItem);
        if (newToken->m_TokenKind == tkNamespace)
            m_TopNameSpaces.insert(newItem);
    }

    // All done!
    return newItem;
}

void TokenTree::RemoveToken(int idx)
{
    if (idx<0 || (size_t)idx >= m_Tokens.size())
        return;

    RemoveToken(m_Tokens[idx]);
}

void TokenTree::RemoveToken(Token* oldToken)
{
    if (!oldToken)
        return;

    int idx = oldToken->m_Index;
    if (m_Tokens[idx]!=oldToken)
        return;

    // Step 1: Detach token from its parent

    Token* parentToken = 0;
    if ((size_t)(oldToken->m_ParentIndex) >= m_Tokens.size())
        oldToken->m_ParentIndex = -1;
    if (oldToken->m_ParentIndex >= 0)
        parentToken = m_Tokens[oldToken->m_ParentIndex];
    if (parentToken)
        parentToken->m_Children.erase(idx);

    TokenIdxSet nodes;
    TokenIdxSet::const_iterator it;

    // Step 2: Detach token from its ancestors

    nodes = (oldToken->m_DirectAncestors);
    for (it = nodes.begin();it!=nodes.end(); ++it)
    {
        int ancestoridx = *it;
        if (ancestoridx < 0 || (size_t)ancestoridx >= m_Tokens.size())
            continue;
        Token* ancestor = m_Tokens[ancestoridx];
        if (ancestor)
            ancestor->m_Descendants.erase(idx);
    }
    oldToken->m_Ancestors.clear();
    oldToken->m_DirectAncestors.clear();

    // Step 3: Remove children

    nodes = (oldToken->m_Children); // Copy the list to avoid interference
    for (it = nodes.begin();it!=nodes.end(); ++it)
        RemoveToken(*it);
    // m_Children SHOULD be empty by now - but clear anyway.
    oldToken->m_Children.clear();

    // Step 4: Remove descendants

    nodes = oldToken->m_Descendants; // Copy the list to avoid interference
    for (it = nodes.begin();it!=nodes.end(); ++it)
    {
        if (*it == idx) // that should not happen, we can not be our own descendant, but in fact that can happen with boost
        {
            CCLogger::Get()->DebugLog(_T("Break out the loop to remove descendants, to avoid a crash. We can not be our own descendant!"));
            break;
        }
        RemoveToken(*it);
    }
    // m_Descendants SHOULD be empty by now - but clear anyway.
    oldToken->m_Descendants.clear();

    // Step 5: Detach token from the SearchTrees

    int idx2 = m_Tree.GetItemNo(oldToken->m_Name);
    if (idx2)
    {
        TokenIdxSet& curList = m_Tree.GetItemAtPos(idx2);
        curList.erase(idx);
    }

    // Now, from the global namespace (if applicable)
    if (oldToken->m_ParentIndex == -1)
    {
        m_GlobalNameSpaces.erase(idx);
        m_TopNameSpaces.erase(idx);
    }

    // Step 6: Finally, remove it from the list.

    RemoveTokenFromList(idx);
}

int TokenTree::AddTokenToList(Token* newToken, int forceidx)
{
    if (!newToken)
        return -1;

    int result = -1;

    // if the token index is specified, then just replace the specified slot to the newToken, this
    // usually happens we construct the whole TokenTree from cache.
    // other wise, we just append one to the vector or reused free slots stored in m_FreeTokens
    // so, it is normal case in any parsing stages.
    if (forceidx >= 0)
    {
        if ((size_t)forceidx >= m_Tokens.size())
        {
            int max = 250*((forceidx + 250) / 250);
            m_Tokens.resize((max),0); // fill next 250 items with null-values
        }
        m_Tokens[forceidx] = newToken;
        result = forceidx;
    }
    else
    {
        if (m_FreeTokens.size())
        {
            result = m_FreeTokens.back();
            m_FreeTokens.pop_back();
            m_Tokens[result] = newToken;
        }
        else
        {
            result = m_Tokens.size();
            m_Tokens.push_back(newToken);
        }
    }

    newToken->m_TokenTree = this;
    newToken->m_Index = result;

    // Clean up extra string memory
    newToken->m_FullType.Shrink();
    newToken->m_BaseType.Shrink();
    newToken->m_Name.Shrink();
    newToken->m_Args.Shrink();
    newToken->m_BaseArgs.Shrink();
    newToken->m_AncestorsString.Shrink();
    newToken->m_TemplateArgument.Shrink();

    return result;
}

void TokenTree::RemoveTokenFromList(int idx)
{
    if (idx < 0 || (size_t)idx >= m_Tokens.size())
        return;
    Token* oldToken = m_Tokens[idx];
    if (oldToken)
    {
        m_Tokens[idx] = 0;
        m_FreeTokens.push_back(idx);
        delete oldToken;
    }
}
// ----------------------------------------------------------------------------
void TokenTree::RemoveFile(const wxString& filename)
// ----------------------------------------------------------------------------
{
    RemoveFile( InsertFileOrGetIndex(filename) );
}
// ----------------------------------------------------------------------------
void TokenTree::RemoveFile(int fileIdx)
// ----------------------------------------------------------------------------
{
    if (fileIdx <= 0) return; // nothing to do

    TokenIdxSet& the_list = m_FileMap[fileIdx];
    for (TokenIdxSet::const_iterator it = the_list.begin(); it != the_list.end();)
    {
        int idx = *it;
        if (idx < 0 || (size_t)idx > m_Tokens.size())
        {
            the_list.erase(it++);
            continue;
        }

        Token* the_token = at(idx);
        if (!the_token)
        {
            the_list.erase(it++);
            continue;
        }

        // do not remove token lightly...
        // only if both its decl filename and impl filename are either empty or match this file
        // if one of those filenames do not match the above criteria
        // just clear the relevant info, leaving the token where it is...
        bool match1 = the_token->m_FileIdx     == 0 || static_cast<int>(the_token->m_FileIdx)     == fileIdx;
        bool match2 = the_token->m_ImplFileIdx == 0 || static_cast<int>(the_token->m_ImplFileIdx) == fileIdx;
        bool match3 = CheckChildRemove(the_token, fileIdx);
        if (match1 && match2 && match3)
        {
            RemoveToken(the_token); // safe to remove the token
            the_list.erase(it++);
            continue;
        }
        else
        {
            // do not remove token, just clear the matching info
            if (match1)
            {
                the_token->m_FileIdx = 0;
                the_token->m_Line = 0;
                the_token->m_Doc.clear();
            }
            else if (match2)
            {
                the_token->m_ImplFileIdx = 0;
                the_token->m_ImplLine = 0;
                the_token->m_ImplDoc.clear();
            }
        }

        ++it;
    }
}

bool TokenTree::CheckChildRemove(const Token* token, int fileIdx)
{
    const TokenIdxSet& nodes = (token->m_Children); // Copy the list to avoid interference
    TokenIdxSet::const_iterator it;
    for (it = nodes.begin(); it != nodes.end(); ++it)
    {
        int idx = *it;
        if (idx < 0 || (size_t)idx > m_Tokens.size())
            continue;

        const Token* the_token = at(idx);
        if (!the_token)
            continue;

        bool match1 = the_token->m_FileIdx     == 0 || static_cast<int>(the_token->m_FileIdx)     == fileIdx;
        bool match2 = the_token->m_ImplFileIdx == 0 || static_cast<int>(the_token->m_ImplFileIdx) == fileIdx;
        if (match1 && match2)
            continue;
        else
            return false;  // one child is belong to another file
    }
    return true;           // no children should be reserved, so we can safely remove the token
}

void TokenTree::RecalcFreeList()
{
    m_FreeTokens.clear();
    for (int i = m_Tokens.size() - 1; i >= 0; --i)
    {
        if (!m_Tokens[i])
            m_FreeTokens.push_back(i);
    }
}
// ----------------------------------------------------------------------------
void TokenTree::RecalcInheritanceChain(Token* token)
// ----------------------------------------------------------------------------
{
    if (!token)
        return;
    if (!(token->m_TokenKind & (tkClass | tkTypedef | tkEnum | tkNamespace)))
        return;
    if (token->m_AncestorsString.IsEmpty())
        return;

    token->m_DirectAncestors.clear();
    token->m_Ancestors.clear();

    wxStringTokenizer tkz(token->m_AncestorsString, _T(","));
    TRACE(_T("RecalcInheritanceChain() : Token %s, Ancestors %s"), token->m_Name.wx_str(),
          token->m_AncestorsString.wx_str());
    TRACE(_T("RecalcInheritanceChain() : Removing ancestor string from %s"), token->m_Name.wx_str());
    token->m_AncestorsString.Clear();

    while (tkz.HasMoreTokens())
    {
        wxString ancestor = tkz.GetNextToken();
        if (ancestor.IsEmpty() || ancestor == token->m_Name)
            continue;

        TRACE(_T("RecalcInheritanceChain() : Ancestor %s"), ancestor.wx_str());

        // ancestors might contain namespaces, e.g. NS::Ancestor
        if (ancestor.Find(_T("::")) != wxNOT_FOUND)
        {
            Token* ancestorToken = 0;
            wxStringTokenizer anctkz(ancestor, _T("::"));
            while (anctkz.HasMoreTokens())
            {
                wxString ns = anctkz.GetNextToken();
                if (!ns.IsEmpty())
                {
                    int ancestorIdx = TokenExists(ns, ancestorToken ? ancestorToken->m_Index : -1,
                                                  tkNamespace | tkClass | tkTypedef);
                    ancestorToken = at(ancestorIdx);
                    if (!ancestorToken) // unresolved
                        break;
                }
            }
            if (   ancestorToken
                && ancestorToken != token
                && (ancestorToken->m_TokenKind == tkClass || ancestorToken->m_TokenKind == tkNamespace) )
            {
                TRACE(_T("RecalcInheritanceChain() : Resolved to %s"), ancestorToken->m_Name.wx_str());
                RecalcInheritanceChain(ancestorToken);
                token->m_Ancestors.insert(ancestorToken->m_Index);
                ancestorToken->m_Descendants.insert(token->m_Index);
                TRACE(_T("RecalcInheritanceChain() :  + '%s'"), ancestorToken->m_Name.wx_str());
            }
            else
            {
                TRACE(_T("RecalcInheritanceChain() :  ! '%s' (unresolved)"), ancestor.wx_str());
            }
        }
        else // no namespaces in ancestor
        {
            // accept multiple matches for inheritance
            TokenIdxSet result;
            FindMatches(ancestor, result, true, false);
            for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
            {
                Token* ancestorToken = at(*it);
                // only classes take part in inheritance
                if (   ancestorToken
                    && (ancestorToken != token)
                    && (   (ancestorToken->m_TokenKind == tkClass)
                        || (ancestorToken->m_TokenKind == tkEnum)
                        || (ancestorToken->m_TokenKind == tkTypedef)
                        || (ancestorToken->m_TokenKind == tkNamespace) ) )
                {
                    RecalcInheritanceChain(ancestorToken);
                    token->m_Ancestors.insert(*it);
                    ancestorToken->m_Descendants.insert(token->m_Index);
                    TRACE(_T("RecalcInheritanceChain() :  + '%s'"), ancestorToken->m_Name.wx_str());
                }
            }
#if defined(CC_TOKEN_DEBUG_OUTPUT)
    #if CC_TOKEN_DEBUG_OUTPUT
            if (result.empty())
                TRACE(_T("RecalcInheritanceChain() :  ! '%s' (unresolved)"), ancestor.wx_str());
    #endif
#endif
        }

        // Now, we have calc all the direct ancestors
        token->m_DirectAncestors = token->m_Ancestors;
    }

#if defined(CC_TOKEN_DEBUG_OUTPUT)
    #if CC_TOKEN_DEBUG_OUTPUT
    wxStopWatch sw;
    TRACE(_T("RecalcInheritanceChain() : First iteration took : %ld ms"), sw.Time());
    sw.Start();
    #endif
#endif

    // recalc
    TokenIdxSet result;
    for (TokenIdxSet::const_iterator it = token->m_Ancestors.begin(); it != token->m_Ancestors.end(); ++it)
        RecalcFullInheritance(*it, result);

    // now, add the resulting set to ancestors set
    for (TokenIdxSet::const_iterator it = result.begin(); it != result.end(); ++it)
    {
        Token* ancestor = at(*it);
        if (ancestor)
        {
            token->m_Ancestors.insert(*it);
            ancestor->m_Descendants.insert(token->m_Index);
        }
    }

#if defined(CC_TOKEN_DEBUG_OUTPUT)
    #if CC_TOKEN_DEBUG_OUTPUT
    if (token)
    {
        // debug loop
        TRACE(_T("RecalcInheritanceChain() : Ancestors for %s:"), token->m_Name.wx_str());
        for (TokenIdxSet::const_iterator it = token->m_Ancestors.begin(); it != token->m_Ancestors.end(); ++it)
        {
            const Token* anc_token = at(*it);
            if (anc_token)
                TRACE(_T("RecalcInheritanceChain() :  + %s"), anc_token->m_Name.wx_str());
            else
                TRACE(_T("RecalcInheritanceChain() :  + NULL?!"));
        }
    }
    #endif
#endif

#if defined(CC_TOKEN_DEBUG_OUTPUT)
    #if CC_TOKEN_DEBUG_OUTPUT
    TRACE(_T("RecalcInheritanceChain() : Second iteration took : %ld ms"), sw.Time());
    #endif
#endif

    TRACE(_T("RecalcInheritanceChain() : Full inheritance calculated."));
}

// caches the inheritance info for each token (recursive function)
void TokenTree::RecalcFullInheritance(int parentIdx, TokenIdxSet& result)
{
    // no parent token? no ancestors...
    if (parentIdx == -1)
        return;

    // no parent token? no ancestors...
    const Token* ancestor = at(parentIdx);
    if (!ancestor)
        return;

    // only classes take part in inheritance
    if ( !(ancestor->m_TokenKind & (tkClass | tkTypedef)) )
        return;

    TRACE(_T("RecalcFullInheritance() : Anc: '%s'"), ancestor->m_Name.wx_str());

    // for all its ancestors
    for (TokenIdxSet::const_iterator it = ancestor->m_Ancestors.begin(); it != ancestor->m_Ancestors.end(); ++it)
    {
        if (*it != -1 && // not global scope
            *it != parentIdx && // not the same token (avoid infinite loop)
            result.find(*it) == result.end()) // not already in the set (avoid infinite loop)
        {
            // add it to the set
            result.insert(*it);
            // and recurse for its ancestors
            RecalcFullInheritance(*it, result);
        }
    }
}

Token* TokenTree::GetTokenAt(int idx)
{
    if (idx < 0 || (size_t)idx >= m_Tokens.size())
        return 0;

    return m_Tokens[idx];
}

const Token* TokenTree::GetTokenAt(int idx) const
{
    if (idx < 0 || (size_t)idx >= m_Tokens.size())
        return 0;

    return m_Tokens[idx];
}

size_t TokenTree::InsertFileOrGetIndex(const wxString& filename)
{
    wxString f(filename); while (f.Replace(_T("\\"),_T("/"))) { ; }

    // Insert does not alter the tree if the filename is already found.
    return m_FilenameMap.insert(f);
}

size_t TokenTree::GetFileMatches(const wxString& filename, std::set<size_t>& result,
                                  bool caseSensitive,       bool              is_prefix)
{
  wxString f(filename); while (f.Replace(_T("\\"),_T("/"))) { ; }

  return m_FilenameMap.FindMatches(f, result, caseSensitive, is_prefix);
}

size_t TokenTree::GetFileIndex(const wxString& filename)
{
  wxString f(filename); while (f.Replace(_T("\\"),_T("/"))) { ; }

  return m_FilenameMap.GetItemNo(f);
}

const wxString TokenTree::GetFilename(size_t fileIdx) const
{
    return m_FilenameMap.GetString(fileIdx);
}

bool TokenTree::IsFileParsed(const wxString& filename)
{
    size_t fileIdx = InsertFileOrGetIndex(filename);

    bool parsed = (   m_FileMap.count(fileIdx)
                   && (m_FileStatusMap[fileIdx]!=fpsNotParsed)
                   && !m_FilesToBeReparsed.count(fileIdx) );

    return parsed;
}

void TokenTree::MarkFileTokensAsLocal(const wxString& filename, bool local, void* userData)
{
    MarkFileTokensAsLocal( InsertFileOrGetIndex(filename), local, userData );
}

void TokenTree::MarkFileTokensAsLocal(size_t fileIdx, bool local, void* userData)
{
    if (!fileIdx)
        return;

    TokenIdxSet& tokens = m_FileMap[fileIdx];
    for (TokenIdxSet::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
    {
        Token* token = m_Tokens.at(*it);
        if (token)
        {
            token->m_IsLocal  = local; // only the tokens belong to project files are marked as local
            token->m_UserData = userData; // a pointer to the c::b project
        }
    }
}
// ----------------------------------------------------------------------------
size_t TokenTree::ReserveFileForParsing(const wxString& filename, bool preliminary)
// ----------------------------------------------------------------------------
{
    const size_t fileIdx = InsertFileOrGetIndex(filename);
    if (   m_FilesToBeReparsed.count(fileIdx)
        && (!m_FileStatusMap.count(fileIdx) || m_FileStatusMap[fileIdx]==fpsDone) )
    {
        RemoveFile(filename);
        m_FilesToBeReparsed.erase(fileIdx);
        m_FileStatusMap[fileIdx]=fpsNotParsed;
    }

    if (m_FileStatusMap.count(fileIdx))
    {
        FileParsingStatus status = m_FileStatusMap[fileIdx];
        if (preliminary)
        {
            if (status >= fpsAssigned)
                return 0; // Already assigned
        }
        else
        {
            if (status > fpsAssigned)
                return 0; // No parsing needed
        }
    }
    m_FilesToBeReparsed.erase(fileIdx);
    m_FileStatusMap[fileIdx] = preliminary ? fpsAssigned : fpsBeingParsed; // Reserve file
    return fileIdx;
}

void TokenTree::FlagFileForReparsing(const wxString& filename)
{
    m_FilesToBeReparsed.insert( InsertFileOrGetIndex(filename) );
}

void TokenTree::FlagFileAsParsed(const wxString& filename)
{
    m_FileStatusMap[ InsertFileOrGetIndex(filename) ] = fpsDone;
}

void TokenTree::AppendDocumentation(int tokenIdx, unsigned int fileIdx, const wxString& doc)
{
    // fetch the Token pointer
    Token* tk = GetTokenAt(tokenIdx);
    if (!tk)
        return;
    if (tk->m_FileIdx == fileIdx) // in the same file index
    {
        wxString& newDoc = tk->m_Doc;
        if (newDoc == doc) // Do not duplicate
            return;
        newDoc += doc; // document could happens before and after the Token, we combine them
        newDoc.Shrink();
    }
    else if (tk->m_ImplFileIdx == fileIdx)
    {
        wxString& newDoc = tk->m_ImplDoc;
        if (newDoc == doc) // Do not duplicate
            return;
        newDoc += doc; // document could happens before and after the Token, we combine them
        newDoc.Shrink();
    }
}

wxString TokenTree::GetDocumentation(int tokenIdx)
{
    // fetch the Token pointer
    Token* tk = GetTokenAt(tokenIdx);
    if (!tk)
        return wxEmptyString;
    return tk->m_Doc + tk->m_ImplDoc;
}
