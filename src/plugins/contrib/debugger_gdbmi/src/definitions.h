#ifndef _Debugger_GDB_MI_DEFINITIONS_H_
#define _Debugger_GDB_MI_DEFINITIONS_H_

#include <deque>
#include <memory>
#include <unordered_map>
#include <wx/sizer.h>

#include <debuggermanager.h>
#include <scrollingdialog.h>
#include "cbplugin.h"

namespace dbg_mi
{
extern const int DEBUGGER_CURSOR_CHANGED; ///< wxCommandEvent ID fired when the cursor has changed.
extern const int DEBUGGER_SHOW_FILE_LINE; ///< wxCommandEvent ID fired to display a file/line (w/out changing the cursor)

    struct Cursor
    {
        Cursor() : line(-1), changed(false) {}
        wxString file;
        wxString address;
        wxString function;
        long int line; ///< If -1, no line info
        bool changed;
    };

// ----------------------------------------------------------------------------
class Breakpoint : public cbBreakpoint
// ----------------------------------------------------------------------------
{
public:
    typedef std::shared_ptr<Breakpoint> Pointer;
public:
    Breakpoint() :
        m_type(_T("Code")),
        m_project(nullptr),
        m_index(-1),
        m_line(-1),
        m_enabled(true),
        m_temporary(false),
        m_ignoreCount(0),
        m_useIgnoreCount(false),
        m_useCondition(false),
        m_breakOnDataRead(false),
        m_breakOnDataWrite(false),
        m_dataAddress( wxEmptyString)
        //-m_info(wxEmptyString)
    {

    }

    Breakpoint(const wxString &filename, int line, cbProject *project) :
        m_type(_T("Code")),
        m_filename(filename),
        m_project(project),
        m_index(-1),
        m_line(line),
        m_enabled(true),
        m_temporary(false),
        m_ignoreCount(0),
        m_useIgnoreCount(false),
        m_useCondition(false),
        m_breakOnDataRead(false),
        m_breakOnDataWrite(false),
        m_dataAddress( wxEmptyString)
        //-m_info(wxEmptyString)
    {
    }

    virtual void SetEnabled(bool flag);
    virtual wxString GetLocation() const;
    virtual int GetLine() const;
    virtual wxString GetLineString() const;
    virtual wxString GetType() const;
    virtual void SetType(const wxString& type) {m_type = type;}
    virtual wxString GetInfo() const;
    virtual bool IsEnabled() const;
    virtual bool IsVisibleInEditor() const;
    virtual bool IsTemporary() const;

    int GetIndex() const { return m_index; }
    const wxString& GetCondition() const { return m_condition; }
    bool HasCondition() const { return m_useCondition && (not m_condition.empty()); }
    void SetUseCondition( bool use){m_useCondition = use;}
    void SetCondition(wxString condition){m_condition = condition;}

    int  GetIgnoreCount() const { return m_ignoreCount; }
    void SetIgnoreCount(int nCount) { m_ignoreCount = nCount; }
    void SetUseIgnoreCount(bool use){m_useIgnoreCount = use;}
    bool HasIgnoreCount() const { return m_useIgnoreCount && (m_ignoreCount>0); }

    bool GetBreakOnDataRead() const {return m_breakOnDataRead;}
    bool GetBreakOnDataWrite() const {return m_breakOnDataWrite;}
    void SetBreakOnDataRead(bool tf){m_breakOnDataRead = tf;}
    void SetBreakOnDataWrite(bool tf){m_breakOnDataWrite = tf;}
    void SetDataAddress(const wxString& addr){m_filename = addr;}
    wxString GetDataAddress() const {return m_filename;}

    void SetIndex(int index) { m_index = index; }

    cbProject* GetProject() { return m_project; }
    void SetProject(cbProject* project) { m_project = project; }
    void SetLine(int line){m_line = line;}
    void ShiftLine(int linesToShift) { m_line += linesToShift; }

private:
    wxString m_type;
    wxString m_filename;
    wxString m_condition;
    cbProject *m_project;
    int m_index;
    int m_line;
    bool m_enabled;
    bool m_temporary;
    int  m_ignoreCount;
    bool m_useIgnoreCount;
    bool m_useCondition;
    bool m_breakOnDataRead;
    bool m_breakOnDataWrite;
    wxString m_dataAddress;
    //-wxString m_info;
};


typedef std::deque<cbStackFrame::Pointer> BacktraceContainer;
typedef std::deque<cbThread::Pointer> ThreadsContainer;
// EditWatches support
// ----------------------------------------------------------------------------
enum WatchFormat
// ----------------------------------------------------------------------------
{
    Undefined = 0,  ///< Format is undefined (whatever the debugger uses by default).
    Decimal,        ///< Variable should be displayed as decimal.
    Unsigned,       ///< Variable should be displayed as unsigned.
    Hex,            ///< Variable should be displayed as hexadecimal (e.g. 0xFFFFFFFF).
    Binary,         ///< Variable should be displayed as binary (e.g. 00011001).
    Char,           ///< Variable should be displayed as a single character (e.g. 'x').
    Float,          ///< Variable should be displayed as floating point number (e.g. 14.35)

    // do not remove these
    Last,           ///< used for iterations
    Any             ///< used for watches searches
};

// ----------------------------------------------------------------------------
class Watch : public cbWatch
// ----------------------------------------------------------------------------
{
public:
    typedef cb::shared_ptr<Watch> Pointer;
public:

    // cbProject* added to compare project ownership in workspaces

    Watch(wxString const &symbol, bool for_tooltip, cbProject* project) :
        m_symbol(symbol),
        m_has_been_expanded(false),
        m_for_tooltip(for_tooltip),
        m_project(project),  // Added to compare project ownership in workspaces
        m_format(Undefined), // EditWatch support
        m_array_start(0),
        m_array_count(0),
        m_is_array(false)
    {
    }

    void Reset()
    {
        m_id = m_type = m_value = wxEmptyString;
        m_has_been_expanded = false;

        RemoveChildren();
        Expand(false);
    }

    wxString const & GetID() const { return m_id; }
    void SetID(wxString const &id) { m_id = id; }

    bool HasBeenExpanded() const { return m_has_been_expanded; }
    void SetHasBeenExpanded(bool expanded) { m_has_been_expanded = expanded; }
    bool ForTooltip() const { return m_for_tooltip; }
public:
    virtual void GetSymbol(wxString &symbol) const { symbol = m_symbol; }
    virtual void GetValue(wxString &value) const { value = m_value; }
    //-virtual bool SetValue(const wxString &value) { m_value = value; return true; }
    virtual bool SetValue(const wxString &value); // EditWatches support
    virtual void GetFullWatchString(wxString &full_watch) const { full_watch = m_value; }
    virtual void GetType(wxString &type) const { type = m_type; }
    virtual void SetType(const wxString &type) { m_type = type; }

    // add cbProject* to watch
    cbProject* GetProject(){return m_project;}
    void SetProject(cbProject* project){m_project = project;}

    // EditWatch support
    void SetSymbol(const wxString& symbol);
    void SetFormat(WatchFormat format);
    WatchFormat GetFormat() const;

    void SetArray(bool flag);
    bool IsArray() const;
    void SetArrayParams(int start, int count);
    int  GetArrayStart() const;
    int  GetArrayCount() const;
    void ConvertValueToUserFormat(); // Edit Watch support

    //virtual wxString const & GetDebugString() const // Does not match interface def //(ph 2024/03/02)
    virtual wxString GetDebugString() const
    {
        m_debug_string = m_id + _T("->") + m_symbol + _T(" = ") + m_value;
        return m_debug_string;
    }

    void SetDebugValue(wxString const &value);
    void GetDebugValue(wxString& debugValue);   // (ph 26/05/28)

    // pure fuctions defined in cbWatch that must be provided for but unused by debugger_gdbmi
    virtual uint64_t GetAddress() const {cbAssert(0 && "GetAddress called but not defined");return 0;}
    virtual void SetAddress(uint64_t address) {wxUnusedVar(address);cbAssert(0 && "SetAddress called but not defined");};
    virtual bool GetIsValueErrorMessage() {cbAssert(0 && "GetIsValueErrorMessage called but not defined");return false;}
    virtual void SetIsValueErrorMessage(bool value) {wxUnusedVar(value);cbAssert(0 && "SetIsValueErrorMessage called but not defined");}

protected:
    virtual void DoDestroy() {}
private:
    wxString m_id;
    wxString m_symbol;
    wxString m_value;
    wxString m_type;

    mutable wxString m_debug_string;
    bool m_has_been_expanded;
    bool m_for_tooltip;
    cbProject* m_project;
    WatchFormat m_format; // EditWatch support
    int m_array_start;
    int m_array_count;
    bool m_is_array;
    wxString m_debug_value;

};
    using GDBWatch = Watch;

// ----------------------------------------------------------------------------
class GDBMemoryRangeWatch  : public cbWatch
// ----------------------------------------------------------------------------
{
    public:
//        GDBMemoryRangeWatch(uint64_t address, uint64_t size, const wxString& symbol);
    GDBMemoryRangeWatch(uint64_t address, uint64_t size, const wxString& symbol) :
        m_address(address),
        m_size(size),
        m_symbol(symbol)
    {
    }

    public:
        void GetSymbol(wxString &symbol) const override { symbol = m_symbol; }
        void SetSymbol(const wxString& symbol) override {m_symbol = symbol; }
        uint64_t GetAddress() const override { return m_address; }
        void SetAddress(uint64_t address) override { m_address = address; }
        void GetValue(wxString &value) const override { value = m_value; }
        bool SetValue(const wxString &value) override { wxUnusedVar(value); return false;} //(ph 2024/03/04)
        bool GetIsValueErrorMessage() override { return m_ValueErrorMessage; }
        void SetIsValueErrorMessage(bool value) override { m_ValueErrorMessage = value; }
        void GetFullWatchString(wxString &full_watch) const override { full_watch = wxEmptyString; }
        void GetType(wxString &type) const override { type = wxT("Memory range"); }
        void SetType(cb_unused const wxString &type) override {wxUnusedVar(type);}

        wxString GetDebugString() const override { return wxString(); }

        wxString MakeSymbolToAddress() const override {return wxString();}
        bool IsPointerType() const override { return false; }

        uint64_t GetSize() const { return m_size; }

    private:
        uint64_t m_address;
        uint64_t m_size;
        wxString m_symbol;
        wxString m_value;
        bool m_ValueErrorMessage;
};

typedef std::vector<dbg_mi::Watch::Pointer> WatchesContainer;
typedef std::vector<cb::shared_ptr<GDBMemoryRangeWatch>> MemoryRangeWatchesContainer; //(ph 2024/03/04)
typedef std::vector<cb::shared_ptr<GDBWatch>> WatchesContainer;
typedef std::vector<cb::shared_ptr<GDBMemoryRangeWatch>> MemoryRangeWatchesContainer;

// ----------------------------------------------------------------------------
enum class WatchType
// ----------------------------------------------------------------------------
{
    Normal,
    MemoryRange
};

typedef std::unordered_map<cb::shared_ptr<cbWatch>, WatchType> MapWatchesToType;      //(ph 2024/03/04)
bool IsPointerType(const wxString &type);
wxString CleanStringValue(wxString value);


Watch::Pointer FindWatch(wxString const &expression, WatchesContainer &watches);

// Custom window to display output of DebuggerInfoCmd
// ----------------------------------------------------------------------------
class TextInfoWindow : public wxScrollingDialog
// ----------------------------------------------------------------------------
{
    public:
        TextInfoWindow(wxWindow *parent, const wxChar *title, const wxString& content) :
            wxScrollingDialog(parent, -1, title, wxDefaultPosition, wxDefaultSize,
                              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX),
            //m_font(8, wxMODERN, wxNORMAL, wxNORMAL)
            m_font(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL) //(ph 2024/03/11)
        {
            wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
            m_text = new wxTextCtrl(this, -1, content, wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY | wxTE_MULTILINE | wxTE_RICH2 | wxHSCROLL);
            m_text->SetFont(m_font);

            sizer->Add(m_text, 1, wxGROW);

            SetSizer(sizer);
            sizer->Layout();
        }
        void SetText(const wxString &text)
        {
            m_text->SetValue(text);
            m_text->SetFont(m_font);
        }
    private:
        wxTextCtrl* m_text;
        wxFont m_font;
};

// ----------------------------------------------------------------------------
class CurrentFrame
// ----------------------------------------------------------------------------
{
public:
    CurrentFrame() :
        m_line(-1),
        m_stack_frame(-1),
        m_user_selected_stack_frame(-1),
        m_thread(-1)
    {
    }

    void Reset()
    {
        m_stack_frame = -1;
        m_user_selected_stack_frame = -1;
    }

    void SwitchToFrame(int frame_number)
    {
        m_user_selected_stack_frame = m_stack_frame = frame_number;
    }

    void SetFrame(int frame_number)
    {
        if (m_user_selected_stack_frame >= 0)
            m_stack_frame = m_user_selected_stack_frame;
        else
            m_stack_frame = frame_number;
    }
    void SetThreadId(int thread_id) { m_thread = thread_id; }
    void SetPosition(wxString const &filename, int line)
    {
        m_filename = filename;
        m_line = line;
    }

    int GetStackFrame() const { return m_stack_frame; }
    int GetUserSelectedFrame() const { return m_user_selected_stack_frame; }
    void GetPosition(wxString &filename, int &line)
    {
        filename = m_filename;
        line = m_line;
    }
    int GetThreadId() const { return m_thread; }

private:
    wxString m_filename;
    int m_line;
    int m_stack_frame;
    int m_user_selected_stack_frame;
    int m_thread;
};

} // namespace dbg_mi


enum DebuggerLanguage
{
    dl_Cpp = 0, ///< C++ or C language.
    dl_Fortran  ///< Fortran language.
};

// ----------------------------------------------------------------------------
//  Globals
// ----------------------------------------------------------------------------
extern DebuggerLanguage g_DebugLanguage;

#endif // _Debugger_GDB_MI_DEFINITIONS_H_
