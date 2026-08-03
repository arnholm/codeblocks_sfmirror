#ifndef _Debugger_GDB_MI_ACTIONS_H_
#define _Debugger_GDB_MI_ACTIONS_H_

#include <memory>
#include <functional>
#include<unordered_map>

#include "cbplugin.h"
#include "debuggermanager.h"
#include "logmanager.h"      // (ph 26/05/11)
#include "loggers.h"         // (ph 26/05/20)

//#include "infowindow.h"
//-#include "definitions.h"
#include "cmd_queue.h"
#include "parsewatchvalue.h" // (ph 26/05/11)
//#include "helpers.h"

namespace dbg_mi
{

    // ----------------------------------------------------------------------------
    class SimpleAction : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        //-SimpleAction(wxString const &cmd) :
        SimpleAction(wxString const &cmd, Logger& logger) :
            m_command(cmd),
            m_logger(logger)
        {
        }

        //-virtual void OnCommandOutput(CommandID const & /*id*/, ResultParser const & /*result*/)
        virtual void OnCommandOutput(CommandID const & id, ResultParser const & result)
        {
            m_logger.Debug(_T("SimpleAction::OnCommandOutput()"));

            wxUnusedVar(id);
            wxUnusedVar(result);
            Finish();
        }
    protected:
        virtual void OnStart()
        {
            m_logger.Debug(_T("SimpleAction::OnStart()"));

            wxUnusedVar(m_logger);
            Execute(m_command);
        }
    private:
        wxString m_command;
        Logger& m_logger;
    };

    // ----------------------------------------------------------------------------
    class BarrierAction : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        BarrierAction()
        {
            SetWaitPrevious(true);

            TextCtrlLogger* pLogger = Manager::Get()->GetDebuggerManager()->GetLogger();
            if (pLogger) pLogger->Append(_T("[     ]GdbCmd_BarrierCallBack::Constructor"));

        }
        virtual void OnCommandOutput(CommandID const & /*id*/, ResultParser const & /*result*/) {}
    protected:
        virtual void OnStart()
        {
            TextCtrlLogger* pLogger = Manager::Get()->GetDebuggerManager()->GetLogger();
            if (pLogger) pLogger->Append(_T("[     ]BarrierAction::OnStart() and Finish()"));

            Finish();
        }
    };

//  GdbCmd_CallBackAction�that mimics the simplicity of wxString's event forwarding.
//  You pass 'this' (the object instance) and the address of the member function.
//  Example:
// int rowNum = 5;
//  wxString errorMsg = _T("GDB timed out reading memory block.");
//  GdbCmd_CallBackAction handles wrapping 'rowNum' and 'errorMsg' automatically
//  m_ActionsMap->Add(new dbg_mi::GdbCmd_CallBackAction(this, &MyDebuggerPanel::LogQueueError, rowNum, errorMsg));
//
//  Working Example:
//
//  Queue a call back to update the value of each watch
//  Can use either a lammbda or the wxWidgets CallAfter() method
//
//  m_actions.Add(new dbg_mi::GdbCmd_CallBackAction([=]() {
//                this->UpdateAllWatchesDone(ignoreAutoUpdate);
//                }));
//  m_actions.Add(new dbg_mi::GdbCmd_CallBackAction(this, &Debugger_GDB_MI::UpdateAllWatchesDone, ignoreAutoUpdate));

// ----------------------------------------------------------------------------
class GdbCmd_BarrierCallBack : public Action
// ----------------------------------------------------------------------------
{
public:
    // C++11 Decoupled Variadic Template Constructor
    template<typename T, typename... MethodArgs, typename... Args>
    GdbCmd_BarrierCallBack(const char* originFunc, int originLine,
                           T* instance, void (T::*method)(MethodArgs...), Args&&... args) :
        m_originFunc(wxString::FromUTF8(originFunc)),
        m_originLine(originLine),
        m_callback(std::bind(method, instance, std::forward<Args>(args)...))
    {
        SetWaitPrevious(true);

        LogMessage(_T("Constructor()"));
    }

    // Support a simple standalone function or a manual lambda capture if needed
    GdbCmd_BarrierCallBack(const char* originFunc, int originLine,
                           std::function<void()> const &callback) :
        m_originFunc(wxString::FromUTF8(originFunc)),
        m_originLine(originLine),
        m_callback(callback)
    {
        SetWaitPrevious(true);

        LogMessage(_T("Constructor()"));
    }

    virtual ~GdbCmd_BarrierCallBack() override
    {
        m_callback = nullptr;
    }

    // ----------------------------------------------------------------------------
    virtual void OnCommandOutput(CommandID const & /*id*/, ResultParser const & /*result*/) override
    // ----------------------------------------------------------------------------
    {
        LogMessage(_T("OnCommandOutput()"));

        // Explicitly mark this action as fully completed.
        Finish();
    }

protected:
    // ----------------------------------------------------------------------------
    virtual void OnStart() override
    // ----------------------------------------------------------------------------
    {
        LogMessage(_T("OnStart() and scheduling CallAfter()"));

        if (m_callback)
        {
            auto callback = m_callback;

            // Queue to Code::Blocks' main event queue
            // in order to release this object from the actions queue
            wxWindow* pMainWin = Manager::Get()->GetAppWindow();
            if (pMainWin)
            {
                pMainWin->CallAfter([callback]() {
                    callback();
                });
            }
        }

        // Explicitly mark this action as fully completed right away.
        Finish();
        LogMessage(_T("OnStart() and Finished"));
    }

private:
    // Helper method to handle unified logging cleanly
    void LogMessage(const wxChar* formatStage)
    {
        TextCtrlLogger* pLogger = Manager::Get()->GetDebuggerManager()->GetLogger();
        if (pLogger)
        {
            pLogger->Append(wxString::Format(_T("[     ]GdbCmd_BarrierCallBack::%s [Set by: %s() @ Line %d]"),
                                             formatStage,
                                             m_originFunc,
                                             m_originLine));
        }
    }

    wxString              m_originFunc;
    int                   m_originLine;
    std::function<void()> m_callback;

};//end BarrierCallBack

    // ----------------------------------------------------------------------------
    class BreakpointAddAction : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        BreakpointAddAction(std::shared_ptr<Breakpoint> const &breakpoint, Logger &logger) :
            m_breakpoint(breakpoint),
            m_logger(logger)
        {
        }
        virtual ~BreakpointAddAction()
        {
            m_logger.Debug(_T("BreakpointAddAction::destructor"));
        }
        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();
        void RemoveBreakpoint(wxString filename, int lineNo);

    private:
        std::shared_ptr<Breakpoint> m_breakpoint;
        CommandID m_initial_cmd, m_disable_cmd;

        Logger &m_logger;
    };

    // ----------------------------------------------------------------------------
    template<typename StopNotification>
    class RunAction : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        RunAction(cbDebuggerPlugin *plugin, const wxString &command,
                  StopNotification notification, Logger &logger) :
            m_plugin(plugin),
            m_command(command),
            m_notification(notification),
            m_logger(logger)
        {
            SetWaitPrevious(true);
        }
        virtual ~RunAction()
        {
            m_logger.Debug(_T("RunAction::destructor"));
        }

        //-virtual void OnCommandOutput(CommandID const &/*id*/, ResultParser const &result)
        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result)
        {
            wxUnusedVar(id);
            if(result.GetResultClass() == ResultParser::ClassRunning)
            {
                m_logger.Debug(_T("RunAction success, the debugger is !stopped!"));
                wxString debugString = result.MakeDebugString();
                debugString.Replace("\n", "\\n");
                m_logger.Debug(_T("RunAction::Output - ") + result.MakeDebugString());
                m_notification(false);
            }

            Finish();
        }
    protected:
        virtual void OnStart()
        {
            Execute(m_command);
            m_logger.Debug(_T("RunAction::OnStart -> ") + m_command);
        }

    private:
        cbDebuggerPlugin *m_plugin;
        wxString m_command;
        StopNotification m_notification;
        Logger &m_logger;
        int polledIntervals = 0;
    };

    struct SwitchToFrameInvoker
    {
        virtual ~SwitchToFrameInvoker() {}

        virtual void Invoke(int frame_number) = 0;
    };

    // ----------------------------------------------------------------------------
    class GenerateBacktrace : public Action
    // ----------------------------------------------------------------------------
    {
        GenerateBacktrace(GenerateBacktrace &);
        GenerateBacktrace& operator =(GenerateBacktrace &);
    public:
        GenerateBacktrace(SwitchToFrameInvoker *switch_to_frame, BacktraceContainer &backtrace,
                          CurrentFrame &current_frame, Logger &logger);
        virtual ~GenerateBacktrace();
        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();
    private:
        SwitchToFrameInvoker *m_switch_to_frame;
        CommandID m_backtrace_id, m_args_id, m_frame_info_id;
        BacktraceContainer &m_backtrace;
        Logger &m_logger;
        CurrentFrame &m_current_frame;
        int m_first_valid, m_old_active_frame;
        bool m_parsed_backtrace, m_parsed_args, m_parsed_frame_info;
    };

    // ----------------------------------------------------------------------------
    class GenerateThreadsList : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        GenerateThreadsList(ThreadsContainer &threads, int current_thread_id, Logger &logger);
        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();
    private:
        ThreadsContainer &m_threads;
        Logger &m_logger;
        int m_current_thread_id;
    };


    // ----------------------------------------------------------------------------
    template<typename Notification>
    class SwitchToThread : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        SwitchToThread(int thread_id, Logger &logger, Notification const &notification) :
            m_thread_id(thread_id),
            m_logger(logger),
            m_notification(notification)
        {
        }

        virtual void OnCommandOutput(CommandID const &/*id*/, ResultParser const &result)
        {
            m_notification(result);
            Finish();
        }
    protected:
        virtual void OnStart()
        {
            Execute(wxString::Format(_T("-thread-select %d"), m_thread_id));
        }

    private:
        int m_thread_id;
        Logger &m_logger;
        Notification m_notification;
    };

    // ----------------------------------------------------------------------------
    template<typename Notification>
    class SwitchToFrame : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        SwitchToFrame(int frame_id, Notification const &notification, bool user_action) :
            m_frame_id(frame_id),
            m_notification(notification),
            m_user_action(user_action)
        {
        }

        virtual void OnCommandOutput(CommandID const &/*id*/, ResultParser const &result)
        {
            m_notification(result, m_frame_id, m_user_action);
            Finish();
        }
    protected:
        virtual void OnStart()
        {
            Execute(wxString::Format(_T("-stack-select-frame %d"), m_frame_id));
        }
    private:
        int m_frame_id;
        Notification m_notification;
        bool m_user_action;
    };

    // ----------------------------------------------------------------------------
    class WatchBaseAction : public Action
    // ----------------------------------------------------------------------------
    {
    public:
        WatchBaseAction(WatchesContainer &watches, Logger &logger);
        virtual ~WatchBaseAction();


    protected:
        void ExecuteListCommand(Watch::Pointer watch, Watch::Pointer parent = Watch::Pointer());
        void ExecuteListCommand(wxString const &watch_id, Watch::Pointer parent); // (ph 26/06/07)
        //-bool ParseListCommand(CommandID const &id, ResultValue const &value, Watch::Pointer watch); // (ph 26/06/07) // (ph 26/06/19)
        bool ParseListCommand(CommandID const &id, ResultValue const &value, Watch::Pointer watch, int &out_commands_launched);

        void SetRange(int start, int end) { m_start = start; m_end = end; }
    protected:
        typedef std::unordered_map<CommandID, Watch::Pointer> ListCommandParentMap;
    protected:
        ListCommandParentMap m_parent_map;
        WatchesContainer& m_watches;
        Logger &m_logger;
        int m_sub_commands_left;
        int m_start, m_end;
    };

    // ----------------------------------------------------------------------------
    class WatchCreateAction : public WatchBaseAction
    // ----------------------------------------------------------------------------
    {
        enum Step
        {
            StepCheckExpr = 0,   // <-- new step
            StepCreate,
            StepListChildren,
            StepSetRange
        };
    public:
        WatchCreateAction(Watch::Pointer const &watch, WatchesContainer &watches, Logger &logger);
        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();
    protected:
        Watch::Pointer m_watch;
        Step m_step;
    };

    // ----------------------------------------------------------------------------
    class WatchCreateTooltipAction : public WatchCreateAction
    // ----------------------------------------------------------------------------
    {
    public:
        WatchCreateTooltipAction(Watch::Pointer const &watch, WatchesContainer &watches, Logger &logger, wxRect const &rect) :
            WatchCreateAction(watch, watches, logger),
            m_rect(rect)
        {
        }
        virtual ~WatchCreateTooltipAction();
    private:
        wxRect m_rect;
    };

    // ----------------------------------------------------------------------------
    class WatchesUpdateAction : public WatchBaseAction
    // ----------------------------------------------------------------------------
    {
    public:
        WatchesUpdateAction(WatchesContainer &watches, Logger &logger);
        WatchesUpdateAction(WatchesContainer &watches, Logger &logger, Watch::Pointer singleWatch); // single watch

        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();

    private:
        bool ParseUpdate(ResultParser const &result,Watch::Pointer watch);

    private:
        TextCtrlLogger* pLog = Manager::Get()->GetDebuggerManager()->GetLogger();
        Watch::Pointer m_watch;
        CommandID   m_update_command;
        bool m_isSingleWatch = false;
        int  m_step = 0;
    };

    // ----------------------------------------------------------------------------
    class WatchEvaluateExpression : public WatchBaseAction
    // ----------------------------------------------------------------------------
    {
    public:
        WatchEvaluateExpression(Watch::Pointer const &watch, WatchesContainer &watches, Logger &logger);

        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();

    private:
        bool ParseUpdate(ResultParser const &result);
    private:
        CommandID   m_update_command;
        Watch::Pointer m_watch;
    };

    ////// ----------------------------------------------------------------------------
    ////class WatchesUpdateFormat : public WatchBaseAction
    ////// ----------------------------------------------------------------------------
    ////{
    ////public:
    ////    WatchesUpdateFormat(WatchesContainer &watches, Logger &logger);
    ////
    ////    virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    ////protected:
    ////    virtual void OnStart();
    ////
    ////private:
    ////    bool ParseUpdate(ResultParser const &result);
    ////private:
    ////    //CommandID   m_update_command;
    ////
    ////};

    // ----------------------------------------------------------------------------
    class WatchExpandedAction : public WatchBaseAction
    // ----------------------------------------------------------------------------
    {
    public:
        WatchExpandedAction(Watch::Pointer parent_watch, Watch::Pointer expanded_watch,
                            WatchesContainer &watches, Logger &logger) :
            WatchBaseAction(watches, logger),
            m_watch(parent_watch),
            m_expanded_watch(expanded_watch)
        {
            SetRange(0, 100);
            if (parent_watch->IsArray()) // implement edit Watches IsArray()
                SetRange(parent_watch->GetArrayStart(),
                            parent_watch->GetArrayStart()+parent_watch->GetArrayCount());
        }

        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();

    private:
        CommandID m_update_id;
        Watch::Pointer m_watch;
        Watch::Pointer m_expanded_watch;
    };

    // ----------------------------------------------------------------------------
    class WatchCollapseAction : public WatchBaseAction
    // ----------------------------------------------------------------------------
    {
    public:
        WatchCollapseAction(Watch::Pointer parent_watch, Watch::Pointer collapsed_watch,
                            WatchesContainer &watches, Logger &logger) :
            WatchBaseAction(watches, logger),
            m_watch(parent_watch),
            m_collapsed_watch(collapsed_watch)
        {
        }

        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
    protected:
        virtual void OnStart();

    private:
        Watch::Pointer m_watch;
        Watch::Pointer m_collapsed_watch;
        Logger* pLogger;
    };
    // ----------------------------------------------------------------------------
    class InfoRegisters : public Action
    // ----------------------------------------------------------------------------
    {

        InfoRegisters(InfoRegisters &);
        InfoRegisters& operator =(InfoRegisters &);

        public:
            // only tested on mingw/pc/win env
            InfoRegisters(Logger& logger, wxString disassemblyFlavor = wxEmptyString);
            virtual ~InfoRegisters();
            virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
        protected:
            virtual void OnStart();
        private:
            Logger& m_logger;
            wxString m_disassemblyFlavor;
            CommandID m_reg_values_id;
            CommandID m_reg_names_id;
            bool m_parsed_reg_names;
            bool m_parsed_reg_values;
            wxArrayString regNames;
            wxArrayString regValues;
    };
    // ----------------------------------------------------------------------------
    class GenerateDisassembly : public Action
    // ----------------------------------------------------------------------------
    {
      // Command to run a disassembly.

        GenerateDisassembly(GenerateDisassembly &);
        GenerateDisassembly& operator =(GenerateDisassembly &);


        Logger& m_logger;
        bool m_mixedMode;
        CommandID m_frame_info_id;
        CommandID m_disassemble_info_id;
        bool m_parsed_frame_info;
        bool m_parsed_disassemble_info;

        public:
            GenerateDisassembly(Logger &logger, bool MixedMode=false, wxString hexAddrStr= _T(""));
            virtual ~GenerateDisassembly();
            //virtual void ParseOutput(const wxString& output);
            virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
        protected:
            virtual void OnStart();
    };
    // ----------------------------------------------------------------------------
    class ExamineMemory : public Action
    // ----------------------------------------------------------------------------
    {

        ExamineMemory(ExamineMemory &);
        ExamineMemory& operator =(ExamineMemory &);

        public:
            // only tested on mingw/pc/win env
            ExamineMemory(Logger& logger);
            virtual ~ExamineMemory();
            virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
        protected:
            virtual void OnStart();
        private:
            Logger& m_logger;
            CommandID m_memory_values_id;
            bool m_parsed_memory_values;
    };

    // ----------------------------------------------------------------------------
    class InfoLocalsAndArgs : public Action
    // ----------------------------------------------------------------------------
    {
        cb::shared_ptr<GDBWatch> m_localsWatch; //single watch with local children
        cb::shared_ptr<GDBWatch> m_argsWatch;   //single watch with args children

        InfoLocalsAndArgs(InfoLocalsAndArgs &);
        InfoLocalsAndArgs& operator =(InfoLocalsAndArgs &);

        public:
            InfoLocalsAndArgs(ActionsMap* pActonsMap, int localsOrArgs, cb::shared_ptr<GDBWatch> localsWatch, cb::shared_ptr<GDBWatch> argsWatch);
            virtual ~InfoLocalsAndArgs();
            virtual void OnCommandOutput(CommandID const &id, ResultParser const &result);
        protected:
            virtual void OnStart();
        private:
            ActionsMap* m_pActionsMap;
            CommandID m_localsArgs_values_id;
            bool m_parsed_result_values;
            int  m_localsOrArgs = 0;
    };

    /**
     * Command to determine the debugging (working) language.
     */
    // ----------------------------------------------------------------------------
    class GdbCmd_DebugLanguage : public Action
    // ----------------------------------------------------------------------------
    {
        // Disable copy constructor and assignment operator
        GdbCmd_DebugLanguage(const GdbCmd_DebugLanguage &);
        GdbCmd_DebugLanguage& operator =(const GdbCmd_DebugLanguage &);

      public:
        GdbCmd_DebugLanguage() : Action() {}
        virtual ~GdbCmd_DebugLanguage() {}

      protected:
        virtual void OnStart() override
        {
            // Use QueueCommand to send the request to GDB
            // Assuming m_pControl or similar is available in the Action base class
            wxString m_cmd = _T("show language");
            Execute(m_cmd);
        }

        // Fixed: Signature to match the Action interface (using ResultParser or raw output)
        // If the base Action uses raw wxString output, keep this.
        // If it uses GDB/MI ResultParser (like InfoLocalsAndArgs), change the parameters.
        virtual void OnCommandOutput(CommandID const &id, ResultParser const &result) override
        {
            // In GDB/MI, 'show language' often returns a string in the 'value' or 'message' field
            // But since 'show' is a console command, we often parse the console stream
            wxUnusedVar(id);
            wxString output = result.MakeDebugString().Lower();

            if (output.Find(wxT("fortran")) != wxNOT_FOUND)
                g_DebugLanguage = dl_Fortran;
            else if (output.Find(wxT(" c")) != wxNOT_FOUND || output.Find(wxT("c++")) != wxNOT_FOUND)
                g_DebugLanguage = dl_Cpp;
            // Optional: add default or other languages
        }
    };//end class

/**
 * Command to get info about a watched variable using GDB/MI.
 */
// ----------------------------------------------------------------------------
class GdbCmd_Watch : public Action
// ----------------------------------------------------------------------------
{
    cb::shared_ptr<GDBWatch> m_watch;

    // Disable copy constructor and assignment operator
    GdbCmd_Watch(const GdbCmd_Watch &);
    GdbCmd_Watch& operator =(const GdbCmd_Watch &);

public:
    virtual ~GdbCmd_Watch() {}

    GdbCmd_Watch(cb::shared_ptr<GDBWatch> watch)
        : Action(),
          m_watch(watch)
    {
    }

    // --------------------------------
    virtual void OnStart() override     // GdbCmd_Watch
    // --------------------------------
    {
        wxString type;
        wxString symbol;

        m_watch->GetSymbol(symbol);
        m_watch->GetType(type);
        type.Trim(true).Trim(false);

        // 1. Start MI Command
        m_Cmd = _T("-data-evaluate-expression ");

        // 2. Map formats to MI flags
        switch (m_watch->GetFormat())
        {
            case dbg_mi::Decimal:   m_Cmd << _T("--format decimal ");     break;
            case dbg_mi::Unsigned:  m_Cmd << _T("--format unsigned ");    break;
            case dbg_mi::Hex:       m_Cmd << _T("--format hexadecimal "); break;
            case dbg_mi::Binary:    m_Cmd << _T("--format binary ");      break;
            case dbg_mi::Char:      m_Cmd << _T("--format raw ");         break; // 'raw' is closest to CLI 'output'
            case dbg_mi::Float:     m_Cmd << _T("--format floating-point "); break;
            default:        break;
        }

        // 3. Construct the expression string (quoted for MI)
        m_Cmd << _T("\"");

        if (g_DebugLanguage == dl_Cpp)
        {
            // auto-set array types
            if (!m_watch->IsArray() && m_watch->GetFormat() == Undefined && type.Contains(_T('[')))
                m_watch->SetArray(true);

            if (m_watch->IsArray() && m_watch->GetArrayCount() > 0)
            {
                m_Cmd << wxT("(") << symbol << wxT(")");
                m_Cmd << wxString::Format(_T("[%d]@%d"), m_watch->GetArrayStart(), m_watch->GetArrayCount());
            }
            else
                m_Cmd << symbol;
        }
        else  // (g_DebugLanguage == dl_Fortran)
        {
            if (m_watch->IsArray() && m_watch->GetArrayCount() > 0)
            {
                if (m_watch->GetArrayStart() < 1)
                    m_watch->SetArrayParams(1, m_watch->GetArrayCount());
                m_Cmd << symbol;
                m_Cmd << wxString::Format(_T("(%d)@%d"), m_watch->GetArrayStart(), m_watch->GetArrayCount());
            }
            else
                m_Cmd << symbol;
        }

        m_Cmd << _T("\"");
        Execute(m_Cmd);
    }

    // GdbCmd_Watch
    // ----------------------------------------------------------------------------
    virtual void OnCommandOutput(CommandID const &id, ResultParser const &result) override
    // ----------------------------------------------------------------------------
    {
        wxUnusedVar(id);

        // 1. Extract the variable's display name of the watch object
        wxString var_name;
        m_watch->GetSymbol(var_name);

        wxString value_str;
        wxString structural_type = _T("Unknown");

        // 2. Fetch the root ResultValue container from the parser
        ResultValue const &root = result.GetResultValue();

        // 3. Locate the 'value' child element in the parsed structure
        ResultValue const *val_node = root.GetTupleValue(_T("value"));

        if (val_node)
        {
            // Determine the MI data structure type
            switch (val_node->GetType())
            {
                case ResultValue::Simple:
                    structural_type = _T("Simple");
                    // The library automatically handles unescaping string literals internally
                    value_str = val_node->GetSimpleValue();
                    break;

                case ResultValue::Array:
                    structural_type = _T("Array");
                    value_str = val_node->MakeDebugString(); // Fallback string representation for structures
                    break;

                case ResultValue::Tuple:
                    structural_type = _T("Tuple");
                    value_str = val_node->MakeDebugString(); // Fallback string representation for structures
                    break;
            }
        }
        else
        {
            // Fallback if GDB didn't return a 'value' field (e.g. error payloads)
            value_str = result.MakeDebugString();
            value_str.Trim(true).Trim(false);
        }

        // 4. Update the watch with the extracted data
        // (Assuming ParseGDBWatchValue expects the unescaped literal or raw structure string)

        if (not ParseGDBWatchValue(m_watch, value_str))
        {
            wxString const &msg = wxT("Parsing GDB/MI output failed for '") + var_name + wxT("'!");
            m_watch->SetValue(msg);
            Manager::Get()->GetLogManager()->LogError(msg);
        }
        else
        {
            // Optional: If the m_watch object possesses setters for tracking type/name
            // m_watch->SetStructuralType(structural_type);
            //-m_watch->SetValue(value_str); // (ph 26/06/02)
            // value is already set
        }

        Finish();
    }//end GdbCmd_Watch
};
/**
 * Command to get a watched variable's type using GDB/MI.
 */
// ----------------------------------------------------------------------------
class GdbCmd_FindWatchType : public Action
// ----------------------------------------------------------------------------
{
    cb::shared_ptr<GDBWatch> m_watch;
    ActionsMap* m_pActionsMap;

    // Disable copy constructor and assignment operator
    GdbCmd_FindWatchType(const GdbCmd_FindWatchType &);
    GdbCmd_FindWatchType& operator =(const GdbCmd_FindWatchType &);

public:
    virtual ~GdbCmd_FindWatchType() {}

    GdbCmd_FindWatchType(ActionsMap* pActonsMap, cb::shared_ptr<GDBWatch> watch)
        : m_watch(watch),
          m_pActionsMap(pActonsMap)
    {
    }

protected:
    virtual void OnStart() override
    {
        if (m_watch)
        {
            wxString symbol;
            m_watch->GetSymbol(symbol);

            // GDB/MI equivalent of CLI 'whatis'
            wxString cmd = wxT("-symbol-info-type ") + symbol;
            Execute(cmd);
        }
    }

    virtual void OnCommandOutput(CommandID const &id, ResultParser const &result) override
    {
        wxUnusedVar(id);

        // GDB/MI returns ^done,type="typename"
        // ResultParser provides access to these key-value pairs directly
        wxString type_Name;
        wxString output = result.MakeDebugString();
        if (not output.Contains("type="))
        {
            // If the MI command fails to find the symbol, it may return an error or empty type
            m_watch->RemoveChildren();
            m_watch->SetType(wxEmptyString);
            m_watch->SetValue(_("Symbol not found or has no type info."));
            return;
        }
        type_Name = output.AfterFirst('=');
        type_Name.Replace("\"","");
        // Clean up the string (MI usually returns clean strings, but Strip is safe)
        type_Name.Strip(wxString::both);

        wxString old_type;
        m_watch->GetType(old_type);

        // If the type has changed, update the watch and clear children
        if (old_type != type_Name)
        {
            m_watch->RemoveChildren();
            m_watch->SetType(type_Name);
            m_watch->SetValue(wxEmptyString);
        }

        // Chain the next command to get the actual value
        m_pActionsMap->Add(new GdbCmd_Watch(m_watch));
    }
};

/** Action-only debugger command to signal the watches tree to update. */
// ----------------------------------------------------------------------------
class DbgCmd_UpdateWindow : public Action
// ----------------------------------------------------------------------------
{
    cbDebuggerPlugin::DebugWindows m_windowToUpdate;

    DbgCmd_UpdateWindow(const DbgCmd_UpdateWindow &);
    DbgCmd_UpdateWindow& operator =(const DbgCmd_UpdateWindow &);

    public:
    virtual ~DbgCmd_UpdateWindow() {}

    DbgCmd_UpdateWindow(cbDebuggerPlugin::DebugWindows windowToUpdate)
        : m_windowToUpdate(windowToUpdate)
        {}

    protected:
    virtual void OnCommandOutput(CommandID const &id, ResultParser const &result) override
    {
        // This routine never entered bec: OnStart() has a Finish() call.
        wxUnusedVar(id);
        wxUnusedVar(result);
        Finish();
    }

    virtual void OnStart() override
    {
        cbDebuggerPlugin* pDebugger = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
        if (pDebugger)
        {
            //cbDebuggerPlugin::DebugWindows m_windowToUpdate;
            CodeBlocksEvent event(cbEVT_DEBUGGER_UPDATED);
            event.SetInt(int(m_windowToUpdate));
            event.SetPlugin(pDebugger);
            Manager::Get()->ProcessEvent(event);
        }
        Finish();
    }
};//end DbgCmd_UpdateWindow()

// ----------------------------------------------------------------------------
} ///end namespace dbg_mi
// ----------------------------------------------------------------------------

#endif // _Debugger_GDB_MI_ACTIONS_H_
