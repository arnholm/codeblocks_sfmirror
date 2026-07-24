#include "frame.h"

#include "cmd_result_parser.h"

namespace dbg_mi
{

bool Frame::ParseOutput(ResultValue const &output_value)
{
    if(output_value.GetType() != ResultValue::Tuple)
        return false;

    dbg_mi::ResultValue const *frame_value = output_value.GetTupleValue(_T("frame"));
    if(not frame_value)
        return false;
    return ParseFrame(*frame_value);
}

bool Frame::ParseFrame(ResultValue const &frame_value)
{
    ResultValue const *function = frame_value.GetTupleValue(_T("func"));
    if(function)
        m_function = function->GetSimpleValue();
    ResultValue const *address = frame_value.GetTupleValue(_T("addr"));
    if(address)
    {
        wxString const &str = address->GetSimpleValue();
        //if(not str.ToULong(&m_address, 16)) //(ph 2024/03/11)
        if(not str.ToULongLong(&m_address, 16)) //(ph 2024/03/11)
            return false;
    }

    ResultValue const *from = frame_value.GetTupleValue(_T("from"));
    if(from)
        m_from = from->GetSimpleValue();

    ResultValue const *line = frame_value.GetTupleValue(_T("line"));
    ResultValue const *filename = frame_value.GetTupleValue(_T("file"));
    ResultValue const *full_filename = frame_value.GetTupleValue(_T("fullname"));

    if(not line && !filename && !full_filename)
    {
        m_has_valid_source = false;
        return true;
    }
    if((not line || line->GetType() != ResultValue::Simple)
       || (not filename || filename->GetType() != ResultValue::Simple)
       || (not full_filename || full_filename->GetType() != ResultValue::Simple))
    {
        return false;
    }

    //m_filename = filename->GetSimpleValue();
    // use full_filename, else double-click on frame will get wrong file
    m_filename = full_filename->GetSimpleValue();
    m_full_filename = full_filename->GetSimpleValue();
    long long_line;
    if(not line->GetSimpleValue().ToLong(&long_line))
        return false;

    m_line = long_line;
    m_has_valid_source = true;

    return true;
}

FrameArguments::FrameArguments() :
    m_stack_args(NULL)
{
}

bool FrameArguments::Attach(ResultValue const &output)
{
    if(output.GetType() != ResultValue::Tuple)
        return false;

    m_stack_args = output.GetTupleValue(_T("stack-args"));
    return m_stack_args;
}

int FrameArguments::GetCount() const
{
    return m_stack_args->GetTupleSize();
}

bool FrameArguments::GetFrame(int index, wxString &args) const
{
    ResultValue const *frame = m_stack_args->GetTupleValueByIndex(index);
    if(not frame || frame->GetName() != _T("frame"))
        return false;

    return ParseFrame(*frame, args);
}

bool FrameArguments::ParseFrame(ResultValue const &frame_value, wxString &args)
{
    args = wxEmptyString;

    ResultValue const *args_tuple = frame_value.GetTupleValue(_T("args"));
    if(not args_tuple || args_tuple->GetType() != ResultValue::Array)
        return false;

    for(int ii = 0; ii < args_tuple->GetTupleSize(); ++ii)
    {
        ResultValue const *arg = args_tuple->GetTupleValueByIndex(ii);
        assert(arg);

        ResultValue const *name = arg->GetTupleValue(_T("name"));
        ResultValue const *value = arg->GetTupleValue(_T("value"));

        if(name && name->GetType() == ResultValue::Simple
           && value && value->GetType() == ResultValue::Simple)
        {
            if(not args.empty())
                args += _T(", ");
            args += name->GetSimpleValue() + _T("=") + value->GetSimpleValue();
        }
        else
            return false;
    }

    return true;
}


StoppedReason StoppedReason::Parse(ResultValue const &value)
{
    //-ResultValue const *reason = value.GetTupleValue(_T("reason")); //causes assert //(ph 2025/01/22)
    ResultValue const *reason = nullptr;
    wxString reasonStr = wxString();
    if (value.GetType() == value.Tuple) reason = value.GetTupleValue(_T("reason"));
    if (value.GetType() == value.Simple) reasonStr = value.GetSimpleValue();
    if(not (reason or reasonStr.Length()))
        return Unknown;
    wxString const& str = reason ? reason->GetSimpleValue() : reasonStr;
    if(str == _T("breakpoint-hit"))
        return BreakpointHit;
    else if(str == _T("exited-signalled"))
        return ExitedSignalled;
    else if(str == _T("exited"))
        return Exited;
    else if(str == _T("exited-normally"))
        return ExitedNormally;
    else if(str == _T("signal-received"))
        return SignalReceived;
    else if(str == _T("watchpoint-scope"))
        return BreakpointHit;
    else if(str == _T("location-reached"))
        return LocationReached;
    else if(str == _T("end-stepping-range"))
        return EndSteppingRange;
    else
        return Unknown;
}

} // namespace dbg_mi
