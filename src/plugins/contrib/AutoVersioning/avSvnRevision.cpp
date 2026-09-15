#include "avSvnRevision.h"
#include <wx/intl.h>
#include <wx/utils.h>
#include <wx/string.h>
#include <wx/arrstr.h>
#include <tinyxml.h>
#include <globals.h>

bool QuerySvn(const wxString& workingDir, wxString& revision, wxString& date)
{
    revision = "0";
    date = "unknown date";
    wxString svncmd = "svn info --xml --non-interactive ";
    svncmd.Append("\"" + workingDir + "\"");
    wxArrayString xmlOutput;

    if (wxExecute(svncmd, xmlOutput) != -1)
    {

        wxString buf;

        for(unsigned int i=0; i<xmlOutput.GetCount(); ++i){
            buf << xmlOutput[i];
        }

        TiXmlDocument doc;
        doc.Parse(cbU2C(buf));

        if (doc.Error())
            return 0;

        TiXmlHandle hCommit(&doc);
        hCommit = hCommit.FirstChildElement("info").FirstChildElement("entry").FirstChildElement("commit");
		if(const TiXmlElement* e = hCommit.ToElement())
        {
            revision = e->Attribute("revision") ? cbC2U(e->Attribute("revision")) : _T("");
            const TiXmlElement* d = e->FirstChildElement("date");
            if(d && d->GetText())
                date = cbC2U(d->GetText());

            return 1;
        }
    }
    return 0;
}
