/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CDB_DEBUGGER_COMMANDS_H
#define CDB_DEBUGGER_COMMANDS_H

#include <wx/string.h>
#include <wx/regex.h>
#include <wx/tipwin.h>
#include <globals.h>
#include <manager.h>
#include <cbdebugger_interfaces.h>
#include "debugger_defs.h"
#include "debuggergdb.h"
#include "debuggermanager.h"
#include "parsewatchvalue.h"

static wxRegEx reProcessInf(_T("id:[[:blank:]]+([A-Fa-f0-9]+)[[:blank:]]+create"));
// For 64-bit:
//  # Child-SP          RetAddr           Call Site
// 00 00000042`85cffde0 00007ff6`4bcbfa3e 111!HELLO+0x24 [C:\tmp\111\main.f90 @ 5]
static wxRegEx reBT1(_T("([0-9]+) ([A-Fa-f0-9`]+) ([A-Fa-f0-9`]+) ([^[]*)"));

// Match lines like:
// 0018ff38 004013ef dbgtest!main+0x3 [main.cpp @ 8]
static wxRegEx reBT2(_T("\\[(.+)[[:blank:]]@[[:blank:]]([0-9]+)\\][[:blank:]]*"));

//    15 00401020 55               push    ebp
//    61 004010f9 ff15dcc24000  call dword ptr [Win32GUI!_imp__GetMessageA (0040c2dc)]
//    71 0040111f c21000           ret     0x10
// For 64-bit:
//     2 00007ff6`4bbf921d 4881ec90000000  sub     rsp,90h
static wxRegEx reDisassembly(_T("^[0-9]+[[:blank:]]+([A-Fa-f0-9`]+)[[:blank:]]+[A-Fa-f0-9]+[[:blank:]]+(.*)$"));
//  # ChildEBP RetAddr
// 00 0012fe98 00401426 Win32GUI!WinMain+0x89 [c:\devel\tmp\win32 test\main.cpp @ 55]
// For 64-bit:
//  # Child-SP          RetAddr           Call Site
// 00 00000042`85cffde0 00007ff6`4bcbfa3e 111!HELLO+0x24 [C:\tmp\111\main.f90 @ 5]
// (00007ff6`4bbf921c)   111!HELLO+0x24   |  (00007ff6`4bbf92d0)   111!for_set_reentrancy
static wxRegEx reDisassemblyFile(_T("[0-9]+[[:blank:]]+([A-Fa-f0-9`]+)[[:blank:]]+[A-Fa-f0-9`]+[[:blank:]]+(.*)\\[([A-z]:)(.*) @ ([0-9]+)\\]"));
static wxRegEx reDisassemblyFunc(_T("^\\(([A-Fa-f0-9`]+)\\)[[:blank:]]+"));

// 01 0012ff68 00404168 cdb_test!main+0xae [c:\dev\projects\tests\cdb_test\main.cpp @ 21]
// For 64-bit:
//  # Child-SP          RetAddr           Call Site
// 00 000000ab`710dfac0 00007ff6`4bcbfa3e 111!HELLO+0x24 [C:\tmp\111\main.f90 @ 5]
static wxRegEx reSwitchFrame(wxT("[[:blank:]]*([0-9]+)[[:blank:]]([0-9a-z`]+)[[:blank:]](.+)[[:blank:]]\\[(.+)[[:blank:]]@[[:blank:]]([0-9]+)\\][[:blank:]]*"));

// 0012ff74  00 00 00 00 c0 ff 12 00-64 13 40 00 01 00 00 00  ........d.@.....
static wxRegEx reExamineMemoryLine(wxT("([0-9a-f`]+) ((( |-)[0-9a-f]{2}){1,16})"));

// .  0  Id: 2d84.1ac0 Suspend: 1 Teb: 00fb3000 Unfrozen
//    1  Id: 33e8.c6c Suspend: 1 Teb: 00f45000 Unfrozen
static wxRegEx reThread("([.# ])  ([0-9]+) (.*)");

// prv local  int la = 0n0
static wxRegEx reLocalsArgs("prv (local|param)  (.+) (.+)=(.+)");

/**
  * Command to add a search directory for source files in debugger's paths.
  */
class CdbCmd_AddSourceDir : public DebuggerCmd
{
    public:
        /** If @c dir is empty, resets all search dirs to $cdir:$cwd, the default. */
        CdbCmd_AddSourceDir(DebuggerDriver* driver, const wxString& dir)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("directory ") << dir;
        }
        void ParseOutput(const wxString& output) override
        {
            // Output:
            // Warning: C:\Devel\tmp\console\111: No such file or directory.
            // Source directories searched: <dir>;$cdir;$cwd
            if (output.StartsWith(_T("Warning: ")))
                m_pDriver->Log(output.BeforeFirst(_T('\n')));
        }
};

/**
  * Command to the set the file to be debugged.
  */
class CdbCmd_SetDebuggee : public DebuggerCmd
{
    public:
        /** @param file The file to debug. */
        CdbCmd_SetDebuggee(DebuggerDriver* driver, const wxString& file)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("file ") << file;
        }
        void ParseOutput(const wxString& output) override
        {
            // Output:
            // Reading symbols from C:\Devel\tmp\console/console.exe...done.
            // or if it doesn't exist:
            // console.exe: No such file or directory.

            // just log everything before the prompt
            m_pDriver->Log(output.BeforeFirst(_T('\n')));
        }
};

/**
  * Command to the add symbol files.
  */
class CdbCmd_AddSymbolFile : public DebuggerCmd
{
    public:
        /** @param file The file which contains the symbols. */
        CdbCmd_AddSymbolFile(DebuggerDriver* driver, const wxString& file)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("add-symbol-file ") << file;
        }
        void ParseOutput(const wxString& output) override
        {
            // Output:
            //
            // add symbol table from file "console.exe" at
            // Reading symbols from C:\Devel\tmp\console/console.exe...done.
            //
            // or if it doesn't exist:
            // add symbol table from file "console.exe" at
            // console.exe: No such file or directory.

            // just ignore the "add symbol" line and log the rest before the prompt
            m_pDriver->Log(output.AfterFirst(_T('\n')).BeforeLast(_T('\n')));
        }
};

/**
  * Command to set the arguments to the debuggee.
  */
class CdbCmd_SetArguments : public DebuggerCmd
{
    public:
        /** @param file The file which contains the symbols. */
        CdbCmd_SetArguments(DebuggerDriver* driver, const wxString& args)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("set args ") << args;
        }
        void ParseOutput(cb_unused const wxString& output) override
        {
            // No output
        }
};

/**
 * Command to find the PID of the active child
 */
class CdbCmd_GetPID : public DebuggerCmd
{
    public:
        /** @param file The file to debug. */
        CdbCmd_GetPID(DebuggerDriver* driver)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("|.");
        }
        void ParseOutput(const wxString& output) override
        {
            // Output:
            // <decimal process num> id: <hex PID> create name: <process name>
            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                if (reProcessInf.Matches(lines[i]))
                {
                    wxString hexID = reProcessInf.GetMatch(lines[i],1);

                    long pid;
                    if (hexID.ToLong(&pid,16))
                    {
                        m_pDriver->SetChildPID(pid);
                    }
                }
            }
        }
};

/**
  * Command to the attach to a process.
  */
class CdbCmd_AttachToProcess : public DebuggerCmd
{
    private:
        int m_pid;
    public:
        /** @param file The file to debug. */
        CdbCmd_AttachToProcess(DebuggerDriver* driver, int pid)
            : DebuggerCmd(driver),
            m_pid(pid)
        {
            m_Cmd << _T("attach ") << wxString::Format(_T("%d"), pid);
        }
        void ParseOutput(const wxString& output) override
        {
            // Output:
            // Attaching to process <pid>
            // or,
            // Can't attach to process.
            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                if (lines[i].StartsWith(_T("Attaching")))
                {
                    m_pDriver->Log(lines[i]);
                    m_pDriver->SetChildPID(m_pid);
                }
                else if (lines[i].StartsWith(_T("Can't ")))
                {
                    // log this and quit debugging
                    m_pDriver->Log(lines[i]);
                    m_pDriver->QueueCommand(new DebuggerCmd(m_pDriver, _T("quit")));
                }
//                m_pDriver->DebugLog(lines[i]);
            }
        }
};

/**
  * Command to the detach from the process.
  */
class CdbCmd_Detach : public DebuggerCmd
{
    public:
        /** @param file The file to debug. */
        CdbCmd_Detach(DebuggerDriver* driver)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T(".detach");
        }
        void ParseOutput(const wxString& output) override
        {
            // output any return, usually "Detached"
            m_pDriver->Log(output);
        }
};

/**
  * Command to continue execution and notify the debugger plugin.
  */
class CdbCmd_Continue : public DebuggerContinueBaseCmd
{
    public:
        /** @param bp The breakpoint to set. */
        CdbCmd_Continue(DebuggerDriver* driver)
            : DebuggerContinueBaseCmd(driver,_T("g"))
        {
        }
        void Action() override
        {
            m_pDriver->NotifyDebuggeeContinued();
        }
};

/**
  * Command to add a breakpoint.
  */
class CdbCmd_AddBreakpoint : public DebuggerCmd
{
        static int m_lastIndex;
    public:
        /** @param bp The breakpoint to set. */
        CdbCmd_AddBreakpoint(DebuggerDriver* driver, cb::shared_ptr<DebuggerBreakpoint> bp)
            : DebuggerCmd(driver),
            m_BP(bp)
        {
            if (bp->enabled)
            {
                if (bp->index==-1)
                    bp->index = m_lastIndex++;

                wxString out = m_BP->filename;
//                DebuggerGDB::ConvertToGDBFile(out);
                QuoteStringIfNeeded(out);
                // we add one to line,  because scintilla uses 0-based line numbers, while cdb uses 1-based
                m_Cmd << _T("bu") << wxString::Format(_T("%ld"), (int) bp->index) << _T(' ');
                if (m_BP->temporary)
                    m_Cmd << _T("/1 ");
                if (bp->func.IsEmpty())
                    m_Cmd << _T('`') << out << _T(":") << wxString::Format(_T("%d"), bp->line) << _T('`');
                else
                    m_Cmd << bp->func;
                bp->alreadySet = true;
            }
        }
        void ParseOutput(const wxString& output) override
        {
            // possible outputs (only output lines starting with ***):
            //
            // *** WARNING: Unable to verify checksum for Win32GUI.exe
            // *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINDOWS\system32\USER32.dll -
            // *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINDOWS\system32\GDI32.dll -
            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                if (lines[i].StartsWith(_T("*** ")))
                    m_pDriver->Log(lines[i]);
            }
        }

        cb::shared_ptr<DebuggerBreakpoint> m_BP;
};

int CdbCmd_AddBreakpoint::m_lastIndex = 1;

/**
  * Command to remove a breakpoint.
  */
class CdbCmd_RemoveBreakpoint : public DebuggerCmd
{
    public:
        /** @param bp The breakpoint to remove. If NULL, all breakpoints are removed. */
        CdbCmd_RemoveBreakpoint(DebuggerDriver* driver, cb::shared_ptr<DebuggerBreakpoint> bp)
            : DebuggerCmd(driver),
            m_BP(bp)
        {
            if (!bp)
                m_Cmd << _T("bc *");
            else
                m_Cmd << _T("bc ") << wxString::Format(_T("%d"), (int) bp->index);
        }
        void ParseOutput(const wxString& output) override
        {
            // usually no output, so display whatever comes in
            if (!output.IsEmpty())
                m_pDriver->Log(output);
        }

        cb::shared_ptr<DebuggerBreakpoint> m_BP;
};

/**
  * Command to get info about a watched variable.
  */
class CdbCmd_Watch : public DebuggerCmd
{
        cb::shared_ptr<GDBWatch> m_watch;
    public:
        CdbCmd_Watch(DebuggerDriver* driver, cb::shared_ptr<GDBWatch> const &watch)
            : DebuggerCmd(driver),
            m_watch(watch)
        {
            wxString symbol;
            m_watch->GetSymbol(symbol);
            m_Cmd << wxT("?? ") << symbol;
        }

        void ParseOutput(const wxString& output) override
        {
            if(!ParseCDBWatchValue(m_watch, output))
            {
                wxString symbol;
                m_watch->GetSymbol(symbol);
                wxString const &msg = wxT("Parsing CDB output failed for '") + symbol + wxT("'!");
                m_watch->SetValue(msg);
                Manager::Get()->GetLogManager()->LogError(msg);
            }
        }
};

/**
  * Command to display a tooltip about a variables value.
  */
class CdbCmd_TooltipEvaluation : public DebuggerCmd
{
        wxTipWindow* m_pWin;
        wxRect m_WinRect;
        wxString m_What;
    public:
        /** @param what The variable to evaluate.
            @param win A pointer to the tip window pointer.
            @param tiprect The tip window's rect.
        */
        CdbCmd_TooltipEvaluation(DebuggerDriver* driver, const wxString& what, const wxRect& tiprect)
            : DebuggerCmd(driver),
            m_pWin(nullptr),
            m_WinRect(tiprect),
            m_What(what)
        {
            m_Cmd << _T("?? ") << what;
        }
        void ParseOutput(const wxString& output) override
        {
//            struct HWND__ * 0x7ffd8000
//
//            struct tagWNDCLASSEXA
//               +0x000 cbSize           : 0x7c8021b5
//               +0x004 style            : 0x7c802011
//               +0x008 lpfnWndProc      : 0x7c80b529     kernel32!GetModuleHandleA+0
//               +0x00c cbClsExtra       : 0
//               +0x010 cbWndExtra       : 2147319808
//               +0x014 hInstance        : 0x00400000
//               +0x018 hIcon            : 0x0012fe88
//               +0x01c hCursor          : 0x0040a104
//               +0x020 hbrBackground    : 0x689fa962
//               +0x024 lpszMenuName     : 0x004028ae  "???"
//               +0x028 lpszClassName    : 0x0040aa30  "CodeBlocksWindowsApp"
//               +0x02c hIconSm          : (null)
//
//            char * 0x0040aa30
//             "CodeBlocksWindowsApp"
            wxString tip = m_What + _T("=") + output;

            if (m_pWin)
                (m_pWin)->Destroy();
            m_pWin = new wxTipWindow((wxWindow*)Manager::Get()->GetAppWindow(), tip, 640, &m_pWin, &m_WinRect);
//            m_pDriver->DebugLog(output);
        }
};

inline bool CDBHasChild(const wxString &line)
{
    return line.Contains("ChildEBP") || line.Contains("Child-SP");
}

/**
  * Command to run a backtrace.
  */
class CdbCmd_Backtrace : public DebuggerCmd
{
    public:
        /** @param dlg The backtrace dialog. */
        CdbCmd_Backtrace(DebuggerDriver* driver, bool switchToFirst)
            : DebuggerCmd(driver),
            m_SwitchToFirst(switchToFirst)
        {
            m_Cmd << _T("k n");
        }
        void ParseOutput(const wxString& output) override
        {
            // output is:
            //  # ChildEBP RetAddr
            // 00 0012fe98 00401426 Win32GUI!WinMain+0x89 [c:\devel\tmp\win32 test\main.cpp @ 55]
            // 00 0012fe98 00401426 Win32GUI!WinMain+0x89
            //
            // so we have a two-steps process:
            // 1) Get match for the second version (without file/line info)
            // 2) See if we have file/line info and read it
            m_pDriver->GetStackFrames().clear();

            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            if (!lines.GetCount() || !CDBHasChild(lines[0]))
                return;

            bool firstValid = true;
            bool sourceValid = false;
            cbStackFrame frameToSwitch;

            // start from line 1
            for (unsigned int i = 1; i < lines.GetCount(); ++i)
            {
                if (reBT1.Matches(lines[i]))
                {
                    cbStackFrame sf;
                    sf.MakeValid(true);

                    long int number;
                    reBT1.GetMatch(lines[i], 1).ToLong(&number);

                    sf.SetNumber(number);
                    sf.SetAddress(cbDebuggerStringToAddress(reBT1.GetMatch(lines[i], 2)));
                    sf.SetSymbol(reBT1.GetMatch(lines[i], 4));
                    // do we have file/line info?
                    if (reBT2.Matches(lines[i]))
                    {
                        sf.SetFile(reBT2.GetMatch(lines[i], 1), reBT2.GetMatch(lines[i], 2));
                        if (firstValid)
                            sourceValid = true;
                    }
                    m_pDriver->GetStackFrames().push_back(cb::shared_ptr<cbStackFrame>(new cbStackFrame(sf)));

                    if (m_SwitchToFirst && sf.IsValid() && firstValid)
                    {
                        firstValid = false;
                        frameToSwitch = sf;
                    }
                }
            }
            Manager::Get()->GetDebuggerManager()->GetBacktraceDialog()->Reload();

            if (!firstValid && sourceValid)
            {
                Cursor cursor;
                cursor.file = frameToSwitch.GetFilename();
                frameToSwitch.GetLine().ToLong(&cursor.line);
                cursor.address = frameToSwitch.GetAddressAsString();
                cursor.changed = true;
                m_pDriver->SetCursor(cursor);
                m_pDriver->NotifyCursorChanged();
            }
        }
    private:
        bool m_SwitchToFirst;
};

class CdbCmd_SwitchFrame : public DebuggerCmd
{
    public:
        CdbCmd_SwitchFrame(DebuggerDriver *driver, int frameNumber) :
            DebuggerCmd(driver)
        {
            if (frameNumber < 0)
                m_Cmd = wxT("k n 1");
            else
                m_Cmd = wxString::Format(wxT(".frame %d"), frameNumber);
        }

        void ParseOutput(const wxString& output) override
        {
            wxArrayString lines = GetArrayFromString(output, wxT('\n'));

            for (unsigned ii = 0; ii < lines.GetCount(); ++ii)
            {
                if (CDBHasChild(lines[ii]))
                    continue;
                else if (reSwitchFrame.Matches(lines[ii]))
                {
                    Cursor cursor;
                    cursor.file = reSwitchFrame.GetMatch(lines[ii], 4);
                    wxString const &line_str = reSwitchFrame.GetMatch(lines[ii], 5);
                    if (!line_str.empty())
                        line_str.ToLong(&cursor.line);
                    else
                        cursor.line = -1;

                    cursor.address = reSwitchFrame.GetMatch(lines[ii], 1);
                    cursor.changed = true;
                    m_pDriver->SetCursor(cursor);
                    m_pDriver->NotifyCursorChanged();
                    Manager::Get()->GetDebuggerManager()->GetBacktraceDialog()->Reload();
                    break;
                }
                else
                    break;
            }
        }
};

/**
  * Command to run a disassembly. Use this instead of CdbCmd_DisassemblyInit, which is chained-called.
  */
class CdbCmd_InfoRegisters : public DebuggerCmd
{
    public:
        /** @param dlg The disassembly dialog. */
        CdbCmd_InfoRegisters(DebuggerDriver* driver)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("r");
        }
        void ParseOutput(const wxString& output) override
        {
            // output is:
            //
            // eax=00400000 ebx=7ffd9000 ecx=00000065 edx=7c97e4c0 esi=00000000 edi=7c80b529
            // eip=0040102c esp=0012fe48 ebp=0012fe98 iopl=0         nv up ei pl nz na po nc
            // cs=001b  ss=0023  ds=0023  es=0023  fs=003b  gs=0000             efl=00000206

            cbCPURegistersDlg *dialog = Manager::Get()->GetDebuggerManager()->GetCPURegistersDialog();

            wxString tmp = output;
            while (tmp.Replace(_T("\n"), _T(" ")))
                ;
            wxArrayString lines = GetArrayFromString(tmp, _T(' '));
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                wxString reg = lines[i].BeforeFirst(_T('='));
                wxString addr = lines[i].AfterFirst(_T('='));
                if (!reg.IsEmpty() && !addr.IsEmpty())
                    dialog->SetRegisterValue(reg, addr, wxEmptyString);
            }
        }
};

/**
  * Command to run a disassembly.
  */
class CdbCmd_Disassembly : public DebuggerCmd
{
    public:
        CdbCmd_Disassembly(DebuggerDriver* driver, const wxString& StopAddress)
            : DebuggerCmd(driver)
        {
            m_Cmd << _T("uf ") << StopAddress;
        }
        void ParseOutput(const wxString& output) override
        {
            // output is a series of:
            //
            // Win32GUI!WinMain [c:\devel\tmp\win32 test\main.cpp @ 15]:
            //    15 00401020 55               push    ebp
            // ...

            cbDisassemblyDlg *dialog = Manager::Get()->GetDebuggerManager()->GetDisassemblyDialog();

            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                if (reDisassembly.Matches(lines[i]))
                {
                    uint64_t addr = cbDebuggerStringToAddress(reDisassembly.GetMatch(lines[i], 1));
                    dialog->AddAssemblerLine(addr, reDisassembly.GetMatch(lines[i], 2));
                }
            }
//            m_pDlg->Show(true);
//            m_pDriver->DebugLog(output);
        }
};

/**
  * Command to run a disassembly. Use this instead of CdbCmd_Disassembly, which is chain-called.
  */
class CdbCmd_DisassemblyInit : public DebuggerCmd
{
        static wxString LastAddr;
    public:
        CdbCmd_DisassemblyInit(DebuggerDriver* driver)
            : DebuggerCmd(driver)
        {
            // print stack frame and nearest symbol (start of function)
            m_Cmd << _T("k n 1; ln");
        }
        void ParseOutput(const wxString& output) override
        {
//            m_pDriver->QueueCommand(new CdbCmd_Disassembly(m_pDriver, m_pDlg, StopAddress)); // chain call

            cbDisassemblyDlg *dialog = Manager::Get()->GetDebuggerManager()->GetDisassemblyDialog();

            long int offset = 0;
            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                if (CDBHasChild(lines[i]))
                {
                    if (reDisassemblyFile.Matches(lines[i + 1]))
                    {
                        ++i; // we 're interested in the next line
                        cbStackFrame sf;
                        wxString addr = reDisassemblyFile.GetMatch(lines[i], 1);
                        sf.SetSymbol(reDisassemblyFile.GetMatch(lines[i], 2));
                        wxString offsetStr = sf.GetSymbol().AfterLast(_T('+'));
                        if (!offsetStr.IsEmpty())
                            offsetStr.ToLong(&offset, 16);
                        if (addr != LastAddr)
                        {
                            LastAddr = addr;
                            sf.SetAddress(cbDebuggerStringToAddress(addr));
                            sf.MakeValid(true);
                            dialog->Clear(sf);
                            m_pDriver->QueueCommand(new CdbCmd_Disassembly(m_pDriver, sf.GetSymbol())); // chain call
//                            break;
                        }
                    }
                }
                else
                {
                    m_pDriver->Log(_("Checking for current function start"));
                    if (reDisassemblyFunc.Matches(lines[i]))
                    {
                        uint64_t start = cbDebuggerStringToAddress(reDisassemblyFunc.GetMatch(lines[i], 1));
                        // FIXME (obfuscated#): the offset is wrong type, probably should be fixed.
                        dialog->SetActiveAddress(start + offset);
                    }
                }
            }
        }
};
wxString CdbCmd_DisassemblyInit::LastAddr;

/**
  * Command to examine a memory region.
  */
class CdbCmd_ExamineMemory : public DebuggerCmd
{
    public:
        /** @param dlg The memory dialog. */
        CdbCmd_ExamineMemory(DebuggerDriver* driver)
            : DebuggerCmd(driver)
        {
            cbExamineMemoryDlg *dialog = Manager::Get()->GetDebuggerManager()->GetExamineMemoryDialog();
            const wxString &address = CleanStringValue(dialog->GetBaseAddress());
            m_Cmd.Printf(_T("db %s L%x"), address.c_str(),dialog->GetBytes());
        }
        void ParseOutput(const wxString& output)
        {
            // output is a series of:
            //
            // 0012ff74  00 00 00 00 c0 ff 12 00-64 13 40 00 01 00 00 00  ........d.@.....

            cbExamineMemoryDlg *dialog = Manager::Get()->GetDebuggerManager()->GetExamineMemoryDialog();

            dialog->Begin();
            dialog->Clear();

            wxArrayString lines = GetArrayFromString(output, _T('\n'));
            wxString addr, memory;
            for (unsigned int i = 0; i < lines.GetCount(); ++i)
            {
                if (reExamineMemoryLine.Matches(lines[i]))
                {
                    addr = reExamineMemoryLine.GetMatch(lines[i], 1);
                    memory = reExamineMemoryLine.GetMatch(lines[i], 2);
                    memory.Replace(_T("-"),_T(" "),true);
                }
                else
                {   int pos = lines[i].Find(_T('*'));
                    if ( pos == wxNOT_FOUND || pos > 0)
                    {
                      dialog->AddError(lines[i]);
                    }
                    continue;
                }

                size_t pos = memory.find(_T(' '));
                while (pos != wxString::npos)
                {
                    wxString hexbyte;
                    hexbyte << memory[pos + 1];
                    hexbyte << memory[pos + 2];
                    dialog->AddHexByte(addr, hexbyte);
                    pos = memory.find(_T(' '), pos + 1); // skip current ' '
                }
            }
            dialog->End();
        }
};

/**
  * Command to get info about running threads.
  */
class CdbCmd_Threads : public DebuggerCmd
{
    public:
        CdbCmd_Threads(DebuggerDriver* driver) :
            DebuggerCmd(driver)
        {
            m_Cmd << "~*";
        }

        void ParseOutput(const wxString& output)
        {
            // output is
            //.  0  Id: 2d84.1ac0 Suspend: 1 Teb: 00fb3000 Unfrozen
            //      Start: test_vc!ILT+30(_mainCRTStartup) (00c81023)
            //      Priority: 0  Priority class: 32  Affinity: f

            DebuggerDriver::ThreadsContainer &threads = m_pDriver->GetThreads();
            threads.clear();

            const wxArrayString lines = GetArrayFromString(output, '\n', false);
            for (size_t i = 0; i < lines.GetCount(); ++i)
            {
                m_pDriver->Log(lines[i]);
                if (reThread.Matches(lines[i]))
                {
                    const bool active = (reThread.GetMatch(lines[i], 1).Trim(false) == '.');
                    const wxString num = reThread.GetMatch(lines[i], 2);

#if defined(_WIN64)
                    long long int number;
                    num.ToLongLong(&number, 10);
#else
                    long number;
                    num.ToLong(&number, 10);
#endif
                    const wxString info = reThread.GetMatch(lines[i], 3) + " " +
                                          wxString(lines[i + 1]).Trim(false) + " " +
                                          wxString(lines[i + 2]).Trim(false);
                    threads.push_back(cb::shared_ptr<cbThread>(new cbThread(active, number, info)));
                }
            }
            Manager::Get()->GetDebuggerManager()->GetThreadsDialog()->Reload();
        }
};

class CdbCmd_LocalsFuncArgs : public DebuggerCmd
{
        cb::shared_ptr<GDBWatch> m_watch;
        bool m_doLocals;
    public:
        CdbCmd_LocalsFuncArgs(DebuggerDriver* driver, cb::shared_ptr<GDBWatch> watch, bool doLocals) :
            DebuggerCmd(driver),
            m_watch(watch),
            m_doLocals(doLocals)
        {
            m_Cmd = "dv /i /t";
        }
        void ParseOutput(const wxString& output)
        {
            //output
            //prv param  int a = 0n0
            //prv local  int la = 0n0

            if (output.empty())
            {
                m_watch->RemoveChildren();
                return;
            }

            m_watch->MarkChildsAsRemoved();
            wxString symb, wtype, type, value;
            m_watch->GetSymbol(wtype);
            const bool locals = (wtype == "Locals");

            wxArrayString lines = GetArrayFromString(output, '\n');
            for (size_t i = 0; i < lines.GetCount(); ++i)
            {
                m_pDriver->Log(lines[i]);
                if (reLocalsArgs.Matches(lines[i]))
                {
                    wtype = reLocalsArgs.GetMatch(lines[i], 1);
                    if ((locals && wtype == "local") || (!locals && wtype == "param"))
                    {
                        m_pDriver->Log(lines[i]);
                        type = reLocalsArgs.GetMatch(lines[i], 2);
                        symb = reLocalsArgs.GetMatch(lines[i], 3);
                        value = reLocalsArgs.GetMatch(lines[i], 4);
                        cb::shared_ptr<GDBWatch> watch = AddChild(m_watch, symb);
                        watch->SetValue(value);
                        watch->SetType(type);
                    }
                }
            }
            m_watch->RemoveMarkedChildren();
        }
};

#endif // DEBUGGER_COMMANDS_H
