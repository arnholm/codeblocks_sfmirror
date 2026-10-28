/////////////////////////////////////////////////////////////////////////////
// Name:        keybinder.cpp
// Purpose:     wxKeyBind, wxCmd, wxKeyBinder, wxKeyConfigPanel
// Author:      Aleksandras Gluchovas
// Modified by: Francesco Montorsi
// Modified by: Pecan Heber
// Created:     2000/02/10
// Copyright:   (c) Aleksandras Gluchovas and (c) Francesco Montorsi
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////
// RCS-ID:      $Id$

// Modified Keybinder for CodeBlocks KeyBnder v2.0 2019/04/8

// includes
#include <wx/event.h>
#include <wx/frame.h> // Manager::Get()->GetAppWindow()
#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/string.h>
#include "wx/statline.h" // used no matter what clangd says
#include "wx/accel.h"    // Support for wx3.2.6 NumKeyPad changes

#include "keybinder.h"
#include "menuutils.h"
#include "wx/config.h"  // used no matter what clangd says
#include "wx/tokenzr.h"

#if defined(kbLOGGING)
    #include "debugging.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
    #include "wx/log.h"
#endif

#include "manager.h"
#include "logmanager.h"

//#if defined(CB_PRECOMP)
//#include "sdk.h"
//#else
//    #include "sdk_events.h"
//    #include "manager.h"
//    #include "projectmanager.h"
//    #include "editormanager.h"
//    #include "cbworkspace.h"
//    #include "cbproject.h"
//    #include "logmanager.h"
//#endif

// ----------------------------------------------------------------------------
namespace // Support for wx3.2.6 NumKeyPad changes
// ----------------------------------------------------------------------------
{
    struct NumpadKeyName
    {
        int code;
        const wxChar* name;
        const wxChar* legacyName;
    };

    const NumpadKeyName numpadKeyNames[] =
    {
        { WXK_NUMPAD0,          wxT("KP_0"),         wxT("0 (numpad)") },
        { WXK_NUMPAD1,          wxT("KP_1"),         wxT("1 (numpad)") },
        { WXK_NUMPAD2,          wxT("KP_2"),         wxT("2 (numpad)") },
        { WXK_NUMPAD3,          wxT("KP_3"),         wxT("3 (numpad)") },
        { WXK_NUMPAD4,          wxT("KP_4"),         wxT("4 (numpad)") },
        { WXK_NUMPAD5,          wxT("KP_5"),         wxT("5 (numpad)") },
        { WXK_NUMPAD6,          wxT("KP_6"),         wxT("6 (numpad)") },
        { WXK_NUMPAD7,          wxT("KP_7"),         wxT("7 (numpad)") },
        { WXK_NUMPAD8,          wxT("KP_8"),         wxT("8 (numpad)") },
        { WXK_NUMPAD9,          wxT("KP_9"),         wxT("9 (numpad)") },

        { WXK_NUMPAD_SPACE,     wxT("KP_Space"),     wxT("SPACE (numpad)") },
        { WXK_NUMPAD_TAB,       wxT("KP_Tab"),       wxT("TAB (numpad)") },
        { WXK_NUMPAD_ENTER,     wxT("KP_Enter"),     wxT("ENTER (numpad)") },

        { WXK_NUMPAD_F1,        wxT("KP_F1"),        wxT("F1 (numpad)") },
        { WXK_NUMPAD_F2,        wxT("KP_F2"),        wxT("F2 (numpad)") },
        { WXK_NUMPAD_F3,        wxT("KP_F3"),        wxT("F3 (numpad)") },
        { WXK_NUMPAD_F4,        wxT("KP_F4"),        wxT("F4 (numpad)") },

        { WXK_NUMPAD_LEFT,      wxT("KP_Left"),      wxT("LEFT (numpad)") },
        { WXK_NUMPAD_UP,        wxT("KP_Up"),        wxT("UP (numpad)") },
        { WXK_NUMPAD_RIGHT,     wxT("KP_Right"),     wxT("RIGHT (numpad)") },
        { WXK_NUMPAD_DOWN,      wxT("KP_Down"),      wxT("DOWN (numpad)") },
        { WXK_NUMPAD_HOME,      wxT("KP_Home"),      wxT("HOME (numpad)") },
        { WXK_NUMPAD_PAGEUP,    wxT("KP_PageUp"),    wxT("PAGEUP (numpad)") },
        { WXK_NUMPAD_PAGEDOWN,  wxT("KP_PageDown"),  wxT("PAGEDOWN (numpad)") },
        { WXK_NUMPAD_END,       wxT("KP_End"),       wxT("END (numpad)") },
        { WXK_NUMPAD_BEGIN,     wxT("KP_Begin"),     wxT("BEGIN (numpad)") },
        { WXK_NUMPAD_INSERT,    wxT("KP_Insert"),    wxT("INSERT (numpad)") },
        { WXK_NUMPAD_DELETE,    wxT("KP_Delete"),    wxT("DELETE (numpad)") },

        { WXK_NUMPAD_EQUAL,     wxT("KP_Equal"),     wxT("= (numpad)") },
        { WXK_NUMPAD_MULTIPLY,  wxT("KP_Multiply"),  wxT("* (numpad)") },
        { WXK_NUMPAD_ADD,       wxT("KP_Add"),       wxT("+ (numpad)") },
        { WXK_NUMPAD_SEPARATOR, wxT("KP_Separator"), wxT("SEPARATOR (numpad)") },
        { WXK_NUMPAD_SUBTRACT,  wxT("KP_Subtract"),  wxT("- (numpad)") },
        { WXK_NUMPAD_DECIMAL,   wxT("KP_Decimal"),   wxT(". (numpad)") },
        { WXK_NUMPAD_DIVIDE,    wxT("KP_Divide"),    wxT("/ (numpad)") }
    };

    const size_t numpadKeyNameCount =
        sizeof(numpadKeyNames) / sizeof(numpadKeyNames[0]);

} // anonymous namespace


// class definition for wxKeyProfile
IMPLEMENT_CLASS(wxKeyProfile, wxKeyBinder)


// event table for wxKeyBinder
IMPLEMENT_CLASS(wxKeyBinder, wxObject)

// event table for wxKeyMonitorTextCtrl
IMPLEMENT_CLASS(wxKeyMonitorTextCtrl, wxTextCtrl)
BEGIN_EVENT_TABLE(wxKeyMonitorTextCtrl, wxTextCtrl)

    // keydown events
    EVT_KEY_DOWN(wxKeyMonitorTextCtrl::OnKey)
    EVT_KEY_UP(wxKeyMonitorTextCtrl::OnKey)

END_EVENT_TABLE()



// event table for wxKeyConfigPanel
IMPLEMENT_CLASS(wxKeyConfigPanel, wxPanel)
BEGIN_EVENT_TABLE(wxKeyConfigPanel, wxPanel)

    // miscellaneous
    EVT_TEXT(wxKEYBINDER_KEY_FIELD_ID, wxKeyConfigPanel::OnKeyPressed)
    EVT_TEXT(wxKEYBINDER_KEYPROFILES_ID, wxKeyConfigPanel::OnProfileEditing)
    EVT_COMBOBOX(wxKEYBINDER_KEYPROFILES_ID, wxKeyConfigPanel::OnProfileSelected)
    EVT_LISTBOX(wxKEYBINDER_BINDINGS_BOX_ID, wxKeyConfigPanel::OnBindingSelected)

    // used when the treectrl is used
    EVT_TREE_SEL_CHANGED(wxKEYBINDER_COMMANDS_BOX_ID,
                        wxKeyConfigPanel::OnTreeCommandSelected)

    // used when the listbox+combobox is used
    EVT_COMBOBOX(wxKEYBINDER_CATEGORIES_ID, wxKeyConfigPanel::OnCategorySelected)
    EVT_LISTBOX(wxKEYBINDER_COMMANDS_BOX_ID, wxKeyConfigPanel::OnListCommandSelected)

    // buttons
    EVT_BUTTON(wxID_APPLY, wxKeyConfigPanel::OnApplyChanges)
    EVT_BUTTON(wxKEYBINDER_ASSIGN_KEY_ID, wxKeyConfigPanel::OnAssignKey)
    EVT_BUTTON(wxKEYBINDER_REMOVE_KEY_ID, wxKeyConfigPanel::OnRemoveKey)
    EVT_BUTTON(wxKEYBINDER_REMOVEALL_KEY_ID, wxKeyConfigPanel::OnRemoveAllKey)
    EVT_BUTTON(wxKEYBINDER_ADD_PROFILEBTN_ID, wxKeyConfigPanel::OnAddProfile)
    EVT_BUTTON(wxKEYBINDER_REMOVE_PROFILEBTN_ID, wxKeyConfigPanel::OnRemoveProfile)

    // during idle cycles, wxKeyConfigPanel checks if the wxKeyMonitorTextCtrl
    // associated must be cleared...

END_EVENT_TABLE()

// ------------------
// some statics
// ------------------
    int wxCmd::m_nCmdTypes = 0;
    wxCmd::wxCmdType wxCmd::m_arrCmdType[];

// ----------------------------------------------------------------------------
// wxKeyBind STATIC utilities
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
wxString wxKeyBind::NumpadKeyCodeToString(int keyCode) // Support for wx3.2.6 NumKeyPad changes
// ----------------------------------------------------------------------------
{
    for (size_t i = 0; i < numpadKeyNameCount; ++i)
    {
        if (numpadKeyNames[i].code == keyCode)
            return wxString(numpadKeyNames[i].name);
    }

    return wxEmptyString;
}

// ----------------------------------------------------------------------------
wxString wxKeyBind::KeyCodeToString(int keyCode)
// ----------------------------------------------------------------------------
{
    // Special-key codes must be recognized before character classification.
    // Support for wx3.2.6 NumKeyPad changes
    const wxString numpadName = NumpadKeyCodeToString(keyCode);
    if (!numpadName.IsEmpty())
        return numpadName;

    wxString res;

    switch (keyCode)
    {        // IGNORED KEYS
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


    default:
        // Support for wx3.2.6 NumKeyPad changes
        // Remaining ordinary keyboard characters are ASCII key codes.
        // Do not reinterpret unknown wx special-key codes as Unicode text.
        if (keyCode >= 0 && keyCode < 128 && wxIsprint(keyCode))
        {
            res << static_cast<wxChar>(keyCode);
        }
        else
        {
            wxLogDebug(
                wxT("wxKeyBind::KeyCodeToString - unknown key: [%d]"),
                keyCode
            );
            return wxEmptyString;
        }
        break;
    }

    return res;

}//KeyCodeToString
// ----------------------------------------------------------------------------
int wxKeyBind::StringToKeyCode(const wxString &keyName)
// ----------------------------------------------------------------------------
{
    // Support for wx3.2.6 NumKeyPad changes    2026/10/26
    // Avoid accessing character 0 of an empty string.
    if (keyName.IsEmpty())
        return WXK_NONE;

    // Check keypad names before ordinary function keys.
    // Accept both canonical "KP_..." names and legacy "... (numpad)" names.
    for (size_t i = 0; i < numpadKeyNameCount; ++i)
    {
        if (keyName.CmpNoCase(numpadKeyNames[i].name) == 0 ||
            keyName.CmpNoCase(numpadKeyNames[i].legacyName) == 0)
        {
            return numpadKeyNames[i].code;
        }
    }

    // Alternative keypad navigation names.
    if (keyName.CmpNoCase(wxT("KP_Prior")) == 0)
        return WXK_NUMPAD_PAGEUP;

    if (keyName.CmpNoCase(wxT("KP_Next")) == 0)
        return WXK_NUMPAD_PAGEDOWN;

    // An ordinary function key: F1 through F24.
    if (keyName.Len() > 1 &&
        keyName.Left(1).CmpNoCase(wxT("F")) == 0)
    {
        long n = 0;
        const wxString number = keyName.Mid(1);

        // Require digits only, and check conversion success and range.
        if (number.find_first_not_of(wxT("0123456789")) == wxString::npos &&
            number.ToLong(&n) &&
            n >= 1 && n <= 24)
        {
            return WXK_F1 + static_cast<int>(n) - 1;
        }

        return WXK_NONE;
    }

    // Ordinary special keys.
    if (keyName.CmpNoCase(wxT("BACK")) == 0)     return WXK_BACK;
    if (keyName.CmpNoCase(wxT("ENTER")) == 0)    return WXK_RETURN;
    if (keyName.CmpNoCase(wxT("RETURN")) == 0)   return WXK_RETURN;
    if (keyName.CmpNoCase(wxT("TAB")) == 0)      return WXK_TAB;
    if (keyName.CmpNoCase(wxT("ESCAPE")) == 0)   return WXK_ESCAPE;
    if (keyName.CmpNoCase(wxT("SPACE")) == 0)    return WXK_SPACE;
    if (keyName.CmpNoCase(wxT("DELETE")) == 0)   return WXK_DELETE;

    if (keyName.CmpNoCase(wxT("LEFT")) == 0)     return WXK_LEFT;
    if (keyName.CmpNoCase(wxT("UP")) == 0)       return WXK_UP;
    if (keyName.CmpNoCase(wxT("RIGHT")) == 0)    return WXK_RIGHT;
    if (keyName.CmpNoCase(wxT("DOWN")) == 0)     return WXK_DOWN;
    if (keyName.CmpNoCase(wxT("HOME")) == 0)     return WXK_HOME;
    if (keyName.CmpNoCase(wxT("PAGEUP")) == 0)   return WXK_PAGEUP;
    if (keyName.CmpNoCase(wxT("PAGEDOWN")) == 0) return WXK_PAGEDOWN;
    if (keyName.CmpNoCase(wxT("END")) == 0)      return WXK_END;
    if (keyName.CmpNoCase(wxT("INSERT")) == 0)   return WXK_INSERT;

    // Accept a single ASCII character only.
    // Do not silently turn an unknown name such as "KP_Unknown" into 'K'.
    if (keyName.Len() == 1)
    {
        const int keyCode = static_cast<int>(keyName.GetChar(0));

        if (keyCode >= 0 && keyCode < 128)
            return keyCode;
    }

    return WXK_NONE;
}

// ----------------------------------------------------------------------------
wxString wxKeyBind::KeyModifierToString(int keyModifier)
// ----------------------------------------------------------------------------
{
    wxString result;

    if (keyModifier & wxACCEL_CMD)
        //result += wxT("Ctrl+");   //CB uses dashes
        result += wxT("Ctrl-");
#if defined(__WXMAC__) || defined(__WXCOCOA__)
    if (keyModifier & wxACCEL_CTRL)
        //result += wxT("XCtrl+");   //CB uses dashes
        result += wxT("XCtrl-");
#endif
    if (keyModifier & wxACCEL_ALT)
        //result += wxT("Alt+");   //CB uses dashes
        result += wxT("Alt-");
    if (keyModifier & wxACCEL_SHIFT)
        //result += wxT("Shift+");   //CB uses dashes
        result += wxT("Shift-");

    return result;
}

// ----------------------------------------------------------------------------
int wxKeyBind::StringToKeyModifier(const wxString &keyModifier)
// ----------------------------------------------------------------------------
{
    int mod = 0;

    // this search must be case-insensitive
    wxString str = keyModifier;
    str.MakeUpper();

    if (str.Contains(wxT("ALT")))
        mod |= wxACCEL_ALT;

    if (str.Contains(wxT("CTRL")))
        mod |= wxACCEL_CMD;
#if defined(__WXMAC__) || defined(__WXCOCOA__)
    if (str.Contains(wxT("XCTRL")))
        mod |= wxACCEL_CTRL;
#endif

    if (str.Contains(wxT("SHIFT")))
        mod |= wxACCEL_SHIFT;

    return mod;
}

// ----------------------------------------------------------------------------
int wxKeyBind::GetKeyModifier(wxKeyEvent &event)
// ----------------------------------------------------------------------------
{
// Note: wxWidgets2.6.2 returns false on modifier keys for Ctrl+Alt+UP/DOWN combination.
//       It returns Ctrl+Alt+PRIOR instead of UP/DOWN and shows false for ctrl & alt.
//       Same is true for Ctrl+Shift+UP/Down.
//       Alt+Shift+Up/Down work ok.

    int mod = 0;
    if (event.AltDown())
        mod |= wxACCEL_ALT;

    if (event.ControlDown())
        mod |= wxACCEL_CTRL;

    if (event.ShiftDown())
        mod |= wxACCEL_SHIFT;

#if defined(__WXMAC__) || defined(__WXCOCOA__)
    if (event.CmdDown())
        mod |= wxACCEL_CMD;
#endif

    return mod;
}

// ----------------------------------------------------------------------------
wxString wxKeyBind::GetKeyStrokeString(wxKeyEvent &event)
// ----------------------------------------------------------------------------
{
    const int keyCode = event.GetKeyCode(); // Support for wx3.2.6 NumKeyPad changes

    //- bug key stroke string = key modifiers (Ctrl, Alt or Shift) + key code
    // was turning numpad key codes into unicode chars.

    return KeyModifierToString(GetKeyModifier(event)) +
           KeyCodeToString(keyCode);
}

// ----------------------------------------------------------------------------
// wxKeyBind
// ----------------------------------------------------------------------------

bool wxKeyBind::MatchKey(const wxKeyEvent &key) const
{


    bool b = (key.GetKeyCode() == m_nKeyCode); // (ph 26/10/18) // bug:mangles laptop numpad nav keys

    // all the flags must match
    b &= (key.AltDown() == ((m_nFlags & wxACCEL_ALT) != 0));
    b &= (key.ControlDown() == ((m_nFlags & wxACCEL_CTRL) != 0));
    b &= (key.ShiftDown() == ((m_nFlags & wxACCEL_SHIFT) != 0));
#if defined(__WXMAC__) || defined(__WXCOCOA__)
    b &= (key.CmdDown() == ((m_nFlags & wxACCEL_CMD) != 0));
#endif

    return b;
}




// --------------------
// wxCmd
// --------------------

// static
// ----------------------------------------------------------------------------
wxCmd::wxCmdType *wxCmd::FindCmdType(int type)
// ----------------------------------------------------------------------------
{
    int found = -1;
    for (int i=0; i < m_nCmdTypes; i++)
        if (m_arrCmdType[i].type == type)
            found = i;

    if (found == -1)
        return NULL;
    return &m_arrCmdType[found];
}

// static
//// ----------------------------------------------------------------------------
//wxCmd *wxCmd::CreateNew(int type, int id)
//// ----------------------------------------------------------------------------
//{-v0.3 original code, removed in order to search by text, not id
//    wxCmdType *found = FindCmdType(type);
//    if (!found) return NULL;
//
//    // get the creation function address
//    wxCmdCreationFnc fnc = found->cmdCreateFnc;
//    wxASSERT(fnc);            // for debug builds
//    if (!fnc) return NULL;    // for release builds
//
//    // create the wxCmd-derived class & init it
//    wxCmd *ret = fnc(id);
//    //-wxASSERT(ret);            // for debug builds
//    if (!ret) return NULL;    // for release builds
//    ret->Update();
//
//    return ret;
//}
// --+v0.3---------------------------------------------------------------------
wxCmd *wxCmd::CreateNew(wxString cmdName, int type, int id, bool updateMnu)
// ----------------------------------------------------------------------------
{//+v0.3
    wxCmdType *found = FindCmdType(type);
    if (!found) return NULL;

    // get the creation function address
    wxCmdCreationFnc fnc = found->cmdCreateFnc;
    wxASSERT(fnc);            // for debug builds
    if (!fnc) return NULL;    // for release builds

    // create the wxCmd-derived class & init it
    wxCmd* ret = fnc(cmdName, id);
// ret == NULL can easily happen on runtime when dynamic menu entries are stored    //patch 2885
// into the config file and upon reload during startup the same entries don't       //patch 2885
// exist - usage of wxASSERT isn't probably a good idea here...                     //patch 2885
//    wxASSERT(ret);            // for debug builds                                     //patch 2885
    if (!ret) return NULL;    // for release builds
    if (updateMnu) ret->Update();

    return ret;
}


// static
// ----------------------------------------------------------------------------
void wxCmd::AddCmdType(int type, wxCmdCreationFnc f)
// ----------------------------------------------------------------------------
{
    if (FindCmdType(type) != NULL)
        return;        // already registered

    m_arrCmdType[m_nCmdTypes].type = type;
    m_arrCmdType[m_nCmdTypes].cmdCreateFnc = f;
    m_nCmdTypes++;
}

//// ----------------------------------------------------------------------------
//bool wxCmd::Save(wxConfigBase *p, const wxString &key, bool bCleanOld) const
//// ----------------------------------------------------------------------------
//{
//    // build the shortcut string separating each one with a "|"
//    wxString shortcuts;
//    for (int j=0; j < GetShortcutCount(); j++)
//        shortcuts += GetShortcut(j)->GetStr() + wxT("|");
//
//    // write the entry in the format NAME|DESC|SHORTCUT1|SHORTCUT2...|SHORTCUTn
//    wxString value = wxString::Format(wxT("%s|%s|%s"),
//                                    GetName().c_str(),
//                                    GetDescription().c_str(),
//                                    shortcuts.c_str());
//
//    // does the given key already exists ?
//    if (bCleanOld && p->Exists(key))
//        p->DeleteEntry(key);        // delete old stuff...
//
//    return p->Write(key, value);
//}
// ----------------------------------------------------------------------------
bool wxCmd::Save(wxConfigBase *p, const wxString &key, bool bCleanOld) const
// ----------------------------------------------------------------------------
{
    // build the shortcut string separating each one with a "|"
    wxString shortcuts;
    for (int j = 0; j < GetShortcutCount(); ++j)
        shortcuts += GetShortcut(j)->GetStr() + wxT("|");

    const wxString fullMenuPath = GetFullMenuPath(GetId());
    //wxLogMessage( _T("\nfullPath[%s]"), fullMenuPath.c_str() );

    // write the entry in the format NAME|DESC|SHORTCUT1|SHORTCUT2...|SHORTCUTn
    wxString value = wxString::Format(wxT("%s|%s|%s"),
                                    //GetName().c_str(),
                                    fullMenuPath.c_str(),       //(2007/6/15)
                                    GetDescription().c_str(),
                                    shortcuts.c_str());

    // does the given key already exists ?
    if (bCleanOld && p->Exists(key))
        p->DeleteEntry(key);        // delete old stuff...

    //wxLogMessage( _T("wxCmd::Save Key[%s] value[%s]"), key.c_str(), value.c_str() );
    return p->Write(key, value);
}
// ----------------------------------------------------------------------------
bool wxCmd::Load(wxConfigBase *p, const wxString &key)
// ----------------------------------------------------------------------------
{
    wxString fmt;
    if (!p->Read(key, &fmt, wxT("|")))
        return FALSE;

    // extract name & desc
    wxStringTokenizer tknzr(fmt, wxT("|"));
    m_strName = tknzr.GetNextToken();
    m_strDescription = tknzr.GetNextToken();
    if (m_strName.IsEmpty())
        return FALSE;    // this is an invalid entry...

    //(2007/6/15)
    wxString fullMenuPath = m_strName;
    m_strName = fullMenuPath.AfterLast(wxT('\\'));
    //wxLogMessage( _T("wxCmd::Load fullMenuPath[%s],m_strName[%s]"), fullMenuPath.c_str(), m_strName.c_str() );

    // the ID of this command should have been already set by the caller
    // which created us... niz, wxCmd::Create() called from wxKeyBinder::Load().
    // NB: the id came from the cfg file and it could be invalid if the menu
    // id's have changed or shifted. Happens a lot during CodeBlocks development
    // or when a plugin churns a bunch with the menu items.
    wxASSERT_MSG(m_nId != wxID_INVALID,
        wxT("ID must be set when creating this wxCmd"));
    // extract the keybindings...
    while (tknzr.HasMoreTokens())
    {   wxString token = tknzr.GetNextToken();
        AddShortcut(token);
    }

    Update();
    return TRUE;
}
// ----------------------------------------------------------------------------
bool wxCmd::LoadFromString(const wxString& cfgCmdString)
// ----------------------------------------------------------------------------
{
    wxString fmt = cfgCmdString;
    if (fmt.empty())
        return FALSE;

    // extract name & desc
    wxStringTokenizer tknzr(fmt, wxT("|"));
    m_strName = tknzr.GetNextToken();
    m_strDescription = tknzr.GetNextToken();
    if (m_strName.IsEmpty())
        return FALSE;    // this is an invalid entry...

    //(2007/6/15)
    wxString fullMenuPath = m_strName;
    m_strName = fullMenuPath.AfterLast(wxT('\\'));
    //wxLogMessage( _T("wxCmd::Load fullMenuPath[%s],m_strName[%s]"), fullMenuPath.c_str(), m_strName.c_str() );

    // the ID of this command should have been already set by the caller
    // which created us... niz, wxCmd::Create() called from wxKeyBinder::Load().
    // NB: the id came from the cfg file and it could be invalid if the menu
    // id's have changed or shifted. Happens a lot during CodeBlocks development
    // or when a plugin farts a bunch with the menu items.
    wxASSERT_MSG(m_nId != wxID_INVALID,
        wxT("ID must be set when creating this wxCmd"));

    // extract the keybindings...
    while (tknzr.HasMoreTokens())
    {
        wxString token = tknzr.GetNextToken();
        AddShortcut(token);
    }

    Update();
    return TRUE;
}

// --------------------
// wxCmdArray
// --------------------

// ----------------------------------------------------------------------------
void wxCmdArray::Remove(int n)
// ----------------------------------------------------------------------------
{
    if (n < 0 || n >= GetCount())
        return;

    // first, delete the memory associated with the n-th wxCmd
    delete Item(n);

    // then, remove that pointer from the array
    m_arr.RemoveAt(n);
}

// ----------------------------------------------------------------------------
void wxCmdArray::Clear()
// ----------------------------------------------------------------------------
{
    for (int i=GetCount(); i > 0; i--)
        Remove(0);

    // the array should be already empty
    m_arr.Clear();
}
// ----------------------------------------------------------------------------
//  wxKeyBinder
// ----------------------------------------------------------------------------
void wxKeyBinder::GetMenuItemAccStr(wxMenuItem* pMenuItem, wxString& MenuItemKeyStr)
// ----------------------------------------------------------------------------
{
    MenuItemKeyStr = wxEmptyString;
    wxAcceleratorEntry* pAcc = pMenuItem->GetAccel();
    if (pAcc)
    {   MenuItemKeyStr = wxKeyBind::KeyModifierToString(pAcc->GetFlags())
            + wxKeyBind::KeyCodeToString(pAcc->GetKeyCode()) ;
    }

    if (pAcc) delete pAcc;

}//GetMenuItemAccStr
// ----------------------------------------------------------------------------
int wxKeyBinder::MergeSubMenu(wxMenu* pMenu, int& modified)           //+v0.4.25
// ----------------------------------------------------------------------------
{
    // Recursively scan & record dynamically changed submenu items

    int changed = 0;
    size_t itemKnt = pMenu->GetMenuItemCount();
    for (size_t j=0; j<itemKnt; j++ )
    {
        // check each item on this subMenu
        wxMenuItem* pMenuItem = pMenu->FindItemByPosition(j);
        // recursively walk down to deepest submenu
        if ( pMenuItem->GetSubMenu() )
            MergeSubMenu( pMenuItem->GetSubMenu(), modified );
        //---------------------------
        // Now at deepest menu items
        //---------------------------
        // skip separater menu items
        if (pMenuItem->GetKind() == wxITEM_SEPARATOR) continue;
        int nMenuItemID = pMenuItem->GetId();

        // Skip any menu items beginning with numerics
        if (wxMenuCmd::IsNumericMenuItem(pMenuItem)) continue;

        //-wxString menuItemText = pMenuItem->GetText();
        // Find matching menu item in keybinder array of commands
        wxCmd*  pCmd = 0;
        changed = 0;
        wxString menuItemLabel = pMenuItem->GetItemLabelText().Trim();
        //-wxString menuItemKeyStr = pMenuItem->GetText().AfterFirst('\t');
        //^^ This will not work on wxGTK. GTK GetText() doesn't contain the shortcut
        wxString menuItemKeyStr;
        GetMenuItemAccStr(pMenuItem, menuItemKeyStr);

        // Find app menu item in our command array
        if ( (pCmd = GetCmd(nMenuItemID)) )
        {   // menu id found in KeyProfile Array
            // Has the menuitem Label changed?
            if ( pCmd->GetName().Trim() != menuItemLabel.Trim() )
               changed = 1;
            // Has the menuitem key or cmdArray key or both changed?
            if (menuItemKeyStr.IsEmpty() != (pCmd->GetShortcutCount()==0) )
                changed = 2;
            if (pCmd->GetShortcutCount()
                && (GetShortcutStr(nMenuItemID,0) != menuItemKeyStr)  )
               changed = 3;
            //if (changed)
            //{ //show change in log
            //    #ifdef LOGGING
            //     wxLogMessage( _T("MnuLabel[%s] MnuKey[%s]"), menuItemLabel.GetData(), menuItemKeyStr.GetData() );
            //     int n = pCmd->GetShortcutCount();
            //     wxLogMessage( _T("arrLabel[%s] arrKey[%s]"), pCmd->GetName().GetData(),
            //            n?GetShortcutStr(nMenuItemID,0).GetData():wxEmptyString );
            //   #endif //LOGGING
            //}
        }//if
        else
        {// menu item not found in KeyProfileArray
            changed = 4;
        }

        if ( changed )
        {   // menu item has been changed dynamically by core or plugins
            // remove old dynamic menu item from wxKeyProfileArray
            // update wxKeyProfileArray with dynamically changed menu item
            //-if ( menuItemLabel == wxT("Braces")) TRAP; //debugging
            // if the key bind was owned by other commands, remove the binding
            wxCmd* p = 0;
            while ((p = GetCmdBindTo(menuItemKeyStr) ))
            {
                // another command already owns this key bind...
                wxKeyBind tmp(menuItemKeyStr);
                int n;
                if (p->IsBindTo(tmp, &n))
                {   // remove array entry, but not actual menuitem key
                    //p->RemoveShortcut(n, false);
                    // ^^Have to update anyway else app menu items and array
                    //   menu items will never match causing constant update overhead
                    p->RemoveShortcut(n, true);
                }
            }//endwhile
            // if changed < 4(not found in array), remove the existing mis-matching wxCmd entry
            if (changed<4)
            {   // remove pre-existing mis-matching wxCmd
                RemoveCmd(pCmd);
            }
            // Leave Ctrl-C/V/X alone
            if ( (6 == menuItemKeyStr.Length())
                 && (menuItemKeyStr.StartsWith(_T("Ctrl-"))) )
            {
                const wxChar c = menuItemKeyStr.GetChar(5);
                switch(c)
                {
                    case _T('C'):
                        if (menuItemLabel.Matches(_T("Copy"))) continue;  /*falls through*/
                    case _T('V'):
                        if (menuItemLabel.Matches(_T("Paste"))) continue;  /*falls through*/
                    case _T('S'):
                        if (menuItemLabel.Matches(_T("Cut"))) continue;
                    default:
                        break;
                }
                if ( (c == _T('C')) || (c == _T('V')) || (c == _T('X')) )
                    continue;

            }
            // add the missing menu item as a wxCmd and update app menu items
            pCmd = wxCmd::CreateNew(menuItemLabel, wxMENUCMD_TYPE, nMenuItemID, false);
            if (not pCmd)
            { //CreateNew command did not allocate
                #if defined(LOGGING)
                wxLogMessage(_("Merge:CreateNew refused to allocate the new wxCmd"));
                wxLogMessage(_("Label[%s],ID[%d]"), menuItemLabel.GetData(), nMenuItemID);
                #endif
                return modified;
            }
            pCmd->m_strName = menuItemLabel;
            pCmd->m_strDescription = pMenuItem->GetHelp();
            AddCmd (pCmd);
            //*error* add shortcut without updateing menu item
            //*error* AddShortcut(nMenuItemID, menuItemKeyStr, false );
            // ^^Have to update anyway else app menu items and array
            //   menu items will never match causing constant update overhead
            AddShortcut(nMenuItemID, menuItemKeyStr, true );
            #ifdef LOGGING
                wxLogMessage(_("Merge change type[%d]:item[%zu]:id[%d]:@[%p]text[%s]key[%s]"), changed, j, nMenuItemID, pMenuItem, pMenuItem->GetItemLabel(), menuItemKeyStr);
            #endif
        }//if changed
        else
        {   // menu item has not changed
            #ifdef LOGGING
             //wxLogMessage(_("Merge:NoChange:pos[%d]id[%d]@[%p]text[%s]key[%s]"),j,nMenuItemID,pMenuItem,pMenuItem->GetText().c_str(), menuItemKeyStr.c_str() );
            #endif
        }
        // count changed array wxCmd items
        modified += (changed != 0) ;
    }//for itemKnt
    return modified;
}//mergeSubmenu
// ----------------------------------------------------------------------------
int wxKeyBinder::MergeDynamicMenuItems(wxMenuBar* pMenuBar)     //v0.4.25
// ----------------------------------------------------------------------------
{

    // Merge any new/dynamic CB Menu/Key items into the current wxKeyProfileArray
    // CB's plugins etc can add/change menu items and key assignments
    // cf: Help plugin which dynamically switches F1 assignment
    int changed = 0;
    //-if (m_arrHandlers.GetCount() == 0)
    //-    return false;    // we are not attached to any window... we can skip
    //-                    // this update...


   //menu bar item count (level 1)
    size_t nLevel1Knt = pMenuBar->GetMenuCount();
    for (size_t i=0; i < nLevel1Knt ;i++ )
    {
        wxMenu* pMenu = pMenuBar->GetMenu(i);
        MergeSubMenu(pMenu, changed);
    }
    #ifdef LOGGING
     wxLogMessage( _("MergeDynamicMenuItems() modified %d items"), changed );
    #endif //LOGGING

    // ---------------------------------------------------------------
    // Loop through the wxCmdArray and remove any wxCmds that have no
    // associated menu item. This is caused by dynamic menu items changing their id
    // or being removed by plugins and projects
    // ---------------------------------------------------------------
    wxCmdArray* pCmdArray = GetArray();
    for (int i=0; i < GetCmdCount(); i++)
    {
        wxCmd* pCmd = pCmdArray->Item(i);
        if (not pMenuBar->FindItem( pCmd->GetId(), NULL) )
        {
            #if defined(LOGGING)
            wxLogMessage( _("Merge Removing old[%s][%d]"), pCmd->GetName().c_str(),pCmd->GetId() );
            #endif
            RemoveCmd( pCmd);
            ++changed;
        }
    }

    // return true when any menu items changed
    return (changed);

}//MergeDynamicMenuItems
// ----------------------------------------------------------------------------
void wxKeyBinder::UpdateSubMenu(wxMenu* pMenu)
// ----------------------------------------------------------------------------
{
    // Recursively update shortcut keys for sub menu items

    size_t itemKnt = pMenu->GetMenuItemCount();
    for (size_t j=0; j<itemKnt; j++ )
    {
        // check each item on this subMenu
        wxMenuItem* pMenuItem = pMenu->FindItemByPosition(j);
        // recursively walk down to deepest submenu
        if ( pMenuItem->GetSubMenu() )
            UpdateSubMenu( pMenuItem->GetSubMenu() );
        // Now at deepest menu items
        int nMenuItemID = pMenuItem->GetId();
        // Find item in array of keybinder commands
        int k=0;
        wxString menuItemKeyStr;
        if ( -1 != ( k = FindCmd(nMenuItemID) ) )   //item found in local array
        {
            GetMenuItemAccStr(pMenuItem, menuItemKeyStr);
            #ifdef LOGGING
             //wxLogMessage(_("UpdateAllCmd ById Ok on:[%d][%s] key[%s]"),
             //       pMenuItem->GetId(),pMenuItem->GetLabel().Trim().GetData(), menuItemKeyStr.GetData() );
            #endif
            m_arrCmd.Item(k)->Update(pMenuItem);
            // *^^* pMenuItem will be invalid now if item was destroyed/created/updated **
        }
        else{
            // a menu id failed to match any in the file bindings
            // it's probably had its id dynamically (re)assigned
            if (not (pMenuItem->GetKind() == wxITEM_SEPARATOR)
                && (not wxMenuCmd::IsNumericMenuItem(pMenuItem)) )
            {
                #ifdef LOGGING
                 wxLogMessage(_("UpdateAllCmd ById Failed on:[%d][%s]"), pMenuItem->GetId(), pMenuItem->GetItemLabel().GetData() );
                #else
                 Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("KeyBinder failed UpdateByID on[%d][%s]"), nMenuItemID, pMenuItem->GetItemLabel().GetData()));
                #endif
////                // When a .ini id cannot be found: (menu ids have shifted)
////                // The following code causes real problems when menu labels are duplicates
////                // eg,. if two items have the label "cut", the second ones key will
////                // be assigned to the first. So "Cut" (line)Ctrl-T will end up in
////                // "Cut"(marked), overwriting Ctrl-X
////                // Try to find the wxCmd that matches this menu label
////                if (-1 != (k = FindMatchingName(pMenuItem->GetLabel().Trim() )))
////                {   // Look for wxCmd with this menu label
////                    #ifdef LOGGING
////                     wxLogMessage(_("UpdateAllCmd ByLabel on:%d:%d:%p:[%s] key[%s]"),
////                            j,k,pMenuItem,pMenuItem->GetLabel().Trim().GetData(), menuItemKeyStr.GetData() );
////                    #endif
////                    m_arrCmd.Item(k)->Update(pMenuItem);
////                }//if
            }//if (not...
        }
    }//rof
}//updateSubmenu
// ----------------------------------------------------------------------------
void wxKeyBinder::UpdateAllCmd(wxMenuBar* pMenuBar)
// ----------------------------------------------------------------------------
{
    //! Updates all the commands on the menu

    //menu bar item count (level 1)
    size_t nLevel1Knt = pMenuBar->GetMenuCount();
    for (size_t i=0; i < nLevel1Knt ;i++ )
    {
        wxMenu* pMenu = pMenuBar->GetMenu(i);
        UpdateSubMenu(pMenu);
    }
}
// ----------------------------------------------------------------------------
wxWindow* wxKeyBinder::FindWindowRecursively(const wxWindow* parent, const wxWindow* handle)
// ----------------------------------------------------------------------------
{//+v0.4.4
    if ( parent )
    {
        // see if this is the one we're looking for
        if ( parent == handle )
            return (wxWindow *)parent;

        // It wasn't, so check all its children
        for ( wxWindowList::compatibility_iterator node = parent->GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            // recursively check each child
            wxWindow *win = (wxWindow *)node->GetData();
            wxWindow *retwin = FindWindowRecursively(win, handle);
            if (retwin)
                return retwin;
        }
    }

    // Not found
    return NULL;
}
// ----------------------------------------------------------------------------
wxWindow* wxKeyBinder::winExists(wxWindow *parent)
// ----------------------------------------------------------------------------
{ //+v0.4.4

    if ( !parent )
    {
        return NULL;
    }

    // start at very top of wx's windows
    for ( wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
          node;
          node = node->GetNext() )
    {
        // recursively check each window & its children
        wxWindow* win = node->GetData();
        wxWindow* retwin = FindWindowRecursively(win, parent);
        if (retwin)
            return retwin;
    }

    return NULL;
}
// ----------------------------------------------------------------------------
//  wxKeyBinder ImportMenuBarCmd
// ----------------------------------------------------------------------------
void wxKeyBinder::ImportMenuBarCmd(wxMenuBar *p)
{
    wxMenuShortcutWalker wlkr;

    // it's very easy performing this task using a wxMenuShortcutWalker  :-)
    wlkr.ImportMenuBarCmd(p, &m_arrCmd);
}
// ----------------------------------------------------------------------------
//  wxKeyBinder Save
// ----------------------------------------------------------------------------
bool wxKeyBinder::Save(wxConfigBase *cfg, const wxString &key, bool bCleanOld) const
{
    wxString basekey = (key.IsEmpty() ? wxString(wxT("")) : wxString(key + wxT("/")));
    bool b = TRUE;

    // does the given key already exists ?
    if (bCleanOld && cfg->Exists(basekey))
        cfg->DeleteGroup(basekey);        // delete old stuff...

    for (int i=0; i < m_arrCmd.GetCount(); i++) {

        wxCmd *curr = m_arrCmd.Item(i);

        // write the key in the format: bindID-typeID
        wxString keyname = wxString::Format(wxT("%s%s%d-type%d"),
            basekey.wx_str(), wxCMD_CONFIG_PREFIX, curr->GetId(), curr->GetType());

        // save this wxCmd...
        b &= curr->Save(cfg, keyname);
    }

    return b;
}//Save

/// --+v0.3--------------------------------------------------------------------
bool wxKeyBinder::GetNameandDescription(wxConfigBase* p, const wxString& key, wxString& strName, wxString& strDescription)
// ----------------------------------------------------------------------------
{
    wxString fmt;
    if (!p->Read(key, &fmt, wxT("|")))
        return FALSE;

    // extract name & desc
    wxStringTokenizer tknzr(fmt, wxT("|"));
    strName = tknzr.GetNextToken();
    strDescription = tknzr.GetNextToken();
    if (strName.IsEmpty())
        return FALSE;    // this is an invalid entry...
    return TRUE;
}
// ----------------------------------------------------------------------------
//  wxKeyBinder Load
// ----------------------------------------------------------------------------
bool wxKeyBinder::Load(wxConfigBase *p, const wxString &key)
{
    wxString str;
    bool cont;
    bool b = TRUE;
    int total = 0;
    long idx;

    // before starting...
    p->SetPath(key);
    m_arrCmd.Clear();

    cont = p->GetFirstEntry(str, idx);
    while (cont) {

        // try to decode this entry
        if (str.StartsWith(wxCMD_CONFIG_PREFIX))    // "bind" string ?
        {
            wxString id(str.BeforeFirst(wxT('-')));
            wxString type(str.AfterFirst(wxT('-')));
            id = id.Right(id.Len()-wxString(wxCMD_CONFIG_PREFIX).Len());
            type = type.Right(type.Len()-wxString(wxT("type")).Len());

            if (str.StartsWith(_T("bind-"))) //oops, negative menu id //(2015/08/21)
            {
                id = _T("-") + str.Mid(5).BeforeFirst(_T('-')); // eg. "bind-31782-type4660="
                int typepos = str.Find(_T("type"));
                if (typepos != wxNOT_FOUND)
                    type = str.Mid(typepos+4).BeforeFirst(_T('='));
            }

            // is this a valid entry ?
            if (id.IsNumber() && type.IsNumber()
                //&& p->GetEntryType(str) == wxConfigBase::Type_String)
                )
            {
                // we will interpret this group as a command ID
                int nid = wxAtoi(id);
                int ntype = wxAtoi(type);

                // create & load this command
                //-v0.3 wxCmd *cmd = wxCmd::CreateNew(ntype, nid);
                //+v0.3get command name and descriptions string
                wxString cmdName;
                wxString cmdDesc;
                if (not GetNameandDescription(p, str, cmdName, cmdDesc))
                {  //-v0.3 cont = FALSE; continue to load next command
                    //-break;
                }
                wxCmd* cmd = wxCmd::CreateNew(cmdName, ntype, nid);
                if (cmd && cmd->Load(p, str))
                 {
                    m_arrCmd.Add(cmd);        // add to the array
                    total++;
                 }
            }//if(id.
        }//if(str.

        // proceed with next entry (if it does exist)
        cont = p->GetNextEntry(str, idx);
    }//while

    return (b && total > 0);
}//Load
// ----------------------------------------------------------------------------
bool wxKeyBinder::LoadFromString(const wxString& cfgCmdString)
// ----------------------------------------------------------------------------
{
    // example cfgCmdString
    // bind1044-type4660=Build\\Build|Build current project|Ctrl-F9|F9|

    wxString str = cfgCmdString;
    //-bool cont;
    bool b = TRUE;
    int total = 0;
    //-long idx;

    // before starting...
    //p->SetPath(key);
    // dont clear or we'll wipe out our previus key definitions
    //m_arrCmd.Clear();

    //-cont = p->GetFirstEntry(str, idx);
    switch(1)
    {
        default:
        // try to decode this entry
        if (str.StartsWith(wxCMD_CONFIG_PREFIX))    // "bind" string ?
        {
            wxString id(str.BeforeFirst(wxT('-')));
            wxString type(str.AfterFirst(wxT('-')));
            type = type.BeforeFirst(_T('='));
            type = type.Mid(4);
            id = id.Right(id.Len()-wxString(wxCMD_CONFIG_PREFIX).Len());

            // is this a valid entry ?
            if (id.IsNumber() && type.IsNumber()
                //&& p->GetEntryType(str) == wxConfigBase::Type_String)
                )
            {
                // we will interpret this group as a command ID
                int nid = wxAtoi(id);
                int ntype = wxAtoi(type);

                // create & load this command
                //-v0.3 wxCmd *cmd = wxCmd::CreateNew(ntype, nid);
                //+v0.3get command name and descriptions string
                wxString cmdName;
                wxString cmdDesc;
                //if (not GetNameandDescription(p, str, cmdName, cmdDesc))
                //{  //-v0.3 cont = FALSE; continue to load next command
                //    //-break;
                //}
                cmdDesc = str.AfterFirst(_T('|'));
                cmdDesc = cmdDesc.BeforeFirst(_T('|'));
                cmdName = str.After(_T('\\'));
                cmdName = cmdName.BeforeFirst(_T('|'));

                wxCmd* cmd = wxCmd::CreateNew(cmdName, ntype, nid);
                if (cmd && cmd->LoadFromString( str))
                 {
                    m_arrCmd.Add(cmd);        // add to the array
                    total++;
                 }
            }//if(id.
        }//if(str.

        // proceed with next entry (if it does exist)
        //cont = p->GetNextEntry(str, idx);
    }//for only once

    return (b && total > 0);
}//wxKeyBinder::LoadFromString

// ----------------------------------------------------------------------------
// wxKeyProfile
// ----------------------------------------------------------------------------

bool wxKeyProfile::Save(wxConfigBase *cfg, const wxString &key, bool bCleanOld) const
{
    // we will encode our name into the key used to group all this profile
    wxString basekey = key.IsEmpty() ? wxString(wxT("")) : wxString(key + wxT("/"));

    // does the given key already exists ?
    if (bCleanOld && cfg->Exists(basekey))
        cfg->DeleteGroup(basekey);        // delete old stuff...

    // write our name & description into a specific key
    if (!cfg->Write(basekey + wxT("/desc"), GetDesc()))
        return FALSE;
    if (!cfg->Write(basekey + wxT("/name"), GetName()))
        return FALSE;

    // tell wxKeyBinder to save all keybindings into a group with our name
    // and also tell it NOT to delete the given key if it already exists
    // because it ALWAYS exists since we've just created it...
    return wxKeyBinder::Save(cfg, basekey, FALSE);
}

// ----------------------------------------------------------------------------
bool wxKeyProfile::Load(wxConfigBase *p, const wxString &key)
// ----------------------------------------------------------------------------
{
    // wxKeyProfile is derived from wxKeyBinder

    p->SetPath(key);        // enter into this group

    wxString name;
    wxString desc;

    // do we have valid DESC & NAME entries ?
    if (p->HasEntry(wxT("desc")) && p->HasEntry(wxT("name"))) {

        if (!p->Read(wxT("desc"), &desc))
            return FALSE;
        if (!p->Read(wxT("name"), &name))
            return FALSE;

        // check for valid name & desc
        if (name.IsEmpty())
            return FALSE;

        SetName(name);
        SetDesc(desc);

        // load from current path (we cannot use '.')
        return wxKeyBinder::Load(p, wxT("../") + key);
    }

    return FALSE;    // no valid DESC/NAME entry...
}//Load


// ----------------------------------------------------------------------------
// wxKeyProfileArray
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
bool wxKeyProfileArray::Save(wxConfigBase *cfg, const wxString &key, bool bCleanOld) const
// ----------------------------------------------------------------------------
{
    wxString basekey = (key.IsEmpty()) ? wxString(wxT("")) : wxString(key + wxT("/"));
    bool b = true;

    cfg->SetPath(key);

    if (!cfg->Write(basekey + wxT("nSelProfile"), m_nSelected))
        return false;

    for (int i=0; i<GetCount(); i++)
    {
        #if defined(LOGGING)
        wxLogMessage(_("wxKeyProfileArray::Save profile[%d]"),i);
        #endif
        // save all our elements into a subkey of the given key
        if (!Item(i)->Save(cfg, basekey + wxKEYPROFILE_CONFIG_PREFIX + wxString::Format(wxT("%d"), i), bCleanOld))
            b = false;
    }//for

    // if required, remove any previously stored key profile...
    if (bCleanOld) {

        // the Save() calls could have changed our current path...
        cfg->SetPath(key);

        // enumerate all groups
        wxString str;
        long n, idx;

        bool cont = cfg->GetFirstGroup(str, idx);
        while (cont) {
            if (str.StartsWith(wxKEYPROFILE_CONFIG_PREFIX)) {

                // extract the id of this group...
                wxString id=str.Right(str.Len()-wxString(wxKEYPROFILE_CONFIG_PREFIX).Len());
                id.ToLong(&n);

                if (n >= GetCount()) {

                    // this is a profile which was saved in a previous session
                    // but which has now been removed by the user... remove it
                    // from our config settings...
                    cfg->DeleteGroup(str);

                    // re-start our enumeration; otherwise GetNextGroup() won't work...
                    cont = cfg->GetFirstGroup(str, idx);
                    if (!cont) break;
                }
            }

            // proceed with next one...
            cont = cfg->GetNextGroup(str, idx);
        }
    }

    return b;
}//Save

// ----------------------------------------------------------------------------
bool wxKeyProfileArray::Load(wxConfigBase *p, const wxString &key)
// ----------------------------------------------------------------------------
{
    wxKeyProfile tmp;
    wxString str;
    bool cont;
    long idx;

    // before starting...
    p->SetPath(key);

    if (!p->Read(wxT("nSelProfile"), &m_nSelected))
        return FALSE;

    cont = p->GetFirstGroup(str, idx);
    while (cont) {

        // try to decode this group name
        if (str.StartsWith(wxKEYPROFILE_CONFIG_PREFIX)) {

            // is this a valid entry ?
            if (!tmp.Load(p, str))
                return FALSE;

            Add(new wxKeyProfile(tmp));        // yes, it is; add it to the array
        }

        // set the path again (it could have been changed...)
        p->SetPath(key);

        // proceed with next entry (if it does exist)
        cont = p->GetNextGroup(str, idx);
    }

    return TRUE;
}//Load

// ----------------------------------------------------------------------------
// wxKeyMonitorTextCtrl
// ----------------------------------------------------------------------------
void wxKeyMonitorTextCtrl::OnKey(wxKeyEvent &event)
{
    //    // **Debugging** // (ph 26/10/18)
    //    const wxString stroke = wxKeyBind::GetKeyStrokeString(event);
    //    wxString LogMessage; LogMessage =wxString::Format(
    //        wxT("KeyBinder: %s code=%d ctrl=%d alt=%d shift=%d stroke=[%s]"),
    //        event.GetEventType() == wxEVT_KEY_DOWN //code
    //            ? wxT("DOWN") : wxT("UP"),          //ctrl
    //        event.GetKeyCode(),
    //        static_cast<int>(event.ControlDown()), //ctrl
    //        static_cast<int>(event.AltDown()),      //alt
    //        static_cast<int>(event.ShiftDown()),    //shift
    //        stroke.c_str()                          //char
    //    );
    //    Manager::Get()->GetLogManager()->DebugLog(LogMessage);
    // **Debugging** end

    // backspace cannot be used as shortcut key...
    if (event.GetKeyCode() == WXK_BACK) {

        // this text ctrl contains something and the user pressed backspace...
        // we must delete the keypress...
        Clear();
        return;
    }

    if (event.GetEventType() == wxEVT_KEY_DOWN ||
        (event.GetEventType() == wxEVT_KEY_UP && !IsValidKeyComb())) {

        // the user pressed some key combination which must be displayed
        // in this text control.... or he has just stopped pressing a
        // modifier key like shift, ctrl or alt without adding any
        // other alphanumeric char, thus generating an invalid keystroke
        // which must be cleared out...
        //-SetValue(wxKeyBind::GetKeyStrokeString(event));
        //-SetInsertionPointEnd();

        // Command must begin with 'Ctrl-' 'Alt-' or 'Shift-' F1-F??
        wxString keyStrokeString = wxKeyBind::GetKeyStrokeString(event);
        #if defined(LOGGING)
            wxLogMessage( _T("KeyStrokString[%s]"),keyStrokeString.c_str() );
        #endif

        switch (keyStrokeString.IsEmpty() ? 1 : 0)
        {
            case 1: break;
            default:
            if (keyStrokeString.Length() < 2) { keyStrokeString.Clear(); break;}
            if ( (keyStrokeString[0] == 'F') && (keyStrokeString.Mid(1,1).IsNumber()) ) break;
            if ( not (validCmdPrefixes.Contains(keyStrokeString.BeforeFirst('-'))) )
            {   keyStrokeString.Clear(); break; }
            // if not accepted as accelerator, disallow it. 2020/05/30
            if ( keyStrokeString.AfterLast(wxT('-')).Length() )
            {
                wxAcceleratorEntry accelEntry;
                bool ok = accelEntry.FromString(keyStrokeString);

                // **Debugging** // (ph 26/10/18)
                //    wxString LogMessage; LogMessage = wxString::Format(
                //        wxT("KeyBinder: parse [%s] -> %d"),
                //        keyStrokeString.c_str(),
                //        static_cast<int>(ok)
                //    );
                //    Manager::Get()->GetLogManager()->DebugLog(LogMessage);
                // **Debugging** end

                if (not ok) { keyStrokeString.Clear(); break; }
            }
        }//switch
        SetValue( keyStrokeString );
        SetInsertionPointEnd();
    }
}
// ----------------------------------------------------------------------------
// wxKeyConfigPanel - BUILD functions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
wxKeyConfigPanel::wxKeyConfigPanel(wxWindow* parent,
                int buildMode, wxWindowID id,
                const wxPoint& pos, const wxSize& size,
                long style, const wxString& name)
// ----------------------------------------------------------------------------
                : wxPanel(parent, id, pos, size, style, name)
{
    m_nBuildMode = buildMode;
    m_bProfileHasBeenModified = FALSE;

    wxASSERT_MSG((m_nBuildMode & wxKEYBINDER_USE_LISTBOX) ||
                (m_nBuildMode & wxKEYBINDER_USE_TREECTRL),
                wxT("You must specify one of the two !!"));
    wxASSERT_MSG(!((m_nBuildMode & wxKEYBINDER_USE_LISTBOX) &&
                 (m_nBuildMode & wxKEYBINDER_USE_TREECTRL)),
                wxT("You cannot specify them both !!"));

    // build everything
    BuildCtrls();
    wxSizer *column1 = BuildColumn1();
    wxSizer *column2 = BuildColumn2();
    wxSizer *main = BuildMain(column1, column2,
        (m_nBuildMode & wxKEYBINDER_SHOW_APPLYBUTTON) != 0);

    // set the final sizer as window's sizer
    SetSizer(main);
    main->SetSizeHints(this);

    // set up the controls: the user of the panel must call one of the
    // ImportXXXX() functions to enable the use of the panel !!!!
    GetMainCtrl()->SetFocus();
    UpdateButtons();

    // init capture of ok button //(2019/04/6)
    m_pOK = nullptr;
}
// ----------------------------------------------------------------------------
wxKeyConfigPanel::~wxKeyConfigPanel()
// ----------------------------------------------------------------------------
{
    // with the AddXXXXX functions we created wxKeyProfiles which we
    // then added into the m_pKeyProfiles combobox... we now must delete them.
    for (size_t i=0; i < m_pKeyProfiles->GetCount(); i++) {
        wxKeyProfile *data = (wxKeyProfile *)m_pKeyProfiles->GetClientData(i);

        // we can delete the client data safely because wxComboBox will leave
        // the client data field untouched...
        if (data) delete data;
    }
    if (m_pOK) //(2019/04/6)
    {
        #if wxVERSION_NUMBER >= 3000    //(wx28 2019/04/19)
            m_pOK->GetEventHandler()->Disconnect(wxID_OK, wxEVT_BUTTON, wxCommandEventHandler( wxKeyConfigPanel::OnApplyChanges), NULL, this);
        #else
            m_pOK->GetEventHandler()->Disconnect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( wxKeyConfigPanel::OnApplyChanges), NULL, this);
        #endif
        m_pOK = nullptr;
    }
}
// ----------------------------------------------------------------------------
void wxKeyConfigPanel::BuildCtrls()
// ----------------------------------------------------------------------------
{
    if (m_nBuildMode & wxKEYBINDER_USE_TREECTRL) {

        // use a wxTreeCtrl to show the commands hierarchy
        m_pCommandsTree = new wxTreeCtrl(this, wxKEYBINDER_COMMANDS_BOX_ID, wxDefaultPosition,
                                    wxDefaultSize, wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);
    } else {

        // use a combobox + a listbox
        m_pCommandsList = new wxListBox(this, wxKEYBINDER_COMMANDS_BOX_ID, wxDefaultPosition,
                                    wxDefaultSize);
        m_pCategories = new wxComboBox(this, wxKEYBINDER_CATEGORIES_ID,
                                wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                0, NULL, wxCB_READONLY);
    }

    m_pKeyField = new wxKeyMonitorTextCtrl(this, wxKEYBINDER_KEY_FIELD_ID);
    m_pBindings = new wxListBox(this, wxKEYBINDER_BINDINGS_BOX_ID);

    m_pAssignBtn = new wxButton(this, wxKEYBINDER_ASSIGN_KEY_ID, _("&Add"));
    m_pRemoveBtn = new wxButton(this, wxKEYBINDER_REMOVE_KEY_ID, _("&Remove"));
    m_pRemoveAllBtn = new wxButton(this, wxKEYBINDER_REMOVEALL_KEY_ID, _("Remove all"));

    m_pCurrCmdField = new wxStaticText(this, -1, wxT(""), wxDefaultPosition,
        wxSize(-1, 20), wxSIMPLE_BORDER | wxST_NO_AUTORESIZE | wxALIGN_CENTRE);

#if wxCHECK_VERSION(3, 3, 0)
    const bool isDark = wxSystemSettings::GetAppearance().IsDark();
#else
    const bool isDark = false;
#endif

    // we won't make it completely white (or black) because it must be clear to the user that this
    // is not a text control...
    m_pCurrCmdField->SetBackgroundColour(isDark ? wxColour(55, 55, 55) : wxColour(200, 200, 200));

#ifdef __WXGTK__
    m_pDescLabel = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition,
                                 wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
#else
    m_pDescLabel = new wxStaticText(this, -1, wxT(""), wxDefaultPosition,
        wxSize(-1, 40), wxSIMPLE_BORDER | wxST_NO_AUTORESIZE);
    m_pDescLabel->SetBackgroundColour(isDark ? *wxBLACK : *wxWHITE);
#endif

    // KEY PROFILES
    // create the key profiles combobox & panel
    m_bEnableKeyProfiles = TRUE;

    // the style of a combobox must be set at the beginning:
    // you cannot change the wxCB_READONLY flag presence later...
    // VERY IMPORTANT: *NEVER* ADD THE CB_SORT STYLE:
    // IT WOULD GIVE US GREAT PROBLEMS WHEN EDITING THE KEYPROFILE...
    long style = (m_nBuildMode & wxKEYBINDER_ENABLE_PROFILE_EDITING) ? 0 : wxCB_READONLY;
    m_pKeyProfiles = new wxComboBox(this, wxKEYBINDER_KEYPROFILES_ID,
                                wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                0, NULL, style);

    wxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_pKeyProfiles, 6, wxGROW);

    if (m_nBuildMode & wxKEYBINDER_SHOW_ADDREMOVE_PROFILE) {

        // create the Add & remove profile buttons
        sizer->Add(new wxButton(this, wxKEYBINDER_ADD_PROFILEBTN_ID, _("Add new")), 0,
                            wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
        sizer->Add(new wxButton(this, wxKEYBINDER_REMOVE_PROFILEBTN_ID, _("Remove")), 0,
                            wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    }

    m_pKeyProfilesSizer = new wxBoxSizer(wxVERTICAL);
    m_pKeyProfilesSizer->Add(new wxStaticText(this, -1, _("Key profile:")), 0, wxGROW | wxALL, 5);
    m_pKeyProfilesSizer->Add(sizer, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    m_pKeyProfilesSizer->Add(new wxStaticLine(this, -1), 0, wxGROW | wxALL, 5);
}

// ----------------------------------------------------------------------------
wxSizer *wxKeyConfigPanel::BuildColumn1()
// ----------------------------------------------------------------------------
{
    // FIRST COLUMN: "Groups"
    wxBoxSizer *column1 = new wxBoxSizer(wxVERTICAL);

    if (m_nBuildMode & wxKEYBINDER_USE_TREECTRL) {

        // add the treectrl
        column1->Add(new wxStaticText(this, -1, _("Commands:")), 0, wxGROW | wxALL, 5);
        column1->Add(m_pCommandsTree, 1, wxGROW | wxRIGHT | wxLEFT | wxBOTTOM, 5);

    } else {

        // add a combobox + listbox
        column1->Add(new wxStaticText(this, -1, _("Categories:")), 0, wxGROW | wxALL, 5);
        column1->Add(m_pCategories, 1, wxGROW | wxRIGHT | wxLEFT | wxBOTTOM, 5);

        column1->Add(new wxStaticText(this, -1, _("Commands:")), 0,
                                    wxGROW | wxRIGHT | wxLEFT | wxBOTTOM, 5);
        column1->Add(m_pCommandsList, 5, wxGROW | wxRIGHT | wxLEFT | wxBOTTOM, 5);
    }

    return column1;
}

// ----------------------------------------------------------------------------
wxSizer *wxKeyConfigPanel::BuildColumn2()
// ----------------------------------------------------------------------------
{
    // SECOND COLUMN: wxT("Current keys") + "Press new shortcut key"
    wxBoxSizer *column2 = new wxBoxSizer(wxVERTICAL);
    column2->Add(new wxStaticText(this, -1, _("Current shortcuts:")), 0, wxGROW | wxALL, 5);
    column2->Add(m_pBindings, 2, wxGROW | wxRIGHT | wxLEFT, 5);

    wxBoxSizer *removebtns = new wxBoxSizer(wxHORIZONTAL);
    removebtns->Add(m_pRemoveBtn, 1, wxGROW | wxALL, 5);
    removebtns->Add(m_pRemoveAllBtn, 1, wxGROW | wxALL, 5);

    column2->Add(removebtns, 0, wxGROW);
    //column2->Add(new wxStaticLine(this, -1), 0, wxGROW | wxALL, 10);
    column2->Add(new wxStaticText(this, -1, _("New shortcut:")), 0, wxGROW | wxALL, 5);
    column2->Add(m_pKeyField, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    column2->Add(new wxStaticText(this, -1, _("Currently assigned to:")), 0, wxGROW | wxALL, 5);
    column2->Add(m_pCurrCmdField, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    column2->Add(m_pAssignBtn, 0, wxGROW | wxALL, 5);

    return column2;
}

// ----------------------------------------------------------------------------
wxSizer *wxKeyConfigPanel::BuildMain(wxSizer *column1, wxSizer *column2, bool bApplyBtn)
// ----------------------------------------------------------------------------
{
    // set up the column container
    wxBoxSizer *cont = new wxBoxSizer(wxHORIZONTAL);
    cont->Add(column1, 4, wxGROW);
    cont->Add(1, 1, 0, wxGROW);
    cont->Add(column2, 4, wxGROW);

    // create the main sizer
    wxBoxSizer *main = new wxBoxSizer(wxVERTICAL);
    main->Add(m_pKeyProfilesSizer, 0, wxGROW);
    main->Add(cont, 5, wxGROW);
    main->Add(new wxStaticLine(this, -1), 0, wxGROW | wxALL, 5);

    // key description
    main->Add(new wxStaticText(this, -1, _("Description:")), 0, wxGROW | wxALL, 5);
#ifdef __WXGTK__
    main->Add(m_pDescLabel, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
#else
    main->Add(m_pDescLabel, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
#endif

    // if required, add Apply and Cancel buttons
    if (bApplyBtn) {

        wxBoxSizer *btn = new wxBoxSizer(wxHORIZONTAL);
        wxButton *applyBtn = new wxButton(this, wxID_APPLY,  _("A&pply"));
        wxButton *cancelBtn = new wxButton(this, wxID_CANCEL, _("C&ancel"));

        // create the button panel with some interleaved spacer
        btn->Add(1, 1, 1, wxGROW);
        btn->Add(applyBtn, 4, wxGROW | wxALL, 5);
        btn->Add(1, 1, 1, wxGROW);
        btn->Add(cancelBtn, 4, wxGROW | wxALL, 5);
        btn->Add(1, 1, 1, wxGROW);

        main->Add(1, 1, 0, wxGROW);
        main->Add(btn, 1, wxGROW | wxALL, 5);
    }

    return main;
}

// ----------------------------------------------------------------------------
// wxKeyConfigPanel - IMPORT functions
// ----------------------------------------------------------------------------
void wxKeyConfigPanel::ImportMenuBarCmd(wxMenuBar *p, const wxString &rootname)
{
    // do some std things...
    Reset();

    // the wxKeyBinder class can easily generate a tree structure...
    if (IsUsingTreeCtrl())
    {

        AddRootIfMissing(rootname);

        wxMenuTreeWalker wlkr;
        wlkr.FillTreeCtrl(p, m_pCommandsTree, rootname);

        // expand the root (just for aesthetic/comfort reasons)...
        m_pCommandsTree->Expand(m_pCommandsTree->GetRootItem());

    } else //using combo box
    {

        wxMenuComboListWalker wlkr;
        wlkr.FillComboListCtrl(p, m_pCategories);

        // select the first item (just for aesthetic/comfort reasons)...
        m_pCategories->SetSelection(0);

        wxCommandEvent fake;
        OnCategorySelected(fake);
    }
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::ImportKeyProfileCmd(const wxKeyProfile &toimport,
                                           const wxString &rootname)
// ----------------------------------------------------------------------------
{
    // do some std things...
    Reset();

    if (IsUsingTreeCtrl()) {

        AddRootIfMissing(rootname);
        wxTreeItemId rootid = m_pCommandsTree->GetRootItem();

        // scan all the commands of the key binder...
        const wxCmdArray *arr = toimport.GetArray();
        for (int i=0; i < (int)arr->GetCount(); i++) {

            // to each tree branch attach a wxTreeItemData containing
            // the ID of the menuitem which it represents...
            wxExTreeItemData *treedata = new wxExTreeItemData(arr->Item(i)->GetId());

            // create the new item in the tree ctrl
            m_pCommandsTree->AppendItem(rootid, arr->Item(i)->GetName(), -1, -1, treedata);
        }

        // expand the root (just for aesthetic/comfort reasons)...
        m_pCommandsTree->Expand(m_pCommandsTree->GetRootItem());

    } else {

        const wxCmdArray *arr = toimport.GetArray();
        for (int i=0; i < (int)arr->GetCount(); i++) {

            // create a list of items containing as untyped client data
            // (void*) the INT which is their ID...
            m_pCommandsList->Append(arr->Item(i)->GetName(),
                                    reinterpret_cast<void*>(arr->Item(i)->GetId()));
        }

        // in the categories combobox just add a generic title
        m_pCategories->Append(_("Generic"));
    }
}
// ----------------------------------------------------------------------------
// wxKeyConfigPanel - MISCELLANEOUS functions
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::AddProfile(const wxKeyProfile &p)
// ----------------------------------------------------------------------------
{
    // add a new profile to the array
    m_pKeyProfiles->Append(p.GetName(), (void *)(new wxKeyProfile(p)));

    if (m_pKeyProfiles->GetCount() == 1)

        // the profile we added is the only one present... select it
        SetSelProfile(0);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::AddProfiles(const wxKeyProfileArray &arr)
// ----------------------------------------------------------------------------
{
    // copy the given profiles into the listbox data list
    for (int i=0; i < arr.GetCount(); i++) {
        wxKeyProfile *copy = new wxKeyProfile(*arr.Item(i));
        m_pKeyProfiles->Append(arr.Item(i)->GetName(), (void *)copy);
    }

    SetSelProfile(arr.GetSelProfileIdx() >= 0 ? arr.GetSelProfileIdx() : 0);

    //#if defined(LOGGING)
    ////wxKeyBind keybind(entries[ii].GetFlags(), entries[ii].GetKeyCode());
    //const wxKeyProfile* pkp = arr.GetSelProfile();
    //wxString strKeyCode = _T("Ctrl-Shift-W");
    //wxCmd* pcmd = pkp->GetCmdBindTo(strKeyCode);
    //if (pcmd)
    //{
    //    int id = pcmd->GetId();
    //    int shortcutsCount = pcmd->GetShortcutCount();
    //    wxString desc = pcmd->GetDescription();
    //    wxString name = pcmd->GetName();
    //    int type = pcmd->GetType();
    //    wxLogMessage( _T("shortcut count for[%s]is[%d]type[%d]"),
    //          strKeyCode.c_str(), shortcutsCount, type);
    //    for (int kk = 0; kk < shortcutsCount; ++kk )
    //    {
    //        wxKeyBind* pkbind = pcmd->GetShortcut(kk);
    //        if (pkbind)
    //        {   wxString strKeyCode = wxKeyBind::KeyCodeToString(pkbind->GetKeyCode());
    //            if (pkbind->GetModifiers() & wxACCEL_SHIFT)
    //                strKeyCode.Prepend(_T("Shift-"));
    //            if (pkbind->GetModifiers() & wxACCEL_CTRL)
    //                strKeyCode.Prepend(_T("Ctrl-"));
    //            if (pkbind->GetModifiers() & wxACCEL_ALT)
    //                strKeyCode.Prepend(_T("Alt-"));
    //
    //            wxLogMessage( _T("keybind[%d.%d] for [%s] is [%s]"),
    //                    id, kk, name.c_str(), strKeyCode.c_str());
    //        }
    //    }//for kk
    //}//if
    //#endif

}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::SetSelProfile(int n)
// ----------------------------------------------------------------------------
{
    wxASSERT(m_pKeyProfiles && (n >= 0) && (n < static_cast<int>(m_pKeyProfiles->GetCount())) );

    m_pKeyProfiles->SetSelection(n);
    m_nCurrentProf = n;

    // generate a fake event: SetSelection does not generate it
    wxCommandEvent ev;
    OnProfileSelected(ev);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::Reset()
// ----------------------------------------------------------------------------
{
    if (IsUsingTreeCtrl()) {

        m_pCommandsTree->DeleteAllItems();

    } else {

        m_pCommandsList->Clear();
        m_pCategories->Clear();
    }

    m_pBindings->Clear();
#ifdef __WXGTK__
    m_pDescLabel->Clear();
#else
    m_pDescLabel->SetLabel(wxT(""));
#endif
    m_pKeyField->Clear();
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::AddRootIfMissing(const wxString &rootname)
// ----------------------------------------------------------------------------
{
    wxASSERT(IsUsingTreeCtrl());    // this function is treectrl-specific

    if (!m_pCommandsTree->GetRootItem().IsOk()) {

        // I don't know what happens if we try to create the root
        // of the control while there are still some items in the
        // control... even if I don't think some items maybe
        // present in the control if the root is not okay....
        m_pCommandsTree->DeleteAllItems();

        m_pCommandsTree->AddRoot(rootname);
    }
}

// ----------------------------------------------------------------------------
wxCmd *wxKeyConfigPanel::GetSelCmd() const
// ----------------------------------------------------------------------------
{
    intptr_t id;

    if (IsUsingTreeCtrl()) {

        wxTreeItemId treeid = GetSelCmdId();
        if (!treeid.IsOk())
            return NULL;

        // each tree item has a wxMenuTreeItemData associated which contains
        // the ID of the menu item which is attached...
        id  = ((wxExTreeItemData *)m_pCommandsTree->GetItemData(treeid))->GetMenuItemId();

    } else {

        int sel = m_pCommandsList->GetSelection();
        if (sel < 0)
            return NULL;

        id = (intptr_t)m_pCommandsList->GetClientData(sel);
    }

    return m_kBinder.GetCmd(id);
}

// ----------------------------------------------------------------------------
wxTreeItemId wxKeyConfigPanel::GetSelCmdId() const
// ----------------------------------------------------------------------------
{
    wxTreeItemId selection = m_pCommandsTree->GetSelection();

    // selection must be valid...
    if (!selection.IsOk())
        return selection;

    // ...and the just selected must be a valid menu item...
    wxTreeItemData *p = m_pCommandsTree->GetItemData(selection);
    if (p == NULL)
        return wxTreeItemId();        // an empty wxTreeItemId is always invalid...

    // if tree item is a sub-menu don't allow key assignment
    if (m_pCommandsTree->ItemHasChildren(selection))        //patch 2885
        return wxTreeItemId();                              //patch 2885

    return selection;
}
// ----------------------------------------------------------------------------
wxControl *wxKeyConfigPanel::GetMainCtrl() const
// ----------------------------------------------------------------------------
{
    if (m_nBuildMode & wxKEYBINDER_USE_TREECTRL)
        return (wxControl*)m_pCommandsTree;
    return m_pCommandsList;
}

// ----------------------------------------------------------------------------
wxString wxKeyConfigPanel::GetSelCmdStr() const
// ----------------------------------------------------------------------------
{
    wxTreeItemId id = GetSelCmdId();

    // wxEmpyString is returned if there's no valid selection
    if (!id.IsOk())
        return wxEmptyString;

    return m_pCommandsTree->GetItemText(id);
}

// ----------------------------------------------------------------------------
bool wxKeyConfigPanel::IsSelectedValidCmd() const
// ----------------------------------------------------------------------------
{
////    if (IsUsingTreeCtrl())
////        return GetSelCmdId().IsOk();

    if (IsUsingTreeCtrl())
        return GetSelCmdId().IsOk();
    else
        return m_pCommandsList->GetSelection() >= 0;
}
// ----------------------------------------------------------------------------
wxKeyProfileArray wxKeyConfigPanel::GetProfiles() const
// ----------------------------------------------------------------------------
{
    wxKeyProfileArray arr;

    // just copy the combobox item's client data (which are wxKeyProfiles)
    // into our array...
    // NB: it's very important to *copy* the profiles into the new array
    //     since the destructor of wxKeyConfigPanel expects the m_pKeyProfiles
    //     control to contain always valid pointers NOT shared with anyone else
    for (size_t i=0; i < m_pKeyProfiles->GetCount(); i++)
        arr.Add(new wxKeyProfile(*GetProfile(i)));
    arr.SetSelProfile(GetSelProfileIdx());

    return arr;
}

// ----------------------------------------------------------------------------
// wxKeyConfigPanel - UPDATE functions
// ----------------------------------------------------------------------------
void wxKeyConfigPanel::UpdateButtons()
// ----------------------------------------------------------------------------
{
    //-wxLogDebug(wxT("wxKeyConfigPanel::UpdateButtons"));

    wxString str;

    // is the remove button to be enabled ?
    //-m_pRemoveBtn->Enable(m_pBindings->GetSelection() >= 0);        //DerMeister 2007/01/13
    m_pRemoveBtn->Enable(m_pBindings->GetSelection() != wxNOT_FOUND); //DerMeister 2007/01/13

    m_pRemoveAllBtn->Enable(m_pBindings->GetCount() > 0);

    // is the assign button to be enabled ?
    bool b = IsSelectedValidCmd() && m_pKeyField->IsValidKeyComb();
    m_pAssignBtn->Enable(b);

    // must the "Currently assigned to" field be updated ?
    if (m_pKeyField->IsValidKeyComb()) {
        wxCmd *p = m_kBinder.GetCmdBindTo(m_pKeyField->GetValue());

        if (p) {

            // another command already owns this key bind...
            m_pCurrCmd = p;
            str = p->GetName();

        } else {

            str = _("None");
            m_pCurrCmd = NULL;
        }
    }

    m_pCurrCmdField->SetLabel(str);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::UpdateDesc()
// ----------------------------------------------------------------------------
{
    //-wxLogDebug(wxT("wxKeyConfigPanel::UpdateDesc"));
    //-wxLogDebug("\n");
    wxCmd *p = GetSelCmd();

    if (p != NULL) {

        // and then update the description
#ifdef __WXGTK__
        m_pDescLabel->SetValue(p->GetDescription());
#else
        m_pDescLabel->SetLabel(p->GetDescription());
#endif
    } else {

        // an invalid command is selected ? clear this field...
        m_pDescLabel->SetLabel(wxT(""));
        if (IsUsingTreeCtrl())
        {
            wxTreeItemId selection = m_pCommandsTree->GetSelection();
            if (selection and (not m_pCommandsTree->ItemHasChildren(selection)) )
                m_pDescLabel->SetLabel(wxT("GetSelCmd() failed for this tree item.")); //(pecan 2020/02/27)
        }
    }
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::FillInBindings()
// ----------------------------------------------------------------------------
{
    // remove (eventually present) previous shortcuts...
    m_pBindings->Clear();

    // is the selected item okay ?
    wxCmd *p = GetSelCmd();
    if (p == NULL)
        return;

    // update the listbox with the shortcuts for the selected menuitem
    m_pBindings->Append(p->GetShortcutsList());

    // select the first shortcut...
    if (m_pBindings->GetCount() > 0)
        m_pBindings->SetSelection(0);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::ApplyChanges()
// ----------------------------------------------------------------------------
{
    wxLogDebug(_("wxKeyConfigPanel::ApplyChanges"));
    //-wxLogDebug("\n");

    wxKeyProfile *prof = GetSelProfile();
    wxASSERT(prof);

    // just copy the internal key binder used to allow a sort of
    // "undo" feature into the currently selected profile
    prof->DeepCopy(m_kBinder);

    // and update the label of the m_pKeyProfiles control
    // (the name of the profile could have been changed)
    m_pKeyProfiles->SetString(GetSelProfileIdx(), m_kBinder.GetName());
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::EnableKeyProfiles(bool bEnable)
// ----------------------------------------------------------------------------
{
    m_bEnableKeyProfiles = bEnable;

    // we must hide the keyprofile combobox
    ShowSizer(m_pKeyProfilesSizer, m_bEnableKeyProfiles);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::ShowSizer(wxSizer *toshow, bool show)
// ----------------------------------------------------------------------------
{
    // first of all, update the items of the sizer
    toshow->ShowItems(m_bEnableKeyProfiles);

    // then, since the size has been changed...
    wxSizer *main = GetSizer();

    // check the old show state...
    // VERY IMPORTANT: this is not only an optimization
    //                 the code below expect a size change and
    //                 if the size change did not happen it will
    //                 set some vars to wrong values
    bool oldshow = main->IsShown(toshow);
    if ((show && oldshow) || (!show && !oldshow))
        return;

    // add or detach the given sizer
    if (show)
        main->Prepend(toshow, 0, wxGROW);
    else
        main->Detach(toshow);


    // THIS PIECE OF CODE HAS BEEN COPIED & PASTED
    // FROM THE wxLogDialog::OnDetails FUNCTION OF
    // THE wxWidgets/src/generic/logg.cpp FILE
    // -------------------------------------------
    m_minHeight = m_maxHeight = -1;

   // wxSizer::FitSize() is private, otherwise we might use it directly...
    wxSize sizeTotal = GetSize(),
           sizeClient = GetClientSize();

    wxSize size = main->GetMinSize();
    size.x += sizeTotal.x - sizeClient.x;
    size.y += sizeTotal.y - sizeClient.y;

    // we don't want to allow expanding the dialog in vertical direction as
    // this would show the "hidden" details but we can resize the dialog
    // vertically while the details are shown
    if ( !show )
        m_maxHeight = size.y;

    SetSizeHints(size.x, size.y, m_maxWidth, m_maxHeight);

    // don't change the width when expanding/collapsing
    SetSize(wxDefaultCoord, size.y);

#ifdef __WXGTK__
    // VS: this is neccessary in order to force frame redraw under
    // WindowMaker or fvwm2 (and probably other broken WMs).
    // Otherwise, detailed list wouldn't be displayed.
    Show();
#endif // wxGTK
}




// ----------------------------------------------------------------------------
// wxKeyConfigPanel - event handlers
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnApplyChanges(wxCommandEvent &event)
// ----------------------------------------------------------------------------
{
    // If shortcut control is not empty, give user change to 'Add' //(2019/04/6)
    if (not m_pKeyField->GetValue().empty())
        if (wxYES == wxMessageBox(_("Did you forget to 'Add' the shortcut key?"),
                     _("Warning"), wxYES_NO, this) )
                    return;

    // apply changed bindings to the original binder
    ApplyChanges();
    event.Skip();        // let parent know that changes were applied
}
// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnProfileEditing(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    wxASSERT(m_nCurrentProf != -1);
    // This routine is screwing up unix, and its never called on MSW
    // so.. forget it //(2006/9/23)
    return ;

    wxString oldname = m_kBinder.GetName();
        // on unix, this routine is being entered with oldname == ""
        // We're not going to save the blank temp profile anyway
        // so ignore this situation //(2006.09.22)
    if ( oldname == wxEmptyString )     //(2006.09.23)
        return;
    wxString newname = m_pKeyProfiles->GetValue();//ev.GetString();
    if (newname == oldname)
        return;

    // now the profile has been changed...
    m_bProfileHasBeenModified = TRUE;

    // change the name of the current profile
    m_kBinder.SetName(newname);

#if 0
    // and the string of the combobox...
    int n = m_pKeyProfiles->FindString(oldname);
    if (n != wxNOT_FOUND)
        m_pKeyProfiles->SetString(n, newname);
#endif
}
// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnTreeCommandSelected(wxTreeEvent &)
// ----------------------------------------------------------------------------
{
    //-wxLogDebug(wxT("wxKeyConfigPanel::OnTreeCommandSelected"));
    //-wxLogDebug("\n");

    // one of the group in the tree ctrl has been selected:
    // the listbox with the keybindings must be synchronized...
    FillInBindings();

    // if the user selected an invalid/valid item in the tree ctrl,
    // the buttons must be disabled/enabled...
    UpdateButtons();

    // ...and the description must be updated
    UpdateDesc();

    // find and capture the OK button //(2019/04/6)
    if (not m_pOK)
    {
        //-wxWindow* pwin = wxFindWindowByName(_("KeyBinder"));
        wxWindow* pwin = wxFindWindowByName(_("Configure editor"));
        if ( pwin )
            m_pOK = (wxButton*)pwin->FindWindowById(wxID_OK,pwin);
        if (m_pOK)
        #if wxVERSION_NUMBER >= 3000    //(wx28 2019/04/19)
            m_pOK->GetEventHandler()->Connect(wxID_OK, wxEVT_BUTTON, wxCommandEventHandler( wxKeyConfigPanel::OnApplyChanges), NULL, this);
        #else
            m_pOK->GetEventHandler()->Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( wxKeyConfigPanel::OnApplyChanges), NULL, this);
        #endif // wxVERSION_NUMBER
    }
}
// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnListCommandSelected(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    wxLogDebug(_("wxKeyConfigPanel::OnListCommandSelected"));
    //-wxLogDebug("\n");

    // like OnTreeCommandSelected
    FillInBindings();
    UpdateButtons();
    UpdateDesc();
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnBindingSelected(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    wxLogDebug(_("wxKeyConfigPanel::OnBindingSelected"));

    // the remove button should be enabled if the
    // element just selected is valid...
    UpdateButtons();
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnCategorySelected(wxCommandEvent &ev)
// ----------------------------------------------------------------------------
{
    wxLogDebug(_("wxKeyConfigPanel::OnCategorySelected"));

    // update selection
    int sel = m_pCategories->GetSelection();
    if (sel == -1) return;

    wxExComboItemData *data = (wxExComboItemData*)m_pCategories->GetClientObject(sel);
    wxArrayString &arr = data->GetCmdNameArr();

    // clear the old elements & insert the new ones
    m_pCommandsList->Clear();
    for (int i=0; i < (int)arr.GetCount(); i++)
        m_pCommandsList->Append(arr.Item(i), reinterpret_cast<void *>(data->GetID(i)));

    // select the first
    m_pCommandsList->Select(0);
    OnListCommandSelected(ev);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnProfileSelected(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    //-wxLogDebug(wxT("wxKeyConfigPanel::OnProfileSelected"));
    //-wxLogDebug("\n");

    int selidx = m_pKeyProfiles->GetSelection();
    wxKeyProfile *sel;

    if (selidx != -1) {

        // did the user modify the old profile ?
        if (m_bProfileHasBeenModified)
        {
            //(2006/9/23)
            //////////////////////////////////////////////////////////////////////////////
            // This routine keeps saving an empty profile. Disabled until I find out why.
            //////////////////////////////////////////////////////////////////////////////
////            // NB: m_nCurrentProf now retains the old profile index
////            int choice = wxMessageBox(
////                wxString::Format(wxT("The previous profile (named \"%s\") has been modified.\n")
////                        wxT("Do you want to save the changes to that profile ?"),
////                        GetProfile(m_nCurrentProf)->GetName().c_str()),
////                wxT("Warning"), wxYES_NO | wxICON_QUESTION);
////
////            if (choice == wxYES) {
////
////                ApplyChanges();
////
////            } else if (choice == wxCANCEL) {
////
////                // WARNING: the wxCANCEL flag has been removed from the wxMessageBox
////                // above because the call to m_pKeyProfiles->SetSelection below
////                // provokes (even if wx docs says no) with wxMSW 2.5.4 another event
////
////                // re select the old profile... without generating another event
////                m_pKeyProfiles->SetSelection(m_nCurrentProf);
////                return;        // and abort this call
////
////            } else if (choice == wxNO) {
////
                // just restore the original label of the old profile
                // into the wxcombobox...
                // NB: the original label is stored in the client data associated
                // with the previously selected combo item; the user-edited
                // label has been set only in the m_kBinder profile.
                m_pKeyProfiles->SetString(m_nCurrentProf,
                            GetProfile(m_nCurrentProf)->GetName());
////            }
        }

        // update the current selected profile index
        m_nCurrentProf = selidx;
        sel = GetProfile(m_nCurrentProf);

    } else {

        // GetSelProfile() will use our current m_nCurrentProf element...
        sel = GetSelProfile();
    }

    if (!sel) return;

    // copy original bindings to the temporary binder,
    // which is the one which the user edits (and which
    // will be copied in the original one if the
    // #ApplyChanges function is called...)
    m_kBinder.DeepCopy(*sel);
    m_bProfileHasBeenModified = FALSE;

    // call other event handlers
    if (IsUsingTreeCtrl()) {

        wxTreeEvent ev;
        OnTreeCommandSelected(ev);

    } else {

        wxCommandEvent ev;
        OnListCommandSelected(ev);
    }
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnAssignKey(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    // the new key combination should be valid because only when
    // it's valid is this Add button enabled...
    wxASSERT(m_pKeyField->IsValidKeyComb());

    wxCmd *sel = GetSelCmd();
    if (not sel)
    {  //got null sel
        wxString cmdStr = GetSelCmdStr();
        int actualMnuId = wxFindMenuItem(Manager::Get()->GetAppFrame()->GetMenuBar(), cmdStr);
        wxString msg = wxString::Format(_T("KeyBinding error in OnAssignKey()\nid[%d] label[%s]\n"),
                                        actualMnuId, cmdStr.wx_str());
        wxLogDebug(msg);
        wxMessageBox(msg);
        return;
    }

    if (sel->GetShortcutCount() >= wxCMD_MAX_SHORTCUTS) {

        // sorry...
        wxMessageBox(wxString::Format(_("Cannot add more than %d shortcuts to a single command..."),
                      wxCMD_MAX_SHORTCUTS),
                    _("Cannot add another shortcut"));
        return;
    }

    // if the key bind was owned by other commands,
    // remove it from the old commands...
    wxCmd *p = 0;
    while ((p = m_kBinder.GetCmdBindTo(m_pKeyField->GetValue()) ))
    {
        // another command already owns this key bind...
        wxKeyBind tmp(m_pKeyField->GetValue());
        int n;
        if (p->IsBindTo(tmp, &n))
            p->RemoveShortcut(n);
    }//endwhile

    // actually add the new shortcut key
    // (if there are already the max. number of shortcuts for
    // this command, the shortcut won't be added).
    sel->AddShortcut(m_pKeyField->GetValue());

    // now the user has modified the currently selected profile...
    m_bProfileHasBeenModified = TRUE;

////    // if the just added key bind was owned by another command,
////    // remove it from the old command...
////    if (m_pCurrCmd) {     //pecan 2006/4/10 testing
////        wxKeyBind tmp(m_pKeyField->GetValue());
////        int n;
////        bool bind = m_pCurrCmd->IsBindTo(tmp, &n);     //pecan 2006/03/25
////    #ifdef __WXDEBUG__
////        //-bool bind = m_pCurrCmd->IsBindTo(tmp, &n);  //pecan 2006/03/25
////        wxASSERT_MSG(bind, wxT("the m_pCurrCmd variable should be NULL then..."));
////    #endif        // to avoid warnings in release mode
////
////        //m_pCurrCmd->RemoveShortcut(n);
////        if (bind) m_pCurrCmd->RemoveShortcut(n);        //pecan 2006/03/25
////    }

    // and update the list of the key bindings
    FillInBindings();
    m_pKeyField->Clear();

}//OnAssignKey

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnRemoveKey(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    // Check if there is a selected shortcut:       //DerMeister 2007/01/13
    int selection = m_pBindings->GetSelection();    //DerMeister 2007/01/13
    if (selection == wxNOT_FOUND)                   //DerMeister 2007/01/13
            return; // Nothing to do.               //DerMeister 2007/01/13


    // now the user has modified the currently selected profile...
    m_bProfileHasBeenModified = TRUE;

    // remove the selected shortcut
    //-GetSelCmd()->RemoveShortcut(m_pBindings->GetSelection()); //DerMeister 2007/01/13
    GetSelCmd()->RemoveShortcut(selection);                      //DerMeister 2007/01/13

    // and update the list of the key bindings
    FillInBindings();
    UpdateButtons();
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnRemoveAllKey(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    // now the user has modified the currently selected profile...
    m_bProfileHasBeenModified = TRUE;

    // remove the selected shortcut
    GetSelCmd()->RemoveAllShortcuts();

    // and update the list of the key bindings
    FillInBindings();
    UpdateButtons();
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnAddProfile(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    wxKeyProfile *sel = GetSelProfile();
    if (!sel) return;

    // we'll use one of wx common dialogs
    wxTextEntryDialog dlg(this,
        _("Input the name of the new profile.\n"
          "The new profile will be initially set to a copy of the last selected profile."),
        _("Add new profile"));
    dlg.SetValue(sel->GetName());
    PlaceWindow(&dlg);

    bool valid = false;
    while (!valid) {

        if (dlg.ShowModal() == wxID_CANCEL)
            return;

        // if the name is the same of one of the existing profiles, we have to abort...
        valid = true;
        for (size_t j=0; j < m_pKeyProfiles->GetCount(); j++)
            if (GetProfile(j)->GetName() == dlg.GetValue())
                valid = false;

        if (!valid) {

            wxMessageBox(_("The given profile name is already in use.\n"
                           "Enter another name."));
        }
    }

    // create the new profile copying the last selected one
    wxKeyProfile *newprof = new wxKeyProfile(*sel);
    newprof->SetName(dlg.GetValue());        // just change its name
    AddProfile(*newprof);
    delete newprof;

    // set the new profile as selected (the new profile is the last one)
    SetSelProfile(m_pKeyProfiles->GetCount()-1);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnRemoveProfile(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    wxASSERT(m_nCurrentProf != -1);

    if (m_pKeyProfiles->GetCount() == 1) {

        wxMessageBox(_("Cannot delete this profile. It's the only available profile."),
                    _("Warning"));
        return;
    }

    // delete the keyprofile associated with that item...
    delete ((wxKeyProfile*)m_pKeyProfiles->GetClientData(m_nCurrentProf));
    m_pKeyProfiles->Delete(m_nCurrentProf);

    // update the currently selected profile
    int newsel = m_nCurrentProf-1;
    if (newsel < 0) newsel=0;
    wxASSERT(newsel < static_cast<int>(m_pKeyProfiles->GetCount()));

    // keep sync m_nCurrentProf with the currently really selected item
    SetSelProfile(newsel);
}

// ----------------------------------------------------------------------------
void wxKeyConfigPanel::OnKeyPressed(wxCommandEvent &)
// ----------------------------------------------------------------------------
{
    // if there were no key combinations in the wxKeyMonitorTextCtrl
    // before this event, maybe that now there is one.... this means
    // that the "Assign" could be enabled...
    UpdateButtons();
}
// ----------------------------------------------------------------------------
void TestNumpadConversions()
// ----------------------------------------------------------------------------
{
    for (size_t i = 0; i < numpadKeyNameCount; ++i)
    {
        const NumpadKeyName& item = numpadKeyNames[i];

        const wxString name = wxKeyBind::KeyCodeToString(item.code);
        wxASSERT(name == item.name);

        wxASSERT(wxKeyBind::StringToKeyCode(name) == item.code);
        wxASSERT(
            wxKeyBind::StringToKeyCode(wxString(item.legacyName))
                == item.code
        );

        wxString shortcut = wxT("Ctrl-");
        shortcut += name;

        wxAcceleratorEntry entry;
        const bool parsed = entry.FromString(shortcut);

        wxASSERT_MSG(parsed, shortcut);
        if (not parsed)
            continue;

        wxASSERT(entry.GetKeyCode() == item.code);
        wxASSERT((entry.GetFlags() & wxACCEL_CTRL) != 0);
    }
}
