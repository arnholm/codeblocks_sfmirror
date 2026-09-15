/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#include "logmanager.h"
#include "manager.h"
#include "compilerLCC.h"
#include <wx/intl.h>
#include <wx/regex.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/msgdlg.h>

#ifdef __WXMSW__
    #include <wx/msw/registry.h>
#endif // __WXMSW__

CompilerLCC::CompilerLCC() :
    Compiler(_("LCC Compiler"), "lcc"),
    m_RegistryUpdated(false)
{
    m_Weight = 36;
    Reset();
}

CompilerLCC::~CompilerLCC()
{
    //dtor
}

Compiler* CompilerLCC::CreateCopy()
{
    return (new CompilerLCC(*this));
}

void CompilerLCC::Reset()
{
    m_RegistryUpdated = false; // Check the registry another time on IsValid()

    m_Options.ClearOptions();
    LoadDefaultOptions(GetID());

    LoadDefaultRegExArray();

    m_CompilerOptions.Clear();
    m_LinkerOptions.Clear();
    m_LinkLibs.Clear();
    m_CmdsBefore.Clear();
    m_CmdsAfter.Clear();
}

AutoDetectResult CompilerLCC::AutoDetectInstallationDir()
{
    wxString compiler; compiler << wxFILE_SEP_PATH << "bin" << wxFILE_SEP_PATH << m_Programs.C;

#ifdef __WXMSW__
    wxRegKey key; // defaults to HKCR
    wxString mpHKLM     = wxEmptyString;
    wxString mpHKCU     = wxEmptyString;
    wxString mpLccRoot  = wxEmptyString;
    wxString mpLccLnk   = wxEmptyString;
    wxString mpCompiler = wxEmptyString;

    // Query uninstall information if installed with admin rights:
    key.SetName("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\lcc-win32 (base system)_is1");
    if (key.Exists() && key.Open(wxRegKey::Read))
        key.QueryValue("Inno Setup: App Path", mpHKLM);

    // Query uninstall information if installed *without* admin rights:
    key.SetName("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\lcc-win32 (base system)_is1");
    if (key.Exists() && key.Open(wxRegKey::Read))
        key.QueryValue("Inno Setup: App Path", mpHKCU);

    // Check the LCC lccroot path
    key.SetName("HKEY_CURRENT_USER\\Software\\lcc");
    if (key.Exists() && key.Open(wxRegKey::Read))
        key.QueryValue("lccroot", mpLccRoot);
    if (mpLccRoot.IsEmpty())
    {
        // Check the LCC lccroot path
        key.SetName("HKEY_CURRENT_USER\\Software\\lcc\\lccroot");
        if (key.Exists() && key.Open(wxRegKey::Read))
            key.QueryValue("path", mpLccRoot);
    }

    // Check the LCC lcclnk path
    key.SetName("HKEY_CURRENT_USER\\Software\\lcc\\lcclnk");
    if (key.Exists() && key.Open(wxRegKey::Read))
    {
        key.QueryValue("libpath", mpLccLnk);
        wxString lib_path = "\\lib";
        if (   !mpLccLnk.IsEmpty()
            && (mpLccLnk.Length()>lib_path.Length())
            && (mpLccLnk.Lower().EndsWith(lib_path)) )
        {
            // Remove the lib path to point to the LCC root folder
            mpLccLnk.Remove( (mpLccLnk.Length()-lib_path.Length()), lib_path.Length() );
        }
    }

    // Check the LCC compiler path
    key.SetName("HKEY_CURRENT_USER\\Software\\lcc\\compiler");
    if (key.Exists() && key.Open(wxRegKey::Read))
    {
        key.QueryValue("includepath", mpCompiler);
        wxString inc_path = "\\include";
        if (   !mpCompiler.IsEmpty()
            && (mpCompiler.Length()>inc_path.Length())
            && (mpCompiler.Lower().EndsWith(inc_path)) )
        {
            // Remove the include path to point to the LCC root folder
            mpCompiler.Remove( (mpCompiler.Length()-inc_path.Length()), inc_path.Length() );
        }
    }

    // Verify all path's obtained
    if      (wxFileExists(mpHKLM     + compiler))
        m_MasterPath = mpHKLM;
    else if (wxFileExists(mpHKCU     + compiler))
        m_MasterPath = mpHKCU;
    else if (wxFileExists(mpLccRoot  + compiler))
        m_MasterPath = mpLccRoot;
    else if (wxFileExists(mpLccLnk   + compiler))
        m_MasterPath = mpLccLnk;
    else if (wxFileExists(mpCompiler + compiler))
        m_MasterPath = mpCompiler;
    else
#endif // __WXMSW__
        m_MasterPath = "C:\\lcc"; // just a guess; the default installation dir

    if (!m_MasterPath.IsEmpty())
    {
        AddIncludeDir   (m_MasterPath + wxFILE_SEP_PATH + "include");
        AddLibDir       (m_MasterPath + wxFILE_SEP_PATH + "lib");
        m_ExtraPaths.Add(m_MasterPath + wxFILE_SEP_PATH + "bin");
    }

    m_RegistryUpdated = false; // Check the registry another time on IsValid()

    return wxFileExists(m_MasterPath+compiler) ? adrDetected : adrGuessed;
}

#ifdef __WXMSW__
bool CompilerLCC::IsValid()
{
    if (!m_RegistryUpdated)
    {
        wxString compiler = m_MasterPath + wxFILE_SEP_PATH
                          + "bin" + wxFILE_SEP_PATH + m_Programs.C;

        if (wxFileExists(compiler))
        {
            Manager::Get()->GetLogManager()->DebugLog("LCC: Updating registry...");

            // Make sure the registry is setup as it should be after an installation.
            // This avoids the "smart and clever" LCC compiler asking for the
            // <include> header and lcc libraries path's on the command line (huh?!).
            // Note: A compiler *never ever* should ask on std::cin anything!!!
            wxRegKey key; // defaults to HKCR

            key.SetName("HKEY_CURRENT_USER\\Software\\lcc");
            if (!key.Exists() && key.Create())
                key.SetValue("lccroot",     m_MasterPath                 );

            key.SetName("HKEY_CURRENT_USER\\Software\\lcc\\compiler");
            if (!key.Exists() && key.Create())
                key.SetValue("includepath", m_MasterPath+"\\include" );

            key.SetName("HKEY_CURRENT_USER\\Software\\lcc\\lcclnk");
            if (!key.Exists() && key.Create())
                key.SetValue("libpath",     m_MasterPath+"\\lib"     );

            key.SetName("HKEY_CURRENT_USER\\Software\\lcc\\lccroot");
            if (!key.Exists() && key.Create())
                key.SetValue("path",        m_MasterPath                 );

            m_RegistryUpdated = true;
        }
    }

    return Compiler::IsValid();
}
#endif // __WXMSW__
