#include "sdk.h"

#ifndef CB_PRECOMP
    #include <wx/arrstr.h>
#endif // CB_PRECOMP

#include <wx/filefn.h>
#include <wx/textfile.h>
#include <wx/regex.h>
#include <wx/xml/xml.h>
#ifdef __WXMSW__ // for wxRegKey
    #include <wx/msw/registry.h>
#endif // __WXMSW__

#include "compilerXML.h"

#include "manager.h"
#include "macrosmanager.h"

CompilerXML::CompilerXML(const wxString& name, const wxString& ID, const wxString& file)
    : Compiler(wxGetTranslation(name), ID), m_fileName(file)
{
    wxXmlDocument compiler;
    compiler.Load(m_fileName);
    m_Weight = wxAtoi(compiler.GetRoot()->GetAttribute("weight", "100"));
    m_MultiLineMessages = "0" != compiler.GetRoot()->GetAttribute("multilinemessages", "0");
    Reset();
}

CompilerXML::~CompilerXML()
{
}

Compiler* CompilerXML::CreateCopy()
{
    return (new CompilerXML(*this));
}

AutoDetectResult CompilerXML::AutoDetectInstallationDir()
{
    SearchMode sm = none;

    wxString path;
    wxGetEnv("PATH", &path);
    wxString origPath = path;

    if (!m_MasterPath.IsEmpty())
    {
        path += wxPATH_SEP + m_MasterPath;
        wxSetEnv("PATH", path);
        m_MasterPath.Clear();
    }

    wxXmlDocument compiler;
    if ( compiler.Load(m_fileName) )
    {
        int depth = 0;
        wxXmlNode* node = compiler.GetRoot()->GetChildren();
        while (node)
        {
            // *****  Nested IFs
            if (node->GetName() == "if" && node->GetChildren())
            {
                if (EvalXMLCondition(node))
                {
                    node = node->GetChildren();
                    ++depth;
                    continue;
                }
                // *****  Nested ELSE  IFs
                else if (node->GetNext() && node->GetNext()->GetName() == "else" &&
                         node->GetNext()->GetChildren())
                {
                    node = node->GetNext()->GetChildren();
                    ++depth;
                    continue;
                }
            }
            // **** Compiler path
            else if (node->GetName() == "Path" && node->GetChildren())
            {
                wxString value = node->GetAttribute("type", wxEmptyString);
                if (value == "master")
                    sm = master;
                else if (value == "extra")
                    sm = extra;
                else if (value == "include")
                    sm = include;
                else if (value == "resource")
                    sm = resource;
                else if (value == "lib")
                    sm = lib;
                if (sm != master || m_MasterPath.IsEmpty())
                {
                    node = node->GetChildren();
                    ++depth;
                    continue;
                }
                else
                    sm = none;
            }
            // **** Search path
            else if (node->GetName() == "Search" && sm != none)
            {
                wxString value;
                // **** Environment variables
                if (node->GetAttribute("envVar", &value))
                {
                    wxString pathValues;
                    wxGetEnv(value, &pathValues);
                    if (!pathValues.IsEmpty())
                    {
                        wxArrayString pathArray = GetArrayFromString(pathValues, wxPATH_SEP);
                        wxString targ = GetExecName(node->GetAttribute("for", wxEmptyString));
                        for (size_t i = 0; i < pathArray.GetCount(); ++i)
                        {
                            if ((targ.IsEmpty() && wxDirExists(pathArray[i])) || wxFileExists(pathArray[i] + wxFILE_SEP_PATH + targ))
                            {
                                if (AddPath(pathArray[i], sm, wxAtoi(compiler.GetRoot()->GetAttribute("rmDirs", "0"))))
                                    break;
                            }
                            else if (sm == master && (   (targ.IsEmpty() && wxDirExists(value + wxFILE_SEP_PATH + "bin"))
                                                      || wxFileExists(pathArray[i] + wxFILE_SEP_PATH + "bin" + wxFILE_SEP_PATH + targ)) )
                            {
                                if (AddPath(pathArray[i] + wxFILE_SEP_PATH + "bin", sm))
                                    break;
                            }
                        }
                    }
                }
                // **** Code::Blocks macros
                if (node->GetAttribute("macro", &value))
                {
                    wxString value_without_macros(value);
                    Manager::Get()->GetMacrosManager()->ReplaceMacros(value_without_macros);
                    if (!value_without_macros.IsEmpty())
                    {
                        wxArrayString pathArray = GetArrayFromString(value_without_macros, wxPATH_SEP);
                        wxString targ = GetExecName(node->GetAttribute("for", wxEmptyString));
                        for (size_t i = 0; i < pathArray.GetCount(); ++i)
                        {
                            if ((targ.IsEmpty() && wxDirExists(pathArray[i])) || wxFileExists(pathArray[i] + wxFILE_SEP_PATH + targ))
                            {
                                if (AddPath(pathArray[i], sm, wxAtoi(compiler.GetRoot()->GetAttribute("rmDirs", "0"))))
                                    break;
                            }
                            else if (sm == master && (   (targ.IsEmpty() && wxDirExists(value + wxFILE_SEP_PATH + "bin"))
                                                      || wxFileExists(pathArray[i] + wxFILE_SEP_PATH + "bin" + wxFILE_SEP_PATH + targ)) )
                            {
                                if (AddPath(pathArray[i] + wxFILE_SEP_PATH + "bin", sm))
                                    break;
                            }
                        }
                    }
                }
                // **** Path
                else if (node->GetAttribute("path", &value))
                {
                    wxString targ = GetExecName(node->GetAttribute("for", wxEmptyString));
                    if (wxIsWild(value))
                    {
                        path = wxFindFirstFile(value, wxDIR);
                        if (!path.IsEmpty() &&
                             ((targ.IsEmpty() && wxDirExists(path)) ||
                              wxFileExists(path + wxFILE_SEP_PATH + targ) ||
                              wxFileExists(path + wxFILE_SEP_PATH + "bin" + wxFILE_SEP_PATH + targ)))
                        {
                            AddPath(path, sm);
                        }
                    }
                    else if ((targ.IsEmpty() && wxDirExists(value)) || wxFileExists(value + wxFILE_SEP_PATH + targ))
                        AddPath(value, sm);
                    else if (sm == master && (  (targ.IsEmpty() && wxDirExists(value + wxFILE_SEP_PATH + "bin"))
                                              || wxFileExists(value + wxFILE_SEP_PATH + "bin" + wxFILE_SEP_PATH + targ)))
                        AddPath(value + wxFILE_SEP_PATH + "bin", sm);
                }
                // **** Files
                else if (node->GetAttribute("file", &value))
                {
                    wxString regexp = node->GetAttribute("regex", wxEmptyString);
                    int idx = wxAtoi(node->GetAttribute("index", "0"));
                    wxRegEx re;
                    if (wxFileExists(value) && re.Compile(regexp))
                    {
                        wxTextFile file(value);
                        for (size_t i = 0; i < file.GetLineCount(); ++i)
                        {
                            if (re.Matches(file.GetLine(i)))
                            {
                                AddPath(re.GetMatch(file.GetLine(i), idx), sm);
                                if (sm == master && !m_MasterPath.IsEmpty())
                                    break;
                            }
                        }
                    }
                }
#ifdef __WXMSW__ // for wxRegKey
                // **** Registry on MSW
                else if (node->GetAttribute("registry", &value))
                {
                    wxRegKey key;
                    wxString dir;
                    key.SetName(value);
                    if (key.Exists() && key.Open(wxRegKey::Read))
                    {
                        key.QueryValue(node->GetAttribute("value", wxEmptyString), dir);
                        if (!dir.IsEmpty() && wxDirExists(dir))
                            AddPath(dir, sm);
                        key.Close();
                    }
                }
#endif // __WXMSW__
            }
            // **** Additional information
            else if (node->GetName() == "Add")
            {
                wxString value;
                if (node->GetAttribute("cFlag", &value))
                    AddCompilerOption(value);
                else if (node->GetAttribute("lFlag", &value))
                    AddLinkerOption(value);
                else if (node->GetAttribute("lib", &value))
                    AddLinkLib(value);
                else if (sm != none)
                {
                    path.Clear();
                    wxXmlNode* child = node->GetChildren();
                    while (child)
                    {
                        if (child->GetType() == wxXML_TEXT_NODE || child->GetType() == wxXML_CDATA_SECTION_NODE)
                            path << child->GetContent();
                        else if (child->GetName() == "master")
                            path << m_MasterPath;
                        else if (child->GetName() == "separator")
                            path << wxFILE_SEP_PATH;
                        else if (child->GetName() == "envVar")
                        {
                            value = child->GetAttribute("default", wxEmptyString);
                            wxGetEnv(child->GetAttribute("value", wxEmptyString), &value);
                            path << value;
                        }
                        child = child->GetNext();
                    }
                    AddPath(path.Trim().Trim(false), sm);
                }
            }
            else if (node->GetName() == "Fallback" && sm != none)
            {
                wxString value = node->GetAttribute("path", wxEmptyString);
                switch (sm)
                {
                case master:
                    if (m_MasterPath.IsEmpty())
                        AddPath(value, sm);
                    break;
                case extra:
                    if (m_ExtraPaths.IsEmpty())
                        AddPath(value, sm);
                    break;
                case include:
                    if (m_IncludeDirs.IsEmpty())
                        AddPath(value, sm);
                    break;
                case resource:
                    if (m_ResIncludeDirs.IsEmpty())
                        AddPath(value, sm);
                    break;
                case lib:
                    if (m_LibDirs.IsEmpty())
                        AddPath(value, sm);
                    break;
                case none: // fall-through
                default:
                    break;
                }
            }

            while ( (!node->GetNext() || (sm == master && !m_MasterPath.IsEmpty())) &&
                    depth > 0 )
            {
                node = node->GetParent();
                if (node->GetName() == "Path")
                {
                    sm = none;
                }
                --depth;
            }
            node = node->GetNext();
        }
    }

    wxSetEnv("PATH", origPath);

    wxString master_path_without_macros(m_MasterPath);
    Manager::Get()->GetMacrosManager()->ReplaceMacros(master_path_without_macros);

    if (   wxFileExists(master_path_without_macros + wxFILE_SEP_PATH + "bin" + wxFILE_SEP_PATH + m_Programs.C)
        || wxFileExists(master_path_without_macros + wxFILE_SEP_PATH + m_Programs.C)
        || (GetID() == "null") ) // Special case so "No Compiler" is valid
    {
        return adrDetected;
    }

    for (size_t i = 0; i < m_ExtraPaths.GetCount(); ++i)
    {
        if (wxFileExists(m_ExtraPaths[i] + wxFILE_SEP_PATH + m_Programs.C))
            return adrDetected;
    }
    return adrGuessed;
}

bool CompilerXML::AddPath(const wxString& pth, SearchMode sm, int rmDirs)
{
    wxFileName fn(pth + wxFILE_SEP_PATH);
    fn.Normalize(wxPATH_NORM_ENV_VARS|wxPATH_NORM_DOTS);
    for (int i = rmDirs; i > 0; --i)
        fn.RemoveLastDir();
    wxString path = fn.GetPath();
    switch (sm)
    {
    case master:
        if (path.AfterLast(wxFILE_SEP_PATH) == "bin")
            m_MasterPath = path.BeforeLast(wxFILE_SEP_PATH);
        else
            m_MasterPath = path;
        return true;
    case extra:
        if (m_ExtraPaths.Index(path, !platform::windows) == wxNOT_FOUND)
            m_ExtraPaths.Add(path);
        break;
    case include:
        AddIncludeDir(path);
        break;
    case resource:
        AddResourceIncludeDir(path);
        break;
    case lib:
        AddLibDir(path);
        break;
    case none: // fall-through
    default:
        break;
    }
    return false;
}
