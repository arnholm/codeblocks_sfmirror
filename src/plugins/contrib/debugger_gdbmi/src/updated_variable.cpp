#include "updated_variable.h"
#include "cmd_result_parser.h"

namespace dbg_mi
{

bool UpdatedVariable::Parse(ResultValue const &output)
{
    m_new_num_children = -1;
    wxString str;
    if(Lookup(output, _T("in_scope"), str))
    {
        if(str == _T("true"))
            m_inscope = InScope_Yes;
        else if(str == _T("false"))
            m_inscope = InScope_No;
        else if(str == _T("invalid"))
            m_inscope = InScope_Invalid;
    }
    else
        return false;

    if(not Lookup(output, _T("name"), m_name))
        return false;

    if(not Lookup(output, _T("type_changed"), m_type_changed))
        return false;

    if(Lookup(output, _T("value"), m_value))
        m_has_value = true;

    if(m_type_changed)
    {
        if(not Lookup(output, _T("new_type"), m_new_type))
            return false;
    }
    if(not Lookup(output, _T("new_num_children"), m_new_num_children))
        m_new_num_children = -1;

    int value;
    if(Lookup(output, _T("has_more"), value))
        m_has_more = (value == 1);
    else
        m_has_more = false;

    if(Lookup(output, _T("dynamic"), value))
        m_dynamic = (value == 1);
    else
        m_dynamic = false;


    return true;
}

wxString UpdatedVariable::MakeDebugString() const
{
    wxString in_scope;

    switch(m_inscope)
    {
    case InScope_Yes:
        in_scope = _T("InScope_Yes");
        break;
    case InScope_No:
        in_scope = _T("InScope_No");
        break;
    case InScope_Invalid:
        in_scope = _T("InScope_Invalid");
        break;
    }

    return wxString::Format(_T("name=%s; value=%s; new_type=%s; in_scope=%s; new_num_children=%d;")
                            _T(" type_changed=%d; has_value=%d; has_more=%d; dynamic=%d;"),
                            m_name.c_str(), m_value.c_str(), m_new_type.c_str(), in_scope.c_str(),
                            m_new_num_children, m_type_changed ? 1 : 0, m_has_value ? 1 : 0,
                            m_has_more ? 1 : 0, m_dynamic ? 1 : 0);
}

} // namespace dbg_mi
