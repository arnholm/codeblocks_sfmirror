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
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/filename.h>
    #include <wx/intl.h>
    #include <wx/listctrl.h>
    #include <wx/string.h>
    #include <wx/utils.h>
    #include <wx/xrc/xmlres.h>

    #include "manager.h"
    #include "configmanager.h"
    #include "pluginmanager.h"
    #include "cbplugin.h" // IsAttached
#endif

#include "annoyingdialog.h"
#include "ccmanager.h"
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/html/htmlwin.h>
#include <wx/progdlg.h>
#include <wx/settings.h>
#include <wx/tooltip.h>

#include "pluginsconfigurationdlg.h" // class's header file

static wxString GetInitialInfo()
{
    wxString initialInfo;
    initialInfo << _T("<html><body><font color=\"#0000AA\">");
    initialInfo << _("Tip: The above list allows for multiple selections.");
    initialInfo << _T("</font><br /><br /><b><font color=\"red\">");
    initialInfo << _("Have you saved your work first?");
    initialInfo << _T("</font></b><br /><i><font color=\"black\">\n");
    initialInfo << _("If a plugin is not well-written, it could cause Code::Blocks to crash when performing any operation on it...");
    initialInfo << _T("<br></font></b><br /><i><font color=\"green\">\n");
    initialInfo << _("Some additional plugins can be found here:");
    initialInfo << _T("</font></b><br /><i><font color=\"black\">\n");
    initialInfo << _T("<A href=\"https://wiki.codeblocks.org/index.php?title=Announcement_for_plugins/patches\">");
    initialInfo << _T("https://wiki.codeblocks.org/index.php?title=Announcement_for_plugins/patches\n </A>");

    if (PluginManager::GetSafeMode())
    {
        initialInfo << _T("</font></i><br /><br /><b><font color=\"red\">");
        initialInfo << _("Code::Blocks started up in \"safe-mode\"");
        initialInfo << _T("</font></b><br /><i><font color=\"black\">\n");
        initialInfo << _("All plugins were disabled on startup so that you can troubleshoot problematic plugins. Enable plugins at will now...");
    }

    initialInfo << _T("</font></i><br /></body></html>\n");
    return initialInfo;
}

inline int wxCALLBACK sortByTitle(wxIntPtr item1, wxIntPtr item2, cb_unused wxIntPtr sortData)
{
    const PluginElement* elem1 = (const PluginElement*)item1;
    const PluginElement* elem2 = (const PluginElement*)item2;

    return elem1->info.title.CmpNoCase(elem2->info.title);
}

BEGIN_EVENT_TABLE(PluginsConfigurationDlg, wxScrollingDialog)
    EVT_BUTTON(XRCID("btnEnable"), PluginsConfigurationDlg::OnToggle)
    EVT_BUTTON(XRCID("btnDisable"), PluginsConfigurationDlg::OnToggle)
    EVT_BUTTON(XRCID("btnInstall"), PluginsConfigurationDlg::OnInstall)
    EVT_BUTTON(XRCID("btnUninstall"), PluginsConfigurationDlg::OnUninstall)
    EVT_BUTTON(XRCID("btnExport"), PluginsConfigurationDlg::OnExport)
    EVT_LIST_ITEM_SELECTED(XRCID("lstPlugins"), PluginsConfigurationDlg::OnSelect)

    EVT_UPDATE_UI(-1, PluginsConfigurationDlg::OnUpdateUI)
    EVT_HTML_LINK_CLICKED(XRCID("htmlInfo"), PluginsConfigurationDlg::OnLinkClicked)
END_EVENT_TABLE()

// class constructor
PluginsConfigurationDlg::PluginsConfigurationDlg(wxWindow* parent)
{
    wxXmlResource::Get()->LoadObject(this, parent, _T("dlgConfigurePlugins"),_T("wxScrollingDialog"));
    XRCCTRL(*this, "wxID_CANCEL", wxButton)->SetDefault();
    FillList();

    // install options
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("plugins"));
    bool globalInstall = cfg->ReadBool(_T("/install_globally"), true);
    bool confirmation = cfg->ReadBool(_T("/install_confirmation"), true);

    // verify user can install globally
    DirAccessCheck access = cbDirAccessCheck(ConfigManager::GetFolder(sdPluginsGlobal));
    if (access != dacReadWrite)
    {
        globalInstall = false;
        // disable checkbox
        XRCCTRL(*this, "chkInstallGlobally", wxCheckBox)->Enable(false);
    }
    XRCCTRL(*this, "chkInstallGlobally", wxCheckBox)->SetValue(globalInstall);
    XRCCTRL(*this, "chkInstallConfirmation", wxCheckBox)->SetValue(confirmation);

    // Set default font size based on system default font size
#ifdef __linux__
    /* NOTE (mandrav#1#): wxWidgets documentation on wxHtmlWindow::SetFonts(),
    states that the sizes array accepts values from -2 to +4.
    My tests (under linux at least) have showed that it actually
    expects real point sizes. */

    wxFont systemFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    int sizes[7] = {};
    for (int i = 0; i < 7; ++i)
        sizes[i] = systemFont.GetPointSize();
    XRCCTRL(*this, "htmlInfo", wxHtmlWindow)->SetFonts(wxEmptyString, wxEmptyString, &sizes[0]);
#endif

    XRCCTRL(*this, "htmlInfo", wxHtmlWindow)->SetPage(GetInitialInfo());

    XRCCTRL(*this, "lstPlugins", wxListCtrl)->Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(PluginsConfigurationDlg::OnMouseMotion));
    XRCCTRL(*this, "lstPlugins", wxListCtrl)->Connect(wxEVT_MOTION,       wxMouseEventHandler(PluginsConfigurationDlg::OnMouseMotion));
}

void PluginsConfigurationDlg::FillList()
{
    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    if (list->GetColumnCount() == 0)
    {
        list->InsertColumn(0, _("Title"));
        list->InsertColumn(1, _("Version"));
        list->InsertColumn(2, _("Enabled"), wxLIST_FORMAT_CENTER);
        list->InsertColumn(3, _("Filename"));
    }

    PluginManager* man = Manager::Get()->GetPluginManager();
    const PluginElementsArray& plugins = man->GetPlugins();

    // populate Plugins checklist
    list->DeleteAllItems();
    for (unsigned int i = 0; i < plugins.GetCount(); ++i)
    {
        const PluginElement* elem = plugins[i];

        long idx = list->InsertItem(i, elem->info.title);
        list->SetItem(idx, 1, elem->info.version);
        list->SetItem(idx, 2, elem->plugin->IsAttached() ? _("Yes") : _("No"));
        list->SetItem(idx, 3, UnixFilename(elem->fileName).AfterLast(wxFILE_SEP_PATH));
        list->SetItemPtrData(idx, (wxUIntPtr)elem);

        if (!elem->plugin->IsAttached())
            list->SetItemTextColour(idx, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        else
            list->SetItemTextColour(idx, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }

    list->SetColumnWidth(0, wxLIST_AUTOSIZE);
    list->SetColumnWidth(1, wxLIST_AUTOSIZE_USEHEADER);
    list->SetColumnWidth(2, wxLIST_AUTOSIZE_USEHEADER);
    list->SetColumnWidth(3, wxLIST_AUTOSIZE);

    list->SortItems(sortByTitle, 0);
}

// class destructor
PluginsConfigurationDlg::~PluginsConfigurationDlg()
{
    XRCCTRL(*this, "lstPlugins", wxListCtrl)->Disconnect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(PluginsConfigurationDlg::OnMouseMotion));
    XRCCTRL(*this, "lstPlugins", wxListCtrl)->Disconnect(wxEVT_MOTION,       wxMouseEventHandler(PluginsConfigurationDlg::OnMouseMotion));
}

void PluginsConfigurationDlg::OnToggle(wxCommandEvent& event)
{
    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    if (list->GetSelectedItemCount() == 0)
        return;
    bool isEnable = event.GetId() == XRCID("btnEnable");

    wxBusyCursor busy;

    wxProgressDialog pd(wxString::Format(_("%s plugin(s)"), isEnable ? _("Enabling") : _("Disabling")),
                        wxString(L'\u00a0', 150),
                        list->GetSelectedItemCount(),
                        this,
                        wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_CAN_ABORT);

    int count = 0;
    long sel = -1;
    bool skip = false;
    wxString failure;
    while (true)
    {
        sel = list->GetNextItem(sel, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1)
            break;

        const PluginElement* elem = (const PluginElement*)list->GetItemData(sel);
        if (elem && elem->plugin)
        {
            pd.Update(++count,
                        wxString::Format("%s \"%s\"...", isEnable ? _("Enabling") : _("Disabling"), elem->info.title),
                        &skip);
            if (skip)
                break;

            if (elem->plugin->IsAttached() and (not elem->plugin->CanDetach()))
            {
                failure << elem->info.title << '\n';
                continue;
            }

            if (!isEnable && elem->plugin->IsAttached())
                Manager::Get()->GetPluginManager()->DetachPlugin(elem->plugin);
            else if (isEnable && !elem->plugin->IsAttached())
                Manager::Get()->GetPluginManager()->AttachPlugin(elem->plugin, true); // ignore safe-mode here
            else
                continue;

            wxListItem item;
            item.SetId(sel);
            item.SetColumn(2);
            item.SetMask(wxLIST_MASK_TEXT);
            list->GetItem(item);

            list->SetItem(sel, 2, elem->plugin->IsAttached() ? _("Yes") : _("No"));
            if (!elem->plugin->IsAttached())
                list->SetItemTextColour(sel, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
            else
                list->SetItemTextColour(sel, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

            // update configuration
            wxString baseKey;
            baseKey << _T("/") << elem->info.name;
            Manager::Get()->GetConfigManager(_T("plugins"))->Write(baseKey, elem->plugin->IsAttached());
        }
    }
    if (!failure.IsEmpty())                                                     //(ph 2021/07/15)
        cbMessageBox(_("One or more plugins were not enabled/disabled successfully:\n\n") + failure, _("Warning"), wxICON_WARNING, this); //(ph 2021/07/15)
}

void PluginsConfigurationDlg::OnInstall(cb_unused wxCommandEvent& event)
{
    wxFileDialog fd(this,
                        _("Select plugin to install"),
                        wxEmptyString, wxEmptyString,
                        _("Code::Blocks Plugins") + " (*.cbplugin)|*.cbplugin",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE | compatibility::wxHideReadonly);
    PlaceWindow(&fd);
    if (fd.ShowModal() != wxID_OK)
        return;

    wxBusyCursor busy;

    wxArrayString paths;
    fd.GetPaths(paths);

    // install in global or user dirs?
    bool globalInstall = XRCCTRL(*this, "chkInstallGlobally", wxCheckBox)->GetValue();
    bool confirm = XRCCTRL(*this, "chkInstallConfirmation", wxCheckBox)->GetValue();

    wxString failure;
    for (size_t i = 0; i < paths.GetCount(); ++i)
    {
        if (!Manager::Get()->GetPluginManager()->InstallPlugin(paths[i], globalInstall, confirm))
            failure << paths[i] << _T('\n');
    }

    FillList();
    if (!failure.IsEmpty())
        cbMessageBox(_("One or more plugins were not installed successfully:\n\n") + failure, _("Warning"), wxICON_WARNING, this);
}

void PluginsConfigurationDlg::OnUninstall(cb_unused wxCommandEvent& event)
{
    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    if (list->GetSelectedItemCount() == 0)
        return;

    wxBusyCursor busy;

    long sel = -1;
    wxString failure;
    while (true)
    {
        sel = list->GetNextItem(sel, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1)
            break;

        const PluginElement* elem = (const PluginElement*)list->GetItemData(sel);
        if (elem && elem->plugin)
        {
            wxString title = elem->info.title; //fetch info before uninstalling
            if (!Manager::Get()->GetPluginManager()->UninstallPlugin(elem->plugin))
                failure << title << '\n';
        }
    }

    FillList();
    if (!failure.IsEmpty())
        cbMessageBox(_("One or more plugins were not un-installed successfully:\n\n") + failure, _("Warning"), wxICON_WARNING, this);

    XRCCTRL(*this, "htmlInfo", wxHtmlWindow)->SetPage(GetInitialInfo());
}

void PluginsConfigurationDlg::OnExport(cb_unused wxCommandEvent& event)
{
    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    if (list->GetSelectedItemCount() == 0)
        return;

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("plugins_configuration"));
    wxDirDialog dd(this, _("Select directory to export plugin"), cfg->Read(_T("/last_export_path")), wxDD_NEW_DIR_BUTTON);
    PlaceWindow(&dd);
    if (dd.ShowModal() != wxID_OK)
        return;
    cfg->Write(_T("/last_export_path"), dd.GetPath());

    wxBusyCursor busy;
    wxProgressDialog pd(_("Exporting plugin(s)"),
                        wxString(L'\u00a0', 150),
                        list->GetSelectedItemCount(),
                        this,
                        wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_CAN_ABORT |
                        wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);

    int count = 0;
    long sel = -1;
    bool skip = false;
    bool confirmed = false;
    wxString failure;
    wxArrayString files; // avoid exporting different plugins from the same file twice
    while (true)
    {
        sel = list->GetNextItem(sel, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1)
            break;

        const PluginElement* elem = (const PluginElement*)list->GetItemData(sel);
        if (!elem || !elem->plugin)
        {
            failure << list->GetItemText(sel) << '\n';
            continue;
        }

        // avoid duplicates
        if (files.Index(elem->fileName) != wxNOT_FOUND)
            continue;
        files.Add(elem->fileName);

        // normalize version
        wxString version = wxGetTranslation(elem->info.version);
        version.Replace("/",  "_", true);
        version.Replace("\\", "_", true);
        version.Replace("?",  "_", true);
        version.Replace("*",  "_", true);
        version.Replace(">",  "_", true);
        version.Replace("<",  "_", true);
        version.Replace(" ",  "_", true);
        version.Replace("\t", "_", true);
        version.Replace("|",  "_", true);

        wxFileName fname;
        fname.SetPath(dd.GetPath());
        fname.SetName(wxFileName(elem->fileName).GetName() + "-" + version);
        fname.SetExt(_T("cbplugin"));

        pd.Update(++count,
                    wxString::Format(_("Exporting \"%s\"..."), elem->info.title),
                    &skip);
        if (skip)
            break;

        wxString filename = fname.GetFullPath();

        if (!confirmed && wxFileExists(filename))
        {
            AnnoyingDialog dlg(_("Overwrite confirmation"),
                                wxString::Format(_("%s already exists.\n"
                                "Are you sure you want to overwrite it?"), filename),
                                wxART_QUESTION,
                                AnnoyingDialog::THREE_BUTTONS,
                                AnnoyingDialog::rtONE,
                                _("&Yes"), _("Yes to &all"), _("&No"));
            switch (dlg.ShowModal())
            {
                case AnnoyingDialog::rtTHREE:
                    continue;
                    break;

                case AnnoyingDialog::rtTWO:
                    confirmed = true;
                    break;

                default:
                    break;
            }
        }

        if (!Manager::Get()->GetPluginManager()->ExportPlugin(elem->plugin, filename))
            failure << list->GetItemText(sel) << '\n';
    }

    if (!failure.IsEmpty())
        cbMessageBox(_("Failed exporting one or more plugins:\n\n") + failure, _("Warning"), wxICON_WARNING, this);
}

void PluginsConfigurationDlg::OnSelect(cb_unused wxListEvent& event)
{
    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    if (list->GetSelectedItemCount() != 1)
        return;

    long sel = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    const PluginElement* elem = (const PluginElement*)list->GetItemData(sel);
    if (!elem)
        return;

    wxString description(elem->info.description);
    description.Replace(_T("\n"), _T("<br />\n"));

    wxString info;
    info << _T("<html><body>\n");
    info << _T("<h3>") << elem->info.title << " ";
    info << _T("<font color=\"#0000AA\">") << elem->info.version << _T("</font></h3>");
    info << _T("<i><font color=\"#808080\" size=\"-1\">") << UnixFilename(elem->fileName) << _T("</font></i><br />\n");
    info << _T("<br />\n");
    info << description << _T("<br />\n");
    info << _T("</body></html>\n");

    XRCCTRL(*this, "htmlInfo", wxHtmlWindow)->SetPage(info);
}

void PluginsConfigurationDlg::OnMouseMotion(wxMouseEvent& event)
{
    event.Skip();
    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    if (event.Leaving())
    {
        if (list->GetToolTip())
            list->UnsetToolTip();
        return;
    }
    int flags = 0;
    long idx = list->HitTest(event.GetPosition(), flags);
    wxString path;
    if (flags & wxLIST_HITTEST_ONITEM)
    {
        const PluginElement* elem = (const PluginElement*)list->GetItemData(idx);
        if (elem)
            path = elem->fileName;
    }
    if (list->GetToolTip())
    {
        if (path.IsEmpty())
            list->UnsetToolTip();
        else if (path != list->GetToolTip()->GetTip())
            list->SetToolTip(path);
    }
    else if (!path.IsEmpty())
        list->SetToolTip(path);
}

void PluginsConfigurationDlg::OnUpdateUI(wxUpdateUIEvent& event)
{
    static long lastSelection = -2;
    static bool lastSelectionMultiple = false;
    event.Skip();

    wxListCtrl* list = XRCCTRL(*this, "lstPlugins", wxListCtrl);
    long sel = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    // no need to overdraw all the time
    if (sel == lastSelection && (lastSelectionMultiple && list->GetSelectedItemCount() > 1))
        return;
    lastSelection = sel;
    lastSelectionMultiple = list->GetSelectedItemCount() > 1;

    bool en = sel != -1;
    const PluginElement* elem = en ? (const PluginElement*)list->GetItemData(sel) : nullptr;
    bool hasPlugin = elem && elem->plugin;
    bool isAttached = hasPlugin && elem->plugin->IsAttached();

    XRCCTRL(*this, "btnEnable", wxButton)->Enable(en && (lastSelectionMultiple || (hasPlugin && !isAttached)));
    XRCCTRL(*this, "btnDisable", wxButton)->Enable(en && (lastSelectionMultiple || (hasPlugin && isAttached)));
    XRCCTRL(*this, "btnUninstall", wxButton)->Enable(en);
    XRCCTRL(*this, "btnExport", wxButton)->Enable(en);
}

void PluginsConfigurationDlg::EndModal(int retCode)
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("plugins"));

    cfg->Write(_T("/install_globally"), XRCCTRL(*this, "chkInstallGlobally", wxCheckBox)->GetValue());
    cfg->Write(_T("/install_confirmation"), XRCCTRL(*this, "chkInstallConfirmation", wxCheckBox)->GetValue());

    Manager::Get()->GetCCManager()->NotifyPluginStatus();

    wxScrollingDialog::EndModal(retCode);
}

void PluginsConfigurationDlg::OnLinkClicked(wxHtmlLinkEvent& event)
{
    wxLaunchDefaultBrowser(event.GetLinkInfo().GetHref());
}
