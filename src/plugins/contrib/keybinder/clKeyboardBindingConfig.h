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
#ifndef CLKEYBOARDBINDINGCONFIG_H
#define CLKEYBOARDBINDINGCONFIG_H

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "configmanager.h"
#include "clKeyboardManager.h"

// ----------------------------------------------------------------------------
class  clKeyboardBindingConfig
// ----------------------------------------------------------------------------
{
    MenuItemDataVec_t m_bindings;
 public:
    clKeyboardBindingConfig();
    virtual ~clKeyboardBindingConfig();

    clKeyboardBindingConfig& Load();
    clKeyboardBindingConfig& Save();
    bool SortBindings( std::vector<MenuItemDataVec_t::iterator>& sortedIters);

    // ----------------------------------------------------------------
    bool Exists() const
    // ----------------------------------------------------------------
    {
        wxFileName fn(ConfigManager::GetConfigFolder(), _T("cbKeyBinder20.conf"));
        wxString personality = Manager::Get()->GetPersonalityManager()->GetPersonality();
        fn.SetName(personality + _T(".") + fn.GetName());

        #if defined(LOGGING)
            wxString look = fn.GetFullPath(); ;
        #endif
        return fn.FileExists();
    }

    // ----------------------------------------------------------------
    clKeyboardBindingConfig& SetBindings(const MenuItemDataVec_t& menus, const MenuItemDataVec_t& globals)
    // ----------------------------------------------------------------
    {
        this->m_bindings = menus;
        this->m_bindings.insert(m_bindings.end(), globals.begin(), globals.end());
        return *this;
    }
    // ----------------------------------------------------------------
    const MenuItemDataVec_t& GetBindings() const { return m_bindings; }
    // ----------------------------------------------------------------
};

#endif // CLKEYBOARDBINDINGCONFIG_H
