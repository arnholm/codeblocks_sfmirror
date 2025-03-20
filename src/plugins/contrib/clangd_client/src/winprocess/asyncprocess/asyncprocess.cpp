// ----------------------------------------------------------------------------
//
// copyright            : (C) 2014 Eran Ifrah
// file name            : asyncprocess.cpp
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
// ----------------------------------------------------------------------------

class wxEvtHandler;
class IProcess;

//#include "asyncprocess.h"
#include "StringUtils.h"
#include "asyncprocess.h"
#include "processreaderthread.h"
#include <wx/arrstr.h>
#include <wx/string.h>

#include "logmanager.h"

#ifdef __WXMSW__
static bool shell_is_cmd = true;
#include "winprocess_impl.h"
#else
static bool shell_is_cmd = false;
#include "unixprocess_impl.h"
#endif
class __AsyncCallback : public wxEvtHandler
{
    std::function<void(const wxString&)> m_cb;
    wxString m_output;

public:
    __AsyncCallback(std::function<void(const wxString&)> cb)
        : m_cb(std::move(cb))
    {
        Bind(wxEVT_ASYNC_PROCESS_TERMINATED, &__AsyncCallback::OnProcessTerminated, this);
        Bind(wxEVT_ASYNC_PROCESS_OUTPUT, &__AsyncCallback::OnProcessOutput, this);
    }
    ~__AsyncCallback()
    {
        Unbind(wxEVT_ASYNC_PROCESS_TERMINATED, &__AsyncCallback::OnProcessTerminated, this);
        Unbind(wxEVT_ASYNC_PROCESS_OUTPUT, &__AsyncCallback::OnProcessOutput, this);
    }

    // --------------------------------------------------------------
    void OnProcessOutput(wxThreadEvent& event)
    // --------------------------------------------------------------
    {
        LogManager* pLogMgr = Manager::Get()->GetLogManager();
        wxString msg = wxString::Format("%s entered. Should be unused.", __FUNCTION__);
        pLogMgr->DebugLogError(msg);
    }

    // --------------------------------------------------------------
    void OnProcessTerminated(wxThreadEvent& event)
    // --------------------------------------------------------------
    {
        LogManager* pLogMgr = Manager::Get()->GetLogManager();
        wxString msg = wxString::Format("%s entered. Should be unused.", __FUNCTION__);
        pLogMgr->DebugLogError(msg);

    }
};

static void __WrapSpacesForShell(wxString& str, size_t flags)
{
    str.Trim().Trim(false);
    auto tmpArgs = StringUtils::BuildArgv(str);
    if(tmpArgs.size() > 1) {
        if(!shell_is_cmd || (flags & IProcessCreateSSH)) {
            // escape any occurances of "
            str.Replace("\"", "\\\"");
        }
        str.Append("\"").Prepend("\"");
    }
}

static wxArrayString __WrapInShell(const wxArrayString& args, size_t flags)
{
    wxArrayString tmparr = args;
    for(wxString& arg : tmparr) {
        __WrapSpacesForShell(arg, flags);
    }

    wxString cmd = wxJoin(tmparr, ' ', 0);
    wxArrayString command;

    bool is_ssh = flags & IProcessCreateSSH;
    if(shell_is_cmd && !is_ssh) {
        wxChar* shell = wxGetenv(wxT("COMSPEC"));
        if(!shell) {
            shell = (wxChar*)wxT("CMD.EXE");
        }
        command.Add(shell);
        command.Add("/C");
        command.Add("\"" + cmd + "\"");

    } else {
        command.Add("/bin/sh");
        command.Add("-c");
        command.Add("'" + cmd + "'");
    }
    return command;
}

static void __FixArgs(wxArrayString& args)
{
    for(wxString& arg : args) {
        // escape LF/CR
        arg.Replace("\n", "");
        arg.Replace("\r", "");
#if defined(__WXOSX__) || defined(__WXGTK__)
        arg.Trim().Trim(false);
        if(arg.length() > 1) {
            if(arg.StartsWith("'") && arg.EndsWith("'")) {
                arg.Remove(0, 1);
                arg.RemoveLast();
            } else if(arg.StartsWith("\"") && arg.EndsWith("\"")) {
                arg.Remove(0, 1);
                arg.RemoveLast();
            }
        }
#endif
    }
}

// --------------------------------------------------------------
IProcess* CreateAsyncProcess(wxEvtHandler* parent, const std::vector<wxString>& args, size_t flags,
                             const wxString& workingDir, const clEnvList_t* env, const wxString& sshAccountName)
// --------------------------------------------------------------
{
    wxArrayString wxargs;
    wxargs.reserve(args.size());
    for(const wxString& s : args) {
        wxargs.Add(s);
    }
    return CreateAsyncProcess(parent, wxargs, flags, workingDir, env, sshAccountName);
}

// --------------------------------------------------------------
IProcess* CreateAsyncProcess(wxEvtHandler* parent, const wxArrayString& args, size_t flags, const wxString& workingDir,
                             const clEnvList_t* env, const wxString& sshAccountName)
// --------------------------------------------------------------
{
    clEnvironment e(env);
    wxArrayString c = args;

    //    clDEBUG1() << "1: CreateAsyncProcess called with:" << c << endl;

    if(flags & IProcessWrapInShell) {
        // wrap the command in OS specific terminal
        c = __WrapInShell(c, flags);
    }

    // needed on linux where fork does not require the extra quoting
    __FixArgs(c);
    //    clDEBUG1() << "2: CreateAsyncProcess called with:" << c << endl;

#ifdef __WXMSW__
    return WinProcessImpl::Execute(parent, c, flags, workingDir);
#else
    return UnixProcessImpl::Execute(parent, c, flags, workingDir);
#endif
}

// --------------------------------------------------------------
IProcess* CreateAsyncProcess(wxEvtHandler* parent, const wxString& cmd, size_t flags, const wxString& workingDir,
                             const clEnvList_t* env, const wxString& sshAccountName)
// --------------------------------------------------------------
{
    auto args = StringUtils::BuildArgv(cmd);
    return CreateAsyncProcess(parent, args, flags, workingDir, env, sshAccountName);
}

// --------------------------------------------------------------
void CreateAsyncProcessCB(const wxString& cmd, std::function<void(const wxString&)> cb, size_t flags,
                          const wxString& workingDir, const clEnvList_t* env)
// --------------------------------------------------------------
{
    clEnvironment e(env);
    CreateAsyncProcess(new __AsyncCallback(std::move(cb)), cmd, flags, workingDir, env, wxEmptyString);
}

// --------------------------------------------------------------
IProcess* CreateSyncProcess(const wxString& cmd, size_t flags, const wxString& workingDir, const clEnvList_t* env)
// --------------------------------------------------------------
{
    return CreateAsyncProcess(nullptr, StringUtils::BuildArgv(cmd), flags | IProcessCreateSync, workingDir, env,
                              wxEmptyString);
}

// Static methods:
std::map<int,int> IProcess::m_ProcessExitCodeMap; //pid,exitcode
// --------------------------------------------------------------
bool IProcess::GetProcessExitCode(int pid, int& exitCode)
// --------------------------------------------------------------
{

    if (m_ProcessExitCodeMap.count(pid))
    {
        exitCode = m_ProcessExitCodeMap[pid];
        return true;
    }
    return false;
}

// --------------------------------------------------------------
void IProcess::SetProcessExitCode(int pid, int exitCode)
// --------------------------------------------------------------
{
    m_ProcessExitCodeMap[pid] = exitCode;
}

// --------------------------------------------------------------
void IProcess::WaitForTerminate(wxString& output)
// --------------------------------------------------------------
{
    if(IsRedirect()) {
        wxString buff;
        wxString buffErr;
        while(Read(buff, buffErr)) {
            output << buff;
            if(!buff.IsEmpty() && !buffErr.IsEmpty()) {
                output << "\n";
            }
            output << buffErr;
        }
    } else {
        // Just wait for the process to terminate in a busy loop
        while(IsAlive()) {
            wxThread::Sleep(10);
        }
    }
}

// --------------------------------------------------------------
void IProcess::SuspendAsyncReads()
// --------------------------------------------------------------
{
    if(m_thr) {
    //        clDEBUG1() << "Suspending process reader thread..." << endl;
        m_thr->Suspend();
    //        clDEBUG1() << "Suspending process reader thread...done" << endl;
    }
}

// --------------------------------------------------------------
void IProcess::ResumeAsyncReads()
// --------------------------------------------------------------
{
    if(m_thr) {
    //        clDEBUG1() << "Resuming process reader thread..." << endl;
        m_thr->Resume();
    //        clDEBUG1() << "Resuming process reader thread..." << endl;
    }
}
