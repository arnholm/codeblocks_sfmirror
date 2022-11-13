#ifndef LSPEVENTCALLBACKHANDLER_H_INCLUDED
#define LSPEVENTCALLBACKHANDLER_H_INCLUDED

// Asynchronous method call events: these event are processed by wxEvtHandler
// itself and result in a call to its Execute() method which simply calls the
// specified method. The difference with a simple method call is that this is
// done asynchronously, i.e. at some later time, instead of immediately when
// the event object is constructed.

#include <deque>
#include "wx/defs.h"
#include "wx/event.h"
#include "wx/window.h"
#include "manager.h"
#include "logmanager.h"

#ifdef HAVE_OVERRIDE
    #define wxOVERRIDE override
#else /*  !HAVE_OVERRIDE */
    #define wxOVERRIDE
#endif /*  HAVE_OVERRIDE */

// This is a base class used to process all method calls.
// ----------------------------------------------------------------------------
class LSPMethodCallbackEvent : public wxEvent
// ----------------------------------------------------------------------------
{
public:
    LSPMethodCallbackEvent(wxObject* object)
        : wxEvent(wxID_ANY, wxEVT_ASYNC_METHOD_CALL)
    {
        SetEventObject(object);
    }

    LSPMethodCallbackEvent(const LSPMethodCallbackEvent& other)
        : wxEvent(other)
    {
    }

    virtual void Execute(wxCommandEvent& event) = 0;
};
// ----------------------------------------------------------------------------
// This is a version for calling methods without parameters.
// ----------------------------------------------------------------------------
template <typename T>
class LSPMethodCallbackEvent0 : public LSPMethodCallbackEvent
{
public:
    typedef T ObjectType;
    typedef void (ObjectType::*MethodType)();

    LSPMethodCallbackEvent0(ObjectType* object,
                            MethodType method)
        : LSPMethodCallbackEvent(object),
          m_object(object),
          m_method(method)
    {
    }

    LSPMethodCallbackEvent0(const LSPMethodCallbackEvent0& other)
        : LSPMethodCallbackEvent(other),
          m_object(other.m_object),
          m_method(other.m_method)
    {
    }

    virtual LSPMethodCallbackEvent* Clone() const wxOVERRIDE
    {
        return new LSPMethodCallbackEvent0(*this);
    }

    virtual void Execute(wxCommandEvent& event) wxOVERRIDE
    {
        (m_object->*m_method)();
    }

private:
    ObjectType* const m_object;
    const MethodType m_method;
};
// ----------------------------------------------------------------------------
// This is a version for calling methods with a single parameter.
// ----------------------------------------------------------------------------
template <typename T, typename T1>
class LSPMethodCallbackEvent1 : public LSPMethodCallbackEvent
{
public:
    typedef T ObjectType;
    typedef void (ObjectType::*MethodType)(T1 x1);
    typedef typename wxRemoveRef<T1>::type ParamType1;

    LSPMethodCallbackEvent1(ObjectType* object,
                            MethodType method,
                            const ParamType1& x1)
        : LSPMethodCallbackEvent(object),
          m_object(object),
          m_method(method),
          m_param1(x1)
    {
    }

    LSPMethodCallbackEvent1(const LSPMethodCallbackEvent1& other)
        : LSPMethodCallbackEvent(other),
          m_object(other.m_object),
          m_method(other.m_method),
          m_param1(other.m_param1)
    {
    }

    virtual LSPMethodCallbackEvent* Clone() const wxOVERRIDE
    {
        return new LSPMethodCallbackEvent1(*this);
    }

    virtual void Execute(wxCommandEvent& event) wxOVERRIDE
    {
        (m_object->*m_method)(event);
    }

private:
    ObjectType* const m_object;
    const MethodType m_method;
    /*const*/ ParamType1 m_param1;
};

// ----------------------------------------------------------------------------
// This is a version for calling methods with two parameters.
// ----------------------------------------------------------------------------
template <typename T, typename T1, typename T2>
class LSPMethodCallbackEvent2 : public LSPMethodCallbackEvent
{
public:
    typedef T ObjectType;
    typedef void (ObjectType::*MethodType)(T1 x1, T2 x2);
    typedef typename wxRemoveRef<T1>::type ParamType1;
    typedef typename wxRemoveRef<T2>::type ParamType2;

    LSPMethodCallbackEvent2(ObjectType* object,
                            MethodType method,
                            const ParamType1& x1,
                            const ParamType2& x2)
        : LSPMethodCallbackEvent(object),
          m_object(object),
          m_method(method),
          m_param1(x1),
          m_param2(x2)
    {
    }

    LSPMethodCallbackEvent2(const LSPMethodCallbackEvent2& other)
        : LSPMethodCallbackEvent(other),
          m_object(other.m_object),
          m_method(other.m_method),
          m_param1(other.m_param1),
          m_param2(other.m_param2)
    {
    }

    virtual LSPMethodCallbackEvent* Clone() const wxOVERRIDE
    {
        return new LSPMethodCallbackEvent2(*this);
    }

    virtual void Execute(wxCommandEvent& event) wxOVERRIDE
    {
        (m_object->*m_method)(event, m_param2);
    }

private:
    ObjectType* const m_object;
    const MethodType m_method;
    const ParamType1 m_param1;
    const ParamType2 m_param2;
};

// ----------------------------------------------------------------------------
// This is a version for calling any functors
// ----------------------------------------------------------------------------
template <typename T>
class LSPMethodCallbackEventFunctor : public LSPMethodCallbackEvent
{
public:
    typedef T FunctorType;

    LSPMethodCallbackEventFunctor(wxObject *object, const FunctorType& fn)
        : LSPMethodCallbackEvent(object),
          m_fn(fn)
    {
    }

    LSPMethodCallbackEventFunctor(const LSPMethodCallbackEventFunctor& other)
        : LSPMethodCallbackEvent(other),
          m_fn(other.m_fn)
    {
    }

    virtual LSPMethodCallbackEvent* Clone() const wxOVERRIDE
    {
        return new LSPMethodCallbackEventFunctor(*this);
    }

    virtual void Execute(wxCommandEvent& event) wxOVERRIDE
    {
        m_fn();
    }

private:
    FunctorType m_fn;
};//end class LSPMethodCallbackEventFunctor


// ----------------------------------------------------------------------------
class LSPEventCallbackHandler: public wxEvtHandler
// ----------------------------------------------------------------------------
{
  private:
    // -------------------------------------------------------
    //LSP callbacks
    // -------------------------------------------------------
    typedef std::multimap<int, LSPMethodCallbackEvent*> LSPEventCallbackMap;
    LSPEventCallbackMap m_LSPEventCallbackQueue;
    size_t m_RRIDsequence = 0;

  public:
    // ----------------------------------------------------------------------------
    void OnLSPEventCallback(int lspRRID, wxCommandEvent& event)
    // ----------------------------------------------------------------------------
    {
        // ----------------------------------------------------------------------------
        // Invoke the first queued callback that matches lspRRID (LSP RequestResponse ID)
        // ----------------------------------------------------------------------------
        if (m_LSPEventCallbackQueue.count(lspRRID))
        {
            //    // **Debugging**  show queue contents
            //    LogManager* pLogMgr = Manager::Get()->GetLogManager();
            //    for (auto const& x : m_LSPEventCallbackQueue)
            //    {
            //        wxString msg = wxString::Format("%s Key:%i value: %p", __FUNCTION__, x.first, x.second);
            //        pLogMgr->DebugLog(msg);
            //    }

            LSPEventCallbackMap::iterator mit = m_LSPEventCallbackQueue.find(lspRRID);
            if (mit != m_LSPEventCallbackQueue.end())
            {
                for (LSPEventCallbackMap::iterator it = m_LSPEventCallbackQueue.begin(); it != m_LSPEventCallbackQueue.end();)
                {
                    if (it->first == lspRRID)
                    {
                        LSPMethodCallbackEvent* pAsyncCall = it->second;
                        LSPMethodCallbackEvent* pCall =  (LSPMethodCallbackEvent*)pAsyncCall->Clone();
                        it = m_LSPEventCallbackQueue.erase(it); //remove the entry before doing call !!
                        delete(pAsyncCall);
                        pCall->Execute(event);
                        delete(pCall);
                        return; //leave other entries for their matching (incoming) response
                    }
                    ++it;
                }
                return;
            }//endif mit
        }//endif m_EventSinks

    }

    size_t Count(){return m_LSPEventCallbackQueue.size();}

    // Verify that an event handler is still in the chain of event handlers
    // -------------------------------------------------------------
    wxEvtHandler* FindEventHandler(wxEvtHandler* pEvtHdlr)
    // -------------------------------------------------------------
    {
        wxEvtHandler* pFoundEvtHdlr =  Manager::Get()->GetAppWindow()->GetEventHandler();

        while (pFoundEvtHdlr != nullptr)
        {
            if (pFoundEvtHdlr == pEvtHdlr)
                return pFoundEvtHdlr;
            pFoundEvtHdlr = pFoundEvtHdlr->GetNextHandler();
        }
        return nullptr;
    }

    // ----------------------------------------------------------------------------
    LSPEventCallbackHandler()
    // ----------------------------------------------------------------------------
    {
        //ctor
        //LSPEventCallbackHandler* pThis = this;
        //Manager::Get()->GetAppWindow()->PushEventHandler(pThis);
    }
    // ----------------------------------------------------------------------------
    ~LSPEventCallbackHandler()
    // ----------------------------------------------------------------------------
    {
        //dtor
        if (FindEventHandler(this))
                Manager::Get()->GetAppWindow()->RemoveEventHandler(this);
    }

    // GetLSPEventCallackQueue ptr
    // ----------------------------------------------------------------------------
    std::multimap<int, LSPMethodCallbackEvent*>* GetLSPEventCallbackQueue()
    // ----------------------------------------------------------------------------
        { return &m_LSPEventCallbackQueue; }

    // ----------------------------------------------------------------------------
    void ClearLSPEventCallbacks()
    // ----------------------------------------------------------------------------
    {
        m_LSPEventCallbackQueue.clear();
    }

    // ----------------------------------------------------------------------------
    void ClearLSPEventCallback(int lspRRID)
    // ----------------------------------------------------------------------------
    {
        // clear any call backs for this response type
        LSPEventCallbackMap::iterator mit = m_LSPEventCallbackQueue.find(lspRRID);
        while ((mit = m_LSPEventCallbackQueue.find(lspRRID)) != m_LSPEventCallbackQueue.end())
        {
            m_LSPEventCallbackQueue.erase(mit);
        }
    }

    // ----------------------------------------------------------------------------
    // Method callback with no parameters
    // ----------------------------------------------------------------------------
    template <typename ID, typename TP, typename T>
    size_t LSP_RegisterEventSink(ID id, TP* thisptr, void (T::*method)())
    {
        //-        QueueEvent(
        //-            new LSPMethodCallbackEvent1<T, T1>(
        //-                static_cast<T*>(this), method, x1)
        //-        );
        LSPMethodCallbackEvent* pCallBackEvent = new LSPMethodCallbackEvent0<T>(
                                                    static_cast<TP*>(thisptr), method) ;
        m_RRIDsequence += 1;
        m_LSPEventCallbackQueue.insert(std::pair<int,LSPMethodCallbackEvent*>(m_RRIDsequence, pCallBackEvent));
        return m_RRIDsequence;
    }

    // Notice that we use P1 and not T1 for the parameter to allow passing
    // parameters that are convertible to the type taken by the method
    // instead of being exactly the same, to be closer to the usual method call
    // semantics.
    // ----------------------------------------------------------------------------
    // Call back with one parameter
    // ----------------------------------------------------------------------------
    template <typename ID, typename TP, typename T, typename T1, typename P1>
    size_t LSP_RegisterEventSink(ID id, TP* thisptr, void (T::*method)(T1 x1), P1 x1)
    {
        //        QueueEvent(
        //            new LSPMethodCallbackEvent1<T, T1>(
        //                static_cast<T*>(this), method, x1)
        //        );
        LSPMethodCallbackEvent* pCallBackEvent = new LSPMethodCallbackEvent1<T, T1>(
                                                    static_cast<TP*>(thisptr), method, x1) ;
        m_RRIDsequence += 1;
        m_LSPEventCallbackQueue.insert(std::pair<int,LSPMethodCallbackEvent*>(m_RRIDsequence, pCallBackEvent));

        return m_RRIDsequence;
    }
    // ----------------------------------------------------------------------------
    // Call back with two parameters
    // ----------------------------------------------------------------------------
    template <typename ID, typename TP, typename T, typename T1, typename T2, typename P1, typename P2>
    size_t LSP_RegisterEventSink(ID id, TP* thisptr, void (T::*method)(T1 x1, T2 x2), P1 x1, P2 x2)
    {
        LSPMethodCallbackEvent* pCallBackEvent = new LSPMethodCallbackEvent2<T, T1, T2>(
                                                    static_cast<TP*>(thisptr), method, x1, x2);
        m_RRIDsequence += 1;
        m_LSPEventCallbackQueue.insert(std::pair<int,LSPMethodCallbackEvent*>(id, pCallBackEvent));
        return m_RRIDsequence;
    }
//    // ----------------------------------------------------------------------------
//    // Call back a functor (does not compile when not in same class as caller
//    // ----------------------------------------------------------------------------
//    template <typename T>
//    void LSP_RegisterEventSink(const T& fn)
//    {
//        //-QueueEvent(new wxLSPMethodCallbackEventFunctor<T>(this, fn));
//        LSPMethodCallbackEventFunctor<T>* pCallBackEvent = (new LSPMethodCallbackEventFunctor<T>(this, fn));
//        m_LSPEventCallbackQueue.push_back(pCallBackEvent);
//    }
    // ----------------------------------------------------------------------------
    // Call back a functor
    // ----------------------------------------------------------------------------
    template <typename ID, typename T>
    size_t LSP_RegisterEventSinkFunctor(ID id, wxObject* pThis, const T& fn)
    {
        //-QueueEvent(new wxLSPMethodCallbackEventFunctor<T>(this, fn));
        m_RRIDsequence += 1;
        LSPMethodCallbackEventFunctor<T>* pCallBackEvent = (new LSPMethodCallbackEventFunctor<T>(pThis, fn));
        m_LSPEventCallbackQueue.insert(std::pair<int,LSPMethodCallbackEvent*>(id, pCallBackEvent));
        return m_RRIDsequence;
    }

};

#endif // LSPEVENTCALLBACKHANDLER_H_INCLUDED
