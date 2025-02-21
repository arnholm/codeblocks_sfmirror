/////////////////////////////////////////////////////////////////////////////
// Name:        menuutils.cpp
// Purpose:     wxMenuCmd, wxMenuWalker, wxMenuTreeWalker,
//              wxMenuShortcutWalker...
// Author:      Francesco Montorsi
// Created:     2004/02/19
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////
// RCS-ID:      $Id$

// menuutils for KeyBinder v2.0 2019/04/8

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

//#if defined(CB_PRECOMP) //(2019/03/1)-
//#include "sdk.h"
//#else
//    #include <wx/event.h>
//    #include <wx/frame.h> // Manager::Get()->GetAppWindow()
//    #include <wx/intl.h>
//    #include <wx/menu.h>
//    #include <wx/menuitem.h>
//    #include <wx/string.h>
//    #include "sdk_events.h"
//    #include "manager.h"
//    #include "projectmanager.h"
//    #include "editormanager.h"
//    #include "cbworkspace.h"
//    #include "cbproject.h"
//    #include "logmanager.h"
//#endif

// includes
//-#include "debugging.h"
#include "menuutils.h"


// static
wxMenuBar* wxMenuCmd::m_pMenuBar = NULL;
// ----------------------------------------------------------------------------
namespace
// ----------------------------------------------------------------------------
{
    bool wxFound(int result){return result != wxNOT_FOUND;}
}
// ----------------------------------------------------------------------------
// Global utility functions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
int wxFindMenuItem(wxMenuBar *p, const wxString &str)
// ----------------------------------------------------------------------------
{
    int id = wxNOT_FOUND;

    for (int i=0; i < (int)p->GetMenuCount(); i++) {

        id = p->GetMenu(i)->FindItem(str);
        if (id != wxNOT_FOUND)
            break;
    }

    return id;
}

namespace
{
//// ----------------------------------------------------------------------------
//int FindMenuDuplicateCount(wxMenuBar *p, wxString &str)
//// ----------------------------------------------------------------------------
//{
//    //int id = wxNOT_FOUND;
//    int count = 0;
//
//    for (int i = 0; i < (int)p->GetMenuCount(); ++i) {
//
//        //id = p->GetMenu(i)->FindItem(str);
//        //if (id != wxNOT_FOUND)
//        //  count++;
//        FindMenuDuplicateItems( p->GetMenu(i), str, count);
//    }
//
//    return count;
//}
}
// ----------------------------------------------------------------------------
int FindMenuDuplicateItems(wxMenu* pMenu, wxString& rStr, int& rCount)
// ----------------------------------------------------------------------------
{
    // Recursively scan & count submenu items with name rStr

    size_t itemKnt = pMenu->GetMenuItemCount();
    for (size_t j=0; j<itemKnt; j++ )
    {
        // check each item on this subMenu
        wxMenuItem* pMenuItem = pMenu->FindItemByPosition(j);
        // recursively walk down to deepest submenu
        if ( pMenuItem->GetSubMenu() )
            FindMenuDuplicateItems( pMenuItem->GetSubMenu(), rStr, rCount );
        //---------------------------
        // Now at deepest menu items
        //---------------------------
        // skip separater menu items
        if (pMenuItem->GetKind() == wxITEM_SEPARATOR) continue;
        //int nMenuItemID = pMenuItem->GetId();

        // Skip any menu items beginning with numerics
        if (wxMenuCmd::IsNumericMenuItem(pMenuItem)) continue;

        // Find matching menu item in keybinder array of commands
        // wxString menuItemLabel = pMenuItem->GetItemLabelText().Trim();
        if (rStr == pMenuItem->GetItemLabelText().Trim() )
        {    rCount++;
            #if defined(LOGGING)
             wxLogMessage( _T("Duplicate menu item [%d][%s]"), pMenuItem->GetId(), pMenuItem->GetItemLabelText().GetData()  );
            #endif
        }
    }//for
    return rCount;
}//mergeSubmenu

// ----------------------------------------------------------------------------
wxString GetFullMenuPath(int id)
// ----------------------------------------------------------------------------
{
    // fetch the full menu path from menu structure via a menu id
    // It will look like "File\\Open\\Recent Files"
    // or "" on failure

    wxString fullMenuPath = wxEmptyString;
    wxMenuBar* pbar = wxMenuCmd::m_pMenuBar;
    wxMenu* pMenu = 0;

    // try to find the menu item
    wxMenuItem* pMenuItem = pbar->FindItem(id, &pMenu);
    if ( pMenuItem == NULL ) return fullMenuPath;

    // fetch wxMenuItem label
    fullMenuPath = pMenuItem->GetItemLabelText().Trim();

    //wxLogMessage( _T("fullMenuPath[%s]"), fullMenuPath.c_str() );
    // get parent menu of the wxMenuItem
    wxMenu* pParentMenu = pMenu->GetParent();

    // prepend menu labels by iterating upwards through the menu structure
    while (pParentMenu)
    {    for (int i=0; i < (int)pParentMenu->GetMenuItemCount(); i++)
        {
            wxMenuItem* pitem = pParentMenu->GetMenuItems().Item(i)->GetData();
            if (pitem->GetSubMenu() && (pitem->GetSubMenu()== pMenu ))
            {
                fullMenuPath.Prepend( pitem->GetItemLabelText().Trim() + wxT("\\"));
                //wxLogMessage( _T("ParentMenu[%s]"),pitem->GetLabel().c_str() );
                break;
            }
        }
        pMenu = pParentMenu;
        pParentMenu = pParentMenu->GetParent();
    }//while

    // prepend last parent from menu bar
    for (int i=0; i<(int)pbar->GetMenuCount() ;++i )
    {
        wxMenu* pBarMenu = pbar->GetMenu(i);
        if ( pBarMenu == pMenu)
        {
            fullMenuPath.Prepend( pbar->GetMenuLabel(i) + wxT("\\"));
            //wxLogMessage( _T("ParentMenu[%s]"),pbar->GetLabelTop(i).c_str() );
        }
    }

    //wxLogMessage( _T("fullPath[%s]"), fullMenuPath.c_str() );

    return fullMenuPath;
}

namespace
{

// ----------------------------------------------------------------------------
int FindMenuIdUsingFullMenuPath( const wxString& sFullMenuPath )
// ----------------------------------------------------------------------------
{
    // verify sFullMenuPath and return the menuitem id of its last level
    // the path is in the form "File\Open\Recent Files\File01" etc.
    // like a file path.

    if ( sFullMenuPath.IsEmpty() ) return wxNOT_FOUND;
    #if defined(LOGGING)
    wxLogMessage( _T("FindMenuIdUsingFullMenuPath[%s]"), sFullMenuPath.c_str() );
    #endif
    wxMenuBar* pMenuBar = wxMenuCmd::m_pMenuBar;
    int id = wxNOT_FOUND;
    int menuIndex = wxNOT_FOUND;

    wxString fullMenuPath = sFullMenuPath;
    int levelCount = fullMenuPath.Freq(wxT('\\'))+1;
    wxArrayString levels;

    // break the full menu path string into levels
    for ( int i=0; i < levelCount; ++i )
    {
        levels.Add( fullMenuPath.BeforeFirst(wxT('\\')) );
        fullMenuPath.Remove(0, levels[i].Length()+1 );
        levels[i].Trim();
        //wxLogMessage( _T("level[%d][%s]"), i, levels[i].c_str() );
    }
    // start searching from the menubar level
    if ( wxNOT_FOUND == (menuIndex = pMenuBar->FindMenu( levels[0]) ))
        return wxNOT_FOUND;
    wxMenu* pMenu = pMenuBar->GetMenu(menuIndex);
    wxMenuItem* pMenuItem = 0;
    bool found = false;

    // find and compare file key path levels to each level of the actual menu
    for (int i=1; i < (int)levels.GetCount(); ++i)
    {
        #if defined(LOGGING)
        wxLogMessage( _T("Searching for Level[%d][%s]"), i, levels[i].wx_str() );
        #endif
        if (not pMenu) return wxNOT_FOUND;
        found = false;
        for (int j=0; j < (int)pMenu->GetMenuItemCount(); ++j )
        {
            pMenuItem = pMenu->FindItemByPosition(j);
            //wxLogMessage( _T("MenuItem[%d][%s]"), j, pMenuItem->GetLabel().c_str() );
            if ( pMenuItem->GetItemLabelText().Trim() == levels[i])
            {   menuIndex = j;
                pMenu = pMenuItem->GetSubMenu();
                found = true;
                #if defined(LOGGING)
                    wxLogMessage( _T("Found menuItem [%s]"), pMenuItem->GetItemLabelText().c_str() );
                #endif
                break;
            }
        }//for j
        // if menu entry found, look at next level
        if ( found) continue;
        else return wxNOT_FOUND;
    }//for i

    if (found) id = pMenuItem->GetId();
    else id = wxNOT_FOUND;

    return id;

}//FindMenuIdUsingFullMenuPath
}
// ----------------------------------------------------------------------------

/*
#ifdef __WXGTK__

    #include "wx/gtk/private.h"
    #include <gdk/gdkkeysyms.h>

#endif
*/


// ----------------------------------------------------------------------------
// wxMenuCmd
// ----------------------------------------------------------------------------

#if defined( __WXGTK__) || defined(__WXMAC__)
// ----------------------------------------------------------------------------
void wxMenuCmd::Update(wxMenuItem* pSpecificMenuItem) //for __WXGTK__
// ----------------------------------------------------------------------------
{
//    //+v0.4.11
//    // verify menu item has not changed its id or disappeared
//    if (m_pMenuBar->FindItem(m_nId) != m_pItem)
//        return;
    //v0.4.17
    // Test if caller wants a different menu item other than in keybinder array
    wxMenuItem* pLclMnuItem = m_pItem;
    if (pSpecificMenuItem) pLclMnuItem = pSpecificMenuItem;
    //+v0.4
    // verify menu item has not changed its id or disappeared
    if (not pSpecificMenuItem)
        if (m_pMenuBar->FindItem(m_nId) != pLclMnuItem)
            return;


    //+v0.4.11
    // leave numeric menu items alone. They get replaced by CodeBlocks
    if (IsNumericMenuItem(pLclMnuItem))
      return;

    wxString strText = pLclMnuItem->GetItemLabel();

    // *bug* 2007/01/19 v1.0.15
    // Dont use  GetLabel to re-establish the menu text. It doesn't
    // contain the underlined mnemonic. Use GetText()
    //-wxString str = pLclMnuItem->GetLabel()

    wxString str = strText.BeforeFirst('\t');
     // GTK is substituting '&' with an underscore
    int idx = 0;
    // change the first underscore to an & mnemonic, all others to blank
    if ( -1 != (idx = str.Find('_'))) str[idx] = '&';
    for ( size_t i=0; i<str.Length(); ++i)
        if ( str[i]=='_'){ str[i] = ' ';}
    #if defined(LOGGING)
     wxLogMessage( _T("Updating menu item Label[%s]Text[%s]id[%d]"), str.wx_str(), strText.wx_str(), pLclMnuItem->GetId() );
    #endif


    // on GTK, an optimization in wxMenu::SetText checks
    // if the new label is identical to the old and in this
    // case, it returns without doing anything... :-(
    // to solve the problem, a space is added or removed
    // from the label to override this optimization check
    str.Trim();
    if (m_nShortcuts <= 0)
    {
        #if defined(LOGGING)
        wxLogMessage(_("wxMenuCmd::Update - no shortcuts defined for [%s]"), str.c_str());
        #endif

        // no more shortcuts for this menuitem: SetText()
        // will delete the hotkeys associated...
        pLclMnuItem->SetItemLabel(str);
        return;
    }

    wxString newtext = str+wxT("\t")+GetShortcut(0)->GetStr();
    #if defined(LOGGING)
    wxLogMessage(_("wxMenuCmd::Update - setting the new text to [%s]"), newtext.c_str());
    #endif

    // on GTK, the SetAccel() function doesn't have any effect...
    pLclMnuItem->SetItemLabel(newtext);
#ifdef __WXGTK20__

    //   gtk_menu_item_set_accel_path(GTK_MENU_ITEM(m_pItem), wxGTK_CONV(newtext));

#endif
}
#endif //update for __WXGTK__
// ----------------------------------------------------------------------------

#if defined( __WXMSW__ )
// ----------------------------------------------------------------------------
void wxMenuCmd::Update(wxMenuItem* pSpecificMenuItem) // for __WXMSW__
// ----------------------------------------------------------------------------
{
    wxMenuItem* pLclMnuItem = m_pItem;

    // Test if caller wants a different menu item than in wxCmd item
    if (pSpecificMenuItem)
        pLclMnuItem = pSpecificMenuItem;

    // verify menu item has not changed its id or disappeared
    if ( NULL == m_pMenuBar->FindItem(m_nId) )
        return;
    // if using wxCmd item, and its not a program menu item, punt
    // this happens when a plugin deletes a menu item
    if (not pSpecificMenuItem)
        if (m_pMenuBar->FindItem(m_nId) != pLclMnuItem)
            return;

    // leave numeric menu items alone. They get replaced by CodeBlocks
    if (IsNumericMenuItem(pLclMnuItem))
      return;

    wxString strText = pLclMnuItem->GetItemLabel();
    //use full text to get label in order to preserve mnemonics/accelerators
    wxString strLabel = strText.BeforeFirst(_T('\t'));
    wxString newtext = strLabel; //no accel, contains mnemonic

    wxAcceleratorEntry* pItemAccel = pLclMnuItem->GetAccel();
    // clearing previous shortcuts if none now assigned
    if (m_nShortcuts <= 0) {
        if ( ! pItemAccel) return;
        #if LOGGING
         wxLogMessage(_("wxMenuCmd::Update - Removing shortcuts [%d][%s] for [%d][%s]"),pLclMnuItem->GetId(), strText.wx_str(), m_nId, newtext.wx_str());
        #endif
        // set "non bitmapped" text to preserve menu width
        pLclMnuItem->SetItemLabel(newtext);
        //now rebuild the menuitem if bitmapped
        if (pLclMnuItem->GetBitmap().GetWidth())
            pLclMnuItem = RebuildMenuitem(pLclMnuItem); //+v0.4.6
        return;
    }

    //make new Label+Accelerator string
    newtext = strLabel+wxT("\t")+GetShortcut(0)->GetStr();

    // change the accelerator...but only if it has changed
    wxAcceleratorEntry* pPrfAccel = wxAcceleratorEntry::Create(newtext);
    if ( ! pPrfAccel) return;
    if ( pItemAccel
         && ( pItemAccel->GetFlags() == pPrfAccel->GetFlags() )
         && ( pItemAccel->GetKeyCode() == pPrfAccel->GetKeyCode() ) )
         return;
    #if LOGGING
     wxLogMessage(_("wxMenuCmd::Update - Setting shortcuts for [%d][%s]"), pLclMnuItem->GetId(), newtext.wx_str());
    #endif
    pLclMnuItem->SetItemLabel(newtext);
    //now rebuild the menuitem if bitmapped
    if (pLclMnuItem->GetBitmap().GetWidth())
        pLclMnuItem = RebuildMenuitem(pLclMnuItem); //+v0.4.6

}//Update
//// ----------------------------------------------------------------------------
//// RebuildMenuitem
//// ----------------------------------------------------------------------------
////wxMenuItem* wxMenuCmd::RebuildMenuitem(wxMenuItem* pMnuItem)
////{//+v0.4.25 WXMSW
////   // Since wxWidgets 2.6.3, we don't have to rebuild the menuitem
////   // to preserve the bitmapped menu icon.
////    return pMnuItem;
////
////}//RebuildMenuitem

// ----------------------------------------------------------------------------
// The following routine was used when wxWidgets would not SetText()
// without clobbering the menu Bitmap icon
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
wxMenuItem* wxMenuCmd::RebuildMenuitem(wxMenuItem* pMnuItem)
// ----------------------------------------------------------------------------
{//Reinstated v1.0.13 2006/12/30 for wx2.6.3 w/fixes and wx2.8.0
 // which now cause the same problem as 2.6.2
    // ---------------------------------------------------------------
    //  Do it the slow/hard way, remove and delete the menu item
    // ---------------------------------------------------------------
    wxMenu* pMenu = pMnuItem->GetMenu();
    wxMenuItemList items = pMenu->GetMenuItems();
    int pos = items.IndexOf(pMnuItem);
   // rebuild the menuitem
    wxMenuItem* pnewitem = new wxMenuItem(pMenu, m_nId,
                pMnuItem->GetItemLabel(),
                pMnuItem->GetHelp(), pMnuItem->GetKind(),
                pMnuItem->GetSubMenu() );
    pnewitem->SetBitmap(pMnuItem->GetBitmap() );
    pnewitem->SetFont(pMnuItem->GetFont() );
   #if wxUSE_OWNER_DRAWN    //TimS 2006/12/30 v1.0.13
    if ( pMnuItem->IsOwnerDrawn() )
    {
        pnewitem->SetOwnerDrawn(true);
        pnewitem->SetMarginWidth(pMnuItem->GetMarginWidth());
        pnewitem->SetDisabledBitmap(pMnuItem->GetDisabledBitmap());
        if (pMnuItem->IsCheckable())
        {
            pnewitem->SetCheckable(true);
            pnewitem->SetBitmaps(pMnuItem->GetBitmap(true), pMnuItem->GetBitmap(false));
        }
    }
   #endif
    // remove the menuitem
    pMenu->Destroy(pMnuItem);
    // update keybinder array menu item pointer
    m_pItem = pnewitem;
    // put the menuitem back on the menu
    pMenu->Insert(pos, pnewitem);
    return pnewitem;

}//RebuildMenuitem
#endif //#if defined( __WXMSW__ )
// ----------------------------------------------------------------------------
bool wxMenuCmd::IsNumericMenuItem(wxMenuItem* pwxMenuItem)   //v0.2
// ----------------------------------------------------------------------------
{//v0.2
    wxString str = pwxMenuItem->GetItemLabel();
    if (str.Length() <2) return false;
    if (str.Left(1).IsNumber()) return true;
    if ( (str[0] == '&') && (str.Mid(1,1).IsNumber()) )
        return true;
    if ( (str[0] == '_') && (str.Mid(1,1).IsNumber()) )
        return true;
    return false;
}//IsNumericMeuItem
// ----------------------------------------------------------------------------
void wxMenuCmd::Exec(wxObject *origin, wxEvtHandler *client)
// ----------------------------------------------------------------------------
{
    wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED, GetId());
    wxASSERT_MSG(client, wxT("An empty client handler ?!?"));

    // set up the event and process it...
    menuEvent.SetEventObject(origin);

    // 2007/08/12
    // Had to change AddPendingEvent() to ProcessEvent() to avoid crashes
    // when secondary command key would close editors wx284
    ////client->AddPendingEvent(menuEvent);//ProcessEvent(menuEvent);

    client->ProcessEvent(menuEvent);//ProcessEvent(menuEvent);
}

//// ----------------------------------------------------------------------------
////wxCmd *wxMenuCmd::CreateNew(int id)
//// ----------------------------------------------------------------------------
////{-v0.3
////    if (!m_pMenuBar) return NULL;
////
////    // search the menuitem which is tied to the given ID
////    wxMenuItem *p = m_pMenuBar->FindItem(id);
////
////    if (!p) return NULL;
////    wxASSERT(id == p->GetId());
////    return new wxMenuCmd(p);
////}
// --+v0.3---------------------------------------------------------------------
wxCmd *wxMenuCmd::CreateNew(wxString sCmdName, int id)
// ----------------------------------------------------------------------------
{
    if (!m_pMenuBar) return NULL;

    // search for a matching menu item
    // CodeBlocks has dynamic (shifty) menu item id's
    // so the file loaded item may have a different/stale item id.

    wxMenuItem* pMenuItem = 0;
    wxString fullMenuPath = sCmdName;       //(2007/6/15)
    wxString cmdName = fullMenuPath.AfterLast(wxT('\\'));
    cmdName.Trim();
    int actualMenuID = id;

    // Try to match id and label to avoid duplicate named menu items
    wxMenuItem* pMenuItemByCfgId = m_pMenuBar->FindItem(id);
    if ( pMenuItemByCfgId && (pMenuItemByCfgId->GetItemLabelText().Trim() == cmdName) )
        pMenuItem = pMenuItemByCfgId;
    else
    {   // didn't find the menu id with the specified cmd name.
        #if defined(LOGGING)
            wxLogMessage( _T("CreateNew() Unmatched id[%d][%s]"), id, cmdName.GetData() );
        #endif
        actualMenuID = FindMenuIdUsingFullMenuPath( fullMenuPath ) ;
        if (wxFound(actualMenuID) )
            pMenuItem = m_pMenuBar->FindItem( actualMenuID );
        #if defined(LOGGING)
        else
            wxLogMessage( _T("CreateNew() UnFound id[%d][%s]"), id, cmdName.GetData() );
        #endif

    }//end else

    if (not pMenuItem)
    {
        wxLogDebug(_("KeyBinder:CreateNew() not created[%d][%s]"), id, cmdName.GetData());
        // ... also an untranslated msg so I can read it.
        wxLogDebug(_T("KeyBinder:CreateNew() not created[%d][%s]"), id, cmdName.GetData());
        return NULL;
    }

    //-wxASSERT(id == p->GetId());
    //wxLogMessage( _T("CreatingNew for [%d][%s]"), actualMenuID, cmdName.GetData() );

    //-return new wxMenuCmd(pMenuItem); //(2019/03/5)-
    return new wxMenuCmd(pMenuItem, cmdName, pMenuItem->GetHelp()); //(2019/03/5)
}

// ****************************************************************************
//                          wxMenuWalker
// ****************************************************************************

// ----------------------------------------------------------------------------
bool wxMenuWalker::IsNumericMenuItem(wxMenuItem* pwxMenuItem)   //v0.2
// ----------------------------------------------------------------------------
{//v0.2
    wxString str = pwxMenuItem->GetItemLabel();
    if (str.Length() <2) return false;
    if (str.Left(1).IsNumber()) return true;
    if ( (str[0] == '&') && (str.Mid(1,1).IsNumber()) )
        return true;
    if ( (str[0] == '_') && (str.Mid(1,1).IsNumber()) )
        return true;
    return false;
}
// ----------------------------------------------------------------------------
void wxMenuWalker::WalkMenuItem(wxMenuBar* p, wxMenuItem* m, void* data)
// ----------------------------------------------------------------------------
{
    //dont fool with itemized filenames, GetLabel cant handle file slashes //v0.2
    if (IsNumericMenuItem(m)) return;   //v0.2

    //wxLogMessage(_("wxMenuWalker::WalkMenuItem - walking on [%s] at level [%d]"),
    //          m->GetLabel().c_str(), m_nLevel);

    void* tmp = OnMenuItemWalk(p, m, data);

    if (m->GetSubMenu())
    {
        // if this item contains a sub menu, add recursively the menu items
        // of that sub menu... using the cookie from OnMenuItemWalk.
        //wxLogMessage(_("wxMenuWalker::WalkMenuItem - recursing on [%s]"), m->GetLabel().c_str());

        m_nLevel++;
        WalkMenu(p, m->GetSubMenu(), tmp);
        OnMenuExit(p, m->GetSubMenu(), tmp);
        m_nLevel--;
    }

    // we can delete the cookie we got form OnMenuItemWalk
    DeleteData(tmp);
}

// ----------------------------------------------------------------------------
void wxMenuWalker::WalkMenu(wxMenuBar* p, wxMenu* m, void* data)
// ----------------------------------------------------------------------------
{

    //wxLogMessage(_("wxMenuWalker::WalkMenu - walking on [%s] at level [%d]"),
    //          m->GetTitle().c_str(), m_nLevel);

    for (int i=0; i < (int)m->GetMenuItemCount(); i++)
    {
        wxMenuItem* pitem = m->GetMenuItems().Item(i)->GetData();

        // inform the derived class that we have reached a menu
        // and get the cookie to give on WalkMenuItem...
        void* tmp = OnMenuWalk(p, m, data);

        // skip separators (on wxMSW they are marked as wxITEM_NORMAL
        // but they do have empty labels)...
        if (pitem->GetKind() != wxITEM_SEPARATOR &&
            pitem->GetItemLabelText().Trim() != wxEmptyString)
            WalkMenuItem(p, pitem, tmp);

        // the cookie we gave to WalkMenuItem is not useful anymore
        DeleteData(tmp);
    }

    OnMenuExit(p, m, data);
}

// ----------------------------------------------------------------------------
void wxMenuWalker::Walk(wxMenuBar* pMnuBar, void *data)
// ----------------------------------------------------------------------------
{
    wxASSERT(pMnuBar);

    for (int i=0; i < (int)pMnuBar->GetMenuCount(); i++)
    {

        // create a new tree branch for the i-th menu of this menubar
        wxMenu* pMnu = pMnuBar->GetMenu(i);

        m_nLevel++;
        //wxLogMessage(_("wxMenuWalker::Walk - walking on [%s] at level [%d]"),
        //          p->GetLabelTop(i).c_str(), m_nLevel);

        void* tmp = OnMenuWalk(pMnuBar, pMnu, data);

        // and fill it...
        WalkMenu(pMnuBar, pMnu, tmp);
        m_nLevel--;

        DeleteData(tmp);
    }
}
// ****************************************************************************
//                          wxMenuTreeWalker
// ****************************************************************************
// ----------------------------------------------------------------------------
void wxMenuTreeWalker::FillTreeBranch(wxMenuBar* pMnuBar, wxTreeCtrl* pTreectrl, wxTreeItemId branch)
// ----------------------------------------------------------------------------
{
    // these will be used in the recursive functions...
    m_root = branch;
    m_pTreeCtrl = pTreectrl;

    // be sure that the given tree item is empty...
    m_pTreeCtrl->DeleteChildren(branch);

    // ...start !!!
    Walk(pMnuBar, &branch);
}

// ----------------------------------------------------------------------------
void* wxMenuTreeWalker::OnMenuWalk(wxMenuBar *p, wxMenu *m, void *data)
// ----------------------------------------------------------------------------
{
    wxTreeItemId* id = (wxTreeItemId *)data;
    int i;

    // if we receive an invalid tree item ID, we must stop everything...
    // (in fact a NULL value given as DATA in wxMenuTreeWalker function
    // implies the immediate processing stop)...
    if (!id->IsOk())
        return NULL;

    // if this is the first level of menus, we must create a new tree item
    if (*id == m_root) {

        // find the index of the given menu
        for (i=0; i < (int)p->GetMenuCount(); i++)
            if (p->GetMenu(i) == m)
                break;
        wxASSERT(i != (int)p->GetMenuCount());

        // and append a new tree branch with the appropriate label
        wxTreeItemId newId = m_pTreeCtrl->AppendItem(*id,
            wxMenuItem::GetLabelText(p->GetMenuLabel(i)));

        // menu items contained in the given menu must be added
        // to the just created branch
        return new wxTreeItemId(newId);
    }

    // menu items contained in the given menu must be added
    // to this same branch...
    return new wxTreeItemId(*id);
}
// ----------------------------------------------------------------------------
void* wxMenuTreeWalker::OnMenuItemWalk(wxMenuBar *, wxMenuItem *m, void *data)
// ----------------------------------------------------------------------------
{
    wxTreeItemId* id = (wxTreeItemId *)data;
    if (id->IsOk()) {

        // to each tree branch attach a wxTreeItemData containing
        // the ID of the menuitem which it represents...
        wxExTreeItemData* treedata = new wxExTreeItemData(m->GetId());

        // create the new item in the tree ctrl
        wxTreeItemId newId = m_pTreeCtrl->AppendItem(*id,
            m->GetItemLabelText().Trim(), -1, -1, treedata);

        return new wxTreeItemId(newId);
    }

    return NULL;
}

// ----------------------------------------------------------------------------
void wxMenuTreeWalker::DeleteData(void *data)
// ----------------------------------------------------------------------------
{
    wxTreeItemId* p = (wxTreeItemId *)data;
    if (p) delete p;
}

// ****************************************************************************
//                       wxMenuComboListWalker
// ****************************************************************************
// ----------------------------------------------------------------------------
void wxMenuComboListWalker::FillComboListCtrl(wxMenuBar *p, wxComboBox *combo)
// ----------------------------------------------------------------------------
{
    // these will be used in the recursive functions...
    m_pCategories = combo;

    // be sure that the given tree item is empty...
    m_pCategories->Clear();

    // ...start !!!
    Walk(p, NULL);
}

// ----------------------------------------------------------------------------
void *wxMenuComboListWalker::OnMenuWalk(wxMenuBar *p, wxMenu *m, void *)
// ----------------------------------------------------------------------------
{
    //wxLogMessage(_("wxMenuWalker::OnMenuWalk - walking on [%s]"), m->GetTitle().c_str());

    wxString toadd;

    // find the index of the given menu
    if (m_strAcc.IsEmpty()) {

        int i;
        for (i=0; i < (int)p->GetMenuCount(); i++)
            if (p->GetMenu(i) == m)
                break;
        wxASSERT(i != (int)p->GetMenuCount());
        toadd = wxMenuItem::GetLabelText(p->GetMenuLabel(i));
        m_strAcc = toadd;

    } else {

        //toadd = m->GetTitle();
        toadd = m_strAcc;
        //wxString str((wxString)()acc);
        //m_strAcc += str;
    }

    //int last = m_pCategories->GetCount()-1;
    int found;
    if ((found = m_pCategories->FindString(toadd)) != wxNOT_FOUND)
        return m_pCategories->GetClientObject(found);

    // create the clientdata that our new combobox item will contain
    wxClientData* cd = new wxExComboItemData();

    // and create a new element in our combbox
    //wxLogMessage(_("wxMenuComboListWalker::OnMenuWalk - appending [%s]"), toadd.c_str());

    m_pCategories->Append(toadd, cd);
    return cd;
}

// ----------------------------------------------------------------------------
void *wxMenuComboListWalker::OnMenuItemWalk(wxMenuBar *, wxMenuItem *m, void *data)
// ----------------------------------------------------------------------------
{
    //wxLogMessage(_("wxMenuWalker::OnMenuItemWalk - walking on [%s]"), m->GetLabel().c_str());

    //int last = m_pCategories->GetCount()-1;
    wxExComboItemData *p = (wxExComboItemData *)data;//m_pCategories->GetClientObject(last);

    // append a new item
    if (m->GetSubMenu() == NULL)
        p->Append(m->GetItemLabelText().Trim(), m->GetId());
    else
        m_strAcc += wxT(" | ") + m->GetItemLabelText().Trim();

    // no info to give to wxMenuComboListWalker::OnMenuWalk
    return NULL;//(void *)str;
}

// ----------------------------------------------------------------------------
void wxMenuComboListWalker::OnMenuExit(wxMenuBar *, wxMenu * /*m*/, void *)
// ----------------------------------------------------------------------------
{
    //-wxLogDebug(wxT("wxMenuWalker::OnMenuExit - walking on [%s]"), m->GetTitle().c_str());
    //-wxLogDebug("\n");

    if (!m_strAcc.IsEmpty()){// && m_strAcc.Right() == str) {

        int diff = m_strAcc.Find(wxT('|'), TRUE);

        if (diff == wxNOT_FOUND)
            m_strAcc = wxEmptyString;
        else
            m_strAcc = m_strAcc.Left(diff);
        m_strAcc.Trim();
    }
}

// ----------------------------------------------------------------------------
void wxMenuComboListWalker::DeleteData(void *)
// ----------------------------------------------------------------------------
{ /* we need NOT TO DELETE the given pointer !! */
}

// ***************************************************************************
//                  wxMenuShortcutWalker
// ***************************************************************************
// ----------------------------------------------------------------------------
void *wxMenuShortcutWalker::OnMenuItemWalk(wxMenuBar *, wxMenuItem *m, void *)
// ----------------------------------------------------------------------------
{
    wxASSERT(m);

    // add an entry to the command array
    wxCmd *cmd = new wxMenuCmd(m, m->GetItemLabelText().Trim(), m->GetHelp());
    m_pArr->Add(cmd);

    // check for shortcuts
    wxAcceleratorEntry* a = m->GetAccel();      // returns a pointer which we have to delete
    if (a) {

        // this menuitem has an associated accelerator... add an entry
        // to the array of bindings for the relative command...
        cmd->AddShortcut(a->GetFlags(), a->GetKeyCode());
    }

    // cleanup
    if (a) delete a;
    return NULL;
}

// ----------------------------------------------------------------------------
void wxMenuShortcutWalker::DeleteData(void *
#ifdef __WXDEBUG__
                                      data
#endif  // to avoid warnings about unused arg
                                      )
// ----------------------------------------------------------------------------
{
    wxASSERT_MSG(data == NULL,
                wxT("wxMenuShortcutWalker does not use the 'data' parameter")
        );
}
// ----------------------------------------------------------------------------

