/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef ParseManager_H
#define ParseManager_H

//#include <queue>
#include <map>
#include <memory>
#include <mutex>  //(Christo 2024/03/30)
#include <unordered_map>
#include <wx/event.h>
#include <wx/aui/aui.h> //(ph 2024/01/19)

#include <cbstyledtextctrl.h> //(ph 2023/12/22)
#include "parsemanager_base.h"
//-#include "parser/parser_base.h"
#include "parser/cclogger.h"
#include "LSPEventCallbackHandler.h"
//-#include "IdleCallbackHandler.h"


/** debug only variable, used to print the AI match related log message*/
extern bool s_DebugSmartSense;

extern const int g_EditorActivatedDelay;

// forward declaration
class cbEditor;
class EditorBase;
class cbProject;
class cbStyledTextCtrl;
class ClassBrowser;
class Compiler;
class Token;

#include "parser/ParserCommon.h"
class  Parser;
class  ParserBase;
struct ParserOptions;
struct BrowserOptions;
class  IdleCallbackHandler;
class  ProjectFile;
class  ProcessLanguageClient;
class  ClgdCompletion;

////// TODO (ollydbg#1#), this class is dirty, I'm going to change its name like CursorLocation
/////** Search location combination, a pointer to cbStyledTextCtrl and a filename is enough */
struct ccSearchData
{
    cbStyledTextCtrl* control;
    wxString          file;
};

/** Symbol browser tree showing option */
enum BrowserViewMode
{
    bvmRaw = 0,
    bvmInheritance
};

/** @brief ParseManager class is just like a manager class to control Parser objects.
 *
 * Normally, Each C::B Project (cbp) will have an associated Parser object.
 * ParseManager will manage all the Parser objects.
 */
// ----------------------------------------------------------------------------
class ParseManager : public wxEvtHandler, private ParseManagerBase
// ----------------------------------------------------------------------------
{
private:
    /** Constructor */
    ParseManager(){};

public:
    /** Constructor */
    ParseManager( LSPEventCallbackHandler* pLSPEventSinkHandler );


    /** Destructor */
    ~ParseManager();

    /** return a reference to the current active Parser object */
    Parser& GetParser() { return *m_ActiveParser; }

    /** return the Parser pointer corresponding to the input C::B project
     * @param project input C::B project pointer
     * @return a pointer to Parser object
     */
    Parser* GetParserByProject(cbProject* project);

    /** return the Parser pointer associated with the input file
     * If a file belongs to several Parser objects, the first found Parser will be returned.
     * @param filename filename with full path.
     * @return a Parser pointer
     */
    Parser* GetParserByFilename(const wxString& filename);

    /** return the C::B project associated with Parser pointer
     * @param parser Parser pointer
     * @return C::B Project pointer
     */
    cbProject* GetProjectByParser(Parser* parser);

    /** return the C::B project containing the filename
     * The function first try to match the filename in the active project, next to match other
     * projects opened, If the file exists in several projects, the first matched project will be
     * returned.
     * @param filename input filename
     * @return a project pointer containing the file
     */
    cbProject* GetProjectByFilename(const wxString& filename);

    /** return the C::B project containing the cbEditor pointer
     * @param editor any valid cbEditor pointer
     * @return project pointer
     */
    cbProject* GetProjectByEditor(cbEditor* editor);

    /** Get current project by active editor or just return active project */
    cbProject* GetActiveEditorProject();

    /** Return true if one Parser per whole workspace option is selected
     *   For Clangd, this is alway false
     */
    //-bool IsParserPerWorkspace() const { return m_ParserPerWorkspace; }
    bool IsParserPerWorkspace() const { return false; }

    /** Return true if all the Parser's batch-parse stages are finished, otherwise return false */
    bool Done();

    /** Provides images for the Symbol browser (for tree node images) and AutoCompletion list.
     *  @param maxSize Maximum size that will fit in the UI.
     */
    wxImageList* GetImageList(int maxSize);

    /** Returns the image assigned to a specific token for a symbol browser */
    int GetTokenKindImage(const Token* token);

    /** Returns the image assigned to a specific token from Clangd Semantic Token type */
    int GetTokenImageFromSemanticTokenType(Token* token);

    /** Get the implementation file path if the input is a header file. or Get the header file path
     * if the input is an implementation file.
     * Both the implementation file and header file can be in different directories.
     * Caution: this func only returns dir paths, not full file paths.
     * @param filename input filename
     * @return corresponding file paths, in wxArrayString format
     */
    wxArrayString GetAllPathsByFilename(const wxString& filename);

    wxString GetHeaderForSourceFile(cbProject* pProject, wxString& filename);
    wxString GetSourceForHeaderFile(cbProject* pProject, wxString& filename);
    wxString GetSourceOrHeaderForFile(cbProject* pProject, wxString& filename);

    /** Add the paths to path array, and this will be used in GetAllPathsByFilename() function.
     *  internally, all the folder paths were recorded in UNIX format(path separator is '/').
     * @param dirs the target dir collection
     * @param path the new added path
     * @param hasExt the file path has extensions, such as C:/aaa/bbb.cpp
     */
    static void AddPaths(wxArrayString& dirs, const wxString& path, bool hasExt);

    // the functions below are handling and managing Parser object

    /** Dynamically allocate a Parser object for the input C::B project, note that when creating a
     * new Parser object, the DoFullParsing() function will be called, which collects the macro
     * definitions, and start the batch parsing.
     * @param project C::B project
     * @param useSavedOptions ( previously saved by calling [Parser|ClassBrowser]OptionsSave() )
     * @return Parser pointer of the project.
     */
    Parser* CreateParser(cbProject* project, bool useSavedOptions=false);

    /** delete the Parser object for the input project
     * @param project C::B project.
     * @return true if success.
     */
    bool DeleteParser(cbProject* project);

    /** Single file re-parse.
     * This is called when you add a single file to project, or a file was modified.
     * the main logic of this function call is:
     * 1, once this function is called, the file will be marked as "need to be reparsed" in the
     *    token tree, and a timer(reparse timer) is started.
     * 2, on reparse timer hit, we collect all the files marked as "need to be reparsed" from the
     *    token tree, remove them from the token tree, and call AddParse() to add parsing job, this
     *    will ticket the On batch timer
     * 3, when on batch timer hit, it see there are some parsing jobs to do (m_BatchParseFiles is
     *    not empty), then it will run a thread job ParserThreadedTask
     * 4, Once the ParserThreadedTask is running, it will create all the Parserthreads and run them
     *    in the thread pool
     * @param project C::B project
     * @param filename filename with full path in the C::B project
     */
    bool ReparseFile(cbProject* project, const wxString& filename);

    /** New file was added to the C::B project, so this will cause a re-parse on the new added file.
     * @param project C::B project
     * @param filename filename with full path in the C::B project
     */
    bool AddFileToParser(cbProject* project, const wxString& filename, Parser* parser = nullptr);

    /** remove a file from C::B project and Parser
     * @param project C::B project
     * @param filename filename with full patch in the C::B project
     */
    void RemoveFileFromParser(cbProject* project, const wxString& filename);

    /** when user changes the CC option, we should re-read the option */
    void RereadParserOptions();

    /** Is Settings/Editor/Clangd_client/Symbols browser/ enabled/disabled */
    bool IsClassBrowserEnabled();       //(ph 2023/11/29)

    /** re-parse the active Parser (the project associated with m_Parser member variable) */
    void ReparseCurrentEditor();

    /** re-parse the project select by context menu in projects management panel */
    void ReparseSelectedProject();

    /** Reparse the active project */
    void ReparseCurrentProject();

////    /** collect tokens where a code suggestion list can be shown
////     * @param[in] searchData search location, the place where the caret locates
////     * @param[out] result containing all matching result token indexes
////     * @param reallyUseAI true means the context scope information should be considered,
////     *        false if only do a plain word match
////     * @param isPrefix partially match which result all the Tokens' name with the same prefix,
////              otherwise use full-text match
////     * @param caseSensitive case sensitive or not
////     * @param caretPos Where the current caret locates, -1 means we use the current caret position.
////     * @return the matching Token count
////     */
////    size_t MarkItemsByAI(ccSearchData* searchData, TokenIdxSet& result, bool reallyUseAI = true,
////                         bool isPrefix = true, bool caseSensitive = false, int caretPos = -1);
////
////    /** the same as before, but we don't specify the searchData information, so it will use the active
////     *  editor and current caret information.
////     */
////    size_t MarkItemsByAI(TokenIdxSet& result, bool reallyUseAI = true, bool isPrefix = true,
////                         bool caseSensitive = false, int caretPos = -1);

////    /** Call tips are tips when you are typing function arguments
////     * these tips information could be:
////     * the prototypes information of the current function,
////     * the type information of the argument.
////     * Here are the basic algorithm
////     *
////     * if you have a function declaration like this: int fun(int a, float b, char c);
////     * when user are typing code, the caret is located here
////     * fun(arg1, arg2|
////     *    ^end       ^ begin
////     * we first do a backward search, should find the "fun" as the function name
////     * and later return the string "int fun(int a, float b, char c)" as the call tip
////     * typedCommas is 1, since one argument is already typed.
////     *
////     * @param[out] items array to store the tip results.
////     * @param typedCommas how much comma characters the user has typed in the current line before the cursor.
////     * @param ed the editor
////     * @param pos the location of the caret, if not supplied, the current caret is used
////     * @return The location in the editor of the beginning of the argument list
////     */
////    int GetCallTips(wxArrayString& items, int& typedCommas, cbEditor* ed, int pos = wxNOT_FOUND);

    /** project search path is used for auto completion for #include <>
     * there is an "code_completion" option for each cbp, where user can specify
     * addtional C++ search paths
     */
    wxArrayString ParseProjectSearchDirs(const cbProject &project);

    /** set the addtional C++ search paths in the C::B project's code_completion setting */
    void SetProjectSearchDirs(cbProject &project, const wxArrayString &dirs);

    // The functions below is used to manage symbols browser
    /** return active class browser pointer */
    ClassBrowser* GetClassBrowser() const { return m_ClassBrowser; }

    /** create the class browser */
    void CreateClassBrowser();

    /** remove the class browser */
    void RemoveClassBrowser(bool appShutDown = false);

    /** update the class browser tree */
    void UpdateClassBrowser(bool force=false);

    /** check if ok to update the ClassBrower Symbols window */
    bool IsOkToUpdateClassBrowserView();
    bool GetSymbolsWindowHasFocus(){return m_SymbolsWindowHasFocus;} //(ph 2023/12/06)
    void SetSymbolsWindowHasFocus(bool torf){ m_SymbolsWindowHasFocus = torf; } //(ph 2024/01/20)
    void SetUpdatingClassBrowserBusy(bool torf) {m_ClassBrowserUpdating = torf;}; //(ph 2024/01/21)
    bool GetUpdatingClassBrowserBusy(){return m_ClassBrowserUpdating;}

    // save current options and BrowserOptions
    void ParserOptionsSave(Parser* pParser);
    void BrowserOptionsSave(Parser* pParser);
    ParserOptions&  GetSavedOptions()        {return m_OptionsSaved;}
    BrowserOptions& GetSavedBrowserOptions() {return m_BrowserOptionsSaved;}

    wxString m_RenameSymbolToChange;
    void SetRenameSymbolToChange(wxString sysmbolToChange){m_RenameSymbolToChange = sysmbolToChange;}
    wxString GetRenameSymbolToChange(){return m_RenameSymbolToChange;}

    void RefreshSymbolsTab(); //(ph 2023/11/30)
    void OnAUIProjectPageChanging(wxAuiNotebookEvent& event); //(ph 2024/01/19)
    void OnAUIProjectPageChanged(wxAuiNotebookEvent& event);  //(ph 2024/01/19)

    // ----------------------------------------------------------------------------
    wxEvtHandler* FindEventHandler(wxEvtHandler* pEvtHdlr)
    // ----------------------------------------------------------------------------
    {
        wxEvtHandler* pFoundEvtHdlr =  Manager::Get()->GetAppWindow()->GetEventHandler();

        while (pFoundEvtHdlr != nullptr)
        {
            if (pFoundEvtHdlr == pEvtHdlr)
                return pFoundEvtHdlr;
            pFoundEvtHdlr = pFoundEvtHdlr->GetNextHandler();
        }
        return nullptr;
    }
    // ----------------------------------------------------------------------------
    size_t GetNowMilliSeconds()
    // ----------------------------------------------------------------------------
    {
        auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return millis;
    }
    // ----------------------------------------------------------------------------
    size_t GetDurationMilliSeconds(int startMillis)
    // ----------------------------------------------------------------------------
    {
        int nowMillis = GetNowMilliSeconds();
        return nowMillis - startMillis;
    }

    ProcessLanguageClient* CreateNewLanguageServiceProcess(cbProject* pcbProject, int LSPeventID);
    bool DoLockClangd_CacheAccess(cbProject* pcbProject);

    LSPEventCallbackHandler* GetLSPEventSinkHandler(){return m_pLSPEventSinkHandler;}

    /** Set from CodeCompletion::OnCompiler{Started|Finished}() event */
    bool IsCompilerRunning(){return m_CompilerIsRunning;}
    void SetCompilerIsRunning(bool torf){m_CompilerIsRunning = torf;}

    // Get pointer to Idle callbacks
    IdleCallbackHandler* GetIdleCallbackHandler(cbProject* pProject);

    void ClearAllIdleCallbacks();

    // Put clangd proxy project into codeblocks config folder
    bool InstallClangdProxyProject();

    // Create a hidden proxy project to hold the non-project info for clangd_client
    void SetProxyProject(cbProject* pActiveProject);

    // Fetch the temporary cbProject used to send non-project files to clangd.
    cbProject* GetProxyProject() {return m_pProxyProject;}
    bool IsProxyProject(cbProject* pProject){return pProject == m_pProxyProject;}
    // Fetch the ProxyParser used to parse non-project owned files
    Parser* GetProxyParser() {return m_pProxyParser;}
    bool IsProxyParser(Parser* pParser) {return pParser == m_pProxyParser;}

    bool GetPluginIsShuttingDown()
    {
        if (Manager::IsAppShuttingDown() or m_PluginIsShuttingDown)
            return true;
        return false;
    }
    void SetPluginIsShuttingDown();
    // ----------------------------------------------------------------
    ProcessLanguageClient* GetLSPclientAllocated(cbProject* pProject)
    // ----------------------------------------------------------------
    {
        ProcessLanguageClient* pClient =  nullptr;
        if (not pProject) return nullptr;

        if (m_LSP_Clients.count(pProject))
            pClient =  m_LSP_Clients[pProject];
        if (pClient)
            return pClient;

        return nullptr;
    }

    bool DoUnlockClangd_CacheAccess(cbProject* pcbProject);
    ProcessLanguageClient* GetLSPclient(cbProject* pProject);
    ProcessLanguageClient* GetLSPclient(cbEditor* pEd);
    cbProject* GetProjectByClientAndFilename(ProcessLanguageClient* pClient, wxString& Filename);

    bool IsDebuggerRunning() {return m_DebuggerIsRunning;} //(ph 2023/11/17)
    void SetDebuggerIsRunning(bool torf) {m_DebuggerIsRunning = torf;}

    // Get pointer to hidden cbStyledTextCtrl //(ph 2023/12/22)
    cbStyledTextCtrl* GetHiddenEditor(){return m_pHiddenEditor.get();} //(ph 2023/12/22)

    bool GetParsingIsBusy(){ return m_ParsingIsBusy;}
    void SetParsingIsBusy(bool trueOrFalse){ m_ParsingIsBusy = trueOrFalse;}

    bool GetUseCCIconsOption(); //option to use icons in completion popup

    bool DoShowDiagnostics(wxString filename, int line);  //(Christo 2024/03/30)
    void InsertDiagnostics(wxString filename, std::vector<std::pair<int, wxString>> diagnostics);  //(Christo 2024/03/30)
    void ClearDiagnostics(wxString filename);  //(Christo 2024/03/30)
    bool HasDiagnostics(wxString filename);    //(ph 2024/05/02)

    bool GetHoverRequestIsActive(){return m_HoverRequestIsActive;} //(ph 2024/04/25)
    void SetHoverRequestIsActive(bool trueOrFalse) //(ph 2024/04/25)
        { m_HoverRequestIsActive = trueOrFalse;}


protected:
    /** When a Parser is created, we need a full parsing stage including:
     * 1, parse the priority header files firstly.
     * 2, parse all the other project files.
     */
    bool DoFullParsing(cbProject* project, Parser* parser);
    void GetPriorityFilesForParsing(StringList& localSourcesList, cbProject* pProject);

    bool AddCompilerAndIncludeDirs(cbProject* project, Parser* parser);

  /** Add compiler include directories (from search paths) to a parser */
    void AddCompilerIncludeDirsToParser(const Compiler* compiler, Parser* parser);

    /** Switch parser object according the current active editor and filename */
    bool SwitchParser(cbProject* project, Parser* parser);

    /** Set a new Parser as the active Parser
     * Set the active parser pointer (m_Parser member variable)
     * update the ClassBrowser's Parser pointer
     * re-fresh the symbol browser tree.
     * if we did switch the parser, we also need to remove the temporary tokens of the old parser.
     */
    void SetParser(Parser* parser);

    /** Clear all Parser object */
    void ClearParsers();

////    /** Remove all the obsolete Parser objects
////     * if the number exceeds the limited number (can be set in the CC's option), then all the
////     * obsolete parsers will be removed.
////     */
////    void RemoveObsoleteParsers();

    /** Get cbProject and Parser pointer, according to the current active editor */
    std::pair<cbProject*, Parser*> GetParserInfoByCurrentEditor();

    /** set the class browser view mode */
    void SetCBViewMode(const BrowserViewMode& mode);

private:
    friend class ClgdCompletion;

////    /** Start an Artificial Intelligence search algorithm to gather all the matching tokens.
////     * The actual AI is in FindAIMatches() below.
////     * @param result output parameter.
////     * @param searchData cbEditor information.
////     * @param lineText current statement.
////     * @param isPrefix if true, then the result contains all the tokens whose name is a prefix of current lineText.
////     * @param caseSensitive true is case sensitive is enabled on the match.
////     * @param search_scope it is the "parent token" where we match the "search-key".
////     * @param caretPos use current caret position if it is -1.
////     * @return matched token number
////     */
////    size_t AI(TokenIdxSet& result,
////              ccSearchData* searchData,
////              const wxString& lineText = wxEmptyString,
////              bool isPrefix = false,
////              bool caseSensitive = false,
////              TokenIdxSet* search_scope = 0,
////              int caretPos = -1);

////    /** return all the tokens matching the current function(hopefully, just one)
////     * @param editor editor pointer
////     * @param result output result containing all the Token index
////     * @param caretPos -1 if the current caret position is used.
////     * @return number of result Tokens
////     */
////    size_t FindCurrentFunctionToken(ccSearchData* searchData, TokenIdxSet& result, int caretPos = -1);

    /** returns the position where the current function scope starts.
     * optionally, returns the function's namespace (ends in double-colon ::), name and token
     * @param[in] searchData search data struct pointer
     * @param[out] nameSpace get the namespace modifier
     * @param[out] procName get the function name
     * @param[out] functionToken get the token of current function
     * @param caretPos caret position in cbEditor
     * @return current function line number
     */
    int FindCurrentFunctionStart(bool callerHasTokenLock,
                                 ccSearchData* searchData,
                                 wxString*     nameSpace = 0L,
                                 wxString*     procName = 0L,
                                 int*          functionIndex = 0L,
                                 int           caretPos = -1);

////    /** used in CodeCompletion suggestion list to boost the performance, we use a cache */
////    bool LastAISearchWasGlobal() const { return m_LastAISearchWasGlobal; }

////    /** The same as above */
////    const wxString& LastAIGlobalSearch() const { return m_LastAIGlobalSearch; }

////    /** collect the using namespace directive in the editor specified by searchData
////     * @param searchData search location
////     * @param search_scope resulting tokens collection
////     * @param caretPos caret position, if not specified, we use the current caret position
////     */
////    bool ParseUsingNamespace(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos = -1);

////    /** collect the using namespace directive in the buffer specified by searchData
////     * @param buffer code snippet to be parsed
////     * @param search_scope resulting tokens collection
////     * @param bufferSkipBlocks skip brace sets { }
////     */
////    bool ParseBufferForUsingNamespace(const wxString& buffer, TokenIdxSet& search_scope, bool bufferSkipBlocks = true);

////    /** collect function argument, add them to the token tree (as temporary tokens)
////     * @param searchData search location
////     * @param caretPos caret position, if not specified, we use the current caret position
////     */
////    bool ParseFunctionArguments(ccSearchData* searchData, int caretPos = -1);

////    /** parse the contents from the start of function body to the cursor, this is used to collect local variables.
////     * @param searchData search location
////     * @param search_scope resulting tokens collection of local using namespace
////     * @param caretPos caret position, if not specified, we use the current caret position
////     */
////    bool ParseLocalBlock(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos = -1);

////    /** collect the header file search directories, those dirs include:
////     * todo: a better name could be: AddHeaderFileSearchDirs
////     *  1, project's base dir, e.g. if you cbp file was c:/bbb/aaa.cbp, then c:/bbb is added.
////     *  2, project's setting search dirs, for a wx project, then c:/wxWidgets2.8.12/include is added.
////     *  3, a project may has some targets, so add search dirs for those targets
////     *  4, compiler's own search path(system include search paths), like: c:/mingw/include
////     */
////    bool AddCompilerDirs(cbProject* project, Parser* parser);

////    /** collect compiler specific predefined preprocessor definition, this is usually run a special
////     * compiler command, like GCC -dM for gcc.
////     * @return true if there are some macro definition added, else it is false
////     */
////    bool AddCompilerPredefinedMacros(cbProject* project, Parser* parser);

////    /** collect GCC compiler predefined preprocessor definition */
////    bool AddCompilerPredefinedMacrosGCC(const wxString& compilerId, cbProject* project, wxString& defs, Parser* parser);

////    /** lookup GCC compiler -std=XXX option
////     * return a string such as "c++11" or "c++17" or "gnu++17"
////     */
////    wxString GetCompilerStandardGCC(Compiler* compiler, cbProject* project);

////    /** lookup GCC compiler -std=XXX option for specific GCC options*/
////    wxString GetCompilerUsingStandardGCC(const wxArrayString& compilerOptions);

////    /** collect VC compiler predefined preprocessor definition */
////    bool AddCompilerPredefinedMacrosVC(const wxString& compilerId, wxString& defs, Parser* parser);

////    /** collect project (user) defined preprocessor definition, such as for wxWidgets project, the
////     * macro may have "#define wxUSE_UNICODE" defined in its project file.
////    * @return true if there are some macro definition added, return false if nothing added
////     */
////    bool AddProjectDefinedMacros(cbProject* project, Parser* parser);

////    /** Add compiler include directories (from search paths) to a parser */
////    void AddCompilerIncludeDirsToParser(const Compiler* compiler, Parser* parser);

    /** Collect the default compiler include file search paths. called by AddCompilerDirs() function
     * @return compiler's own search path(system include search paths), like: c:/mingw/include
     */
    const wxArrayString& GetGCCCompilerDirs(const wxString& cpp_path, const wxString& cpp_executable);

    /** Add the collected default GCC compiler include search paths to a parser
     * todo: document this
     */
     void AddGCCCompilerDirs(const wxString& masterPath, const wxString& compilerCpp, Parser* parser);

    /** Add a list of directories to the parser's search directories, normalise to "base" path, if
     * "base" is not empty. Replaces macros.
     * @param dirs user defined search path, such as a sub folder named "inc"
     * @param base the base folder of the "dirs", for example, if base is "d:/abc", then the search
     * path "d:/abc/inc" is added to the parser
     */
    void AddIncludeDirsToParser(const wxArrayString& dirs, const wxString& base, Parser* parser);

    /** Runs an app safely (protected against re-entry) and returns output and error */
    bool SafeExecute(const wxString& app_path, const wxString& app, const wxString& args, wxArrayString &output, wxArrayString &error);

////    /** Event handler when the batch parse starts, print some log information */
////    void OnParserStart(wxCommandEvent& event);

////    /** Event handler when the batch parse finishes, print some log information, check whether the active editor
////     * belong to the current parser, if not, do a parser switch
////     */
////    void OnParserEnd(wxCommandEvent& event);

////    /** If use one parser per whole workspace, we need parse all project one by one, E.g.
////     * If a workspace contains A.cbp, B.cbp and C.cbp, and we are in the mode of one parser for
////     * the whole workspace, we first parse A.cbp, after that we should continue to parse B.cbp. When
////     * finishing parsing B.cbp, we need the timer again to parse the C.cbp.
////     * If we are in the mode of one parser for one project, then after parsing A.cbp, the timer is
////     * kicked, so there is a chance to parse the B.cbp or C.cbp, but only when user opened some files
////     * of B.cbp or C.cbp when the timer event arrived.
////     */
////    void OnParsingOneByOneTimer(wxTimerEvent& event);

    /** Event handler when an editor activate, *NONE* project is handled here */
    void OnEditorActivated(EditorBase* editor);

    /** Event handler when an editor closed, if it is the last editor belong to *NONE* project, then
     *  the *NONE* Parser will be removed
     */
    void OnEditorClosed(EditorBase* editor);

    /** Init cc search member variables */
    void InitCCSearchVariables();

////    /** Add one project to the common parser in one parser for the whole workspace mode
////     * @return true means there are some thing (macro and files) need to parse, otherwise it is false
////     */
////    bool AddProjectToParser(cbProject* project);

////    /** Remove cbp from the common parser, this only happens in one parser for whole workspace mode
////     * when a parser is removed from the workspace, we should remove the project from the parser
////     */
////    bool RemoveProjectFromParser(cbProject* project);


private:
    typedef std::pair<cbProject*, Parser*> ProjectParserPair;
    typedef std::list<ProjectParserPair>       ParserList;

    /** a list holding all the cbp->parser pairs, if in one parser per project mode, there are many
     * many pairs in this list. In one parser per workspace mode, there is only one pair, and the
     * m_ParserList.begin()->second is the common parser for all the projects in workspace.
     */
    ParserList               m_ParserList;
    /** a temp Parser object pointer used when there's no available parser */
    Parser*                  m_NullParser = nullptr;
    /** active Parser object pointer */
    Parser*                  m_ActiveParser;
    /** Proxy Parser object pointer used to parse non-project owned files*/
    Parser*                  m_pProxyParser = nullptr;

////    /** a delay timer to parser every project in sequence */
////    wxTimer                      m_TimerParsingOneByOne;
    /** symbol browser window */
    ClassBrowser*                m_ClassBrowser;
    /** if true, which means m_ClassBrowser is floating (not docked) */
    bool                         m_ClassBrowserIsFloating;

    /// Stores image lists for different sizes. See GetImageList.
    typedef std::unordered_map<int, std::unique_ptr<wxImageList>> SizeToImageList;
    SizeToImageList m_ImageListMap;

    /** all the files which opened in editors, but do not belong to any cbp */
    wxArrayString                m_StandaloneFiles;

    /** if true, which means the parser holds tokens of the entire workspace's project, if false
     * then one parser per a cbp
     */
    bool                         m_ParserPerWorkspace = false; //m_ParserPerWorkspace always false for clangd

    /** only used when m_ParserPerWorkspace is true, and holds all the cbps for the common parser */
    std::set<cbProject*>         m_ParsedProjects; //m_ParserPerWorkspace always false for clangd

    /* CC Search Member Variables => START */
////    wxString          m_LastAIGlobalSearch;    //!< same case like above, it holds the search string
////    bool              m_LastAISearchWasGlobal; //!< true if the phrase for code-completion is empty or partial text (i.e. no . -> or :: operators)
    cbStyledTextCtrl* m_LastControl;
    wxString          m_LastFile;
    int               m_LastFunctionIndex;
    int               m_LastFuncTokenIdx;      //!< saved the function token's index, for remove all local variable tokens
    int               m_LastLine;
    wxString          m_LastNamespace;
    wxString          m_LastPROC;
    int               m_LastResult;
    /* CC Search Member Variables => END */


    ParserOptions   m_OptionsSaved;
    BrowserOptions  m_BrowserOptionsSaved;

    // project pointers and their associated LSP client pointers
    typedef std::map<cbProject*, ProcessLanguageClient*> LSPClientsMapType;
    LSPClientsMapType m_LSP_Clients; //map of all LSP clients by project*
    void CloseAllClients();          //shutdown


    LSPEventCallbackHandler* m_pLSPEventSinkHandler;
    ClgdCompletion* m_pClientEventHandler;
    void SetClientEventHandler(ClgdCompletion* pClientEventHandler)
            {m_pClientEventHandler = (ClgdCompletion*)pClientEventHandler;}

    // Idle callback Handler pointer  moved to parser_base.h
    //- 2022/08/1 std::unique_ptr<IdleCallbackHandler> pIdleCallbacks;

    bool m_CompilerIsRunning = false;

    cbProject* m_pProxyProject = nullptr;

    // List of ProjectFiles* added to the ProxyProject
    std::vector<ProjectFile*> ProxyProjectFiles;

    bool m_PluginIsShuttingDown = false;
    bool m_DebuggerIsRunning = false;       //(ph 2023/11/17)
    bool m_SymbolsWindowHasFocus = false;   //(ph 2024/01/20)
    bool m_ClassBrowserUpdating = false;    //(ph 2024/01/21)
    bool m_ParsingIsBusy = false;

    // a global hidden temp cbStyledTextCtrl for symbol/src/line searches
    // to avoid constantly allocating wxIDs.
    std::unique_ptr<cbStyledTextCtrl> m_pHiddenEditor = nullptr; //(ph 2023/12/22)

    typedef std::unordered_map<wxString, std::vector<std::pair<int, wxString>>> DiagnosticsCache_t;
    //std::unordered_map<wxString, std::vector<std::pair<int, wxString>>> m_diagnosticsCache;  //(Christo 2024/03/30)
    DiagnosticsCache_t m_diagnosticsCache;  //(Christo 2024/03/30)
    //typedef std::vector<std::pair<int, wxString>> InnerMap_t;

    std::mutex m_diagnosticsCacheMutex;
    bool m_HoverRequestIsActive = false;    //(ph 2024/04/26)

};

#endif // ParseManager_H

