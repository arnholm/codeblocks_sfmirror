#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <fstream>
#include <tuple>
#include <chrono>

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <wx/gauge.h>
    #include <wx/sizer.h>
    #include "wx/xrc/xmlres.h"
    #include <wx/event.h>
    #include <wx/debug.h>
    #include <wx/utils.h>

    #include "logmanager.h"
    #include "compilerfactory.h"
    #include "loggers.h"
    #include "configmanager.h"      //LSP diagnostics log
    #include "loggers.h"
    #include "encodingdetector.h"
    #include "globals.h"
#endif

#include "macrosmanager.h"
#include "compilercommandgenerator.h"
#include "wx/textfile.h"        //to modify .Clangd file containing log and cache file lock
#include "infowindow.h"

#include <lspdiagresultslog.h>   // LSP diagnostics log

#if defined(_WIN32)
    #include "winprocess/asyncprocess/procutils.h" // GetProcessNameByPid(pid)
#endif
#if not defined(_WIN32) //unix
    #include "procutils.h"
    #include "unixprocess/asyncprocess/UnixProcess.h"
    #include "unixprocess/asyncThreadTypes.h"
#endif

#include "ClangLocator.h"
#include "client.h"

// ----------------------------------------------------------------------------
namespace //annonymous
// ----------------------------------------------------------------------------
{
    wxString fileSep = wxFILE_SEP_PATH;
    // unused const char STX = '\u0002';
    int an_SavedFileMethod = 0;

    const int idBuildLog = wxNewId();

    #if defined(_WIN32)
        wxString clangexe("clang.exe");
        wxString clangdexe("clangd.exe");
    #else
        wxString clangexe("clang");
        wxString clangdexe("clangd");
    #endif

    // ----------------------------------------------------------------------------
    void StdString_ReplaceAll(std::string& str, const std::string& from, const std::string& to)
    // ----------------------------------------------------------------------------
    {
        if(from.empty())
            return;
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
        }
        return;
    }

    // ----------------------------------------------------------------------------
    __attribute__((used))
    void StdString_ReplaceSubstring(std::string& s, const std::string& f,
                                  const std::string& t)
    // ----------------------------------------------------------------------------
    {
        cbAssert(not f.empty());
        if (f.empty()) return;
        for (auto pos = s.find(f);                // find first occurrence of f
                pos != std::string::npos;         // make sure f was found
                s.replace(pos, f.size(), t),      // replace with t, and
                pos = s.find(f, pos + t.size()))  // find next occurrence of f
        {}
    }

    // ----------------------------------------------------------------------------
    __attribute__((used))
    void StdString_MakeLower(std::string& data)
    // ----------------------------------------------------------------------------
    {
        std::transform(data.begin(), data.end(), data.begin(),
            [](unsigned char c){ return std::tolower(c); });
    }

    // ----------------------------------------------------------------------------
    bool StdString_EndsWith(const std::string& str, const std::string& suffix)
    // ----------------------------------------------------------------------------
    {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // ----------------------------------------------------------------------------
    bool StdString_StartsWith(const std::string& str, const std::string& prefix)
    // ----------------------------------------------------------------------------
    {
        return str.size() >= prefix.size() &&
               str.compare(0, prefix.size(), prefix) == 0;
    }

    // ----------------------------------------------------------------------------
    bool StdString_Contains(const std::string& source, const std::string& token)
    // ----------------------------------------------------------------------------
    {
        //returns position of token in string
        size_t posn = source.find(token);
        if (posn != std::string::npos)
            return true;
        return false;
    }
    // ----------------------------------------------------------------------------
    //  String_trim or String_Reduce
    // ----------------------------------------------------------------------------
    __attribute__((used))
    std::string StdString_Trim(const std::string& str, const std::string& whitespace = " \t")
    {
        const auto strBegin = str.find_first_not_of(whitespace);
        if (strBegin == std::string::npos)
            return ""; // no content

        const auto strEnd = str.find_last_not_of(whitespace);
        const auto strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }
    __attribute__((used))
    // ----------------------------------------------------------------------------
    int StdString_FindOpeningEnclosureChar(const std::string& source, int index)
    // ----------------------------------------------------------------------------
    {
        // Find enclosure char such as () {} []
        // source string, src position of char to match(zero origin).
        // Returns zero origin index of paired char or -1.

        // Find index of Opening bracket, paren, brace for given opening char.

        int i;
        // Stack to store opening brackets.
        std::vector<int> st;
        char targetChar = '\0';
        char srcChar = source[index];

        // If index given is invalid and is
        // not an opening paren, bracket, or brace.
        if (srcChar == ')' ) targetChar = '(';
        if (srcChar == ']' ) targetChar = '[';
        if (srcChar == '}' ) targetChar = '{';
        if (targetChar == '\0')
        {
            wxString msg = wxString::Format("Error: %s failed:", __FUNCTION__);
            msg << source << ", " << srcChar << ", " << index << ": -1\n";
            Manager::Get()->GetLogManager()->DebugLog(msg);
            return -1;
        }

        // Traverse through string starting from
        // given index.
        for (i = index; i > -1; --i)
        {

            // If current character is an
            // opening bracket push it in stack.
            if(source[i] == srcChar)
              st.push_back(source[i]);

            // If current character is a closing
            // char, pop from stack. If stack
            // is empty, then this closing
            // char is the closing char.
            else if(source[i] == targetChar)
            {
                st.pop_back();
                if(st.empty())
                    return i;
            }
        }

        // If no matching closing bracket/paren/brace is found.
        wxString msg = wxString::Format("Error: %s failed:", __FUNCTION__);
        msg << source << ", " << srcChar << ", " << index << ": -1\n";
        Manager::Get()->GetLogManager()->DebugLog(msg);
        return -1;
    }
    // ----------------------------------------------------------------------------
    int StdString_FindClosingEnclosureChar(const std::string& source, int index)
    // ----------------------------------------------------------------------------
    {
        // Find enclosure char such as () {} []
        // source string, src position of char to match(zero origin).
        // Returns zero origin index of paired char or -1.

        // Find index of closing bracket, paren, brace for given opening char.

        int i;
        // Stack to store opening brackets.
        std::vector<int> st;
        char targetChar = '\0';
        char srcChar = source[index];

        // If index given is invalid and is
        // not an opening paren, bracket, or brace.
        if (srcChar == '(' ) targetChar = ')';
        if (srcChar == '[' ) targetChar = ']';
        if (srcChar == '{' ) targetChar = '}';
        if (targetChar == '\0')
        {
            wxString msg = wxString::Format("Error: %s failed:", __FUNCTION__);
            msg << source << ", " << srcChar << ", " << index << ": -1";
            Manager::Get()->GetLogManager()->DebugLog(msg);
            return -1;
        }

        // Traverse through string starting from
        // given index.
        for (i = index; i < (int)source.length(); i++)
        {

            // If current character is an
            // opening bracket push it in stack.
            if(source[i] == srcChar)
              st.push_back(source[i]);

            // If current character is a closing
            // char, pop from stack. If stack
            // is empty, then this closing
            // char is the closing char.
            else if(source[i] == targetChar)
            {
                st.pop_back();
                if(st.empty())
                    return i;
            }
        }

        // If no matching closing bracket is found.
        wxString msg = wxString::Format("Error: %s failed:", __FUNCTION__);
        msg << source << ", " << srcChar << ", " << index << ": -1\n";
        Manager::Get()->GetLogManager()->DebugLog(msg);

        return -1;
    }

    //    #if (This_Version_Causes_Errors)
    //    // https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    //    // Causes error: note:Cannot pass object of non-trivial type 'std::basic_string<char>' through variadic function; call will abort at runtime
    //    // ----------------------------------------------------------------------------
    //    template<typename ... Args>
    //    std::string StdString_Format( const std::string& format, Args ... args )
    //    // ----------------------------------------------------------------------------
    //    {
    //        int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    //        if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    //        auto size = static_cast<size_t>( size_s );
    //        std::unique_ptr<char[]> buf( new char[ size ] );
    //        std::snprintf( buf.get(), size, format.c_str(), args ... );
    //        return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
    //    }
    //    #endif

    // https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
    // C++11 solution that uses vsnprintf() internally:
    //#include <stdarg.h>  // For va_start, etc.
    // ----------------------------------------------------------------------------
    std::string StdString_Format(const std::string fmt, ...)
    // ----------------------------------------------------------------------------
    {
        int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
        std::string str;
        va_list ap;
        while (1) {     // Maximum two passes on a POSIX system...
            str.resize(size);
            va_start(ap, fmt);
            int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
            va_end(ap);
            if (n > -1 && n < size) {  // Everything worked
                str.resize(n);
                return str;
            }
            if (n > -1)  // Needed size returned
                size = n + 1;   // For null char
            else
                size *= 2;      // Guess at a larger size (OS specific)
        }
        return str;
    }

        __attribute__((used))
    // --------------------------------------------------------------
    wxString GetwxUTF8Str(const std::string stdString)
    // --------------------------------------------------------------
    {
        return wxString(stdString.c_str(), wxConvUTF8);
    }
    // --------------------------------------------------------------
    std::string GetstdUTF8Str(const wxString& wxStr)
    // --------------------------------------------------------------
    {
        wxScopedCharBuffer buf(wxStr.mb_str(wxConvUTF8));
        return std::string(buf.data(), buf.length());
    }

    bool wxFound(int result){return result != wxNOT_FOUND;};
    bool stdFound(size_t result){return result != std::string::npos;}

    static wxMutex m_MutexInputBufGuard; //jsonread buffer guard


}//end namespace
// ----------------------------------------------------------------------------
namespace ClientHelp
// ----------------------------------------------------------------------------
{
    // Log declarations taken from directcommands.cpp
    const wxString COMPILER_SIMPLE_LOG = _T("SLOG:");
    const wxString COMPILER_NOTE_LOG(_T("SLOG:NLOG:"));
    /// Print a NOTE log message to the build log, without advancing the progress counter
    const wxString COMPILER_ONLY_NOTE_LOG(_T("SLOG:ONLOG:"));
    const wxString COMPILER_WARNING_LOG(_T("SLOG:WLOG:"));
    const wxString COMPILER_ERROR_LOG(_T("SLOG:ELOG:"));
    const wxString COMPILER_TARGET_CHANGE(_T("SLOG:TGT:"));
    const wxString COMPILER_WAIT(_T("SLOG:WAIT"));
    const wxString COMPILER_WAIT_LINK(_T("SLOG:LINK"));

    const wxString COMPILER_NOTE_ID_LOG = COMPILER_NOTE_LOG.AfterFirst(wxT(':'));
    const wxString COMPILER_ONLY_NOTE_ID_LOG = COMPILER_ONLY_NOTE_LOG.AfterFirst(wxT(':'));
    const wxString COMPILER_WARNING_ID_LOG = COMPILER_WARNING_LOG.AfterFirst(wxT(':'));
    const wxString COMPILER_ERROR_ID_LOG = COMPILER_ERROR_LOG.AfterFirst(wxT(':'));

}//namespace clientHelp

// static initialization
LSPDiagnosticsResultsLog* ProcessLanguageClient::m_pDiagnosticsLog = nullptr;

// ----------------------------------------------------------------------------
ProcessLanguageClient::ProcessLanguageClient(const cbProject* pProject, const char* program, const char* arguments)
// ----------------------------------------------------------------------------
{
    LogManager* pLogMgr = Manager::Get()->GetLogManager();
    m_LSP_responseStatus = false;

    wxString tempDir = wxFileName::GetTempDir();

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));
    wxString cfgClangdMasterPath = cfg->Read("/LLVM_MasterPath", wxString());
    if (cfgClangdMasterPath.Length())
    {
        Manager::Get()->GetMacrosManager()->ReplaceMacros(cfgClangdMasterPath);
        if (not wxFileExists(cfgClangdMasterPath))
        {
            wxString msg; msg << _("The clangd path:\n") << cfgClangdMasterPath << _(" does not exist.");
            msg << _("\nClangd_Client will attempt to find it in some know locations.");
                cbMessageBox(msg, _("ERROR: Clangd client") );
        }
    }

    // Locate folder for Clangd
    ClangLocator clangLocator;
    wxString clangd_Dir;

    wxFileName fnClangdMasterPath(cfgClangdMasterPath);
    if (not fnClangdMasterPath.Exists())
    {
        // User did not specify a valid path to clangd
        fnClangdMasterPath.Clear();
        cfgClangdMasterPath.Empty();
    }
    // clang master path was obtained from settings clangd_client config
    if (cfgClangdMasterPath.Length())
        clangd_Dir = fnClangdMasterPath.GetPath();
    else clangd_Dir = clangLocator.Locate_ClangdDir();

    if (cfgClangdMasterPath.empty() and clangd_Dir.length() )
    {
        fnClangdMasterPath.AssignDir(clangd_Dir);
        fnClangdMasterPath.SetFullName(clangdexe);
    }

    wxString clangdResourceDir = clangLocator.Locate_ResourceDir(fnClangdMasterPath);
    if (clangd_Dir.empty() or clangdResourceDir.empty())
    {
        wxString msg; msg << "clangd_client plugin could not auto detect a clangd installation.";
        msg << "\nPlease enter the location of clangd using";
        msg << "\n MainMenu->Settings->Editor->clangd_client/C/C++ parser(tab)";
        msg << "\n 'clangd's installation location' using the button to the left";
        msg << "\n of 'Auto detect' to locate the clangd executable file";
        cbMessageBox( msg, "Error");

        msg.Empty(); msg << "Error: " << __FUNCTION__ << "() Could not find clangd installation.";
        if (clangd_Dir.empty()) msg << " clangd dir not found.";
        if (clangdResourceDir.empty()) msg << " clangd Resource dir not found.";
        CCLogger::Get()->LogError(msg);
        CCLogger::Get()->DebugLogError(msg);
        return;
    }

    // Set the clangd --query-dirver parameter
    Compiler* pCompiler = CompilerFactory::GetCompiler(pProject->GetCompilerID());
    if (not pCompiler)
    {
        wxString msg; msg << "Error: " << __FUNCTION__ << "() Could not find projects' compiler installation.";
        pLogMgr->Log(msg);
        return;
    }
    wxString masterPath = pCompiler ? pCompiler->GetMasterPath() : "";

    // get the first char of executable name from the compiler toolchain
    CompilerPrograms toolchain = pCompiler->GetPrograms();
    wxString toolchainCPP = toolchain.CPP.Length() ? wxString(toolchain.CPP[0]) : "";
    // " --query-driver=f:\\usr\\MinGW810_64seh\\**\\g*"
    wxString queryDriver = masterPath + fileSep + "**" + fileSep + toolchainCPP + "*";
    if (not platform::windows) queryDriver.Replace("\\","/");

    wxString pgmExec = clangd_Dir + fileSep + clangdexe;
    QuoteStringIfNeeded(pgmExec);

    wxString command = pgmExec + " --log=verbose";

    // Use users settings path of clangd if present (Settings=>editor=>clangd_client=>c/c++ parser=>clangd's installation location)
    if (fnClangdMasterPath.Exists())
    {
        pgmExec = fnClangdMasterPath.GetFullPath();
        QuoteStringIfNeeded(pgmExec);
        command = pgmExec + " --log=verbose";
    }

    QuoteStringIfNeeded(queryDriver);
    if (not platform::windows)
        queryDriver.Replace("\\","/"); //AndrewCo ticket 39  applied: 2022/05/25
    command += " --query-driver=" + queryDriver;

    // suggestion: -j=# should be no more than half of processors
    int max_parallel_processes = std::max(1, wxThread::GetCPUCount());
    if (max_parallel_processes > 1) max_parallel_processes = max_parallel_processes >> 1; //use only half of cpus

    // Restrict the "~ProxyProject~" to avoid excess cpu usage since active project will use half of processes     //(ph 2022/05/31)
    if (pProject->GetTitle() == "~ProxyProject~") max_parallel_processes = 1;

    // I dont think I want to set "maximum usable threads" to the same as "max parsing threads". Some threads should
    // be availablle to clangd for immediate foreground find/goto functions etc. while editors parse in the backgound.
    // Currently, the parser (OnLSP_BatchTimer) is limiting parallel parsing to the user specified config max.
    // I've been runnng 8 months with max usable threads set to 4 and max parsing threads set to 2 on my HP laptop (i7/2.5GHZ)
    // It works well. (only runs the fan when compiling and banging on clangd at the same time.)
    //- int cfg_parallel_processes = std::max(cfg->ReadInt("/max_threads", 1), 1);  //user specified config max, don't allow 0
    //- max_parallel_processes = std::min(max_parallel_processes, cfg_parallel_processes);

    command += " -j=" + std::to_string(max_parallel_processes) ;  // Number of async workers used by clangd. Background index also uses this many workers.

    // Some other parameter maybe useful in the future
    //?command += " --completion-style=bundled";   // Similar completion items (e.g. function overloads) are combined. Type information shown where possible
    //?command += " --background-index";           // Index project code in the background and persist index on disk.
    //-command += " --suggest-missing-includes";   // Attempts to fix diagnostic errors caused by missing includes using index

    // use --compile-commands-dir=<path> to restrict compilation database search to to <path>
    //https://clangd.llvm.org/faq#how-do-i-fix-errors-i-get-when-opening-headers-outside-of-my-project-directory

    // Get number of code completions returned from  config max_matches //(ph 2022/10/06)
    int ccMaxMatches = cfg->ReadInt(_T("/max_matches"), 20);
    command += wxString::Format(" --limit-results=%d", ccMaxMatches);

    if (wxDirExists(clangdResourceDir))
    {
        QuoteStringIfNeeded(clangdResourceDir);
        command += " --resource-dir=" + clangdResourceDir;  // Directory for system includes
    }

    // Show clangd start command in Code::Blocks Debug log
    CCLogger::Get()->DebugLog("Clangd start command:" + command);

  #if defined(_WIN32)  //<<------------windows only -------------------
    /** Info:
     * @brief start process
     * @param parent the parent. all events will be sent to this object
     * @param cmd command line to execute
     * @param flags possible creation flag
     * @param workingDir set the working directory of the executed process
     * @return
     */
    // Info:
    //IProcess* CreateAsyncProcess(wxEvtHandler* parent, const wxString& cmd,
    //                                            size_t flags = IProcessCreateDefault,
    //                                            const wxString& workingDir = wxEmptyString,
    //                                            const clEnvList_t* env = nullptr);
    //-#define SHOW_SERVER_CONSOLE
    m_pServerProcess = ::CreateAsyncProcess(this,
                    command,
                    #if defined(SHOW_SERVER_CONSOLE)
                    IProcessCreateConsole |
                    #endif
                    IProcessRawOutput | IProcessStderrEvent,
                    wxGetCwd()
                                           );

    //-wxMilliSleep(1000); // **Debugging**

  #else // _nix //<<------------------------unix only -------------------------------
    wxArrayString argsArrayString = GetArrayFromString(command, " ");
    m_pServerProcess = new UnixProcess(this, argsArrayString);

    if (not m_pServerProcess)
    {
        wxString msg = wxString::Format("%s: The child process for clangd failed allocation.", __FUNCTION__);
        //cbMessageBox(msg, "ERROR");
        CCLogger::Get()->DebugLogError(msg);
    }
  #endif //_WIN32 vs _nix

    if (m_pServerProcess) processServerPID = m_pServerProcess->GetPid();

    if ( (not m_pServerProcess) or (not IsAlive()) )
    {
        wxString msg = "Starting server for client failed:\n";
        msg << command ;
        msg << "\n\nTry entering the location of clangd using Setting->Editor->clangd_client C/C++ parser";
        cbMessageBox(msg, "clangd_client Error");
        return;
    }

    // -------------------------------------------------------
    // if logging, open a client log file
    // -------------------------------------------------------
    wxString logFilename;
    bool isClientLogging = cfg->ReadBool("/logClangdClient_check", false);
    if (isClientLogging)
    {
        logFilename = CreateLSPClientLogName(processServerPID, pProject); //(ph 2021/02/12)
        if (logFilename.Length())
        {
            wxString clientLogFilename = logFilename;
            lspClientLogFile.Open( clientLogFilename,"w" );
            if (not lspClientLogFile.IsOpened())
                cbMessageBox(wxString::Format(wxT("Failed to open %s"), clientLogFilename ));
            // write the project title to the log for identification
            //-std::string logLine = "Project: " + pProject->GetTitle().ToStdString() + ": " + pProject->GetFilename().ToStdString();
            std::string logLine = "Project: " + GetstdUTF8Str(pProject->GetTitle()) + ": " + GetstdUTF8Str(pProject->GetFilename()); //(ollydbg 2022/10/30) ticket #78
            writeClientLog(logLine);
            wxString envPath;
            wxGetEnv("PATH", &envPath);
            logLine = "SystemPath: " + envPath;
            writeClientLog(logLine);

        }
    }

    // if logging, open a server log file
    bool isServerLogging = cfg->ReadBool("/logClangdServer_check", false);
    if (isServerLogging)
    {
        if (logFilename.empty())
            logFilename = CreateLSPClientLogName(processServerPID, pProject);
        wxString serverLogFilename = logFilename;
        serverLogFilename.Replace("client", "server");
        lspServerLogFile.Open( serverLogFilename,  "w" );
        if (not lspServerLogFile.IsOpened())
            cbMessageBox(wxString::Format(wxT("Failed to open %s"), serverLogFilename ));
        // write the project title to the log for identification
        wxString logLine = "Project: " + pProject->GetTitle() + ": " + pProject->GetFilename()+ "\n";
        writeServerLog(logLine.ToStdString());
        wxString envPath;
        wxGetEnv("PATH", &envPath);
        logLine = "SystemPath: " + envPath + "\n";
        writeServerLog(logLine.ToStdString());
    }

    Manager::Get()->GetAppFrame()->PushEventHandler(this);
    Bind(wxEVT_ASYNC_PROCESS_OUTPUT, &ProcessLanguageClient::OnClangd_stdout, this);
    Bind(wxEVT_ASYNC_PROCESS_STDERR, &ProcessLanguageClient::OnClangd_stderr, this);
    Bind(wxEVT_ASYNC_PROCESS_TERMINATED, &ProcessLanguageClient::OnLSP_PipedProcessTerminated, this);
    //-Connect(GetLSP_ID(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ProcessLanguageClient::OnLSP_Response), nullptr, this);
    Connect(GetLSP_EventID(), wxEVT_COMMAND_MENU_SELECTED, wxThreadEventHandler(ProcessLanguageClient::OnLSP_Response), nullptr, this);
    Connect(wxEVT_IDLE, wxIdleEventHandler(ProcessLanguageClient::OnLSP_Idle) );

    if (not m_pDiagnosticsLog)
        CreateDiagnosticsLog();

    ListenForSavedFileMethod();
    // ----------------------------------------------------------------------------
    // Thread: start Language Server Process input reader
    // ----------------------------------------------------------------------------
    // The pipe to clangd has it's own thread. This one is the json analyzer.
    // the pipe thread stuffs data in the buffer while this thread takes it out.
    // The two can fight locking the buffer while the GUI thread takes events.
    // The GUI thread gets it's jason data from a wxThreadEvent issued by this
    // MapMsgHndlr thread via transport.h
    m_MapMsgHndlr.SetLSP_EventID(GetLSP_EventID());
    m_pJsonReadThread = new std::thread([&] {
        loop(m_MapMsgHndlr);
        //thread exits here and is deleted in the dtor
        jsonTerminationThreadRC = 2; //show thread has terminated
    });

}//end ProcessLanguageClient()
// ----------------------------------------------------------------------------
ProcessLanguageClient::~ProcessLanguageClient()
// ----------------------------------------------------------------------------
{
    // dtor

    m_terminateLSP = true; //tell the json read thread to terminate
    m_MapMsgHndlr.SetLSP_TerminateFlag(1);

    if (m_pServerProcess and platform::windows)
        m_pServerProcess->Detach(); //ignore any further messages //(ph 2022/08/08)
        //Detach() for linux is done in unixProcess dtor. Doing it here causes hangs and crashes

    if (lspClientLogFile.IsOpened())
        lspClientLogFile.Close();
    if (lspServerLogFile.IsOpened())
        lspServerLogFile.Close();


    Unbind(wxEVT_ASYNC_PROCESS_OUTPUT, &ProcessLanguageClient::OnClangd_stdout, this);
    Unbind(wxEVT_ASYNC_PROCESS_STDERR, &ProcessLanguageClient::OnClangd_stderr, this);
    Unbind(wxEVT_ASYNC_PROCESS_TERMINATED, &ProcessLanguageClient::OnLSP_PipedProcessTerminated, this);
    //-Disconnect(GetLSP_ID(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ProcessLanguageClient::OnLSP_Response), nullptr, this);
    Disconnect(GetLSP_EventID(), wxEVT_COMMAND_MENU_SELECTED, wxThreadEventHandler(ProcessLanguageClient::OnLSP_Response), nullptr, this);
    Disconnect(wxEVT_IDLE, wxIdleEventHandler(ProcessLanguageClient::OnLSP_Idle) );
    if (FindEventHandler(this))
        Manager::Get()->GetAppWindow()->RemoveEventHandler(this);

    // Remove the CodeBlocks LSP diagnostics log tab from log management window
    if (m_pDiagnosticsLog) switch(1)
    {
        default:

        size_t prjKnt = Manager::Get()->GetProjectManager()->GetProjects()->GetCount();
        if (prjKnt > 0) //dont remove a shared log
            break;

        // Tell DragScroll plugin this log is closing
        wxWindow* pWindow = m_pDiagnosticsLog->m_pControl;
        cbPlugin* pPlgn = Manager::Get()->GetPluginManager()->FindPluginByName(_T("cbDragScroll"));
        if (pWindow && pPlgn)
        {
                wxCommandEvent dsEvt(wxEVT_COMMAND_MENU_SELECTED, XRCID("idDragScrollRemoveWindow"));
                dsEvt.SetEventObject(pWindow);
                //Using ProcessEvent(), AddPendingEvent() does't work here. Wierd !!
                //Manager::Get()->GetAppFrame()->GetEventHandler()->ProcessEvent(dsEvt);
                pPlgn->ProcessEvent(dsEvt);

        }

        CodeBlocksLogEvent evt(cbEVT_REMOVE_LOG_WINDOW, m_pDiagnosticsLog);
        if (FindEventHandler(m_pDiagnosticsLog))
            Manager::Get()->GetAppWindow()->RemoveEventHandler(m_pDiagnosticsLog);
        Manager::Get()->ProcessEvent(evt);
        m_pDiagnosticsLog = nullptr; //LSPDiagnosticsLog was deleted during previous statment
    }

     if (m_pServerProcess and IsAlive()) {
        writeClientLog("~ProcessLanguageClient: Teminate process error!\n");
    }
    if (jsonTerminationThreadRC < 2) m_MapMsgHndlr.GetLSP_TerminateFlag();
    if ( m_pJsonReadThread and (jsonTerminationThreadRC < 2) )
    {
        // This occurs when this dtor executes before ReadJson() can process the clangd shutdown command.
        // Give the pipe (producer) and jsonRead (consumer) threads a chance to terminate
        for (int ii=10; (ii > 0) and (jsonTerminationThreadRC < 2); --ii)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            jsonTerminationThreadRC = m_MapMsgHndlr.GetLSP_TerminateFlag();
        }
    }
    if ( m_pJsonReadThread and (jsonTerminationThreadRC < 2) )
    {
        // return should have been 2 //0=running; 1=terminateRequested; 2=Terminated
        m_pJsonReadThread->join();
        wxString msg = wxString::Format("%s() Json read thread termination error rc:%d\n", __FUNCTION__, int(jsonTerminationThreadRC) );
        if (not Manager::IsAppShuttingDown()) //skip logging when shutting down, else we hang in linux
            CCLogger::Get()->DebugLogError(msg);
    }

    // setting terminateLSP above should have already caused read thread to exit.
    if (m_pJsonReadThread and m_pJsonReadThread->joinable() )
        m_pJsonReadThread->join();
    if (m_pJsonReadThread)
    {
        delete m_pJsonReadThread;
        m_pJsonReadThread = nullptr;
    }

    if (m_pServerProcess)
    {
        #if defined(_WIN32)
        IProcess* pProcess = m_pServerProcess;
        #else
        UnixProcess* pProcess = m_pServerProcess;
        #endif
        m_pServerProcess = nullptr;
        delete pProcess;
    }

    return;
}//end ~ProcessLanguageClient()
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::Has_LSPServerProcess()
// ----------------------------------------------------------------------------
{
    // data from LSP clangd stderr
    //Using pProcess->GetPid() does not work. It always gets the pid even tho it's <defunct>
    if ( (not m_pServerProcess) or (not IsAlive()) )
        return false;
    return true;
}

#if defined(_WIN32)
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::IsAlive() {return m_pServerProcess->IsAlive();}
// ----------------------------------------------------------------------------
#else //linux
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::IsAlive() //for linux
// ----------------------------------------------------------------------------
{
    int ProcessId = GetLSP_Server_PID();
    // Wait for child process, this should clean up defunct processes
    waitpid(ProcessId, nullptr, WNOHANG);
    // kill failed let's see why..
    if (kill(ProcessId, 0) == -1)
    {
        // First of all kill may fail with EPERM if we run as a different user
        // and we have no access, so let's make sure the errno is ESRCH (Process not found!)
        if (errno != ESRCH)
        {
            return true;
        }
        return false;
    }
    // If kill didn't fail the process is still running
    return true;
}
#endif

// ----------------------------------------------------------------------------
void ProcessLanguageClient::writeClientLog(const std::string& logmsg)
// ----------------------------------------------------------------------------
{
    if (not lspClientLogFile.IsOpened()) return;
    std::string logcr = "";
    if (not StdString_EndsWith(logmsg, "\n"))
        logcr = "\n";
    //-lspClientLogFile.Write("\n" + GetTime_in_HH_MM_SS_MMM() + " " + logmsg + logcr);
    std::string out = "\n" + GetTime_in_HH_MM_SS_MMM() + " " + logmsg + logcr;
    lspClientLogFile.Write(out.c_str(), out.size());
    lspClientLogFile.Flush();
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::writeServerLog(const std::string& logmsg)
// ----------------------------------------------------------------------------
{
    if (not lspServerLogFile.IsOpened()) return;
    lspServerLogFile.Write(logmsg.c_str(), logmsg.size());
    lspServerLogFile.Flush();

    //(ph 2022/02/16)
    // V[10:58:51.183] Reusing preamble version 0 for version 0 of F:\usr\Proj\HelloWorld\HelloWorld3.h
    // clangd does not always respond when it reuses a file (esp., with didOpen() )
    // So here we check if that's what happend by checking the server log response.
    // If so, we clear our "waiting for response" status flags
    if (StdString_Contains(logmsg,"Reusing preamble version")
            and StdString_Contains(logmsg, " for version "))
    {
        wxString filename;
        int filenamePosn = logmsg.find(" of ");
        if (stdFound(filenamePosn))
        {
            filename = logmsg.substr(filenamePosn+4);
            filename.Trim();    //remove CRLF or LF
            filename.Replace("\\", "/");
            cbEditor* pEditor = Manager::Get()->GetEditorManager()->IsBuiltinOpen(filename);
            if (pEditor)
            {
                LSP_RemoveFromServerFilesParsing(filename);
                SetLSP_EditorIsParsed(pEditor, true);
            }
        }
    }//endIf logmsg

}
// ----------------------------------------------------------------------------
wxString ProcessLanguageClient::GetTime()
// ----------------------------------------------------------------------------
{
    wxDateTime now = wxDateTime::Now();
    return now.Format("%H:%M:%S", wxDateTime::Local);
}
// ----------------------------------------------------------------------------
std::string ProcessLanguageClient::GetTime_in_HH_MM_SS_MMM()
// ----------------------------------------------------------------------------
{
    using namespace std::chrono;

    // get current time
    auto now = system_clock::now();

    // get number of milliseconds for the current second
    // (remainder after division into seconds)
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    // convert to std::time_t in order to convert to std::tm (broken time)
    auto timer = system_clock::to_time_t(now);
    // convert to broken time
    std::tm bt = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}
// ----------------------------------------------------------------------------
size_t ProcessLanguageClient::GetNowMilliSeconds()
// ----------------------------------------------------------------------------
{
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return millis;
}
// ----------------------------------------------------------------------------
size_t ProcessLanguageClient::GetDurationMilliSeconds(int startMillis)
// ----------------------------------------------------------------------------
{
    int nowMillis = GetNowMilliSeconds();
    return nowMillis - startMillis;
}
// ----------------------------------------------------------------------------
cbStyledTextCtrl* ProcessLanguageClient::GetNewHiddenEditor(const wxString& filename)              //(ph 2021/04/10)
// ----------------------------------------------------------------------------
{
    // Create new hidden editor and load its data

    wxString resultText;
    cbStyledTextCtrl* control = nullptr;

    if (wxFileExists(filename))
    {
        EditorManager* edMan = Manager::Get()->GetEditorManager();
        wxWindow* parent = Manager::Get()->GetAppWindow();
        control = new cbStyledTextCtrl(parent, wxID_ANY, wxDefaultPosition, wxSize(0, 0));
        control->Show(false);

        // check if the file is already opened in built-in editor
        cbEditor* ed = edMan->IsBuiltinOpen(filename);
        if (ed)
            control->SetText(ed->GetControl()->GetText());
        else // else load the file in the control
        {
            EncodingDetector detector(filename, false);
            if (not detector.IsOK())
            {
                wxString msg(wxString::Format("%s():%d failed EncodingDetector for %s", __FUNCTION__, __LINE__, filename));
                Manager::Get()->GetLogManager()->Log(msg);
                delete control;
                return nullptr;
            }
            control->SetText(detector.GetWxStr());
        }//else

    }//swith

    return control;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnClangd_stderr(wxThreadEvent& event)
// ----------------------------------------------------------------------------
{
    std::string* pErrStr = event.GetPayload<std::string*>();
    if ( pErrStr->length())
        writeServerLog( pErrStr->c_str() );

    return;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnClangd_stdout(wxThreadEvent& event)
// ----------------------------------------------------------------------------
{
    // This routine is driven by the clangd pipe thread.
    // Raw data from clangd server stdout event to this clients stdin.
    // concatenate LSP response to buffer.

    // **Debugging**
    // See the clangd incoming raw data
    //std::string* pRawData = event.GetPayload<std::string*>();
    //if (pRawData->length() == 0) { asm("int3");}
    //wxString utfData = wxString(pRawData->c_str(), wxConvUTF8);
    //writeClientLog(wxString::Format(">>> OnClangd_stdout() rawlen[%d] utflen[%d] currbuflen[%d]\n%s\n",
    //                    int(pRawData->length()), int(utfData.length()),  int(m_std_LSP_IncomingStr.length()) ,wxString(pRawData->c_str())) );

        /// ---Lock the clangd input buffer --------------------------
        wxMutexError lockerr = m_MutexInputBufGuard.Lock();

        if (lockerr != wxMUTEX_NO_ERROR)
        {
            wxString msg = wxString::Format("LSP data loss. %s() Failed to obtain input buffer lock", __FUNCTION__);
            wxSafeShowMessage("Lock fail, lost data", msg);
            CCLogger::Get()->DebugLogError(msg);
            writeClientLog(msg.ToStdString());
            return;
        }

    // Ignore any clangd data when app is shutting down. //(ph 2022/07/29)
    if (Manager::IsAppShuttingDown()) return;

    // Append clangd incomming response data to buffer;
    std::string* pRawOutput = event.GetPayload<std::string*>();
    if (not pRawOutput->size())
    {
        writeClientLog("Error: clangd responded with a zero length buffer.");
    }
    std::string std_clangdRawOutput = *pRawOutput;
    m_std_LSP_IncomingStr.append(*pRawOutput);

    /// unlock the input buffer
    m_MutexInputBufGuard.Unlock();
    return;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnLSP_PipedProcessTerminated(wxThreadEvent& event_pipedprocess_terminated)
// ----------------------------------------------------------------------------
{
    // Entered here when the LSP server terminates
    // This function called differently for Linux than MSWindows
    // For Linux, it's called when the server output pipe is closed and when unixProcess is Deleted.
    // For Windows, it's only called on an unexpected termination of winProcessImpl piped process.

    int processExitCode = 0; //no good, return code is always 0
    if (GetLSP_Initialized()) processExitCode = -1; //terminated while initialized and running.
    if (m_pServerProcess)
        m_terminateLSP = true;   //tell json read thread to exit.

    #if defined(_WIN32)
    if (m_pServerProcess->GetProcessExitCode(processServerPID, processExitCode))
    {
    }
    #endif

    if (m_pCBProject)
    {
        // Moved this code to ClgdCompletion::OnLSP_ProcessTerminated() because here
        // cbMessageBox() and wxMessageBox() causes an X Windows assert and crash on Linux Mint 20.2
        //
        //    wxString msg = _("Unusual termination of LanguageProcessClient(LSP) occured.");
        //    msg += "\n\nProject: " + m_pCBProject->GetTitle();
        //    if (lspClientLogFile.IsOpened() )
        //        msg += "\nClient Log: " + lspClientLogFile.GetName();
        //    if (lspServerLogFile.IsOpened() )
        //        msg += "\nServer Log: " + lspServerLogFile.GetName();
        //    #if defined(_WIN32)
        //    cbMessageBox(msg, "clangd client"); //Crashes with X window error on Linux Mint 20.2
        //    #else
        //    msg.Replace("\n\n","\n");
        //    Manager::Get()->GetLogManager()->LogError(msg);
        //    Manager::Get()->GetLogManager()->DebugLogError(msg);
        //    #endif

        // Notify ClgdCompletion that clangd process terminated.
        // This call cleans up and deletes both the client and the parser
        wxCommandEvent terminatedEvt(wxEVT_COMMAND_MENU_SELECTED, XRCID("idLSP_Process_Terminated"));
        terminatedEvt.SetEventObject((wxObject*)m_pCBProject);
        terminatedEvt.SetInt(processExitCode);
        //-Manager::Get()->GetAppFrame()->GetEventHandler()->ProcessEvent(terminatedEvt); can cause Linux Mint20.2 to hang
        Manager::Get()->GetAppFrame()->GetEventHandler()->AddPendingEvent(terminatedEvt);
    }
    return;
}//end OnLSP_PipedProcessTerminated()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnLSP_Idle(wxIdleEvent& event)
// ----------------------------------------------------------------------------
{
    event.Skip(); //always event.Skip() to allow others use of idle events

    if (Manager::IsAppShuttingDown()) return;       //(ph 2022/07/29)

    // ----------------------------------------------------------------------------
    // Invoke any queued client call backs
    // ----------------------------------------------------------------------------
    while (m_LSPClientCallBackSinks.size())
    {
        LSP_ClientCallBackMap::iterator it = m_LSPClientCallBackSinks.begin();
        if (it != m_LSPClientCallBackSinks.end() )
        {
                auto mem = it->second;      //function to call
                cbEditor* ed = it->first;   //editor to use
                it = m_LSPClientCallBackSinks.erase(it); //remove the entry
                (this->*mem)(ed);
                return; //get out bec editor may have set a new callback
        }
    }// end while

    return;
}
// ----------------------------------------------------------------------------
int ProcessLanguageClient::SkipLine() //return json data position
// ----------------------------------------------------------------------------
{
    // this function is driven by thread in transport::loop()
    // thread was started in constructor

    // Skip past header to get to actual data. "content-length: nnnncrlf"
    if ( (not Has_LSPServerProcess()) or ( m_std_LSP_IncomingStr.empty()) )
        return wxNOT_FOUND;

    size_t ndx = m_std_LSP_IncomingStr.find('\n');
    if (not stdFound(ndx) )
        return wxNOT_FOUND ;

    while( (m_std_LSP_IncomingStr[ndx] == '\r') or (m_std_LSP_IncomingStr[ndx] == '\n') )
        ndx += 1;

    return ndx;
}
// ----------------------------------------------------------------------------
int ProcessLanguageClient::SkipToJsonData()
// ----------------------------------------------------------------------------
{
    // this function is driven by readJson() called by thread in transport::loop()
    // thread was started in constructor

    // Skip over header to get to actual data. "content-length: ####crlf{"jsonrpc": ...
    if ( (not Has_LSPServerProcess()) or ( m_std_LSP_IncomingStr.empty()) )
        return wxNOT_FOUND;

    //-int ndx = LSP_IncomingStr.Find("{\"jsonrpc");    //CCLS format
    size_t ndx = m_std_LSP_IncomingStr.find("Content-Length: ");
    if (ndx == std::string::npos)
    {
        writeClientLog("ERROR: SkipToJsonData(): clangd header not at start of buffer.");
        return wxNOT_FOUND; //something wrong, clangd header not at start of buffer
    }
    ndx = m_std_LSP_IncomingStr.find("\r\n{\"");         //clangd format "Content-Length: <digits>\r\n\r\n{"
    if (ndx != std::string::npos)
        return ndx+2;                               // skip over '\r\n' before json data

    return wxNOT_FOUND;
}
// ----------------------------------------------------------------------------
int ProcessLanguageClient::ReadLSPinputLength()
// ----------------------------------------------------------------------------
{
    // this function is driven by readJson() via the thread in transport::loop()
    // and the incoming data is locked.
    // thread was started in constructor and called from readJason() after locking the buffer

    // "Content-Length: <digits>\r\n\r\n"

    if (Has_LSPServerProcess() and m_std_LSP_IncomingStr.length())
    {
        // search for LSP header
        size_t hdrPosn = m_std_LSP_IncomingStr.find("Content-Length: ");
        if (hdrPosn == std::string::npos)
            return wxNOT_FOUND;
        else //have incoming text
        {
            if (hdrPosn != 0)   // verify LSP header is at beginning of buffer
            {
                // Error: header is not at beginning of buffer. Try to fix it.
                // usually caused by clangd invalid utf8 sequence
                std::string msg = StdString_Format("ERROR:%s(): buffLength (%d): Position of content header %d.\n",
                             __FUNCTION__, int(m_std_LSP_IncomingStr.length()), int(hdrPosn));
                msg += "Buffer contents written to client log.";
                //#if defined(cbDEBUG)
                //    wxSafeShowMessage("Input Buffer error",msg);
                //#endif
                msg += "LSP_IncomingStrBuf:\n" + m_std_LSP_IncomingStr + "\n";
                writeClientLog(msg);
                // adjust the data buf to get clangd header at buff beginning
                m_std_LSP_IncomingStr = m_std_LSP_IncomingStr.substr(hdrPosn);
            }

            size_t jdataPosn = m_std_LSP_IncomingStr.find("\r\n{\"");        //find beginning of json data
            if (jdataPosn == std::string::npos)
                return wxNOT_FOUND;                                          //all json data is not yet in.
            jdataPosn += 2;                                                  // skip over "\r\n" prefixed before '{"' json chars
            //-int jdataLength = atoi(std_LSP_IncomingStr.at(16));
            long jdataLength = std::stoi(&m_std_LSP_IncomingStr[16], nullptr, 10); //length of json data is at buf + 16

            if (m_std_LSP_IncomingStr.length() >= (jdataPosn + jdataLength) ) //clangd entry must be complete
            {
                if (m_std_LSP_IncomingStr[jdataPosn + jdataLength-1] == '}' )
                    return jdataLength;

                // Length in LSP data header was wrong. Should have seen a '}' at end of data.
                //cbAssertNonFatal(m_std_LSP_IncomingStr[jdataPosn + jdataLength-1] == '}'); // **Debugging**
                LogManager* pLogMgr = Manager::Get()->GetLogManager();
                std::string msg = StdString_Format("Error:%s() invalid LSP dataLth[%d]", __FUNCTION__, int(jdataLength));
                pLogMgr->DebugLogError(msg);
                msg = StdString_Format("Error:%s() invalid LSP dataLth[%d]\n%s", __PRETTY_FUNCTION__, int(jdataLength), m_std_LSP_IncomingStr.c_str());
                writeClientLog( msg);
                // Must return the length even if invalid else the data gets stuck in the buffer.
                // Try for valid length by looking for next LSP data entry
                size_t actualLength = m_std_LSP_IncomingStr.find("Content-Length: ", 1);
                if (stdFound(actualLength))
                {
                    jdataLength = actualLength - jdataPosn;
                    // FIXME (ph#): After debugging, remove "Error" of  DebugLog()
                    pLogMgr->DebugLogError(wxString::Format("\tCorrectd data length is %d:", jdataLength));
                    return jdataLength;
                }
                else // try to match beginning '{' with ending '}' to get actual length //(ph 2022/10/10)
                {
                    int idx = StdString_FindClosingEnclosureChar(&m_std_LSP_IncomingStr[jdataPosn], 0);
                    if (idx > 0)
                    {
                        // FIXME (ph#): After debugging, remove "Error" from DebugLog()
                        pLogMgr->DebugLogError(wxString::Format("\tCorrected data length is %d", idx+1));
                        return idx+1;
                    }
                }
                // Must return the length even if invalid else the data gets stuck in the buffer
                // FIXME (ph#): After debugging, remove "Error" from DebugLog()
                pLogMgr->DebugLogError("\tCannot correct data length.");
                return jdataLength;
            }
            else // all data not in yet.
            {
                // **Debugging**
                if (m_std_LSP_IncomingStr.length())
                {
                    wxString msg = wxString::Format("Header[%s] buffLth[%d]",
                                m_std_LSP_IncomingStr.substr(0,jdataPosn),  //header
                                int(m_std_LSP_IncomingStr.length()) );      //full buffer length
                    msg.Replace("\r\n","\\r\\n"); //show crlf
                    writeClientLog(StdString_Format(" >>> Info:%s() %s", __FUNCTION__, msg.ToStdString().c_str()) );
                }
            }
        }
    }
    return 0;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::ReadLSPinput(int startPosn, int length, std::string& stdStrOut)
// ----------------------------------------------------------------------------
{
    // this function is driven by thread in transport::loop()
    // thread was started in constructor

    if (Has_LSPServerProcess() and m_std_LSP_IncomingStr.length())
    {
        //ReadLength() guaranteed input hdr was at start of buf
        stdStrOut = m_std_LSP_IncomingStr.substr(startPosn, length);
        if (stdStrOut.length())
        {
            //writeClientLog(wxString::Format("Read()\n:%s", wxString(out)) ); // **Debugging**
            size_t nextHdrPosn = m_std_LSP_IncomingStr.find("Content-Length: ", 1);
            if (nextHdrPosn != std::string::npos)
                m_std_LSP_IncomingStr = m_std_LSP_IncomingStr.substr(nextHdrPosn);
            else    // no more data or missing header
                m_std_LSP_IncomingStr = m_std_LSP_IncomingStr.substr(startPosn + length);   //remove used chars
        }
    }
    return;
}//end Read()
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::readJson(json &json)
// ----------------------------------------------------------------------------
{
    // This function is driven by thread in transport::loop().
    // Thread was started in constructor

    json.clear();
    int length = 0;
    std::string stdStrInputbuf;

    if ( m_terminateLSP or (not Has_LSPServerProcess()) )
    {   // terminate the read loop thread
        stdStrInputbuf = "{\"jsonrpc\":\"2.0\",\"Exit!\":\"Exit!\",\"params\":null}";
        length = stdStrInputbuf.length();
        json = json::parse(stdStrInputbuf);
        return true;
    }

    // --------------------------------------------
    /// lock the clangd stdout buffer
    // --------------------------------------------
    wxMutexError lockerr = m_MutexInputBufGuard.Lock();
    if (lockerr != wxMUTEX_NO_ERROR)
    {
        std::string msg = StdString_Format("LSP data loss. %s() Failed to obtain input buffer lock", __FUNCTION__);
        //-wxSafeShowMessage("Lock failed, lost data", msg); // **Debugging**
        CCLogger::Get()->DebugLogError(msg);
        writeClientLog(msg);
        wxMilliSleep(500); //let pipe thread do its thing
        return false;
    }

    length = ReadLSPinputLength();
    if (not length)
    {
        /// no data, UNlock the input buffer
        m_MutexInputBufGuard.Unlock();
        wxMilliSleep(250);
        return false;
    }

    int dataPosn = SkipToJsonData();    //skip over the Clangd length header
    if (dataPosn != wxNOT_FOUND)
        ReadLSPinput(dataPosn, length, stdStrInputbuf);
    else {
        /// UNLock the input buffer
        m_MutexInputBufGuard.Unlock();
        wxMilliSleep(250);
        return false;
    }

    /// we have the data, UNlock the input buffer
    m_MutexInputBufGuard.Unlock();

    if (stdStrInputbuf.size())
        writeClientLog(StdString_Format(">>> readJson() len:%d:\n%s", length, stdStrInputbuf.c_str()) );

    // remove any invalid utf8 chars
    bool validData = DoValidateUTF8data(stdStrInputbuf);

    // Remove some extended ascii chars that have clobber completion and hover responses
    if (stdStrInputbuf.find("{\"id\":\"textDocument/hover") != std::string::npos) //{"id":"textDocument/hover
    {
        std::string badBytes =  "\xE2\x86\x92" ; //Wierd chars in hover results
        StdString_ReplaceAll(stdStrInputbuf, badBytes, " ");
    }
    if (stdStrInputbuf.find("{\"id\":\"textDocument/completion") != std::string::npos) //{"id":"textDocument/completion
    {
        std::string badBytes =  "\xE2\x80\xA2" ; //Wierd chars in completion empty params
        StdString_ReplaceAll(stdStrInputbuf, badBytes, " ");
        badBytes = "\xE2\x80\xA6"; // wx3.0 produces an empty string
        StdString_ReplaceAll(stdStrInputbuf, badBytes, " ");
    }


    if (not validData)
    {
        //message to log that data had illegal utf8 already written
    }

    int retryCount = 0;
    while (++retryCount)
    {
        try {
                // **Debugging** the catch code below
                //char const* errorBuf = "ill-formed UTF-8 byte column 12";
                //throw std::runtime_error(std::string("Testing: ") + errorBuf);

            json = json::parse(stdStrInputbuf);
            break;
        }
        catch (std::exception &e)
        {
            std::string msg = StdString_Format(" >>> readJson() error:%s", e.what()) ;
            #if defined(cbDEBUG)
                //-Manager::Get()->GetLogManager()->DebugLogError(msg);
                CCLogger::Get()->DebugLog(msg);
            #endif
            if (retryCount == 1) // do only once
                msg += "\n" + stdStrInputbuf;
            writeClientLog(msg);
            if (retryCount > 10) break; //allow nine errors
            if ( not StdString_Contains(msg, "ill-formed UTF-8 byte") )
                break;
            // find the location of the invalid utf8 char
            size_t posn = msg.find("column ");
            if (stdFound(posn))
            {
                long utf8BytePosn = 0;
                utf8BytePosn = std::stol(msg.substr(posn + 7));
                if (not utf8BytePosn) break;
                // wipe out the invalid utf8 char with a blank
                if (stdStrInputbuf[--utf8BytePosn] & 0x80)
                    stdStrInputbuf[utf8BytePosn] = ' ';
            }
        }//endcatch
    }//endwhile

    if (StdString_StartsWith(stdStrInputbuf, R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics")") )
    {
        // whenever diagnostics arrive, an open, save or didModified was issued. //(ph 2021/02/9)
        // clear busy and modified flags
        SetDidChangeTimeBusy(0);
    }

    return true;
}//end readJson
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::writeJson(json& json)
// ----------------------------------------------------------------------------
{
    if (not Has_LSPServerProcess()) return false;

    std::string content = json.dump();
    std::string header = "Content-Length: " + std::to_string(content.length()) + "\r\n\r\n" + content;
    return WriteHdr(header);
}
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::WriteHdr(const std::string& in)
// ----------------------------------------------------------------------------
{
    // write json header and data string to the log then write it to clangd
    // Data to the log is snipped to 512 bytes to save disk space.
    std::string limitedLogOut(in);
    std::string out(in);

    // limit "text" output to log at 512 chars
    if (StdString_Contains(limitedLogOut,"\"textDocument/didOpen\"")
        or StdString_Contains(limitedLogOut, "\"textDocument/didChange\"") )
    {
        size_t posnText = limitedLogOut.find("\"text\":");
        size_t posnUri =  limitedLogOut.find("\"uri\":");
        if ( (not stdFound(posnText)) or (not stdFound(posnUri)) )
            cbAssert(0 && "Badly formated log out data WriteHdr()");

        // if uri follows text, make adjustments
        if (posnUri > posnText)
        {
            int txtBeg = posnText + 7; //skip over "text":
            int txtEnd = posnUri - 4;  //skip back over "uri": where text ends
            int txtLen = txtEnd - txtBeg ;
            if (txtLen > 512)
            {
                std::string tmpStr = limitedLogOut.substr(0,txtBeg+120) + "<...SNIP...>" + limitedLogOut.substr(limitedLogOut.size()-120);
                tmpStr.append(limitedLogOut.substr(posnUri-8) ); //append uri to end
                limitedLogOut = tmpStr;
            }
        }
        else
            limitedLogOut = "<<< Write():\n" + in.substr(0,512) + "<...DATA SNIPPED BY LOG WRITE()...>" ;
    }//endif contains didOpen

    if (not StdString_StartsWith(limitedLogOut,"<<< "))
        limitedLogOut.insert(0,"<<< ");
    writeClientLog(limitedLogOut);

    // Write raw data to clangd server
    #if defined(_WIN32)
        bool ok = m_pServerProcess->WriteRaw( out ); //windows
        if (not ok)
        {
            writeClientLog("Error: WriteHdr() failed WriteRaw()");
            return false;
        }
    #else
        // unix just posts the msg to an output thread queue, so no return code.
        //-m_pServerProcess->Write( fileUtils.ToStdString(out) );            //unix
        m_pServerProcess->Write( out );            //unix
    #endif
    return true;
}//end WriteHdr()
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::DoValidateUTF8data(std::string& strdata)
// ----------------------------------------------------------------------------
{
    // These invalid utr8 chars are coming from clangd textDocument/completion
    // and textDocument/hover responses.

    // convert string data to vector
    //eg., const std::vector<int> charvect(json_str.begin(), json_str.end());
    std::vector<int>utf8Vector(strdata.begin(), strdata.end());

    std::vector<int>invalidLocs;
    ValidateUTF8vector validateUTF8;
    bool result = validateUTF8.validUtf8(utf8Vector,invalidLocs);
    if (invalidLocs.size())
    {
        ConfigManager *cfgApp = Manager::Get()->GetConfigManager(_T("app"));
        // Avoid utf8 asserts in internationalized CodeBlocks.
        bool i18n = cfgApp->ReadBool(_T("/locale/enable"), false);

        // Erase the invalid utf8 chars (if any) in reverse order
        for (int ii=invalidLocs.size(); ii-- > 0; )
        {
            int invloc = invalidLocs[ii];
            std::string invStr(&strdata[invloc], 1);
            unsigned char invChar(invStr[0]);

            // clangd response:
            // {"id":"textDocument/completion","jsonrpc":"2.0","result":{
            // a URI is not always included in the response
            //  "textDocument":{"uri":"file:///F:/usr/Proj/Clangd_Client-uw/clone/src/LSPclient/src/client.cpp"}
            std::string respID;
            std::string respURI;
            int respIDposn = strdata.find("textDocument/");
            int respURIposn = strdata.find("{\"uri\":\"file://");
            if (stdFound(respIDposn))
                respID = strdata.substr(respIDposn, 24);
            if (stdFound(respURIposn))
            {
                int uriend = strdata.find("\"}", respURIposn);
                if (wxFound(uriend))
                    respURI = strdata.substr(respURIposn+7, uriend);
            }

            wxString msg = "Error: Removed clangd response invalid utf8 char:";
            if (not i18n) //if not internationalization show U(<codepoint>)
            {
                // With internationalization the wxUniChar gets an assert in wxString::Format
                wxUniChar uniChar(invChar);
                msg += wxString::Format("position(%d), hex(%02hhX), U(%x), \'%s\'", invloc, (unsigned int)invChar, (int)uniChar.GetValue(), invStr );
            }
            else
                msg += wxString::Format("position(%d), hex(%02hhX), \'%s\'", invloc, (unsigned int)invChar, invStr );

            if (respID.size())
                msg += wxString::Format(" ResponseID:%s", respID);
            if (respURI.size())
                msg += wxString::Format(" URI(%s)", respURI);
            CCLogger::Get()->DebugLog(msg);
            writeClientLog(msg.ToStdString());

            // erase the invalid utf8 char
            strdata.erase (invloc, 1);
        }//endFor
    }//endIf
    return result;

}//end DoValidateUTF8data
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnLSP_Response(wxThreadEvent& threadEvent)
// ----------------------------------------------------------------------------
{
    // This member event was Connected() in ProcessLanguageClient() constructor;
    // and issued from  transport.h in "loop(MessageHandler &handler)"
    // with event.clientdata set to incoming json data
    // tranport.h loop analyses the raw clangd stdout data.
    // Here we dispatch the different messages types to the appropriate routine.

    m_LSP_responseStatus = true;
    if (not Has_LSPServerProcess()) return;

    json* pJson = nullptr;
    try {
        pJson = (json*)threadEvent.GetPayload<json*>();
    }
    catch (std::exception &e){
        cbMessageBox("OnLSP_Response() error: %s", e.what());
    }

    #if defined(cbDEBUG)
        //std::string see = pJson->dump(); // **debugging**
    #endif //LOGGING

    wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED);
    event.SetString(threadEvent.GetString()); //id of json  eg. id:result or id:method etc.
    event.SetClientData(pJson);
    wxString rspHeader = event.GetString(); //id of json eg. d:result or id:method etc

    try
    {
        if (pJson->size())
        {
            if (pJson->count("id"))
            {
                if (pJson->contains("initialize"))
                    OnIDResult(event)

;               else if (pJson->contains("method"))
                {
                    OnIDMethod(event);
                }
                else if (pJson->contains("result"))
                {
                    OnIDResult(event);
                }
                else if (pJson->contains("error"))
                {
                    OnIDError(event);
                }
            }
            else if (pJson->contains("method"))
            {
                if (pJson->contains("params"))
                {
                    OnMethodParams(event);
                }
            }
            else if (pJson->contains("Exit!")) //(ph 2020/09/26)
            {
                //This never occurs
            }
        }
    }
    catch(std::exception &err)
    {
        wxString errMsg(wxString::Format("\nOnLSP_Response() error: %s", err.what()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
        return;
    }

    return;
}//end OnLSP_Response()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnIDMethod(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    //- unused- json* pJson = (json*)event.GetClientData();

}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnIDResult(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    json* pJson = (json*)event.GetClientData();
    //#if defined(cbDEBUG)
    //    std::string see = pJson->dump(); //debugging
    //#endif //LOGGING

    wxCommandEvent lspevt(wxEVT_COMMAND_MENU_SELECTED, GetLSP_UserEventID());

    if (pJson->contains("id"))
    {
        wxString idValue;
        try { idValue = GetwxUTF8Str(pJson->at("id").get<std::string>()); }     //(ph 2022/10/01)
        catch(std::exception &err)
        {
            wxString errMsg(wxString::Format("\nOnIDResult() error: %s", err.what()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
            return;
        }

        lspevt.SetString(idValue);

        if (idValue == "initialize")
        {
            m_LSP_initialized = true;
            lspevt.SetString("LSP_Initialized:true");
        }

        else if (idValue == "shutdown")
        {
            m_LSP_initialized = false;
            // Terminate the input thread
            m_terminateLSP = true; //tell the read thread to terminate //(ph 2021/01/15)
            m_MapMsgHndlr.SetLSP_TerminateFlag(1); //(ph 2021/07/8)
            lspevt.SetString("LSP_Initialized:false");
        }

        else if(idValue.StartsWith("textDocument/declaration")
                or idValue.StartsWith("textDocument/definition") )
        {
            lspevt.SetString(idValue +  STX +"result");
            json resultValue = pJson->at("result"); // now array

            #if defined(cbDEBIG)
                //std::string see = resultValue.dump(); //debugging
            #endif //LOGGING

        }// if textDocument/def... textDocument/decl...

        else if (idValue.StartsWith("textDocument/references") )
        {
            //{"jsonrpc":"2.0","id":"textDocument/references","result":[{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","range":{"start":{"line":49,"character":45},"end":{"line":49,"character":52}}},{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","range":{"start":{"line":89,"character":4},"end":{"line":89,"character":11}}}]}
            lspevt.SetString(idValue + STX + "result");
            json resultValue = pJson->at("result"); // now array

        }// if textDocument/references

        else if(idValue.StartsWith("textDocument/documentSymbol") )
        {
            //{"jsonrpc":"2.0","id":"textDocument/documentSymbol","result":[{"name":"wxbuildinfoformat","detail":"enum wxbuildinfoformat {}","kind":10,"range":{"start":{"line":19,"character":0},"end":{"line":20,"character":21}},"selectionRange":{"start":{"line":19,"character":5},"end":{"line":19,"character":22}},"children":[]},{"name":"short_f","detail":"short_f","kind":22,"range":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},"selectionRange":{"start":{"line":20,"character":4},"end":{"line":20,"character":11}},...etc
            //debugging wxString showit = pJson->dump(3); //pretty print with 3 tabs spacing //(ph 2020/10/30)
            //debugging writeClientLog(showit);
            //debugging return;
            lspevt.SetString(idValue + STX + "result");
            //-json resultValue = pJson->at("result"); // now array
        }
        else if(idValue.StartsWith("textDocument/completion") )
        {
            lspevt.SetString(idValue + STX +"result");
            SetCompletionTimeBusy(0);
        }
        else if(idValue.StartsWith("textDocument/hover") )
        {
            lspevt.SetString(idValue + STX +"result");
        }
        else if(idValue.StartsWith("textDocument/signatureHelp") )
        {
            lspevt.SetString(idValue + STX +"result");
        }
        else if(idValue.StartsWith("textDocument/rename") )
        {
            lspevt.SetString(idValue + STX +"result");
        }

    }//endif "id"

    // A copy of the json object is necessary for AddPendingEvent(). The current one
    // will be reused  by the readJson() function.
    // The new json object will be freed in CodeCompletion code.
    json* pJsonData = new json(*pJson);
    #if defined(cbDEBUG)
    cbAssertNonFatal(pJsonData && "Failure to allocate json data");
    #endif
    if (not pJsonData) return;

    lspevt.SetClientData(pJsonData);
    lspevt.SetEventObject(this);
    Manager::Get()->GetAppFrame()->GetEventHandler()->AddPendingEvent(lspevt);

    return;

}//OnIDResult()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnIDError(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    wxCommandEvent lspevt(wxEVT_COMMAND_MENU_SELECTED, GetLSP_UserEventID());

    //{"jsonrpc":"2.0","id":"textDocument/declaration","error":{"code":-32600,"message":"not indexed"}}

    json* pJson = (json*)event.GetClientData();

    wxString idValue;
    try { idValue = GetwxUTF8Str(pJson->at("id").get<std::string>()); }     //(ph 2022/10/01)
    catch(std::exception &err)
    {
        wxString errMsg(wxString::Format("\nOnIDError() error: %s", err.what()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
        return;
    }

    //if(idValue.StartsWith("textDocument/declaration")
    //            or idValue.StartsWith("textDocument/definition") )
    if(idValue.StartsWith("textDocument/") )
    {
        lspevt.SetString(idValue + STX + "error");
        if (idValue.Contains("/completion"))
            SetCompletionTimeBusy(0);
    }
    else return; //for now

    // A copy of the json is necessary for AddPendingEvent().
    // The current one will be reused  by the readJson() function.
    // new json object will be freed in CodeCompletion::OnLSP_Event() code
    json* pJsonData = new json(*pJson);
    #if defined(cbDEBUG)
    cbAssertNonFatal(pJsonData && "Failure to allocate json data");
    #endif
    if (not pJsonData) return;

    lspevt.SetClientData(pJsonData);
    lspevt.SetEventObject(this);
    Manager::Get()->GetAppFrame()->GetEventHandler()->AddPendingEvent(lspevt);

}//end IDError
// ----------------------------------------------------------------------------
void ProcessLanguageClient::OnMethodParams(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    wxString methodValue;
    json* pJson = nullptr;
    try
    {
        pJson = (json*)event.GetClientData();
        methodValue = GetwxUTF8Str(pJson->at("method").get<std::string>());
    }
    catch (std::exception &e)
    {
        wxString msg = wxString::Format("OnMethodParams() %s", e.what());
        writeClientLog(msg.ToStdString());
        cbMessageBox(msg);
        return;
    }


    // event used to pass json object to CodeCompletion functions via CodeCompletion::OnLSP_Event()
    wxCommandEvent lspevt(wxEVT_COMMAND_MENU_SELECTED, GetLSP_UserEventID());

    // ----------------------------------------------------------------------------
    //" textDocument/publishDiagnostics"
    // ----------------------------------------------------------------------------
    // {"jsonrpc":"2.0",
    //      "method":"textDocument/publishDiagnostics",
    //       "params":{
    //         "uri":"file://F%3A/usr/Proj/Clangd/CB_Client/HelloWorld/HelloWorld.cpp","diagnostics":[{"range":{"start":{"line":3,"character":16},"end":{"line":3,"character":19}},"severity":2,"code":2,"source":"Clangd","message":"using directive refers to implicitly-defined namespace 'std'","relatedInformation":[]},{...

    if ((methodValue == "textDocument/publishDiagnostics"))
    {
        lspevt.SetString(methodValue + STX + "params");
    }

    // A copy of the json is necessary for AddPendingEvent(). The current one
    // will be reused  by the readJson() function.
    // The new json object will be freed in CodeCompletion code.
    json* pJsonData = new json(*pJson);
    #if defined(cbDEBUG)
    cbAssertNonFatal(pJsonData && "Failure to allocate json data");
    #endif
    if (not pJsonData) return;

    lspevt.SetClientData(pJsonData);
    lspevt.SetEventObject(this);
    Manager::Get()->GetAppFrame()->GetEventHandler()->AddPendingEvent(lspevt);

    return;

}//end OnMethodParams
// ----------------------------------------------------------------------------
wxString ProcessLanguageClient::GetRRIDvalue(wxString& lspHdrString)
// ----------------------------------------------------------------------------
{
    // RRID == RequestRedirectionID, an int to redirect responses to the requestor

    int posn = wxNOT_FOUND;
    long lspRRID = 0;
    wxString RRIDstr;

    if ( wxFound(posn = lspHdrString.Find(wxString(STX) + "RRID")) )
    {
        RRIDstr = lspHdrString.Mid(posn+1);     // skip over STX (StartOfText) char
        RRIDstr = RRIDstr.BeforeFirst(STX);     //eliminate any other trailing strings
        bool ok = (lspRRID = RRIDstr.Mid(4).ToLong(&lspRRID)); //skip over 'RRID' to get int chars
        if (not ok) return wxString();
    }

    return RRIDstr;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_Shutdown()
// ----------------------------------------------------------------------------
{
    m_LSP_initialized = false;

    if (Has_LSPServerProcess())
    {
        writeClientLog("<<< Shutdown():\n") ;
        Shutdown(); //Tell LSP to close/save/whatever goodness
        Exit();
    }

    return;
}//end LSP_Shutdown()
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::ClientProjectOwnsFile(cbEditor* pcbEd, bool notify)
// ----------------------------------------------------------------------------
{
    // Verify that the project that created this client owns this editors file

    bool owned = false;
    if (pcbEd)
    {
        // There must be an active project
        cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pActiveProject) return owned = false;
        wxString edFilename = pcbEd->GetFilename();

        // For LSP, file must belong to a project, because project compile parameters are used
        // Find the project and ProjectFile this editor is holding.
        ProjectFile* pProjectFile = pcbEd->GetProjectFile();
        // if no ProjectFile, try to find it via active project
        // This is caused by OnFileAdded() event before ProjectFile is created
        if (not pProjectFile)
            pProjectFile = pActiveProject->GetFileByFilename(edFilename, false);

        cbProject* pEdProject = nullptr;
        if (pProjectFile)
            pEdProject = pProjectFile->GetParentProject();
        if ( (not pProjectFile) or (not pEdProject) )
            owned = false;
        else if (pEdProject == GetClientsCBProject()) //GetCBProject returns client project
            owned = true;
        else if (pEdProject->GetTitle() == "~ProxyProject~") //for non-owned project files
        {
            if (m_pParser and (pEdProject == m_pParser->GetParseManager()->GetProxyProject()))
                owned = true;
        }

        if ((not owned) and notify)
        {
            wxString msg = wxString::Format("LSP: This file is not contained in a loaded project.\n%s", edFilename);
            cbMessageBox(msg);
        }
    }

    return owned;
}
// ----------------------------------------------------------------------------
cbProject* ProcessLanguageClient::GetProjectFromEditor(cbEditor* pcbEd)
// ----------------------------------------------------------------------------
{
    cbProject* pActiveProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pActiveProject) return nullptr;

    if (pcbEd)
    {
        wxString edFilename = pcbEd->GetFilename();
        cbProject* pEdProject = nullptr;
        ProjectFile* pProjectFile = pcbEd->GetProjectFile();
        // No project file yet when event is OnProjectFileAdded()
        // Try to get ProjectFile via active project
        if (not pProjectFile)
            pProjectFile = pActiveProject->GetFileByFilename(edFilename, false);
        if (pProjectFile)
            pEdProject = pProjectFile->GetParentProject();
        if ( (not pProjectFile) or (not pEdProject) )
            return nullptr;
        return pEdProject;
    }

    return nullptr;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_Initialize(cbProject* pProject)
// ----------------------------------------------------------------------------
{
    #if defined(cbDEBUG)
    cbAssertNonFatal(pProject && "LSP_Initialize called without pProject");
    #endif
    if (not pProject) return;

    //LSP_rootURI = dirname; // contains backword slashes
    wxString dirname = wxPathOnly(pProject->GetFilename() );
    dirname.Replace("\\", "/");

    // Call UpdateCompilationDatabase() with currently opened files before LSP initializtion
    // to assure that compile_commands.json contains entries with the correct compiler parameters.
    // Else they'll be parsed with clang defaults instead of our compiler settings.
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();

    for (int ii=0; ii< pEdMgr->GetEditorsCount(); ++ii)
    {
        // Find the project and ProjectFile this editor is holding.
        cbEditor* pcbEd = pEdMgr->GetBuiltinEditor(ii);
        if (pcbEd)
        {
            ProjectFile* pProjectFile = pcbEd->GetProjectFile();
            if (not pProjectFile) continue;
            cbProject* pEdProject = pProjectFile->GetParentProject();
            if (not pEdProject) continue;
            if (pEdProject != pProject) continue;

            wxString filename = pcbEd->GetFilename();
            UpdateCompilationDatabase(pProject, filename);
            // cancel the changed time so clangd doesnt get restarted;
////            SetCompileCommandsChangedTime(false);
        }
    }//for

    if (not GetLSP_Initialized())
    {
        //-writeClientLog(StdString_Format("<<< Initialize(): %s", dirname.ToStdString().c_str()) );
        std::string stdDirName = GetstdUTF8Str(dirname); //(ollydbg 2022/10/30) ticket #78
        writeClientLog(StdString_Format("<<< Initialize(): %s", stdDirName.c_str()) );

        // Set the project folder as the folder containing the commands file. //(ollydbg 2022/10/19) Ticket #75
        try { Initialize(string_ref(fileUtils.FilePathToURI(dirname)), string_ref(dirname.ToUTF8())); } //(ollydbg 2022/10/19) ticket #75
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            //-wxString errMsg(wxString::Format("\nLSP_Initialize() error: %s\n%s", err.what(), dirname.c_str()) );
            wxString errMsg(wxString::Format("\nLSP_Initialize() error: %s\n%s", err.what(), stdDirName.c_str()) ); //(ollydbg 2022/10/30) ticket #78
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }

    }
    return;
}//end LSP_Initialize()
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::LSP_DidOpen(cbEditor* pcbEd)
// ----------------------------------------------------------------------------
{
    if (not GetLSP_Initialized())
    {
        wxString msg = _("LSP_DidOpen() Attempt to add file before initialization.");
        msg += wxString::Format("\n%s",pcbEd->GetFilename());
        CCLogger::Get()->DebugLogError(msg);
        cbMessageBox(msg,"LSP_DidOpen");
        return false;
    }

    cbProject* pProject = GetProjectFromEditor(pcbEd);
    wxString infilename = pcbEd->GetFilename();

    if (not ClientProjectOwnsFile(pcbEd, false))
            return false;

    if (GetLSP_EditorIsOpen(pcbEd))
        return false;

    // Open only ParserCommon::EFileType extensions specified in config
    ProjectFile* pProjectFile = pcbEd->GetProjectFile();
    if (not pProjectFile) return false;
    ParserCommon::EFileType filetype = ParserCommon::FileType(pProjectFile->relativeFilename);
    if ( not ((filetype == ParserCommon::ftHeader) or (filetype == ParserCommon::ftSource)) )
        return false;

    // Add file to compiler_commands.json if it's absent
    UpdateCompilationDatabase(pProject, infilename);

    #if wxCHECK_VERSION(3,1,0)
    std::string srcFilename = infilename.ToStdString(wxConvUTF8);
    std::string srcDirname = wxPathOnly(pProject->GetFilename()).ToStdString(wxConvUTF8);
    #else
    //std::string srcFilename = infilename.utf8_str().ToStdString();
    std::string srcFilename = std::string(infilename.utf8_str());
    std::string srcDirname = std::string(wxPathOnly(pProject->GetFilename()).utf8_str());
    #endif

    std::vector<string_ref> vecOfCompileCommands;

    if (platform::windows)
    {
        StdString_ReplaceAll(srcFilename, "\\","/");
        StdString_ReplaceAll(srcDirname, "\\","/");
    }

    wxString fileURI = fileUtils.FilePathToURI(infilename); //(ph 2022/01/5)
    fileURI.Replace("\\", "/");
    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    cbStyledTextCtrl* pCntl = pcbEd->GetControl();
    if (not pCntl) return false;

    // save current length of the file
    m_FileLinesHistory[pcbEd] = pCntl->GetLineCount();

    // 2022/07/16 reverted from rev 67 back to rev 66
    // This change reverted because it caused empty source to be sent to clangd for ollydbg
    // using chinese chars
    // Cf:  https://forums.codeblocks.org/index.php/topic,24357.msg170563.html#msg170563
    //    #if wxCHECK_VERSION(3,1,5) //3.1.5 or higher
    //    wxString strText = pCntl->GetText().utf8_string(); //solves most illegal utf8chars
    //    #else
    //    //const char* pText = strText.mb_str();         //works //(ph 2022/01/17)
    //    wxString strText = pCntl->GetText().ToUTF8();  //ollydbg  220115 did not solve illegal utf8chars
    //      #endif
    //
    //const char* pText = strText.c_str();

    wxString strText = pCntl->GetText();
    //-const char* pText = strText.mb_str();        //works //(ph 2022/01/17)
    const char* pText = strText.ToUTF8();           //ollydbg  220115 did not solve illegal utf8char

    writeClientLog(StdString_Format("<<< LSP_DidOpen:%s", docuri.c_str()) );

    try { DidOpen(docuri, string_ref(pText, strText.Length()) ); }
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        std::string errMsg(StdString_Format("\nLSP_DidOpen() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg);
        cbMessageBox(errMsg);
        return false;
    }

    //-SetParseTimeStart(pcbEd); deprecated
    LSP_AddToServerFilesParsing(pcbEd->GetFilename() );

    SetLSP_EditorIsOpen(pcbEd, true);
    SetLastLSP_Request(infilename, "textDocument/didOpen");
    SetLSP_EditorHasSymbols(pcbEd, false);

    /**Debugging**/
    //LogManager* pLogMgr = Manager::Get()->GetLogManager();
    //pLogMgr->DebugLog(wxString::Format("%s(): %s",__FUNCTION__, infilename));

    return true;
}//end LSP_DidOpen() cbEditor version
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::LSP_DidOpen(wxString filename, cbProject* pProject)
// ----------------------------------------------------------------------------
{
    // This function is NOT used for files open in an editor

    if (not GetLSP_Initialized()) {
        wxString msg = wxString::Format("%s() %d: ", __FUNCTION__, __LINE__);
        msg += "\n attempt to add file before initialization.";
        cbMessageBox(msg);
        return false;
    }

    wxString infilename = filename;
    if (not wxFileExists(filename) ) return false;
    if (not pProject) return false;
    if (not pProject->GetFileByFilename(filename, false))
            return false;

    // This function is not used for files open in an editor
    // Dont DidOpen() editors or files multile times, LSP will yell at you.
    EditorManager* pEdMgr =  Manager::Get()->GetEditorManager();
    if (pEdMgr->IsOpen(filename) )
        return false;

    // Open only .c* or .h* file types
    ProjectFile* pProjectFile = pProject->GetFileByFilename(filename, false);
    if (not pProjectFile) return false;
    ParserCommon::EFileType filetype = ParserCommon::FileType(pProjectFile->relativeFilename); //(ph 2022/06/1)
    if ( filetype == ParserCommon::ftOther) // if not header or source
        return false;

    // Add file to compiler_commands.json if it's absent
    UpdateCompilationDatabase(pProject, infilename);

    #if wxCHECK_VERSION(3,1,0)
    std::string srcFilename = infilename.ToStdString(wxConvUTF8);
    std::string srcDirname = wxPathOnly(pProject->GetFilename()).ToStdString(wxConvUTF8);
    #else
    std::string srcFilename = infilename.ToStdString();
    std::string srcDirname = wxPathOnly(pProject->GetFilename()).ToStdString();
    #endif // wxCHECK

    std::vector<string_ref> vecOfCompileCommands;

    if (platform::windows)
    {
        StdString_ReplaceAll(srcFilename, "\\","/");
        StdString_ReplaceAll(srcDirname, "\\","/");
    }

    wxString fileURI = fileUtils.FilePathToURI(infilename);
    fileURI.Replace("\\", "/");
    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    cbStyledTextCtrl* pCtrl = GetNewHiddenEditor(filename);
    if (not pCtrl) return false;

    #if wxCHECK_VERSION(3,1,0)
    std::string strText = pCtrl->GetText().ToStdString(wxConvUTF8);
    #else
    std::string strText = std::string(pCtrl->GetText().utf8_str());
    #endif

    const char* pText = strText.c_str();           //works

    writeClientLog(StdString_Format("<<< LSP_DidOpen:%s", docuri.c_str()) );

    try { DidOpen(docuri, string_ref(pText, strText.size()) ); }
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        //-wxString errMsg(wxString::Format("\nLSP_DidOpen() error: %s\n%s", err.what(), docuri.c_str()) );
        wxString errMsg(wxString::Format("\nLSP_DidOpen(wxString filename, cbProject* pProject) error: %s\n%s", err.what(), docuri.c_str()) ); //(ollydbg 2022/10/30) ticket #78
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
        return false;
    }

    LSP_AddToServerFilesParsing(filename );
    SetLastLSP_Request(infilename, "textDocument/didOpen");

    /**Debugging**/
    //LogManager* pLogMgr = Manager::Get()->GetLogManager();
    //pLogMgr->DebugLog(wxString::Format("%s(): %s",__FUNCTION__, infilename));

    if (pCtrl)
        delete pCtrl;
    return true;

}//end LSP_DidOpen()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_DidClose(cbEditor* pcbEd)
// ----------------------------------------------------------------------------
{
    #if defined(cbDEBUG)
    cbAssertNonFatal(pcbEd && "LSP_DidClose called with nullptr");
    #endif
    if (not pcbEd) return;

    if (not GetLSP_Initialized())
    {
        // Comment out to avoid flooding user with unuseful debugging msgs
        //cbMessageBox("LSP: attempt to close file before initialization.");
        return ;
    }
    //if ( IsEditorOpened() and (not GetLSP_IsEditorParsed(pcbEd) )
    //{
    //    cbMessageBox("Editors file is not yet parsed.");
    //    return;
    //}

    wxString infilename = pcbEd->GetFilename();
    wxString fileURI = fileUtils.FilePathToURI( infilename );
    fileURI.Replace("\\", "/");
    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    // There wont be a wxStyledTextCtrl !!
    //-cbStyledTextCtrl* pCntl = pcbEd->GetControl();
    //-if (not pCntl) return;

    writeClientLog(StdString_Format("<<< LSP_DidClose File:\n%s", docuri.c_str()) );

    try {DidClose(docuri); }
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        wxString errMsg(wxString::Format("\nLSP_DidClose() error: %s\n%s", err.what(), fileURI.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    //-SetLSP_EditorRequest(pcbEd, "textDocument/didClose", 0);
    SetLSP_EditorIsParsed(pcbEd, false);
    SetLSP_EditorIsOpen(pcbEd, false);
    SetLSP_EditorRemove(pcbEd);
    SetLSP_EditorHasSymbols(pcbEd, false);

    SetLastLSP_Request(infilename, "textDocument/didClose");

    return ;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_DidClose(wxString filename, cbProject* pProject)
// ----------------------------------------------------------------------------
{
    #if defined(cbDEBUG)
    cbAssertNonFatal(filename.Length() && "LSP_DidClose called with nullptr");
    #endif
    if (not filename.Length()) return;

    if (not GetLSP_Initialized())
    {
        // Comment out to avoid flooding user with unuseful debugging msgs
        //cbMessageBox("LSP: attempt to close file before initialization.");
        return ;
    }

    wxString infilename = filename;
    // project must own file
    if (not pProject->GetFileByFilename(infilename,false) )
        return;
    wxString fileURI = fileUtils.FilePathToURI(infilename);
    fileURI.Replace("\\", "/");
    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    writeClientLog(StdString_Format("<<< LSP_DidClose File:\n%s", docuri.c_str()) );

    try {DidClose(docuri); }
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        wxString errMsg(wxString::Format("\nLSP_DidClose() error: %s\n%s", err.what(), fileURI.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    SetLastLSP_Request(infilename, "textDocument/didClose");
    // If file in open in an editor, signal that it has been closed
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();
    cbEditor* pcbEd = pEdMgr->IsBuiltinOpen(filename);
    if (pcbEd)
    {
        SetLSP_EditorIsParsed(pcbEd, false); //(ph 2021/11/10)
        SetLSP_EditorIsOpen(pcbEd, false);
        SetLSP_EditorRemove(pcbEd);
        SetLSP_EditorHasSymbols(pcbEd, false);
    }

    return ;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_DidSave(cbEditor* pcbEd)
// ----------------------------------------------------------------------------
{
    // There's a bug in clangd that causes completions to stop after a didClose.
    // Clangd gets an unhangled notifications (see server log) and #301 bug
    // after a didSave. But if we issue a didClose/DidOpen after didSave,
    // completions start up again

    #if defined(cbDEBUG)
    cbAssertNonFatal(pcbEd && "LSP_DidSave called with nullptr");
    #endif
    if (not pcbEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to save file before initialization.");
        return ;
    }
    if (not GetLSP_IsEditorParsed(pcbEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pcbEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pcbEd).Length() ? GetEditorsProjectTitle(pcbEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    //-unused-ProjectFile* pProjectFile = pcbEd->GetProjectFile();
    //-unused-cbProject* pProject = pProjectFile ? pProjectFile->GetParentProject() : nullptr;
    wxString infilename = pcbEd->GetFilename();
    wxString fileURI = fileUtils.FilePathToURI(infilename);
    fileURI.Replace("\\", "/");
    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    writeClientLog(StdString_Format("<<< LSP_DidSave File:\n%s", docuri.c_str()) );

    // Here only when editor was changed, so tell LSP server

    /// clangd issues unhandled exception on didSave()'s (bug #320)
    /// Then it disables completions.
    // the work around is just to do a didClose/didOpen (see below)

    //    try { DidSave(docuri); }
    //    catch(std::exception &err)
    //    {
    //        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
    //        wxString errMsg(wxString::Format("\nLSP_DidSave() error: %s\n%s", err.what(), docuri.c_str()) );
    //        writeClientLog(errMsg);
    //        cbMessageBox(errMsg);
    //    }

    // clear the "LSP messages" log if user set option
    ConfigManager* pCfg = Manager::Get()->GetConfigManager("clangd_client");
    bool doClear = pCfg->ReadBool("/lspMsgsClearOnSave_check", false);
    if (doClear and m_pDiagnosticsLog )
        m_pDiagnosticsLog->Clear(); //(ph 2022/07/13)

    pcbEd->SetErrorLine(-1);            ;//clear any error indicator in editor

    // There's a bug in clangd that causes completions to stop after a DidSave(uri).
    // Clangd gets an unhandled exception (see server log and clangd #320 bug),
    // when processing a didSave. But if we issue a didClose/DidOpen after didSave,
    // completions remain working.

    LSP_DidClose(pcbEd);

    //-SetParseTimeStart(pcbEd); deprecated
    LSP_AddToServerFilesParsing(pcbEd->GetFilename() );

    LSP_DidOpen(pcbEd);

    //-SetLSP_EditorRequest(pcbEd, "textDocument/didSave", 0);
    SetLastLSP_Request(infilename, "textDocument/didSave");

    return ;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_GoToDefinition(cbEditor* pcbEd, int argCaretPosition, size_t id)
// ----------------------------------------------------------------------------
{
    // goto definition / implementation

    #if defined(cbDEBUG)
    cbAssertNonFatal(pcbEd && "LSP_GoToDefinition called with nullptr");
    #endif
    if (not pcbEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to use LSP_GoToDefinition() before initialization.");
        return ;
    }

    if (not GetLSP_IsEditorParsed(pcbEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pcbEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pcbEd).Length() ? GetEditorsProjectTitle(pcbEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    wxString fileURI = fileUtils.FilePathToURI( pcbEd->GetFilename() );
    fileURI.Replace("\\", "/");
    //-fileURI.MakeLower().Replace("f:", "");

    cbStyledTextCtrl* pCtrl = pcbEd->GetControl();
    if (not pCtrl) return;

    int edCaretPosn = argCaretPosition;
    if (not edCaretPosn)
        edCaretPosn = pCtrl->GetCurrentPos();
    int edLineNum   = pCtrl->LineFromPosition(edCaretPosn);
    //-int edColumn    = pCtrl->GetColumn(edCaretPosn); //(ollydbg 2022/11/06 forum msg#244)
    int edColumn   = GetEditorsCaretColumn(pCtrl);

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    Position position;
    //-const int startPosn = pCtrl->WordStartPosition(edCaretPosn, true);
    //-const int endPosn   = pCtrl->WordEndPosition(posn, true); //not needed
    position.line       = edLineNum;
    position.character  = edColumn;
    writeClientLog(StdString_Format("<<< GoToDefinition:\n%s,line[%d], char[%d]", docuri.c_str(), position.line, position.character) );

    //Tell LSP server if text has changed
    LSP_DidChange(pcbEd);

    // CB goto implementation == LSP GoToDefinition
    if (id)
    {
        //RRID == RequestRedirectionID to return response to original requestor routine
        wxString reqID = wxString::Format("%cRRID%d", STX, int(id));
        reqID.Replace(wxString::Format("%c%c", STX ,STX), STX);
        //-try { GoToDefinitionByID(docuri, position, reqID.ToStdString()); }
        try { GoToDefinitionByID(docuri, position, GetstdUTF8Str(reqID)); } //(ollydbg 2022/10/30) ticket #78

        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_GoToDefinition() error: %s\n%s", err.what(), docuri.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    else
    {
        try { GoToDefinition(docuri, position); }
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_GoToDefinition() error: %s\n%s", err.what(), docuri.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    //-SetLSP_EditorRequest(pcbEd, "textDocument/definition", argCaretPosition);
    SetLastLSP_Request(pcbEd->GetFilename(), "textDocument/definition");

    return ;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_GoToDeclaration(cbEditor* pcbEd, int argCaretPosition, size_t id)
// ----------------------------------------------------------------------------
{
    // goto signature

    if (not pcbEd)
    {
        #if defined(cbDEBUG)
        cbAssertNonFatal(pcbEd && "LSP_ToToDeclaration called with nullptr");
        #endif
        return;
     }

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to LSP_GoToDeclaration before initialization.");
        return ;
    }
    if (not GetLSP_IsEditorParsed(pcbEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pcbEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pcbEd).Length() ? GetEditorsProjectTitle(pcbEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    wxString fileURI = fileUtils.FilePathToURI(pcbEd->GetFilename() );
    fileURI.Replace("\\", "/");
    //-fileURI.MakeLower().Replace("f:", "");

    cbStyledTextCtrl* pCtrl = pcbEd->GetControl();
    if (not pCtrl) return;

    int edCaretPosn = argCaretPosition;
    if (not edCaretPosn)
        edCaretPosn = pCtrl->GetCurrentPos();
    int edLineNum   = pCtrl->LineFromPosition(edCaretPosn);
    //-int edColumn    = pCtrl->GetColumn(edCaretPosn); //(ollydbg 2022/11/06 forum msg#244)
    int edColumn   = GetEditorsCaretColumn(pCtrl);

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    Position position;
    position.line       = edLineNum;
    position.character  = edColumn;
    writeClientLog(StdString_Format("<<< GoToDeclaration:\n%s,line[%d], char[%d]", docuri.c_str(), position.line, position.character) );

    // Tell server if text has changed
    LSP_DidChange(pcbEd);

    if (id)
    {
        // RRID is a RequestResponseID used to return cland response to requestor
        wxString reqID = wxString::Format("%cRRID%d", STX, int(id));
        reqID.Replace(wxString::Format("%c%c", STX ,STX), STX);

        try { GoToDeclarationByID(docuri, position, reqID.ToStdString()); }
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_GoToDeclaration() error: %s\n%s", err.what(), fileURI.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    else
    {
        try { GoToDeclaration(docuri, position); }
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_GoToDeclaration() error: %s\n%s", err.what(), fileURI.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    //-SetLSP_EditorRequest(pcbEd, "textDocument/declaration", argCaretPosition);
    SetLastLSP_Request(pcbEd->GetFilename(), "textDocument/declaration");

    return ;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_FindReferences(cbEditor* pEd, int argCaretPosition)
// ----------------------------------------------------------------------------
{
    // goto signature

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_FindReferences called with nullptr");
    #endif
    if (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to LSP_FindReferences before initialization.");
        return ;
    }

    if (not pEd)  return;

    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pEd).Length() ? GetEditorsProjectTitle(pEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    int caretPosn  = argCaretPosition;
    int edLineNum  = pCtrl->LineFromPosition(caretPosn);
    //-int edColumn    = pCtrl->GetColumn(caretPosn); //(ollydbg 2022/11/06 forum msg#244)
    int edColumn   = GetEditorsCaretColumn(pCtrl);

    if (not argCaretPosition)
    {
        caretPosn  = pCtrl->GetCurrentPos();
        edLineNum  = pCtrl->LineFromPosition(caretPosn);
        //-edColumn    = pCtrl->GetColumn(caretPosn); //(ollydbg 2022/11/06 forum msg#244)
        edColumn   = GetEditorsCaretColumn(pCtrl);
    }

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    Position position;
    //-const int posn      = pCtrl->GetCurrentPos();
    //-int startPosn = pCtrl->WordStartPosition(caretPosn, true);
    //-const int endPosn   = pCtrl->WordEndPosition(posn, true); //not needed
    position.line       = edLineNum;
    position.character  = edColumn;
    writeClientLog(StdString_Format("<<< FindReferences:\n%s,line[%d], char[%d]", docuri.c_str(), position.line, position.character) );

    // Report changes to server else reported line references will be wrong.
    LSP_DidChange(pEd);

    try { References(docuri, position); }
    catch(std::exception &err)
    {
        wxString errMsg(wxString::Format("\nLSP_FindReferences() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
        writeClientLog("Error: " + errMsg.ToStdString());
        return;
    }

    //-SetLSP_EditorRequest(pEd, "textDocument/references", argCaretPosition);
    SetLastLSP_Request(pEd->GetFilename(), "textDocument/references");

    return ;
}//end LSP_FindReferences()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_RequestRename(cbEditor* pEd, int argCaretPosition, wxString newName)
// ----------------------------------------------------------------------------
{
    // LSP Rename

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_FindReferences called with nullptr");
    #endif
    if (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to LSP_RequestRename() before initialization.");
        return ;
    }

    if (not pEd)  return;

    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pEd).Length() ? GetEditorsProjectTitle(pEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    int caretPosn  = argCaretPosition;
    int edLineNum  = pCtrl->LineFromPosition(caretPosn);
    //-int edColumn = pCtrl->GetColumn(caretPosn); //(ollydbg 2022/11/06 forum msg#244)
    int edColumn   = GetEditorsCaretColumn(pCtrl);


    if (not argCaretPosition)
    {
        caretPosn  = pCtrl->GetCurrentPos();
        edLineNum  = pCtrl->LineFromPosition(caretPosn);
        //-edColumn    = pCtrl->GetColumn(caretPosn); //(ollydbg 2022/11/06 forum msg#244)
        edColumn   = GetEditorsCaretColumn(pCtrl);

    }

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    Position position;
    //-const int posn      = pCtrl->GetCurrentPos();
    //-int startPosn = pCtrl->WordStartPosition(caretPosn, true);
    //-const int endPosn   = pCtrl->WordEndPosition(posn, true); //not needed
    position.line       = edLineNum;
    position.character  = edColumn;
    writeClientLog(StdString_Format("<<< RequestRename:\n%s,line[%d], char[%d]", docuri.c_str(), position.line, position.character) );

    // Report changes to server else reported line references will be wrong.
    LSP_DidChange(pEd);

    string_ref newNameStrRef(newName.c_str()); //(ph 2022/01/3)
    try { Rename(docuri, position, newNameStrRef); }
    catch(std::exception &err)
    {
        wxString errMsg(wxString::Format("\nLSP_FindReferences() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    //-SetLSP_EditorRequest(pEd, "textDocument/references", argCaretPosition);
    SetLastLSP_Request(pEd->GetFilename(), "textDocument/references");

    return ;
}//end LSP_FindReferences()

// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_RequestSymbols(cbEditor* pEd, size_t rrid)
// ----------------------------------------------------------------------------
{
    // goto signature

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_GetSymbols called with nullptr");
    #endif
    if (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to LSP_GetSymbols before initialization.");
        return ;
    }

    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pEd).Length() ? GetEditorsProjectTitle(pEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    //Does this really matter ?
    //-if ((not pEd) or (not ClientProjectOwnsFile(pEd)) ) return; //(ph 2021/11/9)
    if (not pEd) return;

    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    writeClientLog(StdString_Format("<<< LSP_GetSymbols:\n%s", docuri.c_str()) );

    // Tell LSP server when text has changed
    LSP_DidChange(pEd);

    wxString rridHdr = fileURI;
    if (rrid)
    {
        rridHdr.Append(wxString::Format("%cRRID%d", STX, int(rrid))); // set ResponseRequestID
        rridHdr.Replace(wxString::Format("%c%c", STX ,STX), STX);
    }

    //-try { DocumentSymbolByID(docuri, rridHdr.ToStdString() ); }
    try { DocumentSymbolByID(docuri, GetstdUTF8Str(rridHdr)); } //(ollydbg 2022/10/30) ticket #78

    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        wxString errMsg(wxString::Format("\nLSP_RequestSymbols() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    SetLastLSP_Request(pEd->GetFilename(), "textDocument/documentSymbol");
    SetLSP_EditorHasSymbols(pEd, false);

    return ;
}//end LSP_RequestSymbols()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_RequestSymbols(wxString filename, cbProject* pProject, size_t rrid) //(ph 2021/04/11)
// ----------------------------------------------------------------------------
{
    #if defined(cbDEBUG)
    cbAssertNonFatal(pProject && "LSP_GetSymbols called with null Project ptr");
    cbAssertNonFatal(filename.Length() && "LSP_GetSymbols called with null filename");
    #endif
    if ((not pProject) or (not filename.Length())) return;

    if (not GetLSP_Initialized())
    {
        wxString msg = "LSP: attempt to LSP_GetSymbols before initialization.";
        msg += wxString::Format("\n %s() Line:%d", __FUNCTION__, __LINE__);
        cbMessageBox(msg);
        return ;
    }

    if ((not pProject) or (not pProject->GetFileByFilename(filename,false)) ) return;
    if (not wxFileExists(filename)) return;

    wxString fileURI = fileUtils.FilePathToURI(filename);
    fileURI.Replace("\\", "/");
    //-fileURI.MakeLower().Replace("f:", "");

    std::unique_ptr<cbStyledTextCtrl> pCtrl = std::unique_ptr<cbStyledTextCtrl>(GetNewHiddenEditor(filename));
    if (not pCtrl) return;

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    writeClientLog(StdString_Format("<<< LSP_GetSymbols:\n%s", docuri.c_str()) );

    wxString rridHdr = fileURI;
    if (rrid)
    {
        rridHdr.Append(wxString::Format("%cRRID%d", STX, int(rrid))); // set RequestResponseID used to return response to requestor
        rridHdr.Replace(wxString::Format("%c%c", STX ,STX), STX);
    }

    //try { DocumentSymbol(docuri); }
    //-try { DocumentSymbolByID(docuri, rridHdr.ToStdString() ); }
    try { DocumentSymbolByID(docuri, GetstdUTF8Str(rridHdr) ); } //(ollydbg 2022/10/30) ticket #78
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        wxString errMsg(wxString::Format("\nLSP_RequestSymbols() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    SetLastLSP_Request(filename, "textDocument/documentSymbol");

    return ;
}//end LSP_RequestSymbols()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_RequestSemanticTokens(cbEditor* pEd, size_t rrid)            //(ph 2021/03/16)
// ----------------------------------------------------------------------------
{
    // goto signature

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_RequestSemanticTokens called with nullptr");
    #endif
    if (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to LSP_GetSemanticTokens before initialization.");
        return ;
    }

    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format("%s: %s not yet parsed.", __FUNCTION__,
                            wxFileName(pEd->GetFilename()).GetFullName());
        //-InfoWindow::Display("LSP", wxString::Format(_("%s\n not yet parsed."), pEd->GetFilename()) );
        CCLogger::Get()->DebugLog(msg);
        return;
    }

    if ((not pEd) or (not ClientProjectOwnsFile(pEd)) ) return;
    if (not pEd) return;

    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");
    //-fileURI.MakeLower().Replace("f:", "");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    writeClientLog(StdString_Format("<<< LSP_GetSemanticTokens:\n%s", docuri.c_str()) );

    // Tell LSP server when text has changed
    LSP_DidChange
    (pEd);

    //get SemanticTokens
    if (rrid)
    {
        //RRID == RequestRedirectionID used to return the response data to the requestor
        wxString rridHdr = wxString::Format("%s%cRRID%d", fileURI, STX, rrid);
        rridHdr.Replace(wxString::Format("%c%c", STX ,STX), STX);

        //-try { SemanticTokensByID(docuri, rridHdr.ToStdString() ); }
        try { SemanticTokensByID(docuri, GetstdUTF8Str(rridHdr) ); } //(ollydbg 2022/10/30) ticket #78
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_RequestSemanticTokens() error: %s\n%s", err.what(), docuri.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    else
    {
        //-try { SemanticTokensByID(docuri, fileURI.ToStdString() ); }
        try { SemanticTokensByID(docuri, GetstdUTF8Str(fileURI) ); } //(ollydbg 2022/10/30) ticket #78

        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_RequestSemanticTokens() error: %s\n%s", err.what(), docuri.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    //-SetLSP_EditorRequest(pEd, "textDocument/documentTokens", 0);
    SetLastLSP_Request(pEd->GetFilename(), "textDocument/semanticTokens");


    return ;
}//end LSP_RequestSymbols()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_RequestSemanticTokens(wxString filename, cbProject* pProject, size_t rrid)
// ----------------------------------------------------------------------------
{
    #if defined(cbDEBUG)
    cbAssertNonFatal(pProject && "LSP_RequestSemanticTokens() called with null Project ptr");
    cbAssertNonFatal(filename.Length() && "LSP_RequestSemanticTokens() called with null filename");
    #endif
    if ((not pProject) or (not filename.Length())) return;

    if (not GetLSP_Initialized())
    {
        wxString msg = "LSP: attempt to LSP_RequestSemanticTokens() before initialization.";
        msg += wxString::Format("\n %s() Line:%d", __FUNCTION__, __LINE__);
        cbMessageBox(msg);
        return ;
    }

    if ((not pProject) or (not pProject->GetFileByFilename(filename,false)) ) return;
    if (not wxFileExists(filename)) return;

    wxString fileURI = fileUtils.FilePathToURI(filename);
    fileURI.Replace("\\", "/");
    //-fileURI.MakeLower().Replace("f:", "");

    std::unique_ptr<cbStyledTextCtrl> pCtrl = std::unique_ptr<cbStyledTextCtrl>(GetNewHiddenEditor(filename));
    if (not pCtrl) return;

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    writeClientLog(StdString_Format("<<< LSP_RequestSemanticTokens:\n%s", docuri.c_str()) );

    wxString rridHdr = fileURI;
    if (rrid)
    {
        rridHdr.Append(wxString::Format("%cRRID%d", STX, int(rrid))); // set RequestResponseID used to return response to requestor
        rridHdr.Replace(wxString::Format("%c%c", STX ,STX), STX);
    }

    //try { DocumentSymbol(docuri); }
    //-try { SemanticTokensByID(docuri, fileURI.ToUTF8(wxConvUTF8); } //(ollydbg 2022/10/30) ticket #78
    try { SemanticTokensByID(docuri, GetstdUTF8Str(fileURI) ); }
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        wxString errMsg(wxString::Format("\nLSP_RequestSemanticTokens() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    SetLastLSP_Request(filename, "textDocument/documentTokens");

    return ;
}//end LSP_SemanticTokens()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_DidChange(cbEditor* pEd)
// ----------------------------------------------------------------------------
{
    // DidChange the text

    // Info:
    //    method: textDocument/didChange
    //    params: DidChangeTextDocumentParams defined as follows:
    //    interface DidChangeTextDocumentParams {
    //        /**
    //         * The document that did change. The version number points
    //         * to the version after all provided content changes have
    //         * been applied.
    //         */
    //        textDocument: VersionedTextDocumentIdentifier;
    //
    //        /**
    //         * The actual content changes. The content changes describe single state
    //         * changes to the document. So if there are two content changes c1 (at
    //         * array index 0) and c2 (at array index 1) for a document in state S then
    //         * c1 moves the document from S to S' and c2 from S' to S''. So c1 is
    //         * computed on the state S and c2 is computed on the state S'.
    //         *
    //         * To mirror the content of a document using change events use the following
    //         * approach:
    //         * - start with the same initial content
    //         * - apply the 'textDocument/didChange' notifications in the order you
    //         *   receive them.
    //         * - apply the `TextDocumentContentChangeEvent`s in a single notification
    //         *   in the order you receive them.
    //         */
    //        contentChanges: TextDocumentContentChangeEvent[];
    //    }
    //
    //       An event describing a change to a text document.
    ///       If range and rangeLength are
    ///       omitted the new text is considered to be the full content of the document.

    //    export type TextDocumentContentChangeEvent = {
    //        /**
    //         * The range of the document that changed.
    //         */
    //        range: Range;
    //
    //        /**
    //         * The optional length of the range that got replaced.
    //         *
    //         * @deprecated use range instead.
    //         */
    //        rangeLength?: uinteger;
    //
    //        /**
    //         * The new text for the provided range.
    //         */
    //        text: string;
    //    } | {
    //        /**
    //         * The new text of the whole document.
    //         */
    //        text: string;
    //    }

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_DidChange called with nullptr");
    #endif
    if (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to call LSP_DidChange() before initialization.");
        return ;
    }

    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pEd).Length() ? GetEditorsProjectTitle(pEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    // If not modified, there's no need to send file text to LSP server.
    bool modified = GetLSP_IsEditorModified(pEd);
    if (not modified)
        return;

    int oldLineCount = m_FileLinesHistory[pEd];
    int newLineCount = pCtrl->GetLineCount();
    bool hasChangedLineCount = oldLineCount != newLineCount;
    m_FileLinesHistory[pEd] = newLineCount;

    int currPosn        = pCtrl->GetCurrentPos();
    int lineChangedNbr  = pCtrl->LineFromPosition(currPosn);
    int lineEndNbr      = lineChangedNbr;
    int lineBeginCol    = 0;
    wxString lineText   = pCtrl->GetLine(lineChangedNbr);

    TextDocumentContentChangeEvent didChangeEvent;

    wxString edText;
    // If line count has changed, send full text, else send changed line.
    // Also, special handling for last line of text
        ///- hasChangedLineCount = true; //(ph 2021/07/26) //(ph 2021/10/11) clangd v13 looks ok
    if ( (hasChangedLineCount) or (lineChangedNbr >= oldLineCount-1) )
        // send the whole editor text to the server.

        //2022/07/16 reverted from rev 67 to rev66 because causing empty buffer to be sent to clangd
        // when using chinese chars.
        // Assure text is UTF8 before handing to DidChange()
        //#if wxCHECK_VERSION(3,1,5) //3.1.5 or higher
        //edText = pCtrl->GetText().utf8_string();    //(ph 2022/06/22)
        //#else
        //edText = pCtrl->GetText().ToUTF8();
        //#endif

        edText = pCtrl->GetText();
    else
    {
        // send only the one line that changed. //(send previous, current, and next line maybe??)
        edText = lineText;
        Range range;
        range.start.line = lineChangedNbr;
        range.start.character = lineBeginCol;
        range.end.line = lineEndNbr+1;
        range.end.character = 0;
        //didChangeEvent.rangeLength = lineText.Length(); dont use. it's been deprecated
        didChangeEvent.range = range;
    }

    #if wxCHECK_VERSION(3,1,5) //3.1.5 or higher
    didChangeEvent.text = edText.ToStdString(wxConvUTF8); //(ollydbg 2022/07/22) https://forums.codeblocks.org/index.php/topic,24357.msg170611.html#msg170611
    #else
    //didChangeEvent.text = edText; //works but does not observe std::string utf8 protocol
    const wxScopedCharBuffer edTestBuffer = edText.ToUTF8(); //(AndrewCo 2022/07/31) https://sourceforge.net/p/cb-clangd-client/discussion/general/thread/dd87cbec03/
    didChangeEvent.text = std::string(edTestBuffer.data());
    #endif

    std::vector<TextDocumentContentChangeEvent> tdcce{didChangeEvent};
    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    // **debugging**
    //    writeClientLog(wxString::Format("DidChange: lineStrt[%d] colStrt[%d] lineEnd[%d] colEnd[%d] textLth[%d] text[%s]\n",
    //                    range.start.line, range.start.character,
    //                    range.end.line, range.end.character,
    //                    didChangeEvent.rangeLength.value(),
    //                    didChangeEvent.text) );

    try { DidChange(docuri, tdcce); }
    catch(std::exception &err)
    {
        wxString errMsg(wxString::Format("\nLSP_DidChange error: %s\n%s", err.what(), fileURI.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    SetLSP_EditorModified(pEd, false);
    SetLastLSP_Request(pEd->GetFilename(), "textDocument/didChange");

    //clangd seems to allow completions to work w/o waiting for didChange
    SetDidChangeTimeBusy(0); //clangd seems to allow completions to work w/o waiting

    // Don't do this, there's no response from a didChange() to clear this:
    //- LSP_AddToServerFilesParsing(pEd->GetFilename() );

    return;
}//end LSP_DidChange()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_CompletionRequest(cbEditor* pEd, int rrid)
// ----------------------------------------------------------------------------
{
    // Code completion

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_Completion called with nullptr");
    #endif
    if (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to call LSP_Completion() before initialization.");
        return ;
    }
    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pEd).Length() ? GetEditorsProjectTitle(pEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    if (not pEd) return;
    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    // Allow completion requests to punt when a previous completion is busy for this file.
    // It's cleared when the LSP responds to the completion request.
    if ( GetCompletionTimeBusy()) return;

    // Tell LSP server if file has changed, else asking for completion w/o updating can crash Clangd
    LSP_DidChange(pEd);

    int tknStart = 0; int tknEnd = 0;
    tknEnd = pCtrl->GetCurrentPos();
    tknStart = pCtrl->WordStartPosition(tknEnd, true);


    wxString fileURI = fileUtils.FilePathToURI( pEd->GetFilename());
    fileURI.Replace("\\", "/");

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    Position position;
    position.line        = pCtrl->LineFromPosition(tknStart);
    size_t lineStartPosn = pCtrl->PositionFromLine(position.line);
    size_t relativeTokenStart = tknStart - lineStartPosn;
    size_t relativeTokenEnd   = tknEnd - lineStartPosn;

    position.character   = relativeTokenEnd; //(ph 2021/07/31)
    CompletionContext context;
    context.triggerKind = CompletionTriggerKind::Invoked;
    context.triggerCharacter = ".";
    wxString lineText = pCtrl->GetLine(position.line);
    wxString token    = lineText.Mid(relativeTokenStart, tknEnd-tknStart);

    writeClientLog(StdString_Format("<<< Completion:\nline[%d], col[%d] token[%s] uri[%s]",
                              position.line, position.character, token.ToStdString().c_str(), docuri.c_str()) );

    try { Completion(docuri, position, context); }
    catch(std::exception &err)
    {
        //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
        wxString errMsg(wxString::Format("\nLSP_Completion error: %s\n%s", err.what(), fileURI.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
        return;
    }

    //-SetLSP_EditorRequest(pEd, "textDocument/completion", tknStart);
    SetLastLSP_Request(pEd->GetFilename(), "textDocument/completion");
    SetCompletionTimeBusy(2000); //allow max 2 seconds block to complete request.

    return ;
}//end LSP_Completion()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_Hover(cbEditor* pEd, int posn, int rrid)
// ----------------------------------------------------------------------------
{
    // Hover
    //rrid param is Request redirection id.

    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_Hover called with nullptr");
    #endif
    if  (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to call LSP_Hover() before initialization.");
        return ;
    }
    if (not GetLSP_IsEditorParsed(pEd))
    {
        InfoWindow::Display("LSP", wxString::Format(_("%s\n not yet parsed."),
                    wxFileName(pEd->GetFilename()).GetFullName()) );
        return;
    }

    if (not pEd) return;
    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());

    const int startPosn = pCtrl->WordStartPosition(posn, true);
    Position position;
    position.line       = pCtrl->LineFromPosition(posn);
    position.character  = startPosn - pCtrl->PositionFromLine(position.line);
    writeClientLog(StdString_Format("<<< Hover:\n%s,line[%d], char[%d]", docuri.c_str(), position.line, position.character) );

    // Inform LSP server if text has changed
    LSP_DidChange(pEd);

    wxString rridHdr = fileURI;
    if (rrid) //if Requesting redirection id
    {
        rridHdr.Append(wxString::Format("%cRRID%d", STX, int(rrid))); // set ResponseRequestID
        rridHdr.Replace(wxString::Format("%c%c", STX ,STX), STX);
        //-try { HoverByID(docuri, position, rridHdr.ToStdString() ); }
        try { HoverByID(docuri, position, GetstdUTF8Str(rridHdr) ); } //(ollydbg 2022/10/30) ticket #78
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_Hover() error: %s\n%s", err.what(), docuri.c_str()) );
            writeClientLog(errMsg.ToStdString());
            cbMessageBox(errMsg);
        }
    }
    else //ordinary request without redirection
    {
        try { Hover(docuri, position); }
        catch(std::exception &err)
        {
            //printf("read error -> %s\nread -> %s\n ", e.what(), read.c_str());
            wxString errMsg(wxString::Format("\nLSP_Hover() error: %s\n%s", err.what(), docuri.c_str()) );
            writeClientLog(GetstdUTF8Str(errMsg));
            cbMessageBox(errMsg);
        }
    }

    SetLastLSP_Request(pEd->GetFilename(), "textDocument/hover");

    return;

}//end LSP_Hover()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::LSP_SignatureHelp(cbEditor* pEd, int posn)
// ----------------------------------------------------------------------------
{
    // SignatureHelp
    #if defined(cbDEBUG)
    cbAssertNonFatal(pEd && "LSP_SignatureHelp called with nullptr");
    #endif
    if  (not pEd) return;

    if (not GetLSP_Initialized())
    {
        cbMessageBox("LSP: attempt to call LSP_SignatureHelp() before initialization.");
        return ;
    }
    if (not GetLSP_IsEditorParsed(pEd))
    {
        wxString msg = wxString::Format(_("%s\nnot yet parsed.\nProject:"),
                wxFileName(pEd->GetFilename()).GetFullName());
        msg += GetEditorsProjectTitle(pEd).Length() ? GetEditorsProjectTitle(pEd) : "None";
        InfoWindow::Display("LSP: File not yet parsed", msg ) ;
        return;
    }

    if (not pEd) return;
    wxString fileURI = fileUtils.FilePathToURI(pEd->GetFilename());
    fileURI.Replace("\\", "/");

    cbStyledTextCtrl* pCtrl = pEd->GetControl();
    if (not pCtrl) return;

    //-DocumentUri docuri = DocumentUri(fileURI.c_str());
    std::string stdFileURI = GetstdUTF8Str(fileURI); //(ollydbg 2022/10/30) ticket #78
    DocumentUri docuri = DocumentUri(stdFileURI.c_str());
    const int startPosn = pCtrl->WordStartPosition(posn, true);
    Position position;
    position.line       = pCtrl->LineFromPosition(posn);
    position.character  = startPosn - pCtrl->PositionFromLine(position.line);
    writeClientLog(StdString_Format("<<< SignatureHelp:\n%s,line[%d], char[%d]", docuri.c_str(), position.line, position.character) );

    // Inform LSP server if text has changed
    LSP_DidChange(pEd);

    try { SignatureHelp(docuri, position); }
    catch(std::exception &err)
    {
        wxString errMsg(wxString::Format("\nLSP_SignatureHelp() error: %s\n%s", err.what(), docuri.c_str()) );
        writeClientLog(errMsg.ToStdString());
        cbMessageBox(errMsg);
    }

    //-SetLSP_EditorRequest(pEd, "textDocument/hover", posn);
    SetLastLSP_Request(pEd->GetFilename(), "textDocument/signatureHelp");

    return;

}//end LSP_SignatureHelp()
// ----------------------------------------------------------------------------
std::string ProcessLanguageClient::LSP_GetTimeHMSM() //Get time in hours minute second milliseconds
// ----------------------------------------------------------------------------
{
     return GetTime_in_HH_MM_SS_MMM();
}
// ----------------------------------------------------------------------------
size_t ProcessLanguageClient::GetCompilerDriverIncludesByFile(wxArrayString& resultArray, cbProject* pProject, wxString filename)
// ----------------------------------------------------------------------------
{
    // Get the includes generated by the compiler diriver
    // GetCompilerDriverIncludesByFile() not needed with llvm v12
    return 0;

    if (not pProject) return 0;

    ProjectFile* pProjectFile = pProject->GetFileByFilename(filename, false);
    if (not pProjectFile) return 0;

    // create array of compiler built-in include files needed for for clang '-fsyntax-only'
    // The files arn't found by clang for some unknown reason to me.
    wxArrayString gccResults, gccErrors;

    ProjectBuildTarget* pTarget = nullptr;
    if (not pTarget)  // No target when target is virtual (eg. 'ALL');
    {
        // Use the first target associated with this file.
        //get build targets using this file
        wxArrayString buildTargets = pProjectFile->GetBuildTargets();
        if (not buildTargets.Count()) return 0;
        //fetch the first build target using this file
        pTarget = pProject->GetBuildTarget(buildTargets[0]);
    }
    if (not pTarget) return 0;

    Compiler* pCompiler = CompilerFactory::GetCompiler(pTarget->GetCompilerID());
    wxString masterPath = pCompiler ? pCompiler->GetMasterPath() : "";
    wxString compilerID = pCompiler ? pCompiler->GetID() : "";
    CompilerPrograms compilerPrograms;

    // if no entry for compilerID get compiler include filenames
    if (pCompiler and (CompilerDriverIncludesMap.find(compilerID) == CompilerDriverIncludesMap.end()) )
    {
        compilerPrograms = pCompiler->GetPrograms() ;
        wxString exeCmd = masterPath + "\\bin\\" + compilerPrograms.CPP;
        exeCmd += " -E -Wp,-v -xc++ nul" ;
        if (not platform::windows)
           exeCmd.Replace("nul", "/dev/null") ;
        wxArrayString gccResults ;
        // The output we want will go to stderr
       wxExecute(exeCmd, gccResults, gccErrors, wxEXEC_BLOCK);

        // Example response from "c++.exe -E -Wp,-v -xc++ nul":
        // F:\usr\proj\cbDevel31\trunk\src\output31_64>f:\usr\MinGW810_64seh\bin\g++.exe -E -Wp,-v -xc++ nul
        // Example result:
        // ignoring duplicate directory "f:/usr/MinGW810_64seh/lib/gcc/../../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++"
        // ignoring duplicate directory "f:/usr/MinGW810_64seh/lib/gcc/../../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++/x86_64-w64-mingw32"
        // ignoring duplicate directory "f:/usr/MinGW810_64seh/lib/gcc/../../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++/backward"
        // ignoring duplicate directory "f:/usr/MinGW810_64seh/lib/gcc/../../lib/gcc/x86_64-w64-mingw32/8.1.0/include"
        // ignoring nonexistent directory "C:/mingw810/x86_64-810-posix-seh-rt_v6-rev0/mingw64C:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/8.1.0/../../../../include"
        // ignoring duplicate directory "f:/usr/MinGW810_64seh/lib/gcc/../../lib/gcc/x86_64-w64-mingw32/8.1.0/include-fixed"
        // ignoring duplicate directory "f:/usr/MinGW810_64seh/lib/gcc/../../lib/gcc/x86_64-w64-mingw32/8.1.0/../../../../x86_64-w64-mingw32/include"
        // ignoring nonexistent directory "C:/mingw810/x86_64-810-posix-seh-rt_v6-rev0/mingw64/mingw/include"
        // #include "..." search starts here:
        // #include <...> search starts here:
        //  f:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++
        //  f:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++/x86_64-w64-mingw32
        //  f:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++/backward
        //  f:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include
        //  f:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include-fixed
        //  f:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/../../../../x86_64-w64-mingw32/include
        // End of search list.
        // # 1 "nul"
        // # 1 "<built-in>"
        // # 1 "<command-line>"
        // # 1 "nul"

        LogManager* pLogMgr = Manager::Get()->GetLogManager();
        wxUnusedVar(pLogMgr);

        // Search for "#include.." lines in compiler response and pull out the
        // lines until "End of search list" string is reached.

        //output compiler includes of executing "compiler.exe -E -Wp,-v -xc++ nul"
        wxArrayString aCompileDriverIncludes;
        aCompileDriverIncludes.Clear();
        bool searchStartsHere = false;
        for (wxString& line : gccErrors)
        {
            //-pLogMgr->DebugLog(line); debugging
            if (line.StartsWith("#include <"))
            {
                searchStartsHere = true;
                continue; //jump past "#include <"
            }
            if (line.StartsWith("End of search list.")) break;
            if (not searchStartsHere) continue;
            line.Trim(true).Trim(false);
            wxFileName fnIncludeDir(line, "");
            if (fnIncludeDir.DirExists())
                fnIncludeDir.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE );
            else continue;
            wxString incFile = fnIncludeDir.GetPath();
            incFile.Replace("\\", "/");
            // lib/gcc/x86_64-w64-mingw32/8.1.0/include causes g++ undeclared identifier '__builtin_ia32_bsrsi' errors.
            if (incFile.Contains("/lib/gcc/") and incFile.EndsWith("/include") )
                continue; // skip this line
            if (searchStartsHere)
                aCompileDriverIncludes.Add(incFile);
        }//endfor line

        wxString sDriverIncludes;
        for (wxString entry : aCompileDriverIncludes)
            sDriverIncludes += ("-I" + entry + " ");

        if (sDriverIncludes.Length())
            CompilerDriverIncludesMap[compilerID] = sDriverIncludes;

        if (aCompileDriverIncludes.GetCount())
            resultArray = aCompileDriverIncludes;

    }//endif get compiler include filenames
    else
    {
        // compiler driver includes have already been found
        wxString sDriverIncludes = CompilerDriverIncludesMap[compilerID];
        resultArray = GetArrayFromString(sDriverIncludes, " ", /*trimSpaces*/ true);
    }
    return resultArray.Count();

}//GetCompilersIncludesForFile
// ----------------------------------------------------------------------------
void ProcessLanguageClient::CheckForTooLongCommandLine(wxString& executableCmd, wxArrayString& outputCommandArray,
                                    const wxString& basename ,const wxString& path) const
// ----------------------------------------------------------------------------
{

#ifndef CB_COMMAND_LINE_MAX_LENGTH
#ifdef __WXMSW__
// the actual limit is 32767 (source: https://devblogs.microsoft.com/oldnewthing/20031210-00/?p=41553 )
#define CB_COMMAND_LINE_MAX_LENGTH 32767
#else
// On Linux the limit should be inf
// List of collected length limits: https://www.in-ulm.de/~mascheck/various/argmax/
// Actual limit on Linux Mint 18 is 131072 (this is the limit for args + environ for exec())
#define CB_COMMAND_LINE_MAX_LENGTH 131072
#endif // __WXMSW__
#endif // CB_COMMAND_LINE_MAX_LENGTH
    const int maxLength = CB_COMMAND_LINE_MAX_LENGTH;
    if (executableCmd.length() > maxLength)
    {
        wxFileName responseFileName(path);
        responseFileName.SetName(basename);
        responseFileName.SetExt("respFile");
        // Path handling has to be so complicated because of wxWidgets error https://trac.wxwidgets.org/ticket/831
        // The path for creating the folder structure has to be relative
        const wxString responseFilePath = responseFileName.GetFullPath();
        wxFileName relative = responseFileName;
        relative.MakeRelativeTo(wxFileName::GetCwd());
        if (!wxFileName::Mkdir(relative.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        {
            outputCommandArray.Add(ClientHelp::COMPILER_ERROR_LOG + _("Could not create directory for ") + responseFilePath);
            return;
        }

        outputCommandArray.Add(ClientHelp::COMPILER_ONLY_NOTE_LOG + wxString::Format(_("Command line is too long: Using responseFile: %s"), responseFilePath));
        outputCommandArray.Add(ClientHelp::COMPILER_ONLY_NOTE_LOG + wxString::Format(_("Complete command line: %s"), executableCmd));

        // Begin from the back of the command line and search for a position to split it. A suitable position is a white space
        // so that the resulting command line inclusive response file is shorter than the length limit
        const int responseFileLength = responseFilePath.length();
        size_t startPos = executableCmd.rfind(' ', maxLength - responseFileLength);
        if (startPos == 0 || startPos == wxString::npos)   // Try to find the first command again...
            startPos = executableCmd.find(' ');
        if (startPos > maxLength)
        {
            outputCommandArray.Add(ClientHelp::COMPILER_WARNING_LOG + _("Could not split command line for response file. This probably will lead to failed compiling") );
        }
        wxString restCommand = executableCmd.Right(executableCmd.length() - startPos);
        outputCommandArray.Add(ClientHelp::COMPILER_ONLY_NOTE_LOG + wxString::Format(_("Response file: %s"), restCommand));
        // Path escaping Needed for windows.  '\' has to be '\\' in the response file for mingw-gcc
        restCommand.Replace("\\", "\\\\");
        wxFile file(responseFilePath, wxFile::OpenMode::write);
        if (!file.IsOpened())
        {
            outputCommandArray.Add(ClientHelp::COMPILER_ERROR_LOG + wxString::Format(_("Could not open response file in %s"), responseFilePath));
            return;
        }

        file.Write(restCommand);
        file.Close();
        executableCmd = executableCmd.Left(startPos) + " @\"" + responseFilePath + "\"";
        outputCommandArray.Add(ClientHelp::COMPILER_ONLY_NOTE_LOG + wxString::Format(_("New command: %s"), executableCmd));
    }
}//end CheckForTooLongCommandLine()
// ----------------------------------------------------------------------------
wxArrayString ProcessLanguageClient::GetCompileFileCommand(ProjectBuildTarget* pTarget, ProjectFile* pf) const
// ----------------------------------------------------------------------------
{
    // This code lifted from directcommand.cpp
    wxArrayString retArray;
    wxArrayString ret_generatedArray;

    // Sanity checks                     //(ph 2022/10/17)
    #if defined(cbDEBUG)
    cbAssertNonFatal(pTarget && "null ProjectBuildTarget pointer");
    cbAssertNonFatal(pf && "null ProjectFile pointer");
    #endif

    if (not pTarget)
        return retArray;
    if (not pf)
        return retArray;

    // is it compilable?
    if (!pf || !pf->compile)
        return retArray;

    if (pf->compilerVar.IsEmpty())
    {
        CCLogger::Get()->DebugLog(_("Cannot resolve compiler var for project file."));
        return retArray;
    }

    // Verify that the projectFile is associated with the target, else crashes occur.
    wxArrayString buildTargets = pf->GetBuildTargets();
    if (not buildTargets.Count()) return retArray;
    bool targetFound = false;
    for (wxString tgtTitle: buildTargets)
        if (tgtTitle == pTarget->GetTitle()) targetFound = true; //ok
    if (not targetFound) return retArray;

    Compiler* compiler = CompilerFactory::GetCompiler(pTarget->GetCompilerID());

    if (!compiler)
    {
        CCLogger::Get()->DebugLog(_("Can't access compiler for file."));
        return retArray;
    }

    cbProject* pProject = pTarget->GetParentProject();
    if (not pProject) return retArray;

    const pfDetails& pfd = pf->GetFileDetails(pTarget);
    wxString object      = (compiler->GetSwitches().UseFlatObjects)
                         ? pfd.object_file_flat : pfd.object_file;
    wxString object_dir  = (compiler->GetSwitches().UseFlatObjects)
                         ? pfd.object_dir_flat_native : pfd.object_dir_native;

    // lookup file's type
    const FileType ft = FileTypeOf(pf->relativeFilename);
    bool is_resource = (ft == ftResource);
    bool is_header   = (ft == ftHeader) or (ParserCommon::FileType(pf->relativeFilename) == ParserCommon::ftHeader); //(ph 2022/06/1)

    // allowed resources under all platforms: makes sense when cross-compiling for
    // windows under linux.
    // and anyway, if the user is dumb enough to try to compile resources without
    // having a resource compiler, (s)he deserves the upcoming build error ;)

    wxString compiler_cmd;
    if (!is_header || compiler->GetSwitches().supportsPCH)
    {
        const CompilerTool* tool = compiler->GetCompilerTool(is_resource ? ctCompileResourceCmd : ctCompileObjectCmd, pf->file.GetExt());

        // does it generate other files to compile?
        for (size_t i = 0; i < pf->generatedFiles.size(); ++i)
            AppendArray(GetCompileFileCommand(pTarget, pf->generatedFiles[i]), ret_generatedArray); // recurse

        pfCustomBuild& pcfb = pf->customBuild[compiler->GetID()];
        if (pcfb.useCustomBuildCommand)
            compiler_cmd = pcfb.buildCommand;
        else if (tool)
            compiler_cmd = tool->command;
        else
            compiler_cmd = wxEmptyString;

        wxString source_file;
        if (compiler->GetSwitches().UseFullSourcePaths)
            source_file = UnixFilename(pfd.source_file_absolute_native);
        else
            source_file = pfd.source_file;

    #ifdef command_line_generation
        #warning command_line_generation is defined for debugging purposes
        CCLogger::Get()->DebugLog(wxString::Format(_T("GetCompileFileCommand[1]: compiler_cmd='%s', source_file='%s', object='%s', object_dir='%s'."),
                                                    compiler_cmd.wx_str(), source_file.wx_str(), object.wx_str(), object_dir.wx_str()));
    #endif

        // for resource files, use short from if path because if windres bug with spaces-in-paths
        if (is_resource && compiler->GetSwitches().UseFullSourcePaths)
            source_file = pf->file.GetShortPath();

        QuoteStringIfNeeded(source_file);

    #ifdef command_line_generation
        CCLogger::Get()->DebugLog(wxString::Format(_T("GetCompileFileCommand[2]: source_file='%s'."),
                                                    source_file.wx_str()));
    #endif
        cb::shared_ptr<CompilerCommandGenerator> generator(compiler ? compiler->GetCommandGenerator(pProject) : nullptr);
        generator->GenerateCommandLine(compiler_cmd, pTarget, pf, source_file, object,
                                          pfd.object_file_flat, pfd.dep_file);
    }

    if (!is_header && compiler_cmd.IsEmpty())
    {
        retArray.Add(ClientHelp::COMPILER_SIMPLE_LOG + _("Skipping file (no compiler program set): ") + pfd.source_file_native );
        return retArray;
    }

    switch (compiler->GetSwitches().logging)
    {
        case clogFull:
            retArray.Add(ClientHelp::COMPILER_SIMPLE_LOG + compiler_cmd);
            break;

        case clogSimple:
            if (is_header)
                retArray.Add(ClientHelp::COMPILER_SIMPLE_LOG + _("Pre-compiling header: ") + pfd.source_file_native );
            else
                retArray.Add(ClientHelp::COMPILER_SIMPLE_LOG + _("Compiling: ") + pfd.source_file_native );
            break;

        case clogNone: // fall-through
        default:
            break;
    }

    CheckForTooLongCommandLine(compiler_cmd, retArray, pf->file.GetFullName() ,object_dir);

    retArray.Add(compiler_cmd);

    if (is_header)
        retArray.Add(ClientHelp::COMPILER_WAIT);

    if (ret_generatedArray.GetCount())
    {
        // not only append commands for (any) generated files to be compiled
        // but also insert a "pause" to allow this file to generate its files first
        if (!is_header) // if is_header, the "pause" has already been added
            retArray.Add(ClientHelp::COMPILER_WAIT);
        AppendArray(ret_generatedArray, retArray);
    }

    // if it's a PCH, delete the previously generated PCH to avoid problems
    // (it 'll be recreated anyway)
    if ( (ft == ftHeader) && pf->compile )
    {
        wxString object_abs = (compiler->GetSwitches().UseFlatObjects)
                            ? pfd.object_file_flat_absolute_native
                            : pfd.object_file_absolute_native;

        if ( wxFileExists(object_abs) && !wxRemoveFile(object_abs) )
            CCLogger::Get()->DebugLog(_("Cannot remove old PCH file:\n") + object_abs);
    }

    for (int ii=retArray.GetCount()-1; ii>=0 ;--ii)
    {
        // clean out comment/log lines
        if (retArray[ii].StartsWith("SLOG:"))
            retArray.RemoveAt(ii);
    }
    return retArray;
}//end GetCompileFileCommand()
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::AddFileToCompileDBJson(cbProject* pProject, ProjectBuildTarget* pTarget, const wxString& argFullFilePath, json* pJson)    //(ph 2020/12/1)
// ----------------------------------------------------------------------------
{
    // Add file to compile_commands.json if not already present.

    // gcc/g++ example flags to get extra includes "./g++.exe --% -E -Wp,-v -xc++ nul"
    // compile_commands.json needs the result of this to add to the command line.
    // else '#include <files> ' will not be found.
    // This is the resulting data were searching within...
    //    #include <...> search starts here:
    //     F:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++
    //     F:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++/x86_64-w64-mingw32
    //     F:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include/c++/backward
    //     F:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include
    //     F:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/include-fixed
    //     F:/usr/MinGW810_64seh/bin/../lib/gcc/x86_64-w64-mingw32/8.1.0/../../../../x86_64-w64-mingw32/include
    //    End of search list.

    size_t compileCommandDBchanged = 0;

    wxString newFullFilePath = argFullFilePath;
    if (platform::windows)
        newFullFilePath.Replace(fileSep, "/");

    ProjectFile* pProjectFile = pProject->GetFileByFilename(newFullFilePath, false);
    if (not pProjectFile) return false;

    wxArrayString buildTargets = pProjectFile->GetBuildTargets();
    if (not buildTargets.Count()) return false;

    //-ProjectBuildTarget* pTarget = pProject->GetBuildTarget(buildTargets[0]);
    if (not pTarget) return false;

    wxArrayString compileCommands;

    // Clangd wants source files, not header files
    if (ParserCommon::FileType(pProjectFile->relativeFilename) == ParserCommon::ftHeader)
    {
        // find the matching source file to this header file
        wxFileName fnFilename(pProjectFile->file);
        for (FilesList::const_iterator flist_it = pTarget->GetFilesList().begin(); flist_it != pTarget->GetFilesList().end(); ++flist_it)
        {

            ProjectFile* pf = *flist_it;
            if ( pf and (pf->file.GetName() == fnFilename.GetName()) )
            {
                if (ParserCommon::FileType(pf->relativeFilename) == ParserCommon::ftHeader) continue;
                if ((ParserCommon::FileType(pf->relativeFilename) == ParserCommon::ftSource)
                  or (FileTypeOf(pf->relativeFilename) == ftTemplateSource) )
                {
                        compileCommands = GetCompileFileCommand(pTarget, pf);
                        if (not compileCommands.Count()) continue;
                        wxString workingDir = wxPathOnly(pTarget->GetParentProject()->GetFilename());
                        if (platform::windows) workingDir.Replace("\\", "/");
                        //-compileCommands.Add("-working-directory=" + workingDir);
                        pProjectFile = pf;
                        newFullFilePath = pf->file.GetFullPath(); //use file name instead of header
                        if (platform::windows) newFullFilePath.Replace("\\", "/");
                        //-foundSource = true; break;
                        break;
                }//end if source file
            }//end if found matching base file name
        }//endfor

    }//end if header file
    else
    {
        // Not a header file
        compileCommands = GetCompileFileCommand(pTarget, pProjectFile);
        if (not compileCommands.Count()) return false;
    }

    wxString compileCommand;
    for (size_t ccknt=0; ccknt<compileCommands.Count(); ++ccknt)
    {
        // Some entries start with "SLOG:g++.exe ..." etc
        //-wxString look = compileCommands[ccknt]; debugging
        if ( compileCommands[ccknt].StartsWith("SLOG:") )
            continue;
        compileCommand = compileCommands[ccknt]; // use the first good compile command.
        break;
    }
    if (compileCommand.empty()) return false;

    // Add the extra search directories
    wxString compilerID = pTarget->GetCompilerID();
    wxString compilerIncludes = CompilerDriverIncludesMap[compilerID];
    if (compilerIncludes.Length())
    {
        //-for ( wxString& extraInclude : LSPParseExtraIncludes)
        compileCommand.Append(" " + compilerIncludes.Trim(true).Trim(false) );
    }

    compileCommand.Replace("\\", "/");
    wxString cbWorkingDir = wxPathOnly(pProject->GetFilename()); //(ph 2021/01/15)
    cbWorkingDir.Replace("\\", "/");

    json newEntry;
    //-newEntry["directory"] = cbWorkingDir; //(ollydbg 2022/10/30) ticket #78
    //-newEntry["command"]   = compileCommand;
    //-newEntry["file"]      = newFullFilePath; //(ph 2021/01/15) must be fullpath
    //-newEntry["output"]    = buildTargets[0];
    newEntry["directory"] = GetstdUTF8Str(cbWorkingDir); //(ollydbg 2022/10/30) ticket #78
    newEntry["command"]   = GetstdUTF8Str(compileCommand);
    newEntry["file"]      = GetstdUTF8Str(newFullFilePath); //must be fullpath
    newEntry["output"]    = GetstdUTF8Str(buildTargets[0]);

    // If this file is already in compile_commands.json just leave it.
    // changing it will cause LSP server to reload the src file and compiler_commands.json
    // because clangd watches the mod time on compiler_commands.json
    int entryknt = pJson->size();

    // Remove the older entry if any
    size_t found = 0, changed = 0;
    for (int ii=0; ii<entryknt; ++ii)
    {
        json entry;
        try { entry =  pJson->at(ii); }
        catch(std::exception &err)
        {
            wxString errMsg(wxString::Format("\nAddFileToCompileDBJson() error: %s\n", err.what()) );
            writeClientLog(GetstdUTF8Str(errMsg));
            cbMessageBox(errMsg);
        }
        //wxString look = entry.dump(); //debugging
        std::string ccjDir     = entry["directory"];
        std::string ccjFile    = entry["file"];
        std::string ccjCommand = entry["command"];

        //-wxString ccjPath = ccjDir + filesep + ccjFile ;
        //-if (ccjFile == newFullFilePath)         // make compile_commands file fullpath
        if (ccjFile == GetstdUTF8Str(newFullFilePath) ) // make compile_commands file fullpath //(ollydbg 2022/10/30) ticket #78

        {
            // filename and directory name have matched.
            // If commands match, leave entry alone.
            if (not found)
            {
                found++;    //update first entry only
                if (ccjCommand != newEntry["command"])
                {
                    pJson->at(ii) = newEntry;
                    changed++;
                }
                continue;
            }
                // **debugging** write changes to log
                //writeClientLog(wxString::Format("AddFileToCompilerDBJson File:%s", ccjPath.c_str()) );
                //writeClientLog(wxString::Format("\tOldCommand:%s", ccjCommand.c_str()) );
                //writeClientLog(wxString::Format("\tNewCommand:%s", newEntry["command"].get<std::string>()) );

            //This is a duplicate entry
            // duplicate entries.
            try {   pJson->erase(ii);
            } catch (std::exception &e)
            {
                writeClientLog(StdString_Format("\nAddFileToCompileDBJson() error:%s erase", e.what()) );
                return false;
            }
            changed++; //file has been changed
            --entryknt;
            --ii;
        }//endif
    }//endfor

    // add the new or changed entry
    if ((not found) or changed)
    {
        if (not found)
            pJson->push_back(newEntry);
        //SetCompile_CommandsChanged(true);
        compileCommandDBchanged++;
        #if defined(cbDEBUG)
            //LogManager* pLogMgr = Manager::Get()->GetLogManager();
            CCLogger::Get()->DebugLog(wxString::Format("NewCompileCommand:%s", newEntry["command"].get<std::string>()) );
        #endif
    }

    //wxString look = pJson->dump(); debugging
    if (compileCommandDBchanged)
        return true;
    return false;

}//end AddFileToCompileDBJson()
// ----------------------------------------------------------------------------
void ProcessLanguageClient::UpdateCompilationDatabase(cbProject* pProject, wxString filename)
// ----------------------------------------------------------------------------
{
    if (pProject == m_pParser->GetParseManager()->GetProxyProject() )
    {
        // Don't update the compile_commands.json database if this is the
        // Proxy cbProject containing non-project files.
        // clangd will attempt to find another translation unit that somewhat
        // matches this projects' unassociated file and use its compile parameters.
        // We do this because clangd starts searching for the database within the directory
        // containing the file, then searching backup the tree.
        // There's likely no such database there; just wasting time.
        return;
    }

    // create compilation database 'compile_commands.json' from project files.
    // If compile_commands.json already exists, use it to insert project file data.

    // compile_commands.json format: https://clang.llvm.org/docs/JSONCompilationDatabase.html
    // [
    //    { "directory": "/home/user/llvm/build",
    //      "command": "/usr/bin/clang++ -Irelative -DSOMEDEF=\"With spaces, quotes and \\-es.\" -c -o file.o file.cc",
    //      "file": "file.cc" ,
    //      "output": "targetName"
    //    },
    //    ...
    // ]

    // Add an active project owned file and it's compiling info to the clangd compile_database.json file.
    // If the file does not belong to a project, add it to proxy project
    // compile_commands.json file so Clangd can find  and parse it.

    std::fstream jsonFile;           //("out.json", std::ofstream::in | std::ofstream::out);
    json jdb = json::array();        //create the main array

    wxString editorProjectDotCBP = pProject->GetFilename();
    wxString compileCommandsFullPath = wxPathOnly(editorProjectDotCBP) + "\\compile_commands.json";
    if (not platform::windows) compileCommandsFullPath.Replace("\\","/");
    if (wxFileExists(compileCommandsFullPath)) switch(1)
    {
        default:
        jsonFile.open (compileCommandsFullPath.ToStdString(), std::ofstream::in);
        if (not jsonFile.is_open())
        {
            wxString msg = wxString::Format("Existing compile_commands.json file failed to open.\n%s", editorProjectDotCBP);
            cbMessageBox(msg);
            break;
        }
        jsonFile >> jdb; //read file json object
        jsonFile.close();
    }//endif wxFileExists

    ProjectFile* pProjectFile = pProject->GetFileByFilename(filename, false);
    if (not pProjectFile) return;

    // create array of compiler built-in include files needed for for clang '-fsyntax-only'
    // The files arn't found by clang for some unknown reason to me.
    wxArrayString gccResults, gccErrors;

    ProjectBuildTarget* pTarget = nullptr;
    if (not pTarget)
    {
        // Find the active target by matching what the ProjectFile says vs what the cbProject says.
        // If the target is a virtual target like "All", cbProject returns a nullptr.
        // If the user has deleted a project, ProjectFile may still return it as valid.
        // So get the actual target by making ProjectFile and cbProject agree with one another.
        wxArrayString pfBuildTargets = pProjectFile->GetBuildTargets();
        if (not pfBuildTargets.Count()) return;
        int prjTargetsCount = pProject->GetBuildTargetsCount();
        for (size_t pfbtx=0; pfbtx < pfBuildTargets.GetCount(); ++pfbtx)
        {
            // Match the list from ProjectFile targets to the Project targets
            // to find the non-virtual, active compile target
            if (pProject->BuildTargetValid(pfBuildTargets[pfbtx], false))
            {
                wxString pfTargetTitle = pfBuildTargets[pfbtx];
                for (int prjbtx=0; prjbtx < prjTargetsCount; ++prjbtx)
                {
                    wxString prjTargetTitle = pProject->GetBuildTarget(prjbtx)->GetTitle();
                    if ( pfTargetTitle == prjTargetTitle )
                    {
                        pTarget = pProject->GetBuildTarget(prjbtx);
                        break;
                    }
                }
                if (pTarget) break;
            }//endfor prjBuildTargets
        }//endfor pfBuildTargets
    }
    if (not pTarget) return;

    Compiler* pCompiler = CompilerFactory::GetCompiler(pTarget->GetCompilerID());
    wxString masterPath = pCompiler ? pCompiler->GetMasterPath() : "";
    wxString compilerID = pCompiler ? pCompiler->GetID() : "";
    CompilerPrograms compilerPrograms;

    wxArrayString aCompilerDriverIncludeFiles;
    GetCompilerDriverIncludesByFile(aCompilerDriverIncludeFiles, pProject, filename);

    // Add the file that should be represented to LSP into the json object array
    int fileCount = 0;
    if ( pProjectFile and (pProjectFile->file.GetFullPath() == filename) )
    {
        ParserCommon::EFileType eft = ParserCommon::FileType(pProjectFile->relativeFilename);
        if (   (eft == ParserCommon::ftHeader) or (eft == ParserCommon::ftSource)
            or (FileTypeOf(pProjectFile->relativeFilename) == ftTemplateSource) )
        {
            if ( AddFileToCompileDBJson(pProject, pTarget,  pProjectFile->file.GetFullPath(), &jdb) )
                 ++fileCount;
        }
    }

    if (fileCount)
    {
        jsonFile.open (compileCommandsFullPath.ToStdString(), std::ofstream::out | std::ofstream::trunc);
        if (not jsonFile.is_open())
        {
            wxString msg(wxString::Format("compile_commands.json file %s\nfailed to open for output.\n", compileCommandsFullPath) );
            cbMessageBox(msg);
        }
        else
        {
            // update compile_commands.json file
            jsonFile << jdb; //write file json object
            jsonFile.close();

            // This code un-needed after clangd version 12
////        // updates before LSP is initialized should not set the LSP restart timer.
////        // File opens after initialization have compile_commands already set
////        // so filecount will be zero.
////        // Ergo, the restart timer is set only when a new file is opened that
////        // was not previously added to compile_commands.json
////            if (GetLSP_Initialized())
////                SetCompileCommandsChangedTime(true);
        }
    }

}//end UpdateCompilationDatabase()
// ----------------------------------------------------------------------------
int ProcessLanguageClient::GetCompilationDatabaseEntry(wxArrayString& resultArray, cbProject* pProject, wxString filename)
// ----------------------------------------------------------------------------
{
    // Get an entry from the compilation database (compile_commands.json)

    wxFileName fn_ccdbj(pProject->GetFilename());
    fn_ccdbj.SetFullName("compile_commands.json");

    wxString unixFilename = filename;
    if (platform::windows)
        unixFilename.Replace("\\", "/");

    nlohmann::json jArray;
    std::ifstream jsonFile(fn_ccdbj.GetFullPath().ToStdString());
    try {
        jsonFile >> jArray;
        jsonFile.close();
    } catch (std::exception &e)
    {
        wxString msg = "GetCompilationDatabaseEntry(): error reading " + fn_ccdbj.GetFullPath();
        CCLogger::Get()->DebugLog(msg);
        cbMessageBox(msg);
        return 0;
    }

    int knt = jArray.size();
    if (not knt) return 0;

    wxString jCommand;
    wxString jDirectory;
    wxString jFile;
    bool found = false;
    try {
        for (int ii=0; ii< knt; ++ii)
        {
            json jentry = jArray.at(ii);
            jCommand   = GetwxUTF8Str(jentry.at("command").get<std::string>());
            jDirectory = GetwxUTF8Str(jentry.at("directory").get<std::string>());
            jFile      = GetwxUTF8Str(jentry.at("file").get<std::string>());
            if (jFile == unixFilename)
            {
                found = true;
                break;
            }
        }//endfor
    }//endtry
    catch (std::exception &e) {
        wxString msg = wxString::Format("GetCompilationDatabaseEntry():parse error %s", e.what());
        cbMessageBox(msg);
        return 0;
    }//endcatch

    if (not found) return 0;
    resultArray = GetArrayFromString(jCommand, " -", /*trimSpaces*/true);
    // put back the "-"s we just removed except for the first item (exec's name).
    for (size_t ii=1; ii<resultArray.GetCount(); ++ii)
        resultArray[ii].Prepend("-");

    return resultArray.Count();
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::CreateDiagnosticsLog()
// ----------------------------------------------------------------------------
{
    // If there is a LSP messages log already, use it.
    int logIndex = GetLogIndex("LSP messages");
    if (logIndex)
    {
        LogManager* pLogMgr = Manager::Get()->GetLogManager();
        LogSlot& logslot = pLogMgr->Slot(logIndex);
        ListCtrlLogger* pLogger = (ListCtrlLogger*)logslot.GetLogger();
        // pLogger->Clear();
        if (pLogger)
        {
            m_pDiagnosticsLog = (LSPDiagnosticsResultsLog*)pLogger;
            m_pDiagnosticsLog->Clear();
        }
        return;
    }

    if (not m_pDiagnosticsLog ) //(ph 2021/01/8)
    {

        wxArrayInt widths;
        wxArrayString titles;
        titles.Add(_("File"));
        titles.Add(_("Line"));
        titles.Add(_("Text"));
        widths.Add(128);
        widths.Add(48);
        widths.Add(640);

        const int uiSize = Manager::Get()->GetImageSize(Manager::UIComponent::InfoPaneNotebooks);
        wxString prefix(ConfigManager::GetDataFolder()+"/resources.zip#zip:/images/");

        #if wxCHECK_VERSION(3, 1, 6)
         const double uiScaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::InfoPaneNotebooks);
         wxBitmapBundle* logbmp = new wxBitmapBundle(cbLoadBitmapBundle(prefix, "filefind.png", wxRound(uiSize/uiScaleFactor), wxBITMAP_TYPE_PNG));
        #else
         prefix << wxString::Format("%dx%d/", uiSize, uiSize);
         wxBitmap* logbmp = new wxBitmap(cbLoadBitmap(prefix+"filefind.png", wxBITMAP_TYPE_PNG));
        #endif

        m_pDiagnosticsLog = new LSPDiagnosticsResultsLog(titles, widths, GetLSP_IgnoredDiagnostics());
        CodeBlocksLogEvent evt(cbEVT_ADD_LOG_WINDOW, m_pDiagnosticsLog, _("LSP messages"), logbmp);
        Manager::Get()->ProcessEvent(evt);

        // Ask DragScroll plugin to apply its support for this log
        wxWindow* pWindow = m_pDiagnosticsLog->m_pControl;
        cbPlugin* pPlgn = Manager::Get()->GetPluginManager()->FindPluginByName(_T("cbDragScroll"));
        if (pWindow && pPlgn)
        {
            wxCommandEvent dsEvt(wxEVT_COMMAND_MENU_SELECTED, XRCID("idDragScrollAddWindow"));
            dsEvt.SetEventObject(pWindow);
            pPlgn->ProcessEvent(dsEvt);
        }
    }

}//end createLog
// ----------------------------------------------------------------------------
int ProcessLanguageClient::GetLogIndex (const wxString& logRequest)
// ----------------------------------------------------------------------------
{
    // Usage:
    //    m_cbSearchResultsLogIndex = GetLogIndex (_T("Search results") );

    //    LogManager* pLogMgr = Manager::Get()->GetLogManager();
    //    LogSlot& logslot = pLogMgr->Slot(m_cbSearchResultsLogIndex);
    //    ListCtrlLogger* pLogger = (ListCtrlLogger*)logslot.GetLogger();
    //    pLogger->Clear();



    int nNumLogs = 16; //just a guess
    int lm_BuildLogIndex = 0;
    int lm_BuildMsgLogIndex = 0;
    int lm_DebuggerLogIndex = 0;
    int lm_DebuggersDebugLogIndex = 0;
    int lm_CodeBlocksDebugLogIndex = 0;
    int lm_CodeBlocksSearchResultsLogIndex = 0;
    int lm_CodeBlocksLSPMessagesLogIndex = 0;
    LogManager* pLogMgr = Manager::Get()->GetLogManager();

    for (int i = 0; i < nNumLogs; ++i)
    {
        LogSlot& logSlot = pLogMgr->Slot (i);
        if (pLogMgr->FindIndex (logSlot.log) == pLogMgr->invalid_log) continue;
        {
            if ( logSlot.title.IsSameAs (wxT ("Build log") ) )
                lm_BuildLogIndex = i;
            if ( logSlot.title.IsSameAs (wxT ("Build messages") ) )
                lm_BuildMsgLogIndex = i;
            if ( logSlot.title.IsSameAs (wxT ("Debugger") ) )
                lm_DebuggerLogIndex = i;
            if ( logSlot.title.IsSameAs (wxT ("Debugger (debug)") ) )
                lm_DebuggersDebugLogIndex = i;
            if ( logSlot.title.IsSameAs (wxT ("Code::Blocks Debug") ) )         //(ICC 2016/05/4)
                lm_CodeBlocksDebugLogIndex = i;
            if ( logSlot.title.IsSameAs (wxT ("Search results") ) )             //(pecan 2018/04/6)
                lm_CodeBlocksSearchResultsLogIndex = i;
            if ( logSlot.title.IsSameAs (wxT ("LSP messages") ) )             //(ph 2021/01/14)
                lm_CodeBlocksLSPMessagesLogIndex = i;
        }
    }//for

    if (logRequest == _ ("Build log") )
        return lm_BuildLogIndex;
    else if (logRequest == _ ("Build messages") )
        return lm_BuildMsgLogIndex;
    else if (logRequest == _ ("Debugger") )
        return lm_DebuggerLogIndex;
    else if (logRequest == _ ("Debugger (debug)") )
        return lm_DebuggersDebugLogIndex;
    else if (logRequest == _ ("Code::Blocks Debug") )
        return lm_CodeBlocksDebugLogIndex;
    else if (logRequest == _ ("Search results") )
        return lm_CodeBlocksSearchResultsLogIndex;
    else if (logRequest == _ ("LSP messages") )
        return lm_CodeBlocksLSPMessagesLogIndex;
    return 0;
}
// ----------------------------------------------------------------------------
wxString ProcessLanguageClient::CreateLSPClientLogName(int pid, const cbProject* pProject)
// ----------------------------------------------------------------------------
{
    // Create or find an appropriate client/server log filename for this project
    wxString resultName = wxEmptyString;
    wxString tempDir = wxFileName::GetTempDir();
    wxString fileSep = wxFILE_SEP_PATH;
    wxString clientLogIndexesFilename = tempDir + fileSep + "CBclangd_LogsIndex.txt";
    wxTextFile clientLogIndexesFile;
    if ( not wxFileExists(clientLogIndexesFilename) )
        clientLogIndexesFile.Create(clientLogIndexesFilename);
    if (not clientLogIndexesFile.IsOpened())
        clientLogIndexesFile.Open(clientLogIndexesFilename);

    if (not clientLogIndexesFile.IsOpened())
    {
        wxString msg = "Error: CreateLSPClientLogName() could not open " + clientLogIndexesFilename;
        writeClientLog(msg.ToStdString());
        cbMessageBox(msg);
        return wxEmptyString;
    }
    size_t logLineCount = clientLogIndexesFile.GetLineCount();
    // line consists of pid;dateTime;exemodulePath;cbpFileName;
    wxString pidStr = std::to_string(pid);
    //tempDir + fileSep + CBclangd_client-####.log
    int slot = wxNOT_FOUND;
    wxString lineTxt;
    size_t oldPidNum = 0;
    //Find a slot usable for the current project log
    for (size_t ii=0; ii<logLineCount; ++ii)
    {
        lineTxt = clientLogIndexesFile.GetLine(ii);
        wxString linePid = lineTxt.BeforeFirst(';');
        if (pidStr == linePid)  {slot = ii; break;} //its us
        std::string::size_type sz;   // alias of size_t
        oldPidNum = std::stol(linePid.ToStdString(),&sz);
        // Is this pid still alive ?
        wxString oldPidExePath = ProcUtils::GetProcessNameByPid(oldPidNum);
        if (not oldPidExePath) {slot = ii; break;} //old process is dead, slot is available
        oldPidNum = 0;
    }

    wxDateTime now = wxDateTime::Now();
    wxString nowTime = now.Format("%H:%M:%S", wxDateTime::Local);
    wxString nowDate = now.FormatDate();
    wxString itemsep = ";";
    wxString logLine = pidStr + itemsep + nowDate+"_"+nowTime + itemsep + ProcUtils::GetProcessNameByPid(pid)
                        + itemsep + pProject->GetFilename();

    if (slot == wxNOT_FOUND)
    {
        clientLogIndexesFile.AddLine(logLine);
        //tempdir + fileSep + CBclangd_client-####.log
        lineTxt = tempDir + fileSep  + "CBclangd_client-" + pidStr + ".log";
    }
    else //found a usable slot
    {
        // Delete the old log associated with this slot.
        if (oldPidNum)
        {
            wxString logFilename = tempDir + fileSep  + "CBclangd_client-" + std::to_string(oldPidNum) + ".log";
            // For some unknown reason, windows is not allowing these closed files to be deleted.
            // Probably because of Windows delayed deletions.
            // The PID is gone but no access to the file is allowed.
            // Access and delete is allowed after the project is closed.
            // The logs are now removed in void CodeCompletion::CleanUpLSPLogs() after
            // the project is closed.
            if (wxFileExists(logFilename))
                ;//-wxRemoveFile(logFilename);
            logFilename.Replace("client", "server");
            if (wxFileExists(logFilename))
                ;//-wxRemoveFile(logFilename);
        }
        clientLogIndexesFile[slot] = logLine;
        lineTxt = tempDir + fileSep + "CBclangd_client-" + pidStr + ".log";
    }

    clientLogIndexesFile.Write();
    clientLogIndexesFile.Close();
    return lineTxt;
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::ListenForSavedFileMethod()
// ----------------------------------------------------------------------------
{
    wxFrame* pAppFrame = Manager::Get()->GetAppFrame();
    int saveFileID = wxFindMenuItemId(pAppFrame, "File", "Save file");
    int saveEverythingID = wxFindMenuItemId(pAppFrame, "File", "Save everything");

    Bind(wxEVT_COMMAND_MENU_SELECTED, &ProcessLanguageClient::SetSaveFileEventOccured, this, saveFileID);
    Bind(wxEVT_COMMAND_MENU_SELECTED, &ProcessLanguageClient::SetSaveFileEventOccured, this, saveEverythingID);
}
// ----------------------------------------------------------------------------
void ProcessLanguageClient::SetSaveFileEventOccured(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    // Set a flag to show that the user did a save or saveEverything from the menu system.
    event.Skip();
    an_SavedFileMethod = event.GetId();
}
// ----------------------------------------------------------------------------
bool ProcessLanguageClient::GetSaveFileEventOccured()
// ----------------------------------------------------------------------------
{
    // This routine called to avoid switchng the log focus unless user did a save.
    // If so, the focus can switch to the LSP messages log when the "textDocument/Diagnostics"
    // LSP response event occurs.
    bool isSaveButton = false;
    wxFrame* pAppFrame = Manager::Get()->GetAppFrame();
    int saveFileID = wxFindMenuItemId(pAppFrame, "File", "Save file");
    int saveEverythingID = wxFindMenuItemId(pAppFrame, "File", "Save everything");

    if ((an_SavedFileMethod == saveFileID) or (an_SavedFileMethod == saveEverythingID))
        isSaveButton =true;

    an_SavedFileMethod = 0;

    return isSaveButton;
}
