/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CODECOMPLETION_H
#define CODECOMPLETION_H

#include <settings.h> // SDK
#include <cbplugin.h>
#include <cbproject.h>
#include <sdk_events.h>
#include <infowindow.h> //(ph 2021/06/28)
#include <filefilters.h>

//#include <iostream> //json input/output //(ph 2020/12/1)
//#include <fstream>

#include "coderefactoring.h"
#include "parsemanager.h"
#include "doxygen_parser.h"
#include "client.h"                  //(ph 2020/10/2)
#include "LSPEventCallbackHandler.h" //(ph 2021/10/23)

#if defined(_WIN32)
    #include "winprocess/misc/fileutils.h"               //(ph 2021/12/21)
#else
    #include "unixprocess/fileutils.h"
#endif //_Win32

#include <wx/arrstr.h>
#include <wx/listctrl.h>
#include <wx/string.h>
#include <wx/timer.h>

#include <map>
#include <unordered_map>
#include <vector>
#include <set>

class cbEditor;
class wxScintillaEvent;
class wxChoice;
class DocumentationHelper;

/** Code completion plugin has those features:
 * show tool-tip when the mouse hover over the variables/functions.
 * show call-tip when you hit the ( after the function name
 * automatically auto-completion lists prompted while entering code.
 * navigate the source files, jump between declarations and implementations.
 * find symbol usage, or even rename a symbol(code re-factoring).
 *
 * We later use "CC" as an abbreviation of Code Completion plugin.
 * See the general architecture of code completion plugin on wiki page
 *  http://wiki.codeblocks.org/index.php?title=Code_Completion_Design
 */
// ----------------------------------------------------------------------------
class ClgdCompletion : public cbCodeCompletionPlugin
// ----------------------------------------------------------------------------
{
public:
    /** Identify a function body's position, the underline data structure of the second wxChoice of
     * CC's toolbar
     */
    struct FunctionScope
    {
        FunctionScope() {}

        /** a namespace token can be convert to a FunctionScope type */
        FunctionScope(const NameSpace& ns):
            StartLine(ns.StartLine), EndLine(ns.EndLine), Scope(ns.Name) {}

        int StartLine;      ///< function body (implementation) start line
        int EndLine;        ///< function body (implementation) end line
        wxString ShortName; ///< function's base name (without scope prefix)
        wxString Name;      ///< function's long name (including arguments and return type)
        wxString Scope;     ///< class or namespace
    };

    /** vector containing all the function information of a single source file */
    typedef std::vector<FunctionScope> FunctionsScopeVec;

    /** helper class to support FunctionsScopeVec */
    typedef std::vector<int> ScopeMarksVec;


    struct FunctionsScopePerFile
    {
        FunctionsScopeVec m_FunctionsScope; // all functions in the file
        NameSpaceVec m_NameSpaces;          // all namespaces in the file
        bool parsed;                        // indicates whether this file is parsed or not
    };
    /** filename -> FunctionsScopePerFile map, contains all the opened files scope info */
    typedef std::map<wxString, FunctionsScopePerFile> FunctionsScopeMap;

    /** Constructor */
    ClgdCompletion();
    /** Destructor */
    ~ClgdCompletion() override;

    // the function below were virtual functions from the base class
    void OnAttach() override;
    void OnRelease(bool appShutDown) override;
    int  GetConfigurationGroup() const override { return cgEditor; }
    bool CanDetach() const override;

    /** CC's config dialog */
    cbConfigurationPanel* GetConfigurationPanel(wxWindow* parent) override;
    /** CC's config dialog which show in the project options panel */
    cbConfigurationPanel* GetProjectConfigurationPanel(wxWindow* parent, cbProject* project) override;
    /** build menus in the main frame */
    void BuildMenu(wxMenuBar* menuBar) override;
    /** build context popup menu */
    void BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data = 0) override;
    /** build CC Toolbar */
    bool BuildToolBar(wxToolBar* toolBar) override;
    /** toolbar priority value */
    int GetToolBarPriority() override { return 10; }

    // override virtual functions in cbCodeCompletionPlugin class (cbplugin.h calls from ccmanager)
    CCProviderStatus       GetProviderStatusFor(cbEditor* ed) override;
    std::vector<CCToken>   GetAutocompList(bool isAuto, cbEditor* ed, int& tknStart, int& tknEnd) override;
    std::vector<CCCallTip> GetCallTips(int pos, int style, cbEditor* ed, int& argsPos) override;
    wxString               GetDocumentation(const CCToken& token) override;
    std::vector<CCToken>   GetTokenAt(int pos, cbEditor* ed, bool& allowCallTip) override;
    // ccmanager to handle a click on a link in the documentation popup. cf: cbplugin.h
    wxString               OnDocumentationLink(wxHtmlLinkEvent& event, bool& dismissPopup) override;
    // callback from wxScintilla/ccmanager to insert a completion into the editor. cf: cbpluign.h
    void                   DoAutocomplete(const CCToken& token, cbEditor* ed) override;
    // ccmanager call to complete the editor insertion of a selected completion item
    void                   LSP_DoAutocomplete(const CCToken& token, cbEditor* ed); //(ph 2021/03/9)

    /** handle all the editor event */
    void EditorEventHook(cbEditor* editor, wxScintillaEvent& event);

    /** read CC's options, mostly happens the user change some setting and press APPLY */
    void RereadOptions(); // called by the configuration panel

    // Get the filename from a LSP response json string
    wxString GetFilenameFromLSP_Response(wxCommandEvent& event);  //(ph 2022/03/29)

    // Find wxTopLevelWindow from the windows list
    // ----------------------------------------------------------------------------
    wxWindow* GetWxTopLevelWindow()
    // ----------------------------------------------------------------------------
    {
        // travers the list of windows to get the top level window
        wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
        while (node)
        {
            //wxWindow*pwin = node->GetData(); removed for ticket #63 //(ac 2022/08/22)
            //if (node->GetNext() == nullptr)
            //    return pwin;
            if (not node->GetNext())               //ticket #63 support for msys2
            {
                wxWindow*pwin = node->GetData();
                return pwin;
            }
            node = node->GetNext();
        }
        return nullptr;
    }

    // Find top parent of a window
    // ----------------------------------------------------------------------------
    wxWindow* GetTopParent(wxWindow* pWindow)
    // ----------------------------------------------------------------------------
    {
        wxWindow* pWin = pWindow;
        while(pWin->GetParent())
                pWin = pWin->GetParent();
        return pWin;
    }

    // get top window to use as cbMessage parent, else MessageBoxes hide behind dialogs
    // ----------------------------------------------------------------------------
    wxWindow* GetTopWxWindow()
    // ----------------------------------------------------------------------------
    {
        wxWindow* appWindow = Manager::Get()->GetAppWindow();
        wxWindow* topWindow =GetWxTopLevelWindow();
        if (not topWindow)
            topWindow = appWindow;
        return topWindow;
    }
    // ----------------------------------------------------------------------------
    wxString GetwxUTF8Str(const std::string stdString)
    // ----------------------------------------------------------------------------
    {
        return wxString(stdString.c_str(), wxConvUTF8);
    }

private:
    void OnWindowActivated(wxActivateEvent& event); //on Window activated

    /** update CC's ToolBar, the user may disable the first wxChoice, so we need to recreate the
     * wxChoice and measure the best fit size
     */
    void UpdateToolBar();

    /** event handler for updating UI e.g. menu statues */
    void OnUpdateUI(wxUpdateUIEvent& event);

    /** event handler when user click Menu->View->Symbols browser */
    void OnViewClassBrowser(wxCommandEvent& event);

    /** event handler when user click Menu->Search->Goto function */
    void OnGotoFunction(wxCommandEvent& event);

    /** navigate to the previous function body */
    void OnGotoPrevFunction(wxCommandEvent& event);

    /** navigate to the next function body */
    void OnGotoNextFunction(wxCommandEvent& event);

    /** handle CC's context menu->insert "Class method declaration/implementation..." */
    void OnClassMethod(wxCommandEvent& event);

    /** handle CC's context menu->insert "All class methods without implementation..." */
    void OnUnimplementedClassMethods(wxCommandEvent& event);

    /** handle both goto declaration and implementation event */
    void OnGotoDeclaration(wxCommandEvent& event);

    /** CC's re-factoring function, find all the reference place */
    void OnFindReferences(wxCommandEvent& event);

    /** CC's re-factoring function, rename a symbol */
    void OnRenameSymbols(wxCommandEvent& event);

    /** open the include file under the caret position */
    void OnOpenIncludeFile(wxCommandEvent& event);

    /** event handler when user select context menu->reparse file/projects */
    void OnCurrentProjectReparse(wxCommandEvent& event);
    void OnReparseSelectedProject(wxCommandEvent& event);
    void OnSelectedFileReparse(wxCommandEvent& event);
    void OnLSP_SelectedFileReparse(wxCommandEvent& event);  //(ph 2021/05/13)
    void OnEditorFileReparse(wxCommandEvent& event);        //(ph 2021/11/16)
    void OnLSP_EditorFileReparse(wxCommandEvent& event);    //(ph 2021/11/16)
    void ClearReparseConditions();

    // event handlers for the standard events sent from sdk core
    /** SDK event when application has started up */
    void OnAppStartupDone(CodeBlocksEvent& event);
    void OnPluginEnabled(); //Used as CallAfter() ot OnAppStartupDone();

    /** SDK workspace related events */
    void OnWorkspaceChanged(CodeBlocksEvent& event);
    void OnWorkspaceClosingBegin(CodeBlocksEvent& event);
    void OnWorkspaceClosingEnd(CodeBlocksEvent& event);

    /** SDK project related events */
    void OnProjectActivated(CodeBlocksEvent& event);
    void OnProjectOpened(CodeBlocksEvent& event); //(ph 2020/10/3)
    void OnProjectClosed(CodeBlocksEvent& event);
    void OnProjectSaved(CodeBlocksEvent& event);
    void OnProjectFileAdded(CodeBlocksEvent& event);
    void OnProjectFileRemoved(CodeBlocksEvent& event);
    //    void OnProjectFileChanged(CodeBlocksEvent& event); never issued by codeblocks
    /** SDK editor related events */
    void OnEditorSave(CodeBlocksEvent& event);
    void OnEditorOpen(CodeBlocksEvent& event);
    void OnEditorActivated(CodeBlocksEvent& event);
    void OnEditorClosed(CodeBlocksEvent& event);
    void OnDebuggerStarting(CodeBlocksEvent& event);
    void OnDebuggerFinished(CodeBlocksEvent& event);
    void OnCompilerStarted(CodeBlocksEvent& event);
    void OnCompilerFinished(CodeBlocksEvent& event);
    void OnCompilerMenuSelected(wxCommandEvent& event);

    void OnEditorActivatedCallback(wxString filename, bool IsOpening=false); //(ph 2022/04/25)

    /** CC's own logger, to handle log events sent from other worker threads or itself(the main GUI
     * thread), the log messages will be printed in the "Code::Blocks" log panel.
     */
    void OnCCLogger(CodeBlocksThreadEvent& event);
    /** CC's own debug logger, to handle log event sent from other worker threads or itself(the main
     * GUI thread), the log messages will be printed in the "Code::Blocks Debug" log panel.
     */
    void OnCCDebugLogger(CodeBlocksThreadEvent& event);

    /** receive event from SystemHeadersThread */
    void OnSystemHeadersThreadMessage(CodeBlocksThreadEvent& event);
    void OnSystemHeadersThreadFinish(CodeBlocksThreadEvent& event);

    /** fill the tokens with correct preprocessor directives, such as #i will prompt "if", "include"
     * @param tknStart the start of the completed word
     * @param tknEnd current caret location
     * @param ed current active editor
     * @param tokens results storing all the suggesting texts
     */
    void DoCodeCompletePreprocessor(int tknStart, int tknEnd, cbEditor* ed, std::vector<CCToken>& tokens);

    /** ContextMenu->Insert-> declaration/implementation */
    int DoClassMethodDeclImpl();

    /** ContextMenu->Insert-> All class methods */
    int DoAllMethodsImpl();

    /** modify the string content to follow the current editor's code style
     * The code style includes the EOL, TAB and indent
     * @param[in,out] str the input string, but also the modified string
     * @param eolStyle an int value to indicate the EOL style
     * @param indent a wxString containing the whole intent text
     * @param useTabs whether TAB is used
     * @param tabSize how long is the TAB
     */
    void MatchCodeStyle(wxString& str, int eolStyle = wxSCI_EOL_LF, const wxString& indent = wxEmptyString, bool useTabs = false, int tabSize = 4);

    // CC's toolbar related functions
    /** helper method in finding the function position in the vector for the function containing the current line */
    void FunctionPosition(int &scopeItem, int &functionItem) const;

    /** navigate between function bodies */
    void GotoFunctionPrevNext(bool next = false);

    /** find the namespace whose scope covers the current line
     * the m_CurrentLine is used
     * @return  the found namespace index
     */
    int NameSpacePosition() const;

    /** Toolbar select event */
    void OnScope(wxCommandEvent& event);

    /** Toolbar select event */
    void OnFunction(wxCommandEvent& event);

    /** normally the editor has changed, then CC need to parse the document again, and (re)construct
     * the internal database, and refresh the toolbar(wxChoice's content)
     */
    void ParseFunctionsAndFillToolbar();

    /** the caret has changed, so the wxChoice need to be updated to indicates which scope and
     * function in which the caret locates.
     */
    void FindFunctionAndUpdate(int currentLine);

    /** the scope item has changed or becomes invalid, so the associated function wxChoice should
     * be updated.
     * @param scopeItem the new item in scope wxChoice.
     */
    void UpdateFunctions(unsigned int scopeItem);

    /** enable the two wxChoices */
    void EnableToolbarTools(bool enable = true);

    /** if C::B starts up with some projects opened, this function will be called to parse the
     * already opened projects
     */
    void DoParseOpenedProjectAndActiveEditor(wxTimerEvent& event);

    /** highlight member variables */
    void UpdateEditorSyntax(cbEditor* ed = NULL);

    /** delayed for toolbar update */
    void OnToolbarTimer(wxTimerEvent& event);
    void InvokeToolbarTimer(wxCommandEvent& event); //(ph 2022/08/31)

    /** delayed running of editor activated event, only the last activated editor should be considered */
    //-old- void OnEditorActivatedTimer(wxTimerEvent& event);
    void NotifyParserEditorActivated(wxCommandEvent& event);

    std::vector<ClgdCCToken>* GetCompletionTokens() {return &m_CompletionTokens;}

    /** Indicates CC's initialization is done */
    bool                    m_InitDone;

    /** menu pointers to the frame's main menu */
    wxMenu*                 m_EditMenu;
    wxMenu*                 m_SearchMenu;
    wxMenu*                 m_ViewMenu;
    wxMenu*                 m_ProjectMenu;

    std::unique_ptr<ParseManager> m_pParseManager;
    ParseManager* GetParseManager(){return m_pParseManager.get();}

    /** code re-factoring tool */
    CodeRefactoring*        m_pCodeRefactoring;

    int                     m_EditorHookId;

    /** timer triggered by editor hook function to delay the real-time parse */
    //wxTimer                 m_TimerRealtimeParsing;

    /** timer for toolbar
     *  we only show an updated item in CC's toolbar's item list when caret position is stable for
     *  a period of time.
     */
    wxTimer                 m_TimerToolbar;

    /** delay after receive editor activated event
     *  the reason we need a timer is that we want to get a stable editor activate information
     *  thus we will only handle the last editor activated editor
     *  The timer will be restart when an editor activated event happens.
     */
    wxTimer                 m_TimerEditorActivated;

    /** the last valid editor
     *  it is saved in editor activated event handler, and will be verified in editor activated timer
     *  event handler
     */
    cbEditor*               m_LastEditor;

    wxTimer                 m_TimerStartupDelay;

    // The variables below were related to CC's toolbar
    /** the CC's toolbar */
    wxToolBar*              m_ToolBar;
    /** function choice control of CC's toolbar, it is the second choice */
    wxChoice*               m_Function;
    /** namespace/scope choice control, it is the first choice control */
    wxChoice*               m_Scope;

    /** current active file's function body info
     *  @see CodeCompletion::ParseFunctionsAndFillToolbar for more details about how the data
     *  structure of the CC's toolbar is constructed
     */
    FunctionsScopeVec       m_FunctionsScope;

    /** current active file's namespace/scope info */
    NameSpaceVec            m_NameSpaces;

    /** current active file's line info, helper member to access function scopes
     *  @see CodeCompletion::ParseFunctionsAndFillToolbar for more details about how the data
     *  structure of the CC's toolbar is constructed
     */
    ScopeMarksVec           m_ScopeMarks;

    /** this is a "filename->info" map containing all the opening files choice info */
    FunctionsScopeMap       m_AllFunctionsScopes;

    /** indicate whether the CC's toolbar need a refresh, this means the toolbar list will be
     *  reconstructed
     */
    bool                    m_ToolbarNeedRefresh;

    /** force to re-collect the CC toolbar's item information
     *  this means we will parse the buffer to collect the scope information
     *  and then rebuild the toolbar items
     */
    bool                    m_ToolbarNeedReparse;

    /** current caret line, this is actually the saved caret line */
    int                     m_CurrentLine;

    /** the file updating the toolbar info */
    wxString                m_LastFile;

    /** indicate whether the predefined keywords set should be added in the suggestion list */
    bool                    m_LexerKeywordsToInclude[9];

    /** indicate the editor has modified by the user and a real-time parse should be start */
    //bool                    m_NeedReparse;

    /** remember the number of bytes in the current editor/document
     *  this is actually the saved editor or file's size
     */
    int                     m_CurrentLength;

    /** batch run UpdateEditorSyntax() after first parsing */
    bool                    m_NeedsBatchColour;

    //options on code completion (auto suggestion list) feature

    /** maximum allowed code-completion list entries */
    size_t                  m_CCMaxMatches;

    /** whether add parentheses after user selects a function name in the code-completion suggestion list */
    bool                    m_CCAutoAddParentheses;

    /** add function arguments' types and names when autocompleted outside function. The default
     * value is false.
     */
    bool                    m_CCDetectImplementation;

    /** user defined characters that work like Tab (empty by Default). They will be inserted
     *  with the selected item.
     */
    wxString                m_CCFillupChars;

    /** Delay for auto completion kick-in while typing.
     */
    int                     m_CCDelay;

    /** give code completion list for header files, it happens after the #include directive */
    bool                    m_CCEnableHeaders;

    /** do not allow code completion to add include files of projects/targets
     *  to the parser that are not supported by the current platform
     */
    bool                    m_CCEnablePlatformCheck;

    /** map to record all re-parsing files
     *
     * Here is an example how the ReparsingMap is used. Suppose you have two cbp opened:
     * a.cbp, which contains a1.cpp, a2.cpp and a3.cpp
     * b.cbp, which contains b1,cpp, b2,cpp and b3.cpp
     * now, if a1,cpp and b2.cpp b3.cpp are modified, and the user press the Save all button
     * Then CC receives event about project saved, then we store such information.
     * ReparsingMap contains such two elements
     * (a.cbp, (a1,cpp))
     * (b.cbp, (b2.cpp, b3.cpp))
     * there two elements will be passed to m_ParseManager, and m_ParseManager will distribute
     * to each Parser objects
     */

    /** Provider of documentation for the popup window */
    DocumentationHelper*     m_pDocHelper;

    // requires access to: m_ParseManager.GetParser().GetTokenTree()
    friend wxString DocumentationHelper::OnDocumentationLink(wxHtmlLinkEvent&, bool&);

private:
    /// @{ Code related to image storage. Images are different size and are used in the Auto
    /// Completion list.

    struct ImageId
    {
        enum Id : int
        {
            HeaderFile,
            KeywordCPP,
            KeywordD,
            Unknown,
            Last
        };

        ImageId() : id(Last), size(-1) {}
        ImageId(Id id, int size) : id(id), size(size) {}

        bool operator==(const ImageId &o) const
        {
            return id == o.id && size == o.size;
        }

        Id id;
        int size;
    };

    struct ImageIdHash
    {
        size_t operator()(const ImageId &id) const
        {
            return std::hash<uint64_t>()(uint64_t(id.id))+(uint64_t(id.size)<<32);
        }
    };

    typedef std::unordered_map<ImageId, wxBitmap, ImageIdHash> ImagesMap;
    ImagesMap m_images;

    wxBitmap GetImage(ImageId::Id id, int fontSize);
    /// @}

    static bool m_CCHasTreeLock;

    bool m_WorkspaceClosing = false;
    // ----------------------------------------------------------------------------
    /// Language Service Process ( LSP client )
    // ----------------------------------------------------------------------------
    static std::vector<ClgdCCToken> m_CompletionTokens; //cache
    std::vector<ClgdCCToken> m_HoverTokens;
    std::vector<CCCallTip> m_SignatureTokens;
    bool m_OnEditorOpenEventOccured = false;
    int  m_LastModificationMilliTime = 0;
    bool m_PendingCompletionRequest = false;
    int  m_MenuFleSaveFileID = 0;
    bool m_PluginNeedsAppRestart = false;
    int  m_HoverLastPosition = 0;
    bool m_HoverIsActive = false;
    cbProject* m_PrevProject = nullptr;
    cbProject* m_CurrProject = nullptr;
    wxString m_PreviousCompletionPattern = "~~abuseme~~";

    FileUtils fileUtils;

    // project pointers and their associated LSP client pointers
    typedef std::map<cbProject*, ProcessLanguageClient*> LSPClientsMapType;
    LSPClientsMapType m_LSP_Clients; //map of all LSP clients by project*

    wxString m_RenameSymbolToReplace; //Holds original target of RenameSymbols()
    wxString GetRenameSymbolToReplace() {return m_RenameSymbolToReplace;}

    // LSP legends for textDocument/semanticTokens
    std::vector<std::string> m_SemanticTokensTypes;
    std::vector<std::string> m_SemanticTokensModifiers;

    ProcessLanguageClient* m_pLSP_ClientForDebugging =  nullptr;

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
    // ----------------------------------------------------------------
    ProcessLanguageClient* GetLSPclient(cbProject* pProject)
    // ----------------------------------------------------------------
    {
        ProcessLanguageClient* pClient =  nullptr;
        if (not pProject) return nullptr;

        if (m_LSP_Clients.count(pProject))
            pClient =  m_LSP_Clients[pProject];
        if (pClient and pClient->GetLSP_Initialized(pProject))
            return pClient;

        return nullptr;
    }
    // ---------------------------------------------------------
    ProcessLanguageClient* GetLSPclient(cbEditor* pEd)
    // ---------------------------------------------------------
    {
        // Return client ptr or nullptr

        if (not pEd) return nullptr;
        ProjectFile* pProjectFile = pEd->GetProjectFile();
        if (not pProjectFile)
            return nullptr;
        cbProject* pEdProject = pProjectFile->GetParentProject();
        if (not pEdProject) return nullptr;
        if (GetLSPclient(pEdProject))
            return GetLSPclient(pEdProject);
        return nullptr;
    }

    ProcessLanguageClient* CreateNewLanguageServiceProcess(cbProject* pcbProject);                //(ph 2020/11/4)

    // ---------------------------------------------------------
    bool GetLSP_Initialized(cbProject* pProject)
    // ---------------------------------------------------------
    {
        ProcessLanguageClient* pClient = GetLSPclient(pProject);
        if (pClient and pClient->GetLSP_Initialized(pProject) )
            return true;
        return false;
    }
    // ---------------------------------------------------------
    bool GetLSP_Initialized(cbEditor* pEd)
    // ---------------------------------------------------------
    {
        ProjectFile* pPrjFile = pEd->GetProjectFile();
        if (not pPrjFile) return false;
        cbProject* pProject = pPrjFile->GetParentProject();
        if (not pProject) return false;
        ProcessLanguageClient* pClient = GetLSPclient(pProject);
        if (not pClient) return false;
        if (not pClient->GetLSP_Initialized(pProject) )
            return false;
        //-if (pClient->GetLSP_IsEditorParsed(pEd)) //(ph 2022/07/23)
        //-    return true;
        return true;
    }

    // ---------------------------------------------------------
    bool GetLSP_IsEditorParsed(cbEditor* pEd)       //(ph 2022/07/23)
    // ---------------------------------------------------------
    {
        ProjectFile* pPrjFile = pEd->GetProjectFile();
        if (not pPrjFile) return false;
        cbProject* pProject = pPrjFile->GetParentProject();
        if (not pProject) return false;
        ProcessLanguageClient* pClient = GetLSPclient(pProject);
        if (not pClient) return false;
        if (not pClient->GetLSP_Initialized(pProject) )
            return false;
        if (pClient->GetLSP_IsEditorParsed(pEd))
            return true;
        return false;
    }

    void OnLSP_Event(wxCommandEvent& event);
    void OnLSP_ProcessTerminated(wxCommandEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnPluginAttached(CodeBlocksEvent& event);
    void OnPluginLoadingComplete(CodeBlocksEvent& event);


    // Handle responses from LSPserver
    void OnLSP_ProjectFileAdded(cbProject* pProject, wxString filename);

    bool DoLockClangd_CacheAccess(cbProject* pcbProject);
    bool DoUnlockClangd_CacheAccess(cbProject* pcbProject);    //(ph 2021/03/13)
    void ShutdownLSPclient(cbProject* pProject);
    void CleanUpLSPLogs();
    void CleanOutClangdTempFiles();
    wxString GetLineTextFromFile(const wxString& file, const int lineNum); //(ph 2020/10/26)
    wxString VerifyEditorParsed(cbEditor* pEd);      //(ph 2022/07/25)
    wxString VerifyEditorHasSymbols(cbEditor* pEd);  //(ph 2022/07/26)

    IdleCallbackHandler* GetIdleCallbackHandler(cbProject* pProjectParm = nullptr)
    {
        cbProject* pProject = pProjectParm;
        if (not pProject)
            pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pProject and GetParseManager()->GetProxyProject())
            pProject = GetParseManager()->GetProxyProject();
        if (not pProject)
            cbAssert(pProject);
        ParserBase* pParser = GetParseManager()->GetParserByProject(pProject);
        if (not pParser) pParser = GetParseManager()->GetProxyParser();
        // **Debugging** Trying to catch on Linux first run, I get null GetIdleCallBackHandler() //(ph 2022/08/04)
        //      from ClgdCompletion::ParseFunctionsAndFillToolbar() 4688
        //      But when I added the following 2 checks, the asserts disappeared.
        if (not pParser)
            wxMessageBox(wxString::Format("NULL pParser: %s() %d",__PRETTY_FUNCTION__, __LINE__ ), "Assert");
        if (pParser and (not pParser->GetIdleCallbackHandler()))
            wxMessageBox(wxString::Format("NULL pParser->GetIdleCallbackHandler(): %s() %d",__PRETTY_FUNCTION__, __LINE__ ),"Assert");
        cbAssert(pParser && pParser->GetIdleCallbackHandler());
        return pParser->GetIdleCallbackHandler();
    }
    // ------------------------------------------------------------------------
    //LSP callbacks             (ph 2020/11/11)
    // ------------------------------------------------------------------------

    // LSPEventCallbackHandler pointer
    std::unique_ptr<LSPEventCallbackHandler> pLSPEventSinkHandler;    //(ph 2021/10/23)
    // Get pointer to LSP event callbacks
    LSPEventCallbackHandler* GetLSPEventSinkHandler(){return pLSPEventSinkHandler.get();}

    void OnSelectedPauseParsing(wxCommandEvent& event); //(ph 2021/07/28)

    // ----------------------------------------------------------------
    inline int GetCaretPosition(cbEditor* pEditor)
    // ----------------------------------------------------------------
    {
        if (not pEditor) return 0;
        cbStyledTextCtrl* pCntl = pEditor->GetControl();
        if (not pCntl) return 0;
        return pCntl->GetCurrentPos();
    }

    wxString GetTargetsOutFilename(cbProject* pProject);                  //(ph 2021/05/11)
    // Check if allowable files parsing are maxed out.
    bool ParsingIsVeryBusy();

    // This is set to false if ctor completes.
    // Forces CB restart when clangd_client first enabled
    bool m_CC_initDeferred = true;
    // Set to true when the old CodeCompletion plugin is enabled
    bool m_OldCC_enabled = true;
    // Initial condition of Clangd_Client at ctor (enabled/disabled);
    bool m_ctorClientStartupStatusEnabled = false;

    // FIXME (ph#): This is unecessary after a nightly for rev 12975 //(ph 2022/10/13)
    cbPlugin* m_pCompilerPlugin =  nullptr;

    // ----------------------------------------------------------------------------
    void SetClangdClient_Disabled()
    // ----------------------------------------------------------------------------
    {
        // if enable/disable status not set, assume a new enabled CB installation (true) so that disabling works
        bool m_Clangd_client_enabled = Manager::Get()->GetConfigManager(_T("plugins"))->ReadBool(_T("/clangd_client"), true );
        if (m_Clangd_client_enabled)
            Manager::Get()->GetConfigManager(_T("plugins"))->Write(_T("/clangd_client"), false );
        return ;//m_Clangd_client_enabled;
    }
    // ----------------------------------------------------------------------------
    bool IsOldCCEnabled()
    // ----------------------------------------------------------------------------
    {
        // Determine if CodeCompletion is enabled and its plugin lib exists.
        // Note: if the .conf has no info for the plugin CB reports it disabled but runs it anyway.
        wxString sep = wxFILE_SEP_PATH;
        bool bCCLibExists = false;
        bool oldCC_enabled = Manager::Get()->GetConfigManager("plugins")->ReadBool("/CodeCompletion");

        wxString oldCC_PluginLibName("codecompletion" + FileFilters::DYNAMICLIB_DOT_EXT);
        wxString ccLibFolder = ConfigManager::GetPluginsFolder(true); //Get global plugins folder
        bCCLibExists =  wxFileName(ccLibFolder + sep + oldCC_PluginLibName).Exists();
        if (not bCCLibExists) // Check if local plugins folder has codecompletion lib
        {
            wxString ccLibFolder = ConfigManager::GetPluginsFolder(false); //Get local plugins folder
            bCCLibExists = wxFileName(ccLibFolder + sep + oldCC_PluginLibName).Exists();
        }
        return (oldCC_enabled and bCCLibExists);
    }


    DECLARE_EVENT_TABLE()
};

#endif // CODECOMPLETION_H
