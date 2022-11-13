/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CCLOGGER_H
#define CCLOGGER_H

#include <wx/string.h>
#include <wx/ffile.h>   //(ph 2021/08/30) ExternLogFile
#include <wx/thread.h>

#include <memory> // unique_ptr
#include <chrono>

#include <cbexception.h> // cbAssert
#include <logmanager.h>  // F()
#include <prep.h>        // nullptr

class wxEvtHandler;

#ifndef CC_PROCESS_LOG_EVENT_TO_PARENT
    #define CC_PROCESS_LOG_EVENT_TO_PARENT 0
#endif

#ifdef CC_PARSER_TEST
    #undef CC_PROCESS_LOG_EVENT_TO_PARENT
    #define CC_PROCESS_LOG_EVENT_TO_PARENT 1
#endif

extern bool           g_EnableDebugTrace; //!< Toggles tracing into file.
extern const wxString g_DebugTraceFile;   //!< Trace file name (if above is enabled).
extern long           g_idCCAddToken;
extern long           g_idCCLogger;
extern long           g_idCCErrorLogger;
extern long           g_idCCDebugLogger;
extern long           g_idCCDebugErrorLogger;
extern wxString       s_TokenTreeMutex_Owner;     // location of the last tree lock
extern wxString       s_ParserMutex_Owner;        // location of the last parser lock
extern wxString       m_ClassBrowserBuilderThreadMutex_Owner;

class CCLogger
{
public:
    static CCLogger* Get();

    void Init(wxEvtHandler* parent, int logId, int logErrorId, int debugLogId, int debugLogErrorId, int addTokenId = -1);
    void AddToken(const wxString& msg);

    void Log(const wxString& msg, int id=g_idCCLogger);
    void LogError(const wxString& msg);

    void DebugLog(const wxString& msg, int id=g_idCCDebugLogger);
    void DebugLogError(const wxString& msg);

    bool GetExternalLogStatus(){return m_ExternLogActive;}
    void SetExternalLog(bool OnOrOff);

protected:
    CCLogger();
    virtual ~CCLogger();
    CCLogger(const CCLogger&)            { ; }
    CCLogger& operator=(const CCLogger&) { return *this; }

    // static member variables (instance and critical section for Parser)
    friend class std::default_delete<CCLogger>;
    static std::unique_ptr<CCLogger> s_Inst;

private:
    wxEvtHandler* m_Parent;
    int           m_LogId;
    int           m_LogErrorId;
    int           m_DebugLogId;
    int           m_DebugLogErrorId;
    int           m_AddTokenId;
    bool          m_ExternLogActive;
    int           m_ExternLogPID;
    wxFFile       m_ExternLogFile;

    ConfigManager* m_pCfgMgr = nullptr;
};

// --------Lock debugging ------------------------------------------
// For tracking, either uncomment:
//#define CC_ENABLE_LOCKER_TRACK
// ...or:
// #define CC_ENABLE_LOCKER_ASSERT
// ..or none of the above.
//-------------------------------------------------------------------

// ----------------------------------------------------------------------------
// MUTEX TRACKING
// ----------------------------------------------------------------------------
#if defined(CC_ENABLE_LOCKER_TRACK)
    // [1] Implementations for tracking mutexes:
    #define THREAD_LOCKER_MTX_LOCK(NAME)                                         \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Lock() : %s(), %s, %d"),              \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    #define THREAD_LOCKER_MTX_LOCK_SUCCESS(NAME)                                 \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Lock().Success() : %s(), %s, %d"),    \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    #define THREAD_LOCKER_MTX_UNLOCK(NAME)                                       \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Unlock() : %s(), %s, %d"),            \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    #define THREAD_LOCKER_MTX_UNLOCK_SUCCESS(NAME)                               \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Unlock().Success() : %s(), %s, %d"),  \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    #define THREAD_LOCKER_MTX_FAIL(NAME)                                         \
        CCLogger::Get()->DebugLogError(wxString::Format(_T("%s.Fail() : %s(), %s, %d"),              \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    // ----------------------------------------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK
    // [2] Cumulative convenient macros for tracking mutexes [USE THESE!]:
    // ----------------------------------------------------------------------------
    #define CC_LOCKER_TRACK_TT_MTX_LOCK(M)    \
    {                                         \
        auto locker_result = M.LockTimeout(250); \
        if (locker_result==wxMUTEX_NO_ERROR) \
          THREAD_LOCKER_MTX_LOCK_SUCCESS(M);  \
        else                                  \
          THREAD_LOCKER_MTX_FAIL(M);          \
    }
    // --CC_LOCKER_TRACK_TT_MTX_UNLOCK TRACK-----------
    #define CC_LOCKER_TRACK_TT_MTX_UNLOCK(M)    \
    {                                           \
        if (M.Unlock()==wxMUTEX_NO_ERROR)       \
          THREAD_LOCKER_MTX_UNLOCK_SUCCESS(M);  \
        else                                    \
          THREAD_LOCKER_MTX_FAIL(M);            \
    }
    #define CC_LOCKER_TRACK_CBBT_MTX_LOCK   CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_CBBT_MTX_UNLOCK CC_LOCKER_TRACK_TT_MTX_UNLOCK
    #define CC_LOCKER_TRACK_P_MTX_LOCK      CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_P_MTX_UNLOCK    CC_LOCKER_TRACK_TT_MTX_UNLOCK

    // ----------------------------------------------------------------------------
    // TRACKING CRITICAL SECTIONS
    // [2] Implementations for tracking critical sections:
    // ----------------------------------------------------------------------------
    #define THREAD_LOCKER_CS_ENTER(NAME)                                         \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Enter() : %s(), %s, %d"),             \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    #define THREAD_LOCKER_CS_ENTERED(NAME)                                       \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Entered() : %s(), %s, %d"),           \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    #define THREAD_LOCKER_CS_LEAVE(NAME)                                         \
        CCLogger::Get()->DebugLog(wxString::Format(_T("%s.Leave() : %s(), %s, %d"),             \
                                    wxString(#NAME, wxConvUTF8).wx_str(),        \
                                    wxString(__FUNCTION__, wxConvUTF8).wx_str(), \
                                    wxString(__FILE__, wxConvUTF8).wx_str(),     \
                                    __LINE__))
    // ----------------------------------------------------------------------------
    //  TRACKING CRITICAL SECTIONS
    // [2] Cumulative convenient macros for tracking critical sections [USE THESE!]:
    // ----------------------------------------------------------------------------
    #define CC_LOCKER_TRACK_CS_ENTER(CS) \
    {                                    \
         THREAD_LOCKER_CS_ENTER(CS);     \
         CS.Enter();                     \
         THREAD_LOCKER_CS_ENTERED(CS);   \
    }
    #define CC_LOCKER_TRACK_CS_LEAVE(CS) \
    {                                    \
          THREAD_LOCKER_CS_LEAVE(CS);    \
          CS.Leave();                    \
    }
// ----------------------------------------------------------------------------
//  MUTEX LOCK ASSERTS
// ----------------------------------------------------------------------------
#elif defined CC_ENABLE_LOCKER_ASSERT
    #define CC_LOCKER_TRACK_CS_ENTER(CS)     CS.Enter();
    #define CC_LOCKER_TRACK_CS_LEAVE(CS)     CS.Leave();

    // ----------------------------------------------------------------------------
    //  UNUSED -- 2021/09/8
    // ----------------------------------------------------------------------------
    #define CC_LOCKER_TRACK_TT_MTX_LOCK_UNUSED(M)      \
        do {                                    \
            auto locker_result = M.LockTimeout(250);   \
            if (locker_result != wxMUTEX_NO_ERROR)  \
            {   wxString err = wxString::Format("LOCK FAILED: Owner: %s", M##_Owner); \
                err.Printf(_T("Assertion failed in %s at %s:%d.\n\n%s"), __FUNCTION__, __FILE__, __LINE__, err); \
                wxSafeShowMessage(_T("Assertion error"), err);  \
            }                                                   \
            cbAssert(locker_result==wxMUTEX_NO_ERROR);          \
            M##_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/  \
        } while (false);
    // ----------------------------------------------------------------------------
    //  Assert on failed lockTimeout
    // ----------------------------------------------------------------------------
    #define CC_LOCKER_TRACK_TT_MTX_LOCK(M)      \
        do {                                    \
            auto locker_result = M.LockTimeout(250);   \
            if (locker_result != wxMUTEX_NO_ERROR)  \
            {   wxString err1st = wxString::Format("Owner: %s", M##_Owner); \
                wxString err; \
                err.Printf(_T("LockTimeout() failed in %s at %s:%d \n\t%s"), __FUNCTION__, __FILE__, __LINE__, err1st); \
                CCLogger::Get()->DebugLogError(wxString("Lock error") + err);  \
                /* wxSafeShowMessage(_T("Assertion error"), err); */  \
                locker_result = M.Lock(); /* block anyway*/ \
                cbAssert(locker_result==wxMUTEX_NO_ERROR); /*assert if blocking lock fails*/ \
                M##_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/  \
            } \
            else /*lock succeeded, record new owner*/ \
                M##_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); \
        } while (false);

    // ----------------------------------------------------------------------------
    // assert on failed UNLOCK
    // ----------------------------------------------------------------------------
    #define CC_LOCKER_TRACK_TT_MTX_UNLOCK(M)    \
        do {                                    \
            auto locker_result = M.Unlock();       \
            if (locker_result != wxMUTEX_NO_ERROR)  \
            {   wxString err1st = wxString::Format("Last Owner: %s", M##_Owner); \
                wxString err; \
                err.Printf(_T("UNLOCK failed in %s at %s:%d \n\t%s"), __FUNCTION__, __FILE__, __LINE__, err1st); \
                CCLogger::Get()->DebugLogError(wxString("UnLock error") + err);  \
                wxSafeShowMessage(_T("Assertion error"), err);  \
                cbAssert(locker_result==wxMUTEX_NO_ERROR); /*assert if unlock fails*/ \
            } \
            else M##_Owner = "NONE"; /*record owner*/  \
        } while (false);

    #define CC_LOCKER_TRACK_CBBT_MTX_LOCK    CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_CBBT_MTX_UNLOCK  CC_LOCKER_TRACK_TT_MTX_UNLOCK
    #define CC_LOCKER_TRACK_P_MTX_LOCK       CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_P_MTX_UNLOCK     CC_LOCKER_TRACK_TT_MTX_UNLOCK
// ----------------------------------------------------------------------------
// Neither MUTEX TRACK or ASSERT (ie, release code)
// ----------------------------------------------------------------------------
#else
    #define CC_LOCKER_TRACK_CS_ENTER(CS)     CS.Enter();
    #define CC_LOCKER_TRACK_CS_LEAVE(CS)     CS.Leave();

    //#define CC_LOCKER_TRACK_TT_MTX_LOCK(M)   M.LockTimeout(250);
    //#define CC_LOCKER_TRACK_TT_MTX_LOCK(M)   M.Lock();
    #define CC_LOCKER_TRACK_TT_MTX_LOCK(M)      \
        do {                                    \
            auto locker_result = M.Lock();   \
            if (locker_result != wxMUTEX_NO_ERROR)  \
            {   wxString err1st = wxString::Format("Owner: %s", M##_Owner); \
                wxString err; \
                err.Printf(_T("Lock() failed in %s at %s:%d \n\t%s"), __FUNCTION__, __FILE__, __LINE__, err1st); \
                CCLogger::Get()->DebugLogError(wxString("Lock error") + err);  \
            } \
            else /*lock succeeded, record new owner*/ \
                M##_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); \
        } while (false);

    #define CC_LOCKER_TRACK_TT_MTX_UNLOCK(M) M.Unlock();
    #define CC_LOCKER_TRACK_CBBT_MTX_LOCK    CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_CBBT_MTX_UNLOCK  CC_LOCKER_TRACK_TT_MTX_UNLOCK
    #define CC_LOCKER_TRACK_P_MTX_LOCK       CC_LOCKER_TRACK_TT_MTX_LOCK
    #define CC_LOCKER_TRACK_P_MTX_UNLOCK     CC_LOCKER_TRACK_TT_MTX_UNLOCK
#endif //defined CC_ENABLE_LOCKER_ASSERT

// ----------------------------------------------------------------------------
// Record the current msec time in a variable
// ----------------------------------------------------------------------------
#define RECORD_TIME(varName) \
    {   \
        auto duration = std::chrono::high_resolution_clock::now().time_since_epoch(); \
        int now_millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); \
        varName = now_millis; \
    }

// ----------------------------------------------------------------------------
// **DEBUGGING** Invoke the debugger when exceeding allowed msecs set by RECORD_TIME()
// ----------------------------------------------------------------------------
#define CHECK_TIME_TRAP(varNameHoldingMsecs,allowedMillisecs) \
    { \
        auto duration = std::chrono::high_resolution_clock::now().time_since_epoch(); \
        int now_millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); \
        if ( (now_millis - varNameHoldingMsecs) > allowedMillisecs) \
            asm("int3"); \
    }
// ----------------------------------------------------------------------------
// Write log message when exceeding allowed msecs set by RECORD_TIME()
// ----------------------------------------------------------------------------
#define CHECK_TIME(varNameHoldingMsecs,allowedMillisecs) \
    { \
        auto duration = std::chrono::high_resolution_clock::now().time_since_epoch(); \
        int now_millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); \
        if ( (now_millis - varNameHoldingMsecs) > allowedMillisecs) \
            CCLogger::Get()->DebugLogError(wxString::Format("%s:%d Exceeded Allowed Time(%d ms)", __FUNCTION__, __LINE__, allowedMillisecs)); \
    }


#endif // CCLOGGER_H
