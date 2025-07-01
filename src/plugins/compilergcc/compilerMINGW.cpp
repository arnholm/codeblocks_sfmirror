/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#include "compilerMINGW.h"
#include <wx/intl.h>
#include <wx/regex.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include "manager.h"
#include "macrosmanager.h"
#include "logmanager.h"
#include "compilerMINGWgenerator.h"

#include "configmanager.h"

#ifdef __WXMSW__
    #include <wx/dir.h>
    #include <wx/msw/registry.h>
#endif

CompilerMINGW::CompilerMINGW(const wxString& name, const wxString& ID)
    : Compiler(name, ID)
{
    m_Weight = 4;
    Reset();
}

CompilerMINGW::~CompilerMINGW()
{
    //dtor
}

Compiler * CompilerMINGW::CreateCopy()
{
    return (new CompilerMINGW(*this));
}

CompilerCommandGenerator* CompilerMINGW::GetCommandGenerator(cbProject *project)
{
    CompilerMINGWGenerator *generator = new CompilerMINGWGenerator;
    generator->Init(project);
    return generator;
}

AutoDetectResult CompilerMINGW::AutoDetectInstallationDir()
{
    LogManager* logMgr = Manager::Get()->GetLogManager();
    logMgr->DebugLog("MinGW compiler detection for compiler ID: '" + GetID() + "' (parent ID= '" + GetParentID()
                   + "') - C program '" + m_Programs.C + "' or 'mingw32-" + m_Programs.C + "'");

    const wxString sep(wxFileName::GetPathSeparator());

    // Check if current configuration (after translating macros) is valid
    if (!m_MasterPath.empty())
    {
        wxString path_no_macros(m_MasterPath);
        Manager::Get()->GetMacrosManager()->ReplaceMacros(path_no_macros);
        wxString program_no_macros(m_Programs.C);
        Manager::Get()->GetMacrosManager()->ReplaceMacros(program_no_macros);
        if (wxFileExists(path_no_macros + sep + "bin" + sep + program_no_macros))
        {
            logMgr->DebugLog("Already configured MinGW master path='" + path_no_macros + "', compiler='" + program_no_macros + "'");
            return adrDetected;
        }
    }

    wxString bin_gcc        = sep + "bin" + sep + m_Programs.C;
    wxString bin_mingw32gcc = sep + "bin" + sep + "mingw32-" + m_Programs.C;

    // Try to find MinGW in environment variable PATH first
    logMgr->DebugLog("Checking [PATH] master path='" + m_MasterPath + "'");
    wxString pathValues;
    wxGetEnv("PATH", &pathValues);
    if (!pathValues.IsEmpty())
    {
        wxString list_sep = platform::windows ? ";"  : ":";
        wxChar   path_sep = platform::windows ? '\\' : '/';
        wxArrayString pathArray = GetArrayFromString(pathValues, list_sep);
        for (size_t i = 0; i < pathArray.GetCount(); ++i)
        {
            if (   wxFileExists(pathArray[i] + sep + m_Programs.C)
                || wxFileExists(pathArray[i] + sep + "mingw32-" + m_Programs.C) )
            {
                if (pathArray[i].AfterLast(path_sep).IsSameAs("bin"))
                {
                    m_MasterPath = pathArray[i].BeforeLast(path_sep);
                    if (wxFileExists(m_MasterPath + bin_mingw32gcc))
                        m_Programs.C = "mingw32-" + m_Programs.C;
                    logMgr->DebugLog("Final MinGW master path='" + m_MasterPath + "', compiler='" + m_Programs.C + "'");
                    return adrDetected;
                }
            }
        }
    }

    if (platform::windows)
    {
        // Look first if MinGW was installed with Code::Blocks (new in beta6)
        m_MasterPath = ConfigManager::GetExecutableFolder();

        logMgr->DebugLog("Checking [C::B\\bin] master path='" + m_MasterPath + "'");
        if (   !wxFileExists(m_MasterPath + bin_gcc)
            && !wxFileExists(m_MasterPath + bin_mingw32gcc) )
        {
            // If that didn't do it, look under [C::B]\MinGW, too (new in 08.02)
            m_MasterPath += sep + "MinGW";
        }
        else if (wxFileExists(m_MasterPath + bin_mingw32gcc))
            m_Programs.C = "mingw32-" + m_Programs.C;

        logMgr->DebugLog("Checking [C::B\\MinGW] master path='" + m_MasterPath + "'");
        if (   !wxFileExists(m_MasterPath + bin_gcc)
            && !wxFileExists(m_MasterPath + bin_mingw32gcc) )
        {
            // No... now search for MinGW installation dir
            wxString windir = wxGetOSDirectory();
            wxFileConfig ini("", "", windir + sep + "MinGW.ini", "", wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_NO_ESCAPE_CHARACTERS);
            m_MasterPath = ini.Read("/InstallSettings/InstallPath", "C:\\MinGW");

            logMgr->DebugLog("Checking [INI] master path='" + m_MasterPath + "'");
            if (!wxFileExists(m_MasterPath + bin_gcc))
            {
#ifdef __WXMSW__ // for wxRegKey
                // Look for dev-cpp installation in Registry
                wxRegKey key; // defaults to HKCR
                key.SetName("HKEY_LOCAL_MACHINE\\Software\\Dev-C++");
                if (key.Exists() && key.Open(wxRegKey::Read))
                {
                    // found; read it
                    key.QueryValue("Install_Dir", m_MasterPath);
                    logMgr->DebugLog("Checking [registry] master path='" + m_MasterPath + "'");
                }
                else
                {
                    // Look for MinGW/TDM installtion using inno-setup
                    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Minimalist GNU for Windows 4.1_is1
                    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TDM-GCC
                    wxString name;
                    long index;
                    // Prepare Round 1: 32 bit Windows
                    key.SetName("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
                    for (int i = 0; i < 2; ++i) // Round 1: 32 bit Windows; Round 2: 64 bit Windows
                    {
                        bool ok = key.GetFirstKey(name, index);
                        while ( ok && !name.StartsWith("Minimalist GNU for Windows")
                                   && !name.IsSameAs("TDM-GCC") )
                        {
                            ok = key.GetNextKey(name, index);
                        }
                        if (ok)
                        {
                            name = key.GetName() + "\\" + name;
                            key.SetName(name);
                            if (key.Exists() && key.Open(wxRegKey::Read))
                            {
                                key.QueryValue("InstallLocation", m_MasterPath);
                                logMgr->DebugLog("Checking [registry] master path='" + m_MasterPath + "'");

                                // Determine compiler executable, eg: "x86_64-w64-mingw32-gcc.exe", "mingw32-gcc.exe" or "gcc.exe"
                                wxDir binFolder(m_MasterPath + sep + "bin");
                                if (binFolder.IsOpened() && binFolder.GetFirst(&name, "*gcc*.exe", wxDIR_FILES))
                                {
                                    m_Programs.C = name;
                                    while (binFolder.GetNext(&name))
                                    {
                                        if (name.Length() < m_Programs.C.Length())
                                            m_Programs.C = name; // Avoid "x86_64-w64-mingw32-gcc-4.8.1.exe" or "gcc-ar.exe"
                                    }
                                    m_Programs.CPP = m_Programs.C;
                                    m_Programs.CPP.Replace("mingw32-gcc", "mingw32-g++");
                                    m_Programs.LD = m_Programs.CPP;
                                    break;
                                }
                            }
                        }
                        // Prepare Round 2: 64 bit Windows
                        key.SetName("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
                    }
                }
#endif // __WXMSW__
            }

            // Check for PortableApps.com installation
            if ( platform::windows && !wxFileExists(m_MasterPath + bin_gcc)
                                   && !wxFileExists(m_MasterPath + bin_mingw32gcc) )
            {
                wxString drive = wxFileName(ConfigManager::GetExecutableFolder()).GetVolume() + ":\\";
                logMgr->DebugLog("Checking [PortableApps] master path='" + drive + "PortableApps\\CommonFiles\\MinGW" + "'");
                logMgr->DebugLog("Checking [PortableApps] master path='" + drive + "CommonFiles\\MinGW"               + "'");
                logMgr->DebugLog("Checking [PortableApps] master path='" + drive + "MinGW"                            + "'");
                if (   wxFileExists(drive + "PortableApps\\CommonFiles\\MinGW" + bin_gcc)
                    || wxFileExists(drive + "PortableApps\\CommonFiles\\MinGW" + bin_mingw32gcc) )
                {
                    m_MasterPath = drive + "PortableApps\\CommonFiles\\MinGW";
                    if (wxFileExists(m_MasterPath + bin_mingw32gcc))
                        m_Programs.C = "mingw32-" + m_Programs.C;
                }
                else if (   wxFileExists(drive + "CommonFiles\\MinGW" + bin_gcc)
                         || wxFileExists(drive + "CommonFiles\\MinGW" + bin_mingw32gcc) )
                {
                    m_MasterPath = drive + "CommonFiles\\MinGW";
                    if (wxFileExists(m_MasterPath + bin_mingw32gcc))
                        m_Programs.C = "mingw32-" + m_Programs.C;
                }
                else if (   wxFileExists(drive + "MinGW" + bin_gcc)
                         || wxFileExists(drive + "MinGW" + bin_mingw32gcc) )
                {
                    m_MasterPath = drive + "MinGW";
                    if (wxFileExists(m_MasterPath + bin_mingw32gcc))
                        m_Programs.C = "mingw32-" + m_Programs.C;
                }
            }
            else if (wxFileExists(m_MasterPath + bin_mingw32gcc))
                m_Programs.C = "mingw32-" + m_Programs.C;
        }
        else if (wxFileExists(m_MasterPath + bin_mingw32gcc))
            m_Programs.C = "mingw32-" + m_Programs.C;
    }
    else
        m_MasterPath = "/usr";

    logMgr->DebugLog("Final MinGW master path='" + m_MasterPath + "', compiler='" + m_Programs.C + "'");
    AutoDetectResult ret = (wxFileExists(m_MasterPath + bin_gcc) || (wxFileExists(m_MasterPath + bin_mingw32gcc))) ? adrDetected : adrGuessed;
    // Don't add lib/include dirs for MinGW/GCC. GCC knows itself where its files are located

    SetVersionString();
    return ret;
}

void CompilerMINGW::SetVersionString()
{
//    Manager::Get()->GetLogManager()->DebugLog("Compiler detection for compiler ID: '" + GetID() + "' (parent ID= '" + GetParentID() + "')");

    wxArrayString output;
    wxString sep = wxFileName::GetPathSeparator();
    wxString master_path = m_MasterPath;
    wxString compiler_exe = m_Programs.C;

    /* We should read the master path from the configuration manager as
     * the m_MasterPath is empty if AutoDetectInstallationDir() is not
     * called
     */
    ConfigManager* cmgr = Manager::Get()->GetConfigManager("compiler");
    if (cmgr)
    {
        wxString settings_path;
        wxString compiler_path;
        /* Differ between user-defined compilers (copies of base compilers) */
        if (GetParentID().IsEmpty())
        {
            settings_path = "/sets/"      + GetID() + "/master_path";
            compiler_path = "/sets/"      + GetID() + "/c_compiler";
        }
        else
        {
            settings_path = "/user_sets/" + GetID() + "/master_path";
            compiler_path = "/user_sets/" + GetID() + "/c_compiler";
        }
        cmgr->Read(settings_path, &master_path);
        cmgr->Read(compiler_path, &compiler_exe);
    }
    if (master_path.IsEmpty())
    {
        /* Notice: In general this is bad luck as e.g. all copies of a
         * compiler have a different path, most likely.
         * Thus the following might even return a wrong command!
         */
        if (platform::windows)
            master_path = "C:\\MinGW";
        else
            master_path = "/usr";
    }
    wxString gcc_command = master_path + sep + "bin" + sep + compiler_exe;

    Manager::Get()->GetMacrosManager()->ReplaceMacros(gcc_command);
    if (!wxFileExists(gcc_command))
    {
//        Manager::Get()->GetLogManager()->DebugLog("Compiler version detection: Compiler not found: " + gcc_command);
        return;
    }

//    Manager::Get()->GetLogManager()->DebugLog("Compiler version detection: Issuing command: " + gcc_command);

    if ( Execute(gcc_command + " --version", output) != 0 )
    {
//        Manager::Get()->GetLogManager()->DebugLog("Compiler version detection: Error executing command.");
    }
    else
    {
        if (output.GetCount() > 0)
        {
//            Manager::Get()->GetLogManager()->DebugLog("Extracting compiler version from: " + output[0]);
            wxRegEx reg_exp;
            if (reg_exp.Compile("[0-9]+[.][0-9]+[.][0-9]+") && reg_exp.Matches(output[0]))
            {
                m_VersionString = reg_exp.GetMatch(output[0]);
//                Manager::Get()->GetLogManager()->DebugLog("Compiler version via RegExp: " + m_VersionString);
            }
            else
            {
                m_VersionString = output[0].Mid(10);
                m_VersionString = m_VersionString.Left(5);
                m_VersionString.Trim(false);
//                Manager::Get()->GetLogManager()->DebugLog("Compiler version: " + m_VersionString);
            }
        }
    }
}
