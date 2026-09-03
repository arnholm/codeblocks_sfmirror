/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <wx/event.h>
#include <wx/string.h>

#include "tokentree.h"

#include "token.h"

#define CC_TOKEN_DEBUG_OUTPUT 0

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_TOKEN_DEBUG_OUTPUT
        #define CC_TOKEN_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_TOKEN_DEBUG_OUTPUT
        #define CC_TOKEN_DEBUG_OUTPUT 2
    #endif
#endif

#if CC_TOKEN_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(F(format, ##args))
    #define TRACE2(format, args...)
#elif CC_TOKEN_DEBUG_OUTPUT == 2
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
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif

Token::Token(const wxString& name, unsigned int file, unsigned int line, size_t ticket) :
    m_Name(name),
    m_FileIdx(file),
    m_Line(line),
    m_ImplFileIdx(0),
    m_ImplLine(0),
    m_ImplLineStart(0),
    m_ImplLineEnd(0),
    m_Scope(tsUndefined),
    m_TokenKind(tkUndefined),
    m_IsOperator(false),
    m_IsLocal(false),
    m_IsTemp(false),
    m_IsConst(false),
    m_IsNoExcept(false),
    m_IsAnonymous(false),
    m_Index(-1),
    m_ParentIndex(-1),
    m_UserData(0),
    m_TokenTree(0),
    m_Ticket(ticket)
{
    //ctor
}

Token::~Token()
{
    //dtor
    m_TemplateMap.clear();
    m_TemplateType.clear();
}

wxString Token::DisplayName() const
{
    wxString result;
    if      (m_TokenKind == tkClass)
        return result << "class "     << m_Name << m_BaseArgs << " {...}";
    else if (m_TokenKind == tkNamespace)
        return result << "namespace " << m_Name << " {...}";
    else if (m_TokenKind == tkEnum)
        return result << "enum "    << m_Name << " {...}";
    else if (m_TokenKind == tkTypedef)
    {
        result << "typedef";

        if (!m_FullType.IsEmpty())
            result << " " << m_FullType;

        // we support 2 cases of typedef'd function pointers, and in each case the type is stored
        // as below:
        // typedef void (*dMessageFunction)(int errnum, const char *msg, va_list ap);
        // --> type is stored as: (*)
        // typedef void (MyClass::*Function)(int);
        // --> type is stored as: (MyClass::*)
        // so, ensure we really have ')' as the last char.
        if (result.Find('*', true) != wxNOT_FOUND && result.Last() == ')')
        {
            result.RemoveLast();
            return result << m_Name << ")" <<  GetFormattedArgs();
        }

        if (!m_TemplateArgument.IsEmpty())
            result << m_TemplateArgument;

        return result << " " << m_Name;
    }
    else if (m_TokenKind == tkMacroDef)
    {
        result << "#define " << m_Name << GetFormattedArgs();
        if (!m_FullType.IsEmpty())
            result << " " << m_FullType;

        return result;
    }

    // else
    if (!m_FullType.IsEmpty())
        result << m_FullType << m_TemplateArgument << " ";

    if (m_TokenKind == tkEnumerator)
        return result << GetNamespace() << m_Name << "=" << GetFormattedArgs();

    return result << GetNamespace() << m_Name << GetStrippedArgs();
}

bool Token::IsValidAncestor(const wxString& ancestor)
{
    switch (ancestor.Len())
    {
    case 3:
        if (ancestor == "int")
            return false;
        break;

    case 4:
        if (   ancestor == "void"
            || ancestor == "bool"
            || ancestor == "long"
            || ancestor == "char" )
        {
            return false;
        }
        break;

    case 5:
        if (   ancestor == "short"
            || ancestor == "float" )
        {
            return false;
        }
        break;

    case 6:
        if (   ancestor == "size_t"
            || ancestor == "double" )
        {
            return false;
        }
        break;

    case 10:
        if (ancestor == "value_type")
            return false;
        break;

    default:
        if (   ancestor.StartsWith("unsigned")
            || ancestor.StartsWith("signed") )
        {
            return false;
        }
        break;
    }

    return true;
}

wxString Token::GetFilename() const
{
    if (!m_TokenTree)
        return wxEmptyString;
    return m_TokenTree->GetFilename(m_FileIdx);
}

wxString Token::GetImplFilename() const
{
    if (!m_TokenTree)
        return wxEmptyString;
    return m_TokenTree->GetFilename(m_ImplFileIdx);
}

wxString Token::GetFormattedArgs() const
{
    wxString args(m_Args);
    args.Replace("\n", wxEmptyString);
    return args;
}

wxString Token::GetStrippedArgs() const
{
    // the argument should have the format (xxxx = y, ....) or just an empty string
    // if it is empty, we just return an empty string
    if (m_Args.IsEmpty())
        return wxEmptyString;

    wxString args;
    args.Alloc(m_Args.Len() + 1);
    bool skipDefaultValue = false;
    for (size_t i = 0; i < m_Args.Len(); ++i)
    {
        const wxChar ch = m_Args[i];
        if (ch == '\n')
            continue;
        else if (ch == '=')
        {
            skipDefaultValue = true;
            args.Trim();
        }
        else if (ch == ',')
            skipDefaultValue = false;

        if (!skipDefaultValue)
            args << ch;
    }

    if (args.Last() != ')')
        args << ')';

    return args;
}

bool Token::MatchesFiles(const TokenFileSet& files)
{
    if (!files.size())
        return true;

    if (!m_FileIdx && !m_ImplFileIdx)
        return true;

    if ((m_FileIdx && files.count(m_FileIdx)) || (m_ImplFileIdx && files.count(m_ImplFileIdx)))
        return true;

    return false;
}

wxString Token::GetNamespace() const
{
    const wxString dcolon("::");
    wxString res;
    Token* parentToken = m_TokenTree->at(m_ParentIndex);
    while (parentToken)
    {
        res.Prepend(dcolon);
        res.Prepend(parentToken->m_Name);
        parentToken = m_TokenTree->at(parentToken->m_ParentIndex);
    }
    return res;
}

bool Token::AddChild(int childIdx)
{
    if (childIdx < 0)
        return false;
    m_Children.insert(childIdx);
    return true;
}

bool Token::DeleteAllChildren()
{
    if (!m_TokenTree)
        return false;
    for (;;)
    {
        TokenIdxSet::const_iterator it = m_Children.begin();
        if (it == m_Children.end())
            break;
        m_TokenTree->erase(*it);
    }
    return true;
}

bool Token::InheritsFrom(int idx) const
{
    if (idx < 0 || !m_TokenTree)
        return false;

    Token* token = m_TokenTree->at(idx);
    if (!token)
        return false;

    for (TokenIdxSet::const_iterator it = m_DirectAncestors.begin(); it != m_DirectAncestors.end(); it++)
    {
        int idx2 = *it;
        const Token* ancestor = m_TokenTree->at(idx2);
        if (!ancestor)
            continue;

        if (ancestor == token || ancestor->InheritsFrom(idx)) // ##### is this intended?
            return true;
    }
    return false;
}

wxString Token::GetTokenKindString() const
{
    switch (m_TokenKind)
    {
        case tkClass:           return "class";
        case tkNamespace:       return "namespace";
        case tkTypedef:         return "typedef";
        case tkEnum:            return "enum";
        case tkEnumerator:      return "enumerator";
        case tkFunction:        return "function";
        case tkConstructor:     return "constructor";
        case tkDestructor:      return "destructor";
        case tkMacroDef:        return "macro definition";
        case tkMacroUse:        return "macro usage";
        case tkVariable:        return "variable";
        case tkAnyContainer:    return "any container";
        case tkAnyFunction:     return "any function";
        case tkUndefined:       return "undefined";
        default:                return wxEmptyString; // tkUndefined
    }
}

wxString Token::GetTokenScopeString() const
{
    switch (m_Scope)
    {
        case tsPrivate:   return "private";
        case tsProtected: return "protected";
        case tsPublic:    return "public";
        case tsUndefined: return "undefined";
        default:          return wxEmptyString;
    }
}
