/***************************************************************
 * Name:      debbugger_gdbmi
 * Purpose:   Code::Blocks plugin
 * Author:    Original template code Teodor Petrov a.k.a obfuscated (fuscated@gmail.com)
 *            Implemented for Codeblocks by pecan
 * Created:   2009-06-20 and implement 2024-03-04
 * Copyright: Teodor Petrov a.k.a obfuscated and CodeBlocks team
 * License:   GPL
 **************************************************************/


#ifndef _Debugger_GDB_MI_PLUGIN_H_
#define _Debugger_GDB_MI_PLUGIN_H_

// For compilers that support precompilation, includes <wx/wx.h>
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

//#include <memory>
#include <cbplugin.h> // for "class cbPlugin"
#include <editor_hooks.h>
#include <logmanager.h>

#include "debuggeroptionsdlg.h"
#include "cmd_queue.h"
#include "definitions.h"
//#include "events.h"
#include "gdb_executor.h"
#include "remotedebugging.h"

class TextCtrlLogger;
class Compiler;

namespace dbg_mi
{
class Configuration;
} // namespace dbg_mi


// ----------------------------------------------------------------------------
class Debugger_GDB_MI : public cbDebuggerPlugin
// ----------------------------------------------------------------------------
{

    public:
        /** Constructor. */
        Debugger_GDB_MI();
        /** Destructor. */
        virtual ~Debugger_GDB_MI();

    public:

        using GDBWatch = dbg_mi::Watch;
        virtual void SetupToolsMenu(wxMenu &menu);
        virtual bool ToolMenuEnabled() const { return true; }

        cbConfigurationPanel* GetProjectConfigurationPanel(wxWindow* parent, cbProject* project);
        virtual bool SupportsFeature(cbDebuggerFeature::Flags flag);

        virtual cbDebuggerConfiguration* LoadConfig(const ConfigManagerWrapper &config);

        //-dbg_mi::Configuration& GetActiveConfigEx(); //(ph 2024/03/11)
        DebuggerConfiguration& GetActiveConfigEx(); //(ph 2024/03/11)

        virtual bool Debug(bool breakOnEntry);
        virtual void Continue();
        virtual bool RunToCursor(const wxString& filename, int line, const wxString& line_text);
        virtual void SetNextStatement(const wxString& filename, int line);
        virtual void Next();
        virtual void NextInstruction();
        virtual void StepIntoInstruction();
        virtual void Step();
        virtual void StepOut();
        virtual void Break();
        virtual void Stop();

        virtual bool IsRunning() const;
        virtual bool IsStopped() const;
        virtual bool IsBusy() const;
        virtual int GetExitCode() const;
        void SetExitCode(int code) { m_exit_code = code; }

		// stack frame calls;
		virtual int GetStackFrameCount() const;
		virtual cbStackFrame::ConstPointer GetStackFrame(int index) const;
		virtual void SwitchToFrame(int number);
		virtual int GetActiveStackFrame() const;

        // breakpoints calls
        virtual cbBreakpoint::Pointer AddBreakpoint(const wxString& filename, int line);
        virtual void AddBreakpoint(dbg_mi::Breakpoint::Pointer breakpoint);
        virtual cbBreakpoint::Pointer AddDataBreakpoint(const wxString& dataExpression);
        virtual void RemoveProjectBreakpoints(const cbProject* project);
        virtual void RemoveProjectWatches(const cbProject* project);
        virtual int GetBreakpointsCount() const;
        virtual int GetBreakpointsCount(const cbProject* project);
        virtual int GetWatchesCount();
        virtual int GetWatchesCount(const cbProject* project);
        virtual cbBreakpoint::Pointer GetBreakpoint(int index);
        virtual cbBreakpoint::ConstPointer GetBreakpoint(int index) const;
        virtual void UpdateBreakpoint(cbBreakpoint::Pointer breakpoint);
        virtual void DeleteBreakpoint(cbBreakpoint::Pointer breakpoint);
        virtual void DeleteAllBreakpoints();
        virtual void ShiftBreakpoint(int index, int lines_to_shift);
        virtual void EnableBreakpoint(cb::shared_ptr<cbBreakpoint> breakpoint, bool enable);
        void ResetBreakpoint(dbg_mi::Breakpoint& breakpoint);

        // threads
        virtual int GetThreadsCount() const;
        virtual cbThread::ConstPointer GetThread(int index) const;
        virtual bool SwitchToThread(int thread_number);

        // watches
        virtual cb::shared_ptr<cbWatch> AddWatch(const wxString &symbol, bool updateImmediately);
        virtual cb::shared_ptr<cbWatch> AddMemoryRange(uint64_t address, uint64_t size, const wxString &symbol, bool update);
        void AddTooltipWatch(const wxString &symbol, wxRect const &rect);
        virtual void DeleteWatch(cb::shared_ptr<cbWatch> watch);
        virtual void AutoUpdateWatch(cb::shared_ptr<cbWatch> watch);
        virtual void GatherWatchesDone(int stepToPerform);
        void DeleteAllWatches();
        virtual bool HasWatch(cb::shared_ptr<cbWatch> watch);
        virtual void ShowWatchProperties(cb::shared_ptr<cbWatch> watch);
        virtual bool SetWatchValue(cb::shared_ptr<cbWatch> watch, const wxString &value);
        virtual void GetWatchValue(cb::shared_ptr<cbWatch> watch, wxString& value); // (ph 26/05/15)
        virtual void ExpandWatch(cb::shared_ptr<cbWatch> watch);
        virtual void CollapseWatch(cb::shared_ptr<cbWatch> watch);
        virtual void OnWatchesContextMenu(wxMenu& menu, const cbWatch& watch, wxObject* property, int& disabledMenus);

        virtual void OnMenuWatchDereference(cb_unused wxCommandEvent& event); //(ph 2024/03/08)
        virtual void OnMenuWatchSymbol(cb_unused wxCommandEvent& event);    //(ph 2024/03/08)

        virtual void UpdateWatch(cb::shared_ptr<cbWatch> watch);
        virtual void UpdateLocalsWatches();
        virtual void UpdateSingleWatch(cb::shared_ptr<cbWatch> watch);
        virtual void UpdateAllWatches();

        dbg_mi::WatchesContainer& GetWatchesContainer() {return m_watchesContainer;} //(ph 2024/03/05)

        virtual void SendCommand(const wxString& cmd, bool debugLog);

        virtual void AttachToProcess(const wxString& pid);
        virtual void DetachFromProcess();
        virtual bool IsAttachedToProcess() const;

        virtual void GetCurrentPosition(wxString &filename, int &line);
        virtual void RequestUpdate(DebugWindows window);

        virtual void OnValueTooltip(const wxString &token, const wxRect &evalRect);
        virtual bool ShowValueTooltip(int style);
        void NotifyCursorChanged();

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
        virtual void OnAttachReal();

        /** Any descendent plugin should override this virtual method and
          * perform any necessary de-initialization. This method is called by
          * Code::Blocks (PluginManager actually) when the plugin has been
          * loaded, attached and should de-attach from Code::Blocks.\n
          * Think of this method as the actual destructor...
          * @param appShutDown If true, the application is shutting down. In this
          *         case *don't* use Manager::Get()->Get...() functions or the
          *         behaviour is undefined...
          */
        virtual void OnReleaseReal(bool appShutDown);

    protected:
        virtual void ConvertDirectory(wxString& /*str*/, wxString /*base*/, bool /*relative*/);
        //virtual cbProject* GetProject() { return m_project; }
        virtual void ResetProject() { m_project = NULL; }
        virtual void CleanupWhenProjectClosed(cbProject *project);
        virtual void InitWhenProjectOpened(cbProject *project);
        virtual void InitWhenProjectActivated(cbProject *project);
        virtual bool CompilerFinished(bool compilerFailed, StartType startType);

    public:
        void UpdateWhenStopped();
        void UpdateOnFrameChanged(bool wait);
        dbg_mi::CurrentFrame& GetCurrentFrame() { return m_current_frame; }

        dbg_mi::GDBExecutor& GetGDBExecutor() { return m_executor; }

        virtual cbProject* GetProject() { return m_project; }
        virtual Compiler* GetCompiler(){return m_compiler;}
        virtual ProjectBuildTarget* GetTarget(){return m_target;}

        bool IsActionsMapEmpty(){return m_actions.Empty();}
        bool IsExecutorStopped() const {return m_executor.IsStopped();}
        bool IsExecutorRunning() const {return m_executor.IsRunning();}

    private:
        DECLARE_EVENT_TABLE();

        void OnGDBOutput(wxCommandEvent& event);
        void OnGDBTerminated(wxCommandEvent& event);
        void OnAppStartShutdown(CodeBlocksEvent& event);
        void OnUserClosingApp(wxCloseEvent& event);

        void OnTimer(wxTimerEvent& event);
        void OnIdle(wxIdleEvent& event);

        void OnMenuInfoCommandStream(wxCommandEvent& event);

        int LaunchDebugger(wxString const &debugger, wxString const &debuggee, wxString const &args,
                           wxString const &working_dir, int pid, bool console, StartType start_type);

    private:
        void AddStringCommand(wxString const &command);
        void DoSendCommand(const wxString& cmd);
        void RunQueue();
        void ParseOutput(wxString const &str);

        bool SelectCompiler(cbProject &project, Compiler *&compiler,
                            ProjectBuildTarget *&target, long pid_to_attach);
        int StartDebugger(cbProject *project, StartType startType);
        void CommitBreakpoints(bool force);
        void CommitRunCommand(wxString const &command);
        void CommitWatches();
        void DoLocalWatches(int stepToPerform = 1);   // (ph 26/05/10)

        void KillConsole();
        void OnDebuggerStoppedMode(CodeBlocksEvent& event);
        void EnableProgrammingFlash(bool trueOrFalse);

        void OnEditorEventHook(cbEditor* pcbEditor, wxScintillaEvent& event);
        void ResetCursor();
        void DetermineLanguage();

    private:
        wxTimer m_timer_poll_debugger;
        cbProject *m_project;

        Compiler* m_compiler;
        ProjectBuildTarget* m_target;

        dbg_mi::GDBExecutor m_executor;
        dbg_mi::ActionsMap  m_actions;
        dbg_mi::LogPaneLogger m_execution_logger;

        typedef std::vector<dbg_mi::Breakpoint::Pointer> Breakpoints;

        Breakpoints m_breakpoints;
        Breakpoints m_temporary_breakpoints;
        dbg_mi::BacktraceContainer m_backtrace;
        dbg_mi::ThreadsContainer m_threads;
        dbg_mi::WatchesContainer m_watchesContainer;
        wxString m_FileName = wxString();
        dbg_mi::Cursor m_Cursor;
        wxString m_LastCursorAddress;

        dbg_mi::MemoryRangeWatchesContainer m_memoryRanges;
        dbg_mi::MapWatchesToType m_mapWatchesToType;

        cb::shared_ptr<dbg_mi::Watch> m_localsWatch, m_funcArgsWatch;
        wxString m_watchToDereferenceSymbol;
        wxObject *m_watchToDereferenceProperty;

        wxString m_watchToAddSymbol;

        dbg_mi::TextInfoWindow *m_command_stream_dialog;

        dbg_mi::CurrentFrame m_current_frame;
        int m_exit_code;
        int m_console_pid;
        int m_Pid;
        int m_PidToAttach; //(ph 2025/01/24)
        bool m_hasStartUpError;
        cbProject* m_currentActiveProject;
        bool m_bAppStartShutdown;

        EditorHooks::HookFunctorBase* myEdhook;
        int m_EditorHookId;  // Editor/scintilla events hook ID
        int m_nEditorHookBusy;

    // -------------------------------------------------------------------------
    // Remote Debugging
    // -------------------------------------------------------------------------
    public:  //remote debugging
        RemoteDebuggingMap& GetRemoteDebuggingMap(cbProject* project);
        RemoteDebugging* GetRemoteDebuggingInfo(ProjectBuildTarget* pTarget);
        bool IsRemoteDebugging(ProjectBuildTarget* pTarget);
        void PrepareConsoleDebugging(bool isConsole); // (ph 26/01/08)
        void PrepareRemoteDebugging(/*bool isConsole,*/ int printElements);
        wxArrayString& GetSearchDirs(cbProject* prj);

    private: //remote debugging

        wxString GetTargetRemoteCmd(RemoteDebugging* rd);

        typedef std::map<cbProject*, RemoteDebuggingMap> ProjectRemoteDebuggingMap;
        ProjectRemoteDebuggingMap m_RemoteDebugging;
        // merged remote debugging (project-level + target-level)
        RemoteDebugging m_MergedRDInfo;
        void OnProjectLoadingHook(cbProject* project, TiXmlElement* elem, bool loading);
        void SaveProjectBreakpoints(cbProject* project);
        void GetBreakpointsIntoStrings(wxArrayString& brkptStrArray, cbProject* pProject);
        void LoadProjectBreakpoints(cbProject* project, TiXmlElement* elem, bool loading);
        void LoadProjectWatches(cbProject* pProject);
        void OnSaveProjectBreakpoints(wxCommandEvent& event);


        void OnProjectClosed(CodeBlocksEvent& event);   //(ph 2024/03/15)
        void OnProjectSave(CodeBlocksEvent& event);     //(ph 2024/03/16)

        wxString GetWXVariableValue(wxString varName);  //(ph 2025/01/17)
        wxString CreateGDBinitFile(cbProject* pProject, ProjectBuildTarget* pTarget); // (ph 26/06/18)
        wxString CreateGdbinitFile(cbProject* pProject, wxString wxValue);   //(ph 2025/01/18)
        wxString CreateGdbinitFile(cbProject* pPrjoect, ProjectBuildTarget* pTarget);   //(ph 2025/01/18)
        void     RemoveGdbinitFile(cbProject* pProject);


        // per-project debugger search-dirs
        typedef std::map<cbProject*, wxArrayString> SearchDirsMap;
        SearchDirsMap m_SearchDirs;

        int m_HookId; // project loader hook ID
        LogManager* GetLogMgr() {return Manager::Get()->GetLogManager();} //(ph 2024/03/04)
        DebuggerManager* GetDbgMgr() {return Manager::Get()->GetDebuggerManager();} //(ph 2024/03/04)
        dbg_mi::LogPaneLogger GetDebugLog() {return m_execution_logger;}
};
#endif // _Debugger_GDB_MI_PLUGIN_H_
//-----Release-Feature-Fix------------------
#define VERSION wxT("2.2.32 26/7/24")
//------------------------------------------
// Release - Current development identifier
// Feature - User interface level
// Fix     - bug fix or non UI breaking addition
//
//Versions
// 2026/07/24   2.2.32 Use different special watch root names from other debuggers
//                     to avoid wx3.3 new asserts. Like "Local vars" instead of "Locals"
// 2026/06/28   2.2.31 Hide debugging code for release
// 2026/06/20   2.2.31 Modify ParseListCommand() to avoid gdb child count errors
//                      GDB is sometimes returning a value=<trillions> of child(s)
//                      causing ParseListComand() to infinite loop on illusions.
// 2026/06/19   2.2.31 Moved OnWatches() into OnFrameChanged();
//                      Separated local and args watches;
//                      Set nullptr guards for watches container references;
// 2026/05/30   2.2.31 Moved watches updates when stepping to OnWatches()
// 2026/05/30   2.2.30 Finally got locals watches to update on stepping
// 2026/05/29   2.2.30 Display and auto update local watches and args
// 2026/05/21   2.2.29 Mark watches container as type ":container" to identify it
//                     as not a real watch. GDB throws a rejection fit over it.
// 2026/05/19   2.2.28 Show watches and args when config watches are requested.
// 2026/01/24   2.2.27 Clean up code with -Wextra.
// 2026/01/24   2.2.26 Add wxKILL_CHILDREN to wxKill calls.
// 2026/01/10   2.2.25 Send GDB "set new-console on" command when target is console application
// 2025/08/31   2.2.24 Allow "output.Contains(_("error,msg="))". It's now used for
//                     non critical, non abort messages in newer GDB like "var out of scope".
//                     See: cmd_queue.cpp line 168
// 2025/08/21   2.2.23 T Find cause of errors on shutdown of app
// 2025/05/14   Log critical error message in red
//              Queue command to abort debugger after critical errors
// 2025/04/02   2.2.22 If gdbinit file is size(0) do not use it.
// 2025/02/11   2.2.20 Fix SwitchFrameNotification to sync editor.
// 2025/02/10   2.2.19 Disable SwitchToDebuggingLayout(); It trashes log order
//                      FIXME: dbgGDBmi.cpp 1105
// 2025/01/22   2.2.18 Fix asserts in AttachToProcesss() code .
// 2025/01/19   2.2.17 if a gdbini file is already in the project dir, leave it as is.
// 2025/01/18   2.2.16 Create a gdbini file in the project dir to load pretty printers.
//                     CreateGdbinitFile(cbProject* pProject, wxString wxValue)
// 2025/01/15   2.2.15 Add gdb -ix gdbinit file to args if present in project dir.
// 2025/01/11   2.2.14 Switch to the Debugger log in StartDebugger()
// 2025/01/11   2.2.13 Enable User commands active_config.GetInitialCommands()
//                     dbgGDBmi.cpp 1020
// 2024/08/19   2.2.12 fixes to cpu register display
// 2024/04/29   2.2.11 Move saved bkpt data to external file and out of <EXTENSION>
//              so commit annoyance is avoided.
// 2024/04/25   2.2.10 Comment out Info::Display msg every time a break is toggled when
//              debugger is running. Cf: ParseStateInfo() //(ph 2024/04/25)
// 2024/04/24   2.2.09 Filter and reformat '&' error output msgs.
//              CommandExecutor::GetFilteredOutputError()
// 2024/04/12   2.2.08 Output msg colors according to their prefixed GDB/MI type.
//              *,+,~,&,=,@
//              See CommandExecutor::ProcessOutput() and the comments above it.
// 2024/04/11   2.2.07 Output msg when dbg response with an error
// 2024/04/10   2.2.06 24/04/10 For the working directory use the absolute path because
//                  GDB can change the currrent directory when only "." is specified
//                  as the target source working directory. It'll produce "no such file" errors.
// 2024/04/6    2.2.05 24/04/6  Save breakpoints support and Debug menu item
// 2024/04/6    2.2.06 24/04/7  Remove old debugging tags
