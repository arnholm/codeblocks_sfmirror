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

#include "compilerOW.h"
#include "compilerOWgenerator.h"

#include <wx/config.h>
#include <wx/filefn.h>
#include <wx/intl.h>
#include <wx/regex.h>
#include <wx/utils.h>

CompilerOW::CompilerOW()
    : Compiler(_("OpenWatcom (W32) Compiler"), "ow")
{
    m_Weight = 28;
    Reset();
}

CompilerOW::~CompilerOW()
{
	//dtor
}

Compiler * CompilerOW::CreateCopy()
{
    return (new CompilerOW(*this));
}

CompilerCommandGenerator* CompilerOW::GetCommandGenerator(cbProject *project)
{
    // also see hack in: DirectCommands::GetTargetLinkCommands()
    CompilerOWGenerator *generator = new CompilerOWGenerator;
    generator->Init(project);
    return generator;
}

AutoDetectResult CompilerOW::AutoDetectInstallationDir()
{
    /* Following code is Not necessary as OpenWatcom does not write to
       Registry anymore */
    /*wxRegKey key; // defaults to HKCR
    key.SetName("HKEY_LOCAL_MACHINE\\Software\\Open Watcom\\c_1.0");
    if (key.Open())
        // found; read it
        key.QueryValue("Install Location", m_MasterPath);*/

    if (m_MasterPath.IsEmpty())
        // just a guess; the default installation dir
        m_MasterPath = "C:\\watcom";

    if (!m_MasterPath.IsEmpty())
    {
        AddIncludeDir(m_MasterPath + wxFILE_SEP_PATH + "h");
        AddIncludeDir(m_MasterPath + wxFILE_SEP_PATH + "h" + wxFILE_SEP_PATH + "nt");
        AddLibDir(m_MasterPath + wxFILE_SEP_PATH + "lib386");
        AddLibDir(m_MasterPath + wxFILE_SEP_PATH + "lib386" + wxFILE_SEP_PATH + "nt");
        AddResourceIncludeDir(m_MasterPath + wxFILE_SEP_PATH + "h");
        AddResourceIncludeDir(m_MasterPath + wxFILE_SEP_PATH + "h" + wxFILE_SEP_PATH + "nt");
        m_ExtraPaths.Add(m_MasterPath + wxFILE_SEP_PATH + "binnt");
        m_ExtraPaths.Add(m_MasterPath + wxFILE_SEP_PATH + "binw");
    }
    wxSetEnv("WATCOM", m_MasterPath);

    return wxFileExists(m_MasterPath + wxFILE_SEP_PATH + "binnt" + wxFILE_SEP_PATH + m_Programs.C) ? adrDetected : adrGuessed;
}

void CompilerOW::LoadSettings(const wxString& baseKey)
{
    Compiler::LoadSettings(baseKey);
    wxSetEnv("WATCOM", m_MasterPath);
}

void CompilerOW::SetMasterPath(const wxString& path)
{
    Compiler::SetMasterPath(path);
    wxSetEnv("WATCOM", m_MasterPath);
}
