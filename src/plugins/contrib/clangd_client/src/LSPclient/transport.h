//
// Created by Alex on 2020/1/28.
//

#ifndef LSP_TRANSPORT_H
#define LSP_TRANSPORT_H

#include <wx/event.h> //(ph 2020/10/1)
#include <wx/frame.h> //(ph 2020/10/1)

#include "uri.h" // this is local uri.h not system or wx uri.h
#include <functional>
#include <utility>

#include "manager.h" //(ph 2020/12/14) //for log writes

using value = json;
using RequestID = std::string;

// ----------------------------------------------------------------------------
class MessageHandler
// ----------------------------------------------------------------------------
{
public:
    MessageHandler() = default;
    virtual void onNotify(string_ref method, value &params) {}
    virtual void onResponse(value &ID, value &result) {}
    virtual void onError(value &ID, value &error) {}
    virtual void onRequest(string_ref method, value &params, value &ID) {}
    int id = -1;
    void* clientID = nullptr;
    int   GetLSP_EventID() {return id;}
    void  SetLSP_EventID(int _id) {id = _id;}
    //void* GetLSP_ClientID() {return clientID;}
    //-void  SetLSP_ClientID(void* ID) {clientID = ID;}
    // Termination control from ProcessLanguageClient //(ph 2021/07/8)
    int m_LSP_TerminateFlag = 0;
    int GetLSP_TerminateFlag(){ return m_LSP_TerminateFlag;}
    void SetLSP_TerminateFlag(int terminateFlag){m_LSP_TerminateFlag = terminateFlag;}
};
// ----------------------------------------------------------------------------
class MapMessageHandler : public MessageHandler
// ----------------------------------------------------------------------------
{
    public:
        std::map<std::string, std::function<void(value &, RequestID)>> m_calls;
        std::map<std::string, std::function<void(value &)>> m_notify;
        std::vector<std::pair<RequestID, std::function<void(value &)>>> m_requests;
        MapMessageHandler() = default;
        void SetLSP_EventID(int _id) { MessageHandler::SetLSP_EventID(_id); }

        template<typename Param>
        // ----------------------------------------------------------------------------
        void bindRequest(const char *method, std::function<void(Param &, RequestID)> func)
        // ----------------------------------------------------------------------------
        {
            m_calls[method] = [=](json &params, json &id)
            {
                Param param = params.get<Param>();
                func(param, id.get<RequestID>());
            };
        }
        // ----------------------------------------------------------------------------
        void bindRequest(const char *method, std::function<void(value &, RequestID)> func)
        // ----------------------------------------------------------------------------
        {
            m_calls[method] = std::move(func);
        }
        // ----------------------------------------------------------------------------
        template<typename Param>
        void bindNotify(const char *method, std::function<void(Param &)> func)
        // ----------------------------------------------------------------------------
        {
            m_notify[method] = [=](json &params)
            {
                Param param = params.get<Param>();
                func(param);
            };
        }
        // ----------------------------------------------------------------------------
        void bindNotify(const char *method, std::function<void(value &)> func)
        // ----------------------------------------------------------------------------
        {
            m_notify[method] = std::move(func);
        }
        // ----------------------------------------------------------------------------
        void bindResponse(RequestID id, std::function<void(value &)>func)
        // ----------------------------------------------------------------------------
        {
            m_requests.emplace_back(id, std::move(func));
        }
        // ----------------------------------------------------------------------------
        void onNotify(string_ref method, value &params) override
        // ----------------------------------------------------------------------------
        {
            std::string str = method.str();
            if (m_notify.count(str))
            {
                m_notify[str](params);
            }
        }
        // ----------------------------------------------------------------------------
        void onResponse(value &ID, value &result) override
        // ----------------------------------------------------------------------------
        {
            //-for (int i = 0; i < m_requests.size(); ++i) { //(ph 2020/08/19)  .size is "size_t" not int
            for (size_t i = 0; i < m_requests.size(); ++i)
            {
                if (ID == m_requests[i].first)
                {
                    m_requests[i].second(result);
                    m_requests.erase(m_requests.begin() + i);
                    return;
                }
            }
        }
        // ----------------------------------------------------------------------------
        void onError(value &ID, value &error) override
        // ----------------------------------------------------------------------------
        {

        }
        // ----------------------------------------------------------------------------
        void onRequest(string_ref method, value &params, value &ID) override
        // ----------------------------------------------------------------------------
        {
            std::string string = method.str();
            if (m_calls.count(string))
            {
                m_calls[string](params, ID);
            }
        }
};

// ----------------------------------------------------------------------------
class Transport
// ----------------------------------------------------------------------------
{
    public:
        virtual void notify(string_ref method, value &params) = 0;
        virtual void request(string_ref method, value &params, RequestID &id) = 0;
        virtual int loop(MessageHandler &) = 0;

        wxString GetwxUTF8Str(const std::string stdString)  //(ph 2022/10/01)
        {
            return wxString(stdString.c_str(), wxConvUTF8);
        }

};

// ----------------------------------------------------------------------------
class JsonTransport : public Transport
// ----------------------------------------------------------------------------
{
  public:
    const char *jsonrpc = "2.0";

    // ----------------------------------------------------------------------------
    int loop(MessageHandler &handler) override
    // ----------------------------------------------------------------------------
    {
        wxThreadEvent evt(wxEVT_COMMAND_MENU_SELECTED, handler.GetLSP_EventID());

        while (true)
        {
            if (handler.GetLSP_TerminateFlag()) break; //(ph 2021/07/8)
            try
            {
                json value;

                if (readJson(value))
                {
                    if (value.count("id"))
                    {
                        if (value.contains("method"))
                        {
                            handler.onRequest(value["method"].get<std::string>(), value["params"], value["id"]);

                            evt.SetString("id:method");
                            //evt.SetClientData(&value);
                            //evt.SetClientData(new json(value));
                            evt.SetPayload(new json(value));
                            Manager::Get()->GetAppFrame()->GetEventHandler()->QueueEvent(evt.Clone());

                        }
                        else if (value.contains("result"))
                        {
                            evt.SetString("id:result");
                            //evt.SetClientData(&value);
                            evt.SetPayload(new json(value));
                            Manager::Get()->GetAppFrame()->GetEventHandler()->QueueEvent(evt.Clone());
                        }
                        else if (value.contains("error"))
                        {
                            //-not used- handler.onError(value["id"], value["error"]);

                            evt.SetString("id:error");
                            //evt.SetClientData(&value);
                            evt.SetPayload(new json(value));
                            Manager::Get()->GetAppFrame()->GetEventHandler()->QueueEvent(evt.Clone());

                        }
                    }
                    else if (value.contains("method"))
                    {
                        if (value.contains("params"))
                        {
                            // avoid swamping wxWidges event system with these Clangd messages
                            wxString methodValue = GetwxUTF8Str(value.at("method").get<std::string>());
                            //{"jsonrpc":"2.0","method":"$/progress","params":{"token":"index","value":{"kind":"report","message":"5/6","percentage":83.33333333333333}}}
                            if (methodValue.StartsWith("$/progress"))
                                continue;
                            //{"jsonrpc":"2.0","method":"$Clangd/publishSemanticHighlight","params":{"uri":"file://F%3A/usr/Proj/HelloWxWorld/HelloWxWorldMain.cpp","symbols":[{"id":16045,"parentKind":1
                            if (methodValue.StartsWith("$Clangd/publishSemanticHighlight"))
                                continue;

                            //-not used- handler.onNotify(value["method"].get<std::string>(), value["params"]);

                            evt.SetString("method:params");
                            //evt.SetClientData(&value);
                            evt.SetPayload(new json(value));
                            Manager::Get()->GetAppFrame()->GetEventHandler()->QueueEvent(evt.Clone());
                            if (methodValue == "exit")
                                break; //end the input thread //(ph 2021/01/15)
                        }
                    }
                    else if (value.contains("Exit!")) //(ph 2020/09/26)
                    {
                        evt.SetString("Exit!:Exit!");
                        //evt.SetClientData(&value);
                        evt.SetPayload(new json(value));
                        Manager::Get()->GetAppFrame()->GetEventHandler()->QueueEvent(evt.Clone());
                        break; //exit the thread;
                    }
                }
            }
            catch (std::exception &e)
            {
                //printf("error -> %s\n", e.what());
                wxString msg = wxString::Format("JsonTransport:loop() error -> %s\n", e.what()); //(ph 2020/08/21)
                // This code is in a thread. How to write the message to CB?
            }
        }
        handler.SetLSP_TerminateFlag(2); //report that thread has exited
        return 0;
    }
    // ----------------------------------------------------------------------------
    void notify(string_ref method, value &params) override
    // ----------------------------------------------------------------------------
    {
        json value = {{"jsonrpc", jsonrpc},
            {"method",  method},
            {"params",  params}
        };
        writeJson(value);
    }
    // ----------------------------------------------------------------------------
    void request(string_ref method, value &params, RequestID &id) override
    // ----------------------------------------------------------------------------
    {
        json rpc = {{"jsonrpc", jsonrpc},
            {"id",      id},
            {"method",  method},
            {"params",  params}
        };
        writeJson(rpc);
    }

    virtual bool readJson(value &) = 0;
    virtual bool writeJson(value &) = 0;
};

#endif //LSP_TRANSPORT_H
