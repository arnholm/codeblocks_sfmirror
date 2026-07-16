/***************************************************************
 * Name:      CB-Plugin
 * Purpose:   Code::Blocks plugin
 * Author:     ()
 * Created:   2009-02-02
 * Copyright:
 * License:   GPL
 **************************************************************/

#ifndef FILEMANAGER_H_INCLUDED
#define FILEMANAGER_H_INCLUDED

// For compilers that support precompilation, includes <wx/wx.h>
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <cbplugin.h> // for "class cbPlugin"

#include "FileExplorer.h" // provides the FileExplorer class

class FileManagerPlugin : public cbPlugin
{
    public:
        FileManagerPlugin();
        virtual ~FileManagerPlugin();

    protected:
        void BuildMenu(wxMenuBar* /*menuBar*/) override {}
        void BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data = nullptr) override;
        bool BuildToolBar(wxToolBar* /*toolBar*/) override {return false;}
        void OnAttach() override;
        void OnRelease(bool appShutDown) override;
        void OnOpenProjectInFileBrowser(wxCommandEvent& event);
        FileExplorer *m_fe;
        DECLARE_EVENT_TABLE();
    private:
        wxString m_project_selected;
};

#endif // FILEMANAGER_H_INCLUDED
