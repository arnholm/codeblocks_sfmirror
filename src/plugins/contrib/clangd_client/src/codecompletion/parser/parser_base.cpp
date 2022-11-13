#include <sdk.h>

#ifndef CB_PRECOMP
    #include "editorbase.h"
    #include <wx/dir.h>
#endif

#include <wx/tokenzr.h>

#include <cbstyledtextctrl.h>
#include "editormanager.h"

#include "parser_base.h"


#define CC_PARSER_BASE_DEBUG_OUTPUT 0

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_PARSER_BASE_DEBUG_OUTPUT
        #define CC_PARSER_BASE_DEBUG_OUTPUT 1
    #elif CC_PARSER_BASE_DEBUG_OUTPUT == 2
        #undef CC_PARSER_BASE_DEBUG_OUTPUT
        #define CC_PARSER_BASE_DEBUG_OUTPUT 2
    #endif
#endif

#ifdef CC_PARSER_TEST
    #define TRACE(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2_SET_FLAG(traceFile)

    // don't use locker macros in the cctest project
    #undef CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_TT_MTX_LOCK(a)
    #undef CC_LOCKER_TRACK_TT_MTX_UNLOCK
    #define CC_LOCKER_TRACK_TT_MTX_UNLOCK(a)
#else
    #if CC_PARSER_BASE_DEBUG_OUTPUT == 1
        #define TRACE(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
        #define TRACE2(format, args...)
        #define TRACE2_SET_FLAG(traceFile)
    #elif CC_PARSER_BASE_DEBUG_OUTPUT == 2
        #define TRACE(format, args...)                                              \
            do                                                                      \
            {                                                                       \
                if (g_EnableDebugTrace)                                             \
                    CCLogger::Get()->DebugLog(wxString::Format(format, ##args));                   \
            }                                                                       \
            while (false)
        #define TRACE2(format, args...) \
            CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
        #define TRACE2_SET_FLAG(traceFile) \
            g_EnableDebugTrace = !g_DebugTraceFile.IsEmpty() && traceFile.EndsWith(g_DebugTraceFile)
    #else
        #define TRACE(format, args...)
        #define TRACE2(format, args...)
        #define TRACE2_SET_FLAG(traceFile)
    #endif
#endif // CC_PARSER_TEST

// both cctest and codecompletion plugin need the FileType() function, but the former is much
// simpler, so we use a preprocess directive here
#ifdef CC_PARSER_TEST
// ----------------------------------------------------------------------------
ParserCommon::EFileType ParserCommon::FileType(const wxString& filename, bool /*force_refresh*/)
// ----------------------------------------------------------------------------
{
    static bool          empty_ext = true;
    static wxArrayString header_ext;
    header_ext.Add(_T("h")); header_ext.Add(_T("hpp")); header_ext.Add(_T("tcc")); header_ext.Add(_T("xpm"));
    static wxArrayString source_ext;
    source_ext.Add(_T("c")); source_ext.Add(_T("cpp")); source_ext.Add(_T("cxx")); source_ext.Add(_T("cc")); source_ext.Add(_T("c++"));

    if (filename.IsEmpty())
    {
        wxString log;
        log.Printf(wxT("ParserDummy::ParserCommon::FileType() : File '%s' is of type 'ftOther' (empty)."), filename.wx_str());
        //CCLogger::Get()->Log(log);
        return ParserCommon::ftOther;
    }

    const wxString file = filename.AfterLast(wxFILE_SEP_PATH).Lower();
    const int      pos  = file.Find(_T('.'), true);
    wxString       ext;
    if (pos != wxNOT_FOUND)
        ext = file.SubString(pos + 1, file.Len());

    if (empty_ext && ext.IsEmpty())
    {
        wxString log;
        log.Printf(wxT("ParserDummy::ParserCommon::FileType() : File '%s' is of type 'ftHeader' (w/o ext.)."), filename.wx_str());
        //CCLogger::Get()->Log(log);
        return ParserCommon::ftHeader;
    }

    for (size_t i=0; i<header_ext.GetCount(); ++i)
    {
        if (ext==header_ext[i])
        {
            wxString log;
            log.Printf(wxT("ParserDummy::ParserCommon::FileType() : File '%s' is of type 'ftHeader' (w/ ext.)."), filename.wx_str());
            TRACE(log);
            return ParserCommon::ftHeader;
        }
    }

    for (size_t i=0; i<source_ext.GetCount(); ++i)
    {
        if (ext==source_ext[i])
        {
            wxString log;
            log.Printf(wxT("ParserDummy::ParserCommon::FileType() : File '%s' is of type 'ftSource' (w/ ext.)."), filename.wx_str());
            TRACE(log);
            return ParserCommon::ftSource;
        }
    }

    wxString log;
    log.Printf(wxT("ParserDummy::ParserCommon::FileType() : File '%s' is of type 'ftOther' (unknown ext)."), filename.wx_str());
    TRACE(log);

    return ParserCommon::ftOther;
}
#else
// ----------------------------------------------------------------------------
ParserCommon::EFileType ParserCommon::FileType(const wxString& filename, bool force_refresh)
// ----------------------------------------------------------------------------
{
    static bool          cfg_read  = false;
    static bool          empty_ext = true;
    static wxArrayString header_ext;
    static wxArrayString source_ext;

    if (!cfg_read || force_refresh)
    {
        ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));
        empty_ext               = cfg->ReadBool(_T("/empty_ext"), true);
        wxString header_ext_str = cfg->Read(_T("/header_ext"), _T("h,hpp,hxx,hh,h++,tcc,tpp,xpm"));
        wxString source_ext_str = cfg->Read(_T("/source_ext"), _T("c,cpp,cxx,cc,c++"));

        header_ext.Clear();
        wxStringTokenizer header_ext_tknzr(header_ext_str, _T(","));
        while (header_ext_tknzr.HasMoreTokens())
            header_ext.Add(header_ext_tknzr.GetNextToken().Trim(false).Trim(true).Lower());

        source_ext.Clear();
        wxStringTokenizer source_ext_tknzr(source_ext_str, _T(","));
        while (source_ext_tknzr.HasMoreTokens())
            source_ext.Add(source_ext_tknzr.GetNextToken().Trim(false).Trim(true).Lower());

        cfg_read = true; // caching done
    }

    if (filename.IsEmpty())
        return ParserCommon::ftOther;

    const wxString file = filename.AfterLast(wxFILE_SEP_PATH).Lower();
    const int      pos  = file.Find(_T('.'), true);
    wxString       ext;
    if (pos != wxNOT_FOUND)
        ext = file.SubString(pos + 1, file.Len());

    if (empty_ext && ext.IsEmpty())
        return ParserCommon::ftHeader;

    for (size_t i=0; i<header_ext.GetCount(); ++i)
    {
        if (ext==header_ext[i])
            return ParserCommon::ftHeader;
    }

    for (size_t i=0; i<source_ext.GetCount(); ++i)
    {
        if (ext==source_ext[i])
            return ParserCommon::ftSource;
    }

    return ParserCommon::ftOther;
}
#endif //CC_PARSER_TEST
////// ----------------------------------------------------------------------------
////namespace Now defined in namespze LSP_DocumentSymbolKind
////// ----------------------------------------------------------------------------
////{
////    // ----------------------------------------------------------------------------
////    // Language Server symbol kind.
////    // ----------------------------------------------------------------------------
////    // defined in https://microsoft.github.io/language-server-protocol/specification
////    enum LSP_DocumentSymbolKind {
////        File = 1,
////        Module = 2,
////        Namespace = 3,
////        Package = 4,
////        Class = 5,
////        Method = 6,
////        Property = 7,
////        Field = 8,
////        Constructor = 9,
////        Enum = 10,
////        Interface = 11,
////        Function = 12,
////        Variable = 13,
////        Constant = 14,
////        String = 15,
////        Number = 16,
////        Boolean = 17,
////        Array = 18,
////        Object = 19,
////        Key = 20,
////        Null = 21,
////        EnumMember = 22,
////        Struct = 23,
////        Event = 24,
////        Operator = 25,
////        TypeParameter = 26
////    };
////}
// ----------------------------------------------------------------------------
ParserBase::ParserBase()
// ----------------------------------------------------------------------------
{
    m_TokenTree     = new TokenTree;
    m_TempTokenTree = new TokenTree;
    m_pLSP_Client   = nullptr;  //initialized by SetLSP_Client()

    // create Idle time CallbackHandler     //(ph 2022/08/01)
    IdleCallbackHandler* pIdleCallBackHandler = new IdleCallbackHandler();
    pIdleCallbacks.reset( pIdleCallBackHandler );
}
// ----------------------------------------------------------------------------
ParserBase::~ParserBase()
// ----------------------------------------------------------------------------
{
    //(ph 2021/11/9)
    // Locking the token tree is not necessary.
    // 1) The clangd client has already been shutdown, so no TokenTree updates.
    // 2) Since no updates to TokenTree, UpdateClassBrowserView() will not be called.
    // 3) UpdateClassBrowserview() will not access TokenTree because
    //    SetInternalParsingPaused(true) has been set.
    // ----------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)       //Lock TokenTree
    // ----------------------------------------------

    Delete(m_TokenTree);
    Delete(m_TempTokenTree);

    // CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)     //UNlock TokenTree
}
// ----------------------------------------------------------------------------
TokenTree* ParserBase::GetTokenTree() const
// ----------------------------------------------------------------------------
{
    return m_TokenTree;
}

////bool ParserBase::ParseFile(const wxString& filename, bool isGlobal, bool /*locked*/)
////{
////    return Reparse(filename, !isGlobal);
////}

////// ----------------------------------------------------------------------------
////bool ParserBase::Reparse(const wxString& file, cb_unused bool isLocal)
////// ----------------------------------------------------------------------------
////{
////    cbThrow("LSP "+wxString(__FUNCTION__)+ " Shouldn't be here!");
////    return true;
////    #warning ParserBase::Reparse is castrated @ 211
////
////    FileLoader* loader = new FileLoader(file);
////    (*loader)();
////
////    ParserThreadOptions opts;
////
////    opts.useBuffer             = false; // default
////    opts.parentIdxOfBuffer     = -1;    // default
////    opts.initLineOfBuffer      = -1;    // default
////    opts.bufferSkipBlocks      = false; // default
////    opts.bufferSkipOuterBlocks = false; // default
////    opts.isTemp                = false; // default
////
////    opts.followLocalIncludes   = true;  // default
////    opts.followGlobalIncludes  = true;  // default
////    opts.wantPreprocessor      = true;  // default
////    opts.parseComplexMacros    = true;  // default
////    opts.platformCheck         = true;  // default
////
////    opts.handleFunctions       = true;  // default
////    opts.handleVars            = true;  // default
////    opts.handleClasses         = true;  // default
////    opts.handleEnums           = true;  // default
////    opts.handleTypedefs        = true;  // default
////
////    opts.storeDocumentation    = true;  // enable this option to enable cctest for doxygen doc reading
////
////    opts.loader                = loader;
////
////    // the file should first be put in the TokenTree, so the index is correct when initializing the
////    // Tokenizer object inside the ParserThread::ParserThread()
////
////    m_TokenTree->ReserveFileForParsing(file, true);
////
////    ParserThread* pt = new ParserThread(this, file, true, opts, m_TokenTree);
////    bool success = pt->Parse();
////    delete pt;
////
////    return success;
////}


////bool ParserBase::ParseBuffer(const wxString& buffer,
////                             bool isLocal,
////                             bool bufferSkipBlocks,
////                             bool isTemp,
////                             const wxString& filename,
////                             int parentIdx,
////                             int initLine)
////{
////
////    cbThrow( wxString(__FUNCTION__) + "Should not be here");
////    //-ParserThreadOptions opts; //(ph 2021/07/27)
////    LSP_SymbolsParserOptions opts;
////
////    opts.useBuffer            = true;
////    opts.fileOfBuffer         = filename;
////    opts.parentIdxOfBuffer    = parentIdx;
////    opts.initLineOfBuffer     = initLine;
////    opts.bufferSkipBlocks     = bufferSkipBlocks;
////    opts.isTemp               = isTemp;
////
////    opts.followLocalIncludes  = true;
////    opts.followGlobalIncludes = true;
////    opts.wantPreprocessor     = m_Options.wantPreprocessor;
////    opts.parseComplexMacros   = true;
////    opts.platformCheck        = true;
////
////    opts.handleFunctions      = true;   // enabled to support function ptr in local block
////
////    opts.storeDocumentation   = m_Options.storeDocumentation;
////
////    ParserThread thread(this, buffer, isLocal, opts, m_TokenTree);
////
////    bool success = thread.Parse();
////
////    return success;
////
////}

// ----------------------------------------------------------------------------
void ParserBase::AddIncludeDir(const wxString& dir)
// ----------------------------------------------------------------------------
{
    if (dir.IsEmpty())
        return;

    wxString base = dir;
    if (base.Last() == wxFILE_SEP_PATH)
        base.RemoveLast();
    if (not wxDir::Exists(base))
    {
        TRACE(_T("ParserBase::AddIncludeDir(): Directory %s does not exist?!"), base.wx_str());
        return;
    }

    if (m_IncludeDirs.Index(base) == wxNOT_FOUND)
    {
        TRACE(_T("ParserBase::AddIncludeDir(): Adding %s"), base.wx_str());
        m_IncludeDirs.Add(base);
    }
}
// ----------------------------------------------------------------------------
wxString ParserBase::FindFirstFileInIncludeDirs(const wxString& file)
// ----------------------------------------------------------------------------
{
    wxString FirstFound = m_GlobalIncludes.GetItem(file);
    if (FirstFound.IsEmpty())
    {
        wxArrayString FoundSet = FindFileInIncludeDirs(file,true);
        if (FoundSet.GetCount())
        {
            FirstFound = UnixFilename(FoundSet[0]);
            m_GlobalIncludes.AddItem(file, FirstFound);
        }
    }
    return FirstFound;
}
// ----------------------------------------------------------------------------
wxArrayString ParserBase::FindFileInIncludeDirs(const wxString& file, bool firstonly)
// ----------------------------------------------------------------------------
{
    wxArrayString FoundSet;
    for (size_t idxSearch = 0; idxSearch < m_IncludeDirs.GetCount(); ++idxSearch)
    {
        wxString base = m_IncludeDirs[idxSearch];
        wxFileName tmp = file;
        NormalizePath(tmp,base);
        wxString fullname = tmp.GetFullPath();
        if (wxFileExists(fullname))
        {
            FoundSet.Add(fullname);
            if (firstonly)
                break;
        }
    }

    TRACE(_T("ParserBase::FindFileInIncludeDirs(): Searching %s"), file.wx_str());
    TRACE(_T("ParserBase::FindFileInIncludeDirs(): Found %lu"), static_cast<unsigned long>(FoundSet.GetCount()));

    return FoundSet;
}
// ----------------------------------------------------------------------------
wxString ParserBase::GetFullFileName(const wxString& src, const wxString& tgt, bool isGlobal)
// ----------------------------------------------------------------------------
{
    wxString fullname;
    if (isGlobal)
    {
        fullname = FindFirstFileInIncludeDirs(tgt);
        if (fullname.IsEmpty())
        {
            // not found; check this case:
            //
            // we had entered the previous file like this: #include <gl/gl.h>
            // and it now does this: #include "glext.h"
            // glext.h was correctly not found above but we can now search
            // for gl/glext.h.
            // if we still not find it, it's not there. A compilation error
            // is imminent (well, almost - I guess the compiler knows a little better ;).
            wxString base = wxFileName(src).GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
            fullname = FindFirstFileInIncludeDirs(base + tgt);
        }
    }

    // NOTE: isGlobal is always true. The following code never executes...

    else // local files are more tricky, since they depend on two filenames
    {
        wxFileName fname(tgt);
        wxFileName source(src);
        if (NormalizePath(fname,source.GetPath(wxPATH_GET_VOLUME)))
        {
            fullname = fname.GetFullPath();
            if (!wxFileExists(fullname))
                fullname.Clear();
        }
    }

    return fullname;
}
// ----------------------------------------------------------------------------
size_t ParserBase::FindTokensInFile(bool hasTokenTreeLock, const wxString& filename, TokenIdxSet& result, short int kindMask)
// ----------------------------------------------------------------------------
{
    result.clear();
    size_t tokens_found = 0;

    // Caller must obtain the TokenTree lock before calling this routine.
    cbAssert(hasTokenTreeLock and "Caller must own TokenTree lock");

////    TRACE(_T("Parser::FindTokensInFile() : Searching for file '%s' in tokens tree..."), filename.wx_str());

    TokenIdxSet tmpresult;
    if ( m_TokenTree->FindTokensInFile(filename, tmpresult, kindMask) )
    {
        for (TokenIdxSet::const_iterator it = tmpresult.begin(); it != tmpresult.end(); ++it)
        {
            const Token* token = m_TokenTree->at(*it);
            if (token)
                result.insert(*it);
        }
        tokens_found = result.size();
    }

    return tokens_found;
}
// ----------------------------------------------------------------------------
Token* ParserBase::GetTokenInFile(wxString filename, wxString requestedDisplayName, bool callerHasLock)
// ----------------------------------------------------------------------------
{
    // Get a specific token from a specified file.
    // This is called from OnLSPCompletionResponse to get the token index
    // If the lock fails, the token will be skipped.

    TokenTree* tree = nullptr;
    Token* pFoundToken = nullptr;


    // -----------------------------------------------------
    //CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // -----------------------------------------------------
    auto locker_result = callerHasLock ? wxMUTEX_NO_ERROR : s_TokenTreeMutex.LockTimeout(250);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread
        // Note: This function called from a UI/clangd event, so it's extremely unlikely that
        // any other thread has the lock, unless the user is fast enough
        // to refresh the ClassBrowser while this event is running.
        wxString msg = wxString::Format("Error: Lock failed: %s", __FUNCTION__);
        Manager::Get()->GetLogManager()->DebugLog(msg);
        return nullptr;
    }
    else /*lock succeeded*/
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/

    tree = GetTokenTree();
    if ( (not tree) or tree->empty())
    {
        if (not callerHasLock)
            // ---------------------------------------------
            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex) //UNlock TokenTree
            // ---------------------------------------------
       return nullptr;
    }
    else
    {
        // Search in filenames (both headers and implementations)
        wxFileName fnFilename = filename;
        fnFilename.SetExt("");
        wxString edFilename = fnFilename.GetFullPath();
        edFilename.Replace('\\','/');
        for (size_t i = 0; i < tree->size(); i++)
        {
            Token* pToken = tree->at(i);
            if (not pToken) continue;                           //(ph 2021/10/27)
            //-bool isImpl = ParserCommon::FileType(edFilename) == ParserCommon::ftSource;
            wxString tokenFilename = pToken->GetFilename();
            if ( pToken && (not pToken->GetFilename().StartsWith(edFilename)) ) continue;      //(ph 2021/05/22)
            if ( pToken && (pToken->m_TokenKind & tkUndefined) )
            {
                // Do we need to clone the internal data of the strings to make them thread safe?
                wxString token_m_Name = wxString(pToken->m_Name.c_str());
                if (not requestedDisplayName.StartsWith(token_m_Name)) continue;
                wxString displayName = wxString(pToken->DisplayName().c_str());
                if (not displayName.Contains(requestedDisplayName)) continue;
                pFoundToken = pToken;
                break;
            }//endif
        }//endfor

        if (not callerHasLock)
            // -------------------------------------------------
            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)     //UNlock TokenTree
            // -------------------------------------------------

    }//end else

    return pFoundToken;
}
// ----------------------------------------------------------------------------
TokenKind ParserBase::ConvertLSPSymbolKindToCCTokenKind(int docSymKind)
// ----------------------------------------------------------------------------
{

    /// FIXME (ph#): the following ccTokenKind(s) may not be correct //(ph 2021/03/22)
    TokenKind ccTokenKind = tkUndefined;

    switch(docSymKind)
    {
        case LSP_DocumentSymbolKind::File:      ccTokenKind = tkUndefined;  break;
        case LSP_DocumentSymbolKind::Module:    ccTokenKind = tkUndefined;  break;
        case LSP_DocumentSymbolKind::Namespace: ccTokenKind = tkNamespace;  break;
        case LSP_DocumentSymbolKind::Package:   ccTokenKind = tkUndefined;  break;
        case LSP_DocumentSymbolKind::Class:     ccTokenKind = tkClass;      break;
        case LSP_DocumentSymbolKind::Method:    ccTokenKind = tkFunction;   break;
        case LSP_DocumentSymbolKind::Property:  ccTokenKind = tkVariable;   break;
        case LSP_DocumentSymbolKind::Field:     ccTokenKind = tkVariable;   break;
        case LSP_DocumentSymbolKind::Constructor:ccTokenKind = tkConstructor;break;
        case LSP_DocumentSymbolKind::Enum:      ccTokenKind = tkEnum;       break;
        case LSP_DocumentSymbolKind::Interface: ccTokenKind = tkClass;      break;
        case LSP_DocumentSymbolKind::Function:  ccTokenKind = tkFunction;   break;
        case LSP_DocumentSymbolKind::Variable:  ccTokenKind = tkVariable;   break;
        case LSP_DocumentSymbolKind::Constant:  ccTokenKind = tkVariable;   break;
        case LSP_DocumentSymbolKind::String:    ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::Number:    ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::Boolean:   ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::Array:     ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::Object:    ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::Key:       ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::Null :     ccTokenKind = tkTypedef;    break;
        case LSP_DocumentSymbolKind::EnumMember:ccTokenKind = tkEnumerator; break;
        case LSP_DocumentSymbolKind::Struct:    ccTokenKind = tkClass;      break;
        case LSP_DocumentSymbolKind::Event:     ccTokenKind = tkUndefined;  break;
        case LSP_DocumentSymbolKind::Operator:  ccTokenKind = tkUndefined;  break;

    }//endswitch
    return ccTokenKind;
}
// ----------------------------------------------------------------------------
int ParserBase::ConvertLSPCompletionSymbolKindToSemanticTokenType(int lspSymKind)   //(ph 2022/06/12)
// ----------------------------------------------------------------------------
{
    int semTknType = LSP_SemanticTokenType::Unknown;

    switch(lspSymKind)
    {
        case LSP_CompletionSymbolKind::Text:         semTknType = LSP_SemanticTokenType::Comment;     break;
        case LSP_CompletionSymbolKind::Method:       semTknType = LSP_SemanticTokenType::Method;      break;
        case LSP_CompletionSymbolKind::Function:     semTknType = LSP_SemanticTokenType::Function;    break;
        case LSP_CompletionSymbolKind::Constructor:  semTknType = LSP_SemanticTokenType::Function;    break;
        case LSP_CompletionSymbolKind::Field:        semTknType = LSP_SemanticTokenType::Variable;    break;
        case LSP_CompletionSymbolKind::Variable:     semTknType = LSP_SemanticTokenType::Variable+1;    break;
        case LSP_CompletionSymbolKind::Class:        semTknType = LSP_SemanticTokenType::Class;       break;
        case LSP_CompletionSymbolKind::Interface:    semTknType = LSP_SemanticTokenType::Interface;   break;
        case LSP_CompletionSymbolKind::Module:       semTknType = LSP_SemanticTokenType::Namespace;   break; //not right
        case LSP_CompletionSymbolKind::Property:     semTknType = LSP_SemanticTokenType::Property;    break;
        case LSP_CompletionSymbolKind::Unit:         semTknType = LSP_SemanticTokenType::Unknown;     break;
        case LSP_CompletionSymbolKind::Value:        semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Enum:         semTknType = LSP_SemanticTokenType::Enum;        break;
        case LSP_CompletionSymbolKind::Keyword:      semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as SemanticType
        case LSP_CompletionSymbolKind::Snippet:      semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Color:        semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::File:         semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Reference:    semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Folder:       semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::EnumMember:   semTknType = LSP_SemanticTokenType::EnumMember;  break;
        case LSP_CompletionSymbolKind::Constant:     semTknType = LSP_SemanticTokenType::Variable;    break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Struct:       semTknType = LSP_SemanticTokenType::Class;       break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Event:        semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::Operator:     semTknType = LSP_SemanticTokenType::Unknown;     break; //not defined as Semantic type
        case LSP_CompletionSymbolKind::TypeParameter: semTknType = LSP_SemanticTokenType::TypeParameter; break;

        default: semTknType = LSP_SemanticTokenType::Unknown;;
    }//endswitch
    return semTknType;
}
// ----------------------------------------------------------------------------
TokenKind ParserBase::ConvertLSPSemanticTypeToCCTokenKind(int semTokenType)
// ----------------------------------------------------------------------------
{

    /// FIXME (ph#): the following ccTokenKind(s) may not be correct //(ph 2021/03/22)
    TokenKind ccTokenKind = tkUndefined;

    switch(semTokenType)
    {
        case LSP_SemanticTokenType::Variable:       ccTokenKind = tkVariable;   break;
        case LSP_SemanticTokenType::Variable_2:     ccTokenKind = tkVariable;   break;
        case LSP_SemanticTokenType::Parameter:      ccTokenKind = tkUndefined;  break;
        case LSP_SemanticTokenType::Function:       ccTokenKind = tkFunction;   break;
        case LSP_SemanticTokenType::Method:         ccTokenKind = tkFunction;   break;
        case LSP_SemanticTokenType::Function_2:     ccTokenKind = tkFunction;   break;
        case LSP_SemanticTokenType::Property:       ccTokenKind = tkUndefined;  break; //public,private/protecte ?
        case LSP_SemanticTokenType::Variable_3:     ccTokenKind = tkVariable;   break;
        case LSP_SemanticTokenType::Class:          ccTokenKind = tkClass;      break;
        case LSP_SemanticTokenType::Interface:      ccTokenKind = tkClass;      break;
        case LSP_SemanticTokenType::Enum:           ccTokenKind = tkEnum;       break;
        case LSP_SemanticTokenType::EnumMember:     ccTokenKind = tkEnum;       break;
        case LSP_SemanticTokenType::Type:           ccTokenKind = tkTypedef;    break;
        case LSP_SemanticTokenType::Type_2:         ccTokenKind = tkTypedef;    break;
        case LSP_SemanticTokenType::Unknown:        ccTokenKind = tkUndefined;  break;
        case LSP_SemanticTokenType::Namespace:      ccTokenKind = tkNamespace;  break;
        case LSP_SemanticTokenType::TypeParameter:  ccTokenKind = tkUndefined;  break;
        case LSP_SemanticTokenType::Concept:        ccTokenKind = tkUndefined;  break;
        case LSP_SemanticTokenType::Type_3:         ccTokenKind = tkTypedef;    break;
        case LSP_SemanticTokenType::Macro :         ccTokenKind = tkMacroDef;   break;
        case LSP_SemanticTokenType::Comment:         ccTokenKind = tkUndefined; break;

    }//endswitch
    return ccTokenKind;
}
