//////////////////////////////////////////////////////////////////////////////
//
// Copyright            : (C) 2015 Eran Ifrah
// File name            : clKeyboardManager.h
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
// Modifed for Code::Blocks by pecan
//
#include <vector>
#include <algorithm>

#include <wx/app.h>
#include <wx/menu.h>
#include <wx/xrc/xmlres.h>
#include <wx/log.h>
#include <algorithm>
#include <wx/tokenzr.h>
#include <wx/log.h>
#include <wx/ffile.h>   //(2019/04/3)
#include <wx/textfile.h>    //( 2019/10/26)

#include "manager.h"
#include "personalitymanager.h"
#include "annoyingdialog.h" //(2019/04/27)
#include "logmanager.h"     //2020/04/6
#include "debugging.h" //(2019/05/3)
#include "clKeyboardManager.h"
#include "clKeyboardBindingConfig.h"

namespace{
    wxString sep = wxFileName::GetPathSeparator();
    int frameKnt = 0;
}
    //-int wxEVT_INIT_DONE = XRCID("wxEVT_INIT_DONE");

BEGIN_EVENT_TABLE( clKeyboardManager, wxEvtHandler )
    //-EVT_MENU( wxEVT_INIT_DONE, clKeyboardManager::OnStartupCompleted )
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
void clKeyboardShortcut::Clear()
// ----------------------------------------------------------------------------
{
    m_ctrl = false;
    m_alt = false;
    m_shift = false;
    m_keyCode.Clear();
}
// ----------------------------------------------------------------------------
wxString clKeyboardShortcut::ToString() const
// ----------------------------------------------------------------------------
{
    // An accelerator must contain a key code
    if(m_keyCode.IsEmpty()) {
        return _T("");
    }

    wxString str;
    if(m_ctrl) {
        str << _T("Ctrl-");
    }
    if(m_alt) {
        str << _T("Alt-");
    }
    if(m_shift) {
        str << _T("Shift-");
    }
    str << m_keyCode;
    return str;
}
// ----------------------------------------------------------------------------
void clKeyboardShortcut::FromString(const wxString& accelString)
// ----------------------------------------------------------------------------
{
    Clear();
    wxArrayString tokens = ::wxStringTokenize(accelString, _T("-+"), wxTOKEN_STRTOK);
    for(size_t i = 0; i < tokens.GetCount(); ++i) {
        wxString token = tokens.Item(i);
        token.MakeLower();
        if(token == _T("shift")) {
            m_shift = true;
        } else if(token == _T("alt")) {
            m_alt = true;
        } else if(token == _T("ctrl")) {
            m_ctrl = true;
        } else {
            m_keyCode = tokens.Item(i);
        }
    }
}

// ----------------------------------------------------------------------------
clKeyboardManager::clKeyboardManager()
// ----------------------------------------------------------------------------
{
    // A-Z
    for(size_t i = 65; i < 91; ++i) {
        char asciiCode = (char)i;
        m_keyCodes.insert(wxString() << asciiCode);
    }

    // 0-9
    for(size_t i = 48; i < 58; ++i) {
        char asciiCode = (char)i;
        m_keyCodes.insert(wxString() << asciiCode);
    }

    // Special chars
    m_keyCodes.insert(_T("`"));
    m_keyCodes.insert(_T("-"));
    m_keyCodes.insert(_T("*"));
    m_keyCodes.insert(_T("="));
    m_keyCodes.insert(_T("BACK"));
    m_keyCodes.insert(_T("TAB"));
    m_keyCodes.insert(_T("["));
    m_keyCodes.insert(_T("]"));
    m_keyCodes.insert(_T("ENTER"));
    m_keyCodes.insert(_T("CAPITAL"));
    m_keyCodes.insert(_T("SCROLL_LOCK"));
    m_keyCodes.insert(_T("PASUE"));
    m_keyCodes.insert(_T(";"));
    m_keyCodes.insert(_T("'"));
    m_keyCodes.insert(_T("\\"));
    m_keyCodes.insert(_T(","));
    m_keyCodes.insert(_T("."));
    m_keyCodes.insert(_T("/"));
    m_keyCodes.insert(_T("SPACE"));
    m_keyCodes.insert(_T("INS"));
    m_keyCodes.insert(_T("HOME"));
    m_keyCodes.insert(_T("PGUP"));
    m_keyCodes.insert(_T("PGDN"));
    m_keyCodes.insert(_T("DEL"));
    m_keyCodes.insert(_T("END"));
    m_keyCodes.insert(_T("UP"));
    m_keyCodes.insert(_T("DOWN"));
    m_keyCodes.insert(_T("RIGHT"));
    m_keyCodes.insert(_T("LEFT"));
    m_keyCodes.insert(_T("F1"));
    m_keyCodes.insert(_T("F2"));
    m_keyCodes.insert(_T("F3"));
    m_keyCodes.insert(_T("F4"));
    m_keyCodes.insert(_T("F5"));
    m_keyCodes.insert(_T("F6"));
    m_keyCodes.insert(_T("F7"));
    m_keyCodes.insert(_T("F8"));
    m_keyCodes.insert(_T("F9"));
    m_keyCodes.insert(_T("F10"));
    m_keyCodes.insert(_T("F11"));
    m_keyCodes.insert(_T("F12"));

    // There can be the following options:
    // Ctrl-Alt-Shift
    // Ctrl-Alt
    // Ctrl-Shift
    // Ctrl
    // Alt-Shift
    // Alt
    // Shift
    std::for_each(m_keyCodes.begin(), m_keyCodes.end(), [&](const wxString& keyCode) {
        m_allShorcuts.insert(_T("Ctrl-Alt-Shift-") + keyCode);
        m_allShorcuts.insert(_T("Ctrl-Alt-") + keyCode);
        m_allShorcuts.insert(_T("Ctrl-Shift-") + keyCode);
        m_allShorcuts.insert(_T("Ctrl-") + keyCode);
        m_allShorcuts.insert(_T("Alt-Shift-") + keyCode);
        m_allShorcuts.insert(_T("Alt-") + keyCode);
        m_allShorcuts.insert(_T("Shift-") + keyCode);
    });
}

// ----------------------------------------------------------------------------
clKeyboardManager::~clKeyboardManager()
// ----------------------------------------------------------------------------
{
    // Here on App Shutdown
    // Final save of the Key bindings
    Save();
}

static clKeyboardManager* m_mgr = NULL;
// ----------------------------------------------------------------------------
clKeyboardManager* clKeyboardManager::Get()
// ----------------------------------------------------------------------------
{   // breaking/stepping gdb prior to 8.0 will crash here //(2019/05/3)
    if(NULL == m_mgr) {
        m_mgr = new clKeyboardManager();
    }
    return m_mgr;
}

// ----------------------------------------------------------------------------
void clKeyboardManager::Release()
// ----------------------------------------------------------------------------
{
    if(m_mgr) {
        delete m_mgr;
    }
    m_mgr = NULL;
}

// ----------------------------------------------------------------------------
void clKeyboardManager::DoGetFrames(wxFrame* parent, clKeyboardManager::FrameList_t& frames)
// ----------------------------------------------------------------------------
{
    frames.push_back(parent);
    const wxWindowList& children = parent->GetChildren();
    wxWindowList::const_iterator iter = children.begin();
    for(; iter != children.end(); ++iter) {
        wxFrame* frameChild = dynamic_cast<wxFrame*>(*iter);
        if(frameChild) {
            if(std::find(frames.begin(), frames.end(), frameChild) == frames.end()) {
                frames.push_back(frameChild);
                DoGetFrames(frameChild, frames);
            }
        }
    }
}
// ----------------------------------------------------------------------------
void clKeyboardManager::DoUpdateMenu(wxMenu* menu, MenuItemDataVec_t& accels, std::vector<wxAcceleratorEntry>& table)
// ----------------------------------------------------------------------------
{
    wxMenuItemList items = menu->GetMenuItems();
    wxMenuItemList::iterator iter = items.begin();
    for(; iter != items.end(); iter++) {
        wxMenuItem* item = *iter;
        if(item->GetSubMenu())
        {
            DoUpdateMenu(item->GetSubMenu(), accels, table);
            continue;
        }

        MenuItemData* pMenuItemData = FindMenuTableEntryByID(accels, item->GetId()); //(ph 2023/03/07)
        if(pMenuItemData)
        {
            wxString itemText = item->GetItemLabel();
            // remove the old shortcut
            itemText = itemText.BeforeFirst('\t');
            itemText << _T("\t") << pMenuItemData->accel;

            // Replace the item text (mnemonics + accel + label)
            item->SetItemLabel(itemText);

            // remove the matching entry from the accels map
            // Get an iterator to the MenuItemData pointer
            MenuItemDataVec_t::iterator it = accels.begin() + std::distance(accels.data(), pMenuItemData);
            if (it != accels.end())
                accels.erase(it);
        }

        //(2019/06/29) Linux: set menu accels in global table, else linux menu accels wont work.
        // If menu item with accelerator within the label,
        // create accelerator corresponding to the specified string, return NULL if
        // string couldn't be parsed or a pointer to be deleted by the caller.
        // Otherwise, parse out flags and keycodes following the '/t' in the menu item label
        wxAcceleratorEntry* a = wxAcceleratorEntry::Create(item->GetItemLabel());
        if(a)
        {
            a->Set(a->GetFlags(), a->GetKeyCode(), item->GetId());
            table.push_back(*a);
            wxDELETE(a);
        }

    }//for iter
}
// ----------------------------------------------------------------------------
void clKeyboardManager::DoUpdateFrame(wxFrame* frame, MenuItemDataVec_t& accels)
// ----------------------------------------------------------------------------
{
    std::vector<wxAcceleratorEntry> table;

    // Update menus. If a match is found remove it from the 'accel' table
    wxMenuBar* menuBar = frame->GetMenuBar();
    if(!menuBar) return;
    for(size_t i = 0; i < menuBar->GetMenuCount(); ++i)
    {
        wxMenu* menu = menuBar->GetMenu(i);
        DoUpdateMenu(menu, accels, table);
    }
    // table will now have all menu accels that contained menu label accelerators
    // accel will be missing all accels found in the menu system, but retaining global accels
    #if defined(LOGGING) //debug accelerator counts
        size_t tableKnt = table.size(); wxUnusedVar(tableKnt);
        size_t accelsKnt = accels.size(); wxUnusedVar(accelsKnt);
    #endif // defined LOGING
    if(!table.empty() || !accels.empty()) {
        wxAcceleratorEntry* entries = new wxAcceleratorEntry[table.size() + accels.size()];

        // append to table, the globals retained in the accel table (not found as menu items)
        for(MenuItemDataVec_t::iterator iter = accels.begin(); iter != accels.end(); ++iter) {
            wxString dummyText;
            dummyText << iter->action << _T("\t") << iter->accel;
            wxAcceleratorEntry* entry = wxAcceleratorEntry::Create(dummyText);
            if(entry) {
                wxString resourceIDstr = iter->resourceID;
                long ldResourceID; resourceIDstr.ToLong(&ldResourceID);
                entry->Set(entry->GetFlags(), entry->GetKeyCode(), ldResourceID);
                table.push_back(*entry);
                wxDELETE(entry);
            }
        }

        // move global accel entries from table to wxAcceleratorTable array
        for(size_t i = 0; i < table.size(); ++i) {
            entries[i] = table.at(i);
        }

        #if defined(LOGGING)
        DumpAccelerators(table.size(), entries, frame); //(2019/10/27)
        #endif

        // Set the wxAcceleratorTable for this frame
        wxAcceleratorTable acceleTable(table.size(), entries);
        frame->SetAcceleratorTable(acceleTable);
        wxDELETEA(entries);

    }
}
// ----------------------------------------------------------------------------
void clKeyboardManager::Save()
// ----------------------------------------------------------------------------
{
    clKeyboardBindingConfig config;
    config.SetBindings(m_menuTable, m_globalTable).Save();
}
// ----------------------------------------------------------------------------
void clKeyboardManager::Initialize(bool isRefreshRequest)
// ----------------------------------------------------------------------------
{
    wxUnusedVar(isRefreshRequest);
    m_menuTable.clear();

    // First, try to load accelerators from %appdata%\<personality>.cbkeybinder<version>.conf
    //      containing merged default + user defined accerators
    // Second, try loading from default accelerators in previously create in %temp% dir

    clKeyboardBindingConfig config;
    if( not config.Exists()) //does cbKeyBinder__.conf exist? eg. %appdata%\<personality>.cbKeyBinder<version>.conf
    {
        #if defined(LOGGING)
        LOGIT( _T("[%s]"), _("Keyboard manager: No configuration found - importing old settings"));
        #endif

        // Old pre version 2.0 accererator setting are in %temp% dir (created from current menu structure + cbKeybinder10.ini)
        wxFileName fnOldSettings(clKeyboardManager::Get()->GetTempKeyMnuAccelsFilename());

        wxFileName fnFileToLoad;
        bool canDeleteOldSettings(false);
        // If %appdata% accerators.conf exist, use it
        if(fnOldSettings.FileExists())
        {
            fnFileToLoad = fnOldSettings;
        }

        if(fnFileToLoad.FileExists())
        {
            #if defined(LOGGING)
            LOGIT( _T("KeyboardManager:Importing settings from:\n\t[%s]"), fnFileToLoad.GetFullPath().wx_str());
            #endif
            // Apply the old settings to the menus
            wxString content;
            if(not ReadFileContent(fnFileToLoad, content)) return;
            wxArrayString lines = ::wxStringTokenize(content, _T("\r\n"), wxTOKEN_STRTOK);
            for(size_t i = 0; i < lines.GetCount(); ++i)
            {
                #if defined(LOGGING)
                    #if wxVERSION_NUMBER > 3000
                    LOGIT( _T("AccelFile[%u:%s]"), (unsigned)i, lines.Item(i).wx_str() );
                    #else
                    LOGIT( _T("AccelFile[%u:%s]"), i, lines.Item(i).wx_str() );
                    #endif
                #endif
                wxArrayString parts = ::wxStringTokenize(lines.Item(i), _T("|"), wxTOKEN_RET_EMPTY);
                if(parts.GetCount() < 3) continue;
                MenuItemData binding;
                binding.resourceID = parts.Item(0);
                binding.parentMenu = parts.Item(1);
                binding.action = parts.Item(2);
                if(parts.GetCount() == 4) {
                    binding.accel = parts.Item(3);
                }
                m_menuTable.push_back(binding);
            }

            if(canDeleteOldSettings) {
                if (fnFileToLoad.FileExists())
                    ::wxRemoveFile(fnFileToLoad.GetFullPath());
            }
        }
    }
    else //config exists: Load "%appdata%\<personality>cbKeybinder<ver>.conf"
    {
        config.Load();
        m_menuTable = config.GetBindings();
    }

    // ----------------------------------------------------------------------------
    // Load the default settings from %temp%/<personality>.keyMnuAccels_pid.conf"));
    // ----------------------------------------------------------------------------
    MenuItemDataVec_t defaultEntries = DoLoadDefaultAccelerators();

    // **Debugging
    // wxString msg = wxString::Format("Number of DFT items %zu", defaultEntries.size());
    //msg << wxString::Format("\nNumber of USR items %zu", m_menuTable.size());
    // cbMessageBox(msg, "Menu Item Count");

    // ----------------------------------------------------------------------------
    // Remove/Replace any map items nolonger matching the current menu structure
    // ----------------------------------------------------------------------------
    for (MenuItemDataVec_t::iterator usrIter = m_menuTable.begin(); usrIter != m_menuTable.end(); ++usrIter)
    {
        if (usrIter == m_menuTable.end()) break;

        ContinueAfterErase:
        //search menu structure map for menuId from .conf file
        MenuItemData* pUsrMenuItemData = &(*usrIter);
        //-if ( defaultEntries.count(usrIter->first) == 0) //(ph 2023/03/07)
        MenuItemData* pDftTableEntry = FindMenuTableEntryByPath(defaultEntries, pUsrMenuItemData);
        if (not pDftTableEntry)
        {   // menuID nolonger exists in CB
            wxString usrAccel = usrIter->accel;
            wxString usrParent = usrIter->parentMenu;
            wxString usrMnuID = usrIter->resourceID;
            #if defined(LOGGING)
                LOGIT( _T("Removing ID mismatch[%s][%s][%s]"), usrMnuID.wx_str(), usrParent.wx_str(), usrAccel.wx_str());
            #endif
            Manager::Get()->GetLogManager()->DebugLog(F( _T("KeyBinder:Removing ID mismatch[%s][%s][%s]"), usrMnuID.wx_str(), usrParent.wx_str(), usrAccel.wx_str()));
            // remove the user menu with the bad menu id
            usrIter = m_menuTable.erase(usrIter);
            goto ContinueAfterErase;
        }
        else // Have matching usr menu and default menu paths
        {
            // set the default menu accel to the user accel
            wxString usrParent    = usrIter->parentMenu;
            wxString dftMnuParent = pDftTableEntry->parentMenu;

            bool isGlobal = (usrParent.empty() or dftMnuParent.empty()); //(2020/07/14)
            if (isGlobal) continue;    //skip .conf global accelerators

            {
                // accel mismatch for same path names (software change or user accel)
                wxString usrMnuID  = usrIter->resourceID;
                wxString usrAccel  = usrIter->accel;
                wxString dftMnuID  = pDftTableEntry->resourceID;
                wxString dftAccel  = pDftTableEntry->accel;
                // Change the users menuID to match this menu label/accelerator
                // if users accel is different than menu structure, update the user usr menu //2020/04/6
                if (pDftTableEntry and
                        ((usrAccel != dftAccel) or (usrMnuID != dftMnuID)))
                {
                    // Path matches but Accelerator or Menu ID mismatch for same menu path
                    #if defined(LOGGING)
                    LOGIT( _T("         UserMapAccel[%s] != DftAccel[%s]"), usrAccel, dftAccel);
                    #endif
                    Manager::Get()->GetLogManager()->DebugLog(F( _T("KeyBinder:         UserMapAccel[%s] != DftAccel[%s]"), usrIter->accel.wx_str(), pDftTableEntry->accel.wx_str()));
                    //replace users menuID to correct one from default menu but keep usr accel.
                    pUsrMenuItemData->resourceID = dftMnuID;
                    #if defined(LOGGING)
                        LOGIT( _T("Setting LabelMismatch[%s][%s][%s]"), usrMnuID, usrParent, usrAccel);
                    #endif
                    Manager::Get()->GetLogManager()->DebugLog(F(_T("KeyBinder:Setting Path/Accel mismatch[%s][%s][%s]"), usrMnuID.wx_str(), usrParent.wx_str(), usrAccel.wx_str()));
                }
            }//endif label compare
        }//endif else have matching resourceID
    }//endfor vecIter

    #if defined(LOGGING)
        LogAccelerators(m_menuTable, _T("Log 1"));
    #endif

    // ----------------------------------------------------------------------------
    // Add any new entries from %temp%/<personality>.keyMnuAccels_pid.conf (the current menu structure)
    // ----------------------------------------------------------------------------
    for (MenuItemDataVec_t::iterator dftvecIter = defaultEntries.begin(); dftvecIter != defaultEntries.end(); ++dftvecIter)
    {
        int dftResourceID = std::stoi(dftvecIter->resourceID.ToStdString());
        MenuItemData* pUsrMenuItemData = FindMenuTableEntryByID(m_menuTable, dftResourceID );
        if (not pUsrMenuItemData)
        {   // add missing dft menu item
            m_menuTable.push_back(*dftvecIter); // add missing dft menu item if not in menuTable
            wxString dftMenuItemStr = dftvecIter->resourceID + _T("|") + dftvecIter->parentMenu + _T("|") + dftvecIter->accel;
            #if defined(LOGGING)
                LOGIT( _T("KeyBinder: adding missing menuItem[%s]"), dftMenuItemStr.wx_str());
            #endif
            Manager::Get()->GetLogManager()->DebugLog(F(_T("KeyBinder: adding missing menuItem[%s] "), dftMenuItemStr.wx_str()));
        }
        else // found item by this ID in .conf file, but is it the same path
        {
            MenuItemData* pDftMenuItemData = &(*dftvecIter);
            // Compare default path with .conf path
            if (pDftMenuItemData->parentMenu != pUsrMenuItemData->parentMenu)
            {   //Correct the menu item in .conf
                pUsrMenuItemData->accel = pDftMenuItemData->accel;
                pUsrMenuItemData->action = pDftMenuItemData->action;
                pUsrMenuItemData->parentMenu = pDftMenuItemData->parentMenu;
                wxString vdfltMenuItem = dftvecIter->resourceID + _T("|") + dftvecIter->parentMenu + _T("|") + dftvecIter->accel;
                #if defined(LOGGING)
                    LOGIT( _T("KeyBinder: adding default menuItem[%s]"), vdfltMenuItem.wx_str());
                #endif
                Manager::Get()->GetLogManager()->DebugLog(F(_T("KeyBinder: adding default menuItem[%s]"), vdfltMenuItem.wx_str()));
            }
            #if defined(LOGGING)
            else
            {
                wxString vdfltMenuItem = dftvecIter->resourceID + _T("|") + dftvecIter->parentMenu + _T("|") + dftvecIter->accel;
                LOGIT( _T("Keybinder: skipping already defined menuItem[%s]"), vdfltMenuItem.wx_str());
            }
            #endif
        }
    };

    #if defined(LOGGING)
        LogAccelerators(m_menuTable, _T("MergedLog 2"));
    #endif

    // Warn about duplicate shortcut entries (eg., (Print/PrevCallTip Ctrl-P) and (CC Search/Ctrl-Shift-.) have duplicates) //(2019/04/23)
    //? CheckForDuplicateAccels(m_menuTable);

    // update the menu and global accelerators //(pecan 2020/02/29)
    SetAccelerators(m_menuTable);

    // Store the correct configuration; globalTable is inserted into menuTable
    // The following has already been done by the SeetAccelerators() call above
    //- config.SetBindings(m_menuTable, m_globalTable).Save();

    #if defined(LOGGING)
        Logit("MenuLog 3 -----User Keybindings separated out ----------------");
        LogAccelerators(m_menuTable, "MenuLog 3");
        Logit("GlobalLog 3 -----Global Keybindings separated out ------------");
        LogAccelerators(m_globalTable, "GlobalsLog 3");
    #endif

    // And apply the changes
    // The following has already been done by the SetAccelerators() call above
    //-Update(); //(ph 2023/03/07)
}
// ----------------------------------------------------------------------------
void clKeyboardManager::GetAllAccelerators(MenuItemDataVec_t& accels) const
// ----------------------------------------------------------------------------
{
    accels.clear();
    accels.insert(accels.end(), m_menuTable.begin(), m_menuTable.end());
    accels.insert(accels.end(), m_globalTable.begin(), m_globalTable.end());
}

// ----------------------------------------------------------------------------
void clKeyboardManager::SetAccelerators(const MenuItemDataVec_t& accels)
// ----------------------------------------------------------------------------
{
    // separate the globals from the menu accelerators
    // The process is done by checking each item's parentMenu
    // If the parentMenu is empty, it's a global accelerator
    MenuItemDataVec_t globals, menus;
    MenuItemDataVec_t::const_iterator iter = accels.begin();
    for(; iter != accels.end(); ++iter)
    {
        if(iter->parentMenu.IsEmpty())
        {
            MenuItemData* pMenuItemData = (MenuItemData*)&(*iter);
            // skip duplicates of previouly entered globals
            MenuItemData* pGlobalTableEntry = FindMenuTableEntryByPathAndAccel(globals, pMenuItemData); //(2020/07/14)
            if (not pGlobalTableEntry)                                          //2020/07/14)
                globals.push_back(*iter);
            #if defined(LOGGING)
            else
                LOGIT( _T("Keybinder: skipping duplicate global[%s],[%s]"), iter->resourceID, iter->accel.wx_str());
            #endif
        }
        else
        {
            menus.push_back(*iter);
        }
    }

    m_menuTable.swap(menus);
    m_globalTable.swap(globals);
    Update(); //update accelerator tables
    Save();
}

// ----------------------------------------------------------------------------
void clKeyboardManager::Update(wxFrame* frame)
// ----------------------------------------------------------------------------
{
    // Since we keep the accelerators with their original resource ID in the form of string
    // we need to convert the map into a different integer with integer as the resource ID

    // Note that we place the items from the m_menuTable first and then we add the globals
    // this is because menu entries takes precedence over global accelerators
    MenuItemDataVec_t accels = m_menuTable;
    accels.insert(accels.end(), m_globalTable.begin(), m_globalTable.end());

    MenuItemDataVec_t intAccels;
    DoConvertToIntMap(accels, intAccels);

    if(!frame) {
        // update all frames
        wxFrame* topFrame = dynamic_cast<wxFrame*>(wxTheApp->GetTopWindow());
        CHECK_PTR_RET(topFrame);

        FrameList_t frames;
        DoGetFrames(topFrame, frames);
        for(FrameList_t::iterator iter = frames.begin(); iter != frames.end(); ++iter) {

            DoUpdateFrame(*iter, intAccels);
        }
    } else {
        // update only the requested frame
        DoUpdateFrame(frame, intAccels);
    }
}

//int clKeyboardManager::PopupNewKeyboardShortcutDlg(wxWindow* parent, MenuItemData& menuItemData)
//{
//    NewKeyShortcutDlg dlg(parent, menuItemData);
//    if(dlg.ShowModal() == wxID_OK) {
//        menuItemData.accel = dlg.GetAccel();
//        return wxID_OK;
//    }
//    return wxID_CANCEL;
//}
// ----------------------------------------------------------------------------
bool clKeyboardManager::Exists(const wxString& accel) const
// ----------------------------------------------------------------------------
{
    if(accel.IsEmpty()) return false;

    MenuItemDataVec_t accels;
    GetAllAccelerators(accels);

    MenuItemDataVec_t::const_iterator iter = accels.begin();
    for(; iter != accels.end(); ++iter) {
        if(iter->accel == accel) {
            return true;
        }
    }
    return false;
}
// -----------------------------------------------------------------------------------------------------------------
MenuItemDataVec_t::iterator clKeyboardManager::ExistsALikeAccel(MenuItemDataVec_t& srcMap, MenuItemDataVec_t::iterator srcvecIter) const //(2019/04/22)
// -----------------------------------------------------------------------------------------------------------------
{   // search for a like accelerator starting from specified map iterator

    MenuItemDataVec_t& accels = srcMap;
    if (srcvecIter == accels.end()) return accels.end();

    const wxString srcAccel = srcvecIter->accel;
    //-if(srcAccel.IsEmpty()) return accels.end(); //(ph 2023/03/07)

    MenuItemDataVec_t::iterator srcIter = srcvecIter;
    MenuItemDataVec_t::iterator iter = ++srcIter;
    for(; iter != accels.end(); ++iter)
    {
        //-if(iter->accel == srcAccel) //(ph 2023/03/07)
        if (iter->parentMenu == srcvecIter->parentMenu)
        {
            #if defined(LOGGING)
                // found a duplicate accelerator further down the accelerator map
                wxString srcAction = srcvecIter->action;
                wxString srcMnuID  = srcvecIter->resourceID;
                wxString dupAccel  = iter->accel;
                wxString dupAction = iter->action;
                wxString dupMnuID  = iter->resourceID;
                long srcMenuID; srcMnuID.ToLong(&srcMenuID);
                long dupMenuID; dupMnuID.ToLong(&dupMenuID);
            #endif
            if (iter->parentMenu.empty() )
                continue; //skip global accelerator
            return iter;
        }
    }
    return accels.end();
}
// -----------------------------------------------------------------------------------------------------------------
void clKeyboardManager::CheckForDuplicateAccels(MenuItemDataVec_t& accelMap) const //(2019/04/22)
// -----------------------------------------------------------------------------------------------------------------
{
    // Warn about duplicate Menu accelerators //(2019/04/22)

    wxArrayString dupMsgs;
    for(MenuItemDataVec_t::iterator accelIter = accelMap.begin(); accelIter != accelMap.end(); ++accelIter)
    {
        ContinueAfterErase:
        if (accelIter == accelMap.end()) break;

        //-if (accelIter->accel.empty()) continue;      //(ph 2023/03/07)
        //-if (accelIter->parentMenu.empty()) continue; //skip global accelerators //(ph 2023/03/07)
        MenuItemDataVec_t::iterator foundIter   = accelMap.end();
        MenuItemDataVec_t::iterator patternIter = accelIter;
        while (accelMap.end() != (foundIter = ExistsALikeAccel(accelMap, patternIter)) )
        {
            #if defined(LOGGING)
            wxString patternAccel  = patternIter->accel;
            wxString patternAction = patternIter->action;
            wxString dupAccel      = foundIter->accel;
            wxString dupAction     = foundIter->action;
            #endif
            //skip found global accelerators
            if (foundIter->parentMenu.empty())
            {
                patternIter = foundIter;
                continue;
            }

            // found a duplicate menu accelerator further down the accelerator map
            MenuItemDataVec_t::iterator srcIter = patternIter;


            wxString srcMenuLabel = srcIter->parentMenu;
            srcMenuLabel.Replace(_T("\t"), _T(" "));
            srcMenuLabel.Replace(_T("&"), _T(""));
            srcMenuLabel.Replace(_T("::"), _T("/"));
            if (srcMenuLabel.Contains(_T("Code/Blocks")) ) //special case of "Code::Blocks" text in menu title
                srcMenuLabel.Replace(_T("Code/Blocks"), _T("Code::Blocks"));

            wxString foundMenuLabel = foundIter->parentMenu;
            foundMenuLabel.Replace(_T("\t"), _T(" "));
            foundMenuLabel.Replace(_T("&"), _T(""));
            foundMenuLabel.Replace(_T("::"), _T("/"));
            if (foundMenuLabel.Contains(_T("Code/Blocks")) ) //special case of "Code::Blocks" text in menu title
                foundMenuLabel.Replace(_T("Code/Blocks"), _T("Code::Blocks"));

            long srcMenuID; srcIter->resourceID.ToLong(&srcMenuID);
            long foundMenuID; foundIter->resourceID.ToLong(&foundMenuID);

            // Remove duplicates with the same Menu ID
            if (srcMenuID == foundMenuID)
            {
                accelMap.erase(foundIter);
                goto ContinueAfterErase;
            }
            wxString msg = wxString::Format(_("Conflicting menu items: \'%s\' && \'%s\'"),
                                            srcMenuLabel.wx_str(), foundMenuLabel.wx_str())
                         + wxString::Format(_("\n   Both using shortcut: \'%s\'"), foundIter->accel.wx_str())
                         + wxString::Format(_(" (IDs [%ld] [%ld])"),srcMenuID, foundMenuID );
            msg += _T("\n\n");
            dupMsgs.Add(msg);
            patternIter = foundIter;

        }//end while
    }
    if (dupMsgs.GetCount())
    {
        bool isParentWindowDialog = false;
        // Get top window to solve msg window getting hidden behind keybinder dialog
        // Issue the key conflicts msg at CB startup and when user makes a change;
        // but not just because a plugin is {en|dis}abled or {un|in}stalled.
        wxWindow* pMainWin = nullptr;
        if ( (pMainWin = wxFindWindowByLabel(_("Configure editor"))) )
        {
            isParentWindowDialog = true;
        }
        else if ( (pMainWin = wxFindWindowByLabel(_("Manage plugins"))) )
        {
            // Don't issue msg when enabling/disabling plugins
            return;
        }
        else pMainWin = Manager::Get()->GetAppWindow();

        wxString msg = _("Keyboard shortcut conflicts found.\n");
        if (not isParentWindowDialog)
            msg += _("Use Settings/Editor/KeyboardShortcuts to resolve conflicts.\n\n");
        for (size_t ii=0; ii<dupMsgs.GetCount(); ++ii)
            msg += dupMsgs[ii];
        AnnoyingDialog dlg(_("Keyboard shortcuts conflicts"), msg, wxART_INFORMATION,  AnnoyingDialog::OK);
        dlg.ShowModal();
    }//endif dupMsgs

    return;
}
// ----------------------------------------------------------------------------
void clKeyboardManager::AddGlobalAccelerator(const wxString& resourceID,
                                             const wxString& keyboardShortcut,
                                             const wxString& description)
// ----------------------------------------------------------------------------
{
    MenuItemData mid;
    mid.action = description;
    mid.accel = keyboardShortcut;
    mid.resourceID = resourceID;
    m_globalTable.push_back( mid);
}
// ----------------------------------------------------------------------------
void clKeyboardManager::RestoreDefaults()
// ----------------------------------------------------------------------------
{
    // FIXME (ph#): RestoreDefaults needs to be supported
    wxASSERT_MSG(0, wxT("RestoreDefaults not supported yet !"));
    return ;

    // Decide which file we want to load, take the user settings file first
    // FIXME (ph#): This file was deleted at end of OnAppStartupDone()
    //-wxFileName fnOldSettings(wxStandardPaths::Get().GetTempDir(), _T("keyMnuAccels.conf"));
    wxString personality = Manager::Get()->GetPersonalityManager()->GetPersonality();
    //-fnOldSettings.SetName(personality + _T(".") + fnOldSettings.GetName());
    wxFileName fnOldSettings(clKeyboardManager::Get()->GetTempKeyMnuAccelsFilename()); //(2020/02/25)

    wxFileName fnNewSettings(ConfigManager::GetConfigFolder(), _T("cbKeyBinder20.conf"));
    fnNewSettings.SetName(personality + _T(".") + fnNewSettings.GetName());


    if(fnOldSettings.FileExists()) {
        wxRemoveFile(fnOldSettings.GetFullPath());
    }

    if(fnNewSettings.FileExists()) {
        wxRemoveFile(fnNewSettings.GetFullPath());
    }

    // Call initialize again
    bool isRefreshRequest = false;
    Initialize(isRefreshRequest);
}
//// ----------------------------------------------------------------------------
//void clKeyboardManager::OnStartupCompleted(wxCommandEvent& event)
//// ----------------------------------------------------------------------------
//{
//    event.Skip();
//    this->Initialize();
//}
// ----------------------------------------------------------------------------
void clKeyboardManager::DoConvertToIntMap(const MenuItemDataVec_t& strMap, MenuItemDataVec_t& intMap)
// ----------------------------------------------------------------------------
{
    // Convert the string map into int based map
    MenuItemDataVec_t::const_iterator iter = strMap.begin();
    for(; iter != strMap.end(); ++iter)
    {
        wxString resourceIDStr = iter->resourceID;
        long lnResourceID; resourceIDStr.ToLong(&lnResourceID);
        //-intMap.insert(std::make_pair(wxXmlResource::GetXRCID(iter->second.resourceID), iter->second));
        //-intMap.insert(std::make_pair(lnResourceID, iter->second)); //(ph 2023/03/06)
        intMap.push_back(*iter);    //(ph 2023/03/06)
    }
}
// ----------------------------------------------------------------------------
wxArrayString clKeyboardManager::GetAllUnasignedKeyboardShortcuts() const
// ----------------------------------------------------------------------------
{
    /// There are no calls to this function

    MenuItemDataVec_t accels;
    GetAllAccelerators(accels);

    wxStringSet_t usedShortcuts;
    //-std::for_each(accels.begin(), accels.end(), [&](const std::pair<wxString, MenuItemData>& p) { //(ph 2023/03/07)
    std::for_each(accels.begin(), accels.end(), [&](const MenuItemData& p )
    {
        if(!p.accel.IsEmpty()) {
            usedShortcuts.insert(p.accel);
        }
    });

    // Remove all duplicate entries
    wxArrayString allUnasigned;
    std::set_difference(m_allShorcuts.begin(),
                        m_allShorcuts.end(),
                        usedShortcuts.begin(),
                        usedShortcuts.end(),
                        std::back_inserter(allUnasigned));
    return allUnasigned;
}
// ----------------------------------------------------------------------------
MenuItemDataVec_t clKeyboardManager::DoLoadDefaultAccelerators()
// ----------------------------------------------------------------------------
{
    MenuItemDataVec_t entries;
    wxFileName fnDefaultOldSettings(clKeyboardManager::Get()->GetTempKeyMnuAccelsFilename()); //(2020/02/25)

    if(fnDefaultOldSettings.FileExists())
    {
        wxString content;
        if(not ReadFileContent(fnDefaultOldSettings, content)) {
            return entries;
        }
        wxArrayString lines = ::wxStringTokenize(content, _T("\r\n"), wxTOKEN_STRTOK);
        for(size_t i = 0; i < lines.GetCount(); ++i)
        {
            wxArrayString parts = ::wxStringTokenize(lines.Item(i), _T("|"), wxTOKEN_RET_EMPTY);
            if(parts.GetCount() < 2) continue;
            MenuItemData binding;
            binding.resourceID = parts.Item(0);
            binding.parentMenu = parts.Item(1);
            if (parts.GetCount() > 2)
                binding.action = parts.Item(2);
            if(parts.GetCount() == 4) {
                binding.accel = parts.Item(3);
            }

            // assure accelerator is legal 2020/05/30
            wxAcceleratorEntry legalAccel;
            if (binding.accel.Length())
                if ( not legalAccel.FromString(binding.accel))
                    continue;

            //entries.insert(std::make_pair(binding.resourceID, binding));
            entries.push_back( binding);
        }
    }
    return entries;
}
// ----------------------------------------------------------------------------
wxString clKeyboardManager::KeyCodeToString(int keyCode) //(2019/02/25)
// ----------------------------------------------------------------------------
{
	wxString res;

    //LOGIT("KeyCodeToString_IN:keyCode[%d]char[%c]", keyCode, keyCode );

	switch (keyCode)
	{
		// IGNORED KEYS
		// ---------------------------
	case WXK_START:
	case WXK_LBUTTON:
	case WXK_RBUTTON:
	case WXK_MBUTTON:
	case WXK_CLEAR:

	case WXK_PAUSE:
	case WXK_NUMLOCK:
	case WXK_SCROLL :
		wxLogDebug(_("wxKeyBind::KeyCodeToString - ignored key: [%d]"), keyCode);
		return wxEmptyString;

		// these must be ABSOLUTELY ignored: they are key modifiers
		// we won't output any LOG message since these keys could be pressed
		// for long time while the user choose its preferred keycombination:
		// this would result into a long long queue of "ignored key" messages
		// which would be useless even in debug builds...
	case WXK_SHIFT:
	case WXK_CONTROL:
	case WXK_ALT:                           //+v0.5
		return wxEmptyString;




		// FUNCTION KEYS
		// ---------------------------

	case WXK_F1: case WXK_F2:
	case WXK_F3: case WXK_F4:
	case WXK_F5: case WXK_F6:
	case WXK_F7: case WXK_F8:
	case WXK_F9: case WXK_F10:
	case WXK_F11: case WXK_F12:
	case WXK_F13: case WXK_F14:
    case WXK_F15: case WXK_F16:
    case WXK_F17: case WXK_F18:
    case WXK_F19: case WXK_F20:
    case WXK_F21: case WXK_F22:
    case WXK_F23: case WXK_F24:
		res << wxT('F') << wxString::Format(_T("%d"), keyCode - WXK_F1 + 1);
		break;


		// MISCELLANEOUS KEYS
		// ---------------------------

	case WXK_BACK:
        res << wxT("BACK"); break;
	case WXK_TAB:
        res << wxT("TAB"); break;
	case WXK_RETURN:
        res << wxT("RETURN"); break;
	case WXK_ESCAPE:
        res << wxT("ESCAPE"); break;
	case WXK_SPACE:
        res << wxT("SPACE"); break;
	case WXK_DELETE:
        res << wxT("DELETE"); break;
	case WXK_MULTIPLY:
		res << wxT("*"); break;
	case WXK_ADD:
		res << wxT("+"); break;
	case WXK_SEPARATOR:
		res << wxT("SEPARATOR"); break;
	case WXK_SUBTRACT:
		res << wxT("-"); break;
	case WXK_DECIMAL:
		res << wxT("."); break;
	case WXK_DIVIDE:
		res << wxT("/"); break;
	case WXK_PAGEUP:
		res << wxT("PAGEUP"); break;
	case WXK_PAGEDOWN:
		res << wxT("PAGEDOWN"); break;
	case WXK_LEFT:
        res << wxT("LEFT"); break;
	case WXK_UP:
        res << wxT("UP"); break;
	case WXK_RIGHT:
        res << wxT("RIGHT"); break;
	case WXK_DOWN:
        res << wxT("DOWN"); break;
	case WXK_SELECT:
        res << wxT("SELECT"); break;
	case WXK_PRINT:
        res << wxT("PRINT"); break;
	case WXK_EXECUTE:
        res << wxT("EXECUTE"); break;
	case WXK_SNAPSHOT:
        res << wxT("SNAPSHOT"); break;
	case WXK_INSERT:
        res << wxT("INSERT"); break;
	case WXK_HELP:
        res << wxT("HELP"); break;
	case WXK_CANCEL:
        res << wxT("CANCEL"); break;
	case WXK_MENU:
        res << wxT("MENU"); break;
	case WXK_CAPITAL:
        res << wxT("CAPITAL"); break;
	case WXK_END:
        res << wxT("END"); break;
	case WXK_HOME:
        res << wxT("HOME"); break;

//+V.05 (Pecan#1#): wxIsalnm is excluding keys not num or a-z like }{ etc
//+v.05 (Pecan#1#): Holding Alt shows ALT+3 A: added WXK_ALT: to above case
//+v.05 (Pecan#1#): ALT +Ctrl Left/Right show in Dlg, up/Down dont. Printable?
//               A: wxWidgets2.6.2 returns false on modifier keys for Ctrl+Alt+UP/DOWN combination.
//                  It returns Ctrl+Alt+PRIOR instead of UP/DOWN and shows false for ctrl & alt.
//                  Same is true for Ctrl+Shift+UP/Down.
//                  Alt+Shift+Up/Down work ok.
	default:
		// ASCII chars...
		if (wxIsalnum(keyCode))
		{
			res << (wxChar)keyCode;
			break;

		} else if ((res=NumpadKeyCodeToString(keyCode)) != wxEmptyString) {

			res << wxT(" (numpad)");		// so it is clear it's different from other keys
			break;

		} else if (wxIsprint(keyCode)) { //v+0.5
			res << (wxChar)keyCode;
			break;

		} else {

			// we couldn't create a description for the given keycode...
			wxLogDebug(_("wxKeyBind::KeyCodeToString - unknown key: [%d]"), keyCode);
			return wxEmptyString;
		}
	}//default

    //#if LOGGING
    // LOGIT(_T("KeyCodeToStringOUT:keyCode[%d]char[%c]Desc[%s]"),
    //            keyCode, keyCode, res.GetData() );
    //#endif

	return res;

}//KeyCodeToString
// ----------------------------------------------------------------------------
wxString clKeyboardManager::NumpadKeyCodeToString(int keyCode) //(2019/02/25)
// ----------------------------------------------------------------------------
{
	wxString res;

	switch (keyCode)
	{
		// NUMPAD KEYS
		// ---------------------------

	case WXK_NUMPAD0:
	case WXK_NUMPAD1:
	case WXK_NUMPAD2:
	case WXK_NUMPAD3:
	case WXK_NUMPAD4:
	case WXK_NUMPAD5:
	case WXK_NUMPAD6:
	case WXK_NUMPAD7:
	case WXK_NUMPAD8:
	case WXK_NUMPAD9:
		res << wxString::Format(_T("%d"), keyCode - WXK_NUMPAD0);
		break;

	case WXK_NUMPAD_SPACE:
		res << wxT("SPACE"); break;
	case WXK_NUMPAD_TAB:
		res << wxT("TAB"); break;
	case WXK_NUMPAD_ENTER:
		res << wxT("ENTER"); break;

	case WXK_NUMPAD_F1:
	case WXK_NUMPAD_F2:
	case WXK_NUMPAD_F3:
	case WXK_NUMPAD_F4:
		res << wxT("F") << wxString::Format(_T("%d"), keyCode - WXK_NUMPAD_F1);
		break;

	case WXK_NUMPAD_LEFT:
		res << wxT("LEFT"); break;
	case WXK_NUMPAD_UP:
		res << wxT("UP"); break;
	case WXK_NUMPAD_RIGHT:
		res << wxT("RIGHT"); break;
	case WXK_NUMPAD_DOWN:
		res << wxT("DOWN"); break;
	case WXK_NUMPAD_HOME:
		res << wxT("HOME"); break;
	case WXK_NUMPAD_PAGEUP:
		res << wxT("PAGEUP"); break;
	case WXK_NUMPAD_PAGEDOWN:
		res << wxT("PAGEDOWN"); break;
	case WXK_NUMPAD_END:
		res << wxT("END"); break;
	case WXK_NUMPAD_BEGIN:
		res << wxT("BEGIN"); break;
	case WXK_NUMPAD_INSERT:
		res << wxT("INSERT"); break;
	case WXK_NUMPAD_DELETE:
		res << wxT("DELETE"); break;
	case WXK_NUMPAD_EQUAL:
		res << wxT("="); break;
	case WXK_NUMPAD_MULTIPLY:
		res << wxT("*"); break;
	case WXK_NUMPAD_ADD:
		res << wxT("+"); break;
	case WXK_NUMPAD_SEPARATOR:
		res << wxT("SEPARATOR"); break;
	case WXK_NUMPAD_SUBTRACT:
		res << wxT("-"); break;
	case WXK_NUMPAD_DECIMAL:
		res << wxT("."); break;
	case WXK_NUMPAD_DIVIDE:
		res << wxT("/"); break;
    default:
        break;
	}

	return res;
}
// ----------------------------------------------------------------------------
bool clKeyboardManager::WriteFileContent(const wxFileName& fn, const wxString& content, const wxMBConv& conv)  //(2019/04/3)
// ----------------------------------------------------------------------------

{
    wxFFile file(fn.GetFullPath(), wxT("w+b"));
    if(!file.IsOpened()) { return false; }

    if(!file.Write(content, conv)) { return false; }
    return true;
}
// ----------------------------------------------------------------------------
bool clKeyboardManager::ReadFileContent(const wxFileName& fn, wxString& data, const wxMBConv& conv)    //(2019/04/3)
// ----------------------------------------------------------------------------

{
    wxString filename = fn.GetFullPath();
    wxFFile file(filename, wxT("rb"));
    if(file.IsOpened() == false) {
        // Nothing to be done
        return false;
    }
    return file.ReadAll(&data, conv);
}
// ----------------------------------------------------------------------------
void clKeyboardManager::DumpAccelerators(size_t tableCount, wxAcceleratorEntry* pEntries, wxFrame* pFrame)
// ----------------------------------------------------------------------------
{

    if (0 == tableCount) return;

    wxString tmpDir = wxFileName::GetTempDir();
    wxString txtFilename = tmpDir +sep +_T("KBGlobalsFrame_") + wxString::Format(_T("%d"),++frameKnt) + _T(".txt");
    if (wxFileExists(txtFilename))
        wxRemoveFile(txtFilename);
    wxTextFile txtAccels(txtFilename);
    txtAccels.Create();
    txtAccels.AddLine(pFrame->GetTitle());

    for (size_t ii = 0; ii < tableCount; ++ii)
    {
        //int flags;
        //int keyCode;
        //int command;
        wxString strCommand;
        wxString txtLine = wxString::Format(_T("accelEntry[%d] flags[%d] code[%d] id[%d]"),
                        int(ii),
                        pEntries[ii].GetFlags(),
                        pEntries[ii].GetKeyCode(),
                        pEntries[ii].GetCommand()    //numeric id
                      );

        strCommand = pEntries[ii].ToString();
        txtLine += _T(" ") + strCommand;
        txtAccels.AddLine(txtLine);
    }
    txtAccels.Write();
    txtAccels.Close();

}
// ----------------------------------------------------------------------------
void clKeyboardManager::LogAccelerators(MenuItemDataVec_t& menuTable, wxString title)
// ----------------------------------------------------------------------------
{
    //LogAccelerator
    #if defined(LOGGING)
    wxString logTitle = title;
    if (logTitle.Length() == 0)
        logTitle = _T("MenuTable:");
    LOGIT( _T("[%s]"), logTitle);
    for (MenuItemDataVec_t::iterator vecIter = menuTable.begin(); vecIter != menuTable.end(); ++vecIter)
    {
        wxString mapAccel = vecIter->accel;
        wxString mapParent = vecIter->parentMenu;
        wxString mapMnuID = vecIter->resourceID;
        LOGIT( _T("[%s][%s][%s][%s]"), logTitle.wx_str(), mapMnuID.wx_str(), mapParent.wx_str(), mapAccel.wx_str());
    }
    #else
        wxUnusedVar(menuTable);
        wxUnusedVar(title);
    #endif
}
// ----------------------------------------------------------------------------
MenuItemData* clKeyboardManager::FindMenuTableEntryFor(MenuItemDataVec_t& vecTable, MenuItemData* pMenuMapItem)
// ----------------------------------------------------------------------------
{
    for (MenuItemDataVec_t::iterator vecIter = vecTable.begin(); vecIter != vecTable.end(); ++vecIter)
    {
        if ( (vecIter->resourceID == pMenuMapItem->resourceID)
            and (vecIter->parentMenu == pMenuMapItem->parentMenu) )
            return  &(*vecIter);
    }
    return nullptr;
}
// ----------------------------------------------------------------------------
MenuItemData* clKeyboardManager::FindMenuTableEntryByPathAndAccel(MenuItemDataVec_t& vecTable, MenuItemData* pMenuMapItem)
// ----------------------------------------------------------------------------
{
    for (MenuItemDataVec_t::iterator vecIter = vecTable.begin(); vecIter != vecTable.end(); ++vecIter)
    {
        if ( (vecIter->accel == pMenuMapItem->accel)
                and (vecIter->parentMenu == pMenuMapItem->parentMenu) )
            return  &(*vecIter);
    }
    return nullptr;
}
// ----------------------------------------------------------------------------
MenuItemData* clKeyboardManager::FindMenuTableEntryByPath(MenuItemDataVec_t& vecTable, MenuItemData* pMenuMapItem)
// ----------------------------------------------------------------------------
{
    for (MenuItemDataVec_t::iterator vecIter = vecTable.begin(); vecIter != vecTable.end(); ++vecIter)
    {
        //-if ( (vecIter->second.accel == pMenuMapItem->accel) //(ph 2023/03/07)
        if (vecIter->parentMenu == pMenuMapItem->parentMenu)
            return  &(*vecIter);
    }
    return nullptr;
}
// ----------------------------------------------------------------------------
MenuItemData* clKeyboardManager::FindMenuTableEntryByID(MenuItemDataVec_t& vecTable, int ID)
// ----------------------------------------------------------------------------
{
    for (MenuItemDataVec_t::iterator vecIter = vecTable.begin(); vecIter != vecTable.end(); ++vecIter)
    {
        //-if ( (vecIter->second.accel == pMenuMapItem->accel) //(ph 2023/03/07)
        int resourceIDToInt = std::stoi(vecIter->resourceID.ToStdString());
        if (resourceIDToInt == ID)
            return  &(*vecIter);
    }
    return nullptr;
}
