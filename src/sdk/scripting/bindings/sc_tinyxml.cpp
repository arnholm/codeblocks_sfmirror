
#include <sdk_precomp.h>
#ifndef CB_PRECOMP
#include "tinyxml.h"
#include "scriptingmanager.h"
#endif // CB_PRECOMP

#include "scripting/bindings/sc_utils.h"
#include "scripting/bindings/sc_typeinfo_all.h"
#include <type_traits>

namespace ScriptBindings
{
const char* extractString(HSQUIRRELVM v, const int pos, ExtractParamsBase& extractor, const char* name)
{
    const SQObjectType type = sq_gettype(v, pos);
    switch (type)
    {
    case OT_STRING:
    {
        // Construct from Squirrel string
        const SQChar *value = extractor.GetParamString(pos);
        cbAssert(value);
        return value;
    }
    case OT_INSTANCE:   // wxString
    {
        const wxString *value = nullptr;
        if (!extractor.ProcessParam(value, pos, name))
        {
            extractor.ErrorMessage();
            return nullptr;
        }
        return value->c_str();
    }
    default:
        return nullptr;
    }
}

SQInteger TiXmlDocument_ctor(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, "TiXmlDocument_Ctor"))
        return extractor.ErrorMessage();
    const int numArgs = sq_gettop(v);

    if (numArgs == 1) // empty ctor
    {
        UserDataForType<TiXmlDocument> *data;
        data = SetupUserPointer<TiXmlDocument, InstanceAllocationMode::InstanceIsInline>(v, 1);
        if (!data)
            return -1; // SetupUserPointer should have called sq_throwerror!
        new (&(data->userdata)) TiXmlDocument();
        return 0;
    }
    else
    {
        // 1 argument ctor
        const char* value = extractString(v, 2, extractor, "TiXmlDocument_ctor");
        if (!value)
            return -1;
        UserDataForType<TiXmlDocument> *data;
        data = SetupUserPointer<TiXmlDocument, InstanceAllocationMode::InstanceIsInline>(v, 1);
        if (!data)
            return -1; // SetupUserPointer should have called sq_throwerror!
        new (&(data->userdata)) TiXmlDocument(value);
        return 0;
    }
}

SQInteger TiXmlDocument_Accept(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, "TiXmlDocument_Accept"))
        return extractor.ErrorMessage();

    TiXmlDocument *self = nullptr;
    if (!extractor.ProcessParam(self, 1, "TiXmlDocument_Accept"))
        return extractor.ErrorMessage();

    TiXmlPrinter *printer = nullptr;
    if (!extractor.ProcessParam(printer, 2, "TiXmlDocument_Accept"))
        return extractor.ErrorMessage();

    sq_pushbool(v, self->Accept(printer));
    return 1;
}

SQInteger TiXMLDocument_LoadFile(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, "TiXMLDocument_loadFile"))
        return extractor.ErrorMessage();

    // Get self
    TiXmlDocument *self = nullptr;
    if (!extractor.ProcessParam(self, 1, "TiXMLDocument_loadFile"))
    {
        extractor.ErrorMessage();
        return -1;
    }

    bool ret = false;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1) // empty function
    {
        ret = self->LoadFile();
    }
    else
    {
        const char* value = extractString(v, 2, extractor, "TiXMLDocument_loadFile");
        if (!value)
            return -1;
        ret =  self->LoadFile(value);
    }
    sq_pushbool(v, ret);
    return 1;
}

SQInteger TiXMLDocument_SaveFile(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, "TiXMLDocument_SaveFile"))
        return extractor.ErrorMessage();

    // Get self
    TiXmlDocument *self = nullptr;
    if (!extractor.ProcessParam(self, 1, "TiXMLDocument_SaveFile"))
    {
        extractor.ErrorMessage();
        return -1;
    }

    bool ret = false;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1) // empty function
    {
        ret = self->SaveFile();
    }
    else
    {
        const char* value = extractString(v, 2, extractor, "TiXMLDocument_SaveFile");
        if (!value)
            return -1;
        ret =  self->SaveFile(value);
    }
    sq_pushbool(v, ret);
    return 1;
}

SQInteger TiXmlDocument_RootElement(HSQUIRRELVM v)
{
    ExtractParams1<TiXmlDocument*> extractor(v);
    if (!extractor.Process("TiXmlDocument_RootElement"))
        return extractor.ErrorMessage();
    return ConstructAndReturnNonOwnedPtrOrNull(v, extractor.p0->RootElement());
}

SQInteger TiXmlDocument_Error(HSQUIRRELVM v)
{
    ExtractParams1<TiXmlDocument*> extractor(v);
    if (!extractor.Process("TiXmlDocument_Error"))
        return extractor.ErrorMessage();
    sq_pushbool(v, extractor.p0->Error());
    return 1;
}


SQInteger TiXmlDocument_Parse(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, "TiXmlDocument_Parse"))
        return extractor.ErrorMessage();

    // Get self
    TiXmlDocument *self = nullptr;
    if (!extractor.ProcessParam(self, 1, "TiXmlDocument_Parse"))
        return extractor.ErrorMessage();


    const char* value = extractString(v, 2, extractor, "TiXmlDocument_Parse");
    if (!value)
        return -1;

    sq_pushstring(v, self->Parse(value), -1);
    return 1;
}

SQInteger TiXmlElement_ctor(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, "TiXmlElement_ctor"))
        return extractor.ErrorMessage();
    const int numArgs = sq_gettop(v);

    if (numArgs == 2)
    {
        // 1 argument ctor
        const char* value = extractString(v, 2, extractor, "TiXmlElement_ctor");
        if (!value)
            return -1;
        UserDataForType<TiXmlElement> *data;
        data = SetupUserPointer<TiXmlElement, InstanceAllocationMode::InstanceIsInline>(v, 1);
        if (!data)
            return -1; // SetupUserPointer should have called sq_throwerror!
        new (&(data->userdata)) TiXmlElement(value);
        return 0;
    }
    return sq_throwerror(v, _SC("Unsupported argument count passed to TiXmlElement constructor!"));
}

SQInteger TiXmlAttribute_ctor(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 3, "TiXmlAttribute_ctor"))
        return extractor.ErrorMessage();
    const int numArgs = sq_gettop(v);

    if (numArgs == 1) // empty ctor
    {
        UserDataForType<TiXmlAttribute> *data;
        data = SetupUserPointer<TiXmlAttribute, InstanceAllocationMode::InstanceIsInline>(v, 1);
        if (!data)
            return -1; // SetupUserPointer should have called sq_throwerror!
        new (&(data->userdata)) TiXmlAttribute();
        return 0;
    }
    if (numArgs == 3)
    {
        // 2 argument ctor
        const char* name = extractString(v, 2, extractor, "TiXmlAttribute_ctor");
        const char* value = extractString(v, 3, extractor, "TiXmlAttribute_ctor");
        if (!name || !value)
            return -1;
        UserDataForType<TiXmlAttribute> *data;
        data = SetupUserPointer<TiXmlAttribute, InstanceAllocationMode::InstanceIsInline>(v, 1);
        if (!data)
            return -1; // SetupUserPointer should have called sq_throwerror!
        new (&(data->userdata)) TiXmlAttribute(name, value);
        return 0;
    }
    return sq_throwerror(v, _SC("Unsupported argument count passed to TiXmlAttribute constructor!"));
}


// this template construct accepts const char* T::func(const char*) and const char* T::func(const char*) const
template<typename T> using TiXMLStrToStrFunc = typename std::conditional<std::is_const<T>::value, const char *(T::*)(const char*) const, const char *(T::*)(const char*)>::type;
template<typename T, TiXMLStrToStrFunc<T> func>
SQInteger GetStringReturnString(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, _SC("GetStringReturnString")))
        return extractor.ErrorMessage();

    // Get self
    T *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("GetStringReturnString")))
        return extractor.ErrorMessage();

    const char* ret = nullptr;
    const int numArgs = sq_gettop(v);
    if (numArgs == 2) // first instance then String parameter
    {
        const char* value = extractString(v, 2, extractor, "GetStringReturnString");
        if (!value)
            return -1;
        ret = (self->*func)(value);
    }
    sq_pushstring(v, ret, -1);
    return 1;
}


// this template construct accepts const char* T::func() and const char* T::func() const
template<typename T> using TiXMLStrFunc = typename std::conditional<std::is_const<T>::value, const char *(T::*)() const, const char *(T::*)()>::type;
template<typename T, TiXMLStrFunc<T> func>
SQInteger GetReturnString(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 1, _SC("GetReturnString")))
        return extractor.ErrorMessage();

    // Get self
    T *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("GetReturnString")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    const char* ret = (self->*func)();
    sq_pushstring(v, ret, -1);
    return 1;
}

template<class T> using TiXMLIntFunc = int (T::*)() const;
template<typename T, TiXMLIntFunc<T> func>
SQInteger GetReturnInt(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 1, _SC("GetReturnInt")))
        return extractor.ErrorMessage();

    // Get self
    T *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("GetReturnInt")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    int ret = (self->*func)();
    sq_pushinteger(v, ret);
    return 1;
}

template<class T> using TiXMLDoString = void (T::*)(const char*);
template<typename T, TiXMLDoString<T> func>
SQInteger DoStringFunc(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, _SC("DoStringFunc")))
        return extractor.ErrorMessage();

    // Get self
    T *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("DoStringFunc")))
    {
        extractor.ErrorMessage();
        return -1;
    }
    const char* value = extractString(v, 2, extractor, "DoStringFunc");
    if (!value)
        return -1;
    (self->*func)(value);
    return 0;
}

template<class T> using TiXMLDoubleFunc = double (T::*)() const;
template<typename T, TiXMLDoubleFunc<T> func>
SQInteger GetReturnDouble(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 1, _SC("GetReturnDouble")))
        return extractor.ErrorMessage();

    // Get self
    T *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("GetReturnDouble")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    double ret = (self->*func)();
    sq_pushfloat(v, ret);
    return 1;
}

SQInteger TiXmlElement_FirstChildElement(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, _SC("TiXmlElement_FirstChildElement")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_FirstChildElement")))
        return extractor.ErrorMessage();


    TiXmlElement* ret = nullptr;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1)
        ret = self->FirstChildElement();
    if (numArgs == 2)
    {
        const char* value = extractString(v, 2, extractor, "TiXmlElement_FirstChildElement");
        if (!value)
            return -1;
        ret = self->FirstChildElement(value);
    }
    return ConstructAndReturnNonOwnedPtrOrNull(v, ret);
}

SQInteger TiXmlElement_NextSiblingElement(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, _SC("TiXmlElement_NextSiblingElement")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_NextSiblingElement")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    TiXmlElement* ret = nullptr;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1)
        ret = self->NextSiblingElement();
    else if (numArgs == 2)
    {
        const char* value = extractString(v, 2, extractor, "TiXmlElement_NextSiblingElement");
        if (!value)
            return -1;
        ret = self->NextSiblingElement(value);
    }
    return ConstructAndReturnNonOwnedPtrOrNull(v, ret);
}

SQInteger TiXmlElement_LastChild(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, _SC("TiXmlElement_LastChild")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_LastChild")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    TiXmlNode* ret = nullptr;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1)
        ret = self->LastChild();
    if (numArgs == 2)
    {
        const char* value = extractString(v, 2, extractor, "TiXmlElement_LastChild");
        if (!value)
            return -1;
        ret = self->LastChild(value);
    }
    return ConstructAndReturnNonOwnedPtrOrNull(v, ret);
}

SQInteger TiXmlElement_LastChildElement(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, _SC("TiXmlElement_LastChildElement")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_LastChildElement")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    TiXmlNode* node = nullptr;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1)
        node = self->LastChild();
    else if (numArgs == 2)
    {
        const char* value = extractString(v, 2, extractor, "TiXmlElement_LastChildElement");
        if (!value)
            return -1;
        node = self->LastChild(value);
    }

    TiXmlElement* ret = nullptr;
    if (node != nullptr)
        ret = node->ToElement();

    return ConstructAndReturnNonOwnedPtrOrNull(v, ret);
}

SQInteger TiXmlElement_PreviousSibling(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, _SC("TiXmlElement_PreviousSibling")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_PreviousSibling")))
    {
        extractor.ErrorMessage();
        return -1;
    }

    TiXmlNode* ret = nullptr;
    const int numArgs = sq_gettop(v);
    if (numArgs == 1)
        ret = self->PreviousSibling();
    if (numArgs == 2)
    {
        const char* value = extractString(v, 2, extractor, "TiXmlElement_PreviousSibling");
        if (!value)
            return -1;
        ret = self->PreviousSibling(value);
    }
    return ConstructAndReturnNonOwnedPtrOrNull(v, ret);
}

SQInteger TiXmlElement_GetAttribute(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, _SC("TiXmlElement_GetAttribute")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_GetAttribute")))
    {
        extractor.ErrorMessage();
        return false;
    }

    const SQChar *name = extractString(v, 2, extractor, "TiXmlElement_GetAttribute");
    if(!name)
        return -1;

    TiXmlAttribute* attr = self->FirstAttribute();
    while(attr != nullptr)
    {
        if(wxStrcmp(attr->Name(), name) == 0)
            return ConstructAndReturnNonOwnedPtrOrNull(v, attr);
        attr = attr->Next();
    }

    sq_pushnull(v);
    return 1;
}

SQInteger TiXmlElement_FirstAttribute(HSQUIRRELVM v)
{
    ExtractParams1<TiXmlElement*> extractor(v);
    if (!extractor.Process("TiXmlElement_FirstAttribute"))
        return extractor.ErrorMessage();
    return ConstructAndReturnNonOwnedPtrOrNull(v, extractor.p0->FirstAttribute());
}

SQInteger TiXmlElement_LastAttribute(HSQUIRRELVM v)
{
    ExtractParams1<TiXmlElement*> extractor(v);
    if (!extractor.Process("TiXmlElement_LastAttribute"))
        return extractor.ErrorMessage();
    return ConstructAndReturnNonOwnedPtrOrNull(v, extractor.p0->LastAttribute());
}


SQInteger TiXmlAttribute_Next(HSQUIRRELVM v)
{
    ExtractParams1<TiXmlAttribute*> extractor(v);
    if (!extractor.Process("TiXmlAttribute_Next"))
        return extractor.ErrorMessage();
    return ConstructAndReturnNonOwnedPtrOrNull(v, extractor.p0->Next());
}

SQInteger TiXmlElement_SetAttribute(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(3, 3, _SC("TiXmlElement_SetAttribute")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlElement *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("TiXmlElement_SetAttribute")))
    {
        extractor.ErrorMessage();
        return false;
    }

    const SQChar *name = extractString(v, 2, extractor, "TiXmlElement_SetAttribute");
    if(!name)
        return -1;

    const SQObjectType type = sq_gettype(v, 3);
    switch (type)
    {
    case OT_STRING:
    {
        // Construct from Squirrel string
        const SQChar *value = extractor.GetParamString(3);
        cbAssert(value);
        self->SetAttribute(name, value);
        return 0;
    }
    case OT_INSTANCE:   // wxString
    {
        const wxString *value = nullptr;
        if (!extractor.ProcessParam(value, 3, "TiXmlElement_SetAttribute"))
        {
            extractor.ErrorMessage();
            return -1;
        }
        self->SetAttribute(name, value->c_str());
        return 0;
    }
    case OT_INTEGER:
    {
        int value = extractor.GetParamInt(3);
        self->SetAttribute(name, value);
        return 0;
    }
    case OT_FLOAT:
    {
        float value = extractor.GetParamFloat(3);
        self->SetDoubleAttribute(name, value);
        return 0;
    }
    default:
        return -1;
    }
    return 0;
}

SQInteger TiXmlHandle_ctor(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(2, 2, "TiXmlHandle_ctor"))
        return extractor.ErrorMessage();

    const int numArgs = sq_gettop(v);
    if (numArgs == 2)
    {
        // 1 argument ctor
        TiXmlNode* node = nullptr;
        TiXmlHandle* handle = nullptr;
        if (extractor.ProcessParam(node, 2, _SC("TiXmlHandle_ctor")))
        {
            if (node == nullptr)
                return -1;
            UserDataForType<TiXmlHandle> *data;
            data = SetupUserPointer<TiXmlHandle, InstanceAllocationMode::InstanceIsInline>(v, 1);
            if (!data)
                return -1; // SetupUserPointer should have called sq_throwerror!
            new (&(data->userdata)) TiXmlHandle(node);
            return 0;
        }
        else if (extractor.ProcessParam(handle, 2, _SC("TiXmlHandle_ctor")))
        {
            if (handle == nullptr)
                return -1;
            UserDataForType<TiXmlHandle> *data;
            data = SetupUserPointer<TiXmlHandle, InstanceAllocationMode::InstanceIsInline>(v, 1);
            if (!data)
                return -1; // SetupUserPointer should have called sq_throwerror!
            new (&(data->userdata)) TiXmlHandle(*handle);
            return 0;
        }
        else
        {
            return sq_throwerror(v, _SC("Unsupported argument type passed to TiXmlHandle constructor!"));
        }

    }
    return sq_throwerror(v, _SC("Unsupported argument count passed to TiXmlHandle constructor!"));
}


using TiXMLStringHandleC = TiXmlHandle (TiXmlHandle::*)(const char*) const;
using TiXMLEmptyHandleC = TiXmlHandle (TiXmlHandle::*)() const;
template<TiXMLEmptyHandleC empFunc, TiXMLStringHandleC func>
SQInteger GetTiXMLHandleEmtyOrString(HSQUIRRELVM v)
{
    ExtractParamsBase extractor(v);
    if (!extractor.CheckNumArguments(1, 2, _SC("GetTiXMLHandle")))
        return extractor.ErrorMessage();

    // Get self
    TiXmlHandle *self = nullptr;
    if (!extractor.ProcessParam(self, 1, _SC("GetTiXMLHandleEmtyOrString")))
    {
        extractor.ErrorMessage();
        return -1;
    }
    const int numArgs = sq_gettop(v);
    if (numArgs == 1)
    {
        return ConstructAndReturnInstance(v, (self->*empFunc)());
    }
    else if (numArgs == 2)
    {
        const SQChar *name = extractString(v, 2, extractor, "GetTiXMLHandleEmtyOrString");
        if(!name)
            return -1;

        return ConstructAndReturnInstance(v, (self->*func)(name));
    }
    return sq_throwerror(v, _SC("Unsupported argument type passed to GetTiXMLHandleEmtyOrString!"));
}

template<class T, class R> using TiXMLRetPtr = R* (T::*)() const;
template<class T, class R, TiXMLRetPtr<T, R> func>
SQInteger TiXml_RetPtr(HSQUIRRELVM v)
{
    ExtractParams1<T*> extractor(v);
    if (!extractor.Process("TiXml_RetPtr"))
        return extractor.ErrorMessage();
    return ConstructAndReturnNonOwnedPtrOrNull(v, (extractor.p0->*func)());
}


void Register_TinyXMLBindings(HSQUIRRELVM v, ScriptingManager *manager)
{
    PreserveTop preserveTop(v);
    sq_pushroottable(v);

    {
        // Register TiXmlPrinter
        const SQInteger classDecl = CreateClassDecl<TiXmlPrinter>(v);
        BindEmptyCtor<TiXmlPrinter>(v);

        BindMethod(v, _SC("CStr"), GetReturnString<TiXmlPrinter, &TiXmlPrinter::CStr>, _SC("TiXmlPrinter::CStr"));
        BindMethod(v, _SC("SetIndent"), DoStringFunc<TiXmlPrinter, &TiXmlPrinter::SetIndent>, _SC("TiXmlPrinter::SetIndent"));
        BindMethod(v, _SC("SetLineBreak"), DoStringFunc<TiXmlPrinter, &TiXmlPrinter::SetLineBreak>, _SC("TiXmlPrinter::SetLineBreak"));
        // Put the class in the root table. This must be last!
        sq_newslot(v, classDecl, SQFalse);
    }
    {
        // Register TiXmlNode
        const SQInteger classDecl = CreateClassDecl<TiXmlNode>(v);
        //BindEmptyCtor<TiXmlNode>(v);

        BindMethod(v, _SC("Value"), GetReturnString<TiXmlNode const, &TiXmlNode::Value>, _SC("TiXmlNode::Value"));
        BindMethod(v, _SC("SetValue"), DoStringFunc<TiXmlNode, &TiXmlNode::SetValue>, _SC("TiXmlNode::SetValue"));

        BindDefaultInstanceCmp<TiXmlNode>(v);
        // Put the class in the root table. This must be last!
        sq_newslot(v, classDecl, SQFalse);
    }

    {
        // Register TiXmlDocument
        const SQInteger classDecl = CreateClassDecl<TiXmlDocument>(v);
        BindEmptyCtor<TiXmlDocument>(v);

        BindMethod(v, _SC("constructor"), TiXmlDocument_ctor, _SC("TiXmlDocument::constructor"));

        BindMethod(v, _SC("SaveFile"), TiXMLDocument_SaveFile, _SC("TiXmlDocument::SaveFile"));
        BindMethod(v, _SC("LoadFile"), TiXMLDocument_LoadFile, _SC("TiXmlDocument::LoadFile"));

        BindMethod(v, _SC("Parse"), TiXmlDocument_Parse, _SC("TiXmlDocument::Parse"));

        BindMethod(v, _SC("RootElement"), TiXmlDocument_RootElement, _SC("TiXmlDocument::RootElement"));

        BindMethod(v, _SC("Error"), TiXmlDocument_Error, _SC("TiXmlDocument::Error"));
        BindMethod(v, _SC("ErrorDesc"), GetReturnString<TiXmlDocument const, &TiXmlDocument::ErrorDesc>, _SC("TiXmlDocument::ErrorDesc"));

        BindMethod(v, _SC("Accept"), TiXmlDocument_Accept, _SC("TiXmlDocument::Accept"));



        BindDefaultInstanceCmp<TiXmlDocument>(v);
        // Put the class in the root table. This must be last!
        sq_newslot(v, classDecl, SQFalse);
    }

    {
        // Register TiXmlElement
        const SQInteger classDecl = CreateClassDecl<TiXmlElement>(v);
        BindMethod(v, _SC("constructor"), TiXmlElement_ctor, _SC("TiXmlElement::constructor"));

        BindMethod(v, _SC("Attribute"), GetStringReturnString<TiXmlElement const, &TiXmlElement::Attribute>, _SC("TiXmlElement::Attribute"));
        BindMethod(v, _SC("GetAttribute"), TiXmlElement_GetAttribute, _SC("TiXmlElement::GetAttribute"));
        BindMethod(v, _SC("FirstAttribute"), TiXmlElement_FirstAttribute, _SC("TiXmlElement::FirstAttribute"));
        BindMethod(v, _SC("LastAttribute"), TiXmlElement_LastAttribute, _SC("TiXmlElement::LastAttribute"));
        BindMethod(v, _SC("RemoveAttribute"), DoStringFunc<TiXmlElement, &TiXmlElement::RemoveAttribute>, _SC("TiXmlElement::RemoveAttribute"));
        BindMethod(v, _SC("SetAttribute"), TiXmlElement_SetAttribute, _SC("TiXmlElement::SetAttribute"));

        BindMethod(v, _SC("FirstChildElement"), TiXmlElement_FirstChildElement, _SC("TiXmlElement::FirstChildElement"));
        BindMethod(v, _SC("LastChild"), TiXmlElement_LastChild, _SC("TiXmlElement::LastChild"));
        BindMethod(v, _SC("LastChildElement"), TiXmlElement_LastChildElement, _SC("TiXmlElement::LastChildElement"));
        BindMethod(v, _SC("NextSiblingElement"), TiXmlElement_NextSiblingElement, _SC("TiXmlElement::NextSiblingElement"));
        BindMethod(v, _SC("PreviousSibling"), TiXmlElement_PreviousSibling, _SC("TiXmlElement::PreviousSibling"));

        BindMethod(v, _SC("GetText"), GetReturnString<TiXmlElement const, &TiXmlElement::GetText>, _SC("TiXmlElement::GetText"));


        BindDefaultInstanceCmp<TiXmlElement>(v);
        // Put the class in the root table. This must be last!
        sq_newslot(v, classDecl, SQFalse);
    }

    {
        // Register TiXmlAttribute
        const SQInteger classDecl = CreateClassDecl<TiXmlAttribute>(v);
        BindMethod(v, _SC("constructor"), TiXmlAttribute_ctor, _SC("TiXmlAttribute::constructor"));

        BindMethod(v, _SC("Value"), GetReturnString<TiXmlAttribute const, &TiXmlAttribute::Value>, _SC("TiXmlAttribute::Value"));
        BindMethod(v, _SC("Name"), GetReturnString<TiXmlAttribute const, &TiXmlAttribute::Name>, _SC("TiXmlAttribute::Name"));
        BindMethod(v, _SC("IntValue"), GetReturnInt<TiXmlAttribute, &TiXmlAttribute::IntValue>, _SC("TiXmlAttribute::IntValue"));
        BindMethod(v, _SC("DoubleValue"), GetReturnDouble<TiXmlAttribute, &TiXmlAttribute::DoubleValue>, _SC("TiXmlAttribute::DoubleValue"));
        BindMethod(v, _SC("Next"), TiXmlAttribute_Next, _SC("TiXmlAttribute::Next"));

        BindMethod(v, _SC("SetValue"), DoStringFunc<TiXmlAttribute, &TiXmlAttribute::SetValue>, _SC("TiXmlAttribute::SetValue"));
        BindMethod(v, _SC("SetName"), DoStringFunc<TiXmlAttribute, &TiXmlAttribute::SetName>, _SC("TiXmlAttribute::SetName"));
        sq_newslot(v, classDecl, SQFalse);
    }

    {
        // Register TiXmlHandle
        const SQInteger classDecl = CreateClassDecl<TiXmlHandle>(v);
        BindMethod(v, _SC("constructor"), TiXmlHandle_ctor, _SC("TiXmlHandle::constructor"));

        BindMethod(v, _SC("FirstChild"), GetTiXMLHandleEmtyOrString<&TiXmlHandle::FirstChild, &TiXmlHandle::FirstChild>, _SC("TiXmlHandle::FirstChild"));
        BindMethod(v, _SC("FirstChildElement"), GetTiXMLHandleEmtyOrString<&TiXmlHandle::FirstChildElement, &TiXmlHandle::FirstChildElement>, _SC("TiXmlHandle::FirstChildElement"));


        BindMethod(v, _SC("ToNode"), TiXml_RetPtr<TiXmlHandle, TiXmlNode, &TiXmlHandle::ToNode>, _SC("TiXmlHandle::ToNode"));
        BindMethod(v, _SC("ToElement"), TiXml_RetPtr<TiXmlHandle, TiXmlElement, &TiXmlHandle::ToElement>, _SC("TiXmlHandle::ToElement"));

        BindMethod(v, _SC("Node"), TiXml_RetPtr<TiXmlHandle, TiXmlNode, &TiXmlHandle::Node>, _SC("TiXmlHandle::Node"));
        BindMethod(v, _SC("Element"), TiXml_RetPtr<TiXmlHandle, TiXmlElement, &TiXmlHandle::Element>, _SC("TiXmlHandle::Element"));


        sq_newslot(v, classDecl, SQFalse);
    }

    sq_pop(v, 1); // Pop root table.

}
}
