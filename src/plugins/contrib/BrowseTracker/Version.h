// ----------------------------------------------------------------------------
/*
	This file is part of Code BrowseTracker, a plugin for Code::Blocks
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
// ----------------------------------------------------------------------------
// RCS-ID: $Id$

#ifndef VERSION_H
#define VERSION_H
// ---------------------------------------------------------------------------
// Logging / debugging
// ---------------------------------------------------------------------------
//debugging control
#include <wx/log.h>

#define LOGIT wxLogDebug
#if defined(LOGGING)
 #define LOGGING 1
 #undef LOGIT
 #define LOGIT wxLogMessage
 #define TRAP asm("int3")
#endif

// ----------------------------------------------------------------------------
   #if LOGGING
	extern wxLogWindow*    m_pLog;
   #endif
// ----------------------------------------------------------------------------
class AppVersion
// ----------------------------------------------------------------------------
{
    public:
        AppVersion();
       ~AppVersion();

    wxString GetVersion(){return m_version;}

    wxString m_version;

    protected:
    private:
};

#include <wx/string.h>
//-----Release-Feature-Fix------------------
#define VERSION wxT("1.4.120 25/01/30")
//------------------------------------------
// Release - Current development identifier
// Feature - User interface level
// Fix     - bug fix or non UI breaking addition
#endif // VERSION_H

// ----------------------------------------------------------------------------
// 1.4.120  2025/01/30 Rework OnEditorActivate to catch editors loaded via layout.
// 1.4.119  2025/01/29 Rework the EditorHook for efficiency.
// 1.4.118  2025/01/28 Fix assert "idx < m_size" failed in at() in AddEditor().
// 1.4.117  2024/06/18 Fix GetMaxEntries() (Helpers.cpp) to query conf only once.
// 1.4.116  2024/06/1 Allow user to set max entries for jump,browse marks etc.
// 1.4.115  2023/10/23 Deprecate OnEditorActivated() to stop OnEditorActivated() from
//          entering the current location of the activated editor before OnEditorUpdateUIEvent()
//          gets to enters the location of the Clangd_client response causing the next
//          jumpBack to jump to the incorrect location.
//          Allow user to set number of jump entries minimum to 10 for faster debugging..
// 1.4.114 Commit 2023/02/23
//          Fix crashes in jumptrackerView using unvalidated editor->Control() in Head 13210
// 1.4.113  Commit 23/02/13
//          2023/01/21 Allow user to set the number of JumpTracker view rows.
//          2023/01/20 JumpTracker view docking window
//          2023/01/20
//          Move BrowsTracker config file .ini usage to CB <personality>.conf file
//          Force usage of <personality>.conf file, then remove <personality.>BrowseTracker.ini file
// 1.2.113   Add sink event cbEVT_WORKSPACE_CHANGED to clear waiting load/close conditions
// 1.2.112   2021/11/27
//           Fixed crash when using the EditorManager to get the EditorBase* in OnEditorActivated().
//           Something has changed such that the EditorManager no longer knows the EditorBase* with:
//           ProjectManager::GetEditor(filename) during the Editor activated event.
//           Cf., https://forums.codeblocks.org/index.php?topic=24716.msg168611#msg168611
//  1.2.111  2021/06/19
//           Implemented EditorManager's stack based switching into BrowseTracker
//           Linux support, support for stack based editor switching
//  1.2.110  2021/05/6
//           Implement cb main.cpp method of switching editors and remove BrowseSelector.h/.cpp
//  pecan  1.2.108 2020/06/16
//           Fix focusing the wrong editor on active editor being close.
//           Add config item to enable/disable focusing previously active editor
//           http://forums.codeblocks.org/index.php?topic=23977.0;topicseen
//  pecan  1.2.107 2019/07/5
//           If no <personality.>BrowseTracker.ini use standalone BrowseTracker.ini
//  pecan  1.2.106 2019/04/6
//           Use wxFormBuilder 3.9.0 to fix wxFont deprecation in config.cpp
//  pecan  1.2.105 2018/12/18
//           Honor Toolbar activation/deactivation from View/Toolbars
//           Change config tool bar setting to "Show Toolbar Always"
//  pecan  1.2.104 2018/02/17
//           Re-patch wx30 and wx31 versions with shutdown fix 1.2.101
//           Left out when creating CB 17.12
//  pecan  1.2.103 2018/02/6
//           Port of above to CB 17.12 trunk
//           changes to includes and .cbp for CB 17.12
//  ICC    1.2.102 2018/01/6
//           Elimination use of BrowseMarksStyle
//           Set BookMarks as default. //2018/01/31-2018/01/2
//  ICC    1.2.101 2018/01/3
//           Fix shutdown crash //2017/12/7
//           Begin elimination of BrowseMarks
//           ICC restore BrowseTracker Toolbar, default unshown
//           Set BookMarks as default. //2018/01/31-2018/01/2
//  Commit 1.2.100 2014/10/9
//      100) Fix incorrect scintilla margin marker usage
//           Better resolution of Jump line recording
//           Add modifed user contrib tool bar (by sbezgodov)
//  Commit 1.2.99 2012/11/18
//       99) remove shadowed var from GetCurrentScreenPosition()
//  Commit 1.2.98 2012/08/10
//       98) Add Wrap Jump Entries option
//  Commit 1.2.97 2012/01/11
//       96) Record last position for deactivated editor
//           Remove recording position for activated editor
//  Commit 1.2.96 2011/12/13
//       96) Remove wrap on JumpTracker jump back/forward
//  Commit 1.2.95 2010/06/30
//       95) Do not record firt source line in JumpTracker
//  Commit 1.2.94 2010/02/25
//       94) Apply patch 2886 by techy

//  Commit 1.2.93 2010/02/19
//       93) Diable Ctrl-Left_Mouse key usage when user sets editor multi-selection enabled.
// ----------------------------------------------------------------------------
//  Commit 1.2.92 2009/12/11
//       91) Clear m_bProjectClosing in OnProjectOpened() else no initial activation recorded after project closed.
//       92) Fix JumpTracker inablility to switch between editors (caused by Editor Activation fix)
// ----------------------------------------------------------------------------
//  Commit 1.2.90 2009/11/30
//       86) Add Shutdown test to OnIdle
//       87) OnCloseEditor, Activate the previously active edtor, not the last tab
//           EditorManager::OnUpdateUI() used to do this. wxAuiNotebook broke it.
//       88) OnProjectClosing() ignore recording closing editors
//           OnProjectActivated() activate the current edtior for this project (not last tab).
//       89) Record last deactivated editor; OnEditorClose activate last deactivated editor (vs. last tab)
//       90) Fixed: loop in OnIdle() after svn 5939 changes
// ----------------------------------------------------------------------------
//  Commit 1.2.85 2009/11/9
//       81) Set browse marks sorting flag in OnEditorActivated()
//       82) Set BrowseSelector width window by filename width
//       83) Added JumpTracker; record each activated cursor posn within a half-page
//       84) Activate previously active editor when secondary project closes.
//       85  Fix crash when disabling plugins (in BuildMenu)
// ----------------------------------------------------------------------------
//  Commit  1.2.80 2009/07/22
//      79) Call OnEditorActivated() from OnEditorOpened() because editors actived
//          by Alt-G, "Swap header/source", and "Recent files" have no cbEditor
//          associated in EVT_EDITOR_ACTIVATED, and GetActiveEditor() returns NULL.
//      80) Hack to find editor's project. Since wxAuiNotebook, the initial
//          EVT_EDITOR_ACTIVATED has no cbEditor or cbProject associated.
// ----------------------------------------------------------------------------
//  Commit 1.2.78 2009/07/13
//         77) Fix activation by keyboard after wxAuiNotebook added.
//         78) Sort browse marks in idle time.
// ----------------------------------------------------------------------------
//  Commit 1.2.76 2009/04/28
//         76) Add include ConfigManager for linux
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  Commit 1.2.75 2009/04/26
//         75) Added GetCBConfigDir() to call routines that check for APPDATA var
// ----------------------------------------------------------------------------
//  Commit 1.2.74 2008/03/15
//         74) changed user interface
//             Added Config settings panel to CB config settings menu
// ----------------------------------------------------------------------------
//  Commit 1.2.72 2008/02/19
//         72) guard all asm(int3) with defined(LOGGING)
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  Commit 1.2.71 2008/02/18
//         71) removed unnecessary debugging files ToolBox.*
// ----------------------------------------------------------------------------
//  Commit 1.2.70 2008/02/13
//         69) Fixed: Setting a BookMark via the margin context menu is not seen by BT 2008/01/25
//         70) rework sizers on settings dlg
// ----------------------------------------------------------------------------
//  Commit 1.2.68 2008/01/24
//         67) guard against null project pointer when importing projects
//             Sdk is issueing project events without a project pointer.
//             Eg. importing Visual Studio Solution project
//         68) OnProjectOpen: turn off ProjectLoading flag when no project pointer
// ----------------------------------------------------------------------------
//  Commit 1.2.66 2008/01/13
//         65) Use m_bProjectIsLoading in OnProjectOpened() to avoid scanning editors
//         66) Don't set marks on mouse drags
//         66) Release ProjectLoadingHook in OnRelease()
//  Commit 1.2.64 2008/01/4
//         59) update Makefile.am
//         60) Re-instated use of ProjectLoadingHook. Hook is skipped when extra </unit>
//              in project.cbp after file adds. Re-saving project.cbp solves problem.
//         61) Finish code to shadow Book marks as BrowseMark.
//         62) Refactored ClearLineBrowseMarks/ClearLineBookMarks/ToggleBook_Mark
//         63) Don't initiate a BrowseMark on initial load of source file.
//         64) Correct linux cbstyledtextctrl.h spelling
// ----------------------------------------------------------------------------
//  Commit 1.2.58 2008/01/2
//         58) #include for cbStyledTextCtrl.h CB refactoring
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  Commit 1.2.57 2008/01/2
//         51) Added: user request: Hidden BrowseMarks
//         52) Fixed: Editor selection dialog stalls after "Clear All"
//         53) Added: User choice of BrowseMarks, BookMarks, or HiddenMarks
//         54) Fixed: Don't set BrowseMark on doubleclick
//         55) On clear all: Also clear BookMarks when being used as BrowseMarks
//         56) Remove use of LoadingProjectHook since it's not called if project
//              file has no xml "extensions" node.
//         57) Remove use of IsLoading() since SDK actually turns it OFF during
//              editor loading and activation.
// ----------------------------------------------------------------------------
//  Commit  1.2.49 2007/12/21
//          49) Add Settings dialog and support for Ctrl-Left_Mouse toggling
//              and Ctrl-Left_Mouse Double click clearAll.
//              See Menu/Browse Tracker/Settings
//          50) Changed Cfg.cpp labels back to default colors
// ----------------------------------------------------------------------------
//  Commit  1.2.48 2007/12/17
//          45) Fixed layout loading in multi-project workspace
//          46) Fixed frozen dialog popup in multi-project workspace
//          47) Decoupled ProjectData from dependency on EditorBase indexes.
//          48) Added Search for ProjectData/cbProject by fullPath
// ----------------------------------------------------------------------------
//  Commit  1.2.44 2007/12/16
//          30) Ctrl+LeftMouse click on non-marked line clears *all* markers;
//              Ctrl+LeftMouse click on marked line clears the line markers;
//          31) Added BrowseTracker ProjectData class
//          32) Added BrowseTrackerLayout class
//          33) Fixed: Crash when no project and file opened at StartUp
//          34) Persistent Browse_Marks vis Load/Save layout file.
//          35) Restore BrowseMarks when a previously active editor closed/reopened
//          36) Toggle BrowseMark with Ctrl+LeftMouseClick
//          37) Convert BookMarks class to use full file names as index to save
//              Browse/Book marks of unactivated files in project
//          39) Persistent Book_Marks via Load/Save Layout file "<projectName>.bmarks"
//          39) Fix: Browse/Book marks not saved when fileShortName used
//          40) Save Browse/Book marks when CB exited w/o closing projects
//          41) Ignore Browse/Book marks for files outside projects in EVT_EDITOR_CLOSE
//          42) Mouse must be down 250 millisecs to toggle BrowseMark. Better control.
//          43) Ignore Left-Double-clicks that read like a long left click.
//          44) Fix non-pch includes for linux
// ----------------------------------------------------------------------------
//  commit  1.2.29 2007/12/9
//          25) add sort and "Sort BrowseMarks" menu  command
//          26) On ClearAllMarkers(), tell scintilla to also clear visible BrowseMarks.
//          27) shadow scintilla BrowseMarks when lines added/deleted; keeps marks in user set order
//          28) append BrowseTracker menu to the context popup menu
//          29) remove OnEditorActivated() code causing linux to re-set deleted markers
//  commit 1.2.24 2007/12/8
//         16) ignore recording BrowseMarks on duplicate editor activations
//             eg. activations from double clicking search results panel
//         17) fixed GetCurrentScreenPositions() when doc less than screen size
//         18) honor WXK_RETURN to dismiss selector dialog
//         19) honor WXK_RETURN to dismiss wxListBox in dialog
//         20) convert editor pointers to circular queue
//         21) compress active browsed editor array for better availability
//         22) correct active editor order when using selection dlg
//         23) fix index overflow crash in BrowseMarks.cpp::ClearMarks()
//         24) implement visable browseMarks as "..."
// ----------------------------------------------------------------------------
//  commit  1.2.15 2007/12/4
//          6) remove redundancy of initial editor BrowseMark
//          7) record "previous position" forward; allowing easy copy/paste operations
//          8) add "Clear All BrowseMarks" menu item for a single editor
//          9) don't show navigation dlg when no active cbEditors
//         11) Clear previous marks on line when setting a new one (unless Ctrl held)
//         12) when current browsemark off screen, go to "current", not "previous/next"
//         13) save BrowseMarks in circular queue. More understandable interface.
//         14) switch Ctrl-LeftMouse click to delete browse marks on line.
//         15) allow multiple browse marks on page, but only one per line
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  commit  1.2.5 2007/11/30
//          4) correct case of some filenames
//          5) Add menu items "Set BrowseMark", "Clear BrowseMark"
// ----------------------------------------------------------------------------
//  commit  1.2.3 2007/11/29
//          2) re-add previous browse mark clobbered by copy/paste so user
//             can (most likely) return to paste insertion area.
//          3) fixed editor activation when multiple projects close
//  commit  1.2.1 2007/11/27
//          1) implement browse markers within each editor. Cursor position is memorized
//             one per page on each left mouse key "up". User can force the browse mark
//             by holding ctrl+left-mouse-up. (Alt-mouse-up) seems to be hogged by the
//             frame manager.
//             Alt+Up & Alt+Dn keys cycle through the memorized BrowseMarks
// ----------------------------------------------------------------------------
//  Commit  0.1.11 2007/08/29
//          11) fixes for sdk 1.11.12. SDK removed Manger::GetAppWindow()
// ----------------------------------------------------------------------------
//          0.1.12 2007/11/18
//          11) use OnIdle to focus the new activated editor. Else its visible but dead.
//              SDK.editormanager::OnUpdateUI used to do this, but now it's gone.
// ----------------------------------------------------------------------------
//  Commit  0.1.10 2007/08/2
//          09) fixes for editor activation changes caused by wxFlatNotebook 2.2
//          10) fix for non-focused dialog in wxGTK284
// ----------------------------------------------------------------------------
//  Commit  0.1.8 2007/07/11
//          08) changes for sdk RegisterEventSink

// ----------------------------------------------------------------------------
//  Commit  0.1.3 2007/06/2
//          01) Intial working Browse Tracker
//          02) Added ProjectLoadingHook to get around ProjectActivate and ProjectOpen event bugs
//              Dont store Editor ptrs while Project_Open event is occuring
//              BUG: CB EVT_PROJECT_ACTIVATE is occuring before EVT_PROJECT_OPEN
//              BUG: Editor Activate events occur before Project_Open and Project_Activated
//          03) Add Clear Menu item to clear recorded Editors
//          04) Navigation popup dialog window
//          05) create Makefile.am BrowseTracker-unix.cbp and .cbplugin(s)
//          06) changed "Opened tabs" to "Browsed Tabs"
//          07) Honor the --personality arg when finding .ini file
// ----------------------------------------------------------------------------
//  //FIXME: Bugs
//      01) Requires CB to be restarted after Install before Alt-Left/Right work.
//          When CB reloads a changed editor, the marks are missing
//       3) On first project load, browse/book marks dont set bec there's no active editor in arrays
// ----------------------------------------------------------------------------
//  //TODO:   All
//          Config dialog: Max tracked editors Max tracked lines etc
//          Navigation toolbar arrows
//          Shadow the menuitem cmdkey definitions w/ wxMenuItem->GetAccel()
//          Selection/history of Marks via dlg
// ----------------------------------------------------------------------------
