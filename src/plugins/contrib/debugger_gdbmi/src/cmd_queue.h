#ifndef _DEBUGGER_MI_GDB_CMD_QUEUE_H_
#define _DEBUGGER_MI_GDB_CMD_QUEUE_H_


#include <deque>
#include <ostream>

#include <wx/string.h>

#include "globals.h"
#include "infowindow.h"
#include "cbplugin.h"

#include "helpers.h"
#include "cmd_result_parser.h"

/*
#include <wx/thread.h>
class PipedProcess;
*/

namespace dbg_mi
{

// ----------------------------------------------------------------------------
class Logger
// ----------------------------------------------------------------------------
{
public:
    struct Line
    {
        enum Type
        {
            Unknown = 0,
            Debug,
            Command,
            CommandResult,
            ProgramState,
            Error //(ph 2024/04/12)
        };

        wxString line;
        Type type;
    };

    struct Log
    {
        enum Type
        {
            Normal = 0,
            Error
        };
    };
public:
    virtual ~Logger() {}

    virtual void Log(wxString const &line, Log::Type type = Log::Normal) = 0;
    virtual void Debug(wxString const &line, Line::Type type = Line::Debug) = 0;
    virtual Line const* GetDebugLine(int index) const = 0;

    virtual void AddCommand(wxString const &command) = 0;
    virtual int GetCommandCount() const = 0;
    virtual wxString const& GetCommand(int index) const = 0;
    virtual void ClearCommand() = 0;
};

// ----------------------------------------------------------------------------
class CommandID
// ----------------------------------------------------------------------------
{
public:
    explicit CommandID(int32_t action = -1, int32_t command_in_action = -1) :
        m_action(action),
        m_command_in_action(command_in_action)
    {
    }

    bool operator ==(CommandID const &o) const
    {
        return m_action == o.m_action && m_command_in_action == o.m_command_in_action;
    }
    bool operator !=(CommandID const &o) const
    {
        return !(*this == o);
    }


    CommandID& operator++() // prefix
    {
        ++m_command_in_action;
        return *this;
    }

    CommandID operator++(int) // postfix
    {
        CommandID old = *this;
        ++m_command_in_action;
        return old;
    }

    wxString ToString() const
    {
        return wxString::Format(_T("%d%010d"), m_action, m_command_in_action);
    }

    int32_t GetActionID() const
    {
        return m_action;
    }

    int32_t GetCommandID() const
    {
        return m_command_in_action;
    }

    int64_t GetFullID() const
    {
        return (static_cast<int64_t>(m_action) >> 32) + m_command_in_action;
    }

private:
    int32_t m_action, m_command_in_action;
};

// ----------------------------------------------------------------------------
inline std::ostream& operator<< (std::ostream& s, CommandID const &id)
// ----------------------------------------------------------------------------
{
    s << id.ToString().utf8_str().data();
    return s;
}

// ----------------------------------------------------------------------------
bool ParseGDBOutputLine(wxString const &line, CommandID &id, wxString &result_str);
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
class Action
// ----------------------------------------------------------------------------
{
    struct Command
    {
        Command() {}
        Command(wxString const &string_, int id_) :
            string(string_),
            id(id_)
        {
        }

        wxString string;
        int id;
    };
    typedef std::deque<Command> PendingCommands;
public:
    Action() :
        m_id(-1),
        m_last_command_id(0),
        m_started(false),
        m_finished(false),
        m_wait_previous(false)
    {

    }

    virtual ~Action() {}

    void SetID(int id) { m_id = id; }
    int GetID() const { return m_id; }

    void Start()
    {
        m_started = true;
        OnStart();
    }

    void Finish()
    {
        m_finished = true;
    }

    bool Started() const { return m_started; }
    bool Finished() const { return m_finished; }

    void SetWaitPrevious(bool flag) { m_wait_previous = flag; }
    bool GetWaitPrevious() const { return m_wait_previous; }

    CommandID Execute(wxString const &command)
    {
        m_pending_commands.push_back(Command(command, m_last_command_id));
        return CommandID(m_id, m_last_command_id++);
    }

    int GetPendingCommandsCount() const { return m_pending_commands.size(); }
    bool HasPendingCommands() const { return !m_pending_commands.empty(); }

    wxString PopPendingCommand(CommandID &id)
    {
        assert(HasPendingCommands());
        Command cmd = m_pending_commands.front();
        m_pending_commands.pop_front();

        id = CommandID(GetID(), cmd.id);
        return cmd.string;
    }


public:
    virtual void OnCommandOutput(CommandID const &id, ResultParser const &result) = 0;
protected:
    virtual void OnStart() = 0;
private:
    PendingCommands m_pending_commands;
    int m_id;
    int m_last_command_id;
    bool m_started;
    bool m_finished;
    bool m_wait_previous;
public:
    wxString m_Cmd; // FIXME: remove m_Cmd
    int polledIntervals = 0;    // checking for stalled actions

};

// ----------------------------------------------------------------------------
class CommandExecutor
// ----------------------------------------------------------------------------
{
public:
    struct Result
    {
        dbg_mi::CommandID id;
        wxString output;
    };
public:
    CommandExecutor() :
        m_last(0),
        m_logger(NULL)
    {
    }
    virtual ~CommandExecutor() {}

    CommandID Execute(wxString const &cmd);
    void ExecuteSimple(dbg_mi::CommandID const &id, wxString const &cmd);

    virtual wxString GetOutput() = 0;

    bool HasOutput() const { return !m_results.empty(); }
    bool ProcessOutput(wxString const &output);
    Logger::Line::Type GetFilteredOutputError(wxString& output, Logger::Line::Type); //(ph 2024/04/24)

    void Clear();

    dbg_mi::ResultParser* GetResult(dbg_mi::CommandID &id)
    {
        assert(not m_results.empty());
        Result const &r = m_results.front();

        id = r.id;
        dbg_mi::ResultParser *parser = new dbg_mi::ResultParser;
        if(not parser->Parse(r.output))
        {
            // This causes GDB/MI to stall
            // On a malformed response, set m_type = TypeError
            // so a message is output and we keep processing.
            // else Finish() will never be called and the debugger stalls
            //-delete parser;
            //-parser = NULL;
        }
        if (parser->GetParseError())
        {
            InfoWindow::Display(_("GDB/MI Parse Error"), wxString::Format(wxString::Format(_T("%s %s"),_("Malformed debug response for"), id.ToString().c_str()), 7000));
            if (GetLogger())
                GetLogger()->Debug(_T("CommandExecutor: Malformed debugger response: ") + id.ToString());

        }

        // Show debugger cursor mark and set cmd executor to stopped (idle)
        if (parser->GetResultClass() == ResultParser::ClassDone)
        {
            //-SetDebugMark(true);
        }

        m_results.pop_front();
        return parser;
    }

    void SetLogger(Logger *logger) { m_logger = logger; }
    Logger* GetLogger() { return m_logger; }

    int32_t GetLastID() const { return m_last; }
protected:
    virtual bool DoExecute(dbg_mi::CommandID const &id, wxString const &cmd) = 0;
    virtual void DoClear() = 0;

protected:
    typedef std::deque<Result> Results;
    Results m_results;
    int32_t m_last;
    Logger *m_logger;
};

// ----------------------------------------------------------------------------
class ActionsMap
// ----------------------------------------------------------------------------
{
public:
    ActionsMap();
    ~ActionsMap();

    void Add(Action *action);
    Action* Find(int id);
    Action const * Find(int id) const;
    void Clear();
    int GetLastID() const { return m_last_id; }

    bool Empty() const { return m_actions.empty(); }
    void Run(CommandExecutor &executor);
    int GetCount(){return m_actions.size();} //(ph 2024/10/20)
private:
    typedef std::deque<Action*> Actions;

    Actions m_actions;
    int m_last_id;
};

// ----------------------------------------------------------------------------
template<typename OnNotify>
bool DispatchResults(CommandExecutor &exec, ActionsMap &actions_map, OnNotify &on_notify)
// ----------------------------------------------------------------------------
{
    while(exec.HasOutput())
    {
        CommandID id;
        ResultParser *parser = exec.GetResult(id);

        if(not parser)
            return false;

        wxString sep = wxFileName::GetPathSeparator();

        switch(parser->GetResultType())
        {
            case ResultParser::Result:
            {
                Action *action = actions_map.Find(id.GetActionID());
                if(action)
                    action->OnCommandOutput(id, *parser);

                // show RunActionMsg
                // class RunAction cannot use cbMessageBox, it's run under an external timer queue.
                // Have to display messages here.
                //note - the debugger is running under a timer, so this will not stop the debugger
                //  and this code will be re-entered with any wxMessageBox active
                while ( IsRunActionMsgQueued())
                {
                    wxString msgType, msgTitle, msgText;
                    if (not GetRunActionMsg(msgType, msgTitle,msgText))
                        break;

                    // clean up msgText
                    msgText.Replace(sep+sep, sep);  //remove doubled path seps
                    if (msgText.Contains("\\\""))   // remove "\"" quotes within quotes
                        msgText.Replace("\\\""," ");
                    int lth = msgText.Length();
                    int posnNL = msgText.Find(sep+"n"); //find ending new line char
                    if ((posnNL != wxNOT_FOUND) and (posnNL > (lth-4) ) )
                        msgText.replace(posnNL,2,""); //remove "\n"

                    if (msgType == _T("Info"))
                        InfoWindow::Display(msgTitle, msgText, 7000);

                    if ( (msgType == _T("Critical")) or (msgType == _T("Abort")))
                    {
                        cbDebuggerPlugin *dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
                        if (msgType == _T("Abort"))
                            dbg->Stop();

                        cbMessageBox(msgText, _("Debugger:")+msgTitle, wxOK, Manager::Get()->GetAppWindow());
                    }
                    if (msgType == _T("Abort"))
                    {
                        cbDebuggerPlugin *dbg = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
                            dbg->Stop();
                    }

                }//endwhile
                //end Show RunActionMsg
                break;
            }//endCase result
            case ResultParser::TypeUnknown:
                break;
            default:
                on_notify(*parser);
        }//endSwitch

        delete parser;
    }//endWhile HasOutput

    return true;
}

//class Logger // moved to top
//{
//public:
//    struct Line
//    {
//        enum Type
//        {
//            Unknown = 0,
//            Debug,
//            Command,
//            CommandResult,
//            ProgramState
//        };
//
//        wxString line;
//        Type type;
//    };
//
//    struct Log
//    {
//        enum Type
//        {
//            Normal = 0,
//            Error
//        };
//    };
//public:
//    virtual ~Logger() {}
//
//    virtual void Log(wxString const &line, Log::Type type = Log::Normal) = 0;
//    virtual void Debug(wxString const &line, Line::Type type = Line::Debug) = 0;
//    virtual Line const* GetDebugLine(int index) const = 0;
//
//    virtual void AddCommand(wxString const &command) = 0;
//    virtual int GetCommandCount() const = 0;
//    virtual wxString const& GetCommand(int index) const = 0;
//    virtual void ClearCommand() = 0;
//};

} // namespace dbg_mi

// Using unary_function gets deprecated error //(ph 2024/03/07)
//// ----------------------------------------------------------------------------
//namespace std
//// ----------------------------------------------------------------------------
//{
//    template <>
//    struct hash<dbg_mi::CommandID> : public unary_function<dbg_mi::CommandID, size_t>
//    {
//       size_t operator()(dbg_mi::CommandID const& v) const
//       {
//           return std::hash<int64_t>()(v.GetFullID());
//       }
//    };
//
//}

#include <functional>

// ----------------------------------------------------------------------------
namespace std
// ----------------------------------------------------------------------------
{
    template <>
    struct hash<dbg_mi::CommandID>
    {
       size_t operator()(const dbg_mi::CommandID& v) const
       {
           return std::hash<int64_t>()(v.GetFullID());
       }
    };
}


#endif // _DEBUGGER_MI_GDB_CMD_QUEUE_H_
