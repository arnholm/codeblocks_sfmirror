/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef DEBUGGER_DEFS_H
#define DEBUGGER_DEFS_H

#include <wx/string.h>
#include <wx/dynarray.h>
#include <deque>
#include <vector>
#include <unordered_map>

#include "debuggermanager.h"
#include "cbplugin.h"

class DebuggerDriver;

extern const int DEBUGGER_CURSOR_CHANGED; ///< wxCommandEvent ID fired when the cursor has changed.
extern const int DEBUGGER_SHOW_FILE_LINE; ///< wxCommandEvent ID fired to display a file/line (w/out changing the cursor)

/** Debugger cursor info.
  *
  * Contains info about the debugger's cursor, i.e. where it currently is at
  * (file, line, function, address).
  */
struct Cursor
{
    Cursor() : line(-1), changed(false) {}
    wxString file;
    wxString address;
    wxString function;
    long int line; ///< If -1, no line info
    bool changed;
};

/** Basic interface for debugger commands.
  *
  * Each command sent to the debugger, is a DebuggerCmd.
  * It encapsulates the call and parsing of return values.
  * The most important function is ParseOutput() inside
  * which it must parse the commands output.
  * It is guaranteed that the @c output argument to ParseOutput()
  * contains the full debugger response to the command given.
  *
  * @remarks This is not an abstract interface, i.e. you can
  * create instances of it. The default implementation just
  * logs the command's output. This way you can debug new commands.
  */
class DebuggerCmd
{
    public:
        DebuggerCmd(DebuggerDriver* driver, const wxString& cmd = _T(""), bool logToNormalLog = false);
        virtual ~DebuggerCmd(){}

        /** Executes an action.
          *
          * This allows for "dummy" debugger commands to enter the commands queue.
          * You can, for example, leave m_Cmd empty and just have Action()
          * do something GUI-related (like the watches command does).
          * Action() is called when the debugger command becomes current in the
          * commands queue. It is called after sending m_Cmd to the debugger (if not empty).
          */
        virtual void Action(){}

        /** Parses the command's output.
          * @param The output. This is the full output up to
          * (and including) the prompt.
          */
        virtual void ParseOutput(const wxString& output);

        /** Tells if the command is a continue type command (continue, step, next and run to cursor
          * commands should be marked as such)
          * @return true if the command is continue type command
          */
        virtual bool IsContinueCommand() const { return false; }

        wxString m_Cmd;         ///< the actual command
    protected:
        DebuggerDriver* m_pDriver; ///< the driver
        bool m_LogToNormalLog;  ///< if true, log to normal log, else the debug log
};

/** This command is similar to DebuggerCmd
  * The only difference is that instead of logging its output in the debugger log,
  * it displays it in a dialog.
  */
class DebuggerInfoCmd : public DebuggerCmd
{
    public:
        DebuggerInfoCmd(DebuggerDriver* driver, const wxString& cmd, const wxString& title)
            : DebuggerCmd(driver, cmd),
            m_Title(title)
        {
            m_Cmd = cmd;
        }
        ~DebuggerInfoCmd() override {}

        void ParseOutput(const wxString& output) override;
        wxString m_Title;
};

/** Base class for all Continue type of commands */
class DebuggerContinueBaseCmd : public DebuggerCmd
{
    public:
        DebuggerContinueBaseCmd(DebuggerDriver* driver, const wxString& cmd = _T(""), bool logToNormalLog = false) :
            DebuggerCmd(driver, cmd, logToNormalLog)
        {
        }

        bool IsContinueCommand() const override { return true; }
};

/** Action-only debugger command to signal the watches tree to update. */
class DbgCmd_UpdateWindow : public DebuggerCmd
{
    public:
        DbgCmd_UpdateWindow(DebuggerDriver* driver, cbDebuggerPlugin::DebugWindows windowToUpdate);
        void Action() override;

    private:
        cbDebuggerPlugin::DebugWindows m_windowToUpdate;
};

/** Debugger breakpoint interface.
  *
  * This is the struct used for debugger breakpoints.
  */
////////////////////////////////////////////////////////////////////////////////
struct DebuggerBreakpoint : cbBreakpoint
{
    enum BreakpointType
    {
        bptCode = 0,    ///< Normal file/line breakpoint
        bptFunction,    ///< Function signature breakpoint
        bptData            ///< Data breakpoint
    };

    /** Constructor.
      * Sets default values for members.
      */
    DebuggerBreakpoint()
        : type(bptCode),
        line(0),
        index(-1),
        temporary(false),
        enabled(true),
        active(true),
        useIgnoreCount(false),
        ignoreCount(0),
        useCondition(false),
        wantsCondition(false),
        address(0),
        alreadySet(false),
        breakOnRead(false),
        breakOnWrite(true),
        userData(nullptr)
    {}

    // from cbBreakpoint
    void SetEnabled(bool flag) override;
    wxString GetLocation() const override;
    int GetLine() const override;
    wxString GetLineString() const override;
    wxString GetType() const override;
    wxString GetInfo() const override;
    bool IsEnabled() const override;
    bool IsVisibleInEditor() const override;
    bool IsTemporary() const override;

    BreakpointType type; ///< The type of this breakpoint.
    wxString filename; ///< The filename for the breakpoint (kept as relative).
    wxString filenameAsPassed; ///< The filename for the breakpoint as passed to the debugger (i.e. full filename).
    int line; ///< The line for the breakpoint.
    long int index; ///< The breakpoint number. Set automatically. *Don't* write to it.
    bool temporary; ///< Is this a temporary (one-shot) breakpoint?
    bool enabled; ///< Is the breakpoint enabled?
    bool active; ///< Is the breakpoint active? (currently unused)
    bool useIgnoreCount; ///< Should this breakpoint be ignored for the first X passes? (@c x == @c ignoreCount)
    int ignoreCount; ///< The number of passes before this breakpoint should hit. @c useIgnoreCount must be true.
    bool useCondition; ///< Should this breakpoint hit only if a specific condition is met?
    bool wantsCondition; ///< Evaluate condition for pending breakpoints at first stop !
    wxString condition; ///< The condition that must be met for the breakpoint to hit. @c useCondition must be true.
    wxString func; ///< The function to set the breakpoint. If this is set, it is preferred over the filename/line combination.
    unsigned long int address; ///< The actual breakpoint address. This is read back from the debugger. *Don't* write to it.
    bool alreadySet; ///< Is this already set? Used to mark temporary breakpoints for removal.
    wxString lineText; ///< Optionally, the breakpoint line's text (used by GDB for setting breapoints on ctors/dtors).
    wxString breakAddress; ///< Valid only for type==bptData: address to break when read/written.
    bool breakOnRead; ///< Valid only for type==bptData: break when memory is read from.
    bool breakOnWrite; ///< Valid only for type==bptData: break when memory is written to.
    void* userData; ///< Custom user data.
};
typedef std::deque<cb::shared_ptr<DebuggerBreakpoint> > BreakpointsList;

/** Watch variable format.
  *
  * @note not all formats are implemented for all debugger drivers.
  */
enum WatchFormat
{
    Undefined = 0, ///< Format is undefined (whatever the debugger uses by default).
    Decimal, ///< Variable should be displayed as decimal.
    Unsigned, ///< Variable should be displayed as unsigned.
    Hex, ///< Variable should be displayed as hexadecimal (e.g. 0xFFFFFFFF).
    Binary, ///< Variable should be displayed as binary (e.g. 00011001).
    Char, ///< Variable should be displayed as a single character (e.g. 'x').
    Float, ///< Variable should be displayed as floating point number (e.g. 14.35)

    // do not remove these
    Last, ///< used for iterations
    Any ///< used for watches searches
};

class GDBWatch : public cbWatch
{
    public:
        GDBWatch(wxString const &symbol);
        ~GDBWatch() override;

    public:
        void GetSymbol(wxString &symbol) const override;
        void SetSymbol(const wxString &symbol) override;
        uint64_t GetAddress() const override;
        void SetAddress(uint64_t address) override;
        void GetValue(wxString &value) const override;
        bool SetValue(const wxString &value) override;
        bool GetIsValueErrorMessage() override;
        void SetIsValueErrorMessage(bool value) override;
        void GetFullWatchString(wxString &full_watch) const override;
        void GetType(wxString &type) const override;
        void SetType(const wxString &type) override;

        wxString GetDebugString() const override;

        wxString MakeSymbolToAddress() const override;
        bool IsPointerType() const override;

    public:
        void SetDebugValue(wxString const &value);

        void SetFormat(WatchFormat format);
        WatchFormat GetFormat() const;

        void SetArray(bool flag);
        bool IsArray() const;
        void SetArrayParams(int start, int count);
        int GetArrayStart() const;
        int GetArrayCount() const;

        void SetForTooltip(bool flag = true);
        bool GetForTooltip() const;

    protected:
        virtual void DoDestroy();

    private:
        wxString m_symbol;
        wxString m_type;
        wxString m_raw_value;
        wxString m_debug_value;
        WatchFormat m_format;
        int m_array_start;
        int m_array_count;
        bool m_is_array;
        bool m_forTooltip;
        bool m_raw_value_is_message;   // True if the m_raw_value is a message instead of data

        uint64_t m_address;
};

class GDBMemoryRangeWatch  : public cbWatch
{
    public:
        GDBMemoryRangeWatch(uint64_t address, uint64_t size, const wxString& symbol);

    public:
        void GetSymbol(wxString &symbol) const override { symbol = m_symbol; }
        void SetSymbol(const wxString& symbol) override {m_symbol = symbol; }
        uint64_t GetAddress() const override { return m_address; }
        void SetAddress(uint64_t address) override { m_address = address; }
        void GetValue(wxString &value) const override { value = m_value; }
        bool SetValue(const wxString &value) override;
        bool GetIsValueErrorMessage() override { return m_ValueErrorMessage; }
        void SetIsValueErrorMessage(bool value) override { m_ValueErrorMessage = value; }
        void GetFullWatchString(wxString &full_watch) const override { full_watch = wxEmptyString; }
        void GetType(wxString &type) const override { type = wxT("Memory range"); }
        void SetType(cb_unused const wxString &type) override {}

        wxString GetDebugString() const override { return wxString(); }

        wxString MakeSymbolToAddress() const override;
        bool IsPointerType() const override { return false; }

        uint64_t GetSize() const { return m_size; }

    private:
        uint64_t m_address;
        uint64_t m_size;
        wxString m_symbol;
        wxString m_value;
        bool m_ValueErrorMessage;
};

typedef std::vector<cb::shared_ptr<GDBWatch>> WatchesContainer;
typedef std::vector<cb::shared_ptr<GDBMemoryRangeWatch>> MemoryRangeWatchesContainer;


enum class WatchType
{
    Normal,
    MemoryRange
};

typedef std::unordered_map<cb::shared_ptr<cbWatch>, WatchType> MapWatchesToType;

bool IsPointerType(const wxString &type);
wxString CleanStringValue(wxString value);

enum DebuggerLanguage
{
    dl_Cpp = 0, ///< C++ or C language.
    dl_Fortran  ///< Fortran language.
};

extern DebuggerLanguage g_DebugLanguage;


#endif // DEBUGGER_DEFS_H
