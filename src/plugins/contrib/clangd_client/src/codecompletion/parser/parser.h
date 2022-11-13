/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef PARSER_H
#define PARSER_H

#include <wx/arrstr.h>
#include <wx/event.h>
#include <wx/file.h>
#include <wx/filefn.h> // wxPathList
#include <wx/imaglist.h>
#include <wx/string.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/treectrl.h>

////#include <cbthreadpool.h>
#include <sdk_events.h>
#include <cbplugin.h>
#include "searchresultslog.h"
#include "../doxygen_parser.h"             //(ph 2022/06/15)

#include "LSP_symbolsparser.h"          //(ph 2021/07/27)
#include "parser_base.h"
#include "../parsemanager.h"            //(ph 2022/02/14)
#include "../IdleCallbackHandler.h"     //(ph 2021/09/25)
#include "LSP_SymbolKind.h"
#include "ClgdCCToken.h"                //(ph 2022/07/09)

#if defined(_WIN32)
#include "winprocess/misc/fileutils.h"  //(ph 2021/12/21)
#else
#include "fileutils.h"                 //(ph 2021/12/21)
#endif //_WIN32

// defines for the icon/resource images
#define PARSER_IMG_NONE                        -2
#define PARSER_IMG_CLASS_FOLDER                 0
#define PARSER_IMG_CLASS                        1
#define PARSER_IMG_CLASS_PRIVATE                2
#define PARSER_IMG_CLASS_PROTECTED              3
#define PARSER_IMG_CLASS_PUBLIC                 4
#define PARSER_IMG_CTOR_PRIVATE                 5
#define PARSER_IMG_CTOR_PROTECTED               6
#define PARSER_IMG_CTOR_PUBLIC                  7
#define PARSER_IMG_DTOR_PRIVATE                 8
#define PARSER_IMG_DTOR_PROTECTED               9
#define PARSER_IMG_DTOR_PUBLIC                  10
#define PARSER_IMG_FUNC_PRIVATE                 11
#define PARSER_IMG_FUNC_PROTECTED               12
#define PARSER_IMG_FUNC_PUBLIC                  13
#define PARSER_IMG_VAR_PRIVATE                  14
#define PARSER_IMG_VAR_PROTECTED                15
#define PARSER_IMG_VAR_PUBLIC                   16
#define PARSER_IMG_MACRO_DEF                    17
#define PARSER_IMG_ENUM                         18
#define PARSER_IMG_ENUM_PRIVATE                 19
#define PARSER_IMG_ENUM_PROTECTED               20
#define PARSER_IMG_ENUM_PUBLIC                  21
#define PARSER_IMG_ENUMERATOR                   22
#define PARSER_IMG_NAMESPACE                    23
#define PARSER_IMG_TYPEDEF                      24
#define PARSER_IMG_TYPEDEF_PRIVATE              25
#define PARSER_IMG_TYPEDEF_PROTECTED            26
#define PARSER_IMG_TYPEDEF_PUBLIC               27
#define PARSER_IMG_SYMBOLS_FOLDER               28
#define PARSER_IMG_VARS_FOLDER                  29
#define PARSER_IMG_FUNCS_FOLDER                 30
#define PARSER_IMG_ENUMS_FOLDER                 31
#define PARSER_IMG_MACRO_DEF_FOLDER             32
#define PARSER_IMG_OTHERS_FOLDER                33
#define PARSER_IMG_TYPEDEF_FOLDER               34
#define PARSER_IMG_MACRO_USE                    35
#define PARSER_IMG_MACRO_USE_PRIVATE            36
#define PARSER_IMG_MACRO_USE_PROTECTED          37
#define PARSER_IMG_MACRO_USE_PUBLIC             38
#define PARSER_IMG_MACRO_USE_FOLDER             39

#define PARSER_IMG_MIN PARSER_IMG_CLASS_FOLDER
#define PARSER_IMG_MAX PARSER_IMG_MACRO_USE_FOLDER

extern wxMutex s_ParserMutex;

/** Tree data associate with the symbol tree item */
// ----------------------------------------------------------------------------
class ClassTreeData : public wxTreeItemData
// ----------------------------------------------------------------------------
{
public:
    ClassTreeData(Token* token)   { m_Token = token; }
    Token* GetToken()             { return m_Token;  }
    void   SetToken(Token* token) { m_Token = token; }
private:
    Token* m_Token;
};

class ClassBrowser;
class cbStyledTextCtrl; //(ph 2021/04/10)
class ParseManager;     //(ph 2021/08/20)

// ----------------------------------------------------------------------------
namespace ParserCommon
// ----------------------------------------------------------------------------
{
    enum ParserState
    {
        /** the Parser object is newly created, and we are parsing the predefined macro buffer, the
         * source files, and finally mark the project's tokens as local
         */
        ptCreateParser    = 1,

        /** some files are changed by the user, so we are parsing the changed files */
        ptReparseFile     = 2,

        /** the user has add some files to the cbproject, so we are parsing the new added files */
        ptAddFileToParser = 3,

        /** non of the above three status, this means our Parser has finish all the jobs, and it is
         * in idle mode
         */
        ptUndefined       = 4
    };
}

/** @brief Parser class holds all the tokens of a C::B project
  *
  * Parser class contains the TokenTree which is a trie structure to record the token information.
  * For details about trie, see http://en.wikipedia.org/wiki/Trie
  */

// ----------------------------------------------------------------------------
class Parser : public ParserBase
// ----------------------------------------------------------------------------
{
    friend class LSP_SymbolsParser;

public:
    /** constructor
     * @param parent which is actually a ParseManager object
     * @param project the C::B project associated with the current Parser
     */
    Parser(ParseManager* pParseManager, cbProject* project);
    /** destructor */
    ~Parser() override;

    /** Add files to batch parse mode, internally. The files will be parsed sequentially.
     * @param filenames input files name array
     */
    void AddBatchParse(const StringList& filenames) override;

    /** Add one file to Batch mode Parsing, this is the bridge between the main thread and the
     * thread pool, after this function call, the file(Parserthread) will be run from the thread
     * pool.
     * @param filenames input file name
     */
    void AddParse(const wxString& filename) override;

    /** Add one file to json mode Parsing
     * @param filenames input file name
     * @param json containing LSP symbols
     */
    void LSP_ParseDocumentSymbols(wxCommandEvent& event);
    void LSP_ParseSemanticTokens(wxCommandEvent& event);                  //(ph 2021/03/17)
    void LSP_OnClientInitialized(cbProject* pProject);                    //(ph 2021/11/11)
    bool IsOkToUpdateClassBrowserView();

    #define SYMBOL_NAME 0  //string
    #define SYMBOL_TYPE 1  //LSP_SymbolKind
    #define SYMBOL_LINE_NUMBER 2 //Line number in editor
    typedef std::tuple<std::string,LSP_SymbolKind,int> LSP_SymbolsTupleType; //fileOpenInServer, editorPosn, editor is ready, editor is modified
    const LSP_SymbolsTupleType emptySymbolsTuple = LSP_SymbolsTupleType("",Null,-1);
    bool  LSP_GetSymbolsByType(json* pJson, std::set<LSP_SymbolKind>& symbolset, std::vector<LSP_SymbolsTupleType>& LSP_VectorOfSymbolsFound);
    //-void  WalkDocumentSymbols(json& jref, wxString& filename, size_t level);
    void  WalkDocumentSymbols(json& jref, wxString& filename, int& nextVectorSlot, std::set<LSP_SymbolKind>& symbolset, std::vector<LSP_SymbolsTupleType>& LSP_VectorOfSymbolsFound);

    /** clears the list of predefined macros after it has been parsed */
    virtual void ClearPredefinedMacros();

    /** return the predefined macro definition string that has been collected */
    const wxString GetPredefinedMacros() const override;

    /** set the associated C::B project pointer. (only used by one parser for whole workspace)
     *  @return true if it can do the switch, other wise, return false, and print some debug logs.
     */
    bool UpdateParsingProject(cbProject* project) override;

    /** this usually happens when user adds some files to an existing project, it just use AddParse()
     * function internally to add the file. and switch the ParserState to ParserCommon::ptAddFileToParser.
     */
    bool AddFile(const wxString& filename, cbProject* project, bool isLocal = true) override;

    /** this usually happens when the user removes a file from the existing project, it will remove
     * all the tokens belonging to the file.
     */
    void RemoveFile(const wxString& filename) override;

    /** check to see a file is parsed already, it first checks the TokenTree to see whether it has
     * the specified file, but if a file is already queued (put in m_BatchParseFiles), we regard it
     * as already parsed.
     */
    bool IsFileParsed(const wxString& filename) override;
    void SetFileParsed(wxString filename) {m_FilesParsed.insert(filename);}

    /** check to see whether Parser is in Idle mode, there is no work need to be done in the Parser*/
    bool Done() override;

    /** if the Parser is not in Idle mode, show which need to be done */
    wxString NotDoneReason() override;

    ParseManager* GetParseManager(){return m_pParseManager;}    //(ph 2021/08/20)

////    IdleCallbackHandler* GetIdleCallbackHandler()               //(ph 2022/02/14)
////    {
////        cbAssert(GetParseManager()->GetIdleCallbackHandler());
////        return GetParseManager()->GetIdleCallbackHandler();
////    }

    bool GetIsShuttingDown()                                    //(ph 2022/07/30)
    {
        ParseManager* pParseMgr = GetParseManager();
        if (not pParseMgr) return true;
        if (pParseMgr->GetPluginIsShuttingDown())
            return true;
        return false;
    }
    void RequestSemanticTokens(cbEditor* pEditor);

    //(ph 2021/10/23)
    void OnLSP_ReferencesResponse(wxCommandEvent& event);
    void OnLSP_DeclDefResponse(wxCommandEvent& event);
    void OnLSP_RequestedSymbolsResponse(wxCommandEvent& event);               //(ph 2021/03/12)
    void OnLSP_RequestedSemanticTokensResponse(wxCommandEvent& event);  //(ph 2022/06/8)
    void OnLSP_CompletionResponse(wxCommandEvent& event, std::vector<ClgdCCToken>& v_completionTokens); //(ph 2021/10/31) //(ph 2022/07/09)
    void OnLSP_DiagnosticsResponse(wxCommandEvent& event);
    void OnLSP_HoverResponse(wxCommandEvent& event, std::vector<ClgdCCToken>& v_HoverTokens, int n_hoverLastPosition);
    void OnLSP_SignatureHelpResponse(wxCommandEvent& event, std::vector<cbCodeCompletionPlugin::CCCallTip>& v_SignatureTokens, int n_HoverLastPosition );
    void OnLSP_RenameResponse(wxCommandEvent& event);
    void OnLSP_GoToPrevFunctionResponse(wxCommandEvent& event);
    void OnLSP_GoToNextFunctionResponse(wxCommandEvent& event);
    void OnLSP_GoToFunctionResponse(wxCommandEvent& event); //unused
    void OnLSP_CompletionPopupHoverResponse(wxCommandEvent& event); //(ph 2022/06/15)

    wxString GetCompletionPopupDocumentation(const ClgdCCToken& token);
    int      FindSemanticTokenEntryFromCompletion( cbCodeCompletionPlugin::CCToken& cctoken, int completionTokenKind);

    // Called from ClgdCompletion when debugger starts and finishes //(ph 2022/07/16)
    void OnDebuggerStarting(CodeBlocksEvent& event);
    void OnDebuggerFinished(CodeBlocksEvent& event);

    FileUtils fileUtils;

protected:

    /** A timer is used to optimized the event handling for parsing, e.g. several files/projects
     * were added to the project, so we don't start the real parsing stage until the last
     * file/project was added,
     */
    void OnLSP_BatchTimer(wxTimerEvent& event);     //(ph 2021/04/10)

    /** read Parser options from configure file */
    void ReadOptions() override;
    /** write Parse options to configure file */
    void WriteOptions() override;

private:

    /** connect event handlers of the timers and thread pool */
    void ConnectEvents();

    /** connect event handlers of the timers and thread pool */
    void DisconnectEvents();

    /** when initialized, this variable will be an instance of a ParseManager */
    ParseManager* m_pParseManager;

    /** referring to the C::B cbp project currently parsing in non-project owned files */
    cbProject*                m_ProxyProject;
    /** referring to the C::B cbp project currently parsing owned project files */
    cbProject*                m_ParsersProject;

    bool m_DebuggerRunning = false;

private:

    /** a timer to delay the operation of batch parsing, see OnBatchTimer() member function as a
     * reference
     */
    wxTimer                   m_BatchTimer;

    /** All other batch parse files, like the normal headers/sources */
    StringList                m_BatchParseFiles;

    /** Pre-defined macros, its a buffer queried from the compiler command line */
    wxString                  m_PredefinedMacros;
    wxString                  m_LastPredefinedMacros; // for debugging

    /** indicated the current state the parser */
    ParserCommon::ParserState m_ParserState;

    // ----------------------------------------------------------------------------
    // LSP
    // ----------------------------------------------------------------------------
    bool m_LSP_ParserDone;
    cbStyledTextCtrl* GetNewHiddenEditor(const wxString& filename);             //(ph 2021/04/10)

    int  m_cfg_parallel_processes;
    int  m_cfg_max_parsers_while_compiling;
    std::set<wxString> m_FilesParsed; // files parsed by clangd parser      //(ph 2021/10/14)

    //(ph 2021/10/23)
    wxArrayString* m_pReferenceValues = nullptr;
    //-int reportedBadFileReference = 0;
    wxArrayString m_ReportedBadFileReferences; //filenames of bad references
    wxString m_LogFileBase = wxString();

  public:
    size_t GetFilesRemainingToParse()
        { return m_BatchParseFiles.size(); }

    bool GetUserParsingPaused()
        {   if (PauseParsingExists("UserPausedParsing")
                and PauseParsingCount("UserPausedParsing") )
                return true;
            return false;
        }
    void SetUserParsingPaused(bool newStatus)
        {
            PauseParsingForReason("UserPausedParsing", newStatus);
        }

    /** stops the batch parse timer and clears the list of waiting files to be parsed */
    void ClearBatchParse();

  private:
    // map of reasons to pause parsing <reason, count>
    typedef std::map<wxString, int> PauseReasonType;
    PauseReasonType m_PauseParsingMap; //map of pauseReason and count
    //std::vector<cbCodeCompletionPlugin::CCToken> m_vHoverTokens;
    wxString m_HoverCompletionString;
    cbCodeCompletionPlugin::CCToken m_HoverCCTokenPending = {-1,"", "", -1, -1};
    /** Provider of documentation for the popup window */
    DocumentationHelper m_DocHelper;

  public:
    // ----------------------------------------------------------------------------
    int PauseParsingCount()
    // ----------------------------------------------------------------------------
    {
        if (not m_PauseParsingMap.size())
            return 0;
        int pauseCounts = 0;
        for (PauseReasonType::iterator it = m_PauseParsingMap.begin(); it != m_PauseParsingMap.end(); ++it)
            pauseCounts += it->second;
        return pauseCounts;
    }

    // ----------------------------------------------------------------------------
    int PauseParsingCount(wxString reason)
    // ----------------------------------------------------------------------------
    {
        wxString the_reason = reason.MakeLower();
        if ( m_PauseParsingMap.find(the_reason) == m_PauseParsingMap.end() )
        {
            return 0;
        }
        return m_PauseParsingMap[the_reason];
    }

    // ----------------------------------------------------------------------------
    bool PauseParsingExists(wxString reason)
    // ----------------------------------------------------------------------------
    {
        wxString the_reason = reason.MakeLower();
        if ( m_PauseParsingMap.find(the_reason) == m_PauseParsingMap.end() )
            return false;
        return true;
    }
    // ----------------------------------------------------------------------------
    bool PauseParsingForReason(wxString reason, bool increment)
    // ----------------------------------------------------------------------------
    {
        //wxString the_project = m_Project->GetTitle();
        wxString the_project = GetParsersProject()->GetTitle();
        wxString the_reason = reason.MakeLower();
        if (PauseParsingExists(the_reason) and increment)
        {   ++m_PauseParsingMap[the_reason];
            wxString reasonMsg = wxString::Format("Pausing parser(%s) for reason %s(%d)", the_project, reason, m_PauseParsingMap[the_reason]);
            CCLogger::Get()->DebugLog(reasonMsg);
            return true;
        }
        else  if (increment) //doesnt exist and increment, create it
        {   m_PauseParsingMap[the_reason] = 1;
            CCLogger::Get()->DebugLog(wxString::Format("Pausing parser(%s) for %s", the_project, reason));
            return true;
        }
        else if (not PauseParsingExists(the_reason) and (increment==false))
        {    //decrement but doesnt exist, is an error
            #if defined (cbDEBUG)
            wxString msg(wxString::Format("%s() line:%d", __FUNCTION__, __LINE__));
            msg += wxString::Format("\nReason param(%s) does not exist", the_reason);
            cbMessageBox(msg, "Assert(non fatal)");
            #endif
            CCLogger::Get()->DebugLogError(wxString::Format("PauseParsing request Error:%s", reason));
            return false;
        }
        else
        {   // decrement the pause reason
            --m_PauseParsingMap[the_reason];
            wxString reasonMsg = wxString::Format("Un-pausing parser(%s) for reason: %s(%d)", the_project, reason, m_PauseParsingMap[the_reason]);
            CCLogger::Get()->DebugLog(reasonMsg);
            if (m_PauseParsingMap[the_reason] < 0)
            {
                CCLogger::Get()->DebugLogError("Un-pausing parser count below zero for reason: " + reason);
                m_PauseParsingMap[the_reason] = 0;
            }
            return true;
        }
        return false;
    }
    // ----------------------------------------------------------------------------
    size_t GetArrayOfPauseParsingReasons(wxArrayString& aryReasons)
    // ----------------------------------------------------------------------------
    {
        if  (0 == PauseParsingCount()) return 0;
        size_t pauseCounts = 0;
        for (PauseReasonType::iterator it = m_PauseParsingMap.begin(); it != m_PauseParsingMap.end(); ++it)
        {
            aryReasons.Add(it->first);
            pauseCounts ++;
        }
        return pauseCounts;
    }

    /** Remember and return the base dir for the SearchLog */
    void SetLogFileBase(wxString filebase){m_LogFileBase = filebase;}
    wxString GetLogFileBase(){return m_LogFileBase;}

    wxString GetLineTextFromFile(const wxString& file, const int lineNum); //(ph 2020/10/26)
    bool FindDuplicateEntry(wxArrayString* pArray, wxString fullPath, wxString& lineNum, wxString& text); //(ph 2021/02/1)

    // ----------------------------------------------------------------
    inline int GetCaretPosition(cbEditor* pEditor)
    // ----------------------------------------------------------------
    {
        if (not pEditor) return 0;
        cbStyledTextCtrl* pCntl = pEditor->GetControl();
        if (not pCntl) return 0;
        return pCntl->GetCurrentPos();
    }

    cbProject* GetProxyProject()   {return m_ProxyProject;}
    cbProject* GetParsersProject() {return m_ParsersProject;}

    wxString GetwxUTF8Str(const std::string stdString)
    {
        return wxString(stdString.c_str(), wxConvUTF8);
    }
};

#endif // PARSER_H
