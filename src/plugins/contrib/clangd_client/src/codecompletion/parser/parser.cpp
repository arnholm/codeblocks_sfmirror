/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    //#include <queue>

    #include <wx/app.h>
    #include <wx/dir.h>
    #include <wx/filename.h>
    #include <wx/intl.h>
    #include <wx/progdlg.h>
    #include <wx/xrc/xmlres.h>

    #include <cbproject.h>
    #include <configmanager.h>
    #include <editormanager.h>
    #include <globals.h>
    #include <infowindow.h>
    #include <logmanager.h>
    #include <manager.h>

#endif

#include <unordered_map> //(christo 2024/03/23)

#include <wx/tokenzr.h>
#include <cbstyledtextctrl.h>
#include <wx/xrc/xmlres.h> //XRCID
#include <wx/wxscintilla.h> //apply clangd CodeAction aka:"fix available" //(ph 2024/02/12)

#if defined(_WIN32)
#include "winprocess/misc/fileutils.h"       // fix the URI intrpretation problem
#else
#include "unixprocess/fileutils.h"
#endif

#include "annoyingdialog.h"

#include "../classbrowser.h"
#include <encodingdetector.h>
#include "client.h"
#include "LSP_symbolsparser.h"

#include "../parsemanager.h"
#include "parser.h"

#include "../IdleCallbackHandler.h"
#include "../gotofunctiondlg.h"
#include "ccmanager.h"

#ifndef CB_PRECOMP
    #include "editorbase.h"
#endif

#define CC_PARSER_DEBUG_OUTPUT 0
//#define CC_PARSER_DEBUG_OUTPUT 1 // **Debugging**

#if defined(CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_PARSER_DEBUG_OUTPUT
        #define CC_PARSER_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_PARSER_DEBUG_OUTPUT
        #define CC_PARSER_DEBUG_OUTPUT 2
    #endif
#endif

#if CC_PARSER_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...)
#elif CC_PARSER_DEBUG_OUTPUT == 2
    #define TRACE(format, args...)                                              \
        do                                                                      \
        {                                                                       \
            if (g_EnableDebugTrace)                                             \
                CCLogger::Get()->DebugLog(wxString::Format(format, ##args));                   \
        }                                                                       \
        while (false)
    #define TRACE2(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
#else
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif

wxMutex s_ParserMutex;
// ----------------------------------------------------------------------------
namespace ParserCommon
// ----------------------------------------------------------------------------
{
    static const int PARSER_BATCHPARSE_TIMER_DELAY           = 300;
    //unused static const int PARSER_BATCHPARSE_TIMER_RUN_IMMEDIATELY = 10;
    //unused static const int PARSER_REPARSE_TIMER_DELAY              = 100;
    static const int PARSER_BATCHPARSE_TIMER_DELAY_LONG      = 1000;

    // this static variable point to the Parser instance which is currently running the taskpool
    // when the taskpool finishes, the pointer is set to nullptr.
    volatile Parser* s_CurrentParser = nullptr;

    // NOTE (ollydbg#1#): This static variable is used to prevent changing the member variables of
    // the Parser class from different threads. Basically, It should not be a static wxMutex for all
    // the instances of the Parser class, it should be a member variable of the Parser class.
    // Maybe, the author of this locker (Loaden?) thought that accessing to different Parser instances
    // from different threads should also be avoided.
    // Note: (ph#): //(2021/09/29) changed for the above reason
    //static          wxMutex  s_ParserMutex;
    //-wxMutex  s_ParserMutex;
    //static          wxString       s_ParserMutex_Owner;
    //wxString       s_ParserMutex_Owner;

}// namespace ParserCommon
//extern wxString ParserCommon::g_Owner_s_ParserMutex;

// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{
    // LSP_Symbol identifiers
    #include "../LSP_SymbolKind.h"

    const char STX = '\u0002';  //start of text

    // a counter to indicate when to update the GUI Symbols tree
    /* int prevDocumentSymbolsFilesProcessed = 0; */

    std::deque<json*> LSP_ParserDocumentSymbolsQueue; // cf: OnLSP_ParseDocumentSysmbols()
    std::deque<json*> LSP_ParserSemanticTokensQueue;  // cf: LSP_ParseSemanticTokens()
    json* pJsonST = nullptr; //SemanticToken json from queue; //(ph 2023/11/14)

    __attribute__((used))
    bool wxFound(int result){return result != wxNOT_FOUND;};
    bool wxFound(size_t result){return result != wxString::npos;}

    bool isBusyParsing = false; //(ph 2023/12/02)

}//endAnonymous namespace
// ----------------------------------------------------------------------------
Parser::Parser(ParseManager* parent, cbProject* project)
// ----------------------------------------------------------------------------
   : m_pParseManager(parent),
     m_ParsersProject(project),
     //?m_BatchTimer(this, wxNewId()),
     m_BatchTimer(this, XRCID("BatchTimer")),  //(ph 2023/12/11)
     m_ParserState(ParserCommon::ptCreateParser),
     m_DocHelper(parent)            // parent must be ClgdCompletion*
{
    m_LSP_ParserDone = false;

    if (m_ParsersProject and (m_ParsersProject->GetTitle() == "~ProxyProject~"))
        m_ProxyProject = m_ParsersProject;

    ReadOptions();
    ConnectEvents();

    #if defined(MEASURE_wxIDs) //(ph 2023/12/16)
    CCLogger::Get()->SetGlobalwxIDStart(__FUNCTION__, __LINE__);
    #endif
}
// ----------------------------------------------------------------------------
Parser::~Parser()
// ----------------------------------------------------------------------------
{
    DisconnectEvents();

    // Free any saved CodeActions aka:"fixes available" //(ph 2024/02/12)
    FixesAvailable.clear();

    // clear any Idle time callbacks
    if (GetIdleCallbackHandler())
    {
        GetIdleCallbackHandler()->ClearIdleCallbacks(this) ;
    }
    // delete any queued/waiting json data
    for (size_t cnt=0; cnt < LSP_ParserDocumentSymbolsQueue.size(); ++cnt)
    {
        json* pJson =  LSP_ParserDocumentSymbolsQueue.front();
        LSP_ParserDocumentSymbolsQueue.pop_front();
        delete(pJson);
    }

    //    CC_LOCKER_TRACK_P_MTX_LOCK(ParserCommon::s_ParserMutex)
    // Locking is unnecessary since Parser can do no parsing. Clangd has been shutdown here.
    if (ParserCommon::s_CurrentParser == this)
        ParserCommon::s_CurrentParser = nullptr;
    //    CC_LOCKER_TRACK_P_MTX_UNLOCK(ParserCommon::s_ParserMutex) //appears to be unnecessary
}

// ----------------------------------------------------------------------------
void Parser::ConnectEvents()
// ----------------------------------------------------------------------------
{
    Manager::Get()->GetAppWindow()->PushEventHandler(this);
    Connect(m_BatchTimer.GetId(),   wxEVT_TIMER, wxTimerEventHandler(Parser::OnLSP_BatchTimer));
}
// ----------------------------------------------------------------------------
void Parser::DisconnectEvents()
// ----------------------------------------------------------------------------
{
    Disconnect(m_BatchTimer.GetId(),   wxEVT_TIMER, wxTimerEventHandler(Parser::OnLSP_BatchTimer));

    if ( GetParseManager()->FindEventHandler(this))
        Manager::Get()->GetAppWindow()->RemoveEventHandler(this);
}
// ----------------------------------------------------------------------------
void Parser::OnDebuggerStarting(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    if ( not m_BatchParseFiles.empty() )
    {
        cbProject* pProject = GetParsersProject(); //This parsers cbProject
        wxString msg = wxString::Format("LSP background parsing PAUSED while debugging project(%s)", pProject->GetTitle());
        CCLogger::Get()->DebugLog(msg);
    }
    // Remove all cland_client error and warning margin markers so that they don't
    // hide the debugger active line marker. //(ph 2024/05/13)
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    for (int ii=0; ii< pEdMgr->GetEditorsCount(); ++ii)
    {
        cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pActiveProject) break;
        // Find the project and ProjectFile this editor is holding.
        cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(ii);
        if (pcbEd)
        {
            ProjectFile* pProjectFile = pcbEd->GetProjectFile();
            if (not pProjectFile) continue;
            cbProject* pEdProject = pProjectFile->GetParentProject();
            if (not pEdProject) continue;
            if (pEdProject != pActiveProject) continue;
            pcbEd->DeleteAllErrorAndWarningMarkers();
        }
    }//endFor
}
// ----------------------------------------------------------------------------
void Parser::OnDebuggerFinished(CodeBlocksEvent& event)
// ----------------------------------------------------------------------------
{
    if ( not m_BatchParseFiles.empty() )
    {
        cbProject* pProject = GetParsersProject(); //This parsers cbProject
        wxString msg = wxString::Format("LSP background parsing CONTINUED after debugging project(%s)", pProject->GetTitle());
        CCLogger::Get()->DebugLog(msg);
    }
}
// ----------------------------------------------------------------------------
bool Parser::Done()
// ----------------------------------------------------------------------------
{
    // ----------------------------------------------------------------------------
    // LSP
    // ----------------------------------------------------------------------------

    // if the active projects active editor has been parsed, then we're good to go.
    // We don't wait for background parsing since clangd calls only works on the current translation unit.
    // but if all background files are parsed, we're also good to go.

    bool done = false; //initialize as not done

    cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();

    ProcessLanguageClient* pClient = GetLSPClient();
    if (not pClient) return done;
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    cbProject* pEdProject = nullptr;

    if (pEditor) //(ph 2023/10/18)
    {
        ProjectFile* pPrjfile = pEditor->GetProjectFile();
        if (pPrjfile and pPrjfile->GetParentProject())
            pEdProject = pPrjfile->GetParentProject();
    }

    if (pClient and pEditor)
    {
        // If current editor belongs to current project, test for parse finished
        if (pEdProject == pActiveProject)
            done = pClient->GetLSP_IsEditorParsed(pEditor);
        if (not done)
           done = (GetFilesRemainingToParse() == 0);

        // if any editor of the active project has been parsed, say we're done
        // to allow the symbols tree to be updated.
        if (not done)
        {
            EditorManager* pEdMgr = Manager::Get()->GetEditorManager();

            for (int ii=0; ii< pEdMgr->GetEditorsCount(); ++ii)
            {
                cbEditor* pEditor = pEdMgr->GetBuiltinEditor(ii);
                if (pEditor)
                {
                    ProjectFile* pProjectFile = pEditor->GetProjectFile();
                    if (not pProjectFile) continue;
                    pEdProject = pProjectFile->GetParentProject();
                    if (not pEdProject) continue;
                    if (pEdProject == pActiveProject)
                        done = pClient->GetLSP_IsEditorParsed(pEditor);
                    if (done) break;
                }
            }//endfor
        }//endif not done
    }//if client and editor

    // No editor is open but there's a client and an active project //(ph 2023/10/18)
    // Parser::LSP_ParseDocumentSymbols() counts the number of files processed.
    // This logic can fail, infrequently, on small projects with no open editors
    // that have less than 4 files. But it usually succeeds.
    // If all other checks fail, we verify the small project has at least 1 file
    // and return "done = true" anyway.
    // The user can right-click Symbols/Refresh context menu to update tree any time.
    if ( (not pEditor) and pClient and pActiveProject )
    {
        // reply done(true) if number of files parsed >=4 or no more files to process
        if ( /* (prevDocumentSymbolsFilesProcessed >= 4)
            or */ (GetFilesRemainingToParse() == 0) )
            done = true;

        if (not done)
        {
            // If files greater than 0 and less than 4, say done = true anyway
            // just to kick the Symbols tree into action for small projects.
            int filesCount = pActiveProject->GetFilesCount();
            if ((filesCount > 0) and (filesCount <= 4) )
                done = true;
        }
    }

    return done;
}
// ----------------------------------------------------------------------------
wxString Parser::NotDoneReason()
// ----------------------------------------------------------------------------
{
    wxString reason = _T(" > Reasons:");
    if (!m_BatchParseFiles.empty())
        reason += _T("\n- still batch parse files to parse");

    return reason;
}

// ----------------------------------------------------------------------------
void Parser::ClearPredefinedMacros()
// ----------------------------------------------------------------------------
{
    CC_LOCKER_TRACK_P_MTX_LOCK(s_ParserMutex)

    m_LastPredefinedMacros = m_PredefinedMacros;
    m_PredefinedMacros.Clear();

    CC_LOCKER_TRACK_P_MTX_UNLOCK(s_ParserMutex);
}
// ----------------------------------------------------------------------------
const wxString Parser::GetPredefinedMacros() const
// ----------------------------------------------------------------------------
{
    CCLogger::Get()->DebugLog(_T("Parser::GetPredefinedMacros()"));
    return m_LastPredefinedMacros;
}

// ----------------------------------------------------------------------------
void Parser::AddBatchParse(const StringList& filenames)
// ----------------------------------------------------------------------------
{
    // this function has the same logic as the previous function Parser::AddPriorityHeader
    // it just adds some files to a m_BatchParseFiles, and tick the m_BatchTimer timer.
    if (m_BatchTimer.IsRunning())
        m_BatchTimer.Stop();

    //// CC_LOCKER_TRACK_P_MTX_LOCK(ParserCommon::s_ParserMutex) deprecated
    // Nothing here that needs to be locked now

    if (m_BatchParseFiles.empty())
        m_BatchParseFiles = filenames;
    else
        std::copy(filenames.begin(), filenames.end(), std::back_inserter(m_BatchParseFiles));

    if (m_ParserState == ParserCommon::ptUndefined)
        m_ParserState = ParserCommon::ptCreateParser;

    if (not m_BatchTimer.IsRunning())
    {
        TRACE(_T("Parser::AddBatchParse(): Starting m_BatchTimer."));
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY, wxTIMER_ONE_SHOT);
    }

    //// CC_LOCKER_TRACK_P_MTX_UNLOCK(ParserCommon::s_ParserMutex) deprecated
}
// ----------------------------------------------------------------------------
void Parser::ClearBatchParse()
// ----------------------------------------------------------------------------
{
    if (m_BatchTimer.IsRunning())
        m_BatchTimer.Stop();

    //// CC_LOCKER_TRACK_P_MTX_LOCK(ParserCommon::s_ParserMutex) deprecated
    // Nothing here that needs to be locked now

    if (m_BatchParseFiles.empty())
        return;
    else
        m_BatchParseFiles.clear();

    m_ParserState = ParserCommon::ptUndefined;

    //// CC_LOCKER_TRACK_P_MTX_UNLOCK(ParserCommon::s_ParserMutex) deprecated
}

// ----------------------------------------------------------------------------
void Parser::AddParse(const wxString& filename)
// ----------------------------------------------------------------------------
{
    // similar logic as the Parser::AddBatchParse, but this function only add one file to
    // m_BatchParseFiles member, also it does not change the m_ParserState state.
    if (m_BatchTimer.IsRunning())
        m_BatchTimer.Stop();


    // ----------------------------------------------------------------------------
    // CC_LOCKER_TRACK_P_MTX_LOCK(s_ParserMutex)
    // ----------------------------------------------------------------------------
    auto locker_result = s_ParserMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread, restart the timer and return later
        if (not m_BatchTimer.IsRunning() )
            m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY, wxTIMER_ONE_SHOT);
        if ( GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
            GetIdleCallbackHandler()->QueueCallback(this, &Parser::AddParse, filename);
        return;
    }
    else /*lock succeeded*/
    {
        s_ParserMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
    }

    m_BatchParseFiles.push_front(filename);

    //-if (!m_IsParsing)
    if (not m_BatchTimer.IsRunning())
    {
        TRACE(_T("Parser::AddParse(): Starting m_BatchTimer."));
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY, wxTIMER_ONE_SHOT);
    }

    // ----------------------------------------------------------------------------
    /// Unlock Parser Mutex
    // ----------------------------------------------------------------------------
    CC_LOCKER_TRACK_P_MTX_UNLOCK(s_ParserMutex);
}
// ----------------------------------------------------------------------------
void Parser::LSP_OnClientInitialized(cbProject* pProject)
// ----------------------------------------------------------------------------
{
    // Once the LSP client is initialized, do call LSP DidOpen()s for project files

    if (pProject != GetParsersProject()) return;  //sanity check
    ProcessLanguageClient* pClient = GetLSPClient();
    if ((not pClient) or (not pClient->GetLSP_Initialized(pProject)) )
    {
        //requeue this request, client is not ready
        GetIdleCallbackHandler()->QueueCallback(this, &Parser::LSP_OnClientInitialized, pProject);
        return;
    }

    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    int edCount = pClient ? pEdMgr->GetEditorsCount() : 0;
    bool paused = GetUserParsingPaused();    //Get pause condition
    //if not paused, send open editor files to clangd to be parsed
    for (int ii=0; (ii< edCount) and (not paused); ++ii)
    {
        cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(ii);
        if (not pcbEd) continue; //happens because of "Start here" tab
        ProjectFile* pPrjFile = pcbEd->GetProjectFile();
        if (not pPrjFile) continue;
        if (pPrjFile->GetParentProject() == pProject)
            if (not pClient->GetLSP_IsEditorParsed(pcbEd))
            {
                bool ok = pClient->LSP_DidOpen(pcbEd); //send editors file to clangd
                if (ok)
                    CCLogger::Get()->DebugLog(wxString::Format("%s DidOpen %s",__FUNCTION__,pcbEd->GetFilename()));
            }
    }
    // allow parsing to proceed
    if (int cnt = PauseParsingForReason("AwaitClientInitialization", false) != 0)
    {
        // error if !=  0 **Debugging**
        wxString msg = wxString::Format("%s: PauseParsing count(%d) > 1", __FUNCTION__, cnt);
        Manager::Get()->GetLogManager()->DebugLogError(msg);
    }

}
// ----------------------------------------------------------------------------
bool  Parser::IsBusyParsing(){return isBusyParsing;} //(ph 2023/12/02)
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void Parser::LSP_ParseDocumentSymbols(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown() ) return;

    /// Do Not free the input pJson pointer, it will be freed on return to caller CodeCompletion::LSP_Event()
    json*  pJson = (json*)event.GetClientData(); //<== original json*

    // ----------------------------------------------------------------------------
    // queue a copy of input json data. then queue a callback for OnIdle() which will call back here with
    // a nullptr for json ptr to indicate that this is a callback from the idle time queue.
    // ----------------------------------------------------------------------------
    if (pJson)
    {
        //LSP_ParserDocumentSymbolsQueue is in anonymous namespace above
        LSP_ParserDocumentSymbolsQueue.push_back(new json(*pJson));
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, XRCID("textDocument/documentSymbol"));
        evt.SetClientData(nullptr); //indicate to use top pJson on queue
        GetIdleCallbackHandler()->QueueCallback(this, &Parser::LSP_ParseDocumentSymbols, evt);
        return;
    }

//    #if defined(MEASURE_wxIDs) // **Debugging**
//    CCLogger::ShowLocalUsedwxIDs_t showLocalUsedwxIDs(__FUNCTION__, __LINE__) ;  //(ph 2023/12/14)
//    #endif

    struct IsBusyParsing_t //(ph 2023/12/02)
    {
        IsBusyParsing_t(){isBusyParsing = true;}
        ~IsBusyParsing_t(){isBusyParsing = false;}
    } isBusyLSP_ParseDocumentSymbols;

    // Control is here because a null pJson indicates this function was called from the OnIdle() waiting queue.
    // When called from OnIdle(), process all entries waiting in the clangd response queue.
    // If we cannot update the classBrowser Symbols window is enabled but busy, we'll just re-queue this callback until
    // Class Browser is nolonger busy.
    while (LSP_ParserDocumentSymbolsQueue.size())
    {
        // Retrieve the the oldest entry from the queue
        pJson = LSP_ParserDocumentSymbolsQueue.front();

        // Validate that parser and project are associated
        cbProject* pProject = m_ParsersProject;
        if (not pProject)
        {
            if (pJson)  Delete(pJson);  //don't leak the local allocated json
            LSP_ParserDocumentSymbolsQueue.pop_front(); //remove the current json queue pointer
            continue; //try next entry //(ph 2023/11/28)
        }

        // record time this routine started
        size_t startMillis = m_pParseManager->GetNowMilliSeconds();

        // Validate that this file belongs to this projects parser
        wxString idValue;
        try { idValue = GetwxUTF8Str(pJson->at("id").get<std::string>()); }
        catch(std::exception &err)
        {
            wxString errMsg(wxString::Format("ERROR: %s:%s", __FUNCTION__, err.what()) );
            CCLogger::Get()->DebugLogError(errMsg);
            if (pJson)  Delete(pJson);  //don't leak the local allocated json
            LSP_ParserDocumentSymbolsQueue.pop_front(); //remove the current json queue pointer
            continue; //(ph 2023/11/28)
        }
        // Get filename from between the STX chars
        wxString URI = idValue.AfterFirst(STX);
        if (URI.Contains(STX))
            URI = URI.BeforeFirst(STX); //filename
        wxString filename = fileUtils.FilePathFromURI(URI);

        // Verify client, project and files are still legitimate
        ProcessLanguageClient* pClient = GetLSPClient();
        ProjectFile* pProjectFile = pProject->GetFileByFilename(filename,false);
        if ( (not pClient) or (not pProjectFile))
        {
            LSP_ParserDocumentSymbolsQueue.pop_front(); //delete the current json queue pointer
            if (pJson) Delete(pJson);
            continue; //try next entry
        }

        //(ph 2023/11/28)
        // If classBrowser Symbols UI is busy, we can't add this json data to the TokenTree
        // because classBrowser UI tree will then contain invalid pointers. So we requeue for a callback.
        if (GetParseManager()->IsClassBrowserEnabled()
            and (GetParseManager()->IsOkToUpdateClassBrowserView()) ) //(ph 2023/12/06)
        {
            // true == IsOkToUpdateClassBrowserView() means the Sysmbols window is in use.
            // ClassBrowser is enabled and Symbols tab is being used (has focus).
            // Changing the tokens tree during its use will cause a crash in the
            // Symbols tab update process (refering to deleted token tree items).
            // Re-Queue this call to the the idle time callback queue.
            wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
            GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine); //(ph 2023/12/06)
            if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine)) //verify max retries
                GetIdleCallbackHandler()->QueueCallback(this, &Parser::LSP_ParseDocumentSymbols, event);
            // **Debugging**
            //-wxString msg = wxString::Format(_("QUEUED for callback[%d]:%s"), GetIdleCallbackHandler()->GetQCallbackCount(lockFuncLine),filename);
            //-CCLogger::Get()->DebugLog(msg);
            return;
        }

        // --------------------------------------------------
        /// CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
        // --------------------------------------------------
        // Avoid blocking the UI. If lock is busy, queue a callback for idle time.
        auto locker_result = s_TokenTreeMutex.LockTimeout(250);
        wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
        if (locker_result != wxMUTEX_NO_ERROR)
        {
            /// lock failed
            // **Debugging**
            //wxString locker_result_reason;
            //if (locker_result == wxMUTEX_DEAD_LOCK)
            //    locker_result_reason += "wxMUTEX_DEAD_LOCK";
            //if (locker_result == wxMUTEX_TIMEOUT)
            //    locker_result_reason += "wxMUTEX_TIMEOUT";
            //wxString msg= wxString::Format("LOCK FAILED:TokenTree %s @(%s:%d)",locker_result_reason, __FUNCTION__, __LINE__);
            //msg += wxString::Format("\nOwner: %s", s_TokenTreeMutex_Owner);
            //-CCLogger::Get()->DebugLogError(msg); // **DEBUGGING**
            //-wxSafeShowMessage("TokenTree Lock fail", msg);  // **DEBUGGING**

            // Pause parsing if the queue is too backed up. Caused by a dialog holding the TokenTree lock.
            if ((LSP_ParserDocumentSymbolsQueue.size() > 4) and (PauseParsingCount(__FUNCTION__)==0) )
                PauseParsingForReason(__FUNCTION__, true); //stop parsing until we can get the TokenTree lock

            // When here: the event is already an idle callback, we can just reuse it.
            // Queue this call to the the idle time callback queue.
            if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine)) //verify max retries
                GetIdleCallbackHandler()->QueueCallback(this, &Parser::LSP_ParseDocumentSymbols, event);
            // The lock FAILED above, no need to unlock
            return;
        }
        else
        { ///now have the lock
            s_TokenTreeMutex_Owner = wxString::Format("%s %d", __FUNCTION__, __LINE__); /*record owner*/
            GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);

            if (PauseParsingCount(__FUNCTION__))
                PauseParsingForReason(__FUNCTION__, false);
        }

        ///
        /// No 'return' statements beyond here until TokenTree Unlock !!!
        ///

        LSP_SymbolsParserOptions opts;

        //opts.useBuffer             = false;
        opts.useBuffer             = true;
        opts.bufferSkipBlocks      = false;
        opts.bufferSkipOuterBlocks = false;

        opts.followLocalIncludes   = m_Options.followLocalIncludes;
        opts.followGlobalIncludes  = m_Options.followGlobalIncludes;
        opts.wantPreprocessor      = m_Options.wantPreprocessor;
        opts.parseComplexMacros    = m_Options.parseComplexMacros;
        opts.LLVM_MasterPath       = m_Options.LLVM_MasterPath;
        opts.platformCheck         = m_Options.platformCheck;
        opts.logClangdClientCheck  = m_Options.logClangdClientCheck;
        opts.logClangdServerCheck  = m_Options.logClangdServerCheck;
        opts.logPluginInfoCheck    = m_Options.logPluginInfoCheck;
        opts.logPluginDebugCheck   = m_Options.logPluginDebugCheck;
        opts.lspMsgsFocusOnSaveCheck  = m_Options.lspMsgsFocusOnSaveCheck;
        opts.lspMsgsClearOnSaveCheck  = m_Options.lspMsgsClearOnSaveCheck;
        opts.useCompletionIconsCheck  = m_Options.useCompletionIconsCheck; //(ph 2024/02/10)

        // whether to collect doxygen style documents.
        opts.storeDocumentation    = m_Options.storeDocumentation;
        opts.loader                = nullptr; // This plugin doesn't use this option
        opts.fileOfBuffer = filename;

        bool isLocal = true;
        m_LSP_ParserDone = false;

        LSP_SymbolsParser* pLSP_SymbolsParser = new LSP_SymbolsParser(this, filename, isLocal, opts, m_TokenTree, GetParseManager()->GetHiddenEditor()); //(ph 2023/12/22)
        // move semantic legend to associated parser
        pLSP_SymbolsParser->m_SemanticTokensTypes = m_SemanticTokensTypes;
        pLSP_SymbolsParser->m_SemanticTokensModifiers = m_SemanticTokensModifiers;

        // Remove file data from the token tree, it's about to be updated
        size_t fileIdx = m_TokenTree->GetFileIndex(filename);
        if (fileIdx)
        {
            wxString msg = wxString::Format("%s(): Removing tokens for %s", __FUNCTION__, filename);
            CCLogger::Get()->DebugLog(msg);
            m_TokenTree->RemoveFile(fileIdx);
        }
        //-const size_t   result  = m_TokenTree->GetFileStatusCountForIndex(fileIdx); // **debugging**
        fileIdx = m_TokenTree->InsertFileOrGetIndex(filename);

        // ----------------------------------------------------------------------------
        // Initialize Tokenizer and Parse this json symbols response into the Token Tree
        // ----------------------------------------------------------------------------
        bool parse_rc = pLSP_SymbolsParser->Parse(pJson, pProject);
        TRACE(wxString::Format("%s()->Parse() returned[%s]", __FUNCTION__, parse_rc?"true":"false") );
        if (parse_rc)
        {
            m_TokenTree->FlagFileAsParsed(filename);
            /**Debugging**/
            //wxString msg = wxString::Format("%s(): Added symbols for %s", __FUNCTION__, filename);
            //pLogMgr->DebugLog(msg);
        }

        if (pLSP_SymbolsParser)
          delete(pLSP_SymbolsParser);
        m_LSP_ParserDone = true;

        // ----------------------------------------------------
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        // ----------------------------------------------------
        s_TokenTreeMutex_Owner = wxString();

        if (pJson)
        {
            Delete(pJson);
            //-pJson = nullptr; not needed when using big D Delete()
        }

        LSP_ParserDocumentSymbolsQueue.pop_front(); //De-queue the current json queue pointer

        // Time to insert textDocument/documentSymbols
        size_t durationMillis = m_pParseManager->GetDurationMilliSeconds(startMillis);
        // **debugging** CCLogger::Get()->DebugLog(wxString::Format("%s() processed %s (%zu msec)", __FUNCTION__, wxFileName(filename).GetFullName(), durationMillis));
        wxUnusedVar(durationMillis);

        // Say we've stowed the tree symbols (only for an active editors)
        EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
        EditorBase* pEditorBase = pEdMgr->GetEditor(filename);
        cbEditor* pEditor = nullptr;
        if (pEditorBase) pEditor = pEdMgr->GetBuiltinEditor( pEditorBase);
        if (pEditor)
        {
            pClient->SetLSP_EditorHasSymbols(pEditor, true);
            // Ask for SemanticTokens usable by CodeCompletion since there are no local variables
            // in the document/sysmbols response. (only if "Settings/Editor/Documentation popup" option is checked)
            if (pEditor and (pEditor == pEdMgr->GetActiveEditor()) )
                RequestSemanticTokens(pEditor);
        }//endif pEditor

    }//endWhile entries in queue

    // ----------------------------------------------------------------
    // Update CC toolBar when appropriate
    // ----------------------------------------------------------------
    // Must have a clangd client running, Symbols window must be idle, or
    // no more files are being parsed.
    ProcessLanguageClient* pClient = GetLSPClient();
    if ( (pClient and (not GetParseManager()->IsOkToUpdateClassBrowserView()))  //(ph 2024/01/18)
            or (pClient->LSP_GetServerFilesParsingCount() == 0)
        )
    {
        //Refresh the CC toolbar internal data if this file is the active editors file
        //   ie, if the user is currently looking at this file.
        cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (pEditor)
        {
            wxCommandEvent toolBarTimerEvt(wxEVT_COMMAND_MENU_SELECTED, XRCID("idToolbarTimer"));
            AddPendingEvent(toolBarTimerEvt);
        }
    }

    return;
}
// ** Debugging routine to print json contents **
//// ----------------------------------------------------------------------------
//void Parser::LSP_WalkTextDocumentSymbolResponse(json& jref, wxString& filename, size_t level)
//// ----------------------------------------------------------------------------
//{
//    size_t indentLevel = level?level:1;
//    LogManager* pLogMgr = Manager::Get()->GetLogManager();
//    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
//    cbEditor* pEditor = pEdMgr->GetBuiltinEditor(filename);
//    cbStyledTextCtrl* pEdCtrl = pEditor->GetControl();
//
//    try {
//        json result = jref;
//        size_t defcnt = result.size();
//        for (size_t symidx=0; symidx<defcnt; ++symidx)
//        {
//            wxString name =   result.at(symidx)["name"].get<std::string>();
//            int kind =        result.at(symidx)["kind"].get<int>();
//            int endCol =      result.at(symidx)["range"]["end"]["character"].get<int>();
//            int endLine =     result.at(symidx)["range"]["end"]["line"].get<int>();
//            int startCol =    result.at(symidx)["range"]["start"]["character"].get<int>();
//            int startLine =   result.at(symidx)["range"]["start"]["line"].get<int>();
//            int selectionRangeStartLine = result.at(symidx)["selectionRange"]["start"]["line"].get<int>();
//            int selectionRangeStartCol =  result.at(symidx)["selectionRange"]["start"]["character"].get<int>();
//            int selectionRangeEndLine =   result.at(symidx)["selectionRange"]["end"]["line"].get<int>();
//            int selectionRangeEndCol =    result.at(symidx)["selectionRange"]["end"]["character"].get<int>();
//            size_t childcnt = 0;
//            childcnt = result.at(symidx).contains("children")?result.at(symidx)["children"].size() : 0;
//            pLogMgr->DebugLog(wxString::Format("%*s%s(%d)[%d:%d:%d:%d]", indentLevel*4, "", name, kind, startLine, startCol, endLine, endCol));
//            pLogMgr->DebugLog(wxString::Format("\tchildren[%d]", childcnt ));
//            if ((kind == 12) or (kind == 6))
//            { //function
//                wxString lineTxt = pEdCtrl->GetLine(startLine);
//                //asm("int3"); /*trap*/
//            }
//            if (0)
//                pLogMgr->DebugLog(wxString::Format("SelectionRange[%d:%d:%d:%d]", selectionRangeStartLine, selectionRangeStartCol, selectionRangeEndLine, selectionRangeEndCol));
//            if (childcnt)
//            {
//                json jChildren = result.at(symidx)["children"];
//                LSP_WalkTextDocumentSymbolResponse(jChildren, filename, indentLevel+1);
//            }
//        }//endfor
//    } catch (std::exception &e)
//    {
//        wxString msg = wxString::Format("%s() Error:%s", __FUNCTION__, e.what());
//        cbMessageBox(msg, "json Exception");
//    }
//
//   return;
//}
// ----------------------------------------------------------------------------
void Parser::LSP_ParseSemanticTokens(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown() ) return;

    // The pJsonData must be copied because the data will be freed on return to caller.

    /// Do Not free the original incoming pJson; it will be freed in CodeCompletion::LSP_Event()
    json*  pJsonOrg = (json*)event.GetClientData();


    // ----------------------------------------------------------------------------
    // queue a copy of input json data. then queue a callback for OnIdle() which will have a nullptr for json ptr
    // ----------------------------------------------------------------------------
    if (pJsonOrg)
    {
        //LSP_ParserSemanticTokensQueue is in anonymous namespace above
        LSP_ParserSemanticTokensQueue.push_back(new json(*pJsonOrg));
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, XRCID("textDocument/semanticTokens"));
        evt.SetClientData(nullptr); //indicate to use top pJson on queue
        evt.SetString(event.GetString());
        GetIdleCallbackHandler()->QueueCallback(this, &Parser::LSP_ParseSemanticTokens, evt);
        return;
    }

    if (LSP_ParserSemanticTokensQueue.size() == 0) return;

    // When called from OnIdle(), process only one entry per idle time waiting in queue

        // Retrieve and the oldest entry from the queue
        pJsonST = LSP_ParserSemanticTokensQueue.front();


        cbProject* pProject = m_ParsersProject;
        wxString filename = event.GetString();

        if (not GetLSPClient())
        {
            LSP_ParserSemanticTokensQueue.pop_front(); //delete the current json queue pointer
            if (pJsonST) Delete(pJsonST);
            return;
        }

        // Validate that this file belongs to this projects parser
        wxString idValue;
        try {
            idValue = GetwxUTF8Str(pJsonST->at("id").get<std::string>());

            // Validate that this file belongs to this projects parser
            if ( (not pProject) or
                (not pProject->GetFileByFilename(filename,false)))
                    throw std::runtime_error("Err: wrong project or filename");
        }//try
        catch(std::exception &err)
        {
            wxString errMsg(wxString::Format("ERROR: %s:%s", __FUNCTION__, err.what()) );
            CCLogger::Get()->DebugLogError(errMsg);
            LSP_ParserSemanticTokensQueue.pop_front(); //delete the current json queue pointer
            if (pJsonST) Delete(pJsonST);
            return;
        }

        // Get filename from between the STX chars
        wxString URI = idValue.AfterFirst(STX);
        if (URI.Contains(STX))
            URI = URI.BeforeFirst(STX); //filename
        filename = fileUtils.FilePathFromURI(URI);

        // Verify client, project and files are still legitimate
        ProcessLanguageClient* pClient = GetLSPClient();
        ProjectFile* pProjectFile = pProject->GetFileByFilename(filename,false);
        if ( (not pClient) or (not pProjectFile))
        {
            LSP_ParserSemanticTokensQueue.pop_front(); //delete the current json queue pointer
            if (pJsonST) Delete(pJsonST);
            return;
        }

        // --------------------------------------------------
        /// CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
        // --------------------------------------------------
        // Avoid blocking the UI. If lock is busy, queue a callback for idle time.
        auto locker_result = s_TokenTreeMutex.LockTimeout(250);
        wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
        if (locker_result != wxMUTEX_NO_ERROR)
        {
            // **Debugging**
            //wxString locker_result_reason;
            //if (locker_result == wxMUTEX_DEAD_LOCK)
            //    locker_result_reason += "wxMUTEX_DEAD_LOCK";
            //if (locker_result == wxMUTEX_TIMEOUT)
            //    locker_result_reason += "wxMUTEX_TIMEOUT";
            //wxString msg= wxString::Format("LOCK FAILED:TokenTree %s @(%s:%d)",locker_result_reason, __FUNCTION__, __LINE__);
            //msg += wxString::Format("\nOwner: %s", s_TokenTreeMutex_Owner);
            //-CCLogger::Get()->DebugLogError(msg); // **DEBUGGING**
            //-wxSafeShowMessage("TokenTree Lock fail", msg);  // **DEBUGGING**

            // Pause parsing if the queue is too backed up. Caused by a dialog holding the TokenTree lock.
            if ((LSP_ParserSemanticTokensQueue.size() > 4) and (PauseParsingCount(__FUNCTION__)==0) )
                PauseParsingForReason(__FUNCTION__, true); //stop parsing until we can get the TokenTree lock

            // When here: the event is already an idle callback, we can just reuse it.
            // Queue this call to the the idle time callback queue.
            if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine)) //verify max retries
                GetIdleCallbackHandler()->QueueCallback(this, &Parser::LSP_ParseSemanticTokens, event);
            // The lock FAILED above, no need to unlock
            return;
        }
        else
        { //now have the lock
            s_TokenTreeMutex_Owner = wxString::Format("%s %d", __FUNCTION__, __LINE__); /*record owner*/
            GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);

            if (PauseParsingCount(__FUNCTION__))
                PauseParsingForReason(__FUNCTION__, false);
        }



    ///This struct guarantees TokenTreeMutex is unlocked on any return
    // beyond the allocation of this struct.
    struct UnlockTokenTree_t
    {
        UnlockTokenTree_t(){;}
        ~UnlockTokenTree_t()
        {   CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex);
            s_TokenTreeMutex_Owner = wxString();

            LSP_ParserSemanticTokensQueue.pop_front(); //remove the current json queue pointer
            if (pJsonST) Delete(pJsonST);   // avoid any leak of the json ptr
        }
    } unlockTokenTree;

    // most ParserThreadOptions were copied from m_Options
    LSP_SymbolsParserOptions opts;

    opts.useBuffer             = false;
    opts.bufferSkipBlocks      = false;
    opts.bufferSkipOuterBlocks = false;

    opts.followLocalIncludes   = m_Options.followLocalIncludes;
    opts.followGlobalIncludes  = m_Options.followGlobalIncludes;
    opts.wantPreprocessor      = m_Options.wantPreprocessor;
    opts.parseComplexMacros    = m_Options.parseComplexMacros;
    opts.LLVM_MasterPath       = m_Options.LLVM_MasterPath;
    opts.platformCheck         = m_Options.platformCheck;
    opts.logClangdClientCheck  = m_Options.logClangdClientCheck;
    opts.logClangdServerCheck  = m_Options.logClangdServerCheck;
    opts.logPluginInfoCheck    = m_Options.logPluginInfoCheck;
    opts.logPluginDebugCheck   = m_Options.logPluginDebugCheck;
    opts.lspMsgsFocusOnSaveCheck  = m_Options.lspMsgsFocusOnSaveCheck;
    opts.lspMsgsClearOnSaveCheck  = m_Options.lspMsgsClearOnSaveCheck;
    opts.useCompletionIconsCheck  = m_Options.useCompletionIconsCheck; //(ph 2024/02/10)

    // whether to collect doxygen style documents.
    opts.storeDocumentation    = m_Options.storeDocumentation;

    opts.loader                = nullptr; // must be 0 at this point
    bool isLocal = true;

    m_LSP_ParserDone = false;

    if (not m_TokenTree)
    {
        // What happened to the TokenTree?
        wxString msg = wxString::Format("%s() called with null m_TokenTree", __FUNCTION__);
        CCLogger::Get()->DebugLogError(msg);
        #if defined(cbDEBUG)
            cbAssertNonFatal(m_TokenTree && "Called with null m_TokenTree");
        #endif
        return;
    }

    LSP_SymbolsParser* pLSP_SymbolsParser = new LSP_SymbolsParser(this, filename, isLocal, opts, m_TokenTree, GetParseManager()-> GetHiddenEditor()); //(ph 2023/12/22)

    // move semantic legends to associated parser
    if (pLSP_SymbolsParser->m_SemanticTokensTypes.size() == 0)
    {
        pLSP_SymbolsParser->m_SemanticTokensTypes     = this->m_SemanticTokensTypes;
        pLSP_SymbolsParser->m_SemanticTokensModifiers = this->m_SemanticTokensModifiers;
    }

    // clear the older SemanticToken responses
    m_SemanticTokensVec.clear();

    // **Sanity check** assure that editor is still activated
    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor) return;
    if (pEditor->GetFilename() != filename) return; //may have been closed or de-activated

    // **debugging** Show Semantic token legends
    //    size_t typesCnt = pLSP_SymbolsParser->m_SemanticTokensTypes.size();
    //    size_t modifiersCnt = pLSP_SymbolsParser->m_SemanticTokensModifiers.size();
    //    for (size_t ii=0; ii<pLSP_SymbolsParser->m_SemanticTokensTypes.size(); ++ii)
    //    {
    //        wxString msg = pLSP_SymbolsParser->m_SemanticTokensTypes[ii];
    //        if (1) asm("int3"); /*trap*/
    //    }

    // **Sanity checks**
    pClient = GetLSPClient();
    bool isEditorInitialized  = pClient ? pClient->GetLSP_Initialized(pProject) : false;
    bool isEditorOpen         = isEditorInitialized ? pClient->GetLSP_EditorIsOpen(pEditor) : false;
    bool isFileParsing        = pClient ? pClient->IsServerFilesParsing(pEditor->GetFilename()) : false;
    bool isEditorParsed       = isEditorOpen ? pClient->GetLSP_IsEditorParsed(pEditor) : false;
    bool hasDocSymbols        = pClient ? pClient->GetLSP_EditorHasSymbols(pEditor) : false;

    // assure the editor is not re-parsing while processing these SemanticTokens
    bool isEditorQuiet = isEditorInitialized and isEditorOpen and isEditorParsed and hasDocSymbols;
    isEditorQuiet = isEditorQuiet and (not isFileParsing);
    if (not isEditorQuiet)
        return;

    int fileIdx = m_TokenTree->GetFileIndex(filename);
    if (not fileIdx)
        CCLogger::Get()->DebugLogError(wxString::Format("%s() Error: Missing TokenTree fileIdx for %s", __FUNCTION__, filename));

    bool parse_rc = false;
    try{
        parse_rc = pLSP_SymbolsParser->Parse(pJsonST, pProject);
    }
    catch (cbException& e)
    { e.ShowErrorMessage(); }

    // **Sanity check**
    if (not parse_rc)
        CCLogger::Get()->DebugLogError(wxString::Format("%s() Error: Failed Semantic token parse for %s", __FUNCTION__, filename));
    else
        CCLogger::Get()->DebugLog(wxString::Format("%s() Added Semantic tokens for %s", __FUNCTION__, filename));

    if (pLSP_SymbolsParser)
        Delete( pLSP_SymbolsParser);

    m_LSP_ParserDone = true;

   return;
}//OnLSP_ParseSemanticTokens
// ----------------------------------------------------------------------------
void Parser::RemoveFile(const wxString& filename)
// ----------------------------------------------------------------------------
{
    // ----------------------------------------------------
    // CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    // ----------------------------------------------------
    // If lock is busy, queue a callback for idle time
    auto locker_result = s_TokenTreeMutex.LockTimeout(250);
    wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
    if (locker_result != wxMUTEX_NO_ERROR)
    {
        // lock failed, do not block the UI thread, call back when idle
        // Parser* pParser = static_cast<Parser*>(m_Parser);
        if (GetIdleCallbackHandler()->IncrQCallbackOk(lockFuncLine))
            GetIdleCallbackHandler()->QueueCallback(this, &Parser::RemoveFile, filename);
        return;
    }
    else /*lock succeeded*/
    {
        s_TokenTreeMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        GetIdleCallbackHandler()->ClearQCallbackPosn(lockFuncLine);
    }


    const size_t fileIdx = m_TokenTree->InsertFileOrGetIndex(filename);
    //-const bool   result  = m_TokenTree->GetFileStatusCountForIndex(fileIdx);

    m_TokenTree->RemoveFile(filename);
    m_TokenTree->EraseFileMapInFileMap(fileIdx);
    m_TokenTree->EraseFileStatusByIndex(fileIdx);
    m_TokenTree->EraseFilesToBeReparsedByIndex(fileIdx);

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
    s_TokenTreeMutex_Owner = wxString();

    //-return result;
    return;
}

// ----------------------------------------------------------------------------
bool Parser::AddFile(const wxString& filename, cbProject* project, cb_unused bool isLocal)
// ----------------------------------------------------------------------------
{
    // this function will lock the token tree twice
    // the first place is the function IsFileParsed() function
    // then the AddParse() call
    if (project != m_ParsersProject)
        return false;

    if ( IsFileParsed(filename) )
        return false;

    if (m_ParserState == ParserCommon::ptUndefined)
        m_ParserState = ParserCommon::ptAddFileToParser;

    AddParse(filename);

    return true;
}
// ----------------------------------------------------------------------------
bool Parser::UpdateParsingProject(cbProject* project)
// ----------------------------------------------------------------------------
{
    if (m_ParsersProject == project)
        return true;
    else if (not Done())
    {
        wxString msg(_T("Parser::UpdateParsingProject(): The Parser is not done."));
        msg += NotDoneReason();
        CCLogger::Get()->DebugLog(msg);
        return false;
    }
    else
    {
        m_ParsersProject = project;
        return true;
    }
}
// ----------------------------------------------------------------------------
cbStyledTextCtrl* Parser::GetStaticHiddenEditor(const wxString& filename) //(ph 2023/12/11)
// ----------------------------------------------------------------------------
{
    // Create hidden editor and load its data. This hidden editor is used
    // whenever necessary to obtain cbStyledEditor resources.
    // Only one is allocated to be used by all functions of this parser.

    wxString resultText;
    cbStyledTextCtrl* pControl = nullptr;

    if (wxFileExists(filename))
    {
        EditorManager* edMan = Manager::Get()->GetEditorManager();
        wxWindow* parent = edMan->GetBuiltinActiveEditor()->GetParent();

        // Allocate (only once) a hidden editor to be used by this parser
        // Doing it here allows zero wxID usage to reparse a file and only 4 wIDs to reparse a project.
        if (pHiddenEditor.get() == nullptr)
            pHiddenEditor.reset( new cbStyledTextCtrl(parent, XRCID("ParserHiddenEditor"), wxDefaultPosition, wxSize(0, 0))); //(ph 2023/12/11)
        pControl = pHiddenEditor.get();
        pControl->Show(false);
        pControl->SetText("");
        pControl->UsePopUp(false);

        // check if the file is already opened in built-in editor
        cbEditor* ed = edMan->IsBuiltinOpen(filename);
        if (ed)
            pControl->SetText(ed->GetControl()->GetText());
        else // else load the file in the control
        {
            EncodingDetector detector(filename, false);
            if (not detector.IsOK())
            {
                wxString msg(wxString::Format(_("%s():%d failed EncodingDetector for %s"), __FUNCTION__, __LINE__, filename));
                Manager::Get()->GetLogManager()->Log(msg);
                pControl->SetText("");
                return nullptr;
            }
            pControl->SetText(detector.GetWxStr());
        }//else

    }//swith

    return pControl;
}
// ----------------------------------------------------------------------------
void Parser::OnLSP_BatchTimer(cb_unused wxTimerEvent& event)
// ----------------------------------------------------------------------------
{
    if (Manager::IsAppShuttingDown())
        return;

    cbProject* pProject = GetParsersProject(); //This parsers cbProject

    // If user paused background parsing, reset the timer and return
    if (PauseParsingCount())
    {
        TRACE(wxString::Format("Parser::OnBatchTimer(): Starting m_BatchTimer(Line:%d).", __LINE__));
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY_LONG, wxTIMER_ONE_SHOT);
        return;
    }

    ProcessLanguageClient* pClient = GetLSPClient();
    if ( (not pClient) or (not pClient->GetLSP_Initialized(pProject)) )
    {
        // if LSP client/server not yet initialized, try later
        TRACE(wxString::Format("Parser::OnBatchTimer(): Starting m_BatchTimer(Line:%d).", __LINE__));
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY_LONG, wxTIMER_ONE_SHOT);
        return;
    }

    //verify max background parsing files allowed at a time
    int parallel_parsing = std::max(1, wxThread::GetCPUCount());
    if (parallel_parsing > 1) parallel_parsing = parallel_parsing >> 1; //use only half of cpu threads
    parallel_parsing = std::min(parallel_parsing, m_cfg_parallel_processes);
    int max_parsers_while_compiling = std::min(parallel_parsing, m_cfg_max_parsers_while_compiling);
    // ** Debbuging **
    //wxString msg = wxString::Format("LSP Parsing stat: parsing(%d) of allowed(%d)",int(pClient->LSP_GetServerFilesParsingCount()), parallel_parsing );
    //pLogMgr->DebugLog(msg);

    // If compiler is busy, check for max allowed parsers running while compiling. (user may have set max==zero)
    if (GetParseManager()->IsCompilerRunning() )
    {
        if (int(pClient->LSP_GetServerFilesParsingCount()) >= max_parsers_while_compiling)
        {
            // server is parsing max files allowed while compiler is busy.
            TRACE(wxString::Format("Parser::OnBatchTimer(): Starting m_BatchTimer(Line:%d).", __LINE__));
            m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY_LONG, wxTIMER_ONE_SHOT);
            return;
        }
    }

    // If clangd is using max allowed cpu cores, try later
    if ( int(pClient->LSP_GetServerFilesParsingCount()) >= parallel_parsing)
    {
        // server is busy parsing max files allowed to be parsed at same time.
        TRACE(wxString::Format("Parser::OnBatchTimer(): Starting m_BatchTimer(Line:%d).", __LINE__));
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY, wxTIMER_ONE_SHOT);
        return;
    }
    // If debugger is running, don't parse background files
    if (GetParseManager()->IsDebuggerRunning()) //(ph 2023/11/17)
    {
        // Debugger is running and active, try later

        /**Debugging**/
        //-TRACE(wxString::Format("Parser::OnBatchTimer(): Starting m_BatchTimer(Line:%d).", __LINE__));
        //wxString msg = "Batch background parsing paused because debugger is running";
        //pLogMgr->DebugLog(msg);

        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY_LONG<<1, wxTIMER_ONE_SHOT);
        return;
    }

    // If this parser response queue is > 1, it's getting backed up.
    // pause parsing until the queue is back to 1 or less. //(ph 2023/11/28)
    if (LSP_ParserDocumentSymbolsQueue.size() > 1)
    {
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY_LONG<<1, wxTIMER_ONE_SHOT);
        return;
    }

    if ( not m_BatchParseFiles.empty() ) switch(1)
    {
        default:
        size_t numEntries = m_BatchParseFiles.size();

        // -------------------------------------------------------
        // CC_LOCKER_TRACK_P_MTX_LOCK(s_ParserMutex)
        // -------------------------------------------------------
        auto locker_result = s_ParserMutex.LockTimeout(250);
        wxString lockFuncLine = wxString::Format("%s_%d", __FUNCTION__, __LINE__);
        if (locker_result != wxMUTEX_NO_ERROR)
        {
            // lock failed, do not block the UI thread, restart the timer and return later
            m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY, wxTIMER_ONE_SHOT);
            return;
        }
        else /*lock succeeded*/
        {
            s_ParserMutex_Owner = wxString::Format("%s %d",__FUNCTION__, __LINE__); /*record owner*/
        }

        wxString filename = m_BatchParseFiles.front();
        m_BatchParseFiles.pop_front();
        /**Debugging**/
        //wxString msg = wxString::Format("OnLSP_BatchTimer has %s", filename);
        //pLogMgr->DebugLog(msg);

        // ----------------------------------------------------------------------------
        /// unlock parser mutex
        // ----------------------------------------------------------------------------
        CC_LOCKER_TRACK_P_MTX_UNLOCK(s_ParserMutex);

        // file must belong to this parsers project
        ProjectFile* pProjectFile = pProject ? pProject->GetFileByFilename(filename, false) : nullptr;
        if ((not pProject) or (not pProjectFile) )
            break;

        // file must belong to a project target (target could have been deleted)
        wxArrayString buildTargetsArray = pProjectFile->GetBuildTargets();
        if (not buildTargetsArray.GetCount())
            break;

        EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
        cbEditor* pEditor = pEdMgr->IsBuiltinOpen(filename);
        if (pEditor)
            AddIncludeDir(wxFileName(filename).GetPath());

        // send LSP server didOpen notifier for open editor not yet LSP_DidOpen()'ed
        bool ok = false;
        ProcessLanguageClient* pClient = m_pLSP_Client;
        if (pClient and pEditor and pClient->GetLSP_EditorIsOpen(pEditor))
            break; //didOpen already done for this editor and file
        if (pClient and pEditor and pClient->GetLSP_IsEditorParsed(pEditor))
            break; //didOpen already done for this editor and file
        if (pClient and pEditor and pClient->IsServerFilesParsing(filename))
            break;

        //send LSP_server didOpen notifier for open editor files not yet parsed
        if (pClient and pEditor)
            ok = pClient->LSP_DidOpen(pEditor);
        else //send LSP server didOpen notifier for background (unopened) file
            ok = pClient->LSP_DidOpen(filename, pProject);

        if (ok)
        {
            pClient->LSP_AddToServerFilesParsing(filename);
            wxString msg = wxString::Format("LSP background parse STARTED for (%s) %s (%d more)", pProject->GetTitle(), filename, int(numEntries-1));
            CCLogger::Get()->DebugLog(msg);
        }
        else
        {
            wxString msg = wxString::Format("LSP background parse FAILED for (%s) %s (%d more)", pProject->GetTitle(), filename, int(numEntries-1));
            CCLogger::Get()->DebugLog(msg);
            CCLogger::Get()->Log(msg);
        }

        break;

    }//endif m_BatchParseFiles

    if ( not m_BatchParseFiles.empty() )
    {
        TRACE(wxString::Format("Parser::OnBatchTimer(): Starting m_BatchTimer(Line:%d).", __LINE__));
        m_BatchTimer.Start(ParserCommon::PARSER_BATCHPARSE_TIMER_DELAY, wxTIMER_ONE_SHOT);
    }
    else
    {
        wxString msg = "Background file parsing queue now empty.";
        CCLogger::Get()->DebugLog(msg);
        msg = wxString::Format("LSP Server is processing %zu remaining files.", pClient->LSP_GetServerFilesParsingCount() );
        CCLogger::Get()->DebugLog(msg);
        #if defined(MEASURE_wxIDs)
            CCLogger::Get()->ShowGlobalUsedwxIDs(__FUNCTION__, __LINE__);
        #endif
    }
}
// ----------------------------------------------------------------------------
bool Parser::IsFileParsed(const wxString& filename)
// ----------------------------------------------------------------------------
{
    bool isParsed = false;

    // File is parsed is set by CodeCompletion::OnLSP_DiagnosticsResponse();
    // on receiving some (or an empty) diagnostic responses.

    std::set<wxString>::iterator it;
    it = m_FilesParsed.find(filename);
    if (it != m_FilesParsed.end())
        isParsed = true;

    if ( not isParsed)
    {
        // This code considers the file parsed if it's in the queue waiting to be parsed.
        StringList::iterator it = std::find(m_BatchParseFiles.begin(), m_BatchParseFiles.end(), filename);
        if (it != m_BatchParseFiles.end())
            isParsed = true;
    }

    return isParsed;
}

// ----------------------------------------------------------------------------
void Parser::ReadOptions()
// ----------------------------------------------------------------------------
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");

    // one-time default settings change: upgrade everyone
    bool force_all_on = !cfg->ReadBool(_T("/parser_defaults_changed"), false);
    if (force_all_on)
    {
        cfg->Write(_T("/parser_defaults_changed"),       true);

        cfg->Write(_T("/parser_follow_local_includes"),  true);
        cfg->Write(_T("/parser_follow_global_includes"), true);
        cfg->Write(_T("/want_preprocessor"),             true);
        cfg->Write(_T("/parse_complex_macros"),          true);
        cfg->Write(_T("/platform_check"),                true);
    }

    // Page "clangd_client"
    m_Options.useSmartSense        = cfg->ReadBool(_T("/use_SmartSense"),                true);
    m_Options.whileTyping          = cfg->ReadBool(_T("/while_typing"),                  true);

    // the m_Options.caseSensitive is following the global option in ccmanager
    // ccmcfg means ccmanager's config
    ConfigManager* ccmcfg = Manager::Get()->GetConfigManager(_T("ccmanager"));
    m_Options.caseSensitive        = ccmcfg->ReadBool(_T("/case_sensitive"),             false);

    // Page "C / C++ parser"
    m_Options.followLocalIncludes  = cfg->ReadBool(_T("/parser_follow_local_includes"),  true);
    m_Options.followGlobalIncludes = cfg->ReadBool(_T("/parser_follow_global_includes"), true);
    m_Options.wantPreprocessor     = cfg->ReadBool(_T("/want_preprocessor"),             true);
    m_Options.parseComplexMacros   = cfg->ReadBool(_T("/parse_complex_macros"),          true);
    m_Options.platformCheck        = cfg->ReadBool(_T("/platform_check"),                true);
    m_Options.LLVM_MasterPath      = cfg->Read    (_T("/LLVM_MasterPath"),                 "");
    m_Options.logClangdClientCheck = cfg->ReadBool(_T("/logClangdClient_check"),        false);
    m_Options.logClangdServerCheck = cfg->ReadBool(_T("/logClangdServer_check"),        false);
    m_Options.logPluginInfoCheck   = cfg->ReadBool(_T("/logPluginInfo_check"),          true);
    m_Options.logPluginDebugCheck  = cfg->ReadBool(_T("/logPluginDebug_check"),         false);
    m_Options.lspMsgsFocusOnSaveCheck = cfg->ReadBool(_T("/lspMsgsFocusOnSave_check"),  false);
    m_Options.lspMsgsClearOnSaveCheck = cfg->ReadBool(_T("/lspMsgsClearOnSave_check"),  false);
    m_Options.useCompletionIconsCheck = cfg->ReadBool(_T("/useCompletionIcons_check"),  false); //(ph 2024/02/10)

    // Page "Symbol browser"
    m_BrowserOptions.showInheritance = cfg->ReadBool(_T("/browser_show_inheritance"),    false);
    m_BrowserOptions.expandNS        = cfg->ReadBool(_T("/browser_expand_ns"),           false);
    m_BrowserOptions.treeMembers     = cfg->ReadBool(_T("/browser_tree_members"),        true);

    // Token tree
    m_BrowserOptions.displayFilter   = (BrowserDisplayFilter)cfg->ReadInt(_T("/browser_display_filter"), bdfFile);
    m_BrowserOptions.sortType        = (BrowserSortType)cfg->ReadInt(_T("/browser_sort_type"),           bstKind);

    // Page "Documentation:
    m_Options.storeDocumentation     = cfg->ReadBool(_T("/use_documentation_helper"),         false);

    // force re-read of file types
    ParserCommon::EFileType ft_dummy = ParserCommon::FileType(wxEmptyString, true);
    wxUnusedVar(ft_dummy);

    // Max number of parallel files allowed to parse
    m_cfg_parallel_processes = std::max(cfg->ReadInt("/max_threads", 1), 1);  //don't allow 0
    m_cfg_max_parsers_while_compiling = std::min(cfg->ReadInt("/max_parsers_while_compiling", 0), m_cfg_parallel_processes);

}
// ----------------------------------------------------------------------------
void Parser::WriteOptions()
// ----------------------------------------------------------------------------
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");

    // Page "clangd_client"
    cfg->Write(_T("/use_SmartSense"),                m_Options.useSmartSense);
    cfg->Write(_T("/while_typing"),                  m_Options.whileTyping);

    // Page "C / C++ parser"
    cfg->Write(_T("/parser_follow_local_includes"),  m_Options.followLocalIncludes);
    cfg->Write(_T("/parser_follow_global_includes"), m_Options.followGlobalIncludes);
    cfg->Write(_T("/want_preprocessor"),             m_Options.wantPreprocessor);
    cfg->Write(_T("/parse_complex_macros"),          m_Options.parseComplexMacros);
    cfg->Write(_T("/platform_check"),                m_Options.platformCheck);
    cfg->Write(_T("/LLVM_MasterPath"),               m_Options.LLVM_MasterPath);
    cfg->Write(_T("/logClangdClient_check"),         m_Options.logClangdClientCheck);
    cfg->Write(_T("/logClangdServer_check"),         m_Options.logClangdServerCheck);
    cfg->Write(_T("/logPluginInfo_check"),           m_Options.logPluginInfoCheck);
    cfg->Write(_T("/logPluginDebug_check"),          m_Options.logPluginDebugCheck);
    cfg->Write(_T("/lspMsgsFocusOnSave_check"),      m_Options.lspMsgsFocusOnSaveCheck);
    cfg->Write(_T("/lspMsgsClearOnSave_check"),      m_Options.lspMsgsClearOnSaveCheck);
    cfg->Write(_T("/useCompletionIcons_check"),      m_Options.useCompletionIconsCheck); //(ph 2024/02/10)

    // Page "Symbol browser"
    cfg->Write(_T("/browser_show_inheritance"),      m_BrowserOptions.showInheritance);
    cfg->Write(_T("/browser_expand_ns"),             m_BrowserOptions.expandNS);
    cfg->Write(_T("/browser_tree_members"),          m_BrowserOptions.treeMembers);

    // Token tree
    cfg->Write(_T("/browser_display_filter"),        m_BrowserOptions.displayFilter);
    cfg->Write(_T("/browser_sort_type"),             m_BrowserOptions.sortType);

    // Page "Documentation":
    // m_Options.storeDocumentation will be written by DocumentationPopup
}
// ----------------------------------------------------------------------------
void Parser::OnLSP_DiagnosticsResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // textDocument/publishDiagnostics

    #if defined(MEASURE_wxIDs)
    // When no more work to do, show the wxIDs used on any return
    //CCLogger::ShowLocalUsedwxIDs_t showLocalUsedwxIDs(__FUNCTION__, __LINE__) ;  //(ph 2023/12/14)
    #endif

    if (GetIsShuttingDown()) return;
    if (not GetLSPClient()) return;

    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    cbEditor* pActiveEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();

    // ----------------------------------------------------------------------------
    ///  GetClientData() contains ptr to json object
    ///  DONT free it, return to OnLSP_Event() will free it as a unique_ptr
    // ----------------------------------------------------------------------------
    json* pJson = (json*)event.GetClientData();

    wxString uri;
    int version = -1; //The version of this source file used by clangd (usually 0)
    try {
        uri = GetwxUTF8Str(pJson->at("params").at("uri").get<std::string>());
        if (pJson->at("params").contains("version"))
            version = pJson->at("params").at("version").get<int>();
    }
    catch (std::exception &err) {
        wxString errMsg(wxString::Format("OnLSP_DiagnosticsResponse(() error: %s", err.what()) );
        CCLogger::Get()->DebugLog(errMsg);
        cbMessageBox(errMsg);
        return;
    }

    // Avoid an empty textDocument/publishDiagnostics with a missing version entry.
    // It's sent as a response to a DidClose()
    // This solves the "LSP: file not yet parsed" loop.
    // see https://forums.codeblocks.org/index.php/topic,24357.msg171622.html#msg171622
    // message number 242.
    if (version == -1)  // This solves the "not parsed yet" loop
        return;

    // Mark this editor as parsed and get editor owning this filename
    cbEditor* pEditor =  nullptr;
    wxString cbFilename;
    Parser* pParser = nullptr;
    cbProject* pProject = nullptr;

    if (uri.Length())
    {
        cbFilename = fileUtils.FilePathFromURI(uri); //(ph 2024/02/12)
        // Clear previous fix available data for this filename.
        FixMap_t::iterator it = FixesAvailable.find(cbFilename);
        if (it != FixesAvailable.end()) {
            it->second.clear();
    }

        // Find the editor matching this files diagnostics
        EditorBase* pEdBase = pEdMgr->GetEditor(cbFilename);
        if ( pEdBase)
        {
            pEditor = pEdMgr->GetBuiltinEditor(pEdBase);
            if (not pEditor) return;
            // verify LSP client is ok (am being paranoid here)
            ProcessLanguageClient* pClient = GetLSPClient(); // get this parsers LSP_client
            if (not pClient) return;
            ProjectFile* pProjectFile = pEditor->GetProjectFile();
            pProject = pProjectFile->GetParentProject();

            // Inform user when editor has finished parsing stage
            if (pClient )
            {
                pClient->SetLSP_EditorIsParsed(pEditor, true);
                pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
                size_t remainingToParse = pParser->GetFilesRemainingToParse();
                if (not remainingToParse)
                {
                    remainingToParse = pClient->LSP_GetServerFilesParsingCount();
                    if (remainingToParse) remainingToParse -= 1; //subtract this finished file
                }
                wxString msg = wxString::Format("LSP (editor) parse FINISHED for (%s) %s (%d ms) (%zu more)", pProject->GetTitle(), pEditor->GetFilename(),
                pClient->LSP_GetServerFilesParsingDurationTime(pEditor->GetFilename()), remainingToParse);
                CCLogger::Get()->DebugLog( msg);
                CCLogger::Get()->Log( msg);
            }
        }
    }//endif uri
    else //no uri name in json
    {
        cbAssert((uri.Length()>0) && "Missing JSON URI filename" );
        return;
    }
    if (not GetLSPClient() ) return; //being paranoid again

    wxString lastLSP_Request = GetLSPClient()->GetLastLSP_Request(cbFilename);
    size_t filesParsingDurationTime = GetLSPClient()->LSP_GetServerFilesParsingDurationTime(cbFilename);

    GetLSPClient()->LSP_RemoveFromServerFilesParsing(cbFilename);

    // Find the parser for this file, else use the ProxyProject parser
    if (not pProject) pProject = GetParsersProject();
    if (not pParser) pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (pEditor and pEditor->GetProjectFile())
        pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject) pProject = GetParseManager()->GetProxyProject();
    pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
    if (not pParser)
    {
        wxString msg; msg << "Error: Missing Parser " << __FUNCTION__;
        msg << wxString::Format("Project:%s, Parser:%p,\nFilename:%s", pProject->GetTitle(), pParser, cbFilename);
        cbMessageBox(msg, "Error");
        msg.Replace("\n", " ");
        CCLogger::Get()->DebugLogError(msg);
        //cbAssert(pParser); /**Debugging**/
        return;
    }

    pParser->SetFileParsed(cbFilename);

    size_t remainingToParse = pParser->GetFilesRemainingToParse();
    if (not remainingToParse)
        remainingToParse = GetLSPClient()->LSP_GetServerFilesParsingCount();

    // Inform user how many files left to parse
    // but Ignore /publishDiagnostics responses from a didClose() requests. Idiot server!!
    if ((not pEditor) and (not lastLSP_Request.EndsWith("didClose")) )
    {
        wxString msg = wxString::Format("LSP background parsing FINISHED for: (%s) %s (%zu ms)",
                                        pProject->GetTitle(), cbFilename, filesParsingDurationTime );
        msg += wxString::Format(" (%zu more)", remainingToParse);
        CCLogger::Get()->DebugLog(msg);
        CCLogger::Get()->Log(msg);

    }

    // This could be a /publishDiagnostics response from a didClose() request. Idiot server!!
    if (lastLSP_Request.EndsWith("didClose") ) return;

    // ----------------------------------------------------------------------------
    // If this file belongs to the Proxy Poject of non-project files, return.
    // We do this to avoid massive textdocument/diagnostics errors on external files
    // because they were parsed/compiled by clangd with this projects compiler settings
    // which may have nothing to do with an external file for which we have no compile info.
    // ----------------------------------------------------------------------------
    if (pProject == GetParseManager()->GetProxyProject()) return;
    if (pProject != Manager::Get()->GetProjectManager()->GetActiveProject())
        return; //editor has file not belonging to the active project.

    if (not pEditor)    // Background parsing response has no associated editor.
    {
        ParserCommon::EFileType filetype = ParserCommon::FileType(cbFilename);
        // If usr didn't set "show inheritance" skip symbols request for headers
        if ( not (filetype == ParserCommon::ftSource) )
        {
            ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");
            bool cfgShowInheritance = cfg->ReadBool(_T("/browser_show_inheritance"),    false);
            BrowserOptions& options = pParser->ClassBrowserOptions();
            if (cfgShowInheritance or options.showInheritance) cfgShowInheritance = true;
            if (not cfgShowInheritance) return;
        }
        // This is a background parse response for files not open in an editor.
        // Issue request for textDocument/documentSymbol to update TokenTree
        if (pProject and pProject->GetFileByFilename(cbFilename,false) and wxFileExists(cbFilename) )
        {
                GetLSPClient()->LSP_RequestSymbols(cbFilename, pProject);
        }
        return;
    }

    if ( (not GetLSPClient()) or (not GetLSPClient()->LSP_GetLog()) )
        return;

    // get the last LSP request for this file
    wxString lastLSPrequest = GetLSPClient()->GetLastLSP_Request(cbFilename);
    // A didSave() response with a missing version is actually a response to didClose()
    if (lastLSPrequest.EndsWith("didSave") and version == -1)
        return;

    // ------------------------------------------------
    //  Display diagnostics from json response
    // ------------------------------------------------
    // get diagnostics array of
    //  range{start{line,character}{end{line,character}},
    //  serverity(int),code(int),source(string),message(string),relatedInformation[]
    json diagnostics;
    try { diagnostics = pJson->at("params").at("diagnostics"); }
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("OnLSP_DiagnosticsResponse error:%s\n%s", e.what() );

        CCLogger::Get()->DebugLog(msg );
        cbMessageBox(msg);
        return;
    }

    if ( (not GetLSPClient()) or (not GetLSPClient()->LSP_GetLog()) )
        return;
    //message parts to LSP diagnostics: log filename, line number, diagnostic message
    wxArrayString LSPdiagnostic;
    int logFocusLine = GetLSPClient()->LSP_GetLog()->GetItemsCount();

    int diagnosticsKnt = 0;
    int ignoredCount = 0;

    try { diagnosticsKnt = diagnostics.size(); }//number of "range" items
    catch ( std::exception &e) {
        wxString msg = wxString::Format("OnLSP_DiagnosticsResponse error:%s\n%s", e.what() );
        CCLogger::Get()->DebugLog(msg );
        cbMessageBox(msg);
    }

    // fetch diagnostic messages the user has set to ignore; (set with right-click on LSP messages log)
    wxArrayString& rIgnoredDiagnostics = GetLSPClient()->GetLSP_IgnoredDiagnostics();
    wxArrayString  aLogLinesToWrite;
    const char STX = '\u0002'; //start-of-text char used as string separator

    std::unordered_map<int, bool> lineWarningMap; //(christo 2024/03/23) 3lines
    lineWarningMap.reserve(diagnosticsKnt);
    std::vector<std::pair<int, wxString>> fileDiagnostics;  //(Christo 2024/03/30)
    fileDiagnostics.reserve(diagnosticsKnt);

    try {
        for (int ii=0; ii<diagnosticsKnt; ++ii)
        {
            int diagLine      = diagnostics[ii]["range"]["start"]["line"].get<int>();
            int diagColstrt   = diagnostics[ii]["range"]["start"]["character"].get<int>();
            //  int diagColend    = diagnostics[ii]["range"]["end"]["character"].get<int>();
            int diagSeverity  = diagnostics[ii]["severity"].get<int>();
            //-int diagCode      = diagnostics[ii]["code"].get<int>(); //CCLS
            wxString diagCode = diagnostics[ii]["code"].get<std::string>(); //clang
            //-unused -wxString diagSrc  = diagnostics[ii]["source"].get<std::string>();
            wxString diagMsg  = diagnostics[ii]["message"].get<std::string>();
            //json Info = diagnostics[ii]["relatedInformation"]; //json array of info usually empty
            wxUnusedVar(diagCode);

            wxString cbFilename = fileUtils.FilePathFromURI(uri);
            wxString severity;
            switch (diagSeverity)
            {
                //case 0: severity = "unknown";  break;
                //case 1: severity = "note";     break;
                //case 2: severity = "warning";  break;
                //case 3: severity = "error";    break;
                //case 4: severity = "fatal";    break;

                //Christo patch ticket 1426 2023/10/23 //(ph 2023/10/23 christo)
                case 0: severity = "unknown";  break;
                case 1: severity = "error";    break;
                case 2: severity = "warning";  break;
                case 3: severity = "note";     break;
                case 4: severity = "remark";   break;
            }

            //(ph 2024/02/12)
            // --------------------------------------------
            // Get the array of codeActions (fix available)
            // --------------------------------------------
            // Example JSON data:
            //
            //    "codeActions":[
            //        {"edit":{"changes":{
            //                    "file:///F:/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp":[
            //                        {"newText":"int","range":{"end":{"character":8,"line":275},"start":{"character":4,"line":275}}} //end file array objects
            //                    ]// end file array
            //        } // end changes object
            //        } //end edit object changes,
            //        "isPreferred":true,"kind":"quickfix","title":"change 'xint' to 'int'"
            //        } // end codeActions objects
            //    ] //end codeActions array

            json jCodeActions = diagnostics[ii]["codeActions"];
            int codeActionsKnt = jCodeActions.size();
            //-unused- int codeActionLine = -1;
            wxString codeActionTitle = wxString();    // text that goes to log
            wxString codeActionFilename = wxString();
            wxString codeActionNewText  =  wxString(); //text to change

            for (int jj = 0; jj < codeActionsKnt; ++jj)
            {
                std::string jdumpStr = jCodeActions[jj].dump();

                // start **Debugging**
                //    wxString dbgStr(wxString::Format("%d: %s", jj, jdumpStr));
                //    CCLogger::Get()->DebugLog(dbgStr);
                //    // Separate each member into a separate JSON string //(ph 2024/03/21)
                //    json data = json::parse(jdumpStr);
                //    std::vector<std::string> separate_json_strings;
                //    for (const auto& item : data)
                //    {
                //        separate_json_strings.push_back(item.dump());
                //        CCLogger::Get()->DebugLog("---> " + separate_json_strings.back());
                //    }
                //    separate_json_strings.clear();
                // end **Debugging**

                codeActionFilename = jCodeActions[jj]["edit"]["changes"].begin().key();
                codeActionTitle = jCodeActions[jj]["title"].get<std::string>();
                codeActionFilename = fileUtils.FilePathFromURI(codeActionFilename);

                // I tried 4 hours to access these vars the nlohmann json way and failed.
                // So I'm going to do it the sane way.
                size_t newTextEnd = std::string::npos;
                size_t newTextBegin = jdumpStr.find("{\"newText\"");
                if (newTextBegin != std::string::npos)
                    newTextEnd = jdumpStr.find("}}}", newTextBegin);
                if (newTextEnd != std::string::npos)
                    codeActionNewText = jdumpStr.substr(newTextBegin, (newTextEnd-newTextBegin)+3);
                if (codeActionNewText.Length())
                {
                    bool duplicateFound = false;
                    // Check for duplicates before adding fix changes
                    // Clangd is responding with duplicate fixes.
                    // It can cause errant changes and deletions of lines.
                    if (FixesAvailable.find(codeActionFilename) != FixesAvailable.end())
                    {
                        // Iterate over the vector associated with the filename
                        for (const auto& text : FixesAvailable[codeActionFilename])
                        {
                            // Check if the fix already exists in the vector of fixes
                            if (text == codeActionNewText)
                            {
                                duplicateFound = true;
                                break;
                            }
                        }
                    }//endIf FixesAvailable find duplicates
                    if(not duplicateFound)
                    {
                        // append error line number so we can use it as a search key
                        //  because codeActions may not have it when the fix is to a
                        //  different line.
                        wxString errorLoc = wxString::Format("%d",diagLine);
                        wxString stowit = codeActionNewText+STX+errorLoc+STX+codeActionTitle;
                        FixesAvailable[codeActionFilename].push_back(stowit); //remember the fix
                    }
                }//endif CodeActionNewText length
            }//endFor jj

            wxString logMsg(wxString::Format("LSP:diagnostic:%s %d:%d  %s: %s", cbFilename, diagLine+1, diagColstrt+1, severity, diagMsg));
           // CCLogger::Get()->Log(logMsg); // **Debugging**

            wxString lspDiagTxt = severity + ":" + diagMsg;

            // skip disagnostics matching those the user has set to ignore
            bool foundIgnoredMsg = false;
            for (size_t ignoreCnt=0; ignoreCnt<rIgnoredDiagnostics.GetCount(); ++ignoreCnt)
                if ( rIgnoredDiagnostics[ignoreCnt] == lspDiagTxt)
                    { foundIgnoredMsg = true; break;}
            if (foundIgnoredMsg) { ignoredCount++; continue;} //continue for(diagnosticKnt)

            LSPdiagnostic.Clear();
            LSPdiagnostic.Add(cbFilename);
            LSPdiagnostic.Add(std::to_string(diagLine+1));
            if (codeActionsKnt) //(ph 2024/02/12)
            {
                if (lspDiagTxt.EndsWith("(fix available)") )
                    lspDiagTxt.Append(" " + codeActionTitle);
            }
            LSPdiagnostic.Add(lspDiagTxt);
            // hold msg in array
            aLogLinesToWrite.Add(STX+ LSPdiagnostic[0] +STX+ LSPdiagnostic[1] +STX+ LSPdiagnostic[2]);
            wxString diagMsgAndTitle = diagMsg +":\n"+ codeActionTitle;
            fileDiagnostics.emplace_back(diagLine, diagMsgAndTitle);  //(Christo 2024/03/30)

            if (diagSeverity >= 2) //(christo 2024/03/23) 8lines
            {
                lineWarningMap.insert({ diagLine, true }); //insert if not present
            }
            else
            {
                lineWarningMap[diagLine] = false; //insert or replace as error takes precedence
            }
        }//endfor diagnosticsKnt
        if (diagnosticsKnt)             //(ph 2024/05/02)
            m_pParseManager->InsertDiagnostics(cbFilename, fileDiagnostics);  //(Christo 2024/03/30)

        // ------------------------------------------------------
        // Always put out a log message even if zero diagnostics
        // ------------------------------------------------------
        {// <=== Inner block ctor
             //write a separator line to the log and clear syntax error marks from this editor
            wxString timeHMSM =  GetLSPClient()? GetLSPClient()->LSP_GetTimeHMSM() : "";
            wxString msg = "----Time: " + timeHMSM + "----";
            msg += wxString::Format(" (%d diagnostics)", diagnosticsKnt);
            if (diagnosticsKnt == 1) msg.Replace(" diagnostics", " diagnostic");
            if (ignoredCount) msg.Replace(")", wxString::Format(", %d set 'ignore' by user)", ignoredCount));
            LSPdiagnostic.Clear();
            LSPdiagnostic.Add(wxString::Format("LSP diagnostics: %s", wxFileName(cbFilename).GetFullName()));
            LSPdiagnostic.Add(":");
            LSPdiagnostic.Add(msg);
            GetLSPClient()->LSP_GetLog()->Append(LSPdiagnostic);
            logFocusLine = GetLSPClient()->LSP_GetLog()->GetItemsCount();
            // Clear error marks for this editor
            if (pEditor) pEditor->SetErrorLine(-1);
        }// <== Inner block dtor

        // ------------------------------------------------------
        // Write error messages to LSP messages log
        // ------------------------------------------------------
        for (size_t ii=0; ii<aLogLinesToWrite.GetCount(); ++ii)
        {
            LSPdiagnostic.Clear();
            LSPdiagnostic = GetArrayFromString(aLogLinesToWrite[ii], wxString(STX));
            //write msg to log
            GetLSPClient()->LSP_GetLog()->Append(LSPdiagnostic);
        }//endfor //(christo 2024/03/23) 1line

        EditorBase *pEb = Manager::Get()->GetEditorManager()->GetEditor(cbFilename); //(christo 2024/03/23) 26 lines
        if (pEb)
        {
            cbEditor *pEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(pEb);
            if (pEd)
            {
                //-for (const auto& [diagLine, warning] : lineWarningMap) //(christo 2024/03/23) is C++17
                for (const auto& pair : lineWarningMap) //(ph 2024/03/23) C++11 of the above
                {
                    const auto& diagLine = pair.first;
                    const auto& warning = pair.second;
                    if (warning)
                    {
                        //-fprintf(stderr, "Parser::%s:%d [%p] set warning. diagLine  %d\n", __FUNCTION__, __LINE__, this,
                        //-        diagLine);
                        pEd->SetWarningLine(diagLine);
                    }
                    else
                    {
                        //-fprintf(stderr, "Parser::%s:%d [%p] set error. diagLine  %d\n", __FUNCTION__, __LINE__, this,
                        //-        diagLine);
                        pEd->SetErrorLine(diagLine);
                    }
                }
            }
        }//(christo 2024/03/23) end
    }//endTry
    catch ( std::exception &e) {
        wxString errmsg(wxString::Format("LSP OnLSP_DiagnosticsResponse() error:\n%s", e.what()) );
        CCLogger::Get()->DebugLog(errmsg);
        cbMessageBox(errmsg);
        return;
    }
    // If new log lines were posted above, focus the log separator line for this editor
    //-if (diagnosticsKnt and (logFocusLine > 0) )
    if (diagnosticsKnt or (logFocusLine > 0) )
    {
        // focus the log to these diagnostics' separator line
        // Dont steal focus from popup windows
        cbStyledTextCtrl* pCtrl = pEditor->GetControl();
        bool popupActive = pCtrl ? pCtrl->AutoCompActive(): true;
        popupActive     |= pCtrl ? pCtrl->CallTipActive() : true;

        GetLSPClient()->LSP_GetLog()->FocusEntry(logFocusLine-1);

        // If last request was anything but "textDocument/didSave", don't steal the log focus.
        // If the compiler is running, do not switch away from build log unless
        // user has set option to do so.
        bool canFocus = not popupActive;
        canFocus = canFocus and (not GetParseManager()->IsCompilerRunning()); //set false if compiler is running
        ConfigManager* pCfg = Manager::Get()->GetConfigManager("clangd_client");
        bool userFocus = pCfg->ReadBool("/lspMsgsFocusOnSave_check", false);
        if ( userFocus and canFocus ) switch(1)
        {
            default:
            // switch to LSP messages log tab only when user used "save"
            if (not GetLSPClient()->GetSaveFileEventOccured()) break;
            wxWindow* pFocusedWin = wxWindow::FindFocus();
            if (not GetLSPClient()->LSP_GetLog()) break;

            CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, GetLSPClient()->LSP_GetLog());
            CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
            Manager::Get()->ProcessEvent(evtSwitch);
            Manager::Get()->ProcessEvent(evtShow);
            if (pFocusedWin) pFocusedWin->SetFocus();
        }

        if ((not m_annoyingLogMsgShown) and diagnosticsKnt)
        {
            // If the current editor has warnings or errors,
            // put out annoying msg re: right-mouse click LSP log line or
            // alt-left-mouse click margin warning/error icon to show msg
            m_annoyingLogMsgShown = true; //Don't show it again
            wxString annoyingMsg = _("Error or warnings occured, see 'LSP messages' log.\n\n"
                                     "Right-mouse click log line to apply fixes (if available)\n"
                                     "or Alt-Left-mouse click on the margin warning/error icon");
            AnnoyingDialog annoyingDlg(_("Apply fixes (if available)"), annoyingMsg, wxART_INFORMATION,  AnnoyingDialog::OK);
            annoyingDlg.ShowModal();
        }
    }//endif diagnosticsKnt
    else if (pEditor == pActiveEditor)
    {
        // when no diagnostics for active editor clear error markers
        pEditor->DeleteAllErrorAndWarningMarkers(); //(christo 2024/03/23)
    }

    // ----------------------------------------------------------------------------
    // Issue request for textDocument/documentSymbol to update TokenTree
    // ----------------------------------------------------------------------------
    if (pEditor and GetLSPClient()->GetLSP_Initialized(pEditor) )
    {
        //  Dont parse symbols if this is only a completion requests
        if (not lastLSPrequest.Contains("/completion"))
        {
            // **debugging**
            //cbProject* pProject = GetLSPClient()->GetClientsCBProject();
            //wxString projectTitle = pProject->GetTitle();

            GetLSPClient()->LSP_RequestSymbols(pEditor);
        }
    }

}//end OnLSP_DiagnosticsResponse

// ----------------------------------------------------------------------------
void Parser::OnLSP_ReferencesResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;
    if (not GetLSPClient()) return;

    // ----------------------------------------------------
    // textDocument references event
    // ----------------------------------------------------

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    if (not pProjectFile)
        return;
    cbProject* pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject)
        return;

    // keep a persistent references array to detect duplicate references
    if (not m_pReferenceValues)
        m_pReferenceValues = new wxArrayString;

    wxString evtString = event.GetString();

    // ----------------------------------------------------------------------------
    ///  GetClientData() contains ptr to json object
    ///  dont free it, OnLSP_Event will free it as a unique_ptr
    // ----------------------------------------------------------------------------
    json* pJson = (json*)event.GetClientData();

    if (evtString.StartsWith("textDocument/references") )
    {
        try
        {
            // Example data:
            // {"jsonrpc":"2.0","id":"textDocument/references","result":[{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","range":{"start":{"line":49,"character":45},"end":{"line":49,"character":52}}},{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","range":{"start":{"line":89,"character":4},"end":{"line":89,"character":11}}}]}
            // {"jsonrpc":"2.0","id":"textDocument/references","error":{"code":-32600,"message":"F:/usr/Proj/HelloWxWorld/HelloWxWorldMain.h is not opened"}}

            json valueResult = pJson->at("result");

            cbSearchResultsLog* searchLog = Manager::Get()->GetSearchResultLogger();
            if (!searchLog)
                return;

            const wxString editorFile = pEditor->GetFilename();
            int editorLine = pEditor->GetControl()->GetCurrentLine() + 1;
            wxFileName fn(editorFile);
            const wxString editorBasePath(fn.GetPath());
            size_t focusIndex = 0;
            m_ReportedBadFileReferences.Clear();

            searchLog->Clear();
            searchLog->SetBasePath(editorBasePath);
            SetLogFileBase(editorBasePath);

            // empty any stale references
            m_pReferenceValues->Empty();

            //- unused size_t resultCount = pJson->count("result");
            size_t entryCount = valueResult.size();

            for (size_t ii=0; ii < entryCount; ++ii)
            {
                wxString URI = GetwxUTF8Str(valueResult[ii].at("uri").get<std::string>());
                URI = fileUtils.FilePathFromURI(URI);
                wxFileName curFn = URI;
                wxString absFilename = curFn.GetFullPath();
                if (not wxFileExists(absFilename))
                {
                    m_ReportedBadFileReferences.Add(absFilename);
                    continue;
                }

                curFn.MakeRelativeTo(GetLogFileBase());
                int linenum         = valueResult[ii].at("range").at("start").at("line").get<int>();
                wxString linenumStr = wxString::Format(_T("%d"), linenum+1 );
                wxString text       = GetLineTextFromFile(absFilename, linenum);
                //-if (FindDuplicateEntry(m_pReferenceValues,fullPath, linenumStr, text ) )
                if (FindDuplicateEntry(m_pReferenceValues, curFn.GetFullPath(), linenumStr, text) )
                    continue;
                m_pReferenceValues->Add(curFn.GetFullPath());
                m_pReferenceValues->Add(linenumStr );
                m_pReferenceValues->Add(text);

            }//endfor

            // FIXME (ph#): add support for textDocument/references 'content' parameter
            // Add current editor line because nlohmann/alextao1999 does not support
            // the textDocument/references 'content' parameter requesting the current line reference
            cbStyledTextCtrl* pctrl = pEditor->GetControl();
            int linenum         = pctrl->LineFromPosition(pctrl->GetCurrentPos());
            wxString linenumStr = wxString::Format("%d", linenum+1);
            wxString text       = GetLineTextFromFile(pEditor->GetFilename(), linenum);
            wxFileName curFn    = pEditor->GetFilename();
            curFn.MakeRelativeTo(GetLogFileBase());
            if (not FindDuplicateEntry(m_pReferenceValues, curFn.GetFullPath(), linenumStr, text ) )
            {

                m_pReferenceValues->Add(curFn.GetFullPath());
                m_pReferenceValues->Add(linenumStr);
                m_pReferenceValues->Add(text);

            }//endif

            // add each referenceValue entry to log via a single logValue array entry
            for (unsigned ii=0; ii<m_pReferenceValues->Count(); ii += 3)
            {
                wxArrayString logValues;
                for (unsigned jj=0; jj<3; ++jj)
                    logValues.Add(m_pReferenceValues->Item(ii+jj));
                searchLog->Append(logValues, Logger::info);
                // if this filename == active editor filename, select it as the focused log line
                //-if (logValues[0] == editorFile && atoi(logValues[1]) == editorLine)  //ticket #62
                if ((logValues[0] == editorFile) && (wxAtoi(logValues[1]) == editorLine) ) //ticket #62
                    focusIndex = ii-1;
                logValues.Empty();
            }
            //focus the log
            if (Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_show_search"), true))
            {
                CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, searchLog);
                CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
                Manager::Get()->ProcessEvent(evtSwitch);
                Manager::Get()->ProcessEvent(evtShow);
            }

            //This is moving the cursor before Definition is requested.
            //-searchLog->FocusEntry(focusIndex);
            wxUnusedVar(focusIndex);

            // alextsao1999_lsp-cpp client.h does not yet support the context parameter for
            // LSP textDocument/references, niz: "context": {"includeDeclaration": true}
            // So here we fake up an event to call OnGotoDeclaration() to add to references.
            // Redirect the GoToDeclaration response to LSP_ReferencesResponse (below).

            // Using the peculiarity of clang that jumps back and forth between declaration and implementation
            // when asked for implementation, first ask for declaration and then try for definition/implementation

            // ask for declaration
            size_t id = GetParseManager()->GetLSPEventSinkHandler()->LSP_RegisterEventSink(XRCID("textDocument/declaration"), this, &Parser::OnLSP_ReferencesResponse, event);
            GetLSPClient()->LSP_GoToDeclaration(pEditor, GetCaretPosition(pEditor), id);
            // ask for the definition/implementation
            id = GetParseManager()->GetLSPEventSinkHandler()->LSP_RegisterEventSink(XRCID("textDocument/definition"), this, &Parser::OnLSP_ReferencesResponse, event);
            GetLSPClient()->LSP_GoToDefinition(pEditor, GetCaretPosition(pEditor), id);

            return;

        }//end OnLSP_ReferencesResponse() try
        catch (std::exception &e)
        {
            wxString msg = wxString::Format("OnLSP_ReferencesResponse %s", e.what());
            CCLogger::Get()->DebugLog(msg);
            cbMessageBox(msg);
        }
    }//endif references
    if (m_ReportedBadFileReferences.Count())
    {
        // This happens when a non-project-owned file is opened. Clangd hands back some
        // filenames out of left field. They don't exist, but used to.
        // Eg.: In Codecompletion.cpp, find declaration of cbPlugin then goto (about)
        // cbPlugin.h line 1030 and find references to PluginRegistrant.
        // The references are neither in .cache nor comile_command.json .
        wxString msg = wxString::Format("The language server reference report contains %d non-existent files.", int(m_ReportedBadFileReferences.GetCount()) );
        msg += "\n The compile_commands.json file or the server cache is likely out of sync with the project";
        msg += "\n\n If you've moved or deleted project files you should also delete the";
        msg += "\n compile_commands.json file and .cache folder from the project folder referenced by the nonexistent files.";
        msg += "\n The .cache and .json files will be rebuilt when the project is next reloaded.";
        msg += "\n See Code::Block Debug log for full filenames.\n";
        wxString msgFilenames;
        for(size_t ii=0; ii<m_ReportedBadFileReferences.GetCount(); ++ii)
            msgFilenames << "\n" << m_ReportedBadFileReferences[ii];
        msg << msgFilenames;
        //#if defined(cbDEBUG)
        cbMessageBox(msg, "TextDocument/references error");
        //#endif
        msg = wxString::Format("%s contained nonexistent file: %s", __FUNCTION__, msgFilenames);
        CCLogger::Get()->DebugLog(msg);
        m_ReportedBadFileReferences.Clear();
    }
    // ----------------------------------------------------------------------------
    // check for LSP textDocument/definition/declaration event redirected here by a call back
    // above.
    // ----------------------------------------------------------------------------
    // We queued a GoToImplement() because clang doesnt report the definition
    // when references are requested from the .h file and vice versa.
    // Clang has the peculiarity of toggling back and forth between .h and .cpp when asked for
    // the implementation.
    if ((evtString.StartsWith("textDocument/definition"))
         or (evtString.StartsWith("textDocument/declaration")) )
    {
        try
        {
            json valueResult = pJson->at("result");

            cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
            if (!editor)
                return;

            const wxString focusFile = editor->GetFilename();
            //- unused int focusLine = editor->GetControl()->GetCurrentLine() + 1;
            wxFileName fn(focusFile);
            const wxString editorBasePath(fn.GetPath());

            cbSearchResultsLog* searchLog = Manager::Get()->GetSearchResultLogger();
            if (!searchLog)
                return;
            searchLog->SetBasePath(GetLogFileBase());

            wxArrayString logValues;
            size_t entryCount = valueResult.size();

            if ( 0 == entryCount)
                return;

            for (size_t ii=0; ii < entryCount; ++ii)
            {
                wxString URI = GetwxUTF8Str(valueResult[ii].at("uri").get<std::string>());
                URI = fileUtils.FilePathFromURI(URI);
                wxFileName curFn = URI;
                wxString absFilename = curFn.GetFullPath();
                if (not wxFileExists(absFilename))
                {
                    m_ReportedBadFileReferences.Add(absFilename);
                    continue;
                }

                curFn.MakeRelativeTo(GetLogFileBase());
                int linenum   = valueResult[ii].at("range").at("start").at("line").get<int>();
                wxString text = GetLineTextFromFile(absFilename, linenum);
                wxString linenumStr = wxString::Format("%d", linenum+1); //adapt for 1 CB origin

                // Don't add entry if already in reference array
                bool found = false;
                for (unsigned refindx=0; refindx<m_pReferenceValues->GetCount(); refindx += 3)
                {
                    #if defined(cbDEBUG) //debugging
                    wxString reffilenm = m_pReferenceValues->Item(refindx);
                    wxString refline   = m_pReferenceValues->Item(refindx+1);
                    wxString reftext   = m_pReferenceValues->Item(refindx+2);
                    wxString newfilenm = curFn.GetFullName();
                    #endif

                    if ( (m_pReferenceValues->Item(refindx) == curFn.GetFullPath())
                        and (m_pReferenceValues->Item(refindx+1) == linenumStr)
                        and (m_pReferenceValues->Item(refindx+2) == text) )
                    { found = true; break; }
                }
                if (found) continue; //continue the outer for loop
                // add response entry to global references
                m_pReferenceValues->Add(curFn.GetFullPath() );
                m_pReferenceValues->Add(linenumStr);
                m_pReferenceValues->Add(text);

                //focusIndex = m_pReferenceValues->size()-1 ;
                logValues.Add(curFn.GetFullPath());
                logValues.Add(linenumStr );
                logValues.Add(text);
                searchLog->Append(logValues, Logger::info);
                logValues.Empty();
            }//endfor json entrycount

            if (Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_show_search"), true))
            {
                CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, searchLog);
                CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
                Manager::Get()->ProcessEvent(evtSwitch);
                Manager::Get()->ProcessEvent(evtShow);
            }

        }//end if definition or declaration try
        catch (std::exception &e)
        {
            wxString msg = wxString::Format("OnLSP_ReferencesResponse decl/def %s", e.what());
            CCLogger::Get()->DebugLog(msg);
            cbMessageBox(msg);
        }
    }//end if decl/def

}//end OnLSP_ReferencesResponse
// ----------------------------------------------------------------------------
wxString Parser::GetLineTextFromFile(const wxString& filename, const int lineNum)
// ----------------------------------------------------------------------------
{
    // Fetch a single line from a text file

    EditorManager* edMan = Manager::Get()->GetEditorManager();

    //-unused- wxWindow* parent = edMan->GetBuiltinActiveEditor()->GetParent();
    cbStyledTextCtrl* pControl = GetStaticHiddenEditor(filename); //(ph 2023/12/11)

   wxString resultText;
   switch(1) //once only
    {
        default:

        // check if the file is already opened in built-in editor and do search in it
        cbEditor* ed = edMan->IsBuiltinOpen(filename);
        if (ed)
            pControl->SetText(ed->GetControl()->GetText());
        else // else load the file in the control
        {
            EncodingDetector detector(filename, false);
            if (not detector.IsOK())
            {
                wxString msg(wxString::Format("%s():%d failed EncodingDetector for %s", __FUNCTION__, __LINE__, filename));
                CCLogger::Get()->Log(msg);
                pControl->SetText("");
                return wxString();
            }
        pControl->SetText(detector.GetWxStr());
        }

            resultText = pControl->GetLine(lineNum).Trim(true).Trim(false);
            break;
    }

    return resultText;

}//end GetLineTextFromFile
// ----------------------------------------------------------------------------
bool Parser::FindDuplicateEntry(wxArrayString* pArray, wxString fullPath, wxString& lineNum, wxString& text)
// ----------------------------------------------------------------------------
{
    // Don't add file if already in references array
    bool found = false;
    for (unsigned refindx=0; refindx < pArray->GetCount(); refindx += 3)
    {
        if ( (pArray->Item(refindx) == fullPath)
            and (pArray->Item(refindx+1) == lineNum)
            and (pArray->Item(refindx+2) == text) )
        { found = true; break; }

    }

    return found;
}
// ----------------------------------------------------------------------------
void Parser::OnLSP_DeclDefResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;
    if (not GetLSPClient()) return;

    // ----------------------------------------------------------------------------
    // textDocument/declaration textDocument/definition event
    // ----------------------------------------------------------------------------
    // LSP Result of FindDeclaration or FindImplementation
    // a CB "find declaration"   == Clangd/Clangd "declaration/signature"
    // a CB "find implementation == Clangd/Clangd "definition"
    // this event.string contains type of eventType:result or error
    // example:
    // {"jsonrpc":"2.0","id":"textDocument/definition","result":[{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","range":{"start":{"line":89,"character":24},"end":{"line":89,"character":30}}}]}

    // GetClientData() contains ptr to json object
    // dont free it, OnLSP_Event will free it as a unique_ptr
    json* pJson = (json*)event.GetClientData();

    bool isDecl = false; bool isImpl = false;
    if (event.GetString().StartsWith("textDocument/declaration") )
        isDecl = true;
    else if (event.GetString().StartsWith("textDocument/definition") )
        isImpl = true;

    // ----------------------------------------------------------------------------
    // default processing for textDocument/definition or declaration
    // ----------------------------------------------------------------------------
    if ( (isDecl or isImpl) and (event.GetString().Contains(wxString(STX) +"result")) )
    try
    {
        json resultValue = pJson->at("result");
        if (not resultValue.size() )
        {
            // if declaration request is empty, try implementation
            if (isDecl)
            {
                cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
                if (editor)
                {
                    GetLSPClient()->LSP_GoToDefinition(editor, GetCaretPosition(editor));
                    return;
                }
            }
            //-cbMessageBox(_("Requested token Not found"), _("Warning"), wxICON_WARNING);
            wxString msg = _("Requested token Not found; LSP returned empty response.");
            InfoWindow::Display("LSP " + wxString(__FUNCTION__), msg, 7000);

            return;
        }

        size_t resultKnt = resultValue.size();
        cbSearchResultsLog* searchLog = Manager::Get()->GetSearchResultLogger();

        for (size_t resultIdx=0; resultIdx<resultKnt; ++resultIdx) //only one result usually but "like a box of chocolate" ...
        {
            // "result":[{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.h","range":{"start":{"line":26,"character":12},"end":{"line":26,"character":22}}}]}
            json resultObj = resultValue[resultIdx]; //position to uri results
            #if defined(cbDEBUG)
                std::string see = resultValue.dump(); //debugging
            #endif //LOGGING
            wxString filenameStr = GetwxUTF8Str(resultObj.at("uri").get<std::string>());
            int linenum  = resultObj["range"]["start"]["line"].get<int>();;
            int charPosn = resultObj["range"]["start"]["character"].get<int>();

            // jump over 'file://' prefix
////            if (platform::windows) filenameStr = filenameStr.Mid(8); else filenameStr = filenameStr.Mid(6);
////            filenameStr.Replace("%3A", ":");
////            if (platform::windows)
////                filenameStr.Replace("/", "\\");
            filenameStr = fileUtils.FilePathFromURI(filenameStr);
            EditorManager* pEdMgr = Manager::Get()->GetEditorManager();

            if (resultKnt == 1)
            {
                cbEditor* targetEditor = pEdMgr->Open(filenameStr);
                if (targetEditor)
                {
                    cbStyledTextCtrl* pCntl = targetEditor->GetControl();
                    int posn = pCntl->PositionFromLine(linenum);
                    posn += charPosn; //increment to column
                    pCntl->GotoPos(posn);
                }
            }
            if (resultKnt > 1)
            {
                //redirect multiple declaration/definition results to search results
                // else a flood of editors may open.
                // add each referenceValue entry to log via a single logValue array entry
                //-unused- cbStyledTextCtrl* pCntl = targetEditor->GetControl();
                //-unused- int posn = pCntl->PositionFromLine(linenum);

                if (resultIdx == 0) //clear before add first log entry
                    searchLog->Clear();
                wxString text = GetLineTextFromFile(filenameStr, linenum);
                wxArrayString logValues;
                logValues.Add(filenameStr);
                logValues.Add(std::to_string(linenum));
                logValues.Add(text);
                searchLog->Append(logValues, Logger::info);
                logValues.Empty();
            }
        }//endif uri

        //focus the log (maybe)
        if (resultKnt > 1)
        {
            if (Manager::Get()->GetConfigManager(_T("message_manager"))->ReadBool(_T("/auto_show_search"), true))
            {
                CodeBlocksLogEvent evtSwitch(cbEVT_SWITCH_TO_LOG_WINDOW, searchLog);
                CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
                Manager::Get()->ProcessEvent(evtSwitch);
                Manager::Get()->ProcessEvent(evtShow);
            }
            //-unneeded- searchLog->FocusEntry(focusIndex);
            cbMessageBox(_("Multiple responses re-directed to Search results log."));
        }

        if (resultKnt == 0)
        {
            if (isImpl)
                cbMessageBox(_("Implementation not found"), _("Warning"), wxICON_WARNING);
            else if (isDecl)
                cbMessageBox(_("Declaration not found"), _("Warning"), wxICON_WARNING);
        }//endelse
    }//endif declaration/definition result try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("LSP OnLSP_DeclDefResponse: %s", e.what());
        CCLogger::Get()->DebugLog(msg);
        cbMessageBox(msg);
    }

    else if ( (isDecl or isImpl) and (event.GetString().Contains(wxString(STX) + "error")) )
    {
        //{"jsonrpc":"2.0","id":"textDocument/declaration","error":{"code":-32600,"message":"not indexed"}}
        wxString errorMsg = wxString::Format("error:%s", pJson->at("error").dump() );
        CCLogger::Get()->DebugLog(errorMsg);
        cbMessageBox(_("LSP returned \"" + errorMsg + "\""), _("Warning"), wxICON_WARNING);
        return;
    }//end if declaration/definition error
}//end OnLSP_DeclDefResponse
// ----------------------------------------------------------------------------
void Parser::OnLSP_RequestedSymbolsResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    #if defined(MEASURE_wxIDs)
    //CCLogger::ShowLocalUsedwxIDs_t show_LocalwxIDS(__FUNCTION__, __LINE__) ; // **Debugging** //(ph 2023/12/16)
    #endif

    // This is a callback after requesting textDocument/Symbol (request done in OnLSP_DiagnosticsResponse)

    if (GetIsShuttingDown() ) return;
    if (not GetLSPClient()) return;

    // ----------------------------------------------------------------------------
    ///  GetClientData() contains ptr to json object
    ///  DONT free it! The return to OnLSP_Event() will free it as a unique_ptr
    // ----------------------------------------------------------------------------
    json* pJson = (json*)event.GetClientData();
    wxString idStr = event.GetString();
    wxString URI = idStr.AfterFirst(STX);
    if (URI.Contains(STX))
        URI = URI.BeforeFirst(STX); //filename

    wxString uriFilename = fileUtils.FilePathFromURI(URI);
    cbEditor*  pEditor =  nullptr;
    cbProject* pProject = nullptr;
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    EditorBase* pEdBase = pEdMgr->IsOpen(uriFilename);
    if (pEdBase)
    {
        pEditor = pEdMgr->GetBuiltinEditor(pEdBase);
        if (not pEditor) return;

        ProjectFile* pProjectFile = pEditor->GetProjectFile();
        if (pProjectFile) pProject = pProjectFile->GetParentProject();
        if ( (not pProjectFile) or (not pProject) ) return;
        ParserBase* pParser = GetParseManager()->GetParserByProject(pProject);
        if (not pParser) return;
    }

    if (not pProject) pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    ProcessLanguageClient* pClient = GetLSPClient();

    // Queue the the json data to OnLSP_ParseDocumentSymbols() event, passing it the json pointer
    // The json data will be placed in a queue to be processed during OnIdle() events.
    wxCommandEvent symEvent(wxEVT_COMMAND_MENU_SELECTED, XRCID("textDocument/documentSymbol"));
    symEvent.SetString(uriFilename);
    symEvent.SetClientData(pJson);
    //This places the event onto the idle process queue
    LSP_ParseDocumentSymbols(symEvent); //This places the event onto the idle process queue

    if (not pEditor)    //Background parsing response for file not open in an editor
    {
        // This must be a background parsed file. Issue didClose() to the server
        // Note: this will cause an empty textDocument/publishDiagnostic response from the idiot server.
        pClient->LSP_DidClose(uriFilename, pProject);
    }

    // Didnt we already remove the file in publishDiagnostics response event?
    // But just in case we didnt get here from there...
    pClient->LSP_RemoveFromServerFilesParsing(uriFilename);

    return;
}
// ----------------------------------------------------------------------------
void Parser::RequestSemanticTokens(cbEditor* pEditor)
// ----------------------------------------------------------------------------
{
    // Issue request for textDocument/semanticTokens to update vSemanticTokens

    if (not GetLSPClient()) return;

    bool useDocumentationPopup = Manager::Get()->GetConfigManager("ccmanager")->ReadBool("/documentation_popup", false);
    if (not useDocumentationPopup) return;
    cbEditor* pActiveEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (pActiveEditor != pEditor) return; //may have been closed or de-activated
    // **Debugging**
    // Manager::Get()->Get()->GetLogManager()->DebugLog(wxString::Format("%s() Requesting SemanticTokens for %s", __FUNCTION__, pEditor->GetFilename()));
    if (pEditor and GetLSPClient())
        GetLSPClient()->LSP_RequestSemanticTokens(pEditor);
}
// ----------------------------------------------------------------------------
void Parser::OnLSP_RequestedSemanticTokensResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;
    if (not GetLSPClient()) return;

    // This is a callback after requesting textDocument/Symbol (request done at end of OnLSP_RequestedSymbolsResponse() )
    // Currently, we allow SemanticTokens for the BuiltinActiveEditor only,

    // ----------------------------------------------------------------------------
    ///  GetClientData() contains ptr to json object
    ///  DONT free it! The return to OnLSP_Event() will free it as a unique_ptr
    // ----------------------------------------------------------------------------
    json* pJson = (json*)event.GetClientData();
    wxString idStr = event.GetString();
    wxString URI = idStr.AfterFirst(STX);
    if (URI.Contains(STX))
        URI = URI.BeforeFirst(STX); //filename

    wxString uriFilename = fileUtils.FilePathFromURI(URI);
    cbEditor*  pEditor =  nullptr;
    cbProject* pProject = nullptr;
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    EditorBase* pEdBase = pEdMgr->IsOpen(uriFilename);
    if (pEdBase)
    {
        pEditor = pEdMgr->GetBuiltinActiveEditor();
        if (not pEditor or (pEditor->GetFilename() != uriFilename))
            return;
        ProjectFile* pProjectFile = pEditor->GetProjectFile();
        if (pProjectFile) pProject = pProjectFile->GetParentProject();
        if ( (not pProjectFile) or (not pProject) ) return;
        ParserBase* pParser = GetParseManager()->GetParserByProject(pProject);
        if (not pParser)
            return;
    }
    else return;

    if (not pProject) pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    ProcessLanguageClient* pClient = GetLSPClient();

    // Queue the the json data to OnLSP_ParseDocumentSymbols() event, passing it the json pointer
    // The json data will be placed in a queue to be processed during OnIdle() events.
    wxCommandEvent symEvent(wxEVT_COMMAND_MENU_SELECTED, XRCID("textDocument/semanticTokens"));
    symEvent.SetString(uriFilename);
    symEvent.SetClientData(pJson);
    LSP_ParseSemanticTokens(symEvent); //Call directly

    if (not pEditor)    //Background parsing response for file not open in an editor
    {
        // This must be a background parsed file. Issue didClose() to the server
        // Note: this will cause an empty textDocument/publishDiagnostic response from the idiot server.
        pClient->LSP_DidClose(uriFilename, pProject);
    }

    // Didnt we already remove the file in publishDiagnostics response event?
    // But just in case we didnt get here from there...
    pClient->LSP_RemoveFromServerFilesParsing(uriFilename);

    return;
}//end OnLSP_RequestedSemanticTokensResponse()
// ----------------------------------------------------------------------------
int Parser::FindSemanticTokenEntryFromCompletion( cbCodeCompletionPlugin::CCToken& cctoken, int completionTokenKind)
// ----------------------------------------------------------------------------
{
    // Find a SemanticToken entry for this cctoken
    //The cctokenKind parameter comes from the "kind" field of the clangd completion response

    std::string tknName = cctoken.name.ToStdString();
    std::vector<int> semanticTokensIndexes;
    // convert the completion kind to a semanticToken type
    int semanticTokenType = ConvertLSPCompletionSymbolKindToSemanticTokenType(completionTokenKind);
    int knt = GetSemanticTokensWithName(tknName, semanticTokensIndexes);
    if (not knt) return -1;
    for (int ii=0; ii<knt; ++ii)
    {
        int semTknIdx = semanticTokensIndexes[ii];
        // **Debugging**
        #if defined(cbDEBUG)
        std::string semName = GetSemanticTokenNameAt(semTknIdx);
        int semLength = GetSemanticTokenLengthAt(semTknIdx);
        int semType = GetSemanticTokenTypeAt(semTknIdx);
        int semCol  = GetSemanticTokenColumnNumAt(semTknIdx);
        int semLine = GetSemanticTokenLineNumAt(semTknIdx);
        int semMods = GetSemanticTokenModifierAt(semTknIdx);
        if (semLength or semType or semCol or semLine or semMods) {;} //STFU!
        #endif


        int semtknEntryKind = GetSemanticTokenTypeAt(semTknIdx);
        int semtknEntryModifiers = GetSemanticTokenModifierAt(semTknIdx);

        if (semtknEntryKind != semanticTokenType)
            continue;
        if (semtknEntryModifiers & LSP_SemanticTokenModifier::Declaration)
            return semTknIdx;
    }
    return -1;

}
// ----------------------------------------------------------------------------
void Parser::OnLSP_CompletionResponse(wxCommandEvent& event, std::vector<ClgdCCToken>& v_CompletionTokens)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;

    // ----------------------------------------------------
    // textDocument/completion event
    // ----------------------------------------------------

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    if (not pProjectFile)
        return;
    cbProject* pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject)
        return;

    LogManager* pLogMgr = Manager::Get()->GetLogManager();
    wxUnusedVar(pLogMgr); //STFU !!
    bool useDocumentationPopup = Manager::Get()->GetConfigManager("ccmanager")->ReadBool("/documentation_popup", false);

    // keep a persistent completion array for other routines to use
    // v_CompletinTokens is a reference to clgdCompletion::m_CompletionTokens vector
    if (v_CompletionTokens.size())
            v_CompletionTokens.clear();

    wxString evtString = event.GetString();
    // GetClientData() contains ptr to json object
    // dont free it, OnLSP_Event will free it as a unique_ptr
    json* pJson = (json*)event.GetClientData();

    if (evtString.EndsWith(wxString(STX) +"result") ) try
    {
        // {"jsonrpc":"2.0","id":"textDocument/completion","result":{"isIncomplete":false,
        //     "items":[{"label":"printf(const char *__format, ...) -> int","kind":3,"detail":"","sortText":"   !","filterText":"printf","insertTextFormat":2,"textEdit":{"range":{"start":{"line":26,"character":4},"end":{"line":26,"character":10}},"newText":"printf"}},
        // {"label":"printf_s(const char *_Format, ...) -> int","kind":3,"detail":"","sortText":"   \"","filterText":"printf_s","insertTextFormat":2,"textEdit":{"range":{"start":{"line":26,"character":4},"end":{"line":26,"character":10}},"newText":"printf_s"}},
        size_t valueResultCount = pJson->at("result").size();
        if (not valueResultCount)
            return;

        size_t valueItemsCount = pJson->at("result").at("items").size();
        if (not valueItemsCount) return;

        // **Debugging**
        //LogManager* pLogMgr = CCLogger::Get();
        //pLogMgr->DebugLog("-------------------Completions-----------------");

        json valueItems = pJson->at("result").at("items");
        // -unused- Parser* pParser = (Parser*)GetParseManager()->GetParserByProject(pProject);
        wxString filename = pEditor->GetFilename();

        ConfigManager* cfg = Manager::Get()->GetConfigManager("clangd_client");
        size_t ccMaxMatches = cfg->ReadInt(_T("/max_matches"), 256);

        for (size_t itemNdx=0; (itemNdx < valueItemsCount) && (itemNdx < ccMaxMatches); ++itemNdx)
        {
            wxString labelValue = GetwxUTF8Str(valueItems[itemNdx].at("label").get<std::string>());
            labelValue.Trim(true).Trim(false); // clangd returning prefixed blank

            if (labelValue.empty()) continue; // this happens on Linux clangd ver13

            // Example code from old CC code:
            // tokens.push_back(CCToken(token->m_Index, token->m_Name + dispStr, token->m_Name, token->m_IsTemp ? 0 : 5, iidx));
            // CCToken(int _id, const wxString& dispNm, int categ = -1) :
            //                id(_id), category(categ), weight(5), displayName(dispNm), name(dispNm) {}

            // Remove the elipsis and bullet chars in responses
            wxString badBytes  = wxString::FromUTF8("\xE2\x80\xA2"); // single dot bullet
            wxString badBytes2 = wxString::FromUTF8("\xE2\x80\xA6"); // 3dots ellipsis
            if (labelValue.Contains(badBytes) )
                labelValue.Replace(badBytes,"");    // remove single dot bullet
            if (labelValue.Contains(badBytes2) )    // remove 3dot ellipsis
                labelValue.Replace(badBytes2,"");

            wxString filterText = GetwxUTF8Str(valueItems[itemNdx].at("filterText").get<std::string>());
            int labelKind = valueItems[itemNdx].at("kind").get<int>();
            ClgdCCToken ccctoken(-1, labelValue, labelValue);    //id and name
            ccctoken.id = -1;                                            //needed Documentation popups, set below.
            // cctoken.category used by CB for image index // FIXME (ph#): implement completion images?
            ccctoken.category = -1;                                      //used by CB for image index
            ccctoken.weight = 5;                                         // FIXME (ph#): could use this to good effect
            ccctoken.displayName = labelValue;
            ccctoken.name = labelValue;
            if (filterText.size()) ccctoken.name = filterText;
            ccctoken.semanticTokenID = -1;
            ccctoken.semanticTokenType = -1;

            //The ccctoken.id index is returned to us if item is selected at popup display time.
            if (useDocumentationPopup)
                ccctoken.semanticTokenID = FindSemanticTokenEntryFromCompletion(ccctoken, labelKind);
            ccctoken.semanticTokenType = ConvertLSPCompletionSymbolKindToSemanticTokenType(labelKind); //(ph 2023/12/27)

            ccctoken.id = v_CompletionTokens.size();

            if (GetParseManager()->GetUseCCIconsOption())
            {
                ccctoken.category = ConvertLSPCompletionSymbolKindToCCTokenKind(labelKind);
                ccctoken.semanticTokenType = ConvertLSPCompletionSymbolKindToSemanticTokenType(labelKind); //(ph 2024/02/10)
            }

            v_CompletionTokens.push_back(ccctoken);

            // **debugging**
            //    for (size_t ij=0; ij<v_CompletionTokens.size(); ++ij)
            //    {
            //        wxString cmpltnStr = wxString::Format(
            //                "Completion:id[%d],category[%d],weight[%d],displayName[%s],name[%s]",
            //                                v_CompletionTokens[ij].id,
            //                                v_CompletionTokens[ij].category,
            //                                v_CompletionTokens[ij].weight,
            //                                v_CompletionTokens[ij].displayName,
            //                                v_CompletionTokens[ij].name
            //                                );
            //        pLogMgr->DebugLog(cmpltnStr);
            //    }//endfor

        }//endfor itemNdx

        if (v_CompletionTokens.size() )
        {
            CodeBlocksEvent evt(cbEVT_COMPLETE_CODE);

            // **debugging**
            //CCLogger::Get()->DebugLog("---------------LSP:Completion Results:-----------");
            //for(size_t itemidx=0; itemidx<v_CompletionTokens.size(); ++itemidx)
            //{
            //    CCToken tkn = v_CompletionTokens[itemidx] ;
            //    wxString logMsg(wxString::Format("%d %s %s %d %d", tkn.id, tkn.displayName, tkn.name, tkn.weight, tkn.category ));
            //    CCLogger::Get()->DebugLog(logMsg);
            //}

            Manager::Get()->ProcessEvent(evt);
        }

    }//if result try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("LSP OnLSP_CompletionResponse: %s", e.what());
        CCLogger::Get()->DebugLog(msg);
    }
}//end OnLSP_CompletionResponse
// ----------------------------------------------------------------------------
wxString Parser::GetCompletionPopupDocumentation(const ClgdCCToken& token)
// ----------------------------------------------------------------------------
{
    //-oldCC- return m_DocHelper.GenerateHTML(token.id, GetParseManager()->GetParser().GetTokenTree());
    // For clangd client we issue a hover request to get clangd data
    // OnLSP_CompletionPopupHoverResponse will push the data int m_HoverTokens and
    // reissue the GetDocumentation request

    if (not GetLSPClient()) return wxString();

    if (m_HoverCompletionString.empty()) //if empty data, ask for hover data
    {
        m_HoverCCTokenPending = token; //save the token param on first call from ccManager

        bool useDocumentationPopup = Manager::Get()->GetConfigManager("ccmanager")->ReadBool("/documentation_popup", false);
        if (not useDocumentationPopup) return wxString();
        if (token.id == -1) return wxString();
        // Get the editor position for the parameter token
        cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pProject) return wxString();
        ParserBase* pParser = GetParseManager()->GetParserByProject(pProject);
        if (not pParser) return wxString();
        cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (not pEditor) return wxString();
        cbStyledTextCtrl* pControl = pEditor->GetControl();
        if (not pEditor) return wxString();

        int semanticTokenId = token.semanticTokenID;
        if (semanticTokenId == -1) return wxString();
        wxString tokenName  = pParser->GetSemanticTokenNameAt(semanticTokenId);
        if (tokenName.empty()) return wxString();

        int edLineNum       = pParser->GetSemanticTokenLineNumAt(semanticTokenId);
        int edColNum        = pParser->GetSemanticTokenColumnNumAt(semanticTokenId);
        int edPosition      = pControl->PositionFromLine(edLineNum);
        edPosition += edColNum +1;
        // invoke LSP_Hover() to get additional info like namespace etc
        // Register event sink function to receive clangd response function
        wxCommandEvent event;
        event.SetInt(token.id);
        size_t rrid = GetParseManager()->GetLSPEventSinkHandler()->LSP_RegisterEventSink(XRCID("textDocument/hover"), (Parser*)pParser, &Parser::OnLSP_CompletionPopupHoverResponse, event);
        GetLSPClient()->LSP_Hover(pEditor, edPosition, rrid);

        return wxString();
    }
    else // have hover info, format it and return completion documentation popup
    {
        // we have hover data to pass to the html popup routine
        //return m_DocHelper.GenerateHTML(int(token.id), &GetParseManager()->GetParser());
        // -save - wxString htmlInfo = m_DocHelper.GenerateHTML(int(m_HoverCCTokenPending.id), m_HoverCompletionString,  &GetParseManager()->GetParser());
        wxString htmlInfo = m_DocHelper.GenerateHTMLbyHover(token, m_HoverCompletionString,  &GetParseManager()->GetParser());
        m_HoverCompletionString.Clear();
        return htmlInfo;
    }
}

// ----------------------------------------------------------------------------
void Parser::OnLSP_CompletionPopupHoverResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;

    // receives hover response issued for Documentation popup info
    // ----------------------------------------------------
    // textDocument hover event
    // ----------------------------------------------------

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    if (not pProjectFile)
        return;
    cbProject* pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject)
        return;

    if (m_HoverCompletionString.Length())
            m_HoverCompletionString.clear();

    wxString evtString = event.GetString();
    if (not evtString.Contains("textDocument/hover"))
    {
        wxString msg = wxString::Format("%s: Received non textDocument/Hover response", __FUNCTION__);
        CCLogger::Get()->DebugLogError(msg);
        return;
    }

    /// GetClientData() contains ptr to json object, dont free it, OnLSP_Event will free it as a unique_ptr
    json* pJson = (json*)event.GetClientData();

    if (evtString.EndsWith(wxString(STX) +"result") ) try
    {
        //Info:
        // {"jsonrpc":"2.0","id":"textDocument/hover","result":
        //    {"contents":["#include <iostream>",
        //                  {"language":"cpp","value":"bool myFunction(std::string aString)"}
        //                ],
        //     "range":{"start":{"line":12,"character":4},"end":{"line":12,"character":14}}
        //    }
        //  }

        // I'm confused about what LSP is returning here. Doesn't match the documentation.
        size_t valueResultCount = pJson->at("result").size();
        if (not valueResultCount) return;

        size_t valueItemsCount = pJson->at("result").at("contents").size();
        if (not valueItemsCount) return;

        json contents = pJson->at("result").at("contents");
        wxString contentsValue = GetwxUTF8Str(contents.at("value").get<std::string>());

        //#if wxCHECK_VERSION(3,1,5) //3.1.5 or higher
        //// wx3.0 cannot produce the utf8 string
        //wxString badBytes =  "\xE2\x86\x92" ; //Wierd chars in hover results
        //contentsValue.Replace(badBytes, "Type:"); // asserts on wx3.0
        //#endif
        contentsValue.Trim(0).Trim(1); //wx3.0 sees a prefixed blank

        m_HoverCompletionString = contentsValue;
        if (m_HoverCompletionString.Length() )
        {
            Manager::Get()->GetCCManager()->NotifyDocumentation();
        }
    }//endif results try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("%s %s", __FUNCTION__, e.what());
        CCLogger::Get()->DebugLog(msg);
        cbMessageBox(msg);
    }
}//end OnLSP_CompletionPopupHoverResponse
// ----------------------------------------------------------------------------
void Parser::OnLSP_HoverResponse(wxCommandEvent& event, std::vector<ClgdCCToken>& v_HoverTokens, int n_HoverLastPosition)
// ----------------------------------------------------------------------------
{

    GetParseManager()->SetHoverRequestIsActive(false); //Hover is now done

    if (GetIsShuttingDown()) return;

    // ----------------------------------------------------
    // textDocument hover event
    // ----------------------------------------------------

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    if (not pProjectFile)
        return;
    cbProject* pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject)
        return;

    // keep a persistent hover token array for other routines to use
    if (v_HoverTokens.size())
            v_HoverTokens.clear();

    wxString evtString = event.GetString();

    /// GetClientData() contains ptr to json object, dont free it, OnLSP_Event will free it as a unique_ptr
    json* pJson = (json*)event.GetClientData();

    if (evtString.EndsWith(wxString(STX) +"result") ) try
    {
        //Info:
        // {"jsonrpc":"2.0","id":"textDocument/hover","result":
        //    {"contents":["#include <iostream>",
        //                  {"language":"cpp","value":"bool myFunction(std::string aString)"}
        //                ],
        //     "range":{"start":{"line":12,"character":4},"end":{"line":12,"character":14}}
        //    }
        //  }

        // I'm confused about what LSP is returning here. Doesn't match the documentation.
        size_t valueResultCount = pJson->at("result").size();
        if (not valueResultCount) return;

        size_t valueItemsCount = pJson->at("result").at("contents").size();
        if (not valueItemsCount) return;

        json contents = pJson->at("result").at("contents");
        wxString contentsValue = GetwxUTF8Str(contents.at("value").get<std::string>());

        // Example Hover contents: L"instance-method HelloWxWorldFrame::OnAbout\n\nType: void\nParameters:\n- wxCommandEvent & event\n\n// In HelloWxWorldFrame\nprivate: void HelloWxWorldFrame::OnAbout(wxCommandEvent &event)"
        // get string array of hover info separated at /n chars.
        wxString hoverString = contentsValue;
        hoverString.Replace("\n\n", "\n"); //remove double newlines
        wxArrayString vHoverInfo = GetArrayFromString(hoverString, "\n");

        //// **Debugging** show incoming data
        //{
        //    #warning comment out this **Debugging**
        //     LogManager* pLogMgr = Manager::Get()->GetLogManager();
        //        for (size_t ii=0; ii<vHoverInfo.size(); ++ii)
        //            pLogMgr->DebugLog(wxString::Format("vHoverInfo[%d]:%s", int(ii), vHoverInfo[ii]));
        //}

        /// ccManager will show max lines = only 1/4 editor lines on the screen
        // ----------------------------------------------------------------------------
        // Reformat the hover response so that it fits into the ccManager allowed 5 to 6 lines.
        // ccManager displays tips by unique ordered std::set, so we have to set the
        // first entry to "~NoSort~" to tell ccManager to use our own order.
        // ----------------------------------------------------------------------------
        wxString hoverText;
        wxString noSort = "~NoSort~";
        size_t posn;

        // Tell ccManager not to sort entries by setting the first entry == "~NoSort~"
        if (vHoverInfo.size())
            v_HoverTokens.push_back(ClgdCCToken(0, noSort, noSort));

        for (size_t ii=0; ii<vHoverInfo.size(); ++ii)
        {
            if (vHoverInfo[ii].StartsWith("----")) //a huge long line of dashes
            {
                if (wxFound(posn = vHoverInfo[ii].find_last_of('-')))
                {
                    // get any text following the huge number of dashes
                    hoverText = vHoverInfo[ii].Mid(posn+1);
                    if (hoverText.Length())
                    {
                        v_HoverTokens.push_back(ClgdCCToken(int(ii), hoverText, hoverText));
                    }
                    hoverText.Empty();
                }
                continue;
            }
            else if (vHoverInfo[ii].StartsWith("Parameters:"))
            {
                // Gather the parameters into one line of text
                hoverText = " Args: ";
                continue;
            }
            else if (vHoverInfo[ii].StartsWith("- ") and hoverText.Contains(" Args: "))
            {
                //Gather the parameters into one line of text
                hoverText += vHoverInfo[ii];
                continue;
            }
            else if (hoverText.Contains(" Args: "))
            {
                // Ignore the parameter line for now. The parameters are shown
                // when the full declaraton is shown anyway.
                if (0) // don't show this parameter text
                    v_HoverTokens.push_back(ClgdCCToken(ii-1, hoverText, hoverText));
                hoverText.Clear();
                if (ii > 0) ii -= 1; //use the current item again
                continue;
            }
            else
            {
                hoverText = vHoverInfo[ii];
                v_HoverTokens.push_back(ClgdCCToken(ii, hoverText, hoverText));
                hoverText.Clear();
            }
        }//endfor vHoverInfo

        //// **Debugging** show what is going to be passed to ccManager
        //#warning comment out this **Debugging**
        //if (v_HoverTokens.size())
        //{
        //     LogManager* pLogMgr = Manager::Get()->GetLogManager();
        //        for (size_t ii=0; ii<v_HoverTokens.size(); ++ii)
        //            pLogMgr->DebugLog(wxString::Format("v_HoverTokens[%d]:%s", int(ii), v_HoverTokens[ii].displayName));
        //}

        // The first entry is "~NoSort~", so count must be > 1
        if (v_HoverTokens.size() > 1 )
        {
            //re-invoke cbEVT_EDITOR_TOOLTIP now that there's data to display
            CodeBlocksEvent evt(cbEVT_EDITOR_TOOLTIP);
            cbEditor* pEd = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
            cbStyledTextCtrl* stc = pEd->GetControl();
            wxPoint pt = stc->PointFromPosition(n_HoverLastPosition);
            evt.SetX(pt.x);
            evt.SetY(pt.y);
            evt.SetInt(stc->GetStyleAt(stc->GetCurrentPos()));
            evt.SetEditor(pEd);
            evt.SetExtraLong(0);
            evt.SetString(wxT("evt from menu"));
            Manager::Get()->ProcessEvent(evt);
        }//endif HoverTokens
    }//endif results try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("OnLSP_HoverResponse() %s", e.what());
        CCLogger::Get()->DebugLog(msg);
        cbMessageBox(msg);
    }
}//end OnLSP_HoverResponse
// ----------------------------------------------------------------------------
void Parser::OnLSP_SignatureHelpResponse(wxCommandEvent& event, std::vector<cbCodeCompletionPlugin::CCCallTip>& v_SignatureTokens, int n_HoverLastPosition )
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;

    // ----------------------------------------------------
    // textDocument/signatureHelp event
    // ----------------------------------------------------

    cbEditor* pEditor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    if (not pProjectFile)
        return;
    cbProject* pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject)
        return;

    // keep a persistent hover token array for other routines to use
    if (v_SignatureTokens.size())
            v_SignatureTokens.clear();

    wxString evtString = event.GetString();
    // GetClientData() contains ptr to json object
    // dont free it, OnLSP_Event will free it as a unique_ptr
    json* pJson = (json*)event.GetClientData();

    if (evtString.EndsWith(wxString(STX) +"result") ) try
    {
        // Example:
        //{"id":"textDocument/signatureHelp","jsonrpc":"2.0","result":
        //    {"activeParameter":0,"activeSignature":0,
        //        "signatures":
        //        [
        //            {"label":"vector()","parameters":[]},
        //            {"label":"vector(const _Alloc &_Al)","parameters":[{"label":[7,24]}]},
        //            {"label":"vector(vector<_Ty, _Alloc> &&_Right)","parameters":[{"label":[7,35]}]},
        //            {"label":"vector(const vector<_Ty, _Alloc> &_Right)","parameters":[{"label":[7,40]}]},
        //            {"label":"vector(initializer_list<_Ty> _Ilist, const _Alloc &_Al = _Alloc())","parameters":[{"label":[7,35]},{"label":[37,65]}]},
        //            {"label":"vector(vector<_Ty, _Alloc> &&_Right, const _Alloc &_Al)","parameters":[{"label":[7,35]},{"label":[37,54]}]},
        //            {"label":"vector(const vector<_Ty, _Alloc> &_Right, const _Alloc &_Al)","parameters":[{"label":[7,40]},{"label":[42,59]}]},
        //            {"label":"vector(_Iter _First, _Iter _Last, const _Alloc &_Al = _Alloc())","parameters":[{"label":[7,19]},{"label":[21,32]},{"label":[34,62]}]}
        //        ]
        //    }
        //}

        size_t resultCount = pJson->at("result").size();
        if (not resultCount) return;

        // Nothing for ShowCalltip is ever in the signature array
        // Show Tootip vs ShowCalltip is so damn confusing !!!
        // **debugging**std::string dumpit = pJson->dump();

        size_t signatureCount = pJson->at("result").at("signatures").size();
        if (not signatureCount) return;

        json signatures = pJson->at("result").at("signatures");
        for (size_t labelndx=0; labelndx<signatureCount && labelndx<10; ++labelndx)
        {
                wxString labelValue = GetwxUTF8Str(signatures[labelndx].at("label").get<std::string>());
                v_SignatureTokens.push_back(cbCodeCompletionPlugin::CCCallTip(labelValue));
        }

        if (v_SignatureTokens.size() )
        {
            //re-invoke ccmanager cbEVT_EDITOR_CALLTIP now that we have hover data
            //int tooltipMode = Manager::Get()->GetConfigManager(wxT("ccmanager"))->ReadInt(wxT("/tooltip_mode"), 1);

            CodeBlocksEvent evt(cbEVT_SHOW_CALL_TIP);
            cbEditor* pEd = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
            cbStyledTextCtrl* stc = pEd->GetControl();
            wxPoint pt = stc->PointFromPosition(n_HoverLastPosition);
            evt.SetX(pt.x);
            evt.SetY(pt.y);
            evt.SetInt(stc->GetStyleAt(stc->GetCurrentPos()));
            evt.SetEditor(pEd);
            evt.SetExtraLong(0);
            evt.SetString(wxT("evtfrom menu"));

            //    CCLogger::Get()->DebugLog("---------------LSP:SignatureHelp Results:-----------");
            //    for(size_t itemidx=0; itemidx<v_SignatureTokens.size(); ++itemidx)
            //    {
            //        cbCodeCompletionPlugin::CCCallTip tkn = v_SignatureTokens[itemidx] ;
            //        wxString logMsg(wxString::Format("%d:%s", int(itemidx), tkn.tip  ));
            //        CCLogger::Get()->DebugLog(logMsg);
            //    }
            Manager::Get()->ProcessEvent(evt);
        }//endif SignatureTokens
    }//endif results try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("%s %s", __FUNCTION__, e.what());
        CCLogger::Get()->DebugLog(msg);
        cbMessageBox(msg);
    }
}//end OnLSP_SignatureHelpResponse
// ----------------------------------------------------------------------------
void Parser::OnLSP_RenameResponse(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;
    // ----------------------------------------------------
    // textDocument/rename event
    // ----------------------------------------------------

    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    cbEditor* pEditor = pEdMgr->GetBuiltinActiveEditor();
    if (not pEditor)
        return;
    ProjectFile* pProjectFile = pEditor->GetProjectFile();
    if (not pProjectFile)
        return;
    cbProject* pProject = pEditor->GetProjectFile()->GetParentProject();
    if (not pProject)
        return;

    wxString evtString = event.GetString();

    // ----------------------------------------------------------------------------
    ///  GetClientData() contains a unique_ptr to json object
    ///  do not free it, The return to OnLSP_Event will free it
    // ----------------------------------------------------------------------------
    json* pJson = (json*)event.GetClientData();

    if (evtString.StartsWith("textDocument/rename") )
    {
            // Example data: (note: there are no carriage returns in the real data)
            // {"id":"textDocument/rename","jsonrpc":"2.0","result":
            //    {"changes":
            //        {"file://F:/usr/Proj/HelloWxWorld/HelloWxWorldAddition.cpp":
            //            [
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":20,"line":42},"start":{"character":0,"line":42}}},
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":42,"line":42},"start":{"character":22,"line":42}}},
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":20,"line":46},"start":{"character":0,"line":46}}},
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":43,"line":46},"start":{"character":23,"line":46}}}
            //            ],
            //         "file://F:/usr/Proj/HelloWxWorld/HelloWxWorldAddition.h":
            //             [
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":26,"line":18},"start":{"character":6,"line":18}}},
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":28,"line":21},"start":{"character":8,"line":21}}},
            //                {"newText":"HelloWxWorldAdditionRenamed","range":{"end":{"character":29,"line":22},"start":{"character":9,"line":22}}}
            //             ]
            //        }
            //    }
            // }

        const wxString editorFile = pEditor->GetFilename();
        wxFileName fn(editorFile);
        const wxString editorBasePath(fn.GetPath());

        wxString prevNewText;
        int prevRangeStartLine = 0;
        int prevRangeEndLine   = 0; wxUnusedVar(prevRangeEndLine);
        int prevRangeStartCol  = 0; wxUnusedVar(prevRangeStartCol);
        int prevRangeEndCol    = 0; wxUnusedVar(prevRangeEndCol);

        wxString curNewText;
        int curRangeStartLine = -1;
        int curRangeEndLine   = -1 ;
        int curRangeStartCol  = -1;
        int curRangeEndCol    = -1 ;

        try
        {
            json result = pJson->at("result");
            auto changes = result.at("changes").get<json::object_t>();
            for (auto& item : changes)
            {
                wxString URI = item.first;
                json fileChanges = item.second;
                wxFileName curFilename = fileUtils.FilePathFromURI(URI);
                wxString absFilename = curFilename.GetFullPath();
                if (not wxFileExists(absFilename))
                    return;

                // If the file does not belong to the active project, ignore it.
                // This is caused by dragging a project to a new directory and not
                // deleting the (now) invalid .cache and compile_commands_json files.
                wxFileName fnRelFilename = absFilename;
                fnRelFilename.MakeRelativeTo(wxPathOnly(pProject->GetFilename()));
                //-wxString lookie = fnRelFilename.GetFullPath(); // **Debugging**
                if (not pProject->GetFileByFilename(fnRelFilename.GetFullPath()))
                {
                    // Mark the project as needing to remove .cache and compile_commands.json
                    ProcessLanguageClient* pClient = GetLSPClient();
                    if (not pClient) continue;
                    pClient->SetProjectNeedsCleanup(pProject->GetFilename());
                    continue;
                }

                // verify already open file or re-open the affected file
                // verify if the file is already opened in built-in editor and do search in it
                cbEditor* ed = pEdMgr->IsBuiltinOpen(absFilename);
                cbStyledTextCtrl* control = ed ? ed->GetControl() : nullptr;
                if (not ed)
                {
                    ProjectFile* pf = pProject ? pProject->GetFileByFilename(absFilename) : 0;
                    ed = pEdMgr->Open(absFilename, 0, pf);
                }
                if (!ed) return;

                control = ed->GetControl();
                control->BeginUndoAction();

                size_t fileChangeCount = fileChanges.size();
                wxString symbolToChange = GetParseManager()->GetRenameSymbolToChange();
                int adjustment = 0;

                for (size_t ii=0; ii<fileChangeCount; ++ii)
                {
                    // save the previous changes in case multiple changes on a line
                    prevNewText         = curNewText;
                    prevRangeStartLine  = curRangeStartLine;
                    prevRangeEndLine    = curRangeEndLine;
                    prevRangeStartCol   = curRangeStartCol;
                    prevRangeEndCol     = curRangeEndCol;

                    curNewText = GetwxUTF8Str(fileChanges[ii].at("newText").get<std::string>());
                    curRangeStartLine = fileChanges[ii].at("range").at("start").at("line").get<int>();
                    curRangeEndLine = fileChanges[ii].at("range").at("end").at("line").get<int>();
                    curRangeStartCol = fileChanges[ii].at("range").at("start").at("character").get<int>();
                    curRangeEndCol = fileChanges[ii].at("range").at("end").at("character").get<int>();

                    curFilename.MakeRelativeTo(editorBasePath);

                    int linePosn = control->PositionFromLine(curRangeStartLine);  //begining of line
                    int colPosn  = linePosn + curRangeStartCol;
                    //int lineLth  = control->LineLength(curRangeStartLine);        // end of line
                    int pos = colPosn;
                    if (curRangeStartLine == prevRangeStartLine)
                    {
                        // adjust next change posn when same line being changed multiple times
                        adjustment +=  (curNewText.Length() - symbolToChange.Length() );
                        pos += adjustment; //new posn of target
                    }
                    else adjustment = 0;

                    control->SetTargetStart(pos);
                    control->SetTargetEnd(pos + symbolToChange.Length());
                    // symboToChange was saved during the initial request dialog CodeCompletion::OnRenameSymbols()
                    control->SetSearchFlags(wxSCI_FIND_MATCHCASE | wxSCI_FIND_WHOLEWORD | wxSCI_FIND_WORDSTART);
                    int tgtStart = control->SearchInTarget(symbolToChange);
                    if (tgtStart > -1)
                        control->ReplaceTarget(curNewText);

                }//endfor file changes

                control->EndUndoAction();

            }//endfor item changes

        }//end OnLSP_RenameResponse() try
        catch (std::exception &e)
        {
            wxString msg = wxString::Format("OnLSP_RenameResponse %s", e.what());
            CCLogger::Get()->DebugLog(msg);
            cbMessageBox(msg);
        }
    }//endif "textDocument/rename"

}//end OnLSP_RenameResponse
// ----------------------------------------------------------------------------
void Parser::OnLSP_GoToPrevFunctionResponse(wxCommandEvent& event)  //response from LSPserver
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;

    // ----------------------------------------------------------------------------
    // textDocument/DocumentSymbol event
    // ----------------------------------------------------------------------------
    if (event.GetString().StartsWith("textDocument/documentSymbol") )
    try
    {
        //{"jsonrpc":"2.0","id":"textDocument/documentSymbol","result":[{"name":"wxbuildinfoformat","detail":"enum wxbuildinfoformat {}","kind":10,"range":{"start":{"line":19,"character":0},"end":{"line":20,"character":21}},"selectionRange":{"start":{"line":19,"character":5},"end":{"line":19,"character":22}},"children":[]},
        //  {"name":"short_f","detail":"short_f","kind":22,"range":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"selectionRange":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"children":[]},
        //  {"name":"long_f","detail":"long_f","kind":22,"range":{"start":{"line":20,"character":13},"end":{"line":20,"character":19}},"selectionRange":{"start":{"line":20,"character":13},"end":{"line":20,"character":19}},"children":[]},...{"name":...,...etc}]}
        // *pJson points to contents of array "result";

        // verify editor is still open
        EditorManager* edMan = Manager::Get()->GetEditorManager();
        cbEditor* ed = edMan->GetBuiltinActiveEditor();
        if (!ed)
            return;
        cbStyledTextCtrl* pStc = ed->GetControl();
        int currLine = pStc->GetCurrentLine();

        // GetClientData() contains ptr to json object
        /// dont free pJson,  OnLSP_Event will free it as a unique_ptr
        json* pJson = (json*)event.GetClientData();

        size_t resultCount = pJson->count("result");
        json valueResult = pJson->at("result");

        if (not resultCount )
        {
            cbMessageBox(_("LSP: No functions parsed in this file..."));
            return;
        }

        std::set<LSP_SymbolKind> symbolsSet = {LSP_SymbolKind::Function, LSP_SymbolKind::Method, LSP_SymbolKind::Constructor, LSP_SymbolKind::Namespace,LSP_SymbolKind::Class};
        std::vector<LSP_SymbolsTupleType> LSP_VectorOfSymbolsFound;
        LSP_GetSymbolsByType(pJson, symbolsSet, LSP_VectorOfSymbolsFound);
        if (not LSP_VectorOfSymbolsFound.size())
        {
            cbMessageBox(_("LSP: No functions parsed in this file..."));
            return;
        }
        // Reverse search for line number < current line
        for (size_t ii=LSP_VectorOfSymbolsFound.size(); ii-- > 0;)
        {
            LSP_SymbolsTupleType symbolTuple = LSP_VectorOfSymbolsFound[ii];
            {
                //wxString symName   = std::get<SYMBOL_NAME>(symbolTuple); // **DEBUGGING**
                int symLine   = std::get<SYMBOL_LINE_NUMBER>(symbolTuple);
                symLine += 1; //make 1 origin
                //-lineNumbers.push_back(symLine);
                int funcLineNum = (symLine > 0) ? (symLine - 1) : 1;
                if (funcLineNum < currLine )
                {
                    pStc->GotoLine(funcLineNum);
                    break;
                }
            }
        }//endfor LSP_VectorOfSymbolsFound

    }//endif textDocument/documentSymbol try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("LSP OnLSP_GoToPrevFunctionResponse: %s", e.what());
        CCLogger::Get()->DebugLog(msg);
    }

}//end OnLSP_GoToPrevFunctionResponse()
// ----------------------------------------------------------------------------
void Parser::OnLSP_GoToNextFunctionResponse(wxCommandEvent& event)  //response from LSPserver
// ----------------------------------------------------------------------------
{
    if (GetIsShuttingDown()) return;

    // ----------------------------------------------------------------------------
    // textDocument/DocumentSymbol event
    // ----------------------------------------------------------------------------
    if (event.GetString().StartsWith("textDocument/documentSymbol") )
    try
    {
        //{"jsonrpc":"2.0","id":"textDocument/documentSymbol","result":[{"name":"wxbuildinfoformat","detail":"enum wxbuildinfoformat {}","kind":10,"range":{"start":{"line":19,"character":0},"end":{"line":20,"character":21}},"selectionRange":{"start":{"line":19,"character":5},"end":{"line":19,"character":22}},"children":[]},
        //  {"name":"short_f","detail":"short_f","kind":22,"range":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"selectionRange":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"children":[]},
        //  {"name":"long_f","detail":"long_f","kind":22,"range":{"start":{"line":20,"character":13},"end":{"line":20,"character":19}},"selectionRange":{"start":{"line":20,"character":13},"end":{"line":20,"character":19}},"children":[]},...{"name":...,...etc}]}
        // *pJson points to contents of array "result";

        // verify the editor is still open
        EditorManager* edMan = Manager::Get()->GetEditorManager();
        cbEditor* ed = edMan->GetBuiltinActiveEditor();
        if (!ed)
            return;
        cbStyledTextCtrl* pStc = ed->GetControl();
        int currLine = pStc->GetCurrentLine();

        // GetClientData() contains ptr to json object
        /// dont free pJson, OnLSP_Event will free it as a unique_ptr
        json* pJson = (json*)event.GetClientData();

        json valueResult = pJson->at("result");
        size_t resultCount = pJson->count("result");

        if (not resultCount )
        {
            cbMessageBox(_("No functions parsed in this file..."));
            return;
        }

        int lastLineNum = pStc->LineFromPosition(pStc->GetLength()) ;

        std::set<LSP_SymbolKind> symbolsSet = {LSP_SymbolKind::Function, LSP_SymbolKind::Method, LSP_SymbolKind::Constructor, LSP_SymbolKind::Namespace,LSP_SymbolKind::Class};
        std::vector<LSP_SymbolsTupleType> LSP_VectorOfSymbolsFound;
        LSP_GetSymbolsByType(pJson, symbolsSet, LSP_VectorOfSymbolsFound);
        if (not LSP_VectorOfSymbolsFound.size())
        {
            cbMessageBox(_("LSP: No functions parsed in this file..."));
            return;
        }
        for (size_t ii=0; ii<LSP_VectorOfSymbolsFound.size(); ++ii)
        {
            // All this stuff is zero origin; currline and symLine included
            LSP_SymbolsTupleType symbolTuple = LSP_VectorOfSymbolsFound[ii];
            {
                //wxString symName   = std::get<SYMBOL_NAME>(symbolTuple); // *DEBUGGING*
                int symLine   = std::get<SYMBOL_LINE_NUMBER>(symbolTuple);
                int funcLineNum = (symLine<lastLineNum) ? symLine : lastLineNum;
                funcLineNum = (symLine < 0) ? 0 : funcLineNum;
                if (funcLineNum > (currLine) )
                {
                    pStc->GotoLine(funcLineNum);
                    break;
                }

            }//end if valueResult == functionType
        }//endfor entryCount
    }//endif textDocument/documentSymbol try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("%s(): %s", __FUNCTION__ , e.what());
        CCLogger::Get()->DebugLog(msg);
    }
}//end OnLSP_GoToNextFunctionResponse
// ----------------------------------------------------------------------------
void Parser::OnLSP_GoToFunctionResponse(wxCommandEvent& event)  //unused
// ----------------------------------------------------------------------------
{
    // currently UNUSED. Using the older CC method to go to function instead.
    // viz., CodeCompletion::OnGotoFunction() using the GoToFunctionDlg dialog
    // ----------------------------------------------------------------------------
    // textDocument/DocumentSymbol event
    // ----------------------------------------------------------------------------
    if (GetIsShuttingDown()) return;

    if (event.GetString().StartsWith("textDocument/documentSymbol") )
    try
    {
        //{"jsonrpc":"2.0","id":"textDocument/documentSymbol","result":[{"name":"wxbuildinfoformat","detail":"enum wxbuildinfoformat {}","kind":10,"range":{"start":{"line":19,"character":0},"end":{"line":20,"character":21}},"selectionRange":{"start":{"line":19,"character":5},"end":{"line":19,"character":22}},"children":[]},
        //  {"name":"short_f","detail":"short_f","kind":22,"range":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"selectionRange":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"children":[]},
        //  {"name":"long_f","detail":"long_f","kind":22,"range":{"start":{"line":20,"character":13},"end":{"line":20,"character":19}},"selectionRange":{"start":{"line":20,"character":13},"end":{"line":20,"character":19}},"children":[]},...{"name":...,...etc}]}
        // *pJson points to contents of array "result";

        EditorManager* edMan = Manager::Get()->GetEditorManager();
        cbEditor* ed = edMan->GetBuiltinActiveEditor();
        if (!ed)
            return;

        // GetClientData() contains ptr to json object
        // dont free it, OnLSP_Event will free it as a unique_ptr
        json* pJson = (json*)event.GetClientData();

        json valueResult = pJson->at("result");
        size_t resultCount = pJson->count("result");
        size_t entryCount = valueResult.size();

        if (not resultCount )
        {
            cbMessageBox(_("No functions parsed in this file..."));
            return;
        }

        //const size_t functionType = 12;; //defined in https://microsoft.github.io/language-server-protocol/specification
        //const size_t classType    = 5; //defined in https://microsoft.github.io/language-server-protocol/specification
        //const size_t methodType   = 6; //defined in https://microsoft.github.io/language-server-protocol/specification

        GotoFunctionDlg::Iterator iterator;
        size_t foundCount = 0;

        for (size_t ii=0; ii<entryCount; ++ii)
        {
            size_t symbolType = valueResult[ii].at("kind").get<int>();
            if ( (symbolType == LSP_SymbolKind::Function) or (symbolType == LSP_SymbolKind::Method)
                    or (symbolType == LSP_SymbolKind::Constructor) or (symbolType == LSP_SymbolKind::Class)
                    or (symbolType==LSP_SymbolKind::Namespace) )
            {
                foundCount += 1;
                wxString symName   = GetwxUTF8Str(valueResult[ii].at("name").get<std::string>());
                //- wxString symDetail = valueResult[ii].at("detail").get<std::string>(); CCLS only
                int      symLine   = valueResult[ii].at("range").at("start").at("line").get<int>();
                symLine += 1; //make 1 origin

                GotoFunctionDlg::FunctionToken ft;
                // We need to clone the internal data of the strings to make them thread safe. // This probably not true for LSP response
                ft.displayName = wxString(symName.c_str());
                ft.name = wxString(symName.c_str());
                ft.line =symLine;
                ft.implLine = symLine;
                //if (!token->m_FullType.empty())
                //-ft.paramsAndreturnType = wxString((symDetail).c_str()); CCLS only
                //ft.funcName = wxString((token->GetNamespace() + token->m_Name).c_str());
                ft.funcName = wxString((symName).c_str());
                iterator.AddToken(ft);
            }//end if valueResult == functionType
        }//endfor entryCount

        if (not foundCount )
        {
            cbMessageBox(_("LSP: No functions parsed in this file..."));
            return;
        }

        //
        iterator.Sort();
        GotoFunctionDlg dlg(Manager::Get()->GetAppWindow(), &iterator);
        PlaceWindow(&dlg);
        if (dlg.ShowModal() == wxID_OK)
        {
            int selection = dlg.GetSelection();
            if (selection != wxNOT_FOUND)
            {
                const GotoFunctionDlg::FunctionToken *ft = iterator.GetToken(selection);
                if (ed && ft)
                    ed->GotoTokenPosition(ft->implLine - 1, ft->name);
            }
        }

    }//endif textDocument/documentSymbol try
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("OnLSP_GoToFunctionResponse %s", e.what());
        CCLogger::Get()->DebugLog(msg);
        cbMessageBox(msg);
    }
}//end OnLSP_GoToFunctionResponse

// ----------------------------------------------------------------------------
bool Parser::LSP_GetSymbolsByType(json* pJson, std::set<LSP_SymbolKind>& symbolset, std::vector<LSP_SymbolsTupleType>& LSP_VectorOfSymbolsFound)
// ----------------------------------------------------------------------------
{
    /// Do Not free pJson, it will be freed in CodeCompletion::LSP_Event()
    bool debugging = false;

    // fetch filename from json id
    wxString URI;
    try{
        URI = GetwxUTF8Str(pJson->at("id").get<std::string>());
    }catch(std::exception &e) {
        cbMessageBox(wxString::Format("%s() %s",__FUNCTION__, e.what()));
        return false;
    }

    URI = URI.AfterFirst(STX);
    URI = URI.BeforeFirst(STX); //isolate the filename
    wxFileName fnFilename = fileUtils.FilePathFromURI(URI);
    wxString filename = fnFilename.GetFullPath();
    if (not wxFileExists(filename))
        return false;

    //-Token* savedLastParent = nullptr;

    if (debugging)
        CCLogger::Get()->DebugLog("-----------------symbols----------------");

    int nextVectorSlot = 0;

    try
    {
        json result = pJson->at("result");
        size_t defcnt = result.size();
        TRACE(wxString::Format("%s() json contains %d major symbols", __FUNCTION__, defcnt));
        for (size_t symidx=0; symidx<defcnt; ++symidx)
        {
            wxString name =   result.at(symidx)["name"].get<std::string>();
            int kind =        result.at(symidx)["kind"].get<int>();
            // startLine etc is the items lines ranges, eg., function start line to end line braces.
            int startLine =   result.at(symidx)["range"]["start"]["line"].get<int>();
            int startCol =    result.at(symidx)["range"]["start"]["character"].get<int>();
            int endLine =     result.at(symidx)["range"]["end"]["line"].get<int>();
            int endCol =      result.at(symidx)["range"]["end"]["character"].get<int>();
            // selection Range is the item name range, eg. function name
            int selectionRangeStartLine = result.at(symidx)["selectionRange"]["start"]["line"].get<int>();
            int selectionRangeStartCol =  result.at(symidx)["selectionRange"]["start"]["character"].get<int>();
            int selectionRangeEndLine =   result.at(symidx)["selectionRange"]["end"]["line"].get<int>();
            int selectionRangeEndCol =    result.at(symidx)["selectionRange"]["end"]["character"].get<int>();
            size_t childcnt = 0;
            childcnt = result.at(symidx).contains("children")?result.at(symidx)["children"].size() : 0;

            if (debugging) { //debugging
            CCLogger::Get()->DebugLog(wxString::Format("name[%s] kind(%d) startLine|startCol|endLine|endCol[%d:%d:%d:%d]", name, kind, startLine, startCol, endLine, endCol));
            CCLogger::Get()->DebugLog(wxString::Format("SelectionRange: startLine|StartCol|endLine|endCol[%d:%d:%d:%d]", selectionRangeStartLine, selectionRangeStartCol, selectionRangeEndLine, selectionRangeEndCol));
            CCLogger::Get()->DebugLog(wxString::Format("\tchildren[%d]", childcnt ));
            }

            //Note: start and end lines contain the whole definition/implementation code.
            //      selectionRange start and end lines are the token name/symbol only.
			if (symbolset.count((LSP_SymbolKind)kind))
            {
				LSP_SymbolsTupleType symTuple;
                int lineNum = selectionRangeStartLine;
				std::get<SYMBOL_LINE_NUMBER>(symTuple) = lineNum;
				std::get<SYMBOL_TYPE>(symTuple) = (LSP_SymbolKind)kind;
				std::get<SYMBOL_NAME>(symTuple) = name;
				LSP_VectorOfSymbolsFound.push_back(symTuple);

            }//if symbolset

            if (childcnt)
            {
                //-savedLastParent = m_LastParent; //save current parent level
                //-m_LastParent = newToken;
                json jChildren = result.at(symidx)["children"];
                WalkDocumentSymbols(jChildren, filename, nextVectorSlot, symbolset, LSP_VectorOfSymbolsFound);
                //-m_LastParent = savedLastParent; //back to prvious parent level
            }

        }//end for
    } catch (std::exception &e)
    {
        wxString msg = wxString::Format(_("%s() Error:%s"), __FUNCTION__, e.what());
        cbMessageBox(msg, _("json Exception"));
    }

   return true;
}//LSP_GetSymbolsByType
 // ----------------------------------------------------------------------------
void Parser::OnRequestCodeActionApply(wxCommandEvent& event) //(ph 2024/02/12)
// ----------------------------------------------------------------------------
{
    // Fetch and apply the clangd "Fix(s) available" entry for a filename and line number.
    // Fixes were stored in the Parser FixesAvailable map/vectors
    // obtained from the LSP textDocument/Diagnostics response.

    wxString msg;
    wxArrayString params = GetArrayFromString(event.GetString(),"|", true);
    wxString logFilename   = params[0];
    wxString logLineNumStr = params[1];
    wxString logText       = params[2];

    int lineNumInt = -1; //an impossible line number
    try { lineNumInt = std::stoi(logLineNumStr.ToStdString()); }
    catch(std::exception &e) { lineNumInt = -1;}

    if (logFilename.empty() or (not wxFileExists(logFilename)) or (lineNumInt == -1) )
    {
        msg = wxString::Format(_("%s or line %s not found.\n"), logFilename, logLineNumStr );
    }
    cbEditor* pEd = Manager::Get()->GetEditorManager()->GetBuiltinEditor(logFilename);
    if (not pEd)
    {   msg << wxString::Format(_("No open editor for filename:%s\n"), logFilename);
    }

    cbProject* pProject = pEd->GetProjectFile() ? pEd->GetProjectFile()->GetParentProject() : nullptr;
    if (not pProject)
    {
        msg << wxString::Format(_("No project found containing filename:\n %s."), logFilename);
    }

        // Example of a textDocument/Diagnostics CodeActive entry
        // {"edit":{"changes":{
        //                    "file:///F:/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp":[
        //                        {"newText":"int","range":{"end":{"character":8,"line":275},"start":{"character":4,"line":275}}}
        //                    ]
        //        }}} //end edit object changes,

    // Verify filename and line number have an entry in available fixes map "FixesAvailable"
    FixMap_t::iterator it_find = FixesAvailable.find(logFilename);
    if (it_find == FixesAvailable.end())
    {
        //There are no entries for this file
        msg << "There are no fixes available.\n";
    }
    // show any error messages and return
    if (msg.Length())
    {
        if (msg.EndsWith('\n')) msg.RemoveLast();
        InfoWindow::Display(_("Apply fix failed"), msg );
        return;
    }

    // Line numbers in codeActions are zero origin, lineNumInt is from the log; so it's 1 origin.
    // Find the fix entries in the map array that match this log filename, lineNum and title.
    wxString logLine = wxString::Format("%d", int(lineNumInt-1)); //clangd needs zero origin lines
    //wxString searchResult; // **Debugging**
    wxString codeActionReplaceStr; // starts with "{"newText":
    wxString codeActionErrLineStr;  // <int string>

    std::vector<wxString>FixesFound;

    // FixesAvailable map contains key:FullFilename element: "<fixText>" STX "line #" STX "<fixTitleText>
    // Find the fix
    FixMap_t::iterator it = FixesAvailable.find(logFilename);
    if (it != FixesAvailable.end())
    {
        std::vector<wxString>& mapVector = it->second;

        for (std::vector<wxString>::iterator elementIt = mapVector.begin(); elementIt != mapVector.end(); ++elementIt)
        {
            wxString& elementFixStr = *elementIt; // Pull the vector element string
            wxArrayString fixDataStrings = GetArrayFromString(elementFixStr, STX, true);
            wxString fixCodeActionStr = fixDataStrings[0]; //clangd new text replacement
            wxString fixErrorLineNum  = fixDataStrings[1]; //clangd error line number
            wxString fixTitleText     = fixDataStrings[2]; //clangd tix title

            // selected log line info and fix info must match
            if ( (fixErrorLineNum == logLine)
               and (logText.find(fixTitleText) != wxString::npos) )
            {
                FixesFound.push_back(fixCodeActionStr);
                break; //apply one at a time.
            }
        }
    }

    if (not FixesFound.size())
    {
        // show a "no fix available" message
        msg = wxString::Format( _("No available fixes found.\n %s"), __FUNCTION__);
        InfoWindow::Display(_("No Fixes"), msg);
        return;
    }

    // ----------------------------------------------------
    // Apply the fixes from right to left to avoid any column shift adjustments
    // on subsequent fix entries.
    // ----------------------------------------------------
    for (size_t ii=FixesFound.size(); ii-- > 0;) //bounds test is made first, then ii is decremented
    {
        const char STX = '\u0002';
        codeActionReplaceStr  = FixesFound[ii].BeforeFirst(STX);
        codeActionErrLineStr = FixesFound[ii].AfterFirst(STX);
        wxString newText;
        int newStartLine; int newStartLineCol; int newEndLine; int newEndLineCol;

        try {
            // std::string testData = "{\"newText\":\"int\",\"range\":{\"end\":{\"character\":8,\"line\":275},\"start\":{\"character\":4,\"line\":275}}}"; // **Debugging**
            nlohmann::json jCodeAction = nlohmann::json::parse(codeActionReplaceStr.ToStdString());
            newText    = jCodeAction["newText"].get<std::string>();
            newStartLine    = jCodeAction["range"]["start"]["line"] ;
            newStartLineCol = jCodeAction["range"]["start"]["character"] ;
            newEndLine      = jCodeAction["range"]["end"]["line"] ;
            newEndLineCol   = jCodeAction["range"]["end"]["character"] ;
        }
        catch(std::exception &err)
        {
            wxString errMsg(wxString::Format("ERROR: %s:%s", __FUNCTION__, err.what()) );
            CCLogger::Get()->DebugLogError(errMsg);
            return;
        }

        // pEd contains the cbEditor ptr from above
        cbStyledTextCtrl* pControl = pEd->GetControl();
         // Replace text; note that the startLine is from the log msg line, so it's 1 origin
        int linePosn = pControl->PositionFromLine(newStartLine); // use zero origin for line
        int targetStart = linePosn + newStartLineCol;
        pControl->SetTargetStart(targetStart);
        int lineEndPosn = pControl->PositionFromLine(newEndLine);
        int targetEnd = lineEndPosn + newEndLineCol;
        pControl->SetTargetEnd(targetEnd);
        pControl->ReplaceTarget(newText);
    }//endfor FixesFound

    // Reparse the editor after any changes
    if (FixesFound.size())
    {
        // Invokes OnSpecifiedFileReparse()
        cbPlugin* pPlgn = Manager::Get()->GetPluginManager()->FindPluginByName("clangd_client");
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, XRCID("idSpecifiedFileReparse"));
        evt.SetString(logFilename);
        pPlgn->AddPendingEvent(evt);
    }

    return;
}//OnRequestCodeActionApply
// ----------------------------------------------------------------------------
void Parser::WalkDocumentSymbols(json& jref, wxString& filename, int& nextVectorSlot, std::set<LSP_SymbolKind>& symbolset, std::vector<LSP_SymbolsTupleType>& LSP_VectorOfSymbolsFound)
// ----------------------------------------------------------------------------
{
    bool debugging = false;

    try {
        json result = jref;
        size_t defcnt = result.size();
        for (size_t symidx=0; symidx<defcnt; ++symidx)
        {
            wxString name =   result.at(symidx)["name"].get<std::string>();
            int kind =        result.at(symidx)["kind"].get<int>();
            int endCol =      result.at(symidx)["range"]["end"]["character"].get<int>();
            int endLine =     result.at(symidx)["range"]["end"]["line"].get<int>();
            int startCol =    result.at(symidx)["range"]["start"]["character"].get<int>();
            int startLine =   result.at(symidx)["range"]["start"]["line"].get<int>();
            int selectionRangeStartLine = result.at(symidx)["selectionRange"]["start"]["line"].get<int>();
            int selectionRangeStartCol =  result.at(symidx)["selectionRange"]["start"]["character"].get<int>();
            int selectionRangeEndLine =   result.at(symidx)["selectionRange"]["end"]["line"].get<int>();
            int selectionRangeEndCol =    result.at(symidx)["selectionRange"]["end"]["character"].get<int>();

            size_t childcnt = 0;
            childcnt = result.at(symidx).contains("children")?result.at(symidx)["children"].size() : 0;

            if (debugging) { //debugging
                //-pLogMgr->DebugLog(wxString::Format("%*sname[%s] kind(%d) startLine|startCol|endLine|endCol[%d:%d:%d:%d]", indentLevel*4, "",  name, kind, startLine, startCol, endLine, endCol));
                CCLogger::Get()->DebugLog(wxString::Format("name[%s] kind(%d) startLine|startCol|endLine|endCol[%d:%d:%d:%d]", name, kind, startLine, startCol, endLine, endCol));
                //-pLogMgr->DebugLog(wxString::Format("%*sSelectionRange: startLine|StartCol|endLine|endCol[%d:%d:%d:%d]", indentLevel*4, "",  selectionRangeStartLine, selectionRangeStartCol, selectionRangeEndLine, selectionRangeEndCol));
                CCLogger::Get()->DebugLog(wxString::Format("SelectionRange: startLine|StartCol|endLine|endCol[%d:%d:%d:%d]", selectionRangeStartLine, selectionRangeStartCol, selectionRangeEndLine, selectionRangeEndCol));
                //-pLogMgr->DebugLog(wxString::Format("%*s\tchildren[%d]", indentLevel*4, "",  childcnt ));
                CCLogger::Get()->DebugLog(wxString::Format("\tchildren[%d]", childcnt ));
            }

			if (symbolset.count((LSP_SymbolKind)kind))
            {
				LSP_SymbolsTupleType symTuple;
                int lineNum = selectionRangeStartLine;
				std::get<SYMBOL_LINE_NUMBER>(symTuple) = lineNum;
				std::get<SYMBOL_TYPE>(symTuple) = (LSP_SymbolKind)kind;
				std::get<SYMBOL_NAME>(symTuple) = name;
				LSP_VectorOfSymbolsFound.push_back(symTuple);
            }//if symbolset

            if (childcnt)
            {
                json jChildren = result.at(symidx)["children"];
                WalkDocumentSymbols(jChildren, filename, nextVectorSlot, symbolset, LSP_VectorOfSymbolsFound);
            }
        }//endfor
    } catch (std::exception &e)
    {
        wxString msg = wxString::Format(_("%s() Error:%s"), __FUNCTION__, e.what());
        cbMessageBox(msg, _("json Exception"));
    }

   return;
}
