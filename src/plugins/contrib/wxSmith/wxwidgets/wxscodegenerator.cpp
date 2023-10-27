#include "wxscodegenerator.h"

wxsCodeGenerator::wxsCodeGenerator(): m_Context(nullptr)
{
}

wxsCodeGenerator::~wxsCodeGenerator()
{
    m_Context = nullptr;
}

void wxsCodeGenerator::BuildCode(wxsCoderContext* Context)
{
    wxsCoderContext* Store = m_Context;
    m_Context = Context;
    long FlagsStore = Context->m_Flags;

    OnUpdateFlags(Context->m_Flags);
    OnBuildCreatingCode();
    OnBuildHeadersCode();
    OnBuildDeclarationsCode();
    OnBuildEventsConnectingCode();
    OnBuildIdCode();
    OnBuildXRCFetchingCode();

    Context->m_Flags = FlagsStore;
    m_Context = Store;
}

