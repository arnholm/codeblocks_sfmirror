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
#   include "compiler.h"
#   include "cbproject.h"
#   include "projectbuildtarget.h"
#   include "logmanager.h"
#   include "macrosmanager.h"
#endif
#include "compileoptionsbase.h"
#include "compilerOWgenerator.h"


CompilerOWGenerator::CompilerOWGenerator()
{
    //ctor
    m_DebuggerType = wxEmptyString;
}

CompilerOWGenerator::~CompilerOWGenerator()
{
    //dtor
}

wxString CompilerOWGenerator::SetupLibrariesDirs(Compiler* compiler, ProjectBuildTarget* target)
{
    wxArrayString LibDirs = compiler->GetLibDirs();
    if (LibDirs.IsEmpty())
        return wxEmptyString;
    wxString ResultStr = compiler->GetSwitches().libDirs + " ";
    if (target)
    {
        wxString tmp, targetStr, projectStr;
        // First prepare the target
        const wxArrayString targetArr = target->GetLibDirs();
        for (size_t i = 0; i < targetArr.GetCount(); ++i)
        {
            tmp = targetArr[i];
            Manager::Get()->GetMacrosManager()->ReplaceMacros(tmp, target);
            targetStr << tmp << ";";
        }
        // Now for project
        const wxArrayString projectArr = target->GetParentProject()->GetLibDirs();
        for (size_t i = 0; i < projectArr.GetCount(); ++i)
        {
            tmp = projectArr[i];
            Manager::Get()->GetMacrosManager()->ReplaceMacros(tmp, target);
            projectStr << tmp << ";";
        }
        // Decide order and arrange it
        ResultStr << GetOrderedOptions(target, ortLibDirs, projectStr, targetStr);
    }
    // Finally add the compiler options
    const wxArrayString compilerArr = compiler->GetLibDirs();
    wxString tmp, compilerStr;
    for (size_t i = 0; i < compilerArr.GetCount(); ++i)
    {
        tmp = compilerArr[i];
        Manager::Get()->GetMacrosManager()->ReplaceMacros(tmp, target);
        compilerStr << tmp << ";";
    }
    // Now append it
    ResultStr << compilerStr;
    // Remove last ';' char
    ResultStr = ResultStr.Trim(true);
    if (ResultStr.Right(1).IsSameAs(_T(';')))
        ResultStr = ResultStr.RemoveLast();
    return ResultStr;
}

wxString CompilerOWGenerator::SetupLinkerOptions(Compiler* compiler, ProjectBuildTarget* target)
{
    wxString Temp, LinkerOptions, ResultStr;
    wxArrayString ComLinkerOptions, OtherLinkerOptions, LinkerOptionsArr;
    int i, j, Count;

    for (j = 0; j < 3; ++j)
    {
        LinkerOptions = wxEmptyString;
        if (j == 0 && target)
        {
            ComLinkerOptions = target->GetCompilerOptions();
            OtherLinkerOptions = target->GetLinkerOptions();
        }
        else if (j == 1 && target)
        {
            ComLinkerOptions = target->GetParentProject()->GetCompilerOptions();
            OtherLinkerOptions = target->GetParentProject()->GetLinkerOptions();
        }
        else if (j == 2)
        {
            ComLinkerOptions = compiler->GetCompilerOptions();
            OtherLinkerOptions = compiler->GetLinkerOptions();
        }
        if (!ComLinkerOptions.IsEmpty())
        {
            Count = ComLinkerOptions.GetCount();
            for (i = 0; i < Count; ++i)
            {
                Temp = ComLinkerOptions[i];

                // Replace any macros
                Manager::Get()->GetMacrosManager()->ReplaceMacros(Temp, target);

// TODO (Biplab#5#): Move the linker options parsing code to a different function
                //Let's not scan all the options unnecessarily
                if (Temp.Matches("-b*"))
                {
                    if (target)
                    {
                        Temp = MapTargetType(Temp, target->GetTargetType());
                        if (!Temp.IsEmpty() && LinkerOptions.Find("system") == wxNOT_FOUND)
                            LinkerOptions += Temp;
                    }
                }
                // TODO: Map and Set All Debug Flags
                else if (Temp.Matches("-d*") && Temp.Length() <= 4)
                {
                    LinkerOptions = LinkerOptions + MapDebugOptions(Temp);
                }
                // Debugger Type: -hw (Watcom), -hd (Dwarf), -hc (CodeView)
                else if (Temp.Matches("-h?"))
                {
                    MapDebuggerOptions(Temp);
                }
                else if (Temp.StartsWith("-l="))
                {
                    Temp = Temp.AfterFirst(_T('='));
                    if (LinkerOptions.Find("system") == wxNOT_FOUND && !Temp.IsEmpty())
                        LinkerOptions += "system " + Temp + " ";
                }
                else if (Temp.StartsWith("-fm"))
                {
                    LinkerOptions += "option map";
                    int pos = Temp.Find(_T('='));
                    if (pos != wxNOT_FOUND)
                        LinkerOptions += Temp.Mid(pos);
                    LinkerOptions.Append(" ");
                }
                else if (Temp.StartsWith("-k"))
                {
                    LinkerOptions += "option stack=" + Temp.Mid(2) + " ";
                }
                else if (Temp.StartsWith("@"))
                {
                    LinkerOptions += Temp + " ";
                }
            }
        }
        /* Following code will allow user to add any valid linker option
        *  in target's linker option section.
        */
        if (!OtherLinkerOptions.IsEmpty())
        {
            Count = OtherLinkerOptions.GetCount();
            for (i = 0; i < Count; ++i)
            {
                Temp = OtherLinkerOptions[i];
                /* Let's make a small check. It should not start with - or /  */
                if ((Temp[0] != _T('-')) && (Temp[0] != _T('/')))
                    LinkerOptions = LinkerOptions + Temp + " ";
            }
        }
        // Finally add it to an array
        LinkerOptionsArr.Add(LinkerOptions);
    }
    // Arrange them in specified order
    if (target)
        ResultStr = GetOrderedOptions(target, ortLinkerOptions, LinkerOptionsArr[1], LinkerOptionsArr[0]);
    // Now append compiler level options
    ResultStr << LinkerOptionsArr[2];

    return ResultStr;
}

wxString CompilerOWGenerator::SetupLinkLibraries(Compiler* compiler, ProjectBuildTarget* target)
{
    wxString ResultStr;
    wxString targetStr, projectStr, compilerStr;
    wxArrayString Libs;

    if (target)
    {
        // Start with target first
        Libs = target->GetLinkLibs();
        for (size_t i = 0; i < Libs.GetCount(); ++i)
            targetStr << Libs[i] + ",";
        // Next process project
        Libs = target->GetParentProject()->GetLinkLibs();
        for (size_t i = 0; i < Libs.GetCount(); ++i)
            projectStr << Libs[i] + ",";
        // Set them in proper order
        if (!targetStr.IsEmpty() || !projectStr.IsEmpty())
            ResultStr << GetOrderedOptions(target, ortLinkerOptions, projectStr, targetStr);
    }
    // Now prepare compiler libraries, if any
    Libs = compiler->GetLinkLibs();
    for (size_t i = 0; i < Libs.GetCount(); ++i)
        compilerStr << Libs[i] << ",";
    // Append it to result
    ResultStr << compilerStr;
    // Now trim trailing spaces, if any, and the ',' at the end
    ResultStr = ResultStr.Trim(true);
    if (ResultStr.Right(1).IsSameAs(_T(',')))
        ResultStr = ResultStr.RemoveLast();

    if (!ResultStr.IsEmpty())
        ResultStr.Prepend("library ");
    return ResultStr;
}

wxString CompilerOWGenerator::MapTargetType(const wxString& Opt, int target_type)
{
    if (Opt.IsSameAs("-bt=nt") || Opt.IsSameAs("-bcl=nt"))
    {
        if (target_type == ttExecutable || target_type == ttStaticLib) // Win32 Executable
            return "system nt_win ";
        else if (target_type == ttConsoleOnly) // Console
            return "system nt ";
        else if (target_type == ttDynamicLib) // DLL
            return "system nt_dll ";
        else
            return "system nt_win ref '_WinMain@16' "; // Default to Win32 executables
    }
    else if (Opt.IsSameAs("-bt=linux") || Opt.IsSameAs("-bcl=linux"))
    {
        /* The support is experimental. Need proper manual to improve it. */
        return "system linux ";
    }
    return wxEmptyString;
}

/* The following function will be expanded later
   to incorporate detailed debug options
*/
wxString CompilerOWGenerator::MapDebugOptions(const wxString& Opt)
{
    if (Opt.IsSameAs("-d0")) // No Debug
    {
        return wxEmptyString;
    }
    if (Opt.IsSameAs("-d1"))
    {
        return wxString("debug " + m_DebuggerType + "lines ");
    }
    if (Opt.IsSameAs("-d2") || Opt.IsSameAs("-d3"))
    {
        return wxString("debug " + m_DebuggerType + "all ");
    }
    // Nothing Matched
    return wxEmptyString;
}

void CompilerOWGenerator::MapDebuggerOptions(const wxString& Opt)
{
  if (Opt.IsSameAs("-hw"))
  {
      m_DebuggerType = "watcom ";
  }
  else if (Opt.IsSameAs("-hd"))
  {
      m_DebuggerType = "dwarf ";
  }
  else if (Opt.IsSameAs("-hc"))
  {
      m_DebuggerType = "codeview ";
  }
  else
  {
      m_DebuggerType = wxEmptyString;
  }
}
