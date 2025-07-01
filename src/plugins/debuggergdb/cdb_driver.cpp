/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#ifndef CB_PRECOMP
    #include "cbproject.h"
    #include "cbexception.h"
    #include "configmanager.h"
    #include "debuggermanager.h"
    #include "globals.h"
    #include "infowindow.h"
    #include "logmanager.h"
    #include "manager.h"
    #include "projectbuildtarget.h"
#endif

#include "cdb_driver.h"
#include "cdb_commands.h"
#include "debuggeroptionsdlg.h"

#include "cbdebugger_interfaces.h"

// Parse CDB prompts. Support both 32 and 64 bit inferiors
// The strings looks something like this:
// 0:000>
// 0:000:x86>
static wxRegEx rePrompt(_T("([0-9]+:){1,2}[0-9]+(:x86)?>"));

static wxRegEx reBP(_T("Breakpoint ([0-9]+) hit"));
// one stack frame (to access current file; is there another way???)
//  # ChildEBP RetAddr
// 00 0012fe98 00401426 Win32GUI!WinMain+0x89 [c:\devel\tmp\win32 test\main.cpp @ 55]
static wxRegEx reFile(_T("[[:blank:]]([A-z]+.*)[[:blank:]]+\\[([A-z]:)(.*) @ ([0-9]+)\\]"));

CDB_driver::CDB_driver(DebuggerGDB* plugin)
    : DebuggerDriver(plugin),
    m_Target(nullptr),
    m_IsStarted(false)
{
    //ctor
}

CDB_driver::~CDB_driver()
{
    //dtor
}

wxString CDB_driver::GetCommonCommandLine(const wxString& debugger)
{
    wxString cmd;
    cmd << debugger;
//    cmd << " -g"; // ignore starting breakpoint
    cmd << " -G"; // ignore ending breakpoint
    cmd << " -lines"; // line info

    if (m_Target->GetTargetType() == ttConsoleOnly)
        cmd << " -2"; // tell the debugger to launch a console for us

    if (m_Dirs.GetCount() > 0)
    {
        // add symbols dirs
        cmd << " -y ";
        for (size_t i = 0; i < m_Dirs.GetCount(); ++i)
            cmd << m_Dirs[i] << wxPATH_SEP;

        // add source dirs
        cmd << " -srcpath ";
        for (size_t i = 0; i < m_Dirs.GetCount(); ++i)
            cmd << m_Dirs[i] << wxPATH_SEP;
    }
    return cmd;
}

wxString CDB_driver::GetCommandLine(const wxString& debugger, const wxString& debuggee, const wxString& userArguments)
{
    wxString cmd = GetCommonCommandLine(debugger);
    cmd << ' ';

    // finally, add the program to debug
    wxFileName debuggeeFileName(debuggee);
    if (debuggeeFileName.IsAbsolute())
        cmd << debuggee;
    else
        cmd << m_Target->GetParentProject()->GetBasePath() << '/' << debuggee;

    if (!userArguments.empty())
        cmd << ' ' << userArguments;

    return cmd;
}

// User arguments has no sense when attaching to a running process
wxString CDB_driver::GetCommandLine(const wxString& debugger, int pid, cb_unused const wxString& userArguments)
{
    wxString cmd = GetCommonCommandLine(debugger);
    // finally, add the PID
    cmd << wxString::Format(" -p %d", pid);
    return cmd;
}

void CDB_driver::SetTarget(ProjectBuildTarget* target)
{
    m_Target = target;
}

void CDB_driver::Prepare(cb_unused bool isConsole, cb_unused int printElements,
                         cb_unused const RemoteDebugging &remoteDebugging)
{
    // The very first command won't get the right output back due to the spam on CDB launch.
    // Throw in a dummy command to flush the output buffer.
    m_QueueBusy = true;
    QueueCommand(new DebuggerCmd(this,_T(".echo Clear buffer")),High);

    // Either way, get the PID of the child
    QueueCommand(new CdbCmd_GetPID(this));
}

void CDB_driver::Start(cb_unused bool breakOnEntry)
{
    // start the process
    QueueCommand(new DebuggerCmd(this, _T("l+t"))); // source mode
    QueueCommand(new DebuggerCmd(this, _T("l+s"))); // show source lines
    QueueCommand(new DebuggerCmd(this, _T("l+o"))); // only source lines

    if (!m_pDBG->GetActiveConfigEx().GetFlag(DebuggerConfiguration::DoNotRun))
    {
        QueueCommand(new CdbCmd_Continue(this));
        m_IsStarted = true;
    }
}

void CDB_driver::Stop()
{
    ResetCursor();
    QueueCommand(new DebuggerCmd(this, _T("q")));
    m_IsStarted = false;
}

void CDB_driver::Continue()
{
    ResetCursor();
    QueueCommand(new CdbCmd_Continue(this));
    m_IsStarted = true;
}

void CDB_driver::Step()
{
    ResetCursor();
    QueueCommand(new DebuggerContinueBaseCmd(this, _T("p")));
    // print a stack frame to find out about the file we 've stopped
    QueueCommand(new CdbCmd_SwitchFrame(this, -1));
}

void CDB_driver::StepInstruction()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::StepIntoInstruction()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::StepIn()
{
    ResetCursor();
    QueueCommand(new DebuggerContinueBaseCmd(this, _T("t")));
    Step();
}

void CDB_driver::StepOut()
{
    ResetCursor();
    QueueCommand(new DebuggerContinueBaseCmd(this, _T("gu")));
    QueueCommand(new CdbCmd_SwitchFrame(this, -1));
}

void CDB_driver::SetNextStatement(cb_unused const wxString& filename, cb_unused int line)
{
    NOT_IMPLEMENTED();
}

void CDB_driver::Backtrace()
{
    DoBacktrace(false);
}

void CDB_driver::DoBacktrace(bool switchToFirst)
{
    if (Manager::Get()->GetDebuggerManager()->UpdateBacktrace())
        QueueCommand(new CdbCmd_Backtrace(this, switchToFirst));
}

void CDB_driver::Disassemble()
{
    QueueCommand(new CdbCmd_DisassemblyInit(this));
}

void CDB_driver::CPURegisters()
{
    QueueCommand(new CdbCmd_InfoRegisters(this));
}

void CDB_driver::SwitchToFrame(size_t number)
{
    ResetCursor();
    QueueCommand(new CdbCmd_SwitchFrame(this, number));
}

void CDB_driver::SetVarValue(cb_unused const wxString& var, cb_unused const wxString& value)
{
    NOT_IMPLEMENTED();
}

void CDB_driver::SetMemoryRangeValue(cb_unused uint64_t addr, cb_unused const wxString& value)
{
    NOT_IMPLEMENTED();
}

void CDB_driver::SetMemoryRangeValue(wxString address, const wxString& value)
{
    NOT_IMPLEMENTED();
}

void CDB_driver::MemoryDump()
{
    QueueCommand(new CdbCmd_ExamineMemory(this));
}

void CDB_driver::RunningThreads()
{
    if (Manager::Get()->GetDebuggerManager()->UpdateThreads())
        QueueCommand(new CdbCmd_Threads(this));
}

void CDB_driver::InfoFrame()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::InfoDLL()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::InfoFiles()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::InfoFPU()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::InfoSignals()
{
    NOT_IMPLEMENTED();
}

void CDB_driver::EnableCatchingThrow(cb_unused bool enable)
{
    NOT_IMPLEMENTED();
}

void CDB_driver::AddBreakpoint(cb::shared_ptr<DebuggerBreakpoint> bp)
{
    QueueCommand(new CdbCmd_AddBreakpoint(this, bp));
}

void CDB_driver::RemoveBreakpoint(cb::shared_ptr<DebuggerBreakpoint> bp)
{
    QueueCommand(new CdbCmd_RemoveBreakpoint(this, bp));
}

void CDB_driver::EvaluateSymbol(const wxString& symbol, const wxRect& tipRect)
{
    QueueCommand(new CdbCmd_TooltipEvaluation(this, symbol, tipRect));
}

void CDB_driver::UpdateWatches(cb_unused cb::shared_ptr<GDBWatch> localsWatch,
                               cb_unused cb::shared_ptr<GDBWatch> funcArgsWatch,
                               WatchesContainer &watches, bool ignoreAutoUpdate)
{
    bool updateWatches = false;
    if (localsWatch && localsWatch->IsAutoUpdateEnabled())
    {
        QueueCommand(new CdbCmd_LocalsFuncArgs(this, localsWatch, true));
        updateWatches = true;
    }
    if (funcArgsWatch && funcArgsWatch->IsAutoUpdateEnabled())
    {
        QueueCommand(new CdbCmd_LocalsFuncArgs(this, funcArgsWatch, false));
        updateWatches = true;
    }

    for (WatchesContainer::iterator it = watches.begin(); it != watches.end(); ++it)
    {
        WatchesContainer::reference watch = *it;
        if (watch->IsAutoUpdateEnabled() || ignoreAutoUpdate)
        {
            QueueCommand(new CdbCmd_Watch(this, *it));
            updateWatches = true;
        }
    }

    if (updateWatches)
        QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::Watches));

    // FIXME (obfuscated#): reimplement this code
//    // start updating watches tree
//    tree->BeginUpdateTree();
//
//    // locals before args because of precedence
//    if (doLocals)
//        QueueCommand(new CdbCmd_InfoLocals(this, tree));
////    if (doArgs)
////        QueueCommand(new CdbCmd_InfoArguments(this, tree));
//    for (unsigned int i = 0; i < tree->GetWatches().GetCount(); ++i)
//    {
//        Watch& w = tree->GetWatches()[i];
//        QueueCommand(new CdbCmd_Watch(this, tree, &w));
//    }
//
//    // run this action-only command to update the tree
//    QueueCommand(new DbgCmd_UpdateWatchesTree(this, tree));
}

void CDB_driver::UpdateWatch(const cb::shared_ptr<GDBWatch> &watch)
{
    QueueCommand(new CdbCmd_Watch(this, watch));
    QueueCommand(new DbgCmd_UpdateWindow(this, cbDebuggerPlugin::DebugWindows::Watches));
}

void CDB_driver::UpdateWatchLocalsArgs(cb_unused cb::shared_ptr<GDBWatch> const &watch,
                                       cb_unused bool locals)
{
    // FIXME (obfuscated#): implement this
}

void CDB_driver::Attach(cb_unused int pid)
{
    // FIXME (obfuscated#): implement this
}

void CDB_driver::UpdateMemoryRangeWatches(cb_unused MemoryRangeWatchesContainer &watches,
                                          cb_unused bool ignoreAutoUpdate)
{
    // FIXME (bluehazzard#): implement this
    NOT_IMPLEMENTED();
}

void CDB_driver::UpdateMemoryRangeWatch(cb_unused const cb::shared_ptr<GDBMemoryRangeWatch> &watch)
{
    // FIXME (bluehazzard#): implement this
    NOT_IMPLEMENTED();
}

void CDB_driver::Detach()
{
    QueueCommand(new CdbCmd_Detach(this));
}

void CDB_driver::ParseOutput(const wxString& output)
{
    m_Cursor.changed = false;
    static wxString buffer;
    buffer << output << _T('\n');

    m_pDBG->DebugLog(output);

    if (rePrompt.Matches(buffer))
    {
        int idx = buffer.First(rePrompt.GetMatch(buffer));
        cbAssert(idx != wxNOT_FOUND);
        m_ProgramIsStopped = true;
        m_QueueBusy = false;
        DebuggerCmd* cmd = CurrentCommand();
        if (cmd)
        {
            RemoveTopCommand(false);
            buffer.Remove(idx);
            if (buffer[buffer.Length() - 1] == _T('\n'))
                buffer.Remove(buffer.Length() - 1);
            cmd->ParseOutput(buffer.Left(idx));
            delete cmd;
            RunQueue();
        }
    }
    else
        return; // come back later

    bool notifyChange = false;

    // non-command messages (e.g. breakpoint hits)
    // break them up in lines
    wxArrayString lines = GetArrayFromString(buffer, _T('\n'));
    for (unsigned int i = 0; i < lines.GetCount(); ++i)
    {
//            Log(_T("DEBUG: ") + lines[i]); // write it in the full debugger log

        if (lines[i].StartsWith(_T("Cannot execute ")))
        {
            Log(lines[i]);
        }
        else if (lines[i].Contains(_T("Access violation")))
        {
            m_ProgramIsStopped = true;
            Log(lines[i]);
            m_pDBG->BringCBToFront();

            Manager::Get()->GetDebuggerManager()->ShowBacktraceDialog();
            DoBacktrace(true);
            InfoWindow::Display(_("Access violation"), lines[i]);
            break;
        }
        else if (notifyChange)
            continue;

        // Breakpoint 0 hit
        // >   38:     if (!RegisterClassEx (&wincl))
        else if (reBP.Matches(lines[i]))
        {
            m_ProgramIsStopped = true;
            Log(lines[i]);
            // Code breakpoint / assert
            m_pDBG->BringCBToFront();
            Manager::Get()->GetDebuggerManager()->ShowBacktraceDialog();
            DoBacktrace(true);
            break;
        }
        else if (lines[i].Contains(_T("Break instruction exception")) && !m_pDBG->IsTemporaryBreak())
        {
            m_ProgramIsStopped = true;
        	// Code breakpoint / assert
            m_pDBG->BringCBToFront();
            Manager::Get()->GetDebuggerManager()->ShowBacktraceDialog();
            DoBacktrace(true);
            break;
        }
    }

    if (notifyChange)
        NotifyCursorChanged();

    buffer.Clear();
}

bool CDB_driver::IsDebuggingStarted() const
{
    return m_IsStarted;
}
