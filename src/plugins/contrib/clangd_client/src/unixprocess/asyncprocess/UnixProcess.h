//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : fileutils.h
//
// -------------------------------------------------------------------------
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
#ifndef UNIX_PROCESS_H
#define UNIX_PROCESS_H

#if defined(__WXGTK__)||defined(__WXOSX__)
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <wx/thread.h>
#include <wx/msgqueue.h>
#include <atomic>
#include <wx/event.h>

// Wrapping pipe in a class makes sure they are closed when we leave scope
// --------------------------------------------------------------
class CPipe
// --------------------------------------------------------------
{
private:
    int fd[2];

public:
    const inline int read_fd() const { return fd[0]; }
    const inline int write_fd() const { return fd[1]; }
    CPipe() { pipe(fd); }
    void close()
    {
        ::close(fd[0]);
        ::close(fd[1]);
    }
    ~CPipe() { close(); }
};

// --------------------------------------------------------------
class UnixProcess : public wxEvtHandler
// --------------------------------------------------------------
{
private:
    CPipe m_childStdin;
    CPipe m_childStdout;
    CPipe m_childStderr;
    std::thread* m_writerThread = nullptr;
    std::thread* m_readerThread = nullptr;
    wxMessageQueue<std::string> m_outgoingQueue;
    std::atomic_bool m_goingDown;
    wxEvtHandler* m_owner = nullptr;

protected:
    // sync operations
    static bool ReadAll(int fdOut, int fdErr, std::string& content, int timeoutMilliseconds, bool& isReadFromFdOut);
    static bool Write(int fd, const std::string& message, std::atomic_bool& shutdown);

    void StartWriterThread();
    void StartReaderThread();

public:
    int child_pid = -1;

    UnixProcess(wxEvtHandler* owner, const wxArrayString& args);
    ~UnixProcess();

    int GetPid(){return (child_pid > 0 ? child_pid : 0);}

    // wait for process termination
    int Wait();

    // Async calls with callbacks
    void Write(const std::string& message);

    // stop the running process
    void Stop();

    /**
     * @brief stop sending events from the process
     */
    void Detach();
};
#endif // defined(__WXGTK__)||defined(__WXOSX__)
#endif // UNIX_PROCESS_H
