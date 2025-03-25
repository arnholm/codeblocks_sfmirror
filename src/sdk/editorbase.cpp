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
    #include <wx/filename.h>
    #include <wx/notebook.h>
    #include <wx/menu.h>
    #include "manager.h"
    #include "editorbase.h"
    #include "cbeditor.h"
    #include "editormanager.h"
    #include "pluginmanager.h"
    #include "cbproject.h" // FileTreeData
    #include "projectmanager.h" // ProjectsArray
    #include <wx/wfstream.h>
#endif

#include "cbauibook.h"

#include "cbstyledtextctrl.h"

// needed for initialization of variables
inline int editorbase_RegisterId(int id)
{
    wxRegisterId(id);
    return id;
}

struct EditorBaseInternalData
{
    EditorBaseInternalData(EditorBase* owner)
        : m_pOwner(owner),
        m_DisplayingPopupMenu(false),
        m_CloseMe(false)
    {}

    EditorBase* m_pOwner;
    bool m_DisplayingPopupMenu;
    bool m_CloseMe;
};

// The following lines reserve 255 consecutive id's
const int EditorMaxSwitchTo = 255;
const int idSwitchFile1 = wxNewId();
const int idSwitchFileMax = editorbase_RegisterId(idSwitchFile1 + EditorMaxSwitchTo -1);

const int idCloseMe = wxNewId();
const int idCloseAll = wxNewId();
const int idCloseAllOthers = wxNewId();
const int idSaveMe = wxNewId();
const int idSaveAll = wxNewId();
const int idSwitchTo = wxNewId();

// Search for... in the context menu, visible with CTRL + right click
const long idGoogle        = wxNewId();
const long idMsdn          = wxNewId();
const long idStackOverflow = wxNewId();
const long idCodeProject   = wxNewId();
const long idCPlusPlusCom  = wxNewId();

BEGIN_EVENT_TABLE(EditorBase, wxPanel)
    EVT_MENU_RANGE(idSwitchFile1, idSwitchFileMax, EditorBase::OnContextMenuEntry)
    EVT_MENU(idCloseMe,           EditorBase::OnContextMenuEntry)
    EVT_MENU(idCloseAll,          EditorBase::OnContextMenuEntry)
    EVT_MENU(idCloseAllOthers,    EditorBase::OnContextMenuEntry)
    EVT_MENU(idSaveMe,            EditorBase::OnContextMenuEntry)
    EVT_MENU(idSaveAll,           EditorBase::OnContextMenuEntry)

    EVT_MENU(idGoogle,            EditorBase::OnContextMenuEntry)
    EVT_MENU(idMsdn,              EditorBase::OnContextMenuEntry)
    EVT_MENU(idStackOverflow,     EditorBase::OnContextMenuEntry)
    EVT_MENU(idCodeProject,       EditorBase::OnContextMenuEntry)
    EVT_MENU(idCPlusPlusCom,      EditorBase::OnContextMenuEntry)
END_EVENT_TABLE()

void EditorBase::InitFilename(const wxString& filename)
{
    if (filename.IsEmpty())
        m_Filename = realpath(CreateUniqueFilename());
    else
        m_Filename = realpath(filename);

    wxFileName fname;
    fname.Assign(m_Filename);
    m_Shortname = fname.GetFullName();
}

wxString EditorBase::CreateUniqueFilename()
{
    const wxString prefix = _("Untitled");
    const wxString path = wxGetCwd() + wxFILE_SEP_PATH;
    wxString tmp;
    int iter = 0;
    while (true)
    {
        tmp.Clear();
        tmp << path << prefix << wxString::Format(_T("%d"), iter);
        if (!Manager::Get()->GetEditorManager()->GetEditor(tmp) &&
                !wxFileExists(path + tmp))
        {
            return tmp;
        }
        ++iter;
    }
}

EditorBase::EditorBase(wxWindow* parent, const wxString& filename, bool addCustomEditor)
        : wxPanel(parent, -1),
        m_IsBuiltinEditor(false),
        m_WinTitle(filename)
{
    m_pData = new EditorBaseInternalData(this);

    if (addCustomEditor)
        Manager::Get()->GetEditorManager()->AddCustomEditor(this);
    InitFilename(filename);
    SetTitle(m_Shortname);
}

EditorBase::~EditorBase()
{
    if (!Manager::Get()->IsAppShuttingDown())
    {
        Manager::Get()->GetEditorManager()->RemoveCustomEditor(this);

        CodeBlocksEvent event(cbEVT_EDITOR_CLOSE);
        event.SetEditor(this);
        event.SetString(m_Filename);

        Manager::Get()->GetPluginManager()->NotifyPlugins(event);
    }
    delete m_pData;
}

const wxString& EditorBase::GetTitle() const
{
    return m_WinTitle;
}

void EditorBase::SetTitle(const wxString& newTitle)
{
    m_WinTitle = newTitle;
    int mypage = Manager::Get()->GetEditorManager()->FindPageFromEditor(this);
    if (mypage != -1)
        Manager::Get()->GetEditorManager()->GetNotebook()->SetPageText(mypage, newTitle);

    // set full filename (including path) as tooltip,
    // if possible add the appropriate project also
    wxString toolTip = GetFilename();
    wxFileName fname(realpath(toolTip));
    NormalizePath(fname, wxEmptyString);
    toolTip = UnixFilename(fname.GetFullPath());

    cbProject* prj = Manager::Get()->GetProjectManager()->FindProjectForFile(toolTip, nullptr, false, true);
    if (prj)
        toolTip += _("\nProject: ") + prj->GetTitle();
    cbAuiNotebook* nb = Manager::Get()->GetEditorManager()->GetNotebook();
    if (nb)
    {
        const int idx = nb->GetPageIndex(this);
        if (idx != wxNOT_FOUND)
        {
            nb->SetPageToolTip(idx, toolTip);
            Manager::Get()->GetEditorManager()->MarkReadOnly(idx, IsReadOnly() || (fname.FileExists() && !wxFile::Access(fname.GetFullPath(), wxFile::write)));
        }
    }
}

void EditorBase::Activate()
{
    Manager::Get()->GetEditorManager()->SetActiveEditor(this);
}

bool EditorBase::QueryClose()
{
    if ( GetModified() )
    {
        wxString msg;
        msg.Printf(_("File %s is modified...\nDo you want to save the changes?"), GetFilename().c_str());
        switch (cbMessageBox(msg, _("Save file"), wxICON_QUESTION | wxYES_NO | wxCANCEL))
        {
            case wxID_YES:
                if (!Save())
                    return false;
                break;
            case wxID_NO:
                break;
            case wxID_CANCEL:
            default:
                return false;
        }
        SetModified(false);
    }
    return true;
}

bool EditorBase::Close()
{
    Destroy();
    return true;
}

bool EditorBase::IsBuiltinEditor() const
{
    return m_IsBuiltinEditor;
}

bool EditorBase::ThereAreOthers() const
{
    return (Manager::Get()->GetEditorManager()->GetEditorsCount() > 1);
}

wxMenu* EditorBase::CreateContextSubMenu(long id) // For context menus
{
    wxMenu* menu = nullptr;

    if (id == idSwitchTo)
    {
        menu = new wxMenu;
        m_SwitchTo.clear();
        for (int i = 0; i < EditorMaxSwitchTo && i < Manager::Get()->GetEditorManager()->GetEditorsCount(); ++i)
        {
            EditorBase* other = Manager::Get()->GetEditorManager()->GetEditor(i);
            if (!other || other == this)
                continue;
            id = idSwitchFile1+i;
            m_SwitchTo[id] = other;
            menu->Append(id, (other->GetModified() ? wxT("*") : wxEmptyString) + other->GetShortName());
        }
        if (!menu->GetMenuItemCount())
        {
            delete menu;
            menu = nullptr;
        }
    }
    return menu;
}

void EditorBase::BasicAddToContextMenu(wxMenu* popup, ModuleType type)
{
    if (type == mtOpenFilesList)
    {
      popup->Append(idCloseMe, _("Close"));
      popup->Append(idCloseAll, _("Close all"));
      popup->Append(idCloseAllOthers, _("Close all others"));
      popup->AppendSeparator();
      popup->Append(idSaveMe, _("Save"));
      popup->Append(idSaveAll, _("Save all"));
      popup->AppendSeparator();
      // enable/disable some items, based on state
      popup->Enable(idSaveMe, GetModified());

      bool hasOthers = ThereAreOthers();
      popup->Enable(idCloseAll, hasOthers);
      popup->Enable(idCloseAllOthers, hasOthers);
    }
    if (type != mtEditorManager) // no editor
    {
        wxMenu* switchto = CreateContextSubMenu(idSwitchTo);
        if (switchto)
            popup->Append(idSwitchTo, _("Switch to"), switchto);
    }
}

void EditorBase::DisplayContextMenu(const wxPoint& position, ModuleType type, wxWindow *menuParent)
{
    bool noeditor = (type != mtEditorManager);
    // noeditor:
    // True if context menu belongs to open files tree;
    // False if belongs to cbEditor

    // inform the editors we 're just about to create a context menu
    if (!OnBeforeBuildContextMenu(position, type))
        return;

    wxMenu* popup = new wxMenu;

    PluginManager *pluginManager = Manager::Get()->GetPluginManager();
    pluginManager->ResetModuleMenu();

    if (!noeditor && wxGetKeyState(WXK_CONTROL))
    {
        cbStyledTextCtrl* control = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor()->GetControl();
        wxString text = control->GetSelectedText();
        if (text.IsEmpty())
        {
            const int pos = control->GetCurrentPos();
            text = control->GetTextRange(control->WordStartPosition(pos, true), control->WordEndPosition(pos, true));
        }

        if (!text.IsEmpty())
        {
            popup->Append(idGoogle,        wxString::Format(_("Search the Internet for \"%s\""),  text));
            popup->Append(idMsdn,          wxString::Format(_("Search MSDN for \"%s\""),          text));
            popup->Append(idStackOverflow, wxString::Format(_("Search StackOverflow for \"%s\""), text));
            popup->Append(idCodeProject,   wxString::Format(_("Search CodeProject for \"%s\""),   text));
            popup->Append(idCPlusPlusCom,  wxString::Format(_("Search CplusPlus.com for \"%s\""), text));
        }
        lastWord = text;

        wxMenu* switchto = CreateContextSubMenu(idSwitchTo);
        if (switchto)
        {
            if (popup->GetMenuItemCount() > 0)
                popup->AppendSeparator();
            popup->Append(idSwitchTo, _("Switch to"), switchto);
        }
    }
    else if (!noeditor && wxGetKeyState(WXK_ALT))
    { // run a script
    }
    else
    {
        // Basic functions
        BasicAddToContextMenu(popup, type);

        // Extended functions, part 1 (virtual)
        AddToContextMenu(popup, type, false);

        // ask other editors / plugins if they need to add any entries in this menu...
        FileTreeData* ftd = new FileTreeData(nullptr, FileTreeData::ftdkUndefined);
        ftd->SetFolder(m_Filename);
        pluginManager->AskPluginsForModuleMenu(type, popup, ftd);
        delete ftd;

        popup->AppendSeparator();
        // Extended functions, part 2 (virtual)
        AddToContextMenu(popup, type, true);
    }

    // Check if the last item is a separator and remove it.
    const wxMenuItemList &popupItems = popup->GetMenuItems();
    if (popupItems.GetCount() > 0)
    {
        wxMenuItem *last = popupItems[popupItems.GetCount() - 1];
        if (last && last->IsSeparator())
        {
            wxMenuItem *removed = popup->Remove(last);
            delete removed;
        }
    }

    // Insert a separator at the end of the "Find XXX" menu group of items.
    const int lastFind = pluginManager->GetFindMenuItemFirst() + pluginManager->GetFindMenuItemCount();
    if (lastFind > 0)
        popup->Insert(lastFind, wxID_SEPARATOR, wxEmptyString);

    // inform the editors we 're done creating a context menu (just about to show it)
    OnAfterBuildContextMenu(type);

    // display menu
    wxPoint clientpos;
    if (position==wxDefaultPosition) // "context menu" key
    {
        // obtain the caret point (on the screen) as we assume
        // that the user wants to work with the keyboard
        cbStyledTextCtrl* const control = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor()->GetControl();
        clientpos = control->PointFromPosition(control->GetCurrentPos());
    }
    else
    {
        if (menuParent)
            clientpos = menuParent->ScreenToClient(position);
        else
            clientpos = ScreenToClient(position);
    }

    m_pData->m_DisplayingPopupMenu = true;
    if (menuParent)
        menuParent->PopupMenu(popup, clientpos);
    else
        PopupMenu(popup, clientpos);
    delete popup;
    m_pData->m_DisplayingPopupMenu = false;

    // this code *must* be the last code executed by this function
    // because it *will* invalidate 'this'
    if (m_pData->m_CloseMe)
        Manager::Get()->GetEditorManager()->Close(this);
}

void EditorBase::OnContextMenuEntry(wxCommandEvent& event)
{
    // we have a single event handler for all popup menu entries
    // This was ported from cbEditor and used for the basic operations:
    // Switch to, close, save, etc.

    const int id = event.GetId();
    m_pData->m_CloseMe = false;

    if (id == idCloseMe)
    {
        if (m_pData->m_DisplayingPopupMenu)
            m_pData->m_CloseMe = true; // defer delete 'this' until after PopupMenu() call returns
        else
            Manager::Get()->GetEditorManager()->Close(this);
    }
    else if (id == idCloseAll)
    {
        if (m_pData->m_DisplayingPopupMenu)
        {
            Manager::Get()->GetEditorManager()->CloseAllInTabCtrlExcept(this);
            m_pData->m_CloseMe = true; // defer delete 'this' until after PopupMenu() call returns
        }
        else
            Manager::Get()->GetEditorManager()->CloseAllInTabCtrl();
    }
    else if (id == idCloseAllOthers)
    {
        Manager::Get()->GetEditorManager()->CloseAllInTabCtrlExcept(this);
    }
    else if (id == idSaveMe)
    {
        Save();
    }
    else if (id == idSaveAll)
    {
        Manager::Get()->GetEditorManager()->SaveAll();
    }
    else if (id >= idSwitchFile1 && id <= idSwitchFileMax)
    {
        // "Switch to..." item
        EditorBase* const ed = m_SwitchTo[id];
        if (ed)
            Manager::Get()->GetEditorManager()->SetActiveEditor(ed);

        m_SwitchTo.clear();
    }
    else
    {
        if      (id == idGoogle)
            wxLaunchDefaultBrowser(wxString(_T("http://www.google.com/search?q="))                       << URLEncode(lastWord));
        else if (id == idMsdn)
            wxLaunchDefaultBrowser(wxString(_T("http://social.msdn.microsoft.com/Search/en-US/?query=")) << URLEncode(lastWord) << _T("&ac=8"));
        else if (id == idStackOverflow)
            wxLaunchDefaultBrowser(wxString(_T("http://stackoverflow.com/search?q="))                    << URLEncode(lastWord));
        else if (id == idCodeProject)
            wxLaunchDefaultBrowser(wxString(_T("http://www.codeproject.com/search.aspx?q="))             << URLEncode(lastWord));
        else if (id == idCPlusPlusCom)
            wxLaunchDefaultBrowser(wxString(_T("http://www.cplusplus.com/search.do?q="))                 << URLEncode(lastWord));
    }
}

bool EditorBase::IsContextMenuOpened() const
{
    return m_pData->m_DisplayingPopupMenu;
}
