#include "FileExplorer.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/aui/aui.h>

#include <sdk.h>
#ifndef CB_PRECOMP
    #include <wx/dnd.h>
    #include <wx/imaglist.h>

    #include <cbproject.h>
    #include <configmanager.h>
    #include <projectmanager.h>
#endif

#include <list>
#include <vector>
#include <iostream>

#include "se_globals.h"
#include "CommitBrowser.h"

#include <wx/arrimpl.cpp> // this is a magic incantation which must be done!
WX_DEFINE_OBJARRAY(VCSstatearray);

int ID_UPDATETIMER=wxNewId();
int ID_FILETREE=wxNewId();
int ID_FILELOC=wxNewId();
int ID_FILEWILD=wxNewId();
int ID_SETLOC=wxNewId();
int ID_VCSCONTROL=wxNewId();
int ID_VCSTYPE=wxNewId();
int ID_VCSCHANGESCHECK = wxNewId();

int ID_OPENINED=wxNewId();
int ID_FILENEWFILE=wxNewId();
int ID_FILENEWFOLDER=wxNewId();
int ID_FILEMAKEFAV=wxNewId();
int ID_FILECOPY=wxNewId();
int ID_FILEDUP=wxNewId();
int ID_FILEMOVE=wxNewId();
int ID_FILEDELETE=wxNewId();
int ID_FILERENAME=wxNewId();
int ID_FILEEXPANDALL=wxNewId();
int ID_FILECOLLAPSEALL=wxNewId();
int ID_FILESETTINGS=wxNewId();
int ID_FILESHOWHIDDEN=wxNewId();
int ID_FILEPARSECVS=wxNewId();
int ID_FILEPARSESVN=wxNewId();
int ID_FILEPARSEHG=wxNewId();
int ID_FILEPARSEBZR=wxNewId();
int ID_FILEPARSEGIT=wxNewId();
int ID_FILE_UPBUTTON=wxNewId();
int ID_FILEREFRESH=wxNewId();
int ID_FILEADDTOPROJECT=wxNewId();

int ID_FILEDIFF=wxNewId();
// 10 additional ID's reserved for FILE DIFF items
int ID_FILEDIFF1=wxNewId();
int ID_FILEDIFF2=wxNewId();
int ID_FILEDIFF3=wxNewId();
int ID_FILEDIFF4=wxNewId();
int ID_FILEDIFF5=wxNewId();
int ID_FILEDIFF6=wxNewId();
int ID_FILEDIFF7=wxNewId();
int ID_FILEDIFF8=wxNewId();
int ID_FILEDIFF9=wxNewId();
int ID_FILEDIFF10=wxNewId();
//


class UpdateQueue
{
public:
    void Add(const wxTreeItemId &ti)
    {
        for(std::list<wxTreeItemId>::iterator it=qdata.begin();it!=qdata.end();it++)
        {
            if(*it==ti)
            {
                qdata.erase(it);
                break;
            }
        }
        qdata.push_front(ti);
    }
    bool Pop(wxTreeItemId &ti)
    {
        if(qdata.empty())
            return false;
        ti=qdata.front();
        qdata.pop_front();
        return true;
    }
    void Clear()
    {
        qdata.clear();
    }
private:
    std::list<wxTreeItemId> qdata;
};


class DirTraverseFind : public wxDirTraverser
{
public:
    DirTraverseFind(const wxString& wildcard) : m_files(), m_wildcard(wildcard) { }
    virtual wxDirTraverseResult OnFile(const wxString& filename)
    {
        if(WildCardListMatch(m_wildcard,filename,true))
            m_files.Add(filename);
        return wxDIR_CONTINUE;
    }
    virtual wxDirTraverseResult OnDir(const wxString& dirname)
    {
        if(WildCardListMatch(m_wildcard,dirname,true))
            m_files.Add(dirname);
        return wxDIR_CONTINUE;
    }
    wxArrayString& GetMatches() {return m_files;}
private:
    wxArrayString m_files;
    wxString m_wildcard;
};


class FEDataObject:public wxDataObjectComposite
{
public:
   FEDataObject():wxDataObjectComposite()
   {
       m_file=new wxFileDataObject;
       Add(m_file,true);
   }
   wxFileDataObject *m_file;

};


class wxFEDropTarget: public wxDropTarget
{
public:
    wxFEDropTarget(FileExplorer *fe):wxDropTarget()
    {
        m_fe=fe;
        m_data_object=new FEDataObject();
        SetDataObject(m_data_object);
    }
    virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def)
    {
        GetData();
        if(m_data_object->GetReceivedFormat().GetType()==wxDF_FILENAME )
        {
            wxArrayString as=m_data_object->m_file->GetFilenames();
            wxTreeCtrl *tree=m_fe->m_Tree;
            int flags;
            wxTreeItemId id=tree->HitTest(wxPoint(x,y),flags);
            if(!id.IsOk())
                return wxDragCancel;
            if(tree->GetItemImage(id)!=fvsFolder)
                return wxDragCancel;
            if(!(flags&(wxTREE_HITTEST_ONITEMICON|wxTREE_HITTEST_ONITEMLABEL)))
                return wxDragCancel;
            if(def==wxDragCopy)
            {
                m_fe->CopyFiles(m_fe->GetFullPath(id),as);
                return def;
            }
            if(def==wxDragMove)
            {
                m_fe->MoveFiles(m_fe->GetFullPath(id),as);
                return def;
            }
            return wxDragCancel;
        }
//            if(sizeof(wxFileDataObject)!=m_data_object->GetDataSize(wxDF_FILENAME))
//            {
//                wxMessageBox(wxString::Format(_("Drop files %i,%i"),sizeof(wxFileDataObject),m_data_object->GetDataSize(wxDF_FILENAME)));
//                return wxDragCancel;
//            }
        return wxDragCancel;
    }
    virtual bool OnDrop(wxCoord /*x*/, wxCoord /*y*/, int /*tab*/, wxWindow */*wnd*/)
    {
        return true;
    }
    virtual wxDragResult OnDragOver(wxCoord /*x*/, wxCoord /*y*/, wxDragResult def)
    {
        return def;
    }
private:
    FEDataObject *m_data_object;
    FileExplorer *m_fe;
};



BEGIN_EVENT_TABLE(FileTreeCtrl, wxTreeCtrl)
//    EVT_TREE_ITEM_ACTIVATED(ID_FILETREE, FileTreeCtrl::OnActivate)  //double click -
    EVT_KEY_DOWN(FileTreeCtrl::OnKeyDown)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(FileTreeCtrl, wxTreeCtrl)

FileTreeCtrl::FileTreeCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos,
    const wxSize& size, long style,
    const wxValidator& validator,
    const wxString& name)
    : wxTreeCtrl(parent,id,pos,size,style,validator,name) {}

FileTreeCtrl::FileTreeCtrl() { }

FileTreeCtrl::FileTreeCtrl(wxWindow *parent): wxTreeCtrl(parent) {}

FileTreeCtrl::~FileTreeCtrl()
{
}

void FileTreeCtrl::OnKeyDown(wxKeyEvent &event)
{
    if(event.GetKeyCode()==WXK_DELETE)
        ::wxPostEvent(GetParent(),event);
    else
        event.Skip(true);
}

int FileTreeCtrl::OnCompareItems(const wxTreeItemId& item1, const wxTreeItemId& item2)
{
    if((GetItemImage(item1)==fvsFolder)>(GetItemImage(item2)==fvsFolder))
        return -1;
    if((GetItemImage(item1)==fvsFolder)<(GetItemImage(item2)==fvsFolder))
        return 1;
    if((GetItemImage(item1)==fvsVcNonControlled)<(GetItemImage(item2)==fvsVcNonControlled))
        return -1;
    if((GetItemImage(item1)==fvsVcNonControlled)<(GetItemImage(item2)==fvsVcNonControlled))
        return 1;
    return (GetItemText(item1).CmpNoCase(GetItemText(item2)));
}

BEGIN_EVENT_TABLE(FileExplorer, wxPanel)
    EVT_TIMER(ID_UPDATETIMER, FileExplorer::OnTimerCheckUpdates)
    EVT_FSWATCHER(wxID_ANY, FileExplorer::OnFsWatcher)
    EVT_IDLE(FileExplorer::OnIdle)
    EVT_COMMAND(0, wxEVT_NOTIFY_UPDATE_COMPLETE, FileExplorer::OnUpdateTreeItems)
    EVT_COMMAND(0, wxEVT_NOTIFY_LOADER_UPDATE_COMPLETE, FileExplorer::OnVCSFileLoaderComplete)
//    EVT_COMMAND(0, wxEVT_NOTIFY_EXEC_REQUEST, FileExplorer::OnExecRequest)
    EVT_TREE_BEGIN_DRAG(ID_FILETREE, FileExplorer::OnBeginDragTreeItem)
    EVT_TREE_END_DRAG(ID_FILETREE, FileExplorer::OnEndDragTreeItem)
    EVT_BUTTON(ID_FILE_UPBUTTON, FileExplorer::OnUpButton)
    EVT_MENU(ID_SETLOC, FileExplorer::OnSetLoc)
    EVT_MENU(ID_OPENINED, FileExplorer::OnOpenInEditor)
    EVT_MENU(ID_FILENEWFILE, FileExplorer::OnNewFile)
    EVT_MENU(ID_FILENEWFOLDER,FileExplorer::OnNewFolder)
    EVT_MENU(ID_FILEMAKEFAV,FileExplorer::OnAddFavorite)
    EVT_MENU(ID_FILECOPY,FileExplorer::OnCopy)
    EVT_MENU(ID_FILEDUP,FileExplorer::OnDuplicate)
    EVT_MENU(ID_FILEMOVE,FileExplorer::OnMove)
    EVT_MENU(ID_FILEDELETE,FileExplorer::OnDelete)
    EVT_MENU(ID_FILERENAME,FileExplorer::OnRename)
    EVT_MENU(ID_FILEEXPANDALL,FileExplorer::OnExpandAll)
    EVT_MENU(ID_FILECOLLAPSEALL,FileExplorer::OnCollapseAll)
    EVT_MENU(ID_FILESETTINGS,FileExplorer::OnSettings)
    EVT_MENU(ID_FILESHOWHIDDEN,FileExplorer::OnShowHidden)
    EVT_MENU(ID_FILEPARSECVS,FileExplorer::OnParseCVS)
    EVT_MENU(ID_FILEPARSESVN,FileExplorer::OnParseSVN)
    EVT_MENU(ID_FILEPARSEHG,FileExplorer::OnParseHG)
    EVT_MENU(ID_FILEPARSEBZR,FileExplorer::OnParseBZR)
    EVT_MENU(ID_FILEPARSEGIT,FileExplorer::OnParseGIT)
    EVT_MENU(ID_FILEREFRESH,FileExplorer::OnRefresh)
    EVT_MENU(ID_FILEADDTOPROJECT,FileExplorer::OnAddToProject)
    EVT_MENU_RANGE(ID_FILEDIFF, ID_FILEDIFF+10, FileExplorer::OnVCSDiff)
    EVT_KEY_DOWN(FileExplorer::OnKeyDown)
    EVT_TREE_ITEM_EXPANDING(ID_FILETREE, FileExplorer::OnExpand)
    //EVT_TREE_ITEM_COLLAPSED(id, func) //delete the children
    EVT_TREE_ITEM_ACTIVATED(ID_FILETREE, FileExplorer::OnActivate)  //double click - open file / expand folder (the latter is a default just need event.skip)
    EVT_TREE_ITEM_MENU(ID_FILETREE, FileExplorer::OnRightClick) //right click open context menu -- interpreter actions, rename, delete, copy, properties, set as root etc
    EVT_COMBOBOX(ID_FILELOC, FileExplorer::OnChooseLoc) //location selected from history of combo box - set as root
    EVT_COMBOBOX(ID_FILEWILD, FileExplorer::OnChooseWild) //location selected from history of combo box - set as root
    //EVT_TEXT(ID_FILELOC, FileExplorer::OnLocChanging) //provide autotext hint for dir name in combo box
    EVT_TEXT_ENTER(ID_FILELOC, FileExplorer::OnEnterLoc) //location entered in combo box - set as root
    EVT_TEXT_ENTER(ID_FILEWILD, FileExplorer::OnEnterWild) //location entered in combo box - set as root  ** BUG RIDDEN
    EVT_CHOICE(ID_VCSCONTROL, FileExplorer::OnVCSControl)
    EVT_CHECKBOX(ID_VCSCHANGESCHECK, FileExplorer::OnVCSChangesCheck)
END_EVENT_TABLE()

FileExplorer::FileExplorer(wxWindow *parent,wxWindowID id,
    const wxPoint& pos, const wxSize& size,
    long style, const wxString& name):
    wxPanel(parent,id,pos,size,style, name)
{
    m_kill=false;
    m_update_queue=new UpdateQueue;
    m_updater=nullptr;
    m_updatetimer=new wxTimer(this,ID_UPDATETIMER);
    m_update_active=false;
    m_updater_cancel=false;
    m_update_expand=false;
    m_fs_watcher=nullptr;
    m_droptarget=new wxFEDropTarget(this);

    m_show_hidden=false;
    m_parse_cvs=false;
    m_parse_hg=false;
    m_parse_bzr=false;
    m_parse_git=false;
    m_parse_svn=false;
    m_vcs_file_loader=nullptr;
    wxBoxSizer* bs = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* bsh = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* bshloc = new wxBoxSizer(wxHORIZONTAL);
    m_Box_VCS_Control = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *box_vcs_top = new wxBoxSizer(wxHORIZONTAL);
    m_Tree = new FileTreeCtrl(this, ID_FILETREE);
    m_Tree->SetIndent(m_Tree->GetIndent()/2);
    m_Tree->SetDropTarget(m_droptarget);
    m_Loc = new wxComboBox(this,ID_FILELOC,"",wxDefaultPosition,wxDefaultSize,0,NULL,wxTE_PROCESS_ENTER|wxCB_DROPDOWN);
    m_WildCards = new wxComboBox(this,ID_FILEWILD,"",wxDefaultPosition,wxDefaultSize,0,NULL,wxTE_PROCESS_ENTER|wxCB_DROPDOWN);
    m_UpButton = new wxButton(this,ID_FILE_UPBUTTON,"^",wxDefaultPosition,wxDefaultSize,wxBU_EXACTFIT);
    bshloc->Add(m_Loc, 1, wxEXPAND);
    bshloc->Add(m_UpButton, 0, wxEXPAND);
    bs->Add(bshloc, 0, wxEXPAND);
    bsh->Add(new wxStaticText(this,wxID_ANY,_("Mask: ")),0,wxALIGN_CENTRE);
    bsh->Add(m_WildCards,1);
    bs->Add(bsh, 0, wxEXPAND);

    m_VCS_Control = new wxChoice(this,ID_VCSCONTROL);
    m_VCS_Type = new wxStaticText(this,ID_VCSTYPE,"");
    m_VCS_ChangesOnly = new wxCheckBox(this, ID_VCSCHANGESCHECK, _("Show changed files only"));
    box_vcs_top->Add(m_VCS_Type,0,wxALIGN_CENTER);
    box_vcs_top->Add(m_VCS_Control,1,wxEXPAND);
    m_Box_VCS_Control->Add(box_vcs_top, 0, wxEXPAND);
    m_Box_VCS_Control->Add(m_VCS_ChangesOnly, 0, wxEXPAND);
    m_Box_VCS_Control->Hide(true);
    bs->Add(m_Box_VCS_Control, 0, wxEXPAND);

    bs->Add(m_Tree, 1, wxEXPAND | wxALL);

    SetAutoLayout(TRUE);

    m_TreeImages = cbProjectTreeImages::MakeImageList(16, *this);
    m_Tree->SetImageList(m_TreeImages.get());

    ReadConfig();
    if(m_Loc->GetCount()>m_favdirs.GetCount())
    {
        m_Loc->Select(m_favdirs.GetCount());
        m_root=m_Loc->GetString(m_favdirs.GetCount());
    } else
    {
        m_root=wxFileName::GetPathSeparator();
        m_Loc->Append(m_root);
        m_Loc->Select(0);
    }
    if(m_WildCards->GetCount()>0)
        m_WildCards->Select(0);
    SetRootFolder(m_root);

    SetSizer(bs);
}

FileExplorer::~FileExplorer()
{
    m_kill=true;
    m_updatetimer->Stop();
    delete m_fs_watcher;
    WriteConfig();
    UpdateAbort();
    delete m_update_queue;
    delete m_updatetimer;
}


bool FileExplorer::SetRootFolder(wxString root)
{
    UpdateAbort();
    if(root[root.Len()-1]!=wxFileName::GetPathSeparator())
        root=root+wxFileName::GetPathSeparator();
#ifdef __WXMSW__
    wxFileName fnroot=wxFileName(root);
    if(fnroot.GetVolume().empty())
    {
        fnroot.SetVolume(wxFileName(::wxGetCwd()).GetVolume());
        root=fnroot.GetFullPath();//(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR)+fnroot.GetFullName();
    }
#endif
    wxDir dir(root);
    if (!dir.IsOpened())
    {
        // deal with the error here - wxDir would already log an error message
        // explaining the exact reason of the failure
        m_Loc->SetValue(m_root);
        return false;
    }
    m_root=root;
    m_VCS_Control->Clear();
    m_commit = wxEmptyString;
    m_VCS_Type->SetLabel(wxEmptyString);
    m_Box_VCS_Control->Hide(true);
    m_Loc->SetValue(m_root);
    m_Tree->DeleteAllItems();
    m_Tree->AddRoot(m_root,fvsFolder);
    m_Tree->SetItemHasChildren(m_Tree->GetRootItem());
    m_Tree->Expand(m_Tree->GetRootItem());
    Layout();

    return true;
//    return AddTreeItems(m_Tree->GetRootItem());

}

// find a file in the filesystem below a selected root
void FileExplorer::FindFile(const wxString &findfilename, const wxTreeItemId &ti)
{
    wxString path=GetFullPath(ti);

    wxDir dir(path);

    if (!dir.IsOpened())
    {
        // deal with the error here - wxDir would already log an error message
        // explaining the exact reason for the failure
        return;
    }
    int flags=wxDIR_FILES|wxDIR_DIRS;
    if(m_show_hidden)
        flags|=wxDIR_HIDDEN;

    DirTraverseFind dtf(findfilename);
    m_findmatchcount=dir.Traverse(dtf,wxEmptyString,flags);
    m_findmatch=dtf.GetMatches();
}

// focus the item in the tree.
void FileExplorer::FocusFile(const wxTreeItemId &ti)
{
    m_Tree->SetFocus();
    m_Tree->UnselectAll();
    m_Tree->SelectItem(ti);
    m_Tree->EnsureVisible(ti);
}

wxTreeItemId FileExplorer::GetNextExpandedNode(wxTreeItemId ti)
{
    wxTreeItemId next_ti;
    if(!ti.IsOk())
    {
        return m_Tree->GetRootItem();
    }
    if(m_Tree->IsExpanded(ti))
    {
        wxTreeItemIdValue cookie;
        next_ti=m_Tree->GetFirstChild(ti,cookie);
        while(next_ti.IsOk())
        {
            if(m_Tree->IsExpanded(next_ti))
                return next_ti;
            next_ti=m_Tree->GetNextChild(ti,cookie);
        }
    }
    next_ti=m_Tree->GetNextSibling(ti);
    while(next_ti.IsOk())
    {
        if(m_Tree->IsExpanded(next_ti))
            return next_ti;
        next_ti=m_Tree->GetNextSibling(next_ti);
    }
    return m_Tree->GetRootItem();
}

bool FileExplorer::GetItemFromPath(const wxString &path, wxTreeItemId &ti)
{
    ti=m_Tree->GetRootItem();
    do
    {
        if(path==GetFullPath(ti))
            return true;
        ti=GetNextExpandedNode(ti);
    } while(ti!=m_Tree->GetRootItem());
    return false;
}


void FileExplorer::GetExpandedNodes(wxTreeItemId ti, Expansion *exp)
{
    exp->name=m_Tree->GetItemText(ti);
    wxTreeItemIdValue cookie;
    wxTreeItemId ch=m_Tree->GetFirstChild(ti,cookie);
    while(ch.IsOk())
    {
        if(m_Tree->IsExpanded(ch))
        {
            Expansion *e=new Expansion();
            GetExpandedNodes(ch,e);
            exp->children.push_back(e);
        }
        ch=m_Tree->GetNextChild(ti,cookie);
    }
}

void FileExplorer::GetExpandedPaths(wxTreeItemId ti,wxArrayString &paths)
{
    if(!ti.IsOk())
    {
        wxMessageBox(_("node error"));
        return;
    }
    if(m_Tree->IsExpanded(ti))
        paths.Add(GetFullPath(ti));
    wxTreeItemIdValue cookie;
    wxTreeItemId ch=m_Tree->GetFirstChild(ti,cookie);
    while(ch.IsOk())
    {
        if(m_Tree->IsExpanded(ch))
            GetExpandedPaths(ch,paths);
        ch=m_Tree->GetNextChild(ti,cookie);
    }
}

void FileExplorer::RefreshExpanded(wxTreeItemId ti)
{
    if(m_Tree->IsExpanded(ti))
        m_update_queue->Add(ti);
    wxTreeItemIdValue cookie;
    wxTreeItemId ch=m_Tree->GetFirstChild(ti,cookie);
    while(ch.IsOk())
    {
        if(m_Tree->IsExpanded(ch))
            RefreshExpanded(ch);
        ch=m_Tree->GetNextChild(ti,cookie);
    }
    m_updatetimer->Start(10,true);

}

void FileExplorer::Refresh(wxTreeItemId ti)
{
    //    Expansion e;
    //    GetExpandedNodes(ti,&e);
    //    RecursiveRebuild(ti,&e);
    //m_updating_node=ti;//m_Tree->GetRootItem();
    m_update_queue->Add(ti);
    m_updatetimer->Start(10,true);
}

void FileExplorer::UpdateAbort()
{
    if(!m_update_active)
        return;
    delete m_updater;
    m_update_active=false;
    m_updatetimer->Stop();
}

void FileExplorer::ResetFsWatcher()
{
    if (!m_fs_watcher)
        return;

    m_fs_watcher->RemoveAll();

    wxArrayString paths;
    GetExpandedPaths(m_Tree->GetRootItem(), paths);
    for (auto path : paths)
    {
        //LogMessage("add path to watcher: " + path);
        wxFileName fname(path);
        fname.DontFollowLink();
        m_fs_watcher->Add(fname, wxFSW_EVENT_CREATE | wxFSW_EVENT_DELETE | wxFSW_EVENT_RENAME | wxFSW_EVENT_MODIFY);
    }
}

void FileExplorer::OnFsWatcher(wxFileSystemWatcherEvent &e)
{
    if(m_kill || !m_fs_watcher)
        return;

    wxFileName fname = e.GetPath();
    wxString fullPath = fname.GetFullPath();
    OnFsWatcher(fullPath);

    if (e.GetNewPath().IsOk())
    {
        wxString newFullPath = e.GetNewPath().GetFullPath();
        if (!newFullPath.IsSameAs(fullPath))
            OnFsWatcher(newFullPath);
    }
}

void FileExplorer::OnFsWatcher(const wxString &fullPath)
{
    wxTreeItemId ti;
    LogMessage(wxString("fsWatcher: notified about path: ") + fullPath);

    if (GetItemFromPath(fullPath, ti))
    {
        m_update_queue->Add(ti);
        m_updatetimer->Start(100,true);
    }
}

void FileExplorer::OnIdle(wxIdleEvent &e)
{
    if (m_fs_watcher)
        return;
    // create the file system watcher here, because it needs an active loop
    m_fs_watcher = new wxFileSystemWatcher();
    m_fs_watcher->SetOwner(this);
}

void FileExplorer::OnTimerCheckUpdates(wxTimerEvent &/*e*/)
{
    if(m_kill)
        return;
    if(m_update_active)
        return;
    wxTreeItemId ti;
    while(m_update_queue->Pop(ti))
    {
        if(!ti.IsOk())
            continue;
        m_updater_cancel=false;
        m_updater=new FileExplorerUpdater(this);
        m_updated_node=ti;
        m_update_active=true;
        m_updater->Update(m_updated_node);
        break;
    }
}

bool FileExplorer::ValidateRoot()
{
    wxTreeItemId ti = m_Tree->GetRootItem();
    if (!ti.IsOk())
        return false;

    if (m_Tree->GetItemImage(ti) != fvsFolder)
        return false;

    return wxFileName::DirExists(GetFullPath(ti));
}

void FileExplorer::OnUpdateTreeItems(wxCommandEvent &/*e*/)
{
    if(m_kill)
        return;
    m_updater->Wait();
    wxTreeItemId ti=m_updated_node;
    const bool viewing_commit = (!m_updater->m_vcs_commit_string.empty() &&
                                ( m_updater->m_vcs_commit_string != _("Working copy")));
    if (ti == m_Tree->GetRootItem() && !viewing_commit)
    {
        m_VCS_Type->SetLabel(m_updater->m_vcs_type);
        if (m_updater->m_vcs_type.empty())
        {
            m_VCS_Control->Clear();
            m_Box_VCS_Control->Hide(true);
            m_commit = "";
        }
        else if (m_commit.empty())
        {
            m_VCS_Control->Clear();
            m_VCS_Control->Append(_("Working copy"));
            m_VCS_Control->Append(_("Select commit..."));
            m_VCS_Control->SetSelection(0);
            m_commit = _("Working copy");
            m_Box_VCS_Control->Show(true);
        }
        Layout();
    }
    if(m_updater_cancel || !ti.IsOk())
    { //NODE WAS DELETED - REFRESH NOW!
        //TODO: Should only need to clean up and restart the timer (no need to change queue)
        delete m_updater;
        m_updater=nullptr;
        m_update_active=false;
        ResetFsWatcher();
        if(ValidateRoot())
        {
            m_update_queue->Add(m_Tree->GetRootItem());
            m_updatetimer->Start(10,true);
        }
        return;
    }
//    cbMessageBox(_("Node OK"));
//    m_Tree->DeleteChildren(ti);
    FileDataVec &removers=m_updater->m_removers;
    FileDataVec &adders=m_updater->m_adders;
    if(removers.size()>0||adders.size()>0)
    {
        m_Tree->Freeze();
        //LOOP THROUGH THE REMOVERS LIST AND REMOVE THOSE ITEMS FROM THE TREE
    //    cbMessageBox(_("Removers"));
        for(FileDataVec::iterator it=removers.begin();it!=removers.end();it++)
        {
    //        cbMessageBox(it->name);
            wxTreeItemIdValue cookie;
            wxTreeItemId ch=m_Tree->GetFirstChild(ti,cookie);
            while(ch.IsOk())
            {
                if(it->name==m_Tree->GetItemText(ch))
                {
                    m_Tree->Delete(ch);
                    break;
                }
                ch=m_Tree->GetNextChild(ti,cookie);
            }
        }
        //LOOP THROUGH THE ADDERS LIST AND ADD THOSE ITEMS TO THE TREE
    //    cbMessageBox(_("Adders"));
        for(FileDataVec::iterator it=adders.begin();it!=adders.end();it++)
        {
    //        cbMessageBox(it->name);
            wxTreeItemId newitem=m_Tree->AppendItem(ti,it->name,it->state);
            m_Tree->SetItemHasChildren(newitem,it->state==fvsFolder);
        }
        m_Tree->SortChildren(ti);
        m_Tree->Thaw();
    }
    if(!m_Tree->IsExpanded(ti))
    {
        m_update_expand=true;
        m_Tree->Expand(ti);
    }
    delete m_updater;
    m_updater=NULL;
    m_update_active=false;
    m_updatetimer->Start(10,true);
    // Restart the monitor (TODO: move this elsewhere??)
    ResetFsWatcher();
}

wxString FileExplorer::GetFullPath(const wxTreeItemId &ti)
{
    if (!ti.IsOk())
        return wxEmptyString;
    wxFileName path(m_root);
    if (ti!=m_Tree->GetRootItem())
    {
        std::vector<wxTreeItemId> vti;
        vti.push_back(ti);
        wxTreeItemId pti=m_Tree->GetItemParent(vti[0]);
        if (!pti.IsOk())
            return wxEmptyString;
        while (pti != m_Tree->GetRootItem())
        {
            vti.insert(vti.begin(), pti);
            pti=m_Tree->GetItemParent(pti);
        }
        //Complicated logic to deal with the fact that the selected item might
        //be a partial path and not just a filename. It would be far simpler to
        for (size_t i=0; i<vti.size() - 1; i++)
            path.AppendDir(m_Tree->GetItemText(vti[i]));
        wxFileName last_part(m_Tree->GetItemText(vti[vti.size()-1]));
        wxArrayString as = last_part.GetDirs();
        for (size_t i=0;i<as.size();i++)
            path.AppendDir(as[i]);
        path = wxFileName(path.GetFullPath(), last_part.GetFullName()).GetFullPath();
    }
    return path.GetFullPath();
}

void FileExplorer::OnExpand(wxTreeEvent &event)
{
    if(m_updated_node==event.GetItem() && m_update_expand)
    {
        m_update_expand=false;
        return;
    }
    m_update_queue->Add(event.GetItem());
    m_updatetimer->Start(10,true);
    event.Veto();
    //AddTreeItems(event.GetItem());
}

void FileExplorer::ReadConfig()
{
    //IMPORT SETTINGS FROM LEGACY SHELLEXTENSIONS PLUGIN - TODO: REMOVE IN NEXT VERSION
    ConfigManager* cfg = Manager::Get()->GetConfigManager("ShellExtensions");
    if(!cfg->Exists("FileExplorer/ShowHiddenFiles"))
        cfg = Manager::Get()->GetConfigManager("FileManager");
    int len=0;
    cfg->Read("FileExplorer/FavRootList/Len", &len);
    for(int i=0;i<len;i++)
    {
        const wxString ref(wxString::Format("FileExplorer/FavRootList/I%i", i));
        FavoriteDir fav;
        cfg->Read(ref+"/alias", &fav.alias);
        cfg->Read(ref+"/path", &fav.path);
        m_Loc->Append(fav.alias);
        m_favdirs.Add(fav);
    }
    len=0;
    cfg->Read("FileExplorer/RootList/Len", &len);
    for(int i=0;i<len;i++)
    {
        const wxString ref(wxString::Format("FileExplorer/RootList/I%i",i));
        wxString loc;
        cfg->Read(ref, &loc);
        m_Loc->Append(loc);
    }
    len=0;
    cfg->Read("FileExplorer/WildMask/Len", &len);
    for(int i=0;i<len;i++)
    {
        const wxString ref(wxString::Format("FileExplorer/WildMask/I%i",i));
        wxString wild;
        cfg->Read(ref, &wild);
        m_WildCards->Append(wild);
    }
    cfg->Read("FileExplorer/ParseCVS",        &m_parse_cvs);
    cfg->Read("FileExplorer/ParseSVN",        &m_parse_svn);
    cfg->Read("FileExplorer/ParseHG",         &m_parse_hg);
    cfg->Read("FileExplorer/ParseBZR",        &m_parse_bzr);
    cfg->Read("FileExplorer/ParseGIT",        &m_parse_git);
    cfg->Read("FileExplorer/ShowHiddenFiles", &m_show_hidden);
}

void FileExplorer::WriteConfig()
{
    //DISCARD SETTINGS FROM LEGACY SHELLEXTENSIONS PLUGIN - TODO: REMOVE IN NEXT VERSION
    ConfigManager* cfg = Manager::Get()->GetConfigManager("ShellExtensions");
    if(cfg->Exists("FileExplorer/ShowHiddenFiles"))
        cfg->DeleteSubPath("FileExplorer");
    cfg = Manager::Get()->GetConfigManager("FileManager");
    //cfg->Clear();
    int count=static_cast<int>(m_favdirs.GetCount());
    cfg->Write("FileExplorer/FavRootList/Len", count);
    for(int i=0;i<count;i++)
    {
        const wxString ref(wxString::Format("FileExplorer/FavRootList/I%i", i));
        cfg->Write(ref+"/alias", m_favdirs[i].alias);
        cfg->Write(ref+"/path", m_favdirs[i].path);
    }
    count=static_cast<int>(m_Loc->GetCount())-static_cast<int>(m_favdirs.GetCount());
    cfg->Write("FileExplorer/RootList/Len", count);
    for(int i=0;i<count;i++)
    {
        const wxString ref(wxString::Format("FileExplorer/RootList/I%i", i));
        cfg->Write(ref, m_Loc->GetString(m_favdirs.GetCount()+i));
    }
    count=static_cast<int>(m_WildCards->GetCount());
    cfg->Write("FileExplorer/WildMask/Len", count);
    for(int i=0;i<count;i++)
    {
        const wxString ref(wxString::Format("FileExplorer/WildMask/I%i", i));
        cfg->Write(ref, m_WildCards->GetString(i));
    }
    cfg->Write("FileExplorer/ParseCVS",        m_parse_cvs);
    cfg->Write("FileExplorer/ParseSVN",        m_parse_svn);
    cfg->Write("FileExplorer/ParseHG",         m_parse_hg);
    cfg->Write("FileExplorer/ParseBZR",        m_parse_bzr);
    cfg->Write("FileExplorer/ParseGIT",        m_parse_git);
    cfg->Write("FileExplorer/ShowHiddenFiles", m_show_hidden);
}

void FileExplorer::OnEnterWild(wxCommandEvent &/*event*/)
{
    wxString wild=m_WildCards->GetValue();
    for(size_t i=0;i<m_WildCards->GetCount();i++)
    {
        wxString cmp;
        cmp=m_WildCards->GetString(i);
        if(cmp==wild)
        {
            m_WildCards->Delete(i);
            m_WildCards->Insert(wild,0);
            m_WildCards->SetSelection(0);
            RefreshExpanded(m_Tree->GetRootItem());
            return;
        }
    }
    m_WildCards->Insert(wild,0);
    if(m_WildCards->GetCount()>10)
        m_WildCards->Delete(10);
    m_WildCards->SetSelection(0);
    RefreshExpanded(m_Tree->GetRootItem());
}

void FileExplorer::OnChooseWild(wxCommandEvent &/*event*/)
{
    // Beware on win32 that if user opens drop down, then types a wildcard the combo box
    // event will contain a -1 selection and an empty string item. Harmless in current code.
    wxString wild=m_WildCards->GetValue();
    m_WildCards->Delete(m_WildCards->GetSelection());
    m_WildCards->Insert(wild,0);
//    event.Skip(true);
//    cbMessageBox(wild);
    m_WildCards->SetSelection(0);
    RefreshExpanded(m_Tree->GetRootItem());
}

void FileExplorer::OnEnterLoc(wxCommandEvent &/*event*/)
{
    wxString loc=m_Loc->GetValue();
    if(!SetRootFolder(loc))
        return;
    for(size_t i=0;i<m_Loc->GetCount();i++)
    {
        wxString cmp;
        if(i<m_favdirs.GetCount())
            cmp=m_favdirs[i].path;
        else
            cmp=m_Loc->GetString(i);
        if(cmp==m_root)
        {
            if(i>=m_favdirs.GetCount())
            {
                m_Loc->Delete(i);
                m_Loc->Insert(m_root,m_favdirs.GetCount());
            }
            m_Loc->SetSelection(m_favdirs.GetCount());
            return;
        }
    }
    m_Loc->Insert(m_root,m_favdirs.GetCount());
    if(m_Loc->GetCount()>10+m_favdirs.GetCount())
        m_Loc->Delete(10+m_favdirs.GetCount());
    m_Loc->SetSelection(m_favdirs.GetCount());
}

void FileExplorer::OnChooseLoc(wxCommandEvent &event)
{
    wxString loc;
    // on WIN32 if the user opens the drop down, but then types a path instead, this event
    // fires with an empty string, so we have no choice but to return null. This event
    // doesn't happen on Linux (the drop down closes when the user starts typing)
    if(event.GetInt()<0)
        return;
    if(event.GetInt()>=static_cast<int>(m_favdirs.GetCount()))
        loc=m_Loc->GetValue();
    else
        loc=m_favdirs[event.GetInt()].path;
    if(!SetRootFolder(loc))
        return;
    if(event.GetInt()>=static_cast<int>(m_favdirs.GetCount()))
    {
        m_Loc->Delete(event.GetInt());
        m_Loc->Insert(m_root,m_favdirs.GetCount());
        m_Loc->SetSelection(m_favdirs.GetCount());
    }
    else
    {
        for(size_t i=m_favdirs.GetCount();i<m_Loc->GetCount();i++)
        {
            wxString cmp;
            cmp=m_Loc->GetString(i);
            if(cmp==m_root)
            {
                m_Loc->Delete(i);
                m_Loc->Insert(m_root,m_favdirs.GetCount());
                m_Loc->SetSelection(event.GetInt());
                return;
            }
        }
        m_Loc->Insert(m_root,m_favdirs.GetCount());
        if(m_Loc->GetCount()>10+m_favdirs.GetCount())
            m_Loc->Delete(10+m_favdirs.GetCount());
        m_Loc->SetSelection(event.GetInt());
    }
}

void FileExplorer::OnSetLoc(wxCommandEvent &/*event*/)
{
    wxString loc=GetFullPath(m_Tree->GetFocusedItem());
    if(!SetRootFolder(loc))
        return;
    m_Loc->Insert(m_root,m_favdirs.GetCount());
    if(m_Loc->GetCount()>10+m_favdirs.GetCount())
        m_Loc->Delete(10+m_favdirs.GetCount());
}

void FileExplorer::OnVCSChangesCheck(wxCommandEvent &/*event*/)
{
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnVCSControl(wxCommandEvent &/*event*/)
{
    wxString commit = m_VCS_Control->GetString(m_VCS_Control->GetSelection());
    if (commit == _("Select commit..."))
    {
        CommitBrowser *cm = new CommitBrowser(this, GetFullPath(m_Tree->GetRootItem()), m_VCS_Type->GetLabel());
        if(cm->ShowModal() == wxID_OK)
        {
            commit = cm->GetSelectedCommit();
            cm->Destroy();
            if (!commit.empty())
            {
                unsigned int i=0;
                for (; i<m_VCS_Control->GetCount(); ++i)
                {
                    if (m_VCS_Control->GetString(i) == commit)
                    {
                        m_VCS_Control->SetSelection(i);
                        break;
                    }
                }
                if (i == m_VCS_Control->GetCount())
                    m_VCS_Control->Append(commit);
                m_VCS_Control->SetSelection(m_VCS_Control->GetCount()-1);
            }
        }
        else
            commit = wxEmptyString;
    }
    if (!commit.empty())
    {
        m_commit = commit;
        Refresh(m_Tree->GetRootItem());
    } else
    {
        unsigned int i=0;
        for (; i<m_VCS_Control->GetCount(); ++i)
        {
            if (m_VCS_Control->GetString(i) == m_commit)
            {
                m_VCS_Control->SetSelection(i);
                break;
            }
        }
    }
}

void FileExplorer::OnOpenInEditor(wxCommandEvent &/*event*/)
{
    for(int i=0;i<m_ticount;i++)
    {
        if (IsBrowsingVCSTree())
        {
            wxFileName path(GetFullPath(m_selectti[i]));
            wxString original_path = path.GetFullPath();
            path.MakeRelativeTo(GetRootFolder());
            wxString name = path.GetFullName();
            wxString vcs_type = m_VCS_Type->GetLabel();
            name = vcs_type + "-" + m_commit.Mid(0,6) + "-" + name;
            path.SetFullName(name);
            wxFileName tmp = wxFileName(wxFileName::GetTempDir(), "");
            tmp.AppendDir("codeblocks-fm");
            path.MakeAbsolute(tmp.GetFullPath());
            if(!path.FileExists())
                m_vcs_file_loader_queue.Add("cat", original_path, path.GetFullPath());
            else
                DoOpenInEditor(path.GetFullPath());
        }
        else
        {
            wxFileName path(GetFullPath(m_selectti[i]));
            wxString filename=path.GetFullPath();
            if(!path.FileExists())
                continue;
            DoOpenInEditor(filename);
        }
    }
    if (m_vcs_file_loader==nullptr && !m_vcs_file_loader_queue.empty())
    {
        LoaderQueueItem item = m_vcs_file_loader_queue.Pop();
        m_vcs_file_loader = new VCSFileLoader(this);
        m_vcs_file_loader->Update(item.op, item.source, item.destination, item.comp_commit);
    }
}

void FileExplorer::OnVCSDiff(wxCommandEvent &event)
{
    wxString comp_commit;
    if (event.GetId() == ID_FILEDIFF) //Diff with head (for working copy) or previous (for commit)
        comp_commit = _("Previous");
    else //Otherwise diff against specific revision
        comp_commit = m_VCS_Control->GetString(event.GetId()-ID_FILEDIFF1);
    if (m_commit == _("Working copy") && comp_commit == _("Working copy"))
        comp_commit = _("Previous");
    if (comp_commit == _("Select commit..."))
    {
        wxString diff_paths;
        for(int i=0;i<m_ticount;i++)
        {
            wxFileName path(GetFullPath(m_selectti[i]));
            path.MakeRelativeTo(GetRootFolder());
            if (path != wxEmptyString)
                diff_paths += " \"" + path.GetFullPath() + "\"";
        }
        CommitBrowser *cm = new CommitBrowser(this, GetFullPath(m_Tree->GetRootItem()), m_VCS_Type->GetLabel(), diff_paths);
        if(cm->ShowModal() == wxID_OK)
        {
            comp_commit = cm->GetSelectedCommit();
        }
        else
            return;
    }
    wxString diff_paths;
    for(int i=0;i<m_ticount;i++)
    {
        wxFileName path(GetFullPath(m_selectti[i]));
        path.MakeRelativeTo(GetRootFolder());
        if (path != wxEmptyString)
            diff_paths += " \"" + path.GetFullPath() + "\"";
    }
    wxFileName tmp = wxFileName(wxFileName::GetTempDir(), "");
    wxFileName root_path(GetRootFolder());
    wxString name = root_path.GetName();
    wxString vcs_type = m_VCS_Type->GetLabel();
    tmp.AppendDir("codeblocks-fm");
    name = "diff-" + vcs_type + "-" + m_commit.Mid(0,7) + "~" + comp_commit + "-" + name + ".patch";
    wxString dest_tmp_path = wxFileName(tmp.GetFullPath(), name).GetFullPath();
    m_vcs_file_loader_queue.Add("diff", diff_paths, dest_tmp_path, comp_commit);
    if (m_vcs_file_loader==nullptr && !m_vcs_file_loader_queue.empty())
    {
        LoaderQueueItem item = m_vcs_file_loader_queue.Pop();
        m_vcs_file_loader = new VCSFileLoader(this);
        m_vcs_file_loader->Update(item.op, item.source, item.destination, item.comp_commit);
    }
}

void FileExplorer::OnVCSFileLoaderComplete(wxCommandEvent& /*event*/)
{
    m_vcs_file_loader->Wait();
    DoOpenInEditor(m_vcs_file_loader->m_destination_path);
    delete m_vcs_file_loader;
    m_vcs_file_loader = nullptr;
    if (!m_vcs_file_loader_queue.empty())
    {
        LoaderQueueItem item = m_vcs_file_loader_queue.Pop();
        m_vcs_file_loader = new VCSFileLoader(this);
        m_vcs_file_loader->Update(item.op, item.source, item.destination, item.comp_commit);
    }
}

void FileExplorer::DoOpenInEditor(const wxString &filename)
{
    EditorManager* em = Manager::Get()->GetEditorManager();
    EditorBase* eb = em->IsOpen(filename);
    if (eb)
    {
        // open files just get activated
        eb->Activate();
        return;
    } else
        em->Open(filename);
}

void FileExplorer::OnActivate(wxTreeEvent &event)
{
    if (IsBrowsingVCSTree())
    {
        //TODO: Should just retrieve the file and use the same mimetype handling as a regular file
        wxCommandEvent e;
        m_ticount=m_Tree->GetSelections(m_selectti);
        OnOpenInEditor(e);
        return;
    }
    wxString filename=GetFullPath(event.GetItem());
    if(m_Tree->GetItemImage(event.GetItem())==fvsFolder)
    {
        event.Skip(true);
        return;
    }
    EditorManager* em = Manager::Get()->GetEditorManager();
    EditorBase* eb = em->IsOpen(filename);
    if (eb)
    {
        // open files just get activated
        eb->Activate();
        return;
    }

    // Use Mime handler to open file
    cbMimePlugin* plugin = Manager::Get()->GetPluginManager()->GetMIMEHandlerForFile(filename);
    if (!plugin)
    {
        wxString msg;
        msg.Printf(_("Could not open file '%s'.\nNo handler registered for this type of file."), filename);
        LogErrorMessage(msg);
//        em->Open(filename); //should never need to open the file from here
    }
    else if (plugin->OpenFile(filename) != 0)
    {
        const PluginInfo* info = Manager::Get()->GetPluginManager()->GetPluginInfo(plugin);
        wxString msg;
        msg.Printf(_("Could not open file '%s'.\nThe registered handler (%s) could not open it."), filename, info ? info->title : wxString(_("<Unknown plugin>")));
        LogErrorMessage(msg);
    }

//    if(!em->IsOpen(file))
//        em->Open(file);

}


void FileExplorer::OnKeyDown(wxKeyEvent &event)
{
    if(event.GetKeyCode() == WXK_DELETE)
    {
        if (IsBrowsingVCSTree())
        {
            wxCommandEvent event2;
            OnDelete(event2);
        }
    }
}


bool FileExplorer::IsBrowsingVCSTree()
{
    return m_commit != _("Working copy") && !m_commit.empty();
}

bool FileExplorer::IsBrowsingWorkingCopy()
{
    return m_commit == _("Working copy") && !m_commit.empty();
}

void FileExplorer::OnRightClick(wxTreeEvent &event)
{
    wxMenu* Popup = new wxMenu();
    m_ticount=m_Tree->GetSelections(m_selectti);
    if(!IsInSelection(event.GetItem())) //replace the selection with right clicked item if right clicked item isn't in the selection
    {
        for(int i=0;i<m_ticount;i++)
            m_Tree->SelectItem(m_selectti[i],false);
        m_Tree->SelectItem(event.GetItem());
        m_ticount=m_Tree->GetSelections(m_selectti);
        m_Tree->Update();
    }
    FileTreeData* ftd = new FileTreeData(0, FileTreeData::ftdkUndefined);
    ftd->SetKind(FileTreeData::ftdkFile);
    if(m_ticount>0)
    {
        if(m_ticount==1)
        {
            int img = m_Tree->GetItemImage(m_selectti[0]);
            if(img==fvsFolder)
            {
                ftd->SetKind(FileTreeData::ftdkFolder);
                Popup->Append(ID_SETLOC,_("Make roo&t"));
                Popup->Append(ID_FILEEXPANDALL,_("Expand all children")); //TODO: check availability in wx2.8 for win32 (not avail wx2.6)
                Popup->Append(ID_FILECOLLAPSEALL,_("Collapse all children")); //TODO: check availability in wx2.8 for win32 (not avail wx2.6)
                if (!IsBrowsingVCSTree())
                {
                    Popup->Append(ID_FILEMAKEFAV,_("Add to favorites"));
                    Popup->Append(ID_FILENEWFILE,_("New file..."));
                    Popup->Append(ID_FILENEWFOLDER,_("New directory..."));
                    Popup->Append(ID_FILERENAME,_("&Rename..."));
                }
            }
            else
            {
                if (!IsBrowsingVCSTree())
                    Popup->Append(ID_FILERENAME,_("&Rename..."));
            }
        }
        if(IsFilesOnly(m_selectti))
        {
            Popup->Append(ID_OPENINED,_("&Open in CB editor"));
            if (!IsBrowsingVCSTree())
                if(Manager::Get()->GetProjectManager()->GetActiveProject())
                    Popup->Append(ID_FILEADDTOPROJECT,_("&Add to active project..."));
        }
        if (!IsBrowsingVCSTree())
        {
            Popup->Append(ID_FILEDUP,_("&Duplicate"));
            Popup->Append(ID_FILECOPY,_("&Copy to..."));
            // Add these menu items only if the selection does not include the root folder
            if (!m_Tree->IsSelected(m_Tree->GetRootItem()))
            {
                Popup->Append(ID_FILEMOVE,_("&Move to..."));
                Popup->Append(ID_FILEDELETE,_("D&elete"));
            }
        }
        if ( IsBrowsingVCSTree() || IsBrowsingWorkingCopy() )
        {
            if (IsBrowsingWorkingCopy())
                Popup->Append(ID_FILEDIFF,_("&Diff"));
            else
                Popup->Append(ID_FILEDIFF,_("&Diff previous"));
            wxMenu *diff_menu = new wxMenu();
            unsigned int n = m_VCS_Control->GetCount();
            if (n>10)
                n=10;
            if (IsBrowsingWorkingCopy())
            {
                diff_menu->Append(ID_FILEDIFF1, _("Head"));
                for (unsigned int i = 1; i<n; ++i)
                    diff_menu->Append(ID_FILEDIFF1 + i, m_VCS_Control->GetString(i));
            }
            else
            {
                for (unsigned int i = 0; i<n; ++i)
                    diff_menu->Append(ID_FILEDIFF1 + i, m_VCS_Control->GetString(i));
            }
            Popup->AppendSubMenu(diff_menu,_("Diff against"));
        }
    }
    wxMenu *viewpop=new wxMenu();
    viewpop->Append(ID_FILESETTINGS,_("Favorite directories..."));
    viewpop->AppendCheckItem(ID_FILESHOWHIDDEN,_("Show &hidden files"))->Check(m_show_hidden);
//    viewpop->AppendCheckItem(ID_FILEPARSECVS,_("CVS decorators"))->Check(m_parse_cvs);
    viewpop->AppendCheckItem(ID_FILEPARSESVN,_("SVN integration"))->Check(m_parse_svn);
    viewpop->AppendCheckItem(ID_FILEPARSEHG,_("Hg integration"))->Check(m_parse_hg);
    viewpop->AppendCheckItem(ID_FILEPARSEBZR,_("Bzr integration"))->Check(m_parse_bzr);
    viewpop->AppendCheckItem(ID_FILEPARSEGIT,_("Git integration"))->Check(m_parse_git);
    if(m_ticount>1)
    {
        ftd->SetKind(FileTreeData::ftdkVirtualGroup);
        wxString pathlist = GetFullPath(m_selectti[0]);
        for(int i=1;i<m_ticount;i++)
            pathlist += "*" + GetFullPath(m_selectti[i]); //passing a '*' separated list of files/directories to any plugin takers
        ftd->SetFolder(pathlist);
    }
    else if (m_ticount > 0)
    {
        wxString filepath = GetFullPath(m_selectti[0]);
        ftd->SetFolder(filepath);
    }
    if(m_ticount>0)
        Manager::Get()->GetPluginManager()->AskPluginsForModuleMenu(mtFileExplorer, Popup, ftd);
    delete ftd;
    Popup->AppendSeparator();
    Popup->AppendSubMenu(viewpop,_("&Settings"));
    Popup->Append(ID_FILEREFRESH,_("Re&fresh"));
    wxWindow::PopupMenu(Popup);
    delete Popup;
}

void FileExplorer::OnNewFile(wxCommandEvent &/*event*/)
{
    wxString workingdir=GetFullPath(m_Tree->GetFocusedItem());
    wxTextEntryDialog te(this, _("Name Your New File: "));
    PlaceWindow(&te);
    if(te.ShowModal()!=wxID_OK)
        return;
    wxString name=te.GetValue();
    wxFileName file(workingdir);
    file.Assign(file.GetFullPath(),name);
    wxString newfile=file.GetFullPath();
    if(!wxFileName::FileExists(newfile) &&!wxFileName::DirExists(newfile))
    {
        wxFile fileobj;
        if(fileobj.Create(newfile))
        {
            fileobj.Close();
            Refresh(m_Tree->GetFocusedItem());
        }
        else
            cbMessageBox(_("File Creation Failed"),_("Error"));
    }
    else
        cbMessageBox(_("File/Directory Already Exists with Name ")+name, _("Error"));
}

void FileExplorer::OnAddFavorite(wxCommandEvent &/*event*/)
{
    FavoriteDir fav;
    fav.path=GetFullPath(m_selectti[0]);
    if(fav.path[fav.path.Len()-1]!=wxFileName::GetPathSeparator())
        fav.path=fav.path+wxFileName::GetPathSeparator();
    wxTextEntryDialog ted(NULL,_("Enter an alias for this directory:"),_("Add Favorite Directory"),fav.path);
    PlaceWindow(&ted);
    if(ted.ShowModal()!=wxID_OK)
        return;
    wxString name=ted.GetValue();
    fav.alias=name;
    m_favdirs.Insert(fav,0);
    m_Loc->Insert(name,0);
}

void FileExplorer::OnNewFolder(wxCommandEvent &/*event*/)
{
    wxString workingdir=GetFullPath(m_Tree->GetFocusedItem());
    wxTextEntryDialog te(this,_("New Directory Name: "));
    PlaceWindow(&te);
    if(te.ShowModal()!=wxID_OK)
        return;
    wxString name=te.GetValue();
    wxFileName dir(workingdir);
    dir.Assign(dir.GetFullPath(),name);
    wxString mkd=dir.GetFullPath();
    if(!wxFileName::DirExists(mkd) &&!wxFileName::FileExists(mkd))
    {
        if (!dir.Mkdir(mkd))
            cbMessageBox(_("A directory could not be created with name ")+name);
        Refresh(m_Tree->GetFocusedItem());
    }
    else
        cbMessageBox(_("A file or directory already exists with name ")+name);
}

void FileExplorer::OnDuplicate(wxCommandEvent &/*event*/)
{
    m_ticount=m_Tree->GetSelections(m_selectti);
    for(int i=0;i<m_ticount;i++)
    {
        wxFileName path(GetFullPath(m_selectti[i]));  //SINGLE: m_Tree->GetSelection()
        if(wxFileName::FileExists(path.GetFullPath())||wxFileName::DirExists(path.GetFullPath()))
        {
            if(!PromptSaveOpenFile(_("File is modified, press Yes to save before duplication, No to copy unsaved file or Cancel to skip file"),wxFileName(path)))
                continue;
            int j=1;
            wxString destpath(path.GetPathWithSep()+path.GetName()+wxString::Format("(%i)", j));
            if(!path.GetExt().empty())
                destpath += "."+path.GetExt();
            while(j<100 && (wxFileName::FileExists(destpath) || wxFileName::DirExists(destpath)))
            {
                j++;
                destpath=path.GetPathWithSep()+path.GetName()+wxString::Format("(%i)", j);
                if(!path.GetExt().empty())
                    destpath += "."+path.GetExt();
            }
            if(j==100)
            {
                cbMessageBox(_("Too many copies of file or directory"));
                continue;
            }

#ifdef __WXMSW__
            wxArrayString output;
            wxString cmdline;
            if(wxFileName::FileExists(path.GetFullPath()))
                cmdline="cmd /c copy /Y \""+path.GetFullPath()+"\" \""+destpath+"\"";
            else
                cmdline="cmd /c xcopy /S/E/Y/H/I \""+path.GetFullPath()+"\" \""+destpath+"\"";
            int hresult=::wxExecute(cmdline,output,wxEXEC_SYNC);
#else
            wxString cmdline="/bin/cp -r -b \""+path.GetFullPath()+"\" \""+destpath+"\"";
            int hresult=::wxExecute(cmdline,wxEXEC_SYNC);
#endif
            if(hresult)
                MessageBox(m_Tree, wxString::Format(_("Command '%s' failed with error %i"), cmdline, hresult));
        }
    }
    Refresh(m_Tree->GetRootItem()); //TODO: Can probably be more efficient than this
    //TODO: Reselect item in new location?? (what it outside root scope?)
}


void FileExplorer::CopyFiles(const wxString &destination, const wxArrayString &selectedfiles)
{
    for(unsigned int i=0;i<selectedfiles.Count();i++)
    {
        wxString path=selectedfiles[i];
        wxFileName destpath;
        destpath.Assign(destination,wxFileName(path).GetFullName());
        if(destpath.SameAs(path))
            continue;
        if(wxFileName::FileExists(path)||wxFileName::DirExists(path))
        {
            if(!PromptSaveOpenFile(_("File is modified, press Yes to save before duplication, No to copy unsaved file or Cancel to skip file"),wxFileName(path)))
                continue;
#ifdef __WXMSW__
            wxArrayString output;
            wxString cmdline;
            if(wxFileName::FileExists(path))
                cmdline="cmd /c copy /Y \""+path+"\" \""+destpath.GetFullPath()+"\"";
            else
                cmdline="cmd /c xcopy /S/E/Y/H/I \""+path+"\" \""+destpath.GetFullPath()+"\"";
            int hresult=::wxExecute(cmdline,output,wxEXEC_SYNC);
#else
            int hresult=::wxExecute("/bin/cp -r -b \""+path+"\" \""+destpath.GetFullPath()+"\"",wxEXEC_SYNC);
#endif
            if (hresult)
                MessageBox(m_Tree, wxString::Format(_("Copying '%s' failed with error %i"), path, hresult));
        }
    }
}

void FileExplorer::OnCopy(wxCommandEvent &/*event*/)
{
    wxDirDialog dd(this,_("Copy to"));
    dd.SetPath(GetFullPath(m_Tree->GetRootItem()));
    wxArrayString selectedfiles;
    m_ticount=m_Tree->GetSelections(m_selectti);
    for(int i=0;i<m_ticount;i++) // really important not to rely on TreeItemId ater modal dialogs because file updates can change the file tree in the background.
    {
        selectedfiles.Add(GetFullPath(m_selectti[i]));  //SINGLE: m_Tree->GetSelection()
    }
    PlaceWindow(&dd);
    if(dd.ShowModal()==wxID_CANCEL)
        return;
    CopyFiles(dd.GetPath(),selectedfiles);
//    Refresh(m_Tree->GetRootItem()); //TODO: Use this if monitoring not available
    //TODO: Reselect item in new location?? (what if outside root scope?)
}

void FileExplorer::MoveFiles(const wxString &destination, const wxArrayString &selectedfiles)
{
    for(unsigned int i=0;i<selectedfiles.Count();i++)
    {
        wxString path=selectedfiles[i];
        wxFileName destpath;
        destpath.Assign(destination,wxFileName(path).GetFullName());
        if(destpath.SameAs(path)) //TODO: Log message that can't copy over self.
            continue;
        if(wxFileName::FileExists(path)||wxFileName::DirExists(path))
        {
#ifdef __WXMSW__
            wxArrayString output;
            int hresult=::wxExecute("cmd /c move /Y \""+path+"\" \""+destpath.GetFullPath()+"\"",output,wxEXEC_SYNC);
#else
            int hresult=::wxExecute("/bin/mv -b \""+path+"\" \""+destpath.GetFullPath()+"\"",wxEXEC_SYNC);
#endif
            if (hresult)
                MessageBox(m_Tree, wxString::Format(_("Moving '%s' failed with error %i"), path, hresult));
        }
    }
}

void FileExplorer::OnMove(wxCommandEvent &/*event*/)
{
    wxDirDialog dd(this,_("Move to"));
    wxArrayString selectedfiles;
    m_ticount=m_Tree->GetSelections(m_selectti);
    for(int i=0;i<m_ticount;i++)
        selectedfiles.Add(GetFullPath(m_selectti[i]));  //SINGLE: m_Tree->GetSelection()
    dd.SetPath(GetFullPath(m_Tree->GetRootItem()));
    PlaceWindow(&dd);
    if(dd.ShowModal()==wxID_CANCEL)
        return;
    MoveFiles(dd.GetPath(),selectedfiles);
//    Refresh(m_Tree->GetRootItem()); //TODO: Can probably be more efficient than this
    //TODO: Reselect item in new location?? (what if outside root scope?)
}

wxArrayString FileExplorer::GetSelectedPaths()
{
    wxArrayString paths;
    for(int i=0;i<m_ticount;i++)
    {
        wxString path(GetFullPath(m_selectti[i]));  //SINGLE: m_Tree->GetSelection()
        paths.Add(path);
    }
    return paths;
}

void FileExplorer::OnDelete(wxCommandEvent &/*event*/)
{
    m_ticount=m_Tree->GetSelections(m_selectti);
    wxArrayString as=GetSelectedPaths();
    wxString prompt=_("You are about to delete\n\n");
    for(unsigned int i=0;i<as.Count();i++)
        prompt+=as[i]+'\n';
    prompt+=_("\nAre you sure?");
    if(MessageBox(m_Tree,prompt,_("Delete"),wxYES_NO)!=wxID_YES)
        return;
    for(unsigned int i=0;i<as.Count();i++)
    {
        wxString path=as[i];  //SINGLE: m_Tree->GetSelection()
        if(wxFileName::FileExists(path))
        {
            //        EditorManager* em = Manager::Get()->GetEditorManager();
            //        if(em->IsOpen(path))
            //        {
            //            cbMessageBox(wxString::Format(_("Close file %s first"), path.GetFullPath()));
            //            return;
            //        }
            if(!::wxRemoveFile(path))
                MessageBox(m_Tree, wxString::Format(_("Delete file '%s' failed"), path));
        }
        else if(wxFileName::DirExists(path))
        {
#ifdef __WXMSW__
            wxArrayString output;
            int hresult=::wxExecute("cmd /c rmdir /S/Q \""+path+"\"",output,wxEXEC_SYNC);
#else
            int hresult=::wxExecute("/bin/rm -r -f \""+path+"\"",wxEXEC_SYNC);
#endif
            if(hresult)
                MessageBox(m_Tree, wxString::Format(_("Delete directory '%s' failed with error %i"), path, hresult));
        }
    }
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnRename(wxCommandEvent &/*event*/)
{
    wxString path(GetFullPath(m_Tree->GetFocusedItem()));
    if(wxFileName::FileExists(path))
    {
        EditorManager* em = Manager::Get()->GetEditorManager();
        if(em->IsOpen(path))
        {
            cbMessageBox(_("Close file first"));
            return;
        }
        wxTextEntryDialog te(this,_("New name:"),_("Rename File"),wxFileName(path).GetFullName());
        PlaceWindow(&te);
        if(te.ShowModal()==wxID_CANCEL)
            return;
        wxFileName destpath(path);
        destpath.SetFullName(te.GetValue());
        if(!::wxRenameFile(path,destpath.GetFullPath()))
            cbMessageBox(_("Rename failed"));
    }
    if(wxFileName::DirExists(path))
    {
        wxTextEntryDialog te(this,_("New name:"),_("Rename File"),wxFileName(path).GetFullName());
        PlaceWindow(&te);
        if(te.ShowModal()==wxID_CANCEL)
            return;
        wxFileName destpath(path);
        destpath.SetFullName(te.GetValue());
#ifdef __WXMSW__
        wxArrayString output;
        int hresult=::wxExecute("cmd /c move /Y \""+path+"\" \""+destpath.GetFullPath()+"\"",output,wxEXEC_SYNC);
#else
        int hresult=::wxExecute("/bin/mv \""+path+"\" \""+destpath.GetFullPath()+"\"",wxEXEC_SYNC);
#endif
        if(hresult)
            MessageBox(m_Tree, wxString::Format(_("Rename directory '%s' failed with error %i"), path, hresult));
    }
    Refresh(m_Tree->GetItemParent(m_Tree->GetFocusedItem()));
}

void FileExplorer::OnExpandAll(wxCommandEvent &/*event*/)
{
    m_Tree->ExpandAllChildren(m_Tree->GetFocusedItem());
}

void FileExplorer::OnCollapseAll(wxCommandEvent &/*event*/)
{
    m_Tree->CollapseAllChildren(m_Tree->GetFocusedItem());
}

void FileExplorer::OnSettings(wxCommandEvent &/*event*/)
{
    FileBrowserSettings fbs(m_favdirs,NULL);
    PlaceWindow(&fbs);
    if(fbs.ShowModal()==wxID_OK)
    {
        size_t count=m_favdirs.GetCount();
        for(size_t i=0;i<count;i++)
            m_Loc->Delete(0);
        m_favdirs=fbs.m_favdirs;
        count=m_favdirs.GetCount();
        for(size_t i=0;i<count;i++)
            m_Loc->Insert(m_favdirs[i].alias,i);
    }

}

void FileExplorer::OnShowHidden(wxCommandEvent &/*event*/)
{
    m_show_hidden = !m_show_hidden;
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnParseCVS(wxCommandEvent &/*event*/)
{
    m_parse_cvs = !m_parse_cvs;
    //cfg->Clear();
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnParseSVN(wxCommandEvent &/*event*/)
{
    m_parse_svn = !m_parse_svn;
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnParseGIT(wxCommandEvent &/*event*/)
{
    m_parse_git = !m_parse_git;
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnParseHG(wxCommandEvent &/*event*/)
{
    m_parse_hg = !m_parse_hg;
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnParseBZR(wxCommandEvent &/*event*/)
{
    m_parse_bzr = !m_parse_bzr;
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnUpButton(wxCommandEvent &/*event*/)
{
    wxFileName loc(m_root);
    if (loc.GetDirCount())
    {
        loc.RemoveLastDir();
        SetRootFolder(loc.GetFullPath());
    }
}

void FileExplorer::OnRefresh(wxCommandEvent &/*event*/)
{
    if(m_Tree->GetItemImage(m_Tree->GetFocusedItem())==fvsFolder)
        Refresh(m_Tree->GetFocusedItem());
    else
        Refresh(m_Tree->GetRootItem());
}

//TODO: Set copy cursor state if necessary
void FileExplorer::OnBeginDragTreeItem(wxTreeEvent &event)
{
//    SetCursor(wxCROSS_CURSOR);
//    if(IsInSelection(event.GetItem()))
//        return; // don't start a drag for an unselected item
    if (!IsBrowsingVCSTree())
        event.Allow();
//    m_dragtest=GetFullPath(event.GetItem());
    m_ticount=m_Tree->GetSelections(m_selectti);
}

bool FileExplorer::IsInSelection(const wxTreeItemId &ti)
{
    for(int i=0;i<m_ticount;i++)
        if(ti==m_selectti[i])
            return true;
    return false;
}

//TODO: End copy cursor state if necessary
void FileExplorer::OnEndDragTreeItem(wxTreeEvent &event)
{
//    SetCursor(wxCursor(wxCROSS_CURSOR));
    if(m_Tree->GetItemImage(event.GetItem())!=fvsFolder) //can only copy to folders
        return;
    for(int i=0;i<m_ticount;i++)
    {
        wxString path(GetFullPath(m_selectti[i]));
        wxFileName destpath;
        if(!event.GetItem().IsOk())
            return;
        destpath.Assign(GetFullPath(event.GetItem()),wxFileName(path).GetFullName());
        if(destpath.SameAs(path))
            continue;
        if(wxFileName::DirExists(path)||wxFileName::FileExists(path))
        {
            if(!::wxGetKeyState(WXK_CONTROL))
            {
                if(wxFileName::FileExists(path))
                    if(!PromptSaveOpenFile(_("File is modified, press Yes to save before move, No to move unsaved file or Cancel to skip file"),wxFileName(path)))
                        continue;
#ifdef __WXMSW__
                wxArrayString output;
                int hresult=::wxExecute("cmd /c move /Y \""+path+"\" \""+destpath.GetFullPath()+"\"",output,wxEXEC_SYNC);
#else
                int hresult=::wxExecute("/bin/mv -b \""+path+"\" \""+destpath.GetFullPath()+"\"",wxEXEC_SYNC);
#endif
                if(hresult)
                    MessageBox(m_Tree, wxString::Format(_("Move directory '%s' failed with error %i"), path, hresult));
            }
            else
            {
                if(wxFileName::FileExists(path))
                    if(!PromptSaveOpenFile(_("File is modified, press Yes to save before copy, No to copy unsaved file or Cancel to skip file"),wxFileName(path)))
                        continue;
#ifdef __WXMSW__
                wxArrayString output;
                wxString cmdline;
                if(wxFileName::FileExists(path))
                    cmdline="cmd /c copy /Y \""+path+"\" \""+destpath.GetFullPath()+"\"";
                else
                    cmdline="cmd /c xcopy /S/E/Y/H/I \""+path+"\" \""+destpath.GetFullPath()+"\"";
                int hresult=::wxExecute(cmdline,output,wxEXEC_SYNC);
#else
                int hresult=::wxExecute("/bin/cp -r -b \""+path+"\" \""+destpath.GetFullPath()+"\"",wxEXEC_SYNC);
#endif
                if(hresult)
                    MessageBox(m_Tree, wxString::Format(_("Copy directory '%s' failed with error %i"), path, hresult));
            }
        }
//        if(!PromptSaveOpenFile(_("File is modified, press \"Yes\" to save before move/copy, \"No\" to move/copy unsaved file or \"Cancel\" to abort the operation"),path)) //TODO: specify move or copy depending on whether CTRL held down
//            return;
    }
    Refresh(m_Tree->GetRootItem());
}

void FileExplorer::OnAddToProject(wxCommandEvent &/*event*/)
{
    wxArrayString files;
    wxString file;
    for(int i=0;i<m_ticount;i++)
    {
        file=GetFullPath(m_selectti[i]);
        if(wxFileName::FileExists(file))
            files.Add(file);
    }
    wxArrayInt prompt;
    Manager::Get()->GetProjectManager()->AddMultipleFilesToProject(files, NULL, prompt);
    Manager::Get()->GetProjectManager()->GetUI().RebuildTree();
}

bool FileExplorer::IsFilesOnly(wxArrayTreeItemIds tis)
{
    for(size_t i=0;i<tis.GetCount();i++)
        if(m_Tree->GetItemImage(tis[i])==fvsFolder)
            return false;
    return true;
}
