#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <compilerfactory.h>
#include <projectmanager.h>
#include <cbproject.h>
#include <wx/dir.h>

#include "dbgGDBmi.h"
#include "definitions.h"
#include "editormanager.h"
#include "cbeditor.h"
#include "cbstyledtextctrl.h"
#include "logmanager.h"
#include "compiler.h" // (ph 26/06/23) for linux

// ----------------------------------------------------------------------------
namespace dbg_mi
// ----------------------------------------------------------------------------
{
int ParseParentPID(const char *line)
{
    const char *p = strchr(line, '(');
    if (not p)
        return -1;
    ++p;
    int open_paren_count = 1;
    while (*p && open_paren_count > 0)
    {
        switch (*p)
        {
        case '(':
            open_paren_count++;
            break;
        case ')':
            open_paren_count--;
            break;
        }

        ++p;
    }
    if (*p == ' ')
        ++p;
    int dummy;
    int ppid;
    int count = sscanf(p, "%c %d", (char *) &dummy, &ppid);
    return count == 2 ? ppid : -1;
}

// ----------------------------------------------------------------------------
// Hack to allow heap allocated Actions to use cbMessageBox.
// A call to xxMessageBox of any kind from the Actions class frees memory too early.
// ----------------------------------------------------------------------------

wxArrayString g_RunActionMsgsQ;
bool          g_RunActionMsgsQBusy = false;

// ----------------------------------------------------------------------------
void SetRunActionMsg(wxString message, wxString title, ERR_MSGTYPE_RESPONSE msgtype)
// ----------------------------------------------------------------------------
{
    while(g_RunActionMsgsQBusy)
    {
        wxMilliSleep(250);
    }

    g_RunActionMsgsQBusy = true;

    wxString sep = _T(";");

    wxString msgTypeString = _T("None");
    switch (msgtype)
    {
        case errNone:
            msgTypeString = _T("None"); break;
        case errInfo:
            msgTypeString = _T("Info"); break;
        case errCritical:
            msgTypeString = _T("Critical"); break;
        case errAbort:
            msgTypeString = _T("Abort"); break;
    }
    wxString qMessage = msgTypeString +sep+ title +sep+ message;
    g_RunActionMsgsQ.Add(qMessage);

    g_RunActionMsgsQBusy = false;

    return;
}

// ----------------------------------------------------------------------------
size_t  IsRunActionMsgQueued()
// ----------------------------------------------------------------------------
{
    return g_RunActionMsgsQ.GetCount();
}
// ----------------------------------------------------------------------------
bool GetRunActionMsg(wxString& msgtype, wxString& msgTitle, wxString& msgText)
// ----------------------------------------------------------------------------
{
    while (g_RunActionMsgsQBusy)
    {
        wxMilliSleep(250);
    }

    g_RunActionMsgsQBusy = true;

    msgtype = msgTitle = msgText = wxEmptyString;

    if (IsRunActionMsgQueued()) do
    {
        wxChar sep = _T(';');
        wxString msg = g_RunActionMsgsQ[0];

        msgtype = msg.BeforeFirst(sep);
        msg = msg.AfterFirst(sep);

        msgTitle = msg.BeforeFirst(sep);
        msg = msg.AfterFirst(sep);

        msgText = msg;

        g_RunActionMsgsQ.RemoveAt(0);

    }while(0);

     g_RunActionMsgsQBusy = false;
     return (msgtype != wxEmptyString);
}
// ----------------------------------------------------------------------------
wxString UnquoteString(const wxString& str)
// ----------------------------------------------------------------------------
{
    wxString s = str;
    if (not str.IsEmpty() && str.GetChar(0) == _T('"') && str.Last() == _T('"'))
        s = str.Mid(1, str.Length() - 2);
    if (not str.IsEmpty() && str.GetChar(0) == _T('\'') && str.Last() == _T('\''))
        s = str.Mid(1, str.Length() - 2);
    return s;
}
// ----------------------------------------------------------------------------
Compiler* GetActiveCompiler(const cbProject* inpProject, ProjectBuildTarget* inpTarget)
// ----------------------------------------------------------------------------
{
    // select the build target to debug
    cbProject* pProject = (cbProject*)inpProject;
    ProjectBuildTarget* pTarget = inpTarget;

    // fetch the default compiler (just in case)
    Compiler* compiler = CompilerFactory::GetDefaultCompiler();

    if (not pProject)
    {
        pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (not pProject)
            return compiler; //returns default compiler
    }
    if (not pProject->GetCompilerID().empty())
        compiler = CompilerFactory::GetCompiler(pProject->GetCompilerID());

    if (not pTarget)
        pTarget = pProject->GetBuildTarget( pProject->GetActiveBuildTarget() );
    if (pTarget)
        compiler = CompilerFactory::GetCompiler(pTarget->GetCompilerID());


    // init target;
    wxString active_build_target = pProject->GetActiveBuildTarget();

    //Now try to get the target's compiler
    if(not active_build_target.empty())
    {
        if (not pProject->BuildTargetValid(active_build_target, false))
        {
            int tgtIdx = pProject->SelectTarget();
            if (tgtIdx == -1)
            {
                return compiler;
            }
            pTarget = pProject->GetBuildTarget(tgtIdx);
            active_build_target = pTarget->GetTitle();
        }
        else
            pTarget = pProject->GetBuildTarget(active_build_target);

        // make sure it's not a commands-only target
        if (pTarget->GetTargetType() == ttCommandsOnly)
        {
            cbMessageBox(_("The selected target is only running pre/post build step commands\n"
                        "Can't debug such a target..."), _("Debugger:Information"), wxICON_INFORMATION, Manager::Get()->GetAppWindow());
            return compiler;
        }

        // find the target's compiler (to see which debugger to use)
        compiler = CompilerFactory::GetCompiler(pTarget ? pTarget->GetCompilerID() : pProject->GetCompilerID());
    }
    else
        if (not compiler)
            compiler = CompilerFactory::GetDefaultCompiler();

    //-if (pTarget)
    //-    *pTarget = target;

    return compiler;

}//GetActiveCompiler
// ----------------------------------------------------------------------------
wxString GetActiveCompilerID(const cbProject* pProject, ProjectBuildTarget* pTarget)
// ----------------------------------------------------------------------------
{
    Compiler* compiler = 0;
    if ((compiler = GetActiveCompiler(pProject, pTarget)))
    {
        return compiler->GetID();
    }
    return wxEmptyString;
}
// ----------------------------------------------------------------------------
wxString GetMasterPath(ProjectBuildTarget* inpTarget)
// ----------------------------------------------------------------------------
{
    ProjectBuildTarget* pTarget = inpTarget;
    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (not pTarget)
        pTarget = pProject->GetBuildTarget( pProject->GetActiveBuildTarget() );

    Compiler* compiler = GetActiveCompiler(pProject,pTarget);
    if (not compiler)
        compiler = CompilerFactory::GetDefaultCompiler();

    wxString compilerID = compiler->GetID();
    if (pTarget) compilerID = pTarget->GetCompilerID();


    wxString masterpath = wxEmptyString;
    if (not compiler) return masterpath;

    masterpath =  compiler->GetMasterPath().Lower();
    if (masterpath.EndsWith(_T("/")) or masterpath.EndsWith(_T("\\")))
        masterpath = masterpath.Truncate(masterpath.Length()-1);
    #if defined(LOGGING)
    //LOGIT( _T("[%s]"), masterpath.c_str());
    #endif
    return masterpath;
}
// ----------------------------------------------------------------------------
void AddCompilerPrettyPrintCommands(wxString& gdbCmdStr, ProjectBuildTarget* pTarget)
// ----------------------------------------------------------------------------
{
    wxString cmd = gdbCmdStr;

    // Get the compiler masterPath  in order to find its pretty printer support
    // Find the compiler pretty printer gdb commands and append to the gdb cmd
    wxString masterPath = dbg_mi::GetMasterPath(pTarget);
    wxString pythonFolder = "python/libstdcxx/v*";
    wxString pyFolder = dbg_mi::FindSpecificFolder(masterPath, pythonFolder);
    if ((not pyFolder.Length()) or (not wxFileExists(pyFolder + "/printers.py")) )
        return ;

    // Normalize all backslashes to forward slashes
    pyFolder.Replace("\\", "/");
    wxString pythonPath = pyFolder;
    // Find the position of "/libstdcxx"
    int pos = pythonPath.Find("/libstdcxx");
    if (pos == wxNOT_FOUND)
        return ;

    // Slice the string right before the slash, or include it:
    // Use 'pos' to exclude the slash (ends at .../python)
    // Use 'pos + 1' to keep the trailing slash (ends at .../python/)
    pythonPath = pythonPath.Left(pos);

    wxString importPath = pyFolder.Mid(pos+1) + ".printers"; // libstdcxx/v6
    importPath.Replace("/",".");    // libstdcxx.v6
    // append the necessary pretty printer command to the gdb cmd string
    // Example:
    //    set print pretty on;
    //    set auto-load safe-path F:/usr/Proj/;
    //    source F:/usr/Proj/wxWidgets332/misc/gdb/print.py;
    //
    //    python import sys;
    //    python sys.path.insert(0, 'F:/usr/Programs/msys64_20.1.3/mingw64/share/gcc-15.2.0/python');
    //    python from libstdcxx.v6.printers import register_libstdcxx_printers;
    //    python register_libstdcxx_printers(None);
    if (cmd.Length() and (cmd.Last() != ';') ) cmd.Append(';');
    cmd += "python import sys;";
    cmd += "python sys.path.insert(0, " + pythonPath +";";
    cmd += "python from " + importPath + " import register_libstdcxx_printers;";
    cmd += "python register_libstdcxx_printers(None);";

    // **Debugging**
    wxString dbgCmd = cmd; dbgCmd.Replace(";","\n");
    gdbCmdStr += cmd;
    //-cbMessageBox(gdbCmdStr,"Debugging");
    return ;
}
// ----------------------------------------------------------------------------
wxString FindSpecificFolder(wxString searchPath, wxString targetPattern)   // (ph 26/06/17)
// ----------------------------------------------------------------------------
{
    //wxString compilerPath = "F:\\usr\\Programs\msys64_20.1.3\\mingw64";
    //wxString targetPattern = "python/libstdcxx.v*";

    wxDir dir(searchPath);

    if (!dir.IsOpened())
    {
        // Handle error: directory doesn't exist or permissions failed
        return wxString();
    }

    wxArrayString matchedFolders;
    FolderFinderTraverser traverser(targetPattern, matchedFolders);

    // Run the recursion. Use wxDIR_DIRS to skip indexing raw files for speed.
    dir.Traverse(traverser, wxEmptyString, wxDIR_DIRS);

    // Evaluate your results
    if (not matchedFolders.IsEmpty())
    {
        for (const auto& path : matchedFolders)
        {
            wxUnusedVar(path);
            //wxLogMessage("Found target directory at: %s", path);
            return matchedFolders[0];
        }
    }
    else
    {
        wxLogMessage("FindSpecificFolder(): Folder structure %s not found.", targetPattern);
    }

    return wxString();
}
//-static int lastDebugLine = 0;
// ----------------------------------------------------------------------------
bool SetDebugMark(const bool show)
// ----------------------------------------------------------------------------
{
    // causing spurious debug markers when setting brkpts in secondary editors
    //TODO: remove any dependency on SetDebugMark() & use SyncEditor() instead.
    wxUnusedVar(show);
    return true;

////    // show debugger busy/stopped state via line debug marker
////
////    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
////    if (not ed) return false;
////    cbStyledTextCtrl* control = ed->GetControl();
////    if (not control) return false;
////    long edlinenum = control->GetCurrentLine();
////
////    if ( show )
////    {
////        if (ed->GetDebugLine() > -1) return true;       //already shown
////        ed->SetDebugLine(lastDebugLine);
////        lastDebugLine = edlinenum;
////    }
////    else //turn off debug line indicator
////    {
////        if (-1 == ed->GetDebugLine()) return false; //already off
////        dbg_mi::lastDebugLine = ed->GetDebugLine();
////        ed->SetDebugLine(-1);
////    }
////    return true;
}
// ----------------------------------------------------------------------------
bool IsActionsMapEmpty()
// ----------------------------------------------------------------------------
{
    cbDebuggerPlugin *dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    return ((Debugger_GDB_MI*)dbg)->IsActionsMapEmpty();
}
#if defined(__WIN32__)
// ----------------------------------------------------------------------------
bool IsProcessRunning(long pid)
// ----------------------------------------------------------------------------
{
    // verify that parent CodeBlocks is still running.
    if (pid)
    {
        //bool itsThere = wxProcess::Exists(m_cbPid);
        //wxProcess::Esists() is returning true even when the task doesn't exist

        //BOOL IsProcessRunning(DWORD pid)
        //{
            HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
            DWORD ret = WaitForSingleObject(process, 0);
            CloseHandle(process);
            //return ret == WAIT_TIMEOUT;
            bool isAlive = (ret == WAIT_TIMEOUT);
        //}
        return isAlive;
    }
    return false;
}
#else
#include <sys/types.h>
#include <signal.h>
// ----------------------------------------------------------------------------
bool IsProcessRunning(long pid)
// ----------------------------------------------------------------------------
{
    if (pid > 0) {
        if (kill(pid, 0) == 0)
        {
            // Process exists
            return true;
        } else
        {
            // Process doesn't exist or we don't have permission to send a signal
            return false;
        }
    }
    return false;
}

#endif
// ----------------------------------------------------------------------------
long AnyToLong(const wxString& inString)
// ----------------------------------------------------------------------------
{
    // Convert any string to a long value

    wxString str = inString;
    signed long nLongValue;

    // char ?
    if (not str.IsEmpty() && str.GetChar(0) == _T('\'') && str.Last() == _T('\''))
    {
        str = UnquoteString(inString);
        str.Printf(_T("%d"), str.GetChar(0));
        str.ToLong(&nLongValue,0);
        return nLongValue;
    }

    // hex

    if ( (str.Length() > 2) and str.Lower().StartsWith(_T("0x"))
        and  (str.GetChar(2) > _T('8')) )
    {   // negative hex needs special attention
        // wxString hexString = str.Lower(); // **Debugging**
        nLongValue = 0;
        str.ToLong(&nLongValue,0);
        long mask = 0;
        for (unsigned ii=0; ii<(str.Length()-2); ++ii)
            mask = (mask << 4) + 0x0f;
        nLongValue = (nLongValue xor mask) +1;
        return nLongValue * -1;
    }
    if (str.Lower().StartsWith(_T("0x")))
    {
        str.ToLong(&nLongValue,0);
        return nLongValue;
    }

    // binary
    //remove prepended 0b from binary forms;
    if (str.Lower().StartsWith(_T("0b")))
    {   str = str.Mid(2);
        str.ToLong(&nLongValue,2);
        return nLongValue;
    }


    // unsigned and signed int
    str.ToLong(&nLongValue,0);
    return nLongValue;
}
// ----------------------------------------------------------------------------
wxString AnyToIntStr(wxString& inString)
// ----------------------------------------------------------------------------
{
    // Convert any string to an Int String

    signed long nLongValue;

    nLongValue = AnyToLong(inString);
    return wxString::Format(_T("%d"), nLongValue);
}
// ----------------------------------------------------------------------------
wxString AnyToUIntStr(wxString& inString)
// ----------------------------------------------------------------------------
{
    // Convert any string to an unsigned Int String
    signed long nLongValue;

    nLongValue = AnyToLong(inString);
    if (nLongValue < 0)
        inString.ToLong(&nLongValue,0);
    return wxString::Format(_T("%lu"), nLongValue);
}
// ----------------------------------------------------------------------------
wxString IntToCharStr(wxString& inString)
// ----------------------------------------------------------------------------
{
    // Convert int to an char String
    long nLongValue = AnyToLong(inString);
    if (nLongValue < 0)
        inString.ToLong(&nLongValue,0);

    return wxString::Format(_T("%1u \'%c\'"), nLongValue, nLongValue);
}
////// ----------------------------------------------------------------------------
////wxString IntStrToHexStr(wxString& inString)
////// ----------------------------------------------------------------------------
////{
////    // Convert an Integer string to a Hex String
////    long nLongValue = AnyToLong(inString);
////    return wxString::Format(_T("%#0x"), nLongValue);
////}
// ----------------------------------------------------------------------------
wxString IntToHexStr(wxString& inNumber)
// ----------------------------------------------------------------------------
{
    // Convert an Integer string to a Hex String
    long nLongValue = AnyToLong(inNumber);
    return wxString::Format(_T("%#0x"), nLongValue);
}
// ----------------------------------------------------------------------------
wxString UIntToHexStr(wxString& inNumber)
// ----------------------------------------------------------------------------
{
    // Convert an Integer string to a Hex String
    long nLongValue = AnyToLong(inNumber);
    return wxString::Format(_T("%#0x"), nLongValue);
}
////// ----------------------------------------------------------------------------
////wxString LongToHexStr(wxString& inNumber)
////// ----------------------------------------------------------------------------
////{
////    // Convert an Integer string to a Hex String
////    long nLongValue = AnyToLong(inNumber);
////    return wxString::Format(_T("%#0x"), nLongValue);
////}
// ----------------------------------------------------------------------------
wxString StdStrToHexStr(std::string& inString)
// ----------------------------------------------------------------------------
{
    // Convert an Integer std::string to a Hex String
    long nLongValue;
    wxString tmp(inString.c_str(), wxConvUTF8);

    nLongValue = AnyToLong(tmp);
    return wxString::Format(_T("%#0x"), nLongValue);
}
// ----------------------------------------------------------------------------
wxString CharToHexStr(wxString& inNumber)
// ----------------------------------------------------------------------------
{
    // Convert an Integer string to a Hex String
    long nLongValue = AnyToLong(inNumber);

////    wxString str = UnquoteString(inNumber);
////    str.Printf(_T("%d"), str.GetChar(0));
////    str.ToLong(&nLongValue,0);
////
////    if (str.Lower().StartsWith(_T("0b")))
////        str.Mid(2).ToLong(&nLongValue,2);

    return wxString::Format(_T("%#0x"), nLongValue);
}
// ----------------------------------------------------------------------------
wxString wxStringToHexStr(wxString& str)
// ----------------------------------------------------------------------------
{
    wxString strout;
    char buf[3] = {'\0'};
    for (size_t ii=0; ii<str.Length(); ++ii)
    {
        TextDecToHex(str[ii], buf);
        strout += buf[0];
        strout += buf[1];
        strout += _T(' ');
    }
    return strout;

}
// ----------------------------------------------------------------------------
void TextDecToHex(int dec, char* buf) //support function
// ----------------------------------------------------------------------------
{
    // Convert decimal integer to 2-character hex string

    // Array used in DecToHex conversion routine.
    static char hexArray[] = "0123456789ABCDEF";

    //int firstDigit = (int)(dec/16.0);
    int firstDigit = (int)(dec>>4);
    //int secondDigit = (int)(dec - (firstDigit*16.0));
    int secondDigit = (int)(dec & 0x0f);
    buf[0] = hexArray[firstDigit];
    buf[1] = hexArray[secondDigit];
}
////// ----------------------------------------------------------------------------
////wxString HexStrToIntStr(wxString& inString)
////// ----------------------------------------------------------------------------
////{
////    // Convert aHex string to a Integer
////    long nLongValue = AnyToLong(inString);
////////    inString.ToLong(&nLongValue,0);
////////    if (inString.Lower().StartsWith(_T("0b")))
////////        inString.Mid(2).ToLong(&nLongValue,2);
////
////    return wxString::Format(_T("%d"), nLongValue);
////}
////// ----------------------------------------------------------------------------
////wxString CharToIntStr(const wxChar& inChar)
////// ----------------------------------------------------------------------------
////{
////    // Convert char string to an Integer
////    return wxString::Format(_T("%d"), inChar);
////}
// ----------------------------------------------------------------------------
wxString UIntToBinStr(const wxString& inStr)
// ----------------------------------------------------------------------------
{
    // Convert unsigned int to binary string
    long nLongValue = AnyToLong(inStr);
    if (nLongValue < 0)
        inStr.ToLong(&nLongValue,0);
    std::string stdsresult = std_itoa(nLongValue, 2);
    wxString wxsResult = wxString(stdsresult.c_str(), wxConvUTF8);
    for (unsigned ii = 0; ii < (wxsResult.Length() % 4); ++ii)
        wxsResult.Prepend(_T("0"));
    wxsResult.Prepend(_T("0b"));
    return wxString::Format(_T("%s"), wxsResult.c_str());
}
// ----------------------------------------------------------------------------
std::string std_itoa(int value, int base)
// ----------------------------------------------------------------------------
{
    std::string buf;

    // check that the base if valid
    if (base < 2 || base > 16) return buf;

    enum { kMaxDigits = sizeof(long)*8+1 };
    buf.reserve( kMaxDigits ); // Pre-allocate enough space.

    int quotient = value;

    // Translating number to string with base:
    do {
        buf += "0123456789abcdef"[ std::abs( quotient % base ) ];
        quotient /= base;
    } while ( quotient );

    // Append the negative sign
    if ( value < 0) buf += '-';

    std::reverse( buf.begin(), buf.end() );
    return buf;
}

// ------------------------------------------------------------------------
wxString AddQuotesIfNeeded(const wxString& str)
// ------------------------------------------------------------------------
{
    wxString outstr = str;
    QuoteStringIfNeeded(outstr);
    return outstr;
}
// ----------------------------------------------------------------------------
Debugger_GDB_MI* GetPluginPtr()
// ----------------------------------------------------------------------------
{
    cbDebuggerPlugin* pPlugin = (cbDebuggerPlugin*)Manager::Get()->GetPluginManager()->FindPluginByName("debugger_gdbmi");
    if (not pPlugin)
    {
        Manager::Get()->GetLogManager()->LogError(_("debugger_gdbmi cannot get its ptr from cbPlugin"));
    }
    return pPlugin ? (Debugger_GDB_MI*)pPlugin : nullptr;
}
// ----------------------------------------------------------------------------
cbDebuggerPlugin* GetPluginParentPtr()
// ----------------------------------------------------------------------------
{
    // Returns cbPlugin* casted to cbDebuggerPlugin*, the parent
    cbDebuggerPlugin* pPlugin = (cbDebuggerPlugin*)Manager::Get()->GetPluginManager()->FindPluginByName("debugger_gdbmi");
    if (not pPlugin)
    {
        Manager::Get()->GetLogManager()->LogError(_("debugger_gdbmi cannot get its ptr from cbPlugin"));
    }

    return pPlugin ? pPlugin : nullptr;
}
// ----------------------------------------------------------------------------
dbg_mi::WatchesContainer& GetWatchesContainer()
// ----------------------------------------------------------------------------
{
    return GetPluginPtr()->GetWatchesContainer(); //returns a reference to plugin::m_watches
}


// This used to be in DisassemblyDlg, needs fixing
// ------------------------------------------------------------------------
void AddSourceLineByFile(unsigned long int lineno, const wxString& filename)
// ------------------------------------------------------------------------
{
    // These decls were in the old defs/cpp
    wxScintilla* m_pCode = nullptr;
    bool m_ClearFlag = false;
    std::vector<char> m_LineTypes; //'S' once; 'D' disassembvly


    m_pCode->SetReadOnly(false);
    if (m_ClearFlag)
    {
        m_ClearFlag = false;
        m_pCode->ClearAll();
    }
    // fetch source line from file
    wxString line = _T("file name not provided");
    if (filename.empty())
        line = _T("file name not provided");
    else for(int onlyonce=1; onlyonce; onlyonce=0)
    {
        EditorManager* edmgr = Manager::Get()->GetEditorManager();
        cbEditor* ed = edmgr->GetBuiltinEditor(filename);
        if (not ed)
            ed = edmgr->Open(filename);
        if (not ed)
            break;
        cbStyledTextCtrl* txtctrl = ed->GetControl();
        if (not txtctrl)
            break;
        line = txtctrl->GetLine((lineno > 0)? lineno-1 : 0).c_str();
        line.Trim(true);
        line.Trim(false);
    }

    wxString fmt;
    fmt.Printf(_T("%-3lu\t%s\n"), lineno, line.c_str());

    m_pCode->AppendText(fmt);

    m_pCode->SetReadOnly(true);
    m_LineTypes.push_back('S') ;
}//end AddSourceLineByFile

} //end namespace dbg_mi
