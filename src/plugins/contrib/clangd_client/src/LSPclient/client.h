//
// Created by Alex on 2020/1/28.
//


#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H


#include "transport.h"
#include "protocol.h"
#include <thread>

//#include "windows.h"
#include "wx/event.h"
#include "wx/string.h"
#include <wx/app.h>                 // wxWakeUpIdle

#include "globals.h"
#include "sdk_events.h"

#if defined(_WIN32)
    #include "winprocess/asyncprocess/asyncprocess.h"          // cl asyncProcess
    #include "winprocess/asyncprocess/processreaderthread.h"   // cl asyncProcess
    #include "winprocess/asyncprocess/winprocess_impl.h"       // cl asyncProcess
    #include "winprocess/misc/fileutils.h" // FilePath{From/To}URI();
#else //unix
    #include "unixprocess/asyncprocess/asyncprocess.h"
    #include "unixprocess/asyncprocess/UnixProcess.h"
    #include "unixprocess/fileutils.h"
#endif //_WIN32


#include "editormanager.h"
#include "cbeditor.h"
#include "editorbase.h"
#include "cbstyledtextctrl.h"
#include "cbproject.h"
#include "lspdiagresultslog.h"
#include "logmanager.h"
#include "configmanager.h"
#include "encodingdetector.h"

#include "codecompletion/parser/parser.h" //(ph 2022/03/30)

class TextCtrlLogger;

// ----------------------------------------------------------------------------
class LanguageClient : public JsonTransport
// ----------------------------------------------------------------------------
{
    public:
        virtual ~LanguageClient() = default;
        const char STX = '\u0002';  //start of text indicator to separate items in the header

    public:
        //(ollydbg 2022/10/19) ticket #75 Thanks ollydbg
        // CB Clangd Client / Tickets / #75 LSP reports that a header is not found when opening the editor
        // in a parent folder.
        // https://sourceforge.net/p/cb-clangd-client/tickets/75/
        // By default, the compile_commands.json file is searched in the parent folder.
        // But we can set the specific path here, because in CodeBlocks,
        // the compile_commands.json file is in the same folder as the cbp file, not its parent.
        RequestID Initialize(option<DocumentUri> rootUri = {}, string_ref compilationDatabasePath = {}) {

            InitializeParams params;
            #if defined(_WIN32)
            params.processId = GetCurrentProcessId();
            #else
            params.processId = getpid();
            #endif
            params.rootUri = rootUri;
            //(ollydbg 2022/10/19) ticket #75
            // CB Clangd Client / Tickets / #75 LSP report that a header is not found when I open the editor
            // https://sourceforge.net/p/cb-clangd-client/tickets/75/
            // since the compile_commands.json file is in the same folder of the cbp file
            params.initializationOptions.compilationDatabasePath = compilationDatabasePath;

            return SendRequest("initialize", params);
        }

        RequestID Shutdown() {
            return SendRequest("shutdown");
        }

        RequestID Sync() {
            return SendRequest("sync");
        }

        void Exit() {
            SendNotify("exit");
        }

        void Initialized() {
            SendNotify("initialized");
        }

        RequestID RegisterCapability() {
            return SendRequest("client/registerCapability");
        }

        void DidOpen(DocumentUri uri, string_ref text, string_ref languageId = "cpp") {
            DidOpenTextDocumentParams params;
            params.textDocument.uri = std::move(uri);
            params.textDocument.text = text;
            params.textDocument.languageId = languageId;
            SendNotify("textDocument/didOpen", params);
        }

        void DidClose(DocumentUri uri) {
            DidCloseTextDocumentParams params;
            params.textDocument.uri = std::move(uri);
            SendNotify("textDocument/didClose", params);
        }

        void DidSave(DocumentUri uri) {
            DidCloseTextDocumentParams params;
            params.textDocument.uri = std::move(uri);
            SendNotify("textDocument/didSave", params);
        }

        void DidChange(DocumentUri uri, std::vector<TextDocumentContentChangeEvent> &changes,
                       option<bool> wantDiagnostics = {}) {
            DidChangeTextDocumentParams params;
            params.textDocument.uri = std::move(uri);
            params.contentChanges = std::move(changes);
            params.wantDiagnostics = wantDiagnostics;
            SendNotify("textDocument/didChange", params);
        }
        //(ph 2021/02/11)
        void NotifyDidChangeConfiguration(ConfigurationSettings &settings) {
            DidChangeConfigurationParams params;
            params.settings = std::move(settings);
            SendNotify("workspace/didChangeConfiguration", std::move(params));
        }

        RequestID RangeFomatting(DocumentUri uri, Range range) {
            DocumentRangeFormattingParams params;
            params.textDocument.uri = std::move(uri);
            params.range = range;
            return SendRequest("textDocument/rangeFormatting", params);
        }

        RequestID FoldingRange(DocumentUri uri) {
            FoldingRangeParams params;
            params.textDocument.uri = std::move(uri);
            return SendRequest("textDocument/foldingRange", params);
        }

        RequestID SelectionRange(DocumentUri uri, std::vector<Position> &positions) {
            SelectionRangeParams params;
            params.textDocument.uri = std::move(uri);
            params.positions = std::move(positions);
            return SendRequest("textDocument/selectionRange", params);
        }

        RequestID OnTypeFormatting(DocumentUri uri, Position position, string_ref ch) {
            DocumentOnTypeFormattingParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            params.ch = std::move(ch);
            return SendRequest("textDocument/onTypeFormatting", std::move(params));
        }

        RequestID Formatting(DocumentUri uri) {
            DocumentFormattingParams params;
            params.textDocument.uri = std::move(uri);
            return SendRequest("textDocument/formatting", std::move(params));
        }

        RequestID CodeAction(DocumentUri uri, Range range, CodeActionContext context) {
            CodeActionParams params;
            params.textDocument.uri = std::move(uri);
            params.range = range;
            params.context = std::move(context);
            return SendRequest("textDocument/codeAction", std::move(params));
        }

        RequestID Completion(DocumentUri uri, Position position, option<CompletionContext> context = {}) {
            CompletionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            params.context = context;
            return SendRequest("textDocument/completion", params);
        }

        RequestID SignatureHelp(DocumentUri uri, Position position) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/signatureHelp", std::move(params));
        }

        RequestID GoToDefinition(DocumentUri uri, Position position) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/definition", std::move(params));
        }

        RequestID GoToDefinitionByID(DocumentUri uri, Position position, std::string reqID)
        {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            //return SendRequest("textDocument/definition", std::move(params));
            return SendRequestByID("textDocument/definition", reqID, std::move(params));

        }

        RequestID GoToDeclaration(DocumentUri uri, Position position) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/declaration", std::move(params));
        }

        RequestID GoToDeclarationByID(DocumentUri uri, Position position, std::string reqID)
        {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            //-return SendRequest("textDocument/declaration", std::move(params));
            return SendRequestByID("textDocument/declaration", reqID, std::move(params));
        }

        RequestID References(DocumentUri uri, Position position) {
            ReferenceParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/references", std::move(params));
        }

        RequestID SwitchSourceHeader(DocumentUri uri) {
            TextDocumentIdentifier params;
            params.uri = std::move(uri);
            return SendRequest("textDocument/references", std::move(params));
        }

        RequestID Rename(DocumentUri uri, Position position, string_ref newName) {
            RenameParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            params.newName = newName;
            return SendRequest("textDocument/rename", std::move(params));
        }

        RequestID Hover(DocumentUri uri, Position position) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/hover", std::move(params));
        }

        RequestID HoverByID(DocumentUri uri, Position position, std::string reqID) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequestByID("textDocument/hover", reqID, std::move(params));
        }

        RequestID DocumentSymbol(DocumentUri uri) {
            DocumentSymbolParams params;
            params.textDocument.uri = std::move(uri);
            return SendRequest("textDocument/documentSymbol", std::move(params));
        }

        RequestID DocumentSymbolByID(DocumentUri uri, std::string reqID) {
            DocumentSymbolParams params;
            params.textDocument.uri = std::move(uri);
            return SendRequestByID("textDocument/documentSymbol", reqID, std::move(params));
        }

        RequestID DocumentColor(DocumentUri uri) {
            DocumentSymbolParams params;
            params.textDocument.uri = std::move(uri);
            return SendRequest("textDocument/documentColor", std::move(params));
        }

        RequestID DocumentHighlight(DocumentUri uri, Position position) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/documentHighlight", std::move(params));
        }
        RequestID SymbolInfo(DocumentUri uri, Position position) {
            TextDocumentPositionParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            return SendRequest("textDocument/symbolInfo", std::move(params));
        }
        RequestID TypeHierarchy(DocumentUri uri, Position position, TypeHierarchyDirection direction, int resolve) {
            TypeHierarchyParams params;
            params.textDocument.uri = std::move(uri);
            params.position = position;
            params.direction = direction;
            params.resolve = resolve;
            return SendRequest("textDocument/typeHierarchy", std::move(params));
        }
        RequestID WorkspaceSymbol(string_ref query) {
            WorkspaceSymbolParams params;
            params.query = query;
            return SendRequest("workspace/symbol", std::move(params));
        }
        RequestID ExecuteCommand(string_ref cmd, option<TweakArgs> tweakArgs = {}, option<WorkspaceEdit> workspaceEdit = {}) {
            ExecuteCommandParams params;
            params.tweakArgs = tweakArgs;
            params.workspaceEdit = workspaceEdit;
            params.command = cmd;
            return SendRequest("workspace/executeCommand", std::move(params));
        }
        RequestID DidChangeWatchedFiles(std::vector<FileEvent> &changes) {
            DidChangeWatchedFilesParams params;
            params.changes = std::move(changes);
            return SendRequest("workspace/didChangeWatchedFiles", std::move(params));
        }
        RequestID DidChangeConfiguration(ConfigurationSettings &settings) {
            DidChangeConfigurationParams params;
            params.settings = std::move(settings);
            return SendRequest("workspace/didChangeConfiguration", std::move(params));
        }
        RequestID SemanticTokensByID(DocumentUri uri, std::string reqID) {         //(ph 2021/03/16)
            DocumentSymbolParams params;
            params.textDocument.uri = std::move(uri);
            return SendRequestByID("textDocument/semanticTokens/full", reqID, std::move(params));
    }

    public:
        RequestID SendRequest(string_ref method, value params = json()) {
            RequestID id = method.str();
            request(method, params, id);
            return id;
        }

        RequestID SendRequestByID(string_ref method, string_ref reqID, value params = json())
        {
            RequestID id = method.str();
            if (reqID.size())
                id += STX + reqID.str();
            request(method, params, id);
            return id;
        }
        void SendNotify(string_ref method, value params = json()) {
            notify(method, params);
        }
};
// ----------------------------------------------------------------------------
class ProcessLanguageClient : public wxEvtHandler, private LanguageClient
// ----------------------------------------------------------------------------
{
    //public:
        //        HANDLE fReadIn = nullptr, fWriteIn = nullptr;
        //        HANDLE fReadOut = nullptr, fWriteOut = nullptr;
        //        PROCESS_INFORMATION fProcess = {nullptr};
    private:

        long            processServerPID = 0;

        #if defined(_WIN32)
        IProcess*       m_pServerProcess = nullptr;
        #else
        UnixProcess*    m_pServerProcess = nullptr;
        #endif

        int             m_idLSP_Process = wxNewId(); //event id for this client instance
        std::string     m_std_LSP_IncomingStr;
        bool            m_terminateLSP = false;
        int             m_LSP_UserEventID = -1;
        cbProject*      m_pCBProject;
        Parser*         m_pParser = nullptr;

        std::map<cbEditor*, int> m_FileLinesHistory;

        size_t          m_msDidChangeTimeBusy = 0;     //Future time:  in eon milliSecs + busy milliseconds allowed
        size_t          m_msCompletionTimeBusy = 0;    //Future time:  in eon milliSecs + busy milliseconds allowed
        bool            m_LSP_initialized = false;
        bool            m_LSP_responseStatus = false;

        // FIXME (ph#): This is no longer needed since clangd version 12
        ////-int             m_LSP_CompileCommandsChangedTime = 0; //contains eon time-of-day in milliseconds

        wxArrayString   m_LSP_aIgnoredDiagnostics;

        //-std::map<cbEditor*,int> m_ParseStartMillsTODmap; //key:cbEditor* value: millisecs TOD time-of-eon

        static LSPDiagnosticsResultsLog* m_pDiagnosticsLog;

        // Default completion max busy time allowed is 2 secs
        // Set the current time + time allowed to be busy
        void SetCompletionTimeBusy(int msTime = 2000)
            { m_msCompletionTimeBusy = msTime ? (msTime + GetNowMilliSeconds()) : 0;}

        bool GetCompletionTimeBusy()
            {   // if a time has been set, return diff(previvouly set future time - now time).
                if (m_msCompletionTimeBusy)
                    return (m_msCompletionTimeBusy > GetNowMilliSeconds());
                return false;
            }

        // Default didModify max busy time allowed is 2 secs
        // Set the current time + time allowed to be busy
        // Cleared on receiving textDocument/publishDiagnostics
        void SetDidChangeTimeBusy(int msTime = 2000)
            { m_msDidChangeTimeBusy = msTime ? (msTime + GetNowMilliSeconds()) : 0;}

        bool GetDidChangeTimeBusy()
            {   // if a time has been set, return diff(previvouly set future time - now time).
                if (m_msDidChangeTimeBusy)
                {
                    //wxString msg = (m_msDidChangeTimeBusy > GetNowMilliSeconds())?"true":"false";
                    //Manager::Get()->GetLogManager()->DebugLog(LSP_GetTimeHMSM() +" DidChangeTime Waiting:"+ msg);
                    return (m_msDidChangeTimeBusy > GetNowMilliSeconds());
                }
                return false;
            }

        MapMessageHandler m_MapMsgHndlr; //LSP output to our input thread
        std::thread* m_pJsonReadThread = nullptr;
        // return code 0=running; 1=terminateRequested; 2=Terminated
        int jsonTerminationThreadRC = 0;

        int  GetCompilationDatabaseEntry(wxArrayString& returnArray, cbProject* pProject, wxString filename);
        void UpdateCompilationDatabase(cbProject* pProject, wxString filename);
        bool AddFileToCompileDBJson(cbProject* pProject, ProjectBuildTarget* pTarget, const wxString& argFullFilePath, json* pJson);    //(ph 2021/01/4)
        wxArrayString GetCompileFileCommand(ProjectBuildTarget* target, ProjectFile* pf) const ;
        size_t GetCompilerDriverIncludesByFile(wxArrayString& resultArray, cbProject* pProject, wxString filename);
        wxString CreateLSPClientLogName(int pid, const cbProject* pProject); //(ph 2021/02/12)

        // \brief Check if the command line is too long for the current running system and create a response file if necessary
        //
        // If the command line (executableCmd) used to execute a command is longer than the operating system limit
        // this function breaks the command line at the limit of the OS and generates a response file in the path folder.
        // Then it replaces the original command line with a constructed command line with the added response file and a pre-appended '@' sign.
        // This is the common marker for response files for many compilers.
        // The command is spit, so that the final command with the response file does not exceed the command line length limit.
        // The filename of the response file is generated from the executable name (string part until the first space from left)
        // and the base name. This name has to be unique.
        // The length limit of the operating system can be overwritten with the CB_COMMAND_LINE_MAX_LENGTH defined during compile time
        // If it is not defined at compile time reasonable limits for windows and UNIX are used.
        //
        // For example:
        //  mingw32-gcc.exe -i this/is/in/the/line -i this/is/longer/then/the/operating/system/supports
        //
        // gets transformed to
        //  mingw32-gcc.exe -i this/is/in/the/lime -i @path/to/response/file.respfile
        //
        // \param[in,out] executableCmd The command line input to check. Returns the modified command line if it was too long
        // \param[in,out] outputCommandArray The command queue. Some logging information is added so it can appear in the output log
        // \param[in] basename A base name, used to create a unique name for the response file
        // \param[in] path Base path where the response file is created
        void CheckForTooLongCommandLine(wxString& executableCmd, wxArrayString& outputCommandArray, const wxString& basename ,const wxString& path) const;

        void CreateDiagnosticsLog(); //Formated Diagnostic messages from LSP server

        // Number to append to log filename to differentiate logs
        int  GetLogIndex(const wxString& logRequest); //(ph 2021/01/14)
        // Set when LSP responds to the initialize request
        bool GetLSP_Initialized() {return m_LSP_initialized;}
        void SetLSP_Initialized(bool trueOrFalse) {m_LSP_initialized = trueOrFalse;}
        // Return ptr to server process for this client instance
        #if defined(_WIN32)
        IProcess*  GetLSP_Server(){return m_pServerProcess;}
        #else
        UnixProcess* GetLSP_Server() {return m_pServerProcess;}
        #endif

        /* Get the Request/Response id number from the LSP message header*/
        wxString GetRRIDvalue(wxString& lspHdrString);

    private:
        std::map<wxString, wxString> m_LSP_LastRequestPerFile;
    public:
        void SetLastLSP_Request(wxString filename, wxString lspRequest)
            {m_LSP_LastRequestPerFile[filename] = lspRequest;}
        wxString GetLastLSP_Request(wxString filename)
            {return m_LSP_LastRequestPerFile[filename];}

    private:
        std::map<wxString,int> m_LSP_CurrBackgroundFilesParsing;

        //-#if not defined(_WIN32)
        FileUtils fileUtils;
        //-#endif

    public:
        void LSP_AddToServerFilesParsing(wxString fname)
            {   wxString filename = fname;
                filename.Replace("\\", "/");
                m_LSP_CurrBackgroundFilesParsing[filename] = GetNowMilliSeconds();
            }
        void LSP_RemoveFromServerFilesParsing(wxString fname)
            {   wxString filename = fname;
                filename.Replace("\\", "/");
                m_LSP_CurrBackgroundFilesParsing.erase(filename);
            }
        size_t LSP_GetServerFilesParsingCount(){return m_LSP_CurrBackgroundFilesParsing.size();}

        int LSP_GetServerFilesParsingStartTime(wxString fname)
            {   wxString filename = fname;
                filename.Replace("\\", "/");
                std::map<wxString,int>::iterator it;
                if ( m_LSP_CurrBackgroundFilesParsing.find(filename) == m_LSP_CurrBackgroundFilesParsing.end())
                    return 0;
              return m_LSP_CurrBackgroundFilesParsing[filename];
            }
        int LSP_GetServerFilesParsingDurationTime(wxString filename)
        {
            int startTime = LSP_GetServerFilesParsingStartTime(filename);
            if (not startTime) return 0;
            return GetDurationMilliSeconds(startTime);
        }
        bool IsServerFilesParsing(wxString fname)
        {   wxString filename = fname;
            filename.Replace("\\", "/");
            if ( m_LSP_CurrBackgroundFilesParsing.find(filename) != m_LSP_CurrBackgroundFilesParsing.end())
                return true;
            return false;
        }
        void ListenForSavedFileMethod();
        void SetSaveFileEventOccured(wxCommandEvent& event);
        bool GetSaveFileEventOccured();

    public:
        // Return PID of the server process
        long       GetLSP_Server_PID(){return processServerPID;}

        // Test a processID for still alive
        bool IsAlive();

        // Used as the event id for this client
        int        GetLSP_EventID(){return m_idLSP_Process;}

        // true when the client for this project has received the initialize response
        bool       GetLSP_Initialized(cbProject* pProject)
                    { return GetLSP_Initialized(); }

        // true when the editor/project has received the first LSP response
        bool       GetLSP_Initialized(cbEditor* pEditor)
                    { return GetLSP_Initialized(); }

        // ptr to the server error/diagnostics log for this client
        LSPDiagnosticsResultsLog* LSP_GetLog() { return m_pDiagnosticsLog;}

        // ID to use when defining clinet/server events
        void       SetLSP_UserEventID(int id) {m_LSP_UserEventID = id;}
        int        GetLSP_UserEventID() {return m_LSP_UserEventID;}

        // The project respresented by this client/server
        void       SetCBProject(cbProject* pProject) {m_pCBProject = pProject;}
        void       SetParser(Parser* pParser) {m_pParser = pParser;} //(ph 2022/03/30)
        cbProject* GetClientsCBProject(){return m_pCBProject;}

        wxString GetwxUTF8Str(const std::string stdString)
        {
            return wxString(stdString.c_str(), wxConvUTF8);
        }

        // For this project, a map containing cbEditor*(key), value: tuple<LSP serverFileOpenStatus, caretPosition, isParsed>
        #define EDITOR_STATUS_IS_OPEN 0         // editors file is open in server
        #define EDITOR_STATUS_CARET_POSITION 1  //eg., 1234
        #define EDITOR_STATUS_READY 2           //Is parsed
        #define EDITOR_STATUS_MODIFIED 3        //Is modified
        #define EDITOR_STATUS_HAS_SYMBOLS 4     // document symbols responded
        typedef std::tuple<bool,int,bool,bool,bool> LSP_EditorStatusTuple; //fileOpenInServer, editorPosn, editor is ready, editor is modified, docSymbols received
        const LSP_EditorStatusTuple emptyEditorStatus = LSP_EditorStatusTuple(false,0,false,false,false);
        std::map<cbEditor*,LSP_EditorStatusTuple> m_LSP_EditorStatusMap;

        // ----------------------------------------------------------------------------
        LSP_EditorStatusTuple GetLSP_EditorStatus(cbEditor* pEditor)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "GetLSP_EditorStatus():null pEditor parm");
            #endif
            if (not pEditor) return emptyEditorStatus;
            if (m_LSP_EditorStatusMap.find(pEditor) != m_LSP_EditorStatusMap.end())
                return m_LSP_EditorStatusMap[pEditor];
            else return emptyEditorStatus;
        }

        // ----------------------------------------------------------------------------
        void SetLSP_EditorStatus(cbEditor* pEditor, LSP_EditorStatusTuple LSPeditorStatus)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return;
            // Only new "didOPen" editors are added to the map
            //if ((m_LSP_EditorStatusMap.find(pEditor) != m_LSP_EditorStatusMap.end())
            //    and (std::get<EDITOR_STATUS_LSPID>(LSPeditorStatus).Contains("textDocument/didOpen")) )

            m_LSP_EditorStatusMap[pEditor] = LSPeditorStatus;
        }
        // ----------------------------------------------------------------------------
        void SetLSP_EditorRemove(cbEditor* pEditor)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return;
            if (m_LSP_EditorStatusMap.find(pEditor) != m_LSP_EditorStatusMap.end())
                m_LSP_EditorStatusMap.erase(pEditor);
        }
        // ----------------------------------------------------------------------------
        bool GetLSP_IsEditorModified(cbEditor* pEditor)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return false;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            if ( std::get<EDITOR_STATUS_MODIFIED>(edStatus) == true )
                return true;
            return false;
        }
        // ----------------------------------------------------------------------------
        void SetLSP_EditorModified(cbEditor* pEditor, bool trueOrFalse)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            std::get<EDITOR_STATUS_MODIFIED>(edStatus) = trueOrFalse;
            SetLSP_EditorStatus(pEditor, edStatus);
        }
        // ----------------------------------------------------------------------------
        bool GetLSP_IsEditorParsed(cbEditor* pEditor)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return false;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            if ( std::get<EDITOR_STATUS_READY>(edStatus) == true )
                return true;
            return false;
        }
        // ----------------------------------------------------------------------------
        void SetLSP_EditorIsParsed(cbEditor* pEditor, bool trueOrFalse)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            std::get<EDITOR_STATUS_READY>(edStatus) = trueOrFalse;
            SetLSP_EditorStatus(pEditor, edStatus);
        }
        // ----------------------------------------------------------------------------
        void SetLSP_EditorIsOpen(cbEditor* pEditor, bool trueOrFalse)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            std::get<EDITOR_STATUS_IS_OPEN>(edStatus) = trueOrFalse;
            SetLSP_EditorStatus(pEditor, edStatus);
        }
        // ----------------------------------------------------------------------------
        bool GetLSP_EditorIsOpen(cbEditor* pEditor)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return false;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            if (std::get<EDITOR_STATUS_IS_OPEN>(edStatus) )
                return true;
            return false;
        }
        //(ph 2022/07/24)
        // ----------------------------------------------------------------------------
        bool GetLSP_EditorHasSymbols(cbEditor* pEditor)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return false;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            if ( std::get<EDITOR_STATUS_HAS_SYMBOLS>(edStatus) == true )
                return true;
            return false;
        }
        // ----------------------------------------------------------------------------
        void SetLSP_EditorHasSymbols(cbEditor* pEditor, bool trueOrFalse)
        // ----------------------------------------------------------------------------
        {
            #if defined(cbDEBUG)
            cbAssertNonFatal(pEditor && "null pEditor");
            #endif
            if (not pEditor) return;
            LSP_EditorStatusTuple edStatus = GetLSP_EditorStatus(pEditor);
            std::get<EDITOR_STATUS_HAS_SYMBOLS>(edStatus) = trueOrFalse;
            SetLSP_EditorStatus(pEditor, edStatus);
        }

    // ------------------------------------------------------------------------
    //Client callbacks             //(ph 2021/02/9)
    // ------------------------------------------------------------------------
    typedef void(ProcessLanguageClient::*LSP_ClientFnc)(cbEditor*);
    typedef std::map<cbEditor*, LSP_ClientFnc> LSP_ClientCallBackMap;
    LSP_ClientCallBackMap m_LSPClientCallBackSinks;

    // ----------------------------------------------------------------
    void SetLSP_ClientCallBack(cbEditor* pEditor, LSP_ClientFnc callback)
    // ----------------------------------------------------------------
    {
        m_LSPClientCallBackSinks.insert(std::pair<cbEditor*,LSP_ClientFnc>(pEditor,callback));
        wxWakeUpIdle();
    }

    // Map of include files from the compiler and added to the compile command line of compile_commands.json
    // Map of key:compilerID string, value:a string containing concatenated "-IcompilerIncludeFile"s
    // used by UpdateCompilationDatabase() avoiding unnecessary wxExecutes()'s
    std::map<wxString, wxString> CompilerDriverIncludesMap;

    wxFFile lspClientLogFile;
    wxFFile lspServerLogFile;

    explicit ProcessLanguageClient(const cbProject* pProject, const char* program = "", const char* arguments = "");
            ~ProcessLanguageClient() override;

    bool Has_LSPServerProcess();
    void OnClangd_stderr(wxThreadEvent& event);
    void OnClangd_stdout(wxThreadEvent& event);
    void OnLSP_PipedProcessTerminated(wxThreadEvent& event_pipedprocess_terminated);
    int  SkipLine();
    int  SkipToJsonData();
    int  ReadLSPinputLength();
    void ReadLSPinput(int dataPosn, int length, std::string &out);
    bool WriteHdr(const std::string &in);
    bool readJson(json &json) override;
    bool writeJson(json &json) override;
    bool DoValidateUTF8data(std::string& buffer);
    //-void OnLSP_Response(wxCommandEvent& event);
    void OnLSP_Response(wxThreadEvent& event);
    void OnIDMethod(wxCommandEvent& event);
    void OnIDResult(wxCommandEvent& event);
    void OnIDError(wxCommandEvent& event);
    void OnMethodParams(wxCommandEvent& event);
    void LSP_Shutdown();
    void OnLSP_Idle(wxIdleEvent& event);

    void LSP_Initialize(cbProject* pProject);
    bool LSP_DidOpen(cbEditor* pcbEd);
    bool LSP_DidOpen(wxString filename, cbProject* pProject); //(ph 2021/04/10)
    void LSP_DidClose(cbEditor* pcbEd);
    void LSP_DidClose(wxString filename, cbProject* pProject);
    void LSP_DidSave(cbEditor* pcbEd);
    void LSP_GoToDefinition(cbEditor* pcbEd, int edCaretPosition, size_t rrid=0);
    void LSP_GoToDeclaration(cbEditor* pcbEd, int edCaretPosition, size_t rrid=0);
    void LSP_FindReferences(cbEditor* pEd, int caretPosn);
    void LSP_RequestRename(cbEditor* pEd, int argCaretPosition, wxString newName);        //(ph 2021/10/12)
    void LSP_RequestSymbols(cbEditor* pEd, size_t rrid=0);
    void LSP_RequestSymbols(wxString filename, cbProject* pProject, size_t rrid=0);
    void LSP_RequestSemanticTokens(cbEditor* pEd, size_t rrid=0); //(ph 2021/03/16)
    void LSP_RequestSemanticTokens(wxString filename, cbProject* pProject, size_t rrid=0); //(ph 2022/06/8)
    void LSP_DidChange(cbEditor* pEd);
    void LSP_CompletionRequest(cbEditor* pEd, int rrid=0);
    void LSP_Hover(cbEditor* pEd, int posn, int rrid=0); //(ph 2022/06/15)
    void LSP_SignatureHelp(cbEditor* pEd, int posn);

    void writeClientLog(const std::string& logmsg);
    void writeServerLog(const std::string& logmsg);
    wxString GetTime();
    std::string LSP_GetTimeHMSM(); //Get time in hours minute second milliseconds
    std::string GetTime_in_HH_MM_SS_MMM();
    size_t GetNowMilliSeconds();
    size_t GetDurationMilliSeconds(int startMillis);

    bool ClientProjectOwnsFile(cbEditor* pcbEd, bool notify=true);
    cbProject* GetProjectFromEditor(cbEditor* pcbEd);

// No longer needed since clangd version 12
////    int   GetCompileCommandsChangedTime()
////        {return m_LSP_CompileCommandsChangedTime;}
////    void  SetCompileCommandsChangedTime(bool trueOrFalse)
////        {
////            if (trueOrFalse)
////                m_LSP_CompileCommandsChangedTime = GetNowMilliSeconds();
////            else m_LSP_CompileCommandsChangedTime = 0;
////        }

    // array of user designated log messages to ignore
    // ----------------------------------------------------------------------------
    wxArrayString& GetLSP_IgnoredDiagnostics()
    // ----------------------------------------------------------------------------
    {
        // Read persistent config array of ignored messages to the config
        ConfigManager* pCfgMgr = Manager::Get()->GetConfigManager("clangd_client");
        m_LSP_aIgnoredDiagnostics.Clear();
        pCfgMgr->Read("ignored_diagnostics", &m_LSP_aIgnoredDiagnostics);

        return m_LSP_aIgnoredDiagnostics;
    }

    cbStyledTextCtrl* GetNewHiddenEditor(const wxString& filename);              //(ph 2021/04/10)

    // Verify that an event handler is still in the chain of event handlers
    // ----------------------------------------------------------------------------
    wxEvtHandler* FindEventHandler(wxEvtHandler* pEvtHdlr)
    // ----------------------------------------------------------------------------
    {
        wxEvtHandler* pFoundEvtHdlr =  Manager::Get()->GetAppWindow()->GetEventHandler();

        while (pFoundEvtHdlr != nullptr)
        {
            if (pFoundEvtHdlr == pEvtHdlr)
                return pFoundEvtHdlr;
            pFoundEvtHdlr = pFoundEvtHdlr->GetNextHandler();
        }
        return nullptr;
    }
    // ----------------------------------------------------------------------------
    wxString GetEditorsProjectTitle(cbEditor* pEditor)
    // ----------------------------------------------------------------------------
    {
        if (pEditor->GetProjectFile() and pEditor->GetProjectFile()->GetParentProject() )
            return pEditor->GetProjectFile()->GetParentProject()->GetTitle();
        else return wxString();
    }
    // ----------------------------------------------------------------------------
    int GetEditorsCaretColumn(cbStyledTextCtrl* pCtrl)
    // ----------------------------------------------------------------------------
    {
        //Get the editors column position compensating for hard Tab chars
        //(ollydbg 2022/11/06 forum: Code completion using LSP and clangd msg#244)
        int edCaretPosn = pCtrl->GetCurrentPos();
        int edLineNum   = pCtrl->LineFromPosition(edCaretPosn);
        int linePos = pCtrl->PositionFromLine(edLineNum); // Retrieve the position at the start of a line.
        int fauxColumn = edCaretPosn - linePos;
        return fauxColumn;
    }

};
// ----------------------------------------------------------------------------
class ValidateUTF8vector
// ----------------------------------------------------------------------------
{
    public:
    ValidateUTF8vector(){;}

    bool validUtf8(std::vector<int>& data, std::vector<int>&badLocs)
    {
        for (int i = 0; i < int(data.size()); ++ i) {
            // 0xxxxxxx
            int bit1 = (data[i] >> 7) & 1;
            if (bit1 == 0) continue;
            // 110xxxxx 10xxxxxx
            int bit2 = (data[i] >> 6) & 1;
            if (bit2 == 0) /*return false;*/
                {badLocs.push_back(i); continue;}
            // 11
            int bit3 = (data[i] >> 5) & 1;
            if (bit3 == 0)
            {
                // 110xxxxx 10xxxxxx
                if ((++ i) < int(data.size()))
                {
                    if (is10x(data[i]))
                    {
                        continue;
                    }
                    //return false;
                    {badLocs.push_back(i); continue;}
                }
                else
                {
                    /*return false;*/
                    badLocs.push_back(i); continue;
                }
            }
            int bit4 = (data[i] >> 4) & 1;
            if (bit4 == 0) {
                // 1110xxxx 10xxxxxx 10xxxxxx
                if (i + 2 < int(data.size())) {
                    if (is10x(data[i + 1]) && is10x(data[i + 2])) {
                        i += 2;
                        continue;
                    }
                    /*return false;*/
                    {badLocs.push_back(i); continue;}

                }
                else {
                    /*return false;*/{badLocs.push_back(i); continue;}
                }
            }
            int bit5 = (data[i] >> 3) & 1;
            if (bit5 == 1) /*return false;*/{badLocs.push_back(i); continue;}

            if (i + 3 < int(data.size()))
            {
                if (is10x(data[i + 1]) && is10x(data[i + 2]) && is10x(data[i + 3]))
                {
                    i += 3;
                    continue;
                }
                /*return false;*/
                {badLocs.push_back(i); continue;}
            }
            else
            {
                /*return false;*/badLocs.push_back(i); continue;
            }
        }
        return true;
    }//end validUtf8

    private:
    inline bool is10x(int a) {
        int bit1 = (a >> 7) & 1;
        int bit2 = (a >> 6) & 1;
        return (bit1 == 1) && (bit2 == 0);
    }
}; //endClass ValidateUTF8vector

#endif //LSP_CLIENT_H
