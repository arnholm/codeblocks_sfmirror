/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>
#include "gdb_driver.h"
#include "gdb_commands.h"
#include "debuggeroptionsdlg.h"
#include "debuggerstate.h"

#include <cbdebugger_interfaces.h>
#include <configmanager.h>
#include <cygwin.h>
#include <globals.h>
#include <infowindow.h>
#include <manager.h>
#include <macrosmanager.h>

// the ">>>>>>" is a hack: sometimes, especially when watching uninitialized char*
// some random control codes in the stream (like 'delete') will mess-up our prompt and the debugger
// will seem like frozen (only "stop" button available). Using this dummy prefix,
// we allow for a few characters to be "eaten" this way and still get our
// expected prompt back.
#define GDB_PROMPT _T("cb_gdb:")
#define FULL_GDB_PROMPT _T(">>>>>>") GDB_PROMPT

//[Switching to thread 2 (Thread 1082132832 (LWP 12298))]#0  0x00002aaaac5a2aca in pthread_cond_wait@@GLIBC_2.3.2 () from /lib/libpthread.so.0
static wxRegEx reThreadSwitch(_T("^\\[Switching to thread .*\\]#0[[:blank:]]+(0x[A-Fa-f0-9]+) in (.*) from (.*)"));
static wxRegEx reThreadSwitch2(_T("^\\[Switching to thread .*\\]#0[[:blank:]]+(0x[A-Fa-f0-9]+) in (.*) from (.*):([0-9]+)"));

// Regular expresion for breakpoint. wxRegEx don't want to recognize '?' command, so a bit more general rule is used
// here.
//  ([A-Za-z]*[:]*) corresponds to windows disk name. Under linux it can be none empty in crosscompiling sessions;
//  ([^:]+) corresponds to the path in linux or to the path within windows disk in windows to current file;
//  ([0-9]+) corresponds to line number in current file;
//  (0x[0-9A-Fa-f]+) correponds to current memory address.
static wxRegEx reBreak(_T("\032*([A-Za-z]*[:]*)([^:]+):([0-9]+):[0-9]+:[begmidl]+:(0x[0-9A-Fa-f]+)"));

static wxRegEx reBreak2(_T("^(0x[A-Fa-f0-9]+) in (.*) from (.*)"));
static wxRegEx reBreak3(_T("^(0x[A-Fa-f0-9]+) in (.*)"));
// Catchpoint 1 (exception thrown), 0x00007ffff7b982b0 in __cxa_throw () from /usr/lib/gcc/x86_64-pc-linux-gnu/4.4.4/libstdc++.so.6
static wxRegEx reCatchThrow(_T("^Catchpoint ([0-9]+) \\(exception thrown\\), (0x[0-9a-f]+) in (.+) from (.+)$"));
// Catchpoint 1 (exception thrown), 0x00401610 in __cxa_throw ()
static wxRegEx reCatchThrowNoFile(_T("^Catchpoint ([0-9]+) \\(exception thrown\\), (0x[0-9a-f]+) in (.+)$"));

// easily match cygwin paths
//static wxRegEx reCygwin(_T("/cygdrive/([A-Za-z])/"));

// Pending breakpoint "C:/Devel/libs/irr_svn/source/Irrlicht/CSceneManager.cpp:1077" resolved
#ifdef __WXMSW__
static wxRegEx rePendingFound(_T("^Pending[[:blank:]]+breakpoint[[:blank:]]+[\"]+([A-Za-z]:)([^:]+):([0-9]+)\".*"));
#else
static wxRegEx rePendingFound(_T("^Pending[[:blank:]]+breakpoint[[:blank:]]+[\"]+([^:]+):([0-9]+)\".*"));
#endif
// Breakpoint 2, irr::scene::CSceneManager::getSceneNodeFromName (this=0x3fa878, name=0x3fbed8 "MainLevel", start=0x3fa87c) at CSceneManager.cpp:1077
static wxRegEx rePendingFound1(_T("^Breakpoint[[:blank:]]+([0-9]+),.*"));

// Temporary breakpoint 2, main () at /path/projects/tests/main.cpp:136
static wxRegEx reTempBreakFound(wxT("^[Tt]emporary[[:blank:]]breakpoint[[:blank:]]([0-9]+),.*"));


// [Switching to Thread -1234655568 (LWP 18590)]
// [New Thread -1234655568 (LWP 18590)]
static wxRegEx reChildPid1(_T("Thread[[:blank:]]+[xA-Fa-f0-9-]+[[:blank:]]+\\(LWP ([0-9]+)\\)]"));
// MinGW GDB 6.8 and later
// [New Thread 2684.0xf40] or [New thread 2684.0xf40]
static wxRegEx reChildPid2(_T("\\[New [tT]hread[[:blank:]]+[0-9]+\\.[xA-Fa-f0-9-]+\\]"));

static wxRegEx reInferiorExited(wxT("^\\[Inferior[[:blank:]].+[[:blank:]]exited normally\\]$"), wxRE_EXTENDED);
static wxRegEx reInferiorExitedWithCode(wxT("^\\[[Ii]nferior[[:blank:]].+[[:blank:]]exited[[:blank:]]with[[:blank:]]code[[:blank:]]([0-9]+)\\]$"), wxRE_EXTENDED);

GDB_driver::GDB_driver(DebuggerGDB* plugin) :
    DebuggerDriver(plugin),
    m_BreakOnEntry(false),
    m_ManualBreakOnEntry(false),
    m_IsStarted(false),
    m_GDBVersionMajor(0),
    m_GDBVersionMinor(0),
    m_attachedToProcess(false),
    m_catchThrowIndex(-1)
{
    //ctor
    m_needsUpdate = false;
    m_forceUpdate = false;

    if (platform::windows)
        m_CygwinPresent = cbIsDetectedCygwinCompiler();
}

GDB_driver::~GDB_driver()
{
    //dtor
}

wxString GDB_driver::GetCommandLine(const wxString& debugger, const wxString& debuggee, const wxString &userArguments)
{
    wxString cmd;
    cmd << debugger;
    if (m_pDBG->GetActiveConfigEx().GetFlag(DebuggerConfiguration::DisableInit))
        cmd << _T(" -nx");      // don't run .gdbinit
    cmd << _T(" -fullname");    // report full-path filenames when breaking
    cmd << _T(" -quiet");       // don't display version on startup
    cmd << wxT(" ") << userArguments;

    wxString actualDebuggee;
    if (platform::windows && m_CygwinPresent)
    {
        actualDebuggee = debuggee;
        cbGetCygwinPathFromWindowsPath(actualDebuggee);
    }
    else
        actualDebuggee = debuggee;

    cmd << _T(" -args ") << actualDebuggee;
    return cmd;
}

wxString GDB_driver::GetCommandLine(const wxString& debugger, cb_unused int pid, const wxString &userArguments)
{
    wxString cmd;
    cmd << debugger;
    if (m_pDBG->GetActiveConfigEx().GetFlag(DebuggerConfiguration::DisableInit))
        cmd << _T(" -nx");      // don't run .gdbinit
    cmd << _T(" -fullname");    // report full-path filenames when breaking
    cmd << _T(" -quiet");       // don't display version on startup
    cmd << wxT(" ") << userArguments;
    return cmd;
}

void GDB_driver::SetTarget(ProjectBuildTarget* target)
{
    // init for remote debugging
    m_pTarget = target;
}

void GDB_driver::Prepare(bool isConsole, int printElements, const RemoteDebugging &remoteDebugging)
{
    // default initialization

    // make sure we 're using the prompt that we know and trust ;)
    QueueCommand(new DebuggerCmd(this, wxString("set prompt ") + FULL_GDB_PROMPT));

    // debugger version
    QueueCommand(new DebuggerCmd(this, "show version"));
    // no confirmation
    QueueCommand(new DebuggerCmd(this, "set confirm off"));
    // no wrapping lines
    QueueCommand(new DebuggerCmd(this, "set width 0"));
    // no pagination
    QueueCommand(new DebuggerCmd(this, "set height 0"));
    // allow pending breakpoints
    QueueCommand(new DebuggerCmd(this, "set breakpoint pending on"));
    // show pretty function names in disassembly
    QueueCommand(new DebuggerCmd(this, "set print asm-demangle on"));
    // unwind stack on signal
    QueueCommand(new DebuggerCmd(this, "set unwindonsignal on"));
    // disable result string truncations
    QueueCommand(new DebuggerCmd(this, wxString::Format("set print elements %d", printElements)));
    // Make sure backtraces use absolute paths, so it is more reliable to find where the sources
    // file when trying to open it in the editor.
    QueueCommand(new DebuggerCmd(this, "set filename-display absolute"));
    // Disable ANSI escape sequences, new in GDB 8.2
    QueueCommand(new DebuggerCmd(this, "set style enabled off"));

    if (platform::windows && isConsole)
        QueueCommand(new DebuggerCmd(this, "set new-console on"));

    flavour = m_pDBG->GetActiveConfigEx().GetDisassemblyFlavorCommand();
    QueueCommand(new DebuggerCmd(this, flavour));

    if (m_pDBG->GetActiveConfigEx().GetFlag(DebuggerConfiguration::CatchExceptions))
    {
        m_catchThrowIndex = -1;
        // catch exceptions
        QueueCommand(new GdbCmd_SetCatch(this, "throw", &m_catchThrowIndex));
    }

    // pass user init-commands
    wxString init = m_pDBG->GetActiveConfigEx().GetInitCommands();
    MacrosManager *macrosManager = Manager::Get()->GetMacrosManager();
    macrosManager->ReplaceMacros(init);
    // commands are passed in one go, in case the user defines functions in there
    // or else it would lock up...
    if (!init.empty())
        QueueCommand(new DebuggerCmd(this, init));

    // add search dirs
    for (unsigned int i = 0; i < m_Dirs.GetCount(); ++i)
        QueueCommand(new GdbCmd_AddSourceDir(this, m_Dirs[i]));

    // set arguments
    if (!m_Args.IsEmpty())
        QueueCommand(new DebuggerCmd(this, "set args " + m_Args));

    // Send additional gdb commands before establishing remote connection.
    // These are executed no matter if doing remote debugging or not.
    if (!remoteDebugging.additionalCmdsBefore.IsEmpty())
    {
        wxArrayString initCmds = GetArrayFromString(remoteDebugging.additionalCmdsBefore, _T('\n'));
        for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
        {
            macrosManager->ReplaceMacros(initCmds[i]);
            QueueCommand(new DebuggerCmd(this, initCmds[i]));
        }
    }
    if (!remoteDebugging.additionalShellCmdsBefore.IsEmpty())
    {
        wxArrayString initCmds = GetArrayFromString(remoteDebugging.additionalShellCmdsBefore, _T('\n'));
        for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
        {
            macrosManager->ReplaceMacros(initCmds[i]);
            QueueCommand(new DebuggerCmd(this, "shell " + initCmds[i]));
        }
    }

    // if performing remote debugging, now is a good time to try and connect to the target :)
    m_isRemoteDebugging = remoteDebugging.IsOk();
    if (m_isRemoteDebugging)
    {
        if (remoteDebugging.connType == RemoteDebugging::Serial)
            QueueCommand(new GdbCmd_RemoteBaud(this, remoteDebugging.serialBaud));
        QueueCommand(new GdbCmd_RemoteTarget(this, &remoteDebugging));
    }

    // run per-target additional commands (remote debugging)
    // moved after connection to remote target (if any)
    if (!remoteDebugging.additionalCmds.IsEmpty())
    {
        wxArrayString initCmds = GetArrayFromString(remoteDebugging.additionalCmds, _T('\n'));
        for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
        {
            macrosManager->ReplaceMacros(initCmds[i]);
            QueueCommand(new DebuggerCmd(this, initCmds[i]));
        }
    }
    if (!remoteDebugging.additionalShellCmdsAfter.IsEmpty())
    {
        wxArrayString initCmds = GetArrayFromString(remoteDebugging.additionalShellCmdsAfter, _T('\n'));
        for (unsigned int i = 0; i < initCmds.GetCount(); ++i)
        {
            macrosManager->ReplaceMacros(initCmds[i]);
            QueueCommand(new DebuggerCmd(this, "shell " + initCmds[i]));
        }
    }
}

#ifdef __WXMSW__
bool GDB_driver::UseDebugBreakProcess()
{
    return !m_isRemoteDebugging;
}
#endif // __WXMSW__

wxString GDB_driver::GetDisassemblyFlavour(void)
{
    return flavour;
}

// Only called from DebuggerGDB::Debug
// breakOnEntry was always false.  Changed by HC.
void GDB_driver::Start(bool breakOnEntry)
{
    m_attachedToProcess = false;
    ResetCursor();

    // reset other states
    GdbCmd_DisassemblyInit::Clear();
    if (Manager::Get()->GetDebuggerManager()->UpdateDisassembly())
    {
        cbDisassemblyDlg *disassembly_dialog = Manager::Get()->GetDebuggerManager()->GetDisassemblyDialog();
        disassembly_dialog->Clear(cbStackFrame());
    }

    m_BreakOnEntry = breakOnEntry && !m_isRemoteDebugging;

    // if performing remote debugging, use "continue" command
    if (!m_pDBG->GetActiveConfigEx().GetFlag(DebuggerConfiguration::DoNotRun))
    {
        m_ManualBreakOnEntry = !m_isRemoteDebugging;
        // start the process
        if (breakOnEntry)
            QueueCommand(new GdbCmd_Start(this, m_isRemoteDebugging ? _T("continue") : _T("start")));
        else
        {
            // if breakOnEntry is not set, we need to use 'run' to make gdb stop at a breakpoint at first instruction
            m_ManualBreakOnEntry=false;  // must be reset or gdb does not stop at first breakpoint
            QueueCommand(new GdbCmd_Start(this, m_isRemoteDebugging ? _T("continue") : _T("run")));
        }
        m_IsStarted = true;
    }
} // Start

void GDB_driver::Stop()
{
    ResetCursor();
    if (m_pDBG->IsAttachedToProcess())
        QueueCommand(new DebuggerCmd(this, wxT("kill")));
    QueueCommand(new DebuggerCmd(this, _T("quit")));
    m_IsStarted = false;
    m_attachedToProcess = false;
}

void GDB_driver::Continue()
{
    ResetCursor();
    if (m_IsStarted)
        QueueCommand(new GdbCmd_Continue(this));
    else
    {
        // if performing remote debugging, use "continue" command
        if (m_isRemoteDebugging)
            QueueCommand(new GdbCmd_Continue(this));
        else
            QueueCommand(new GdbCmd_Start(this, m_ManualBreakOnEntry ? wxT("start") : wxT("run")));
        m_ManualBreakOnEntry = false;
        m_IsStarted = true;
        m_attachedToProcess = false;
    }
}

void GDB_driver::Step()
{
    ResetCursor();
    QueueCommand(new DebuggerContinueBaseCmd(this, _T("next")));
}

void GDB_driver::StepInstruction()
{
    ResetCursor();
    QueueCommand(new GdbCmd_StepInstruction(this));
}

void GDB_driver::StepIntoInstruction()
{
    ResetCursor();
    QueueCommand(new GdbCmd_StepIntoInstruction(this));
}

void GDB_driver::StepIn()
{
    ResetCursor();
    QueueCommand(new DebuggerContinueBaseCmd(this, _T("step")));
}

void GDB_driver::StepOut()
{
    ResetCursor();
    QueueCommand(new DebuggerContinueBaseCmd(this, _T("finish")));
}

void GDB_driver::SetNextStatement(const wxString& filename, int line)
{
    ResetCursor();
    QueueCommand(new DebuggerCmd(this, wxString::Format(wxT("tbreak %s:%d"), filename.c_str(), line)));
    QueueCommand(new DebuggerContinueBaseCmd(this, wxString::Format(wxT("jump %s:%d"), filename.c_str(), line)));
}

void GDB_driver::Backtrace()
{
    QueueCommand(new GdbCmd_Backtrace(this));
}

void GDB_driver::Disassemble()
{
    if (platform::windows)
        QueueCommand(new GdbCmd_DisassemblyInit(this, flavour));
    else
        QueueCommand(new GdbCmd_DisassemblyInit(this));
}

void GDB_driver::CPURegisters()
{
    if (platform::windows)
        QueueCommand(new GdbCmd_InfoRegisters(this, flavour));
    else
        QueueCommand(new GdbCmd_InfoRegisters(this));
}

void GDB_driver::SwitchToFrame(size_t number)
{
    ResetCursor();
    QueueCommand(new DebuggerCmd(this, wxString(_T("frame ")) << number));
}

void GDB_driver::SetVarValue(const wxString& var, const wxString& value)
{
    const wxString &cleanValue=CleanStringValue(value);
    QueueCommand(new DebuggerCmd(this, wxString::Format(_T("set variable %s=%s"), var.c_str(), cleanValue.c_str())));
}

void GDB_driver::SetMemoryRangeValue(uint64_t addr, const wxString& value)
{
    const size_t size = value.size();
    if(size == 0)
        return;

    wxString dataStr = wxT("{");
    const wxCharBuffer &data = value.To8BitData();
    for (size_t i = 0; i < size; i++)
    {
        if (i != 0)
            dataStr << wxT(",");
        dataStr << wxString::Format(wxT("0x%x"), uint8_t(data[i]));
    }
    dataStr << wxT("}");

    wxString commandStr;
// Check if build is for WX MS Windows
#ifdef __WXMSW__
    commandStr.Printf(wxT("set {char [%ul]} 0x%" PRIx64 "="), size, addr);
#else
    commandStr.Printf(wxT("set {char [%zu]} 0x%" PRIx64 "="), size, addr);
#endif // __WXMSW__
    commandStr << dataStr;

    QueueCommand(new DebuggerCmd(this, commandStr));
}

void GDB_driver::SetMemoryRangeValue(wxString address, const wxString& value)
{
    const size_t size = value.size();
    wxULongLong_t llAddres;

    if ((size == 0) || (!address.ToULongLong(&llAddres, 16)))
        return;

    wxString dataStr = wxT("{");
    const wxCharBuffer &data = value.To8BitData();
    for (size_t i = 0; i < size; i++)
    {
        if (i != 0)
            dataStr << wxT(",");
        dataStr << wxString::Format(wxT("0x%x"), uint8_t(data[i]));
    }
    dataStr << wxT("}");

    wxString commandStr;
    // Check if build is for WX MS Windows
    #ifdef __WXMSW__
        commandStr.Printf(wxT("set {char [%ul]} 0x%" PRIx64 "="), size, uint64_t(llAddres));
    #else
        commandStr.Printf(wxT("set {char [%zu]} 0x%" PRIx64 "="), size, uint64_t(llAddres));
    #endif // __WXMSW__

    commandStr << dataStr;

    QueueCommand(new DebuggerCmd(this, commandStr));
}

void GDB_driver::MemoryDump()
{
    QueueCommand(new GdbCmd_ExamineMemory(this));
}

void GDB_driver::RunningThreads()
{
    if (Manager::Get()->GetDebuggerManager()->UpdateThreads())
        QueueCommand(new GdbCmd_Threads(this));
}

void GDB_driver::InfoFrame()
{
    QueueCommand(new DebuggerInfoCmd(this, _T("info frame"), _("Selected frame")));
}

void GDB_driver::InfoDLL()
{
    if (platform::windows)
        QueueCommand(new DebuggerInfoCmd(this, _T("info dll"), _("Loaded libraries")));
    else
        QueueCommand(new DebuggerInfoCmd(this, _T("info sharedlibrary"), _("Loaded libraries")));
}

void GDB_driver::InfoFiles()
{
    QueueCommand(new DebuggerInfoCmd(this, _T("info files"), _("Files and targets")));
}

void GDB_driver::InfoFPU()
{
    QueueCommand(new DebuggerInfoCmd(this, _T("info float"), _("Floating point unit")));
}

void GDB_driver::InfoSignals()
{
    QueueCommand(new DebuggerInfoCmd(this, _T("info signals"), _("Signals handling")));
}

void GDB_driver::EnableCatchingThrow(bool enable)
{
    if (enable)
        QueueCommand(new GdbCmd_SetCatch(this, wxT("throw"), &m_catchThrowIndex));
    else if (m_catchThrowIndex != -1)
    {
        QueueCommand(new DebuggerCmd(this, wxString::Format(wxT("delete %d"), m_catchThrowIndex)));
        m_catchThrowIndex = -1;
    }
}

void GDB_driver::SwitchThread(size_t threadIndex)
{
    ResetCursor();
    QueueCommand(new DebuggerCmd(this, wxString::Format("thread %zu", threadIndex)));
    if (Manager::Get()->GetDebuggerManager()->UpdateBacktrace())
        QueueCommand(new GdbCmd_Backtrace(this));
}

void GDB_driver::AddBreakpoint(cb::shared_ptr<DebuggerBreakpoint> bp)
{
    if (bp->type == DebuggerBreakpoint::bptData)
        QueueCommand(new GdbCmd_AddDataBreakpoint(this, bp));
    //Workaround for GDB to break on C++ constructor/destructor
    else
    {
        if (bp->func.IsEmpty() && !bp->lineText.IsEmpty())
        {
            wxRegEx reCtorDtor(_T("([0-9A-z_]+)::([~]?)([0-9A-z_]+)[[:blank:]\(]*"));
            if (reCtorDtor.Matches(bp->lineText))
            {
                wxString strBase = reCtorDtor.GetMatch(bp->lineText, 1);
                wxString strDtor = reCtorDtor.GetMatch(bp->lineText, 2);
                wxString strMethod = reCtorDtor.GetMatch(bp->lineText, 3);
                if (strBase.IsSameAs(strMethod))
                {
                    bp->func = strBase;
                    bp->func << _T("::");
                    bp->func << strDtor;
                    bp->func << strMethod;
    //                if (bp->temporary)
    //                    bp->temporary = false;
                    NotifyCursorChanged(); // to force breakpoints window update
                }
            }
        }
        //end GDB workaround

        QueueCommand(new GdbCmd_AddBreakpoint(this, bp));
    }
}

void GDB_driver::RemoveBreakpoint(cb::shared_ptr<DebuggerBreakpoint> bp)
{
    if (bp && bp->index != -1)
        QueueCommand(new GdbCmd_RemoveBreakpoint(this, bp));
}

void GDB_driver::EvaluateSymbol(const wxString& symbol, const wxRect& tipRect)
{
    QueueCommand(new GdbCmd_FindTooltipType(this, symbol, tipRect));
}

void GDB_driver::UpdateWatches(cb::shared_ptr<GDBWatch> localsWatch,
                               cb::shared_ptr<GDBWatch> funcArgsWatch,
                               WatchesContainer &watches, bool ignoreAutoUpdate)
{
    if (!m_FileName.IsSameAs(m_Cursor.file))
    {
        m_FileName = m_Cursor.file;
        m_pDBG->DetermineLanguage();
    }

    bool updateWatches = false;
    if (localsWatch && (localsWatch->IsAutoUpdateEnabled() || ignoreAutoUpdate))
    {
        QueueCommand(new GdbCmd_LocalsFuncArgs(this, localsWatch, true));
        updateWatches = true;
    }
    if (funcArgsWatch && (funcArgsWatch->IsAutoUpdateEnabled() || ignoreAutoUpdate))
    {
        QueueCommand(new GdbCmd_LocalsFuncArgs(this, funcArgsWatch, false));
        updateWatches = true;
    }

    for (WatchesContainer::iterator it = watches.begin(); it != watches.end(); ++it)
    {
        WatchesContainer::reference watch = *it;
        if (watch->IsAutoUpdateEnabled() || ignoreAutoUpdate)
        {
            QueueCommand(new GdbCmd_FindWatchType(this, watch));
            updateWatches = true;
        }
    }

    if (updateWatches)
    {
        // run this action-only command to update the tree
        QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::Watches));
    }
}

void GDB_driver::UpdateMemoryRangeWatches(MemoryRangeWatchesContainer &watches,
                                          bool ignoreAutoUpdate)
{
    bool updateWatches = false;
    for (cb::shared_ptr<GDBMemoryRangeWatch> &watch : watches)
    {
        if (watch->IsAutoUpdateEnabled() || ignoreAutoUpdate)
        {
            QueueCommand(new GdbCmd_MemoryRangeWatch(this, watch));
            updateWatches = true;
        }
    }

    if (updateWatches)
        QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::MemoryRange));
}

void GDB_driver::UpdateWatch(const cb::shared_ptr<GDBWatch> &watch)
{
    QueueCommand(new GdbCmd_FindWatchType(this, watch));
    QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::Watches));
}

void GDB_driver::UpdateMemoryRangeWatch(const cb::shared_ptr<GDBMemoryRangeWatch> &watch)
{
    QueueCommand(new GdbCmd_MemoryRangeWatch(this, watch));
    QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::MemoryRange));
}

void GDB_driver::UpdateWatchLocalsArgs(cb::shared_ptr<GDBWatch> const &watch, bool locals)
{
    QueueCommand(new GdbCmd_LocalsFuncArgs(this, watch, locals));
    QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::Watches));
}

void GDB_driver::Attach(int pid)
{
    m_IsStarted = true;
    m_attachedToProcess = true;
    SetChildPID(pid);
    QueueCommand(new GdbCmd_AttachToProcess(this, pid));
}

void GDB_driver::Detach()
{
    QueueCommand(new GdbCmd_Detach(this));
}

void GDB_driver::ParseOutput(const wxString& output)
{
    m_Cursor.changed = false;

    if (platform::windows && m_ChildPID == 0)
    {
        if (reChildPid2.Matches(output)) // [New Thread 2684.0xf40] or [New thread 2684.0xf40]
        {
            wxString pidStr = reChildPid2.GetMatch(output, 0);
            pidStr = pidStr.BeforeFirst(_T('.')); //[New Thread 2684.0xf40] -> [New Thread 2684
            pidStr = pidStr.AfterFirst(_T('d')); //[New Thread 2684 ->  2684
            long pid = 0;
            pidStr.ToLong(&pid);
            SetChildPID(pid);
            m_pDBG->Log(wxString::Format(_("Child process PID: %ld"), pid));
        }
    }
    else if (!platform::windows && m_ChildPID == 0)
    {
        if (reChildPid1.Matches(output)) // [Switching to Thread -1234655568 (LWP 18590)]
        {
            wxString pidStr = reChildPid1.GetMatch(output, 1);
            long pid = 0;
            pidStr.ToLong(&pid);
            SetChildPID(pid);
            m_pDBG->Log(wxString::Format(_("Child process PID: %ld"), pid));
        }
    }

    if (   output.StartsWith(_T("gdb: "))
        || output.StartsWith(_T("warning: "))
        || output.StartsWith(_T("Warning: "))
        || output.StartsWith(_T("ContinueDebugEvent ")) )
    {
        return;
    }

    static wxString buffer;
    buffer << output << _T('\n');

    m_pDBG->DebugLog(output);

    int idx = buffer.First(GDB_PROMPT);
    const bool foundPrompt = (idx != wxNOT_FOUND);
    if (!foundPrompt)
    {
        // don't uncomment the following line
        // m_ProgramIsStopped is set to false in DebuggerDriver::RunQueue()
//        m_ProgramIsStopped = false;
        return; // come back later
    }

    m_QueueBusy = false;
    int changeFrameAddr = 0 ;
    DebuggerCmd* cmd = CurrentCommand();
    if (cmd)
    {
//        DebugLog(wxString::Format(_T("Command parsing output (cmd: %s): %s"), cmd->m_Cmd.c_str(), buffer.Left(idx).c_str()));
        RemoveTopCommand(false);
        buffer.Remove(idx);
        // remove the '>>>>>>' part of the prompt (or what's left of it)
        int cnt = 6; // max 6 '>'
        while (!buffer.empty() && buffer.Last() == _T('>') && cnt--)
            buffer.RemoveLast();
        if (!buffer.empty() && buffer.Last() == _T('\n'))
            buffer.RemoveLast();
        cmd->ParseOutput(buffer.Left(idx));

        //We do NOT want default output processing for a changed frame as it can result
        //in disassembly being done for a non-current location, since some of the frame
        //response lines are in the pattern of breakpoint output.
        GdbCmd_ChangeFrame *changeFrameCmd = dynamic_cast<GdbCmd_ChangeFrame*>(cmd);
        if (changeFrameCmd)
            changeFrameAddr = changeFrameCmd->AddrChgMode();

        delete cmd;
        RunQueue();
    }

    m_needsUpdate = false;
    m_forceUpdate = false;

    // non-command messages (e.g. breakpoint hits)
    // break them up in lines

    const wxArrayString lines = GetArrayFromString(buffer, _T('\n'));
    for (unsigned int i = 0; i < lines.GetCount(); ++i)
    {
        // log GDB's version
        if (lines[i].StartsWith(_T("GNU gdb")))
        {
            // it's the gdb banner. Just display the version and "eat" the rest
            m_pDBG->Log(_("Debugger name and version: ") + lines[i]);
            // keep major and minor version numbers handy
            wxRegEx re(_T("([0-9.]+)"));
            if (!re.Matches(lines[i]))
            {
                m_pDBG->Log(_("Unable to determine the version of gdb"));
                break;
            }
            wxString major = re.GetMatch(lines[i],0);
            wxString minor = major;
            major = major.BeforeFirst(_T('.')); // 6.3.2 -> 6
            minor = minor.AfterFirst(_T('.'));  // 6.3.2 -> 3.2
            minor = minor.BeforeFirst(_T('.')); // 3.2 -> 3
            major.ToLong(&m_GDBVersionMajor);
            minor.ToLong(&m_GDBVersionMinor);
//            wxString log;
//            log.Printf(_T("Line: %s\nMajor: %s (%d)\nMinor: %s (%d)"),
//                        lines[i].c_str(),
//                        major.c_str(),
//                        m_GDBVersionMajor,
//                        minor.c_str(),
//                        m_GDBVersionMinor);
//            m_pDBG->Log(log);
            break;
        }

        // Is the program exited?
        else if (   lines[i].StartsWith(_T("Error creating process"))
                 || lines[i].StartsWith(_T("Program exited"))
                 || lines[i].StartsWith(wxT("Program terminated with signal"))
                 || lines[i].StartsWith(wxT("During startup program exited"))
                 || lines[i].Contains(_T("program is not being run"))
                 || lines[i].Contains(_T("Target detached"))
                 || reInferiorExited.Matches(lines[i])
                 || reInferiorExitedWithCode.Matches(lines[i]) )
        {
            m_pDBG->Log(lines[i]);
            m_ProgramIsStopped = true;
            QueueCommand(new DebuggerCmd(this, _T("quit")));
            m_IsStarted = false;
        }

        // no debug symbols?
        else if (lines[i].Contains(_T("(no debugging symbols found)")))
            m_pDBG->Log(lines[i]);

        // signal
        else if (lines[i].StartsWith(_T("Program received signal SIG")))
        {
            m_ProgramIsStopped = true;
            m_QueueBusy = false;

            if (   lines[i].StartsWith(_T("Program received signal SIGINT"))
                || lines[i].StartsWith(_T("Program received signal SIGTRAP"))
                || lines[i].StartsWith(_T("Program received signal SIGSTOP")) )
            {
                // these are break/trace signals, just log them
                Log(lines[i]);
            }
            else
            {
                Log(lines[i]);
                m_pDBG->BringCBToFront();

                if (Manager::Get()->GetDebuggerManager()->ShowBacktraceDialog())
                    m_forceUpdate = true;

                InfoWindow::Display(_("Signal received"), _T("\n\n") + lines[i] + _T("\n\n"));
                m_needsUpdate = true;
                // the backtrace will be generated when NotifyPlugins() is called
                // and only if the backtrace window is shown
            }
        }

        // general errors
        // we don't deal with them, just relay them back to the user
        else if (   lines[i].StartsWith(_T("Error "))
                 || lines[i].StartsWith(_T("No such"))
                 || lines[i].StartsWith(_T("Cannot evaluate")) )
        {
            m_pDBG->Log(lines[i]);
        }

        else if (   (lines[i].StartsWith(_T("Cannot find bounds of current function")))
                 || (lines[i].StartsWith(_T("No stack"))) )
        {
            m_pDBG->Log(lines[i]);
            m_ProgramIsStopped = true;
        }

        // pending breakpoint resolved?
        // e.g.
        // Pending breakpoint "C:/Devel/libs/irr_svn/source/Irrlicht/CSceneManager.cpp:1077" resolved
        // Breakpoint 2, irr::scene::CSceneManager::getSceneNodeFromName (this=0x3fa878, name=0x3fbed8 "MainLevel", start=0x3fa87c) at CSceneManager.cpp:1077
        else if (lines[i].StartsWith(_T("Pending breakpoint ")))
        {
            m_pDBG->Log(lines[i]);

            // we face a problem here:
            // gdb sets a *new* breakpoint when the pending address is resolved.
            // this means we must update the breakpoint index we have stored
            // or else we can never remove this (because the breakpoint index doesn't match)...

            // Pending breakpoint "C:/Devel/libs/irr_svn/source/Irrlicht/CSceneManager.cpp:1077" resolved
            wxString bpstr = lines[i];

            if (rePendingFound.Matches(bpstr))
            {
                // there are cases where 'newbpstr' is not the next message
                // e.g. [Switching to thread...]
                // so we 'll loop over lines starting with [

                // Breakpoint 2, irr::scene::CSceneManager::getSceneNodeFromName (this=0x3fa878, name=0x3fbed8 "MainLevel", start=0x3fa87c) at CSceneManager.cpp:1077
                wxString newbpstr = lines[++i];
                while (i < lines.GetCount() - 1 && newbpstr.StartsWith(_T("[")))
                    newbpstr = lines[++i];

                if (rePendingFound1.Matches(newbpstr))
                {
//                    m_pDBG->Log(_("MATCH"));

                    wxString file;
                    wxString lineStr;

                    if (platform::windows)
                    {
                        file = rePendingFound.GetMatch(bpstr, 1) + rePendingFound.GetMatch(bpstr, 2);
                        lineStr = rePendingFound.GetMatch(bpstr, 3);
                    }
                    else
                    {
                        file = rePendingFound.GetMatch(bpstr, 1);
                        lineStr = rePendingFound.GetMatch(bpstr, 2);
                    }

                    if (platform::windows && m_CygwinPresent)
                        cbGetWindowsPathFromCygwinPath(file);
                    else
                        file = UnixFilename(file);

    //                m_pDBG->Log(wxString::Format(_("file: %s, line: %s"), file, lineStr));
                    long line;
                    lineStr.ToLong(&line);
                    DebuggerState& state = m_pDBG->GetState();
                    int bpindex = state.HasBreakpoint(file, line - 1, false);
                    cb::shared_ptr<DebuggerBreakpoint> bp = state.GetBreakpoint(bpindex);
                    if (bp)
                    {
    //                    m_pDBG->Log(_("Found BP!!! Updating index..."));
                        long index;
                        wxString indexStr = rePendingFound1.GetMatch(newbpstr, 1);
                        indexStr.ToLong(&index);
                        // finally! update the breakpoint index
                        bp->index = index;
                    }
                }
            }
        }

        else if (lines[i].StartsWith(wxT("Breakpoint ")))
        {
            if (rePendingFound1.Matches(lines[i]))
            {
                long index;
                rePendingFound1.GetMatch(lines[i],1).ToLong(&index);
                DebuggerState& state = m_pDBG->GetState();
                cb::shared_ptr<DebuggerBreakpoint> bp = state.GetBreakpointByNumber(index);
                if (bp && bp->wantsCondition)
                {
                    bp->wantsCondition = false;
                    QueueCommand(new GdbCmd_AddBreakpointCondition(this, bp));
                    m_needsUpdate = true;
                }
            }
        }

        else if (lines[i].StartsWith(wxT("Temporary breakpoint")))
        {
            if (reTempBreakFound.Matches(lines[i]))
            {
                long index;
                reTempBreakFound.GetMatch(lines[i],1).ToLong(&index);
                DebuggerState& state = m_pDBG->GetState();
                cb::shared_ptr<DebuggerBreakpoint> bp = state.GetBreakpointByNumber(index);
                state.RemoveBreakpoint(bp, false);
                Manager::Get()->GetDebuggerManager()->GetBreakpointDialog()->Reload();
            }
        }

        // cursor change
        else if (!lines[i].empty() && lines[i][0] == wxUniChar(26)) // ->->
        {
            // breakpoint, e.g.
            // C:/Devel/tmp/test_console_dbg/tmp/main.cpp:14:171:beg:0x401428

            // Main breakpoint handler is wrapped into a function so we can use
            // the same code with different regular expressions - depending on
            // the platform.

            //NOTE: This also winds up matching response to a frame command which is generated as
            //part of a backtrace with autoswitch enabled, (from gdb7.2 mingw) as in:
            //(win32, x86, mingw gdb 7.2)
            //>>>>>>cb_gdb:
            //> frame 1
            //#1  0x6f826722 in wxInitAllImageHandlers () at ../../src/common/imagall.cpp:29
            //^Z^ZC:\dev\wxwidgets\wxWidgets-2.8.10\build\msw/../../src/common/imagall.cpp:29:961:beg:0x6f826722
            //>>>>>>cb_gdb:

            HandleMainBreakPoint(reBreak, lines[i]);
        }
        else
        {
            bool isFileUpdated = false;
            // other break info, e.g.
            // 0x7c9507a8 in ntdll!KiIntSystemCall () from C:\WINDOWS\system32\ntdll.dll
            wxRegEx* re = 0;
            if ( reBreak2.Matches(lines[i]) )
                re = &reBreak2;
            else if (reThreadSwitch.Matches(lines[i]))
                re = &reThreadSwitch;

            if ( re )
            {
                m_Cursor.file = re->GetMatch(lines[i], 3);
                m_Cursor.function = re->GetMatch(lines[i], 2);
                // wxString lineStr = _T("");
                m_Cursor.address = re->GetMatch(lines[i], 1);
                m_Cursor.line = -1;
                m_Cursor.changed = true;
                m_needsUpdate = true;
                isFileUpdated = true;
            }
            else if ( reThreadSwitch2.Matches(lines[i]) )
            {
                m_Cursor.file = reThreadSwitch2.GetMatch(lines[i], 3);
                m_Cursor.function = reThreadSwitch2.GetMatch(lines[i], 2);
                // wxString lineStr = reThreadSwitch2.GetMatch(lines[i], 4);
                m_Cursor.address = reThreadSwitch2.GetMatch(lines[i], 1);
                m_Cursor.line = -1;
                m_Cursor.changed = true;
                m_needsUpdate = true;
                isFileUpdated = true;
            }
            else if (reBreak3.Matches(lines[i]) )
            {
                m_Cursor.file=_T("");
                m_Cursor.function= reBreak3.GetMatch(lines[i], 2);
                m_Cursor.address = reBreak3.GetMatch(lines[i], 1);
                m_Cursor.line = -1;
                m_Cursor.changed = true;
                m_needsUpdate = true;
            }
            else if (reCatchThrow.Matches(lines[i]) )
            {
                m_Cursor.file = reCatchThrow.GetMatch(lines[i], 4);
                m_Cursor.function= reCatchThrow.GetMatch(lines[i], 3);
                m_Cursor.address = reCatchThrow.GetMatch(lines[i], 2);
                m_Cursor.line = -1;
                m_Cursor.changed = true;
                m_needsUpdate = true;
            }
            else if (reCatchThrowNoFile.Matches(lines[i]) )
            {
                m_Cursor.file = wxEmptyString;
                m_Cursor.function= reCatchThrowNoFile.GetMatch(lines[i], 3);
                m_Cursor.address = reCatchThrowNoFile.GetMatch(lines[i], 2);
                m_Cursor.line = -1;
                m_Cursor.changed = true;
                m_needsUpdate = true;
            }

            if (isFileUpdated && platform::windows && m_CygwinPresent)
                cbGetWindowsPathFromCygwinPath(m_Cursor.file);
        }
    }
    buffer.Clear();

    if (foundPrompt && m_DCmds.empty() && !m_ProgramIsStopped && !m_Cursor.changed)
    {
        QueueCommand(new GdbCmd_FindCursor(this));
        m_ProgramIsStopped = true;
    }

    // if program is stopped, update various states
    if (m_needsUpdate)
    {
        if (1 == changeFrameAddr)
        {
            // clear to avoid change of disassembly address on (auto) frame change
            // when NotifyCursorChanged() executes
            m_Cursor.address.clear();
        }
        if (m_Cursor.changed)
        {
            m_ProgramIsStopped = true;
            m_QueueBusy = false;
        }
        if (m_Cursor.changed || m_forceUpdate)
            NotifyCursorChanged();
    }

    if (m_ProgramIsStopped)
        RunQueue();
}


void GDB_driver::HandleMainBreakPoint(const wxRegEx& reBreak_in, wxString line)
{
    if ( reBreak_in.Matches(line) )
    {
        if (m_ManualBreakOnEntry)
            QueueCommand(new GdbCmd_InfoProgram(this), DebuggerDriver::High);

        if (m_ManualBreakOnEntry && !m_BreakOnEntry)
            Continue();
        else
        {
            m_ManualBreakOnEntry = false;
            wxString lineStr;

            if (platform::windows)
            {
                m_Cursor.file = reBreak_in.GetMatch(line, 1) + reBreak_in.GetMatch(line, 2);
                if (m_CygwinPresent)
                    cbGetWindowsPathFromCygwinPath(m_Cursor.file);
            }
            else
            {
                // For debugging of usual linux application 'GetMatch(line, 1)' is empty.
                // While for debugging of application under wine the name of the disk is useless.
                m_Cursor.file = reBreak_in.GetMatch( line, 2);
            }

            lineStr = reBreak_in.GetMatch(line, 3);
            m_Cursor.address = reBreak_in.GetMatch(line, 4);

            lineStr.ToLong(&m_Cursor.line);
            m_Cursor.changed = true;
            m_needsUpdate = true;
        }
    }
    else
    {
        m_pDBG->Log(_("The program has stopped on a breakpoint but the breakpoint format is not recognized:"));
        m_pDBG->Log(line);
        m_Cursor.changed = true;
        m_needsUpdate = true;
    }
}

void GDB_driver::DetermineLanguage()
{
    QueueCommand(new GdbCmd_DebugLanguage(this));
}

