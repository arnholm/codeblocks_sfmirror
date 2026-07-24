#include "actions.h"

#include <cbdebugger_interfaces.h>
#include <cbplugin.h>
#include <cbeditor.h>
#include <editormanager.h>
#include <logmanager.h>
#include <infowindow.h>
#include <loggers.h>

#include "cmd_result_parser.h"
#include "frame.h"
#include "updated_variable.h"
#include "helpers.h"
#include "parsewatchvalue.h" // (ph 26/05/19)
#include "wx/chartype.h"

namespace dbg_mi
{

// ----------------------------------------------------------------------------
void BreakpointAddAction::OnStart()
// ----------------------------------------------------------------------------
{
    wxString cmd = wxEmptyString;
    if (m_breakpoint->GetType() == _T("Code"))
    {
        cmd = _T("-break-insert ");

        if (not m_breakpoint->IsEnabled())
            cmd += _T("-d ");
        if(m_breakpoint->HasCondition())
            cmd += _T("-c \"") + m_breakpoint->GetCondition() + _T("\" ");
        if(m_breakpoint->HasIgnoreCount())
            cmd += _T("-i ") + wxString::Format(_T("%d "), m_breakpoint->GetIgnoreCount());

        cmd += wxString::Format(_T("-f %s:%d"), m_breakpoint->GetLocation().c_str(), m_breakpoint->GetLine());
    }
    else if (m_breakpoint->GetType() == _T("Data"))
    {
        //-if (not m_breakpoint->IsEnabled())
        //-    ;//return; causes gdb/mi to hang waiting for response

        cmd = _T("-break-watch ");
        if (m_breakpoint->GetBreakOnDataWrite() and m_breakpoint->GetBreakOnDataRead())
            cmd += _T("-a ");
        if (m_breakpoint->GetBreakOnDataRead())
            cmd += _T("-r ");
        if (m_breakpoint->GetBreakOnDataWrite())
            cmd.Append( _T(" ")); //neither -a or -r means a write breakpoint
        cmd += wxString::Format(_T("%s "), m_breakpoint->GetDataAddress().c_str());
    }
    else return;

    //    // **Debugging** //(ph 2024/04/13) causing an error to test
    //    if (cmd.Contains("main.cpp:15"))
    //        cmd.Replace(":15", ":99");

    m_initial_cmd = Execute(cmd);
    m_logger.Debug(_T("BreakpointAddAction::m_initial_cmd = ") + m_initial_cmd.ToString());
}

// ----------------------------------------------------------------------------
void BreakpointAddAction::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    m_logger.Debug(_T("BreakpointAddAction::OnCommandResult: ") + id.ToString());

    if(m_initial_cmd == id)
    {
        bool finish = true;
        const ResultValue &value = result.GetResultValue();
        if (result.GetResultClass() == ResultParser::ClassDone)
        {
            //const ResultValue *number = value.GetTupleValue(_T("bkpt.number"));
            const ResultValue* number = 0;
            if (m_breakpoint->GetType() == _T("Code"))
                number = value.GetTupleValue(_T("bkpt.number"));
            if (m_breakpoint->GetType() == _T("Data"))
            {
                if (m_breakpoint->GetBreakOnDataWrite())
                    number = value.GetTupleValue(_T("wpt.number"));
                if (m_breakpoint->GetBreakOnDataRead())
                    number= value.GetTupleValue(_T("hw-rwpt.number"));
            }

            if(number)
            {
                const wxString &number_value = number->GetSimpleValue();
                long n;
                if(number_value.ToLong(&n, 10))
                {
                    m_logger.Debug(wxString::Format(_T("BreakpointAddAction::breakpoint index is %ld"), n));
                    m_breakpoint->SetIndex(n);

                    if(not m_breakpoint->IsEnabled())
                    {
                        m_disable_cmd = Execute(wxString::Format(_T("-break-disable %d"), n));
                        finish = false;
                    }
                }
                else
                    m_logger.Debug(_T("BreakpointAddAction::error getting the index :( "));
            }
            else
            {
                m_logger.Debug(_T("BreakpointAddAction::error getting number value:( "));
                m_logger.Debug(value.MakeDebugString());
            }
        }
        else if (result.GetResultClass() == ResultParser::ClassError)
        {
            wxString message;
            if (Lookup(value, _T("msg"), message))
            {
                // remove rejected breakpoint
                wxString lineInfo(wxString::Format(_T(" %s:%d"), m_breakpoint->GetLocation().c_str(), m_breakpoint->GetLine()));
                message.Append(lineInfo);
                if (not message.StartsWith(_T("Already have breakpoint")))
                    RemoveBreakpoint(m_breakpoint->GetLocation(), m_breakpoint->GetLine());
                m_logger.Log(message, Logger::Log::Error);
            }
        }

        if (finish)
        {
            m_logger.Debug(_T("BreakpointAddAction::Finishing1"));
            Finish();
        }
    }
    else if(m_disable_cmd == id)
    {
        m_logger.Debug(_T("BreakpointAddAction::Finishing2"));
        Finish();
    }
    // Show changes in breakpoint window
    Manager::Get()->GetDebuggerManager()->GetBreakpointDialog()->Reload(); //(pecan 2012/12/27)
}
// ----------------------------------------------------------------------------
void BreakpointAddAction::RemoveBreakpoint(wxString filename, int lineNo)
// ----------------------------------------------------------------------------
{
    // remove rejected breakpoint
    cbBreakpointsDlg *dlg = Manager::Get()->GetDebuggerManager()->GetBreakpointDialog();
    cbDebuggerPlugin* plugin = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();

    if (dlg)
        dlg->RemoveBreakpoint(plugin, filename, lineNo);
    cbEditor *ed = Manager::Get()->GetEditorManager()->IsBuiltinOpen(m_breakpoint->GetLocation());
    if (ed)
        ed->RefreshBreakpointMarkers();
}//RemoveBreakpoint

// ----------------------------------------------------------------------------
GenerateBacktrace::GenerateBacktrace(SwitchToFrameInvoker *switch_to_frame, BacktraceContainer &backtrace,
                                     CurrentFrame &current_frame, Logger &logger)
// ----------------------------------------------------------------------------
  : m_switch_to_frame(switch_to_frame),
    m_backtrace(backtrace),
    m_logger(logger),
    m_current_frame(current_frame),
    m_first_valid(-1),
    m_old_active_frame(-1),
    m_parsed_backtrace(false),
    m_parsed_args(false),
    m_parsed_frame_info(false)
{
    //void implementation
}

// ----------------------------------------------------------------------------
GenerateBacktrace::~GenerateBacktrace()
// ----------------------------------------------------------------------------
{
    delete m_switch_to_frame;
}

// ----------------------------------------------------------------------------
void GenerateBacktrace::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    if(id == m_backtrace_id)
    {
        ResultValue const *stack = result.GetResultValue().GetTupleValue(_T("stack"));
        if(not stack)
            m_logger.Debug(_T("GenerateBacktrace::OnCommandOutput: no stack tuple in the output"));
        else
        {
            int size = stack->GetTupleSize();
            m_logger.Debug(wxString::Format(_T("GenerateBacktrace::OnCommandOutput: tuple size %d %s"),
                                            size, stack->MakeDebugString().c_str()));

            m_backtrace.clear();

            for(int ii = 0; ii < size; ++ii)
            {
                ResultValue const *frame_value = stack->GetTupleValueByIndex(ii);
                assert(frame_value);
                Frame frame;
                if(frame.ParseFrame(*frame_value))
                {
                    cbStackFrame s;
                    if(frame.HasValidSource())
                        s.SetFile(frame.GetFilename(), wxString::Format(_T("%d"), frame.GetLine()));
                    else
                        s.SetFile(frame.GetFrom(), wxEmptyString);
                    s.SetSymbol(frame.GetFunction());
                    s.SetNumber(ii);
                    s.SetAddress(frame.GetAddress());
                    s.MakeValid(frame.HasValidSource());
                    if(s.IsValid() && m_first_valid == -1)
                        m_first_valid = ii;

                    m_backtrace.push_back(cbStackFrame::Pointer(new cbStackFrame(s)));
                }
                else
                    m_logger.Debug(_T("can't parse frame: ") + frame_value->MakeDebugString());
            }
        }
        m_parsed_backtrace = true;
    }
    else if(id == m_args_id)
    {
        m_logger.Debug(_T("GenerateBacktrace::OnCommandOutput arguments"));
        dbg_mi::FrameArguments arguments;

        if(not arguments.Attach(result.GetResultValue()))
        {
            m_logger.Debug(_T("GenerateBacktrace::OnCommandOutput: can't attach to output of command: ")
                           + id.ToString());
        }
        else if(arguments.GetCount() != static_cast<int>(m_backtrace.size()))
        {
            m_logger.Debug(_T("GenerateBacktrace::OnCommandOutput: stack arg count differ from the number of frames"));
        }
        else
        {
            int size = arguments.GetCount();
            for(int ii = 0; ii < size; ++ii)
            {
                wxString args;
                if(arguments.GetFrame(ii, args))
                    m_backtrace[ii]->SetSymbol(m_backtrace[ii]->GetSymbol() + _T("(") + args + _T(")"));
                else
                {
                    m_logger.Debug(wxString::Format(_T("GenerateBacktrace::OnCommandOutput: ")
                                                    _T("can't get args for frame %d"),
                                                    ii));
                }
            }
        }
        m_parsed_args = true;
    }
    else if (id == m_frame_info_id)
    {
        m_parsed_frame_info = true;

        //^done,frame={level="0",addr="0x0000000000401060",func="main",
        //file="/path/main.cpp",fullname="/path/main.cpp",line="80"}
        if (result.GetResultClass() != ResultParser::ClassDone)
        {
            m_old_active_frame = 0;
            m_logger.Debug(_T("Wrong result class, using default value!"));
        }
        else
        {
            if (not Lookup(result.GetResultValue(), _T("frame.level"), m_old_active_frame))
                m_old_active_frame = 0;
        }
    }

    if(m_parsed_backtrace && m_parsed_args && m_parsed_frame_info)
    {
        if (not m_backtrace.empty())
        {
            int frame = m_current_frame.GetUserSelectedFrame();
            if (frame < 0 && cbDebuggerCommonConfig::GetFlag(cbDebuggerCommonConfig::AutoSwitchFrame))
                frame = m_first_valid;
            if (frame < 0)
                frame = 0;

            m_current_frame.SetFrame(frame);
            int number = m_backtrace.empty() ? 0 : m_backtrace[frame]->GetNumber();
            if (m_old_active_frame != number)
                m_switch_to_frame->Invoke(number);
        }

        Manager::Get()->GetDebuggerManager()->GetBacktraceDialog()->Reload();
        Finish();
    }
}
// ----------------------------------------------------------------------------
void GenerateBacktrace::OnStart()
// ----------------------------------------------------------------------------
{
    m_frame_info_id = Execute(_T("-stack-info-frame"));
    m_backtrace_id = Execute(_T("-stack-list-frames 0 30"));
    m_args_id = Execute(_T("-stack-list-arguments 1 0 30"));
}

// ----------------------------------------------------------------------------
GenerateThreadsList::GenerateThreadsList(ThreadsContainer &threads, int current_thread_id, Logger &logger) :
// ----------------------------------------------------------------------------
    m_threads(threads),
    m_logger(logger),
    m_current_thread_id(current_thread_id)
{
}

// ----------------------------------------------------------------------------
void GenerateThreadsList::OnCommandOutput(CommandID const & /*id*/, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    Finish();
    m_threads.clear();

    int current_thread_id = 0;
    if(not Lookup(result.GetResultValue(), _T("current-thread-id"), current_thread_id))
    {
        m_logger.Debug(_T("GenerateThreadsList::OnCommandOutput - no current thread id"));
        return;
    }

    ResultValue const *threads = result.GetResultValue().GetTupleValue(_T("threads"));
    if(not threads || (threads->GetType() != ResultValue::Tuple && threads->GetType() != ResultValue::Array))
    {
        m_logger.Debug(_T("GenerateThreadsList::OnCommandOutput - no threads"));
        return;
    }
    int count = threads->GetTupleSize();
    for(int ii = 0; ii < count; ++ii)
    {
        ResultValue const &thread_value = *threads->GetTupleValueByIndex(ii);

        int thread_id;
        if(not Lookup(thread_value, _T("id"), thread_id))
            continue;

        wxString info;
        if(not Lookup(thread_value, _T("target-id"), info))
            info = wxEmptyString;

        ResultValue const *frame_value = thread_value.GetTupleValue(_T("frame"));

        if(frame_value)
        {
            wxString str;

            if(Lookup(*frame_value, _T("addr"), str))
                info += _T(" ") + str;
            if(Lookup(*frame_value, _T("func"), str))
            {
                info += _T(" ") + str;

                if(FrameArguments::ParseFrame(*frame_value, str))
                    info += _T("(") + str + _T(")");
                else
                    info += _T("()");
            }

            int line;

            if(Lookup(*frame_value, _T("file"), str) && Lookup(*frame_value, _T("line"), line))
            {
                info += wxString::Format(_T(" in %s:%d"), str.c_str(), line);
            }
            else if(Lookup(*frame_value, _T("from"), str))
                info += _T(" in ") + str;
        }

        m_threads.push_back(cbThread::Pointer(new cbThread(thread_id == current_thread_id, thread_id, info)));
    }

    Manager::Get()->GetDebuggerManager()->GetThreadsDialog()->Reload();
}

// ----------------------------------------------------------------------------
void GenerateThreadsList::OnStart()
// ----------------------------------------------------------------------------
{
    Execute(_T("-thread-info"));
}

// ----------------------------------------------------------------------------
void ParseWatchInfo(ResultValue const &value, int &children_count, bool &dynamic, bool &has_more)
// ----------------------------------------------------------------------------
{
    dynamic = has_more = false;

    int temp;
    if (Lookup(value, _T("dynamic"), temp))
        dynamic = (temp == 1);
    if (Lookup(value, _T("has_more"), temp))
        has_more = (temp == 1);

    if(not Lookup(value, _T("numchild"), children_count))
        children_count = -1;
}

// ----------------------------------------------------------------------------
void ParseWatchValueID(Watch &watch, ResultValue const &value)
// ----------------------------------------------------------------------------
{
    wxString s;
    if(Lookup(value, _T("name"), s))
        watch.SetID(s);

    if(Lookup(value, _T("value"), s))
        watch.SetValue(s);

    if(Lookup(value, _T("type"), s))
        watch.SetType(s);
}

// ----------------------------------------------------------------------------
bool WatchHasType(ResultValue const &value)
// ----------------------------------------------------------------------------
{
    wxString s;
    return Lookup(value, _T("type"), s);
}

// ----------------------------------------------------------------------------
void AppendNullChild(Watch::Pointer watch)
// ----------------------------------------------------------------------------
{
    cbWatch::AddChild(watch, cbWatch::Pointer(new Watch(_T("updating..."), watch->ForTooltip(), watch->GetProject())));
}

// ----------------------------------------------------------------------------
Watch::Pointer AddChild(Watch::Pointer parent, ResultValue const &child_value, wxString const &symbol,
                        WatchesContainer &watches)
// ----------------------------------------------------------------------------
{
    wxString id;
    if(not Lookup(child_value, _T("name"), id))
        return Watch::Pointer();

    Watch::Pointer child = FindWatch(id, watches);
    if(child)
    {
        wxString s;
        if(Lookup(child_value, _T("value"), s))
            child->SetValue(s);

        if(Lookup(child_value, _T("type"), s))
            child->SetType(s);
    }
    else
    {
        child = Watch::Pointer(new Watch(symbol, parent->ForTooltip(), parent->GetProject()));
        ParseWatchValueID(*child, child_value);
        cbWatch::AddChild(parent, child);
        child->ConvertValueToUserFormat();
    }

    child->MarkAsRemoved(false);
    return child;
}
// ----------------------------------------------------------------------------
void UpdateWatches(dbg_mi::Logger &logger)
// ----------------------------------------------------------------------------
{
    // all calls are from actions.cpp
    // Issues event to watchedlg.cpp to update watches grid.

    logger.Debug(_T("actions.cpp UpdateWatches()"));

    cbDebuggerPlugin* pPlugin = (cbDebuggerPlugin*)Manager::Get()->GetPluginManager()->FindPluginByName("debugger_gdbmi");
    if (not pPlugin)
    {
        // This is probably unnecessary, because, Hello!,  we're here !!
        logger.Debug(_T("UpdateWatches() could not obtain address of debugger_gdbmi"));
        return;
    }

    if (not GetWatchesContainer().empty())
    {
        std::vector<cb::shared_ptr<dbg_mi::Watch>> watches;
        watches.reserve(GetWatchesContainer().size()); // Reserve space for efficiency

        cbDebuggerPlugin::DebugWindows windowToUpdate = cbDebuggerPlugin::Watches;
        CodeBlocksEvent event(cbEVT_DEBUGGER_UPDATED);
        event.SetInt(int(windowToUpdate));
        event.SetPlugin((cbPlugin*)GetPluginPtr());
        Manager::Get()->ProcessEvent(event);


    }
}

// ----------------------------------------------------------------------------
void UpdateWatchesTooltipOrAll(const dbg_mi::Watch::Pointer &watch, dbg_mi::Logger& logger)
// ----------------------------------------------------------------------------
{
    if (watch->ForTooltip())
    {
        logger.Debug(_T("updating tooltip watch"));
        wxString type, symbol; wxUnusedVar(symbol);
        watch->GetType(type);
        Manager::Get()->GetDebuggerManager()->GetInterfaceFactory()->UpdateValueTooltip();
    }
    else
        UpdateWatches(logger); //calls update watches dlg via event cbEVT_DEBUGGER_UPDATED
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
WatchBaseAction::WatchBaseAction(WatchesContainer &watches, Logger &logger) :
    // ----------------------------------------------------------------------------
    m_watches(watches),
    m_logger(logger),
    m_sub_commands_left(0),
    m_start(-1),
    m_end(-1)
{
}

WatchBaseAction::~WatchBaseAction()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
bool WatchBaseAction::ParseListCommand(CommandID const &id, ResultValue const &value, Watch::Pointer watch, int &out_commands_launched)
// ----------------------------------------------------------------------------
{
    // Initialize the command counter tracking for this pass
    out_commands_launched = 0;

    // Note: watch is null when iterating all watches vs. using a single watch  // (ph 26/06/19)
    wxString watchSymbol;
    if (watch) watch->GetSymbol(watchSymbol);

    bool error = false;
    m_logger.Debug(_T("WatchBaseAction::ParseListCommand - steplistchildren for id: ")
                   + id.ToString() + _T(" -> ") + value.MakeDebugString());

    // 1. DO NOT traverse to the root. Use the exact watch node passed to the function.
    if (watch)
    {
        auto derived_watch = std::dynamic_pointer_cast<dbg_mi::Watch>(watch);
        if (derived_watch)
        {
            m_parent_map[id] = derived_watch;
        }
        else
        {
            m_logger.Debug(_T("WatchBaseAction::ParseListCommand - Error: Target watch is not a dbg_mi::Watch instance."));
            error = true;
        }
    }

    ListCommandParentMap::iterator it = m_parent_map.find(id);
    if(it == m_parent_map.end() || !it->second)
    {
        m_logger.Debug(_T("WatchBaseAction::ParseListCommand - no parent mapped for command id: ") + id.ToString() + " " + watchSymbol);
        return false;
    }

    ResultValue const *children = value.GetTupleValue(_T("children"));
    if(children)
    {
        int count = children->GetTupleSize();

        m_logger.Debug(wxString::Format(_T("WatchBaseAction::ParseListCommand - children %d"), count));
        Watch::Pointer parent_watch = it->second;

        for(int ii = 0; ii < count; ++ii)
        {
            ResultValue const *child_value;
            child_value = children->GetTupleValueByIndex(ii);

            if(child_value->GetName() == _T("child"))
            {
                wxString symbol;
                if(not Lookup(*child_value, _T("exp"), symbol))
                    symbol = _T("--unknown--");

                Watch::Pointer child;
                bool dynamic, has_more;

                int children_count;
                ParseWatchInfo(*child_value, children_count, dynamic, has_more);

                // --- SANITY GUARD 1: Corrupt / Massive Child Count Protection ---
                const int MAX_ALLOWED_CHILDREN = 50; //was 5000 // (ph 26/06/20)
                if (children_count > MAX_ALLOWED_CHILDREN)
                {
                    wxString errMsg = wxString::Format(_T("ERROR: Corrupt or excessive child count detected (%d) for symbol %s. Capping to 0 leaf node."),
                                                    children_count, symbol.c_str());
                    m_logger.Debug(errMsg,Logger::Line::Error);

                    children_count = 0; // Prevent GDB memory corruption cascades
                    has_more = false;
                    dynamic = false;
                }

                if(dynamic && has_more)
                {
                    child = Watch::Pointer(new Watch(symbol, parent_watch->ForTooltip(), parent_watch->GetProject()));
                    ParseWatchValueID(*child, *child_value);

                    ExecuteListCommand(child, parent_watch);
                    ++out_commands_launched; // Track async command lifecycle
                }
                else
                {
                    switch(children_count)
                    {
                    case -1:
                        error = true;
                        break;
                    case 0:
                        if(not parent_watch->HasBeenExpanded())
                        {
                            parent_watch->SetHasBeenExpanded(true);
                            parent_watch->RemoveChildren();
                        }
                        child = AddChild(parent_watch, *child_value, symbol, m_watches);
                        if (dynamic)
                        {
                            wxString id;
                            if(Lookup(*child_value, _T("name"), id))
                            {
                                ExecuteListCommand(id, child);
                                ++out_commands_launched; // Track async command lifecycle
                            }
                        }
                        child = Watch::Pointer();
                        break;
                    default:
                        if(WatchHasType(*child_value))
                        {
                            if(not parent_watch->HasBeenExpanded())
                            {
                                parent_watch->SetHasBeenExpanded(true);
                                parent_watch->RemoveChildren();
                            }
                            child = AddChild(parent_watch, *child_value, symbol, m_watches);

                            // --- SANITY GUARD: Null Pointer Safety ---
                            if (child != nullptr)
                            {
                                AppendNullChild(child); //sets value to "updating..."

                                m_logger.Debug(_T("WatchBaseAction::ParseListCommand - adding child ")
                                               + child->GetDebugString()
                                               + _T(" to ") + parent_watch->GetDebugString());
                            }
                            else
                            {
                                m_logger.Debug(_T("WatchBaseAction::ParseListCommand - Error: AddChild returned nullptr for ") + symbol);
                            }
                            child = Watch::Pointer();
                        }
                        else
                        {
                            wxString id;
                            if(Lookup(*child_value, _T("name"), id))
                            {
                                ExecuteListCommand(id, parent_watch);
                                ++out_commands_launched; // Track async command lifecycle
                            }
                        }
                    }
                }
            }
            else
            {
                m_logger.Debug(_T("WatchBaseAction::ParseListCommand - can't find child in ")
                               + children->GetTupleValueByIndex(ii)->MakeDebugString());
            }
        }
        parent_watch->RemoveMarkedChildren();
    }
    return !error;
}

// ----------------------------------------------------------------------------
void WatchBaseAction::ExecuteListCommand(Watch::Pointer watch, Watch::Pointer parent)
// ----------------------------------------------------------------------------
{
    CommandID id;

    if(m_start > -1 && m_end > -1)
    {
        id = Execute(wxString::Format(_T("-var-list-children 2 \"%s\" %d %d "),
                                      watch->GetID().c_str(), m_start, m_end));
    }
    else
        id = Execute(wxString::Format(_T("-var-list-children 2 \"%s\""), watch->GetID().c_str()));

    m_parent_map[id] = parent ? parent : watch;
    ++m_sub_commands_left;
}

// ----------------------------------------------------------------------------
void WatchBaseAction::ExecuteListCommand(wxString const &watch_id, Watch::Pointer parent)
// ----------------------------------------------------------------------------
{
    if (not parent)
    {
        m_logger.Debug(_T("Parent for '") + watch_id + _T("' is NULL; skipping this watch"));
        return;
    }
    CommandID id;

    if(m_start > -1 && m_end > -1)
        id = Execute(wxString::Format(_T("-var-list-children 2 \"%s\" %d %d "), watch_id.c_str(), m_start, m_end));
    else
        id = Execute(wxString::Format(_T("-var-list-children 2 \"%s\""), watch_id.c_str()));

    m_parent_map[id] = parent;
    ++m_sub_commands_left;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
WatchCreateAction::WatchCreateAction(Watch::Pointer const &watch, WatchesContainer &watches, Logger &logger) :
    // ----------------------------------------------------------------------------
    WatchBaseAction(watches, logger),
    m_watch(watch),
    m_step(StepCreate)
{
}

// ----------------------------------------------------------------------------
void WatchCreateAction::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    wxString debugString = result.MakeDebugString();
    debugString.Replace("\n", "\\n");
    m_logger.Debug(_T("WatchCreateAction::OnCommandOutput()" + debugString));

    --m_sub_commands_left;

    m_logger.Debug(_T("WatchCreateAction::OnCommandOutput - processing command ") + id.ToString());
    bool error = false;

    if(result.GetResultClass() == ResultParser::ClassDone)
    {
        m_logger.Debug(_T("WatchCreateAction::Output - result.ClassDone"));

        ResultValue const &value = result.GetResultValue();
        switch(m_step)
        {
        case StepCheckExpr: //step 0
            {
                 m_logger.Debug(_T("WatchCreateAction::Output - case:StepCheckExpr"));
                // if the "-data-evaluate-expression sizeof(%s)" result is valid, proceed with -var-create
                wxString symbol;
                m_watch->GetSymbol(symbol);
                symbol.Replace(_T("\""), _T("\\\""));

                wxString cmd = wxString::Format(_T("-var-create - @ \"%s\""), symbol.c_str());
                m_logger.Debug(_T("WatchCreateAction::OnCommandOutput() issued: ")+cmd);
                Execute(wxString::Format(_T("-var-create - @ \"%s\""), symbol.c_str()));
                m_step = StepCreate;
                m_sub_commands_left = 1; // expecting another response
            }
            break;

            case StepCreate: //step 1
            {
                m_logger.Debug(_T("WatchCreateAction::Output - case:StepCreate"));
                bool dynamic, has_more;
                int children;
                ParseWatchInfo(value, children, dynamic, has_more);
                ParseWatchValueID(*m_watch, value);

                if(dynamic && has_more)
                {
                    m_logger.Debug(_T("WatchCreateAction::Output - StepCreate:dynamic && has_more"));
                    m_step = StepSetRange;

                    wxString cmd = _T("-var-set-update-range \"") + m_watch->GetID() + _T("\" 0 100") ;
                    m_logger.Debug(_T("WatchCreateAction::OnCommandOutput() issued: ")+cmd);

                    Execute(_T("-var-set-update-range \"") + m_watch->GetID() + _T("\" 0 100"));
                    AppendNullChild(m_watch);
                    m_sub_commands_left = 1; // Keep action alive for range response
                }
                else if(children > 0)
                {
                    m_logger.Debug(_T("WatchCreateAction::Output - StepCreate:children>0"));

                    // 1. Shift the state machine step
                    m_step = StepListChildren;

                    // 2. TELL GDB TO LIST THE CHILDREN NOW
                    wxString cmd = _T("-var-list-children \"") + m_watch->GetID() + _T("\"");
                    m_logger.Debug(_T("WatchCreateAction::OnCommandOutput() issued: ") + cmd);

                    Execute(_T("-var-list-children \"") + m_watch->GetID() + _T("\""));

                    // 3. Tell the action framework to expect 1 more response from GDB
                    m_sub_commands_left = 1;

                    AppendNullChild(m_watch);
                }
                else
                {
                    m_logger.Debug(_T("WatchCreateAction::Output - StepCreate:elseFinish()"));
                    Finish();
                }

                if ( (not m_watch->IsAutoUpdateEnabled()) && (not m_watch->GetID().empty()) )
                {
                    m_logger.Debug(_T("WatchCreateAction::Output - StepCreate:var-set-frozen"));
                    // wxString cmd = _T("-var-set-frozen ") + m_watch->GetID() + _T(" 1"); // unused
                    Execute(_T("-var-set-frozen ") + m_watch->GetID() + _T(" 1"));
                    /// WARNING: This launches an asynchronous GDB command,
                    // but m_sub_commands_left is not incremented to account for its response!
                    // See note 1: at end of page.
                }
            }
            break;

        case StepListChildren: //step 2
            {
                m_logger.Debug(_T("WatchCreateAction::Output - case:StepListChiildren"));

                int new_sub_commands = 0;
                error = (not ParseListCommand(id, value, m_watch, new_sub_commands));

                m_sub_commands_left = 0; // (ph 26/06/19) let ExpandWatch() handle new_sub_commands
                break;
            }
        case StepSetRange:  //step 3
            {
                m_logger.Debug(_T("WatchCreateAction::Output - case:StepSetRange"));
                break;
            }
        }//end switch
    }
    else
    {
        if(result.GetResultClass() == ResultParser::ClassError)
        {
            m_logger.Debug(_T("WatchCreateAction::Output - result ClassError"));

            if (m_step == StepCheckExpr)
            {
                // Expression invalid, don't attempt -var-create
                m_watch->SetValue(_T("The expression can't be evaluated"));
            }
            else
            {
                m_logger.Debug(_T("WatchCreateAction::Output - result Error creating watch"));

                m_watch->SetValue(_T("Error creating watch"));
            }
        }
        error = true;
        const ResultValue &value = result.GetResultValue();
        wxString message;
        if (Lookup(value, _T("msg"), message))
        {
            // Handle debugger message here if needed
            m_logger.Debug(_T("WatchCreateAction::Output - ") + message);
        }
        else
        {
            m_logger.Debug(_T("WatchCreateAction::Output - error:unknown result)"));

        }
    }

    if(error)
    {
        m_logger.Debug(_T("WatchCreateAction::OnCommandOutput - error in command ") + id.ToString());
        UpdateWatches(m_logger);
        Finish();
    }
    else if(m_sub_commands_left == 0)
    {
        m_logger.Debug(_T("WatchCreateAction::Output - finished ") + id.ToString());
        UpdateWatches(m_logger);
        Finish();
    }

    if (result.GetParseError())
    {
        m_logger.Debug(_T("WatchCreateAction::Output - parse error ") + id.ToString());
        m_watch->SetValue(_T("Malformed debugger response"));
        UpdateWatches(m_logger);
        Finish();
    }

    if (Finished()) m_logger.Debug(_T("WatchCreateAction Finished ") + id.ToString());
    else if (m_sub_commands_left) m_logger.Debug(wxString::Format(_T("WatchCreateAction exited %s step:%d"), id.ToString(), m_step));
    else m_logger.Debug(_T("Error: WatchCreateAction exited without Finish() ") + id.ToString());
}

// ----------------------------------------------------------------------------
void WatchCreateAction::OnStart()
// ----------------------------------------------------------------------------
{
    m_logger.Debug(_T("WatchCreateAction::OnStart()"));

    wxString symbol;
    m_watch->GetSymbol(symbol);
    symbol.Replace(_T("\""), _T("\\\""));

    // First check expression validity using "sizeof(var)"
    Execute(wxString::Format(_T("-data-evaluate-expression sizeof(%s)"), symbol.c_str()));
    m_step = StepCheckExpr;
    m_sub_commands_left = 1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
WatchCreateTooltipAction::~WatchCreateTooltipAction()
// ----------------------------------------------------------------------------
{
    //if (m_watch->ForTooltip())
    //    Manager::Get()->GetDebuggerManager()->GetInterfaceFactory()->ShowValueTooltip(m_watch, m_rect);

    if (m_watch->ForTooltip())
    {
        bool shown = Manager::Get()->GetDebuggerManager()->GetInterfaceFactory()->ShowValueTooltip(m_watch, m_rect);
        if (not shown)
        {
            Manager::Get()->GetDebuggerManager()->GetActiveDebugger()->DeleteWatch(m_watch);
            //-Manager::Get()->GetDebuggerManager()->GetInterfaceFactory()->SetAwaitingDbgResponse(false);
        }
    }
}
///// WatchesUpdateAction //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
WatchesUpdateAction::WatchesUpdateAction(WatchesContainer &watches, Logger &logger) :
    WatchBaseAction(watches, logger)
// ----------------------------------------------------------------------------
{
    m_watch = std::shared_ptr<Watch>(); //assign null ptr; //(ICC 2016/05/1)
}

// ----------------------------------------------------------------------------
WatchesUpdateAction::WatchesUpdateAction(WatchesContainer &watches, Logger &logger, Watch::Pointer singleWatch) : //(ICC 2016/05/1) single watch) :
    WatchBaseAction(watches, logger)
// ----------------------------------------------------------------------------
{
    m_watch = singleWatch;
    m_isSingleWatch = true;
}

// ----------------------------------------------------------------------------
void WatchesUpdateAction::OnStart()
// ----------------------------------------------------------------------------
{
    wxString cmd;

    //Check for single variable update
    if (m_watch)
    {
        // single watch update request
        wxString id = m_watch->GetID();
        if (not id.empty())
        {
            if (m_watch)    //single watch update
                cmd = wxString::Format(_T("-var-update 1 %s"),id.wx_str());
            m_update_command = Execute(cmd);
        }
        else    // watch has no GDB var id yet
        {
            m_logger.Debug("WatchesUpdateAction::OnStart - update request for watch with no GDB id.", Logger::Line::Error);
            Finish();
            return;
        }
    }
    else // watch is null
    {
        //m_watch is null when iterating all watches
        cmd = "-var-update 1 *"; // list/print all changed vars
        m_update_command = Execute(cmd);
    }

    m_sub_commands_left = 1;
}

// ----------------------------------------------------------------------------
void WatchesUpdateAction::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    // **Debugging**
    wxString debugString = result.MakeDebugString();
    debugString.Replace("\n", "\\n"); //show newlines also
    m_logger.Debug(_T("WatchesUpdateAction::OnCommandOutput() result: " + debugString));

    --m_sub_commands_left;

    if(id == m_update_command)
    {
        if (not m_watch) // null m_watch indicates a full iteration of watches
        {
            for(WatchesContainer::iterator it = m_watches.begin();  it != m_watches.end(); ++it)
                (*it)->MarkAsChangedRecursive(false);
        }
        else    // This is a single watch unchanged update
            m_watch->MarkAsChanged(false); // (ph 26/06/18)

        if(not ParseUpdate(result, m_watch))
        {
            // nothing in change list
            Finish();
            return;
        }
    }
    else
    {
        ResultValue const &value = result.GetResultValue();
        /// seems to never execute
        // Declare and initialize the tracking variable to 0 ---

        int new_sub_commands = 0;

        if (not ParseListCommand(id, value, m_watch, new_sub_commands))
        {
            m_logger.Debug(_T("WatchUpdateAction::Output - ParseListCommand failed ") + id.ToString());
            Finish();
            return;
        }

        /// Don't add the new_sub_commands returned from ParseListCommand()
        /// GDB bug is returning value=<trillions> of childs causing infinite loop
        //- m_sub_commands_left += new_sub_commands; // (Gemini 26/06/19)
        // Let ExpandWatch() handle commands to expand children // (ph 26/06/20)

        m_sub_commands_left = 0;
    }

    if(m_sub_commands_left == 0)
    {
        m_logger.Debug(_T("WatchUpdateAction::Output - finishing at") + id.ToString());
        // some watch(s) changed
        UpdateWatches(m_logger);
        Finish();
    }
}
// ----------------------------------------------------------------------------
bool WatchesUpdateAction::ParseUpdate(ResultParser const &result, Watch::Pointer pWatch)
// ----------------------------------------------------------------------------
{
    if(result.GetResultClass() == ResultParser::ClassError)
    {
        Finish();
        return false;
    }
    ResultValue const *list = result.GetResultValue().GetTupleValue(_T("changelist"));
    if(list)
    {
        int count = list->GetTupleSize();
        if (count) {
        for(int ii = 0; ii < count; ++ii)
        {
            ResultValue const *value = list->GetTupleValueByIndex(ii);

            wxString expression;
            if(not Lookup(*value, _T("name"), expression))
            {
                m_logger.Debug(_T("WatchesUpdateAction::Output - no name in ") + value->MakeDebugString());
                continue;
            }

            Watch::Pointer watch = FindWatch(expression, m_watches);
            if(not watch)
            {
                m_logger.Debug(_T("WatchesUpdateAction::Output - can't find watch ") + expression);
                continue;
            }

            UpdatedVariable updated_var;
            if(updated_var.Parse(*value))
            {
                switch(updated_var.GetInScope())
                {
                case UpdatedVariable::InScope_No:
                    watch->Expand(false);
                    //(ICC 2018/07/20) If children are deleted, they are not re-established
                    // when InScope==yes and the changelist is empty.
                    //-watch->RemoveChildren(); //(ICC 2018/07/20)- won't update if deleted
                    watch->SetValue(_T("-- not in scope --"));
                    break;
                case UpdatedVariable::InScope_Invalid:
                    watch->Expand(false);
                    //(ICC 2018/07/20) If children are deleted, they are not re-established
                    // when InScope==yes and the changelist is empty.
                    //-watch->RemoveChildren(); //(ICC 2018/07/20)- won't update if deleted
                    watch->SetValue(_T("-- invalid -- "));
                    break;
                case UpdatedVariable::InScope_Yes:
                    if(updated_var.IsDynamic())
                    {
                        if(updated_var.HasNewNumberOfChildren())
                        {
                            watch->RemoveChildren();
                            if(updated_var.GetNewNumberOfChildren() > 0)
                                ExecuteListCommand(watch);
                        }
                        else if(updated_var.HasMore())
                        {
                            watch->MarkChildsAsRemoved(); // watch->RemoveChildren();
                            ExecuteListCommand(watch);
                        }
                        else if(updated_var.HasValue())
                        {
                            watch->SetValue(updated_var.GetValue());
                            watch->MarkAsChanged(true);
                            // mark the parent changed also
                            if (pWatch)
                                pWatch->MarkAsChanged(true); // (ph 26/06/10)
                        }
                        else
                        {
                            m_logger.Debug(_T("WatchesUpdateAction::Output - unhandled dynamic variable"));
                            m_logger.Debug(_T("WatchesUpdateAction::Output - ") + updated_var.MakeDebugString());
                        }
                    }
                    else
                    {
                        if(updated_var.HasNewNumberOfChildren())
                        {
                            watch->RemoveChildren();

                            if(updated_var.GetNewNumberOfChildren() > 0)
                                ExecuteListCommand(watch);
                        }
                        if(updated_var.HasValue())
                        {
                            watch->SetValue(updated_var.GetValue());
                            watch->MarkAsChanged(true);

                            m_logger.Debug(_T("WatchesUpdateAction::Output - ")
                                           + expression + _T(" = ") + updated_var.GetValue());
                        }
                        else
                        {
                            watch->SetValue(wxEmptyString);
                        }
                    }
                    break;
                }
            }

        }//end for
        }//end if count
    }
    return true;
}

//// WatchEvaluateExpression ///////////////////////////////////////////////
// ----------------------------------------------------------------------------
WatchEvaluateExpression::WatchEvaluateExpression(Watch::Pointer const &watch, WatchesContainer &watches, Logger &logger)
// ----------------------------------------------------------------------------
    :WatchBaseAction(watches, logger),
     m_watch(watch)//,
     //m_step(StepCreate)
{
}

// ----------------------------------------------------------------------------
void WatchEvaluateExpression::OnStart()
// ----------------------------------------------------------------------------
{
    m_logger.Debug(_T("WatchEvaluateExpression::OnStart()"));

    wxString symbol;
    m_watch->GetSymbol(symbol);
    symbol.Replace(_T("\""), _T("\\\""));
    Execute(wxString::Format(_T("-var-evaluate-expression %s"), m_watch->GetID().c_str()));
    m_sub_commands_left = 1;
}

// ----------------------------------------------------------------------------
void WatchEvaluateExpression::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    m_logger.Debug(_T("WatchEvaluateExpression::OnCommandOutput()"));

    m_logger.Debug(_T("WatchEvaluateExpression::OnCommandOutput - processing command ") + id.ToString());
    bool error = false;
    if(result.GetResultClass() == ResultParser::ClassDone)
    {
        ResultValue const &value = result.GetResultValue();
        if (value.GetTupleSize())
        {
            m_watch->MarkAsChanged(true);
            ParseWatchValueID(*m_watch, value);

            Finish();
        }
        else
            error = true;
    }
    else
    {
        if(result.GetResultClass() == ResultParser::ClassError)
            m_watch->SetValue(_T("The expression can't be evaluated"));

        error = true;

    }

    if(error)
    {
        m_logger.Debug(_T("WatchEvaluateExpression::OnCommandOutput - error in command: ") + id.ToString());
        UpdateWatches(m_logger);
        Finish();
    }
    else if(m_sub_commands_left == 0)
    {
        m_logger.Debug(_T("WatchEvaluateExpression::Output - finishing at") + id.ToString());
        UpdateWatches(m_logger);
        Finish();
    }

    // parse error on malformed response
    if (result.GetParseError())
    {
        m_logger.Debug(_T("WatchEvaluateExpression::Output - parse error ") + id.ToString());
        m_watch->SetValue(_T("Malformed debugger response"));
        UpdateWatches(m_logger);
        Finish();
    }// parse error
} // WatchEvaluateExpression

////// EditWatches support
////// ----------------------------------------------------------------------------
////WatchesUpdateFormat::WatchesUpdateFormat(WatchesContainer &watches, Logger &logger) :
////// ----------------------------------------------------------------------------
////    WatchBaseAction(watches, logger)
////{
////}
////
////void WatchesUpdateFormat::OnStart()
////{
////     m_logger.Debug(_T("WatchesUpdateFormat::OnStart"));
////    //Update user requested conversions
////    for(dbg_mi::WatchesContainer::iterator it = m_watches.begin(); it != m_watches.end(); ++it)
////    {
////        dbg_mi::Watch::Pointer wp = (*it);
////        if (wp->HasBeenExpanded())
////        {
////            int knt = wp->GetChildCount();
////            if (0 == knt)
////                wp->ConvertValueToUserFormat();
////            else
////            {
////                for (int ii=0; ii<knt; ++ii)
////                {
////                    dbg_mi::Watch::Pointer child_watch = cb::static_pointer_cast<dbg_mi::Watch>(wp->GetChild(ii));
////                    child_watch->ConvertValueToUserFormat();
////                }
////            }
////        }//if expanded
////        else
////        {
////            wp->ConvertValueToUserFormat();
////        }
////    }
////    Manager::Get()->GetDebuggerManager()->GetWatchesDialog()->UpdateWatches();
////    Finish();
////}
////
////// ----------------------------------------------------------------------------
////bool WatchesUpdateFormat::ParseUpdate(ResultParser const &result)
////// ----------------------------------------------------------------------------
////{
////     m_logger.Debug(_T("WatchesUpdateFormat::ParseUpdate"));
////
////    return true;
////}
////
////// ----------------------------------------------------------------------------
////void WatchesUpdateFormat::OnCommandOutput(CommandID const &id, ResultParser const &result)
////// ----------------------------------------------------------------------------
////{
////    m_logger.Debug(_T("WatchUpdateAction::OnCommandOutput"));
////
////    Finish();
////
////}

// ----------------------------------------------------------------------------
void WatchExpandedAction::OnStart()
// ----------------------------------------------------------------------------
{
    m_logger.Debug(_T("WatchExpandedAction::OnStart() id:" +  m_expanded_watch->GetID()));
    //m_sub_commands_left = 1;

    m_update_id = Execute(_T("-var-update ") + m_expanded_watch->GetID());
    ExecuteListCommand(m_expanded_watch, Watch::Pointer());
}
// EditWatches support

// ----------------------------------------------------------------------------
void WatchExpandedAction::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{

    m_logger.Debug(_T("WatchExpandedAction::OnCommandOutput() id:" +  m_expanded_watch->GetID()));

    // Decrement immediately so that the initial update command is accounted for
    // espcecialy before the return statement.
    //- --m_sub_commands_left;

    if (id == m_update_id)
        return;

    --m_sub_commands_left;

    m_logger.Debug(_T("WatchExpandedAction::Output - ") + result.GetResultValue().MakeDebugString());

    // --- FIX 1: Declare and initialize the tracking variable to 0 ---
    int new_sub_commands = 0;

    //-if(not ParseListCommand(id, result.GetResultValue(), m_expanded_watch)) // (ph 26/06/07)
    if (not ParseListCommand(id, result.GetResultValue(), m_expanded_watch, new_sub_commands)) // (Gemini 26/06/19)
    {
        m_logger.Debug(_T("WatchExpandedAction::Output - error in command ") + id.ToString());
        // Update the watches even if there is an error, so some partial information can be displayed.
        UpdateWatchesTooltipOrAll(m_expanded_watch, m_logger);
        Finish();
        return; // Added return to prevent continuing to evaluate flags after Finish()
    }

    if(m_sub_commands_left == 0)
    {
        m_logger.Debug(_T("WatchExpandedAction::Output - done"));
        UpdateWatchesTooltipOrAll(m_expanded_watch, m_logger);
        Finish();
    }

    // parse error on malformed response
    if (result.GetParseError())
    {
        m_logger.Debug(_T("WatchExpandedAction::Output - parse error ") + id.ToString());
        m_watch->SetValue(_T("Malformed debugger response"));
        // Update the watches even if there is an error, so some partial information can be displayed.
        m_expanded_watch->RemoveChildren(); //remove "updating... message"
        UpdateWatchesTooltipOrAll(m_expanded_watch, m_logger);
        Finish();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
void WatchCollapseAction::OnStart()
// ----------------------------------------------------------------------------
{
    m_logger.Debug(_T("WatchCollapseAction::OnStart() " +  m_collapsed_watch->GetID()));

    Execute(_T("-var-delete -c \"") + m_collapsed_watch->GetID() + "\""); // (ph 26/06/20)
}

// ----------------------------------------------------------------------------
void WatchCollapseAction::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    m_logger.Debug("WatchCollapseAction::OnCommandOutput id:" + id.ToString());
    wxUnusedVar(id);
    if(result.GetResultClass() == ResultParser::ClassDone)
    {
        m_collapsed_watch->SetHasBeenExpanded(false);
        m_collapsed_watch->RemoveChildren();
        AppendNullChild(m_collapsed_watch);
        UpdateWatchesTooltipOrAll(m_collapsed_watch, m_logger);
    }
    Finish();
}

// ----------------------------------------------------------------------------
// implement class InfoRegisters : public Action
// ----------------------------------------------------------------------------
// only tested on mingw/pc/win env
InfoRegisters::InfoRegisters(Logger &logger, wxString disassemblyFlavor) :
            m_logger(logger),
            m_disassemblyFlavor(disassemblyFlavor),
            m_parsed_reg_names(false),
            m_parsed_reg_values(false)
{
}
// ----------------------------------------------------------------------------
InfoRegisters::~InfoRegisters()
// ----------------------------------------------------------------------------
{
}

// ----------------------------------------------------------------------------
void InfoRegisters::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
            // output is a series of:
            //
            // eax            0x40e66666       1088841318
            // ecx            0x40cbf0 4246512
            // edx            0x77c61ae8       2009471720
            // ebx            0x4000   16384
            // esp            0x22ff50 0x22ff50
            // ebp            0x22ff78 0x22ff78
            // esi            0x22ef80 2289536
            // edi            0x5dd3f4 6149108
            // eip            0x4013c9 0x4013c9
            // eflags         0x247    583
            // cs             0x1b     27
            // ss             0x23     35
            // ds             0x23     35
            // es             0x23     35
            // fs             0x3b     59
            // gs             0x0      0

    if (id == m_reg_values_id)
    {
        m_parsed_reg_values = true;

        ResultValue const *regs = result.GetResultValue().GetTupleValue(_T("register-values"));
        if(not regs)
            m_logger.Debug(_("InfoRegisters::OnCommandOutput: no register-values tuple in the output"));
        else
        {
            // or32 register string parser
            if(m_disassemblyFlavor == _T("set disassembly-flavor or32"))
            {
                //-ParseOutputFromOR32gdbPort(output);
            }
            else    // use generic parser - this may work for other platforms or you may have to write your own
            {
                //cbCPURegistersDlg *dialog = Manager::Get()->GetDebuggerManager()->GetCPURegistersDialog();
                int isize = regs->GetTupleSize();
                m_logger.Debug(wxString::Format(_T("InfoRegisters::OnCommandOutput: tuple size %d %s"),
                                                isize, regs->MakeDebugString().c_str()));

                for(int ii = 0; ii < isize; ++ii)
                {
                    const ResultValue* pReg_value = regs->GetTupleValueByIndex(ii);
                    assert(pReg_value);
                    // const ResultValue* pRegNumber = pReg_value->GetTupleValue(_T("number")); // **Debugging**
                    const ResultValue* pRegValue = pReg_value->GetTupleValue(_T("value"));

                    if(pRegValue->GetSimpleValue() != wxEmptyString) //keep in sync with regNames
                    {
                        // wxString reg = pRegNumber->GetSimpleValue(); // **Debugging**
                        // wxString addr = pRegValue->GetSimpleValue(); // **Debugging**
                        regValues.Add( pRegValue->GetSimpleValue());
                    }
                    else
                        m_logger.Debug(_T("can't parse registers: ") + pReg_value->MakeDebugString());
                }

            }//else
        }//else
    }//if m_reg_values_id

    //^done,register-names=["r0","r1","r2","r3","r4","r5","r6","r7",
    // "r8","r9","r10","r11","r12","r13","r14","r15","r16","r17","r18",
    // "r19","r20","r21","r22","r23","r24","r25","r26","r27","r28","r29",
    // "r30","r31","f0","f1","f2","f3","f4","f5","f6","f7","f8","f9",
    // "f10","f11","f12","f13","f14","f15","f16","f17","f18","f19","f20",
    // "f21","f22","f23","f24","f25","f26","f27","f28","f29","f30","f31",
    // "", "pc","ps","cr","lr","ctr","xer"]
    if (id == m_reg_names_id)
    {
        m_parsed_reg_names = true;

        ResultValue const *regs = result.GetResultValue().GetTupleValue(_T("register-names"));
        if(not regs)
            m_logger.Debug(_T("InfoRegisters::OnCommandOutput: no register-names tuple in the output"));
        else
        {
            // or32 register string parser
            if(m_disassemblyFlavor == _("set disassembly-flavor or32"))
            {
                //-ParseOutputFromOR32gdbPort(output);
            }
            else    // use generic parser - this may work for other platforms or you may have to write your own
            {
                int isize = regs->GetTupleSize();
                m_logger.Debug(wxString::Format(_T("InfoRegisters::OnCommandOutput: tuple size %d %s"),
                                                isize, regs->MakeDebugString().c_str()));

                for(int ii = 0; ii < isize; ++ii)
                {
                    const ResultValue* pReg_name = regs->GetTupleValueByIndex(ii);
                    assert(pReg_name);
                    wxString regName = pReg_name->GetSimpleValue();
                    if (regName.empty() or (regName == "\"\""))
                        continue;
                    regNames.Add(pReg_name->GetSimpleValue());
                }
            }//else
        }//else
    }//if m_reg_names_id

    if (m_parsed_reg_names && m_parsed_reg_values)
    {
        cbCPURegistersDlg *dialog = Manager::Get()->GetDebuggerManager()->GetCPURegistersDialog();

        for ( unsigned ii = 0;  (ii < regNames.GetCount()) and ( ii < regValues.GetCount()); ++ii )
        {
            if (regNames[ii].StartsWith("\"xmm"))
            {
                wxString floatVals = regValues[ii];
                int floatnum = 0;
                while (floatVals.Contains("},"))
                {
                    int posn = floatVals.find("},");
                    if (posn >0)
                    {
                        wxString regname = wxString::Format("%s%s%d%s",regNames[ii],"(", floatnum, ")");
                        wxString addr = floatVals.Mid(0, posn+1);
                        if ( regname.Length() )
                            dialog->SetRegisterValue(regname, addr, wxEmptyString);
                        floatVals = floatVals.Mid(posn+3);
                        floatnum +=1;
                    }
                }//endWhile
            }//endif name xmm
            else
            {
                if (regNames[ii].empty() or (regNames[ii] == "\"\""))
                    continue;
                unsigned long int addrL;
                wxString addr = regValues[ii];
                addr.ToULong(&addrL, 16);
                dialog->SetRegisterValue(regNames[ii], addr, wxEmptyString);
            }
        }

        Finish();
    }

}//OnCommandOutput

// ----------------------------------------------------------------------------
void InfoRegisters::OnStart()
// ----------------------------------------------------------------------------
{
    m_reg_names_id = Execute(_T("-data-list-register-names"));
    m_reg_values_id = Execute(_T("-data-list-register-values x"));
}
// ----------------------------------------------------------------------------
// Implement class GenerateDisassembly : public Action
// ----------------------------------------------------------------------------
  // Command to run a disassembly.

// ----------------------------------------------------------------------------
GenerateDisassembly::~GenerateDisassembly()
// ----------------------------------------------------------------------------
{
}

// ----------------------------------------------------------------------------
void GenerateDisassembly::OnStart()
// ----------------------------------------------------------------------------
{
    m_parsed_frame_info = false;
    m_parsed_disassemble_info = false;

    if (not m_Cmd.empty())
    {
        m_frame_info_id = Execute(_T("-stack-info-frame"));
        m_disassemble_info_id = Execute(m_Cmd);
    }
}

// ----------------------------------------------------------------------------
GenerateDisassembly::GenerateDisassembly(Logger &logger, bool MixedMode, wxString hexAddrStr)
// ----------------------------------------------------------------------------
    :  m_logger(logger),
       m_mixedMode(MixedMode)
{
    m_Cmd << _T("-data-disassemble -s");

    if(hexAddrStr.IsEmpty())
        //****NOTE: If this branch is taken, disassembly may not reflect the program's
        //actual current location.  Other areas of code will change the current (stack) frame
        //which results in $pc reflecting the eip(x86-based) of that frame.  After changing to
        //a non-top frame, a request (gdb 7.2 x86) to print either '$pc' or '$eip' will
        //return the same value.
        //So, there seems to be no way to obtain the actual current address in this (non-MI)
        //interface.  Hence, we can't get the correct disassembly (when the $pc does not
        //reflect actual current address.)  GDB itself does continue to step from the correct address, so
        //there may be some other way to obtain it yet to be found.
        m_Cmd << _T(" $pc -e \"$pc + 50\"");
    else
    {   // have a hex start address
        if(_T("0x") == hexAddrStr.Left(2) || _T("0X") == hexAddrStr.Left(2))
            m_Cmd << _T(" ") << hexAddrStr << _T(" -e \"") << hexAddrStr << _T(" + 50\"");
        else
            m_Cmd << _T(" 0x") << hexAddrStr << _T(" -e \"0x") << hexAddrStr << _T(" + 50\"");;
    }
    if(m_mixedMode)
    {
        m_Cmd << _T(" -- 1");
    }
    else
    {
        m_Cmd << _T(" -- 0");
    }

}

//-void GenerateDisassembly::ParseOutput(const wxString& output)
// ----------------------------------------------------------------------------
void GenerateDisassembly::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{

    cbDisassemblyDlg *dialog = Manager::Get()->GetDebuggerManager()->GetDisassemblyDialog();

    if (id == m_frame_info_id)
    {
        m_parsed_frame_info = true;
        //^done,frame={level="0",addr="0x0000000000401060",func="main",
        //file="/path/main.cpp",fullname="/path/main.cpp",line="80"}
        ResultValue const* resultValue = result.GetResultValue().GetTupleValue(_T("frame"));
        if(not resultValue)
        {
            m_logger.Debug(_T("GenerateDisassembly::OnCommandOutput: no frame tuple in the output"));
            Finish();
            return;
        }
        const ResultValue* pAddr = resultValue->GetTupleValue(_T("addr"));
        assert(pAddr);
        const ResultValue* pfunc = resultValue->GetTupleValue(_T("func"));
        assert(pfunc);
        unsigned long int addr;
        pAddr->GetSimpleValue().ToULong(&addr, 16);

        cbStackFrame stackFrame;
        stackFrame.SetSymbol(pfunc->GetSimpleValue());
        stackFrame.SetAddress(addr);
        stackFrame.MakeValid(true);
        dialog->Clear(stackFrame);
    }
    else if (id == m_disassemble_info_id)
    {
        m_parsed_disassemble_info = true;
        ResultValue const* resultValue = result.GetResultValue().GetTupleValue(_T("asm_insns"));
        if(not resultValue)
        {
            m_logger.Debug(_T("GenerateDisassembly::OnCommandOutput: no disassemble tuple in the output"));
            Finish();
            return;
        }
        else
        {
            int knt = resultValue->GetTupleSize();
            m_logger.Debug(wxString::Format(_T("GenerateAssembly::OnCommandOutput: tuple size %d %s"),
                                            knt, resultValue->MakeDebugString().c_str()));


            if ( m_mixedMode ) for ( int ii = 0; ii < knt; ++ii)
            {
                // (gdb)
                //- -data-disassemble -f basics.c -l 32 -n 3 -- 1
                // -data-disassemble -s $pc -e "$pc + 20" -- 0
                // ^done,asm_insns=[
                // src_and_asm_line={line="31",
                // file="/kwikemart/marge/ezannoni/flathead-dev/devo/gdb/
                //   testsuite/gdb.mi/basics.c",line_asm_insn=[
                // {address="0x000107bc",func-name="main",offset="0",
                // inst="save  %sp, -112, %sp"}]},
                // src_and_asm_line={line="32",
                // file="/kwikemart/marge/ezannoni/flathead-dev/devo/gdb/
                //   testsuite/gdb.mi/basics.c",line_asm_insn=[
                // {address="0x000107c0",func-name="main",offset="4",
                // inst="mov  2, %o0"},
                // {address="0x000107c4",func-name="main",offset="8",
                // inst="sethi  %hi(0x11800), %o2"}]}]
                // (gdb)

                const ResultValue* pSrcAsmLine = resultValue->GetTupleValueByIndex(ii);
                assert(pSrcAsmLine);
                const ResultValue* pLineNo = pSrcAsmLine->GetTupleValue(_T("line"));

                assert(pLineNo);
                const ResultValue* pFile = pSrcAsmLine->GetTupleValue(_T("file"));
                assert(pFile);

                const ResultValue* pLine_asm_insn = pSrcAsmLine->GetTupleValue(_T("line_asm_insn"));
                assert(pLine_asm_insn);

                unsigned long int lineno ;
                pLineNo->GetSimpleValue().ToULong(&lineno, 10) ;
                // cb editors are 0 oriented, gdb is 1 oriented
                //-if (lineno > 0) lineno -= 1;
                //dialog->AddSourceLineByFile(lineno, pFile->GetSimpleValue());
                AddSourceLineByFile(lineno, pFile->GetSimpleValue());

                int laiKnt = pLine_asm_insn->GetTupleSize();
                for ( int jj = 0; jj < laiKnt; ++jj)
                {
                    const ResultValue* pLineInsns = pLine_asm_insn->GetTupleValueByIndex(jj);
                    const ResultValue* pAddr = pLineInsns->GetTupleValue(_T("address"));
                    assert(pAddr);
                    const ResultValue* pfunc = pLineInsns->GetTupleValue(_T("func-name"));
                    assert(pfunc);
                    const ResultValue* pOffset = pLineInsns->GetTupleValue(_T("offset"));
                    assert(pOffset);
                    const ResultValue* pInst = pLineInsns->GetTupleValue(_T("inst"));
                    assert(pInst);
                    unsigned long int addr;
                    pAddr->GetSimpleValue().ToULong(&addr, 16);
                    dialog->AddAssemblerLine(addr, pInst->GetSimpleValue());
                }

            }//for mixedMode

            if (not m_mixedMode) for ( int ii = 0; ii < knt; ++ii)
            {
                // (gdb)
                // -data-disassemble -s $pc -e "$pc + 20" -- 0
                // ^done,
                // asm_insns=[
                // {address="0x000107c0",func-name="main",offset="4",
                // inst="mov  2, %o0"},
                // {address="0x000107c4",func-name="main",offset="8",
                // inst="sethi  %hi(0x11800), %o2"},
                // {address="0x000107c8",func-name="main",offset="12",
                // inst="or  %o2, 0x140, %o1\t! 0x11940 <_lib_version+8>"},
                // {address="0x000107cc",func-name="main",offset="16",
                // inst="sethi  %hi(0x11800), %o2"},
                // {address="0x000107d0",func-name="main",offset="20",
                // inst="or  %o2, 0x168, %o4\t! 0x11968 <_lib_version+48>"}]
                // (gdb)

                const ResultValue* pAsmInsns = resultValue->GetTupleValueByIndex(ii);
                assert(pAsmInsns);
                const ResultValue* pAddr = pAsmInsns->GetTupleValue(_T("address"));
                assert(pAddr);
                const ResultValue* pfunc = pAsmInsns->GetTupleValue(_T("func-name"));
                assert(pfunc);
                const ResultValue* pOffset = pAsmInsns->GetTupleValue(_T("offset"));
                assert(pOffset);
                const ResultValue* pInst = pAsmInsns->GetTupleValue(_T("inst"));
                assert(pInst);
                unsigned long int addr;
                pAddr->GetSimpleValue().ToULong(&addr, 16);
                dialog->AddAssemblerLine(addr, pInst->GetSimpleValue());

            }// for not mixedMode

            dialog->CenterCurrentLine();
        }//endElse
    }//endElse if id == m_disassemble_info_id

    if ( m_parsed_frame_info && m_parsed_disassemble_info)
    {
        Finish();
    }

    // parse error on malformed response
    if (result.GetParseError())
    {
        m_logger.Debug(_T("GenerateDisassembly::OnCommandOutput: Malformed response parse error ") + id.ToString());
        Finish();
    }// parse error
}
// ----------------------------------------------------------------------------
// implement class ExamineMemory : public Action
// ----------------------------------------------------------------------------
// only tested on mingw/pc/win env
ExamineMemory::ExamineMemory(Logger &logger) :
            m_logger(logger),
            m_parsed_memory_values(false)
{
    //ctor
}
ExamineMemory::~ExamineMemory()
{
    //dtor
}

void ExamineMemory::OnCommandOutput(CommandID const &id, ResultParser const &result)
{
    // Example:
    // [debug]cmd==>70000000000-data-read-memory "&sStruct_1" x 1 1 32
    // [debug]output==>70000000000^done,addr="0x0028ff18",nr-bytes="32",total-bytes="32",
    //      next-row="0x0028ff38",prev-row="0x0028fef8",next-page="0x0028ff38",
    //      prev-page="0x0028fef8",memory=[{addr="0x0028ff18",data=["0x55","0x00","0x00",
    //      "0x00","0x00","0x00","0x00","0x00","0x00","0x00","0x00","0x00","0x01","0x00",
    //      "0x00","0x00","0xfe","0xff","0xff","0xff","0x62","0x11","0x9b","0x76","0x50",
    //      "0xff","0x28","0x00","0x00","0x00","0x00","0x00"]}]

    if (id == m_memory_values_id)
    {

        m_parsed_memory_values = true;

        const ResultValue* pMemory = result.GetResultValue().GetTupleValue(_T("memory"));
        if(not pMemory)
        {
            m_logger.Debug(_T("ExamineMemory::OnCommandOutput: no 'memory' tuple in the output"));
            Finish();
            return;
        }

        cbExamineMemoryDlg *dialog = Manager::Get()->GetDebuggerManager()->GetExamineMemoryDialog();
        dialog->Begin();
        dialog->Clear();

        int isize = pMemory->GetTupleSize();
        //m_logger.Debug(wxString::Format(_T("ExamineMemory::OnCommandOutput: tuple size %d %s"),
        //                                isize, pMemory->MakeDebugString().c_str()));

        for(int ii = 0; ii < isize; ++ii)
        {
            const ResultValue* pentry = pMemory->GetTupleValueByIndex(ii);
            assert(pentry);
            const ResultValue* pAddr_tuple = pentry->GetTupleValue(_T("addr"));
            assert(pAddr_tuple);
            const ResultValue* pData_tuple =  pentry->GetTupleValue(_T("data"));
            assert(pData_tuple);
            int jsize = pData_tuple->GetTupleSize();

            // The cb Memory display is set up to accept lines of 8 bytes each
            // So we'll report the address of every eight byte
            const int BYTES_TO_REPORT = 8;
            long byteAddr = 0;
            pAddr_tuple->GetSimpleValue().ToLong(&byteAddr,16);
            int bytesPerLine = BYTES_TO_REPORT;
            wxString addrToReport = wxString::Format(_T("%#x"), byteAddr);
            for (int jj = 0; jj<jsize; ++jj)
            {
                const ResultValue* pdata_value = pData_tuple->GetTupleValueByIndex(jj);
                assert(pdata_value);
                wxString hexByte = pdata_value->GetSimpleValue();
                hexByte = hexByte.BeforeLast('\"');
                hexByte = hexByte.AfterFirst('x');
                if (bytesPerLine == 0)
                {   addrToReport = wxString::Format(_T("%#x"), byteAddr);
                    bytesPerLine = BYTES_TO_REPORT;
                    //m_logger.Debug(wxString::Format(_T("addrToReport[%s]"),addrToReport.c_str()));
                }
                dialog->AddHexByte(addrToReport, hexByte);
                bytesPerLine -= 1;
                byteAddr += 1;
            }
        }
        dialog->End();
        Finish();
    }

}//OnCommandOutput

void ExamineMemory::OnStart()
{
    cbExamineMemoryDlg *dialog = Manager::Get()->GetDebuggerManager()->GetExamineMemoryDialog();
    //-size_t nrRows = (dialog->GetBytes())>>2;
    //-if (not nrRows) nrRows += 1;
    m_Cmd.Printf(_T("-data-read-memory \"%s\" x 1 1 %d"), dialog->GetBaseAddress().c_str(), dialog->GetBytes());
    m_memory_values_id = Execute(m_Cmd);

    // -data-read-memory [ -o byte-offset ]
    //   address word-format word-size
    //   nr-rows nr-cols [ aschar ]
    //where:
    //`address'
    //    An expression specifying the address of the first memory word to be read. Complex expressions containing embedded white space should be quoted using the C convention.
    //`word-format'
    //    The format to be used to print the memory words. The notation is the same as for GDB's print command (see section Output formats).
    //`word-size'
    //    The size of each memory word in bytes.
    //`nr-rows'
    //    The number of rows in the output table.
    //`nr-cols'
    //    The number of columns in the output table.
    //`aschar'
    //    If present, indicates that each row should include an ASCII dump. The value of aschar is used as a padding character when a byte is not a member of the printable ASCII character set (printable ASCII characters are those whose code is between 32 and 126, inclusively).
    //`byte-offset'
    //    An offset to add to the address before fetching memory.
    //
    //This command displays memory contents as a table of nr-rows by nr-cols words, each word being word-size bytes. In total, nr-rows * nr-cols * word-size bytes are read (returned as `total-bytes'). Should less then the requested number of bytes be returned by the target, the missing words are identified using `N/A'. The number of bytes read from the target is returned in `nr-bytes' and the starting address used to read memory in `addr'.
    //
    //The address of the next/previous row or page is available in `next-row' and `prev-row', `next-page' and `prev-page'.
    //GDB Command
    //
    //The corresponding GDB command is `x'. gdbtk has `gdb_get_mem' memory read command.

}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Example of output to dialog for disassembly
//0x00401380	mov    -0x30(%ebp),%ecx
//0x00401383	rep stos %al,%es:(%edi)
//0x00401385	lea    -0x24(%ebp),%eax
//0x00401388	mov    %eax,-0x34(%ebp)
//0x0040138B	mov    $0x0,%al
//0x0040138D	movl   $0xc,-0x38(%ebp)
//0x00401394	mov    -0x34(%ebp),%edi
//0x00401397	mov    -0x38(%ebp),%ecx
//0x0040139A	rep stos %al,%es:(%edi)
//0x0040139C	movb   $0x55,-0x18(%ebp)
//0x004013A0	movl   $0x1234,-0x14(%ebp)
//0x004013A7	lea    -0x24(%ebp),%eax
//0x004013AA	mov    %eax,-0x10(%ebp)
//0x004013AD	sub    $0xc,%esp
//0x004013B0	push   $0x403030
//0x004013B5	call   0x401890 <printf>
//0x004013BA	add    $0x10,%esp
//0x004013BD	sub    $0xc,%esp
//0x004013C0	push   $0x1f4
//0x004013C5	call   0x4018d8 <Sleep@4>
//0x004013CA	add    $0xc,%esp
//0x004013CD	jmp    0x4013ad <main+93>



//mixed mode disassembly mi response
//[debug]output==>50000000000
//^done,asm_insns=
//[src_and_asm_line={line="20",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[{address="0x0040139c",func-name="main",offset="76",inst="movb   $0x55,-0x18(%ebp)"}]},
//src_and_asm_line={line="21",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[{address="0x004013a0",func-name="main",offset="80",inst="movl   $0x1234,-0x14(%ebp)"}]},
//src_and_asm_line={line="22",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[{address="0x004013a7",func-name="main",offset="87",inst="lea    -0x24(%ebp),%eax"},
//                   {address="0x004013aa",func-name="main",offset="90",inst="mov    %eax,-0x10(%ebp)"}]},
//src_and_asm_line={line="23",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[]},
//src_and_asm_line={line="24",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[]},src_and_asm_line={line="25",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[]},src_and_asm_line={line="26",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[{address="0x004013ad",func-name="main",offset="93",inst="sub    $0xc,%esp"},
//                   {address="0x004013b0",func-name="main",offset="96",inst="push   $0x403030"},
//                   {address="0x004013b5",func-name="main",offset="101",inst="call   0x401890 <printf>"},
//                   {address="0x004013ba",func-name="main",offset="106",inst="add    $0x10,%esp"}]},
//src_and_asm_line={line="27",file="C:\\Usr\\Proj\\TestProject\\TestProject_IDB\\Tests\\gdbArmMITest\\main.cpp",
//    line_asm_insn=[{address="0x004013bd",func-name="main",offset="109",inst="sub    $0xc,%esp"},
//                   {address="0x004013c0",func-name="main",offset="112",inst="push   $0x1f4"},
//                   {address="0x004013c5",func-name="main",offset="117",inst="call   0x4018d8 <Sleep@4>"},
//                   {address="0x004013ca",func-name="main",offset="122",inst="add    $0xc,%esp"},
//                   {address="0x004013cd",func-name="main",offset="125",inst="jmp    0x4013ad <main+93>"}]}]

// ----------------------------------------------------------------------------
// implement class InfoLocalsAndArgs : public Action
// ----------------------------------------------------------------------------
// only tested on mingw/pc/win env
InfoLocalsAndArgs::InfoLocalsAndArgs(ActionsMap* pActionsMap, int localsOrArgs, cb::shared_ptr<GDBWatch> localsWatch, cb::shared_ptr<GDBWatch> argsWatch)
    :  m_localsWatch(localsWatch),       //a single watch with func args as children
       m_argsWatch(argsWatch),
       m_pActionsMap(pActionsMap),     //a single watch with local vars as children
       m_parsed_result_values(false),
       m_localsOrArgs(localsOrArgs)
{
    //ctor
}
InfoLocalsAndArgs::~InfoLocalsAndArgs()
{
    //dtor
}

// ----------------------------------------------------------------------------
void InfoLocalsAndArgs::OnCommandOutput(CommandID const &id, ResultParser const &result)
// ----------------------------------------------------------------------------
{
    TextCtrlLogger* pLog = Manager::Get()->GetDebuggerManager()->GetLogger();
    pLog->Append(_T("InfoLocalsAndArgs::OnCommandOutput()"));

    wxUnusedVar(id);
    // use: m_localsWatch, m_argsWatch,

    // Set request for locals, args or  both
    bool showVars = m_localsOrArgs & 1;
    bool showArgs = m_localsOrArgs & 2;
    wxUnusedVar(showVars); wxUnusedVar(showArgs); // (ph 26/06/08)

    // **Debugging**
    wxString debugString = result.MakeDebugString();
    debugString.Replace(_T("\n"), _T("\\n")); //show the new lines also
    pLog->Append(_T("InfoLocalsAndArgs::OnCommandOutput() result: " + debugString));

    // 1. Get the "variables" result
    const ResultValue* pVars = result.GetResultValue().GetTupleValue(_T("variables"));
    if (not pVars)
    {
        pLog->Append(_T("InfoLocalsAndArgs: 'variables' key missing"));
        Finish();
        return;
    }

    // 2. Use GetTupleSize() to check if the list is empty
    // This works because GetTupleSize() calls m_value.tuple.size()
    if (pVars->GetTupleSize() == 0)
    {
        pLog->Append(_T("InfoLocalsAndArgs: No variables in this frame"));
        if (m_localsWatch)
            m_localsWatch->RemoveChildren();
        if (m_argsWatch)
            m_argsWatch->RemoveChildren();

        Finish();
        return;
    }

    WatchesContainer m_container;
    if (m_localsWatch)
        m_localsWatch->MarkChildsAsRemoved();
    if (m_argsWatch)
        m_argsWatch->MarkChildsAsRemoved();

    // iterate through the variables
    // to add to locals and function argument
    for (int i = 0; i < pVars->GetTupleSize(); ++i)
    {
        const ResultValue* pVarEntry = pVars->GetTupleValueByIndex(i);
        if (pVarEntry and m_localsWatch)
        {
            wxString name, value, type, isArg;
            // The header provides Lookup helpers to extract data easily
            dbg_mi::Lookup(*pVarEntry, _T("name"), name);
            dbg_mi::Lookup(*pVarEntry, _T("value"), value);
            dbg_mi::Lookup(*pVarEntry, _T("type"), type);
            dbg_mi::Lookup(*pVarEntry, _T("arg"), isArg);

            // Process the variable name and value here
            cb::shared_ptr<GDBWatch> child;
            if (isArg.empty())
                child =  AddChild(m_localsWatch, name);
            else child = AddChild(m_argsWatch, name);
        }//endif
    }//endfor

    Finish();

}//OnCommandOutput

// ----------------------------------------------------------------------------
void InfoLocalsAndArgs::OnStart()
// ----------------------------------------------------------------------------
{
    TextCtrlLogger* pLogger = Manager::Get()->GetDebuggerManager()->GetLogger();
    if (pLogger) pLogger->Append(_T("[     ]InfoLocalsAndArgs::OnStart()"));

    m_Cmd.Printf("-stack-list-variables 2");
    m_localsArgs_values_id = Execute(m_Cmd);

}//end InfoLocalsAndArgs::OnStart()

}//namespace dbg_mi


/*
Note 1:
    If m_watch->IsAutoUpdateEnabled() evaluates to false, your action fires off
    -var-set-frozen right as it shifts into StepListChildren or calls Finish().
    If it branches to else (no children) and calls Finish(), the action dies
    immediately, and the incoming response from -var-set-frozen will arrive
    orphaned later with no action instance bound to consume it.

    If it branches to else if(children > 0), it sets m_sub_commands_left = 1
    for the child list command, completely overwriting or ignoring the frozen
    command's lifetime.

    Recommendation: If your framework's command queue tracks responses strictly
    by CommandID and discards responses to actions that have already called Finish(),
    you can safely ignore this. But if your system requires strict confirmation
    matching, ensure you increment ++m_sub_commands_left; whenever you fire
    -var-set-frozen.
*/

/*
            // (ph 26/06/13) see notes notebook page 26.5.12
            //    if (not pWatch->IsExpanded())
            //    {
            //        wxString symbol; pWatch->GetSymbol(symbol);
            //        wxString value; pWatch->GetValue(value);
            //        wxString cmd = "-data-evaluate-expressioin " + symbol;
            //        Execute(cmd);
            //        asm("nop"); // **Debugging**
            //    }
            //TODO: Determine a way to show the raw data for the locals and args "special containers"
            // Be aware that turning the special container flag "readonly" to false will
            //      allow the watch raw editor to work, but will cause the
            //      the watches window to fail to show values when items are expanded.
            //      Follow the OnExpand/ExpandWatch code flow in watchesdlg to find out why.
            // Look at watches.dlg WatchRawDialog class
            // Add to cbplugin.h See OnWatchesContexMenu() for an example.
            //      Add festure "supports WatchesViewRawData or some such
            //      WatchRawDialog class 205 227
            //      created 351
            //      stored in s_dialog[watchptr]
            //      allow SetValue(watches properties) to take place first
            //      check if supports feature watchrawdialog or watchRawData
            //      call debugger_gdbmi plugin->SetWatchRawValue(watch, m_type) 281
            //      intercept at 307
            //  Needs support feature FEATURE::WATCHRAWDIALOG addes

*/
