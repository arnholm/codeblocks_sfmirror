#include "cmd_queue.h"

// ----------------------------------------------------------------------------
namespace dbg_mi
// ----------------------------------------------------------------------------
{
    int callCount = 0;

bool ParseGDBOutputLine(wxString const &line, CommandID &id, wxString &result_str)
{
    size_t pos = 0;
    while(pos < line.length() && wxIsdigit(line[pos]))
        ++pos;
    if(pos <= 10)
    {
        if(pos != 0)
            return false;
        if(line[0] == _T('*') || line[0] == _T('^') || line[0] == _T('+') || line[0] == _T('='))
        {
            id = CommandID();
            result_str = line;
            return true;
        }
        else
            return false;
    }
    else
    {
        long action_id, cmd_id;

        wxString const &str_action = line.substr(0, pos - 10);
        str_action.ToLong(&action_id, 10);

        wxString const &str_cmd = line.substr(pos - 10, 10);
        str_cmd.ToLong(&cmd_id, 10);

        id = dbg_mi::CommandID(action_id, cmd_id);
        result_str = line.substr(pos, line.length() - pos);
        return true;
    }
}

CommandID CommandExecutor::Execute(wxString const &cmd)
{
    dbg_mi::CommandID id(0, m_last++);
    if(m_logger)
    {
        m_logger->Debug(_T("cmd==>") + id.ToString() + cmd, Logger::Line::Command);
        m_logger->AddCommand(id.ToString() + cmd);
    }

    if(DoExecute(id, cmd))
        return id;
    else
        return dbg_mi::CommandID();
}
void CommandExecutor::ExecuteSimple(dbg_mi::CommandID const &id, wxString const &cmd)
{
    if(m_logger)
    {
        m_logger->Debug(_T("cmd==>") + id.ToString() + cmd, Logger::Line::Command);
        m_logger->AddCommand(id.ToString() + cmd);
    }
    DoExecute(id, cmd);

}
/***
All output sequences end in a single line containing a period.
The token is from the corresponding request. If an execution command is interrupted
    by the `-exec-interrupt' command, the token associated with the `*stopped'
    message is the one of the original execution command, not the one of the interrupt
    command.
status-async-output contains on-going status information about the progress of a
    slow operation. It can be discarded. All status output is prefixed by `+'.
exec-async-output contains asynchronous state change on the target
    (stopped, started, disappeared). All async output is prefixed by `*'.
notify-async-output contains supplementary information that the client should
    handle (e.g., a new breakpoint information). All notify output is prefixed by `='.
console-stream-output is output that should be displayed as is in the console.
    It is the textual response to a CLI command. All the console output is prefixed by `~'.
target-stream-output is the output produced by the target program. All the target
    output is prefixed by `@'.
log-stream-output is output text coming from GDB's internals, for instance messages
    that should be displayed as part of an error log. All the log output is prefixed by `&'.
New GDB/MI commands should only output lists containing values.

GDB/MI Output Records
GDB/MI Result Records
GDB/MI Stream Records
GDB/MI Out-of-band Records
GDB/MI Result Records
In addition to a number of out-of-band notifications, the response to a GDB/MI
    command includes one of the following result indications:

"^done" [ "," results ]
The synchronous operation was successful, results are the return values.

"^running"
The asynchronous operation was successfully started. The target is running.

"^error" "," c-string
The operation failed. The c-string contains the corresponding error message.

GDB/MI Stream Records
GDB internally maintains a number of output streams: the console, the target, and
    the log. The output intended for each of these streams is funneled through the
    GDB/MI interface using stream records.

Each stream record begins with a unique prefix character which identifies its stream
    (see section GDB/MI Output Syntax). In addition to the prefix, each stream record
    contains a string-output. This is either raw text (with an implicit new line) or a
    quoted C string (which does not contain an implicit newline).

"~" string-output
The console output stream contains text that should be displayed in the CLI console
    window. It contains the textual responses to CLI commands.

"@" string-output
The target output stream contains any textual output from the running target.

"&" string-output
The log stream contains debugging messages being produced by GDB's internals.

GDB/MI Out-of-band Records
Out-of-band records are used to notify the GDB/MI client of additional changes that
    have occurred. Those changes can either be a consequence of GDB/MI (e.g.,
    a breakpoint modified) or a result of target activity (e.g., target stopped).

The following is a preliminary list of possible out-of-band records.
"*" "stop"

***/
// ----------------------------------------------------------------------------
bool CommandExecutor::ProcessOutput(wxString const &output)
// ----------------------------------------------------------------------------
{
    dbg_mi::CommandID id;
    Result r;

    // Message prefixes from gdb
    // tilde (~) Text responses meant for the CLI console display.
    // ampersand (&) Internal debugging messages, warnings, and error logs from GDB.
    // (@) Literal stdout/stderr output produced by the program you are debugging.
    // (*) Asynchronous state changes about the target (e.g., *stopped, *running).
    // (=) Supplementary notifications (e.g., a new thread was created, a breakpoint was modified).
    // (^) The immediate result of your last MI command (e.g., ^done, ^error).

    if(m_logger)
    {
        if(not dbg_mi::ParseGDBOutputLine(output, r.id, r.output))
        {
            // a beginning '&' means an error msg from GDB
            if (output.Length() and (output[0] == '&'))     //(ph 2024/04/12)
            {
                // These errors are useless //(ph 2024/05/07)

                // Logger::Line::Type lineType = Logger::Line::Error;
                // wxString filteredOutput = output;
                // lineType = GetFilteredOutputError(filteredOutput, lineType);
                // if (lineType == Logger::Line::Error)
                // {
                //     m_logger->Debug(_T("unparsable_output==>") + output, Logger::Line::Error);
                //     SetRunActionMsg(filteredOutput, "GDB/MI error", errInfo );
                // }
            }
            else do //unhandled messages from gdb's CLI output
            {
                wxString unparsable  = _T("unparsable_output");
                wxString outOfBand   =  _T("information");
                wxString outputType = unparsable;     //default

                if (output == "(gdb)") break;
                else if (output.StartsWith("~"))
                            outputType = outOfBand;

                m_logger->Debug(outputType + "==>" + output, Logger::Line::Unknown);

            }while(0);

            return false;
        }
        else //parse is ok
        {
            if (   output.Contains(_("No symbol table loaded"))
                    || output.Contains(_("No executable file specified"))
                    || output.Contains(_("No executable specified"))
                    || output.Contains(_("Don't know how to run"))
                    //- || output.Contains(_("error,msg=")) // (ph 25/08/31)
                    // allow the error msg above
                    // newer GDB uses the above when var is out of scope
                )
            {
                m_logger->Debug(_T("output==>") + output, Logger::Line::Error); // (ph 25/05/14)
                Logger::Line::Type lineType = Logger::Line::Error;
                wxString filteredOutput = output;
                lineType = GetFilteredOutputError(filteredOutput, lineType);
                SetRunActionMsg(filteredOutput, "GDB/MI error", errAbort );

            }
            else
                m_logger->Debug(_T("output==>") + output, Logger::Line::CommandResult);
        }
    }
    else // no m_logger
    {
        if(not dbg_mi::ParseGDBOutputLine(output, r.id, r.output))
            return false;
    }

    m_results.push_back(r);
    return true;
}
// ----------------------------------------------------------------------------
Logger::Line::Type CommandExecutor::GetFilteredOutputError(wxString& output, Logger::Line::Type lineType)
// ----------------------------------------------------------------------------
{
    Logger::Line::Type result = lineType;

    if (output.Contains("No source file named"))
    {
        wxString filename = output.Mid(23); // capture the overly escaped filename
        filename.Replace("\"","");          // remove ending quote of double quotes
        filename.Replace("\\\\", "\\");     // reduce the double double escapes
        filename.Replace(".\\n", "");       // remove the ending newLine

        if (wxFileExists(filename))
        {
            result =  Logger::Line::CommandResult; // say msg is not an error
        }
        else    //msg is a valid error
        {
            // create "No source file named <someFilename>" error msg
            output.Replace("&\"", "");      // remove the & and quote of double quotes
            output = output.Mid(0,21);      // truncate the excessive escaped filename
            output += "\n";                 // add newLine before filename
            output += filename;             // append the newly formated filename
            result = Logger::Line::Error;
        }

    }

    return result;
}
// ----------------------------------------------------------------------------
void CommandExecutor::Clear()
// ----------------------------------------------------------------------------
{
    m_last = 0;
    m_results.clear();

    DoClear();
}

ActionsMap::ActionsMap() :
    m_last_id(1)
{
}

ActionsMap::~ActionsMap()
{
    for(Actions::iterator it = m_actions.begin(); it != m_actions.end(); ++it)
        delete *it;
}

void ActionsMap::Add(Action *action)
{
    action->SetID(m_last_id++);
    m_actions.push_back(action);
}

Action* ActionsMap::Find(int id)
{
    for(Actions::iterator it = m_actions.begin(); it != m_actions.end(); ++it)
    {
        if((*it)->GetID() == id)
            return *it;
    }
    return NULL;
}

Action const * ActionsMap::Find(int id) const
{
    for(Actions::const_iterator it = m_actions.begin(); it != m_actions.end(); ++it)
    {
        if((*it)->GetID() == id)
            return *it;
    }
    return NULL;
}

void ActionsMap::Clear()
{
    for(Actions::iterator it = m_actions.begin(); it != m_actions.end(); ++it)
        delete *it;
    m_actions.clear();
    m_last_id = 1;
}
int waitPreviousstall = 0;
void ActionsMap::Run(CommandExecutor &executor)
{

    if(Empty())
    {
        return;
    }

    Logger *logger = executor.GetLogger();

    bool first = true;
    for(Actions::iterator it = m_actions.begin(); it != m_actions.end(); )
    {
        Action &action = **it;

        if (first)
            action.polledIntervals += 1; // top entry is timed for stalling
        else action.polledIntervals = 0; // others get timed later

        // test if we have a barrier action
        if(action.GetWaitPrevious() && !first)
            break; // not at top yet, let it sleep

        if(not action.Started())
        {
            if(logger)
            {
                logger->Debug(wxString::Format(_T("ActionsMap::Run -> starting action: %p id: %d"),
                                               &action, action.GetID()),
                              Logger::Line::Debug);
            }

            // reset polling for newly started action
            action.polledIntervals = 0;
            action.Start();
        }

        while(action.HasPendingCommands())
        {
            CommandID id;
            wxString const &command = action.PopPendingCommand(id);
            executor.ExecuteSimple(id, command);
            action.polledIntervals = 0; //allow more time for multiple commands
        }

        first = false;
        if(not action.Finished())
        {
            // check for a stalled action
            if (action.polledIntervals > 200) // assumes 20 mil timer
            {
                //caused by unhandled gdb error messages
                action.Finish(); //force Finish() a stalled action
                if(logger)
                    logger->Debug(wxString::Format(_T("ActionsMap::Run -> Error: Forced Finish() on stalled action: %p id: %d"),
                                                   &action, action.GetID()), Logger::Line::Error);
            }

            // set to get next entry
            ++it;
        }
        else //action has finished
        {
            if(logger && action.HasPendingCommands())
            {
                logger->Debug(wxString::Format(_T("ActionsMap::Run -> action[%p id: %d] ")
                                               _T("has pending commands but is being removed"),
                                               &action, action.GetID()),
                              Logger::Line::Debug);
            }
            else
            {
                logger->Debug(wxString::Format(_T("ActionsMap::Run -> action[%p id: %d] Finished()"),
                                               &action, action.GetID()),
                              Logger::Line::Debug);
            }


            delete *it;
            it = m_actions.erase(it);
            if (it == m_actions.begin())
                first = true;
        }
    }//for
}

// ----------------------------------------------------------------------------
} // namespace dbg_mi
// ----------------------------------------------------------------------------
