#include <sdk.h> // Code::Blocks SDK

#include <cbplugin.h>
#include "dbgGDBmi.h"

#include <algorithm>
#include <wx/xrc/xmlres.h>
//#include <wx/wxscintilla.h>
#include <wx/fileconf.h>
#include <wx/tokenzr.h>
#include <wx/busyinfo.h>

#include <globals.h>
#include <cbdebugger_interfaces.h>
#include <compilerfactory.h>
#include <cbproject.h>
#include <configurationpanel.h>
#include <configmanager.h>
#include <editbreakpointdlg.h>
#include <infowindow.h>
#include <macrosmanager.h>
#include <pipedprocess.h>
#include <projectmanager.h>
#include <databreakpointdlg.h>
#include <editormanager.h>
#include <cbeditor.h>
#include <cbstyledtextctrl.h>
#include <sdk_events.h>
#include <uservarmanager.h>
#include "events.h"

#include "actions.h"
#include "cmd_result_parser.h"
//-#include "config.h"
#include "debuggeroptionsdlg.h" //(ph 2024/03/11)
#include "escape.h"
#include "frame.h"
#include "helpers.h"
#include "editwatchdlg.h"
#include "definitions.h"
#include "compilercommandgenerator.h"
#include "remotedebugging.h"
#include "projectloader_hooks.h"
#include "debuggeroptionsprjdlg.h"
#include "debuggermanager.h"    // (ph 26/05/20)
//#include "loggers.h"             // (ph 26/05/20)
#include "json_node.h" //save/load brakpoints

#ifndef __WX_MSW__
#include <dirent.h>
#include <stdlib.h>
#endif


// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{
    PluginRegistrant<Debugger_GDB_MI> reg(_T("debugger_gdbmi"));
    // wxString GetDebuggersName(){return _T("debugger_gdbmi");}    // **Debugging**
    #if defined(LOGGING)
        #warning INFO: This is debugging version GDB/MI (cf: conpiler define for LOGGING)
    #endif

    int const id_gdb_process = wxNewId();
    int const id_gdb_poll_timer = wxNewId();
    int const id_menu_info_command_stream = wxNewId();
    int const idDebugMenuSaveBreakpoints = wxNewId();

    long idMenuWatchDereference = wxNewId();
    long idMenuWatchSymbol = wxNewId();

    wxString m_Version = VERSION;
    wxString breakpointFilename = "dbgrSavedData.json";

    bool wxFound(int result){return result != wxNOT_FOUND;};
    wxString fileSep = wxFILE_SEP_PATH;

    wxString GetLibraryPath(const wxString &oldLibPath, Compiler *compiler, ProjectBuildTarget *target)
    {
        cbProject* project = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (compiler && target)
        {
            wxString newLibPath;
            const wxString libPathSep = platform::windows ? _T(";") : _T(":");
            newLibPath << _T(".") << libPathSep;
            CompilerCommandGenerator* generator = compiler->GetCommandGenerator(project);
            newLibPath << GetStringFromArray(generator->GetLinkerSearchDirs(target), libPathSep);
            if (newLibPath.Mid(newLibPath.Length() - 1, 1) != libPathSep)
                newLibPath << libPathSep;
            newLibPath << oldLibPath;
            return newLibPath;
        }
        else
            return oldLibPath;
    }

} // anonymous namespace

// ----------------------------------------------------------------------------
// events handling
// ----------------------------------------------------------------------------
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
BEGIN_EVENT_TABLE(Debugger_GDB_MI, cbDebuggerPlugin)

    EVT_MENU(idMenuWatchDereference, Debugger_GDB_MI::OnMenuWatchDereference)
    EVT_MENU(idMenuWatchSymbol, Debugger_GDB_MI::OnMenuWatchSymbol)

    EVT_PIPEDPROCESS_STDOUT(id_gdb_process, Debugger_GDB_MI::OnGDBOutput)
    EVT_PIPEDPROCESS_STDERR(id_gdb_process, Debugger_GDB_MI::OnGDBOutput)
    EVT_PIPEDPROCESS_TERMINATED(id_gdb_process, Debugger_GDB_MI::OnGDBTerminated)

    EVT_IDLE(Debugger_GDB_MI::OnIdle)
    EVT_TIMER(id_gdb_poll_timer, Debugger_GDB_MI::OnTimer)

    EVT_MENU(id_menu_info_command_stream, Debugger_GDB_MI::OnMenuInfoCommandStream)
END_EVENT_TABLE()
#pragma GCC diagnostic pop
// constructor
// ----------------------------------------------------------------------------
Debugger_GDB_MI::Debugger_GDB_MI()
// ----------------------------------------------------------------------------
    : cbDebuggerPlugin(_T("GDB/MI"), _T("debugger_gdbmi")), //GUI name, plugin setting name
       #if defined(LOGGING)
           #warning INFO: This is debugging/logging version GDB/MI (cf: compiler define for LOGGING)
       #endif
       m_project(nullptr),
       m_execution_logger(this),
       m_command_stream_dialog(nullptr),
       m_console_pid(-1),
       m_Pid(0),
       m_PidToAttach(0)
       //m_pid_to_attach(0) //(ph 2025/01/22)
{
    // Make sure our resources are available.
    if(not Manager::LoadResource(_T("debugger_gdbmi.zip")))
    {
        NotifyMissingFile(_T("debugger_gdbmi.zip"));
    }
    //Note  m_executor; is initialized implicitly since it's an object, not a pointer.
    m_executor.SetLogger(&m_execution_logger);
    ResetCursor();
}
// destructor
// ----------------------------------------------------------------------------
Debugger_GDB_MI::~Debugger_GDB_MI()
// ----------------------------------------------------------------------------
{
    //dtor
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnAttachReal()
// ----------------------------------------------------------------------------
{
    m_timer_poll_debugger.SetOwner(this, id_gdb_poll_timer);

    // hook to project loading procedure
    ProjectLoaderHooks::HookFunctorBase* myhook = new ProjectLoaderHooks::HookFunctor<Debugger_GDB_MI>(this, &Debugger_GDB_MI::OnProjectLoadingHook);
    m_HookId = ProjectLoaderHooks::RegisterHook(myhook);

    DebuggerManager &dbg_manager = *Manager::Get()->GetDebuggerManager();
    dbg_manager.RegisterDebugger(this);
    m_currentActiveProject = 0;
    // Set current plugin version
    PluginInfo* pInfo = (PluginInfo*)(Manager::Get()->GetPluginManager()->GetPluginInfo(this));
    pInfo->version = m_Version;

    m_bAppStartShutdown = false;
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_SAVE,      new cbEventFunctor<Debugger_GDB_MI, CodeBlocksEvent>(this, &Debugger_GDB_MI::OnProjectSave)); //(ph 2024/03/16)
    Manager::Get()->RegisterEventSink(cbEVT_PROJECT_CLOSE,      new cbEventFunctor<Debugger_GDB_MI, CodeBlocksEvent>(this, &Debugger_GDB_MI::OnProjectClosed)); //(ph 2024/03/15)
    Manager::Get()->RegisterEventSink(cbEVT_APP_START_SHUTDOWN, new cbEventFunctor<Debugger_GDB_MI, CodeBlocksEvent>(this, &Debugger_GDB_MI::OnAppStartShutdown));
    // hook to editors
    myEdhook = new EditorHooks::HookFunctor<Debugger_GDB_MI>(this, &Debugger_GDB_MI::OnEditorEventHook);

}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnReleaseReal(bool appShutDown)
// ----------------------------------------------------------------------------
{
    // TODO: remove this when the loggers are fixed in C::B
    if (appShutDown)
        m_execution_logger.MarkAsShutdowned();

    ProjectLoaderHooks::UnregisterHook(m_HookId, true);
    Manager::Get()->GetDebuggerManager()->UnregisterDebugger(this);

    KillConsole();
    m_executor.ForceStop();
    if (m_command_stream_dialog)
    {
        m_command_stream_dialog->Destroy();
        m_command_stream_dialog = nullptr;
    }

    EditorHooks::UnregisterHook(m_EditorHookId, true);
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnUserClosingApp(wxCloseEvent& event)
// ----------------------------------------------------------------------------
{
    wxUnusedVar(event);

    Stop();     //close the debugger

    event.Skip();
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::SetupToolsMenu(wxMenu &menu)
// ----------------------------------------------------------------------------
{
    menu.Append(id_menu_info_command_stream, _("Show command stream"));
    wxMenuBar* pMnuBar = Manager::Get()->GetAppFrame()->GetMenuBar();
    int idRemoveAllBreakpoints = pMnuBar->FindMenuItem("Debug","Remove all breakpoints");
    if (wxFound(idRemoveAllBreakpoints))
    {
        int idSaveBreakpoints = pMnuBar->FindMenuItem("Debug","Save breakpoints");
        // When not found "save breakpoints", insert it
        if (not wxFound(idSaveBreakpoints) )
        {
            // Add "Save breakpoints to Debug menu
            // Find the Debug menu
            wxMenu* pDebugMenu = pMnuBar->GetMenu(pMnuBar->FindMenu("Debug"));

            // Insert "Save breakpoints" above "Remove all breakpoints"
            size_t posn = wxNOT_FOUND;
            pDebugMenu->FindChildItem(idRemoveAllBreakpoints, &posn);
            if (wxFound(posn))
            {
                pDebugMenu->Insert( posn, idDebugMenuSaveBreakpoints,
                    "Save breakpoints",
                    "Save current project breakpoints" );
                Bind( wxEVT_COMMAND_MENU_SELECTED, &Debugger_GDB_MI::OnSaveProjectBreakpoints, this, idDebugMenuSaveBreakpoints );
            }// insert "Save breakpoints"
        }//not found Save breakpoints
    }//Found Remove all breakpoints
}
// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::SupportsFeature(cbDebuggerFeature::Flags flag)
// ----------------------------------------------------------------------------
{

    switch (flag)
    {
        case cbDebuggerFeature::Breakpoints:
        case cbDebuggerFeature::Callstack:
        case cbDebuggerFeature::CPURegisters:
        case cbDebuggerFeature::Disassembly:
        case cbDebuggerFeature::ExamineMemory:
        case cbDebuggerFeature::Threads:
        case cbDebuggerFeature::Watches:
        case cbDebuggerFeature::RunToCursor:
        case cbDebuggerFeature::SetNextStatement:
        case cbDebuggerFeature::ValueTooltips:
            return true;

        default:
            return false;
    }
}

// ----------------------------------------------------------------------------
cbDebuggerConfiguration* Debugger_GDB_MI::LoadConfig(const ConfigManagerWrapper &config)
// ----------------------------------------------------------------------------
{
    //return new dbg_mi::Configuration(config); //(ph 2024/03/11)
    return new DebuggerConfiguration(config);
}

// ----------------------------------------------------------------------------
DebuggerConfiguration& Debugger_GDB_MI::GetActiveConfigEx()
// ----------------------------------------------------------------------------
{
    return static_cast<DebuggerConfiguration&>(GetActiveConfig());
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::SelectCompiler(cbProject& project, Compiler *&compiler,
                                     ProjectBuildTarget *&target, long pid_to_attach)
// ----------------------------------------------------------------------------
{
   // select the build target to debug
    target = NULL;
    compiler = NULL;
    wxString active_build_target = wxString();

    if (pid_to_attach == 0)          //(ph 2025/01/22)
        active_build_target = project.GetActiveBuildTarget();

    if(pid_to_attach == 0)
    {
        Log(_("Selecting target: "));
        if (not project.BuildTargetValid(active_build_target, false))
        {
            int tgtIdx = project.SelectTarget();
            if (tgtIdx == -1)
            {
                Log(_("canceled"), Logger::error);
                return false;
            }
            target = project.GetBuildTarget(tgtIdx);
            active_build_target = target->GetTitle();
        }
        else
            target = project.GetBuildTarget(active_build_target);

        // make sure it's not a commands-only target
        if (target->GetTargetType() == ttCommandsOnly)
        {
            cbMessageBox(_("The selected target is only running pre/post build step commands\n"
                        "Can't debug such a target..."), _("Debugger:Information"), wxICON_INFORMATION, Manager::Get()->GetAppWindow());
            Log(_("aborted"), Logger::error);
            return false;
        }
        Log(target->GetTitle());

        // find the target's compiler (to see which debugger to use)
        compiler = CompilerFactory::GetCompiler(target ? target->GetCompilerID() : project.GetCompilerID());
    }
    else
        compiler = CompilerFactory::GetDefaultCompiler();
    return true;
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnGDBOutput(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (Manager::IsAppShuttingDown())
        return; //avoid crashes
    wxString const &msg = event.GetString();
    if (not msg.IsEmpty())
        ParseOutput(msg);
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnAppStartShutdown(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // Do Stop() debugger here or we crash during shutdown
    // when pluginmanager deletes this plugin.

    Stop();
    m_bAppStartShutdown = true;
    event.Skip(); // allow others to process it too
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnGDBTerminated(wxCommandEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    if (Manager::IsAppShuttingDown()) //(pecan 2012/07/31)
        return; //avoid crashes

    EditorHooks::UnregisterHook(m_EditorHookId, false);
    m_nEditorHookBusy = 0;

    ClearActiveMarkFromAllEditors();
    //-RemoveAllDataBreakpoints(); //(pecan 2024/03/6) Don't do this?

    Log(_("debugger terminated!"), Logger::warning);
    m_timer_poll_debugger.Stop();
    m_actions.Clear();
    m_executor.Clear();

    // Notify debugger plugins for end of debug session
    PluginManager *plm = Manager::Get()->GetPluginManager();
    CodeBlocksEvent evt(cbEVT_DEBUGGER_FINISHED);
    plm->NotifyPlugins(evt);

    SwitchToPreviousLayout();

    KillConsole();
    MarkAsStopped();

    for (Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
        (*it)->SetIndex(-1);
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnIdle(wxIdleEvent& event)
// ----------------------------------------------------------------------------
{
    if(m_executor.IsStopped() && m_executor.IsRunning())
    {
        m_actions.Run(m_executor);
    }
    if(m_executor.ProcessHasInput())
        event.RequestMore();
    else
        event.Skip();
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnTimer(wxTimerEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    // Dispatch any gdb results and restart the executor
    // nb: search this project for m_timer_poll_debugger to see usage.
    RunQueue();
    wxWakeUpIdle();
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnMenuInfoCommandStream(wxCommandEvent& /*event*/)
// ----------------------------------------------------------------------------
{
    // Show collected log messages in log pane
    wxString full;
    for (int ii = 0; ii < m_execution_logger.GetCommandCount(); ++ii)
        full += m_execution_logger.GetCommand(ii) + _T("\n");
    if (m_command_stream_dialog)
    {
        m_command_stream_dialog->SetText(full);
        m_command_stream_dialog->Show();
    }
    else
    {
        m_command_stream_dialog = new dbg_mi::TextInfoWindow(Manager::Get()->GetAppWindow(), _T("Command stream"), full);
        m_command_stream_dialog->Show();
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::AddStringCommand(wxString const &command)
// ----------------------------------------------------------------------------
{
    if (IsExecutorRunning())
        m_actions.Add(new dbg_mi::SimpleAction(command, m_execution_logger));
}
// ----------------------------------------------------------------------------
struct Notifications
// ----------------------------------------------------------------------------
{
    Notifications(Debugger_GDB_MI *plugin, dbg_mi::GDBExecutor &executor, bool simple_mode) :
        m_plugin(plugin),
        m_executor(executor),
        m_simple_mode(simple_mode)
    {
    }

    void operator()(dbg_mi::ResultParser const &parser)
    {
        dbg_mi::ResultValue const &result_value = parser.GetResultValue();
        m_plugin->DebugLog(_T("notification event received!"));

        if(m_simple_mode)
        {
            ParseStateInfo(result_value);
            m_plugin->UpdateWhenStopped();
        }
        else
        {
            if (parser.GetResultType() == dbg_mi::ResultParser::NotifyAsyncOutput)
                ParseNotifyAsyncOutput(parser);
            else if(parser.GetResultClass() == dbg_mi::ResultParser::ClassStopped)
            {
                dbg_mi::StoppedReason reason = dbg_mi::StoppedReason::Parse(result_value);

                switch(reason.GetType())
                {
                case dbg_mi::StoppedReason::SignalReceived:
                    {
                        wxString signal_name, signal_meaning;

                        dbg_mi::Lookup(result_value, _T("signal-name"), signal_name);

                        if(signal_name != _T("SIGTRAP") && signal_name != _T("SIGINT"))
                        {
                            dbg_mi::Lookup(result_value, _T("signal-meaning"), signal_meaning);
                            // avoid empty signal info
                            if ( signal_name.empty())
                                ;//avoid empty signal messages
                            else
                            // non empty signal info
                            InfoWindow::Display(_("GDB/MI Signal received"),
                                                wxString::Format(_T("\n%s: %s (%s)\n\n"), _("Program received signal"),
                                                                 signal_meaning.c_str(),
                                                                 signal_name.c_str()), 7000);

                        }

                        Manager::Get()->GetDebuggerManager()->ShowBacktraceDialog();
                        UpdateCursor(result_value, true);
                    }
                    break;
                case dbg_mi::StoppedReason::ExitedNormally:
                case dbg_mi::StoppedReason::ExitedSignalled:
                {
                    m_executor.Execute(wxT("-gdb-exit")); //should be -gdb-exit
                    break;
                }
                case dbg_mi::StoppedReason::Exited:
                    {
                        int code = -1;
                        if(not dbg_mi::Lookup(result_value, _T("exit-code"), code))
                            code = -1;
                        m_plugin->SetExitCode(code);
                        m_executor.Execute(wxT("-gdb-exit")); //should be -gdb-exit
                    }
                    break;
                case dbg_mi::StoppedReason::LocationReached:
                case dbg_mi::StoppedReason::EndSteppingRange:
                    {
                        // FIXME (ph#): new StoppedReasons This needs testing
                        wxString frameFuncValue = wxEmptyString;
                        wxString frameFromValue = wxEmptyString;

                        dbg_mi::Lookup(result_value, _T("frame.func"), frameFuncValue);
                        dbg_mi::Lookup(result_value, _T("frame.from"), frameFromValue);

                        //-Manager::Get()->GetDebuggerManager()->ShowBacktraceDialog();     //(ph 2024/03/06))
                        //-UpdateCursor(result_value, true);                                //(ph 2024/03/06)
                        UpdateCursor(result_value, not m_executor.IsTemporaryInterrupt());  //(ph 2024/03/06)

                    }// LocationReached
                    break;
                default:
                    UpdateCursor(result_value, !m_executor.IsTemporaryInterrupt());
                }

                if(not m_executor.IsTemporaryInterrupt())
                    m_plugin->BringCBToFront();
            }
            //(ph 2024/03/06)begin: Some debuggers can send a "Running" notification
            else if(parser.GetResultClass() == dbg_mi::ResultParser::ClassRunning) //(ph 2024/03/06)
            {
                // Handle possible unsolicited '*running' notification event
                // such as one sent to us from a remote external pgm
                //-m_plugin->DebugLog(_T("'Running' notification event received!"));
                m_executor.Stopped(false);
                //if(not stopped)
                if(not m_executor.IsStopped())
                    m_plugin->ClearActiveMarkFromAllEditors();
            }
            //(ph 2024/03/06)end
        }
    }

private:
    // ----------------------------------------------------------------------------
    void UpdateCursor(dbg_mi::ResultValue const &result_value, bool parse_state_info)
    // ----------------------------------------------------------------------------
    {
        if(parse_state_info)
            ParseStateInfo(result_value);

        m_executor.Stopped(true);

        // Notify debugger plugins for end of debug session
        PluginManager *plm = Manager::Get()->GetPluginManager();
        CodeBlocksEvent evt(cbEVT_DEBUGGER_PAUSED);
        plm->NotifyPlugins(evt);

        m_plugin->UpdateWhenStopped();
    }
    // ----------------------------------------------------------------------------
    void ParseStateInfo(dbg_mi::ResultValue const &result_value)
    // ----------------------------------------------------------------------------
    {
        dbg_mi::Frame frame;
        if(not frame.ParseOutput(result_value))
        {
            m_plugin->DebugLog(_T("Debugger_GDB_MI::OnGDBNotification: can't find/parse frame value:("),
                               Logger::error);
            m_plugin->DebugLog(wxString::Format(_T("Debugger_GDB_MI::OnGDBNotification: %s"),
                                                result_value.MakeDebugString().c_str()));
        }
        else
        {
            dbg_mi::ResultValue const *thread_id_value;
            thread_id_value = result_value.GetTupleValue(m_simple_mode ? _T("new-thread-id") : _T("thread-id"));
            if(thread_id_value)
            {
                long id;
                if(not thread_id_value->GetSimpleValue().ToLong(&id, 10))
                {
                    m_plugin->Log(wxString::Format(_T("Debugger_GDB_MI::OnGDBNotification ")
                                                   _T(" thread_id parsing failed (%s)"),
                                                   result_value.MakeDebugString().c_str()));
                }
                else
                    m_plugin->GetCurrentFrame().SetThreadId(id);
            }
            if(frame.HasValidSource())
            {
                m_plugin->GetCurrentFrame().SetPosition(frame.GetFilename(), frame.GetLine());
                m_plugin->SyncEditor(frame.GetFilename(), frame.GetLine(), true);
            }
            else
            {
                m_plugin->DebugLog(_T("ParseStateInfo: Frame does not have valid source"), Logger::error);
                //(ph 2024/04/25)
                // FIXME (ph#): When setting or clearing a breakpoint, this happens, Why?
                // InfoWindow::Display(_("GDB/MI Error"), _("ParseStateInfo: Frame does not have valid source"), 7000);
            }
        }
    }

    // ----------------------------------------------------------------------------
    void ParseNotifyAsyncOutput(dbg_mi::ResultParser const &parser)
    // ----------------------------------------------------------------------------
    {
        if (parser.GetAsyncNotifyType() == _T("thread-group-started"))
        {
            int pid;
            dbg_mi::Lookup(parser.GetResultValue(), _T("pid"), pid);
            m_plugin->Log(wxString::Format(_T("Found child pid: %d\n"), pid));
            dbg_mi::GDBExecutor &exec = m_plugin->GetGDBExecutor();
            if (not exec.HasChildPID())
                exec.SetChildPID(pid);
        }
        else
            m_plugin->Log(wxString::Format(_T("Notification: %s\n"), parser.GetAsyncNotifyType().c_str()));
    }

private:
    Debugger_GDB_MI *m_plugin;
    //int m_page_index; unused
    dbg_mi::GDBExecutor &m_executor;
    bool m_simple_mode;
}; //struct Notification
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateOnFrameChanged(bool wait)
// ----------------------------------------------------------------------------
{
    int hasAutoUpdates = 0;
    bool watchesDone = false;

    // If watching locals vars and args, create the containers
    if ( (not m_localsWatch) or (not m_funcArgsWatch))
    {
        DoLocalWatches(/*step*/ 1);
        watchesDone = true;
    }

    if(wait)
        {;} //m_actions.Add(new dbg_mi::BarrierAction); // (ph 26/06/18)

    DebuggerManager *dbg_manager = Manager::Get()->GetDebuggerManager();

    if(IsWindowReallyShown(dbg_manager->GetWatchesDialog()->GetWindow()) && !m_watchesContainer.empty())
    {
        for(dbg_mi::WatchesContainer::iterator it = m_watchesContainer.begin(); it != m_watchesContainer.end(); ++it)
        {
            // For unregisterd variables, register new var with GDB
            if((*it)->GetID().empty() && !(*it)->ForTooltip())
            {
                wxString type; (*it)->GetType(type);
                // if this is a watch container let DoWatches() handle it.
                if (type == ":container:") continue;    // (ph 26/05/31)

                m_actions.Add(new dbg_mi::WatchCreateAction(*it, m_watchesContainer, m_execution_logger));
            }

            // Skip watch updates when no watch has property "Auto Update" //(ph 2024/03/06)
            if ( (*it)->IsAutoUpdateEnabled())
                ++hasAutoUpdates;
        }

        if (hasAutoUpdates)
        {
            // update all manual watches, Local and args watches
            m_actions.Add(new dbg_mi::WatchesUpdateAction(m_watchesContainer, m_execution_logger));
        }
        else if (m_localsWatch or m_funcArgsWatch)
        {
            if (not watchesDone)
            // update the Local and args watches
            DoLocalWatches(/*stepToPerform*/1); // (ph 26/06/18)
        }
    }//endif watch window shown
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateWhenStopped()
// ----------------------------------------------------------------------------
{
    // If the window is actually showing, update it.

    DebuggerManager *dbg_manager = Manager::Get()->GetDebuggerManager();
    if(dbg_manager->UpdateBacktrace())
        RequestUpdate(Backtrace);

    if(dbg_manager->UpdateThreads())
        RequestUpdate(Threads);

    if(dbg_manager->UpdateCPURegisters())
        RequestUpdate(CPURegisters);

    if(dbg_manager->UpdateDisassembly())
        RequestUpdate(Disassembly);

    UpdateOnFrameChanged(false);

    DebuggerConfiguration& active_config = GetActiveConfigEx();

    if (active_config.GetFlag(DebuggerConfiguration::WatchFuncArgs))
    {
        DoSendCommand(_T("catch throw"));
        DoSendCommand(_T("catch catch"));
    }

}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::RunQueue()
// ----------------------------------------------------------------------------
{
    if (m_executor.IsRunning())
    {
        Notifications notifications(this, m_executor, false);
        dbg_mi::DispatchResults(m_executor, m_actions, notifications);

        if(m_executor.IsStopped())
            m_actions.Run(m_executor);
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ParseOutput(wxString const &str)
// ----------------------------------------------------------------------------
{
    if(not str.IsEmpty())
    {
        wxArrayString const &lines = GetArrayFromString(str, _T('\n'));
        for(size_t ii = 0; ii < lines.GetCount(); ++ii)
            m_executor.ProcessOutput(lines[ii]);
        m_actions.Run(m_executor);
    }
}
// ----------------------------------------------------------------------------
struct StopNotification
// ----------------------------------------------------------------------------
{
    StopNotification(cbDebuggerPlugin *plugin, dbg_mi::GDBExecutor &executor) :
        m_plugin(plugin),
        m_executor(executor)
    {
    }

    void operator()(bool stopped)
    {
        m_executor.Stopped(stopped);
        if(not stopped)
            m_plugin->ClearActiveMarkFromAllEditors();
    }

    cbDebuggerPlugin *m_plugin;
    dbg_mi::GDBExecutor &m_executor;
};
// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::Debug(bool breakOnEntry)
// ----------------------------------------------------------------------------
{
    m_hasStartUpError = false;
    ShowLog(true); //clear the log

    cbDebuggerPlugin *dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    Log(wxString::Format(_T("Debugger version %s %s"),dbg->GetGUIName().c_str(), m_Version.c_str()));

    Log(_("start debugger"));

    ProjectManager &project_manager = *Manager::Get()->GetProjectManager();
    cbProject *project = project_manager.GetActiveProject();
    if(not project)
    {
        Log(_("no active project"), Logger::error);
        return false;
    }
    StartType start_type = breakOnEntry ? StartTypeStepInto : StartTypeRun;

    if(not EnsureBuildUpToDate(start_type))
        return false;

    if(not WaitingCompilerToFinish() && !m_executor.IsRunning() && !m_hasStartUpError)
        return StartDebugger(project, start_type) == 0;
    else
        return true;
}
// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::CompilerFinished(bool compilerFailed, StartType startType)
// ----------------------------------------------------------------------------
{
    if (compilerFailed || startType == StartTypeUnknown)
        m_temporary_breakpoints.clear();
    else
    {
        ProjectManager &project_manager = *Manager::Get()->GetProjectManager();
        cbProject *project = project_manager.GetActiveProject();
        if(project)
            return StartDebugger(project, startType) == 0;
        else
            Log(_("no active project"), Logger::error);
    }
    ShowLog(true);
    return false;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ConvertDirectory(wxString& str, wxString base, bool relative)
// ----------------------------------------------------------------------------
{
    dbg_mi::ConvertDirectory(str, base, relative);
}
// ----------------------------------------------------------------------------
struct BreakpointMatchProject
// ----------------------------------------------------------------------------
{
    BreakpointMatchProject(cbProject *project) : project(project) {}
    bool operator()(dbg_mi::Breakpoint::Pointer bp) const
    {
        return bp->GetProject() == project;
    }
    cbProject *project;
};
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::CleanupWhenProjectClosed(cbProject *project)
// ----------------------------------------------------------------------------
{
    // Called drectly from cbplugin, not OnProjectClosed

    Stop(); //stop the debugger

    Breakpoints::iterator it = std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                                              BreakpointMatchProject(project));
    if (it != m_breakpoints.end())
    {
        m_breakpoints.erase(it, m_breakpoints.end());
        // FIXME (pecan#): Optimize this when multiple projects are closed
        //                      (during workspace close operation for exmaple).
        cbBreakpointsDlg *dlg = Manager::Get()->GetDebuggerManager()->GetBreakpointDialog();
        dlg->Reload();
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::InitWhenProjectOpened(cbProject *project)
// ----------------------------------------------------------------------------
{
    wxUnusedVar(project);
    //(ph 2024/03/06)begin: InitWhenProjectOpened
    //  OnProjectActivated is no good here because LoadingOrClosing() is always true
    //  so we get called on cbCVT_PROJECT_OPEN when loading is finished.

    if ( Manager::Get()->GetProjectManager()->IsLoadingOrClosing())
    {
        return;
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::InitWhenProjectActivated(cbProject *project)
// ----------------------------------------------------------------------------
{
    // This routine called on both cbCVT_PROJECT_ACTIVATE and cbCVT_PROJECT_OPEN
    // because IsLoadingOrClosing() is always true on cbCVT_PROJECT_ACTVATE
    if ( Manager::Get()->GetProjectManager()->IsLoadingOrClosing())
    {
        return;
    }
    InitWhenProjectOpened(project);
}
// ----------------------------------------------------------------------------
int Debugger_GDB_MI::StartDebugger(cbProject *project, StartType start_type)
// ----------------------------------------------------------------------------
{
    ShowLog(true); // clear the log was done already, but switch to Debugger log tab
    m_execution_logger.ClearCommand();
    m_PidToAttach = 0;

    SelectCompiler(*project, m_compiler, m_target, m_PidToAttach); //(ph 2025/01/23)
    ProjectBuildTarget* target = GetTarget();
    Compiler* compiler = GetCompiler();

    if(not m_compiler)
    {
        Log(_("no compiler found!"), Logger::error);
        m_hasStartUpError = true;
        return 2;
    }
    if(not target)
    {
        Log(_("no target found!"), Logger::error);
        m_hasStartUpError = true;
        return 3;
    }
    // Code block to fix the "no such file" problem when users working dir is set to a single dot.
    // Record the project current working directory which gdb may change and //(ph 2024/04/10)
    //  report a "no such file" error when looking for source files.
    wxString absWorking_dir;
    { // <=== code block
        wxString origDebuggee;
        if (not GetDebuggee(origDebuggee, absWorking_dir, target))
        {
            m_hasStartUpError = true;
            return 6;
        }
        if (absWorking_dir.Contains('.'))
        {
            wxFileName relFilename(absWorking_dir);
            bool ok = relFilename.Normalize(wxPATH_NORM_ABSOLUTE|wxPATH_NORM_DOTS);
            if (ok)
                absWorking_dir = relFilename.GetFullPath();
            // The damn NormalizePath does not remove the last "\."
            if (absWorking_dir.EndsWith(fileSep + "."))
                absWorking_dir.RemoveLast(2);
        }

    } //end <=== code block

    // is gdb accessible, i.e. can we find it?
    wxString debugger = GetActiveConfigEx().GetDebuggerExecutable();
    wxString args = target->GetExecutionParameters();
    wxString debuggee, working_dir;

    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(debugger); //(ph 2024/03/06)


    if (not GetDebuggee(debuggee, working_dir, target))
    {
        m_hasStartUpError = true;
        return 6;
    }
    // use the absolute path for the projects working dir //(ph 2024/04/10)
    // because if gdb changes the cwd, a relative dot will be wrong for the working_dir
    if (working_dir != absWorking_dir)
        working_dir = absWorking_dir;

    //-m_MasterPath = compiler->GetMasterPath();    //(ph 2024/03/06)

    bool isConsole = target->GetTargetType() == ttConsoleOnly;

    //(ph 2024/03/06) begin
    RemoteDebuggingMap& rdprj = GetRemoteDebuggingMap(project);
    RemoteDebugging rd = rdprj[0]; // project settings
    RemoteDebuggingMap::iterator it = rdprj.find(target); // target settings
    if (it != rdprj.end())
        rd.MergeWith(it->second);
    //(ph 2024/03/06) end


    wxString oldLibPath;
    wxGetEnv(CB_LIBRARY_ENVVAR, &oldLibPath);
    wxString newLibPath = GetLibraryPath(oldLibPath, compiler, target);
    //newLibPath = GetLibraryPath("F:\\usr\\proj\\cbNightly\\mingw64\\bin", compiler, target); //(ph 2024/04/08)
    if (oldLibPath != newLibPath)
    {
        wxSetEnv(CB_LIBRARY_ENVVAR, newLibPath);
        DebugLog(CB_LIBRARY_ENVVAR _T("=") + newLibPath);
    }

    m_project = project;    //(ph 2024/03/06)

    int res = LaunchDebugger(debugger, debuggee, args, working_dir, 0, isConsole, start_type);
    if (res != 0)
    {
        m_hasStartUpError = true;
        return res;
    }
    m_executor.SetAttachedPID(-1);

    m_project = project;
    m_hasStartUpError = false;

    if (oldLibPath != newLibPath)
        wxSetEnv(CB_LIBRARY_ENVVAR, oldLibPath);

    m_EditorHookId = EditorHooks::RegisterHook(myEdhook);
    m_nEditorHookBusy = 0;

    return 0;
}
// ----------------------------------------------------------------------------
int Debugger_GDB_MI::LaunchDebugger(wxString const &debugger, wxString const &debuggee,
                                    wxString const &args, wxString const &working_dir,
                                    int pid, bool isConsole, StartType start_type)
// ----------------------------------------------------------------------------
{
    ProjectManager &project_manager = *Manager::Get()->GetProjectManager();
    cbProject* m_project = project_manager.GetActiveProject();
    Compiler* compiler;
    ProjectBuildTarget* pTarget;
    SelectCompiler(*m_project, compiler, pTarget, m_PidToAttach); //(ph 2025/01/23)
    DebuggerConfiguration &config = GetActiveConfigEx();
    bool disableScripts = config.GetFlag(DebuggerConfiguration::DisableInit);
    bool usePrettyPrinters = config.GetFlag(DebuggerConfiguration::UsePrettyPrinters);

    m_current_frame.Reset();
    if(debugger.IsEmpty())
    {
        Log(_("no debugger executable found (full path)!"), Logger::error);
        return 5;
    }

    Log(_("GDB path: ") + debugger);
    if (pid == 0)
        Log(_("DEBUGGEE path: ") + debuggee);

    wxString cmd;
    cmd << debugger;

    if (disableScripts)
        cmd << _T(" -nx");          // don't run .gdbinit
    cmd << _T(" -fullname ");       // report full-path filenames when breaking
    cmd << _T(" -quiet");           // don't display version on startup
    cmd << _T(" --interpreter=mi"); // use gdb machine interface

    // Create or delete the gdbinit file here since we know the compiler
    // and target that is being used.
    wxString gdbinitFileLocation;
    if ( (not disableScripts) and usePrettyPrinters)    // (ph 26/06/18)
        gdbinitFileLocation = CreateGDBinitFile(m_project, pTarget);
    else //remove gdbinit to allow for future code or file changes
        RemoveGdbinitFile(m_project);

    if ( (not disableScripts) and usePrettyPrinters
                    and wxFileExists(gdbinitFileLocation))
            cmd << " -ix " << gdbinitFileLocation;

    if (pid == 0)
    {
        cmd << _T(" -args ") << debuggee;
    }
    else
        cmd << _T(" -pid=") << pid;

    // start the gdb process
    Log(_T("Command-line: ") + cmd);
    if (pid == 0)
        Log(_T("Working dir : ") + working_dir);

    int ret = m_executor.LaunchProcess(cmd, working_dir, id_gdb_process, this, m_execution_logger);

    if (ret != 0)
        return ret;

//    m_executor.Stopped(true);
    m_executor.Stopped(false);  //command execution may be too early for remotes //(ph 2024/03/06)
//    m_executor.Execute(_T("-enable-timings"));

    // if remote debugging
    if ( pTarget and IsRemoteDebugging(pTarget) ) //(ph 2025/01/22)
    {
        // although I don't really like these do-nothing loops, we must wait a small amount of time
        // for gdb to see if it really started: it may fail to load shared libs or whatever
        // the reason this is added is because I had a case where gdb would error and bail out
        // *while* the driver->Prepare() call was running below and hell broke loose...
        int i = 50;
        while (i)
        {
            wxMilliSleep(1);
            Manager::Yield();
            --i;
        }
        //Assure GDB is still running before issuing Prepare call for remote debugging
        if (not m_executor.IsRunning() )
            return -1;

        //bool isConsole = (pTarget && pTarget->GetTargetType() == ttConsoleOnly);
        // PrepareRemoteDebugging(isConsole, m_printElements 50);
        // FIXME (ph#): m_printElements needs attention
    }

    CommitBreakpoints(true);
    CommitWatches();

    // Set program arguments
    m_actions.Add(new dbg_mi::SimpleAction(_T("-exec-arguments ") + args, m_execution_logger));

    // Display data format as decimal by default
    //-m_actions.Add(new dbg_mi::SimpleAction(_T("-data-display-radix D"), m_execution_logger)); //(ph 2024/03/06)
    // FIXME (ph#): undefined gdb/mi command

    if(isConsole) // Console project target?
    {
        if (platform::windows) // (ph 26/01/09)
        {
            DoSendCommand(_T("set new-console on"));
        }
        else //linux
        {
            wxString console_tty;
            m_console_pid = RunNixConsole(console_tty);
            if(m_console_pid >= 0)
                //-m_actions.Add(new dbg_mi::SimpleAction(_T("-inferior-tty-set ") + console_tty));                    //(ph 2024/03/06)
                m_actions.Add(new dbg_mi::SimpleAction(_T("-inferior-tty-set ") + console_tty, m_execution_logger));   //(ph 2024/03/06)
        }
    }

    DebuggerConfiguration& active_config = GetActiveConfigEx();

        m_actions.Add(new dbg_mi::SimpleAction(_T("-enable-pretty-printing"), m_execution_logger));

    wxArrayString const &commands = active_config.GetInitialCommands(); //(ph 2025/01/11)
    for (unsigned ii = 0; ii < commands.GetCount(); ++ii)
        DoSendCommand(commands[ii]);

    //-if (active_config.GetFlag(dbg_mi::Configuration::CatchCppExceptions)) //(ph 2024/03/11)
    if (active_config.GetFlag(DebuggerConfiguration::CatchExceptions))
    {
        DoSendCommand(_T("catch throw"));
        DoSendCommand(_T("catch catch"));
    }

    // get remote debugging info
    // if performing remote debugging, use "continue" command
    RemoteDebugging* rd = pTarget ? GetRemoteDebuggingInfo(pTarget): nullptr; //(ph 2025/01/22)
    bool remoteDebugging = rd && rd->IsOk();
    /* emIDE change [JL] - break at main function*/
    if (remoteDebugging)
    {
        //-bool breakAtMain = rd->breakAtMain;
        bool breakAtMain = true;    //(ph 2024/03/06) forced to true for now
        //-wxString breakAtMainSymbol = rd->breakAtMainSymbol;
        if(breakAtMain)
        {
          //-if (not breakAtMainSymbol.IsEmpty()) {
            //-m_State.GetDriver()->QueueCommand(new DebuggerCmd(m_State.GetDriver(), wxT("tbreak " + breakAtMainSymbol), false));
            m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("tbreak "))+ _T("main"), m_execution_logger));    //(ph 2024/03/06)
        }
        else
            start_type = StartTypeStepInto;
        //-m_State.GetDriver()->Start(breakOnEntry);
        //-}
        //-else
        //-{
        //-  m_State.GetDriver()->QueueCommand(new DebuggerCmd(m_State.GetDriver(), wxT("info line *$pc"), false));
        //-}
    }

    if (pid == 0)
    {
        switch (start_type)
        {
            case StartTypeRun:
                if (remoteDebugging)
                    CommitRunCommand(_T("-exec-continue"));
                else
                    CommitRunCommand(_T("-exec-run"));
                break;
            case StartTypeStepInto:
                CommitRunCommand(_T("-exec-step"));
                break;
            case StartTypeUnknown:
                break;
        }
    }

    m_executor.Stopped(true); // Allow command execution
    m_actions.Run(m_executor);

    m_timer_poll_debugger.Start(20);

    // switch to the user-defined layout for debugging
    if (m_executor.IsRunning())  //checks for m_Process exists
    {

        // FIXME (ph#): the switch below is constantly trashing the order of the logs
        //SwitchToDebuggingLayout(); //why is this switching away from Debug log?
        ShowLog(false); //(ph 2025/02/10)
    }
    return 0;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::CommitBreakpoints(bool force)
// ----------------------------------------------------------------------------
{
    DebugLog(_T("Debugger_GDB_MI::CommitBreakpoints"));
    for(Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
    {
        if((*it)->GetIndex() == -1 || force)
        {
            m_actions.Add(new dbg_mi::BreakpointAddAction(*it, m_execution_logger));
        }
    }

    for(Breakpoints::const_iterator it = m_temporary_breakpoints.begin(); it != m_temporary_breakpoints.end(); ++it)
    {
        AddStringCommand(wxString::Format(_T("-break-insert -t %s:%d"), (*it)->GetLocation().c_str(),
                                          (*it)->GetLine()));
    }
    m_temporary_breakpoints.clear();
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::CommitWatches()
// ----------------------------------------------------------------------------
{
    // Clean up last sessons watches window // (ph 26/06/19)
    DeleteAllWatches();     // (ph 26/06/19)
    // Safely deletes the GDBWatch object and sets m_localsWatch to nullptr
    // so we don't show a watch window with stale watches on debugger restart.
    if ( m_localsWatch)
        m_localsWatch.reset();
    if (m_funcArgsWatch)
        m_funcArgsWatch.reset();;

    // The watches now are always empty at first run or re-run of the debugger.
    // It guarantees that we arn't showing stale watches
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::CommitRunCommand(wxString const &command)
// ----------------------------------------------------------------------------
{
    m_current_frame.Reset();
    m_actions.Add(new dbg_mi::RunAction<StopNotification>(this, command,
                                                          StopNotification(this, m_executor),
                                                          m_execution_logger)
                  );
}
// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::RunToCursor(const wxString& filename, int line, const wxString& /*line_text*/)
// ----------------------------------------------------------------------------
{
    if(IsExecutorRunning())
    {
        if(IsExecutorStopped())
        {
            CommitRunCommand(wxString::Format(_T("-exec-until %s:%d"), dbg_mi::AddQuotesIfNeeded(filename).c_str(), line));
            return true;
        }
        return false;
    }
    else
    {
        dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint(filename, line, nullptr));
        m_temporary_breakpoints.push_back(ptr);
        return Debug(false);
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::SetNextStatement(const wxString& filename, int line)
// ----------------------------------------------------------------------------
{
    if(IsExecutorStopped())
    {
        AddStringCommand(wxString::Format(_T("-break-insert -t %s:%d"), dbg_mi::AddQuotesIfNeeded(filename).c_str(), line));
        CommitRunCommand(wxString::Format(_T("-exec-jump %s:%d"), dbg_mi::AddQuotesIfNeeded(filename).c_str(), line));
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::Continue()
// ----------------------------------------------------------------------------
{
    if(not IsExecutorStopped() && !m_executor.Interrupting())
    {
        dbg_mi::Logger *log = m_executor.GetLogger();
        if(log)
            log->Debug(_T("Continue failed -> debugger is not interrupted!"));

        return;
    }
    DebugLog(_T("Debugger_GDB_MI::Continue"));
    CommitRunCommand(_T("-exec-continue"));
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::Next()
// ----------------------------------------------------------------------------
{
    CommitRunCommand(_T("-exec-next"));
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::NextInstruction()
// ----------------------------------------------------------------------------
{
    CommitRunCommand(_T("-exec-next-instruction"));
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::StepIntoInstruction()
// ----------------------------------------------------------------------------
{
    CommitRunCommand(_T("-exec-step-instruction"));
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::Step()
// ----------------------------------------------------------------------------
{
    CommitRunCommand(_T("-exec-step"));
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::StepOut()
// ----------------------------------------------------------------------------
{
    CommitRunCommand(_T("-exec-finish"));
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::Break()
// ----------------------------------------------------------------------------
{
    m_executor.Interrupt(false);
    // cbEVT_DEBUGGER_PAUSED will be sent, when the debugger has pause for real
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::Stop()
// ----------------------------------------------------------------------------
{
    if(not IsExecutorRunning())
        return;

    m_timer_poll_debugger.Stop();
    wxBusyInfo wait(_("Closing debugger..."),
                            Manager::Get()->GetAppWindow());

    //Stop user from terminating CB
    //else we'll crash while waiting for debugger termination
    Manager::Get()->GetAppWindow()->Enable(false);

    ClearActiveMarkFromAllEditors();
    Log(_("stopping debugger"));
    m_executor.ForceStop();
    MarkAsStopped();

    DeleteAllWatches();     // (ph 26/06/19)
    // Safely deletes the GDBWatch object and sets m_localsWatch to nullptr
    // so we don't show a watch window with stale watches.
    if ( m_localsWatch)
        m_localsWatch.reset();
    if (m_funcArgsWatch)
        m_funcArgsWatch.reset();;

    //Re-enable CB for input
    Manager::Get()->GetAppWindow()->Enable(true);

    EnableProgrammingFlash(true);
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::IsRunning() const
// ----------------------------------------------------------------------------
{
    return m_executor.IsRunning();
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::IsStopped() const
// ----------------------------------------------------------------------------
{

    return m_executor.IsStopped();
}
// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::IsBusy() const
// ----------------------------------------------------------------------------
{
    return not m_executor.IsStopped();
}

// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetExitCode() const
// ----------------------------------------------------------------------------
{
    return m_exit_code;
}

// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetStackFrameCount() const
// ----------------------------------------------------------------------------
{
    return m_backtrace.size();
}

// ----------------------------------------------------------------------------
cbStackFrame::ConstPointer Debugger_GDB_MI::GetStackFrame(int index) const
// ----------------------------------------------------------------------------
{
    return m_backtrace[index];
}

// ----------------------------------------------------------------------------
struct SwitchToFrameNotification
// ----------------------------------------------------------------------------
{
    SwitchToFrameNotification(Debugger_GDB_MI *plugin, int toFrameNumber) :
        m_plugin(plugin),
        frame_number(toFrameNumber)

    {
    }

    void operator()(dbg_mi::ResultParser const &WXUNUSED(result), int frame_number, bool user_action)
    {
        if (frame_number < m_plugin->GetStackFrameCount())
        {
            dbg_mi::CurrentFrame &current_frame = m_plugin->GetCurrentFrame();
            if (user_action)
                current_frame.SwitchToFrame(frame_number);
            else
            {
                current_frame.Reset();
                current_frame.SetFrame(frame_number);
            }

            cbStackFrame::ConstPointer frame = m_plugin->GetStackFrame(frame_number);

            wxString const &filename = frame->GetFilename();
            long line;
            if (frame->GetLine().ToLong(&line))
            {
                current_frame.SetPosition(filename, line);
                m_plugin->SyncEditor(filename, line, true);
            }

            m_plugin->UpdateOnFrameChanged(true);
            Manager::Get()->GetDebuggerManager()->GetBacktraceDialog()->Reload();
        }
    }

    Debugger_GDB_MI *m_plugin;
    int frame_number;
};

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::SwitchToFrame(int number)
// ----------------------------------------------------------------------------
{
    m_execution_logger.Debug(_T("Debugger_GDB_MI::SwitchToFrame"));
    if(IsExecutorRunning() && IsExecutorStopped())
    {
        if(number < static_cast<int>(m_backtrace.size()))
        {
            m_execution_logger.Debug(_T("Debugger_GDB_MI::SwitchToFrame - adding command"));

            int frame = m_backtrace[number]->GetNumber();
            typedef dbg_mi::SwitchToFrame<SwitchToFrameNotification> SwitchType;
            m_actions.Add(new SwitchType(frame, SwitchToFrameNotification(this, number), true));
        }
    }
}

// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetActiveStackFrame() const
// ----------------------------------------------------------------------------
{
    return m_current_frame.GetStackFrame();
}

// ----------------------------------------------------------------------------
cbBreakpoint::Pointer Debugger_GDB_MI::AddBreakpoint(const wxString& filename, int line)
// ----------------------------------------------------------------------------
{
    if(IsExecutorRunning())
    {
        if(not IsExecutorStopped())
        {
            DebugLog(wxString::Format(_T("Debugger_GDB_MI::Addbreakpoint: %s:%d"),
                                      filename.c_str(), line));
            m_executor.Interrupt();

            cbProject *project;
            project = Manager::Get()->GetProjectManager()->FindProjectForFile(filename, nullptr, false, false);
            dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint(filename, line, project));
            m_breakpoints.push_back(ptr);
            CommitBreakpoints(false);
            Continue();
        }
        else
        {
            cbProject *project;
            project = Manager::Get()->GetProjectManager()->FindProjectForFile(filename, nullptr, false, false);
            dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint(filename, line, project));
            m_breakpoints.push_back(ptr);
            CommitBreakpoints(false);
        }
    }
    else
    {
        cbProject *project = Manager::Get()->GetProjectManager()->FindProjectForFile(filename, nullptr, false, false);
        dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint(filename, line, project));
        m_breakpoints.push_back(ptr);
    }

    return cb::static_pointer_cast<cbBreakpoint>(m_breakpoints.back());
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::AddBreakpoint(dbg_mi::Breakpoint::Pointer ptr) //(pecan 2012/07/29)
// ----------------------------------------------------------------------------
{
    if(IsExecutorRunning())
    {
        if(not IsExecutorStopped())
        {
            //DebugLog(wxString::Format(_T("Debugger_GDB_MI::Addbreakpoint: %s:%d"),
            //                          filename.c_str(), line));
            m_executor.Interrupt();

            m_breakpoints.push_back(ptr);
            CommitBreakpoints(false);
            Continue();
        }
        else
        {
            m_breakpoints.push_back(ptr);
            CommitBreakpoints(false);
        }
    }
    else
    {
        m_breakpoints.push_back(ptr);
    }

    //return cb::static_pointer_cast<cbBreakpoint>(m_breakpoints.back());
}

// ----------------------------------------------------------------------------
cbBreakpoint::Pointer Debugger_GDB_MI::AddDataBreakpoint(const wxString& dataExpression)
// ----------------------------------------------------------------------------
{
    dbg_mi::DataBreakpointDlg dlg(Manager::Get()->GetAppWindow(), dataExpression, true, 1);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        //-const wxString& newDataExpression = dlg.GetDataExpression();
        int sel = dlg.GetSelection();
        //Breakpoint::Pointer bp(new Breakpoint);
        dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint);
        ptr->SetType(_T("Data"));
        ptr->SetDataAddress( dlg.GetDataExpression());
        ptr->SetBreakOnDataRead( sel not_eq 1);
        ptr->SetBreakOnDataWrite( sel not_eq 0);
        ptr->SetProject(Manager::Get()->GetProjectManager()->GetActiveProject());
        AddBreakpoint(ptr);
        return cb::static_pointer_cast<cbBreakpoint>(m_breakpoints.back());

    }
    return cbBreakpoint::Pointer();
}
// remove project bkpts and watches
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::RemoveProjectBreakpoints(const cbProject* project)
// ----------------------------------------------------------------------------
{
    bool changed = false;
    Breakpoints::iterator it = m_breakpoints.begin();
    while (it != m_breakpoints.end())
    {
        if (project == (*it)->GetProject())
        {
            m_breakpoints.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    if (changed)
    {
        Manager::Get()->GetDebuggerManager()->GetBreakpointDialog()->Reload();
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DoLocalWatches(int stepToPerform)
// ----------------------------------------------------------------------------
{
    // Create the Locals and Function arguments containers
    // In step 1, just create the containters (from UpdateOnFrameChange())
    // In step 2, update the watches and watches window (from RequestUpdate())

    if (not m_executor.IsRunning())  //checks for m_Process exists//(ph 2025/01/24))
        return;

    DebuggerConfiguration &config = GetActiveConfigEx();
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();

    bool locals = config.GetFlag(DebuggerConfiguration::WatchLocals);
    bool funcArgs = config.GetFlag(DebuggerConfiguration::WatchFuncArgs);

    if (locals)
    {
        if (m_localsWatch == nullptr)   //create if necessary
        {
            m_localsWatch = cb::shared_ptr<GDBWatch>(new GDBWatch(_("Local vars"),false,pProject));
            m_localsWatch->Expand(true);
            m_localsWatch->MarkAsChanged(false);
            cbWatchesDlg *watchesDialog = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
            watchesDialog->AddSpecialWatch(m_localsWatch, /*readonly*/true);
            /// Turning off "readonly" flag causes expand to fail showing child values.
            //-watchesDialog->AddSpecialWatch(m_localsWatch, /*readonly*/false); // (ph 26/06/13) /*experimental*/
            m_localsWatch->SetType(":container:");
            m_watchesContainer.push_back(m_localsWatch);
        }
    }

    if (funcArgs)   //create if necessary
    {
        if (m_funcArgsWatch == nullptr)
        {
            m_funcArgsWatch = cb::shared_ptr<GDBWatch>(new GDBWatch(_("Function args"),false,pProject));
            m_funcArgsWatch->Expand(true);
            m_funcArgsWatch->MarkAsChanged(false);
            cbWatchesDlg *watchesDialog = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
            watchesDialog->AddSpecialWatch(m_funcArgsWatch, true);
            m_funcArgsWatch->SetType(":container:");
            m_watchesContainer.push_back(m_funcArgsWatch);
        }
        //-m_funcArgsWatch->RemoveChildren();
    }

    if (stepToPerform == 1)
        {}//-return; fix // (ph 26/06/18) was short-circuiting logic


    // watches window must be visible, else ignore the request
    if (not IsWindowReallyShown(Manager::Get()->GetDebuggerManager()->GetWatchesDialog()->GetWindow()))
       return;

    DebuggerConfiguration& active_config = GetActiveConfigEx();
    int requestLocalsOrArgs = 0;

    // Display results for locals(1) and or args(2)
    if (active_config.GetFlag(DebuggerConfiguration::WatchLocals))
        requestLocalsOrArgs |= 1;

    if (active_config.GetFlag(DebuggerConfiguration::WatchFuncArgs))
        requestLocalsOrArgs |= 2;

    if (requestLocalsOrArgs)
    {
        // Get a list of the local args and watches for the current function
        // and add them to the watches container
        m_actions.Add(new dbg_mi::InfoLocalsAndArgs(&m_actions,
                        requestLocalsOrArgs,m_localsWatch,m_funcArgsWatch));

    }

    // Queue a barrier callback for after the local watches have been obtained from GDB
    m_actions.Add(new dbg_mi::GdbCmd_BarrierCallBack(__FUNCTION__, __LINE__, this,
                &Debugger_GDB_MI::GatherWatchesDone, /*step*/ 1));

    return;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::GatherWatchesDone(int stepToPerform)
// ----------------------------------------------------------------------------
{
    // Should now have watch data in local and args container
    // Register this frames watches with GDB if not done already
    int localsAndArgsCount = 0;
    if (m_localsWatch)
        localsAndArgsCount += m_localsWatch->GetChildCount();
    if (m_funcArgsWatch)
        localsAndArgsCount += m_funcArgsWatch->GetChildCount();

    int didAddVars = 0;

    // Step 1:
    if ((stepToPerform == 1) and localsAndArgsCount)
    {
        // Loop through the number of local children
        if (m_localsWatch) {
        for (int i = 0; i < m_localsWatch->GetChildCount(); ++i)
        {
            // Directly grab the child and cast it to the custom dbg_mi::Watch
            dbg_mi::Watch::Pointer derivedChild = std::static_pointer_cast<dbg_mi::Watch>(m_localsWatch->GetChild(i));
            // Register to GDB the local watches to get its internal GDB id
            if (derivedChild)
                if (derivedChild->GetID().empty())
                {
                    m_actions.Add(new dbg_mi::WatchCreateAction(derivedChild, m_watchesContainer, m_execution_logger));
                    didAddVars++;
                }
        }}//end if end for

        if (m_funcArgsWatch) {
        for (int i = 0; i < m_funcArgsWatch->GetChildCount(); ++i)
        {
            // Directly grab the child and cast it to the custom dbg_mi::Watch
            dbg_mi::Watch::Pointer derivedChild = std::static_pointer_cast<dbg_mi::Watch>(m_funcArgsWatch->GetChild(i));
            // Register to GDB the local watches to get its internal GDB id
            if (derivedChild)
                if (derivedChild->GetID().empty())
                {
                    m_actions.Add(new dbg_mi::WatchCreateAction(derivedChild, m_watchesContainer, m_execution_logger));
                    didAddVars++;
                }
        }}//end if for

        // If any vars were added
        // Queue a barrier callback to allow DoWatches to finish registering
        // locals and args that may be added or deleted after a step or
        // breakpoint. On return, UpdatelocalsWatches() will be called
        // when the barrier reaches the top of the queue.
        if (didAddVars)
        {
            m_actions.Add(new dbg_mi::GdbCmd_BarrierCallBack(__FUNCTION__, __LINE__, this,
                        &Debugger_GDB_MI::UpdateLocalsWatches));
            return;
        }
        else //update any old watches whos structure may have changed
            UpdateLocalsWatches();

    }//end if step 1

    return;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateLocalsWatches()
// ----------------------------------------------------------------------------
{

    if (not (m_localsWatch or m_funcArgsWatch))
        return;

    // Update all watches with one command
    // uses: m_actions.Add(new dbg_mi::WatchesUpdateAction(m_watchesContainer, m_execution_logger));
    UpdateAllWatches();
    return;

    // ----------------------------------------------------------------------------
    // Working Example of iterating watches to update single watches
    // ----------------------------------------------------------------------------

//    // **Debugging**
//    //Call updateSingleWatch() to refresh the watches window.
//    if (m_localsWatch) {
//    for (int i = 0; i < m_localsWatch->GetChildCount(); ++i)
//    {
//        cb::shared_ptr<GDBWatch> child = cb::static_pointer_cast<GDBWatch>(m_localsWatch->GetChild(i));
//
//            // **Debugging**
//            //    wxString ID, symbol, value, type;
//            //    child->GetID(); //always before first update
//            //    child->GetSymbol(symbol);
//            //    child->GetValue(value);
//            //    child->GetType(type);
//
//        UpdateSingleWatch(child);
//
//    }}//endiffor
//
//    if (m_funcArgsWatch) {
//    for (int i = 0; i < m_funcArgsWatch->GetChildCount(); ++i)
//    {
//        cb::shared_ptr<GDBWatch> child = cb::static_pointer_cast<GDBWatch>(m_funcArgsWatch->GetChild(i));
//
//            // **Debugging**
//            //    wxString ID, symbol, value, type;
//            //    child->GetID(); //always before first update
//            //    child->GetSymbol(symbol);
//            //    child->GetValue(value);
//            //    child->GetType(type);
//
//        UpdateSingleWatch(child);
//
//    }}//endiffor
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DeleteAllWatches()
// ----------------------------------------------------------------------------
{
    dbg_mi::WatchesContainer::iterator it = m_watchesContainer.begin();
    cbWatchesDlg* dlg = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    while (it != m_watchesContainer.end())
    {
        // delete thru the WatchesDlg so that display window gets updated
        //-dlg->DeleteWatch(*it);
        dlg->RemoveWatch(*it);
        it = m_watchesContainer.begin();
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::RemoveProjectWatches(const cbProject* project) //(pecan 2012/07/31)
// ----------------------------------------------------------------------------
{
    dbg_mi::WatchesContainer::iterator it = m_watchesContainer.begin();
    cbWatchesDlg* dlg = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    while (it != m_watchesContainer.end())
    {
        if (project == (*it)->GetProject())
        {
            // delete thru the WatchesDlg so that display window gets updated
            //-dlg->DeleteWatch(*it);
            dlg->RemoveWatch(*it);
        }
        else
            ++it;
    }
}
// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetBreakpointsCount() const
// ----------------------------------------------------------------------------
{
    return m_breakpoints.size();
}
// Get bkpt count for single project
// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetBreakpointsCount(const cbProject* project)
// ----------------------------------------------------------------------------
{
    int knt = 0;
    for(Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
    {
        if ((*it)->GetProject() == project)
            knt += 1;
    }
    return knt;
}
// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetWatchesCount()
// ----------------------------------------------------------------------------
{
    return m_watchesContainer.size();
}
// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetWatchesCount(const cbProject* project)
// ----------------------------------------------------------------------------
{
    int knt = 0;
    for(dbg_mi::WatchesContainer::iterator it = m_watchesContainer.begin(); it != m_watchesContainer.end(); ++it)
    {
        dbg_mi::Watch::Pointer wp = (*it);
        if (wp->GetProject() == project)
            knt += 1;
    }
    return knt;
}

// ----------------------------------------------------------------------------
cbBreakpoint::Pointer Debugger_GDB_MI::GetBreakpoint(int index)
// ----------------------------------------------------------------------------
{
    return cb::static_pointer_cast<cbBreakpoint>(m_breakpoints[index]);
}
// ----------------------------------------------------------------------------
cbBreakpoint::ConstPointer Debugger_GDB_MI::GetBreakpoint(int index) const
// ----------------------------------------------------------------------------
{
    return cb::static_pointer_cast<const cbBreakpoint>(m_breakpoints[index]);
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateBreakpoint(cbBreakpoint::Pointer breakpoint)
// ----------------------------------------------------------------------------
{
    bool changed = false;
    dbg_mi::Breakpoint temp;
    dbg_mi::Breakpoint current;
    dbg_mi::Breakpoint::Pointer bp;

    for(Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
    {
        if (it == m_breakpoints.end())
            return;
        if (*it != breakpoint)
            continue;
        if (*it == breakpoint)
        {
            current = **it;
            bp = *it;
            break;
        }
    }

        temp = current;

        //-case cbBreakpoint::Code:
        if (breakpoint->GetType() == _T("Code"))
        {
            dbg_mi::EditBreakpointDlg dialog(temp);
            PlaceWindow(&dialog);
            if(dialog.ShowModal() == wxID_OK)
            {
                // if the breakpoint is not sent to debugger, just copy

                //-if(current.GetIndex() != -1 || !IsExecutorRunning())

                if (current.IsEnabled() != temp.IsEnabled())
                    changed = true;

                if(current.GetIndex() != -1 and !IsExecutorRunning()) //active bkpt and debugger not running
                {
                    current = temp;
                    current.SetIndex(-1);
                }
                else
                //if(current.GetIndex() != -1 || !IsExecutorStopped())
                {
                    bool resume = !m_executor.IsStopped();
                    if(resume)
                        m_executor.Interrupt();

                    if(breakpoint->IsEnabled() != temp.IsEnabled())
                    {
                        //if(temp.IsEnabled())
                        //    AddStringCommand(wxString::Format(_T("-break-enable %d"), temp.GetIndex()));
                        //else
                        //    AddStringCommand(wxString::Format(_T("-break-disable %d"), temp.GetIndex()));
                        changed = true;
                    }

                    // count and conditions
                    if (current.GetInfo() != temp.GetInfo())
                        changed = true;

                    if(changed)
                    {
                        current = temp;
                        //-m_executor.Interrupt();
                        //-Continue();
                    }
                }//else

            }//if ok
        }//if code

        // case cbBreakpoint::Data:
        if (breakpoint->GetType() == _T("Data"))
        {
            int old_sel = 0;
            if (current.GetBreakOnDataRead() && current.GetBreakOnDataWrite())
                old_sel = 2;
            else if (not current.GetBreakOnDataRead() && current.GetBreakOnDataWrite() )
                old_sel = 1;
            dbg_mi::DataBreakpointDlg dlg(Manager::Get()->GetAppWindow(), current.GetDataAddress(), current.IsEnabled(), old_sel);
            PlaceWindow(&dlg);
            if (dlg.ShowModal() == wxID_OK)
            {
                bool resume = !m_executor.IsStopped();
                if(resume)
                    m_executor.Interrupt();

                temp.SetEnabled( dlg.IsBreakpointEnabled());
                temp.SetBreakOnDataRead( dlg.GetSelection() != 1);
                temp.SetBreakOnDataWrite( dlg.GetSelection() != 0);
                temp.SetDataAddress( dlg.GetDataExpression());

                if (current.GetInfo() != temp.GetInfo())
                    changed = true;

                // when disabled, just issure the disable and copy any changes
                // when enabled with changes, delete then re-issue the data brkpt
                if (not temp.IsEnabled())
                {
                        // just disable, don't delete then re-issue
                        EnableBreakpoint(breakpoint, false);
                        bp->SetEnabled(false);
                        if (changed)
                            *bp = temp;
                        changed = false;
                }
                 //bkpt is enabled
                else if ( current.IsEnabled() != temp.IsEnabled())
                {
                    if(temp.IsEnabled() and (not changed))
                    {
                        // no changes, just enable
                        EnableBreakpoint(breakpoint, true);
                    }
                    // stow any enabling change
                    bp->SetEnabled(temp.IsEnabled());
                }

                // data bkpt conditions have changed w/o disabling/enabling changes
                if(changed)
                {
                    // stow the user changes
                    *bp = temp;
                    //-m_executor.Interrupt();
                    //-Continue();
                }
            }

        }//if data
        if (changed)
        {
            ResetBreakpoint(temp);
        }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ResetBreakpoint(dbg_mi::Breakpoint& breakpoint)
// ----------------------------------------------------------------------------
{
    cbBreakpointsDlg *dlg = Manager::Get()->GetDebuggerManager()->GetBreakpointDialog();
    dlg->RemoveBreakpoint(this, breakpoint.GetLocation(), breakpoint.GetLine());
    breakpoint.SetIndex(-1); //say this is new breakpoint
    dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint);
    *ptr = breakpoint;
    AddBreakpoint(ptr);
    dlg->Reload();
}

//-void Debugger_GDB_MI::DeleteBreakpoint(cbBreakpoint::Pointer breakpoint)
 // FIXME: on occasion mutltiple brkpts with the same line number )
 //     get recorded. We need the means to iterate thru the bkpoints and remove
 //     all with the same line number.
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DeleteBreakpoint(cbBreakpoint::Pointer breakpoint)
// ----------------------------------------------------------------------------
{
    // override pure function from CbPlugin

    if(not IsExecutorStopped())
    {
        // should something be done ?
    }

    /// Debugger is paused
    Breakpoints::iterator it = std::find(m_breakpoints.begin(), m_breakpoints.end(), breakpoint);
    if (it != m_breakpoints.end())
    {
        DebugLog(wxString::Format(_T("Debugger_GDB_MI::DeleteBreakpoint: %s:%d"),
                                  breakpoint->GetLocation().c_str(), breakpoint->GetLine()));

        int index = (*it)->GetIndex();
        if (index != -1)
        {
            if (not IsExecutorStopped())
            {
                m_executor.Interrupt();
                AddStringCommand(wxString::Format(_T("-break-delete %d"), index));
                Continue();
            }
            else
                AddStringCommand(wxString::Format(_T("-break-delete %d"), index));

            m_breakpoints.erase(it);
            return;
        }

        // if debugger not running or paused, allow CB to remove breakpoint
        // Have breakpoing but breakpoint has no number
        {//  removed the if() condition
            m_breakpoints.erase(it);
            return;
        }
    }
    return ;   //didnt find breakpoint
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DeleteAllBreakpoints()
// ----------------------------------------------------------------------------
{

    if(IsRunning())
    {
        wxString breaklist;
        for(Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
        {
            dbg_mi::Breakpoint &current = **it;
            if(current.GetIndex() != -1)
                breaklist += wxString::Format(wxT(" %d"), current.GetIndex());
        }

        if(!breaklist.empty())
        {
            if(!IsStopped())
            {
                m_executor.Interrupt();

                AddStringCommand(wxT("-break-delete") + breaklist);
                Continue();
            }
            else
                AddStringCommand(wxT("-break-delete") + breaklist);
        }
    }
    m_breakpoints.clear();
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ShiftBreakpoint(int index, int lines_to_shift)
// ----------------------------------------------------------------------------
{

    if (index < 0 || index >= static_cast<int>(m_breakpoints.size()))
        return;
    cb::shared_ptr<dbg_mi::Breakpoint> bp = m_breakpoints[index];
    bp->ShiftLine(lines_to_shift);

    if(IsRunning())
    {
        // just remove the breakpoints as they will become invalid
        if(not IsStopped())
        {
            m_executor.Interrupt();
            if (bp->GetIndex()>=0)
            {
                AddStringCommand(wxString::Format(wxT("-break-delete %d"), bp->GetIndex()));
                bp->SetIndex(-1);
            }
            Continue();
        }
        else
        {
            if (bp->GetIndex()>=0)
            {
                AddStringCommand(wxString::Format(wxT("-break-delete %d"), bp->GetIndex()));
                bp->SetIndex(-1);
            }
        }
    }

    return;


        // code crashes on entry to change the second line.
//
//    // MOD: Inserted/Deleted lines will not be know by the debugger
//    if (IsRunning())
//    {
//        return; //new editor hook will catch this.
//
//        //-cbMessageBox(_T("Editing source invalidates debugger data."), _T("Closing debugger"), wxOK, Manager::Get()->GetAppWindow());
//        // close/exit the debugger
//        //-int dbgStopID = wxFindMenuItemId(Manager::Get()->GetAppFrame(), _("Debug"), _("Stop debugger"));
//        //-if (dbgStopID > wxNOT_FOUND)
//        //-{
//        //-    wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, dbgStopID);
//        //-    Manager::Get()->GetAppWindow()->GetEventHandler()->AddPendingEvent(evt);
//        //-}
//    }
//
//    cbBreakpointsDlg *dlg = Manager::Get()->GetDebuggerManager()->GetBreakpointDialog();
//    for (Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
//    {
//        std::advance(it, index);
//        if (it != m_breakpoints.end())
//        {
//            //-m_State.ShiftBreakpoint(*it, lines_to_shift);
//            //-RemoveBreakpoint(*it);
//            dbg_mi::Breakpoint::Pointer pBkpt = std::shared_ptr<dbg_mi::Breakpoint>();
//            pBkpt = cb::static_pointer_cast<dbg_mi::Breakpoint>(GetBreakpoint(index));
//            wxString filename; int line;
//            filename = pBkpt->GetLocation();
//            line = pBkpt->GetLine();
//            dlg->RemoveBreakpoint(this, (*it)->GetLocation(), (*it)->GetLine());
//            line += lines_to_shift;
//            pBkpt->SetLine(line);
//            pBkpt->SetIndex(-1);
//            //AddBreakpoint(pBkpt);
//            dlg->AddBreakpoint(this, (*it)->GetLocation(), (*it)->GetLine());
//        }
//    }//for
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::EnableBreakpoint(cb::shared_ptr<cbBreakpoint> breakpoint, bool enable)
// ----------------------------------------------------------------------------
{
    Breakpoints::iterator it = std::find(m_breakpoints.begin(), m_breakpoints.end(), breakpoint);
    if (it != m_breakpoints.end())
    {
        DebugLog(wxString::Format(_T("Debugger_GDB_MI::EnableBreakpoint: %s:%d"),
                                  breakpoint->GetLocation().c_str(), breakpoint->GetLine()));
        int index = (*it)->GetIndex();
        if (index != -1)
        {
            wxString toggled = enable ? _T("enable") : _T("disable");
            if (not IsExecutorStopped())
            {
                m_executor.Interrupt();
                AddStringCommand(wxString::Format(_T("-break-%s %d"), toggled.c_str(), index));
                Continue();
            }
            else
                AddStringCommand(wxString::Format(_T("-break-%s %d"), toggled.c_str(), index));
        }
        (*it)->SetEnabled(enable);
    }
}

// ----------------------------------------------------------------------------
int Debugger_GDB_MI::GetThreadsCount() const
// ----------------------------------------------------------------------------
{
    return m_threads.size();
}

// ----------------------------------------------------------------------------
cbThread::ConstPointer Debugger_GDB_MI::GetThread(int index) const
// ----------------------------------------------------------------------------
{
    return m_threads[index];
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::SwitchToThread(int thread_number)
// ----------------------------------------------------------------------------
{
    if(IsExecutorStopped())
    {
        dbg_mi::SwitchToThread<Notifications> *a;
        a = new dbg_mi::SwitchToThread<Notifications>(thread_number, m_execution_logger,
                                                      Notifications(this, m_executor, true));
        m_actions.Add(a);
        return true;
    }
    else
        return false;
}

// ----------------------------------------------------------------------------
cb::shared_ptr<cbWatch> Debugger_GDB_MI::AddWatch(const wxString& symbol, bool updateImmediately)
// ----------------------------------------------------------------------------
{
    // override of cbPlugin pure function

    wxUnusedVar(updateImmediately);
    // A new watch is always updated, the param is not needed for this plugin

    dbg_mi::Watch::Pointer newWatch(new dbg_mi::Watch(symbol, false, Manager::Get()->GetProjectManager()->GetActiveProject()));
    m_watchesContainer.push_back(newWatch);

    if(IsExecutorRunning())
        m_actions.Add(new dbg_mi::WatchCreateAction(newWatch, m_watchesContainer, m_execution_logger));
    return newWatch;
}
// ----------------------------------------------------------------------------
cb::shared_ptr<cbWatch> Debugger_GDB_MI::AddMemoryRange(uint64_t address, uint64_t size, const wxString &symbol, bool update)
// ----------------------------------------------------------------------------
{
    /// There are no calls to this function //(ph 2024/03/11)
    GetLogMgr()->LogError("AddMemoryRange() call, but not tested");

    cb::shared_ptr<dbg_mi::GDBMemoryRangeWatch> watch(new dbg_mi::GDBMemoryRangeWatch(address, size, symbol));
    m_memoryRanges.push_back(watch);
    m_mapWatchesToType[watch] = dbg_mi::WatchType::MemoryRange;

    //-if (m_pPipedProcess && update) unreachable piped process is in GDBExecutor //(ph 2024/03/04)
    if (update)
        //m_memoryRanges.back();
        std::ignore = m_memoryRanges.back(); ///fix this


    return watch;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::AddTooltipWatch(const wxString &symbol, wxRect const &rect)
// ----------------------------------------------------------------------------
{
    // Display value of variable under the cursor in a popup window

    //-if (Manager::Get()->GetDebuggerManager()->GetInterfaceFactory()->GetAwaitingDbgResponse())
    //-    return;
    //-Manager::Get()->GetDebuggerManager()->GetInterfaceFactory()->SetAwaitingDbgResponse(true);

    dbg_mi::Watch::Pointer w(new dbg_mi::Watch(symbol, true, Manager::Get()->GetProjectManager()->GetActiveProject()));
    m_watchesContainer.push_back(w);

    if(IsExecutorRunning())
        m_actions.Add(new dbg_mi::WatchCreateTooltipAction(w, m_watchesContainer, m_execution_logger, rect));
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DeleteWatch(cbWatch::Pointer watch)
// ----------------------------------------------------------------------------
{
    DebugLog(wxString::Format(_T("DeleteWatch entered")));
    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);
    if(it == m_watchesContainer.end())
    {
        return;
    }

    if (not (*it)->GetID().empty() // validate watch has id
            and IsExecutorRunning() )
    {
        if(IsExecutorStopped())
            AddStringCommand(_T("-var-delete ") + (*it)->GetID());
        else
        {
            m_executor.Interrupt();
            AddStringCommand(_T("-var-delete ") + (*it)->GetID());
            Continue();
        }
    }
    m_watchesContainer.erase(it);
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::AutoUpdateWatch(cbWatch::Pointer watch)
// ----------------------------------------------------------------------------
{
    DebugLog(wxString::Format(_T("AutoUpdateWatch entered")));
    if(not IsExecutorStopped())
    {
        //...
    }

    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);
    if(it == m_watchesContainer.end())
    {
        return;
    }

    if (not (*it)->GetID().empty()) // validate watch has id
    if(IsExecutorRunning())
    {
        wxString varFreeze = _T("-var-set-frozen ") + (*it)->GetID() + ((*it)->IsAutoUpdateEnabled()?_T(" 0"):_T(" 1"));
        if(IsExecutorStopped())
            AddStringCommand(varFreeze);
        else
        {
            m_executor.Interrupt();
            AddStringCommand(varFreeze);
            Continue();
        }
    }
    //-m_watches.erase(it);
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::HasWatch(cb::shared_ptr<cbWatch> watch)
// ----------------------------------------------------------------------------
{
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), watch);
    return it != m_watchesContainer.end();
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ShowWatchProperties(cb::shared_ptr<cbWatch> watch)
// ----------------------------------------------------------------------------
{
    //begin: EditWatches support
    // not supported for child nodes!
    if (watch->GetParent())
        return;

    dbg_mi::Watch::Pointer real_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);
    EditWatchDlg dlg(real_watch, Manager::Get()->GetAppWindow());

    if (dlg.ShowModal() == wxID_OK)
    {
        // update the selected watch children
        if (real_watch->HasBeenExpanded())
        {
            CollapseWatch(real_watch);
            m_actions.Add(new dbg_mi::BarrierAction);
            real_watch->SetHasBeenExpanded(false);
            ExpandWatch(real_watch);
            m_actions.Add(new dbg_mi::BarrierAction);
        }
        else
        {   // single element type
            m_actions.Add(new dbg_mi::WatchEvaluateExpression(real_watch, m_watchesContainer, m_execution_logger));
        }
        //end EditWatches support

    }//ShowModal
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::GetWatchValue(cb::shared_ptr<cbWatch> watch, wxString& value) // (ph 26/05/15)
// ----------------------------------------------------------------------------
{
    //begin: EditWatches support
    // not supported for child nodes!
    if (watch->GetParent())
        return;

    dbg_mi::Watch::Pointer real_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);
        // update the selected watch children
        if (real_watch->HasBeenExpanded())
        {
            CollapseWatch(real_watch);
            m_actions.Add(new dbg_mi::BarrierAction);
            real_watch->SetHasBeenExpanded(false);
            ExpandWatch(real_watch);
            m_actions.Add(new dbg_mi::BarrierAction);
        }
        else
        {   // single element type
            m_actions.Add(new dbg_mi::WatchEvaluateExpression(real_watch, m_watchesContainer, m_execution_logger));
            real_watch->GetValue(value);
        }
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::SetWatchValue(cb::shared_ptr<cbWatch> watch, const wxString &value)
// ----------------------------------------------------------------------------
{
    if(not IsExecutorStopped() || !IsExecutorRunning())
        return false;

    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);

    if(it == m_watchesContainer.end())
        return false;

    dbg_mi::Watch::Pointer real_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);

    AddStringCommand(_T("-var-assign ") + real_watch->GetID() + _T(" ") + value);

//    m_actions.Add(new dbg_mi::WatchSetValueAction(*it, static_cast<dbg_mi::Watch*>(watch), value, m_execution_logger));
    dbg_mi::Action *update_action = new dbg_mi::WatchesUpdateAction(m_watchesContainer, m_execution_logger);
    update_action->SetWaitPrevious(true);
    m_actions.Add(update_action);

    return true;
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ExpandWatch(cb::shared_ptr<cbWatch> watch)
// ----------------------------------------------------------------------------
{
    if( (not IsExecutorStopped()) || (not IsExecutorRunning()) )
        return;

    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);
    if(it != m_watchesContainer.end())
    {
        dbg_mi::Watch::Pointer real_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);
        if(not real_watch->HasBeenExpanded())
            m_actions.Add(new dbg_mi::WatchExpandedAction(*it, real_watch, m_watchesContainer, m_execution_logger));
    }
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::CollapseWatch(cb::shared_ptr<cbWatch> watch)
// ----------------------------------------------------------------------------
{
    if(not IsExecutorStopped() || !IsExecutorRunning())
        return;

    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);
    if(it != m_watchesContainer.end())
    {
        dbg_mi::Watch::Pointer real_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);
        if(real_watch->HasBeenExpanded())
            m_actions.Add(new dbg_mi::WatchCollapseAction(*it, real_watch, m_watchesContainer, m_execution_logger));
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateWatch(cb::shared_ptr<cbWatch> watch)
// ----------------------------------------------------------------------------
{
    // Required by pure fuction in cbPlugin
    if(not IsExecutorStopped() || !IsExecutorRunning())
        return;

    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);
    if(it != m_watchesContainer.end())
    {
        m_actions.Add(new dbg_mi::WatchesUpdateAction(m_watchesContainer, m_execution_logger, *it));
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateSingleWatch(cb::shared_ptr<cbWatch> watch)
// ----------------------------------------------------------------------------
{
    // clone of UpdateWatch() above with a rememberable name

    if(not IsExecutorStopped() || !IsExecutorRunning())
        return;

    cbWatch::Pointer root_watch = cbGetRootWatch(watch);
    dbg_mi::WatchesContainer::iterator it = std::find(m_watchesContainer.begin(), m_watchesContainer.end(), root_watch);
    if(it != m_watchesContainer.end())
    {
        // Use the native cb static cast with the correct namespace qualification
        dbg_mi::Watch::Pointer derived_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);

        m_actions.Add(new dbg_mi::WatchesUpdateAction(m_watchesContainer, m_execution_logger, derived_watch));
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::UpdateAllWatches()
// ----------------------------------------------------------------------------
{
    // Update all watches

    if(not IsExecutorStopped() || !IsExecutorRunning())
        return;

    m_actions.Add(new dbg_mi::WatchesUpdateAction(m_watchesContainer, m_execution_logger));
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::SendCommand(const wxString& cmd, bool WXUNUSED(debugLog))
// ----------------------------------------------------------------------------
{
    if(not IsExecutorRunning())
    {
        wxString message(_("Command will not be executed because the debugger is not running!"));
        Log(message);
        DebugLog(message);
        return;
    }
    if(not IsExecutorStopped())
    {
        wxString message(_("Command will not be executed because the debugger/debuggee is not paused/interupted!"));
        Log(message);
        DebugLog(message);
        return;
    }

    if (cmd.empty())
        return;

    DoSendCommand(cmd);
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DoSendCommand(const wxString& cmd)
// ----------------------------------------------------------------------------
{
    wxString escaped_cmd = cmd;
    escaped_cmd.Replace(_T("\n"), _T("\\n"), true);

    if (escaped_cmd[0] == _T('-'))
        AddStringCommand(escaped_cmd);
    else
    {
        escaped_cmd.Replace(_T("\\"), _T("\\\\"), true);
        AddStringCommand(_T("-interpreter-exec console \"") + escaped_cmd + _T("\""));
    }
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::AttachToProcess(const wxString& pid)
// ----------------------------------------------------------------------------
{
    m_project = NULL;

    long number;
    if (not pid.ToLong(&number))
        return;

    m_PidToAttach = number; //(ph 2025/01/22)

    LaunchDebugger(GetActiveConfigEx().GetDebuggerExecutable(), wxEmptyString, wxEmptyString,
                   wxEmptyString, number, false, StartTypeRun);

    m_executor.SetAttachedPID(number);
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DetachFromProcess()
// ----------------------------------------------------------------------------
{
    AddStringCommand(wxString::Format(_T("-target-detach %ld"), m_executor.GetAttachedPID()));
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::IsAttachedToProcess() const
// ----------------------------------------------------------------------------
{
    return m_PidToAttach != 0;
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::RequestUpdate(DebugWindows window)
// ----------------------------------------------------------------------------
{
    if(not IsExecutorStopped())
        return;

    switch(window)
    {
    case Backtrace:
        {
            struct Switcher : dbg_mi::SwitchToFrameInvoker
            {
                Switcher(Debugger_GDB_MI *plugin, dbg_mi::ActionsMap &actions) :
                    m_plugin(plugin),
                    m_actions(actions)
                {
                }

                virtual void Invoke(int frame_number)
                {
                    typedef dbg_mi::SwitchToFrame<SwitchToFrameNotification> SwitchType;
                    m_actions.Add(new SwitchType(frame_number, SwitchToFrameNotification(m_plugin, frame_number), false));
                }

                Debugger_GDB_MI *m_plugin;
                dbg_mi::ActionsMap& m_actions;
            };

            Switcher *switcher = new Switcher(this, m_actions);
            m_actions.Add(new dbg_mi::GenerateBacktrace(switcher, m_backtrace, m_current_frame, m_execution_logger));
        }
        break;

    case Threads:
    {
        m_actions.Add(new dbg_mi::GenerateThreadsList(m_threads, m_current_frame.GetThreadId(), m_execution_logger));
        break;
    }

    case CPURegisters:
        {
            m_actions.Add(new dbg_mi::InfoRegisters(m_execution_logger, wxEmptyString));
            break;
        }
    case Disassembly:
        {
            bool flavour = Manager::Get()->GetDebuggerManager()->IsDisassemblyMixedMode();
            m_actions.Add(new dbg_mi::GenerateDisassembly(m_execution_logger, flavour, wxEmptyString));
            break;
        }
    case ExamineMemory:
        {
            m_actions.Add(new dbg_mi::ExamineMemory(m_execution_logger));
            break;
        }
    case Watches:
        {
            // watches window must be visible, else ignore the request
            if (not IsWindowReallyShown(Manager::Get()->GetDebuggerManager()->GetWatchesDialog()->GetWindow()))
                break;

            DebuggerConfiguration& active_config = GetActiveConfigEx();
            int requestLocalsOrArgs = 0;

            // Display results for locals(1) and or args(2)
            if (active_config.GetFlag(DebuggerConfiguration::WatchLocals))
                requestLocalsOrArgs |= 1;

            if (active_config.GetFlag(DebuggerConfiguration::WatchFuncArgs))
                requestLocalsOrArgs |= 2;

            if (requestLocalsOrArgs)
                {;}//-DoLocalWatches(2); // (ph 26/06/18) deprecated

            break;
        }
    default:
        {break;}
    }//switch
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::GetCurrentPosition(wxString &filename, int &line)
// ----------------------------------------------------------------------------
{
    m_current_frame.GetPosition(filename, line);
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::KillConsole()
// ----------------------------------------------------------------------------
{
#ifdef __WXMSW__
    if(m_console_pid >= 0)
    {
        wxKill(m_console_pid, wxSIGKILL, nullptr, wxKILL_CHILDREN);
        m_console_pid = -1;
    }
#else
    // kill any linux console
    if ( m_console_pid && (m_console_pid > 0) )
    {
       ::wxKill(m_console_pid, wxSIGINT, nullptr, wxKILL_CHILDREN); //ticket 1571
        m_console_pid = 0;
    }
#endif
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnValueTooltip(const wxString &token, const wxRect &evalRect)
// ----------------------------------------------------------------------------
{
    // Display value of var under the cursor
    AddTooltipWatch(token, evalRect);
}

// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::ShowValueTooltip(int style)
// ----------------------------------------------------------------------------
{
    if (not IsExecutorRunning() || !IsExecutorStopped())
        return false;

    //-if (style != wxSCI_C_DEFAULT && style != wxSCI_C_OPERATOR && style != wxSCI_C_IDENTIFIER && style != wxSCI_C_WORD2)
    //-    return false;
    if (style != wxSCI_C_DEFAULT && style != wxSCI_C_OPERATOR && style != wxSCI_C_IDENTIFIER &&
        style != wxSCI_C_WORD2 && style != wxSCI_C_GLOBALCLASS)
    {
        return false;
    }

    return true;
}
//////Begin Watches context menu
////// ----------------------------------------------------------------------------
////void Debugger_GDB_MI::OnWatchesContextMenu(wxMenu &menu, const cbWatch &watch, wxObject *property, int &disabledMenus)
////// ----------------------------------------------------------------------------
////{
////    wxUnusedVar(menu); wxUnusedVar(property);
////    wxString type, symbol;
////    watch.GetType(type);
////    watch.GetSymbol(symbol);
////
//////    if (IsPointerType(type))
//////    {
//////        menu.InsertSeparator(0);
//////        menu.Insert(0, idMenuWatchDereference, _("Dereference ") + symbol);
//////        m_watchToDereferenceSymbol = symbol;
//////        m_watchToDereferenceProperty = property;
//////    }
////
////    if (watch.GetParent())
////    {
////        disabledMenus = WatchesDisabledMenuItems::Rename;
////        disabledMenus |= WatchesDisabledMenuItems::Properties;
////        disabledMenus |= WatchesDisabledMenuItems::Delete;
////        disabledMenus |= WatchesDisabledMenuItems::AddDataBreak;
////    }
////} //End: Watches context menu

// ----------------------------------------------------------------------------
namespace //from debuggerGDB //(ph 2024/03/08)
// ----------------------------------------------------------------------------
{
    wxString createSymbolFromWatch(const cbWatch &watch)
    {
        wxString symbol;
        watch.GetSymbol(symbol);

        cb::shared_ptr<const cbWatch> parentWatch = watch.GetParent();
        if(parentWatch)
        {
            wxString parent = createSymbolFromWatch(*parentWatch);
            if(!parent.IsEmpty())
                return parent + "." + symbol;
        }
        return symbol;
    }
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnWatchesContextMenu(wxMenu &menu, const cbWatch &watch, wxObject *property, int &disabledMenus)
// ----------------------------------------------------------------------------
{
    // Called from watchesdlg.cpp

    wxString type, symbol;
    watch.GetType(type);
    watch.GetSymbol(symbol);

    if (dbg_mi::IsPointerType(type))
    {
        menu.InsertSeparator(0);
        menu.Insert(0, idMenuWatchDereference, _("Dereference ") + symbol);
        m_watchToDereferenceSymbol = symbol;
        m_watchToDereferenceProperty = property;
    }

    if (watch.GetParent())
    {
        disabledMenus = WatchesDisabledMenuItems::Rename;
        disabledMenus |= WatchesDisabledMenuItems::Properties;
        disabledMenus |= WatchesDisabledMenuItems::Delete;
        disabledMenus |= WatchesDisabledMenuItems::AddDataBreak;
        disabledMenus |= WatchesDisabledMenuItems::ExamineMemory;

        menu.InsertSeparator(0);
        menu.Insert(0, idMenuWatchSymbol, _("Watch ") + symbol);
        m_watchToAddSymbol = createSymbolFromWatch(watch);
    }
}

//begin save breakpoints & watches for this project

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnMenuWatchDereference(cb_unused wxCommandEvent& event) //(ph 2024/03/08)
// ----------------------------------------------------------------------------
{
    cbWatchesDlg *watches = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    if (!watches)
        return;

    watches->RenameWatch(m_watchToDereferenceProperty, wxT("*") + m_watchToDereferenceSymbol);
    m_watchToDereferenceProperty = NULL;
    m_watchToDereferenceSymbol = wxEmptyString;
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnMenuWatchSymbol(cb_unused wxCommandEvent& event)    //(ph 2024/03/08)
// ----------------------------------------------------------------------------
{
    cbWatchesDlg *watches = Manager::Get()->GetDebuggerManager()->GetWatchesDialog();
    if (!watches)
        return;

    watches->AddWatch(AddWatch(m_watchToAddSymbol, true));
    m_watchToAddSymbol = wxEmptyString;
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::LoadProjectWatches(cbProject* pProject)
// ----------------------------------------------------------------------------
{

    ///  never called // (ph 26/06/18)

    // This routine is called from cbCVT_PROJECT_ACTIVATE, cbCVT_PROJECT_OPEN

    cbProject* pDeactivatingProject = Manager::Get()->GetProjectManager()->GetActiveProject();

    if ( pProject )
        m_currentActiveProject = pProject;

    if ( pDeactivatingProject != pProject )
    {
        // avoid "activating" projects, only want "activated" projects
        //don't proceed with new project until it's officially activated.
        return;
    }

    if (Manager::Get()->GetProjectManager()->IsLoadingOrClosing())
    {   // avoid projects "activating" during workspace loading
        return;
    }

    if (not pProject)
        return;

    // clear Watches window
    DeleteAllWatches();

    // Don't duplicate the watches
    unsigned watchKnt = GetWatchesCount(pProject);
    if ( watchKnt )
        return;

    wxString projectFilename = pProject->GetFilename();
    wxFileName fn(projectFilename);
    //-fn.SetExt(_T("prj"));
    fn.SetExt(_T("<fileExtn>"));
    if (not fn.FileExists())
    {
        DebugLog(_("LoadProjectWatchess:File Not Found:") + fn.GetFullPath());
        return;
    }
    // Open the <projectname>.prj file (it's in MS ini format)
    wxFileConfig config( wxTheApp->GetAppName(), _T("Embedded"),
         fn.GetFullPath(), fn.GetFullPath(), wxCONFIG_USE_LOCAL_FILE );

    //"<prjname>_Watches" was the old group name before remote debugging
    // We now prefer to just use "Watches" for any debugging
    wxString cfgGroup = _T("Watches");
    if (config.HasGroup(_T("<prjname>_Watches")))
        cfgGroup = _T("<prjname>_Watches");
    config.SetPath(cfgGroup);

    unsigned wpKnt = config.GetNumberOfEntries();
    if (0 == wpKnt)
        return;

    for (unsigned ii = 0; ii < wpKnt; ++ii)
    {
        wxString entryName = wxString::Format(_T("%d"), ii+1);
        wxString entryValue = wxEmptyString;
        if (config.Read(entryName, &entryValue))
        {
            if (not entryValue.empty())
            {
                // .prj watch format entry changed from merely a symbol name to
                // "symbol;WatchFormat;IsArray;ArrayStart;ArrayCount"
                wxString symbol = wxEmptyString;
                dbg_mi::WatchFormat format = dbg_mi::Undefined;
                bool isArray = false;
                int  arrayStart = 0;
                int  arrayCount = 0;
                wxArrayString watchValues = GetArrayFromString(entryValue,_T(";"),true);
                if (watchValues.GetCount())
                    symbol = watchValues[0];
                if (watchValues.GetCount() > 1)
                    format = (dbg_mi::WatchFormat)dbg_mi::AnyToLong(watchValues[1]);
                if (watchValues.GetCount() > 2)
                    isArray = dbg_mi::AnyToLong(watchValues[2]);
                if (watchValues.GetCount() > 3)
                    arrayStart = dbg_mi::AnyToLong(watchValues[3]);
                if (watchValues.GetCount() > 4)
                    arrayCount = dbg_mi::AnyToLong(watchValues[4]);

                if (symbol.empty())
                    continue;
                //-cbWatch::Pointer watch = this->AddWatch(entryValue, bool UpdateImmediatly);
                cbWatch::Pointer watch = this->AddWatch(symbol,false);
                dbg_mi::Watch::Pointer real_watch = cb::static_pointer_cast<dbg_mi::Watch>(watch);
                real_watch->SetProject(pProject); //override "active project" with "activating project"
                real_watch->SetFormat( format );
                real_watch->SetArray( isArray != 0 );
                real_watch->SetArrayParams( arrayStart, arrayCount);

                if (watch.get())
                    Manager::Get()->GetDebuggerManager()->GetWatchesDialog()->AddWatch(watch);
            }
        }
    }//for

    // Clear current watches in config file else user deleted watches will get reloaded.
    config.SetPath(_T("/"));
    config.DeleteGroup(cfgGroup);
    config.Flush();

} //end LoadProjectWatches

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnDebuggerStoppedMode(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    // true == debugger stopped
    // false == debugger about to run
    bool stopped = event.GetInt();
    if (stopped)
    {
        // show the debugger line marker
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (not ed) return;
        wxString fn = ed->GetFilename();
        cbStyledTextCtrl* control = ed->GetControl();
        if (not control) return;
        long linenum = control->GetCurrentLine();
        // line number is zero oriented
        SyncEditor(fn, ++linenum, true);
    }
    else
    {
        ClearActiveMarkFromAllEditors();
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::EnableProgrammingFlash(bool trueorfalse)
// ----------------------------------------------------------------------------
{
    // Disable user ability to program flash
    // since the an external debugger device may do it for them.
    wxMenuBar* mbar = Manager::Get()->GetAppFrame()->GetMenuBar();
    int itemId = mbar->FindMenuItem(_("Embedded"), _("Program Device with Project Output (iflash)"));
    if (wxNOT_FOUND == itemId)
        return;
    mbar->Enable(itemId, trueorfalse);
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnEditorEventHook(cbEditor* pcbEditor, wxScintillaEvent& event)
// ----------------------------------------------------------------------------
{

    // Note GDB/MI is allowing source changes while the debugger is running
    // Inserted/Deleted/modified lines will not be known by the debugger

    // **Debugging**
    //    wxString txt = _T("OnEditorModified(): ");
    //    int flags = event.GetModificationType();
    //    if (flags & wxSCI_MOD_CHANGEMARKER) txt << _T("wxSCI_MOD_CHANGEMARKER, ");
    //    if (flags & wxSCI_MOD_INSERTTEXT) txt << _T("wxSCI_MOD_INSERTTEXT, ");
    //    if (flags & wxSCI_MOD_DELETETEXT) txt << _T("wxSCI_MOD_DELETETEXT, ");
    //    if (flags & wxSCI_MOD_CHANGEFOLD) txt << _T("wxSCI_MOD_CHANGEFOLD, ");
    //    if (flags & wxSCI_PERFORMED_USER) txt << _T("wxSCI_PERFORMED_USER, ");
    //    if (flags & wxSCI_MOD_BEFOREINSERT) txt << _T("wxSCI_MOD_BEFOREINSERT, ");
    //    if (flags & wxSCI_MOD_BEFOREDELETE) txt << _T("wxSCI_MOD_BEFOREDELETE, ");
    //    txt << _T("pos=")
    //        << wxString::Format(_T("%d"), event.GetPosition())
    //        << _T(", line=")
    //        << wxString::Format(_T("%d"), event.GetLine())
    //        << _T(", linesAdded=")
    //        << wxString::Format(_T("%d"), event.GetLinesAdded());
    //    Manager::Get()->GetLogManager()->DebugLog(txt);

    event.Skip();
    if (not pcbEditor->GetModified())
    {
        m_nEditorHookBusy = 0;
        return;
    }

    // Allow non-debugged source files be to modified
    ProjectFile* pf = pcbEditor->GetProjectFile();
    if ( (not pf)
        or (Manager::Get()->GetProjectManager()->GetActiveProject() != pf->GetParentProject()) )
    {
        m_nEditorHookBusy = 0;
        return;
    }

    // Decrement the busy count on Ctrl-Z Undo editor command
    if (event.GetModificationType() & wxSCI_PERFORMED_UNDO)
        {
            --m_nEditorHookBusy;
            if (m_nEditorHookBusy < 0) m_nEditorHookBusy = 0;
            return;
        }

    if (IsRunning() && ( event.GetEventType() == wxEVT_SCI_MODIFIED ))
    {

        ++m_nEditorHookBusy;
        // A user reports that the pop-up msg box causes crashes. This cannot be verified.
        // I suspect it's getting hidden behind the debugger external .exe
        //-if (m_nEditorHookBusy < 2)
        //-    cbMessageBox(_T("Editing source invalidates debugger data."), _T("Disallowed"), wxOK, Manager::Get()->GetAppWindow());
        int editUndoID = wxFindMenuItemId(Manager::Get()->GetAppFrame(), _("Edit"), _("Undo"));
        if (editUndoID > wxNOT_FOUND)
        {
            wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, editUndoID);
            Manager::Get()->GetAppWindow()->GetEventHandler()->AddPendingEvent(evt);
        }
    }
}//OnEditorEventHook
//begin ------ remote debugging --------------------------------
//#ifdef __WXMSW__
// ----------------------------------------------------------------------------
bool Debugger_GDB_MI::IsRemoteDebugging(ProjectBuildTarget* pTarget)
// ----------------------------------------------------------------------------
{
    assert(pTarget);
    RemoteDebugging* rd = GetRemoteDebuggingInfo(pTarget);
    bool remoteDebugging = rd && rd->IsOk();
    return remoteDebugging;
}
//#endif

// ----------------------------------------------------------------------------
RemoteDebuggingMap& Debugger_GDB_MI::GetRemoteDebuggingMap(cbProject* project)
// ----------------------------------------------------------------------------
{
    if (not project)
        //-project = m_pProject;
        assert(project);
    ProjectRemoteDebuggingMap::iterator it = m_RemoteDebugging.find(project);
    if (it == m_RemoteDebugging.end()) // create an empty set for this project
        it = m_RemoteDebugging.insert(m_RemoteDebugging.begin(), std::make_pair(project, RemoteDebuggingMap()));

    return it->second;
}
// remote debugging
// ----------------------------------------------------------------------------
RemoteDebugging* Debugger_GDB_MI::GetRemoteDebuggingInfo(ProjectBuildTarget* pTarget)
// ----------------------------------------------------------------------------
{
//    if (not m_pTarget)
//        return 0;
    assert(pTarget);

    cbProject* pProject = pTarget->GetParentProject();

    // first, project-level (straight copy)
    m_MergedRDInfo = GetRemoteDebuggingMap(pProject)[0];

    // then merge with target settings
    RemoteDebuggingMap::iterator it = GetRemoteDebuggingMap(pProject).find(pTarget);
    if (it != GetRemoteDebuggingMap(pProject).end())
    {
        m_MergedRDInfo.MergeWith(it->second);
    }
    return &m_MergedRDInfo;
}
// ----------------------------------------------------------------------------
wxArrayString& Debugger_GDB_MI::GetSearchDirs(cbProject* prj)
// ----------------------------------------------------------------------------
{
    SearchDirsMap::iterator it = m_SearchDirs.find(prj);
    if (it == m_SearchDirs.end()) // create an empty set for this project
        it = m_SearchDirs.insert(m_SearchDirs.begin(), std::make_pair(prj, wxArrayString()));

    return it->second;
}
// ----------------------------------------------------------------------------
wxString Debugger_GDB_MI::GetTargetRemoteCmd(RemoteDebugging* rd)
// ----------------------------------------------------------------------------
{
    const wxString targetRemote = rd->extendedRemote ? _T("target extended-remote ") : _T("target remote ");
    //-not supported yet- const wxString targetRemote = rd->extendedRemote ? _T("target extended-async ") : _T("target async ");
    wxString m_Cmd;

    switch (rd->connType)
    {
        case RemoteDebugging::TCP:
        {
            if (not rd->ip.IsEmpty() && !rd->ipPort.IsEmpty())
                m_Cmd << targetRemote << _T("tcp:") << rd->ip << _T(":") << rd->ipPort;
        }
        break;

        case RemoteDebugging::UDP:
        {
            if (not rd->ip.IsEmpty() && !rd->ipPort.IsEmpty())
                m_Cmd << targetRemote << _T("udp:") << rd->ip << _T(":") << rd->ipPort;
        }
        break;

        case RemoteDebugging::Serial:
        {
            if (not rd->serialPort.IsEmpty())
                m_Cmd << targetRemote << rd->serialPort;
        }
        break;

        default:
          /* emIDE change [JL] - always use remote target*/
            if (not rd->ip.IsEmpty() && !rd->ipPort.IsEmpty())
                m_Cmd << targetRemote << rd->ip << _T(":") << rd->ipPort;
            break;
    }

    Manager::Get()->GetMacrosManager()->ReplaceEnvVars(m_Cmd);

    if (not m_Cmd.IsEmpty())
        Log(_("Connecting to remote target"));
    else
        Log(_("Invalid settings for remote debugging!"));

    return m_Cmd;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::PrepareRemoteDebugging(/*bool isConsole,*/ int printElements)
// ----------------------------------------------------------------------------
{
    // default initialization

    // for the possibility that the program to be debugged is compiled under Cygwin
    //-if (platform::windows)
    //-    DetectCygwinMount();

#define GDB_PROMPT _T("cb_gdb:")
#define FULL_GDB_PROMPT _T(">>>>>>") GDB_PROMPT

    // make sure we 're using the prompt that we know and trust ;)
    //-QueueCommand(new DebuggerCmd(this, wxString(_T("set prompt ")) + FULL_GDB_PROMPT));
    //-m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set prompt ")) + FULL_GDB_PROMPT, m_execution_logger));


    // debugger version
    //-QueueCommand(new DebuggerCmd(this, _T("show version")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("show version")), m_execution_logger));

    // no confirmation
    //-QueueCommand(new DebuggerCmd(this, _T("set confirm off")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set confirm off")), m_execution_logger));

    // no wrapping lines
    //-QueueCommand(new DebuggerCmd(this, _T("set width 0")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set width 0")), m_execution_logger));

    // no pagination
    //-QueueCommand(new DebuggerCmd(this, _T("set height 0")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set height 0")), m_execution_logger));

    // allow pending breakpoints
    //-QueueCommand(new DebuggerCmd(this, _T("set breakpoint pending on")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set breakpoint pending on")), m_execution_logger));

    // show pretty function names in disassembly
    //-QueueCommand(new DebuggerCmd(this, _T("set print asm-demangle on")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set print asm-demangle on")), m_execution_logger));

    // unwind stack on signal
    //-QueueCommand(new DebuggerCmd(this, _T("set unwindonsignal on")));
    m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set unwindonsignal on")), m_execution_logger));

    // disalbe result string truncations
    //-QueueCommand(new DebuggerCmd(this, wxString::Format(wxT("set print elements %d"), printElements)));
    m_actions.Add(new dbg_mi::SimpleAction(wxString::Format(_T("set print elements %d"), printElements), m_execution_logger));

    //if (platform::windows && isConsole)
        //-QueueCommand(new DebuggerCmd(this, _T("set new-console on")));
        //m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("set new-console on")), m_execution_logger));

    //-flavour = m_pDBG->GetActiveConfigEx().GetDisassemblyFlavorCommand();
    //-QueueCommand(new DebuggerCmd(this, flavour));

    //-if (m_pDBG->GetActiveConfigEx().GetFlag(DebuggerConfiguration::CatchExceptions))
    //-{
    //-    m_catchThrowIndex = -1;
    //-    // catch exceptions
    //-    QueueCommand(new GdbCmd_SetCatch(this, wxT("throw"), &m_catchThrowIndex));
    //-}

//    // define all scripted types
//    m_Types.Clear();
//    InitializeScripting();

    // pass user init-commands
    //-wxString init = GetActiveConfigEx().GetInitialCommandsString(); //(ph 2024/03/11)
    wxString init = GetActiveConfigEx().GetInitCommands();
    MacrosManager *macrosManager = Manager::Get()->GetMacrosManager();
    macrosManager->ReplaceMacros(init);
    // commands are passed in one go, in case the user defines functions in there
    // or else it would lock up...
    if (not init.empty())
        //-QueueCommand(new DebuggerCmd(this, init));
        m_actions.Add(new dbg_mi::SimpleAction(init, m_execution_logger));

    // add search dirs
    wxArrayString& m_Dirs = GetSearchDirs(GetProject());
    for (unsigned int i = 0; i < m_Dirs.GetCount(); ++i)
        //-QueueCommand(new GdbCmd_AddSourceDir(this, m_Dirs[i]));
         m_actions.Add(new dbg_mi::SimpleAction(m_Dirs[i], m_execution_logger));

    // set arguments
    //-if (not m_Args.IsEmpty())
    //-    QueueCommand(new DebuggerCmd(this, _T("set args ") + m_Args));

    //-RemoteDebugging* rd = GetRemoteDebuggingInfo();
    RemoteDebugging* rd = GetRemoteDebuggingInfo(GetTarget());

    // send additional gdb commands before establishing remote connection
    if (rd)
    {
        if (not rd->additionalCmdsBefore.IsEmpty())
        {
            wxArrayString initCmds = GetArrayFromString(rd->additionalCmdsBefore, _T('\n'));
            for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
            {
                macrosManager->ReplaceMacros(initCmds[i]);
                //-QueueCommand(new DebuggerCmd(this, initCmds[i]));
                m_actions.Add(new dbg_mi::SimpleAction(initCmds[i], m_execution_logger));
            }
        }
        if (not rd->additionalShellCmdsBefore.IsEmpty())
        {
            wxArrayString initCmds = GetArrayFromString(rd->additionalShellCmdsBefore, _T('\n'));
            for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
            {
                macrosManager->ReplaceMacros(initCmds[i]);
                //-QueueCommand(new DebuggerCmd(this, _T("shell ") + initCmds[i]));
                m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("shell "))+initCmds[i], m_execution_logger));
            }
        }
    }

    // if performing remote debugging, now is a good time to try and connect to the target :)
    if (rd && rd->IsOk())
    {
        if (rd->connType == RemoteDebugging::Serial)
            //-QueueCommand(new GdbCmd_RemoteBaud(this, rd->serialBaud));
            m_actions.Add(new dbg_mi::SimpleAction(rd->serialBaud, m_execution_logger));
        //-QueueCommand(new GdbCmd_RemoteTarget(this, rd));
        wxString targetRemoteCmd = GetTargetRemoteCmd(rd);
        if (not targetRemoteCmd.empty())
            m_actions.Add(new dbg_mi::SimpleAction(targetRemoteCmd, m_execution_logger));
    }

    // run per-target additional commands (remote debugging)
    // moved after connection to remote target (if any)
    if (rd)
    {
        if (not rd->additionalCmds.IsEmpty())
        {
            wxArrayString initCmds = GetArrayFromString(rd->additionalCmds, _T('\n'));
            for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
            {
                macrosManager->ReplaceMacros(initCmds[i]);
                //-QueueCommand(new DebuggerCmd(this, initCmds[i]));
                m_actions.Add(new dbg_mi::SimpleAction(initCmds[i], m_execution_logger));
            }
        }
        if (not rd->additionalShellCmdsAfter.IsEmpty())
        {
            wxArrayString initCmds = GetArrayFromString(rd->additionalShellCmdsAfter, _T('\n'));
            for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
            {
                macrosManager->ReplaceMacros(initCmds[i]);
                //-QueueCommand(new DebuggerCmd(this, _T("shell ") + initCmds[i]));
                m_actions.Add(new dbg_mi::SimpleAction(wxString(_T("shell "))+initCmds[i], m_execution_logger));
            }
        }
    }

}//PrepareRemoteDebugging
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnProjectLoadingHook(cbProject* project, TiXmlElement* elem, bool loading)
// ----------------------------------------------------------------------------
{
    wxArrayString& pdirs = GetSearchDirs(project);
    RemoteDebuggingMap& rdprj = GetRemoteDebuggingMap(project);

    // wxString dbgsName = GetDebuggersName(); debugging

    if (loading)
    {
        rdprj.clear();

        // Hook called when loading project file.
        TiXmlElement* conf = elem->FirstChildElement("debugger_gdbmi");
        if (conf)
        {
            TiXmlElement* pathsElem = conf->FirstChildElement("search_path");
            while (pathsElem)
            {
                if (pathsElem->Attribute("add"))
                {
                    wxString dir = cbC2U(pathsElem->Attribute("add"));
                    if (pdirs.Index(dir) == wxNOT_FOUND)
                        pdirs.Add(dir);
                }

                pathsElem = pathsElem->NextSiblingElement("search_path");
            }

            TiXmlElement* rdElem = conf->FirstChildElement("remote_debugging");
            while (rdElem)
            {
                wxString targetName = cbC2U(rdElem->Attribute("target"));
                ProjectBuildTarget* bt = project->GetBuildTarget(targetName);

                TiXmlElement* rdOpt = rdElem->FirstChildElement("options");

                if (rdOpt)
                {
                    RemoteDebugging rd;

                    if (rdOpt->Attribute("conn_type"))
                        rd.connType = (RemoteDebugging::ConnectionType)atol(rdOpt->Attribute("conn_type"));
                    if (rdOpt->Attribute("serial_port"))
                        rd.serialPort = cbC2U(rdOpt->Attribute("serial_port"));
                    if (rdOpt->Attribute("serial_baud"))
                        rd.serialBaud = cbC2U(rdOpt->Attribute("serial_baud"));
                    if (rdOpt->Attribute("ip_address"))
                        rd.ip = cbC2U(rdOpt->Attribute("ip_address"));
                    if (rdOpt->Attribute("ip_port"))
                        rd.ipPort = cbC2U(rdOpt->Attribute("ip_port"));
                    if (rdOpt->Attribute("additional_cmds"))
                        rd.additionalCmds = cbC2U(rdOpt->Attribute("additional_cmds"));
                    if (rdOpt->Attribute("additional_cmds_before"))
                        rd.additionalCmdsBefore = cbC2U(rdOpt->Attribute("additional_cmds_before"));
                    if (rdOpt->Attribute("skip_ld_path"))
                        rd.skipLDpath = cbC2U(rdOpt->Attribute("skip_ld_path")) != _T("0");
                    if (rdOpt->Attribute("extended_remote"))
                        rd.extendedRemote = cbC2U(rdOpt->Attribute("extended_remote")) != _T("0");
                    if (rdOpt->Attribute("additional_shell_cmds_after"))
                        rd.additionalShellCmdsAfter = cbC2U(rdOpt->Attribute("additional_shell_cmds_after"));
                    if (rdOpt->Attribute("additional_shell_cmds_before"))
                        rd.additionalShellCmdsBefore = cbC2U(rdOpt->Attribute("additional_shell_cmds_before"));

                    rdprj.insert(rdprj.end(), std::make_pair(bt, rd));
                }

                rdElem = rdElem->NextSiblingElement("remote_debugging");
            }

        }//endif conf
        // Load breakpoints and watches
         LoadProjectBreakpoints(project, elem, loading);
    }
    // ----------------------------------------------------------------------------
    else ///Saving project file
    // ----------------------------------------------------------------------------
    {
        // Hook called when saving project file.

        // since rev4332, the project keeps a copy of the <Extensions> element
        // and re-uses it when saving the project (so to avoid losing entries in it
        // if plugins that use that element are not loaded atm).
        // so, instead of blindly inserting the element, we must first check it's
        // not already there (and if it is, clear its contents)
        TiXmlElement* node = elem->FirstChildElement("debugger_gdbmi");
        if (not node)
            node = elem->InsertEndChild(TiXmlElement("debugger_gdbmi"))->ToElement();
        node->Clear();

        if (pdirs.GetCount() > 0)
        {
            for (size_t i = 0; i < pdirs.GetCount(); ++i)
            {
                TiXmlElement* path = node->InsertEndChild(TiXmlElement("search_path"))->ToElement();
                path->SetAttribute("add", cbU2C(pdirs[i]));
            }
        }

        if (rdprj.size())
        {
            for (RemoteDebuggingMap::iterator it = rdprj.begin(); it != rdprj.end(); ++it)
            {
//                // valid targets only
//                if (not it->first)
//                    continue;

                RemoteDebugging& rd = it->second;

                // if no different than defaults, skip it
                if (rd.serialPort.IsEmpty() && rd.ip.IsEmpty() &&
                    rd.additionalCmds.IsEmpty() && rd.additionalCmdsBefore.IsEmpty() &&
                    !rd.skipLDpath && !rd.extendedRemote)
                {
                    continue;
                }

                TiXmlElement* rdnode = node->InsertEndChild(TiXmlElement("remote_debugging"))->ToElement();
                if (it->first)
                    rdnode->SetAttribute("target", cbU2C(it->first->GetTitle()));

                TiXmlElement* tgtnode = rdnode->InsertEndChild(TiXmlElement("options"))->ToElement();
                tgtnode->SetAttribute("conn_type", (int)rd.connType);
                if (not rd.serialPort.IsEmpty())
                    tgtnode->SetAttribute("serial_port", cbU2C(rd.serialPort));
                if (not rd.serialBaud.IsEmpty())
                    tgtnode->SetAttribute("serial_baud", cbU2C(rd.serialBaud));
                if (not rd.ip.IsEmpty())
                    tgtnode->SetAttribute("ip_address", cbU2C(rd.ip));
                if (not rd.ipPort.IsEmpty())
                    tgtnode->SetAttribute("ip_port", cbU2C(rd.ipPort));
                if (not rd.additionalCmds.IsEmpty())
                    tgtnode->SetAttribute("additional_cmds", cbU2C(rd.additionalCmds));
                if (not rd.additionalCmdsBefore.IsEmpty())
                    tgtnode->SetAttribute("additional_cmds_before", cbU2C(rd.additionalCmdsBefore));
                if (rd.skipLDpath)
                    tgtnode->SetAttribute("skip_ld_path", "1");
                if (rd.extendedRemote)
                    tgtnode->SetAttribute("extended_remote", "1");
                if (not rd.additionalShellCmdsAfter.IsEmpty())
                    tgtnode->SetAttribute("additional_shell_cmds_after", cbU2C(rd.additionalShellCmdsAfter));
                if (not rd.additionalShellCmdsBefore.IsEmpty())
                    tgtnode->SetAttribute("additional_shell_cmds_before", cbU2C(rd.additionalShellCmdsBefore));
            }//end for remote debugger map
        }//endif rdprj

        // --------------------------------------------------------------------
        // Save the project breakpoints
        // --------------------------------------------------------------------
        SaveProjectBreakpoints(project);

    }//endElse saving project file
    return;
}
#include "json_node.h"
#include <wx/file.h>
#include <wx/filename.h>
#include <vector>
#include <string>
#include <wx/ffile.h>
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::LoadProjectBreakpoints(cbProject* pProject, TiXmlElement* elem, bool loading)
// ----------------------------------------------------------------------------
{
    wxUnusedVar(loading);
    wxUnusedVar(elem);
    // Check if the breakpoint file exists
    wxString wrkingDir = pProject->GetBasePath();
    wxFileName filename = wrkingDir + fileSep + breakpointFilename;

    if (! filename.Exists())
    {
        // File not found, return
        return;
    }

    // Create a JSONRoot object from the JSON data in the file
    JSONRoot root(filename);

    // Get the "debugger_mi" object
    JSONElement debugger_mi_object = root.toElement().namedObject("debugger_mi");

    // Check if debugger_mi_object is an object
    if (debugger_mi_object.isNull())
    {
        // 'debugger_mi' object not found, return
        DebugLog("Fail: Breakpoints file incorrectly formated.", Logger::error);
        return;
    }

    JSONElement breakpoints_array = debugger_mi_object.namedObject("breakpoints");
    // Check if breakpoints_array is an array
    if (not breakpoints_array.isArray())
    {
        // 'breakpoints' array not found, return
        DebugLog("Fail: Breakpoints file incorrectly formated.", Logger::error);
        return;
    }

    // Vector to store the breakpoints
    std::vector<std::string> brkptStrArray;

    // Iterate through the array and extract strings
    for (int i = 0; i < breakpoints_array.arraySize(); ++i)
    {
        JSONElement element = breakpoints_array.arrayItem(i);
        std::string bkpt = element.toString().ToStdString();
        brkptStrArray.push_back(bkpt);
    }

    // **Debugging**
    //    for (const auto& breakpoint : breakpoints)
    //    {
    //        DebugLog(wxString::Format("%s\n", breakpoint.c_str()));
    //    }

    // ----------------------------------------------------------------------------
    // Clear all previous breakpoints
    // ----------------------------------------------------------------------------
    cbBreakpointsDlg* dlg = Manager::Get()->GetDebuggerManager()->GetBreakpointDialog();
    DeleteAllBreakpoints();
    dlg->RemoveAllBreakpoints();

    for (int onetime = 1; onetime; --onetime)
    {
        // Get access to the project file
        if (not pProject)
            break;

        unsigned bpKnt = brkptStrArray.size();
        if ( 0 == bpKnt)
            break;

        for (unsigned ii = 0; ii < bpKnt; ++ii)
        {
            wxString bkptStr = brkptStrArray[ii];
            // wxString entryName = wxString::Format(_T("bkpt%d"), ii+1);   // **Debugging**
            wxString entryValue = bkptStr;
            if (bkptStr.Length())
            {
                if (entryValue.Lower().StartsWith(_T("code")))
                {
                    wxStringTokenizer tkz(entryValue, _T(";"));

                    // wxString bpType = tkz.GetNextToken();    // **Debugging**
                    bool bpEnabled = (tkz.GetNextToken() == _T("enabled"));

                    wxString filename = tkz.GetNextToken();
                    wxString lineStr = tkz.GetNextToken();
                    // find full path of filename
                    wxFileName fname(realpath(filename));
                    fname.MakeAbsolute(pProject->GetBasePath());
                    filename = fname.GetFullPath();
                    // If file does not belong to project, ignore it
                    if ( 0 == pProject->GetFileByFilename(filename, /*relative=*/false))
                    {
                        DebugLog(wxString::Format(_T("%s [%s],%s[%s]"), _("Skipped non-project brkpt for"), filename.c_str(), _("line"), lineStr.c_str()));
                        continue;
                    }
                    long lineNum = 0;
                    lineStr.ToLong(&lineNum);
                    wxString ignoreCountStr = tkz.GetNextToken();
                    long ignoreCount = 0;
                    ignoreCountStr.ToLong(&ignoreCount);
                    bool hasIgnoreCount = tkz.GetNextToken() == _T("true");
                    wxString condition = tkz.GetNextToken();
                    bool hasCondition = tkz.GetNextToken() == _T("true");

                    dbg_mi::Breakpoint::Pointer bp(new dbg_mi::Breakpoint(filename, lineNum, pProject));
                    bp->SetType(_T("Code"));
                    bp->SetEnabled(bpEnabled);
                    bp->SetIgnoreCount( ignoreCount);
                    bp->SetUseIgnoreCount( hasIgnoreCount);
                    bp->SetCondition(condition);
                    bp->SetUseCondition(hasCondition);
                    AddBreakpoint(bp);
                }

                if (entryValue.StartsWith(_T("Data")))
                {
                    entryValue = entryValue.AfterFirst(_T(';'));
                    wxString symbol = entryValue.BeforeFirst(_T(';'));
                    wxString condition = entryValue.AfterFirst(_T(';'));
                    condition = condition.BeforeFirst(_T(';'));
                    wxString enabled = entryValue.AfterLast(_T(';'));

                    dbg_mi::Breakpoint::Pointer ptr(new dbg_mi::Breakpoint);
                    ptr->SetType(_T("Data"));
                    ptr->SetDataAddress( symbol);
                    if (condition.Contains(_T("write")))
                        ptr->SetBreakOnDataWrite(true);
                    if (condition.Contains(_T("read")))
                        ptr->SetBreakOnDataRead(true);
                    ptr->SetEnabled(enabled == _T("enabled"));
                    ptr->SetProject(pProject);
                    AddBreakpoint(ptr);
                }
            }
            else break;
        }//for each breakpoint

    }//for onetime

    //-if ( bkptAdded) always refresh bkpt window to clear any previous projects data
    {
        dlg->Reload();
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (ed)
            ed->RefreshBreakpointMarkers();
    }

}//end LoadProjectBreakpoints
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnSaveProjectBreakpoints(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    wxUnusedVar(event);
    // Save breakpoint to a json file named "dbgrActnData.json"
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pProject) return;

    SaveProjectBreakpoints(pProject);

}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::SaveProjectBreakpoints(cbProject* project)
// ----------------------------------------------------------------------------
{
    // Get array of breakpoints into string format
    wxArrayString brkptStrArray;
    GetBreakpointsIntoStrings(brkptStrArray, project);

    // Form full path of breakpoint filename
    wxString brkptFilename = project->GetBasePath() +fileSep+ breakpointFilename;

    if (brkptStrArray.empty())
    {
        if (wxFileExists(brkptFilename))
            wxRemove(brkptFilename);
        return;
    }

    // Create a JSONRoot object with cJSON_Object type
    JSONRoot root(cJSON_Object);

    // Create a JSONElement object to hold the array of strings
    JSONElement debugger_mi_object = JSONElement::createObject("debugger_mi");

    // Create a JSON array to hold breakpoints
    JSONElement breakpoints_array = JSONElement::createArray("breakpoints");

    // Append each breakpoint string to the array
    for (const auto& str : brkptStrArray)
    {
        JSONElement bkpt_element("", wxString(str), cJSON_String);
        breakpoints_array.arrayAppend(bkpt_element);
    }

    // Add the array of breakpoints as a property to the object
    debugger_mi_object.addProperty("breakpoints", breakpoints_array);

    // Add the object to the root JSON object
    root.toElement().addProperty("debugger_mi", debugger_mi_object);

    // Save the JSON data to a file named "breakpoints.json"
    wxFileName filename(breakpointFilename);
    root.save(filename);

    // Check if the file was saved successfully
    if (!filename.FileExists())
    {
        // Warn about failed breakpoint storage
        DebugLog("GDB/MI Failed to save breakpoints.", Logger::error);
        return;
    }
    else
    {
        // Log the number of breakpoints saved
        DebugLog(wxString::Format("GDB/MI Saved %d breakpoints.", int(brkptStrArray.size())), Logger::info);
    }
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::GetBreakpointsIntoStrings(wxArrayString& brkptStrArray, cbProject* pProject)
// ----------------------------------------------------------------------------
{
    wxString itemSep = _T(";");
    for (Breakpoints::iterator it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it)
    {
        dbg_mi::Breakpoint::Pointer bp = (*it);
        if (bp->GetProject() != pProject)
            continue;

        if (bp->GetType() == _T("Code"))
        {
            //format: type;enablement;filename;line;ignoreCount;hasIgnoreCouunt;condition;hasCondition
            wxString bpEnabled = (bp->IsEnabled()?_T("enabled"):_T("disabled"));
            wxString filename = bp->GetLocation();
            // if the search is not relative, make it
            wxFileName fname(realpath(filename));
            fname.MakeRelativeTo(pProject->GetBasePath());
            filename = fname.GetFullPath();
            wxString line = wxString::Format(_T("%d"),bp->GetLine());
            wxString bkptInfo = wxString::Format(_T("Code;%s;%s;%s"), bpEnabled.c_str(),filename.c_str(), line.c_str());
            bkptInfo << itemSep << bp->GetIgnoreCount() << itemSep << (bp->HasIgnoreCount()?_T("true"):_T("false"));
            wxString condition = bp->GetCondition();
            //dont allow our special separator in a breakpoint `edcondition
            if (condition.Length() and wxFound(condition.Find(itemSep)))
            {
                    bp->SetCondition(wxString());
                    bp->SetUseCondition(false);
            }
            bkptInfo << itemSep << bp->GetCondition() << itemSep << (bp->HasCondition()?_T("true"):_T("false"));
            brkptStrArray.Add( bkptInfo );
        }

        if (bp->GetType() == _T("Data"))
        {
            //format type;symbol;condition;enablement
            wxString symbol = bp->GetDataAddress();
            wxString condition = bp->GetInfo();
            condition = condition.AfterFirst(_T(' '));
            wxString enabled = bp->IsEnabled()?_T("enabled"):_T("disabled");
            wxString bkptInfo = wxString::Format("Data;%s;%s;%s", symbol.c_str(), condition.c_str(), enabled.c_str());
            brkptStrArray.Add(bkptInfo);
        }
    }

}
// ----------------------------------------------------------------------------
cbConfigurationPanel* Debugger_GDB_MI::GetProjectConfigurationPanel(wxWindow* parent, cbProject* project)
// ----------------------------------------------------------------------------
{
    // Don't add panel if not active debugger
    if (this != Manager::Get()->GetDebuggerManager()->GetActiveDebugger())
        return 0;

    DebuggerOptionsProjectDlg* dlg = new DebuggerOptionsProjectDlg(parent, this, project);
    return dlg;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnProjectClosed(CodeBlocksEvent& event) //(ph 2024/03/15)
// ----------------------------------------------------------------------------
{
    event.Skip(); // always, else double entries and screwed fellow plugins.

    cbProject* pProject = event.GetProject();
    if (not pProject) return;

    // Save the breakpoints if changed
    SaveProjectBreakpoints(pProject);
    DeleteAllWatches();     // (ph 26/06/18)

    // Safely deletes the GDBWatch object and sets m_localsWatch to nullptr
    // so we don't show a watch window with stale watches.
    if ( m_localsWatch)
        m_localsWatch.reset();
    if (m_funcArgsWatch)
        m_funcArgsWatch.reset();;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::OnProjectSave(CodeBlocksEvent& event) //(ph 2024/03/15)
// ----------------------------------------------------------------------------
{
    event.Skip(); // always else double entries and screwed fellow plugins.

    cbProject* pProject = event.GetProject();
    if (not pProject) return;
}
// ----------------------------------------------------------------------------
wxString Debugger_GDB_MI::GetWXVariableValue(wxString variableName) //(ph 2024/03/15)
// ----------------------------------------------------------------------------
{
    // UserVariableManager* pVarMgr = Manager::Get()->GetUserVariableManager();
    // /** \brief Get value of member variable. First search in program arguments, then in configuration file
    //  *
    //  * \param setName set name
    //  * \param varName variable name
    //  * \param memberName member name
    //  * \return member variable value
    // wxString GetMemberValue(const wxString& setName, const wxString& varName, const wxString& memberName) const;
    //  */
    /// The above public call was never implemented //(ph 2025/01/18)

    wxString varName = variableName;
    // avoid the undefined variable dialog popup
    // varName must exists before calling ReplaceMacros, else a dlg will popup
    UserVariableManager * pVarMgr = Manager::Get()->GetUserVariableManager();
    if (not pVarMgr->Exists(varName)) return wxString();
    MacrosManager* pMacMgr = Manager::Get()->GetMacrosManager();
    pMacMgr->ReplaceMacros(varName);
    return varName;
}
// ----------------------------------------------------------------------------
wxString Debugger_GDB_MI::CreateGDBinitFile(cbProject* pProject, ProjectBuildTarget* pTarget) // (ph 26/06/18)
// ----------------------------------------------------------------------------
{
    // main call to create the project gdbinit file containing
    // wxWidgets and compilers pretty printer gdb commands
    // Find any previous gdbinit file in the project folder
    // Return any existing gdbinit file, else create it.

    wxString gdbinitFile = wxPathOnly(pProject->GetFilename()) + "/gdbinit";

    // Find any previous gdbinit file in the project folder.
    // Return any existing gdbinit file, else create it.
    if (wxFileExists(gdbinitFile))
    {
        wxFileName fn(gdbinitFile);
        wxULongLong fileSize = fn.GetSize();
        if (fileSize > 2) // use file if info is > bom size
            return gdbinitFile;
        else // remove and rebuild the file;
            wxRemoveFile(gdbinitFile);
    }

    // Try finding wWidgets pretty printer commands
    wxString gdbinitFileLocation;   // (ph 26/06/17)

    // Get value of the  "$(#wx)" wxWidgets active global variable
    wxString wxValue = GetWXVariableValue("$(#wx)"); //(ph 2024/03/15

    // Try finding the wxWidgets pretty printer script files
    if ( pProject and wxDirExists(wxValue))
        gdbinitFileLocation = CreateGdbinitFile(m_project, wxValue);

    // Try finding the compilers pretty printer script files
    if ( pProject and pTarget)
        gdbinitFileLocation = CreateGdbinitFile(m_project, pTarget);

    return gdbinitFileLocation;
}

// ----------------------------------------------------------------------------
wxString Debugger_GDB_MI::CreateGdbinitFile(cbProject* pProject, wxString wxValue)
// ----------------------------------------------------------------------------
{
    // Create a gdbinit file in the project folder or return an existing one
    /*
    python
    import sys
    sys.path.insert(0, r'F:\usr\proj\wxWidgets3260\misc\gdb')  # Path to print.py
    import print  # Import the pretty-printer module

    # The pretty-printers should already be registered by appending wxLookupFunction
    # in print.py, so no need to call any additional registration function.
    end
    */

    // Find and format the wxWidgets pretty print gdbinit commands

    wxString gdbinitFile = wxPathOnly(pProject->GetFilename()) + "/gdbinit"; //<projectDir> + gdbinit
    wxString wxPrintDotpyDir = wxValue + "\\misc\\gdb";    // location of wxWidgets print.py
    wxString wxSafePath = wxValue;
    wxSafePath.Replace("\\","/");

    // check this wxWidgets does contain the print.py file
    wxPrintDotpyDir.Replace("\\", "/");
    if (not wxFileExists(wxPrintDotpyDir + "/print.py"))
        return wxString();

    wxTextFile gdbinit(gdbinitFile);
    gdbinit.Create();
    gdbinit.AddLine("# setup wxWidgets pretty printers");
    gdbinit.AddLine("set print pretty on");
    gdbinit.AddLine("set auto-load safe-path " + wxSafePath);

    gdbinit.AddLine("python ");
    gdbinit.AddLine("import sys");
    wxString sysPathTemplate = "sys.path.insert(0, r'%s')  # Path to print.py";
    gdbinit.AddLine(wxString::Format(sysPathTemplate, wxPrintDotpyDir));
    gdbinit.AddLine("import print");
    gdbinit.AddLine("# The pretty-printers should already be registered by the appending of wxLookupFunction");
    gdbinit.AddLine("# in print.py, so we don't need to call any additional registration function.");
    gdbinit.AddLine("end");
    gdbinit.Write();
    gdbinit.Close();
    if (wxFileExists(gdbinitFile))
        return gdbinitFile;     //success
    else
        return wxString();       //failure
}
// ----------------------------------------------------------------------------
wxString Debugger_GDB_MI::CreateGdbinitFile(cbProject* pProject, ProjectBuildTarget* pTarget)
// ----------------------------------------------------------------------------
{
    // Create a gdbinit file in the project folder or return an existing one
    // or append the compile pretty printer loading commands
    /*
    set print pretty on;
    set auto-load safe-path F:/usr/Proj/;
    python import sys;
    python sys.path.insert(0, 'F:/usr/Programs/msys64_20.1.3/mingw64/share/gcc-15.2.0/python');
    python from libstdcxx.v6.printers import register_libstdcxx_printers;
    python register_libstdcxx_printers(None);
    */
    wxString gdbinitFile = wxPathOnly(pProject->GetFilename()) + "/gdbinit";
    wxString gdbCmdStr;

    dbg_mi::AddCompilerPrettyPrintCommands(gdbCmdStr, pTarget);
    if (gdbCmdStr.empty())
        return gdbinitFile;

    // Find any previous gdbinit file in the project folder
     wxTextFile gdbinit(gdbinitFile);
    if (not wxFileExists(gdbinitFile))
        gdbinit.Create();
    else
        gdbinit.Open();
    wxArrayString cmdLines;
    cmdLines = GetArrayFromString(gdbCmdStr, ";", /*trimSpaces*/true);

    Compiler* compiler = GetCompiler();
    wxString masterPath = compiler->GetMasterPath();
    masterPath.Replace("\\","/");

    gdbinit.AddLine("# setup compiler pretty print support");
    gdbinit.AddLine("set print pretty on");
    gdbinit.AddLine("set auto-load safe-path " + masterPath);
    gdbinit.AddLine("python");
    gdbinit.AddLine("import sys");

    for (size_t ii=0; ii<cmdLines.Count(); ++ii)
        gdbinit.AddLine(cmdLines[ii]);

    gdbinit.Write();
    gdbinit.Close();
    return gdbinitFile;

}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::RemoveGdbinitFile(cbProject* pProject)
// ----------------------------------------------------------------------------
{
    if (not pProject) return;
    wxFileName fnFilePath(pProject->GetFilename());
    fnFilePath.SetFullName("gdbinit");
    //wxString fname = fnFilePath.GetFullPath(); // **Debugging**
    if ( fnFilePath.FileExists() )
    {
        wxRemoveFile(fnFilePath.GetFullPath());
    }
}

// ----------------------------------------------------------------------------
void Debugger_GDB_MI::NotifyCursorChanged()
// ----------------------------------------------------------------------------
{
    if (!m_Cursor.changed || m_LastCursorAddress == m_Cursor.address)
        return;
    m_LastCursorAddress = m_Cursor.address;
    wxCommandEvent event(dbg_mi::DEBUGGER_CURSOR_CHANGED);
    this->ProcessEvent(event);
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::ResetCursor()
// ----------------------------------------------------------------------------
{
    m_LastCursorAddress.Clear();
    m_Cursor.address.Clear();
    m_Cursor.file.Clear();
    m_Cursor.function.Clear();
    m_Cursor.line = -1;
    m_Cursor.changed = false;
}
// ----------------------------------------------------------------------------
void Debugger_GDB_MI::DetermineLanguage()
// ----------------------------------------------------------------------------
{
    //-QueueCommand(new GdbCmd_DebugLanguage(this));
    m_actions.Add(new dbg_mi::GdbCmd_DebugLanguage());
}
