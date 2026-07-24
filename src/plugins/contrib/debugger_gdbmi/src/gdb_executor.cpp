#include "gdb_executor.h"

#include <cassert>

#include <sdk.h>
#include <cbproject.h>
#include <cbplugin.h>
#include <loggers.h>
#include <logmanager.h>
#include <manager.h>
#include <debuggermanager.h>
#include <compiler.h>
#include <compilerfactory.h>
#include <pipedprocess.h>

#include "helpers.h"

namespace
{
// function pointer to DebugBreakProcess under windows (XP+)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0501)
#include "Tlhelp32.h"
typedef BOOL WINAPI   (*DebugBreakProcessApiCall)       (HANDLE);
typedef HANDLE WINAPI (*CreateToolhelp32SnapshotApiCall)(DWORD  dwFlags,   DWORD             th32ProcessID);
typedef BOOL WINAPI   (*Process32FirstApiCall)          (HANDLE hSnapshot, LPPROCESSENTRY32W lppe);
typedef BOOL WINAPI   (*Process32NextApiCall)           (HANDLE hSnapshot, LPPROCESSENTRY32W lppe);

DebugBreakProcessApiCall        DebugBreakProcessFunc = 0;
CreateToolhelp32SnapshotApiCall CreateToolhelp32SnapshotFunc = 0;
Process32FirstApiCall           Process32FirstFunc = 0;
Process32NextApiCall            Process32NextFunc = 0;

HINSTANCE kernelLib = 0;


void InitDebuggingFuncs()
{
    // get a function pointer to DebugBreakProcess under windows (XP+)
    kernelLib = LoadLibrary(TEXT("kernel32.dll"));
    if (kernelLib)
    {
        //-DebugBreakProcessFunc = (DebugBreakProcessApiCall)GetProcAddress(kernelLib, "DebugBreakProcess");
        DebugBreakProcessFunc = (DebugBreakProcessApiCall)(void*)GetProcAddress(kernelLib, "DebugBreakProcess");
        // Windows XP compatibility
        DebugBreakProcessFunc = (DebugBreakProcessApiCall)(void*)GetProcAddress(kernelLib, "DebugBreakProcess");
        CreateToolhelp32SnapshotFunc = (CreateToolhelp32SnapshotApiCall)(void*)GetProcAddress(kernelLib, "CreateToolhelp32Snapshot");
        Process32FirstFunc = (Process32FirstApiCall)(void*)GetProcAddress(kernelLib, "Process32First");
        Process32NextFunc = (Process32NextApiCall)(void*)GetProcAddress(kernelLib, "Process32Next");
    }
}

void FreeDebuggingFuncs()
{
    if (kernelLib)
        FreeLibrary(kernelLib);
}
#else
void InitDebuggingFuncs()
{
}
void FreeDebuggingFuncs()
{
}
#endif
////
////void InterruptChild(int child_pid)
////{
////#ifndef __WXMSW__
////    wxKillError error;
////    wxKill(child_pid, wxSIGINT, &error);
////#else //is __WXMSW__
////    if (DebugBreakProcessFunc)
////    {
////        HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)child_pid);
////        if (proc)
////        {
////            DebugBreakProcessFunc(proc); // yay!
////            CloseHandle(proc);
////        }
////    }
////#endif
////}

void GetChildPIDs(int parent, std::vector<int> &childs)
{
#ifndef __WXMSW__
    const char *c_proc_base = "/proc";

    DIR *dir = opendir(c_proc_base);
    if(not dir)
        return;

    struct dirent *entry;
    do
    {
        entry = readdir(dir);
        if(entry)
        {
            int pid = atoi(entry->d_name);
            if(pid != 0)
            {
                char filestr[PATH_MAX + 1];
                snprintf(filestr, PATH_MAX, "%s/%d/stat", c_proc_base, pid);

                FILE *file = fopen(filestr, "r");
                if(file)
                {
                    char line[101];
                    fgets(line, 100, file);
                    fclose(file);

                    int ppid = dbg_mi::ParseParentPID(line);
                    if(ppid == parent)
                        childs.push_back(pid);
                }
            }
        }
    } while(entry);

    closedir(dir);
#else //is __WXMSW__
    if((CreateToolhelp32SnapshotFunc!=NULL) && (Process32FirstFunc!=NULL) && (Process32NextFunc!=NULL) )
    {
        HANDLE snap = CreateToolhelp32SnapshotFunc(TH32CS_SNAPALL,0);
        if (snap!=INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 lppe;
            lppe.dwSize = sizeof(PROCESSENTRY32);
            BOOL ok = Process32FirstFunc(snap, &lppe);
            while ( ok == TRUE)
            {
                if (lppe.th32ParentProcessID == (DWORD)parent) // Have my Child...
                {
                    childs.push_back(lppe.th32ProcessID);
                }
                lppe.dwSize = sizeof(PROCESSENTRY32);
                ok = Process32NextFunc(snap, &lppe);
            }
            CloseHandle(snap);
        }
    }
#endif
}

} //namespace end

namespace dbg_mi
{

void LogPaneLogger::Log(wxString const &line, Log::Type type)
{
    if (m_shutdowned)
        return;

    //int index; wxUnusedVar(index);

    switch (type)
    {
        case Log::Normal:
            m_plugin->Log(line, ::Logger::info);
            break;
        case Log::Error:
            m_plugin->Log(line, ::Logger::error);
            break;
    }
}
/***
All output sequences end in a single line containing a period.
The token is from the corresponding request. If an execution command is interrupted
    by the `-exec-interrupt' command, the token associated with the `*stopped'
    message is the one of the original execution command, not the one of the interrupt
    command.
status-async-output contains on-going status information about the progress of a
    slow operation. It can be discarded. All status output is prefixed by `+'.
exec-async-output contains asynchronous state change on the target
    (stopped, started, disappeared). All async output is prefixed by `*'.
notify-async-output contains supplementary information that the client should
    handle (e.g., a new breakpoint information). All notify output is prefixed by `='.
console-stream-output is output that should be displayed as is in the console.
    It is the textual response to a CLI command. All the console output is prefixed by `~'.
target-stream-output is the output produced by the target program. All the target
    output is prefixed by `@'.
log-stream-output is output text coming from GDB's internals, for instance messages
    that should be displayed as part of an error log. All the log output is prefixed by `&'.
New GDB/MI commands should only output lists containing values.

GDB/MI Output Records
GDB/MI Result Records
GDB/MI Stream Records
GDB/MI Out-of-band Records
GDB/MI Result Records
In addition to a number of out-of-band notifications, the response to a GDB/MI
    command includes one of the following result indications:

"^done" [ "," results ]
The synchronous operation was successful, results are the return values.

"^running"
The asynchronous operation was successfully started. The target is running.

"^error" "," c-string
The operation failed. The c-string contains the corresponding error message.

GDB/MI Stream Records
GDB internally maintains a number of output streams: the console, the target, and
    the log. The output intended for each of these streams is funneled through the
    GDB/MI interface using stream records.

Each stream record begins with a unique prefix character which identifies its stream
    (see section GDB/MI Output Syntax). In addition to the prefix, each stream record
    contains a string-output. This is either raw text (with an implicit new line) or a
    quoted C string (which does not contain an implicit newline).

"~" string-output
The console output stream contains text that should be displayed in the CLI console
    window. It contains the textual responses to CLI commands.

"@" string-output
The target output stream contains any textual output from the running target.

"&" string-output
The log stream contains debugging messages being produced by GDB's internals.

GDB/MI Out-of-band Records
Out-of-band records are used to notify the GDB/MI client of additional changes that
    have occurred. Those changes can either be a consequence of GDB/MI (e.g.,
    a breakpoint modified) or a result of target activity (e.g., target stopped).

The following is a preliminary list of possible out-of-band records.
"*" "stop"

***/
// ----------------------------------------------------------------------------
void LogPaneLogger::Debug(wxString const &line, Line::Type type)
// ----------------------------------------------------------------------------
{
    if (m_shutdowned)
        return;

    ::Logger::level cmdLevel  = ::Logger::warning;  //use the blue color
    ::Logger::level cmdResult = ::Logger::warning;  //use the black color

    switch (type)
    {
        case Line::Debug:
            m_plugin->DebugLog(line, ::Logger::info);
            break;
        case Line::Unknown:
            m_plugin->DebugLog(line, ::Logger::info);
            break;
        case Line::Command:
            m_plugin->DebugLog(line, cmdLevel); //blue  //(ph 2024/04/12)
            break;
        case Line::CommandResult:
            m_plugin->DebugLog(line, cmdResult); //blue = //(ph 2024/04/12)
            break;
        case Line::ProgramState:
            m_plugin->DebugLog(line, ::Logger::critical);
            break;
        case Line::Error:
            m_plugin->DebugLog(line, ::Logger::error); //red & //(ph 2024/04/12)
            break;
    }
}

GDBExecutor::GDBExecutor() :
    m_pPipedProcess(NULL),
    m_pid(-1),
    m_child_pid(-1),
    m_attached_pid(-1),
    m_stopped(true),
    m_interupting(false),
    m_temporary_interupt(false)
{
    InitDebuggingFuncs();
}

GDBExecutor::~GDBExecutor()
{
    FreeDebuggingFuncs();
}

int GDBExecutor::LaunchProcess(wxString const &cmd, wxString const& cwd, int id_gdb_process,
                               wxEvtHandler *event_handler, Logger &logger)
{
    if(m_pPipedProcess)
        return -1;

    // start the gdb process
    m_pPipedProcess = new PipedProcess(&m_pPipedProcess, event_handler, id_gdb_process, true, cwd);
    logger.Log(_("Starting debugger: "));
    logger.Debug(_T("Executing command: ") + cmd);
    m_pid = wxExecute(cmd, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, m_pPipedProcess);
    //m_pid = wxExecute(cmd, wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER | wxEXEC_NOHIDE, m_process);
    m_child_pid = -1;

#ifdef __WXMAC__
    if (m_pid == -1)
        logger.Log(_("debugger has fake macos PID"), Logger::Log::Error);
#endif

    if (not m_pid)
    {
        delete m_pPipedProcess;
        m_pPipedProcess = 0;
        logger.Log(_("failed"), Logger::Log::Error);
        return -1;
    }
    else if (not m_pPipedProcess->GetOutputStream())
    {
        delete m_pPipedProcess;
        m_pPipedProcess = 0;
        logger.Log(_("failed (to get debugger's stdin)"), Logger::Log::Error);
        return -2;
    }
    else if (not m_pPipedProcess->GetInputStream())
    {
        delete m_pPipedProcess;
        m_pPipedProcess = 0;
        logger.Log(_("failed (to get debugger's stdout)"), Logger::Log::Error);
        return -2;
    }
    else if (not m_pPipedProcess->GetErrorStream())
    {
        delete m_pPipedProcess;
        m_pPipedProcess = 0;
        logger.Log(_("failed (to get debugger's stderr)"), Logger::Log::Error);
        return -2;
    }
    logger.Log(_("done"));

    return 0;
}

long GDBExecutor::GetChildPID()
{
    if(m_pid <= 0)
        m_child_pid = -1;
    else if(m_child_pid <= 0)
    {
        std::vector<int> children;
        GetChildPIDs(m_pid, children);

        if(children.size() != 0)
        {
            if(children.size() > 1)
                Manager::Get()->GetLogManager()->Log(_T("the debugger has more that one child"));
            m_child_pid = children.front();
        }
    }

    return m_child_pid;
}


bool GDBExecutor::ProcessHasInput()
{
    return m_pPipedProcess && m_pPipedProcess->HasInput();
}

bool GDBExecutor::IsRunning() const
{
    return m_pPipedProcess;
}

void GDBExecutor::Stopped(bool flag)
{
    if(m_logger)
    {
        if(flag)
            //-m_logger->Debug(_T("Executor stopped(ready)"));
            m_logger->Debug(wxString::Format(_T("Executor stopped(ready)(Q %sEmpty)"),IsActionsMapEmpty()?_T(""):_T("Not ")));
        else
            m_logger->Debug(_T("Executor started(busy)"));
    }
    m_stopped = flag;
    if(flag)
        m_interupting = false;
    else
        m_temporary_interupt = false;
}

// ----------------------------------------------------------------------------
void GDBExecutor::Interrupt(bool temporary)
// ----------------------------------------------------------------------------
{
    if(not IsRunning() || IsStopped())
        return;

    if(m_logger)
        m_logger->Debug(_T("Interrupting debugger"));

    // FIXME (pecam#): do something similar for the windows platform
    // non-windows gdb can interrupt the running process. yay!
    if(m_pid <= 0) // look out for the "fake" PIDs (killall)
    {
        cbMessageBox(_("Unable to stop the debug process!"), _("Debugger:Error"), wxOK | wxICON_WARNING, Manager::Get()->GetAppWindow());
        return;
    }
    else
    {
        m_temporary_interupt = temporary;
        m_interupting = true;


        if (m_attached_pid > 0)
            InterruptChild(m_attached_pid);
        else
        {
            GetChildPID();
            if (m_child_pid > 0)
                InterruptChild(m_child_pid);
        }

        return;
    }
}
// ----------------------------------------------------------------------------
void GDBExecutor::ForceStop()
// ----------------------------------------------------------------------------
{
    if(not IsRunning())
        return;

    // FIXME (pecan#): do something similar for the windows platform
    // non-windows idb can interrupt the running process. yay!

    if(m_pid <= 0) // look out for the "fake" PIDs (killall)
    {
        cbMessageBox(_("Unable to stop the debug process!"), _("Debugger:Error"), wxOK | wxICON_WARNING, Manager::Get()->GetAppWindow());
        return;
    }
    else
    {
        Interrupt(false);

        if (m_attached_pid > 0)
            Execute(_T("kill"));

        Execute(wxT("-gdb-exit")); //should be -gdb-exit

        // Wait max 12 seconds for idbEngine & idbShell to terminate            //changed from 125 to 12
        int waitMaxSecs = (12 << 1);

        waitMaxSecs = 3<<1;
        for (int ii=0; ii<waitMaxSecs; ++ii)
        {
            if (IsProcessRunning(m_pid))
            {
                if (ii and ( ii % 2 == 0 ))
                {
                    cbDebuggerPlugin *dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
                    m_logger->Log(wxString::Format(_T("(%s) CodeBlocks awaiting pod termination acknowledgment. (%d)"),dbg->GetGUIName().c_str(), ii>>1));
                }

                wxMilliSleep(500);
                Manager::Yield();
            }
            else
                break;
        }
        // Kill idbEngine & idbShell; they must be hung up
        if (IsProcessRunning(m_pid))
        {
            if (m_pPipedProcess)
            {

                cbDebuggerPlugin *dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
                m_logger->Debug(wxString::Format(_T("(%s) CodeBlocks force killed zombie debugger engine."),dbg->GetGUIName().c_str()));

                m_pPipedProcess->CloseOutput();
                //m_process->Detach(); // avoid cb & wx events when closing down, else crashes
            }
            wxProcess::Kill(m_pid, wxSIGKILL);
            m_pid = 0;
        }

        return;
    }
}

wxString GDBExecutor::GetOutput()
{
    assert(false);
    return wxEmptyString;
}

bool GDBExecutor::DoExecute(dbg_mi::CommandID const &id, wxString const &cmd)
{
    if(not m_pPipedProcess)
        return false;
    if(not m_stopped && m_logger)
    {
        m_logger->Debug(wxString::Format(_T("GDBExecutor is not stopped, but command (%s) was executed!"),
                                         cmd.c_str())
                        );
    }
    // Note: SendString validates or adds \n.
    m_pPipedProcess->SendString(id.ToString() + cmd);
    return true;
}
void GDBExecutor::DoClear()
{
    m_stopped = true;
    if (m_pPipedProcess)
        delete m_pPipedProcess;
    m_pPipedProcess = NULL;
}
// ----------------------------------------------------------------------------
void GDBExecutor::InterruptChild(int child_pid)
// ----------------------------------------------------------------------------
{
#ifndef __WXMSW__   //Linux
    wxKillError error;
    //-wxKill(child_pid, wxSIGINT, &error);
    wxKill(child_pid, wxSIGINT, &error, wxKILL_CHILDREN); //ticket 1571
#else //is __WXMSW__//Windows
    // note - There's no child_pid for a remote process
    if (DebugBreakProcessFunc)
    {
        GetLogger()->Debug(_("Trying to pause the running process..."));
        HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)child_pid);
        if (proc)
        {
            DebugBreakProcessFunc(proc); // yay!
            CloseHandle(proc);
        }
        else
        {
            GetLogger()->Debug(_("Failed using DebugBreakProcessFunc."));
            if (child_pid > 0)
            {
                if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0) == 0)
                {
                    GetLogger()->Debug(wxT("GenerateConsoleCtrlEvent failed."));
                    GetLogger()->Debug(wxT("Interrupting debugger failed."));
                    //-SetRunActionMsg(_T("Interrupting debugger failed."), _T("Error"), errInfo );
                    return;
                }
            }
        }

    }
#endif
}

} // namespace dbg_mi
