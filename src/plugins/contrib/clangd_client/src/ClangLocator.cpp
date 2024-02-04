
#include <string>
#include "ClangLocator.h"

#include <wx/string.h>
#include <wx/arrstr.h>
#include <wx/filefn.h>
#include <wx/utils.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/dir.h>
#if defined(__WXMSW__)
    #include <wx/msw/registry.h>
    #include "winprocess/asyncprocess/procutils.h"
#else
    #include "procutils.h"
#endif // __WXMSW__

#include "globals.h"
#include "manager.h"
#include "logmanager.h"
#include "cbproject.h"
#include "compiler.h"
#include "compilerfactory.h"
#include "configmanager.h"
#include "projectmanager.h"
#include "codecompletion/parser/cclogger.h"
#include "infowindow.h"

#if defined(_WIN32)
#include "winprocess/asyncprocess/procutils.h"
#else
#include "procutils.h"
#endif


// ----------------------------------------------------------------------------
namespace   //anonymous
// ----------------------------------------------------------------------------
{
    #if defined(_WIN32)
        wxString clangdexe("clangd.exe");
        wxString LLVM_Dirmaybe = "c:\\program files\\LLVM\\bin";
    #else
        wxString clangdexe("clangd");
        wxString LLVM_Dirmaybe = "/usr/bin";
        #ifdef __WXMAC__
            wxString LLVM_DirmaybeMAC = "/usr/local/opt/llvm/bin";
        #endif
    #endif

    wxString fileSep   = wxFILE_SEP_PATH;
    wxChar fileSepChar = wxFILE_SEP_PATH;
    bool wxFound(int result){return result != wxNOT_FOUND;};
}
// ----------------------------------------------------------------------------
ClangLocator::ClangLocator()
// ----------------------------------------------------------------------------
{
    //ctor
    wxUnusedVar(fileSepChar);
}
// ----------------------------------------------------------------------------
ClangLocator::~ClangLocator()
// ----------------------------------------------------------------------------
{
    //dtor
}
// ----------------------------------------------------------------------------
wxString ClangLocator::Locate_ResourceDir(wxFileName fnClangd)
// ----------------------------------------------------------------------------
{
    // Find the resource dir used by this version of clangd

    wxString clangdDir = fnClangd.GetPath();
    wxString clangdExecutable = fnClangd.GetFullName();
    if (clangdDir.empty())
    {
        clangdExecutable = Locate_ClangdDir();
        if (clangdExecutable.Length()) clangdExecutable += (fileSep + clangdexe);
    }

    if (clangdExecutable.empty() or (not fnClangd.Exists()) )
        return wxString();

    // Get the version of this clangd, we need to match it with the same resources dir
    wxString clangdVersion;
    wxString cmdLine = fnClangd.GetFullPath();
    // Use clangd to get the clang version
    cmdLine.Append (" --version");
    wxArrayString clangdResponse;
    // If on first run, wxExecute() stalls on Windows 11 for 3-6 seconds,
    // change Windows Settings>Windows security>App and Browser control>Smart App Control Settings>off
    wxExecute(cmdLine, clangdResponse);
    if (not clangdResponse.Count())
        return wxString(); //somethings wrong with clangd.exe

    if (clangdResponse.Count()) //tease out the version number
    {
        cmdLine = clangdResponse[0]; //usually "clangd version 13.0.1" followed by a space or -+whatever
        CCLogger::Get()->DebugLog("Using Clangd version: " + clangdResponse[0]);
        size_t sBgn = cmdLine.find("version ");
        if (sBgn) sBgn += 8; //jump over "version"
        size_t sEnd = sBgn;
        for( ; sEnd < cmdLine.length(); ++sEnd)
        {
            if ( ((cmdLine[sEnd] >= '0') and (cmdLine[sEnd] <= '9')) or (cmdLine[sEnd] == '.') )
                continue;
            break;
        }
        if (sBgn and sEnd)
            clangdVersion = cmdLine.SubString(sBgn, sEnd-1);
    }

    if (clangdVersion.Length())
    {
        int versionMajorID = std::stoi(clangdVersion.ToStdString());
        if (versionMajorID < 13)
        {
            wxString msg = wxString::Format(_("Error: clangd version (%s) is older than the required version 13."), clangdVersion);
            CCLogger::Get()->DebugLogError(msg);
        }
    }

    wxFileName fnClangdExecutablePath(clangdDir, clangdExecutable);
    if (fnClangdExecutablePath.GetPath().EndsWith(fileSep + "bin"))
    {
        fnClangdExecutablePath.RemoveLastDir();
        fnClangdExecutablePath.AppendDir("lib");
        fnClangdExecutablePath.AppendDir("clang");
        fnClangdExecutablePath.AppendDir(clangdVersion);
    }
    fnClangdExecutablePath.SetName(wxString("clang") << "-" << clangdVersion);
    wxString resource = fnClangdExecutablePath.GetFullPath(); // **Debugging**

    if (fnClangdExecutablePath.DirExists())
        return fnClangdExecutablePath.GetPath();
    else
    {
        // Example: \llvm\lib\16.0.0\ not found because the clangd version does not match
        // lib name \llvm\lib\16\ . (Why do they keep changing the naming convention?)
        // so, back up from end of version string to see if we can find it.
        // It might have a shorter folder name.
        wxString dirname = fnClangdExecutablePath.GetPath();
        wxString sep = fnClangdExecutablePath.GetPathSeparator();
        wxChar chr = dirname.Last();
        while (dirname.Length() and (chr != sep) )
        {
            dirname.RemoveLast();
            if (dirname.Last() == '.') dirname.RemoveLast();
            if (dirname.EndsWith(sep)) break;
            if (wxDirExists(dirname))
                return dirname;
            chr = dirname.Last();
        }
    }

    // Failed to find the clangd resource directory //(ph 2024/01/31)
    // Search the clang path to find a matching resource lib
    //wxString clangExePath = fnClangdExecutablePath.GetFullPath(); // **Debugging**
    wxString resourceDir = SearchAllLibDirsForResourceDir(fnClangdExecutablePath);
    if (resourceDir.Length()) //empty or have "ver|libPath"
    {
        wxString verNum = resourceDir.BeforeFirst('|');
        resourceDir = resourceDir.AfterFirst('|');
        return resourceDir+fileSep+"clang"+fileSep+verNum;
    }

    // Failed to find the clangd resource directory
    // Say that we can't find the clangd resource directory
    wxString msg = wxString::Format(_("Error: %s\n clangd version (%s) was unable to locate the necessary clangd resource directory."), __FUNCTION__, clangdVersion);
    msg << "\n\t" << fnClangdExecutablePath.GetPath();
    CCLogger::Get()->LogError(msg);
    CCLogger::Get()->DebugLogError(msg);
    cbMessageBox(msg);

    return wxString();
}//end Locate_ResourceDir
// ----------------------------------------------------------------------------
wxString ClangLocator::SearchAllLibDirsForResourceDir(wxFileName ClangExePath)
// ----------------------------------------------------------------------------
{
    // Find all "lib*" dirs that can contain the clang/clangd resource header et.

    //InfoWindow::Display(_("LSP Searching"), _("Searching for clangd resources"), 7000 );
    // ClangExePath will look like this:
    //Ref:  F:\\usr\\programs\\msys64_13.0.1\\mingw64\\lib\\clang\\16.0.5\\clang-16.0.5.exe"

    wxString clangPath = ClangExePath.GetPath();
    // chop it down to just before "lib*" paren, usually "clang"
    int pos = clangPath.find(fileSep+"lib"+fileSep);
    clangPath = clangPath.Mid(0,pos);

    // Verify that the lib parent exists
    if (not wxDirExists(clangPath))
        return wxString();

    // remember our current directory to be restored later
    // and set the pwd to the clang dir which might contain the resources
    wxString priorDir = wxGetCwd();
    wxSetWorkingDirectory(clangPath);
    wxString versionNum = ClangExePath.GetPath().Mid(pos);
    versionNum = versionNum.AfterLast(fileSep[0]);

    // Extract the clangd version number from the input path
    wxString firstLevelVersionNum = versionNum.BeforeFirst('.');
    wxArrayString listOfLibDirs;
    // Find any .../clang/lib* that contains the version number (or partial num),
    // placing the verNum and path of any dir found.
    FindClangResourceDirs(clangPath, firstLevelVersionNum, listOfLibDirs);

    // Restore to the prio directory
    wxSetWorkingDirectory(priorDir) ;

    // contents of listOfLibDirs is text string of
    // "version string | fullPath", using '|' as the separator
    if (listOfLibDirs.GetCount() == 0)
        return wxString();
    if (listOfLibDirs.GetCount() == 1)
       return listOfLibDirs[0];
    // Find any dir that matches the version number (or partial match)
    //Backup through the version Number looking for a match,
    // eg., 17.0.3 or 17.0 or 17
    // The array entries look like "versionNum|libpath", '|' is a separator char.
    for(int lvl=0; lvl<3; ++lvl)        // iterate through all version levels
    {
        for (size_t ii=0; ii<listOfLibDirs.GetCount();  ++ii) // iterate through the lib dir names
        {
            wxString verAndLibPath = listOfLibDirs[ii];
            wxString ver = verAndLibPath.BeforeFirst('|');
            //wxString lib = verAndLibPath.AfterFirst('|'); <== not neededd yet

            if (ver == versionNum)
                return verAndLibPath;

        }
        versionNum = versionNum.BeforeLast('.'); //Remove last level of version num
    }

    return wxString();

}//end SearchAllLibDirsForResourceDir
// ----------------------------------------------------------------------------
void ClangLocator::FindClangResourceDirs(const wxString& path, wxString& firstLevelVersionNum, wxArrayString& versionPaths)
// ----------------------------------------------------------------------------
{
    wxString priorDir = wxGetCwd();
    wxSetWorkingDirectory(path);

   // ----------------------------------------------------------------------------
   #if not defined (__WXMSW__) // Linux <=============================
   // ----------------------------------------------------------------------------
    wxString LinuxCmdTemplate = "sh -c 'ls -ld lib*/clang/%s*'";
    wxString LinuxCmd = wxString::Format(LinuxCmdTemplate, firstLevelVersionNum);

    wxSetWorkingDirectory(path);
    wxExecute(LinuxCmd, versionPaths);
    size_t respCount = versionPaths.GetCount();
    for (size_t ii=0; ii<respCount; ++ii)
    {
        wxString result = versionPaths[ii];
        wxString respath = result.AfterLast(' ').BeforeLast(fileSepChar);
        wxString verNum = result.AfterLast(fileSepChar);
        versionPaths[ii] = verNum + "|" + path + fileSepChar + respath;
        // **Debugging**
        //wxString msg = wxString::Format("versionPath:%d %s", int(ii), versionPaths[ii]);
        //CCLogger::Get()->DebugLog(msg);
        //asm("nop"); // ,----- set breakpoint here
    }
   // ----------------------------------------------------------------------------
   #else // windows <=================================
   // ----------------------------------------------------------------------------
    // for /f "tokens=*" %i in ('dir /ad /b /s lib*') do @dir /ad /b "%i\clang" 2>nul && (echo Directory: %i\clang )
    wxString cmd = "cmd /c for /f \"tokens=*\" %i in ('dir /ad /b /s lib*') do @dir /ad /b \"%i\\clang\" 2>nul && (echo Directory: %i\\clang)";
    wxExecute(cmd, versionPaths);

    // **Debugging**
    //for (size_t ii=0; ii<versionPaths.GetCount(); ++ii)
    //{
        //Result from cmd.exe can look semilar this;
        //    14.0.6
        //    Directory: f:\usr\Programs\msys64_14.0.6\clang64\lib\clang
        //    14.0.6
        //    Directory: f:\usr\Programs\msys64_14.0.6\mingw64\lib\clang
        //    14
        //    14.0.6
        //    17
        //    17.0.1
        //    Directory: f:\usr\Programs\msys64_14.0.6\mingw64\lib64\clang

        //CCLogger::Get(->DebugLog(versionPaths[ii]);  // **Debugging**
    //}

    wxSetWorkingDirectory(priorDir);
    wxString pathText;
    for (size_t ii=versionPaths.GetCount(); ii-->0;)
    {
        // restructure the result in the array so they look like <version>|fullpath
        if (versionPaths[ii].StartsWith("Directory:"))
        {
            pathText = versionPaths[ii].AfterFirst(' '); //Remove "Directory: "
            if (pathText.EndsWith("\\clang"))
                pathText.Truncate(pathText.Length()-6); //clang is appended later
        }
        else
        {
            versionPaths[ii].Append("|" + pathText); //append the path to the version string
        }
    }

    // Remove the "Directory: " entries, They're no longer needed.
    for (size_t ii=versionPaths.GetCount(); ii-->0;)
    {
        if (versionPaths[ii].StartsWith("Directory: "))
            versionPaths.RemoveAt(ii);
    }

   #endif // not __WXMSW__
}
// ----------------------------------------------------------------------------
wxString ClangLocator::Locate_ClangdDir()
// ----------------------------------------------------------------------------
{
    wxFileName fnClangdPath;
    LogManager* pLogMgr = Manager::Get()->GetLogManager();

    // Set filename to find
    fnClangdPath.SetFullName(clangdexe);

    // See if executable dir contains ...lsp/clangd.*
    wxString execDir  = Manager::Get()->GetConfigManager("app")->GetExecutableFolder();
    fnClangdPath.SetPath(execDir + fileSep + "lsp");
    if (fnClangdPath.FileExists())
    {
        CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), execDir + fileSep + "lsp"));
    }

    if (not fnClangdPath.FileExists())
    {
        // Check Project default compiler path to search if a project is loaded
        cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
        if (pProject)
        {
            // First check project global compiler is valid
            int compilerIdx = CompilerFactory::GetCompilerIndex(pProject->GetCompilerID());
            if (compilerIdx != -1)
            {
                Compiler* prjCompiler = CompilerFactory::GetCompiler(compilerIdx);
                if (prjCompiler)
                {
                    wxString mPath = prjCompiler->GetMasterPath();
                    if (!mPath.empty() && wxDirExists(mPath))
                    {
                        fnClangdPath.SetPath(mPath);
                        if (fnClangdPath.FileExists())
                        {
                            CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
                        }
                        else
                        {
                            fnClangdPath.SetPath(mPath + fileSep + "bin");
                            if (fnClangdPath.FileExists())
                            {
                                CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
                            }
                        }
                    }
                }
            }
        }
    }

    if (not fnClangdPath.FileExists())
    {
        // Check default compiler path to search
        Compiler* defaultCompiler = CompilerFactory::GetDefaultCompiler();
        if (defaultCompiler)
        {
            wxString mPath = defaultCompiler->GetMasterPath();
            if (!mPath.empty() && wxDirExists(mPath))
            {
                fnClangdPath.SetPath(mPath);
                if (fnClangdPath.FileExists())
                {
                    CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
                }
                else
                {
                    fnClangdPath.SetPath(mPath + fileSep + "bin");
                    if (fnClangdPath.FileExists())
                    {
                        CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
                    }
                }
            }
        }
    }


    // Try to find clangd from the environment path
    if (not fnClangdPath.FileExists())
    {
        wxArrayString clangLocations;
        wxLogNull nolog; // turn off 'not found' messages
        wxArrayString envPaths = GetEnvPaths();
        for (wxString path : envPaths)
        {
            clangLocations.Clear();
            size_t cnt = ScanForFiles(path, clangLocations, clangdexe);
            for (size_t ii=0; ii<cnt; ++ii)
            {
                fnClangdPath.Assign(clangLocations[ii]);
                if (fnClangdPath.FileExists())
                {
                    CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
                    break;
                }
            }
            if (fnClangdPath.FileExists())
                break;
        }
    }

    #if defined(__WXMSW__)
    if (not fnClangdPath.FileExists())
    {
        // Check Windows LLVM registry entries
        wxString llvmInstallPath, llvmVersion;
        wxArrayString regKeys;
        regKeys.Add("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LLVM");
        regKeys.Add("Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LLVM");
        for(size_t i = 0; i < regKeys.size(); ++i)
        {
            if(ReadMSWInstallLocation(regKeys.Item(i), llvmInstallPath, llvmVersion))
            {
                if (!llvmInstallPath.empty() && wxDirExists(llvmInstallPath))
                {
                    fnClangdPath.SetPath(llvmInstallPath);
                    if (fnClangdPath.FileExists())
                    {
                        CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
                        break;
                    }
                }
            }
        }
    }
    #endif

    if (not fnClangdPath.FileExists())
    {
        fnClangdPath.SetPath(LLVM_Dirmaybe);
        if (fnClangdPath.FileExists())
        {
            CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
        }
        #ifdef __WXMAC__
        else
        {
            fnClangdPath.SetPath(LLVM_DirmaybeMAC);
            if (fnClangdPath.FileExists())
            {
                CCLogger::Get()->DebugLog(wxString::Format(_("Locate_ClangdDir detected clangd in : %s"), fnClangdPath.GetPath()));
            }

        }
        #endif

    }

    if (fnClangdPath.FileExists())
    {
        // clangd must be version 13 or newer
        // Version 12 crashes when changing lines at bottom of .h files
        wxString versionNative;
        wxString version = GetClangdVersion(fnClangdPath.GetFullPath(), versionNative);
        // eg., clangd version 10.0,0
        version = version.BeforeFirst('.').AfterLast(' ');
        if (version.IsEmpty())
        {
            wxString msg = _("clangd version could not be determined from string '" + versionNative + "'");
            cbMessageBox(msg, _("Error"));
            pLogMgr->LogError(msg);
            return wxString();
        }

        int versionNum = std::stoi(version.ToStdString());
        if (versionNum < 13)
        {
            wxString msg = _("clangd version must be 13 or newer.");
            cbMessageBox(msg, _("Error"));
            pLogMgr->LogError(msg);
            return wxString();
        }
    }

    if (not fnClangdPath.Exists())
        fnClangdPath.Clear();

    return fnClangdPath.GetPath();
}
// ----------------------------------------------------------------------------
wxArrayString ClangLocator::GetEnvPaths() const
// ----------------------------------------------------------------------------
{
    wxString path;
    if(!::wxGetEnv("PATH", &path))
    {
        //-clWARNING() << "Could not read environment variable PATH";
        wxString msg; msg << "GetEnvPaths() Could not read environment variable PATH";
        CCLogger::Get()->DebugLog(msg);
        return {};
    }

    wxArrayString mergedPaths;
    wxArrayString paths = ::wxStringTokenize(path, ENV_PATH_SEPARATOR, wxTOKEN_STRTOK);
    return paths;
}
// ----------------------------------------------------------------------------
size_t ClangLocator::ScanForFiles(wxString path, wxArrayString& foundFiles, wxString mask)
// ----------------------------------------------------------------------------
{
    #if defined(__WXGTK__)
        // Windows sublayer for unix places the entire windows path into the Linux $PATH environment as mount points
        // like:
        //    /mnt/c/Program Files/WindowsApps/Microsoft.WindowsTerminal_1.11.2921.0_x64__8wekyb3d8bbwe:
        //    /mnt/f/User/Programs/VMWare/bin/:
        //    /mnt/c/usr/bin:
        //    /mnt/c/Program Files (x86)/Intel/iCLS Client/:
        //    /mnt/c/Program Files/Intel/iCLS Client/:
        //    /mnt/c/WINDOWS/system32:
        //        ,,, nmany, many more ...
        //    /mnt/c/Users/Pecan/AppData/Local/Microsoft/WindowsApps:
        //    /mnt/f/user/Programs/LLVM/bin:
        //    /mnt/c/usr/bin:/snap/bin

    // Eliminate WSL windows mount points, else the search takes forever..
    if (path.Matches("/mnt/?/*")) return 0; //eliminate massive number of wsl windows paths
    #endif

    if (not wxDirExists(path)) return 0;

    wxString filename = wxFindFirstFile(path + wxFILE_SEP_PATH + mask, wxFILE);
    while(filename.Length())
    {
        foundFiles.Add(filename);
        filename = wxFindNextFile();
        if (filename.empty())
            break;
    }

    return foundFiles.GetCount();
}
// ----------------------------------------------------------------------------
bool ClangLocator::ReadMSWInstallLocation(const wxString& regkey, wxString& installPath, wxString& llvmVersion)
// ----------------------------------------------------------------------------
{
#ifdef __WXMSW__
    wxRegKey reg(wxRegKey::HKLM, regkey);
    installPath.Clear();
    llvmVersion.Clear();
    if (reg.Exists())
    {
        reg.QueryValue("DisplayIcon", installPath);
        reg.QueryValue("DisplayVersion", llvmVersion);
    }
    return !installPath.IsEmpty() && !llvmVersion.IsEmpty();
#else
    return false;
#endif
}
// ----------------------------------------------------------------------------
wxString ClangLocator::GetClangdVersion(const wxString& clangBinary, wxString& versionNative)
// ----------------------------------------------------------------------------
{
    wxString command;
    wxArrayString stdoutArr;
    command << clangBinary << " --version";
    ProcUtils::SafeExecuteCommand(command, stdoutArr);
    if (not stdoutArr.IsEmpty())
    {
        versionNative = stdoutArr.Item(0);
        wxString versionString(versionNative);
        if (wxFound(versionString.Find("(")) )
        {
            //versionString = versionString.AfterLast('(');
            //versionString = versionString.BeforeLast(')');
            versionString = versionString.BeforeFirst('(');
        }
        return versionString;
    }
    return wxString();
}
// ----------------------------------------------------------------------------
wxString ClangLocator::GetClangdVersionID(const wxString& clangdBinary)
// ----------------------------------------------------------------------------
{
    wxFileName fnClangdPath(clangdBinary);
    if (not fnClangdPath.GetFullPath().empty())
    {
        // Get the version of this clangd, we need to match it with the same resources dir
        wxString clangdVersion;
        wxString cmdLine = fnClangdPath.GetFullPath();
        // Use clangd to get the clang version
        cmdLine.Append (" --version");
        wxArrayString clangdResponse;
        wxExecute(cmdLine, clangdResponse);
        if (clangdResponse.Count()) //tease out the version number
        {
            cmdLine = clangdResponse[0]; //usually "clangd version 13.0.1" followed by a space or -+whatever
            size_t sBgn = cmdLine.find("version ");
            if (sBgn) sBgn += 8; //jump over "verson"
            size_t sEnd = sBgn;
            for( ; sEnd < cmdLine.length(); ++sEnd)
            {
                if ( ((cmdLine[sEnd] >= '0') and (cmdLine[sEnd] <= '9')) or (cmdLine[sEnd] == '.') )
                    continue;
                break;
            }
            if (sBgn and sEnd)
                clangdVersion = cmdLine.SubString(sBgn, sEnd-1);
        }
        if (not clangdVersion.empty())
            return clangdVersion;
    }

    return wxString();
}
