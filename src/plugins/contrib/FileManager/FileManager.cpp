#include <wx/aui/aui.h>
#include <sdk.h> // Code::Blocks SDK
#ifndef CB_PRECOMP
    #include <cbauibook.h>
    #include <cbproject.h>
    #include <projectmanager.h>
#endif
//#include <configurationpanel.h>

#include "FileManager.h"

// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
    PluginRegistrant<FileManagerPlugin> reg("FileManager");
}

int ID_ProjectOpenInFileBrowser=wxNewId();

BEGIN_EVENT_TABLE(FileManagerPlugin, cbPlugin)
    EVT_MENU(ID_ProjectOpenInFileBrowser, FileManagerPlugin::OnOpenProjectInFileBrowser)
END_EVENT_TABLE()


// constructor
FileManagerPlugin::FileManagerPlugin() : m_fe(nullptr)
{
    if(!Manager::LoadResource("FileManager.zip"))
        NotifyMissingFile("FileManager.zip");
}

// destructor
FileManagerPlugin::~FileManagerPlugin()
{
}

void FileManagerPlugin::OnAttach()
{
    //Create a new instance of the FileExplorer and attach it to the Project Manager notebook
    cbAuiNotebook *nb = Manager::Get()->GetProjectManager()->GetUI().GetNotebook();
    m_fe = new FileExplorer(nb);
    nb->AddPage(m_fe, _("Files"));
}

void FileManagerPlugin::OnRelease(bool /*appShutDown*/)
{
    if (m_fe) //remove the File Explorer from the management pane and destroy it
    {
        cbAuiNotebook *notebook = Manager::Get()->GetProjectManager()->GetUI().GetNotebook();
        int idx = notebook->GetPageIndex(m_fe);
        if (idx != -1)
            notebook->RemovePage(idx);

        delete m_fe;
        m_fe = nullptr;
    }
}

void FileManagerPlugin::BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
{
	if(type==mtProjectManager && data && data->GetKind()==FileTreeData::ftdkProject)
	{
	    m_project_selected=wxFileName(data->GetProject()->GetFilename()).GetPath();
        menu->Append(ID_ProjectOpenInFileBrowser, _("Open Project Folder in File Browser"), _("Opens the folder containing the project file in the file browser"));
	}
}

void FileManagerPlugin::OnOpenProjectInFileBrowser(wxCommandEvent& /*event*/)
{
    cbAuiNotebook *nb = Manager::Get()->GetProjectManager()->GetUI().GetNotebook();
    nb->SetSelection(nb->GetPageIndex(m_fe));
    m_fe->SetRootFolder(m_project_selected);
}
