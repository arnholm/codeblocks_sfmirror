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
    #include <wx/dir.h>
    #include <wx/filesys.h>
    #include <wx/intl.h>
    #include <wx/menu.h>
    #include <wx/string.h>

    #include "pluginmanager.h"
    #include "cbexception.h"
    #include "cbplugin.h"
    #include "infowindow.h"
    #include "logmanager.h"
    #include "macrosmanager.h"
    #include "manager.h"
    #include "editormanager.h"
    #include "configmanager.h"
    #include "personalitymanager.h"
    #include "scriptingmanager.h"
    #include "globals.h"
    #include "sdk_events.h"
#endif

#include <algorithm>

#include <wx/dynlib.h>
#include <wx/filesys.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <wx/filename.h>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/txtstrm.h>

#include "filefilters.h"
#include <tinyxml.h>

#include "annoyingdialog.h"
#include "pluginsconfigurationdlg.h"

#include "scripting/bindings/sc_plugin.h"

template<> PluginManager* Mgr<PluginManager>::instance = nullptr;
template<> bool  Mgr<PluginManager>::isShutdown = false;

inline void VersionStringToNumbers(const wxString& version, long* major, long* minor, long* release)
{
    wxString majorS = version.BeforeFirst('.'); // 6.3.2 -> 6
    wxString minorS = version.AfterFirst('.'); // 6.3.2 -> 3.2
    wxString releaseS = version.AfterLast('.'); // 6.3.2 -> 2
    minorS = minorS.BeforeFirst('.'); // 3.2 -> 3
    if (major)
        majorS.ToLong(major);

    if (minor)
        minorS.ToLong(minor);

    if (release)
        releaseS.ToLong(release);
}

// returns -1 if new is less then old, 0 if equal and 1 if new is greater than old
inline int CompareVersions(const wxString& new_version, const wxString& old_version)
{
    long new_major, new_minor, new_release;
    long old_major, old_minor, old_release;

    VersionStringToNumbers(new_version, &new_major, &new_minor, &new_release);
    VersionStringToNumbers(old_version, &old_major, &old_minor, &old_release);

#define SIGN(a) (a>0?1:(a<0?-1:0))
    int result = 0;
    result += SIGN(new_major - old_major) << 2;
    result += SIGN(new_minor - old_minor) << 1;
    result += SIGN(new_release - old_release) << 0;
#undef SIGN

    if (result < 0)
        return -1;
    else if (result > 0)
        return 1;

    return 0;
}

namespace LibLoader
{
    struct RefCountedLib
    {
        RefCountedLib() : lib(nullptr), ref(0) {}
        wxDynamicLibrary* lib;
        int ref;
    };
    typedef std::map<wxString, RefCountedLib> Libs;
    Libs s_Libs;

    inline wxDynamicLibrary* LoadLibrary(const wxString& filename)
    {
        Libs::iterator it = s_Libs.find(filename);
        if (it != s_Libs.end())
        {
            // existing lib./codeblocks
            it->second.ref++;
            return it->second.lib;
        }
        // new lib
        it = s_Libs.insert(s_Libs.end(), std::make_pair(filename, RefCountedLib()));
        it->second.lib = new wxDynamicLibrary;
        it->second.ref = 1;
        it->second.lib->Load(filename);
        return it->second.lib;
    }

    inline void RemoveLibrary(wxDynamicLibrary* lib)
    {
        Libs::iterator it;
        for (it = s_Libs.begin(); it != s_Libs.end(); ++it)
        {
            RefCountedLib& rcl = it->second;
            if (rcl.lib == lib)
            {
                // found
                rcl.ref--;
                if (rcl.ref == 0)
                {
                    // only delete the lib if not shutting down
                    // if we are shutting down, it will be deleted automatically
                    if (!Manager::IsAppShuttingDown())
                        delete rcl.lib;
                    s_Libs.erase(it);
                }
                return;
            }
        }
        // if we reached here, it's a lib that was not handled by us
        // (or had wrong refcounting)
    }

    inline void Cleanup()
    {
        Libs::iterator it;
        for (it = s_Libs.begin(); it != s_Libs.end(); ++it)
        {
            RefCountedLib& rcl = it->second;
            // only delete the lib if not shutting down
            // if we are shutting down, it will be deleted automatically
            if (!Manager::IsAppShuttingDown())
                delete rcl.lib;
        }
        s_Libs.clear();
    }
};

//static
bool PluginManager::s_SafeMode = false;

BEGIN_EVENT_TABLE(PluginManager, wxEvtHandler)
//
END_EVENT_TABLE()

// class constructor
PluginManager::PluginManager()
    : m_pCurrentlyLoadingLib(nullptr),
    m_pCurrentlyLoadingManifestDoc(nullptr)
{
    Manager::Get()->GetAppWindow()->PushEventHandler(this);
}

// class destructor
PluginManager::~PluginManager()
{
    UnloadAllPlugins();
}

void PluginManager::CreateMenu(cb_unused wxMenuBar* menuBar)
{
}

void PluginManager::ReleaseMenu(cb_unused wxMenuBar* menuBar)
{
}

bool PluginManager::AttachPlugin(cbPlugin* plugin, bool ignoreSafeMode)
{
    if (!plugin)
        return false;

    if (plugin->IsAttached())
        return true;

    if (!s_SafeMode || ignoreSafeMode)
        plugin->Attach();

    return true;
}

bool PluginManager::DetachPlugin(cbPlugin* plugin)
{
    if (!plugin)
        return false;

    if (!plugin->IsAttached())
        return true;

    Manager::Get()->RemoveAllEventSinksFor(plugin);
    plugin->Release(Manager::IsAppShuttingDown());
    return true;
}

bool PluginManager::InstallPlugin(const wxString& pluginName, bool forAllUsers, bool askForConfirmation)
{
    if (pluginName.empty())
        return false;

    wxString actualName = pluginName;
    Manager::Get()->GetMacrosManager()->ReplaceMacros(actualName);

    // base name
    wxString basename = wxFileName(actualName).GetName();
    wxString version;
    if (basename.Contains('-'))
    {
        version = basename.AfterFirst('-');
        basename = basename.BeforeFirst('-');
    }

//    Manager::Get()->GetLogManager()->DebugLog(wxString::Format("InstallPlugin: basename='%s', version=%s", basename, version));

    // if plugin with the same name exists, ask to uninstall first
    cbPlugin* existingPlugin = FindPluginByName(basename);
    if (existingPlugin)
    {
        if (askForConfirmation)
        {
            wxString msg = _("A plugin with the same name is already installed.\n");
            if (!version.empty())
            {
                const PluginInfo* existingInfo = GetPluginInfo(existingPlugin);
                if (CompareVersions(version, existingInfo->version) < 0)
                {
                    msg = _("The plugin you are trying to install, is older "
                            "than the one currently installed.");
                }
            }

            if (cbMessageBox(msg + '\n' +
                            _("If you want to proceed, the installed plugin will be "
                            "uninstalled first.\n"
                            "Do you want to proceed?"),
                            _("Confirmation"), wxICON_QUESTION | wxYES_NO) == wxID_NO)
            {
                return false;
            }
        }

        if (!UninstallPlugin(existingPlugin))
            return false;
    }

    wxString pluginDir;
    wxString resourceDir;
    if (forAllUsers)
    {
        pluginDir = ConfigManager::GetFolder(sdPluginsGlobal);
        resourceDir = ConfigManager::GetFolder(sdDataGlobal);
    }
    else
    {
        pluginDir = ConfigManager::GetFolder(sdPluginsUser);
        resourceDir = ConfigManager::GetFolder(sdDataUser);
    }

    wxProgressDialog pd(_("Installing: ") + basename, wxString(L'\u00a0', 150), 5);

    wxString localName = basename + FileFilters::DYNAMICLIB_DOT_EXT;
    wxString resourceName = basename + ".zip";
    wxString settingsOnName = basename + ".png";
    wxString settingsOffName = basename + "-off.png";
    if (!platform::windows && resourceName.StartsWith("lib"))
        resourceName.Remove(0, 3);

    if (!platform::windows && settingsOnName.StartsWith("lib"))
        settingsOnName.Remove(0, 3);

    if (!platform::windows && settingsOffName.StartsWith("lib"))
        settingsOffName.Remove(0, 3);

    wxString pluginFilename = UnixFilename(pluginDir + '/' + localName);
//    Manager::Get()->GetLogManager()->DebugLog("Plugin filename: " + pluginFilename);
//    Manager::Get()->GetLogManager()->DebugLog("Plugin resources: " + ConfigManager::GetDataFolder() + '/' + resourceName);

    pd.Update(1, _("Extracting plugin"));

    // extract plugin from bundle
    if (!ExtractFile(actualName,
                    localName,
                    pluginFilename))
        return false;
//    Manager::Get()->GetLogManager()->DebugLog("Extracted plugin");

    pd.Update(2, _("Extracting plugin resources"));

    // extract resources from bundle
    if (!ExtractFile(actualName,
                    resourceName,
                    resourceDir + '/' + resourceName))
        return false;
//    Manager::Get()->GetLogManager()->DebugLog("Extracted resources");

    pd.Update(3, _("Extracting plugin icons for \"Settings\" dialog"));

    // extract resources from bundle
    ExtractFile(actualName,
                settingsOnName,
                resourceDir + "/images/settings/" + settingsOnName,
                false);
//    Manager::Get()->GetLogManager()->DebugLog("Extracted resources");

    // extract resources from bundle
    ExtractFile(actualName,
                settingsOffName,
                resourceDir + "/images/settings/" + settingsOffName,
                false);
//    Manager::Get()->GetLogManager()->DebugLog("Extracted resources");

    // extract extra files
    wxArrayString extraFiles;
    ReadExtraFilesFromManifestFile(localName, extraFiles);
    for (size_t i = 0; i < extraFiles.GetCount(); ++i)
    {
        ExtractFile(actualName,
                    extraFiles[i],
                    resourceDir + "/" + extraFiles[i],
                    false);
    }

    pd.Update(4, _("Loading plugin"));

    // bundle extracted; now load the plugin on-the-fly
//    Manager::Get()->GetLogManager()->DebugLog("Loading plugin...");
    ScanForPlugins(pluginDir);
    LoadAllPlugins();
    cbPlugin* plugin = FindPluginByFileName(pluginFilename);
    const PluginInfo* info = GetPluginInfo(plugin);
    if (!plugin || !info)
    {
        Manager::Get()->GetLogManager()->DebugLog("Failed");
        return false;
    }
//    Manager::Get()->GetLogManager()->DebugLog("Succeeded");

    // inform app to update menus and toolbars
    pd.Update(5, _("Updating menus and toolbars"));
    CodeBlocksEvent evt(cbEVT_PLUGIN_INSTALLED);
    evt.SetPlugin(plugin);
    Manager::Get()->ProcessEvent(evt);
//    Manager::Get()->GetLogManager()->DebugLog("Menus updated");

    return true;
}

bool PluginManager::UninstallPlugin(cbPlugin* plugin, bool removeFiles)
{
    if (!plugin)
        return false;

    wxString title;
    wxString pluginFilename;
    wxString resourceFilename;
    wxString settingsOnFilename;
    wxString settingsOffFilename;
    wxArrayString extrafiles;

    if (not plugin->CanDetach())
        return false;

    // find the plugin element
    for (size_t i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* elem = m_Plugins[i];
        if (elem && elem->plugin == plugin)
        {
            // got it
            title = elem->info.title;
            pluginFilename = elem->fileName;
            // now get the resource name
            wxFileName fname(pluginFilename);
            resourceFilename = fname.GetName() + ".zip";
            settingsOnFilename = fname.GetName() + ".png";
            settingsOffFilename = fname.GetName() + "-off.png";
            if (!platform::windows && resourceFilename.StartsWith("lib"))
                resourceFilename.Remove(0, 3);

            if (!platform::windows && settingsOnFilename.StartsWith("lib"))
                settingsOnFilename.Remove(0, 3);

            if (!platform::windows && settingsOffFilename.StartsWith("lib"))
                settingsOffFilename.Remove(0, 3);

            resourceFilename = ConfigManager::LocateDataFile(resourceFilename, sdDataGlobal | sdDataUser);
            settingsOnFilename = ConfigManager::LocateDataFile("images/settings/" + settingsOnFilename, sdDataGlobal | sdDataUser);
            settingsOffFilename = ConfigManager::LocateDataFile("images/settings/" + settingsOffFilename, sdDataGlobal | sdDataUser);

            ReadExtraFilesFromManifestFile(resourceFilename, extrafiles);
            for (size_t n = 0; n < extrafiles.GetCount(); ++n)
            {
                extrafiles[n] = ConfigManager::LocateDataFile(extrafiles[n], sdDataGlobal | sdDataUser);
            }
            break;
        }
    }

    if (wxFileExists(pluginFilename) && !wxFile::Access(pluginFilename, wxFile::write))
    {
        // no write-access; abort
        cbMessageBox(_("You don't have the needed privileges to uninstall this plugin.\n"
                       "Ask your administrator to uninstall this plugin for you..."),
                     _("Warning"), wxICON_WARNING);
        return false;
    }

    LogManager* logManager = Manager::Get()->GetLogManager();

//    logManager->DebugLog("UninstallPlugin:");
//    logManager->DebugLog("Plugin filename: " + pluginFilename);
//    logManager->DebugLog("Plugin resources: " + resourceFilename);

    wxProgressDialog pd(wxString::Format(_("Uninstalling %s"), title.c_str()),
                        wxString(L'\u00a0', 150), 3);

    pd.Update(1, _("Detaching plugin"));
    DetachPlugin(plugin);
//    logManager->DebugLog("Plugin released");

    pd.Update(2, _("Updating menus and toolbars"));
    CodeBlocksEvent event(cbEVT_PLUGIN_UNINSTALLED);
    event.SetPlugin(plugin);
    Manager::Get()->ProcessEvent(event);
//    logManager->DebugLog("Menus updated");

    pd.Update(3, _("Unloading plugin"));
    UnloadPlugin(plugin);
//    logManager->DebugLog("Plugin unloaded");

    if (!removeFiles)
        return true;

    // under linux, if the progress dialog is still visible and updated
    // causes a crash because it re-enters gtk_main_iteration() calling
    // eventually OnUpdateUI() in the config dialog, which in turn references
    // an invalid plugin...
//    pd.Update(4, _("Removing files"));

    if (!pluginFilename.empty())
    {
        if (wxRemoveFile(pluginFilename))
        {
//            logManager->DebugLog("Plugin file removed");
            if (!resourceFilename.empty())
            {
                if (!wxRemoveFile(resourceFilename))
                    logManager->LogWarning(wxString::Format(_("Failed to remove plugin resources: %s"), resourceFilename));
            }

            if (!settingsOnFilename.empty() && wxFileExists(settingsOnFilename))
            {
                if (!wxRemoveFile(settingsOnFilename))
                    logManager->LogWarning(wxString::Format(_("Failed to remove icon for \"Settings\" dialog: %s"), settingsOnFilename));
            }

            if (!settingsOffFilename.empty() && wxFileExists(settingsOffFilename))
            {
                if (!wxRemoveFile(settingsOffFilename))
                    logManager->LogWarning(wxString::Format(_("Failed to remove icon for \"Settings\" dialog: %s"), settingsOffFilename));
            }

            for (size_t i = 0; i < extrafiles.GetCount(); ++i)
            {
                if (!extrafiles[i].empty() && wxFileExists(extrafiles[i]))
                {
                    if (!wxRemoveFile(extrafiles[i]))
                        logManager->LogWarning(wxString::Format(_("Failed to remove extra file: %s"), extrafiles[i]));
                }
            }

            return true;
        }
        else
        {
            logManager->LogWarning(wxString::Format(_("Failed to remove plugin file: %s"), pluginFilename));
            cbMessageBox(_("Plugin could not be completely uninstalled because its files could not be removed.\n\n"
                            "This can happen if the plugin's file is in-use like, for "
                            "example, when the same plugin file provides more than one "
                            "plugin.\n"
                            "In this case either uninstall all other plugins "
                            "which are provided by the same file, or remove it yourself "
                            "(manually) when you shut down Code::Blocks.\n"
                            "The files that could not be deleted are:\n\n") +
                            pluginFilename + '\n' +
                            resourceFilename + '\n' +
                            settingsOnFilename + '\n' +
                            settingsOffFilename,
                            _("Warning"), wxICON_WARNING);
            return false;
        }
    }

    return false;
}

bool PluginManager::ExportPlugin(cbPlugin* plugin, const wxString& filename)
{
    if (!plugin)
        return false;

    wxArrayString sourcefiles;
    wxArrayString extrafiles;
    wxArrayString extrafilesdest;
    wxFileName fname;
    wxString resourceFilename;

    // find the plugin element
    for (size_t i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* elem = m_Plugins[i];
        if (elem && elem->plugin == plugin)
        {
            // got it

            // plugin file
            sourcefiles.Add(elem->fileName);
            fname.Assign(elem->fileName);

            // now get the resource zip filename
            resourceFilename = fname.GetName() + ".zip";
            if (!platform::windows && resourceFilename.StartsWith("lib"))
                resourceFilename.Remove(0, 3);

            resourceFilename = ConfigManager::LocateDataFile(resourceFilename, sdDataGlobal | sdDataUser);
            sourcefiles.Add(resourceFilename);

            // the highlighted icon the plugin may have for its "settings" page
            resourceFilename = fname.GetName() + ".png";
            if (!platform::windows && resourceFilename.StartsWith("lib"))
                resourceFilename.Remove(0, 3);

            resourceFilename.Prepend("images/settings/");
            resourceFilename = ConfigManager::LocateDataFile(resourceFilename, sdDataGlobal | sdDataUser);
            if (!resourceFilename.empty())
                sourcefiles.Add(resourceFilename);

            // the non-highlighted icon the plugin may have for its "settings" page
            resourceFilename = fname.GetName() + "-off.png";
            if (!platform::windows && resourceFilename.StartsWith("lib"))
                resourceFilename.Remove(0, 3);

            resourceFilename.Prepend("images/settings/");
            resourceFilename = ConfigManager::LocateDataFile(resourceFilename, sdDataGlobal | sdDataUser);
            if (!resourceFilename.empty())
                sourcefiles.Add(resourceFilename);

            // export extra files
            resourceFilename = fname.GetName() + ".zip";
            if (!platform::windows && resourceFilename.StartsWith("lib"))
                resourceFilename.Remove(0, 3);

            ReadExtraFilesFromManifestFile(resourceFilename, extrafilesdest);
            for (size_t n = 0; n < extrafilesdest.GetCount(); ++n)
            {
                extrafiles.Add(ConfigManager::LocateDataFile(extrafilesdest[n], sdDataGlobal | sdDataUser));
            }

            break;
        }
    }

    if (wxFileExists(filename))
    {
        if (!wxFile::Access(filename, wxFile::write))
        {
            cbMessageBox(wxString::Format(_("%s is in use.\nAborting..."), filename), _("Warning"), wxICON_WARNING);
            return false;
        }
    }

//    Manager::Get()->GetLogManager()->DebugLog("Creating archive: " + filename);
    wxFileOutputStream out(filename);
    wxZipOutputStream zip(out, 9); // max compression
    for (size_t i = 0; i < sourcefiles.GetCount(); ++i)
    {
        if (sourcefiles[i].empty())
            continue;

        wxFileInputStream in(sourcefiles[i]);
        zip.PutNextEntry(wxFileName(sourcefiles[i]).GetFullName());
        zip << in;
    }

    for (size_t i = 0; i < extrafiles.GetCount(); ++i)
    {
        if (extrafiles[i].empty() || extrafilesdest[i].empty())
            continue;

        wxFileInputStream in(extrafiles[i]);

        zip.PutNextEntry(extrafilesdest[i]);
        zip << in;
    }

    zip.SetComment("This is a redistributable plugin for the Code::Blocks IDE.\n"
                   "See https://www.codeblocks.org for details...");

    return true;
}

bool PluginManager::ExtractFile(const wxString& bundlename,
                                const wxString& src_filename,
                                const wxString& dst_filename,
                                bool isMandatory)
{
//    Manager::Get()->GetLogManager()->DebugLog("ExtractFile:");
//    Manager::Get()->GetLogManager()->DebugLog("Plugin filename: " + bundlename);
//    Manager::Get()->GetLogManager()->DebugLog("Source filename: " + src_filename);
//    Manager::Get()->GetLogManager()->DebugLog("Destination filename: " + dst_filename);

    // check if the destination file already exists
    if (wxFileExists(dst_filename) && !wxFile::Access(dst_filename, wxFile::write))
    {
//        Manager::Get()->GetLogManager()->DebugLog("Destination file in use");
        cbMessageBox(_("The destination file is in use.\nAborting..."), _("Warning"), wxICON_WARNING);
        return false;
    }

    // make sure destination dir exists
    CreateDirRecursively(wxFileName(dst_filename).GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR));

    // actually extract file
//    Manager::Get()->GetLogManager()->DebugLog("Extracting...");
    wxFileSystem* fs = new wxFileSystem;
    wxFSFile* f = fs->OpenFile(bundlename + "#zip:" + src_filename);
    if (f)
    {
        // open output file for writing
        wxFile output(dst_filename, wxFile::write);
        if (!output.IsOpened())
        {
//            Manager::Get()->GetLogManager()->DebugLog("Can't open destination file for writing");
            wxString msg = wxString::Format(_("Can't open destination file '%s' for writing..."),
                                            dst_filename);
            cbMessageBox(msg, _("Error"), wxICON_ERROR);
            delete f;
            delete fs;
            return false;
        }

        // copy file
        wxInputStream* is = f->GetStream();
        char tmp[1025];
        while (!is->Eof() && is->CanRead())
        {
            memset(tmp, 0, sizeof(tmp));
            is->Read(tmp, sizeof(tmp) - 1);
            output.Write(tmp, is->LastRead());
        }

        delete f;
//        Manager::Get()->GetLogManager()->DebugLog("Extracted");
    }
    else
    {
//        Manager::Get()->GetLogManager()->DebugLog("File not found in plugin");
        if (isMandatory)
        {
            wxString msg = wxString::Format(_("File '%s' not found in plugin '%s'"),
                                            src_filename, bundlename);
            cbMessageBox(msg, _("Error"), wxICON_ERROR);
            delete fs;
            return false;
        }
    }

    delete fs;
    return true;
}

void PluginManager::RegisterPlugin(const wxString& name,
                                    CreatePluginProc createProc,
                                    FreePluginProc freeProc,
                                    PluginSDKVersionProc versionProc)
{
    // sanity checks
    if (name.empty() || !createProc || !freeProc || !versionProc)
        return;

    // first check to see it's not already loaded
    if (FindPluginByName(name))
        return; // yes, already loaded

    // read manifest file for plugin
    PluginInfo info;
    if (!ReadManifestFile(m_CurrentlyLoadingFilename, name, &info))
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("No manifest file for plugin \"%s\" filename: %s"), name, m_CurrentlyLoadingFilename));
        return;
    }

    if (info.name.empty())
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Invalid manifest file for plugin \"%s\" filename: %s"), name, m_CurrentlyLoadingFilename));
        return;
    }

    // now get the SDK version number (extra check)
    int major;
    int minor;
    int release;
    versionProc(&major, &minor, &release);
    if (major != PLUGIN_SDK_VERSION_MAJOR ||
        minor != PLUGIN_SDK_VERSION_MINOR ||
        release != PLUGIN_SDK_VERSION_RELEASE)
    {
        // wrong version: in this case, inform the user...
        wxString fmt;
        fmt.Printf(_("SDK version mismatch for %s (%d.%d.%d). Expecting %d.%d.%d"),
                    name,
                    major,
                    minor,
                    release,
                    PLUGIN_SDK_VERSION_MAJOR,
                    PLUGIN_SDK_VERSION_MINOR,
                    PLUGIN_SDK_VERSION_RELEASE);

        Manager::Get()->GetLogManager()->LogError(fmt);
        return;
    }

    // all done
    // add this plugin in the temporary registration vector to be loaded
    // by LoadPlugin() (which triggered the call to this function).
    PluginRegistration pr;
    pr.name = name;
    pr.createProc = createProc;
    pr.freeProc = freeProc;
    pr.versionProc = versionProc;
    pr.info = info;
    m_RegisteredPlugins.push_back(pr);
}

static wxString RemoveCRAndTranslate(const wxString& value)
{
    wxString Result(value);
    Result.Replace("\r\n", "\n");
    return wxGetTranslation(Result);
}

bool PluginManager::ReadManifestFile(const wxString& pluginFilename,
                                     const wxString& pluginName,
                                     PluginInfo* infoOut)
{
    if (!m_pCurrentlyLoadingManifestDoc)
    {
        // find and load plugin's resource file
        // (pluginFilename contains no path info)
        wxFileName fname(pluginFilename);
        fname.SetExt("zip");
        wxString actual = fname.GetFullName();

        // remove 'lib' prefix from plugin name (if any)
#ifdef CB_AUTOCONF
        if (actual.StartsWith("lib"))
#else
        if (!platform::windows && actual.StartsWith("lib"))
#endif
            actual.Remove(0, 3);

        actual = ConfigManager::LocateDataFile(actual, sdPluginsUser | sdDataUser | sdPluginsGlobal | sdDataGlobal);
        if (actual.empty())
        {
            Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Plugin resource not found: %s"), fname.GetFullName()));
            return false; // not found
        }

        // load XML from ZIP
        wxString contents;
        wxFileSystem* fs = new wxFileSystem;
        wxFSFile* f = fs->OpenFile(actual + "#zip:manifest.xml");
        if (f)
        {
            wxInputStream* is = f->GetStream();
            char tmp[1024];
            while (!is->Eof() && is->CanRead())
            {
                memset(tmp, 0, sizeof(tmp));
                is->Read(tmp, sizeof(tmp) - 1);
                contents << cbC2U((const char*)tmp);
            }

            delete f;
        }
        else
        {
            Manager::Get()->GetLogManager()->LogError(wxString::Format(_("No plugin manifest file in resource: %s"), actual));
            delete fs;
            return false;
        }

        delete fs;

        // actually load XML document
        m_pCurrentlyLoadingManifestDoc = new TiXmlDocument;
        if (!m_pCurrentlyLoadingManifestDoc->Parse(cbU2C(contents)))
        {
            Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Plugin manifest could not be parsed: %s"), actual));
            return false;
        }
    }

    TiXmlElement* root = m_pCurrentlyLoadingManifestDoc->FirstChildElement("CodeBlocks_plugin_manifest_file");
    if (!root)
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Plugin resource file not valid (no root element found) for: %s"), pluginFilename));
        return false;
    }

    TiXmlElement* version = root->FirstChildElement("SdkVersion");
    if (!version)
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Plugin resource file not valid (no SdkVersion element found) for: %s"), pluginFilename));
        return false;
    }

    // check version
//    int major;
//    int minor;
//    int release;
//    if (version->QueryIntAttribute("major", &major) != TIXML_SUCCESS)
//        major = 0;
//    if (version->QueryIntAttribute("minor", &minor) != TIXML_SUCCESS)
//        minor = 0;
//    if (version->QueryIntAttribute("release", &release) != TIXML_SUCCESS)
//        release = 0;
//
//    if (major != PLUGIN_SDK_VERSION_MAJOR ||
//        minor != PLUGIN_SDK_VERSION_MINOR ||
//        release != PLUGIN_SDK_VERSION_RELEASE)
//    {
//        // wrong version: in this case, inform the user...
//        wxString fmt;
//        fmt.Printf(_("SDK version mismatch for %s (%d.%d.%d). Expecting %d.%d.%d"),
//                    pluginName.c_str(),
//                    major,
//                    minor,
//                    release,
//                    PLUGIN_SDK_VERSION_MAJOR,
//                    PLUGIN_SDK_VERSION_MINOR,
//                    PLUGIN_SDK_VERSION_RELEASE);
//        Manager::Get()->GetLogManager()->LogError(fmt);
//        return false;
//    }

    // if no plugin name specified, we 're done here (successfully)
    if (pluginName.empty() || !infoOut)
        return true;

    TiXmlElement* plugin = root->FirstChildElement("Plugin");
    while (plugin)
    {
        const char* name = plugin->Attribute("name");
        if (name)
        {
            const wxString convertedName(cbC2U(name));
            if (pluginName.IsSameAs(convertedName))
            {
                infoOut->name = pluginName;
                TiXmlElement* value = plugin->FirstChildElement("Value");
                while (value)
                {
                    // Most manifest*.xml files contain items formatted for Windows (with \r\n)
                    // Remove all \r from the translatable ones so poedit works without complaining
                    if (value->Attribute("title"))
                        infoOut->title = RemoveCRAndTranslate(cbC2U(value->Attribute("title")));

                    if (value->Attribute("version"))
                        infoOut->version = cbC2U(value->Attribute("version"));

                    if (value->Attribute("description"))
                        infoOut->description = RemoveCRAndTranslate(cbC2U(value->Attribute("description")));

                    if (value->Attribute("author"))
                        infoOut->author = RemoveCRAndTranslate(cbC2U(value->Attribute("author")));

                    if (value->Attribute("authorEmail"))
                        infoOut->authorEmail = cbC2U(value->Attribute("authorEmail"));

                    if (value->Attribute("authorWebsite"))
                        infoOut->authorWebsite = cbC2U(value->Attribute("authorWebsite"));

                    if (value->Attribute("thanksTo"))
                        infoOut->thanksTo = RemoveCRAndTranslate(cbC2U(value->Attribute("thanksTo")));

                    if (value->Attribute("license"))
                        infoOut->license = RemoveCRAndTranslate(cbC2U(value->Attribute("license")));

                    value = value->NextSiblingElement("Value");
                }

                break;
            }
            else if (pluginName.IsSameAs(convertedName, false))
            {
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("The plugin name \"%s\" case does not match the name in the \"%s\" file."), pluginName, convertedName));
            }
            else
            {
                Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("The plugin name \"%s\" does not match the name in the \"%s\" file."), pluginName, convertedName));
            }
        }

        plugin = plugin->NextSiblingElement("Plugin");
    }

    return true;
}

void PluginManager::ReadExtraFilesFromManifestFile(const wxString& pluginFilename,
                                                    wxArrayString& extraFiles)
{
    extraFiles.Clear();

    // find and load plugin's resource file
    // (pluginFilename contains no path info)
    wxFileName fname(pluginFilename);
    fname.SetExt("zip");
    wxString actual = fname.GetFullName();

    // remove 'lib' prefix from plugin name (if any)
#ifdef CB_AUTOCONF
    if (actual.StartsWith("lib"))
#else
    if (!platform::windows && actual.StartsWith("lib"))
#endif
        actual.Remove(0, 3);

    actual = ConfigManager::LocateDataFile(actual, sdPluginsUser | sdDataUser | sdPluginsGlobal | sdDataGlobal);
    if (actual.empty())
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Plugin resource not found: %s"), fname.GetFullName()));
        return; // not found
    }

    // load XML from ZIP
    wxString contents;
    wxFileSystem* fs = new wxFileSystem;
    wxFSFile* f = fs->OpenFile(actual + "#zip:manifest.xml");
    if (f)
    {
        wxInputStream* is = f->GetStream();
        char tmp[1025];
        while (!is->Eof() && is->CanRead())
        {
            memset(tmp, 0, sizeof(tmp));
            is->Read(tmp, sizeof(tmp) - 1);
            contents << cbC2U((const char*)tmp);
        }
        delete f;
    }
    else
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("No plugin manifest file in resource: %s"), actual));
        delete fs;
        return;
    }

    delete fs;

    // actually load XML document
    TiXmlDocument doc;
    if (!doc.Parse(cbU2C(contents)))
        return;

    TiXmlElement* root = doc.FirstChildElement("CodeBlocks_plugin_manifest_file");
    if (!root)
        return;

    TiXmlElement* extra = root->FirstChildElement("Extra");
    while (extra)
    {
        const char* file = extra->Attribute("file");
        if (file && *file)
        {
            extraFiles.Add(cbC2U(file));
        }

        extra = extra->NextSiblingElement("Extra");
    }
}

int PluginManager::ScanForPlugins(const wxString& path)
{
    static const wxString PluginsMask = platform::windows                      ? "*.dll"
                                      : (platform::darwin || platform::macosx) ? "*.dylib"
                                      :                                          "*.so";
    int count = 0;
    if (!wxDirExists(path))
        return count;

    wxDir dir(path);

    if (!dir.IsOpened())
        return count;

    bool batch = Manager::IsBatchBuild();

    // when debugging a single plugin, we need to exclude other dlls
    // GDB will start slowly if a lot of plugins(dlls) get loaded, since it has to parsed
    // each debug symbol of those plugins. The below three lines are a hack to use the "batch"
    // mode when debugging, so only a limited number of plugins get loaded.
    // only enable this option if the personality name is "debug-plugin"
    wxString personality(Manager::Get()->GetPersonalityManager()->GetPersonality());
    if (personality == "debug-plugin")
        batch = true;

    wxArrayString bbplugins;
    if (batch)
        bbplugins = cbReadBatchBuildPlugins();

    wxString filename;
    wxString failed;
    bool ok = dir.GetFirst(&filename, PluginsMask, wxDIR_FILES);
    while (ok)
    {
        if (batch)
        {
            // for batch builds, we will load only those plugins that the
            // user has set (default only compiler.dll)
            bool matched = false;
            for (size_t i = 0; i < bbplugins.GetCount(); ++i)
            {
                if (bbplugins[i] == filename)
                {
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                ok = dir.GetNext(&filename);
                continue;
            }
        }

        // load manifest
        m_pCurrentlyLoadingManifestDoc = nullptr;
        if (ReadManifestFile(filename))
        {
            if (LoadPlugin(path + wxFILE_SEP_PATH + filename))
                ++count;
            else
                failed << '\n' << filename;
        }

        if (m_pCurrentlyLoadingManifestDoc)
        {
            delete m_pCurrentlyLoadingManifestDoc;
            m_pCurrentlyLoadingManifestDoc = nullptr;
        }

        ok = dir.GetNext(&filename);
    }

    Manager::Get()->GetLogManager()->Log(wxString::Format(_("Loaded %d plugins"), count));
    if (!failed.empty())
    {
        InfoWindow::Display(_("Warning"),
                            _("One or more plugins were not loaded.\n"
                            "This usually happens when a plugin is built for\n"
                            "a different version of the Code::Blocks SDK.\n"
                            "Check the application log for more info.\n\n"
                            "List of failed plugins:\n") + failed,
                            15000, 3000);
    }

    return count;
}

bool PluginManager::LoadPlugin(const wxString& pluginName)
{
    // clear registration temporary vector
    m_RegisteredPlugins.clear();

    // load library
    m_CurrentlyLoadingFilename = pluginName;
    m_pCurrentlyLoadingLib = LibLoader::LoadLibrary(pluginName);
    if (!m_pCurrentlyLoadingLib->IsLoaded())
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("%s: not loaded (missing symbols?)"), pluginName));
        LibLoader::RemoveLibrary(m_pCurrentlyLoadingLib);
        m_pCurrentlyLoadingLib = nullptr;
        m_CurrentlyLoadingFilename.Clear();
        return false;
    }

    // by now, the library has loaded and its global variables are initialized.
    // this means it has already called RegisterPlugin()
    // now we can actually create the plugin(s) instance(s) :)

    // try to load the plugin(s)
    std::vector<PluginRegistration>::iterator it;
    for (it = m_RegisteredPlugins.begin(); it != m_RegisteredPlugins.end(); ++it)
    {
        PluginRegistration& pr = *it;
        cbPlugin* plug = nullptr;
        try
        {
            plug = pr.createProc();
        }
        catch (cbException& exception)
        {
            exception.ShowErrorMessage(false);
            continue;
        }

        // all done; add it to our list
        PluginElement* plugElem = new PluginElement;
        plugElem->fileName = m_CurrentlyLoadingFilename;
        plugElem->info = pr.info;
        plugElem->library = m_pCurrentlyLoadingLib;
        plugElem->freeProc = pr.freeProc;
        plugElem->plugin = plug;
        m_Plugins.Add(plugElem);
        if (plug->GetType() == ptCompiler)
            m_CompilerPlugins.push_back(static_cast<cbCompilerPlugin*>(plug));

        SetupLocaleDomain(pr.name);

        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("%s: loaded", pr.name));
    }

    if (m_RegisteredPlugins.empty())
    {
        // no plugins loaded from this library, but it's not an error
        LibLoader::RemoveLibrary(m_pCurrentlyLoadingLib);
    }

    m_pCurrentlyLoadingLib = nullptr;
    m_CurrentlyLoadingFilename.Clear();
    return true;
}

void PluginManager::LoadAllPlugins()
{
    // check if a plugin crashed the app last time
    wxString probPlugin = Manager::Get()->GetConfigManager("plugins")->Read("/try_to_activate", wxEmptyString);
    if (!probPlugin.empty())
    {
        wxString msg;
        msg.Printf(_("Plugin \"%s\" failed to load last time Code::Blocks was executed.\n"
                    "Do you want to disable this plugin from loading?"), probPlugin.c_str());
        if (cbMessageBox(msg, _("Warning"), wxICON_WARNING | wxYES_NO) == wxID_NO)
            probPlugin.clear();
    }

    PluginElement* elem = nullptr;
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        elem = m_Plugins[i];
        cbPlugin* plug = elem->plugin;
        if (!plug || plug->IsAttached())
            continue;

        // do not load it if the user has explicitly asked not to...
        wxString baseKey;
        baseKey << '/' << elem->info.name;
        bool loadIt = Manager::Get()->GetConfigManager("plugins")->ReadBool(baseKey, true);

        // if we have a problematic plugin, check if this is it
        if (loadIt && !probPlugin.empty())
        {
            loadIt = elem->info.title != probPlugin;
            // if this is the problematic plugin, don't load it
            if (!loadIt)
                Manager::Get()->GetConfigManager("plugins")->Write(baseKey, false);
        }

        if (loadIt)
        {
            Manager::Get()->GetConfigManager("plugins")->Write("/try_to_activate", elem->info.title);
            Manager::Get()->GetLogManager()->Log(elem->info.name);
            try
            {
                AttachPlugin(plug);
                Manager::Get()->GetConfigManager("plugins")->Write("/try_to_activate", wxEmptyString, false);
            }
            catch (cbException& exception)
            {
                Manager::Get()->GetLogManager()->Log(_("[failed]"));
                exception.ShowErrorMessage(false);

                wxString msg;
                msg.Printf(_("Plugin \"%s\" failed to load...\n"
                            "Do you want to disable this plugin from loading next time?"), elem->info.title);

                if (cbMessageBox(msg, _("Warning"), wxICON_WARNING | wxYES_NO) == wxID_YES)
                    Manager::Get()->GetConfigManager("plugins")->Write(baseKey, false);
            }
        }
    }

    Manager::Get()->GetConfigManager("plugins")->Write("/try_to_activate", wxEmptyString, false);
}

void PluginManager::UnloadAllPlugins()
{
//    Manager::Get()->GetLogManager()->DebugLog("Count %d", m_Plugins.GetCount());

    while (m_Plugins.GetCount())
    {
        UnloadPlugin(m_Plugins[0]->plugin);
    }

    m_CompilerPlugins.clear();
    m_Plugins.Clear();
    LibLoader::Cleanup();
}

void PluginManager::UnloadPlugin(cbPlugin* plugin)
{
    if (!plugin)
        return;

    // detach plugin if needed
    DetachPlugin(plugin);

    // find plugin element
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* plugElem = m_Plugins[i];
        if (plugElem->plugin == plugin)
        {
            if (plugin->GetType() == ptCompiler)
            {
                auto removeIter = std::remove(m_CompilerPlugins.begin(), m_CompilerPlugins.end(), plugin);
                if (removeIter != m_CompilerPlugins.end())
                    m_CompilerPlugins.erase(removeIter);
            }

            // found
            // free plugin
            if (plugElem->freeProc)
                plugElem->freeProc(plugin);
            else
                delete plugin; // try to delete it ourselves...
            // remove lib

            LibLoader::RemoveLibrary(plugElem->library);
            // and delete plugin element
            delete plugElem;
            m_Plugins.RemoveAt(i);

            break;
        }
    }
}

PluginElement* PluginManager::FindElementByName(const wxString& pluginName)
{
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* plugElem = m_Plugins[i];
        if (plugElem->info.name == pluginName)
            return plugElem;
    }

    return nullptr;
}

cbPlugin* PluginManager::FindPluginByName(const wxString& pluginName)
{
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* plugElem = m_Plugins[i];
        if (plugElem->info.name == pluginName)
            return plugElem->plugin;
    }

    return nullptr;
}

cbPlugin* PluginManager::FindPluginByFileName(const wxString& pluginFileName)
{
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* plugElem = m_Plugins[i];
        if (plugElem->fileName == pluginFileName)
            return plugElem->plugin;
    }

    return nullptr;
}

const PluginInfo* PluginManager::GetPluginInfo(const wxString& pluginName)
{
    PluginElement* plugElem = FindElementByName(pluginName);
    if (plugElem && plugElem->info.name == pluginName)
        return &plugElem->info;

    return nullptr;
}

const PluginInfo* PluginManager::GetPluginInfo(cbPlugin* plugin)
{
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        PluginElement* plugElem = m_Plugins[i];
        if (plugElem->plugin == plugin)
            return &plugElem->info;
    }

    return nullptr;
}

int PluginManager::ExecutePlugin(const wxString& pluginName)
{
    PluginElement* elem = FindElementByName(pluginName);
    cbPlugin* plug = elem ? elem->plugin : nullptr;
    if (plug)
    {
        if (plug->GetType() != ptTool)
        {
            Manager::Get()->GetLogManager()->LogError(wxString::Format(_("Plugin %s is not a tool to have Execute() method!"), elem->info.name));
        }
        else
        {
            try
            {
                return ((cbToolPlugin*)plug)->Execute();
            }
            catch (cbException& exception)
            {
                exception.ShowErrorMessage(false);
            }
        }
    }
    else
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format(_("No plugin registered by this name: %s"), pluginName));
    }

    return 0;
}

inline int SortByConfigurationPriority(cbPlugin** first, cbPlugin** second)
{
    return (*first)->GetConfigurationPriority() - (*second)->GetConfigurationPriority();
}

void PluginManager::GetConfigurationPanels(int group, wxWindow* parent,
                                           ConfigurationPanelsArray& arrayToFill,
                                           cbConfigurationPanelColoursInterface *coloursInterface)
{
    // build an array of Plugins* because we need to order it by configuration priority
    PluginsArray arr;
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        cbPlugin* plug = m_Plugins[i]->plugin;
        // all check are done here
        if (plug && plug->IsAttached() && (plug->GetConfigurationGroup() & group))
            arr.Add(plug);
    }

    // sort the array
    arr.Sort(SortByConfigurationPriority);

    // now enumerate the array and fill the supplied configurations panel array
    arrayToFill.Clear();
    for (unsigned int i = 0; i < arr.GetCount(); ++i)
    {
        cbPlugin* plug = arr[i];
        cbConfigurationPanel* pnl = plug->GetConfigurationPanelEx(parent, coloursInterface);
        if (pnl)
            arrayToFill.Add(pnl);
    }
}

void PluginManager::GetProjectConfigurationPanels(wxWindow* parent, cbProject* project, ConfigurationPanelsArray& arrayToFill)
{
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        cbPlugin* plug = m_Plugins[i]->plugin;
        if (plug && plug->IsAttached())
        {
            cbConfigurationPanel* pnl = plug->GetProjectConfigurationPanel(parent, project);
            if (pnl)
                arrayToFill.Add(pnl);
        }
    }
}

cbCompilerPlugin* PluginManager::GetFirstCompiler() const
{
    if (m_CompilerPlugins.empty())
        return nullptr;

    return m_CompilerPlugins.front();
}

PluginsArray PluginManager::GetToolOffers()
{
    return GetOffersFor(ptTool);
}

PluginsArray PluginManager::GetMimeOffers()
{
    return GetOffersFor(ptMime);
}

PluginsArray PluginManager::GetDebuggerOffers()
{
    return GetOffersFor(ptDebugger);
}

PluginsArray PluginManager::GetCodeCompletionOffers()
{
    return GetOffersFor(ptCodeCompletion);
}

PluginsArray PluginManager::GetSmartIndentOffers()
{
    return GetOffersFor(ptSmartIndent);
}

PluginsArray PluginManager::GetOffersFor(PluginType type)
{
    PluginsArray arr;

    // special case for MIME plugins
    // we 'll add the default MIME handler, last in the returned array
    cbPlugin* dflt = nullptr;

    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        cbPlugin* plug = m_Plugins[i]->plugin;
        if (plug && plug->IsAttached() && plug->GetType() == type)
        {
            if (type == ptMime)
            {
                // default MIME handler?
                if (((cbMimePlugin*)plug)->HandlesEverything())
                    dflt = plug;
                else
                    arr.Add(plug);
            }
            else
                arr.Add(plug);
        }
    }

    // add default MIME handler last
    if (dflt)
        arr.Add(dflt);

    return arr;
}

void PluginManager::AskPluginsForModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
{
    std::map<wxString, cbPlugin*> sortedPlugins;
    for (unsigned int i = 0; i < m_Plugins.GetCount(); ++i)
    {
        cbPlugin* plug = m_Plugins[i]->plugin;
        if (plug && plug->IsAttached())
            sortedPlugins[m_Plugins[i]->info.name] = plug;
    }

    // We want the order of iteration to be more stable, so there are fewer surprises on different machines.
    for (auto &pair : sortedPlugins)
    {
        try
        {
            pair.second->BuildModuleMenu(type, menu, data);
        }
        catch (cbException& exception)
        {
            exception.ShowErrorMessage(false);
        }
    }

    // script plugins now
    wxArrayInt ids = ScriptBindings::ScriptPluginWrapper::CreateModuleMenu(type, menu, data);
    for (size_t i = 0; i < ids.GetCount(); ++i)
    {
        Connect(ids[i], -1, wxEVT_COMMAND_MENU_SELECTED,
                (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
                &PluginManager::OnScriptModuleMenu);
    }
}

void PluginManager::ResetModuleMenu()
{
    m_FindMenuItemCount = 0;
    m_FindMenuItemFirst = 0;
    m_LastNonPluginMenuId = 0;
}

void PluginManager::RegisterFindMenuItems(bool before, int count)
{
    if (before)
        m_FindMenuItemFirst += count;
    else
        m_FindMenuItemCount += count;
}

int PluginManager::GetFindMenuItemCount() const
{
    return m_FindMenuItemCount;
}

int PluginManager::GetFindMenuItemFirst() const
{
    return m_FindMenuItemFirst;
}

void PluginManager::RegisterLastNonPluginMenuItem(int id)
{
    m_LastNonPluginMenuId = id;
}

int PluginManager::FindSortedMenuItemPosition(wxMenu &popup, const wxString& label) const
{
    wxString labelNoAmpersands = label;
    // Remove ampersands, because they are not visible but affect the sorting if they are at the
    // start of the label.
    labelNoAmpersands.erase(std::remove(labelNoAmpersands.begin(), labelNoAmpersands.end(), '&'),
                            labelNoAmpersands.end());

    int position = -1;
    const wxMenuItemList &items = popup.GetMenuItems();
    const int count = int(items.size());
    for (int ii = 0; ii < count; ++ii)
    {
        if (items[ii]->GetId() == m_LastNonPluginMenuId)
        {
            position = ii + 1;
            break;
        }
    }

    if (position == -1 || (position >= count))
        return count;

    if (items[position]->GetKind() == wxITEM_SEPARATOR)
        position++;

    // Linear search for now. The number of items isn't large, so it shouldn't be a performance
    // problem.
    for (int ii = position; ii < count; ++ii)
    {
        const wxString &itemLabel = items[ii]->GetItemLabelText();
        if (labelNoAmpersands.CmpNoCase(itemLabel) <= 0)
            return ii;
    }

    return count;
}

void PluginManager::OnScriptMenu(wxCommandEvent& event)
{
    ScriptBindings::ScriptPluginWrapper::OnScriptMenu(event.GetId());
}

void PluginManager::OnScriptModuleMenu(wxCommandEvent& event)
{
    ScriptBindings::ScriptPluginWrapper::OnScriptModuleMenu(event.GetId());
}

cbMimePlugin* PluginManager::GetMIMEHandlerForFile(const wxString& filename)
{
    PluginsArray mimes = GetMimeOffers();
    for (unsigned int i = 0; i < mimes.GetCount(); ++i)
    {
        cbMimePlugin* plugin = (cbMimePlugin*)mimes[i];
        if (plugin && plugin->CanHandleFile(filename))
            return plugin;
    }

    return nullptr;
}

int PluginManager::Configure()
{
    PluginsConfigurationDlg dlg(Manager::Get()->GetAppWindow());
    PlaceWindow(&dlg);
    return dlg.ShowModal();
}

void PluginManager::SetupLocaleDomain(const wxString& DomainName)
{
    int catalogNum = Manager::Get()->GetConfigManager("app")->ReadInt("/locale/catalogNum",(int)0);
    int i = 1;
    for (; i <= catalogNum; ++i)
    {
        wxString catalogName = Manager::Get()->GetConfigManager("app")->Read(wxString::Format("/locale/Domain%d", i), wxEmptyString);
        if (catalogName.Cmp(DomainName) == 0)
            break;
    }

    if (i > catalogNum)
    {
        ++catalogNum;
        Manager::Get()->GetConfigManager("app")->Write("/locale/catalogNum", (int)catalogNum);
        Manager::Get()->GetConfigManager("app")->Write(wxString::Format("/locale/Domain%d", i), DomainName);
    }
}

void PluginManager::NotifyPlugins(CodeBlocksEvent& event)
{
    Manager::Get()->ProcessEvent(event);
}

void PluginManager::NotifyPlugins(CodeBlocksDockEvent& event)
{
    Manager::Get()->ProcessEvent(event);
}

void PluginManager::NotifyPlugins(CodeBlocksLayoutEvent& event)
{
    Manager::Get()->ProcessEvent(event);
}

bool cbHasRunningCompilers(const PluginManager *manager)
{
    for (const cbCompilerPlugin* compiler : manager->GetCompilerPlugins())
    {
        if (compiler && compiler->IsRunning())
            return true;
    }

    return false;
}

void cbStopRunningCompilers(PluginManager *manager)
{
    for (cbCompilerPlugin* compiler : manager->GetCompilerPlugins())
    {
        if (!compiler || !compiler->IsRunning())
            continue;

        compiler->KillProcess();
        while (compiler->IsRunning())
        {
            wxMilliSleep(100);
            Manager::Yield();
        }
    }
}
