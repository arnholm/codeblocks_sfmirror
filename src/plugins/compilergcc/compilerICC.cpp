/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#ifndef CB_PRECOMP
	#include <wx/intl.h>
	#include <wx/msgdlg.h>
	#include <wx/regex.h>

	#include "compilerfactory.h"
	#include "logmanager.h"
	#include "manager.h"
	#include "macrosmanager.h"
#endif // CB_PRECOMP

#include <wx/config.h>
#include <wx/fileconf.h>

#include "compilerICC.h"

class wxIccDirTraverser : public wxDirTraverser
{
    public:
        wxIccDirTraverser(wxArrayString& folders) : m_Dirs(folders)
        {
            m_SepChar = (platform::windows == 1) ? _T('\\') : _T('/');
        }

        wxDirTraverseResult OnFile(const wxString& WXUNUSED(filename)) override
        {
            return wxDIR_CONTINUE;
        }

        wxDirTraverseResult OnDir(const wxString& dirname) override
        {
            if (m_Dirs.Index(dirname) == wxNOT_FOUND &&
                dirname.AfterLast(m_SepChar).Contains("."))
            {
                m_Dirs.Add(dirname);
            }
            return wxDIR_CONTINUE;
        }

    private:
        wxArrayString& m_Dirs;
        wxChar m_SepChar;
};

CompilerICC::CompilerICC()
    : Compiler(_("Intel C/C++ Compiler"), "icc")
{
    m_Weight = 40;
    Reset();
}

CompilerICC::~CompilerICC()
{
    //dtor
}

Compiler * CompilerICC::CreateCopy()
{
    return (new CompilerICC(*this));
}

AutoDetectResult CompilerICC::AutoDetectInstallationDir()
{
    wxString sep = wxFileName::GetPathSeparator();

    if (platform::windows)
    {
        if ( wxDirExists("C:\\Program Files\\Intel\\Compiler") )
        {
            wxDir icc_dir("C:\\Program Files\\Intel\\Compiler\\C++");
            if (icc_dir.IsOpened())
            {
                wxArrayString dirs;
                wxIccDirTraverser IccDirTraverser(dirs);
                icc_dir.Traverse(IccDirTraverser);
                if (!dirs.IsEmpty())
                {
                    // Now sort the array in reverse order to get the latest version's path
                    dirs.Sort(true);
                    m_MasterPath = dirs[0];
                    m_MasterPath.Append("\\IA32");
                }
            }
        }

        int version = 0;
        while ( m_MasterPath.IsEmpty() || !wxDirExists(m_MasterPath) )
        {
            wxString iccEnvVar;
            if (version==0)
            {
                // Try default w/o version number
                iccEnvVar = "ICPP_COMPILER";
                version = 8;
            }
            else if (version>15)
                break;  // exit while-loop
            else
            {
                // Try ICPP_COMPILER80 ... ICPP_COMPILER12
                iccEnvVar.Printf("ICPP_COMPILER%d0", version);
                version++;
            }

            // Read the ICPP_COMPILER[XX] environment variable
            if ( !wxGetEnv(iccEnvVar, &m_MasterPath) )
                m_MasterPath.Clear();
        }

        // Now check for the installation of MSVC
        const wxString msvcIds[4] = { "msvc6",
                                      "msvctk",
                                      "msvc8",
                                      "msvc10" };
        bool msvcFound = false;
        for (unsigned int which_msvc = 0; which_msvc < array_size(msvcIds); ++which_msvc)
        {
            Compiler* vcComp = CompilerFactory::GetCompiler(msvcIds[which_msvc]);
            if (!vcComp)
                continue; // compiler not registered? try next one

            wxString vcMasterNoMacros = vcComp->GetMasterPath();
            Manager::Get()->GetMacrosManager()->ReplaceMacros(vcMasterNoMacros);
            if (   !wxFileExists(vcMasterNoMacros + sep + "bin" + sep + vcComp->GetPrograms().C)
                && !wxFileExists(vcMasterNoMacros + sep + vcComp->GetPrograms().C) )
                continue; // this MSVC is not installed; try next one

            const wxString& vcMasterPath = vcComp->GetMasterPath();
            if (m_ExtraPaths.Index(vcMasterPath) == wxNOT_FOUND)
                m_ExtraPaths.Add(vcMasterPath);
            if (  !vcMasterPath.EndsWith("bin")
                && m_ExtraPaths.Index(vcMasterPath + sep + "bin") == wxNOT_FOUND )
            {
                m_ExtraPaths.Add(vcMasterPath + sep + "bin");
            }
            AddIncludeDir(vcMasterPath + "\\Include");
            AddLibDir(vcMasterPath + "\\Lib");
            AddResourceIncludeDir(vcMasterPath + "\\Include");

            const wxArrayString& vcExtraPaths = vcComp->GetExtraPaths();
            for (size_t i = 0; i < vcExtraPaths.GetCount(); ++i)
            {
                if (   m_ExtraPaths.Index(vcExtraPaths[i]) == wxNOT_FOUND
                    && wxDirExists(vcExtraPaths[i]) )
                {
                    m_ExtraPaths.Add(vcExtraPaths[i]);
                }
            }
            const wxArrayString& vcIncludeDirs = vcComp->GetIncludeDirs();
            for (size_t i = 0; i < vcIncludeDirs.GetCount(); ++i)
            {
                if (wxDirExists(vcIncludeDirs[i]))
                {
                    if (m_IncludeDirs.Index(vcIncludeDirs[i]) == wxNOT_FOUND)
                        AddIncludeDir(vcIncludeDirs[i]);

                    if (m_ResIncludeDirs.Index(vcIncludeDirs[i]) == wxNOT_FOUND)
                        AddResourceIncludeDir(vcIncludeDirs[i]);
                }
            }
            const wxArrayString& vcLibDirs = vcComp->GetLibDirs();
            for (size_t i = 0; i < vcLibDirs.GetCount(); ++i)
            {
                if (   m_LibDirs.Index(vcLibDirs[i]) == wxNOT_FOUND
                    && wxDirExists(vcLibDirs[i]) )
                {
                    AddLibDir(vcLibDirs[i]);
                }
            }
            msvcFound = true;
            break;
        }

        if ( m_MasterPath.IsEmpty() || !wxDirExists(m_MasterPath) )
        {
            // Just a final guess for the default installation dir
            wxString Programs = "C:\\Program Files";
            // what's the "Program Files" location
            // TO DO : support 64 bit ->    32 bit apps are in "ProgramFiles(x86)"
            //                              64 bit apps are in "ProgramFiles"
            wxGetEnv("ProgramFiles", &Programs);
            m_MasterPath = Programs + "\\Intel\\Compiler\\C++\\9.0";
        }
        else if (!msvcFound)
        {
            cbMessageBox(_T("It seems your computer doesn't have a MSVC compiler installed.\n\n"
                            "The ICC compiler requires MSVC for proper functioning and\n"
                            "it may not work without it."),
                         "Error", wxOK | wxICON_ERROR);
        }
    }
    else
    {
        m_MasterPath = "/opt/intel/cc/9.0";
        if (wxDirExists("/opt/intel"))
        {
            wxDir icc_dir("/opt/intel/cc");
            if (icc_dir.IsOpened())
            {
                wxArrayString dirs;
                wxIccDirTraverser IccDirTraverser(dirs);
                icc_dir.Traverse(IccDirTraverser);
                if (!dirs.IsEmpty())
                {
                    // Now sort the array in reverse order to get the latest version's path
                    dirs.Sort(true);
                    m_MasterPath = dirs[0];
                }
            }
        }
    }

    AutoDetectResult ret = wxFileExists(m_MasterPath + sep + "bin" + sep + m_Programs.C) ? adrDetected : adrGuessed;
    if (ret == adrGuessed)
        ret = wxFileExists(m_MasterPath + sep + "bin" + sep + "ia32" + sep + m_Programs.C) ? adrDetected : adrGuessed;
    if (ret == adrGuessed)
        ret = wxFileExists(m_MasterPath + sep + "bin" + sep + "intel64" + sep + m_Programs.C) ? adrDetected : adrGuessed;

    if (ret == adrDetected)
    {
        if ( wxFileExists(m_MasterPath + sep + "bin" + sep + "ia32" + sep + m_Programs.C) )
            m_ExtraPaths.Add(m_MasterPath + sep + "bin" + sep + "ia32");
        if ( wxFileExists(m_MasterPath + sep + "bin" + sep + "intel64" + sep + m_Programs.C) )
            m_ExtraPaths.Add(m_MasterPath + sep + "bin" + sep + "intel64");

        if ( wxDirExists(m_MasterPath + sep + "include") )
        {
            m_IncludeDirs.Insert(m_MasterPath + sep + "include", 0);
            m_ResIncludeDirs.Insert(m_MasterPath + sep + "include", 0);
        }
        if ( wxDirExists(m_MasterPath + sep + "compiler" + sep + "include") )
        {
            m_IncludeDirs.Insert(m_MasterPath + sep + "compiler" + sep + "include", 0);
            m_ResIncludeDirs.Insert(m_MasterPath + sep + "compiler" + sep + "include", 0);
        }

        if ( wxDirExists(m_MasterPath + sep + "lib") )
        {
            m_IncludeDirs.Insert(m_MasterPath + sep + "lib", 0);
            m_ResIncludeDirs.Insert(m_MasterPath + sep + "lib", 0);
        }

        if ( wxDirExists(m_MasterPath + sep + "compiler" + sep + "lib") )
            m_LibDirs.Insert(m_MasterPath + sep + "compiler" + sep + "lib", 0);
        if ( wxDirExists(m_MasterPath + sep + "compiler" + sep + "lib" + sep + "ia32") )
            m_LibDirs.Insert(m_MasterPath + sep + "compiler" + sep + "lib" + sep + "ia32", 0);
        if ( wxDirExists(m_MasterPath + sep + "compiler" + sep + "lib" + sep + "intel64") )
            m_LibDirs.Insert(m_MasterPath + sep + "compiler" + sep + "lib" + sep + "intel64", 0);
    }
    // Try to detect the debugger. If not detected successfully the debugger plugin will
    // complain, so only the autodetection of compiler is considered in return value
    wxString path;
    wxString dbg;
    if (platform::windows)
    {
        dbg = "idb.exe";
        wxGetEnv("IDB_PATH", &path);
        if ( !path.IsEmpty() && wxDirExists(path) )
        {
            int version = 9;
            while ( true )
            {
                wxString idbPath = path + sep + "IDB" + sep + wxString::Format("%d.0", version) + sep + "IA32";
                if ( wxDirExists(idbPath) )
                {
                    path = idbPath; // found
                    break;  // exit while-loop
                }
                else if (version>15)
                    break;  // exit while-loop
                else
                    version++;
            }
        }
    }
    else
    {
        dbg  = "idb";
        path = "/opt/intel/idb/9.0";
        if ( wxDirExists("/opt/intel") )
        {
            wxDir icc_debug_dir("/opt/intel/idb");
            if (icc_debug_dir.IsOpened())
            {
                wxArrayString debug_dirs;
                wxIccDirTraverser IccDebugDirTraverser(debug_dirs);
                icc_debug_dir.Traverse(IccDebugDirTraverser);
                if (!debug_dirs.IsEmpty())
                {
                    // Now sort the array in reverse order to get the latest version's path
                    debug_dirs.Sort(true);
                    path = debug_dirs[0];
                }
            }
        }
    }

    if ( wxFileExists(path + sep + "bin" + sep + dbg) )
        m_ExtraPaths.Add(path);

    return ret;
}
