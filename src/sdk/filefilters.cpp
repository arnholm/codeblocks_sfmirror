/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"
#include <map>
#include <wx/arrstr.h>
#include "filefilters.h"
#include "globals.h"

typedef std::map<wxString, wxString> FileFiltersMap;
static FileFiltersMap s_Filters;

static size_t s_LastFilterAllIndex = 0;

// Let's add some default extensions.
// The rest will be added by editor lexers ;)
void FileFilters::AddDefaultFileFilters()
{
    if (s_Filters.size() != 0)
        return;

    Add(_("Code::Blocks workspace files"),         "*.workspace");
    Add(_("Code::Blocks project files"),           "*.cbp");
    Add(_("Code::Blocks project/workspace files"), "*.workspace;*.cbp");
    Add(_("Bloodshed Dev-C++ project files"),      "*.dev");
    Add(_("MS Visual C++ 6.0 project files"),      "*.dsp");
    Add(_("MS Visual Studio 7.0+ project files"),  "*.vcproj");
    Add(_("MS Visual C++ 6.0 workspace files"),    "*.dsw");
    Add(_("MS Visual Studio 7.0+ solution files"), "*.sln");
    Add(_("Apple Xcode 1.x project bundles"),      "*.xcode");
    Add(_("Apple Xcode 2.x project bundles"),      "*.xcodeproj");
    Add(_("MS Visual Studio 10.0+ project files"), "*.vcxproj");
}

bool FileFilters::Add(const wxString& name, const wxString& mask)
{
    if (name.IsEmpty() || mask.IsEmpty())
        return false; // both must be valid

    if (mask.Index(_T(',')) != wxString::npos)
    {
        // replace commas with semicolons
        wxString tmp = mask;
        while (tmp.Replace(",", ";"))
            ;
        s_Filters[name] = tmp;
    }
    else
        s_Filters[name] = mask;

    return true;
}

wxString FileFilters::GetFilterString(const wxString& ext)
{
    size_t count = 0;
    wxString ret;
    for (FileFiltersMap::iterator it = s_Filters.begin(); it != s_Filters.end(); ++it)
    {
        if (!ext.IsEmpty())
        {
            // filter based on parameter
            bool match = false;
            wxArrayString array = GetArrayFromString(it->second, ";", true);
            for (size_t i = 0; i < array.GetCount(); ++i)
            {
                if (ext.Matches(array[i]))
                {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue; // filtered
        }
        ++count;
        if (!ret.IsEmpty())
            ret << _T('|');
        ret << it->first << _T('|') << it->second;
    }

    // last filter is always "All"
    if (!ret.IsEmpty())
        ret << _T('|');
    ret << GetFilterAll();

    s_LastFilterAllIndex = count;

    return ret;
}

wxString FileFilters::GetFilterAll()
{
    s_LastFilterAllIndex = 0;
    if (platform::windows)
        return _("All files (*.*)|*.*");

    return _("All files (*)|*");
}

size_t FileFilters::GetIndexForFilterAll()
{
    return s_LastFilterAllIndex;
}

bool FileFilters::GetFilterIndexFromName(const wxString& FiltersList, const wxString& FilterName, int& Index)
{
    bool bFound = false;
    // the List will contain 2 entries per type (description, mask)
    wxArrayString List = GetArrayFromString(FiltersList, "|", true);
    int LoopEnd = static_cast<int>(List.GetCount());
    for(int idxList = 0; idxList < LoopEnd; idxList+=2)
    {
        if (List[idxList] == FilterName)
        {
            Index = idxList/2;
            bFound = true;
            break;
        }
    } // end for : idx : idxList
    return bFound;
} // end of GetFilterIndexFromName

bool FileFilters::GetFilterNameFromIndex(const wxString& FiltersList, int Index, wxString& FilterName)
{    // we return the name (not the mask)
    bool bFound = false;
    // the List will contain 2 entries per type (description, mask)
    wxArrayString List = GetArrayFromString(FiltersList, "|", true);
    int LoopEnd = static_cast<int>(List.GetCount());
    if (2*Index < LoopEnd)
    {
        FilterName = List[2*Index];
        bFound = true;
    }
    return bFound;
} // end of GetFilterStringFromIndex

// define some constants used throughout C::B

const wxString FileFilters::WORKSPACE_EXT           = "workspace";
const wxString FileFilters::CODEBLOCKS_EXT          = "cbp";
const wxString FileFilters::DEVCPP_EXT              = "dev";
const wxString FileFilters::MSVC6_EXT               = "dsp";
const wxString FileFilters::MSVC7_EXT               = "vcproj";
const wxString FileFilters::MSVC10_EXT              = "vcxproj";
const wxString FileFilters::MSVC6_WORKSPACE_EXT     = "dsw";
const wxString FileFilters::MSVC7_WORKSPACE_EXT     = "sln";
const wxString FileFilters::XCODE1_EXT              = "xcode";
const wxString FileFilters::XCODE2_EXT              = "xcodeproj";
const wxString FileFilters::ASM_EXT                 = "asm";
const wxString FileFilters::D_EXT                   = "d";
const wxString FileFilters::F_EXT                   = "f";
const wxString FileFilters::F77_EXT                 = "f77";
const wxString FileFilters::F90_EXT                 = "f90";
const wxString FileFilters::F95_EXT                 = "f95";
const wxString FileFilters::FOR_EXT                 = "for";
const wxString FileFilters::FPP_EXT                 = "fpp";
const wxString FileFilters::F03_EXT                 = "f03";
const wxString FileFilters::F08_EXT                 = "f08";
const wxString FileFilters::JAVA_EXT                = "java";
const wxString FileFilters::C_EXT                   = "c";
const wxString FileFilters::CC_EXT                  = "cc";
const wxString FileFilters::CPP_EXT                 = "cpp";
const wxString FileFilters::TPP_EXT                 = "tpp";
const wxString FileFilters::TCC_EXT                 = "tcc";
const wxString FileFilters::CXX_EXT                 = "cxx";
const wxString FileFilters::CPLPL_EXT               = "c++";
const wxString FileFilters::INL_EXT                 = "inl";
const wxString FileFilters::H_EXT                   = "h";
const wxString FileFilters::HH_EXT                  = "hh";
const wxString FileFilters::HPP_EXT                 = "hpp";
const wxString FileFilters::HXX_EXT                 = "hxx";
const wxString FileFilters::HPLPL_EXT               = "h++";
const wxString FileFilters::S_EXT                   = "s";
const wxString FileFilters::SS_EXT                  = "ss";
const wxString FileFilters::S62_EXT                 = "s62";
const wxString FileFilters::OBJECT_EXT              = "o";
const wxString FileFilters::XRCRESOURCE_EXT         = "xrc";
const wxString FileFilters::STATICLIB_EXT           = "a";
const wxString FileFilters::RESOURCE_EXT            = "rc";
const wxString FileFilters::RESOURCEBIN_EXT         = "res";
const wxString FileFilters::XML_EXT                 = "xml";
const wxString FileFilters::SCRIPT_EXT              = "script";
#if   defined(__WXMSW__)
    const wxString FileFilters::DYNAMICLIB_EXT      = "dll";
    const wxString FileFilters::EXECUTABLE_EXT      = "exe";
    const wxString FileFilters::NATIVE_EXT          = "sys";
#elif defined(__WXMAC__)
    const wxString FileFilters::DYNAMICLIB_EXT      = "dylib";
    const wxString FileFilters::EXECUTABLE_EXT      = "";
    const wxString FileFilters::NATIVE_EXT          = "";
#else
    const wxString FileFilters::DYNAMICLIB_EXT      = "so";
    const wxString FileFilters::EXECUTABLE_EXT      = "";
    const wxString FileFilters::NATIVE_EXT          = "";
#endif

// dot.ext version
const wxString FileFilters::WORKSPACE_DOT_EXT       = _T('.') + FileFilters::WORKSPACE_EXT;
const wxString FileFilters::CODEBLOCKS_DOT_EXT      = _T('.') + FileFilters::CODEBLOCKS_EXT;
const wxString FileFilters::DEVCPP_DOT_EXT          = _T('.') + FileFilters::DEVCPP_EXT;
const wxString FileFilters::MSVC6_DOT_EXT           = _T('.') + FileFilters::MSVC6_EXT;
const wxString FileFilters::MSVC7_DOT_EXT           = _T('.') + FileFilters::MSVC7_EXT;
const wxString FileFilters::MSVC10_DOT_EXT          = _T('.') + FileFilters::MSVC10_EXT;
const wxString FileFilters::MSVC6_WORKSPACE_DOT_EXT = _T('.') + FileFilters::MSVC6_WORKSPACE_EXT;
const wxString FileFilters::MSVC7_WORKSPACE_DOT_EXT = _T('.') + FileFilters::MSVC7_WORKSPACE_EXT;
const wxString FileFilters::XCODE1_DOT_EXT          = _T('.') + FileFilters::XCODE1_EXT;
const wxString FileFilters::XCODE2_DOT_EXT          = _T('.') + FileFilters::XCODE2_EXT;
const wxString FileFilters::ASM_DOT_EXT             = _T('.') + FileFilters::ASM_EXT;
const wxString FileFilters::D_DOT_EXT               = _T('.') + FileFilters::D_EXT;
const wxString FileFilters::F_DOT_EXT               = _T('.') + FileFilters::F_EXT;
const wxString FileFilters::F77_DOT_EXT             = _T('.') + FileFilters::F77_EXT;
const wxString FileFilters::F90_DOT_EXT             = _T('.') + FileFilters::F90_EXT;
const wxString FileFilters::F95_DOT_EXT             = _T('.') + FileFilters::F95_EXT;
const wxString FileFilters::FOR_DOT_EXT             = _T('.') + FileFilters::FOR_EXT;
const wxString FileFilters::FPP_DOT_EXT             = _T('.') + FileFilters::FPP_EXT;
const wxString FileFilters::F03_DOT_EXT             = _T('.') + FileFilters::F03_EXT;
const wxString FileFilters::F08_DOT_EXT             = _T('.') + FileFilters::F08_EXT;
const wxString FileFilters::JAVA_DOT_EXT            = _T('.') + FileFilters::JAVA_EXT;
const wxString FileFilters::C_DOT_EXT               = _T('.') + FileFilters::C_EXT;
const wxString FileFilters::CC_DOT_EXT              = _T('.') + FileFilters::CC_EXT;
const wxString FileFilters::CPP_DOT_EXT             = _T('.') + FileFilters::CPP_EXT;
const wxString FileFilters::TPP_DOT_EXT             = _T('.') + FileFilters::TPP_EXT;
const wxString FileFilters::TCC_DOT_EXT             = _T('.') + FileFilters::TCC_EXT;
const wxString FileFilters::CXX_DOT_EXT             = _T('.') + FileFilters::CXX_EXT;
const wxString FileFilters::CPLPL_DOT_EXT           = _T('.') + FileFilters::CPLPL_EXT;
const wxString FileFilters::INL_DOT_EXT             = _T('.') + FileFilters::INL_EXT;
const wxString FileFilters::H_DOT_EXT               = _T('.') + FileFilters::H_EXT;
const wxString FileFilters::HH_DOT_EXT              = _T('.') + FileFilters::HH_EXT;
const wxString FileFilters::HPP_DOT_EXT             = _T('.') + FileFilters::HPP_EXT;
const wxString FileFilters::HXX_DOT_EXT             = _T('.') + FileFilters::HXX_EXT;
const wxString FileFilters::HPLPL_DOT_EXT           = _T('.') + FileFilters::HPLPL_EXT;
const wxString FileFilters::S_DOT_EXT               = _T('.') + FileFilters::S_EXT;
const wxString FileFilters::SS_DOT_EXT              = _T('.') + FileFilters::SS_EXT;
const wxString FileFilters::S62_DOT_EXT             = _T('.') + FileFilters::S62_EXT;
const wxString FileFilters::OBJECT_DOT_EXT          = _T('.') + FileFilters::OBJECT_EXT;
const wxString FileFilters::XRCRESOURCE_DOT_EXT     = _T('.') + FileFilters::XRCRESOURCE_EXT;
const wxString FileFilters::STATICLIB_DOT_EXT       = _T('.') + FileFilters::STATICLIB_EXT;
const wxString FileFilters::RESOURCE_DOT_EXT        = _T('.') + FileFilters::RESOURCE_EXT;
const wxString FileFilters::RESOURCEBIN_DOT_EXT     = _T('.') + FileFilters::RESOURCEBIN_EXT;
const wxString FileFilters::XML_DOT_EXT             = _T('.') + FileFilters::XML_EXT;
const wxString FileFilters::SCRIPT_DOT_EXT          = _T('.') + FileFilters::SCRIPT_EXT;
#ifdef __WXMSW__
    const wxString FileFilters::DYNAMICLIB_DOT_EXT  = _T('.') + FileFilters::DYNAMICLIB_EXT;
    const wxString FileFilters::EXECUTABLE_DOT_EXT  = _T('.') + FileFilters::EXECUTABLE_EXT;
    const wxString FileFilters::NATIVE_DOT_EXT      = _T('.') + FileFilters::NATIVE_EXT;
#else
    const wxString FileFilters::DYNAMICLIB_DOT_EXT  = _T('.') + FileFilters::DYNAMICLIB_EXT;
    const wxString FileFilters::EXECUTABLE_DOT_EXT  = EXECUTABLE_EXT; // no dot, since no extension
    const wxString FileFilters::NATIVE_DOT_EXT      = NATIVE_EXT; // no dot, since no extension
#endif
