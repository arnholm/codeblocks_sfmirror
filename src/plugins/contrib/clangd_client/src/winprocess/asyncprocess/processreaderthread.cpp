// ----------------------------------------------------------------------------
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : worker_thread.cpp
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
// ----------------------------------------------------------------------------
#include "asyncprocess.h"
#include "processreaderthread.h"

//wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_OUTPUT, clProcessEvent);
//wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_STDERR, clProcessEvent);
//wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_TERMINATED, clProcessEvent);
wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_OUTPUT, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_STDERR, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_ASYNC_PROCESS_TERMINATED, wxThreadEvent);

#if defined(__WXGTK__) || defined(__WXMAC__)
#include <sys/wait.h>
#endif

ProcessReaderThread::ProcessReaderThread()
    : wxThread(wxTHREAD_JOINABLE)
    , m_notifiedWindow(NULL)
    , m_process(NULL)
{
    m_suspend.store(false);
    m_is_suspended.store(false);
}

// --------------------------------------------------------------
ProcessReaderThread::~ProcessReaderThread() { m_notifiedWindow = NULL; }
// --------------------------------------------------------------

// --------------------------------------------------------------
void* ProcessReaderThread::Entry()
// --------------------------------------------------------------
{
    while(true) {
        // Did we get a request to terminate?
        if(TestDestroy()) {
            break;
        }

        if(m_suspend.load() && (m_is_suspended.load() == false)) {
            // request to suspend thread, but the status is not suspended
            m_is_suspended.store(true);
        } else if(m_is_suspended.load() && m_suspend.load() == false) {
            // request to resume thread but the status is suspended
            m_is_suspended.store(false);
        }

        if(m_is_suspended.load()) {
            wxThread::Sleep(5);
            continue;
        }

        if(m_process) {
            wxString buff;
            wxString buffErr;
            if(m_process->IsRedirect()) {
                if(m_process->Read(buff, buffErr)) {
                    if(!buff.IsEmpty() || !buffErr.IsEmpty()) {
                        // If we got a callback object, use it
                        if(m_process && m_process->GetCallback()) {
                            m_process->GetCallback()->CallAfter(&IProcessCallback::OnProcessOutput, buff);

                        } else {
                            // We fire an event per data (stderr/stdout)
                            if(!buff.IsEmpty() && m_notifiedWindow) {
                                // fallback to the event system
                                // we got some data, send event to parent
                                //clProcessEvent e(wxEVT_ASYNC_PROCESS_OUTPUT);
                                wxThreadEvent e(wxEVT_ASYNC_PROCESS_OUTPUT, wxID_NONE);
                                //-wxString& b = const_cast<wxString&>(e.GetOutput());
                                //-b.swap(buff);
                                //e.SetProcess(m_process);
                                //m_notifiedWindow->AddPendingEvent(e);
                                //std::string stdstr = buff.ToStdString(); loses data
                                #if wxCHECK_VERSION(3,1,5)
                                std::string stdstr = buff.utf8_string();
                                #else
                                std::string stdstr = buff.ToStdString(wxConvUTF8);
                                #endif
                                e.SetPayload<std::string*>(&stdstr);
                                m_notifiedWindow->ProcessEvent(e);
                            }
                            if(!buffErr.IsEmpty() && m_notifiedWindow) {
                                // we got some data, send event to parent
                                //clProcessEvent e(wxEVT_ASYNC_PROCESS_STDERR);
                                wxThreadEvent e(wxEVT_ASYNC_PROCESS_STDERR, wxID_NONE);
                                //wxString& b = const_cast<wxString&>(e.GetOutput());
                                //b.swap(buffErr);
                                //e.SetProcess(m_process);
                                //m_notifiedWindow->AddPendingEvent(e);
                                //-std::string stdstr = buffErr.ToStdString(); loses data
                                #if wxCHECK_VERSION(3,1,5)
                                std::string stdstr = buff.utf8_string();
                                #else
                                std::string stdstr =  buff.ToStdString(wxConvUTF8);
                                #endif
                                e.SetPayload<std::string*>(&stdstr);
                                m_notifiedWindow->ProcessEvent(e);
                            }
                        }
                    }
                } else {

                    // Process terminated, exit
                    // If we got a callback object, use it
                    NotifyTerminated();
                    break;
                }
            } else {
                // Check if the process is alive
                if(!m_process->IsAlive()) {
                    // Notify about termination
                    NotifyTerminated();
                    break;
                } else {
                    wxThread::Sleep(5);
                }
            }
        } else {
            // No process??
            break;
        }
    }
    m_process = NULL;
    return NULL;
}

void ProcessReaderThread::Stop()
{
    // Notify the thread to exit and
    // wait for it
    if(IsAlive()) {
        Delete(NULL, wxTHREAD_WAIT_BLOCK);
    } else {
        Wait(wxTHREAD_WAIT_BLOCK);
    }
}

// --------------------------------------------------------------
void ProcessReaderThread::Start(int priority)
// --------------------------------------------------------------
{
    Create();
    SetPriority(priority);
    Run();
}

// --------------------------------------------------------------
void ProcessReaderThread::NotifyTerminated()
// --------------------------------------------------------------
{
    // Process terminated, exit
    // If we got a callback object, use it
    if(m_process && m_process->GetCallback()) {
        m_process->GetCallback()->CallAfter(&IProcessCallback::OnProcessTerminated);

    } else {
        // fallback to the event system
        //clProcessEvent e(wxEVT_ASYNC_PROCESS_TERMINATED);
        wxThreadEvent e(wxEVT_ASYNC_PROCESS_TERMINATED);
        //e.SetProcess(m_process);
        if(m_notifiedWindow) {
            //m_notifiedWindow->AddPendingEvent(e);
            m_notifiedWindow->ProcessEvent(e);
        }
    }
}

void ProcessReaderThread::Suspend()
{
    m_suspend.store(true);
    // wait until we make sure that the reader thread is suspended
    while(true) {
        if(m_is_suspended.load()) {
            break;
        }
        wxThread::Sleep(1);
    }
}

void ProcessReaderThread::Resume()
{
    m_suspend.store(false);
    // wait until we make sure that the reader thread is resumed
    while(true) {
        if(!m_is_suspended.load()) {
            break;
        }
        wxThread::Sleep(1);
    }
}
// ----------------------------------------------------------------------------
// The original CodeLite read code (for reference) 2023/01/26
// An important fact I did not understand was that "b.Swap(buff)" was actually
// converting the data to utf8 only if the native character set was set to utf8.
// ----------------------------------------------------------------------------
//void* ProcessReaderThread::Entry()
//{
//    while(true) {
//        // Did we get a request to terminate?
//        if(TestDestroy()) {
//            break;
//        }
//
//        if(m_suspend.load() && (m_is_suspended.load() == false)) {
//            // request to suspend thread, but the status is not suspended
//            m_is_suspended.store(true);
//        } else if(m_is_suspended.load() && m_suspend.load() == false) {
//            // request to resume thread but the status is suspended
//            m_is_suspended.store(false);
//        }
//
//        if(m_is_suspended.load()) {
//            wxThread::Sleep(5);
//            continue;
//        }
//
//        if(m_process) {
//            wxString buff;
//            wxString buffErr;
//            if(m_process->IsRedirect()) {
//                if(m_process->Read(buff, buffErr)) {
//                    if(!buff.IsEmpty() || !buffErr.IsEmpty()) {
//                        // If we got a callback object, use it
//                        if(m_process && m_process->GetCallback()) {
//                            m_process->GetCallback()->CallAfter(&IProcessCallback::OnProcessOutput, buff);
//
//                        } else {
//                            // We fire an event per data (stderr/stdout)
//                            if(!buff.IsEmpty() && m_notifiedWindow) {
//                                // fallback to the event system
//                                // we got some data, send event to parent
//                                clProcessEvent e(wxEVT_ASYNC_PROCESS_OUTPUT);
//                                wxString& b = const_cast<wxString&>(e.GetOutput());
//                                b.swap(buff);
//                                e.SetProcess(m_process);
//                                m_notifiedWindow->AddPendingEvent(e);
//                            }
//                            if(!buffErr.IsEmpty() && m_notifiedWindow) {
//                                // we got some data, send event to parent
//                                clProcessEvent e(wxEVT_ASYNC_PROCESS_STDERR);
//                                wxString& b = const_cast<wxString&>(e.GetOutput());
//                                b.swap(buffErr);
//                                e.SetProcess(m_process);
//                                m_notifiedWindow->AddPendingEvent(e);
//                            }
//                        }
//                    }
//                } else {
//
//                    // Process terminated, exit
//                    // If we got a callback object, use it
//                    NotifyTerminated();
//                    break;
//                }
//            } else {
//                // Check if the process is alive
//                if(!m_process->IsAlive()) {
//                    // Notify about termination
//                    NotifyTerminated();
//                    break;
//                } else {
//                    wxThread::Sleep(5);
//                }
//            }
//        } else {
//            // No process??
//            break;
//        }
//    }
//    m_process = NULL;
//    return NULL;
//}
