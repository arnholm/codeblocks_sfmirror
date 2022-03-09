/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>
#ifndef CB_PRECOMP
    #include <wx/radiobox.h>
    #include <wx/xrc/xmlres.h>
#endif
#include "associations.h"
#include "appglobals.h"
#include <manager.h>
#include <configmanager.h>
#include <filefilters.h>
#include <wx/checklst.h>

 #ifdef __WXMSW__
    #include <wx/msw/registry.h>
    #include <shlobj.h> // for SHChangeNotify()
#endif

const Associations::Assoc knownTypes[] =
{
/*
    { Extension (wxString), Description (wxString), IconIndex (int), IsCore (bbol) }
      Note: "index" is the index of the icon resource in "resources.rc"
            Keep all indices in sync with icon indices in "resources.rc"!
*/
    { FileFilters::CODEBLOCKS_EXT,      "Project file",                  1, true },
    { FileFilters::WORKSPACE_EXT,       "Workspace file",               11, true },

    { FileFilters::C_EXT,               "C source file",                 3, true },

    { FileFilters::CC_EXT,              "C++ source file",               4, true },
    { FileFilters::CPP_EXT,             "C++ source file",               4, true },
    { FileFilters::CXX_EXT,             "C++ source file",               4, true },
    { FileFilters::INL_EXT,             "C++ source file",               4, true },

    { FileFilters::TPP_EXT,             "C++ template source file",      4, true },
    { FileFilters::TCC_EXT,             "C++ template source file",      4, true },

    { FileFilters::H_EXT,               "Header file",                   5, true },
    { FileFilters::HH_EXT,              "Header file",                   5, true },
    { FileFilters::HPP_EXT,             "Header file",                   5, true },
    { FileFilters::HXX_EXT,             "Header file",                   5, true },

    { FileFilters::JAVA_EXT,            "Java source file",              6, false },
    { "cg",                             "cg source file",                7, false },
    { FileFilters::D_EXT,               "D source file",                 8, false },
    { FileFilters::RESOURCE_EXT,        "Resource file",                10, false },
    { FileFilters::XRCRESOURCE_EXT,     "XRC resource file",            10, false },

    { FileFilters::ASM_EXT,             "ASM source file",               2, false },
    { FileFilters::S_EXT,               "ASM source file",               2, false },
    { FileFilters::SS_EXT,              "ASM source file",               2, false },
    { FileFilters::S62_EXT,             "ASM source file",               2, false },

    { FileFilters::F_EXT,               "Fortran source file",           9, false },
    { FileFilters::F77_EXT,             "Fortran source file",           9, false },
    { FileFilters::F90_EXT,             "Fortran source file",           9, false },
    { FileFilters::F95_EXT,             "Fortran source file",           9, false },

    { FileFilters::DEVCPP_EXT,          "Dev-CPP project file",         21, false },
    { FileFilters::MSVC6_EXT,           "MS Visual C++ project file",   22, false },
    { FileFilters::MSVC6_WORKSPACE_EXT, "MS Visual C++ workspace file", 23, false }
    //{ "proj",                           "XCODE Project file",           24, false }
};

inline void DoSetAssociation(const wxString& executable, int index)
{
    Associations::DoSetAssociation(knownTypes[index].ext, knownTypes[index].descr, executable, knownTypes[index].index);
}

inline bool DoCheckAssociation(const wxString& executable, int index)
{
    return Associations::DoCheckAssociation(knownTypes[index].ext, knownTypes[index].descr, executable, knownTypes[index].index);
}

unsigned int Associations::CountAssocs()
{
    return sizeof(knownTypes)/sizeof(Associations::Assoc);
}

void Associations::SetBatchBuildOnly()
{
    wxChar name[MAX_PATH] = {0};
    GetModuleFileName(NULL, name, MAX_PATH);

    ::DoSetAssociation(name, 0);
    ::DoSetAssociation(name, 1);

    UpdateChanges();
}

void Associations::UpdateChanges()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0L, 0L);
}

void Associations::SetCore()
{
    wxChar name[MAX_PATH] = {0};
    GetModuleFileName(NULL, name, MAX_PATH);

    const unsigned int assocCount = CountAssocs();
    for (unsigned int i = 0; i < assocCount; ++i)
        if (knownTypes[i].core)
            ::DoSetAssociation(name, i);

    UpdateChanges();
}

void Associations::SetAll()
{
    wxChar name[MAX_PATH] = {0};
    GetModuleFileName(NULL, name, MAX_PATH);

    const unsigned int assocCount = CountAssocs();
    for (unsigned int i = 0; i < assocCount; ++i)
        ::DoSetAssociation(name, i);

    UpdateChanges();
}

void Associations::ClearAll()
{
    wxChar name[MAX_PATH] = {0};
    GetModuleFileName(NULL, name, MAX_PATH);

    const unsigned int assocCount = CountAssocs();
    for (unsigned int i = 0; i < assocCount; ++i)
        DoClearAssociation(knownTypes[i].ext);

    UpdateChanges();
}

bool Associations::Check()
{
    wxChar name[MAX_PATH] = {0};
    GetModuleFileName(NULL, name, MAX_PATH);

    bool result = true;

    const unsigned int assocCount = CountAssocs();
    for (unsigned int i = 0; i < assocCount; ++i)
        if (knownTypes[i].core)
            result &= ::DoCheckAssociation(name, i);

    return result;
}

void Associations::DoSetAssociation(const wxString& ext, const wxString& descr, const wxString& exe, int icoNum)
{
    wxString BaseKeyName("HKEY_CURRENT_USER\\Software\\Classes\\");
    if (platform::WindowsVersion() == platform::winver_Windows9598ME)
        BaseKeyName = "HKEY_CLASSES_ROOT\\";

    const wxString node("CodeBlocks." + ext);

    wxRegKey key; // defaults to HKCR
    key.SetName(BaseKeyName + '.' + ext);
    key.Create();
    key = "CodeBlocks." + ext;

    key.SetName(BaseKeyName + node);
    key.Create();
    key = descr;

    key.SetName(BaseKeyName + node + "\\DefaultIcon");
    key.Create();
    key = exe + wxString::Format(",%d", icoNum);

    key.SetName(BaseKeyName + node + "\\shell\\open\\command");
    key.Create();
    key = '"' + exe + "\" \"%1\"";

    // Delete old ddeexec keys, because they interfere with C::B's own DDE usage
    key.SetName(BaseKeyName + node + "\\shell\\open\\ddeexec");
    if (key.Exists())
        key.DeleteSelf();

    if (ext.IsSameAs(FileFilters::CODEBLOCKS_EXT) || ext.IsSameAs(FileFilters::WORKSPACE_EXT))
    {
        const wxString batchbuildargs(Manager::Get()->GetConfigManager("app")->Read("/batch_build_args", appglobals::DefaultBatchBuildArgs));
        key.SetName(BaseKeyName + node + "\\shell\\Build\\command");
        key.Create();
        key = '"' + exe + "\" " + batchbuildargs + " --build \"%1\"";

        key.SetName(BaseKeyName + node + "\\shell\\Rebuild (clean)\\command");
        key.Create();
        key = '"' + exe + "\" " + batchbuildargs + " --rebuild \"%1\"";
    }
}

void Associations::DoClearAssociation(const wxString& ext)
{
    wxString BaseKeyName("HKEY_CURRENT_USER\\Software\\Classes\\");
    if (platform::WindowsVersion() == platform::winver_Windows9598ME)
        BaseKeyName = "HKEY_CLASSES_ROOT\\";

    wxRegKey key; // defaults to HKCR
    key.SetName(BaseKeyName + '.' + ext);
    if (key.Exists())
    {
        wxString s;
        if (key.QueryValue(wxEmptyString, s) && s.StartsWith("CodeBlocks"))
            key.DeleteSelf();
    }

    key.SetName(BaseKeyName + "CodeBlocks." + ext);
    if (key.Exists())
        key.DeleteSelf();
}

bool Associations::DoCheckAssociation(const wxString& ext, cb_unused const wxString& descr, const wxString& exe, int icoNum)
{
    wxString BaseKeyName("HKEY_CURRENT_USER\\Software\\Classes\\");
    if (platform::WindowsVersion() == platform::winver_Windows9598ME)
        BaseKeyName = "HKEY_CLASSES_ROOT\\";

    wxRegKey key; // defaults to HKCR
    key.SetName(BaseKeyName + '.' + ext);
    if (!key.Exists())
        return false;

    key.SetName(BaseKeyName + "CodeBlocks." + ext);
    if (!key.Exists())
        return false;

    key.SetName(BaseKeyName + "CodeBlocks." + ext + "\\DefaultIcon");
    if (!key.Exists())
        return false;

    wxString strVal;
    if (!key.QueryValue(wxEmptyString, strVal))
        return false;

    if (strVal != wxString::Format("%s,%d", exe, icoNum))
        return false;

    key.SetName(BaseKeyName + "CodeBlocks." + ext + "\\shell\\open\\command");
    if (!key.Open())
        return false;

    if (!key.QueryValue(wxEmptyString, strVal))
        return false;

    if (strVal != wxString::Format("\"%s\" \"%%1\"", exe))
        return false;

    if (ext.IsSameAs(FileFilters::CODEBLOCKS_EXT) || ext.IsSameAs(FileFilters::WORKSPACE_EXT))
    {
        const wxString batchbuildargs(Manager::Get()->GetConfigManager("app")->Read("/batch_build_args", appglobals::DefaultBatchBuildArgs));
        key.SetName(BaseKeyName + "CodeBlocks." + ext + "\\shell\\Build\\command");
        if (!key.Open())
            return false;

        if (!key.QueryValue(wxEmptyString, strVal))
            return false;

        if (strVal != '"' + exe + "\" " + batchbuildargs + " --build \"%1\"")
            return false;

        key.SetName(BaseKeyName + "CodeBlocks." + ext + "\\shell\\Rebuild (clean)\\command");
        if (!key.Open())
            return false;

        if (!key.QueryValue(wxEmptyString, strVal))
            return false;

        if (strVal != '"' + exe + "\" " + batchbuildargs + " --rebuild \"%1\"")
            return false;
    }

    return true;
}

//////////////////////////////////////////

BEGIN_EVENT_TABLE(ManageAssocsDialog, wxScrollingDialog)
    EVT_BUTTON(XRCID("wxID_OK"), ManageAssocsDialog::OnApply)
    EVT_BUTTON(XRCID("wxID_CANCEL"), ManageAssocsDialog::OnCancel)
    EVT_BUTTON(XRCID("clearAll"), ManageAssocsDialog::OnClearAll)
END_EVENT_TABLE()

ManageAssocsDialog::ManageAssocsDialog(wxWindow* parent)
{
    wxXmlResource::Get()->LoadObject(this, parent, "dlgManageAssocs", "wxScrollingDialog");

    list = XRCCTRL(*this, "checkList", wxCheckListBox);
    assert(list);

    wxChar exe[MAX_PATH] = {0};
    GetModuleFileName(NULL, exe, MAX_PATH);

    const unsigned int assocCount = Associations::CountAssocs();
    for (unsigned int i = 0; i < assocCount; ++i)
    {
        list->Append('.' + knownTypes[i].ext + "  (" + knownTypes[i].descr + ')');
        list->Check(i, Associations::DoCheckAssociation(knownTypes[i].ext, knownTypes[i].descr, exe, knownTypes[i].index));
    }

    CentreOnParent();
}

void ManageAssocsDialog::OnApply(cb_unused wxCommandEvent& event)
{
    wxChar name[MAX_PATH] = {0};
    GetModuleFileName(NULL, name, MAX_PATH);

    for (size_t i = 0; i < list->GetCount(); ++i)
    {
        if (list->IsChecked(i))
            ::DoSetAssociation(name, i);
        else
            Associations::DoClearAssociation(knownTypes[i].ext);
    }

    Associations::UpdateChanges();
    EndModal(0);
}

void ManageAssocsDialog::OnCancel(cb_unused wxCommandEvent& event)
{
    EndModal(0);
}

void ManageAssocsDialog::OnClearAll(cb_unused wxCommandEvent& event)
{
    Associations::ClearAll();
    Associations::UpdateChanges();
    EndModal(0);
}

//////////////////////////////////////////

BEGIN_EVENT_TABLE(AskAssocDialog, wxScrollingDialog)
    EVT_BUTTON(XRCID("wxID_OK"), AskAssocDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, AskAssocDialog::OnESC)
    EVT_CHAR_HOOK(AskAssocDialog::OnCharHook)
END_EVENT_TABLE()

AskAssocDialog::AskAssocDialog(wxWindow* parent)
{
    wxXmlResource::Get()->LoadObject(this, parent, "askAssoc", "wxScrollingDialog");
    SetEscapeId(wxID_NONE);
}

void AskAssocDialog::OnOK(cb_unused wxCommandEvent& event)
{
    EndModal(XRCCTRL(*this, "choice", wxRadioBox)->GetSelection());
}

void AskAssocDialog::OnESC(cb_unused wxCommandEvent& event)
{
    EndModal(ASC_ASSOC_DLG_NO_ONLY_NOW);
}

void AskAssocDialog::OnCharHook(wxKeyEvent& event)
{
    if ( event.GetKeyCode() == WXK_ESCAPE )
        Close(); //wxDialog::Close() send button event with id wxID_CANCEL (wxWidgets 2.8)
    else if ( (event.GetKeyCode() == WXK_RETURN) || (event.GetKeyCode() == WXK_NUMPAD_ENTER) )
        EndModal(XRCCTRL(*this, "choice", wxRadioBox)->GetSelection());

    event.Skip();
}
