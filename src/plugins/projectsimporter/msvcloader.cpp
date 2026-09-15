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
    #include <wx/fileconf.h>
    #include <wx/msgdlg.h>
    #include <wx/intl.h>
    #include <wx/filename.h>
    #include <wx/txtstrm.h>
    #include <wx/dynarray.h>
    #include <wx/wfstream.h>

    #include "manager.h"
    #include "projectmanager.h"
    #include "logmanager.h"
    #include "cbproject.h"
    #include "globals.h"
    #include "compilerfactory.h"
    #include "compiler.h"
#endif

#include <wx/stream.h>
#include <wx/tokenzr.h>

#include "prep.h"
#include "importers_globals.h"
#include "msvcloader.h"
#include "multiselectdlg.h"

/* NOTE:- Replacing all wxString::Remove(size_t, size_t) with wxString::Mid()
 * and Truncate() functions. This function has been marked as a wx-1.xx
 * compatibility function in wxWidgets-2.8. So in future it will be dropped.
 */

MSVCLoader::MSVCLoader(cbProject* project) :
    m_pProject(project),
    m_ConvertSwitches(true)
{
    //ctor
}

MSVCLoader::~MSVCLoader()
{
    //dtor
}

bool MSVCLoader::Open(const wxString& filename)
{
    /* NOTE (mandrav#1#): not necessary to ask for switches conversion... */
    m_ConvertSwitches = m_pProject->GetCompilerID().IsSameAs("gcc");

    m_Filename = filename;
    if (!ReadConfigurations())
        return false;

    // the file is read, now process it
    Manager::Get()->GetLogManager()->DebugLog("Importing MSVC project: " + filename);

    // delete all targets of the project (we 'll create new ones from the imported configurations)
    while (m_pProject->GetBuildTargetsCount())
        m_pProject->RemoveBuildTarget(0);

    wxArrayInt selected_indices;
    if (ImportersGlobals::ImportAllTargets)
    {
        // don't ask; just fill selected_indices with all indices
        for (size_t i = 0; i < m_Configurations.GetCount(); ++i)
            selected_indices.Add(i);
    }
    else
    {
        // ask the user to select a configuration - multiple choice ;)
        MultiSelectDlg dlg(0, m_Configurations, true, _("Select configurations to import:"), m_Filename.GetName());
        PlaceWindow(&dlg);
        if (dlg.ShowModal() == wxID_CANCEL)
        {
            Manager::Get()->GetLogManager()->DebugLog("Canceled...");
            return false;
        }
        selected_indices = dlg.GetSelectedIndices();
    }

    // create all selected targets
    for (size_t i = 0; i < selected_indices.GetCount(); ++i)
    {
        if (!ParseConfiguration(selected_indices[i]))
            return false;
    }

    m_pProject->SetTitle(m_Filename.GetName());
    return ParseSourceFiles();
}

bool MSVCLoader::Save(cb_unused const wxString& filename)
{
    // no support to save MSVC projects
    return false;
}

bool MSVCLoader::ReadConfigurations()
{
    m_Configurations.Clear();
    m_ConfigurationsLineIndex.Clear();
    m_BeginTargetLine = -1;

    wxFileInputStream file(m_Filename.GetFullPath());
    if (!file.Ok())
        return false; // error opening file???

    wxTextInputStream input(file);

    int currentLine = 0;
    while (!file.Eof())
    {
        wxString line = input.ReadLine();
        ++currentLine;
        line.Trim(true).Trim(false);

        int size = -1;
        if (line.StartsWith("# TARGTYPE"))
        {
            // # TARGTYPE "Win32 (x86) Application" 0x0103
            int idx = line.Find(' ', true);
            if (idx != -1)
            {
                TargetType type;
                wxString targtype = line.Mid(12, idx-1-12);
                wxString projcode = line.Mid(idx+3, 4);
                if      (projcode.Matches("0101")) type = ttExecutable;
                else if (projcode.Matches("0102")) type = ttDynamicLib;
                else if (projcode.Matches("0103")) type = ttConsoleOnly;
                else if (projcode.Matches("0104")) type = ttStaticLib;
                else if (projcode.Matches("010a")) type = ttCommandsOnly;
                else
                {
                    type = ttCommandsOnly;
                    Manager::Get()->GetLogManager()->DebugLog("unrecognized target type");
                }

                //Manager::Get()->GetLogManager()->DebugLog("TargType '%s' is %d", targtype.c_str(), type);
                m_TargType[targtype] = type;
            }
            continue;
        }
        else if (line.StartsWith("!MESSAGE \""))
        {
            //  !MESSAGE "anothertest - Win32 Release" (based on "Win32 (x86) Application")
            int pos;
            pos = line.Find('\"');
            line = line.Mid(pos + 1);
            pos = line.Find('\"');
            wxArrayString projectTarget = GetArrayFromString(line.Left(pos), "-");
            wxString target = projectTarget[1];
            if (projectTarget.GetCount() != 2)
            {
                Manager::Get()->GetLogManager()->DebugLog("ERROR: bad target format");
                return false;
            }
            line = line.Mid(pos + 1);
            pos = line.Find('\"');
            line = line.Mid(pos + 1);
            pos = line.Find('\"');
            wxString basedon = line.Left(pos);
            TargetType type = ttCommandsOnly;
            HashTargetType::iterator it = m_TargType.find(basedon);
            if (it != m_TargType.end())
                type = it->second;
            else
            {
                Manager::Get()->GetLogManager()->DebugLog("ERROR: target type not found");
                return false;
            }
            m_TargetBasedOn[target] = type;
            //Manager::Get()->GetLogManager()->DebugLog("Target '%s' type %d", target.c_str(), type);
        }
        else if (line.StartsWith("!IF  \"$(CFG)\" =="))
            size = 16;
        else if (line.StartsWith("!ELSEIF  \"$(CFG)\" =="))
            size = 20;
        else if (line == "# Begin Target")
        {
            // done
            m_BeginTargetLine = currentLine;
            break;
        }
        if (size != -1)
        {
            // read configuration name
            line = line.Mid(size);
            line.Trim(true).Trim(false);

            wxString tmp = RemoveQuotes(line);
            // remove the project name part, i.e "anothertest - "
            int idx = tmp.Find('-');
            if (idx != -1)
            {
                tmp = tmp.Mid(idx + 1);
                tmp.Trim(false);
            }
            if (m_Configurations.Index(tmp) == wxNOT_FOUND)
            {
                m_Configurations.Add(tmp);
                m_ConfigurationsLineIndex.Add(currentLine);
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Detected configuration '%s' at line %d", tmp, currentLine));
            }
        }
    }
    return true;
}

bool MSVCLoader::ParseConfiguration(int index)
{
    wxFileInputStream file(m_Filename.GetFullPath());
    if (!file.Ok())
        return false; // error opening file???

    // create new target
    ProjectBuildTarget* bt = m_pProject->AddBuildTarget(m_Configurations[index]);
    if (!bt)
        return false;
    bt->SetCompilerID(m_pProject->GetCompilerID());
    m_Type = ttCommandsOnly;
    HashTargetType::iterator it = m_TargetBasedOn.find(m_Configurations[index]);
    if (it != m_TargetBasedOn.end()) m_Type = it->second;
    else Manager::Get()->GetLogManager()->DebugLog("ERROR: could not find the target type of " + m_Configurations[index]);
    bt->SetTargetType(m_Type);
    bt->SetOutputFilename(bt->SuggestOutputFilename());

    wxTextInputStream input(file);

    // go to the configuration's line
    int currentLine = 0;
    while (!file.Eof() && currentLine <= m_ConfigurationsLineIndex[index])
    {
        input.ReadLine();
        ++currentLine;
    }

    // start parsing the configuration
    while (!file.Eof())
    {
        wxString line = input.ReadLine();
        line.Trim(true).Trim(false);

        // we want empty lines (skipped) or lines starting with #
        // if we encounter a line starting with !, we break out of here
        if (line.GetChar(0) == '!')
        {
            break;
        }

        if (line.IsEmpty() || line.GetChar(0) != '#')
        {
            // Unhandled, but for this exception:
            if (line.StartsWith("PostBuild_Cmds="))
            {
                line = line.Mid(15);

                while (!line.StartsWith("# End Special Build Tool"))
                {
                    bool next_line = line.EndsWith("\\");
                    ProcessPostBuildCommand(bt, line);

                    if (!next_line)
                        break; // exit while-loop

                    // next line (iteration) as post build commands are usually multiple lines
                    line = input.ReadLine();
                    line.Trim(true).Trim(false);
                }
            }
            else
                continue;
        }

//        if (line.StartsWith("# PROP BASE Output_Dir "))
        if (line.StartsWith("# PROP Output_Dir "))
        {
            line = line.Mid(18);
            line.Trim(true).Trim(false);
            wxString tmp = RemoveQuotes(line);
            if (!line.IsEmpty())
            {
                wxFileName out = bt->GetOutputFilename();
                out.SetPath(tmp); // out could be a full path name and not only a relative one !
                if (out.IsRelative())
                    out.MakeAbsolute(m_Filename.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR));
                bt->SetOutputFilename(out.GetFullPath());
            }
        }
//        else if (line.StartsWith("# PROP BASE Intermediate_Dir "))
        else if (line.StartsWith("# PROP Intermediate_Dir "))
        {
            line = line.Mid(24);
            line.Trim(true).Trim(false);
            wxString tmp = RemoveQuotes(line);
            if (!line.IsEmpty())
            {
                bt->SetObjectOutput(tmp);
            }
        }
        else if (line.StartsWith("# ADD BASE CPP "))
        {
            line = line.Mid(15);
            line.Trim(true).Trim(false);
            ProcessCompilerOptions(bt, line);
        }
        else if (line.StartsWith("# ADD CPP "))
        {
            line = line.Mid(10);
            line.Trim(true).Trim(false);
            ProcessCompilerOptions(bt, line);
        }
        else if (line.StartsWith("# ADD BASE LINK32 "))
        {
            line = line.Mid(18);
            line.Trim(true).Trim(false);
            ProcessLinkerOptions(bt, line);
        }
        else if (line.StartsWith("# ADD LINK32 "))
        {
            line = line.Mid(13);
            line.Trim(true).Trim(false);
            ProcessLinkerOptions(bt, line);
        }
        else if (line.StartsWith("# ADD BASE RSC ")) // To import resource compiler options
        {
            line = line.Mid(16);
            line.Trim(true).Trim(false);
            ProcessResourceCompilerOptions(bt, line);
        }
        else if (line.StartsWith("# ADD RSC "))
        {
            line = line.Mid(11);
            line.Trim(true).Trim(false);
            ProcessResourceCompilerOptions(bt, line);
        }
    }
    return true;
}

bool MSVCLoader::ParseSourceFiles()
{
    wxFileInputStream file(m_Filename.GetFullPath());
    if (!file.Ok())
        return false; // error opening file???

    wxTextInputStream input(file);
    wxString LastProcessedFile = wxEmptyString;
    wxString CurCFG;
    bool FoundIf = false;
    size_t size;

    // go to the begining of source files
    int currentLine = 0;
    while (!file.Eof() && currentLine < m_BeginTargetLine)
    {
        input.ReadLine();
        ++currentLine;
    }

    while (!file.Eof())
    {
        wxString line = input.ReadLine();
        line.Trim(true).Trim(false);

        if (line.StartsWith("SOURCE="))
        {
            line = line.Mid(7);
            line.Trim(true).Trim(false);

            wxString fname (RemoveQuotes(line));

            if ((!fname.IsEmpty()) && (fname != ".\\"))
            {
                if (fname.StartsWith(".\\"))
                    fname.erase(0, 2);

                if (!platform::windows)
                    fname.Replace("\\", "/", true);

                ProjectFile* pf = m_pProject->AddFile(0, fname);
                if (pf)
                {
                    LastProcessedFile = fname;
                    // add it to all configurations, not just the first
                    for (int i = 1; i < m_pProject->GetBuildTargetsCount(); ++i)
                        pf->AddBuildTarget(m_pProject->GetBuildTarget(i)->GetTitle());
                }
            }
        }
        else if (line.StartsWith("!"))
        {
            FoundIf = true;
            if (line.StartsWith("!IF  \"$(CFG)\" =="))
                size = 16;
            else if (line.StartsWith("!ELSEIF  \"$(CFG)\" =="))
                size = 20;
            else
            {
                size = 0;
                FoundIf = false;
            }
            if (size > 0)
            {
                CurCFG = line.Mid(size);
                CurCFG = RemoveQuotes(CurCFG.Trim(false).Trim(true));
                CurCFG = CurCFG.Mid(CurCFG.Find("-") + 1).Trim(true).Trim(false);
            }
            if (line.StartsWith("!ENDIF"))
            {
                FoundIf = false;
                CurCFG = wxEmptyString;
                LastProcessedFile = wxEmptyString;
            }
        }
        else if (line.StartsWith("#"))
        {
            if (FoundIf && line.StartsWith("# PROP Exclude_From_Build "))
            {
                line.Trim(true);
                if (line.Right(1).IsSameAs("1"))
                {
                    ProjectFile* pf = m_pProject->GetFileByFilename(LastProcessedFile);
                    if (pf)
                    {
                        for (int j = 0; j < m_pProject->GetBuildTargetsCount(); ++j)
                        {
                            if (m_pProject->GetBuildTarget(j)->GetTitle().IsSameAs(CurCFG))
                            {
                                pf->RemoveBuildTarget(CurCFG);
                                Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Buid target %s has been excluded from %s",
																		CurCFG.c_str(), LastProcessedFile.c_str()));
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}

void MSVCLoader::ProcessCompilerOptions(ProjectBuildTarget* target, const wxString& opts)
{
    wxArrayString array;
    array = OptStringTokeniser(opts);

    for (unsigned int i = 0; i < array.GetCount(); ++i)
    {
        wxString opt = array[i];
        opt.Trim();

        if (m_ConvertSwitches)
        {
            if (opt.Matches("/D"))
                target->AddCompilerOption("-D" + RemoveQuotes(array[++i]));
            else if (opt.Matches("/U"))
                target->AddCompilerOption("-U" + RemoveQuotes(array[++i]));
            else if (opt.Matches("/Zi") || opt.Matches("/ZI"))
                target->AddCompilerOption("-g");
            else if (opt.Matches("/I"))
                target->AddIncludeDir(RemoveQuotes(array[++i]));
            else if (opt.Matches("/W0"))
                target->AddCompilerOption("-w");
            else if (opt.Matches("/O1") ||
                     opt.Matches("/O2") ||
                     opt.Matches("/O3"))
                target->AddCompilerOption("-O2");
            else if (opt.Matches("/W1") ||
                     opt.Matches("/W2") ||
                     opt.Matches("/W3"))
                target->AddCompilerOption("-W");
            else if (opt.Matches("/W4"))
                target->AddCompilerOption("-Wall");
            else if (opt.Matches("/WX"))
                target->AddCompilerOption("-Werror");
            else if (opt.Matches("/GX"))
                target->AddCompilerOption("-fexceptions");
            else if (opt.Matches("/Ob0"))
                target->AddCompilerOption("-fno-inline");
            else if (opt.Matches("/Ob2"))
                target->AddCompilerOption("-finline-functions");
            else if (opt.Matches("/Oy"))
                target->AddCompilerOption("-fomit-frame-pointer");
            else if (opt.Matches("/GB"))
                target->AddCompilerOption("-mcpu=pentiumpro -D_M_IX86=500");
            else if (opt.Matches("/G6"))
                target->AddCompilerOption("-mcpu=pentiumpro -D_M_IX86=600");
            else if (opt.Matches("/G5"))
                target->AddCompilerOption("-mcpu=pentium -D_M_IX86=500");
            else if (opt.Matches("/G4"))
                target->AddCompilerOption("-mcpu=i486 -D_M_IX86=400");
            else if (opt.Matches("/G3"))
                target->AddCompilerOption("-mcpu=i386 -D_M_IX86=300");
            else if (opt.Matches("/Za"))
                target->AddCompilerOption("-ansi");
            else if (opt.Matches("/Zp1"))
                target->AddCompilerOption("-fpack-struct");
            else if (opt.Matches("/nologo"))
            {
                // do nothing (ignore silently)
            }
            else if (opt.Matches("/c"))
            {
                // do nothing (ignore silently)
            }
            else if (opt.StartsWith("@"))
            {
                wxArrayString options;
                if (ParseResponseFile(m_pProject->GetBasePath() + opt.Mid(1), options))
                {
                    for (size_t j = 0; j < options.GetCount(); ++j)
                        ProcessCompilerOptions(target, options[j]);
                }
                else
                { // Fallback: Remember GCC will process Pre-processor macros only
                    Manager::Get()->GetLogManager()->DebugLog("Can't open " + m_pProject->GetBasePath() + opt.Mid(1) + " for parsing");
                    target->AddCompilerOption("-imacros " + opt.Mid(1));
                }
            }
            //else Manager::Get()->GetLogManager()->DebugLog("Unhandled compiler option: " + opt);
        }
        else // !m_ConvertSwitches
        {
            // only differentiate includes and definitions
            if (opt.Matches("/I"))
                target->AddIncludeDir(RemoveQuotes(array[++i]));
            else if (opt.Matches("/D"))
                target->AddCompilerOption("/D" + RemoveQuotes(array[++i]));
            else if (opt.Matches("/U"))
                target->AddCompilerOption("/U" + RemoveQuotes(array[++i]));
            else if (opt.StartsWith("/Yu"))
                Manager::Get()->GetLogManager()->DebugLog("Ignoring precompiled headers option (/Yu)");
            else if (opt.Matches("/c") || opt.Matches("/nologo"))
            {
                // do nothing (ignore silently)
            }
            else
                target->AddCompilerOption(opt);
        }
    }
}

void MSVCLoader::ProcessLinkerOptions(ProjectBuildTarget* target, const wxString& opts)
{
    wxArrayString array;
    array = OptStringTokeniser(opts);

    for (unsigned int i = 0; i < array.GetCount(); ++i)
    {
        wxString opt = array[i];
        opt.Trim();

        if (m_ConvertSwitches)
        {
            if (opt.StartsWith("/libpath:"))
            {
                opt = opt.Mid(9);
                target->AddLibDir(RemoveQuotes(opt));
            }
            else if (opt.StartsWith("/base:"))
            {
                opt = opt.Mid(6);
                target->AddLinkerOption("--image-base " + RemoveQuotes(opt));
            }
            else if (opt.StartsWith("/implib:"))
            {
                opt = opt.Mid(8);
                target->AddLinkerOption("--implib " + RemoveQuotes(opt));
            }
            else if (opt.StartsWith("/map:"))
            {
                opt = opt.Mid(5);
                target->AddLinkerOption("-Map " + RemoveQuotes(opt) + ".map");
            }
            else if (opt.Matches("/nologo"))
            {
                // do nothing (ignore silently)
            }
            else if (opt.Matches("/dll"))
            {
                opt = opt.Mid(4);
                target->AddLinkerOption("--dll " + RemoveQuotes(opt));
            }
            else if (opt.StartsWith("/out:"))
            {
                // do nothing; it is handled below, in common options
            }
            else if (opt.StartsWith("@"))
            {
                wxArrayString options;
                if (ParseResponseFile(m_pProject->GetBasePath() + opt.Mid(1), options))
                {
                    for (size_t j = 0; j < options.GetCount(); ++j)
                        ProcessLinkerOptions(target, options[j]);
                } // else ignore
            }
            else if (opt.Find(".lib") == -1) // don't add linking lib (added below, in common options)
                Manager::Get()->GetLogManager()->DebugLog(_T("Unknown linker option: " + opt));
        }
        else // !m_ConvertSwitches
        {
            if (opt.StartsWith("/libpath:"))
            {
                opt = opt.Mid(9);
                target->AddLibDir(RemoveQuotes(opt));
            }
            else if (opt.Matches("/nologo")) {} // ignore silently
            else if (opt.StartsWith("@"))
                target->AddLinkerOption(opt);
            else
            {
                // don't add linking lib (added below, in common options)
                int idx = opt.Find(".lib");
                if (idx == -1)
                    target->AddLinkerOption(opt);
            }
        }

        // common options
        if (!opt.StartsWith("/"))
        {
            // probably linking lib
            int idx = opt.Find(".lib");
            if (idx != -1)
            {
                opt.Truncate(idx);
                target->AddLinkLib(opt);
            }
        }
        else if (opt.StartsWith("/out:"))
        {
            opt = opt.Mid(5);
            opt = RemoveQuotes(opt);
            if (m_Type == ttStaticLib)
            {
                // convert lib filename based on compiler
                /* NOTE (mandrav#1#): I think I should move this code somewhere more accessible...
                I need it here and there... */
                wxFileName orig = target->GetOutputFilename();
                wxFileName newf = opt;
                if (newf.IsRelative())
                    newf.MakeAbsolute(m_pProject->GetBasePath());
                Compiler* compiler = CompilerFactory::GetCompiler(m_pProject->GetCompilerID());
                if (compiler)
                {
                    newf.SetExt(compiler->GetSwitches().libExtension);
                    wxString name = newf.GetName();
                    wxString prefix = compiler->GetSwitches().libPrefix;
                    if (!prefix.IsEmpty() && !name.StartsWith(prefix))
                        newf.SetName(prefix + name);
                }
                target->SetOutputFilename(newf.GetFullPath());
            }
            else
                target->SetOutputFilename(opt);
        }
    }
}

void MSVCLoader::ProcessResourceCompilerOptions(ProjectBuildTarget* target, const wxString& opts)
{
    wxArrayString array;
    array = OptStringTokeniser(opts);

    for (unsigned int i = 0; i < array.GetCount(); ++i)
    {
        wxString opt = array[i];
        opt.Trim();

        if (opt.StartsWith("/"))
        {
            if (opt.StartsWith("/i")) // Only include dir is imported
                target->AddResourceIncludeDir(RemoveQuotes(array[++i]));
        }
    }
}

void MSVCLoader::ProcessPostBuildCommand(ProjectBuildTarget* target, const wxString& cmd)
{
    wxString post_build_cmd = cmd;

/*
     remove "line continues", like in:
     PostBuild_Cmds=if NOT exist ..\binary\*.* mkdir ..\binary	\
     copy $(OutDir)\NetHackW.exe ..\binary
*/
    if (post_build_cmd.EndsWith("\\"))
        post_build_cmd.RemoveLast(1).Trim(true).Trim(false);

    if (!post_build_cmd.IsEmpty())
    {
        // Some weirdness: VC6 separates commands by a linefeed OR a TAB (?!)
        // Linefeeds are handled by the caller, we do handle tabs here only.
        wxStringTokenizer tkz(post_build_cmd, "\t");
        while (tkz.HasMoreTokens())
        {
            // process token
            wxString current_post_build_cmd = tkz.GetNextToken().Trim(true).Trim(false);
            if (!current_post_build_cmd.IsEmpty())
                target->AddCommandsAfterBuild(current_post_build_cmd);
        }
    }
}

wxArrayString MSVCLoader::OptStringTokeniser(const wxString& opts)
{
    // tokenise string like:
    // wsock32.lib /nologo /machine:I386 /libpath:"lib" /libpath:"C:\My Folder"

    wxArrayString out;

    wxString search = opts;
    search.Trim(true).Trim(false);

    // trivial case: string is empty or consists of blanks only
    if (search.IsEmpty())
        return out;

    wxString token;
    bool     inside_quot = false;
    size_t   pos         = 0;
    while (pos < search.Length())
    {
        wxString current_char = search.GetChar(pos);

        // for e.g. /libpath:"C:\My Folder"
        if (current_char.CompareTo(wxS("\""))==0) // equality
            inside_quot = !inside_quot;

        if ((current_char.CompareTo(wxS(" "))==0) && (!inside_quot))
        {
            if (!token.IsEmpty())
            {
                out.Add(token);
                token.Clear();
            }
        }
        else
        {
            token.Append(current_char);
        }

        pos++;
        // Append final token
        if ((pos==search.Length()) && (!inside_quot) && (!token.IsEmpty()))
            out.Add(token);
    }

    return out;
}

wxString MSVCLoader::RemoveQuotes(const wxString& src)
{
    wxString res = src;
    if (res.StartsWith("\""))
    {
        res = res.Mid(1);
        res.Truncate(res.Length() - 1);
    }
//    Manager::Get()->GetLogManager()->DebugLog("Removing quotes: %s --> %s", src.c_str(), res.c_str());
    return res;
}

bool MSVCLoader::ParseResponseFile(const wxString filename, wxArrayString& output)
{
    /* Note: MSDN says user cannot call another response file
     * from a response file. Thus it's quite safe to parse the file. */
    bool success = false;
    wxFileInputStream inp_file(filename);
    if (inp_file.Ok())
    {
        wxTextInputStream inp_txt(inp_file);
        success = true;
        while (!inp_file.Eof())
            output.Add(inp_txt.ReadLine());
    }
    else
        success = false;
    return success;
}
