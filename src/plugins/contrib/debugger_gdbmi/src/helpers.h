#ifndef _Debugger_GDB_MI_HELPERS_H_
#define _Debugger_GDB_MI_HELPERS_H_

#include <wx/dir.h>     // (ph 26/06/17) FolderFindTraverser
#include <wx/filefn.h>  // For wxMatchWild
#include <wx/arrstr.h>

//#include <wx\string.h>
#include "definitions.h"

class Compiler;
class ProjectBuildTarget;
class Debugger_GDB_MI;
class CommandID; //(ph 2024/04/13)
class ResultParser; //(ph 2024/04/13)

namespace dbg_mi
{
    int ParseParentPID(const char *line);

    enum ERR_MSGTYPE_RESPONSE{ errNone, errInfo, errCritical, errAbort };

    void      SetRunActionMsg(wxString message, wxString title, ERR_MSGTYPE_RESPONSE msgtype );
    bool      GetRunActionMsg(wxString& msgtype, wxString& msgTitle, wxString& msgText);
    size_t    IsRunActionMsgQueued();

    bool      IsProcessRunning(long pid);

    Compiler* GetActiveCompiler(const cbProject* pProject = 0, ProjectBuildTarget* pTarget = 0);
    wxString  GetActiveCompilerID(const cbProject* pProject = 0, ProjectBuildTarget* pTarget = 0);
    bool      SetDebugMark(const bool onoff);
    bool      IsActionsMapEmpty();
    wxString  AddQuotesIfNeeded(const wxString& str);   //(ph 2024/03/02)
    void AddSourceLineByFile(unsigned long int lineno, const wxString& filename);
    void AddCompilerPrettyPrintCommands(wxString& gdbCmdStr, ProjectBuildTarget* pTarget);    // (ph 26/06/17)
    wxString FindSpecificFolder(wxString searchPath, wxString targetFolder);   // (ph 26/06/17)

    cbDebuggerPlugin* GetPluginParentPtr();
    Debugger_GDB_MI* GetPluginPtr();
    dbg_mi::WatchesContainer& GetWatchesContainer();

    // Routines primarily used to convert watches values (cf EditWatchesDlg)
    long AnyToLong(const wxString& inString);
    wxString AnyToIntStr(wxString& inString);
    wxString AnyToUIntStr(wxString& inString);
    //-    wxString IntStrToHexStr(wxString& inString);
    wxString IntToCharStr(wxString& inString);
    wxString IntToHexStr(wxString& inNumber);
    wxString IntToHexStr(wxString& inNumber);
    //-    wxString LongToHexStr(wxString& inNumber);
    wxString StdStrToHexStr(std::string& inString);
    wxString UIntToHexStr(wxString& inNumber);
    wxString CharToHexStr(wxString& inNumber);

    wxString wxStringToHexStr(wxString& str);
    void     TextDecToHex(int dec, char* buf);

    //-    wxString HexStrToIntStr(wxString& inString);
    //-    wxString CharToIntStr(const wxChar& inChar);
    wxString UIntToBinStr(const wxString& inStr);
    std::string std_itoa(int value, int base); //gcc itoa() didn't work

// ----------------------------------------------------------------------------
class FolderFinderTraverser : public wxDirTraverser
// ----------------------------------------------------------------------------
{
public:
    // We pass the pattern we want to match (e.g., "python/libstdcxx.v?\\printers")
    FolderFinderTraverser(const wxString& pattern, wxArrayString& results)
        : m_pattern(pattern), m_results(results) {}

    virtual wxDirTraverseResult OnFile(const wxString& WXUNUSED(filename)) override
    {
        // Skip files entirely, keep looking
        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir(const wxString& dirname) override
    {
        // Normalize slashes so Windows backslashes and Unix slashes match consistently
        wxString normalizedDir = dirname;
        normalizedDir.Replace("\\", "/");

        wxString normalizedPattern = m_pattern;
        normalizedPattern.Replace("\\", "/");

        // Use wxMatchWild to handle the '?' or '*' wildcard syntax natively
        // We append a wildcard prefix because the pattern is a subsegment of the absolute path
        if (wxMatchWild("*" + normalizedPattern, normalizedDir))
        {
            m_results.Add(dirname);
            // If you only need the first match, you can return wxDIR_STOP here instead!
        }

        return wxDIR_CONTINUE;
    }

private:
    wxString m_pattern;
    wxArrayString& m_results;
};
} // namespace dbg_mi

#endif // _Debugger_GDB_MI_HELPERS_H_

