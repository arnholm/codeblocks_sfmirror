/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 */

#include "sdk.h"

#include "prep.h"
#include "compilerGNUFortran.h"
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

CompilerGNUFortran::CompilerGNUFortran()
    : Compiler(_("GNU Fortran Compiler"), "gfortran")
{
    m_Weight = 88;
    m_MultiLineMessages = true;
    Reset();
}

CompilerGNUFortran::~CompilerGNUFortran()
{
    //dtor
}

Compiler * CompilerGNUFortran::CreateCopy()
{
    return (new CompilerGNUFortran(*this));
}

AutoDetectResult CompilerGNUFortran::AutoDetectInstallationDir()
{
    // try to find MinGW in environment variable PATH first
    wxString pathValues;
    wxGetEnv("PATH", &pathValues);
    if (!pathValues.IsEmpty())
    {
        wxString sep = platform::windows ? ";" : ":";
        wxChar pathSep = platform::windows ? _T('\\') : _T('/');
        wxArrayString pathArray = GetArrayFromString(pathValues, sep);
        for (size_t i = 0; i < pathArray.GetCount(); ++i)
        {
            if (wxFileExists(pathArray[i] + pathSep + m_Programs.C))
            {
                if (pathArray[i].AfterLast(pathSep).IsSameAs("bin"))
                {
                    m_MasterPath = pathArray[i].BeforeLast(pathSep);
                    return adrDetected;
                }
            }
        }
    }

    wxString sep = wxFileName::GetPathSeparator();
    if (platform::windows)
    {
        // look first if MinGW was installed with Code::Blocks (new in beta6)
        m_MasterPath = ConfigManager::GetExecutableFolder();
        if (!wxFileExists(m_MasterPath + sep + "bin" + sep + m_Programs.C))
            // if that didn't do it, look under C::B\MinGW, too (new in 08.02)
            m_MasterPath += sep + "MinGW";
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
                        if (key.Exists() && key.Open(wxRegKey::Read))
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
    // don't add lib/include dirs. GCC knows where its files are located

    //SetVersionString();
    return ret;
}

