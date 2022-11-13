//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : UnixProcess.cpp
//
// -------------------------------------------------------------------------
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Modified for CodeBlocks

#include "UnixProcess.h"
#if defined(__WXGTK__) || defined(__WXOSX__)
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>

#include <wx/msgdlg.h>

#include "unixprocess/asyncThreadTypes.h"
#include <unixprocess/fileutils.h>

// --------------------------------------------------------------
UnixProcess::UnixProcess(wxEvtHandler* owner, const wxArrayString& args)
// --------------------------------------------------------------
    : m_owner(owner)
{
    m_goingDown.store(false);
    child_pid = fork();
    if(child_pid == -1)
        {
            //clERROR() << _("Failed to start child process") << strerror(errno);
            wxString clerror("Failed to start child process:"); clerror << strerror(errno);
            wxMessageBox(clerror, "UnixProcess");
        }
    if(child_pid == 0) {
        // In child process
        dup2(m_childStdin.read_fd(), STDIN_FILENO);
        dup2(m_childStdout.write_fd(), STDOUT_FILENO);
        dup2(m_childStderr.write_fd(), STDERR_FILENO);
        m_childStdin.close();
        m_childStdout.close();
        m_childStderr.close();

        char** argv = new char*[args.size() + 1];
        for(size_t i = 0; i < args.size(); ++i) {
            std::string cstr_arg = FileUtils::ToStdString(args[i]);
            argv[i] = new char[cstr_arg.length() + 1];
            strcpy(argv[i], cstr_arg.c_str());
            argv[i][cstr_arg.length()] = 0;
        }
        argv[args.size()] = 0;

        //wxString args2clangd; // **Debugging**
        //for (unsigned ii=0; ii<args.GetCount(); ++ii)
        //    args2clangd += wxString(argv[ii]) + ".";
        //wxMessageBox(args2clangd, "args2Clangd");

        int result = execvp(argv[0], const_cast<char* const*>(argv));
        int errNo = errno;
        if(result == -1)
        {
            // Note: no point writing to stdout here, it has been redirected
            wxString error("Error: Failed to launch program");
            for (unsigned ii=0; ii<args.GetCount(); ++ii)
                error << args[ii];
            error << "." << strerror(errNo);
            wxMessageBox(error, "UnixProcess Launch error");
            exit(EXIT_FAILURE);
        }
    } else {
        //
        // parent process
        // Closing these causes the second load of any project in a workspace
        // to issue a msgbox "error 9: bad file descriptor for start_here.zip file."  //(ph 2022/08/13)
        //-close(m_childStdin.read_fd());
        //-close(m_childStdout.write_fd());
        //-close(m_childStderr.write_fd());

        // Start the reader and writer threads
        StartWriterThread();
        StartReaderThread();
    }
}

// --------------------------------------------------------------
UnixProcess::~UnixProcess()
// --------------------------------------------------------------
{
    Detach();

    // Kill the child process (if it is still alive)
    Stop();
    Wait();
}

// --------------------------------------------------------------
bool UnixProcess::ReadAll(int fd, std::string& content, int timeoutMilliseconds)
// --------------------------------------------------------------
{
    fd_set rset;
    char buff[1024];
    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    int seconds = timeoutMilliseconds / 1000;
    int ms = timeoutMilliseconds % 1000;

    struct timeval tv = { seconds, ms * 1000 }; //  10 milliseconds timeout
    int rc = ::select(fd + 1, &rset, nullptr, nullptr, &tv);
    if(rc > 0) {
        memset(buff, 0, sizeof(buff));
        if(read(fd, buff, (sizeof(buff) - 1)) > 0) {
            content.append(buff);
            return true;
        }
    } else if(rc == 0) {
        // timeout
        return true;
    }
    // error
    return false;
}

// --------------------------------------------------------------
bool UnixProcess::Write(int fd, const std::string& message, std::atomic_bool& shutdown)
// --------------------------------------------------------------
{
    int bytes = 0;
    std::string tmp = message;
    const int chunkSize = 4096;
    while(!tmp.empty() && !shutdown.load()) {
        errno = 0;
        bytes = ::write(fd, tmp.c_str(), tmp.length() > chunkSize ? chunkSize : tmp.length());
        int errCode = errno;
        if(bytes < 0) {
            if((errCode == EWOULDBLOCK) || (errCode == EAGAIN)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else if(errCode == EINTR) {
                continue;
            } else {
                break;
            }
        } else if(bytes) {
            tmp.erase(0, bytes);
        }
    }
    //clDEBUG() << "Wrote message of size:" << message.length();
    return tmp.empty();
}

// --------------------------------------------------------------
int UnixProcess::Wait()
// --------------------------------------------------------------
{
    int status = 0;
    waitpid(child_pid, &status, WNOHANG);
    return WEXITSTATUS(status);
}

// --------------------------------------------------------------
void UnixProcess::Stop()
// --------------------------------------------------------------
{
    if(child_pid != wxNOT_FOUND) { ::kill(child_pid, SIGTERM); }
}

// --------------------------------------------------------------
void UnixProcess::Write(const std::string& message)
// --------------------------------------------------------------
{
    if(not m_writerThread) { return; }
    m_outgoingQueue.Post(message);
}

// --------------------------------------------------------------
void UnixProcess::StartWriterThread()
// --------------------------------------------------------------
{
    m_writerThread = new std::thread(
        [](UnixProcess* process, int fd) {
            while(!process->m_goingDown.load()) {
                std::string buffer;
                if(process->m_outgoingQueue.ReceiveTimeout(10, buffer) == wxMSGQUEUE_NO_ERROR) {
                    UnixProcess::Write(fd, buffer, std::ref(process->m_goingDown));
                }
            }
            //clDEBUG() << "UnixProcess writer thread: going down";
        },
        this, m_childStdin.write_fd());
}

// --------------------------------------------------------------
void UnixProcess::StartReaderThread()
// --------------------------------------------------------------
{
    m_readerThread = new std::thread(
        [](UnixProcess* process, int stdoutFd, int stderrFd) {
            while(not process->m_goingDown.load()) {
                std::string content;
                if(not ReadAll(stdoutFd, content, 10)) {
                    wxThreadEvent evt(wxEVT_ASYNC_PROCESS_TERMINATED);
                    process->m_owner->ProcessEvent(evt);
                    break;
                } else if(not content.empty()) {
                    wxThreadEvent evt(wxEVT_ASYNC_PROCESS_OUTPUT);
                    evt.SetPayload<std::string*>(&content);
                    process->m_owner->ProcessEvent(evt);
                }
                content.clear();
                if(not ReadAll(stderrFd, content, 10)) {
                    wxThreadEvent evt(wxEVT_ASYNC_PROCESS_TERMINATED);
                    process->m_owner->ProcessEvent(evt);
                    break;
                } else if(not content.empty()) {
                    wxThreadEvent evt(wxEVT_ASYNC_PROCESS_STDERR);
                    evt.SetPayload<std::string*>(&content);
                    process->m_owner->ProcessEvent(evt);
                }
            }
            //clDEBUG() << "UnixProcess reader thread: going down";
        },
        this, m_childStdout.read_fd(), m_childStderr.read_fd());
}

// --------------------------------------------------------------
void UnixProcess::Detach()
// --------------------------------------------------------------
{
    m_goingDown.store(true);
    if(m_writerThread) {
        m_writerThread->join();
        wxDELETE(m_writerThread);
    }
    if(m_readerThread) {
        m_readerThread->join();
        wxDELETE(m_readerThread);
    }
}

#endif // OSX & GTK
