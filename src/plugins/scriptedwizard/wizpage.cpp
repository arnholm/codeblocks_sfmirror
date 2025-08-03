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
    #include <wx/ctrlsub.h>
    #include <manager.h>
    #include <logmanager.h>
    #include <configmanager.h>
    #include <projectmanager.h>
    #include <scriptingmanager.h>
    #include <compilerfactory.h>
    #include <compiler.h>
    #include <macrosmanager.h>
    #include <cbproject.h>
    #include <cbexception.h>
#endif

#include "wizpage.h"

#include "buildtargetpanel.h"
#include "compilerpanel.h"
#include "filepathpanel.h"
#include "genericselectpath.h"
#include "genericsinglechoicelist.h"
#include "infopanel.h"
#include "projectpathpanel.h"
#include "scripting/bindings/sc_utils.h"
#include "scripting/bindings/sc_typeinfo_all.h"

namespace Wizard {

void FillCompilerControl(wxItemContainer *control, const wxString& compilerID, const wxString& validCompilerIDs)
{
    const wxArrayString &valids = GetArrayFromString(validCompilerIDs, _T(";"), true);
    wxString def = compilerID;
    if (def.IsEmpty())
        def = CompilerFactory::GetDefaultCompilerID();
    int id = 0;
    control->Clear();
    for (size_t i = 0; i < CompilerFactory::GetCompilersCount(); ++i)
    {
        Compiler* compiler = CompilerFactory::GetCompiler(i);
        if (compiler)
        {
            for (size_t n = 0; n < valids.GetCount(); ++n)
            {
                // match not only if IDs match, but if ID inherits from it too
                if (CompilerFactory::CompilerInheritsFrom(compiler, valids[n]))
                {
                    control->Append(compiler->GetName());
                    if (compiler->GetID().IsSameAs(def))
                        id = control->GetCount() < 1 ? 0 : (control->GetCount() - 1);
                    break;
                }
            }
        }
    }
    control->SetSelection(id);
}

} // namespace Wizard

using namespace Wizard;

// utility function to append a path separator to the
// string parameter, if needed.
wxString AppendPathSepIfNeeded(const wxString& path)
{
    if (path.IsEmpty() || path.Last() == _T('/') || path.Last() == _T('\\'))
        return path;
    return path + wxFILE_SEP_PATH;
}

// static
PagesByName WizPageBase::s_PagesByName;

////////////////////////////////////////////////////////////////////////////////
// WizPage
////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WizPageBase, wxWizardPageSimple)
    EVT_WIZARD_PAGE_CHANGING(-1, WizPageBase::OnPageChanging)
    EVT_WIZARD_PAGE_CHANGED(-1, WizPageBase::OnPageChanged)
END_EVENT_TABLE()

WizPageBase::WizPageBase(const wxString& pageName, wxWizard* parent, const wxBitmap& bitmap)
    : wxWizardPageSimple(parent, nullptr, nullptr, bitmap),
    m_PageName(pageName)
{
    // duplicate pageIDs are not allowed
    if (s_PagesByName[m_PageName])
        cbThrow(_T("Page ID in use:") + pageName);

    // register this to the static pages map
    s_PagesByName[m_PageName] = this;

    // if this is true, the page won't be added to the wizard
    m_SkipPage = Manager::Get()->GetConfigManager(_T("scripts"))->ReadBool(_T("/generic_wizard/") + m_PageName + _T("/skip"), false);
}

//------------------------------------------------------------------------------
WizPageBase::~WizPageBase()
{
    // unregister this from the static pages map
    s_PagesByName[m_PageName] = nullptr;
}

//------------------------------------------------------------------------------

wxWizardPage* WizPageBase::GetPrev() const
{
    ScriptingManager *scriptMgr = Manager::Get()->GetScriptingManager();
    ScriptBindings::Caller caller(scriptMgr->GetVM());

    const wxString sig = _T("OnGetPrevPage_") + m_PageName;
    if (caller.SetupFunc(cbU2C(sig)))
    {
        wxString *result = nullptr;
        if (caller.CallAndReturn0(result))
        {
            if (result->empty())
                return nullptr;
            return s_PagesByName[*result];
        }
        else
            scriptMgr->DisplayErrors(true);
    }

    return wxWizardPageSimple::GetPrev();
}

//------------------------------------------------------------------------------

wxWizardPage* WizPageBase::GetNext() const
{
    ScriptingManager *scriptMgr = Manager::Get()->GetScriptingManager();
    ScriptBindings::Caller caller(scriptMgr->GetVM());

    const wxString sig = _T("OnGetNextPage_") + m_PageName;
    if (caller.SetupFunc(cbU2C(sig)))
    {
        wxString *result = nullptr;
        if (caller.CallAndReturn0(result))
        {
            if (result->empty())
                return nullptr;
            return s_PagesByName[*result];
        }
        else
            scriptMgr->DisplayErrors(true);
    }

    return wxWizardPageSimple::GetNext();
}

void WizPageBase::OnPageChanging(wxWizardEvent& event)
{
    Manager::Get()->GetConfigManager(_T("scripts"))->Write(_T("/generic_wizard/") + m_PageName + _T("/skip"), (bool)m_SkipPage);
    ScriptingManager *scriptMgr = Manager::Get()->GetScriptingManager();
    ScriptBindings::Caller caller(scriptMgr->GetVM());

    const wxString sig = _T("OnLeave_") + m_PageName;
    if (caller.SetupFunc(cbU2C(sig)))
    {
        bool result;
        const bool forward = (event.GetDirection() != 0); // !=0 forward, ==0 backward
        if (caller.CallAndReturn1(result, forward))
        {
            if (result != true)
                event.Veto();
        }
        else
            scriptMgr->DisplayErrors(true);
    }
}

//------------------------------------------------------------------------------
void WizPageBase::OnPageChanged(wxWizardEvent& event)
{
    ScriptingManager *scriptMgr = Manager::Get()->GetScriptingManager();
    ScriptBindings::Caller caller(scriptMgr->GetVM());

    const wxString sig = _T("OnEnter_") + m_PageName;
    if (caller.SetupFunc(cbU2C(sig)))
    {
        const bool forward = (event.GetDirection() != 0); // !=0 forward, ==0 backward
        if (!caller.Call1(forward))
            scriptMgr->DisplayErrors(true);
    }
}

////////////////////////////////////////////////////////////////////////////////
// WizPage
////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WizPage, WizPageBase)
    EVT_CHOICE(-1, WizPage::OnButton)
    EVT_COMBOBOX(-1, WizPage::OnButton)
    EVT_CHECKBOX(-1, WizPage::OnButton)
    EVT_LISTBOX(-1, WizPage::OnButton)
    EVT_RADIOBOX(-1, WizPage::OnButton)
    EVT_BUTTON(-1, WizPage::OnButton)
END_EVENT_TABLE()

WizPage::WizPage(const wxString& panelName, wxWizard* parent, const wxBitmap& bitmap)
    : WizPageBase(panelName, parent, bitmap)
{
    wxXmlResource::Get()->LoadPanel(this, panelName);
}

//------------------------------------------------------------------------------
WizPage::~WizPage()
{
}

//------------------------------------------------------------------------------
void WizPage::OnButton(wxCommandEvent& event)
{
    wxWindow* win = FindWindowById(event.GetId(), this);
    if (!win)
    {
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("Can't locate window with id %d", event.GetId()));
        return;
    }

    ScriptingManager *scriptMgr = Manager::Get()->GetScriptingManager();
    ScriptBindings::Caller caller(scriptMgr->GetVM());

    const wxString sig = _T("OnClick_") + win->GetName();
    if (caller.SetupFunc(cbU2C(sig)))
    {
        if (!caller.Call0())
            scriptMgr->DisplayErrors(true);
    }
}

////////////////////////////////////////////////////////////////////////////////
// WizInfoPanel
////////////////////////////////////////////////////////////////////////////////

WizInfoPanel::WizInfoPanel(const wxString& pageId, const wxString& intro_msg, wxWizard* parent, const wxBitmap& bitmap)
    : WizPageBase(pageId, parent, bitmap)
{
    m_InfoPanel = new InfoPanel(this);
    m_InfoPanel->SetIntroText(intro_msg);
}

//------------------------------------------------------------------------------
WizInfoPanel::~WizInfoPanel()
{
}

void WizInfoPanel::OnPageChanging(wxWizardEvent& event)
{
    if (!GetSkipPage() && event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        SetSkipPage(m_InfoPanel->chkSkip->GetValue());
    }

    WizPageBase::OnPageChanging(event);
}

////////////////////////////////////////////////////////////////////////////////
// WizFilePathPanel
////////////////////////////////////////////////////////////////////////////////

WizFilePathPanel::WizFilePathPanel(bool showHeaderGuard, wxWizard* parent, const wxBitmap& bitmap)
    : WizPageBase(_T("FilePathPage"), parent, bitmap),
    m_AddToProject(false)
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("scripts"));
    m_pFilePathPanel = new FilePathPanel(this);

    m_pFilePathPanel->ShowHeaderGuard(showHeaderGuard);
    m_pFilePathPanel->SetAddToProject(cfg->ReadBool(_T("/generic_wizard/add_file_to_project"), true));
}

//------------------------------------------------------------------------------
WizFilePathPanel::~WizFilePathPanel()
{
}

int WizFilePathPanel::GetTargetIndex() const
{
    if (m_pFilePathPanel)
    {
        return m_pFilePathPanel->GetTargetIndex();
    }
    return -1;
}

void WizFilePathPanel::SetFilePathSelectionFilter(const wxString& filter)
{
    m_pFilePathPanel->SetFilePathSelectionFilter(filter);
}

void WizFilePathPanel::OnPageChanging(wxWizardEvent& event)
{
    if (event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        m_Filename = m_pFilePathPanel->GetFilename();
        m_HeaderGuard = m_pFilePathPanel->GetHeaderGuard();
        m_AddToProject = m_pFilePathPanel->GetAddToProject();

        if (m_Filename.IsEmpty() || !wxDirExists(wxPathOnly(m_Filename)))
        {
            cbMessageBox(_("Please select a filename with full path for your new file..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }

        ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("scripts"));
        cfg->Write(_T("/generic_wizard/add_file_to_project"), (bool)m_pFilePathPanel->GetAddToProject());
    }
    WizPageBase::OnPageChanging(event); // let the base class handle it too
}

////////////////////////////////////////////////////////////////////////////////
// WizProjectPathPanel
////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WizProjectPathPanel, WizPageBase)
    EVT_BUTTON(-1, WizProjectPathPanel::OnButton)
END_EVENT_TABLE()

WizProjectPathPanel::WizProjectPathPanel(wxWizard* parent, const wxBitmap& bitmap)
    : WizPageBase(_T("ProjectPathPage"), parent, bitmap)
{
    m_pProjectPathPanel = new ProjectPathPanel(this);
}

//------------------------------------------------------------------------------
WizProjectPathPanel::~WizProjectPathPanel()
{
}

//------------------------------------------------------------------------------
wxString WizProjectPathPanel::GetPath() const
{
    return AppendPathSepIfNeeded(m_pProjectPathPanel->GetPath());
}

//------------------------------------------------------------------------------
wxString WizProjectPathPanel::GetName() const
{
    return m_pProjectPathPanel->GetName();
}

//------------------------------------------------------------------------------
wxString WizProjectPathPanel::GetFullFileName() const
{
    return m_pProjectPathPanel->GetFullFileName();
}

//------------------------------------------------------------------------------
wxString WizProjectPathPanel::GetTitle() const
{
    return m_pProjectPathPanel->GetTitle();
}

//------------------------------------------------------------------------------
void WizProjectPathPanel::OnButton(cb_unused wxCommandEvent& event)
{
    wxString dir = m_pProjectPathPanel->GetPath();
    dir = ChooseDirectory(nullptr, _("Please select the folder to create your project in"), dir, wxEmptyString, false, true);
    if (!dir.IsEmpty() && wxDirExists(dir))
        m_pProjectPathPanel->SetPath(dir);
}

//------------------------------------------------------------------------------
void WizProjectPathPanel::OnPageChanging(wxWizardEvent& event)
{
    if (event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        wxString dir = m_pProjectPathPanel->GetPath();
        wxString name = m_pProjectPathPanel->GetName();
        wxString fullname = m_pProjectPathPanel->GetFullFileName();
        wxString title = m_pProjectPathPanel->GetTitle();
//        if (!wxDirExists(dir))
//        {
//            cbMessageBox(_("Please select a valid path to create your project..."), _("Error"), wxICON_ERROR, GetParent());
//            event.Veto();
//            return;
//        }
        if (title.IsEmpty())
        {
            cbMessageBox(_("Please select a title for your project..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }
        if (name.IsEmpty())
        {
            cbMessageBox(_("Please select a name for your project..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }
        if (wxFileExists(fullname))
        {
            if (cbMessageBox(_("A project with the same name already exists in the project folder.\n"
                        "Are you sure you want to use this directory (files may be OVERWRITTEN)?"),
                        _("Confirmation"),
                        wxICON_QUESTION | wxYES_NO, GetParent()) != wxID_YES)
            {
//                cbMessageBox(_("A project with the same name already exists in the project folder.\n"
//                            "Please select a different project name..."), _("Warning"), wxICON_WARNING, GetParent());
                event.Veto();
                return;
            }
        }
        Manager::Get()->GetProjectManager()->SetDefaultPath(dir);
    }
    WizPageBase::OnPageChanging(event); // let the base class handle it too
}

//------------------------------------------------------------------------------
void WizProjectPathPanel::OnPageChanged(wxWizardEvent& event)
{
    if (event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        wxString dir = Manager::Get()->GetProjectManager()->GetDefaultPath();
        m_pProjectPathPanel->SetPath(dir);
    }
    WizPageBase::OnPageChanged(event); // let the base class handle it too
}

////////////////////////////////////////////////////////////////////////////////
// WizGenericSelectPathPanel
////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(WizGenericSelectPathPanel, WizPageBase)
    EVT_BUTTON(-1, WizGenericSelectPathPanel::OnButton)
END_EVENT_TABLE()

WizGenericSelectPathPanel::WizGenericSelectPathPanel(const wxString& pageId, const wxString& descr, const wxString& label, const wxString& defValue,
                                            wxWizard* parent, const wxBitmap& bitmap)
    : WizPageBase(pageId, parent, bitmap)
{
    wxString savedValue = Manager::Get()->GetConfigManager(_T("project_wizard"))->Read(_T("/generic_paths/") + pageId);
    if (savedValue.IsEmpty())
        savedValue = defValue;

    m_pGenericSelectPath = new GenericSelectPath(this);
    m_pGenericSelectPath->txtFolder->SetValue(savedValue);
    m_pGenericSelectPath->SetDescription(descr);
    m_pGenericSelectPath->lblLabel->SetLabel(label);
}

//------------------------------------------------------------------------------
WizGenericSelectPathPanel::~WizGenericSelectPathPanel()
{
}

//------------------------------------------------------------------------------
void WizGenericSelectPathPanel::OnButton(cb_unused wxCommandEvent& event)
{
    wxString dir = Manager::Get()->GetMacrosManager()->ReplaceMacros(m_pGenericSelectPath->txtFolder->GetValue());
    dir = ChooseDirectory(this, _("Please select location"), dir, wxEmptyString, false, true);
    if (!dir.IsEmpty() && wxDirExists(dir))
        m_pGenericSelectPath->txtFolder->SetValue(dir);
}

//------------------------------------------------------------------------------
void WizGenericSelectPathPanel::OnPageChanging(wxWizardEvent& event)
{
    if (event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        const wxString &originalDir = m_pGenericSelectPath->txtFolder->GetValue();
        wxString dir = Manager::Get()->GetMacrosManager()->ReplaceMacros(originalDir);
        if (!wxDirExists(dir))
        {
            cbMessageBox(_("Please select a valid location..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }
    }
    WizPageBase::OnPageChanging(event); // let the base class handle it too

    if (event.GetDirection() != 0 && event.IsAllowed())
    {
        Manager::Get()->GetConfigManager(_T("project_wizard"))->Write(_T("/generic_paths/") + GetPageName(), m_pGenericSelectPath->txtFolder->GetValue());
    }
}

////////////////////////////////////////////////////////////////////////////////
// WizCompilerPanel
////////////////////////////////////////////////////////////////////////////////

WizCompilerPanel::WizCompilerPanel(const wxString& compilerID, const wxString& validCompilerIDs, wxWizard* parent, const wxBitmap& bitmap,
                                    bool allowCompilerChange, bool allowConfigChange)
    : WizPageBase(_T("CompilerPage"), parent, bitmap),
    m_AllowConfigChange(allowConfigChange)
{
    m_pCompilerPanel = new CompilerPanel(this, GetParent());

    wxComboBox* cmb = m_pCompilerPanel->GetCompilerCombo();
    FillCompilerControl(cmb, compilerID, validCompilerIDs);
    cmb->Enable(allowCompilerChange);

    m_pCompilerPanel->EnableConfigurationTargets(m_AllowConfigChange);

    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("scripts"));

    m_pCompilerPanel->SetWantDebug(cfg->ReadBool(_T("/generic_wizard/want_debug"), true));
    m_pCompilerPanel->SetDebugName(cfg->Read(_T("/generic_wizard/debug_name"), _T("Debug")));
    m_pCompilerPanel->SetDebugOutputDir(cfg->Read(_T("/generic_wizard/debug_output"), _T("bin") + wxString(wxFILE_SEP_PATH) + _T("Debug")));
    m_pCompilerPanel->SetDebugObjectOutputDir(cfg->Read(_T("/generic_wizard/debug_objects_output"), _T("obj") + wxString(wxFILE_SEP_PATH) + _T("Debug")));

    m_pCompilerPanel->SetWantRelease(cfg->ReadBool(_T("/generic_wizard/want_release"), true));
    m_pCompilerPanel->SetReleaseName(cfg->Read(_T("/generic_wizard/release_name"), _T("Release")));
    m_pCompilerPanel->SetReleaseOutputDir(cfg->Read(_T("/generic_wizard/release_output"), _T("bin") + wxString(wxFILE_SEP_PATH) + _T("Release")));
    m_pCompilerPanel->SetReleaseObjectOutputDir(cfg->Read(_T("/generic_wizard/release_objects_output"), _T("obj") + wxString(wxFILE_SEP_PATH) + _T("Release")));
}

//------------------------------------------------------------------------------
WizCompilerPanel::~WizCompilerPanel()
{
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetCompilerID() const
{
    Compiler* compiler = CompilerFactory::GetCompilerByName(m_pCompilerPanel->GetCompilerCombo()->GetStringSelection());
    if (compiler)
        return compiler->GetID();
    return wxEmptyString;
}

//------------------------------------------------------------------------------
bool WizCompilerPanel::GetWantDebug() const
{
    return m_pCompilerPanel->GetWantDebug();
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetDebugName() const
{
    return m_pCompilerPanel->GetDebugName();
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetDebugOutputDir() const
{
    return AppendPathSepIfNeeded(m_pCompilerPanel->GetDebugOutputDir());
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetDebugObjectOutputDir() const
{
    return AppendPathSepIfNeeded(m_pCompilerPanel->GetDebugObjectOutputDir());
}

//------------------------------------------------------------------------------
bool WizCompilerPanel::GetWantRelease() const
{
    return m_pCompilerPanel->GetWantRelease();
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetReleaseName() const
{
    return m_pCompilerPanel->GetReleaseName();
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetReleaseOutputDir() const
{
    return AppendPathSepIfNeeded(m_pCompilerPanel->GetReleaseOutputDir());
}

//------------------------------------------------------------------------------
wxString WizCompilerPanel::GetReleaseObjectOutputDir() const
{
    return AppendPathSepIfNeeded(m_pCompilerPanel->GetReleaseObjectOutputDir());
}

//------------------------------------------------------------------------------
void WizCompilerPanel::OnPageChanging(wxWizardEvent& event)
{
    if (event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        if (GetCompilerID().IsEmpty())
        {
            cbMessageBox(_("You must select a compiler for your project..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }
        if (m_AllowConfigChange && !GetWantDebug() && !GetWantRelease())
        {
            cbMessageBox(_("You must select at least one configuration..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }

        if (m_AllowConfigChange)
        {
            ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("scripts"));

            cfg->Write(_T("/generic_wizard/want_debug"), (bool)GetWantDebug());
            cfg->Write(_T("/generic_wizard/debug_name"), GetDebugName());
            cfg->Write(_T("/generic_wizard/debug_output"), GetDebugOutputDir());
            cfg->Write(_T("/generic_wizard/debug_objects_output"), GetDebugObjectOutputDir());

            cfg->Write(_T("/generic_wizard/want_release"), (bool)GetWantRelease());
            cfg->Write(_T("/generic_wizard/release_name"), GetReleaseName());
            cfg->Write(_T("/generic_wizard/release_output"), GetReleaseOutputDir());
            cfg->Write(_T("/generic_wizard/release_objects_output"), GetReleaseObjectOutputDir());
        }
    }
    WizPageBase::OnPageChanging(event); // let the base class handle it too
}

////////////////////////////////////////////////////////////////////////////////
// WizBuildTargetPanel
////////////////////////////////////////////////////////////////////////////////

WizBuildTargetPanel::WizBuildTargetPanel(const wxString& targetName, bool isDebug,
                                    wxWizard* parent, const wxBitmap& bitmap,
                                    bool showCompiler,
                                    const wxString& compilerID, const wxString& validCompilerIDs,
                                    bool allowCompilerChange)
    : WizPageBase(_T("BuildTargetPage"), parent, bitmap)
{
    m_pBuildTargetPanel = new BuildTargetPanel(this);
    m_pBuildTargetPanel->SetTargetName(targetName);
    m_pBuildTargetPanel->SetEnableDebug(isDebug);
    m_pBuildTargetPanel->ShowCompiler(showCompiler);

    if (showCompiler)
    {
        wxComboBox* cmb = m_pBuildTargetPanel->GetCompilerCombo();
        FillCompilerControl(cmb, compilerID, validCompilerIDs);
        cmb->Enable(allowCompilerChange);
    }
}

//------------------------------------------------------------------------------
WizBuildTargetPanel::~WizBuildTargetPanel()
{
}

//------------------------------------------------------------------------------
wxString WizBuildTargetPanel::GetCompilerID() const
{
    if (!m_pBuildTargetPanel->GetCompilerCombo()->IsShown())
        return wxEmptyString;

    Compiler* compiler = CompilerFactory::GetCompilerByName(m_pBuildTargetPanel->GetCompilerCombo()->GetStringSelection());
    if (compiler)
        return compiler->GetID();
    return wxEmptyString;
}

//------------------------------------------------------------------------------
bool WizBuildTargetPanel::GetEnableDebug() const
{
    return m_pBuildTargetPanel->GetEnableDebug();
}

//------------------------------------------------------------------------------
wxString WizBuildTargetPanel::GetTargetName() const
{
    return m_pBuildTargetPanel->GetTargetName();
}

//------------------------------------------------------------------------------
wxString WizBuildTargetPanel::GetTargetOutputDir() const
{
    return AppendPathSepIfNeeded(m_pBuildTargetPanel->GetOutputDir());
}

//------------------------------------------------------------------------------
wxString WizBuildTargetPanel::GetTargetObjectOutputDir() const
{
    return AppendPathSepIfNeeded(m_pBuildTargetPanel->GetObjectOutputDir());
}

//------------------------------------------------------------------------------
void WizBuildTargetPanel::OnPageChanging(wxWizardEvent& event)
{
    if (event.GetDirection() != 0) // !=0 forward, ==0 backward
    {
        if (m_pBuildTargetPanel->GetCompilerCombo()->IsShown() && GetCompilerID().IsEmpty())
        {
            cbMessageBox(_("You must select a compiler for your build target..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }

        cbProject* theproject = Manager::Get()->GetProjectManager()->GetActiveProject(); // can't fail; if no project, the wizard didn't even run
        if (theproject->GetBuildTarget(m_pBuildTargetPanel->GetTargetName()))
        {
            cbMessageBox(_("A build target with that name already exists in the active project..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
            return;
        }
    }
    WizPageBase::OnPageChanging(event); // let the base class handle it too
}

////////////////////////////////////////////////////////////////////////////////
// WizGenericSingleChoiceList
////////////////////////////////////////////////////////////////////////////////

WizGenericSingleChoiceList::WizGenericSingleChoiceList(const wxString& pageId, const wxString& descr, const wxArrayString& choices, int defChoice, wxWizard* parent, const wxBitmap& bitmap)
    : WizPageBase(pageId, parent, bitmap)
{
    int savedValue = Manager::Get()->GetConfigManager(_T("project_wizard"))->ReadInt(_T("/generic_single_choices/") + pageId, -1);
    if (savedValue == -1)
        savedValue = defChoice;

    m_pGenericSingleChoiceList = new GenericSingleChoiceList(this);
    m_pGenericSingleChoiceList->SetDescription(descr);
    m_pGenericSingleChoiceList->SetChoices(choices, savedValue);
}

//------------------------------------------------------------------------------
WizGenericSingleChoiceList::~WizGenericSingleChoiceList()
{
}

//------------------------------------------------------------------------------
int WizGenericSingleChoiceList::GetChoice() const
{
    return m_pGenericSingleChoiceList->GetChoice();
}

//------------------------------------------------------------------------------
void WizGenericSingleChoiceList::SetChoice(int choice)
{
    m_pGenericSingleChoiceList->SetChoice(choice);
}

//------------------------------------------------------------------------------
void WizGenericSingleChoiceList::OnPageChanging(wxWizardEvent& event)
{
    WizPageBase::OnPageChanging(event); // let the base class handle it too

    // save selection value
    if (event.GetDirection() != 0 && event.IsAllowed())
    {
        Manager::Get()->GetConfigManager(_T("project_wizard"))->Write(_T("/generic_single_choices/") + GetPageName(), (int)m_pGenericSingleChoiceList->GetChoice());
    }
}
