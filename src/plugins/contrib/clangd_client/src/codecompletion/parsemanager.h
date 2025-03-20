/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef ParseManager_H
#define ParseManager_H

//unused #include <queue>
//unused #include <map>
#include <memory>
#include <mutex>  //(Christo 2024/03/30)
#include <unordered_map>
#include <wx/event.h>
#include <wx/aui/aui.h>

#include <cbstyledtextctrl.h>
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

// TODO (ollydbg#1#), this class is dirty, I'm going to change its name like CursorLocation
/* * Search location combination, a pointer to cbStyledTextCtrl and a filename is enough */
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
    bool IsClassBrowserEnabled();

    /** re-parse the active Parser (the project associated with m_Parser member variable) */
    void ReparseCurrentEditor();

    /** re-parse the project select by context menu in projects management panel */
    void ReparseSelectedProject();

    /** Reparse the active project */
    void ReparseCurrentProject();

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
    bool GetSymbolsWindowHasFocus(){return m_SymbolsWindowHasFocus;}
    void SetSymbolsWindowHasFocus(bool torf){ m_SymbolsWindowHasFocus = torf; }
    void SetUpdatingClassBrowserBusy(bool torf) {m_ClassBrowserUpdating = torf;};
    bool GetUpdatingClassBrowserBusy(){return m_ClassBrowserUpdating;}

    // save current options and BrowserOptions
    void ParserOptionsSave(Parser* pParser);
    void BrowserOptionsSave(Parser* pParser);
    ParserOptions&  GetSavedOptions()        {return m_OptionsSaved;}
    BrowserOptions& GetSavedBrowserOptions() {return m_BrowserOptionsSaved;}

    wxString m_RenameSymbolToChange;
    void SetRenameSymbolToChange(wxString sysmbolToChange){m_RenameSymbolToChange = sysmbolToChange;}
    wxString GetRenameSymbolToChange(){return m_RenameSymbolToChange;}

    void RefreshSymbolsTab();
    void OnAUIProjectPageChanging(wxAuiNotebookEvent& event);
    void OnAUIProjectPageChanged(wxAuiNotebookEvent& event);

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

    bool IsDebuggerRunning() {return m_DebuggerIsRunning;}
    void SetDebuggerIsRunning(bool torf) {m_DebuggerIsRunning = torf;}

    // Get pointer to hidden cbStyledTextCtrl
    cbStyledTextCtrl* GetHiddenEditor(){return m_pHiddenEditor.get();}

    bool GetParsingIsBusy(){ return m_ParsingIsBusy;}
    void SetParsingIsBusy(bool trueOrFalse){ m_ParsingIsBusy = trueOrFalse;}

    bool GetUseCCIconsOption(); //option to use icons in completion popup

    bool DoShowDiagnostics(wxString filename, int line);  //(Christo 2024/03/30)
    void InsertDiagnostics(wxString filename, std::vector<std::pair<int, wxString>> diagnostics);  //(Christo 2024/03/30)
    void ClearDiagnostics(wxString filename);  //(Christo 2024/03/30)
    bool HasDiagnostics(wxString filename);

    bool GetHoverRequestIsActive(){return m_HoverRequestIsActive;}
    void SetHoverRequestIsActive(bool trueOrFalse)
        { m_HoverRequestIsActive = trueOrFalse;}

    // Functions to stop clobbering global settings on project close (svn 13612 bkport)
    // Set or return Project that changed "Global setting" in workspace
    cbProject* GetOptsChangedByProject(){ return m_pOptsChangedProject;}
    void SetOptsChangedByProject(cbProject* pProject){m_pOptsChangedProject = pProject;}
    // Set or return Parser that changed "Global setting" in Single File workspace
    ParserBase* GetOptsChangedByParser(){ return m_pOptsChangedParser;}
    void SetOptsChangedByParser(ParserBase* pParserBase){m_pOptsChangedParser = pParserBase;}
    Parser*     GetTempParser(){return m_TempParser;}
    ParserBase* GetClosingParser(){return m_pClosingParser;}                        //(ph 2025/02/12)
    void        SetClosingParser(ParserBase* pParser){m_pClosingParser = pParser;}  //(ph 2025/02/12)
    std::unordered_map<cbProject*,ParserBase*>* GetActiveParsers();                 //(ph 2025/02/14)

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

    /** Get cbProject and Parser pointer, according to the current active editor */
    std::pair<cbProject*, Parser*> GetParserInfoByCurrentEditor();

    /** set the class browser view mode */
    void SetCBViewMode(const BrowserViewMode& mode);

private:
    friend class ClgdCompletion;

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

    /** Event handler when an editor activate, *NONE* project is handled here */
    void OnEditorActivated(EditorBase* editor);

    /** Event handler when an editor closed, if it is the last editor belong to *NONE* project, then
     *  the *NONE* Parser will be removed
     */
    void OnEditorClosed(EditorBase* editor);

    /** Init cc search member variables */
    void InitCCSearchVariables();

private:
    typedef std::pair<cbProject*, Parser*> ProjectParserPair;
    typedef std::list<ProjectParserPair>       ParserList;

    /** a list holding all the cbp->parser pairs, if in one parser per project mode, there are many
     * many pairs in this list. In one parser per workspace mode, there is only one pair, and the
     * m_ParserList.begin()->second is the common parser for all the projects in workspace.
     */
    ParserList               m_ParserList;
    /** a temp Parser object pointer used when there's no available parser */
    Parser*                  m_TempParser = nullptr;
    /** active Parser object pointer */
    Parser*                  m_ActiveParser;
    /** Proxy Parser object pointer used to parse non-project owned files*/
    Parser*                  m_pProxyParser = nullptr;

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
    bool m_DebuggerIsRunning = false;
    bool m_SymbolsWindowHasFocus = false;
    bool m_ClassBrowserUpdating = false;
    bool m_ParsingIsBusy = false;

    // a global hidden temp cbStyledTextCtrl for symbol/src/line searches
    // to avoid constantly allocating wxIDs.
    std::unique_ptr<cbStyledTextCtrl> m_pHiddenEditor = nullptr;

    typedef std::unordered_map<wxString, std::vector<std::pair<int, wxString>>> DiagnosticsCache_t;
    //std::unordered_map<wxString, std::vector<std::pair<int, wxString>>> m_diagnosticsCache;  //(Christo 2024/03/30)
    DiagnosticsCache_t m_diagnosticsCache;  //(Christo 2024/03/30)
    //typedef std::vector<std::pair<int, wxString>> InnerMap_t;

    std::mutex m_diagnosticsCacheMutex;
    bool m_HoverRequestIsActive = false;
    // data to stop clobbering global settings on project close (svn 13612 bkport)
    //The latest project to change the .conf file
    cbProject* m_pOptsChangedProject = nullptr;
    //The latest parser to change the .conf file
    ParserBase* m_pOptsChangedParser = nullptr;
    // the currently closing parser
    ParserBase* m_pClosingParser     = nullptr;

    // copy of m_ParserList used in WriteOptions() (svn 13612 bkport)
    std::unordered_map<cbProject*,ParserBase*> m_ActiveParserList;

};

#endif // ParseManager_H

