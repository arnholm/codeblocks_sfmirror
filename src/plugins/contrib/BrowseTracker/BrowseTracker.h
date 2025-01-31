/*
	This file is part of Browse Tracker, a plugin for Code::Blocks
	Copyright (C) 2007 Pecan Heber

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// RCS-ID: $Id$


#ifndef BROWSETRACKER_H_INCLUDED
#define BROWSETRACKER_H_INCLUDED

#include <map>
//#include <string>
#include <mutex>

#include "cbplugin.h" // for "class cbPlugin"
#include "cbeditor.h" // for "class cbPlugin"
#include "editormanager.h"  //(2021/06/19)
#include "BrowseTrackerDefs.h"
#include <logmanager.h>

class wxFileConfig;

class TiXmlElement;
class BrowseMarks;
class ProjectData;
class BrowseMarks;
class cbStyledTextCtrl;
class wxAuiNotebookEvent;
class JumpTracker;
class cbAuiNotebook;

///*
// * Struct for store tabs stack info //(2021/06/19) defined in editormanager.h:44
// */
//// ----------------------------------------------------------------------------
//struct cbNotebookStack
//// ----------------------------------------------------------------------------
//{
//    cbNotebookStack(wxWindow* a_pWindow = nullptr)
//        : window (a_pWindow),
//          next (nullptr)
//   {}
//
//    wxWindow*           window;
//    cbNotebookStack*    next;
//};

// ----------------------------------------------------------------------------
// The following have been moved to BrowseTrackerDefs.h
// ----------------------------------------------------------------------------
// a hash containing pointers to  cursor positions indexed by active editor pointers.
//WX_DECLARE_HASH_MAP(EditorBase*, BrowseMarks*, wxPointerHash, wxPointerEqual, EbBrowse_MarksHash);
// active editors
//WX_DEFINE_ARRAY_PTR(EditorBase*, ArrayOfEditorBasePtrs);
// ProjectData class pointers associating files to a loaded project
//WX_DECLARE_HASH_MAP(cbProject*, ProjectData*, wxPointerHash, wxPointerEqual, ProjectDataHash);
// ----------------------------------------------------------------------------
extern int gBrowse_MarkerId;
extern int gBrowse_MarkerStyle;
// ----------------------------------------------------------------------------
class BrowseTracker : public cbPlugin
// ----------------------------------------------------------------------------
{
    friend class BrowseTrackerConfPanel;

	public:
		/** Constructor. */
		BrowseTracker();
		/** Destructor. */
		~BrowseTracker();

		/** Invoke configuration dialog. */
		virtual int Configure() ;

		/** Return the plugin's configuration priority.
		 * This is a number (default is 50) that is used to sort plugins
		 * in configuration dialogs. Lower numbers mean the plugin's
		 * configuration is put higher in the list.
		*/
		int GetConfigurationPriority() const { return 50; }

		/** Return the configuration group for this plugin. Default is cgUnknown.
		 * Notice that you can logically AND more than one configuration groups,
		 * so you could set it, for example, as "cgCompiler | cgContribPlugin".
		*/
		int GetConfigurationGroup() const { return cgEditor; }

		/** Return plugin's configuration panel.
		  * @param parent The parent window.
		  * @return A pointer to the plugin's cbConfigurationPanel. It is deleted by the caller.
		  */
		cbConfigurationPanel* GetConfigurationPanel(wxWindow* parent) ;

		/** Return plugin's configuration panel for projects.
		 * The panel returned from this function will be added in the project's
		 * configuration dialog.
		 * @param parent The parent window.
		 * @param project The project that is being edited.
		 * @return A pointer to the plugin's cbConfigurationPanel. It is deleted by the caller.
		*/
		cbConfigurationPanel* GetProjectConfigurationPanel(wxWindow* /*parent*/, cbProject* /*project*/){ return 0; }

		/** This method is called by Code::Blocks and is used by the plugin
		 * to add any menu items it needs on Code::Blocks's menu bar.\n
		 * It is a pure virtual method that needs to be implemented by all
		 * plugins. If the plugin does not need to add items on the menu,
		 * just do nothing ;)
		 * @param menuBar the wxMenuBar to create items in
		*/
		void BuildMenu(wxMenuBar* menuBar);

		/** This method is called by Code::Blocks core modules (EditorManager,
		 * ProjectManager etc) and is used by the plugin to add any menu
		 * items it needs in the module's popup menu. For example, when
		 * the user right-clicks on a project file in the project tree,
		 * ProjectManager prepares a popup menu to display with context
		 * sensitive options for that file. Before it displays this popup
		 * menu, it asks all attached plugins (by asking PluginManager to call
		 * this method), if they need to add any entries
		 * in that menu. This method is called.\n
		 * If the plugin does not need to add items in the menu,
		 * just do nothing ;)
		 * @param type the module that's preparing a popup menu
		 * @param menu pointer to the popup menu
		 * @param data pointer to FileTreeData object (to access/modify the file tree)
		*/
		void BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data = 0);

		/** This method is called by Code::Blocks and is used by the plugin
		 * to add any toolbar items it needs on Code::Blocks's toolbar.\n
		 * It is a pure virtual method that needs to be implemented by all
		 * plugins. If the plugin does not need to add items on the toolbar,
		 * just do nothing ;)
		 * @param toolBar the wxToolBar to create items on
		 * @return The plugin should return true if it needed the toolbar, false if not
		*/
		bool BuildToolBar(wxToolBar* /*toolBar*/);/*{ return false; }*/
	protected:
		/** Any descendent plugin should override this virtual method and
		 * perform any necessary initialization. This method is called by
		 * Code::Blocks (PluginManager actually) when the plugin has been
		 * loaded and should attach in Code::Blocks. When Code::Blocks
		 * starts up, it finds and <em>loads</em> all plugins but <em>does
		 * not</em> activate (attaches) them. It then activates all plugins
		 * that the user has selected to be activated on start-up.\n
		 * This means that a plugin might be loaded but <b>not</b> activated...\n
		 * Think of this method as the actual constructor...
		*/
		void OnAttach();

		/** Any descendent plugin should override this virtual method and
		 * perform any necessary de-initialization. This method is called by
		 * Code::Blocks (PluginManager actually) when the plugin has been
		 * loaded, attached and should de-attach from Code::Blocks.\n
		 * Think of this method as the actual destructor...
		 * @param appShutDown If true, the application is shutting down. In this
		 *         case *don't* use Manager::Get()->Get...() functions or the
		 *         behaviour is undefined...
		*/
		void OnRelease(bool appShutDown);

		// ---

    public:

        // Stack holding sequence of activated editors //(2021/06/19)
        cbNotebookStack* GetNotebookStack();
        void DeleteNotebookStack();
        void RebuildNotebookStack();

        wxString    GetPageFilename(int TrackerIndex);
        wxString    GetPageFilename(EditorBase* eb);
        EditorBase* GetEditor(int index);
        int         GetEditor(EditorBase* eb);
        EditorBase* GetCurrentEditor();
        int         GetCurrentEditorIndex(EditorBase* pEb);
        EditorBase* GetPreviousEditor();
        int         GetPreviousEditorIndex();
        int         GetEditorBrowsedCount();
        void        SetSelection(int nEditorIndex);
        void        AddEditor(EditorBase* eb);
        void        RemoveEditor(EditorBase* eb);
        void        ClearEditor(EditorBase* eb);
        void        RecordBrowseMark(EditorBase* eb);
        void        ClearLineBrowseMark(bool removeScreenMark);
        void        ClearLineBrowseMark(int line, bool removeScreenMark);
        void        ImportBrowse_Marks(cbEditor* ed);
        void        RebuildBrowse_Marks(cbEditor* ed, bool addedlines);
        bool        IsBrowseMarksEnabled(){return m_BrowseMarksEnabled;}
        bool        IsWrapJumpEntriesEnabled(){return m_WrapJumpEntries;}
        bool        IsViewToolbarEnabled();
        void        ShowBrowseTrackerToolBar(const bool onOrOff);

        // Book Marks recording
        void        ToggleBook_Mark(EditorBase* eb);
        void        ClearLineBookMark();
        bool        LineHasBookMarker(cbStyledTextCtrl* pControl, int line) const;

        void        ReadUserOptions(wxString configFullPath);
        void        LoadConfOptions();

        void        SaveUserOptions(wxString configFullPath);
        void        SaveConfOptions();
        wxFileConfig* GetBrowseTrackerCfgFile(){return m_pCfgFile; }
        wxString      GetBrowseTrackerCfgFilename(){return m_CfgFilenameStr;}

        int         m_UpdateUIEditorIndex;

        // Activated editor stack maintenance
        void OnPageChanged(wxAuiNotebookEvent& event);
        void OnPageClose(wxAuiNotebookEvent& event);

    protected:
        bool            m_BrowseMarksEnabled; //user has enabled BrowseTracker
        int             m_OldUserMarksStyle;
        bool            m_OldBrowseMarksEnabled;
        bool            m_WrapJumpEntries;    //wrap jump entries when top or botton reached
        bool            m_ToolbarIsShown;     // Show BrowseTracker toolbar
        bool            m_ConfigShowToolbar;  // Show BrowseTracker toolbar
        bool            m_CfgActivatePrevEd;  // Activate previously activated editor
        int             m_CfgJumpViewRowCount;// Max number of JumpView rows

	private:

        wxString FindAppPath(const wxString& argv0, const wxString& cwd, const wxString& appVariableName);
        void     GetCurrentScreenPositions();

		void OnMenuTrackerClear(wxCommandEvent& event);
        void OnMenuTrackerSelect(wxCommandEvent& event);

		void OnMenuTrackerDump(wxCommandEvent& event);
		void OnUpdateUI(wxUpdateUIEvent& event);
		void OnIdle(wxIdleEvent& event);
        void OnWindowSetFocus(wxFocusEvent& event);

		void OnEditorDeactivated(CodeBlocksEvent& event);
		void OnEditorActivated(CodeBlocksEvent& event);
        void OnEditorOpened(CodeBlocksEvent& event);
        void OnEditorClosed(CodeBlocksEvent& event);
        void OnEditorEventHook(cbEditor* pControl, wxScintillaEvent& event);
        void OnMarginContextMenu(wxContextMenuEvent& event);

        void OnProjectOpened(CodeBlocksEvent& event);
        void OnProjectClosing(CodeBlocksEvent& event);
        void OnProjectActivatedEvent(CodeBlocksEvent& event);
        void OnProjectLoadingHook(cbProject* project, TiXmlElement* elem, bool loading);
        void OnWorkspaceChanged(CodeBlocksEvent& event);

        void OnStartShutdown(CodeBlocksEvent& event);
        void OnAppStartupDone(CodeBlocksEvent& event);

        //-void OnPageChanged(wxAuiNotebookEvent& event);
        void AppShuttingDown(CodeBlocksEvent& event); //2017/12/7

        void OnMenuBrowseMarkPrevious(wxCommandEvent& event);
        void OnMenuBrowseMarkNext(wxCommandEvent& event);
        void OnMenuRecordBrowseMark(wxCommandEvent& event);
        void OnMenuClearBrowseMark(wxCommandEvent& event);
        void OnMenuToggleBrowseMark(wxCommandEvent& event);
        void OnMenuClearAllBrowse_Marks(wxCommandEvent& event);
        void OnMenuSortBrowse_Marks( wxCommandEvent& event);
        void OnMenuSettings( wxCommandEvent& event);
        void OnConfigApply();

		void TrackEditorBackward();
		void TrackEditorForward();
        void OnMouseKeyEvent(wxMouseEvent& event);

        //-- BOOK marks
        void AddBook_Mark(EditorBase* eb, int line = -1);
        void OnBook_MarksToggle(wxCommandEvent& event);
        void CloneBookMarkFromEditor( int line );

        void TrackerClearAll();     //clear all active editors and BrowseMarks
        void ClearAllBrowse_Marks(bool clearScreenMarks);
        bool LineHasBrowseMarker(cbStyledTextCtrl* pControl, int line) const;
        void MarkerToggle(cbStyledTextCtrl* pControl, int line);
        void MarkerNext(cbStyledTextCtrl* pControl);
        void MarkerPrevious(cbStyledTextCtrl* pControl);
        void MarkLine(cbStyledTextCtrl* pControl, int line);
        void MarkRemove(cbStyledTextCtrl* pControl, int line);
        BrowseMarks* HashAddBrowse_Marks( const EditorBase* pEdBase);
        void SetBrowseMarksStyle( int userStyle);

        BrowseMarks* GetBrowse_MarksFromHash( EditorBase* eb);
        BrowseMarks* GetBrowse_MarksFromHash( wxString filePath);
        ProjectData* GetProjectDataFromHash(cbProject* pProject);
        ProjectData* GetProjectDataByProjectName( wxString filePath);
        ProjectData* GetProjectDataByEditorName( wxString filePath);
        cbProject*   GetProject(EditorBase* eb);
        bool         IsEditorBaseOpen(EditorBase* eb);

        void         DumpHash( wxString hashtype);

        bool            m_InitDone;
        wxString        m_CfgFilenameStr;

        wxFileConfig*   m_pCfgFile;

        EditorManager*  m_pEdMgr;
        ProjectManager* m_pPrjMgr;
		wxWindow*       m_pAppWin;
        wxMenuBar*      m_pMenuBar;
        wxToolBar*      m_pToolBar;

        wxString        m_ConfigFolder;
        wxString        m_AppName;
        wxString        TrackerCfgFullPath;
        bool            m_bProjectIsLoading;

        wxString        m_LoadingProjectFilename;
        int             m_ProjectHookId; // project loader hook ID
        int             m_EditorHookId;  // Editor/scintilla events hook ID
        //-int             m_CurrEditorIndex;
        int             m_LastEditorIndex;
        ArrayOfEditorBasePtrs  m_apEditors;
        int             m_nBrowsedEditorCount;
        EditorBase*     m_UpdateUIFocusEditor;
        EditorBase*     m_LastEbDeactivated;
        EditorBase*     m_PreviousEbActivated;
        EditorBase*     m_CurrentEbActivated;
        int             m_nBrowseMarkPreviousSentry;
        int             m_nBrowseMarkNextSentry;
        bool            m_OnEditorEventHookIgnoreMarkerChanges;

        EbBrowse_MarksHash m_EbBrowse_MarksHash;

        ProjectDataHash m_ProjectDataHash;

        int             m_CurrScrLine;
        int             m_CurrScrTopLine;
        int             m_CurrLinesOnScreen;
        int             m_CurrScrLastLine;

        int             m_CurrScrPosn;
        int             m_CurrScrTopPosn;
        int             m_CurrScrLastPosn;

        int             m_CurrScrLineStartPosn;
        int             m_CurrScrLineLength;
        int             m_CurrScrLineEndPosn;

        wxLongLong      m_MouseDownTime;
        long            m_MouseXPosn;
        long            m_MouseYPosn;
        bool            m_IsMouseDoubleClick;   //last mouse click was a DClick
        int             m_UserMarksStyle;       //BrowseMarks style Browse/Book/Hidden
        int             m_ToggleKey;            //Left_Mouse or Ctrl-Left_Mouse
        int             m_LeftMouseDelay;       //milliseconds before testing toggle
        int             m_ClearAllKey;          //Ctrl-Left_Mouse or Ctrl-Left_Mouse_DClick
        bool            m_bProjectClosing;      // project close in progress
        bool            m_bAppShutdown;
        int             m_nProjectClosingFileCount;
        int             m_EditorHookCurrentLine;

        //JumpTracker*    m_pJumpTracker;
        std::unique_ptr<JumpTracker> m_pJumpTracker;

        // Activated editors stack maintenance pointers //(2021/06/19)
        cbAuiNotebook*             m_pNotebook;
        cbNotebookStack*           m_pNotebookStackHead;
        cbNotebookStack*           m_pNotebookStackTail;
        size_t                     m_nNotebookStackSize;

        // A map to store file names and line numbers from the EditorHook
        std::multimap<cbEditor*, int> m_EditorHookFileLineMap;

        // Mutex for access to m_EditorHookFileLineMap.
        std::mutex m_EditorHookmapMutex;

        LogManager* m_pLogMgr;


		DECLARE_EVENT_TABLE();

}; //class BrowseTracker

#endif //BROWSETRACKER_H_INCLUDED
