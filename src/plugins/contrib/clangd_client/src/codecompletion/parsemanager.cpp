/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 87 $
 * $Id: parsemanager.cpp 87 2022-10-31 17:58:56Z pecanh $
 * $HeadURL: https://svn.code.sf.net/p/cb-clangd-client/code/trunk/clangd_client/src/codecompletion/parsemanager.cpp $
 */

#include <sdk.h>
#include <time.h> //(ph 2021/04/16)

#ifndef CB_PRECOMP
    #include <cctype>

    #include <wx/dir.h>
    #include <wx/log.h> // for wxSafeShowMessage()
    #include <wx/regex.h>
    #include <wx/wfstream.h>
    #include <wx/window.h>  //(ph 2021/04/15)

    #include <cbauibook.h>
    #include <cbeditor.h>
    #include <cbexception.h>
    #include <cbproject.h>
    #include <compilerfactory.h>
    #include <configmanager.h>
    #include <editormanager.h>
    #include <logmanager.h>
    #include <macrosmanager.h>
    #include <manager.h>
    #include <pluginmanager.h>
    #include <prep.h> // nullptr
    #include <projectmanager.h>

    #include <tinyxml/tinyxml.h>
#endif

#include <wx/tokenzr.h>
#include <wx/filefn.h>      //(ph 2021/04/16)
#include <wx/datetime.h>    //(ph 2021/04/17)
#include <wx/fs_zip.h>      // to unzip proxy project .cbp from resouce file
#include <wx/zipstrm.h>
#include <wx/wfstream.h>

#include <cbstyledtextctrl.h>
#include <compilercommandgenerator.h>
#include <cbworkspace.h>

#include "parsemanager.h"
#include "classbrowser.h"
#include "parser/parser.h"
#include "parser/profiletimer.h"
#include "IdleCallbackHandler.h"    //(ph 2021/09/25)//(ph 2022/02/14)


#define CC_ParseManager_DEBUG_OUTPUT 0
//#define CC_ParseManager_DEBUG_OUTPUT 1      //(ph 2021/05/1)

#if defined (CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_ParseManager_DEBUG_OUTPUT
        #define CC_ParseManager_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_ParseManager_DEBUG_OUTPUT
        #define CC_ParseManager_DEBUG_OUTPUT 2
    #endif
#endif

#if CC_ParseManager_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
    #define TRACE2(format, args...)
#elif CC_ParseManager_DEBUG_OUTPUT == 2
    #define TRACE(format, args...)                                              \
        do                                                                      \
        {                                                                       \
            if (g_EnableDebugTrace)                                             \
                CCLogger::Get()->DebugLog(wxString::Format(format, ##args));   \
        }                                                                       \
        while (false)
    #define TRACE2(format, args...) \
        CCLogger::Get()->DebugLog(wxString::Format(format, ##args))
#else
    #define TRACE(format, args...)
    #define TRACE2(format, args...)
#endif

/*
 * (Recursive) functions that are surrounded by a critical section:
 * GenerateResultSet() -> AddChildrenOfUnnamed
 * GetCallTips() -> PrettyPrintToken (recursive function)
 * FindCurrentFunctionToken() -> ParseFunctionArguments, FindAIMatches (recursive function)
 * GenerateResultSet (recursive function):
 *     FindAIMatches(), ResolveActualType(), ResolveExpression(),
 *     FindCurrentFunctionToken(), ResolveOperator()
 * FindCurrentFunctionStart() -> GetTokenFromCurrentLine
 */

// ----------------------------------------------------------------------------
namespace ParseManagerHelper
// ----------------------------------------------------------------------------
{
    class ParserDirTraverser : public wxDirTraverser
    {
    public:
        ParserDirTraverser(const wxString& excludePath, wxArrayString& files) :
            m_ExcludeDir(excludePath),
            m_Files(files)
        {}

        wxDirTraverseResult OnFile(const wxString& filename) override
        {
            if (ParserCommon::FileType(filename) != ParserCommon::ftOther)
                m_Files.Add(filename);
            return wxDIR_CONTINUE;
        }

        wxDirTraverseResult OnDir(const wxString& dirname) override
        {
            if (dirname == m_ExcludeDir)
                return wxDIR_IGNORE;
            if (m_Files.GetCount() == 1)
                return wxDIR_STOP;
            m_Files.Clear();
            return wxDIR_CONTINUE;
        }

    private:
        const wxString& m_ExcludeDir;
        wxArrayString&  m_Files;
    };
}// namespace ParseManagerHelper

//// /** event id for the sequence project parsing timer */
//// int idTimerParsingOneByOne = wxNewId();

/** if this option is enabled, there will be many log messages when doing semantic match */
bool s_DebugSmartSense = false;

static void AddToImageList(wxImageList *list, const wxString &path)
{
    wxBitmap bmp = cbLoadBitmap(path, wxBITMAP_TYPE_PNG);
    if (!bmp.IsOk())
    {
        printf("failed to load: %s\n", path.utf8_str().data());
    }
    list->Add(bmp);
}

static wxImageList* LoadImageList(int size)
{
    wxImageList *list = new wxImageList(size, size);
    wxBitmap bmp;
    const wxString prefix = ConfigManager::GetDataFolder()
                          + wxString::Format(_T("/clangd_client.zip#zip:images/%dx%d/"), size,
                                             size);

    // Bitmaps must be added by order of PARSER_IMG_* consts.
    AddToImageList(list, prefix + _T("class_folder.png")); // PARSER_IMG_CLASS_FOLDER
    AddToImageList(list, prefix + _T("class.png")); // PARSER_IMG_CLASS
    AddToImageList(list, prefix + _T("class_private.png")); // PARSER_IMG_CLASS_PRIVATE
    AddToImageList(list, prefix + _T("class_protected.png")); // PARSER_IMG_CLASS_PROTECTED
    AddToImageList(list, prefix + _T("class_public.png")); // PARSER_IMG_CLASS_PUBLIC
    AddToImageList(list, prefix + _T("ctor_private.png")); // PARSER_IMG_CTOR_PRIVATE
    AddToImageList(list, prefix + _T("ctor_protected.png")); // PARSER_IMG_CTOR_PROTECTED
    AddToImageList(list, prefix + _T("ctor_public.png")); // PARSER_IMG_CTOR_PUBLIC
    AddToImageList(list, prefix + _T("dtor_private.png")); // PARSER_IMG_DTOR_PRIVATE
    AddToImageList(list, prefix + _T("dtor_protected.png")); // PARSER_IMG_DTOR_PROTECTED
    AddToImageList(list, prefix + _T("dtor_public.png")); // PARSER_IMG_DTOR_PUBLIC
    AddToImageList(list, prefix + _T("method_private.png")); // PARSER_IMG_FUNC_PRIVATE
    AddToImageList(list, prefix + _T("method_protected.png")); // PARSER_IMG_FUNC_PRIVATE
    AddToImageList(list, prefix + _T("method_public.png")); // PARSER_IMG_FUNC_PUBLIC
    AddToImageList(list, prefix + _T("var_private.png")); // PARSER_IMG_VAR_PRIVATE
    AddToImageList(list, prefix + _T("var_protected.png")); // PARSER_IMG_VAR_PROTECTED
    AddToImageList(list, prefix + _T("var_public.png")); // PARSER_IMG_VAR_PUBLIC
    AddToImageList(list, prefix + _T("macro_def.png")); // PARSER_IMG_MACRO_DEF
    AddToImageList(list, prefix + _T("enum.png")); // PARSER_IMG_ENUM
    AddToImageList(list, prefix + _T("enum_private.png")); // PARSER_IMG_ENUM_PRIVATE
    AddToImageList(list, prefix + _T("enum_protected.png")); // PARSER_IMG_ENUM_PROTECTED
    AddToImageList(list, prefix + _T("enum_public.png")); // PARSER_IMG_ENUM_PUBLIC
    AddToImageList(list, prefix + _T("enumerator.png")); // PARSER_IMG_ENUMERATOR
    AddToImageList(list, prefix + _T("namespace.png")); // PARSER_IMG_NAMESPACE
    AddToImageList(list, prefix + _T("typedef.png")); // PARSER_IMG_TYPEDEF
    AddToImageList(list, prefix + _T("typedef_private.png")); // PARSER_IMG_TYPEDEF_PRIVATE
    AddToImageList(list, prefix + _T("typedef_protected.png")); // PARSER_IMG_TYPEDEF_PROTECTED
    AddToImageList(list, prefix + _T("typedef_public.png")); // PARSER_IMG_TYPEDEF_PUBLIC
    AddToImageList(list, prefix + _T("symbols_folder.png")); // PARSER_IMG_SYMBOLS_FOLDER
    AddToImageList(list, prefix + _T("vars_folder.png")); // PARSER_IMG_VARS_FOLDER
    AddToImageList(list, prefix + _T("funcs_folder.png")); // PARSER_IMG_FUNCS_FOLDER
    AddToImageList(list, prefix + _T("enums_folder.png")); // PARSER_IMG_ENUMS_FOLDER
    AddToImageList(list, prefix + _T("macro_def_folder.png")); // PARSER_IMG_MACRO_DEF_FOLDER
    AddToImageList(list, prefix + _T("others_folder.png")); // PARSER_IMG_OTHERS_FOLDER
    AddToImageList(list, prefix + _T("typedefs_folder.png")); // PARSER_IMG_TYPEDEF_FOLDER
    AddToImageList(list, prefix + _T("macro_use.png")); // PARSER_IMG_MACRO_USE
    AddToImageList(list, prefix + _T("macro_use_private.png")); // PARSER_IMG_MACRO_USE_PRIVATE
    AddToImageList(list, prefix + _T("macro_use_protected.png")); // PARSER_IMG_MACRO_USE_PROTECTED
    AddToImageList(list, prefix + _T("macro_use_public.png")); // PARSER_IMG_MACRO_USE_PUBLIC
    AddToImageList(list, prefix + _T("macro_use_folder.png")); // PARSER_IMG_MACRO_USE_FOLDER

    return list;
}

// ----------------------------------------------------------------------------
ParseManager::ParseManager( LSPEventCallbackHandler* pLSPEventSinkHandler ) :
    // ----------------------------------------------------------------------------
////    m_TimerParsingOneByOne(this, idTimerParsingOneByOne),
    m_ClassBrowser(nullptr),
    m_ClassBrowserIsFloating(false),
    m_ParserPerWorkspace(false),
////    m_LastAISearchWasGlobal(false),
    m_LastControl(nullptr),
    m_LastFunctionIndex(-1),
    m_LastFuncTokenIdx(-1),
    m_LastLine(-1),
    m_LastResult(-1)
{
    // parser used when no project is loaded, holds options etc
    m_TempParser = new Parser(this, nullptr); // null pProject
    m_Parser     = m_TempParser;

    m_ParserPerWorkspace = false; //(ph 2021/08/26)

    m_pLSPEventSinkHandler = pLSPEventSinkHandler; //(ph 2021/10/23)
}
// ----------------------------------------------------------------------------
ParseManager::~ParseManager()
// ----------------------------------------------------------------------------
{
////    Disconnect(ParserCommon::idParserStart, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParseManager::OnParserStart));
////    Disconnect(ParserCommon::idParserEnd,   wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParseManager::OnParserEnd));
////    Disconnect(idTimerParsingOneByOne,      wxEVT_TIMER,                 wxTimerEventHandler(ParseManager::OnParsingOneByOneTimer));

    // clear any Idle time callbacks //(ph 2022/08/01)
    ClearAllIdleCallbacks();

    RemoveClassBrowser();
    ClearParsers();
    if (m_TempParser) //(ph 2022/04/13)
        Delete(m_TempParser);

    if (m_pProxyProject)
        m_pProxyProject->SetModified(false);

}
// ----------------------------------------------------------------------------
ParserBase* ParseManager::GetParserByProject(cbProject* project)
// ----------------------------------------------------------------------------
{
    // Returns parser associated with project from either the
    //  one-parser-per-workspace list or from the parser-per-project list

    if (m_ParserPerWorkspace) //always false for clangd_plugin
    {
        // Find parser associated with project from one-parser-per workspace list
        std::set<cbProject*>::iterator it = m_ParsedProjects.find(project);
        if (it != m_ParsedProjects.end())
            return m_ParserList.begin()->second;
    }
    else
    {
        // Find parser from parser-per-project list
        for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
        {
            if (it->first == project)
                return it->second;
        }
    }

    TRACE(_T("ParseManager::GetParserByProject: Returning nullptr."));
    return nullptr;
}
// ----------------------------------------------------------------------------
ParserBase* ParseManager::GetParserByFilename(const wxString& filename)
// ----------------------------------------------------------------------------
{
    cbProject* project = GetProjectByFilename(filename);
    return GetParserByProject(project);
}
// ----------------------------------------------------------------------------
cbProject* ParseManager::GetProjectByParser(ParserBase* parser)
// ----------------------------------------------------------------------------
{
    for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
    {
        if (it->second == parser)
            return it->first;
    }

    TRACE(_T("ParseManager::GetProjectByParser: Returning NULL."));
    return NULL;
}
// ----------------------------------------------------------------------------
cbProject* ParseManager::GetProjectByFilename(const wxString& filename)
// ----------------------------------------------------------------------------
{
    TRACE(_T("ParseManager::GetProjectByFilename: %s"), filename.wx_str());
    cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (activeProject)
    {
        // Get the parser for the active project
        ParserBase* parser = GetParserByProject(activeProject);
        //If (active parser parsed file) or (file belongs to active project) return active project
        if ( (parser && parser->IsFileParsed(filename)) //Why?
            || activeProject->GetFileByFilename(filename, false, true) )
        {
            return activeProject;
        }
        else // file not parsed or file does not belong to active project
        {
            ProjectsArray* projs = Manager::Get()->GetProjectManager()->GetProjects();
            for (size_t i = 0; i < projs->GetCount(); ++i)
            {
                cbProject* project = projs->Item(i);
                if (!project || project == activeProject)
                    continue;

                // if (file has been parsed by project) or (file belongs to project) return project
                parser = GetParserByProject(project);
                if ( (parser && parser->IsFileParsed(filename)) //also checks for standalone file
                    || project->GetFileByFilename(filename, false, true) )
                {
                    return project;
                }
            }
        }
    }

    return nullptr;
}
// ----------------------------------------------------------------------------
cbProject* ParseManager::GetProjectByEditor(cbEditor* editor)
// ----------------------------------------------------------------------------
{
    if (!editor)
        return nullptr;
    ProjectFile* pf = editor->GetProjectFile();
    if (pf && pf->GetParentProject())
        return pf->GetParentProject();
    return GetProjectByFilename(editor->GetFilename());
}
// ----------------------------------------------------------------------------
cbProject* ParseManager::GetActiveEditorProject()
// ----------------------------------------------------------------------------
{
    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    cbProject* project = GetProjectByEditor(editor);
    if (!project)
        project = Manager::Get()->GetProjectManager()->GetActiveProject();
    return project;
}
// ----------------------------------------------------------------------------
bool ParseManager::Done()
// ----------------------------------------------------------------------------
{
    bool done = true;
    for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
    {
        if (!it->second->Done())
        {
            done = false;
            break;
        }
    }
    TRACE(_T("ParseManager::Done: %s"), done ? _T("true"): _T("false"));
    return done;
}
// ----------------------------------------------------------------------------
wxImageList* ParseManager::GetImageList(int maxSize)
// ----------------------------------------------------------------------------
{
    const int size = cbFindMinSize16to64(maxSize);

    SizeToImageList::iterator it = m_ImageListMap.find(size);
    if (it == m_ImageListMap.end())
    {
        wxImageList *list = LoadImageList(size);
        m_ImageListMap.insert(SizeToImageList::value_type(size, std::unique_ptr<wxImageList>(list)));
        return list;
    }
    else
        return it->second.get();
}
int ParseManager::GetTokenKindImage(const Token* token)
{
    if (!token)
        return PARSER_IMG_NONE;

    switch (token->m_TokenKind)
    {
        case tkMacroDef:          return PARSER_IMG_MACRO_DEF;

        case tkEnum:
            switch (token->m_Scope)
            {
                case tsPublic:    return PARSER_IMG_ENUM_PUBLIC;
                case tsProtected: return PARSER_IMG_ENUM_PROTECTED;
                case tsPrivate:   return PARSER_IMG_ENUM_PRIVATE;
                case tsUndefined:
                default:          return PARSER_IMG_ENUM;
            }

        case tkEnumerator:        return PARSER_IMG_ENUMERATOR;

        case tkClass:
            switch (token->m_Scope)
            {
                case tsPublic:    return PARSER_IMG_CLASS_PUBLIC;
                case tsProtected: return PARSER_IMG_CLASS_PROTECTED;
                case tsPrivate:   return PARSER_IMG_CLASS_PRIVATE;
                case tsUndefined:
                default:          return PARSER_IMG_CLASS;
            }

        case tkNamespace:         return PARSER_IMG_NAMESPACE;

        case tkTypedef:
            switch (token->m_Scope)
            {
                case tsPublic:    return PARSER_IMG_TYPEDEF_PUBLIC;
                case tsProtected: return PARSER_IMG_TYPEDEF_PROTECTED;
                case tsPrivate:   return PARSER_IMG_TYPEDEF_PRIVATE;
                case tsUndefined:
                default:          return PARSER_IMG_TYPEDEF;
            }

        case tkMacroUse:
            switch (token->m_Scope)
            {
                case tsPublic:    return PARSER_IMG_MACRO_USE_PUBLIC;
                case tsProtected: return PARSER_IMG_MACRO_USE_PROTECTED;
                case tsPrivate:   return PARSER_IMG_MACRO_USE_PRIVATE;
                case tsUndefined:
                default:          return PARSER_IMG_MACRO_USE;
            }

        case tkConstructor:
            switch (token->m_Scope)
            {
                case tsProtected: return PARSER_IMG_CTOR_PROTECTED;
                case tsPrivate:   return PARSER_IMG_CTOR_PRIVATE;
                case tsUndefined:
                case tsPublic:
                default:          return PARSER_IMG_CTOR_PUBLIC;
            }

        case tkDestructor:
            switch (token->m_Scope)
            {
                case tsProtected: return PARSER_IMG_DTOR_PROTECTED;
                case tsPrivate:   return PARSER_IMG_DTOR_PRIVATE;
                case tsUndefined:
                case tsPublic:
                default:          return PARSER_IMG_DTOR_PUBLIC;
            }

        case tkFunction:
            switch (token->m_Scope)
            {
                case tsProtected: return PARSER_IMG_FUNC_PROTECTED;
                case tsPrivate:   return PARSER_IMG_FUNC_PRIVATE;
                case tsUndefined:
                case tsPublic:
                default:          return PARSER_IMG_FUNC_PUBLIC;
            }

        case tkVariable:
            switch (token->m_Scope)
            {
                case tsProtected: return PARSER_IMG_VAR_PROTECTED;
                case tsPrivate:   return PARSER_IMG_VAR_PRIVATE;
                case tsUndefined:
                case tsPublic:
                default:          return PARSER_IMG_VAR_PUBLIC;
            }

        case tkAnyContainer:
        case tkAnyFunction:
        case tkUndefined:
        default:                  return PARSER_IMG_NONE;
    }
}
// ----------------------------------------------------------------------------
wxArrayString ParseManager::GetAllPathsByFilename(const wxString& filename)
// ----------------------------------------------------------------------------
{
    TRACE(_T("ParseManager::GetAllPathsByFilename: Enter"));

    wxArrayString dirs;
    const wxFileName fn(filename);

    wxDir dir(fn.GetPath());
    if (!dir.IsOpened())
        return wxArrayString();

    wxArrayString files;
    ParseManagerHelper::ParserDirTraverser traverser(wxEmptyString, files);
    const wxString filespec = fn.HasExt() ? fn.GetName() + _T(".*") : fn.GetName();
    CCLogger::Get()->DebugLog(_T("ParseManager::GetAllPathsByFilename: Traversing '") + fn.GetPath() + _T("' for: ") + filespec);

    // search in the same directory of the input file
    dir.Traverse(traverser, filespec, wxDIR_FILES);

    // only find one file in the dir, which is the input file itself, try searching in other places
    if (files.GetCount() == 1)
    {
        cbProject* project = IsParserPerWorkspace() ? GetActiveEditorProject()
                                                    : GetProjectByParser(m_Parser);
        // search in the project
        if (project)
        {
            const wxString prjPath = project->GetCommonTopLevelPath();
            wxString priorityPath;
            if (fn.HasExt() && (fn.GetExt().StartsWith(_T("h")) || fn.GetExt().StartsWith(_T("c"))))
            {
                wxFileName priFn(prjPath);
                // hard-coded candidate path, the ./sdk or ./include under the project top level folder
                priFn.AppendDir(fn.GetExt().StartsWith(_T("h")) ? _T("sdk") : _T("include"));
                if (priFn.DirExists())
                {
                    priorityPath = priFn.GetFullPath();
                    wxDir priorityDir(priorityPath);
                    if ( priorityDir.IsOpened() )
                    {
                        wxArrayString priorityPathSub;
                        ParseManagerHelper::ParserDirTraverser traverser_2(wxEmptyString, priorityPathSub);
                        CCLogger::Get()->DebugLog(_T("ParseManager::GetAllPathsByFilename: Traversing '") + priorityPath + _T("' for: ") + filespec);
                        priorityDir.Traverse(traverser_2, filespec, wxDIR_FILES | wxDIR_DIRS);
                        if (priorityPathSub.GetCount() == 1)
                            AddPaths(dirs, priorityPathSub[0], fn.HasExt());
                    }
                }
            }

            if (dirs.IsEmpty())
            {
                wxDir prjDir(prjPath);
                if (prjDir.IsOpened())
                {
                    // try to search the project top level folder
                    wxArrayString prjDirSub;
                    ParseManagerHelper::ParserDirTraverser traverser_2(priorityPath, prjDirSub);
                    CCLogger::Get()->DebugLog(_T("ParseManager::GetAllPathsByFilename: Traversing '") + priorityPath + wxT(" - ") + prjPath + _T("' for: ") + filespec);
                    prjDir.Traverse(traverser_2, filespec, wxDIR_FILES | wxDIR_DIRS);
                    if (prjDirSub.GetCount() == 1)
                        AddPaths(dirs, prjDirSub[0], fn.HasExt());
                }
            }
        }
    }

    CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::GetAllPathsByFilename: Found %lu files:"), static_cast<unsigned long>(files.GetCount())));
    for (size_t i=0; i<files.GetCount(); i++)
        CCLogger::Get()->DebugLog(wxString::Format(_T("- %s"), files[i].wx_str()));

    if (!files.IsEmpty())
        AddPaths(dirs, files[0], fn.HasExt());

    TRACE(_T("ParseManager::GetAllPathsByFilename: Leave"));
    return dirs;
}
// ----------------------------------------------------------------------------
void ParseManager::AddPaths(wxArrayString& dirs, const wxString& path, bool hasExt)
// ----------------------------------------------------------------------------
{
    wxString s;
    if (hasExt)
        s = UnixFilename(path.BeforeLast(_T('.'))) + _T(".");
    else
        s = UnixFilename(path);

    if (dirs.Index(s, false) == wxNOT_FOUND)
        dirs.Add(s);
}
// ----------------------------------------------------------------------------
wxString ParseManager::GetHeaderForSourceFile(cbProject* pProject, wxString& filename)  //(ph 2021/05/19)
// ----------------------------------------------------------------------------
{
    // find the matching header file to this source file
    //-ProjectFile* pProjectFile = pProject->GetFileByFilename(filename, false);
    wxFileName fnFilename(filename);
    if (ParserCommon::FileType(filename) == ParserCommon::ftHeader) return wxString(); //already a header
    for (FilesList::const_iterator flist_it = pProject->GetFilesList().begin(); flist_it != pProject->GetFilesList().end(); ++flist_it)
    {
        ProjectFile* pf = *flist_it;
        if ( (ParserCommon::FileType(pf->relativeFilename) == ParserCommon::ftSource) //look for hdrs only
            or (FileTypeOf(pf->relativeFilename) == ftTemplateSource) )
            continue;
        if ( pf and (pf->file.GetName() == fnFilename.GetName()) )
        {
            if (ParserCommon::FileType(pf->relativeFilename) == ParserCommon::ftHeader)
            {
                return pf->file.GetFullPath();
            }
        }
    }
    return wxString();
}
// ----------------------------------------------------------------------------
wxString ParseManager::GetSourceForHeaderFile(cbProject* pProject, wxString& filename)   //(ph 2021/05/19)
// ----------------------------------------------------------------------------
{
    // find the matching source file to this header file
    //-ProjectFile* pProjectFile = pProject->GetFileByFilename(filename, false);
    wxFileName fnFilename(filename);
    //-if (FileTypeOf(filename) == ftSource) return wxString(); //already a source file //(ph 2022/06/01)-
    if (ParserCommon::FileType(filename) == ParserCommon::ftSource) return wxString(); //already a source file //(ph 2022/06/1)-
    for (FilesList::const_iterator flist_it = pProject->GetFilesList().begin(); flist_it != pProject->GetFilesList().end(); ++flist_it)
    {
        ProjectFile* pf = *flist_it;
        if ( ParserCommon::FileType(pf->relativeFilename) != ParserCommon::ftSource)
            continue;
        if ( pf and (pf->file.GetName() == fnFilename.GetName()) )
                return pf->file.GetFullPath();
    }
    return wxString();
}
// ----------------------------------------------------------------------------
wxString ParseManager::GetSourceOrHeaderForFile(cbProject* pProject, wxString& filename)    //(ph 2021/05/19)
// ----------------------------------------------------------------------------
{
    if (ParserCommon::FileType(filename) == ParserCommon::ftHeader)
        return GetSourceForHeaderFile(pProject, filename);
    if (ParserCommon::FileType(filename) == ParserCommon::ftSource)
        return GetHeaderForSourceFile(pProject, filename);
    return wxString();
}
// ----------------------------------------------------------------------------
ParserBase* ParseManager::CreateParser(cbProject* project, bool useSavedOptions)    //(ph 2021/05/25)
// ----------------------------------------------------------------------------
{
    if ( GetParserByProject(project) )
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::CreateParser: Parser for this project already exists!"));
        return nullptr;
    }

    // Easy case for "one parser per workspace" that has already been created:
    // m_ParserPerWorkspace always false for clangd
    if (m_ParserPerWorkspace && (not m_ParsedProjects.empty()) )
        return m_ParserList.begin()->second;

    TRACE(_T("ParseManager::CreateParser: Calling DoFullParsing()"));

    ParserBase* parser = new Parser(this, project); //read options, connect events, & stages files to parse

    // if using previous parser options get and set them into this parser   //(ph 2021/05/25)
    if (useSavedOptions)            //(ph 2021/05/25)
    {
        parser->Options() = GetSavedOptions();
        parser->ClassBrowserOptions() =  GetSavedBrowserOptions();
    }
    if ( not DoFullParsing(project, parser) )
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::CreateParser: Full parsing failed!"));
        delete parser;
        return nullptr;
    }

    // If current parser is the temp or proxy parser activate the new parser
    Parser* pProxyParser = (Parser*)GetParserByProject(GetProxyProject());
    if ( m_Parser == m_TempParser)
        SetParser(parser); // Also updates class browser
    else if (m_Parser == pProxyParser)
        SetParser(parser); // Also updates class browser

    if (m_ParserPerWorkspace) //always false for clangd
        m_ParsedProjects.insert(project);

    m_ParserList.push_back(std::make_pair(project, parser));

    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    wxString log(wxString::Format(_("ParseManager::CreateParser: Finished creating a new parser for project '%s'"), prj.wx_str()));
    //-CCLogger::Get()->Log(log);
    CCLogger::Get()->DebugLog(log);

////    RemoveObsoleteParsers();

    return parser;
}
// ----------------------------------------------------------------------------
bool ParseManager::DeleteParser(cbProject* project)
// ----------------------------------------------------------------------------
{
    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));

    ParserList::iterator parserList_it = m_ParserList.begin();
    if (not m_ParserPerWorkspace) //m_ParserPerWorkspace always false for clangd
    {
        for (; parserList_it != m_ParserList.end(); ++parserList_it)
        {
            if (parserList_it->first == project)
                break;
        }
    }

    if (parserList_it == m_ParserList.end())
    {
        CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::DeleteParser: Parser does not exist for delete '%s'!"), prj.wx_str()));
        return false;
    }

    if (m_ParsedProjects.empty()) // This indicates we are in one parser per one project mode
    {   // always true for clangd_client

        wxString log(wxString::Format(_("ParseManager::DeleteParser: Deleting parser for project '%s'!"), prj.wx_str()));
        CCLogger::Get()->Log(log);
        CCLogger::Get()->DebugLog(log);

        // The logic here is : firstly delete the parser instance, then see whether we need an
        // active parser switch (call SetParser())
        //ollydbg crash fix 2022/10/28 Reply#230 https://forums.codeblocks.org/index.php/topic,24357.msg171551.html#msg171551
        ParserBase* pDeletedParser = parserList_it->second;
        delete parserList_it->second;      // delete the instance first, then remove from the list
        m_ParserList.erase(parserList_it); // remove deleted parser from parser list

        // if the active parser is deleted, set the active parser to nullptr
        if (pDeletedParser == m_Parser)
        {
            m_Parser = nullptr;
            SetParser(m_TempParser); // Also updates class browser; do not use SetParser(m_TempParser) //(ph 2022/06/6)-
        }

        return true;
    }

////    if (removeProjectFromParser)
////        return true;

    CCLogger::Get()->DebugLog(_T("ParseManager::DeleteParser: Deleting parser failed!"));
    return false;
}
////// ----------------------------------------------------------------------------
////bool ParseManager::ReparseFile(cbProject* project, const wxString& filename)
////// ----------------------------------------------------------------------------
////{
////    if (ParserCommon::FileType(filename) == ParserCommon::ftOther)
////        return false;
////
////    ParserBase* parser = GetParserByProject(project);
////    if (!parser)
////        return false;
////
////    if (!parser->UpdateParsingProject(project))
////        return false;
////
////    TRACE(_T("ParseManager::ReparseFile: Calling Parser::Reparse()"));
////
////    return parser->Reparse(filename);
////}
// ----------------------------------------------------------------------------
bool ParseManager::AddFileToParser(cbProject* project, const wxString& filename, ParserBase* parser)
// ----------------------------------------------------------------------------
{
    if (ParserCommon::FileType(filename) == ParserCommon::ftOther)
        return false;

    if (!parser)
    {
        parser = GetParserByProject(project);
        if (!parser)
            return false;
    }

    if (!parser->UpdateParsingProject(project))
        return false;

    TRACE(_T("ParseManager::AddFileToParser: Calling Parser::AddFile()"));

    return parser->AddFile(filename, project);
}

// ----------------------------------------------------------------------------
void ParseManager::RemoveFileFromParser(cbProject* project, const wxString& filename)
// ----------------------------------------------------------------------------
{
    ParserBase* parser = GetParserByProject(project);
    if (!parser)
        return ;

    TRACE(_T("ParseManager::RemoveFileFromParser: Calling Parser::RemoveFile()"));

    //return parser->RemoveFile(filename);
    parser->RemoveFile(filename);
    return;

}
// ----------------------------------------------------------------------------
void ParseManager::RereadParserOptions()
// ----------------------------------------------------------------------------
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));
    bool useSymbolBrowser = cfg->ReadBool(_T("/use_symbols_browser"), true);

    if (useSymbolBrowser)
    {
        if (!m_ClassBrowser)
        {
            CreateClassBrowser();
            UpdateClassBrowser();
        }
        // change class-browser docking settings
        else if (m_ClassBrowserIsFloating != cfg->ReadBool(_T("/as_floating_window"), false))
        {
            RemoveClassBrowser();
            CreateClassBrowser();
            // force re-update
            UpdateClassBrowser();
        }
    }
    else if (!useSymbolBrowser && m_ClassBrowser)
        RemoveClassBrowser();

    //-const bool parserPerWorkspace = cfg->ReadBool(_T("/parser_per_workspace"), false);
    const bool parserPerWorkspace = false;//(ph 2021/08/26)
    if (m_Parser == m_TempParser)
    {
        m_ParserPerWorkspace = parserPerWorkspace; //always false for clangd
        return;
    }

////    RemoveObsoleteParsers();

    // re-parse if settings changed
    ParserOptions opts = m_Parser->Options();
    m_Parser->ReadOptions();
    bool reparse = false;
    cbProject* project = GetActiveEditorProject();
    if (   opts.followLocalIncludes  != m_Parser->Options().followLocalIncludes
        || opts.followGlobalIncludes != m_Parser->Options().followGlobalIncludes
        || opts.wantPreprocessor     != m_Parser->Options().wantPreprocessor
        || opts.parseComplexMacros   != m_Parser->Options().parseComplexMacros
        || opts.logClangdClientCheck != m_Parser->Options().logClangdClientCheck
        || opts.logClangdServerCheck != m_Parser->Options().logClangdServerCheck
        || opts.logPluginInfoCheck   != m_Parser->Options().logPluginInfoCheck
        || opts.logPluginDebugCheck  != m_Parser->Options().logPluginDebugCheck
        || opts.LLVM_MasterPath      != m_Parser->Options().LLVM_MasterPath //(ph 2021/11/7)
        || m_ParserPerWorkspace      != parserPerWorkspace ) //always false for clangd
    {
        // important options changed... flag for reparsing
        if (cbMessageBox(_("You changed some class parser options. Do you want to "
                           "reparse your projects now, using the new options?"),
                         _("Reparse?"), wxYES_NO | wxICON_QUESTION) == wxID_YES)
        {
            reparse = true;
        }
    }

    if (reparse)
        ClearParsers();

    //-m_ParserPerWorkspace = parserPerWorkspace; //always false for clangd
    m_ParserPerWorkspace = false;

    if (reparse)
        CreateParser(project);
}
// ----------------------------------------------------------------------------
void ParseManager::ReparseCurrentProject()
// ----------------------------------------------------------------------------
{
    // Invoked from codecompletion::ReparseCurrentProject event

    cbProject* pProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (pProject)
    {
        TRACE(_T("ParseManager::ReparseCurrentProject: Calling DeleteParser() and CreateParser()"));

        // Save the current parser options //(ph 2021/05/25)
        ParserOptionsSave(m_Parser);
        BrowserOptionsSave(m_Parser);

        DeleteParser(pProject);
        // The old options have just been overwritten by DeleteParser();
        // DeleteParser() calls SetParser() calls ClassBrowser->SetParser() calls WriteOptions()
        bool useSavedOptions = true;
        CreateParser(pProject, useSavedOptions);
    }
}
// ----------------------------------------------------------------------------
void ParseManager::ReparseCurrentEditor()
// ----------------------------------------------------------------------------
{
    // Invoked from codecompletion::ReparseCurrentProject event

    cbProject* project = GetActiveEditorProject();
    if (project)
    {
        TRACE(_T("ParseManager::ReparseCurrentProject: Calling DeleteParser() and CreateParser()"));

        // Save the current parser options //(ph 2021/05/25)
        ParserOptionsSave(m_Parser);
        BrowserOptionsSave(m_Parser);

        DeleteParser(project);
        // The old options have just been overwritten by DeleteParser();
        // DeleteParser() calls SetParser() calls ClassBrowser->SetParser() calls WriteOptions()
        bool useSavedOptions = true;
        CreateParser(project, useSavedOptions);
    }
}
// ----------------------------------------------------------------------------
void ParseManager::ReparseSelectedProject()
// ----------------------------------------------------------------------------
{
    wxTreeCtrl* tree = Manager::Get()->GetProjectManager()->GetUI().GetTree();
    if (!tree)
        return;

    wxTreeItemId treeItem = Manager::Get()->GetProjectManager()->GetUI().GetTreeSelection();
    if (!treeItem.IsOk())
        return;

    const FileTreeData* data = static_cast<FileTreeData*>(tree->GetItemData(treeItem));
    if (!data)
        return;

    if (data->GetKind() == FileTreeData::ftdkProject)
    {
        cbProject* project = data->GetProject();
        if (project)
        {
            TRACE(_T("ParseManager::ReparseSelectedProject: Calling DeleteParser() and CreateParser()"));
            DeleteParser(project);
            CreateParser(project);
        }
    }
}

////// Here, we collect the "using namespace XXXX" directives
////// Also, we locate the current caret in which function, then, add the function parameters to Token trie
////// Also, the variables in the function body( local block ) was add to the Token trie
////size_t ParseManager::MarkItemsByAI(ccSearchData* searchData,
////                                   TokenIdxSet&  result,
////                                   bool          reallyUseAI,
////                                   bool          isPrefix,
////                                   bool          caseSensitive,
////                                   int           caretPos)
////{
////    result.clear();
////
////    if (!m_Parser->Done())
////    {
////        wxString msg(_("The Parser is still parsing files."));
////        msg += m_Parser->NotDoneReason();
////        CCLogger::Get()->DebugLog(msg);
////        return 0;
////    }
////
////    TRACE(_T("ParseManager::MarkItemsByAI_2()"));
////
////    TokenTree* tree = m_Parser->GetTempTokenTree();
////
////    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////    // remove old temporaries
////    tree->Clear();
////
////    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////    RemoveLastFunctionChildren(m_Parser->GetTokenTree(), m_LastFuncTokenIdx);
////
////    // find "using namespace" directives in the file
////    TokenIdxSet search_scope;
////    ParseUsingNamespace(searchData, search_scope, caretPos);
////
////    // parse function's arguments
////    ParseFunctionArguments(searchData, caretPos);
////
////    // parse current code block (from the start of function up to the cursor)
////    ParseLocalBlock(searchData, search_scope, caretPos);
////
////    if (!reallyUseAI)
////    {
////        tree = m_Parser->GetTokenTree();
////
////        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////        // all tokens, no AI whatsoever
////        for (size_t i = 0; i < tree->size(); ++i)
////            result.insert(i);
////
////        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////        return result.size();
////    }
////
////    // we have correctly collected all the tokens, so we will do the artificial intelligence search
////    return AI(result, searchData, wxEmptyString, isPrefix, caseSensitive, &search_scope, caretPos);
////}

////size_t ParseManager::MarkItemsByAI(TokenIdxSet& result,
////                                   bool         reallyUseAI,
////                                   bool         isPrefix,
////                                   bool         caseSensitive,
////                                   int          caretPos)
////{
////    if (s_DebugSmartSense)
////        CCLogger::Get()->DebugLog(wxString::Format(_T("MarkItemsByAI_1()")));
////
////    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
////    if (!editor)
////        return 0;
////
////    ccSearchData searchData = { editor->GetControl(), editor->GetFilename() };
////    if (!searchData.control)
////        return 0;
////
////    TRACE(_T("ParseManager::MarkItemsByAI_1()"));
////
////    return MarkItemsByAI(&searchData, result, reallyUseAI, isPrefix, caseSensitive, caretPos);
////}

////int ParseManager::GetCallTips(wxArrayString& items, int& typedCommas, cbEditor* ed, int pos)
////{
////    items.Clear();
////    typedCommas = 0;
////    int commas = 0;
////
////    if (!ed || !m_Parser->Done())
////    {
////        items.Add(wxT("Parsing at the moment..."));
////        return wxSCI_INVALID_POSITION;
////    }
////
////    TRACE(_T("ParseManager::GetCallTips()"));
////
////    ccSearchData searchData = { ed->GetControl(), ed->GetFilename() };
////    if (pos == wxNOT_FOUND)
////        pos = searchData.control->GetCurrentPos();
////    int nest = 0;
////    while (--pos > 0)
////    {
////        const int style = searchData.control->GetStyleAt(pos);
////        if (   searchData.control->IsString(style)
////            || searchData.control->IsCharacter(style)
////            || searchData.control->IsComment(style) )
////        {
////            continue;
////        }
////
////        const wxChar ch = searchData.control->GetCharAt(pos);
////        if (ch == _T(';'))
////            return wxSCI_INVALID_POSITION;
////        else if (ch == _T(','))
////        {
////            if (nest == 0)
////                ++commas;
////        }
////        else if (ch == _T(')'))
////            --nest;
////        else if (ch == _T('('))
////        {
////            ++nest;
////            if (nest > 0)
////                break;
////        }
////    }// while
////
////    // strip un-wanted
////    while (--pos > 0)
////    {
////        if (   searchData.control->GetCharAt(pos) <= _T(' ')
////            || searchData.control->IsComment(searchData.control->GetStyleAt(pos)) )
////        {
////            continue;
////        }
////        break;
////    }
////
////    const int start = searchData.control->WordStartPosition(pos, true);
////    const int end = searchData.control->WordEndPosition(pos, true);
////    const wxString target = searchData.control->GetTextRange(start, end);
////    TRACE(_T("Sending \"%s\" for call-tip"), target.wx_str());
////    if (target.IsEmpty())
////        return wxSCI_INVALID_POSITION;
////
////    TokenIdxSet result;
////    MarkItemsByAI(result, true, false, true, end);
////
////    ComputeCallTip(m_Parser->GetTokenTree(), result, items);
////
////    typedCommas = commas;
////    TRACE(_T("ParseManager::GetCallTips: typedCommas=%d"), typedCommas);
////    items.Sort();
////    return end;
////}

// ----------------------------------------------------------------------------
wxArrayString ParseManager::ParseProjectSearchDirs(const cbProject &project)
// ----------------------------------------------------------------------------
{

    const TiXmlNode *extensionNode = project.GetExtensionsNode();
    if (!extensionNode)
        return wxArrayString();
    const TiXmlElement* elem = extensionNode->ToElement();
    if (!elem)
        return wxArrayString();

    wxArrayString pdirs;
    const TiXmlElement* CCConf = elem->FirstChildElement("clangd_client");
    if (CCConf)
    {
        const TiXmlElement* pathsElem = CCConf->FirstChildElement("search_path");
        while (pathsElem)
        {
            if (pathsElem->Attribute("add"))
            {
                wxString dir = cbC2U(pathsElem->Attribute("add"));
                if (pdirs.Index(dir) == wxNOT_FOUND)
                    pdirs.Add(dir);
            }

            pathsElem = pathsElem->NextSiblingElement("search_path");
        }
    }
    return pdirs;
}
// ----------------------------------------------------------------------------
void ParseManager::SetProjectSearchDirs(cbProject &project, const wxArrayString &dirs)
// ----------------------------------------------------------------------------
{
    TiXmlNode *extensionNode = project.GetExtensionsNode();
    if (!extensionNode)
        return;
    TiXmlElement* elem = extensionNode->ToElement();
    if (!elem)
        return;

    // since rev4332, the project keeps a copy of the <Extensions> element
    // and re-uses it when saving the project (so to avoid losing entries in it
    // if plugins that use that element are not loaded atm).
    // so, instead of blindly inserting the element, we must first check it's
    // not already there (and if it is, clear its contents)
    TiXmlElement* node = elem->FirstChildElement("clangd_client");
    if (!node)
        node = elem->InsertEndChild(TiXmlElement("clangd_client"))->ToElement();
    if (node)
    {
        node->Clear();
        for (size_t i = 0; i < dirs.GetCount(); ++i)
        {
            TiXmlElement* path = node->InsertEndChild(TiXmlElement("search_path"))->ToElement();
            if (path) path->SetAttribute("add", cbU2C(dirs[i]));
        }
    }
}
// ----------------------------------------------------------------------------
void ParseManager::CreateClassBrowser()
// ----------------------------------------------------------------------------
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));
    if (m_ClassBrowser || !cfg->ReadBool(_T("/use_symbols_browser"), true))
        return;

    TRACE(_T("ParseManager::CreateClassBrowser: Enter"));

    m_ClassBrowserIsFloating = cfg->ReadBool(_T("/as_floating_window"), false);

    if (m_ClassBrowserIsFloating)
    {
        m_ClassBrowser = new ClassBrowser(Manager::Get()->GetAppWindow(), this);

        // make this a free floating/docking window
        CodeBlocksDockEvent evt(cbEVT_ADD_DOCK_WINDOW);

        evt.name = _T("SymbolsBrowser");
        evt.title = _("Symbols browser");
        evt.pWindow = m_ClassBrowser;
        evt.dockSide = CodeBlocksDockEvent::dsRight;
        evt.desiredSize.Set(200, 250);
        evt.floatingSize.Set(200, 250);
        evt.minimumSize.Set(150, 150);
        evt.shown = true;
        evt.hideable = true;
        Manager::Get()->ProcessEvent(evt);
        m_ClassBrowser->UpdateSash();
    }
    else
    {
        // make this a tab in projectmanager notebook
        m_ClassBrowser = new ClassBrowser(Manager::Get()->GetProjectManager()->GetUI().GetNotebook(), this);
        Manager::Get()->GetProjectManager()->GetUI().GetNotebook()->AddPage(m_ClassBrowser, _("Symbols"));
        m_ClassBrowser->UpdateSash();
    }

    // Dreaded DDE-open bug related: do not touch unless for a good reason
    // TODO (Morten): ? what's bug? I test it, it's works well now.
    m_ClassBrowser->SetParser(m_Parser); // Also updates class browser

    TRACE(_T("ParseManager::CreateClassBrowser: Leave"));
}
// ----------------------------------------------------------------------------
void ParseManager::RemoveClassBrowser(cb_unused bool appShutDown)
// ----------------------------------------------------------------------------
{
    if (!m_ClassBrowser)
        return;

    TRACE(_T("ParseManager::RemoveClassBrowser()"));

    if (m_ClassBrowserIsFloating)
    {
        CodeBlocksDockEvent evt(cbEVT_REMOVE_DOCK_WINDOW);
        evt.pWindow = m_ClassBrowser;
        Manager::Get()->ProcessEvent(evt);
    }
    else
    {
        int idx = Manager::Get()->GetProjectManager()->GetUI().GetNotebook()->GetPageIndex(m_ClassBrowser);
        if (idx != -1)
            Manager::Get()->GetProjectManager()->GetUI().GetNotebook()->RemovePage(idx);
    }
    m_ClassBrowser->Destroy();
    m_ClassBrowser = NULL;
}
// ----------------------------------------------------------------------------
void ParseManager::UpdateClassBrowser()
// ----------------------------------------------------------------------------
{
    if (not m_ClassBrowser)
          return;

    TRACE(_T("ParseManager::UpdateClassBrowser()"));

    if ( m_Parser != m_TempParser
        && m_Parser->Done()
        && (not Manager::IsAppShuttingDown()) )
    {
        m_ClassBrowser->UpdateClassBrowserView();
    }
}
// ----------------------------------------------------------------------------
void ParseManager::GetPriorityFilesForParsing(StringList& localSourcesList, cbProject* pProject)     //(ph 2021/11/11)
// ----------------------------------------------------------------------------
{
    EditorManager* pEdMgr = Manager::Get()->GetEditorManager();

    if ( pEdMgr->GetEditorsCount())
    {
        // -------------------------------------------------------
        // Add an entry for the active 'GetBuiltinActiveEditor()' first, so the user
        // can use goto def's/decl's and code completion as soon as possible.
        // -------------------------------------------------------
        cbEditor* pEditor = pEdMgr->GetBuiltinActiveEditor();
        if (pEditor) switch(1)
        {
            default:
            wxString filename = pEditor->GetFilename();
            // Find the ProjectFile and project containing this editors file.
            ProjectFile* pProjectFile = pEditor->GetProjectFile();
            if (not pProjectFile) break;
            cbProject* pFilesProject = pProjectFile->GetParentProject();
            // For LSP, file must belong to a project, because LSP needs target compiler parameters.
            if (not pFilesProject) break;
            if (pFilesProject != pProject) break; //file doesnt belong to this project
            ParserCommon::EFileType ft = ParserCommon::FileType(pEditor->GetShortName()); //(ph 2022/06/01)
            //-if ( ft != ftHeader && ft != ftSource && ft != ftTemplateSource) // only parse source/header files
            if (ft == ParserCommon::ftOther)
                break;

            // add file to background parsing queue
            localSourcesList.push_back(filename);

            // The following is wasted work since the file will be opened and parsed anyway
            // when the use requests goto decl/defs.
            //// add associated .h or .cpp file for quick got decl/defs
            ////wxString hdrOrSrcFile = GetSourceOrHeaderForFile((pProject, filename);
            ////if (hdrOrSrcFile.Length())
            ////    localSourcesList.push_back(hdrOrSrcFile);

            break;
        } //endif switch

        // -------------------------------------------------------
        // Add a file list entry for the remaining open editor files
        // -------------------------------------------------------
        for (int ii=0; ii< pEdMgr->GetEditorsCount(); ++ii)
        {
            cbEditor* pEditor = pEdMgr->GetBuiltinEditor(ii);
            if (pEditor)
            {
                // don't re-list an already listed editor file
                wxString filename = pEditor->GetFilename();
                std::list<wxString>::iterator findIter = std::find(localSourcesList.begin(), localSourcesList.end(), filename);
                if (findIter != localSourcesList.end())
                {
                    wxString foundItem = *findIter; // **debugging**
                    continue;
                }
                // Find the ProjectFile and project containing this editors file.
                ProjectFile* pProjectFile = pEditor->GetProjectFile();
                if (not pProjectFile) continue;
                cbProject* pFilesProject = pProjectFile->GetParentProject();
                // For LSP, file must belong to a project, because LSP needs target compile parameters.
                if (not pFilesProject) continue;
                if (pFilesProject != pProject) continue;
                // only parse source/header files
                ParserCommon::EFileType ft = ParserCommon::FileType(pEditor->GetShortName());             //(ph 2022/06/01)
                if ( (ft != ParserCommon::ftHeader) && (ft != ParserCommon::ftSource)                     //(ph 2022/06/01)
                    && (FileTypeOf(pEditor->GetShortName()) != ftTemplateSource) )
                    continue;

                localSourcesList.push_back(filename);
                continue;
            }//endif pcbEd
        }//endfor editorCount
    }
}
// ----------------------------------------------------------------------------
bool ParseManager::DoFullParsing(cbProject* project, ParserBase* parser)
// ----------------------------------------------------------------------------
{
    //-wxStopWatch timer;
    if (!parser)
        return false;

    TRACE(_T("ParseManager::DoFullParsing: Enter"));

    // add per-project dirs
    if (project)
    {
        if ( not parser->Options().platformCheck
            || (parser->Options().platformCheck && project->SupportsCurrentPlatform()) )
        {
            // Note: This parses xml data to get the search directories. It might be expensive if
            //       the list of directories is too large.
            AddIncludeDirsToParser(ParseProjectSearchDirs(*project),
                                   project->GetBasePath(), parser);
        }

        AddCompilerAndIncludeDirs(project, parser); //(ph 2021/11/4)
        //(ph 2021/11/4) // **debugging*
        for (size_t ii=0; ii<parser->GetIncludeDirs().Count(); ++ii)
        {
            if (ii == 0 ) CCLogger::Get()->DebugLog("IncludeDirs array:");
            CCLogger::Get()->DebugLog(wxString::Format("\t%s", parser->GetIncludeDirs()[ii]));
        }

    }//end if project

    //FIXME (ph#): ShowInheritance config setting and context menu setting is out of sync. Only saved on exit.
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));     //(ph 2021/05/24)
    bool cfgShowInheritance = cfg->ReadBool(_T("/browser_show_inheritance"),    false);
    BrowserOptions& options = parser->ClassBrowserOptions();
    if (cfgShowInheritance or options.showInheritance) cfgShowInheritance = true;

    StringList localSources;

    if (project)  //LSP sorts the filenames by modification time //(ph 2021/04/16)
    {
        // Note: Windows 10 file access time is not updated when the volume is > 128 Gig
        //  unless the user has issued "fsutil behavior set disablelastaccess <0-3>"
        //  (only valid in Windows 10 version 1803 and higher.)
        //  cf: "fsutil behavior query disablelastaccess" command to get status.
        //    Value	Description
        //    0	User Managed, Last Access Time Updates Enabled
        //    1	User Managed, Last Access Time Updates Disabled
        //    2 (default)	System Managed, Last Access Time Updates Enabled
        //    3	System Managed, Last Access Time Updates Disabled
        //  https://www.tenforums.com/tutorials/139015-enable-disable-ntfs-last-access-time-stamp-updates-windows-10-a.html
        // So here, we sort by modification time since read access time is usually invalid.

        // For LSP, sort the filenames by modification time //(ph 2021/04/16)
        // so that the latest modified files get parsed first.
        std::multimap<wxDateTime,wxString>sortedSources;
        for (FilesList::const_iterator fl_it = project->GetFilesList().begin();
             fl_it != project->GetFilesList().end(); ++fl_it)
        {
            ProjectFile* pf = *fl_it;
            if (!pf)
                continue;
            // If the file is already open, it'll be skipped later by OnBatchTimer(). //(ph 2021/06/11)
            // It needs to be included so that it's header/source is background parsed
            // Sort the source files by recently "file modified time".
            ParserCommon::EFileType ft = ParserCommon::FileType(pf->relativeFilename);
            if (ft == ParserCommon::ftSource) // parse source files
            {
                wxDateTime lastAccTime;
                wxDateTime lastModTime;
                wxFileName fn(pf->file.GetFullPath() );
                if (not fn.FileExists() ) continue;
                bool ok = fn.GetTimes(&lastAccTime,&lastModTime, nullptr);
                if (not ok) continue;
                // Order the file by last modified time
                sortedSources.insert(std::pair<wxDateTime,wxString>(lastModTime, pf->file.GetFullPath()) );
            }
        }//for file list

        // Add active editor files to localSources list, then add other open editor
        // files before adding unopened files. This allows priority parsing of the most used files.
        GetPriorityFilesForParsing(localSources, project);

        // add the sorted sources list to the  already collected editor sources list
        // This creates a list sorted by active files then modified files, then lesser used files
        // Feeding this list to the parser allows the user to do code completion soonest.
        if (sortedSources.size())
        {
            for (std::multimap<wxDateTime,wxString>::reverse_iterator it=sortedSources.rbegin(); it!=sortedSources.rend(); ++it)
            {
                // If the config asks for inheritance, parse the header files also.
                if (cfgShowInheritance)
                {
                    wxString hdrPath = GetHeaderForSourceFile(project, it->second);
                    if (hdrPath.Length())
                    {
                        // add header file if not duplicate
                        std::list<wxString>::iterator findIter = std::find(localSources.begin(), localSources.end(), hdrPath);
                        if (findIter == localSources.end() )
                            localSources.push_back(hdrPath);
                    }
                }
                // add source file if not duplicate
                std::list<wxString>::iterator findIter = std::find(localSources.begin(), localSources.end(), it->second);
                if (findIter == localSources.end())
                    localSources.push_back(it->second);
            }//endif for
        }//endif sortedSources
    }//endif project

    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));

    if (not localSources.empty())
    {
        CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::DoFullParsing: Added %lu source file(s) for project '%s' to batch-parser..."),
                                    static_cast<unsigned long>( localSources.size()), prj.wx_str()));

        //for (const wxString& entry : localSources) // **Debugging**
        //{
        //    wxString msg = wxString::Format("%s adding file to parser: %s", __FUNCTION__, entry);
        //    CCLogger::Get()->DebugLog(DebugLog(msg);
        //}
        // local source files added to Parser
        parser->AddBatchParse(localSources);
    }

    TRACE(_T("ParseManager::DoFullParsing: Leave"));

    return true;
}
// ----------------------------------------------------------------------------
bool ParseManager::SwitchParser(cbProject* project, ParserBase* parser)
// ----------------------------------------------------------------------------
{
    if (!parser || parser == m_Parser || GetParserByProject(project) != parser)
    {
        TRACE(_T("ParseManager::SwitchParser: No need to / cannot switch."));
        return false;
    }

    TRACE(_T("ParseManager::SwitchParser()"));

    SetParser(parser); // Also updates class browser

    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    wxString log(wxString::Format(_("Switching parser to project '%s'"), prj.wx_str()));
    CCLogger::Get()->Log(log);
    CCLogger::Get()->DebugLog(log);

    return true;
}
// ----------------------------------------------------------------------------
void ParseManager::SetParser(ParserBase* parser)
// ----------------------------------------------------------------------------
{
    // if the active parser is the same as the old active parser, nothing need to be done
    if (m_Parser == parser)
        return;

    #if defined(cbDEBUG)
    if (not Manager::IsAppShuttingDown())
    {
        wxString fromProject = "*NONE*";
        wxString toProject = "*NONE*";
        // The parser and project pointers can be null; Esp., for the TempParser
        fromProject = (m_Parser and ((Parser*)m_Parser)->GetParsersProject()) ? ((Parser*)m_Parser)->GetParsersProject()->GetTitle() : "*NONE*";
        toProject   = (  parser and ((Parser*)  parser)->GetParsersProject()) ? ((Parser*)  parser)->GetParsersProject()->GetTitle() : "*NONE*";
        wxString msg = wxString::Format("Switching parser/project from %s to %s", fromProject, toProject); //(ph 2022/06/4)
        CCLogger::Get()->DebugLog(msg);
    }
    #endif

    // a new parser is active, so remove the old parser's local variable tokens.
    // if m_Parser == nullptr, this means the active parser is already deleted.
    if (m_Parser)
        RemoveLastFunctionChildren(m_Parser->GetTokenTree(), m_LastFuncTokenIdx);

    // refresh code completion related variables
    InitCCSearchVariables();

    // switch the active parser
    m_Parser = parser;

    if (m_ClassBrowser)
        m_ClassBrowser->SetParser(parser); // Also updates class browser
}
// ----------------------------------------------------------------------------
void ParseManager::ClearParsers()
// ----------------------------------------------------------------------------
{
    TRACE(_T("ParseManager::ClearParsers()"));

    if (m_ParserPerWorkspace) //always false for clangd
    {
        while (!m_ParsedProjects.empty() && DeleteParser(*m_ParsedProjects.begin()))
            ;
    }
    else
    {
        while (!m_ParserList.empty() && DeleteParser(m_ParserList.begin()->first))
            ;
    }
}
////// ----------------------------------------------------------------------------
////void ParseManager::RemoveObsoleteParsers()
////// ----------------------------------------------------------------------------
////{
////    // For number of active parsers that exceed 'settings: max parsers allowed',
////    // removes/deletes parsers that are not associated with the current editors file
////
////    cbAssertNonFatal(0 && "RemoveObsoleteParsers() should be removed!");
////    return;
////
////    TRACE(_T("ParseManager::RemoveObsoleteParsers: Enter"));
////
////    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("clangd_client"));
////    const size_t maxParsers = cfg->ReadInt(_T("/max_parsers"), 5);
////    wxArrayString removedProjectNames;
////    std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
////
////    while (m_ParserList.size() > maxParsers)
////    {
////        bool deleted = false;
////        for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
////        {
////            if (it->second == info.second)
////                continue;
////
////            wxString prj = (it->first ? it->first->GetTitle() : _T("*NONE*"));
////            if ( DeleteParser(it->first) )
////            {
////                // Please note that DeleteParser() may erase one element of the m_ParserList, so
////                // do NOT use the constant iterator here again, as the element pointed by it may be
////                // destroyed in DeleteParser().
////                removedProjectNames.Add(prj);
////                deleted = true;
////                break;
////            }
////        }
////
////        if (not deleted)
////            break;
////    }
////
////    for (size_t i = 0; i < removedProjectNames.GetCount(); ++i)
////    {
////        wxString log(wxString::Format(_("ParseManager::RemoveObsoleteParsers:Removed obsolete parser of '%s'"), removedProjectNames[i].wx_str()));
////        CCLogger::Get()->Log(log);
////        CCLogger::Get()->DebugLog(log);
////    }
////
////    TRACE(_T("ParseManager::RemoveObsoleteParsers: Leave"));
////}
// ----------------------------------------------------------------------------
std::pair<cbProject*, ParserBase*> ParseManager::GetParserInfoByCurrentEditor()
// ----------------------------------------------------------------------------
{
    std::pair<cbProject*, ParserBase*> info(nullptr, nullptr);
    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();

    if ( editor ) //No need to check editor->GetFilename, because a built-in editor always have a filename
    {
        info.first  = GetProjectByEditor(editor);
        info.second = GetParserByProject(info.first);
    }

    return info;
}
// ----------------------------------------------------------------------------
void ParseManager::SetCBViewMode(const BrowserViewMode& mode)
// ----------------------------------------------------------------------------
{
    m_Parser->ClassBrowserOptions().showInheritance = (mode == bvmInheritance) ? true : false;
    UpdateClassBrowser();
}

// helper funcs

////// Start an Artificial Intelligence (!) sequence to gather all the matching tokens..
////// The actual AI is in FindAIMatches() below...
//// ----------------------------------------------------------------------------
////size_t ParseManager::AI(TokenIdxSet&    result,
//// ----------------------------------------------------------------------------
////                        ccSearchData*   searchData,
////                        const wxString& lineText,
////                        bool            isPrefix,
////                        bool            caseSensitive,
////                        TokenIdxSet*    search_scope,
////                        int             caretPos)
////{
////    m_LastAISearchWasGlobal = false;
////    m_LastAIGlobalSearch.Clear();
////
////    int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
////    if (pos < 0 || pos > searchData->control->GetLength())
////        return 0;
////
////    int line = searchData->control->LineFromPosition(pos);
////
////    // Get the actual search text, such as "objA.m_aaa.m_bbb"
////    wxString actual_search(lineText);
////    if (actual_search.IsEmpty())
////    {
////        // Get the position at the start of current line
////        const int startPos = searchData->control->PositionFromLine(line);
////        actual_search = searchData->control->GetTextRange(startPos, pos).Trim();
////    }
////
////    // Do the whole job here
////    if (s_DebugSmartSense)
////    {
////        CCLogger::Get()->DebugLog(_T("AI() ========================================================="));
////        CCLogger::Get()->DebugLog(wxString::Format(_T("AI() Doing AI for '%s':"), actual_search.wx_str()));
////    }
////    TRACE(_T("ParseManager::AI()"));
////
////    TokenTree* tree = m_Parser->GetTokenTree();
////
////    // find current function's namespace so we can include local scope's tokens
////    // we ' ll get the function's token (all matches) and add its parent namespace
////    TokenIdxSet proc_result;
////    size_t found_at = FindCurrentFunctionToken(searchData, proc_result, pos);
////
////    TokenIdxSet scope_result;
////    if (found_at)
////        FindCurrentFunctionScope(tree, proc_result, scope_result);
////
////    // add additional search scopes???
////    // for example, we are here:
////    /*  void ClassA::FunctionB(int paraC){
////            m_aaa
////    */
////    // then, ClassA should be added as a search_scope, the global scope should be added too.
////
////    // if search_scope is already defined, then, add scope_result to search_scope
////    // otherwise we just set search_scope as scope_result
////    if (!search_scope)
////        search_scope = &scope_result;
////    else
////    {
////        // add scopes, "tis" refer to "token index set"
////        for (TokenIdxSet::const_iterator tis_it = scope_result.begin(); tis_it != scope_result.end(); ++tis_it)
////            search_scope->insert(*tis_it);
////    }
////
////    // remove non-namespace/class tokens
////    CleanupSearchScope(tree, search_scope);
////
////    // find all other matches
////    std::queue<ParserComponent> components;
////    BreakUpComponents(actual_search, components);
////
////    m_LastAISearchWasGlobal = components.size() <= 1;
////    if (!components.empty())
////        m_LastAIGlobalSearch = components.front().component;
////
////    ResolveExpression(tree, components, *search_scope, result, caseSensitive, isPrefix);
////
////    if (s_DebugSmartSense)
////        CCLogger::Get()->DebugLog(wxString::Format(_T("AI() AI leave, returned %lu results"),static_cast<unsigned long>(result.size())));
////
////    return result.size();
////}

////// find a function where current caret located.
////// We need to find extra class scope, otherwise, we will fail to do the cc in a class declaration
////size_t ParseManager::FindCurrentFunctionToken(ccSearchData* searchData, TokenIdxSet& result, int caretPos)
////{
////    TokenIdxSet scope_result;
////    wxString procName;
////    wxString scopeName;
////    FindCurrentFunctionStart(searchData, &scopeName, &procName, nullptr, caretPos);
////
////    if (procName.IsEmpty())
////        return 0;
////
////    // add current scope
////    if (!scopeName.IsEmpty())
////    {
////        // _namespace ends with double-colon (::). remove it
////        scopeName.RemoveLast();
////        scopeName.RemoveLast();
////
////        // search for namespace
////        std::queue<ParserComponent> ns;
////        BreakUpComponents(scopeName, ns);
////
////        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////        // No critical section needed in this recursive function!
////        // All functions that call this recursive FindAIMatches function, should already entered a critical section.
////        FindAIMatches(m_Parser->GetTokenTree(), ns, scope_result, -1,
////                      true, true, false, tkNamespace | tkClass | tkTypedef);
////
////        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////    }
////
////    // if no scope, use global scope
////    if (scope_result.empty())
////        scope_result.insert(-1);
////
////    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////    for (TokenIdxSet::const_iterator tis_it = scope_result.begin(); tis_it != scope_result.end(); ++tis_it)
////    {
////        GenerateResultSet(m_Parser->GetTokenTree(), procName, *tis_it, result,
////                          true, false, tkAnyFunction | tkClass);
////    }
////
////    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////    return result.size();
////}

// returns current function's position (not line) in the editor
// ----------------------------------------------------------------------------
int ParseManager::FindCurrentFunctionStart(bool callerHasTreeLock,
                                           ccSearchData* searchData,
                                           wxString*     nameSpace,
                                           wxString*     procName,
                                           int*          functionIndex,
                                           int           caretPos )
// ----------------------------------------------------------------------------
{
    // cache last result for optimization
    int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
    if ((pos < 0) || (pos > searchData->control->GetLength()))
    {
        if (s_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format(_T("FindCurrentFunctionStart() Cannot determine position. caretPos=%d, control=%d"),
                                        caretPos, searchData->control->GetCurrentPos()));
        return -1;
    }

    TRACE(_T("ParseManager::FindCurrentFunctionStart()"));

    const int curLine = searchData->control->LineFromPosition(pos) + 1;
    if (   (curLine == m_LastLine)
        && ( (searchData->control == m_LastControl) && (!searchData->control->GetModify()) )
        && (searchData->file == m_LastFile) )
    {
        if (nameSpace)     *nameSpace     = m_LastNamespace;
        if (procName)      *procName      = m_LastPROC;
        if (functionIndex) *functionIndex = m_LastFunctionIndex;

        if (s_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format(_T("FindCurrentFunctionStart() Cached namespace='%s', cached proc='%s' (returning %d)"),
                                        m_LastNamespace.wx_str(), m_LastPROC.wx_str(), m_LastResult));

        return m_LastResult;
    }

    if (s_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format(_T("FindCurrentFunctionStart() Looking for tokens in '%s'"),
                                    searchData->file.wx_str()));
    m_LastFile    = searchData->file;
    m_LastControl = searchData->control;
    m_LastLine    = curLine;

    // we have all the tokens in the current file, then just do a loop on all
    // the tokens, see if the line is in the token's imp.
    TokenIdxSet result;
    size_t num_results = m_Parser->FindTokensInFile(callerHasTreeLock, searchData->file, result, tkAnyFunction | tkClass);
    if (s_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format(_T("FindCurrentFunctionStart() Found %lu results"), static_cast<unsigned long>(num_results)));

    TokenTree* tree = m_Parser->GetTokenTree();

    //CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
    cbAssert(callerHasTreeLock && "Caller must own TokenTree lock");

    const int idx = GetTokenFromCurrentLine(tree, result, curLine, searchData->file);
    const Token* token = tree->at(idx);
    if (token)
    {
        // got it :)
        if (s_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format(_T("FindCurrentFunctionStart() Current function: '%s' (at line %u)"),
                                        token->DisplayName().wx_str(),
                                        token->m_ImplLine));

        m_LastNamespace      = token->GetNamespace();
        m_LastPROC           = token->m_Name;
        m_LastFunctionIndex  = token->m_Index;
        m_LastResult         = searchData->control->PositionFromLine(token->m_ImplLine - 1);

        // locate function's opening brace
        if (token->m_TokenKind & tkAnyFunction)
        {
            while (m_LastResult < searchData->control->GetTextLength())
            {
                wxChar ch = searchData->control->GetCharAt(m_LastResult);
                if (ch == _T('{'))
                    break;
                else if (ch == 0)
                {
                    if (s_DebugSmartSense)
                        CCLogger::Get()->DebugLog(_T("FindCurrentFunctionStart() Can't determine functions opening brace..."));

                    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
                    return -1;
                }

                ++m_LastResult;
            }
        }

        if (nameSpace)     *nameSpace     = m_LastNamespace;
        if (procName)      *procName      = m_LastPROC;
        if (functionIndex) *functionIndex = token->m_Index;

        if (s_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format(_T("FindCurrentFunctionStart() Namespace='%s', proc='%s' (returning %d)"),
                                        m_LastNamespace.wx_str(), m_LastPROC.wx_str(), m_LastResult));

        //CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        cbAssert(callerHasTreeLock && "Caller must own TokenTree lock");
        return m_LastResult;
    }

    if (s_DebugSmartSense)
        CCLogger::Get()->DebugLog(_T("FindCurrentFunctionStart() Can't determine current function..."));

    m_LastResult = -1;
    return -1;
}

////bool ParseManager::ParseUsingNamespace(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos)
////{
////    if (s_DebugSmartSense)
////        CCLogger::Get()->DebugLog(_T("ParseUsingNamespace() Parse file scope for \"using namespace\""));
////    TRACE(_T("ParseManager::ParseUsingNamespace()"));
////
////    int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
////    if (pos < 0 || pos > searchData->control->GetLength())
////        return false;
////
////    // Get the buffer from begin of the editor to the current caret position
////    wxString buffer = searchData->control->GetTextRange(0, pos);
////
////    return ParseBufferForUsingNamespace(buffer, search_scope);
////}

////bool ParseManager::ParseBufferForUsingNamespace(const wxString& buffer, TokenIdxSet& search_scope, bool bufferSkipBlocks)
////{
////    wxArrayString ns;
////    m_Parser->ParseBufferForUsingNamespace(buffer, ns, bufferSkipBlocks);
////
////    TokenTree* tree = m_Parser->GetTokenTree();
////
////    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////    for (size_t i = 0; i < ns.GetCount(); ++i)
////    {
////        std::queue<ParserComponent> components;
////        BreakUpComponents(ns[i], components);
////
////        int parentIdx = -1;
////        while (!components.empty())
////        {
////            ParserComponent pc = components.front();
////            components.pop();
////
////            int id = tree->TokenExists(pc.component, parentIdx, tkNamespace);
////            if (id == -1)
////            {
////                parentIdx = -1;
////                break;
////            }
////            parentIdx = id;
////        }
////
////        if (s_DebugSmartSense && parentIdx != -1)
////        {
////            const Token* token = tree->at(parentIdx);
////            if (token)
////                CCLogger::Get()->DebugLog(wxString::Format(_T("ParseUsingNamespace() Found %s%s"),
////                                            token->GetNamespace().wx_str(), token->m_Name.wx_str()));
////        }
////        search_scope.insert(parentIdx);
////    }
////
////    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////    return true;
////}

////bool ParseManager::ParseFunctionArguments(ccSearchData* searchData, int caretPos)
////{
////    if (s_DebugSmartSense)
////        CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() Parse function arguments"));
////    TRACE(_T("ParseManager::ParseFunctionArguments()"));
////
////    TokenIdxSet proc_result;
////
////    TokenTree* tree = m_Parser->GetTokenTree(); // the one used inside FindCurrentFunctionToken, FindAIMatches and GenerateResultSet
////
////    size_t found_at = FindCurrentFunctionToken(searchData, proc_result, caretPos);
////    if (!found_at)
////    {
////        if (s_DebugSmartSense)
////            CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() Could not determine current function's namespace..."));
////        TRACE(_T("ParseFunctionArguments() Could not determine current function's namespace..."));
////        return false;
////    }
////
////    const int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
////    const unsigned int curLine = searchData->control->LineFromPosition(pos) + 1;
////
////    bool locked = false;
////    for (TokenIdxSet::const_iterator tis_it = proc_result.begin(); tis_it != proc_result.end(); ++tis_it)
////    {
////        wxString buffer;
////        int initLine = -1;
////        int tokenIdx = -1;
////
////        if (locked)
////            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////        locked = true;
////
////        const Token* token = tree->at(*tis_it);
////
////        if (!token)
////            continue;
////        if (curLine < token->m_ImplLineStart || curLine > token->m_ImplLineEnd)
////            continue;
////
////        if (s_DebugSmartSense)
////            CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() + Function match: ") + token->m_Name);
////        TRACE(_T("ParseFunctionArguments() + Function match: ") + token->m_Name);
////
////        if (!token->m_Args.IsEmpty() && !token->m_Args.Matches(_T("()")))
////        {
////            buffer = token->m_Args;
////            // Now we have something like "(int my_int, const TheClass* my_class, float f)"
////            buffer.Remove(0, 1);              // remove (
////            buffer.RemoveLast();              // remove )
////            // Now we have                "int my_int, const TheClass* my_class, float f"
////            buffer.Replace(_T(","), _T(";")); // replace commas with semi-colons
////            // Now we have                "int my_int; const TheClass* my_class; float f"
////            buffer << _T(';');                // aid parser ;)
////            // Finally we have            "int my_int; const TheClass* my_class; float f;"
////            buffer.Trim();
////
////            if (s_DebugSmartSense)
////                CCLogger::Get()->DebugLog(wxString::Format(_T("ParseFunctionArguments() Parsing arguments: \"%s\""), buffer.wx_str()));
////
////            if (!buffer.IsEmpty())
////            {
////                const int textLength= searchData->control->GetLength();
////                if (textLength == -1)
////                    continue;
////                int paraPos = searchData->control->PositionFromLine(token->m_ImplLine - 1);
////                if (paraPos == -1)
////                    continue;
////                while (paraPos < textLength && searchData->control->GetCharAt(paraPos++) != _T('('))
////                    ;
////                while (paraPos < textLength && searchData->control->GetCharAt(paraPos++) < _T(' '))
////                    ;
////                initLine = searchData->control->LineFromPosition(paraPos) + 1;
////                if (initLine == -1)
////                    continue;
////                tokenIdx = token->m_Index;
////            }
////        }
////
////        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////        locked = false;
////
////        if (   !buffer.IsEmpty()
////            && !m_Parser->ParseBuffer(buffer, false, false, true, searchData->file, tokenIdx, initLine)
////            && s_DebugSmartSense)
////        {
////            CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() Error parsing arguments."));
////        }
////    }
////
////    if (locked)
////        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////    return true;
////}

////bool ParseManager::ParseLocalBlock(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos)
////{
////    if (s_DebugSmartSense)
////        CCLogger::Get()->DebugLog(_T("ParseLocalBlock() Parse local block"));
////    TRACE(_T("ParseManager::ParseLocalBlock()"));
////
////    int parentIdx = -1;
////    int blockStart = FindCurrentFunctionStart(searchData, nullptr, nullptr, &parentIdx, caretPos);
////    int initLine = 0;
////    if (parentIdx != -1)
////    {
////        TokenTree* tree = m_Parser->GetTokenTree();
////
////        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////        const Token* parent = tree->at(parentIdx);
////        if (parent && (parent->m_TokenKind & tkAnyFunction))
////        {
////            m_LastFuncTokenIdx = parent->m_Index;
////            initLine = parent->m_ImplLineStart;
////        }
////
////        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////
////        // only need to parse the function body, other type of Tokens' body such as class declaration
////        // should not be parsed.
////        if (!parent || !(parent->m_TokenKind & tkAnyFunction))
////            return false;
////    }
////
////    if (blockStart != -1)
////    {
////        cbStyledTextCtrl* stc = searchData->control;
////        // if we are in a function body, then blockStart points to the '{', so we just skip the '{'.
////        if (stc->GetCharAt(blockStart) == wxT('{'))
////            ++blockStart;
////        const int pos         = (caretPos == -1 ? stc->GetCurrentPos() : caretPos);
////        const int line        = stc->LineFromPosition(pos);
////        const int blockEnd    = stc->GetLineEndPosition(line);
////        if (blockEnd < 0 || blockEnd > stc->GetLength())
////        {
////            if (s_DebugSmartSense)
////            {
////                CCLogger::Get()->DebugLog(wxString::Format(_T("ParseLocalBlock() ERROR blockEnd=%d and edLength=%d?!"),
////                                            blockEnd, stc->GetLength()));
////            }
////            return false;
////        }
////
////        if (blockStart >= blockEnd)
////            blockStart = blockEnd;
////
//////        wxString buffer = searchData->control->GetTextRange(blockStart, blockEnd);
////        wxString buffer;
////        // condense out-of-scope braces {...}
////        int scanPos = blockEnd;
////        for (int curPos = pos; curPos > blockStart; --curPos)
////        {
////            if (stc->GetCharAt(curPos) != wxT('}'))
////                continue;
////            const int style = stc->GetStyleAt(curPos);
////            if (   stc->IsString(style)
////                || stc->IsCharacter(style)
////                || stc->IsComment(style))
////            {
////                continue;
////            }
////            const int scopeStart = stc->BraceMatch(curPos);
////            if (scopeStart < blockStart)
////                break;
////            buffer.Prepend(stc->GetTextRange(curPos, scanPos));
////            int startLn = stc->LineFromPosition(scopeStart);
////            int endLn   = stc->LineFromPosition(curPos);
////            if (startLn < endLn) // maintain correct line numbers for parsed tokens
////                buffer.Prepend( wxString(wxT('\n'), endLn - startLn) );
////            scanPos = scopeStart + 1;
////            curPos  = scopeStart;
////
////            // condense out-of-scope for/if/while declarations
////            int prevCharIdx = scopeStart - 1;
////            for (; prevCharIdx > blockStart; --prevCharIdx)
////            {
////                if (stc->IsComment(stc->GetStyleAt(prevCharIdx)))
////                    continue;
////                if (!wxIsspace(stc->GetCharAt(prevCharIdx)))
////                    break;
////            }
////            if (stc->GetCharAt(prevCharIdx) != wxT(')'))
////                continue;
////            const int paramStart = stc->BraceMatch(prevCharIdx);
////            if (paramStart < blockStart)
////                continue;
////            for (prevCharIdx = paramStart - 1; prevCharIdx > blockStart; --prevCharIdx)
////            {
////                if (stc->IsComment(stc->GetStyleAt(prevCharIdx)))
////                    continue;
////                if (!wxIsspace(stc->GetCharAt(prevCharIdx)))
////                    break;
////            }
////            const wxString text = stc->GetTextRange(stc->WordStartPosition(prevCharIdx, true),
////                                                    stc->WordEndPosition(  prevCharIdx, true));
////            if (text == wxT("for"))
////                buffer.Prepend(wxT("(;;){"));
////            else if (text == wxT("if") || text == wxT("while") || text == wxT("catch"))
////                buffer.Prepend(wxT("(0){"));
////            else
////                continue;
////            startLn = stc->LineFromPosition(prevCharIdx);
////            endLn   = stc->LineFromPosition(scopeStart);
////            if (startLn < endLn)
////                buffer.Prepend( wxString(wxT('\n'), endLn - startLn) );
////            curPos  = stc->WordStartPosition(prevCharIdx, true);
////            scanPos = stc->WordEndPosition(  prevCharIdx, true);
////        }
////        buffer.Prepend(stc->GetTextRange(blockStart, scanPos));
////
////        buffer.Trim();
////
////        ParseBufferForUsingNamespace(buffer, search_scope, false);
////
////        if (   !buffer.IsEmpty()
////            && !m_Parser->ParseBuffer(buffer, false, false, true, searchData->file, m_LastFuncTokenIdx, initLine) )
////        {
////            if (s_DebugSmartSense)
////                CCLogger::Get()->DebugLog(_T("ParseLocalBlock() ERROR parsing block:\n") + buffer);
////        }
////        else
////        {
////            if (s_DebugSmartSense)
////            {
////                CCLogger::Get()->DebugLog(wxString::Format(_T("ParseLocalBlock() Block:\n%s"), buffer.wx_str()));
////                CCLogger::Get()->DebugLog(_T("ParseLocalBlock() Local tokens:"));
////
////                TokenTree* tree = m_Parser->GetTokenTree();
////
////                CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
////
////                for (size_t i = 0; i < tree->size(); ++i)
////                {
////                    const Token* token = tree->at(i);
////                    if (token && token->m_IsTemp)
////                    {
////                        wxString log(wxString::Format(_T(" + %s (%d)"), token->DisplayName().wx_str(), token->m_Index));
////                        const Token* parent = tree->at(token->m_ParentIndex);
////                        if (parent)
////                            log += wxString::Format(_T("; Parent = %s (%d)"), parent->m_Name.wx_str(), token->m_ParentIndex);
////                        CCLogger::Get()->DebugLog(log);
////                    }
////                }
////
////                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
////            }
////            return true;
////        }
////    }
////    else
////    {
////        if (s_DebugSmartSense)
////            CCLogger::Get()->DebugLog(_T("ParseLocalBlock() Could not determine current block start..."));
////    }
////    return false;
////}
// ----------------------------------------------------------------------------
bool ParseManager::AddCompilerAndIncludeDirs(cbProject* project, ParserBase* parser)
// ----------------------------------------------------------------------------
{
    if (!parser)
        return false;

    TRACE(_T("ParseManager::AddCompilerDirs: Enter"));

    // If there is no project, work on default compiler
    if (not project)
    {
        AddCompilerIncludeDirsToParser(CompilerFactory::GetDefaultCompiler(), parser);
        TRACE(_T("ParseManager::AddCompilerDirs: Leave"));
        return true;
    }

    // Otherwise (if there is a project), work on the project's compiler...
    wxString base = project->GetBasePath();
    parser->AddIncludeDir(base); // add project's base path
    TRACE(_T("ParseManager::AddCompilerDirs: Adding project base dir to parser: ") + base);

    // ...so we can access post-processed project's search dirs
    Compiler* compiler = CompilerFactory::GetCompiler(project->GetCompilerID());
    cb::shared_ptr<CompilerCommandGenerator> generator(compiler ? compiler->GetCommandGenerator(project) : nullptr);

    // get project include search dirs
    if (   !parser->Options().platformCheck
        || (parser->Options().platformCheck && project->SupportsCurrentPlatform()) )
    {
        AddIncludeDirsToParser(project->GetIncludeDirs(), base, parser);
    }

    // alloc array for project compiler AND "no. of targets" times target compilers
    int nCompilers = 1 + project->GetBuildTargetsCount();
    Compiler** Compilers = new Compiler* [nCompilers];
    // Changed sizeof(Compiler*) to sizeof(Compiler) to avoid warning:Suspicious usage of 'sizeof(A*)'; pointer to aggregate
    //-memset(Compilers, 0, sizeof(Compiler*) * nCompilers);
    memset(Compilers, 0, sizeof(Compilers) * nCompilers);
    nCompilers = 0; // reset , use as insert index in the next for loop

    // get targets include dirs
    for (int i = 0; i < project->GetBuildTargetsCount(); ++i)
    {
        ProjectBuildTarget* target = project->GetBuildTarget(i);
        if (!target) continue;

        if (   !parser->Options().platformCheck
            || (parser->Options().platformCheck && target->SupportsCurrentPlatform()) )
        {
            // post-processed search dirs (from build scripts)
            if (compiler && generator)
                AddIncludeDirsToParser(generator->GetCompilerSearchDirs(target), base, parser);

            // apply target vars
            AddIncludeDirsToParser(target->GetIncludeDirs(), base, parser);

            // get the compiler
            wxString CompilerIndex = target->GetCompilerID();
            Compiler* tgtCompiler = CompilerFactory::GetCompiler(CompilerIndex);
            if (tgtCompiler)
            {
                Compilers[nCompilers] = tgtCompiler;
                ++nCompilers;
            }
        } // if (target)
    } // end loop over the targets

    // add the project compiler to the array of compilers
    if (compiler)
    {   // note it might be possible that this compiler is already in the list
        // no need to worry since the compiler list of the parser will filter out duplicate
        // entries in the include dir list
        Compilers[nCompilers++] = compiler;
    }

    // add compiler include dirs
    for (int idxCompiler = 0; idxCompiler < nCompilers; ++idxCompiler)
        AddCompilerIncludeDirsToParser(Compilers[idxCompiler], parser);

    if (!nCompilers)
        CCLogger::Get()->DebugLog(_T("ParseManager::AddCompilerDirs: No compilers found!"));

    delete [] Compilers;
    TRACE(_T("ParseManager::AddCompilerDirs: Leave"));
    return true;
}
////bool ParseManager::AddCompilerDirs(cbProject* project, ParserBase* parser)
////{
////    if (!parser)
////        return false;
////
////    TRACE(_T("ParseManager::AddCompilerDirs: Enter"));
////
////    // If there is no project, work on default compiler
////    if (!project)
////    {
////        AddCompilerIncludeDirsToParser(CompilerFactory::GetDefaultCompiler(), parser);
////        TRACE(_T("ParseManager::AddCompilerDirs: Leave"));
////        return true;
////    }
////
////    // Otherwise (if there is a project), work on the project's compiler...
////    wxString base = project->GetBasePath();
////    parser->AddIncludeDir(base); // add project's base path
////    TRACE(_T("ParseManager::AddCompilerDirs: Adding project base dir to parser: ") + base);
////
////    // ...so we can access post-processed project's search dirs
////    Compiler* compiler = CompilerFactory::GetCompiler(project->GetCompilerID());
////    cb::shared_ptr<CompilerCommandGenerator> generator(compiler ? compiler->GetCommandGenerator(project) : nullptr);
////
////    // get project include search dirs
////    if (   !parser->Options().platformCheck
////        || (parser->Options().platformCheck && project->SupportsCurrentPlatform()) )
////    {
////        AddIncludeDirsToParser(project->GetIncludeDirs(), base, parser);
////    }
////
////    // alloc array for project compiler AND "no. of targets" times target compilers
////    int nCompilers = 1 + project->GetBuildTargetsCount();
////    Compiler** Compilers = new Compiler* [nCompilers];
////    memset(Compilers, 0, sizeof(Compiler*) * nCompilers);
////    nCompilers = 0; // reset , use as insert index in the next for loop
////
////    // get targets include dirs
////    for (int i = 0; i < project->GetBuildTargetsCount(); ++i)
////    {
////        ProjectBuildTarget* target = project->GetBuildTarget(i);
////        if (!target) continue;
////
////        if (   !parser->Options().platformCheck
////            || (parser->Options().platformCheck && target->SupportsCurrentPlatform()) )
////        {
////            // post-processed search dirs (from build scripts)
////            if (compiler && generator)
////                AddIncludeDirsToParser(generator->GetCompilerSearchDirs(target), base, parser);
////
////            // apply target vars
//////            target->GetCustomVars().ApplyVarsToEnvironment();
////            AddIncludeDirsToParser(target->GetIncludeDirs(), base, parser);
////
////            // get the compiler
////            wxString CompilerIndex = target->GetCompilerID();
////            Compiler* tgtCompiler = CompilerFactory::GetCompiler(CompilerIndex);
////            if (tgtCompiler)
////            {
////                Compilers[nCompilers] = tgtCompiler;
////                ++nCompilers;
////            }
////        } // if (target)
////    } // end loop over the targets
////
////    // add the project compiler to the array of compilers
////    if (compiler)
////    {   // note it might be possible that this compiler is already in the list
////        // no need to worry since the compiler list of the parser will filter out duplicate
////        // entries in the include dir list
////        Compilers[nCompilers++] = compiler;
////    }
////
////    // add compiler include dirs
////    for (int idxCompiler = 0; idxCompiler < nCompilers; ++idxCompiler)
////        AddCompilerIncludeDirsToParser(Compilers[idxCompiler], parser);
////
////    if (!nCompilers)
////        CCLogger::Get()->DebugLog(_T("ParseManager::AddCompilerDirs: No compilers found!"));
////
////    delete [] Compilers;
////    TRACE(_T("ParseManager::AddCompilerDirs: Leave"));
////    return true;
////}

////bool ParseManager::AddCompilerPredefinedMacros(cbProject* project, ParserBase* parser)
////{
////    if (!parser)
////        return false;
////
////    if (!parser->Options().wantPreprocessor)
////        return false;
////
////    TRACE(_T("ParseManager::AddCompilerPredefinedMacros: Enter"));
////
////    // Default compiler is used for for single file parser (non project)
////    wxString compilerId = project ? project->GetCompilerID() : CompilerFactory::GetDefaultCompilerID();
////
////    wxString defs;
////    // gcc
////    if (compilerId.Contains(_T("gcc")))
////    {
////        if ( !AddCompilerPredefinedMacrosGCC(compilerId, project, defs, parser) )
////            return false;
////    }
////    // vc
////    else if (compilerId.StartsWith(_T("msvc")))
////    {
////        if ( !AddCompilerPredefinedMacrosVC(compilerId, defs, parser) )
////          return false;
////    }
////
////    TRACE(_T("ParseManager::AddCompilerPredefinedMacros: Add compiler predefined preprocessor macros:\n%s"), defs.wx_str());
////    parser->AddPredefinedMacros(defs);
////
////    TRACE(_T("ParseManager::AddCompilerPredefinedMacros: Leave"));
////    if ( defs.IsEmpty() )
////        return false;
////
////    return true;
////}

////bool ParseManager::AddCompilerPredefinedMacrosGCC(const wxString& compilerId, cbProject* project, wxString& defs, ParserBase* parser)
////{
////    Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
////    if (!compiler)
////        return false;
////
////    if (parser->Options().platformCheck && !compiler->SupportsCurrentPlatform())
////    {
////        TRACE(_T("ParseManager::AddCompilerPredefinedMacrosGCC: Not supported on current platform!"));
////        return false;
////    }
////
////    wxString sep = (platform::windows ? _T("\\") : _T("/"));
////    wxString cpp_compiler = compiler->GetMasterPath() + sep + _T("bin") + sep + compiler->GetPrograms().CPP;
////    Manager::Get()->GetMacrosManager()->ReplaceMacros(cpp_compiler);
////
////    static std::map<wxString, wxString> gccDefsMap;
////    if ( gccDefsMap[cpp_compiler].IsEmpty() )
////    {
////        // Check if user set language standard version to use
////        wxString standard = GetCompilerStandardGCC(compiler, project);
////
////        // Different command on Windows and other OSes
////#ifdef __WXMSW__
////        const wxString args(wxString::Format(_T(" -E -dM -x c++ %s nul"), standard.wx_str()) );
////#else
////        const wxString args(wxString::Format(_T(" -E -dM -x c++ %s /dev/null"), standard.wx_str()) );
////#endif
////
////        wxArrayString output, error;
////        if ( !SafeExecute(compiler->GetMasterPath(), compiler->GetPrograms().CPP, args, output, error) )
////            return false;
////
////        // wxExecute can be a long action and C::B might have been shutdown in the meantime...
////        if ( Manager::IsAppShuttingDown() )
////            return false;
////
////        wxString& gccDefs = gccDefsMap[cpp_compiler];
////        for (size_t i = 0; i < output.Count(); ++i)
////            gccDefs += output[i] + _T("\n");
////
////        CCLogger::Get()->DebugLog(_T("ParseManager::AddCompilerPredefinedMacrosGCC: Caching predefined macros for compiler '")
////                                  + cpp_compiler + _T("':\n") + gccDefs);
////    }
////
////    defs = gccDefsMap[cpp_compiler];
////
////    return true;
////}

////wxString ParseManager::GetCompilerStandardGCC(Compiler* compiler, cbProject* project)
////{
////    // Check if user set language standard version to use
////    // 1.) Global compiler settings are first to search in
////    wxString standard = GetCompilerUsingStandardGCC(compiler->GetCompilerOptions());
////    if (standard.IsEmpty() && project)
////    {
////        // 2.) Project compiler setting are second
////        standard = GetCompilerUsingStandardGCC(project->GetCompilerOptions());
////
////        // 3.) And targets are third in row to look for standard
////        // NOTE: If two targets use different standards, only the one we
////        //       encounter first (eg. c++98) will be used, and any other
////        //       disregarded (even if it would be c++1y)
////        if (standard.IsEmpty())
////        {
////            for (int i=0; i<project->GetBuildTargetsCount(); ++i)
////            {
////                ProjectBuildTarget* target = project->GetBuildTarget(i);
////                standard = GetCompilerUsingStandardGCC(target->GetCompilerOptions());
////
////                if (!standard.IsEmpty())
////                    break;
////            }
////        }
////    }
////    return standard;
////}

////wxString ParseManager::GetCompilerUsingStandardGCC(const wxArrayString& compilerOptions)
////{
////    wxString standard;
////    for (wxArrayString::size_type i=0; i<compilerOptions.Count(); ++i)
////    {
////        if (compilerOptions[i].StartsWith(_T("-std=")))
////        {
////            standard = compilerOptions[i];
////            CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::GetCompilerUsingStandardGCC: Using language standard: %s"), standard.wx_str()));
////            break;
////        }
////    }
////    return standard;
////}

////bool ParseManager::AddCompilerPredefinedMacrosVC(const wxString& compilerId, wxString& defs, ParserBase* parser)
////{
////    static wxString vcDefs;
////    static bool     firstExecute = true;
////
////    if (!firstExecute)
////    {
////        defs = vcDefs;
////        return true;
////    }
////
////    firstExecute = false;
////    Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
////    if (!compiler)
////        return false;
////
////    if (parser->Options().platformCheck && !compiler->SupportsCurrentPlatform())
////    {
////        TRACE(_T("ParseManager::AddCompilerPredefinedMacrosVC: Not supported on current platform!"));
////        return false;
////    }
////
////    wxArrayString output, error;
////    if ( !SafeExecute(compiler->GetMasterPath(), compiler->GetPrograms().C, wxEmptyString, output, error) )
////        return false;
////
////    // wxExecute can be a long action and C::B might have been shutdown in the meantime...
////    if ( Manager::IsAppShuttingDown() )
////        return false;
////
////    if (error.IsEmpty())
////    {
////        TRACE(_T("ParseManager::AddCompilerPredefinedMacrosVC: Can't get pre-defined macros for MSVC."));
////        return false;
////    }
////
////    wxString compilerVersionInfo = error[0];
////    wxString tmp(_T("Microsoft (R) "));
////    int pos = compilerVersionInfo.Find(tmp);
////    if (pos != wxNOT_FOUND)
////    {
////        // in earlier versions of MSVC the compiler shows "32 bit" or "64 bit"
////        // in more recent MSVC version the architecture (x86 or x64) is shown instead
////        wxString bit = compilerVersionInfo.Mid(pos + tmp.Length(), 2);
////        if      ( (bit.IsSameAs(_T("32"))) || compilerVersionInfo.Contains(_T("x86")) )
////            defs += _T("#define _WIN32") _T("\n");
////        else if ( (bit.IsSameAs(_T("64"))) || compilerVersionInfo.Contains(_T("x64")) )
////            defs += _T("#define _WIN64") _T("\n");
////    }
////
////    tmp = _T("Compiler Version ");
////    pos = compilerVersionInfo.Find(tmp);
////    if (pos != wxNOT_FOUND)
////    {
////        wxString ver = compilerVersionInfo.Mid(pos + tmp.Length(), 4); // is i.e. 12.0
////        pos = ver.Find(_T('.'));
////        if (pos != wxNOT_FOUND)
////        {
////            // out of "12.0" make "1200" for the #define
////            ver[pos]     = ver[pos + 1]; // move the mintor version first number to the dot position
////            ver[pos + 1] = _T('0');      // add another zero at the end
////            defs += _T("#define _MSC_VER ") + ver;
////            // Known to now (see https://en.wikipedia.org/wiki/Visual_C%2B%2B):
////            // MSVC++ 12.0 _MSC_VER = 1800 (Visual Studio 2013)
////            // MSVC++ 11.0 _MSC_VER = 1700 (Visual Studio 2012)
////            // MSVC++ 10.0 _MSC_VER = 1600 (Visual Studio 2010)
////            // MSVC++ 9.0  _MSC_VER = 1500 (Visual Studio 2008)
////            // MSVC++ 8.0  _MSC_VER = 1400 (Visual Studio 2005)
////            // MSVC++ 7.1  _MSC_VER = 1310 (Visual Studio 2003)
////            // MSVC++ 7.0  _MSC_VER = 1300
////            // MSVC++ 6.0  _MSC_VER = 1200
////            // MSVC++ 5.0  _MSC_VER = 1100
////        }
////    }
////
////    defs = vcDefs;
////    return true;
////}

////bool ParseManager::AddProjectDefinedMacros(cbProject* project, ParserBase* parser)
////{
////    if (!parser)
////        return false;
////
////    if (!project)
////        return true;
////
////    TRACE(_T("ParseManager::AddProjectDefinedMacros: Enter"));
////
////    wxString compilerId = project->GetCompilerID();
////    wxString defineCompilerSwitch(wxEmptyString);
////    if (compilerId.Contains(_T("gcc")))
////        defineCompilerSwitch = _T("-D");
////    else if (compilerId.StartsWith(_T("msvc")))
////        defineCompilerSwitch = _T("/D");
////
////    if (defineCompilerSwitch.IsEmpty())
////        return false; // no compiler options, return false
////
////    wxString defs;
////    wxArrayString opts;
////    if (   !parser->Options().platformCheck
////        || (parser->Options().platformCheck && project->SupportsCurrentPlatform()) )
////    {
////        opts = project->GetCompilerOptions();
////    }
////
////    ProjectBuildTarget* target = project->GetBuildTarget(project->GetActiveBuildTarget());
////    if (target != NULL)
////    {
////        if (   !parser->Options().platformCheck
////            || (parser->Options().platformCheck && target->SupportsCurrentPlatform()) )
////        {
////            wxArrayString targetOpts = target->GetCompilerOptions();
////            for (size_t i = 0; i < targetOpts.GetCount(); ++i)
////                opts.Add(targetOpts[i]);
////        }
////    }
////    // In case of virtual targets, collect the defines from all child targets.
////    wxArrayString targets = project->GetExpandedVirtualBuildTargetGroup(project->GetActiveBuildTarget());
////    for (size_t i = 0; i < targets.GetCount(); ++i)
////    {
////        target = project->GetBuildTarget(targets[i]);
////        if (target != NULL)
////        {
////            if (   !parser->Options().platformCheck
////                || (parser->Options().platformCheck && target->SupportsCurrentPlatform()) )
////            {
////                wxArrayString targetOpts = target->GetCompilerOptions();
////                for (size_t j = 0; j < targetOpts.GetCount(); ++j)
////                    opts.Add(targetOpts[j]);
////            }
////        }
////    }
////
////    for (size_t i = 0; i < opts.GetCount(); ++i)
////    {
////        wxString def = opts[i];
////        Manager::Get()->GetMacrosManager()->ReplaceMacros(def);
////        if ( !def.StartsWith(defineCompilerSwitch) )
////            continue;
////
////        def = def.Right(def.Length() - defineCompilerSwitch.Length());
////        int pos = def.Find(_T('='));
////        if (pos != wxNOT_FOUND)
////            def[pos] = _T(' ');
////
////        defs += _T("#define ") + def + _T("\n");
////    }
////
////    TRACE(_T("Add project and current build target defined preprocessor macros:\n%s"), defs.wx_str());
////    parser->AddPredefinedMacros(defs);
////    TRACE(_T("ParseManager::AddProjectDefinedMacros: Leave"));
////    if ( defs.IsEmpty() )
////        return false;
////
////    return true;
////}

// ----------------------------------------------------------------------------
void ParseManager::AddCompilerIncludeDirsToParser(const Compiler* compiler, ParserBase* parser)
// ----------------------------------------------------------------------------
{
    if (!compiler || !parser) return;

    if (   !parser->Options().platformCheck
        || (parser->Options().platformCheck && compiler->SupportsCurrentPlatform()) )
    {
        // these dirs were the user's compiler include search dirs
        AddIncludeDirsToParser(compiler->GetIncludeDirs(), wxEmptyString, parser);

        // find out which compiler, if gnu, do the special trick
        // to find it's internal include paths
        // but do only once per C::B session, thus cache for later calls
        if (compiler->GetID().Contains(_T("gcc")))
            AddGCCCompilerDirs(compiler->GetMasterPath(), compiler->GetPrograms().CPP, parser);
    }
}

// These dirs are the built-in search dirs of the compiler itself (GCC).
// Such as when you install your MinGW GCC in E:/code/MinGW/bin
// The built-in search dir may contain: E:/code/MinGW/include
// ----------------------------------------------------------------------------
const wxArrayString& ParseManager::GetGCCCompilerDirs(const wxString& cpp_path, const wxString& cpp_executable)
// ----------------------------------------------------------------------------
{
    wxString sep = (platform::windows ? _T("\\") : _T("/"));
    wxString cpp_compiler = cpp_path + sep + _T("bin") + sep + cpp_executable;
    Manager::Get()->GetMacrosManager()->ReplaceMacros(cpp_compiler);

    // keep the gcc compiler path's once if found across C::B session
    // makes opening workspaces a *lot* faster by avoiding endless calls to the compiler
    static std::map<wxString, wxArrayString> dirs;
    static wxArrayString cached_result; // avoid accessing "dirs" too often (re-entry)
    cached_result = dirs[cpp_compiler];
    if ( !cached_result.IsEmpty() )
        return cached_result;

    TRACE(_T("ParseManager::GetGCCCompilerDirs: Enter"));

    // for starters, only do this for gnu compiler
    //CCLogger::Get()->DebugLog(_T("CompilerID ") + CompilerID);
    //
    //   Windows: mingw32-g++ -v -E -x c++ nul
    //   Linux  : g++ -v -E -x c++ /dev/null
    // do the trick only for c++, not needed then for C (since this is a subset of C++)

    // Different command on Windows and other OSes
#ifdef __WXMSW__
    const wxString args(_T(" -v -E -x c++ nul"));
#else
    const wxString args(_T(" -v -E -x c++ /dev/null"));
#endif

    wxArrayString output, error;
    if ( not SafeExecute(cpp_path, cpp_executable, args, output, error) )
        return cached_result;

    // wxExecute can be a long action and C::B might have been shutdown in the meantime...
    if ( Manager::IsAppShuttingDown() )
        return cached_result;

    // start from "#include <...>", and the path followed
    // let's hope this does not change too quickly, otherwise we need
    // to adjust our search code (for several versions ...)
    bool start = false;
    for (size_t idxCount = 0; idxCount < error.GetCount(); ++idxCount)
    {
        wxString path = error[idxCount].Trim(true).Trim(false);
        if (!start)
        {
            if (!path.StartsWith(_T("#include <...>")))
                continue; // Next for-loop
            path = error[++idxCount].Trim(true).Trim(false);
            start = true;
        }

        wxFileName fname(path, wxString());
        //-fname.Normalize(); //Ticket #56 deprecated for wx3.2.0
        fname.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG | wxPATH_NORM_SHORTCUT);

        fname.SetVolume(fname.GetVolume().MakeUpper());
        if (not fname.DirExists())
            break;

        dirs[cpp_compiler].Add(fname.GetPath());

        CCLogger::Get()->DebugLog(_T("ParseManager::GetGCCCompilerDirs: Caching GCC default include dir: ") + fname.GetPath());
    }

    TRACE(_T("ParseManager::GetGCCCompilerDirs: Leave"));
    return dirs[cpp_compiler];
}

void ParseManager::AddGCCCompilerDirs(const wxString& masterPath, const wxString& compilerCpp, ParserBase* parser)
{
    const wxArrayString& gccDirs = GetGCCCompilerDirs(masterPath, compilerCpp);
    TRACE(_T("ParseManager::AddGCCCompilerDirs: Adding %lu cached gcc dirs to parser..."), static_cast<unsigned long>(gccDirs.GetCount()));
    for (size_t i=0; i<gccDirs.GetCount(); ++i)
    {
        parser->AddIncludeDir(gccDirs[i]);
        TRACE(_T("ParseManager::AddGCCCompilerDirs: Adding cached compiler dir to parser: ") + gccDirs[i]);
    }
}

void ParseManager::AddIncludeDirsToParser(const wxArrayString& dirs, const wxString& base, ParserBase* parser)
{
    for (unsigned int i = 0; i < dirs.GetCount(); ++i)
    {
        wxString dir = dirs[i];
        Manager::Get()->GetMacrosManager()->ReplaceMacros(dir);
        if ( not base.IsEmpty() )
        {
            wxFileName fn(dir);
            if ( NormalizePath(fn, base) )
            {
                parser->AddIncludeDir(fn.GetFullPath());
                TRACE(_T("ParseManager::AddIncludeDirsToParser: Adding directory to parser: ") + fn.GetFullPath());
            }
            else
                CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::AddIncludeDirsToParser: Error normalizing path: '%s' from '%s'"), dir.wx_str(), base.wx_str()));
        }
        else
            parser->AddIncludeDir(dir); // no base path, nothing to normalise
    }
}

// ----------------------------------------------------------------------------
bool ParseManager::SafeExecute(const wxString& app_path, const wxString& app, const wxString& args, wxArrayString& output, wxArrayString& error)
// ----------------------------------------------------------------------------
{
    wxString sep = (platform::windows ? _T("\\") : _T("/"));
    wxString pth = (app_path.IsEmpty() ? _T("") : (app_path + sep + _T("bin") + sep));
    Manager::Get()->GetMacrosManager()->ReplaceMacros(pth);
    wxString cmd = pth + app;
    Manager::Get()->GetMacrosManager()->ReplaceMacros(cmd);
//    CCLogger::Get()->DebugLog(_T("ParseManager::SafeExecute: Application command: ") + cmd + _T(", path (in): ") + app_path + _T(", path (set): ") + pth + _T(", args: ") + args);

    if ( !wxFileExists(cmd) )
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::SafeExecute: Invalid application command: ") + cmd);
        return false;
    }

    static bool reentry = false;
    if (reentry)
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::SafeExecute: Re-Entry protection."));
        return false;
    }
    reentry = true;

    // Update PATH environment variable
    wxString path_env;
    if ( !pth.IsEmpty() && wxGetEnv(_T("PATH"), &path_env) )
    {
        wxString tmp_path_env = pth + (platform::windows ? _T(";") : _T(":")) + path_env;
        if ( !wxSetEnv(_T("PATH"), tmp_path_env) )
        {   CCLogger::Get()->DebugLog(_T("ParseManager::SafeExecute: Could not set PATH environment variable: ") + tmp_path_env); }
    }

    if ( wxExecute(cmd + args, output, error, wxEXEC_SYNC | wxEXEC_NODISABLE) == -1 )
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::SafeExecute: Failed application call: ") + cmd + args);
        reentry = false;
        return false;
    }

    if ( !pth.IsEmpty() && !wxSetEnv(_T("PATH"), path_env) )
    {   CCLogger::Get()->DebugLog(_T("ParseManager::SafeExecute: Could not restore PATH environment variable: ") + path_env); }

    reentry = false;

    return true;
}
////// ----------------------------------------------------------------------------
////void ParseManager::OnParserStart(wxCommandEvent& event)
////// ----------------------------------------------------------------------------
////{
////    TRACE(_T("ParseManager::OnParserStart: Enter"));
////
////    cbProject* project = static_cast<cbProject*>(event.GetClientData());
////    wxString   prj     = (project ? project->GetTitle() : _T("*NONE*"));
////    const ParserCommon::ParserState state = static_cast<ParserCommon::ParserState>(event.GetInt());
////
////    switch (state)
////    {
////        case ParserCommon::ptCreateParser:
////            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Starting batch parsing for project '%s'..."), prj.wx_str()));
////            {
////                std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
////                if (info.second && m_Parser != info.second)
////                {
////                    CCLogger::Get()->DebugLog(_T("ParseManager::OnParserStart: Start switch from OnParserStart::ptCreateParser"));
////                    SwitchParser(info.first, info.second); // Calls SetParser() which also calls UpdateClassBrowserView()
////                }
////            }
////            break;
////
////        case ParserCommon::ptAddFileToParser:
////            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Starting add file parsing for project '%s'..."), prj.wx_str()));
////            break;
////
////        case ParserCommon::ptReparseFile:
////            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Starting re-parsing for project '%s'..."), prj.wx_str()));
////            break;
////
////        case ParserCommon::ptUndefined:
////            if (event.GetString().IsEmpty())
////                CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Batch parsing error in project '%s'"), prj.wx_str()));
////            else
////                CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: %s in project '%s'"), event.GetString().wx_str(), prj.wx_str()));
////            return;
////
////        default:
////            break;
////    }
////
////    event.Skip();
////
////    TRACE(_T("ParseManager::OnParserStart: Leave"));
////}

////// ----------------------------------------------------------------------------
////void ParseManager::OnParserEnd(wxCommandEvent& event)
////// ----------------------------------------------------------------------------
////{
////    TRACE(_T("ParseManager::OnParserEnd: Enter"));
////
////    ParserBase* parser = reinterpret_cast<ParserBase*>(event.GetEventObject());
////    cbProject* project = static_cast<cbProject*>(event.GetClientData());
////    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
////    const ParserCommon::ParserState state = static_cast<ParserCommon::ParserState>(event.GetInt());
////
////    switch (state)
////    {
////        case ParserCommon::ptCreateParser:
////            {
////                wxString log(wxString::Format(_("ParseManager::OnParserEnd: Project '%s' parsing stage done!"), prj.wx_str()));
////                CCLogger::Get()->Log(log);
////                CCLogger::Get()->DebugLog(log);
////            }
////            break;
////
////        case ParserCommon::ptAddFileToParser:
////            break;
////
////        case ParserCommon::ptReparseFile:
////            if (parser != m_Parser)
////            {
////                std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
////                if (info.second && info.second != m_Parser)
////                {
////                    CCLogger::Get()->DebugLog(_T("ParseManager::OnParserEnd: Start switch from OnParserEnd::ptReparseFile"));
////                    SwitchParser(info.first, info.second); // Calls SetParser() which also calls UpdateClassBrowserView()
////                }
////            }
////            break;
////
////        case ParserCommon::ptUndefined:
////            CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::OnParserEnd: Parser event handling error of project '%s'"), prj.wx_str()));
////            return;
////
////        default:
////            break;
////    }
////
////    if (!event.GetString().IsEmpty())
////        CCLogger::Get()->DebugLog(event.GetString());
////
////    UpdateClassBrowser();
////
////    // In this case, the parser will record all the cbprojects' token, so this will start parsing
////    // the next cbproject.
////    TRACE(_T("ParseManager::OnParserEnd: Starting m_TimerParsingOneByOne."));
////    m_TimerParsingOneByOne.Start(500, wxTIMER_ONE_SHOT);
////
////    // both ParseManager and CodeCompletion class need to handle this event
////    event.Skip();
////    TRACE(_T("ParseManager::OnParserEnd: Leave"));
////}

////void ParseManager::OnParsingOneByOneTimer(cb_unused wxTimerEvent& event)
////{
////    TRACE(_T("ParseManager::OnParsingOneByOneTimer: Enter"));
////
////    std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
////    if (m_ParserPerWorkspace)
////    {
////        // If there is no parser and an active editor file can be obtained, parse the file according the active project
////        if (!info.second && Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor())
////        {
////            // NOTE (Morten#1#): Shouldn't this actually be a temp parser??? I think this screws things with re-opening files on load of a projects...
////            AddProjectToParser(info.first);
////            CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Add foreign active editor to current active project's parser."));
////        }
////        // Otherwise, there is a parser already present
////        else
////        {
////            // First: try to parse the active project (if any)
////            cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
////            if (m_ParsedProjects.find(activeProject) == m_ParsedProjects.end())
////            {
////                AddProjectToParser(activeProject);
////                CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Add new (un-parsed) active project to parser."));
////            }
////            // Else: add remaining projects one-by-one (if any)
////            else
////            {
////                ProjectsArray* projs = Manager::Get()->GetProjectManager()->GetProjects();
////                // loop on the whole workspace, and only add a new project to the parser
////                // here the "new" means a project haven't been parsed. Once it was parsed, it is
////                // added to the m_ParsedProjects
////                for (size_t i = 0; i < projs->GetCount(); ++i)
////                {
////                    // Only add, if the project is not already parsed
////                    if (m_ParsedProjects.find(projs->Item(i)) == m_ParsedProjects.end())
////                    {
////                        // AddProjectToParser return true means there are something need to parse, otherwise, it is false
////                        if (!AddProjectToParser(projs->Item(i)))
////                        {
////                            CCLogger::Get()->Log(_T("ParseManager::OnParsingOneByOneTimer: nothing need to parse in this project, try next project."));
////                            continue;
////                        }
////
////                        CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Add additional (next) project to parser."));
////                        break;
////                    }
////                }
////            }
////        }
////    }
////    else if (info.first && !info.second)
////    {
////        info.second = CreateParser(info.first);
////        if (info.second && info.second != m_Parser)
////        {
////            CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Start switch from OnParsingOneByOneTimer"));
////            SwitchParser(info.first, info.second); // Calls SetParser() which also calls UpdateClassBrowserView()
////        }
////    }
////    TRACE(_T("ParseManager::OnParsingOneByOneTimer: Leave"));
////}

// ----------------------------------------------------------------------------
void ParseManager::OnEditorActivated(EditorBase* editor)
// ----------------------------------------------------------------------------
{
    cbEditor* curEditor = Manager::Get()->GetEditorManager()->GetBuiltinEditor(editor);
    if (!curEditor)
        return;

    const wxString& activatedFile = editor->GetFilename();
    if ( !wxFile::Exists(activatedFile) )
        return;

    cbProject* project = GetProjectByEditor(curEditor);
    const int pos = m_StandaloneFiles.Index(activatedFile);
    if (project && pos != wxNOT_FOUND)
    {
        m_StandaloneFiles.RemoveAt(pos);
        if (m_StandaloneFiles.IsEmpty())
            DeleteParser(nullptr);
        else
            RemoveFileFromParser(NULL, activatedFile);
    }

    ParserBase* parser = GetParserByProject(project);
    if (!parser)
    {
        ParserCommon::EFileType ft = ParserCommon::FileType(activatedFile);
        if (ft != ParserCommon::ftOther && (parser = CreateParser(project)))
        {
            if (!project && AddFileToParser(project, activatedFile, parser) )
            {
                wxFileName file(activatedFile);
                parser->AddIncludeDir(file.GetPath());
                m_StandaloneFiles.Add(activatedFile);
            }
        }
        else
            parser = m_TempParser; // do *not* use SetParser(m_TempParser)
    }
    else if (!project)
    {
        if (   !parser->IsFileParsed(activatedFile)
            && m_StandaloneFiles.Index(activatedFile) == wxNOT_FOUND
            && AddFileToParser(project, activatedFile, parser) )
        {
            wxFileName file(activatedFile);
            parser->AddIncludeDir(file.GetPath());
            m_StandaloneFiles.Add(activatedFile);
        }
    }

    if (parser != m_Parser)
    {
        CCLogger::Get()->DebugLog(_T("Start switch from OnEditorActivatedTimer"));
        SwitchParser(project, parser); // Calls SetParser() which also calls UpdateClassBrowserView()
    }

    if (m_ClassBrowser)
    {
        if (m_Parser->ClassBrowserOptions().displayFilter == bdfFile)
            m_ClassBrowser->UpdateClassBrowserView(true); // check header and implementation file swap
        else if (   m_ParserPerWorkspace // project view only available in case of one parser per WS //m_ParserPerWorkspace always false for clangd
                 && m_Parser->ClassBrowserOptions().displayFilter == bdfProject)
            m_ClassBrowser->UpdateClassBrowserView();
    }
}
// ----------------------------------------------------------------------------
void ParseManager::OnEditorClosed(EditorBase* editor)
// ----------------------------------------------------------------------------
{
    // the caller of the function should guarantee its a built-in editor
    wxString filename = editor->GetFilename();
    const int pos = m_StandaloneFiles.Index(filename); // not used by clangd_client
    if (pos != wxNOT_FOUND)
    {
        m_StandaloneFiles.RemoveAt(pos);
        if (m_StandaloneFiles.IsEmpty())
            DeleteParser(nullptr);
        else
            RemoveFileFromParser(NULL, filename);
    }

    // Remove the file from ~ProxyProject~
    cbProject* pProxyProject = GetProxyProject();
    if (pProxyProject and pProxyProject->GetFileByFilename(filename,false))
    {
        ProjectFile* pProjectFile = pProxyProject->GetFileByFilename(filename,false);
        if (pProjectFile)
        {
            pProxyProject->RemoveFile(pProjectFile);
            pProxyProject->SetModified(false);
        }
    }
}

// ----------------------------------------------------------------------------
void ParseManager::InitCCSearchVariables()
// ----------------------------------------------------------------------------
{
    m_LastControl       = nullptr;
    m_LastFunctionIndex = -1;
    m_LastLine          = -1;
    m_LastResult        = -1;
    m_LastFile.Clear();
    m_LastNamespace.Clear();
    m_LastPROC.Clear();

    Reset();
}
// ----------------------------------------------------------------------------
bool ParseManager::InstallClangdProxyProject()
// ----------------------------------------------------------------------------
{
    //#include <wx/fs_zip.h>
    //#include <wx/zipstrm.h>
    //#include <wx/wfstream.h>
    //#include <wx/stdpaths.h> //if needed

    wxString userDataFolder   = ConfigManager::GetConfigFolder(); //appdata/roaming/codeblocks
    wxString resourceFolder   = ConfigManager::GetDataFolder(); //share/codeblocks
    wxString resourceZipFile = resourceFolder + "/clangd_client.zip";
    wxFileSystem::AddHandler(new wxZipFSHandler);

    bool done = true;
    if (not wxFileExists(userDataFolder + "/CC_ProxyProject.cbp"))
    {
        done = false;
        wxFileSystem fs;
        wxFSFile* zip = fs.OpenFile(resourceZipFile + "#zip:CC_ProxyProject.cbp");
        if(zip != NULL)
        {
          wxInputStream* in = zip->GetStream();
          if ( in != NULL )
          {
            wxFileOutputStream out( userDataFolder + "CC_ProxyProject.cbp" );
            out.Write(*in);
            out.Close();
            done = true;
          }
          delete zip;
        }
    }
    return done;
}
// ----------------------------------------------------------------------------
void ParseManager::SetProxyProject(cbProject* pActiveProject)
// ----------------------------------------------------------------------------
{
    // This proxy (temp) cbProject is used to handle non-project
    // files sent to clangd. This hidden project may contain the
    // compiler info needed for clangd to parse a file.
    // We'll add all opened non-project files to this proxy.
    // This allows us to manage non-project files without mangling the active cbProject.

    wxString msg = "Creating ProxyProject/Clangd_clinet/Parser for non-project files.";
    CCLogger::Get()->DebugLog(msg);

    if (not m_pProxyProject)
    {
        Manager::Get()->GetLogManager()->Log("ClangdClient: allocating ProxyProject (phase 1).");
        // configure an error msg just in case...
        wxString configFolder = ConfigManager::GetConfigFolder();
        msg.Clear();
        msg = "CodeCompletion parser failed to install the proxy project\n";
        msg << "that handles non-project files. Either " << configFolder;
        msg << "\nis missing or share/codeblocks/clangd_client.zip is misconfigured";

        bool ok = InstallClangdProxyProject();
        if (not ok)
        {
            msg << "\n Install of CC_ProxyProject.cbp failed.";
            cbMessageBox(msg, "Clangd_client Error");
            return;
        }

        // Creation of a hidden cbProject must be done in the following sequence in
        // order not to screw up the event expectations of current plugns.
        // 1) Allocate a new raw cbProject.
        // 2) Load an empty project to clear plugin event expectations.
        // 3) Copy the loaded cbProject into the raw cbProject.
        // 4) Close the loaded project to satisfy plugin expectation.
        ProjectManager* pPrjMgr = Manager::Get()->GetProjectManager();
        m_pProxyProject = new cbProject(configFolder + "/CC_ProxyProject.cbp");
        if (m_pProxyProject)
        {
            Manager::Get()->GetLogManager()->Log("ClangdClient: loading ProxyProject (phase 2.");
            m_pProxyProject->SetNotifications(false);
            // Freeze the editor window so it doesn't flash on project load/close
            Manager::Get()->GetEditorManager()->GetNotebook()->Freeze();
            // open an empty project to get CB to build all the internal project structures
            cbProject* pLoadedEmptyProject = pPrjMgr->LoadProject(configFolder + "/CC_ProxyProject.cbp", false);
            // copy all CB internal structures to the hidden proxy project. Then close the loaded project
            //  now that we have all the internal structures copied into the created hidden cbProject.
            m_pProxyProject = &(m_pProxyProject->cbProject::operator=(*pLoadedEmptyProject));
            pPrjMgr->CloseProject(pLoadedEmptyProject, true, false); //nosave,norefresh
            Manager::Get()->GetEditorManager()->GetNotebook()->Thaw();
        }
        if (not m_pProxyProject)
        {
            msg << "Allocation of new cbProject proxy (ProxyProject) failed in ";
            msg  << wxString::Format("%s", __FUNCTION__);
            cbMessageBox(msg, "Clangd_client Error");
            return;
        }
        // Remove the created hidden "untitled" proxy project from the workspace/project tree.
        // so CB has nothing to save or report about.
        pPrjMgr->GetUI().RemoveProject(m_pProxyProject);
        // Say workspace is not modified
        pPrjMgr->GetWorkspace()->SetModified(false);
    }

    m_pProxyProject->SetTitle("~ProxyProject~");
    // Don't allow the ProyProject to issue events. It's not a full blown cbProject.
    // We do not want to confuse other plugins with a hidden project.
    m_pProxyProject->SetNotifications(false);
    // Set the proxy project into the list of parser->projects
    //This is what happens: m_ParserList.push_back(std::make_pair(m_pProxyProject, pProxyParser));
    // ProxyProject will now look like a regular cbProject to the parsers and clangd_client.
    // We'll be able to add and remove non-project files to the hidden cbProject.
    ParserBase* pProxyParser = CreateParser(m_pProxyProject, false);

    if (pProxyParser) m_pProxyParser = pProxyParser;

    m_pProxyProject->SetCheckForExternallyModifiedFiles(false);
    // Remove the 'default' ProxyProject target if we have an active project clone from.
    if (pActiveProject && m_pProxyProject->GetBuildTargetsCount())
        m_pProxyProject->RemoveBuildTarget(0);

    // If we have a good pattern to copy from, use it.
    if (pActiveProject)
    {   // Set info usable to create a compile command
        m_pProxyProject->SetIncludeDirs(pActiveProject->GetIncludeDirs());
        m_pProxyProject->SetCompilerID(pActiveProject->GetCompilerID());
        m_pProxyProject->SetCompilerOptions(pActiveProject->GetCompilerOptions());
    }
    m_pProxyProject->SetModified(false);

    // Remove any cloned targets from the ProxyProject to avoid possible double deletes
    if (pActiveProject)
        for (int ii=0; ii<m_pProxyProject->GetBuildTargetsCount(); ++ii)
            m_pProxyProject->RemoveBuildTarget(ii);

    // Add active project targets to ProxyProject (usable for clangd compile commands)
    if (pActiveProject)
        for (int ii=0; ii<pActiveProject->GetBuildTargetsCount(); ++ii)
        {
            ProjectBuildTarget* pBuildTarget = pActiveProject->GetBuildTarget(ii);
            ProjectBuildTarget* pNewpbt = m_pProxyProject->AddBuildTarget(pBuildTarget->GetTitle());
            // compilerTargetBase
            pNewpbt->SetTargetType(pBuildTarget->GetTargetType());
            pNewpbt->SetWorkingDir(pBuildTarget->GetWorkingDir());
            pNewpbt->SetObjectOutput(pBuildTarget->GetObjectOutput());
            pNewpbt->SetTargetType(pBuildTarget->GetTargetType());
            pNewpbt->SetCompilerID(pBuildTarget->GetCompilerID());
            //CompileOptionsBase
            pNewpbt->SetPlatforms(pBuildTarget->GetPlatforms());
            pNewpbt->SetCompilerOptions(pBuildTarget->GetCompilerOptions());
            pNewpbt->SetLinkerOptions(pBuildTarget->GetLinkerOptions());
            pNewpbt->SetIncludeDirs(pBuildTarget->GetIncludeDirs());
        }//endif for

    m_pProxyProject->SetModified(false); // changed? say it ain't so.
    return;
}
// ----------------------------------------------------------------------------
void ParseManager::ClearAllIdleCallbacks()
// ----------------------------------------------------------------------------
{
    // clear any Idle time callbacks //(ph 2022/08/01)
    if (m_ParserList.size())
    {
        ParserList::iterator parserList_it = m_ParserList.begin();  //(ph 2022/08/01)
        for (; parserList_it != m_ParserList.end(); ++parserList_it)
        {
            ParserBase* pParserBase = parserList_it->second;
            if (pParserBase and pParserBase->GetIdleCallbackHandler())
                pParserBase->GetIdleCallbackHandler()->ClearIdleCallbacks();
        }
    }
}
