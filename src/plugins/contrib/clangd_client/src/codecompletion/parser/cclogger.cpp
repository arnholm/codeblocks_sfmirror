/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 72 $
 * $Id: cclogger.cpp 72 2022-08-17 18:02:14Z pecanh $
 * $HeadURL: https://svn.code.sf.net/p/cb-clangd-client/code/trunk/clangd_client/src/codecompletion/parser/cclogger.cpp $
 */

#include "cclogger.h"

#include <wx/event.h>
#include <wx/textfile.h>
#include <wx/utils.h> // wxNewId
#include <wx/dir.h>

#include <logmanager.h> // F()
#include <globals.h>    // cbC2U for cbAssert macro
#include <configmanager.h>

std::unique_ptr<CCLogger> CCLogger::s_Inst;

bool            g_EnableDebugTrace     = false;
bool            g_EnableDebugTraceFile = false; // true
const wxString  g_DebugTraceFile       = wxString();
long            g_idCCAddToken         = wxNewId();
long            g_idCCLogger           = wxNewId();
long            g_idCCErrorLogger      = wxNewId();
long            g_idCCDebugLogger      = wxNewId();
long            g_idCCDebugErrorLogger = wxNewId();
// location of the last successful mutex lock
wxString        s_TokenTreeMutex_Owner = wxString();     // location of the last tree lock
wxString        s_ParserMutex_Owner    = wxString();     // location of the last parser lock
wxString        m_ClassBrowserBuilderThreadMutex_Owner = wxString();

#define TRACE_TO_FILE(msg)                                           \
    if (g_EnableDebugTraceFile && !g_DebugTraceFile.IsEmpty())       \
    {                                                                \
        wxTextFile f(g_DebugTraceFile);                              \
        if ((f.Exists() && f.Open()) || (!f.Exists() && f.Create())) \
        {                                                            \
            f.AddLine(msg);                                          \
            bool exp = f.Write() && f.Close();                       \
            cbAssert(exp);                                           \
        }                                                            \
    }                                                                \

#define TRACE_THIS_TO_FILE(msg)                                      \
    if (!g_DebugTraceFile.IsEmpty())                                 \
    {                                                                \
        wxTextFile f(g_DebugTraceFile);                              \
        if ((f.Exists() && f.Open()) || (!f.Exists() && f.Create())) \
        {                                                            \
            f.AddLine(msg);                                          \
            bool exp = f.Write() && f.Close()                        \
            cbAssert(exp);                                           \
        }                                                            \
    }                                                                \


// ----------------------------------------------------------------------------
CCLogger::CCLogger() :
    // ----------------------------------------------------------------------------
    m_Parent(nullptr),
    m_LogId(-1),
    m_LogErrorId(-1),
    m_DebugLogId(-1),
    m_AddTokenId(-1)
{
    m_ExternLogActive = false;
    m_ExternLogPID = wxGetProcessId();

    // location of the last tree lock
    s_TokenTreeMutex_Owner = wxString();
    // location of the last parser lock
    s_ParserMutex_Owner    = wxString();
}
// ----------------------------------------------------------------------------
/*static*/ CCLogger* CCLogger::Get()
// ----------------------------------------------------------------------------
{
    if (!s_Inst.get())
        s_Inst.reset(new CCLogger);

    return s_Inst.get();
}
// ----------------------------------------------------------------------------
void CCLogger::SetExternalLog(bool OnOrOff)
// ----------------------------------------------------------------------------
{
    //(ph 2021/08/30) create external logging

    m_ExternLogActive = OnOrOff;
    if (m_ExternLogActive)
    {
        // Close previous CBCCLogger Files (if any);
        if (m_ExternLogFile.IsOpened())
            m_ExternLogFile.Close();

        wxString tempDir = wxFileName::GetTempDir();
        wxString externLogFileName = wxString::Format("%s\\CBCClogger-%d.log", tempDir, m_ExternLogPID);
        LogManager* pLogMgr =  Manager::Get()->GetLogManager();
        m_ExternLogFile.Open(externLogFileName, "w");
        if (not m_ExternLogFile.IsOpened() )
            pLogMgr->DebugLog("CClogger failed to open CClog "+ externLogFileName);
        else
        {
            wxDateTime now = wxDateTime::Now();
            wxString nowTime = now.Format("%H:%M:%S", wxDateTime::Local);
            wxString nowDate = now.FormatDate();
            wxString itemsep = ";";
            wxString pidToStr = std::to_string(wxGetProcessId() );
            wxString logLine = "PID:" + pidToStr + itemsep + nowDate+"_"+nowTime + itemsep ;
            m_ExternLogFile.Write(logLine + "\n");
            m_ExternLogFile.Flush();
        }
    }
    else
    {
        // Close any open CClogger files
        if (m_ExternLogFile.IsOpened())
            m_ExternLogFile.Close();
    }

}
// ----------------------------------------------------------------------------
CCLogger::~CCLogger()
// ----------------------------------------------------------------------------
{
    if (m_ExternLogFile.IsOpened() )
        m_ExternLogFile.Close();
}

// Initialized from CodeCompletion constructor
// ----------------------------------------------------------------------------
void CCLogger::Init(wxEvtHandler* parent, int logId, int logErrorId, int debugLogId, int debugLogErrorId, int addTokenId)
// ----------------------------------------------------------------------------
{
    m_Parent     = parent;
    m_LogId      = logId;
    m_LogErrorId = logErrorId;
    m_DebugLogId = debugLogId;
    m_DebugLogErrorId = debugLogErrorId;
    m_AddTokenId = addTokenId;
    m_pCfgMgr    = Manager::Get()->GetConfigManager("clangd_client");

    // Remove all previous CBCCLogger Files.
    wxString tempDir = wxFileName::GetTempDir();
    wxArrayString logFiles;
    wxDir::GetAllFiles(tempDir, &logFiles, "CBCCLogger*.log", wxDIR_FILES);
    for (size_t ii=0; ii<logFiles.GetCount(); ++ii)
        wxRemoveFile(logFiles[ii]);
    }
// ----------------------------------------------------------------------------
void CCLogger::AddToken(const wxString& msg)
// ----------------------------------------------------------------------------
{
    if (!m_Parent || m_AddTokenId<1) return;

    CodeBlocksThreadEvent evt(wxEVT_COMMAND_MENU_SELECTED, m_AddTokenId);
    evt.SetString(msg);
#if CC_PROCESS_LOG_EVENT_TO_PARENT
    m_Parent->ProcessEvent(evt);
#else
    wxPostEvent(m_Parent, evt);
#endif
}
// ----------------------------------------------------------------------------
void CCLogger::Log(const wxString& msg, int id)
// ----------------------------------------------------------------------------
{
    //Could crash here; should check if shutting down
    if (Manager::IsAppShuttingDown())
        return;

    if (!m_Parent || m_LogId<1) return;
    bool infoLogging = m_pCfgMgr->ReadBool("/logPluginInfo_check", true);
    // always allow logging of logError msgs
    if (not infoLogging and (id==m_LogId)) return;

    CodeBlocksThreadEvent evt(wxEVT_COMMAND_MENU_SELECTED, id);
    evt.SetString(msg);
#if CC_PROCESS_LOG_EVENT_TO_PARENT
    m_Parent->ProcessEvent(evt);
#else
    wxPostEvent(m_Parent, evt);
#endif
}
// ----------------------------------------------------------------------------
void CCLogger::DebugLog(const wxString& msg, int id)
// ----------------------------------------------------------------------------
{
    // Could crash here; should check if shutting down
    if (Manager::IsAppShuttingDown())
        return;

    if (!m_Parent || m_DebugLogId<1) return;
    bool debugLogging = m_pCfgMgr->ReadBool("/logPluginDebug_check", false);
    if (not debugLogging and (id==m_DebugLogId)) return;
    // Always allow debugError log messages
    CodeBlocksThreadEvent evt(wxEVT_COMMAND_MENU_SELECTED, id);
    evt.SetString(msg);

    // Don't swamp event system with debugger messages
    if (not m_ExternLogActive )
    {
        #if CC_PROCESS_LOG_EVENT_TO_PARENT
            m_Parent->ProcessEvent(evt);
        #else
            wxPostEvent(m_Parent, evt);
        #endif
    }

    //(ph 2021/08/30) write debugging msgs to external file.
    if (m_ExternLogActive and m_ExternLogFile.IsOpened())
    {
        wxDateTime now = wxDateTime::Now();
        wxString nowTime = now.Format("%H:%M:%S", wxDateTime::Local);
        m_ExternLogFile.Write(nowTime + " " + msg + "\n");
        m_ExternLogFile.Flush(); //save data in case of crash.
    }
}
// ----------------------------------------------------------------------------
void CCLogger::LogError(const wxString& msg)
// ----------------------------------------------------------------------------
{
    Log(msg, m_LogErrorId);
}
// ----------------------------------------------------------------------------
void CCLogger::DebugLogError(const wxString& msg)
// ----------------------------------------------------------------------------
{
    DebugLog(msg, m_DebugLogErrorId);
}
