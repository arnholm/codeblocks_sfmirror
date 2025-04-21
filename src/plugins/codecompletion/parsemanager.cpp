/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <cctype>

    #include <wx/dir.h>
    #include <wx/log.h> // for wxSafeShowMessage()
    #include <wx/regex.h>
    #include <wx/wfstream.h>

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
#include <wx/listbook.h> //2025/04/21

#include <cbstyledtextctrl.h>
#include <compilercommandgenerator.h>

#include "codecompletion.h" //2025/04/21
#include "parsemanager.h"
#include "classbrowser.h"
#include "parser/parser.h"
//#include "parser/profiletimer.h"

#define CC_PARSEMANAGER_DEBUG_OUTPUT 0

#if defined (CC_GLOBAL_DEBUG_OUTPUT)
    #if CC_GLOBAL_DEBUG_OUTPUT == 1
        #undef CC_PARSEMANAGER_DEBUG_OUTPUT
        #define CC_PARSEMANAGER_DEBUG_OUTPUT 1
    #elif CC_GLOBAL_DEBUG_OUTPUT == 2
        #undef CC_PARSEMANAGER_DEBUG_OUTPUT
        #define CC_PARSEMANAGER_DEBUG_OUTPUT 2
    #endif
#endif

#if CC_PARSEMANAGER_DEBUG_OUTPUT == 1
    #define TRACE(format, args...) \
        CCLogger::Get()->DebugLog(F(format, ##args))
    #define TRACE2(format, args...)
#elif CC_PARSEMANAGER_DEBUG_OUTPUT == 2
    #define TRACE(format, args...)                            \
        do                                                    \
        {                                                     \
            if (g_EnableDebugTrace)                           \
                CCLogger::Get()->DebugLog(F(format, ##args)); \
        }                                                     \
        while (false)
    #define TRACE2(format, args...) \
        CCLogger::Get()->DebugLog(F(format, ##args))
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

namespace ParseManagerHelper
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

/** event id for the sequence project parsing timer */
int idTimerParsingOneByOne = wxNewId();

static void AddToImageList(wxImageList* list, const wxString &path, int size)
{
#if wxCHECK_VERSION(3, 1, 6)
    wxBitmap bmp = cbLoadBitmapBundleFromSVG(path, wxSize(size, size)).GetBitmap(wxDefaultSize);
#else
    wxBitmap bmp = cbLoadBitmap(path);
#endif

    if (!bmp.IsOk())
    {
        bmp.Create(size, size);
        printf("failed to load: %s\n", path.utf8_str().data());
    }

    list->Add(bmp);
}

static wxImageList* LoadImageList(int size)
{
    wxImageList* list = new wxImageList(size, size);
    wxString prefix(ConfigManager::GetDataFolder() + "/codecompletion.zip#zip:images/");
#if wxCHECK_VERSION(3, 1, 6)
    prefix << "svg/";
    const wxString ext(".svg");
#else
    prefix << wxString::Format("%dx%d/", size, size);
    const wxString ext(".png");
#endif

    // Bitmaps must be added by order of PARSER_IMG_* consts.
    AddToImageList(list, prefix + "class_folder" + ext, size); // PARSER_IMG_CLASS_FOLDER
    AddToImageList(list, prefix + "class" + ext, size); // PARSER_IMG_CLASS
    AddToImageList(list, prefix + "class_private" + ext, size); // PARSER_IMG_CLASS_PRIVATE
    AddToImageList(list, prefix + "class_protected" + ext, size); // PARSER_IMG_CLASS_PROTECTED
    AddToImageList(list, prefix + "class_public" + ext, size); // PARSER_IMG_CLASS_PUBLIC
    AddToImageList(list, prefix + "ctor_private" + ext, size); // PARSER_IMG_CTOR_PRIVATE
    AddToImageList(list, prefix + "ctor_protected" + ext, size); // PARSER_IMG_CTOR_PROTECTED
    AddToImageList(list, prefix + "ctor_public" + ext, size); // PARSER_IMG_CTOR_PUBLIC
    AddToImageList(list, prefix + "dtor_private" + ext, size); // PARSER_IMG_DTOR_PRIVATE
    AddToImageList(list, prefix + "dtor_protected" + ext, size); // PARSER_IMG_DTOR_PROTECTED
    AddToImageList(list, prefix + "dtor_public" + ext, size); // PARSER_IMG_DTOR_PUBLIC
    AddToImageList(list, prefix + "method_private" + ext, size); // PARSER_IMG_FUNC_PRIVATE
    AddToImageList(list, prefix + "method_protected" + ext, size); // PARSER_IMG_FUNC_PRIVATE
    AddToImageList(list, prefix + "method_public" + ext, size); // PARSER_IMG_FUNC_PUBLIC
    AddToImageList(list, prefix + "var_private" + ext, size); // PARSER_IMG_VAR_PRIVATE
    AddToImageList(list, prefix + "var_protected" + ext, size); // PARSER_IMG_VAR_PROTECTED
    AddToImageList(list, prefix + "var_public" + ext, size); // PARSER_IMG_VAR_PUBLIC
    AddToImageList(list, prefix + "macro_def" + ext, size); // PARSER_IMG_MACRO_DEF
    AddToImageList(list, prefix + "enum" + ext, size); // PARSER_IMG_ENUM
    AddToImageList(list, prefix + "enum_private" + ext, size); // PARSER_IMG_ENUM_PRIVATE
    AddToImageList(list, prefix + "enum_protected" + ext, size); // PARSER_IMG_ENUM_PROTECTED
    AddToImageList(list, prefix + "enum_public" + ext, size); // PARSER_IMG_ENUM_PUBLIC
    AddToImageList(list, prefix + "enumerator" + ext, size); // PARSER_IMG_ENUMERATOR
    AddToImageList(list, prefix + "namespace" + ext, size); // PARSER_IMG_NAMESPACE
    AddToImageList(list, prefix + "typedef" + ext, size); // PARSER_IMG_TYPEDEF
    AddToImageList(list, prefix + "typedef_private" + ext, size); // PARSER_IMG_TYPEDEF_PRIVATE
    AddToImageList(list, prefix + "typedef_protected" + ext, size); // PARSER_IMG_TYPEDEF_PROTECTED
    AddToImageList(list, prefix + "typedef_public" + ext, size); // PARSER_IMG_TYPEDEF_PUBLIC
    AddToImageList(list, prefix + "symbols_folder" + ext, size); // PARSER_IMG_SYMBOLS_FOLDER
    AddToImageList(list, prefix + "vars_folder" + ext, size); // PARSER_IMG_VARS_FOLDER
    AddToImageList(list, prefix + "funcs_folder" + ext, size); // PARSER_IMG_FUNCS_FOLDER
    AddToImageList(list, prefix + "enums_folder" + ext, size); // PARSER_IMG_ENUMS_FOLDER
    AddToImageList(list, prefix + "macro_def_folder" + ext, size); // PARSER_IMG_MACRO_DEF_FOLDER
    AddToImageList(list, prefix + "others_folder" + ext, size); // PARSER_IMG_OTHERS_FOLDER
    AddToImageList(list, prefix + "typedefs_folder" + ext, size); // PARSER_IMG_TYPEDEF_FOLDER
    AddToImageList(list, prefix + "macro_use" + ext, size); // PARSER_IMG_MACRO_USE
    AddToImageList(list, prefix + "macro_use_private" + ext, size); // PARSER_IMG_MACRO_USE_PRIVATE
    AddToImageList(list, prefix + "macro_use_protected" + ext, size); // PARSER_IMG_MACRO_USE_PROTECTED
    AddToImageList(list, prefix + "macro_use_public" + ext, size); // PARSER_IMG_MACRO_USE_PUBLIC
    AddToImageList(list, prefix + "macro_use_folder" + ext, size); // PARSER_IMG_MACRO_USE_FOLDER

    return list;
}

ParseManager::ParseManager() :
    m_TimerParsingOneByOne(this, idTimerParsingOneByOne),
    m_ClassBrowser(nullptr),
    m_ClassBrowserIsFloating(false),
    m_ParserPerWorkspace(false),
    m_LastAISearchWasGlobal(false),
    m_LastControl(nullptr),
    m_LastFunctionIndex(-1),
    m_LastFuncTokenIdx(-1),
    m_LastLine(-1),
    m_LastResult(-1)
{
    m_TempParser = new Parser(this, nullptr);
    m_Parser     = m_TempParser;

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("code_completion"));
    m_ParserPerWorkspace = cfg->ReadBool(_T("/parser_per_workspace"), false);

    Connect(ParserCommon::idParserStart, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParseManager::OnParserStart));
    Connect(ParserCommon::idParserEnd,   wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParseManager::OnParserEnd));
    Connect(idTimerParsingOneByOne,      wxEVT_TIMER,                 wxTimerEventHandler(ParseManager::OnParsingOneByOneTimer));
}

ParseManager::~ParseManager()
{
    Disconnect(ParserCommon::idParserStart, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParseManager::OnParserStart));
    Disconnect(ParserCommon::idParserEnd,   wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParseManager::OnParserEnd));
    Disconnect(idTimerParsingOneByOne,      wxEVT_TIMER,                 wxTimerEventHandler(ParseManager::OnParsingOneByOneTimer));
    RemoveClassBrowser();
    ClearParsers();
    Delete(m_TempParser);
}

ParserBase* ParseManager::GetParserByProject(cbProject* project)
{
    if (m_ParserPerWorkspace)
    {
        std::set<cbProject*>::iterator it = m_ParsedProjects.find(project);
        if (it != m_ParsedProjects.end())
            return m_ParserList.begin()->second;
    }
    else
    {
        for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
        {
            if (it->first == project)
                return it->second;
        }
    }

    TRACE(_T("ParseManager::GetParserByProject: Returning nullptr."));
    return nullptr;
}

ParserBase* ParseManager::GetParserByFilename(const wxString& filename)
{
    cbProject* project = GetProjectByFilename(filename);
    return GetParserByProject(project);
}

cbProject* ParseManager::GetProjectByParser(ParserBase* parser)
{
    for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
    {
        if (it->second == parser)
            return it->first;
    }

    TRACE(_T("ParseManager::GetProjectByParser: Returning NULL."));
    return NULL;
}

cbProject* ParseManager::GetProjectByFilename(const wxString& filename)
{
    TRACE(_T("ParseManager::GetProjectByFilename: %s"), filename.wx_str());
    cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (activeProject)
    {
        ParserBase* parser = GetParserByProject(activeProject);
        if (   (   parser
                && parser->IsFileParsed(filename) )
            || activeProject->GetFileByFilename(filename, false, true) )
        {
            return activeProject;
        }
        else
        {
            ProjectsArray* projs = Manager::Get()->GetProjectManager()->GetProjects();
            for (size_t i = 0; i < projs->GetCount(); ++i)
            {
                cbProject* project = projs->Item(i);
                if (!project || project == activeProject)
                    continue;

                parser = GetParserByProject(project);
                if (   (   parser
                        && parser->IsFileParsed(filename) )
                    || project->GetFileByFilename(filename, false, true) )
                {
                    return project;
                }
            }
        }
    }

    return nullptr;
}

cbProject* ParseManager::GetProjectByEditor(cbEditor* editor)
{
    if (!editor)
        return nullptr;
    ProjectFile* pf = editor->GetProjectFile();
    if (pf && pf->GetParentProject())
        return pf->GetParentProject();
    return GetProjectByFilename(editor->GetFilename());
}

cbProject* ParseManager::GetCurrentProject()
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

wxImageList* ParseManager::GetImageList(int maxSize)
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

wxArrayString ParseManager::GetAllPathsByFilename(const wxString& filename)
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
        cbProject* project = IsParserPerWorkspace() ? GetCurrentProject()
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

    CCLogger::Get()->DebugLog(wxString::Format("ParseManager::GetAllPathsByFilename: Found %zu files:", files.GetCount()));
    for (size_t i=0; i<files.GetCount(); i++)
        CCLogger::Get()->DebugLog(wxString::Format("- %s", files[i]));

    if (!files.IsEmpty())
        AddPaths(dirs, files[0], fn.HasExt());

    TRACE(_T("ParseManager::GetAllPathsByFilename: Leave"));
    return dirs;
}

void ParseManager::AddPaths(wxArrayString& dirs, const wxString& path, bool hasExt)
{
    wxString s;
    if (hasExt)
        s = UnixFilename(path.BeforeLast(_T('.'))) + _T(".");
    else
        s = UnixFilename(path);

    if (dirs.Index(s, false) == wxNOT_FOUND)
        dirs.Add(s);
}

ParserBase* ParseManager::CreateParser(cbProject* project)
{
    if ( GetParserByProject(project) )
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::CreateParser: Parser for this project already exists!"));
        return nullptr;
    }

    // Easy case for "one parser per workspace" that has already been created:
    if (m_ParserPerWorkspace && !m_ParsedProjects.empty())
        return m_ParserList.begin()->second;

    TRACE(_T("ParseManager::CreateParser: Calling DoFullParsing()"));

    ParserBase* parser = new Parser(this, project);
    if ( !DoFullParsing(project, parser) )
    {
        CCLogger::Get()->DebugLog(_T("ParseManager::CreateParser: Full parsing failed!"));
        delete parser;
        return nullptr;
    }

    if (m_Parser == m_TempParser)
        SetParser(parser); // Also updates class browser

    if (m_ParserPerWorkspace)
        m_ParsedProjects.insert(project);

    m_ParserList.push_back(std::make_pair(project, parser));

    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    wxString log(wxString::Format(_("ParseManager::CreateParser: Finish creating a new parser for project '%s'"), prj));
    CCLogger::Get()->Log(log);
    CCLogger::Get()->DebugLog(log);

    RemoveObsoleteParsers();

    return parser;
}

bool ParseManager::DeleteParser(cbProject* project)
{
    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));

    ParserList::iterator it = m_ParserList.begin();
    if (!m_ParserPerWorkspace)
    {
        for (; it != m_ParserList.end(); ++it)
        {
            if (it->first == project)
                break;
        }
    }

    if (it == m_ParserList.end())
    {
        CCLogger::Get()->DebugLog(wxString::Format("ParseManager::DeleteParser: Parser does not exist for delete '%s'!", prj));
        return false;
    }

    bool removeProjectFromParser = false;
    if (m_ParserPerWorkspace)
        removeProjectFromParser = RemoveProjectFromParser(project);

    if (m_ParsedProjects.empty()) // this indicates we are in one parser per one project mode
    {
        wxString log(wxString::Format(_("ParseManager::DeleteParser: Deleting parser for project '%s'!"), prj));
        CCLogger::Get()->Log(log);
        CCLogger::Get()->DebugLog(log);

        // the logic here is : firstly delete the parser instance, then see whether we need an
        // active parser switch (call SetParser())
        delete it->second;

        // if the active parser is deleted, set the active parser to nullptr
        if (it->second == m_Parser)
        {
            SetClosingParser(m_Parser); //(ph 2025/02/12)
            m_Parser = nullptr;
            SetParser(m_TempParser); // Also updates class browser
        }

        m_ParserList.erase(it);

        return true;
    }

    if (removeProjectFromParser)
        return true;

    CCLogger::Get()->DebugLog(_T("ParseManager::DeleteParser: Deleting parser failed!"));
    return false;
}

bool ParseManager::ReparseFile(cbProject* project, const wxString& filename)
{
    if (ParserCommon::FileType(filename) == ParserCommon::ftOther)
        return false;

    ParserBase* parser = GetParserByProject(project);
    if (!parser)
        return false;

    if (!parser->UpdateParsingProject(project))
        return false;

    TRACE(_T("ParseManager::ReparseFile: Calling Parser::Reparse()"));

    return parser->Reparse(filename);
}

bool ParseManager::AddFileToParser(cbProject* project, const wxString& filename, ParserBase* parser)
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

bool ParseManager::RemoveFileFromParser(cbProject* project, const wxString& filename)
{
    ParserBase* parser = GetParserByProject(project);
    if (!parser)
        return false;

    TRACE(_T("ParseManager::RemoveFileFromParser: Calling Parser::RemoveFile()"));

    return parser->RemoveFile(filename);
}

void ParseManager::RereadParserOptions()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("code_completion"));
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

    const bool parserPerWorkspace = cfg->ReadBool(_T("/parser_per_workspace"), false);
    if (m_Parser == m_TempParser)
    {
        m_ParserPerWorkspace = parserPerWorkspace;
        return;
    }

    RemoveObsoleteParsers();

    // re-parse if settings changed
    ParserOptions opts = m_Parser->Options();
    m_Parser->ReadOptions();
    bool reparse = false;
    cbProject* project = GetCurrentProject();
    if (   opts.followLocalIncludes  != m_Parser->Options().followLocalIncludes
        || opts.followGlobalIncludes != m_Parser->Options().followGlobalIncludes
        || opts.wantPreprocessor     != m_Parser->Options().wantPreprocessor
        || opts.parseComplexMacros   != m_Parser->Options().parseComplexMacros
        || opts.platformCheck        != m_Parser->Options().platformCheck
        || m_ParserPerWorkspace      != parserPerWorkspace )
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

    m_ParserPerWorkspace = parserPerWorkspace;

    if (reparse)
        CreateParser(project);
}

void ParseManager::ReparseCurrentProject()
{
    cbProject* project = GetCurrentProject();
    if (project)
    {
        TRACE(_T("ParseManager::ReparseCurrentProject: Calling DeleteParser() and CreateParser()"));
        DeleteParser(project);
        CreateParser(project);
    }
}

void ParseManager::ReparseSelectedProject()
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

// Here, we collect the "using namespace XXXX" directives
// Also, we locate the current caret in which function, then, add the function parameters to Token trie
// Also, the variables in the function body( local block ) was add to the Token trie
size_t ParseManager::MarkItemsByAI(ccSearchData* searchData,
                                   TokenIdxSet&  result,
                                   bool          reallyUseAI,
                                   bool          isPrefix,
                                   bool          caseSensitive,
                                   int           caretPos)
{
    result.clear();

    if (!m_Parser->Done())
    {
        wxString msg(_("The Parser is still parsing files."));
        msg += m_Parser->NotDoneReason();
        CCLogger::Get()->DebugLog(msg);
        return 0;
    }

    TRACE(_T("ParseManager::MarkItemsByAI_2()"));

    TokenTree* tree = m_Parser->GetTempTokenTree();

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    // remove old temporaries
    tree->Clear();

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    RemoveLastFunctionChildren(m_Parser->GetTokenTree(), m_LastFuncTokenIdx);

    // find "using namespace" directives in the file
    TokenIdxSet search_scope;
    ParseUsingNamespace(searchData, search_scope, caretPos);

    // parse function's arguments
    ParseFunctionArguments(searchData, caretPos);

    // parse current code block (from the start of function up to the cursor)
    ParseLocalBlock(searchData, search_scope, caretPos);

    if (!reallyUseAI)
    {
        tree = m_Parser->GetTokenTree();

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        // all tokens, no AI whatsoever
        for (size_t i = 0; i < tree->size(); ++i)
            result.insert(i);

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        return result.size();
    }

    // we have correctly collected all the tokens, so we will do the artificial intelligence search
    return AI(result, searchData, wxEmptyString, isPrefix, caseSensitive, &search_scope, caretPos);
}

size_t ParseManager::MarkItemsByAI(TokenIdxSet& result,
                                   bool         reallyUseAI,
                                   bool         isPrefix,
                                   bool         caseSensitive,
                                   int          caretPos)
{
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog("MarkItemsByAI_1()");

    cbEditor* editor = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!editor)
        return 0;

    ccSearchData searchData = { editor->GetControl(), editor->GetFilename() };
    if (!searchData.control)
        return 0;

    TRACE(_T("ParseManager::MarkItemsByAI_1()"));

    return MarkItemsByAI(&searchData, result, reallyUseAI, isPrefix, caseSensitive, caretPos);
}

int ParseManager::GetCallTips(wxArrayString& items, int& typedCommas, cbEditor* ed, int pos)
{
    items.Clear();
    typedCommas = 0;
    int commas = 0;

    if (!ed || !m_Parser->Done())
    {
        items.Add(_("Parsing at the moment") + "...");
        return wxSCI_INVALID_POSITION;
    }

    TRACE(_T("ParseManager::GetCallTips()"));

    ccSearchData searchData = { ed->GetControl(), ed->GetFilename() };
    if (pos == wxNOT_FOUND)
        pos = searchData.control->GetCurrentPos();
    int nest = 0;
    while (--pos > 0)
    {
        const int style = searchData.control->GetStyleAt(pos);
        if (   searchData.control->IsString(style)
            || searchData.control->IsCharacter(style)
            || searchData.control->IsComment(style) )
        {
            continue;
        }

        const wxChar ch = searchData.control->GetCharAt(pos);
        if (ch == _T(';'))
            return wxSCI_INVALID_POSITION;
        else if (ch == _T(','))
        {
            if (nest == 0)
                ++commas;
        }
        else if (ch == _T(')'))
            --nest;
        else if (ch == _T('('))
        {
            ++nest;
            if (nest > 0)
                break;
        }
    }// while

    // strip un-wanted
    while (--pos > 0)
    {
        if (   searchData.control->GetCharAt(pos) <= _T(' ')
            || searchData.control->IsComment(searchData.control->GetStyleAt(pos)) )
        {
            continue;
        }
        break;
    }

    const int start = searchData.control->WordStartPosition(pos, true);
    const int end = searchData.control->WordEndPosition(pos, true);
    const wxString target = searchData.control->GetTextRange(start, end);
    TRACE(_T("Sending \"%s\" for call-tip"), target.wx_str());
    if (target.IsEmpty())
        return wxSCI_INVALID_POSITION;

    TokenIdxSet result;
    MarkItemsByAI(result, true, false, true, end);

    ComputeCallTip(m_Parser->GetTokenTree(), result, items);

    typedCommas = commas;
    TRACE(_T("ParseManager::GetCallTips: typedCommas=%d"), typedCommas);
    items.Sort();
    return end;
}

wxArrayString ParseManager::ParseProjectSearchDirs(const cbProject &project)
{

    const TiXmlNode *extensionNode = project.GetExtensionsNode();
    if (!extensionNode)
        return wxArrayString();
    const TiXmlElement* elem = extensionNode->ToElement();
    if (!elem)
        return wxArrayString();

    wxArrayString pdirs;
    const TiXmlElement* CCConf = elem->FirstChildElement("code_completion");
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

void ParseManager::SetProjectSearchDirs(cbProject &project, const wxArrayString &dirs)
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
    TiXmlElement* node = elem->FirstChildElement("code_completion");
    if (!node)
        node = elem->InsertEndChild(TiXmlElement("code_completion"))->ToElement();
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

void ParseManager::CreateClassBrowser()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("code_completion"));
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

void ParseManager::RemoveClassBrowser(cb_unused bool appShutDown)
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

void ParseManager::UpdateClassBrowser()
{
    if (!m_ClassBrowser)
          return;

    TRACE(_T("ParseManager::UpdateClassBrowser()"));

    if (   m_Parser != m_TempParser
        && m_Parser->Done()
        && !Manager::IsAppShuttingDown() )
    {
        m_ClassBrowser->UpdateClassBrowserView();
    }
}

bool ParseManager::DoFullParsing(cbProject* project, ParserBase* parser)
{
    wxStopWatch timer;
    if (!parser)
        return false;

    TRACE(_T("ParseManager::DoFullParsing: Enter"));

    if (!AddCompilerDirs(project, parser))
        CCLogger::Get()->DebugLog(_T("ParseManager::DoFullParsing: AddCompilerDirs failed!"));

    if (!AddCompilerPredefinedMacros(project, parser))
        CCLogger::Get()->DebugLog(_T("ParseManager::DoFullParsing: AddCompilerPredefinedMacros failed!"));

    if (!AddProjectDefinedMacros(project, parser))
        CCLogger::Get()->DebugLog(_T("ParseManager::DoFullParsing: AddProjectDefinedMacros failed!"));

    // add per-project dirs
    if (project)
    {
        if (   !parser->Options().platformCheck
            || (parser->Options().platformCheck && project->SupportsCurrentPlatform()) )
        {
            // Note: This parses xml data to get the search directories. It might be expensive if
            //       the list of directories is too large.
            AddIncludeDirsToParser(ParseProjectSearchDirs(*project),
                                   project->GetBasePath(), parser);
        }
    }

    StringList localSources;

    if (project)
    {
        for (FilesList::const_iterator fl_it = project->GetFilesList().begin();
             fl_it != project->GetFilesList().end(); ++fl_it)
        {
            ProjectFile* pf = *fl_it;
            if (!pf)
                continue;
            // check the file types in the project files
            ParserCommon::EFileType ft = ParserCommon::FileType(pf->relativeFilename);
            if (ft == ParserCommon::ftSource) // parse source files
            {
                localSources.push_back(pf->file.GetFullPath());
            }
        }
    }

    CCLogger::Get()->DebugLog(_T("ParseManager::DoFullParsing: Adding cpp/c files to batch-parser"));

    // parse priority files
    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));


    if (!localSources.empty())
    {
        CCLogger::Get()->DebugLog(wxString::Format("ParseManager::DoFullParsing: Added %zu source file(s) for project '%s' to batch-parser...",
                                                   localSources.size(), prj));

        // local source files added to Parser
        parser->AddBatchParse(localSources);
    }

    TRACE(_T("ParseManager::DoFullParsing: Leave"));

    long time = timer.Time();
    if (time >= 50)
        Manager::Get()->GetLogManager()->Log(wxString::Format(_("ParseManager::DoFullParsing took: %.3f seconds."), time / 1000.0f));
    return true;
}

bool ParseManager::SwitchParser(cbProject* project, ParserBase* parser)
{
    if (!parser || parser == m_Parser || GetParserByProject(project) != parser)
    {
        TRACE(_T("ParseManager::SwitchParser: No need to / cannot switch."));
        return false;
    }

    TRACE(_T("ParseManager::SwitchParser()"));

    SetParser(parser); // Also updates class browser

    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    wxString log(wxString::Format(_("Switch parser to project '%s'"), prj));
    CCLogger::Get()->Log(log);
    CCLogger::Get()->DebugLog(log);

    return true;
}

void ParseManager::SetParser(ParserBase* parser)
{
    // the active parser is the same as the old active parser, nothing need to be done
    if (m_Parser == parser)
        return;

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

void ParseManager::ClearParsers()
{
    TRACE(_T("ParseManager::ClearParsers()"));

    if (m_ParserPerWorkspace)
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

void ParseManager::RemoveObsoleteParsers()
{
    TRACE(_T("ParseManager::RemoveObsoleteParsers: Enter"));

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("code_completion"));
    const size_t maxParsers = cfg->ReadInt(_T("/max_parsers"), 5);
    wxArrayString removedProjectNames;
    std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();

    while (m_ParserList.size() > maxParsers)
    {
        bool deleted = false;
        for (ParserList::const_iterator it = m_ParserList.begin(); it != m_ParserList.end(); ++it)
        {
            if (it->second == info.second)
                continue;

            wxString prj = (it->first ? it->first->GetTitle() : _T("*NONE*"));
            if ( DeleteParser(it->first) )
            {
                // Please note that DeleteParser() may erase one element of the m_ParserList, so
                // do NOT use the constant iterator here again, as the element pointed by it may be
                // destroyed in DeleteParser().
                removedProjectNames.Add(prj);
                deleted = true;
                break;
            }
        }

        if (!deleted)
            break;
    }

    for (size_t i = 0; i < removedProjectNames.GetCount(); ++i)
    {
        wxString log(wxString::Format(_("ParseManager::RemoveObsoleteParsers:Removed obsolete parser of '%s'"), removedProjectNames[i]));
        CCLogger::Get()->Log(log);
        CCLogger::Get()->DebugLog(log);
    }

    TRACE(_T("ParseManager::RemoveObsoleteParsers: Leave"));
}

std::pair<cbProject*, ParserBase*> ParseManager::GetParserInfoByCurrentEditor()
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

void ParseManager::SetCBViewMode(const BrowserViewMode& mode)
{
    m_Parser->ClassBrowserOptions().showInheritance = (mode == bvmInheritance) ? true : false;
    UpdateClassBrowser();
}

// helper funcs

// Start an Artificial Intelligence (!) sequence to gather all the matching tokens..
// The actual AI is in FindAIMatches() below...
size_t ParseManager::AI(TokenIdxSet&    result,
                        ccSearchData*   searchData,
                        const wxString& lineText,
                        bool            isPrefix,
                        bool            caseSensitive,
                        TokenIdxSet*    search_scope,
                        int             caretPos)
{
    m_LastAISearchWasGlobal = false;
    m_LastAIGlobalSearch.Clear();

    int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
    if (pos < 0 || pos > searchData->control->GetLength())
        return 0;

    int line = searchData->control->LineFromPosition(pos);

    // Get the actual search text, such as "objA.m_aaa.m_bbb"
    wxString actual_search(lineText);
    if (actual_search.IsEmpty())
    {
        // Get the position at the start of current line
        const int startPos = searchData->control->PositionFromLine(line);
        actual_search = searchData->control->GetTextRange(startPos, pos).Trim();
    }

    // Do the whole job here
    if (g_DebugSmartSense)
    {
        CCLogger::Get()->DebugLog("AI() =========================================================");
        CCLogger::Get()->DebugLog(wxString::Format("AI() Doing AI for '%s':", actual_search));
    }
    TRACE(_T("ParseManager::AI()"));

    TokenTree* tree = m_Parser->GetTokenTree();

    // find current function's namespace so we can include local scope's tokens
    // we ' ll get the function's token (all matches) and add its parent namespace
    TokenIdxSet proc_result;
    size_t found_at = FindCurrentFunctionToken(searchData, proc_result, pos);

    TokenIdxSet scope_result;
    if (found_at)
        FindCurrentFunctionScope(tree, proc_result, scope_result);

    // add additional search scopes???
    // for example, we are here:
    /*  void ClassA::FunctionB(int paraC){
            m_aaa
    */
    // then, ClassA should be added as a search_scope, the global scope should be added too.

    // if search_scope is already defined, then, add scope_result to search_scope
    // otherwise we just set search_scope as scope_result
    if (!search_scope)
        search_scope = &scope_result;
    else
    {
        // add scopes, "tis" refer to "token index set"
        for (TokenIdxSet::const_iterator tis_it = scope_result.begin(); tis_it != scope_result.end(); ++tis_it)
            search_scope->insert(*tis_it);
    }

    // remove non-namespace/class tokens
    CleanupSearchScope(tree, search_scope);

    // find all other matches
    std::queue<ParserComponent> components;
    BreakUpComponents(actual_search, components);

    m_LastAISearchWasGlobal = components.size() <= 1;
    if (!components.empty())
        m_LastAIGlobalSearch = components.front().component;

    ResolveExpression(tree, components, *search_scope, result, caseSensitive, isPrefix);

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format("AI() AI leave, returned %zu results", result.size()));

    return result.size();
}

// find a function where current caret located.
// We need to find extra class scope, otherwise, we will failed do the cc in a class declaration
size_t ParseManager::FindCurrentFunctionToken(ccSearchData* searchData, TokenIdxSet& result, int caretPos)
{
    TokenIdxSet scope_result;
    wxString procName;
    wxString scopeName;
    FindCurrentFunctionStart(searchData, &scopeName, &procName, nullptr, caretPos);

    if (procName.IsEmpty())
        return 0;

    // add current scope
    if (!scopeName.IsEmpty())
    {
        // _namespace ends with double-colon (::). remove it
        scopeName.RemoveLast();
        scopeName.RemoveLast();

        // search for namespace
        std::queue<ParserComponent> ns;
        BreakUpComponents(scopeName, ns);

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        // No critical section needed in this recursive function!
        // All functions that call this recursive FindAIMatches function, should already entered a critical section.
        FindAIMatches(m_Parser->GetTokenTree(), ns, scope_result, -1,
                      true, true, false, tkNamespace | tkClass | tkTypedef);

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
    }

    // if no scope, use global scope
    if (scope_result.empty())
        scope_result.insert(-1);

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    for (TokenIdxSet::const_iterator tis_it = scope_result.begin(); tis_it != scope_result.end(); ++tis_it)
    {
        GenerateResultSet(m_Parser->GetTokenTree(), procName, *tis_it, result,
                          true, false, tkAnyFunction | tkClass);
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    return result.size();
}

// returns current function's position (not line) in the editor
int ParseManager::FindCurrentFunctionStart(ccSearchData* searchData,
                                           wxString*     nameSpace,
                                           wxString*     procName,
                                           int*          functionIndex,
                                           int           caretPos)
{
    // cache last result for optimization
    int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
    if ((pos < 0) || (pos > searchData->control->GetLength()))
    {
        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("FindCurrentFunctionStart() Cannot determine position. caretPos=%d, control=%d",
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

        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("FindCurrentFunctionStart() Cached namespace='%s', cached proc='%s' (returning %d)",
                                                       m_LastNamespace, m_LastPROC, m_LastResult));

        return m_LastResult;
    }

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format("FindCurrentFunctionStart() Looking for tokens in '%s'", searchData->file));
    m_LastFile    = searchData->file;
    m_LastControl = searchData->control;
    m_LastLine    = curLine;

    // we have all the tokens in the current file, then just do a loop on all
    // the tokens, see if the line is in the token's imp.
    TokenIdxSet result;
    size_t num_results = m_Parser->FindTokensInFile(searchData->file, result, tkAnyFunction | tkClass);
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(wxString::Format("FindCurrentFunctionStart() Found %zu results", num_results));

    TokenTree* tree = m_Parser->GetTokenTree();

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    const int idx = GetTokenFromCurrentLine(tree, result, curLine, searchData->file);
    const Token* token = tree->at(idx);
    if (token)
    {
        // got it :)
        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("FindCurrentFunctionStart() Current function: '%s' (at line %u)",
                                                       token->DisplayName(), token->m_ImplLine));

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
                    if (g_DebugSmartSense)
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

        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(wxString::Format("FindCurrentFunctionStart() Namespace='%s', proc='%s' (returning %d)",
                                                       m_LastNamespace, m_LastPROC, m_LastResult));

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        return m_LastResult;
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog("FindCurrentFunctionStart() Can't determine current function...");

    m_LastResult = -1;
    return -1;
}

bool ParseManager::ParseUsingNamespace(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos)
{
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(_T("ParseUsingNamespace() Parse file scope for \"using namespace\""));
    TRACE(_T("ParseManager::ParseUsingNamespace()"));

    int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
    if (pos < 0 || pos > searchData->control->GetLength())
        return false;

    // Get the buffer from begin of the editor to the current caret position
    wxString buffer = searchData->control->GetTextRange(0, pos);

    return ParseBufferForUsingNamespace(buffer, search_scope);
}

bool ParseManager::ParseBufferForUsingNamespace(const wxString& buffer, TokenIdxSet& search_scope, bool bufferSkipBlocks)
{
    wxArrayString ns;
    m_Parser->ParseBufferForUsingNamespace(buffer, ns, bufferSkipBlocks);

    TokenTree* tree = m_Parser->GetTokenTree();

    CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

    for (size_t i = 0; i < ns.GetCount(); ++i)
    {
        std::queue<ParserComponent> components;
        BreakUpComponents(ns[i], components);

        int parentIdx = -1;
        while (!components.empty())
        {
            ParserComponent pc = components.front();
            components.pop();

            int id = tree->TokenExists(pc.component, parentIdx, tkNamespace);
            if (id == -1)
            {
                parentIdx = -1;
                break;
            }
            parentIdx = id;
        }

        if (g_DebugSmartSense && parentIdx != -1)
        {
            const Token* token = tree->at(parentIdx);
            if (token)
                CCLogger::Get()->DebugLog(wxString::Format("ParseUsingNamespace() Found %s%s",
                                                           token->GetNamespace(), token->m_Name));
        }
        search_scope.insert(parentIdx);
    }

    CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    return true;
}

bool ParseManager::ParseFunctionArguments(ccSearchData* searchData, int caretPos)
{
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() Parse function arguments"));
    TRACE(_T("ParseManager::ParseFunctionArguments()"));

    TokenIdxSet proc_result;

    TokenTree* tree = m_Parser->GetTokenTree(); // the one used inside FindCurrentFunctionToken, FindAIMatches and GenerateResultSet

    size_t found_at = FindCurrentFunctionToken(searchData, proc_result, caretPos);
    if (!found_at)
    {
        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() Could not determine current function's namespace..."));
        TRACE(_T("ParseFunctionArguments() Could not determine current function's namespace..."));
        return false;
    }

    const int pos = caretPos == -1 ? searchData->control->GetCurrentPos() : caretPos;
    const unsigned int curLine = searchData->control->LineFromPosition(pos) + 1;

    bool locked = false;
    for (TokenIdxSet::const_iterator tis_it = proc_result.begin(); tis_it != proc_result.end(); ++tis_it)
    {
        wxString buffer;
        int initLine = -1;
        int tokenIdx = -1;

        if (locked)
            CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)
        locked = true;

        const Token* token = tree->at(*tis_it);

        if (!token)
            continue;
        if (curLine < token->m_ImplLineStart || curLine > token->m_ImplLineEnd)
            continue;

        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() + Function match: ") + token->m_Name);
        TRACE(_T("ParseFunctionArguments() + Function match: ") + token->m_Name);

        if (!token->m_Args.IsEmpty() && !token->m_Args.Matches(_T("()")))
        {
            buffer = token->m_Args;
            // Now we have something like "(int my_int, const TheClass* my_class, float f)"
            buffer.Remove(0, 1);              // remove (
            buffer.RemoveLast();              // remove )
            // Now we have                "int my_int, const TheClass* my_class, float f"
            buffer.Replace(_T(","), _T(";")); // replace commas with semi-colons
            // Now we have                "int my_int; const TheClass* my_class; float f"
            buffer << _T(';');                // aid parser ;)
            // Finally we have            "int my_int; const TheClass* my_class; float f;"
            buffer.Trim();

            if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(wxString::Format("ParseFunctionArguments() Parsing arguments: \"%s\"", buffer));

            if (!buffer.IsEmpty())
            {
                const int textLength= searchData->control->GetLength();
                if (textLength == -1)
                    continue;
                int paraPos = searchData->control->PositionFromLine(token->m_ImplLine - 1);
                if (paraPos == -1)
                    continue;
                while (paraPos < textLength && searchData->control->GetCharAt(paraPos++) != _T('('))
                    ;
                while (paraPos < textLength && searchData->control->GetCharAt(paraPos++) < _T(' '))
                    ;
                initLine = searchData->control->LineFromPosition(paraPos) + 1;
                if (initLine == -1)
                    continue;
                tokenIdx = token->m_Index;
            }
        }

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
        locked = false;

        if (   !buffer.IsEmpty()
            && !m_Parser->ParseBuffer(buffer, false, false, true, searchData->file, tokenIdx, initLine)
            && g_DebugSmartSense)
        {
            CCLogger::Get()->DebugLog(_T("ParseFunctionArguments() Error parsing arguments."));
        }
    }

    if (locked)
        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

    return true;
}

bool ParseManager::ParseLocalBlock(ccSearchData* searchData, TokenIdxSet& search_scope, int caretPos)
{
    if (g_DebugSmartSense)
        CCLogger::Get()->DebugLog(_T("ParseLocalBlock() Parse local block"));
    TRACE(_T("ParseManager::ParseLocalBlock()"));

    int parentIdx = -1;
    int blockStart = FindCurrentFunctionStart(searchData, nullptr, nullptr, &parentIdx, caretPos);
    int initLine = 0;
    if (parentIdx != -1)
    {
        TokenTree* tree = m_Parser->GetTokenTree();

        CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

        const Token* parent = tree->at(parentIdx);
        if (parent && (parent->m_TokenKind & tkAnyFunction))
        {
            m_LastFuncTokenIdx = parent->m_Index;
            initLine = parent->m_ImplLineStart;
        }

        CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)

        // only need to parse the function body, other type of Tokens' body such as class declaration
        // should not be parsed.
        if (!parent || !(parent->m_TokenKind & tkAnyFunction))
            return false;
    }

    if (blockStart != -1)
    {
        cbStyledTextCtrl* stc = searchData->control;
        // if we are in a function body, then blockStart points to the '{', so we just skip the '{'.
        if (stc->GetCharAt(blockStart) == wxT('{'))
            ++blockStart;
        const int pos         = (caretPos == -1 ? stc->GetCurrentPos() : caretPos);
        const int line        = stc->LineFromPosition(pos);
        const int blockEnd    = stc->GetLineEndPosition(line);
        if (blockEnd < 0 || blockEnd > stc->GetLength())
        {
            if (g_DebugSmartSense)
            {
                CCLogger::Get()->DebugLog(wxString::Format("ParseLocalBlock() ERROR blockEnd=%d and edLength=%d?!",
                                                           blockEnd, stc->GetLength()));
            }
            return false;
        }

        if (blockStart >= blockEnd)
            blockStart = blockEnd;

//        wxString buffer = searchData->control->GetTextRange(blockStart, blockEnd);
        wxString buffer;
        // condense out-of-scope braces {...}
        int scanPos = blockEnd;
        for (int curPos = pos; curPos > blockStart; --curPos)
        {
            if (stc->GetCharAt(curPos) != wxT('}'))
                continue;
            const int style = stc->GetStyleAt(curPos);
            if (   stc->IsString(style)
                || stc->IsCharacter(style)
                || stc->IsComment(style))
            {
                continue;
            }
            const int scopeStart = stc->BraceMatch(curPos);
            if (scopeStart < blockStart)
                break;
            buffer.Prepend(stc->GetTextRange(curPos, scanPos));
            int startLn = stc->LineFromPosition(scopeStart);
            int endLn   = stc->LineFromPosition(curPos);
            if (startLn < endLn) // maintain correct line numbers for parsed tokens
                buffer.Prepend( wxString(wxT('\n'), endLn - startLn) );
            scanPos = scopeStart + 1;
            curPos  = scopeStart;

            // condense out-of-scope for/if/while declarations
            int prevCharIdx = scopeStart - 1;
            for (; prevCharIdx > blockStart; --prevCharIdx)
            {
                if (stc->IsComment(stc->GetStyleAt(prevCharIdx)))
                    continue;
                if (!wxIsspace(stc->GetCharAt(prevCharIdx)))
                    break;
            }
            if (stc->GetCharAt(prevCharIdx) != wxT(')'))
                continue;
            const int paramStart = stc->BraceMatch(prevCharIdx);
            if (paramStart < blockStart)
                continue;
            for (prevCharIdx = paramStart - 1; prevCharIdx > blockStart; --prevCharIdx)
            {
                if (stc->IsComment(stc->GetStyleAt(prevCharIdx)))
                    continue;
                if (!wxIsspace(stc->GetCharAt(prevCharIdx)))
                    break;
            }
            const wxString text = stc->GetTextRange(stc->WordStartPosition(prevCharIdx, true),
                                                    stc->WordEndPosition(  prevCharIdx, true));
            if (text == wxT("for"))
                buffer.Prepend(wxT("(;;){"));
            else if (text == wxT("if") || text == wxT("while") || text == wxT("catch"))
                buffer.Prepend(wxT("(0){"));
            else
                continue;
            startLn = stc->LineFromPosition(prevCharIdx);
            endLn   = stc->LineFromPosition(scopeStart);
            if (startLn < endLn)
                buffer.Prepend( wxString(wxT('\n'), endLn - startLn) );
            curPos  = stc->WordStartPosition(prevCharIdx, true);
            scanPos = stc->WordEndPosition(  prevCharIdx, true);
        }
        buffer.Prepend(stc->GetTextRange(blockStart, scanPos));

        buffer.Trim();

        ParseBufferForUsingNamespace(buffer, search_scope, false);

        if (   !buffer.IsEmpty()
            && !m_Parser->ParseBuffer(buffer, false, false, true, searchData->file, m_LastFuncTokenIdx, initLine) )
        {
            if (g_DebugSmartSense)
                CCLogger::Get()->DebugLog(_T("ParseLocalBlock() ERROR parsing block:\n") + buffer);
        }
        else
        {
            if (g_DebugSmartSense)
            {
                CCLogger::Get()->DebugLog(wxString::Format("ParseLocalBlock() Block:\n%s", buffer));
                CCLogger::Get()->DebugLog("ParseLocalBlock() Local tokens:");

                TokenTree* tree = m_Parser->GetTokenTree();

                CC_LOCKER_TRACK_TT_MTX_LOCK(s_TokenTreeMutex)

                for (size_t i = 0; i < tree->size(); ++i)
                {
                    const Token* token = tree->at(i);
                    if (token && token->m_IsTemp)
                    {
                        wxString log(wxString::Format(" + %s (%d)", token->DisplayName(), token->m_Index));
                        const Token* parent = tree->at(token->m_ParentIndex);
                        if (parent)
                            log += wxString::Format("; Parent = %s (%d)", parent->m_Name, token->m_ParentIndex);
                        CCLogger::Get()->DebugLog(log);
                    }
                }

                CC_LOCKER_TRACK_TT_MTX_UNLOCK(s_TokenTreeMutex)
            }
            return true;
        }
    }
    else
    {
        if (g_DebugSmartSense)
            CCLogger::Get()->DebugLog(_T("ParseLocalBlock() Could not determine current block start..."));
    }
    return false;
}

bool ParseManager::AddCompilerDirs(cbProject* project, ParserBase* parser)
{
    if (!parser)
        return false;

    TRACE(_T("ParseManager::AddCompilerDirs: Enter"));

    // If there is no project, work on default compiler
    if (!project)
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
    memset(Compilers, 0, sizeof(Compiler*) * nCompilers);
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
//            target->GetCustomVars().ApplyVarsToEnvironment();
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

bool ParseManager::AddCompilerPredefinedMacros(cbProject* project, ParserBase* parser)
{
    if (!parser)
        return false;

    if (!parser->Options().wantPreprocessor)
        return false;

    TRACE(_T("ParseManager::AddCompilerPredefinedMacros: Enter"));

    // Default compiler is used for for single file parser (non project)
    wxString compilerId = project ? project->GetCompilerID() : CompilerFactory::GetDefaultCompilerID();

    wxString defs;
    // gcc
    if (compilerId.Contains(_T("gcc")))
    {
        if ( !AddCompilerPredefinedMacrosGCC(compilerId, project, defs, parser) )
            return false;
    }
    // vc
    else if (compilerId.StartsWith(_T("msvc")))
    {
        if ( !AddCompilerPredefinedMacrosVC(compilerId, defs, parser) )
          return false;
    }

    TRACE(_T("ParseManager::AddCompilerPredefinedMacros: Add compiler predefined preprocessor macros:\n%s"), defs.wx_str());
    parser->AddPredefinedMacros(defs);

    TRACE(_T("ParseManager::AddCompilerPredefinedMacros: Leave"));
    if ( defs.IsEmpty() )
        return false;

    return true;
}

bool ParseManager::AddCompilerPredefinedMacrosGCC(const wxString& compilerId, cbProject* project, wxString& defs, ParserBase* parser)
{
    Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
    if (!compiler)
        return false;

    bool canLog = Manager::Get()->GetConfigManager("code_completion")->ReadBool("CCDebugLogging");

    if (parser->Options().platformCheck && !compiler->SupportsCurrentPlatform())
    {
        TRACE(_T("ParseManager::AddCompilerPredefinedMacrosGCC: Not supported on current platform!"));
        return false;
    }

    wxString sep = (platform::windows ? _T("\\") : _T("/"));
    wxString cpp_compiler = compiler->GetMasterPath() + sep + _T("bin") + sep + compiler->GetPrograms().CPP;
    Manager::Get()->GetMacrosManager()->ReplaceMacros(cpp_compiler);

    static std::map<wxString, wxString> gccDefsMap;
    if ( gccDefsMap[cpp_compiler].IsEmpty() )
    {
        // Check if user set language standard version to use
        wxString standard = GetCompilerStandardGCC(compiler, project);

        // Different command on Windows and other OSes
#ifdef __WXMSW__
        const wxString args(wxString::Format(_T(" -E -dM -x c++ %s nul"), standard.wx_str()) );
#else
        const wxString args(wxString::Format(_T(" -E -dM -x c++ %s /dev/null"), standard.wx_str()) );
#endif

        wxArrayString output, error;
        if ( !SafeExecute(compiler->GetMasterPath(), compiler->GetExtraPaths(), compiler->GetPrograms().CPP, args, output, error) )
            return false;

        // wxExecute can be a long action and C::B might have been shutdown in the meantime...
        if ( Manager::IsAppShuttingDown() )
            return false;

        wxString& gccDefs = gccDefsMap[cpp_compiler];
        for (size_t i = 0; i < output.Count(); ++i)
            gccDefs += output[i] + _T("\n");

        if (canLog)
            CCLogger::Get()->DebugLog(_T("ParseManager::AddCompilerPredefinedMacrosGCC: Caching predefined macros for compiler '")
                                   + cpp_compiler + _T("':\n") + gccDefs);
    }

    defs = gccDefsMap[cpp_compiler];

    return true;
}

wxString ParseManager::GetCompilerStandardGCC(Compiler* compiler, cbProject* project)
{
    // Check if user set language standard version to use
    // 1.) Global compiler settings are first to search in
    wxString standard = GetCompilerUsingStandardGCC(compiler->GetCompilerOptions());
    if (standard.IsEmpty() && project)
    {
        // 2.) Project compiler setting are second
        standard = GetCompilerUsingStandardGCC(project->GetCompilerOptions());

        // 3.) And targets are third in row to look for standard
        // NOTE: If two targets use different standards, only the one we
        //       encounter first (eg. c++98) will be used, and any other
        //       disregarded (even if it would be c++1y)
        if (standard.IsEmpty())
        {
            for (int i=0; i<project->GetBuildTargetsCount(); ++i)
            {
                ProjectBuildTarget* target = project->GetBuildTarget(i);
                standard = GetCompilerUsingStandardGCC(target->GetCompilerOptions());

                if (!standard.IsEmpty())
                    break;
            }
        }
    }
    return standard;
}

wxString ParseManager::GetCompilerUsingStandardGCC(const wxArrayString& compilerOptions)
{
    wxString standard;
    for (wxArrayString::size_type i=0; i<compilerOptions.Count(); ++i)
    {
        if (compilerOptions[i].StartsWith(_T("-std=")))
        {
            standard = compilerOptions[i];
            CCLogger::Get()->DebugLog(wxString::Format(_T("ParseManager::GetCompilerUsingStandardGCC: Using language standard: %s"), standard.wx_str()));
            break;
        }
    }
    return standard;
}

bool ParseManager::AddCompilerPredefinedMacrosVC(const wxString& compilerId, wxString& defs, ParserBase* parser)
{
    static wxString vcDefs;
    static bool     firstExecute = true;

    if (!firstExecute)
    {
        defs = vcDefs;
        return true;
    }

    firstExecute = false;
    Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
    if (!compiler)
        return false;

    if (parser->Options().platformCheck && !compiler->SupportsCurrentPlatform())
    {
        TRACE(_T("ParseManager::AddCompilerPredefinedMacrosVC: Not supported on current platform!"));
        return false;
    }

    wxArrayString output, error;
    if ( !SafeExecute(compiler->GetMasterPath(), compiler->GetExtraPaths(), compiler->GetPrograms().C, wxEmptyString, output, error) )
        return false;

    // wxExecute can be a long action and C::B might have been shutdown in the meantime...
    if ( Manager::IsAppShuttingDown() )
        return false;

    if (error.IsEmpty())
    {
        TRACE(_T("ParseManager::AddCompilerPredefinedMacrosVC: Can't get pre-defined macros for MSVC."));
        return false;
    }

    wxString compilerVersionInfo = error[0];
    wxString tmp(_T("Microsoft (R) "));
    int pos = compilerVersionInfo.Find(tmp);
    if (pos != wxNOT_FOUND)
    {
        // in earlier versions of MSVC the compiler shows "32 bit" or "64 bit"
        // in more recent MSVC version the architecture (x86 or x64) is shown instead
        wxString bit = compilerVersionInfo.Mid(pos + tmp.Length(), 2);
        if      ( (bit.IsSameAs(_T("32"))) || compilerVersionInfo.Contains(_T("x86")) )
            defs += _T("#define _WIN32") _T("\n");
        else if ( (bit.IsSameAs(_T("64"))) || compilerVersionInfo.Contains(_T("x64")) )
            defs += _T("#define _WIN64") _T("\n");
    }

    tmp = _T("Compiler Version ");
    pos = compilerVersionInfo.Find(tmp);
    if (pos != wxNOT_FOUND)
    {
        wxString ver = compilerVersionInfo.Mid(pos + tmp.Length(), 4); // is i.e. 12.0
        pos = ver.Find(_T('.'));
        if (pos != wxNOT_FOUND)
        {
            // out of "12.0" make "1200" for the #define
            ver[pos]     = ver[pos + 1]; // move the mintor version first number to the dot position
            ver[pos + 1] = _T('0');      // add another zero at the end
            defs += _T("#define _MSC_VER ") + ver;
            // Known to now (see https://en.wikipedia.org/wiki/Visual_C%2B%2B):
            // MSVC++ 12.0 _MSC_VER = 1800 (Visual Studio 2013)
            // MSVC++ 11.0 _MSC_VER = 1700 (Visual Studio 2012)
            // MSVC++ 10.0 _MSC_VER = 1600 (Visual Studio 2010)
            // MSVC++ 9.0  _MSC_VER = 1500 (Visual Studio 2008)
            // MSVC++ 8.0  _MSC_VER = 1400 (Visual Studio 2005)
            // MSVC++ 7.1  _MSC_VER = 1310 (Visual Studio 2003)
            // MSVC++ 7.0  _MSC_VER = 1300
            // MSVC++ 6.0  _MSC_VER = 1200
            // MSVC++ 5.0  _MSC_VER = 1100
        }
    }

    defs = vcDefs;
    return true;
}

bool ParseManager::AddProjectDefinedMacros(cbProject* project, ParserBase* parser)
{
    if (!parser)
        return false;

    if (!project)
        return true;

    TRACE(_T("ParseManager::AddProjectDefinedMacros: Enter"));

    wxString compilerId = project->GetCompilerID();
    wxString defineCompilerSwitch(wxEmptyString);
    if (compilerId.Contains(_T("gcc")))
        defineCompilerSwitch = _T("-D");
    else if (compilerId.StartsWith(_T("msvc")))
        defineCompilerSwitch = _T("/D");

    if (defineCompilerSwitch.IsEmpty())
        return false; // no compiler options, return false

    wxString defs;
    wxArrayString opts;
    if (   !parser->Options().platformCheck
        || (parser->Options().platformCheck && project->SupportsCurrentPlatform()) )
    {
        opts = project->GetCompilerOptions();
    }

    ProjectBuildTarget* target = project->GetBuildTarget(project->GetActiveBuildTarget());
    if (target != NULL)
    {
        if (   !parser->Options().platformCheck
            || (parser->Options().platformCheck && target->SupportsCurrentPlatform()) )
        {
            wxArrayString targetOpts = target->GetCompilerOptions();
            for (size_t i = 0; i < targetOpts.GetCount(); ++i)
                opts.Add(targetOpts[i]);
        }
    }
    // In case of virtual targets, collect the defines from all child targets.
    wxArrayString targets = project->GetExpandedVirtualBuildTargetGroup(project->GetActiveBuildTarget());
    for (size_t i = 0; i < targets.GetCount(); ++i)
    {
        target = project->GetBuildTarget(targets[i]);
        if (target != NULL)
        {
            if (   !parser->Options().platformCheck
                || (parser->Options().platformCheck && target->SupportsCurrentPlatform()) )
            {
                wxArrayString targetOpts = target->GetCompilerOptions();
                for (size_t j = 0; j < targetOpts.GetCount(); ++j)
                    opts.Add(targetOpts[j]);
            }
        }
    }

    for (size_t i = 0; i < opts.GetCount(); ++i)
    {
        wxString def = opts[i];
        Manager::Get()->GetMacrosManager()->ReplaceMacros(def);
        if ( !def.StartsWith(defineCompilerSwitch) )
            continue;

        def = def.Right(def.Length() - defineCompilerSwitch.Length());
        int pos = def.Find(_T('='));
        if (pos != wxNOT_FOUND)
            def[pos] = _T(' ');

        defs += _T("#define ") + def + _T("\n");
    }

    TRACE(_T("Add project and current build target defined preprocessor macros:\n%s"), defs.wx_str());
    parser->AddPredefinedMacros(defs);
    TRACE(_T("ParseManager::AddProjectDefinedMacros: Leave"));
    if ( defs.IsEmpty() )
        return false;

    return true;
}

void ParseManager::AddCompilerIncludeDirsToParser(const Compiler* compiler, ParserBase* parser)
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
            AddGCCCompilerDirs(compiler->GetMasterPath(), compiler->GetExtraPaths(), compiler->GetPrograms().CPP, parser);
    }
}

// These dirs are the built-in search dirs of the compiler itself (GCC).
// Such as when you install your MinGW GCC in E:/code/MinGW/bin
// The built-in search dir may contain: E:/code/MinGW/include
const wxArrayString& ParseManager::GetGCCCompilerDirs(const wxString& cpp_path, const wxArrayString& extra_path, const wxString& cpp_executable)
{
    wxString sep = (platform::windows ? _T("\\") : _T("/"));
    wxString cpp_compiler = cpp_path + sep + _T("bin") + sep + cpp_executable;
    Manager::Get()->GetMacrosManager()->ReplaceMacros(cpp_compiler);
   bool canLog =  Manager::Get()->GetConfigManager("code_completion")->ReadBool("CCDebugLogging");

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
    if ( !SafeExecute(cpp_path, extra_path, cpp_executable, args, output, error) )
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

        wxFileName fname(path, wxEmptyString);
        fname.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG | wxPATH_NORM_SHORTCUT);
        fname.SetVolume(fname.GetVolume().MakeUpper());
        if (!fname.DirExists())
            break;

        dirs[cpp_compiler].Add(fname.GetPath());

        if(canLog)
            CCLogger::Get()->DebugLog(_T("ParseManager::GetGCCCompilerDirs: Caching GCC default include dir: ") + fname.GetPath());
    }

    TRACE(_T("ParseManager::GetGCCCompilerDirs: Leave"));
    return dirs[cpp_compiler];
}

void ParseManager::AddGCCCompilerDirs(const wxString& masterPath, const wxArrayString& extraPath, const wxString& compilerCpp, ParserBase* parser)
{
    const wxArrayString& gccDirs = GetGCCCompilerDirs(masterPath, extraPath, compilerCpp);
    TRACE(wxString::Format("ParseManager::AddGCCCompilerDirs: Adding %zu cached gcc dirs to parser...", gccDirs.GetCount()));
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
        if ( !base.IsEmpty() )
        {
            wxFileName fn(dir);
            if ( NormalizePath(fn, base) )
            {
                parser->AddIncludeDir(fn.GetFullPath());
                TRACE(_T("ParseManager::AddIncludeDirsToParser: Adding directory to parser: ") + fn.GetFullPath());
            }
            else
                CCLogger::Get()->DebugLog(wxString::Format("ParseManager::AddIncludeDirsToParser: Error normalizing path: '%s' from '%s'", dir, base));
        }
        else
            parser->AddIncludeDir(dir); // no base path, nothing to normalise
    }
}

bool ParseManager::SafeExecute(const wxString& app_path, const wxArrayString& extra_path, const wxString& app, const wxString& args, wxArrayString& output, wxArrayString& error)
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
        wxString tmp_path_env = pth + (platform::windows ? _T(";") : _T(":"));
        if ( extra_path.GetCount() > 0 )
        {
            for (size_t i = 0; i < extra_path.GetCount(); ++i)
            {
                wxString expth = extra_path[i];
                if ( !expth.IsEmpty() )
                {
                    Manager::Get()->GetMacrosManager()->ReplaceMacros(expth);
                    while (!expth.IsEmpty() && (expth.Last() == '\\' || expth.Last() == '/'))
                        expth.RemoveLast();

                    if (!expth.Trim().IsEmpty())
                        tmp_path_env = tmp_path_env + expth + (platform::windows ? _T(";") : _T(":"));
                }
            }
        }
        tmp_path_env = tmp_path_env + path_env;
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

void ParseManager::OnParserStart(wxCommandEvent& event)
{
    TRACE(_T("ParseManager::OnParserStart: Enter"));

    cbProject* project = static_cast<cbProject*>(event.GetClientData());
    wxString   prj     = (project ? project->GetTitle() : _T("*NONE*"));
    const ParserCommon::ParserState state = static_cast<ParserCommon::ParserState>(event.GetInt());

    switch (state)
    {
        case ParserCommon::ptCreateParser:
            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Starting batch parsing for project '%s'..."), prj));
            {
                std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
                if (info.second && m_Parser != info.second)
                {
                    CCLogger::Get()->DebugLog("ParseManager::OnParserStart: Start switch from OnParserStart::ptCreateParser");
                    SwitchParser(info.first, info.second); // Calls SetParser() which also calls UpdateClassBrowserView()
                }
            }
            break;

        case ParserCommon::ptAddFileToParser:
            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Starting add file parsing for project '%s'..."), prj));
            break;

        case ParserCommon::ptReparseFile:
            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Starting re-parsing for project '%s'..."), prj));
            break;

        case ParserCommon::ptUndefined:
            if (event.GetString().IsEmpty())
                CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: Batch parsing error in project '%s'"), prj));
            else
                CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::OnParserStart: %s in project '%s'"), event.GetString(), prj));
            return;

        default:
            break;
    }

    event.Skip();

    TRACE(_T("ParseManager::OnParserStart: Leave"));
}

// ----------------------------------------------------------------------------
void ParseManager::OnParserEnd(wxCommandEvent& event)
// ----------------------------------------------------------------------------
{
    TRACE(_T("ParseManager::OnParserEnd: Enter"));

    ParserBase* parser = reinterpret_cast<ParserBase*>(event.GetEventObject());
    cbProject* project = static_cast<cbProject*>(event.GetClientData());
    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    const ParserCommon::ParserState state = static_cast<ParserCommon::ParserState>(event.GetInt());

    switch (state)
    {
        case ParserCommon::ptCreateParser:
            {
                wxString log(wxString::Format(_("ParseManager::OnParserEnd: Project '%s' parsing stage done!"), prj));
                CCLogger::Get()->Log(log);
                CCLogger::Get()->DebugLog(log);
            }
            break;

        case ParserCommon::ptAddFileToParser:
            break;

        case ParserCommon::ptReparseFile:
            if (parser != m_Parser)
            {
                std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
                if (info.second && info.second != m_Parser)
                {
                    CCLogger::Get()->DebugLog(_T("ParseManager::OnParserEnd: Start switch from OnParserEnd::ptReparseFile"));
                    SwitchParser(info.first, info.second); // Calls SetParser() which also calls UpdateClassBrowserView()
                }
            }
            break;

        case ParserCommon::ptUndefined:
            CCLogger::Get()->DebugLog(wxString::Format("ParseManager::OnParserEnd: Parser event handling error of project '%s'", prj));
            return;

        default:
            break;
    }

    if (!event.GetString().IsEmpty())
        CCLogger::Get()->DebugLog(event.GetString());

    UpdateClassBrowser();

    // In this case, the parser will record all the cbprojects' token, so this will start parsing
    // the next cbproject.
    TRACE(_T("ParseManager::OnParserEnd: Starting m_TimerParsingOneByOne."));
    m_TimerParsingOneByOne.Start(500, wxTIMER_ONE_SHOT);

    // both ParseManager and CodeCompletion class need to handle this event
    event.Skip();
    TRACE(_T("ParseManager::OnParserEnd: Leave"));
}//end OnParserEnd

void ParseManager::OnParsingOneByOneTimer(cb_unused wxTimerEvent& event)
{
    TRACE(_T("ParseManager::OnParsingOneByOneTimer: Enter"));

    std::pair<cbProject*, ParserBase*> info = GetParserInfoByCurrentEditor();
    if (m_ParserPerWorkspace)
    {
        // If there is no parser and an active editor file can be obtained, parse the file according the active project
        if (!info.second && Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor())
        {
            // NOTE (Morten#1#): Shouldn't this actually be a temp parser??? I think this screws things with re-opening files on load of a projects...
            AddProjectToParser(info.first);
            CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Add foreign active editor to current active project's parser."));
        }
        // Otherwise, there is a parser already present
        else
        {
            // First: try to parse the active project (if any)
            cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
            if (m_ParsedProjects.find(activeProject) == m_ParsedProjects.end())
            {
                AddProjectToParser(activeProject);
                CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Add new (un-parsed) active project to parser."));
            }
            // Else: add remaining projects one-by-one (if any)
            else
            {
                ProjectsArray* projs = Manager::Get()->GetProjectManager()->GetProjects();
                // loop on the whole workspace, and only add a new project to the parser
                // here the "new" means a project haven't been parsed. Once it was parsed, it is
                // added to the m_ParsedProjects
                for (size_t i = 0; i < projs->GetCount(); ++i)
                {
                    // Only add, if the project is not already parsed
                    if (m_ParsedProjects.find(projs->Item(i)) == m_ParsedProjects.end())
                    {
                        // AddProjectToParser return true means there are something need to parse, otherwise, it is false
                        if (!AddProjectToParser(projs->Item(i)))
                        {
                            CCLogger::Get()->Log(_("ParseManager::OnParsingOneByOneTimer: Nothing needs to be parseed in this project, switching to next project..."));
                            continue;
                        }

                        CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Add new (next) project to parser."));
                        break;
                    }
                }
            }
        }
    }
    else if (info.first && !info.second)
    {
        info.second = CreateParser(info.first);
        if (info.second && info.second != m_Parser)
        {
            CCLogger::Get()->DebugLog(_T("ParseManager::OnParsingOneByOneTimer: Start switch from OnParsingOneByOneTimer"));
            SwitchParser(info.first, info.second); // Calls SetParser() which also calls UpdateClassBrowserView()
        }
    }
    TRACE(_T("ParseManager::OnParsingOneByOneTimer: Leave"));
}

void ParseManager::OnEditorActivated(EditorBase* editor)
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
            DeleteParser(NULL);
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
            parser = m_TempParser; // do *not* instead by SetParser(m_TempParser)
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
        if      (m_Parser->ClassBrowserOptions().displayFilter == bdfFile)
            m_ClassBrowser->UpdateClassBrowserView(true); // check header and implementation file swap
        else if (   m_ParserPerWorkspace // project view only available in case of one parser per WS
                 && m_Parser->ClassBrowserOptions().displayFilter == bdfProject)
            m_ClassBrowser->UpdateClassBrowserView();
    }
}

void ParseManager::OnEditorClosed(EditorBase* editor)
{
    // the caller of the function should guarantee its a built-in editor
    wxString filename = editor->GetFilename();
    const int pos = m_StandaloneFiles.Index(filename);
    if (pos != wxNOT_FOUND)
    {
        m_StandaloneFiles.RemoveAt(pos);
        if (m_StandaloneFiles.IsEmpty())
            DeleteParser(NULL);
        else
            RemoveFileFromParser(NULL, filename);
    }
}

void ParseManager::InitCCSearchVariables()
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

bool ParseManager::AddProjectToParser(cbProject* project)
{
    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    ParserBase* parser = GetParserByProject(project);
    if (parser)
        return false;

    if (m_ParsedProjects.empty())
        return false;

    m_ParsedProjects.insert(project);
    parser = GetParserByProject(project);
    if (!parser)
        return false;
    else if (!parser->UpdateParsingProject(project))
    {
        m_ParsedProjects.erase(project);
        return false;
    }

    // TODO (ollydbg#1#) did exactly the same thing as the function ParseManager::DoFullParsing()?
    wxString log(wxString::Format(_("ParseManager::AddProjectToParser: Add project (%s) to parser"), prj));
    CCLogger::Get()->Log(log);
    CCLogger::Get()->DebugLog(log);

    bool needParseMacros = false;

    if (!AddCompilerDirs(project, parser))
        CCLogger::Get()->DebugLog(_T("ParseManager::AddProjectToParser: AddCompilerDirs failed!"));

    if (!AddCompilerPredefinedMacros(project, parser))
        CCLogger::Get()->DebugLog(_T("ParseManager::AddProjectToParser: AddCompilerPredefinedMacros failed!"));
    else
        needParseMacros = true;

    if (!AddProjectDefinedMacros(project, parser))
        CCLogger::Get()->DebugLog(_T("ParseManager::AddProjectToParser: AddProjectDefinedMacros failed!"));
    else
    {
        if(!needParseMacros)
            needParseMacros = true;
    }

    if (project)
    {
        size_t fileCount = 0;
        for (FilesList::const_iterator fl_it = project->GetFilesList().begin(); fl_it != project->GetFilesList().end(); ++fl_it)
        {
            ProjectFile* pf = *fl_it;
            if (pf && FileTypeOf(pf->relativeFilename) == ftHeader)
            {
                if ( AddFileToParser(project, pf->file.GetFullPath(), parser) )
                    ++fileCount;
            }
        }
        for (FilesList::const_iterator fl_it = project->GetFilesList().begin(); fl_it != project->GetFilesList().end(); ++fl_it)
        {
            ProjectFile* pf = *fl_it;
            if (pf && (FileTypeOf(pf->relativeFilename) == ftSource || FileTypeOf(pf->relativeFilename) == ftTemplateSource) )
            {
                if ( AddFileToParser(project, pf->file.GetFullPath(), parser) )
                    fileCount++;
            }
        }

        CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::AddProjectToParser: Done adding %zu files of project (%s) to parser."),
                                                   fileCount, prj));

        // in some cases, all the files were already be parsed, so fileCount is still 0
        return ((fileCount>0) || needParseMacros);
    }
    else
    {
        EditorBase* editor = Manager::Get()->GetEditorManager()->GetActiveEditor();
        if (editor && AddFileToParser(project, editor->GetFilename(), parser))
        {
            wxFileName file(editor->GetFilename());
            parser->AddIncludeDir(file.GetPath());
            m_StandaloneFiles.Add(editor->GetFilename());

            CCLogger::Get()->DebugLog(wxString::Format(_("ParseManager::AddProjectToParser: Done adding stand-alone file (%s) of editor to parser."), editor->GetFilename()));
            return true;
        }
    }
    return false;
}

bool ParseManager::RemoveProjectFromParser(cbProject* project)
{
    ParserBase* parser = GetParserByProject(project);
    if (!parser)
        return false;

    // Remove from the cbProject set
    m_ParsedProjects.erase(project);

    if (!project || m_ParsedProjects.empty())
        return true;

    wxString prj = (project ? project->GetTitle() : _T("*NONE*"));
    wxString log(wxString::Format(_("Remove project (%s) from parser"), prj));
    CCLogger::Get()->Log(log);
    CCLogger::Get()->DebugLog(log);

    for (FilesList::const_iterator fl_it = project->GetFilesList().begin(); fl_it != project->GetFilesList().end(); ++fl_it)
    {
        ProjectFile* pf = *fl_it;
        if (pf && ParserCommon::FileType(pf->relativeFilename) != ParserCommon::ftOther)
            RemoveFileFromParser(project, pf->file.GetFullPath());
    }

    return true;
}
// ----------------------------------------------------------------------------
std::unordered_map<cbProject*,ParserBase*> * ParseManager::GetActiveParsers() //(ph 2025/02/14)
// ----------------------------------------------------------------------------
{
    // First, clear the contents of m_ActiveParserList
    m_ActiveParserList.clear();

    // Then, copy the contents of m_ParserList to m_ActiveParserList
    for (const auto& pair : m_ParserList)
    {
        m_ActiveParserList.insert(pair);
    }

    return &m_ActiveParserList;
}
// ----------------------------------------------------------------------------
wxString ParseManager::GetConfigListSelection() //2025/04/21
// ----------------------------------------------------------------------------
{
    wxString activePageTitle;
    // Get the title of the currently active/focused configuration page
    wxWindow* pTopWindow = wxFindWindowByName(_("Configure editor"));
    if (not pTopWindow)
        pTopWindow = GetTopWxWindow();
    if (pTopWindow)
    {
        wxListbook* lb = XRCCTRL(*pTopWindow, "nbMain", wxListbook);
        wxWindow* page = lb ? lb->GetCurrentPage() : nullptr;
        int pageID = page ? lb->FindPage(page) : 0;
        activePageTitle = lb ? lb->GetPageText(pageID) : wxString();
    }
    return activePageTitle;
}
