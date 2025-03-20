#ifndef PARSER_BASE_H
#define PARSER_BASE_H


#include <wx/arrstr.h>
#include <wx/event.h>
#include <wx/file.h>
#include <wx/filefn.h> // wxPathList
#include <wx/imaglist.h>
#include <wx/string.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/treectrl.h>

//unused #include <set>
//unused #include <list>

//unused #include "configmanager.h"

#include "json.hpp" //nlohmann json lib
#include "../IdleCallbackHandler.h"
#include "LSP_symbolsparser.h"
#include "prep.h" //cb_unused
#include "cbproject.h"
#include "tokentree.h"
#include "ParserCommon.h"

using json = nlohmann::json;
class ProcessLanguageClient;

// ----------------------------------------------------------------------------
// Definitions from old parserthread.h
// ----------------------------------------------------------------------------
struct NameSpace
{
    wxString Name;  // namespace's name
    int StartLine;  // namespace start line (the line contains openbrace)
    int EndLine;    // namespace end line (the line contains closebrace)
};
typedef std::vector<NameSpace> NameSpaceVec;

// ----------------------------------------------------------------------------
// Language Server symbol kinds.
// ----------------------------------------------------------------------------
// defined in https://microsoft.github.io/language-server-protocol/specification
#define UNUSED __attribute__((__unused__))
// ----------------------------------------------------------------------------
namespace LSP_DocumentSymbolKind //LSP definitions for textDocument/documentSymbol response
// ----------------------------------------------------------------------------
{
	UNUSED const int File       = 1;
	UNUSED const int Module     = 2;
	UNUSED const int Namespace  = 3;
	UNUSED const int Package    = 4;
	UNUSED const int Class      = 5;
	UNUSED const int Method     = 6;
	UNUSED const int Property   = 7;
	UNUSED const int Field      = 8;
	UNUSED const int Constructor = 9;
	UNUSED const int Enum       = 10;
	UNUSED const int Interface  = 11;
	UNUSED const int Function   = 12;
	UNUSED const int Variable   = 13;
	UNUSED const int Constant   = 14;
	UNUSED const int String     = 15;
	UNUSED const int Number     = 16;
	UNUSED const int Boolean    = 17;
	UNUSED const int Array      = 18;
	UNUSED const int Object     = 19;
	UNUSED const int Key        = 20;
	UNUSED const int Null       = 21;
	UNUSED const int EnumMember = 22;
	UNUSED const int Struct     = 23;
	UNUSED const int Event      = 24;
	UNUSED const int Operator   = 25;
	UNUSED const int TypeParameter = 26;
}
// ----------------------------------------------------------------------------
namespace LSP_CompletionSymbolKind  //LSP defintions for textDocument/completion response
    // ----------------------------------------------------------------------------
{
    UNUSED const int Text        = 1;
    UNUSED const int Method      = 2;
    UNUSED const int Function    = 3;
    UNUSED const int Constructor = 4;
    UNUSED const int Field       = 5;
    UNUSED const int Variable    = 6;
    UNUSED const int Class       = 7;
    UNUSED const int Interface   = 8;
    UNUSED const int Module      = 9;
    UNUSED const int Property    = 10;
    UNUSED const int Unit        = 11;
    UNUSED const int Value       = 12;
    UNUSED const int Enum        = 13;
    UNUSED const int Keyword     = 14;
    UNUSED const int Snippet     = 15;
    UNUSED const int Color       = 16;
    UNUSED const int File        = 17;
    UNUSED const int Reference   = 18;
    UNUSED const int Folder      = 19;
    UNUSED const int EnumMember  = 20;
    UNUSED const int Constant    = 21;
    UNUSED const int Struct      = 22;
    UNUSED const int Event       = 23;
    UNUSED const int Operator    = 24;
    UNUSED const int TypeParameter = 25;
}
// ----------------------------------------------------------------------------
namespace LSP_SemanticTokenType //LSP definitions for textDocument/documentSymbol response
// ----------------------------------------------------------------------------
{
	UNUSED const int Variable       = 0;
	UNUSED const int Variable_2     = 1;
	UNUSED const int Parameter      = 2;
	UNUSED const int Function       = 3;
	UNUSED const int Method         = 4;
	UNUSED const int Function_2     = 5;
	UNUSED const int Property       = 6;
	UNUSED const int Variable_3     = 7;
	UNUSED const int Class          = 8;
	UNUSED const int Interface      = 9;
	UNUSED const int Enum           = 10;
	UNUSED const int EnumMember     = 11;
	UNUSED const int Type           = 12;
	UNUSED const int Type_2         = 13;
	UNUSED const int Unknown        = 14;
	UNUSED const int Namespace      = 15;
	UNUSED const int TypeParameter  = 16;
	UNUSED const int Concept        = 17;
	UNUSED const int Type_3         = 18;
	UNUSED const int Macro          = 19;
	UNUSED const int Comment        = 20;

}
// ----------------------------------------------------------------------------
namespace LSP_SemanticTokenModifier //LSP definitions for textDocument/documentSymbol response
// ----------------------------------------------------------------------------
{
	UNUSED const int Declaration    = 1;
	UNUSED const int Deprecated     = 2;
	UNUSED const int Deduced        = 3;
	UNUSED const int Readonly       = 4;
	UNUSED const int Static         = 5;
	UNUSED const int Abstract       = 6;
	UNUSED const int DependentName  = 7;
	UNUSED const int DefaultLibrary = 8;
	UNUSED const int FunctionScope  = 9;
	UNUSED const int ClassScope     = 10;
	UNUSED const int FileScope      = 11;
	UNUSED const int GlobalScope    = 12;
}


// both the CodeCompletion plugin and the cc_test project share this class, this class holds a Token
// Tree.
// ----------------------------------------------------------------------------
class ParserBase : public wxEvtHandler
// ----------------------------------------------------------------------------
{
    //    friend class ParserThread;
    friend class LSP_SymbolsParser;

public:
    ParserBase();
    virtual ~ParserBase();

    virtual void AddBatchParse(cb_unused const StringList& filenames)           { ; }
    virtual void AddParse(cb_unused const wxString& filename)                   { ; }
    virtual bool UpdateParsingProject(cb_unused cbProject* project)             { return false; }

    virtual bool AddFile(cb_unused const wxString& filename, cb_unused cbProject* project, cb_unused bool isLocal = true) { return false; }
    virtual void RemoveFile(cb_unused const wxString& filename) { return; }
    virtual bool IsFileParsed(cb_unused const wxString& filename) { return false; }

    virtual bool     Done()          { return true; }
    virtual wxString NotDoneReason() { return wxEmptyString; }

    virtual TokenTree* GetTokenTree() const; // allow other implementations of derived (dummy) classes
    TokenTree* GetTempTokenTree()    { return m_TempTokenTree; } // -unused-

    virtual const wxString GetPredefinedMacros() const { return wxEmptyString; } // allow other implementations of derived (dummy) classes

    /** add a directory to the Parser's include path database */
    void                 AddIncludeDir(const wxString& dir);
    const wxArrayString& GetIncludeDirs() const { return m_IncludeDirs; }
    wxString             GetFullFileName(const wxString& src, const wxString& tgt, bool isGlobal);

    /** it mimics what a compiler does to find an include header files, if the firstonly option is
     * true, it will return the first found header file, otherwise, the complete database of the
     * Parser's include paths will be searched.
     */
    wxArrayString   FindFileInIncludeDirs(const wxString& file, bool firstonly = false);

    /** read Parser options from configure file */
    virtual void            ReadOptions() {}
    /** write Parse options to configure file */
    virtual void            WriteOptions(bool classBrowserOnly=false) {} //(svn 13612 bkport)
    // make them virtual, so Parser class can overwrite them!
    virtual ParserOptions&  Options()             { return m_Options;        }
    virtual BrowserOptions& ClassBrowserOptions() { return m_BrowserOptions; }

    /** Get tokens from the token tree associated with this filename
      * Caller must own TokenTree Lock before calling this function
      */
    size_t FindTokensInFile(bool callerHasTreeLock, const wxString& filename, TokenIdxSet& result, short int kindMask);
    /** Get a token in specific filename by token name
      */
    Token* GetTokenInFile(wxString filename, wxString tokenDisplayName, bool callerHasLock);

private:
    wxString FindFirstFileInIncludeDirs(const wxString& file);

protected:
    /** each Parser class contains a TokenTree object which is used to record tokens per project
      * this tree will be created in the constructor and destroyed in destructor.
      */
    TokenTree*           m_TokenTree;

    /** a temp Token tree hold some temporary tokens, e.g. parsing a buffer containing some
      * preprocessor directives, see ParseBufferForFunctions() like functions
      * this tree will be created in the constructor and destroyed in destructor.
      */
    TokenTree*           m_TempTokenTree;

    /** options for how the parser try to parse files */
    ParserOptions        m_Options;
    ParserOptions        m_OptionsSaved;

    /** options for how the symbol browser was shown */
    BrowserOptions       m_BrowserOptions;
    BrowserOptions       m_BrowserOptionsSaved;

private:
    /** wxString -> wxString map */
    SearchTree<wxString> m_GlobalIncludes;

    /** the include directories can be either of three kinds below:
     * 1, compiler's default search paths, e.g. E:\gcc\include
     * 2, project's common folders, e.g. the folder which contains the cbp file
     * 3, the compiler include search paths defined in the cbp, like: E:\wx2.8\msw\include
     */
    wxArrayString        m_IncludeDirs;

    // ----------------------------------------------------------------
    // LSP Parser properties
    // ----------------------------------------------------------------

    // Idle callback Handler pointer
    std::unique_ptr<IdleCallbackHandler> pIdleCallbacks;

public:

    ProcessLanguageClient* m_pLSP_Client;

    void SetLSP_Client(ProcessLanguageClient* pLSPclient) {m_pLSP_Client = pLSPclient;}
    ProcessLanguageClient* GetLSPClient() {return m_pLSP_Client;}

    // LSP legends provided during clangd initialization response
    TokenKind ConvertLSPSymbolKindToCCTokenKind(int lspSymKind);
    int       ConvertLSPCompletionSymbolKindToSemanticTokenType(int lspSymKind);
    TokenKind ConvertLSPSemanticTypeToCCTokenKind(int semTokenType);
    TokenKind ConvertLSPCompletionSymbolKindToCCTokenKind(int lspCCKind);

    std::vector<std::string> m_SemanticTokensTypes;
    std::vector<std::string> m_SemanticTokensModifiers;

    // Vector of clangd SemanticTokens for the current editor
    #define stLINENUM   0 //position of semantic token line number
    #define stCOLNUM    1 //position of semantic token col number
    #define stLENGTH    2 //position of semantic token token length
    #define stTYPE      3 //position of semantic token token type
    #define stMODIFIER  4 //position of semantic token type modifier
    #define stTOKENNAME 5 // std::string token name
    typedef std::tuple<size_t, size_t, size_t, size_t, size_t, std::string> LSP_SemanticToken;
    std::vector<LSP_SemanticToken> m_SemanticTokensVec;

    // ----------------------------------------------------------------------------
    int AddSemanticToken(LSP_SemanticToken semanticTuple)
    // ----------------------------------------------------------------------------
    {
        m_SemanticTokensVec.push_back(semanticTuple);
        return m_SemanticTokensVec.size();
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenLineNum(std::string reqName)
    // ----------------------------------------------------------------------------
    {
        for (size_t ii=0; ii<m_SemanticTokensVec.size(); ++ii)
        {
            if (std::get<stTOKENNAME>(m_SemanticTokensVec[ii]) == reqName)
                return std::get<stLINENUM>(m_SemanticTokensVec[ii]);
        }
        return -1;
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenLineNumAt(size_t idx)
    // ----------------------------------------------------------------------------
    {
        if (idx < (m_SemanticTokensVec.size()))
            return std::get<stLINENUM>(m_SemanticTokensVec[idx]);
        return -1;
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenColumnNum(std::string reqName)
    // ----------------------------------------------------------------------------
    {
        for (size_t ii=0; ii<m_SemanticTokensVec.size(); ++ii)
        {
            if (std::get<stTOKENNAME>(m_SemanticTokensVec[ii]) == reqName)
                return std::get<stCOLNUM>(m_SemanticTokensVec[ii]);
        }
        return -1;
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenColumnNumAt(size_t idx)
    // ----------------------------------------------------------------------------
    {
        if (idx < (m_SemanticTokensVec.size()))
            return std::get<stCOLNUM>(m_SemanticTokensVec[idx]);
        return -1;
    }

    // ----------------------------------------------------------------------------
    int GetSemanticTokenLength(std::string reqName)
    // ----------------------------------------------------------------------------
    {
        for (size_t ii=0; ii<m_SemanticTokensVec.size(); ++ii)
        {
            if (std::get<stTOKENNAME>(m_SemanticTokensVec[ii]) == reqName)
                return std::get<stLENGTH>(m_SemanticTokensVec[ii]);
        }
        return -1;
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenLengthAt(size_t idx)
    // ----------------------------------------------------------------------------
    {
        if (idx < (m_SemanticTokensVec.size()))
            return std::get<stLENGTH>(m_SemanticTokensVec[idx]);
        return -1;
    }

    // ----------------------------------------------------------------------------
    int GetSemanticTokenType(std::string reqName)
    // ----------------------------------------------------------------------------
    {
        for (size_t ii=0; ii<m_SemanticTokensVec.size(); ++ii)
        {
            if (std::get<stTOKENNAME>(m_SemanticTokensVec[ii]) == reqName)
                return std::get<stTYPE>(m_SemanticTokensVec[ii]);
        }
        return -1;
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenTypeAt(size_t idx)
    // ----------------------------------------------------------------------------
    {
        if (idx < (m_SemanticTokensVec.size()))
            return std::get<stTYPE>(m_SemanticTokensVec[idx]);
        return -1;
    }

    // ----------------------------------------------------------------------------
    int GetSemanticTokenModifier(std::string reqName)
    // ----------------------------------------------------------------------------
    {
        for (size_t ii=0; ii<m_SemanticTokensVec.size(); ++ii)
        {
            if (std::get<stTOKENNAME>(m_SemanticTokensVec[ii]) == reqName)
                return std::get<stMODIFIER>(m_SemanticTokensVec[ii]);
        }
        return -1;
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokenModifierAt(size_t idx)
    // ----------------------------------------------------------------------------
    {
        if (idx < (m_SemanticTokensVec.size()))
            return std::get<stMODIFIER>(m_SemanticTokensVec[idx]);
        return -1;
    }

    // ----------------------------------------------------------------------------
    std::string GetSemanticTokenNameAt(size_t idx)
    // ----------------------------------------------------------------------------
    {
        // patch 1408 svn:r13354 Thanks Martin Strunz 2023/09/15
        //-error: if (idx <= m_SemanticTokensVec.size()) out of bounds - should be idx < ...
        if (idx < m_SemanticTokensVec.size())
            return std::get<stTOKENNAME>(m_SemanticTokensVec[idx]);
        return std::string();
    }
    // ----------------------------------------------------------------------------
    int GetSemanticTokensWithName(std::string reqName, std::vector<int>& indexes)
    // ----------------------------------------------------------------------------
    {
        for (size_t ii=0; ii<m_SemanticTokensVec.size(); ++ii)
        {
            if (std::get<stTOKENNAME>(m_SemanticTokensVec[ii]) == reqName)
                indexes.push_back(ii);
        }
        return indexes.size();
    }
	// ----------------------------------------------------------------------------
	std::string GetSemanticTokenTypeString(int semType)
	// ----------------------------------------------------------------------------
	{
	    std::string semTypeString = "Unknown";
        switch(semType)
        {
            case LSP_SemanticTokenType::Variable:      semTypeString = "variable";      break;
            case LSP_SemanticTokenType::Variable_2:    semTypeString = "variable";      break;
            case LSP_SemanticTokenType::Parameter:     semTypeString = "parameter";     break;
            case LSP_SemanticTokenType::Function:      semTypeString = "function";      break;
            case LSP_SemanticTokenType::Method:        semTypeString = "method";        break;
            case LSP_SemanticTokenType::Function_2:    semTypeString = "function";      break;
            case LSP_SemanticTokenType::Property:      semTypeString = "property";      break;
            case LSP_SemanticTokenType::Variable_3:    semTypeString = "variable";      break;
            case LSP_SemanticTokenType::Class:         semTypeString = "class";         break;
            case LSP_SemanticTokenType::Interface:     semTypeString = "interface";     break;
            case LSP_SemanticTokenType::Enum:          semTypeString = "enum";          break;
            case LSP_SemanticTokenType::EnumMember:    semTypeString = "enumMember";    break;
            case LSP_SemanticTokenType::Type:          semTypeString = "type";          break;
            case LSP_SemanticTokenType::Type_2:        semTypeString = "type";          break;
            case LSP_SemanticTokenType::Unknown:       semTypeString = "unknown";       break;
            case LSP_SemanticTokenType::Namespace:     semTypeString = "namespace";     break;
            case LSP_SemanticTokenType::TypeParameter: semTypeString = "typeParameter"; break;
            case LSP_SemanticTokenType::Concept:       semTypeString = "concept";       break;
            case LSP_SemanticTokenType::Type_3:        semTypeString = "type";          break;
            case LSP_SemanticTokenType::Macro:         semTypeString = "macro";         break;
            case LSP_SemanticTokenType::Comment:       semTypeString = "comment";       break;
            default: semTypeString = "unknown";
        }//endSwitch
        return semTypeString;
	}//end GetSemanticTokenTypeString()

    // Get pointer to Idle callbacks
    IdleCallbackHandler* GetIdleCallbackHandler()
        {
            return pIdleCallbacks.get();
        }
};

#endif
