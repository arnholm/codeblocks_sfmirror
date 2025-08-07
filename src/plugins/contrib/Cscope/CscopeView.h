#ifndef CSCOPEVIEW_H
#define CSCOPEVIEW_H

#include <wx/string.h>
#include "loggers.h"


class CscopeConfig;
class CscopeTab;
class CscopeView : public Logger
{
    public:
        CscopeView(CscopeConfig *cfg);
        ~CscopeView();
        virtual wxWindow* CreateControl(wxWindow* parent);

        CscopeTab *GetWindow();

        virtual void Append( const wxString &  msg,  Logger::level  lv = info);
        virtual void Clear();
        virtual void CopyContentsToClipboard(bool selectionOnly = false);

    private:
        CscopeTab *m_pPanel;
        CscopeConfig* m_cfg;
};

#endif

