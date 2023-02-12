/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include <wx/confbase.h>
    #include <wx/fileconf.h>
    #include <wx/intl.h>
    #include <wx/filename.h>
    #include <wx/msgdlg.h>
    #include <wx/stopwatch.h>
    #include "manager.h"
    #include "configmanager.h"
    #include "projectmanager.h"
    #include "logmanager.h"
    #include "macrosmanager.h"
    #include "cbproject.h"
    #include "compilerfactory.h"
    #include "globals.h"
#endif

#include <wx/dir.h>
#include <string>
#include <sstream>

#include <algorithm>
#include "filefilters.h"
#include "projectloader.h"
#include "projectloader_hooks.h"
#include "annoyingdialog.h"
#include "configmanager.h"
#include "tinywxuni.h"
#include "filegroupsandmasks.h"

ProjectLoader::ProjectLoader(cbProject* project)
    : m_pProject(project),
    m_Upgraded(false),
    m_OpenDirty(false),
    m_1_4_to_1_5_deftarget(-1),
    m_IsPre_1_6(false)
{
    //ctor
}

ProjectLoader::~ProjectLoader()
{
    //dtor
}

bool ProjectLoader::Open(const wxString& filename)
{
    return Open(filename, nullptr);
}

bool ProjectLoader::Open(const wxString& filename, TiXmlElement** ppExtensions)
{
    LogManager* pMsg = Manager::Get()->GetLogManager();
    if (!pMsg)
        return false;

    wxStopWatch sw;
    pMsg->DebugLog("Loading project file...");
    TiXmlDocument doc;
    if (!TinyXML::LoadDocument(filename, &doc))
        return false;

    pMsg->DebugLog("Parsing project file...");
    TiXmlElement* root;
    TiXmlElement* proj;

    root = doc.FirstChildElement("CodeBlocks_project_file");
    if (!root)
    {
        // old tag
        root = doc.FirstChildElement("Code::Blocks_project_file");
        if (!root)
        {
            pMsg->DebugLog("Not a valid Code::Blocks project file...");
            return false;
        }
    }
    proj = root->FirstChildElement("Project");
    if (!proj)
    {
        pMsg->DebugLog("No 'Project' element in file...");
        return false;
    }

    m_IsPre_1_2 = false; // flag for some changed defaults in version 1.2
    TiXmlElement* version = root->FirstChildElement("FileVersion");
    // don't show messages if we 're running a batch build (i.e. no gui)
    if (!Manager::IsBatchBuild() && version)
    {
        int major = PROJECT_FILE_VERSION_MAJOR;
        int minor = PROJECT_FILE_VERSION_MINOR;
        version->QueryIntAttribute("major", &major);
        version->QueryIntAttribute("minor", &minor);

        m_IsPre_1_6 = major < 1 || (major == 1 && minor < 6);

        if (major < 1 ||
            (major == 1 && minor < 2))
        {
            // pre-1.2
            pMsg->DebugLog(wxString::Format("Project version is %d.%d. Defaults have changed since then...", major, minor));
            m_IsPre_1_2 = true;
        }
        else if (major >= PROJECT_FILE_VERSION_MAJOR && minor > PROJECT_FILE_VERSION_MINOR)
        {
            pMsg->DebugLog(wxString::Format("Project version is > %d.%d. Trying to load...", PROJECT_FILE_VERSION_MAJOR, PROJECT_FILE_VERSION_MINOR));
            AnnoyingDialog dlg(_("Project file format is newer/unknown"),
                               _("This project file was saved with a newer version of Code::Blocks.\n"
                                 "Will try to load, but you should make sure all the settings were loaded correctly..."),
                                wxART_WARNING,
                                AnnoyingDialog::OK);
            dlg.ShowModal();
        }
        else
        {
            // use one message for all changes
            wxString msg;
            wxString warn_msg;

            // 1.5 -> 1.6: values matching defaults are not written to <Unit> sections
            if (major == 1 && minor == 5)
            {
                msg << _("1.5 to 1.6:\n"
                         "  * only saves values that differ from defaults (i.e. project files are smaller now).\n"
                         "  * added object names generation mode setting (normal/extended).\n"
                         "  * added project notes.\n");
                msg << '\n';

                warn_msg << _("* Project file updated to version 1.6:\n"
                              "   When a project file is saved as version 1.6, it will NO LONGER be read correctly\n"
                              "   by earlier Code::Blocks versions!\n"
                              "   So, if you plan on using this project with an earlier Code::Blocks version, you\n"
                              "   should probably NOT save this project as version 1.6...\n");
                warn_msg << '\n';
            }

            // 1.4 -> 1.5: updated custom build command per-project file
            if (major == 1 && minor == 4)
            {
                msg << _("1.4 to 1.5:\n"
                         "  * added virtual build targets.\n");
                msg << '\n';
            }

            // 1.3 -> 1.4: updated custom build command per-project file
            if (major == 1 && minor == 3)
            {
                msg << _("1.3 to 1.4:\n"
                         "  * changed the way custom file build commands are stored (no auto-conversion).\n");
                msg << '\n';
            }

            if (!msg.IsEmpty())
            {
                m_Upgraded = true;
                msg.Prepend(wxString::Format(_("Project file format is older (%d.%d) than the current format (%d.%d).\n"
                                                "The file will automatically be upgraded on save.\n"
                                                "But please read the following list of changes, as some of them "
                                                "might not automatically convert existing (old) settings.\n"
                                                "If you don't understand what a change means, you probably don't "
                                                "use that feature so you don't have to worry about it.\n\n"
                                                "List of changes:\n"),
                                            major,
                                            minor,
                                            PROJECT_FILE_VERSION_MAJOR,
                                            PROJECT_FILE_VERSION_MINOR));
                AnnoyingDialog dlg(_("Project file format changed"),
                                    msg,
                                    wxART_INFORMATION,
                                    AnnoyingDialog::OK);
                dlg.ShowModal();
            }

            if (!warn_msg.IsEmpty())
            {
                warn_msg.Prepend(_("!!! WARNING !!!\n\n"));
                AnnoyingDialog dlg(_("Project file upgrade warning"),
                                    warn_msg,
                                    wxART_WARNING,
                                    AnnoyingDialog::OK);
                dlg.ShowModal();
            }
        }
    }

    DoProjectOptions(proj);
    DoBuild(proj);
    DoCompilerOptions(proj);
    DoResourceCompilerOptions(proj);
    DoLinkerOptions(proj);
    DoIncludesOptions(proj);
    DoLibsOptions(proj);
    DoExtraCommands(proj);
    DoUnits(proj);

    // if targets still use the "build with all" flag,
    // it's time for conversion
    if (!m_pProject->HasVirtualBuildTarget("All"))
    {
        wxArrayString all;
        for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
        {
            ProjectBuildTarget* bt = m_pProject->GetBuildTarget(i);
            if (bt && bt->GetIncludeInTargetAll())
                all.Add(bt->GetTitle());
        }
        if (all.GetCount())
        {
            m_pProject->DefineVirtualBuildTarget("All", all);
            m_Upgraded = true;
        }
    }

    // convert old deftarget int to string
    if (m_1_4_to_1_5_deftarget != -1)
    {
        ProjectBuildTarget* bt = m_pProject->GetBuildTarget(m_1_4_to_1_5_deftarget);
        if (bt)
            m_pProject->SetDefaultExecuteTarget(bt->GetTitle());
    }

    if (ppExtensions)
        *ppExtensions = nullptr;

    // as a last step, run all hooked callbacks
    TiXmlElement* node = proj->FirstChildElement("Extensions");
    if (node)
    {
        if (ppExtensions)
            *ppExtensions = new TiXmlElement(*node);
        ProjectLoaderHooks::CallHooks(m_pProject, node, true);
    }

    if (!version)
    {
        // pre 1.1 version
        ConvertVersion_Pre_1_1();
        // format changed also:
        // removed <IncludeDirs> and <LibDirs> elements and added them as child elements
        // in <Compiler> and <Linker> elements respectively
        // so set m_Upgraded to true, irrespectively of libs detection...
        m_Upgraded = true;
    }
    else
    {
        // do something important based on version
//        wxString major = version->Attribute("major");
//        wxString minor = version->Attribute("minor");
    }

    pMsg->DebugLog(wxString::Format("Done loading project in %ld ms", sw.Time()));
    return true;
}

void ProjectLoader::ConvertVersion_Pre_1_1()
{
    // ask to detect linker libraries and move them to the new
    // CompileOptionsBase linker libs container
    wxString msg;
    msg.Printf(_("Project \"%s\" was saved with an earlier version of Code::Blocks.\n"
                 "In the current version, link libraries are treated separately from linker options.\n"
                 "Do you want to auto-detect the libraries \"%s\" is using and configure it accordingly?"),
               m_pProject->GetTitle(), m_pProject->GetTitle());

    if (cbMessageBox(msg, _("Question"), wxICON_QUESTION | wxYES_NO) == wxID_YES)
    {
        // project first
        ConvertLibraries(m_pProject);

        for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
        {
            ConvertLibraries(m_pProject->GetBuildTarget(i));
            m_Upgraded = true;
        }
    }
}

void ProjectLoader::ConvertLibraries(CompileTargetBase* object)
{
    wxArrayString linkerOpts = object->GetLinkerOptions();
    wxArrayString linkLibs = object->GetLinkLibs();

    wxString compilerId = object->GetCompilerID();
    Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
    if (!compiler)
        return;
    wxString linkLib = compiler->GetSwitches().linkLibs;
    wxString libExt = compiler->GetSwitches().libExtension;
    size_t libExtLen = libExt.Length();

    size_t i = 0;
    while (i < linkerOpts.GetCount())
    {
        wxString opt = linkerOpts[i];
        if (!linkLib.IsEmpty() && opt.StartsWith(linkLib))
        {
            opt.Remove(0, 2);
            wxString ext = compiler->GetSwitches().libExtension;
            if (!ext.IsEmpty())
                ext = "." + ext;
            linkLibs.Add(compiler->GetSwitches().libPrefix + opt + ext);
            linkerOpts.RemoveAt(i, 1);
        }
        else if (opt.Length() > libExtLen && opt.Right(libExtLen) == libExt)
        {
            linkLibs.Add(opt);
            linkerOpts.RemoveAt(i, 1);
        }
        else
            ++i;
    }

    object->SetLinkerOptions(linkerOpts);
    object->SetLinkLibs(linkLibs);
}

void ProjectLoader::DoMakeCommands(TiXmlElement* parentNode, CompileTargetBase* target)
{
    if (!parentNode)
        return; // no options

    TiXmlElement* node;

    node = parentNode->FirstChildElement("Build");
    if (node && node->Attribute("command"))
        target->SetMakeCommandFor(mcBuild, cbC2U(node->Attribute("command")));

    node = parentNode->FirstChildElement("CompileFile");
    if (node && node->Attribute("command"))
        target->SetMakeCommandFor(mcCompileFile, cbC2U(node->Attribute("command")));

    node = parentNode->FirstChildElement("Clean");
    if (node && node->Attribute("command"))
        target->SetMakeCommandFor(mcClean, cbC2U(node->Attribute("command")));

    node = parentNode->FirstChildElement("DistClean");
    if (node && node->Attribute("command"))
        target->SetMakeCommandFor(mcDistClean, cbC2U(node->Attribute("command")));

    node = parentNode->FirstChildElement("AskRebuildNeeded");
    if (node && node->Attribute("command"))
        target->SetMakeCommandFor(mcAskRebuildNeeded, cbC2U(node->Attribute("command")));

    node = parentNode->FirstChildElement("SilentBuild");
    if (node && node->Attribute("command"))
        target->SetMakeCommandFor(mcSilentBuild, cbC2U(node->Attribute("command")));
}

void ProjectLoader::DoVirtualTargets(TiXmlElement* parentNode)
{
    if (!parentNode)
        return;

    TiXmlElement* node = parentNode->FirstChildElement("Add");
    if (!node)
        return; // no virtual targets

    while (node)
    {
        if (node->Attribute("alias") && node->Attribute("targets"))
        {
            wxString alias = cbC2U(node->Attribute("alias"));
            wxString targets = cbC2U(node->Attribute("targets"));
            wxArrayString arr = GetArrayFromString(targets, ";", true);

            m_pProject->DefineVirtualBuildTarget(alias, arr);
        }

        node = node->NextSiblingElement("Add");
    }
}

void ProjectLoader::DoProjectOptions(TiXmlElement* parentNode)
{
    TiXmlElement* node = parentNode->FirstChildElement("Option");
    if (!node)
        return; // no options

    wxString title;
    wxString makefile;
    bool makefile_custom = false;
    wxString execution_dir;
    wxString defaultTarget;
    wxString compilerId = "gcc";
    bool extendedObjectNames = false;
    wxArrayString vfolders;
    int platformsFinal = spAll;
    PCHMode pch_mode = m_IsPre_1_2 ? pchSourceDir : pchObjectDir;
    bool showNotes = false;
    bool checkFiles = true;
    wxString notes;

    // loop through all options
    while (node)
    {
        if (node->Attribute("title"))
        {
            title = cbC2U(node->Attribute("title"));
            if (title.Trim().IsEmpty())
                title = "untitled";
        }

        else if (node->Attribute("platforms"))
            platformsFinal = GetPlatformsFromString(cbC2U(node->Attribute("platforms")));

        else if (node->Attribute("makefile")) // there is only one attribute per option, so "else" is a safe optimisation
            makefile = UnixFilename(cbC2U(node->Attribute("makefile")));

        else if (node->Attribute("makefile_is_custom"))
            makefile_custom = strncmp(node->Attribute("makefile_is_custom"), "1", 1) == 0;

        else if (node->Attribute("execution_dir"))
            execution_dir = UnixFilename(cbC2U(node->Attribute("execution_dir")));

        // old default_target (int) node
        else if (node->QueryIntAttribute("default_target", &m_1_4_to_1_5_deftarget) == TIXML_SUCCESS)
        {
            // we read the value
        }

        else if (node->Attribute("default_target"))
            defaultTarget = cbC2U(node->Attribute("default_target"));

        else if (node->Attribute("compiler"))
            compilerId = GetValidCompilerID(cbC2U(node->Attribute("compiler")), "the project");

        else if (node->Attribute("extended_obj_names"))
            extendedObjectNames = strncmp(node->Attribute("extended_obj_names"), "1", 1) == 0;

        else if (node->Attribute("pch_mode"))
            pch_mode = (PCHMode)atoi(node->Attribute("pch_mode"));

        else if (node->Attribute("virtualFolders"))
            vfolders = GetArrayFromString(cbC2U(node->Attribute("virtualFolders")), ";");

        else if (node->Attribute("show_notes"))
        {
            TiXmlHandle parentHandle(node);
            TiXmlText* t = (TiXmlText *) parentHandle.FirstChild("notes").FirstChild().Node();
            if (t)
                notes = cbC2U(t->Value());
            showNotes = !notes.IsEmpty() && strncmp(node->Attribute("show_notes"), "1", 1) == 0;
        }
        else if (node->Attribute("check_files"))
            checkFiles = strncmp(node->Attribute("check_files"), "0", 1) != 0;

        node = node->NextSiblingElement("Option");
    }

    m_pProject->SetTitle(title);
    m_pProject->SetPlatforms(platformsFinal);
    m_pProject->SetMakefile(makefile);
    m_pProject->SetMakefileCustom(makefile_custom);
    m_pProject->SetMakefileExecutionDir(execution_dir);
    m_pProject->SetDefaultExecuteTarget(defaultTarget);
    m_pProject->SetCompilerID(compilerId);
    m_pProject->SetExtendedObjectNamesGeneration(extendedObjectNames);
    m_pProject->SetModeForPCH(pch_mode);
    m_pProject->SetVirtualFolders(vfolders);
    m_pProject->SetNotes(notes);
    m_pProject->SetShowNotesOnLoad(showNotes);
    m_pProject->SetCheckForExternallyModifiedFiles(checkFiles);

    DoMakeCommands(parentNode->FirstChildElement("MakeCommands"), m_pProject);
    DoVirtualTargets(parentNode->FirstChildElement("VirtualTargets"));
}

void ProjectLoader::DoBuild(TiXmlElement* parentNode)
{
    TiXmlElement* node = parentNode->FirstChildElement("Build");
    while (node)
    {
        TiXmlElement* opt = node->FirstChildElement("Script");
        while (opt)
        {
            if (opt->Attribute("file"))
                m_pProject->AddBuildScript(cbC2U(opt->Attribute("file")));

            opt = opt->NextSiblingElement("Script");
        }

        DoBuildTarget(node);
        DoEnvironment(node, m_pProject);
        node = node->NextSiblingElement("Build");
    }
}

void ProjectLoader::DoBuildTarget(TiXmlElement* parentNode)
{
    TiXmlElement* node = parentNode->FirstChildElement("Target");
    if (!node)
        return; // no options

    while (node)
    {
        ProjectBuildTarget* target = nullptr;
        wxString title = cbC2U(node->Attribute("title"));
        if (!title.IsEmpty())
            target = m_pProject->AddBuildTarget(title);

        if (target)
        {
            Manager::Get()->GetLogManager()->DebugLog("Loading target " + title);
            DoBuildTargetOptions(node, target);
            DoCompilerOptions(node, target);
            DoResourceCompilerOptions(node, target);
            DoLinkerOptions(node, target);
            DoIncludesOptions(node, target);
            DoLibsOptions(node, target);
            DoExtraCommands(node, target);
            DoEnvironment(node, target);
        }

        node = node->NextSiblingElement("Target");
    }
}

void ProjectLoader::DoBuildTargetOptions(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("Option");
    if (!node)
        return; // no options

    bool use_console_runner = true;
    wxString output;
    wxString imp_lib;
    wxString def_file;
    wxString working_dir;
    wxString obj_output;
    wxString deps_output;
    wxString deps;
    wxString added;
    int type = -1;
    int platformsFinal = spAll;
    wxString compilerId = m_pProject->GetCompilerID();
    wxString parameters;
    wxString hostApplication;
    bool runHostApplicationInTerminal = false;
    bool includeInTargetAll = m_IsPre_1_2 ? true : false;
    bool createStaticLib = false;
    bool createDefFile = false;
    int projectCompilerOptionsRelation = 3;
    int projectLinkerOptionsRelation = 3;
    int projectIncludeDirsRelation = 3;
    int projectLibDirsRelation = 3;
    int projectResIncludeDirsRelation = 3;
    TargetFilenameGenerationPolicy prefixPolicy = tgfpNone; // tgfpNone for compat. with older projects
    TargetFilenameGenerationPolicy extensionPolicy = tgfpNone;

    while (node)
    {
        if (node->Attribute("platforms"))
            platformsFinal = GetPlatformsFromString(cbC2U(node->Attribute("platforms")));

        if (node->Attribute("use_console_runner"))
            use_console_runner = strncmp(node->Attribute("use_console_runner"), "0", 1) != 0;

        if (node->Attribute("output"))
            output = UnixFilename(cbC2U(node->Attribute("output")));

        if (node->Attribute("imp_lib"))
            imp_lib = UnixFilename(cbC2U(node->Attribute("imp_lib")));

        if (node->Attribute("def_file"))
            def_file = UnixFilename(cbC2U(node->Attribute("def_file")));

        if (node->Attribute("prefix_auto"))
            prefixPolicy = atoi(node->Attribute("prefix_auto")) == 1 ? tgfpPlatformDefault : tgfpNone;

        if (node->Attribute("extension_auto"))
            extensionPolicy = atoi(node->Attribute("extension_auto")) == 1 ? tgfpPlatformDefault : tgfpNone;

        if (node->Attribute("working_dir"))
            working_dir = UnixFilename(cbC2U(node->Attribute("working_dir")));

        if (node->Attribute("object_output"))
            obj_output = UnixFilename(cbC2U(node->Attribute("object_output")));

        if (node->Attribute("deps_output"))
            deps_output = UnixFilename(cbC2U(node->Attribute("deps_output")));

        if (node->Attribute("external_deps"))
            deps = UnixFilename(cbC2U(node->Attribute("external_deps")));

        if (node->Attribute("additional_output"))
            added = UnixFilename(cbC2U(node->Attribute("additional_output")));

        if (node->Attribute("type"))
            type = atoi(node->Attribute("type"));

        if (node->Attribute("compiler"))
            compilerId = GetValidCompilerID(cbC2U(node->Attribute("compiler")), target->GetTitle());

        if (node->Attribute("parameters"))
            parameters = cbC2U(node->Attribute("parameters"));

        if (node->Attribute("host_application"))
            hostApplication = UnixFilename(cbC2U(node->Attribute("host_application")));

        if (node->Attribute("run_host_application_in_terminal"))
        {
            wxString runInTerminal = cbC2U(node->Attribute("run_host_application_in_terminal"));
            runHostApplicationInTerminal = (runInTerminal == wxT("1"));
        }

        // used in versions prior to 1.5
        if (node->Attribute("includeInTargetAll"))
            includeInTargetAll = atoi(node->Attribute("includeInTargetAll")) != 0;

        if (node->Attribute("createDefFile"))
            createDefFile = atoi(node->Attribute("createDefFile")) != 0;

        if (node->Attribute("createStaticLib"))
            createStaticLib = atoi(node->Attribute("createStaticLib")) != 0;

        if (node->Attribute("projectCompilerOptionsRelation"))
            projectCompilerOptionsRelation = atoi(node->Attribute("projectCompilerOptionsRelation"));

        if (node->Attribute("projectLinkerOptionsRelation"))
            projectLinkerOptionsRelation = atoi(node->Attribute("projectLinkerOptionsRelation"));

        if (node->Attribute("projectIncludeDirsRelation"))
            projectIncludeDirsRelation = atoi(node->Attribute("projectIncludeDirsRelation"));

        if (node->Attribute("projectLibDirsRelation"))
            projectLibDirsRelation = atoi(node->Attribute("projectLibDirsRelation"));

        if (node->Attribute("projectResourceIncludeDirsRelation"))
        {
            projectResIncludeDirsRelation = atoi(node->Attribute("projectResourceIncludeDirsRelation"));
            // there used to be a bug in this setting and it might have a negative or very big number
            // detect this case and set as default
            if (projectResIncludeDirsRelation < 0 || projectResIncludeDirsRelation >= ortLast)
                projectResIncludeDirsRelation = 3;
        }

        node = node->NextSiblingElement("Option");
    }

    node = parentNode->FirstChildElement("Script");
    while (node)
    {
        if (node->Attribute("file"))
            target->AddBuildScript(cbC2U(node->Attribute("file")));

        node = node->NextSiblingElement("Script");
    }

    if (type != -1)
    {
        target->SetPlatforms(platformsFinal);
        target->SetCompilerID(compilerId);
        target->SetTargetFilenameGenerationPolicy(prefixPolicy, extensionPolicy);
        target->SetTargetType((TargetType)type); // type *must* come before output filename!
        target->SetOutputFilename(output); // because if no filename defined, one will be suggested based on target type...
        target->SetImportLibraryFilename(imp_lib);
        target->SetDefinitionFileFilename(def_file);
        target->SetUseConsoleRunner(use_console_runner);
        if (!working_dir.IsEmpty())
            target->SetWorkingDir(working_dir);
        if (!obj_output.IsEmpty())
            target->SetObjectOutput(obj_output);
        if (!deps_output.IsEmpty())
            target->SetDepsOutput(deps_output);
        target->SetExternalDeps(deps);
        target->SetAdditionalOutputFiles(added);
        target->SetExecutionParameters(parameters);
        target->SetHostApplication(hostApplication);
        target->SetRunHostApplicationInTerminal(runHostApplicationInTerminal);
        target->SetIncludeInTargetAll(includeInTargetAll); // used in versions prior to 1.5
        target->SetCreateDefFile(createDefFile);
        target->SetCreateStaticLib(createStaticLib);
        target->SetOptionRelation(ortCompilerOptions, (OptionsRelation)projectCompilerOptionsRelation);
        target->SetOptionRelation(ortLinkerOptions, (OptionsRelation)projectLinkerOptionsRelation);
        target->SetOptionRelation(ortIncludeDirs, (OptionsRelation)projectIncludeDirsRelation);
        target->SetOptionRelation(ortLibDirs, (OptionsRelation)projectLibDirsRelation);
        target->SetOptionRelation(ortResDirs, (OptionsRelation)projectResIncludeDirsRelation);

        DoMakeCommands(parentNode->FirstChildElement("MakeCommands"), target);
    }
}

void ProjectLoader::DoCompilerOptions(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("Compiler");
    if (!node)
        return; // no options

    TiXmlElement* child = node->FirstChildElement("Add");
    while (child)
    {
        wxString option = cbC2U(child->Attribute("option"));
        wxString dir = UnixFilename(cbC2U(child->Attribute("directory")));
        if (!option.IsEmpty())
        {
            if (target)
                target->AddCompilerOption(option);
            else
                m_pProject->AddCompilerOption(option);
        }
        if (!dir.IsEmpty())
        {
            if (target)
                target->AddIncludeDir(dir);
            else
                m_pProject->AddIncludeDir(dir);
        }

        child = child->NextSiblingElement("Add");
    }
}

void ProjectLoader::DoResourceCompilerOptions(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("ResourceCompiler");
    if (!node)
        return; // no options

    TiXmlElement* child = node->FirstChildElement("Add");
    while (child)
    {
        wxString option = cbC2U(child->Attribute("option"));
        wxString dir = UnixFilename(cbC2U(child->Attribute("directory")));
        if (!option.IsEmpty())
        {
            if (target)
                target->AddResourceCompilerOption(option);
            else
                m_pProject->AddResourceCompilerOption(option);
        }
        if (!dir.IsEmpty())
        {
            if (target)
                target->AddResourceIncludeDir(dir);
            else
                m_pProject->AddResourceIncludeDir(dir);
        }

        child = child->NextSiblingElement("Add");
    }
}

void ProjectLoader::DoLinkerOptions(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("Linker");
    if (!node)
        return; // no options

    TiXmlElement* child = node->FirstChildElement("Add");
    while (child)
    {
        wxString option = cbC2U(child->Attribute("option"));
        wxString dir = UnixFilename(cbC2U(child->Attribute("directory")));
        wxString lib = UnixFilename(cbC2U(child->Attribute("library")));
        if (!option.IsEmpty())
        {
            if (target)
                target->AddLinkerOption(option);
            else
                m_pProject->AddLinkerOption(option);
        }
        if (!lib.IsEmpty())
        {
            if (target)
                target->AddLinkLib(lib);
            else
                m_pProject->AddLinkLib(lib);
        }
        if (!dir.IsEmpty())
        {
            if (target)
                target->AddLibDir(dir);
            else
                m_pProject->AddLibDir(dir);
        }

        child = child->NextSiblingElement("Add");
    }

    child = node->FirstChildElement("LinkerExe");
    if (child)
    {
        const wxString value = cbC2U(child->Attribute("value"));

        wxString str[int(LinkerExecutableOption::Last) - 1] = {
            wxT("CCompiler"),
            wxT("CppCompiler"),
            wxT("Linker")
        };

        int index;
        for (index = 0; index < cbCountOf(str); ++index)
        {
            if (value == str[index])
                break;
        }

        LinkerExecutableOption linkerExe = LinkerExecutableOption::AutoDetect;
        if (index < cbCountOf(str))
            linkerExe = LinkerExecutableOption(index + 1);

        if (target)
            target->SetLinkerExecutable(linkerExe);
    }
}

void ProjectLoader::DoIncludesOptions(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("IncludeDirs");
    if (!node)
        return; // no options

    TiXmlElement* child = node->FirstChildElement("Add");
    while (child)
    {
        wxString option = UnixFilename(cbC2U(child->Attribute("option")));
        if (!option.IsEmpty())
        {
            if (target)
                target->AddIncludeDir(option);
            else
                m_pProject->AddIncludeDir(option);
        }

        child = child->NextSiblingElement("Add");
    }
}

void ProjectLoader::DoLibsOptions(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("LibDirs");
    if (!node)
        return; // no options

    TiXmlElement* child = node->FirstChildElement("Add");
    while (child)
    {
        wxString option = UnixFilename(cbC2U(child->Attribute("option")));
        if (!option.IsEmpty())
        {
            if (target)
                target->AddLibDir(option);
            else
                m_pProject->AddLibDir(option);
        }

        child = child->NextSiblingElement("Add");
    }
}

void ProjectLoader::DoExtraCommands(TiXmlElement* parentNode, ProjectBuildTarget* target)
{
    TiXmlElement* node = parentNode->FirstChildElement("ExtraCommands");
    while (node)
    {
        CompileOptionsBase* base = target ? target : (CompileOptionsBase*)m_pProject;
        TiXmlElement* child = node->FirstChildElement("Mode");
        while (child)
        {
            wxString mode = cbC2U(child->Attribute("after"));
            if (mode == "always")
                base->SetAlwaysRunPostBuildSteps(true);

            child = child->NextSiblingElement("Mode");
        }

        child = node->FirstChildElement("Add");
        while (child)
        {
            wxString before;
            wxString after;

            if (child->Attribute("before"))
                before = cbC2U(child->Attribute("before"));
            if (child->Attribute("after"))
                after = cbC2U(child->Attribute("after"));

            if (!before.IsEmpty())
                base->AddCommandsBeforeBuild(before);
            if (!after.IsEmpty())
                base->AddCommandsAfterBuild(after);

            child = child->NextSiblingElement("Add");
        }
        node = node->NextSiblingElement("ExtraCommands");
    }
}

void ProjectLoader::DoEnvironment(TiXmlElement* parentNode, CompileOptionsBase* base)
{
    if (!base)
        return;

    TiXmlElement* node = parentNode->FirstChildElement("Environment");
    while (node)
    {
        TiXmlElement* child = node->FirstChildElement("Variable");
        while (child)
        {
            wxString name  = cbC2U(child->Attribute("name"));
            wxString value = cbC2U(child->Attribute("value"));
            if (!name.IsEmpty())
                base->SetVar(name, UnixFilename(value));

            child = child->NextSiblingElement("Variable");
        }
        node = node->NextSiblingElement("Environment");
    }
}

namespace
{
wxString MakePathAbsoluteIfNeeded(const wxString& path, const wxString& basePath)
{
    wxString absolute = path;
    wxFileName fname = path;
    if (!fname.IsAbsolute())
    {
        fname.MakeAbsolute(basePath);
        absolute = fname.GetFullPath();
    }
    return absolute;
}

bool IsRelative(wxString path)
{
    return !(path.GetChar(1) == ':' || path.GetChar(0) == '/');
}


void SplitPath(wxString path, std::vector<std::string>& out)
{
    path.Replace("\\","/");
    std::stringstream f(path.ToStdString());
    std::string s;
    while (std::getline(f, s, '/')) {
        out.push_back(s);
    }
}


wxString MakePathRelativeIfNeeded(const wxString& path, const wxString& basePath)
{
    // This code is inspired heavily from the boost::filesystem::relative function.
    // This code is used instead of the wxWidgets internal function, because it is
    // ~1000 times faster on my machine
    // using std::string vs wxString makes this function 24x faster
    // ! WE DO NOT HANDLE SYMLINKS !

    if (IsRelative(path))
        return path;

    std::string pathSeparator;
    if (platform::windows)
        pathSeparator = "\\";
    else
        pathSeparator = "/";


    std::vector<std::string> vPath;
    std::vector<std::string> vBasePath;

    SplitPath(path, vPath);
    SplitPath(basePath, vBasePath);

    std::vector<std::string>::iterator b = vPath.begin(), e = vPath.end(), base_b = vBasePath.begin(), base_e = vBasePath.end();
    std::pair<std::vector<std::string>::iterator, std::vector<std::string>::iterator> mm = std::mismatch(b, e, base_b);
    if (mm.first == b && mm.second == base_b)
      return path;
    if (mm.first == e && mm.second == base_e)
      return ".";

    std::ptrdiff_t n = 0;
    for (; mm.second != base_e; ++mm.second)
    {
      std::string const& p = *mm.second;
      if (p == "..")
        --n;
      else if (!p.empty() && p != ".")
        ++n;
    }
    if (n < 0)
      return path;
    if (n == 0 && (mm.first == e || mm.first->empty()))
      return ".";

    std::string tmp;
    for (; n > 0; --n)
      tmp += ".." + pathSeparator;
    for (; mm.first != e; ++mm.first)
    {
        if (tmp != "")
            tmp += pathSeparator;
        tmp += *mm.first;
    }

    return wxString(tmp);
}

wxArrayString MakePathsRelativeIfNeeded(const wxArrayString& paths, const wxString& basePath)
{
    wxStopWatch timer;
    wxArrayString relatives = paths;
    for (std::size_t index = 0U; index < paths.Count(); ++index)
    {
        wxString& path = relatives[index];
        wxString ret = MakePathRelativeIfNeeded(path, basePath);
        path = ret;
    }
    return relatives;
}

std::vector<wxString> FilterOnWildcards(const wxArrayString& files, const wxString& wildCard)
{
    wxString wild = wildCard;
    if (wild.IsEmpty())
    {
        FilesGroupsAndMasks fgm;
        for (unsigned i = 0; i < fgm.GetGroupsCount(); ++i)
        {
            wild += fgm.GetFileMasks(i);
        }
    }

    const wxArrayString wilds = GetArrayFromString(wild, ";");
    std::vector<wxString> finalFiles;
    for (std::size_t file = 0; file < files.Count(); ++file)
    {
        const wxString& fileName = files[file];
        bool MatchesWildCard = false;
        for (std::size_t x = 0; x < wilds.GetCount(); ++x)
        {
            if (fileName.Matches(wilds[x].Lower()))
            {
                MatchesWildCard = true;
                break;
            }
        }
        if (MatchesWildCard)
        {
            finalFiles.push_back(fileName);
        }
    }
    return finalFiles;
}

void LogTime(const wxString& message, long time)
{
    if(time > 100)
        Manager::Get()->GetLogManager()->Log(wxString::Format(message, time / 1000.0) );
}

std::vector<wxString> FilesInDir(const wxString& directory, const wxString& wildCard, bool recursive, const wxString& basePath)
{
    const wxString directoryPath = MakePathAbsoluteIfNeeded(directory, basePath);
    std::vector<wxString> files;

    int flags = wxDIR_FILES;
    if (recursive)
    {
        flags = flags | wxDIR_DIRS;
    }
    wxStopWatch timer;
    wxArrayString filesUnfiltered;
    wxDir::GetAllFiles(directoryPath, &filesUnfiltered, wxEmptyString, flags);
    LogTime("wxDir::GetAllFiles %f s", timer.Time());
    timer.Start();
    filesUnfiltered = MakePathsRelativeIfNeeded(filesUnfiltered, basePath);
    LogTime("makePathsRelativeIfNeeded %f s", timer.Time());
    timer.Start();
    std::vector<wxString> ret = FilterOnWildcards(filesUnfiltered, wildCard);
    LogTime("filterOnWildcards %f s", timer.Time());
    return ret;
}

} // namespace

bool ProjectLoader::UpdateGlob(const ProjectGlob& glob)
{
    wxStopWatch timer;
    bool modified = false;
    const wxString directory = glob.GetPath();
    const wxString wildCard = glob.GetWildCard();
    const bool isRecursive = glob.GetRecursive();
    std::vector<wxString> globFiles = FilesInDir(directory, wildCard, isRecursive, m_pProject->GetBasePath());
    LogTime("Loading directories took %f s", timer.Time());
    timer.Start();
    // Sort the paths so we can use binary_search
    std::sort(globFiles.begin(), globFiles.end());
    LogTime("Sorting took %f s", timer.Time());
    timer.Start();
    std::vector<ProjectFile*> projectGlobFiles;     // We have to search in this files if the glob is present
    std::vector<ProjectFile*> projectFilesToRemove; // This files are in this glob, but are no longer on the file system
    // First search for valid project files (glob id) and also for project files we have to remove
    for (ProjectFile* file : m_pProject->GetFilesList())
    {
        if (file->globId == glob.GetId())
        {
            bool fileExists = std::binary_search(globFiles.cbegin(), globFiles.cend(), file->relativeFilename);
            if (!fileExists)
                projectFilesToRemove.push_back(file);
            else
                projectGlobFiles.push_back(file);
        }
    }
    LogTime("First loop took %f s", timer.Time());
    timer.Start();
    if(projectFilesToRemove.size() > 0)
        modified = true;
    // Now remove all files from the project that are not present on the filesystem
    for(ProjectFile* pf :  projectFilesToRemove)
        m_pProject->RemoveFile(pf);
    LogTime("Removing took %f s", timer.Time());
    timer.Start();
    // Now lets sort the valid project files for a fast binary search
    std::sort(projectGlobFiles.begin(), projectGlobFiles.end(),[](ProjectFile *a,ProjectFile *b){ return a->relativeFilename < b->relativeFilename; } );
    LogTime("Second sorting took %f s", timer.Time());
    timer.Start();
    // We have to define a custom comparator, to compare wxString <-> ProjectFile
    struct Comparator
    {
       bool operator() ( const ProjectFile* lhs, const wxString& rhs )
       {
          return lhs->relativeFilename < rhs;
       }
       bool operator() ( const wxString& lhs, const ProjectFile* rhs )
       {
          return lhs < rhs->relativeFilename;
       }
    };

    // Now search for the new files, and add them
    for (const wxString& file : globFiles)
    {
        wxStopWatch searchTimer;
        searchTimer.Start();
        bool sear = std::binary_search(projectGlobFiles.cbegin(), projectGlobFiles.cend(), file, Comparator());
        if (!sear)
        {
            ProjectFile* pf = m_pProject->AddFile(-1, UnixFilename(file));
            if (!pf)
                Manager::Get()->GetLogManager()->DebugLog(_T("Can't load file ") + file);
            else
            {
                modified = true;
                const TiXmlElement dummyUnitWithoutOptions("Unit");
                DoUnitOptions(&dummyUnitWithoutOptions, pf);
                pf->globId = glob.GetId();
            }
        }
    }
    LogTime("Adding took %f s", timer.Time() );
    timer.Start();

    return modified;
}

void ProjectLoader::DoUnits(const TiXmlElement* parentNode)
{
    Manager::Get()->GetLogManager()->DebugLog("Loading project files...");
    m_pProject->BeginAddFiles();

    int count = 0;

    const std::string UnitsGlobLabel("UnitsGlob");
    const TiXmlElement* unitsGlob = parentNode->FirstChildElement(UnitsGlobLabel.c_str());
    while (unitsGlob)
    {
        const wxString directory = cbC2U(unitsGlob->Attribute("directory"));
        const wxString wildCard = cbC2U(unitsGlob->Attribute("wildcard"));
        const wxString id = cbC2U(unitsGlob->Attribute("id"));

        int recursive = 0;
        unitsGlob->QueryIntAttribute("recursive", &recursive);

        if (!directory.IsEmpty())
        {
            const bool isRecursive = (recursive == 1) ? true:false;

            ProjectGlob glob;
            long long idNr = -0;
            if (!id.ToLongLong(&idNr))
            {
                  Manager::Get()->GetLogManager()->DebugLog(_T("Can't read glob id for glob ") + directory);
                  glob = ProjectGlob(directory, wildCard, isRecursive);
            }
            else
            {
                glob = ProjectGlob((GlobId) idNr, directory, wildCard, isRecursive);
            }

            m_pProject->AddGlob(glob);
        }
        unitsGlob = unitsGlob->NextSiblingElement(UnitsGlobLabel.c_str());
    }


    const TiXmlElement* unit = parentNode->FirstChildElement("Unit");
    while (unit)
    {
        const wxString filename = cbC2U(unit->Attribute("filename"));
        if (!filename.IsEmpty())
        {
            ProjectFile* file = m_pProject->AddFile(-1, UnixFilename(filename));
            if (!file)
                Manager::Get()->GetLogManager()->DebugLog("Can't load file " + filename);
            else
            {
                ++count;
                if (!DoUnitOptions(unit, file))
                    m_pProject->RemoveFile(file);
            }
        }

        unit = unit->NextSiblingElement("Unit");
    }
    m_pProject->EndAddFiles();
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format("%d files loaded", count));
}

bool ProjectLoader::DoUnitOptions(const TiXmlElement* parentNode, ProjectFile* file)
{
    int tempval = 0;
    bool foundCompile = false;
    bool foundLink = false;
    bool foundCompilerVar = false;
    bool foundTarget = false;
    bool noTarget = false;

//    Compiler* compiler = CompilerFactory::GetCompiler(m_pProject->GetCompilerID());

    const TiXmlElement* node = parentNode->FirstChildElement("Option");
    while (node)
    {
        if (node->Attribute("compilerVar"))
        {
            file->compilerVar = cbC2U(node->Attribute("compilerVar"));
            foundCompilerVar = true;
        }
        //
        if (node->QueryIntAttribute("compile", &tempval) == TIXML_SUCCESS)
        {
            file->compile = tempval != 0;
            foundCompile = true;
        }
        //
        if (node->QueryIntAttribute("link", &tempval) == TIXML_SUCCESS)
        {
            file->link = tempval != 0;
            foundLink = true;
        }
        //
        if (node->QueryIntAttribute("weight", &tempval) == TIXML_SUCCESS)
            file->weight = tempval;
        //
        if (node->Attribute("virtualFolder"))
            file->virtual_path = UnixFilename(cbC2U(node->Attribute("virtualFolder")));
        //
        if (node->Attribute("buildCommand") && node->Attribute("compiler"))
        {
            const wxString cmp = cbC2U(node->Attribute("compiler"));
            wxString tmp = cbC2U(node->Attribute("buildCommand"));
            if (!cmp.IsEmpty() && !tmp.IsEmpty())
            {
                tmp.Replace("\\n", "\n");
                file->SetCustomBuildCommand(cmp, tmp);
                if (node->QueryIntAttribute("use", &tempval) == TIXML_SUCCESS)
                    file->SetUseCustomBuildCommand(cmp, tempval != 0);
            }
        }
        //
        if (node->Attribute("target"))
        {
            wxString targetName = cbC2U(node->Attribute("target"));
            if (!targetName.IsSameAs("<{~None~}>"))
            {
                file->AddBuildTarget(targetName);
                foundTarget = true;
            }
            else
                noTarget = true;
        }

        // Loading project globs
        if (node->Attribute("glob"))
        {
            wxString id = cbC2U(node->Attribute("glob"));
            ProjectGlob glob = m_pProject->SearchGlob(id);
            if (!glob.IsValid())
            {
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Could not find project glob with id %s for file %s", id, file->GetBaseName()));
                return false;
            }
            else
                file->globId = glob.GetId();
        }

        node = node->NextSiblingElement("Option");
    }

    // pre 1.6 versions upgrade
    if (m_IsPre_1_6)
    {
        // make sure the "compile" and "link" flags are honored
        if (!foundCompile)
            file->compile = true;
        if (!foundLink)
            file->link = true;
        if (!foundCompilerVar)
            file->compilerVar = "CPP";
    }

    if (!foundTarget && !noTarget)
    {
        // add to all targets
        for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
        {
            file->AddBuildTarget(m_pProject->GetBuildTarget(i)->GetTitle());
        }
    }

    return true;
}

// convenience function, used in Save()
static TiXmlElement* AddElement(TiXmlElement* parent, const char* name, const char* attr = nullptr,
                                const wxString& attribute = wxEmptyString)
{
    TiXmlElement elem(name);

    if (attr)
        elem.SetAttribute(attr, cbU2C(attribute));

    return parent->InsertEndChild(elem)->ToElement();
}

// convenience function, used in Save()
static TiXmlElement* AddElement(TiXmlElement* parent, const char* name, const char* attr,
                                int attribute)
{
    TiXmlElement elem(name);

    if (attr)
        elem.SetAttribute(attr, attribute);

    return parent->InsertEndChild(elem)->ToElement();
}

// convenience function, used in Save()
static void AddArrayOfElements(TiXmlElement* parent, const char* name, const char* attr,
                               const wxArrayString& array, bool isPath = false)
{
    if (!array.GetCount())
        return;

    for (unsigned int i = 0; i < array.GetCount(); ++i)
    {
        if (array[i].IsEmpty())
            continue;
        AddElement(parent, name, attr, (isPath ? UnixFilename(array[i], wxPATH_UNIX) : array[i]));
    }
}

// convenience function, used in Save()
static void SaveEnvironment(TiXmlElement* parent, CompileOptionsBase* base)
{
    if (!base)
        return;
    const StringHash& v = base->GetAllVars();
    if (v.empty())
        return;

    // explicitly sort the keys
    typedef std::map<wxString, wxString> SortedMap;
    SortedMap map;
    for (StringHash::const_iterator it = v.begin(); it != v.end(); ++it)
        map[it->first] = it->second;

    TiXmlElement* node = AddElement(parent, "Environment");
    for (SortedMap::const_iterator it = map.begin(); it != map.end(); ++it)
    {
        TiXmlElement* elem = AddElement(node, "Variable", "name", it->first);
        elem->SetAttribute("value", cbU2C(it->second));
    }
}

bool ProjectLoader::Save(const wxString& filename)
{
    return Save(filename, nullptr);
}

bool ProjectLoader::Save(const wxString& filename, TiXmlElement* pExtensions)
{
    if (ExportTargetAsProject(filename, wxEmptyString, pExtensions))
    {
        m_pProject->SetModified(false);
        return true;
    }
    return false;
}

// convenience function, used in ExportTargetAsProject()
static void SaveLinkerExecutable(TiXmlElement *linkerNode, const CompileOptionsBase &options)
{
    const LinkerExecutableOption linkerExe = options.GetLinkerExecutable();
    if (linkerExe > LinkerExecutableOption::AutoDetect
        && linkerExe < LinkerExecutableOption::Last)
    {
        wxString str[int(LinkerExecutableOption::Last) - 1] = {
            wxT("CCompiler"),
            wxT("CppCompiler"),
            wxT("Linker")
        };
        AddElement(linkerNode, "LinkerExe", "value", str[int(linkerExe) - 1]);
    }
}

bool ProjectLoader::ExportTargetAsProject(const wxString& filename, const wxString& onlyTarget, TiXmlElement* pExtensions)
{
    const char* ROOT_TAG = "CodeBlocks_project_file";

    TiXmlDocument doc;
    doc.SetCondenseWhiteSpace(false);
    doc.InsertEndChild(TiXmlDeclaration("1.0", "UTF-8", "yes"));
    TiXmlElement* rootnode = static_cast<TiXmlElement*>(doc.InsertEndChild(TiXmlElement(ROOT_TAG)));
    if (!rootnode)
        return false;

//    Compiler* compiler = CompilerFactory::GetCompiler(m_pProject->GetCompilerID());

    rootnode->InsertEndChild(TiXmlElement("FileVersion"));
    rootnode->FirstChildElement("FileVersion")->SetAttribute("major", PROJECT_FILE_VERSION_MAJOR);
    rootnode->FirstChildElement("FileVersion")->SetAttribute("minor", PROJECT_FILE_VERSION_MINOR);

    rootnode->InsertEndChild(TiXmlElement("Project"));
    TiXmlElement* prjnode = rootnode->FirstChildElement("Project");

    AddElement(prjnode, "Option", "title", m_pProject->GetTitle());
    if (m_pProject->GetPlatforms() != spAll)
    {
        wxString platforms = GetStringFromPlatforms(m_pProject->GetPlatforms());
        AddElement(prjnode, "Option", "platforms", platforms);
    }
    if (m_pProject->GetMakefile() != "Makefile")
        AddElement(prjnode, "Option", "makefile", UnixFilename(m_pProject->GetMakefile(), wxPATH_UNIX));
    if (m_pProject->IsMakefileCustom())
        AddElement(prjnode, "Option", "makefile_is_custom", 1);
    if (m_pProject->GetMakefileExecutionDir() != m_pProject->GetBasePath())
        AddElement(prjnode, "Option", "execution_dir", UnixFilename(m_pProject->GetMakefileExecutionDir(), wxPATH_UNIX));
    if (m_pProject->GetModeForPCH() != pchObjectDir)
        AddElement(prjnode, "Option", "pch_mode", (int)m_pProject->GetModeForPCH());
    if (!m_pProject->GetDefaultExecuteTarget().IsEmpty() && m_pProject->GetDefaultExecuteTarget() != m_pProject->GetFirstValidBuildTargetName())
        AddElement(prjnode, "Option", "default_target", m_pProject->GetDefaultExecuteTarget());
    AddElement(prjnode, "Option", "compiler", m_pProject->GetCompilerID());

    wxArrayString virtualFolders = m_pProject->GetVirtualFolders();
    if (virtualFolders.GetCount() > 0)
    {
        wxString result; // the concatenated string
        for (size_t i = 0; i < virtualFolders.GetCount(); i++)
        {
            if (!result.IsEmpty())
                result << wxT(";"); // add the delimiter

            result << UnixFilename(virtualFolders[i], wxPATH_UNIX); // append Unix format folder name
        }
        AddElement(prjnode, "Option", "virtualFolders", result);
    }

    if (m_pProject->GetExtendedObjectNamesGeneration())
        AddElement(prjnode, "Option", "extended_obj_names", 1);
    if (m_pProject->GetShowNotesOnLoad() || !m_pProject->GetNotes().IsEmpty())
    {
        TiXmlElement* notesBase = AddElement(prjnode, "Option", "show_notes", m_pProject->GetShowNotesOnLoad() ? 1 : 0);
        if (!m_pProject->GetNotes().IsEmpty())
        {
            TiXmlElement* notes = AddElement(notesBase, "notes");
            TiXmlText t(m_pProject->GetNotes().mb_str(wxConvUTF8));
            t.SetCDATA(true);
            notes->InsertEndChild(t);
        }
    }
    if (!m_pProject->GetCheckForExternallyModifiedFiles())
        AddElement(prjnode, "Option", "check_files", 0);

    if (m_pProject->MakeCommandsModified())
    {
        TiXmlElement* makenode = AddElement(prjnode, "MakeCommands");
        AddElement(makenode, "Build",            "command", m_pProject->GetMakeCommandFor(mcBuild));
        AddElement(makenode, "CompileFile",      "command", m_pProject->GetMakeCommandFor(mcCompileFile));
        AddElement(makenode, "Clean",            "command", m_pProject->GetMakeCommandFor(mcClean));
        AddElement(makenode, "DistClean",        "command", m_pProject->GetMakeCommandFor(mcDistClean));
        AddElement(makenode, "AskRebuildNeeded", "command", m_pProject->GetMakeCommandFor(mcAskRebuildNeeded));
        AddElement(makenode, "SilentBuild",      "command", m_pProject->GetMakeCommandFor(mcSilentBuild));
    }

    prjnode->InsertEndChild(TiXmlElement("Build"));
    TiXmlElement* buildnode = prjnode->FirstChildElement("Build");

    for (size_t x = 0; x < m_pProject->GetBuildScripts().GetCount(); ++x)
        AddElement(buildnode, "Script", "file", UnixFilename(m_pProject->GetBuildScripts().Item(x), wxPATH_UNIX));

    // now decide which target we're exporting.
    // remember that if onlyTarget is empty, we export all targets (i.e. normal save).
    ProjectBuildTarget* onlytgt = m_pProject->GetBuildTarget(onlyTarget);

    for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
    {
        ProjectBuildTarget* target = m_pProject->GetBuildTarget(i);
        if (!target)
            break;

        // skip every target except the desired one
        if (onlytgt && onlytgt != target)
            continue;

        TiXmlElement* tgtnode = AddElement(buildnode, "Target", "title", target->GetTitle());
        if (target->GetPlatforms() != spAll)
        {
            wxString platforms = GetStringFromPlatforms(target->GetPlatforms());
            AddElement(tgtnode, "Option", "platforms", platforms);
        }
        if (target->GetTargetType() != ttCommandsOnly)
        {
            TargetFilenameGenerationPolicy prefixPolicy;
            TargetFilenameGenerationPolicy extensionPolicy;
            target->GetTargetFilenameGenerationPolicy(prefixPolicy, extensionPolicy);

            wxString outputFileName = target->GetOutputFilename();
            if (extensionPolicy == tgfpPlatformDefault)
            {
                wxFileName fname(outputFileName);
                fname.ClearExt();
                outputFileName = fname.GetFullPath();
            }

            if (   (prefixPolicy == tgfpPlatformDefault)
                && (   (!platform::windows && target->GetTargetType() == ttDynamicLib)
                    || (target->GetTargetType() == ttStaticLib) ) )
            {
                wxString compilerId = target->GetCompilerID();
                Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
                if (compiler)
                {
                    wxFileName fname(outputFileName);
                    wxString outputFileNameFile(fname.GetFullName());

                    wxString compilerLibPrefix(compiler->GetSwitches().libPrefix);
                    wxString outputFileNameWOPrefix;
                    if (outputFileNameFile.StartsWith(compilerLibPrefix))
                    {
                        outputFileNameWOPrefix = outputFileNameFile.Mid(compilerLibPrefix.Len());
                        if (!outputFileNameWOPrefix.IsEmpty())
                        {
                            fname.SetFullName(outputFileNameWOPrefix);
                            outputFileName = fname.GetFullPath();
                        }
                    }
                }
            }

            TiXmlElement* outnode = AddElement(tgtnode, "Option", "output", UnixFilename(outputFileName, wxPATH_UNIX));
            if (target->GetTargetType() == ttDynamicLib)
            {
                if (target->GetDynamicLibImportFilename() != "$(TARGET_OUTPUT_DIR)$(TARGET_OUTPUT_BASENAME)")
                  outnode->SetAttribute("imp_lib",  cbU2C(UnixFilename(target->GetDynamicLibImportFilename(), wxPATH_UNIX)));
                if (target->GetDynamicLibImportFilename() != "$(TARGET_OUTPUT_DIR)$(TARGET_OUTPUT_BASENAME)")
                  outnode->SetAttribute("def_file", cbU2C(UnixFilename(target->GetDynamicLibDefFilename(), wxPATH_UNIX)));
            }
            outnode->SetAttribute("prefix_auto",    prefixPolicy    == tgfpPlatformDefault ? "1" : "0");
            outnode->SetAttribute("extension_auto", extensionPolicy == tgfpPlatformDefault ? "1" : "0");

            if (target->GetWorkingDir() != ".")
                AddElement(tgtnode, "Option", "working_dir",   UnixFilename(target->GetWorkingDir(), wxPATH_UNIX));
            if (target->GetObjectOutput() != ".objs")
                AddElement(tgtnode, "Option", "object_output", UnixFilename(target->GetObjectOutput(), wxPATH_UNIX));
            if (target->GetDepsOutput() != ".deps")
                AddElement(tgtnode, "Option", "deps_output",   UnixFilename(target->GetDepsOutput(), wxPATH_UNIX));
        }
        if (!target->GetExternalDeps().IsEmpty())
            AddElement(tgtnode, "Option", "external_deps",     UnixFilename(target->GetExternalDeps(), wxPATH_UNIX));
        if (!target->GetAdditionalOutputFiles().IsEmpty())
            AddElement(tgtnode, "Option", "additional_output", UnixFilename(target->GetAdditionalOutputFiles(), wxPATH_UNIX));
        AddElement(tgtnode, "Option", "type", target->GetTargetType());
        AddElement(tgtnode, "Option", "compiler", target->GetCompilerID());
        if (target->GetTargetType() == ttConsoleOnly && !target->GetUseConsoleRunner())
            AddElement(tgtnode, "Option", "use_console_runner", 0);
        if (!target->GetExecutionParameters().IsEmpty())
            AddElement(tgtnode, "Option", "parameters", target->GetExecutionParameters());
        if (!target->GetHostApplication().IsEmpty())
        {
            AddElement(tgtnode, "Option", "host_application", UnixFilename(target->GetHostApplication(), wxPATH_UNIX));
            if (target->GetRunHostApplicationInTerminal())
                AddElement(tgtnode, "Option", "run_host_application_in_terminal", 1);
            else
                AddElement(tgtnode, "Option", "run_host_application_in_terminal", 0);
        }

        // used in versions prior to 1.5
//        if (target->GetIncludeInTargetAll())
//            AddElement(tgtnode, "Option", "includeInTargetAll", 1);
        if ((target->GetTargetType() == ttStaticLib || target->GetTargetType() == ttDynamicLib) && target->GetCreateDefFile())
            AddElement(tgtnode, "Option", "createDefFile", 1);
        if (target->GetTargetType() == ttDynamicLib && target->GetCreateStaticLib())
            AddElement(tgtnode, "Option", "createStaticLib", 1);
        if (target->GetOptionRelation(ortCompilerOptions) != 3) // 3 is the default
            AddElement(tgtnode, "Option", "projectCompilerOptionsRelation",     target->GetOptionRelation(ortCompilerOptions));
        if (target->GetOptionRelation(ortLinkerOptions) != 3) // 3 is the default
            AddElement(tgtnode, "Option", "projectLinkerOptionsRelation",       target->GetOptionRelation(ortLinkerOptions));
        if (target->GetOptionRelation(ortIncludeDirs) != 3) // 3 is the default
            AddElement(tgtnode, "Option", "projectIncludeDirsRelation",         target->GetOptionRelation(ortIncludeDirs));
        if (target->GetOptionRelation(ortResDirs) != 3) // 3 is the default
            AddElement(tgtnode, "Option", "projectResourceIncludeDirsRelation", target->GetOptionRelation(ortResDirs));
        if (target->GetOptionRelation(ortLibDirs) != 3) // 3 is the default
            AddElement(tgtnode, "Option", "projectLibDirsRelation",             target->GetOptionRelation(ortLibDirs));

        for (size_t x = 0; x < target->GetBuildScripts().GetCount(); ++x)
            AddElement(tgtnode, "Script", "file", target->GetBuildScripts().Item(x));

        TiXmlElement* node = AddElement(tgtnode, "Compiler");
        AddArrayOfElements(node, "Add", "option",    target->GetCompilerOptions());
        AddArrayOfElements(node, "Add", "directory", target->GetIncludeDirs(), true);
        if (node->NoChildren())
            tgtnode->RemoveChild(node);

        node = AddElement(tgtnode, "ResourceCompiler");
        AddArrayOfElements(node, "Add", "option",    target->GetResourceCompilerOptions());
        AddArrayOfElements(node, "Add", "directory", target->GetResourceIncludeDirs(), true);
        if (node->NoChildren())
            tgtnode->RemoveChild(node);

        node = AddElement(tgtnode, "Linker");
        AddArrayOfElements(node, "Add", "option",    target->GetLinkerOptions());
        AddArrayOfElements(node, "Add", "library",   target->GetLinkLibs(), true);
        AddArrayOfElements(node, "Add", "directory", target->GetLibDirs(), true);
        SaveLinkerExecutable(node, *target);
        if (node->NoChildren())
            tgtnode->RemoveChild(node);

        node = AddElement(tgtnode, "ExtraCommands");
        AddArrayOfElements(node, "Add", "before", target->GetCommandsBeforeBuild());
        AddArrayOfElements(node, "Add", "after",  target->GetCommandsAfterBuild());
        if (node->NoChildren())
            tgtnode->RemoveChild(node);
        else
        {
            if (target->GetAlwaysRunPostBuildSteps())
                AddElement(node, "Mode", "after", wxString("always"));
        }

        SaveEnvironment(tgtnode, target);

        if (target->MakeCommandsModified())
        {
            TiXmlElement* makenode = AddElement(tgtnode, "MakeCommands");
            AddElement(makenode, "Build",            "command", target->GetMakeCommandFor(mcBuild));
            AddElement(makenode, "CompileFile",      "command", target->GetMakeCommandFor(mcCompileFile));
            AddElement(makenode, "Clean",            "command", target->GetMakeCommandFor(mcClean));
            AddElement(makenode, "DistClean",        "command", target->GetMakeCommandFor(mcDistClean));
            AddElement(makenode, "AskRebuildNeeded", "command", target->GetMakeCommandFor(mcAskRebuildNeeded));
            AddElement(makenode, "SilentBuild",      "command", target->GetMakeCommandFor(mcSilentBuild));
        }
    }

    // virtuals only for whole project
    if (onlyTarget.IsEmpty())
    {
        TiXmlElement* virtnode = AddElement(prjnode, "VirtualTargets");
        wxArrayString virtuals = m_pProject->GetVirtualBuildTargets();
        for (size_t i = 0; i < virtuals.GetCount(); ++i)
        {
            const wxArrayString& group = m_pProject->GetVirtualBuildTargetGroup(virtuals[i]);
            wxString groupStr = GetStringFromArray(group, ";");
            if (!groupStr.IsEmpty())
            {
                TiXmlElement* elem = AddElement(virtnode, "Add", "alias", virtuals[i]);
                elem->SetAttribute("targets", cbU2C(groupStr));
            }
        }
        if (virtnode->NoChildren())
            prjnode->RemoveChild(virtnode);
    }

    SaveEnvironment(buildnode, m_pProject);

    TiXmlElement* node = AddElement(prjnode, "Compiler");
    AddArrayOfElements(node, "Add", "option",    m_pProject->GetCompilerOptions());
    AddArrayOfElements(node, "Add", "directory", m_pProject->GetIncludeDirs(), true);
    if (node->NoChildren())
        prjnode->RemoveChild(node);

    node = AddElement(prjnode, "ResourceCompiler");
    AddArrayOfElements(node, "Add", "option",    m_pProject->GetResourceCompilerOptions());
    AddArrayOfElements(node, "Add", "directory", m_pProject->GetResourceIncludeDirs(), true);
    if (node->NoChildren())
        prjnode->RemoveChild(node);

    node = AddElement(prjnode, "Linker");
    AddArrayOfElements(node, "Add", "option",    m_pProject->GetLinkerOptions());
    AddArrayOfElements(node, "Add", "library",   m_pProject->GetLinkLibs(), true);
    AddArrayOfElements(node, "Add", "directory", m_pProject->GetLibDirs(), true);
    if (node->NoChildren())
        prjnode->RemoveChild(node);

    node = AddElement(prjnode, "ExtraCommands");
    AddArrayOfElements(node, "Add", "before", m_pProject->GetCommandsBeforeBuild());
    AddArrayOfElements(node, "Add", "after",  m_pProject->GetCommandsAfterBuild());
    if (node->NoChildren())
        prjnode->RemoveChild(node);
    else
    {
        if (m_pProject->GetAlwaysRunPostBuildSteps())
            AddElement(node, "Mode", "after", wxString("always"));
    }


    ProjectFileArray pfa(ProjectFile::CompareProjectFiles);

    for (const ProjectGlob& glob : m_pProject->GetGlobs())
    {
        TiXmlElement *element = AddElement(prjnode, "UnitsGlob", "directory", glob.GetPath());
        element->SetAttribute("wildcard", glob.GetWildCard());
        element->SetAttribute("recursive", glob.GetRecursive() ? 1 : 0);
        element->SetAttribute("id", wxString::Format("%lld", glob.GetId()));
    }

    for (FilesList::iterator it = m_pProject->GetFilesList().begin(); it != m_pProject->GetFilesList().end(); ++it)
    {
        ProjectFile* f = *it;

        // do not save auto-generated files
        if (f->AutoGeneratedBy())
            continue;
        // do not save project files that do not belong in the target we 're exporting
        if (onlytgt && (onlytgt->GetFilesList().find(f) == onlytgt->GetFilesList().end()))
            continue;

        pfa.Add(f);
    }
    for (size_t i=0; i<pfa.GetCount(); ++i)
    {
        ProjectFile* f = pfa[i];
        FileType ft = FileTypeOf(f->relativeFilename);

        TiXmlElement* unitnode = AddElement(prjnode, "Unit", "filename", UnixFilename(f->relativeFilename, wxPATH_UNIX));
        if (!f->compilerVar.IsEmpty())
        {
            const wxString ext = f->relativeFilename.AfterLast('.').Lower();
            if (f->compilerVar != "CC" && (ext.IsSameAs(FileFilters::C_EXT)))
                AddElement(unitnode, "Option", "compilerVar", f->compilerVar);
#ifdef __WXMSW__
            else if (f->compilerVar != "WINDRES" && ext.IsSameAs(FileFilters::RESOURCE_EXT))
                AddElement(unitnode, "Option", "compilerVar", f->compilerVar);
#endif
            else if (f->compilerVar != "CPP") // default
                AddElement(unitnode, "Option", "compilerVar", f->compilerVar);
        }

        if (f->compile != (ft == ftSource || ft == ftResource))
            AddElement(unitnode, "Option", "compile", f->compile ? 1 : 0);

        if (f->link != (   ft == ftSource || ft == ftResource
                        || ft == ftObject || ft == ftResourceBin
                        || ft == ftStaticLib ) )
        {
            AddElement(unitnode, "Option", "link", f->link ? 1 : 0);
        }
        if (f->weight != 50)
            AddElement(unitnode, "Option", "weight", f->weight);

        if (!f->virtual_path.IsEmpty())
            AddElement(unitnode, "Option", "virtualFolder", UnixFilename(f->virtual_path, wxPATH_UNIX));

        if (f->IsGlobValid())
        {
            AddElement(unitnode, "Option", "glob", f->globId);
        }

        // loop and save custom build commands
        for (pfCustomBuildMap::iterator it = f->customBuild.begin(); it != f->customBuild.end(); ++it)
        {
            pfCustomBuild& pfcb = it->second;
            if (!pfcb.buildCommand.IsEmpty())
            {
                wxString tmp = pfcb.buildCommand;
                tmp.Replace("\n", "\\n");
                TiXmlElement* elem = AddElement(unitnode, "Option", "compiler", it->first);
                elem->SetAttribute("use", pfcb.useCustomBuildCommand ? "1" : "0");
                elem->SetAttribute("buildCommand", cbU2C(tmp));
            }
        }

        if ((int)f->buildTargets.GetCount() != m_pProject->GetBuildTargetsCount())
        {
            for (unsigned int x = 0; x < f->buildTargets.GetCount(); ++x)
                AddElement(unitnode, "Option", "target", f->buildTargets[x]);
        }

        /* Add a target with a weird name if no targets are present. *
         * This will help us detecting a file with no targets.       */
        if ((int)f->buildTargets.GetCount() == 0)
            AddElement(unitnode, "Option", "target", wxString("<{~None~}>"));
    }

    // as a last step, run all hooked callbacks
    TiXmlElement* extnode = pExtensions
                            ? prjnode->InsertEndChild(*pExtensions)->ToElement()
                            : AddElement(prjnode, "Extensions");
    if (ProjectLoaderHooks::HasRegisteredHooks() && extnode)
        ProjectLoaderHooks::CallHooks(m_pProject, extnode, false);

    // Sort by tag name and if tag names are equal use the original index of the element in the
    // extensions node. Do this because we assume the code which has inserted them has done so in
    // some sorted manner.
    // Ideally there would be no duplicates.

    struct Key
    {
        std::string tagName;
        int index;

        bool operator<(const Key &o) const
        {
            if (tagName == o.tagName)
                return index < o.index;
            return tagName < o.tagName;
        }
    };
    std::map<Key, TiXmlNode*> sortedExtensions;
    int index = 0;
    for (TiXmlNode *child = extnode->FirstChild(); child; child = child->NextSibling(), ++index)
    {
        TiXmlElement *element = child->ToElement();
        if (!element)
            continue;
        // Skip empty elements, because we don't want to pollute the project file.
        if (element->NoChildren() && element->FirstAttribute() == nullptr)
            continue;

        sortedExtensions.emplace(Key{element->Value(), index}, element->Clone());
    }

    // Clear the old node and place the elements in sorted order.
    // We assume there are only element xml nodes, everything else would be lost.
    extnode->Clear();
    for (const std::map<Key, TiXmlNode*>::value_type &ext : sortedExtensions)
    {
        extnode->LinkEndChild(ext.second);
    }

    return cbSaveTinyXMLDocument(&doc, filename);
}

wxString ProjectLoader::GetValidCompilerID(const wxString& proposal, const wxString& scope)
{
    if (CompilerFactory::GetCompiler(proposal))
        return proposal;

    // check the map; maybe we asked the user before
    CompilerSubstitutes::iterator it = m_CompilerSubstitutes.find(proposal);
    if (it != m_CompilerSubstitutes.end())
        return it->second;

    Compiler* compiler = nullptr;

    // if compiler is a number, then this is an older version of the project file
    // propose the same compiler by index
    if (!proposal.IsEmpty())
    {
        long int idx = -1;
        if (proposal.ToLong(&idx))
            compiler = CompilerFactory::GetCompiler(idx);
    }

    if (!compiler)
    {
        if(!(Manager::Get()->GetConfigManager("app")->ReadBool("/environment/ignore_invalid_targets", true)))
        {
            wxString msg;
            msg.Printf(_("The defined compiler for %s cannot be located (ID: %s).\n"
                         "Please choose the compiler you want to use instead and click \"OK\".\n"
                         "If you click \"Cancel\", the project/target will be excluded from the build."),
                       scope, proposal);

            compiler = CompilerFactory::SelectCompilerUI(msg);
        }
    }

    if (!compiler)
    {
        // allow for invalid compiler IDs to be preserved...
        m_CompilerSubstitutes[proposal] = proposal;
        return proposal;
    }

    m_OpenDirty = true;

    // finally, keep the user selection in the map so we don't ask him again
    m_CompilerSubstitutes[proposal] = compiler->GetID();
    return compiler->GetID();
}
