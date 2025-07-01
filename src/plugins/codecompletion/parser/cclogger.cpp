/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "cclogger.h"

#include <wx/event.h>
#include <wx/textfile.h>
#include <wx/utils.h> // wxNewId

#include "logmanager.h" // F()
#include "globals.h"    // cbC2U for cbAssert macro

std::unique_ptr<CCLogger> CCLogger::s_Inst;

bool           g_DebugSmartSense      = false; // If this option is enabled, there will be many log messages when doing semantic match
bool           g_EnableDebugTrace     = false;
const wxString g_DebugTraceFile       = wxEmptyString;
long           g_idCCAddToken         = wxNewId();
long           g_idCCLogger           = wxNewId();
long           g_idCCDebugLogger      = wxNewId();

// Set CC_GLOBAL_DEBUG_OUTPUT via #define in the project options to 0:
// --> No debugging output for CC will be generated (this is the default)
// Set CC_GLOBAL_DEBUG_OUTPUT via #define in the project options to 1:
// --> Debugging output for CC will be generated
// Set CC_GLOBAL_DEBUG_OUTPUT via #define in the project options to 2:
// --> Debugging output for CC will be generated only, when the user enabled this
//     through the menu in the symbols browser (similar to debug smart sense)
// For single files only, the same applies to the individual #define per file
// (like CC_BUILDERTHREAD_DEBUG_OUTPUT, CC_NATIVEPARSER_DEBUG_OUTPUT, etc.)

CCLogger::CCLogger() :
    m_Parent(nullptr),
    m_LogId(-1),
    m_DebugLogId(-1),
    m_AddTokenId(-1)
{
}

/*static*/ CCLogger* CCLogger::Get()
{
    if (!s_Inst.get())
        s_Inst.reset(new CCLogger);

    return s_Inst.get();
}

// Initialized from CodeCompletion constructor
void CCLogger::Init(wxEvtHandler* parent, int logId, int debugLogId, int addTokenId)
{
    m_Parent     = parent;
    m_LogId      = logId;
    m_DebugLogId = debugLogId;
    m_AddTokenId = addTokenId;
}

void CCLogger::AddToken(const wxString& msg)
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

void CCLogger::Log(const wxString& msg)
{
    //Could crash here; should check if shutting down
    if (Manager::IsAppShuttingDown())
        return;

    if (!m_Parent || m_LogId<1) return;

    CodeBlocksThreadEvent evt(wxEVT_COMMAND_MENU_SELECTED, m_LogId);
    evt.SetString(msg);
#if CC_PROCESS_LOG_EVENT_TO_PARENT
    m_Parent->ProcessEvent(evt);
#else
    wxPostEvent(m_Parent, evt);
#endif
}

void CCLogger::DebugLog(const wxString& msg)
{
    // Could crash here; should check if shutting down
    if (Manager::IsAppShuttingDown())
        return;

    if (!m_Parent || m_DebugLogId<1) return;

    CodeBlocksThreadEvent evt(wxEVT_COMMAND_MENU_SELECTED, m_DebugLogId);
    evt.SetString(msg);
#if CC_PROCESS_LOG_EVENT_TO_PARENT
    m_Parent->ProcessEvent(evt);
#else
    wxPostEvent(m_Parent, evt);
#endif
}
