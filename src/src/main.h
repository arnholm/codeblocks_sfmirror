/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef MAIN_H
#define MAIN_H

#include <map>

#include <wx/aui/aui.h> // wxAuiManager
#include <wx/toolbar.h>
#include <wx/docview.h> // for wxFileHistory
#include <wx/notebook.h>
#include <wx/dynarray.h>
#include <cbeditor.h>
#include "manager.h"
#include "cbexception.h"
#include "cbplugin.h"
#include "find_replace.h"
#include "sdk_events.h"
#include "recentitemslist.h"
#include "scrollingdialog.h"

#include <unordered_map>

WX_DECLARE_HASH_MAP(int, wxString, wxIntegerHash, wxIntegerEqual, PluginIDsMap);
WX_DECLARE_HASH_MAP(cbPlugin*, wxToolBar*, wxPointerHash, wxPointerEqual, PluginToolbarsMap);
WX_DECLARE_STRING_HASH_MAP(wxString, LayoutViewsMap);

extern int idStartHerePageLink;
extern int idStartHerePageVarSubst;

class cbAuiNotebook;
class DebuggerMenuHandler;
class DebuggerToolbarHandler;
class InfoPane;
class wxGauge;
class cbProjectManagerUI;

struct ToolbarInfo
{
    ToolbarInfo() {}
    ToolbarInfo(wxToolBar *toolbar_in, const wxAuiPaneInfo &paneInfo_in, int priority_in) :
        paneInfo(paneInfo_in),
        toolbar(toolbar_in),
        priority(priority_in)
    {
    }

    bool operator<(const ToolbarInfo& b) const
    {
        return priority < b.priority;
    }

    wxAuiPaneInfo paneInfo;
    wxToolBar *toolbar;
    int priority;
};

class MainFrame : public wxFrame
{
    public:
        MainFrame& operator=(const MainFrame&) = delete;
        MainFrame(const MainFrame&) = delete;
    private:
        bool LayoutDifferent(const wxString& layout1, const wxString& layout2,
                             const wxString& delimiter);
        bool LayoutMessagePaneDifferent(const wxString& layout1, const wxString& layout2,
                                        bool checkSelection=false);
    public:

        MainFrame(wxWindow* parent = nullptr);
        ~MainFrame();

        bool Open(const wxString& filename, bool addToHistory = true);
        bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);
        void ShowTips(bool forceShow = false);

        wxScrollingDialog* GetBatchBuildDialog(){ return m_pBatchBuildDialog; }

        // show a file-open dialog and return the selection
        wxString ShowOpenFileDialog(const wxString& caption, const wxString& filter);
        // open the filename (based on what it is)
        bool OpenGeneric(const wxString& filename, bool addToHistory = true);

        void StartupDone();

        /** \brief Return true if the log pane is visible to the user
         */
        bool IsLogPaneVisible();

        cbProjectManagerUI* GetProjectManagerUI() { return m_pPrjManUI; }
    private:
        // event handlers

        void OnEraseBackground(wxEraseEvent& event);
        void OnApplicationClose(wxCloseEvent& event);
        void OnStartHereLink(wxCommandEvent& event);

        // the two functions below are used to show context menu to toggle toolbar view status
        // OnMouseRightUp is handler for right click on MainFrame's free area which is not covered by
        // any sub panels, OnToolBarRightClick is used to response the mouse right click command
        // on the toolbar.
        void OnMouseRightUp(wxMouseEvent& event);
        void OnToolBarRightClick(wxCommandEvent& event);

        // common function to show context menu for toggle toolbars
        void PopupToggleToolbarMenu();

        // File->New submenu entries handler
        void OnFileNewWhat(wxCommandEvent& event);

        void OnFileNew(wxCommandEvent& event);
        void OnFileOpen(wxCommandEvent& event);
        void OnFileReopenProject(wxCommandEvent& event);
        void OnFileOpenRecentProjectClearHistory(wxCommandEvent& event);
        void OnFileReopen(wxCommandEvent& event);
        void OnFileOpenRecentClearHistory(wxCommandEvent& event);
        void OnFileImportProjectDevCpp(wxCommandEvent& event);
        void OnFileImportProjectMSVC(wxCommandEvent& event);
        void OnFileImportProjectMSVCWksp(wxCommandEvent& event);
        void OnFileImportProjectMSVS(wxCommandEvent& event);
        void OnFileImportProjectMSVSWksp(wxCommandEvent& event);
        void OnFileSave(wxCommandEvent& event);
        void OnFileSaveAs(wxCommandEvent& event);
        void OnFileSaveProject(wxCommandEvent& event);
        void OnFileSaveProjectAs(wxCommandEvent& event);
        void OnFileSaveProjectTemplate(wxCommandEvent& event);
        void OnFileOpenDefWorkspace(wxCommandEvent& event);
        void OnFileSaveWorkspace(wxCommandEvent& event);
        void OnFileSaveWorkspaceAs(wxCommandEvent& event);
        void OnFileSaveAll(wxCommandEvent& event);
        void OnFileCloseWorkspace(wxCommandEvent& event);
        void OnFileClose(wxCommandEvent& event);
        void OnFileCloseAll(wxCommandEvent& event);
        void OnFileCloseProject(wxCommandEvent& event);
        void OnFilePrintSetup(wxCommandEvent& event);
        void OnFilePrint(wxCommandEvent& event);
        void OnFileQuit(wxCommandEvent& event);
        void OnFileNext(wxCommandEvent& event);
        void OnFilePrev(wxCommandEvent& event);

        void OnEditUndo(wxCommandEvent& event);
        void OnEditRedo(wxCommandEvent& event);
        void OnEditClearHistory(wxCommandEvent& event);
        void OnEditCopy(wxCommandEvent& event);
        void OnEditCut(wxCommandEvent& event);
        void OnEditPaste(wxCommandEvent& event);
        void OnEditSwapHeaderSource(wxCommandEvent& event);
        void OnEditGotoMatchingBrace(wxCommandEvent& event);
        void OnEditHighlightMode(wxCommandEvent& event);
        void OnEditHighlightModeUpdateUI(wxUpdateUIEvent &event);
        void OnEditFoldAll(wxCommandEvent& event);
        void OnEditUnfoldAll(wxCommandEvent& event);
        void OnEditToggleAllFolds(wxCommandEvent& event);
        void OnEditFoldBlock(wxCommandEvent& event);
        void OnEditUnfoldBlock(wxCommandEvent& event);
        void OnEditToggleFoldBlock(wxCommandEvent& event);
        void OnEditEOLMode(wxCommandEvent& event);
        void OnEditEncoding(wxCommandEvent& event);
        void OnEditParaUp(wxCommandEvent& event);
        void OnEditParaUpExtend(wxCommandEvent& event);
        void OnEditParaDown(wxCommandEvent& event);
        void OnEditParaDownExtend(wxCommandEvent& event);
        void OnEditWordPartLeft(wxCommandEvent& event);
        void OnEditWordPartLeftExtend(wxCommandEvent& event);
        void OnEditWordPartRight(wxCommandEvent& event);
        void OnEditWordPartRightExtend(wxCommandEvent& event);
        void OnEditZoomIn(wxCommandEvent& event);
        void OnEditZoomOut(wxCommandEvent& event);
        void OnEditZoomReset(wxCommandEvent& event);
        void OnEditLineCut(wxCommandEvent& event);
        void OnEditLineDelete(wxCommandEvent& event);
        void OnEditLineDuplicate(wxCommandEvent& event);
        void OnEditLineTranspose(wxCommandEvent& event);
        void OnEditLineCopy(wxCommandEvent& event);
        void OnEditLinePaste(wxCommandEvent& event);
        void OnEditLineMove(wxCommandEvent& event);
        void OnEditUpperCase(wxCommandEvent& event);
        void OnEditLowerCase(wxCommandEvent& event);
        void OnEditInsertNewLine(wxCommandEvent& event);
        void OnEditGotoLineEnd(wxCommandEvent& event);
        void OnEditInsertNewLineBelow(wxCommandEvent& event);
        void OnEditInsertNewLineAbove(wxCommandEvent& event);
        void OnEditSelectAll(wxCommandEvent& event);
        void OnEditSelectNext(wxCommandEvent& event);
        void OnEditSelectNextSkip(wxCommandEvent& event);
        void OnEditCommentSelected(wxCommandEvent& event);
        void OnEditUncommentSelected(wxCommandEvent& event);
        void OnEditToggleCommentSelected(wxCommandEvent& event);
        void OnEditStreamCommentSelected(wxCommandEvent& event);
        void OnEditBoxCommentSelected(wxCommandEvent& event);
        void OnEditShowCallTip(wxCommandEvent& event);
        void OnEditCompleteCode(wxCommandEvent& event);

        void OnEditBookmarksToggle(wxCommandEvent& event);
        void OnEditBookmarksNext(wxCommandEvent& event);
        void OnEditBookmarksPrevious(wxCommandEvent& event);
        void OnEditBookmarksClearAll(wxCommandEvent& event);

        void OnViewLayout(wxCommandEvent& event);
        void OnViewLayoutSave(wxCommandEvent& event);
        void OnViewLayoutDelete(wxCommandEvent& event);
        void OnViewScriptConsole(wxCommandEvent& event);
        void OnViewHideEditorTabs(wxCommandEvent& event);

        void OnSearchFind(wxCommandEvent& event);
        void OnSearchFindNext(wxCommandEvent& event);
        void OnSearchFindNextSelected(wxCommandEvent& event);
        void OnSearchReplace(wxCommandEvent& event);
        void OnSearchGotoLine(wxCommandEvent& event);
        void OnSearchGotoNextChanged(wxCommandEvent& event);
        void OnSearchGotoPrevChanged(wxCommandEvent& event);

        void OnPluginsExecuteMenu(wxCommandEvent& event);

        void OnSettingsEnvironment(wxCommandEvent& event);
        void OnSettingsKeyBindings(wxCommandEvent& event);
        void OnGlobalUserVars(wxCommandEvent& event);
        void OnBackticks(wxCommandEvent& event);
        void OnSettingsEditor(wxCommandEvent& event);
        void OnSettingsCompiler(wxCommandEvent& event);
        void OnSettingsDebugger(wxCommandEvent& event);
        void OnSettingsPlugins(wxCommandEvent& event);
        void OnSettingsScripting(wxCommandEvent& event);

        void OnHelpAbout(wxCommandEvent& event);
        void OnHelpTips(wxCommandEvent& event);
        void OnHelpPluginMenu(wxCommandEvent& event);

        void OnViewToolbarsFit(wxCommandEvent& event);
        void OnViewToolbarsOptimize(wxCommandEvent& event);
        void OnToggleBar(wxCommandEvent& event);
        void OnToggleStatusBar(wxCommandEvent& event);
        void OnFocusEditor(wxCommandEvent& event);
        void OnFocusManagement(wxCommandEvent& event);
        void OnFocusLogsAndOthers(wxCommandEvent& event);
        void OnSwitchTabs(wxCommandEvent& event);
        void OnToggleFullScreen(wxCommandEvent& event);
        void OnToggleStartPage(wxCommandEvent& event);

        // plugin events
        void OnPluginLoaded(CodeBlocksEvent& event);
        void OnPluginUnloaded(CodeBlocksEvent& event);
        void OnPluginInstalled(CodeBlocksEvent& event);
        void OnPluginUninstalled(CodeBlocksEvent& event);

        // general UpdateUI events
        void OnEditorUpdateUI(CodeBlocksEvent& event);

        void OnFileMenuUpdateUI(wxUpdateUIEvent& event);
        void OnEditMenuUpdateUI(wxUpdateUIEvent& event);
        void OnViewMenuUpdateUI(wxUpdateUIEvent& event);
        void OnSearchMenuUpdateUI(wxUpdateUIEvent& event);
        void OnProjectMenuUpdateUI(wxUpdateUIEvent& event);
        void OnUpdateCheckablePluginMenu(wxUpdateUIEvent &event);

        // project events
        void OnProjectActivated(CodeBlocksEvent& event);
        void OnProjectOpened(CodeBlocksEvent& event);
        void OnProjectClosed(CodeBlocksEvent& event);

        // dock/undock window requests
        void OnRequestDockWindow(CodeBlocksDockEvent& event);
        void OnRequestUndockWindow(CodeBlocksDockEvent& event);
        void OnRequestShowDockWindow(CodeBlocksDockEvent& event);
        void OnRequestHideDockWindow(CodeBlocksDockEvent& event);
        void OnDockWindowVisibility(CodeBlocksDockEvent& event);

        // layout requests
        void OnLayoutUpdate(CodeBlocksLayoutEvent& event);
        void OnLayoutQuery(CodeBlocksLayoutEvent& event);
        void OnLayoutSwitch(CodeBlocksLayoutEvent& event);

        // log requests
        void OnAddLogWindow(CodeBlocksLogEvent& event);
        void OnRemoveLogWindow(CodeBlocksLogEvent& event);
        void OnHideLogWindow(CodeBlocksLogEvent& event);
        void OnSwitchToLogWindow(CodeBlocksLogEvent& event);
        void OnGetActiveLogWindow(CodeBlocksLogEvent& event);
        void OnShowLogManager(CodeBlocksLogEvent& event);
        void OnHideLogManager(CodeBlocksLogEvent& event);
        void OnLockLogManager(CodeBlocksLogEvent& event);
        void OnUnlockLogManager(CodeBlocksLogEvent& event);

        // editor changed events
        void OnEditorOpened(CodeBlocksEvent& event);
        void OnEditorActivated(CodeBlocksEvent& event);
        void OnEditorClosed(CodeBlocksEvent& event);
        void OnEditorSaved(CodeBlocksEvent& event);
        void OnEditorModified(CodeBlocksEvent& event);
        void OnPageChanged(wxNotebookEvent& event);
        void OnShiftTab(wxCommandEvent& event);
        void OnCtrlAltTab(wxCommandEvent& event);
        void OnNotebookDoubleClick(CodeBlocksEvent& event);
        // Statusbar highlighting menu
        void OnHighlightMenu(wxCommandEvent& event);

        // Allow plugins to obtain copy of global accelerators
        void OnGetGlobalAccels(wxCommandEvent& event);

    protected:
        void CreateIDE();
        void CreateMenubar();
        void CreateToolbars();
        void ScanForPlugins();
        void AddToolbarItem(int id, const wxString& title, const wxString& shortHelp, const wxString& longHelp, const wxString& image);
        void RecreateMenuBar();
        void RegisterEvents();
        void SetupGUILogging(int uiSize16);
        void SetupDebuggerUI();
        void SafeAuiUpdate();

        void RegisterScriptFunctions();
        void RunStartupScripts();

        enum { Installed, Uninstalled, Unloaded };
        void PluginsUpdated(cbPlugin* plugin, int status);

        void DoAddPlugin(cbPlugin* plugin);
        ToolbarInfo DoAddPluginToolbar(cbPlugin* plugin);
        void DoAddPluginStatusField(cbPlugin* plugin);
        void AddPluginInPluginsMenu(cbPlugin* plugin);
        void AddPluginInHelpPluginsMenu(cbPlugin* plugin);
        wxMenuItem* AddPluginInMenus(wxMenu* menu, cbPlugin* plugin, wxObjectEventFunction callback, int pos = -1, bool checkable = false);

        void LoadViewLayout(const wxString& name, bool isTemp = false);
        void SaveViewLayout(const wxString& name, const wxString& layout, const wxString& layoutMP, bool select = false);
        void DoSelectLayout(const wxString& name);
        void DoFixToolbarsLayout();
        bool DoCheckCurrentLayoutForChanges(bool canCancel = true);

        void AddEditorInWindowMenu(const wxString& filename, const wxString& title);
        void RemoveEditorFromWindowMenu(const wxString& filename);
        int IsEditorInWindowMenu(const wxString& filename);
        wxString GetEditorDescription(EditorBase* eb);

        bool DoCloseCurrentWorkspace();
        bool DoOpenProject(const wxString& filename, bool addToHistory = true);
        bool DoOpenFile(const wxString& filename, bool addToHistory = true);
        void DoOnFileOpen(bool bProject = false);

        void DoUpdateStatusBar();
        void DoUpdateAppTitle();
        void DoUpdateLayout();
        void DoUpdateLayoutColours();
        void DoUpdateEditorStyle();
        void DoUpdateEditorStyle(cbAuiNotebook* target, const wxString& prefix, long defaultStyle);

        void ShowHideStartPage(bool forceHasProject = false, int forceState = 0);
        void ShowHideScriptConsole();

        void LoadWindowState();
        void SaveWindowState();
        void LoadWindowSize();

        void InitializeRecentFilesHistory();
        void TerminateRecentFilesHistory();
        #if wxUSE_STATUSBAR
        wxStatusBar *OnCreateStatusBar(int number, long style, wxWindowID id, const wxString& name) override;
        #endif
    private:
        wxAuiManager m_LayoutManager;
        LayoutViewsMap m_LayoutViews;
        LayoutViewsMap m_LayoutMessagePane;
        std::unique_ptr<wxAcceleratorTable> m_pAccel;
        std::unique_ptr<wxAcceleratorEntry[]> m_pAccelEntries;
        size_t m_AccelCount;

        RecentItemsList m_filesHistory, m_projectsHistory;

        /// "Close FullScreen" button. Only shown when in FullScreen view
        wxButton* m_pCloseFullScreenBtn;

        EditorManager*      m_pEdMan;
        ProjectManager*     m_pPrjMan;
        cbProjectManagerUI* m_pPrjManUI;
        LogManager*         m_pLogMan;
        InfoPane*           m_pInfoPane;

        wxToolBar* m_pToolbar; // main toolbar
        PluginToolbarsMap m_PluginsTools; // plugin -> toolbar map

        PluginIDsMap m_PluginIDsMap;
        wxMenu* m_ToolsMenu;
        wxMenu* m_PluginsMenu;
        wxMenu* m_HelpPluginsMenu;
        bool    m_ScanningForPlugins; // this variable is used to delay the UI construction

        bool m_StartupDone;
        bool m_InitiatedShutdown;

        int m_AutoHideLockCounter;
        int m_LastCtrlAltTabWindow; //!< Last window focussed in the cycle 1 = Mgmt. panel, 2 = Editor, 3 = Logs & others

        wxString m_PreviousLayoutName;
        wxString m_LastLayoutName;
        wxString m_LastLayoutData;
        wxString m_LastMessagePaneLayoutData;
        bool m_LastLayoutIsTemp;

        wxWindow* m_pScriptConsole;

        typedef std::map<int, const wxString> MenuIDToScript; // script menuitem ID -> script function name
        MenuIDToScript m_MenuIDToScript;

        typedef std::unordered_map<int, wxString> MenuIDToLanguage;
        MenuIDToLanguage m_MapMenuIDToLanguage;

        wxScrollingDialog* m_pBatchBuildDialog;

        DebuggerMenuHandler*    m_debuggerMenuHandler;
        DebuggerToolbarHandler* m_debuggerToolbarHandler;

        FindReplace m_findReplace;

        DECLARE_EVENT_TABLE()
};

#endif // MAIN_H
