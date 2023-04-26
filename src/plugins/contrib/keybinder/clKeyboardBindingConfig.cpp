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
#include <wx/filename.h>
# if defined(LOGGING)
    #include <wx/private/wxprintf.h>
    #include <wx/textfile.h>
#endif

#include "manager.h"
#include "personalitymanager.h"
#include "configmanager.h"

#include "clKeyboardBindingConfig.h"
#include "json_node.h"

// ----------------------------------------------------------------------------
clKeyboardBindingConfig::clKeyboardBindingConfig() {}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
clKeyboardBindingConfig::~clKeyboardBindingConfig() {}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
clKeyboardBindingConfig& clKeyboardBindingConfig::Load()
// ----------------------------------------------------------------------------
{
    wxFileName fn(ConfigManager::GetConfigFolder(), _T("cbKeyBinder20.conf"));
    wxString personality = Manager::Get()->GetPersonalityManager()->GetPersonality();
    fn.SetName(personality + _T(".") + fn.GetName());
    if(not fn.FileExists()) return *this;

    m_bindings.clear();
    JSONRoot root(fn);
    //- unused typedef std::unordered_multimap<wxString, wxString> GlobalAccelMap_t;     //(2019/04/3)
    MenuItemDataVec_t globalBindings;                                                   //(2019/04/3)

    {//Block
        JSONElement menus = root.toElement().namedObject(_T("menus"));
        int arrSize = menus.arraySize();
        for(int i = 0; i < arrSize; ++i) {
            JSONElement item = menus.arrayItem(i);
            MenuItemData binding;
            binding.action = item.namedObject(_T("description")).toString();
            binding.accel = item.namedObject(_T("accelerator")).toString();
            binding.parentMenu = item.namedObject(_T("parentMenu")).toString();
            binding.resourceID = item.namedObject(_T("resourceID")).toString();
            if (binding.parentMenu.empty()) //Empty parent menu means a <global> accelerator
            {
                // Hold global key bindings until later
                globalBindings.push_back( binding);
                continue;
            }
            // insert regular menu items before globals
            m_bindings.push_back( binding);
        }//endif for arrSize

        // Append the global key bindings to the end of the menu bindings
        m_bindings.insert(m_bindings.end(), globalBindings.begin(), globalBindings.end());

    }//end Block
    return *this;
}
// ----------------------------------------------------------------------------
clKeyboardBindingConfig& clKeyboardBindingConfig::Save()
// ----------------------------------------------------------------------------
{
    // ----------------------------------------------------------------------------
    // Sort the bindings
    // ----------------------------------------------------------------------------
    std::vector<MenuItemDataVec_t::iterator> sortedIters;
    SortBindings(sortedIters);

    JSONRoot root(cJSON_Object);
    JSONElement mainObj = root.toElement();
    JSONElement menuArr = JSONElement::createArray(_T("menus"));
    mainObj.append(menuArr);
    //-for(MenuItemDataVec_t::iterator iter = m_bindings.begin(); iter != m_bindings.end(); ++iter) { //(ph 2023/03/07)
    for (size_t ii=0; ii< sortedIters.size(); ++ii)
    {
        MenuItemDataVec_t::iterator iter = sortedIters[ii];
        JSONElement binding = JSONElement::createObject();
        binding.addProperty(_T("description"), iter->action);
        binding.addProperty(_T("accelerator"), iter->accel);
        binding.addProperty(_T("resourceID"), iter->resourceID);
        binding.addProperty(_T("parentMenu"), iter->parentMenu);
        menuArr.arrayAppend(binding);
    }

    wxString configDir = ConfigManager::GetConfigFolder();
    wxFileName fn(configDir, _T("cbKeyBinder20.conf"));
    wxString personality = Manager::Get()->GetPersonalityManager()->GetPersonality();
    fn.SetName(personality + _T(".") + fn.GetName());

    root.save(fn);
    return *this;
}
// ----------------------------------------------------------------------------
bool clKeyboardBindingConfig::SortBindings( std::vector<MenuItemDataVec_t::iterator>& sortedIters)
// ----------------------------------------------------------------------------
{
    // ----------------------------------------------------------------------------
    // set a vector of iters sorted by parent menu strings
    // ----------------------------------------------------------------------------
    std::vector<MenuItemDataVec_t::iterator> bindGlobals; //indexes to globals in sorted order

    for(MenuItemDataVec_t::iterator iter = m_bindings.begin(); iter != m_bindings.end(); ++iter)
    {
        wxString description = iter->action;     //description
        wxString accelerator = iter->accel;      //accelerator
        wxString resourceID = iter->resourceID;  //menu resource ID
        wxString parentMenu = iter->parentMenu;  //parent menu

        if (parentMenu.empty()) //global accelerator
        {
            bindGlobals.push_back(iter);
            continue;
        }

        MenuItemDataVec_t::iterator iterOne;
        MenuItemDataVec_t::iterator iterTwo;
        wxString strOne;
        wxString strTwo;

        if (0 == sortedIters.size())
        {
            sortedIters.push_back(iter);
            iterTwo = iterOne = iter;
            strOne = strTwo = parentMenu;
            continue;
        }

        strOne = parentMenu;
        iterOne = iter;

        bool inserted = false;
        for (size_t ii=0; ii< sortedIters.size(); ++ii)
        {
            iterTwo = sortedIters[ii];
            strTwo =  iterTwo->parentMenu;
            if (strOne <= strTwo)
            {
                sortedIters.insert(sortedIters.begin()+ii, iterOne );
                inserted = true;
                break; //breaks the for loop
            }
        }//endfor
        if (not inserted)
        {
            sortedIters.push_back(iterOne);
        }
    }
    // append the global accelerator iters to the sortedIters vector
    for (size_t ii=0; ii < bindGlobals.size(); ++ii)
        sortedIters.push_back(bindGlobals[ii]);

    #if defined(LOGGING) //debugging
    // ----------------------------------------------------------------------------
    // Debugging: write log file of the sorted sequence
    // ----------------------------------------------------------------------------
    wxString tempDir = wxFileName::GetTempDir();
    wxString txtFilename = tempDir + _T("\\JsonSortLogFile.txt");
    if (wxFileExists(txtFilename))
        wxRemoveFile(txtFilename);
    wxTextFile logFile(txtFilename);
    logFile.Create();
    for (size_t ii=0; ii< sortedIters.size(); ++ii)
    {
        MenuItemDataVec_t::iterator iterTwo = sortedIters[ii];
        wxString strTwo =  iterTwo->parentMenu;
        if (not strTwo.empty())
        {
            logFile.AddLine(strTwo);
            continue;
        }
        // must be a global
        if (strTwo.empty())
        {
            wxString strTwo =  iterTwo->action +_T(" ") + iterTwo->accel;
            logFile.AddLine(strTwo);
        }
    }

    logFile.Write();
    logFile.Close();
    #endif //defined logging

    return (sortedIters.size() > 0);
}

