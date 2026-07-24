#ifndef _DEBUGGER_MI_CMD_RESULT_PARSER_H_
#define _DEBUGGER_MI_CMD_RESULT_PARSER_H_

#include <algorithm>
#include <vector>
#include <wx/string.h>

namespace dbg_mi
{

class ResultValue
{
    struct Equal
    {
        bool operator()(ResultValue *l, ResultValue *r) const
        {
            return *l == *r;
        }
    };
public:
    typedef std::vector<ResultValue*> Container;
    enum Type
    {
        Simple = 0,
        Array,
        Tuple
    };
public:
    ResultValue() :
        m_type(Simple)
    {
    }
    ResultValue(wxChar const *name, Type type) :
        m_name(name),
        m_type(type)
    {
    }

    bool operator ==(ResultValue const &o) const
    {
        if(m_name == o.m_name && m_type == o.m_type)
        {
            switch(m_type)
            {
            case Simple:
                return m_value.simple == o.m_value.simple;
            case Array:
            case Tuple:
                return std::equal(m_value.tuple.begin(), m_value.tuple.end(), o.m_value.tuple.begin(), Equal());
                break;
            }
        }
        return false;
    }
    bool operator !=(ResultValue const &o) const { return !(*this == o); }

public:

    void SetName(wxString const &name) { m_name = name; }
    void SetSimpleValue(wxString const &value) { assert(m_type == Simple); m_value.simple = value; }


    Type GetType() const { return m_type; }
    void SetType(Type type);
    wxString const & GetName() const { return m_name; }
    wxString const & GetSimpleValue() const { assert(m_type == Simple); return m_value.simple; }

    int GetTupleSize() const { assert(m_type != Simple); return m_value.tuple.size(); }

    void SetTupleValue(ResultValue *value);
    ResultValue const * GetTupleValue(wxString const &key) const;
    ResultValue const* GetTupleValueByIndex(int index) const;
    wxString MakeDebugString() const;
private:
    Container::iterator FindTupleValue(wxChar const *name)
    {
        for(Container::iterator it = m_value.tuple.begin(); it != m_value.tuple.end(); ++it)
        {
            if((*it)->GetName() == name)
                return it;
        }
        return m_value.tuple.end();
    }
    Container::const_iterator FindTupleValue(wxChar const *name) const
    {
        for(Container::const_iterator it = m_value.tuple.begin(); it != m_value.tuple.end(); ++it)
        {
            if((*it)->GetName() == name)
                return it;
        }
        return m_value.tuple.end();
    }
private:
    wxString m_name;
    Type     m_type;

    struct Value
    {
        wxString simple;
        Container tuple;

        friend void swap(Value &lhs, Value &rhs)
        {
            lhs.simple.swap(rhs.simple);
            std::swap(lhs.tuple, rhs.tuple);
        }

        Value() {}
        Value(Value const &v) :
            simple(v.simple)
        {
            for(Container::const_iterator it = v.tuple.begin(); it != v.tuple.end(); ++it)
            {
                tuple.push_back(new ResultValue(**it));
            }
        }
        ~Value()
        {
            for(Container::const_iterator it = tuple.begin(); it != tuple.end(); ++it)
                delete *it;
        }

        Value const& operator =(Value v)
        {
            swap(*this, v);
            return *this;
        }

    } m_value;
};

bool ParseValue(wxString const &str, ResultValue &results, int start = 0);

class ResultParser
{
public:
    enum Type
    {
        Result = 0,
        ExecAsyncOutput,
        StatusAsyncOutput,
        NotifyAsyncOutput,
        TypeUnknown
    };

    enum Class
    {
        ClassDone = 0,
        ClassRunning,
        ClassConnected,
        ClassError,
        ClassExit,
        ClassStopped,
        ClassUnknown
    };

public:

    bool operator ==(ResultParser const &o) const
    {
        return m_type == o.m_type && m_class == o.m_class && m_value == o.m_value;
    }
    bool operator !=(ResultParser const &o) const { return !(*this == o); }

public:
    bool Parse(wxString const &str);
    static Type ParseType(wxString const &str);

    wxString MakeDebugString() const;
    Type GetResultType() const { return m_type; }
    Class GetResultClass() const { return m_class; }

    wxString GetAsyncNotifyType() const { return m_async_type; }

    ResultValue const & GetResultValue() const { return m_value; }

    void SetParseError(bool tf) {m_parseError = tf;}
    bool GetParseError() const {return m_parseError;}

private:
    Type m_type;
    Class m_class;
    ResultValue m_value;
    wxString m_async_type;
    bool m_parseError;
};

inline bool ToInt(ResultValue const &value, int &result_value)
{
    assert(value.GetType() == ResultValue::Simple);

    long l;
    if(value.GetSimpleValue().ToLong(&l, 10))
    {
        result_value = l;
        return true;
    }
    else
        return false;
}

inline bool Lookup(ResultValue const &value, wxString const &name, int &result_value)
{
    assert(value.GetType() != ResultValue::Simple);
    ResultValue const *v = value.GetTupleValue(name);
    if(not v)
        return false;

    return ToInt(*v, result_value);
}

inline bool Lookup(ResultValue const &value, wxString const &name, bool &result_value)
{
    assert(value.GetType() != ResultValue::Simple);
    ResultValue const *v = value.GetTupleValue(name);
    if(not v || v->GetType() != ResultValue::Simple)
        return false;

    wxString const &s = v->GetSimpleValue();

    if(s == _T("true"))
        result_value = true;
    else if(s == _T("false"))
        result_value = false;
    else
        return false;
    return true;
}

inline bool Lookup(ResultValue const &value, wxString const &name, wxString &result_value)
{
    assert(value.GetType() != ResultValue::Simple);
    ResultValue const *v = value.GetTupleValue(name);
    if(not v)
        return false;

    assert(v->GetType() == ResultValue::Simple);
    result_value = v->GetSimpleValue();
    return true;
}


} // namespace dbg_mi

#endif // _DEBUGGER_MI_CMD_RESULT_PARSER_H_
