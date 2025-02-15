/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef PARSEMANAGER_H
#define PARSEMANAGER_H

#include "parsemanager_base.h"
#include "parser/parser.h"

//unused #include <queue>
// unused #include <map>
#include <memory>
#include <unordered_map>

#include <wx/event.h>

extern const int g_EditorActivatedDelay;

// forward declaration
class cbEditor;
class EditorBase;
class cbProject;
class cbStyledTextCtrl;
class ClassBrowser;
class Compiler;
class Token;

// TODO (ollydbg#1#), this class is dirty, I'm going to change its name like CursorLocation
/** Search location combination, a pointer to cbStyledTextCtrl and a filename is enough */
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
 * In another mode, all C::B projects belong to a C::B workspace share a single Parser object.
 * Nativeparser will manage all the Parser objects.
 */
class ParseManager : public wxEvtHandler, ParseManagerBase
{
public:
    /** Constructor */
    ParseManager();

    /** Destructor */
    ~ParseManager();

    /** return a reference to the current active Parser object */
    ParserBase& GetParser() { return *m_Parser; }

    /** return the Parser pointer corresponding to the input C::B project
     * @param project input C::B project pointer
     * @return a pointer to Parser object
     */
    ParserBase* GetParserByProject(cbProject* project);

    /** return the Parser pointer associated with the input file
     * If a file belongs to several Parser objects, the first found Parser will be returned.
     * @param filename filename with full path.
     * @return a Parser pointer
     */
    ParserBase* GetParserByFilename(const wxString& filename);

    /** return the C::B project associated with Parser pointer
     * @param parser Parser pointer
     * @return C::B Project pointer
     */
    cbProject* GetProjectByParser(ParserBase* parser);

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
    cbProject* GetCurrentProject();

    /** Return true if one Parser per whole workspace option is selected */
    bool IsParserPerWorkspace() const { return m_ParserPerWorkspace; }

    /** Return true if all the Parser's batch-parse stages are finished, otherwise return false */
    bool Done();

    /** Provides images for the Symbol browser (for tree node images) and AutoCompletion list.
     *  @param maxSize Maximum size that will fit in the UI.
     */
    wxImageList* GetImageList(int maxSize);

    /** Returns the image assigned to a specific token for a symbol browser */
    int GetTokenKindImage(const Token* token);

    /** Get the implementation file path if the input is a header file. or Get the header file path
     * if the input is an implementation file.
     * Both the implementation file and header file can be in different directories.
     * @param filename input filename
     * @return corresponding file paths, in wxArrayString format
     */
    wxArrayString GetAllPathsByFilename(const wxString& filename);

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
     * definitions, and start the batch parsing from the thread pool.
     * @param project C::B project
     * @return Parser pointer of the project.
     */
    ParserBase* CreateParser(cbProject* project);

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
    bool AddFileToParser(cbProject* project, const wxString& filename, ParserBase* parser = nullptr);

    /** remove a file from C::B project and Parser
     * @param project C::B project
     * @param filename filename with full patch in the C::B project
     */
    bool RemoveFileFromParser(cbProject* project, const wxString& filename);

    /** when user changes the CC option, we should re-read the option */
    void RereadParserOptions();

    /** re-parse the active Parser (the project associated with m_Parser member variable) */
    void ReparseCurrentProject();

    /** re-parse the project select by context menu in projects management panel */
    void ReparseSelectedProject();

    /** collect tokens where a code suggestion list can be shown
     * @param[in] searchData search location, the place where the caret locates
     * @param[out] result containing all matching result token indexes
     * @param reallyUseAI true means the context scope information should be considered,
     *        false if only do a plain word match
     * @param isPrefix partially match which result all the Tokens' name with the same prefix,
              otherwise use full-text match
     * @param caseSensitive case sensitive or not
     * @param caretPos Where the current caret locates, -1 means we use the current caret position.
     * @return the matching Token count
     */
    size_t MarkItemsByAI(ccSearchData* searchData, TokenIdxSet& result, bool reallyUseAI = true,
                         bool isPrefix = true, bool caseSensitive = false, int caretPos = -1);

    /** the same as before, but we don't specify the searchData information, so it will use the active
     *  editor and current caret information.
     */
    size_t MarkItemsByAI(TokenIdxSet& result, bool reallyUseAI = true, bool isPrefix = true,
                         bool caseSensitive = false, int caretPos = -1);

    /** Call tips are tips when you are typing function arguments
     * these tips information could be:
     * the prototypes information of the current function,
     * the type information of the argument.
     * Here are the basic algorithm
     *
     * if you have a function declaration like this: int fun(int a, float b, char c);
     * when user are typing code, the caret is located here
     * fun(arg1, arg2|
     *    ^end       ^ begin
     * we first do a backward search, should find the "fun" as the function name
     * and later return the string "int fun(int a, float b, char c)" as the call tip
     * typedCommas is 1, since one argument is already typed.
     *
     * @param[out] items array to store the tip results.
     * @param typedCommas how much comma characters the user has typed in the current line before the cursor.
     * @param ed the editor
     * @param pos the location of the caret, if not supplied, the current caret is used
     * @return The location in the editor of the beginning of the argument list
     */
    int GetCallTips(wxArrayString& items, int& typedCommas, cbEditor* ed, int pos = wxNOT_FOUND);

    /** project search path is used for auto completion for #include <>
     * there is an "code_completion" option for each cbp, where user can specify
     * addtional C++ search paths
     */
    wxArrayString ParseProjectSearchDirs(const cbProject &project);

    /** set the addtional C++ search paths in the C::B project's code_completion setting */
    void SetProjectSearchDirs(cbProject &project, const wxArrayString &dirs);

    // The function below is used to manage symbols browser
    /** return active class browser pointer */
    ClassBrowser* GetClassBrowser() const { return m_ClassBrowser; }

    /** create the class browser */
    void CreateClassBrowser();

    /** remove the class browser */
    void RemoveClassBrowser(bool appShutDown = false);

    /** update the class browser tree */
    void UpdateClassBrowser();

    bool GetParsingIsBusy(){ return m_ParsingIsBusy;}
    void SetParsingIsBusy(bool trueOrFalse){ m_ParsingIsBusy = trueOrFalse;}

    bool GetUpdatingClassBrowserBusy(){ return m_UpdateClassBrowserViewBusy;}
    void SetUpdatingClassBrowserBusy(bool trueOrFalse){ m_UpdateClassBrowserViewBusy = trueOrFalse;}

    bool GetClassBrowserViewIsStale() {return m_ClassBrowserViewIsStale;}
    void SetClassBrowserViewIsStale(bool trueOrFalse) {m_ClassBrowserViewIsStale = trueOrFalse;}

    void SetSymbolsWindowHasFocus(bool trueOrFalse){ m_SymbolsWindowHasFocus = trueOrFalse;}
    bool GetSymbolsWindowHasFocus(){return m_SymbolsWindowHasFocus;}

    // Set or return Project that changed "Global setting" in workspace
    cbProject* GetOptsChangedByProject(){ return m_pOptsChangedProject;}
    void SetOptsChangedByProject(cbProject* pProject){m_pOptsChangedProject = pProject;}
    // Set or return Parser that changed "Global setting" in Single File workspace
    ParserBase* GetOptsChangedByParser(){ return m_pOptsChangedParser;}
    void SetOptsChangedByParser(ParserBase* pParserBase){m_pOptsChangedParser = pParserBase;}
    ParserBase* GetTempParser(){return m_TempParser;}
    ParserBase* GetClosingParser(){return m_pClosingParser;}                        //(ph 2025/02/12)
    void        SetClosingParser(ParserBase* pParser){m_pClosingParser = pParser;}  //(ph 2025/02/12)
    std::unordered_map<cbProject*,ParserBase*>* GetActiveParsers();                 //(ph 2025/02/14)


protected:
    /** When a Parser is created, we need a full parsing stage including:
     * 1, parse the priority header files firstly.
     * 2, parse all the other project files.
     */
    bool DoFullParsing(cbProject* project, ParserBase* parser);

    /** Switch parser object according the current active editor and filename */
    bool SwitchParser(cbProject* project, ParserBase* parser);

    /** Set a new Parser as the active Parser
     * Set the active parser pointer (m_Parser member variable)
     * update the ClassBrowser's Parser pointer
     * re-fresh the symbol browser tree.
     * if we did switch the parser, we also need to remove the temporary tokens of the old parser.
     */
    void SetParser(ParserBase* parser);

    /** Clear all Parser object */
    void ClearParsers();

    /** Remove all the obsolete Parser object
     * if the number exceeds the limited number (can be set in the CC's option), then all the
     * obsolete parser will be removed.
     */
    void RemoveObsoleteParsers();

    /** Get cbProject and Parser pointer, according to the current active editor */
    std::pair<cbProject*, ParserBase*> GetParserInfoByCurrentEditor();

    /** set the class browser view mode */
    void SetCBViewMode(const BrowserViewMode& mode);

private:
    friend class CodeCompletion;

    /** Start an Artificial Intelligence search algorithm to gather all the matching tokens.
     * The actual AI is in FindAIMatches() below.
     * @param result output parameter.
     * @param searchData cbEditor information.
     * @param lineText current statement.
     * @param isPrefix if true, then the result contains all the tokens whose name is a prefix of current lineText.
     * @param caseSensitive true is case sensitive is enabled on the match.
     * @param search_scope it is the "parent token" where we match the "search-key".
     * @param caretPos use current caret position if it is -1.
     * @return matched token number
     */
    size_t AI(TokenIdxSet& result,
              ccSearchData* searchData,
              const wxString& lineText = wxEmptyString,
              bool isPrefix = false,
              bool caseSensitive = false,
              TokenIdxSet* search_scope = 0,
              int caretPos = -1);

    /** return all the tokens matching the current function(hopefully, just one)
     * @param editor editor pointer
     * @param result output result containing all the Token index
     * @param caretPos -1 if the current caret position is used.
     * @return number of result Tokens
     */
    size_t FindCurrentFunctionToken(ccSearchData* searchData, TokenIdxSet& result, int caretPos = -1);

    /** returns the position where the current function scope starts.
     * optionally, returns the function's namespace (ends in double-colon ::), name and token
     * @param[in] searchData search data struct pointer
     * @param[out] nameSpace get the namespace modifier
     * @param[out] procName get the function name
     * @param[out] functionToken get the token of current function
     * @param caretPos caret position in cbEditor
     * @return current function line number
     */
    int FindCurrentFunctionStart(ccSearchData* searchData,
                                 wxString*     nameSpace = nullptr,
                                 wxString*     procName = nullptr,
                                 int*          functionIndex = nullptr,
                                 int           caretPos = -1);

    /** used in CodeCompletion suggestion list to boost the performance, we use a cache */
    bool LastAISearchWasGlobal() const { return m_LastAISearchWasGlobal; }

    /** The same as above */
    const wxString& LastAIGlobalSearch() const { return m_LastAIGlobalSearch; }

    /** collect the using namespace directive in the editor specified by searchData
     * @param searchData search location
     * @param search_scope resulting tokens collection
     * @param caretPos caret position, if not specified, we use the current caret position
     */
    bool ParseUsingNamespace(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos = -1);

    /** collect the using namespace directive in the buffer specified by searchData
     * @param buffer code snippet to be parsed
     * @param search_scope resulting tokens collection
     * @param bufferSkipBlocks skip brace sets { }
     */
    bool ParseBufferForUsingNamespace(const wxString& buffer, TokenIdxSet& search_scope, bool bufferSkipBlocks = true);

    /** collect function argument, add them to the token tree (as temporary tokens)
     * @param searchData search location
     * @param caretPos caret position, if not specified, we use the current caret position
     */
    bool ParseFunctionArguments(ccSearchData* searchData, int caretPos = -1);

    /** parse the contents from the start of function body to the cursor, this is used to collect local variables.
     * @param searchData search location
     * @param search_scope resulting tokens collection of local using namespace
     * @param caretPos caret position, if not specified, we use the current caret position
     */
    bool ParseLocalBlock(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos = -1);

    /** collect the header file search directories, those dirs include:
     * todo: a better name could be: AddHeaderFileSearchDirs
     *  1, project's base dir, e.g. if you cbp file was c:/bbb/aaa.cbp, then c:/bbb is added.
     *  2, project's setting search dirs, for a wx project, then c:/wxWidgets2.8.12/include is added.
     *  3, a project may has some targets, so add search dirs for those targets
     *  4, compiler's own search path(system include search paths), like: c:/mingw/include
     */
    bool AddCompilerDirs(cbProject* project, ParserBase* parser);

    /** collect compiler specific predefined preprocessor definition, this is usually run a special
     * compiler command, like GCC -dM for gcc.
     * @return true if there are some macro definition added, else it is false
     */
    bool AddCompilerPredefinedMacros(cbProject* project, ParserBase* parser);

    /** collect GCC compiler predefined preprocessor definition */
    bool AddCompilerPredefinedMacrosGCC(const wxString& compilerId, cbProject* project, wxString& defs, ParserBase* parser);

    /** lookup GCC compiler -std=XXX option
     * return a string such as "c++11" or "c++17" or "gnu++17"
     */
    wxString GetCompilerStandardGCC(Compiler* compiler, cbProject* project);

    /** lookup GCC compiler -std=XXX option for specific GCC options*/
    wxString GetCompilerUsingStandardGCC(const wxArrayString& compilerOptions);

    /** collect VC compiler predefined preprocessor definition */
    bool AddCompilerPredefinedMacrosVC(const wxString& compilerId, wxString& defs, ParserBase* parser);

    /** collect project (user) defined preprocessor definition, such as for wxWidgets project, the
     * macro may have "#define wxUSE_UNICODE" defined in its project file.
     * @return true if there are some macro definition added, return false if nothing added
     */
    bool AddProjectDefinedMacros(cbProject* project, ParserBase* parser);

    /** Add compiler include directories (from search paths) to a parser */
    void AddCompilerIncludeDirsToParser(const Compiler* compiler, ParserBase* parser);

    /** Collect the default compiler include file search paths. called by AddCompilerDirs() function
     * @return compiler's own search path(system include search paths), like: c:/mingw/include
     */
    const wxArrayString& GetGCCCompilerDirs(const wxString& cpp_path, const wxArrayString& extra_path, const wxString& cpp_executable);

    /** Add the collected default GCC compiler include search paths to a parser
     * todo: document this
     */
    void AddGCCCompilerDirs(const wxString& masterPath, const wxArrayString& extraPath, const wxString& compilerCpp, ParserBase* parser);

    /** Add a list of directories to the parser's search directories, normalise to "base" path, if
     * "base" is not empty. Replaces macros.
     * @param dirs user defined search path, such as a sub folder named "inc"
     * @param base the base folder of the "dirs", for example, if base is "d:/abc", then the search
     * path "d:/abc/inc" is added to the parser
     */
    void AddIncludeDirsToParser(const wxArrayString& dirs, const wxString& base, ParserBase* parser);

    /** Runs an app safely (protected against re-entry) and returns output and error
     * @param app_path the path where the compiler(such as gcc.exe) locates, this is usually the master(bin) path of the compiler plugin
     * @param extra_path some extra paths from the compiler plugin, this usually are the extra compiler path of the C::B project
     * @param app the compiler command, for example "gcc.exe" under Windows OS
     * @param args the compiler option to run the command
     * @param[out] output the stdout of the command result
     * @param[out] error the stderr of the command result
     */
    bool SafeExecute(const wxString& app_path, const wxArrayString& extra_path, const wxString& app, const wxString& args, wxArrayString &output, wxArrayString &error);

    /** Event handler when the batch parse starts, print some log information */
    void OnParserStart(wxCommandEvent& event);

    /** Event handler when the batch parse finishes, print some log information, check whether the active editor
     * belong to the current parser, if not, do a parser switch
     */
    void OnParserEnd(wxCommandEvent& event);

    /** If use one parser per whole workspace, we need parse all project one by one, E.g.
     * If a workspace contains A.cbp, B.cbp and C.cbp, and we are in the mode of one parser for
     * the whole workspace, we first parse A.cbp, after that we should continue to parse B.cbp. When
     * finishing parsing B.cbp, we need the timer again to parse the C.cbp.
     * If we are in the mode of one parser for one project, then after parsing A.cbp, the timer is
     * kicked, so there is a chance to parse the B.cbp or C.cbp, but only when user opened some files
     * of B.cbp or C.cbp when the timer event arrived.
     */
    void OnParsingOneByOneTimer(wxTimerEvent& event);

    /** Event handler when an editor activate, *NONE* project is handled here */
    void OnEditorActivated(EditorBase* editor);

    /** Event handler when an editor closed, if it is the last editor belong to *NONE* project, then
     *  the *NONE* Parser will be removed
     */
    void OnEditorClosed(EditorBase* editor);

    /** Init cc search member variables */
    void InitCCSearchVariables();

    /** Add one project to the common parser in one parser for the whole workspace mode
     * @return true means there are some thing (macro and files) need to parse, otherwise it is false
     */
    bool AddProjectToParser(cbProject* project);

    /** Remove cbp from the common parser, this only happens in one parser for whole workspace mode
     * when a parser is removed from the workspace, we should remove the project from the parser
     */
    bool RemoveProjectFromParser(cbProject* project);


private:
    typedef std::pair<cbProject*, ParserBase*> ProjectParserPair;
    typedef std::list<ProjectParserPair>       ParserList;

    /** a list holding all the cbp->parser pairs, if in one parser per project mode, there are many
     * many pairs in this list. In one parser per workspace mode, there is only one pair, and the
     * m_ParserList.begin()->second is the common parser for all the projects in workspace.
     */
    ParserList                   m_ParserList;
    /** a temp Parser object pointer */
    ParserBase*                  m_TempParser;
    /** active Parser object pointer */
    ParserBase*                  m_Parser;

    /** a delay timer to parser every project in sequence */
    wxTimer                      m_TimerParsingOneByOne;
    /** symbol browser window */
    ClassBrowser*                m_ClassBrowser;
    /** if true, which means m_ClassBrowser is floating (not docked) */
    bool                         m_ClassBrowserIsFloating;

    /// Stores image lists for different sizes. See GetImageList.
    typedef std::unordered_map<int, std::unique_ptr<wxImageList>> SizeToImageList;
    SizeToImageList m_ImageListMap;

    /** all the files which opened in editors, but do not belong to any cbp */
    wxArrayString                m_StandaloneFiles;
    /** if true, which means the parser hold tokens of the whole workspace's project, if false
     * then one parser per a cbp
     */
    bool                         m_ParserPerWorkspace;
    /** only used when m_ParserPerWorkspace is true, and holds all the cbps for the common parser */
    std::set<cbProject*>         m_ParsedProjects;

    /* CC Search Member Variables => START */
    wxString          m_LastAIGlobalSearch;    //!< same case like above, it holds the search string
    bool              m_LastAISearchWasGlobal; //!< true if the phrase for code-completion is empty or partial text (i.e. no . -> or :: operators)
    cbStyledTextCtrl* m_LastControl;
    wxString          m_LastFile;
    int               m_LastFunctionIndex;
    int               m_LastFuncTokenIdx;      //!< saved the function token's index, for remove all local variable tokens
    int               m_LastLine;
    wxString          m_LastNamespace;
    wxString          m_LastPROC;
    int               m_LastResult;
    /* CC Search Member Variables => END */

    //(ph 2024/01/25)
    bool m_ParsingIsBusy = false;
    bool m_UpdateClassBrowserViewBusy = false;
    bool m_ClassBrowserViewIsStale = true;
    bool m_SymbolsWindowHasFocus = false;

    //The latest project to change the .conf file   //(ph 2025/02/04)
    cbProject* m_pOptsChangedProject = nullptr;
    //The latest parser to change the .conf file    //(ph 2025/02/04)
    ParserBase* m_pOptsChangedParser = nullptr;
    // the currently closing parser                 //(ph 2025/02/15)
    ParserBase* m_pClosingParser     = nullptr;

    // copy of m_ParserList
    std::unordered_map<cbProject*,ParserBase*> m_ActiveParserList; //(ph 2025/02/14)

};

#endif // PARSEMANAGER_H

