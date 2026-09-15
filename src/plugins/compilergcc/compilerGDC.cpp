/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#include "prep.h"
#include "compilerGDC.h"
#include <wx/intl.h>
#include <wx/regex.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/msgdlg.h>
#include "manager.h"
#include "logmanager.h"

#include "configmanager.h"

#ifdef __WXMSW__
    #include <wx/msw/registry.h>
#endif

CompilerGDC::CompilerGDC()
    : Compiler(_("GDC D Compiler"), "gdc")
{
    m_Weight = 76;
    Reset();
}

CompilerGDC::~CompilerGDC()
{
    //dtor
}

Compiler * CompilerGDC::CreateCopy()
{
    return (new CompilerGDC(*this));
}

AutoDetectResult CompilerGDC::AutoDetectInstallationDir()
{
    wxString sep = wxFileName::GetPathSeparator();
    if (platform::windows)
    {
        // look first if MinGW was installed with Code::Blocks (new in beta6)
        m_MasterPath = ConfigManager::GetExecutableFolder();
        if (!wxFileExists(m_MasterPath + sep + "bin" + sep + m_Programs.C))
        {
            // no... search for MinGW installation dir
            wxString windir = wxGetOSDirectory();
            wxFileConfig ini(wxEmptyString, wxEmptyString, windir + "/MinGW.ini", wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_NO_ESCAPE_CHARACTERS);
            m_MasterPath = ini.Read("/InstallSettings/InstallPath", "C:\\MinGW");
            if (!wxFileExists(m_MasterPath + sep + "bin" + sep + m_Programs.C))
            {
#ifdef __WXMSW__ // for wxRegKey
                // not found...
                // look for dev-cpp installation
                wxRegKey key; // defaults to HKCR
                key.SetName("HKEY_LOCAL_MACHINE\\Software\\Dev-C++");
                if (key.Exists() && key.Open(wxRegKey::Read))
                {
                    // found; read it
                    key.QueryValue("Install_Dir", m_MasterPath);
                }
                else
                {
                    // installed by inno-setup
                    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Minimalist GNU for Windows 4.1_is1
                    wxString name;
                    long index;
                    key.SetName("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
                    //key.SetName("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion");
                    bool ok = key.GetFirstKey(name, index);
                    while (ok && !name.StartsWith("Minimalist GNU for Windows"))
                    {
                        ok = key.GetNextKey(name, index);
                    }

                    if (ok)
                    {
                        name = key.GetName() + "\\" + name;
                        key.SetName(name);
                        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("name: %s", name));
                        if (key.Exists())
                            key.QueryValue("InstallLocation", m_MasterPath);
                    }
                }
#endif
            }
        }
        else
            m_Programs.MAKE = "make.exe"; // we distribute "make" not "mingw32-make"
    }
    else
        m_MasterPath = "/usr";

    AutoDetectResult ret = wxFileExists(m_MasterPath + sep + "bin" + sep + m_Programs.C) ? adrDetected : adrGuessed;
    if (ret == adrDetected)
    {
        AddIncludeDir(m_MasterPath + sep + "include" + sep + "d");
        AddLibDir(m_MasterPath + sep + "lib");
    }
    return ret;
}
