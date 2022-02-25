/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <algorithm>

#include <wx/intl.h>
#include <wx/dynarray.h>
#include <wx/textfile.h>
#include <configmanager.h>

#include "help_common.h"
#include "logmanager.h"

using std::find;
using std::make_pair;

int HelpCommon::m_DefaultHelpIndex = -1;
int HelpCommon::m_NumReadFromIni = 0;

void HelpCommon::LoadHelpFilesVector(HelpCommon::HelpFilesVector &vect)
{
    LogManager *log = Manager::Get()->GetLogManager();

    vect.clear();
    HelpCommon::setNumReadFromIni(0);
    ConfigManager* conf = Manager::Get()->GetConfigManager("help_plugin");
    m_DefaultHelpIndex = conf->ReadInt("/default", -1);
    wxArrayString list = conf->EnumerateSubPaths("/");

    for (unsigned int i = 0; i < list.GetCount(); ++i)
    {
        HelpFileAttrib hfa;
        wxString name = conf->Read(list[i] + "/name", wxEmptyString);
        hfa.name = conf->Read(list[i] + "/file", wxEmptyString);
        conf->Read(list[i] + "/isexec", &hfa.isExecutable);
        conf->Read(list[i] + "/embeddedviewer", &hfa.openEmbeddedViewer);
        // Patch by Yorgos Pagles: Read new attributes from settings
        int keyWordCase = 0;
        (conf->Read(list[i] + "/keywordcase", &keyWordCase));
        hfa.keywordCase = static_cast<HelpCommon::StringCase>(keyWordCase);
        hfa.defaultKeyword = conf->Read(list[i] + "/defaultkeyword", wxEmptyString);

        if (!name.IsEmpty() && !hfa.name.IsEmpty())
        {
            vect.push_back(make_pair(name, hfa));
        }
    }

    wxString docspath = ConfigManager::GetFolder(sdDataGlobal) +  wxFileName::GetPathSeparator() + "docs";
    wxString iniFileName =  docspath + wxFileName::GetPathSeparator() + "index.ini";

    if (wxFileName::FileExists(iniFileName))
    {
        log->Log(wxString::Format("Help plugin ini file: %s", iniFileName));

        wxTextFile hFile(iniFileName);
        hFile.Open();
        unsigned int cnt = hFile.GetLineCount();

        for(unsigned int i = 0; i < cnt; i++)
        {
            wxString line = hFile.GetLine(i);

            if (!line.IsEmpty())
            {
                wxString item = line.BeforeLast('=').Strip();
                wxString file = line.AfterLast('=').Strip();
                file = docspath + wxFileName::GetPathSeparator() + file;

                if (!item.IsEmpty() && !file.IsEmpty())
                {
                    HelpFileAttrib hfa;
                    hfa.name = file;
                    hfa.isExecutable = false;
                    hfa.openEmbeddedViewer = false;
                    hfa.readFromIni = true;
                    hfa.keywordCase = static_cast<HelpCommon::StringCase> (0);
                    hfa.defaultKeyword = wxEmptyString;

                    if (!hfa.name.IsEmpty())
                    {
                        vect.push_back(make_pair(item, hfa));
                        ++HelpCommon::m_NumReadFromIni;
                    }
                }
            }
        }

        hFile.Close();
    }
    else
    {
        if (wxFileName::DirExists(iniFileName))
        {
            if (!wxFileName::FileExists(iniFileName))
            {
                log->LogError(wxString::Format(_("Missing Help plugin ini file: %s"), iniFileName));
            }
        }
        else
        {
            log->LogError(wxString::Format(_("Missing Help plugin doc directory : %s"), docspath));
        }
    }
}

void HelpCommon::SaveHelpFilesVector(HelpCommon::HelpFilesVector &vect)
{
    ConfigManager* conf = Manager::Get()->GetConfigManager("help_plugin");
    wxArrayString list = conf->EnumerateSubPaths("/");

    for (unsigned int i = 0; i < list.GetCount(); ++i)
    {
        conf->DeleteSubPath(list[i]);
    }

    HelpFilesVector::iterator it;

    int count = 0;

    for (it = vect.begin(); it != vect.end(); ++it)
    {
        HelpFileAttrib hfa;
        wxString name = it->first;
        hfa = it->second;

        if (!name.IsEmpty() && !hfa.name.IsEmpty() && !hfa.readFromIni)
        {
            wxString key = wxString::Format("/help%d/", count++);
            conf->Write(key + "name", name);
            conf->Write(key + "file", hfa.name);
            conf->Write(key + "isexec", hfa.isExecutable);
            conf->Write(key + "embeddedviewer", hfa.openEmbeddedViewer);
            // Patch by Yorgos Pagles: Write new attributes in settings
            conf->Write(key + "keywordcase", static_cast<int>(hfa.keywordCase));
            conf->Write(key + "defaultkeyword", hfa.defaultKeyword);
        }
    }

    conf->Write("/default", m_DefaultHelpIndex);
}

