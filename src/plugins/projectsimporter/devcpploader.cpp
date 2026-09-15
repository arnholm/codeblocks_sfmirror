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
    #include <wx/confbase.h>
    #include <wx/intl.h>
    #include <wx/filename.h>

    #include "manager.h"
    #include "projectmanager.h"
    #include "logmanager.h"
    #include "cbproject.h"
    #include "globals.h"
#endif

#include "devcpploader.h"

#include <wx/fileconf.h>


DevCppLoader::DevCppLoader(cbProject* project)
    : m_pProject(project)
{
    //ctor
}

DevCppLoader::~DevCppLoader()
{
    //dtor
}

bool DevCppLoader::Open(const wxString& filename)
{
    if (! m_pProject)
    {
        return false;
    }

    m_pProject->ClearAllProperties();

    wxFileConfig dev(wxEmptyString, wxEmptyString, filename, wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_NO_ESCAPE_CHARACTERS);
    dev.SetPath("/Project");
    int unitCount{};
    dev.Read("UnitCount", &unitCount, 0);

    wxString path, tmp, title, output, out_path, obj_path;
    wxArrayString array;

    // read project options
    dev.Read("Name", &title, wxEmptyString);
    m_pProject->SetTitle(title);

    dev.Read("CppCompiler", &tmp, wxEmptyString);
    if (tmp.IsEmpty())
        dev.Read("Compiler", &tmp, wxEmptyString);
    array = GetArrayFromString(tmp, "_@@_");
    m_pProject->SetCompilerOptions(array);

    dev.Read("Linker", &tmp, wxEmptyString);
    // some .dev I got my hands on, had the following in the linker options
    // remove them
    tmp.Replace("-o$@", wxEmptyString);
    tmp.Replace("-o $@", wxEmptyString);
    // read the list of linker options
    array = GetArrayFromString(tmp, "_@@_");
    // but separate the libs
    size_t i = 0;
    while (i < array.GetCount())
    {
        if (array[i].StartsWith("-l"))
        {
            wxString tmplib = array[i].Right(array[i].Length() - 2);
            // there might be multiple libs defined in a single line, like:
            // -lmingw32 -lscrnsave -lcomctl32 -lpng -lz -mwindows
            // we got to split by "-l" too...
            if (tmplib.Find(_T(' ')) != wxNOT_FOUND)
            {
                wxArrayString tmparr = GetArrayFromString(array[i], " ");
                while (tmparr.GetCount())
                {
                    if (tmparr[0].StartsWith("-l"))
                        m_pProject->AddLinkLib(tmparr[0].Right(tmparr[0].Length() - 2));
                    else
                        array.Add(tmparr[0]);
                    tmparr.RemoveAt(0, 1);
                }
            }
            else
                m_pProject->AddLinkLib(tmplib);
            array.RemoveAt(i, 1);
        }
        else
            ++i;
    }
    // the remaining are linker options
    m_pProject->SetLinkerOptions(array);

    // read compiler's dirs
    dev.Read("Includes", &tmp, wxEmptyString);
    array = GetArrayFromString(tmp, ";");
    m_pProject->SetIncludeDirs(array);

    // read linker's dirs
    dev.Read("Libs", &tmp, wxEmptyString);
    array = GetArrayFromString(tmp, ";");
    m_pProject->SetLibDirs(array);

    // read resource files
    dev.Read("Resources", &tmp, wxEmptyString);
    array = GetArrayFromString(tmp, ","); // make sure that this is comma-separated
    for (unsigned int j = 0; j < array.GetCount(); ++j)
    {
        if (array[j].IsEmpty())
            continue;
        tmp = array[j];
        m_pProject->AddFile(0, tmp, true, true);
    }

    // read project units
    for (int x = 0; x < unitCount; ++x)
    {
        path.Printf("/Unit%d", x + 1);
        dev.SetPath(path);
        tmp.Clear();
        dev.Read("FileName", &tmp, wxEmptyString);
        if (tmp.IsEmpty())
            continue;

        bool compile{};
        dev.Read("Compile", &compile, false);
        bool compileCpp{};
        dev.Read("CompileCpp", &compileCpp, true);
        bool link{};
        dev.Read("Link", &link, true);

        // .dev files set Link=0 for resources which is plain wrong for C::B.
        // correct this...
        if (!link && FileTypeOf(tmp) == ftResource)
            link = true;

        ProjectFile* pf = m_pProject->AddFile(0, tmp, compile || compileCpp, link);
        if (pf)
            pf->compilerVar = compileCpp ? "CPP" : "CC";
    }
    dev.SetPath("/Project");

    // set the target type
    ProjectBuildTarget* const target = m_pProject->GetBuildTarget(0);
    if (! target)
    {
      return false;
    }
    int typ{};
    dev.Read("Type", &typ, 0);
    target->SetTargetType(TargetType(typ));

    // decide on the output filename
    if (dev.ReadLong("OverrideOutput", 0) == 1)
        dev.Read("OverrideOutputName", &output, wxEmptyString);
    if (output.IsEmpty())
        output = target->SuggestOutputFilename();
    dev.Read("ExeOutput", &out_path, wxEmptyString);
    if (!out_path.IsEmpty())
        output = out_path + "\\" + output;
    target->SetOutputFilename(output);

    // set the object output
    dev.Read("ObjectOutput", &obj_path, wxEmptyString);
    if (!obj_path.IsEmpty())
        target->SetObjectOutput(obj_path);

    m_pProject->SetModified(true);
    return true;
}

bool DevCppLoader::Save(cb_unused const wxString& filename)
{
    // no support to save DevCpp projects
    return false;
}
