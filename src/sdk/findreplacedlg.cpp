/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#include "findreplacedlg.h"

#include <algorithm>

#ifndef CB_PRECOMP
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/choice.h>
    #include <wx/combobox.h>
    #include <wx/intl.h>
    #include <wx/notebook.h>
    #include <wx/radiobox.h>
    #include <wx/regex.h>
    #include <wx/sizer.h>
    #include <wx/stattext.h>
    #include <wx/textctrl.h>
    #include <wx/xrc/xmlres.h>

    #include "cbproject.h"
    #include "configmanager.h"
    #include "editormanager.h"
    #include "globals.h"
    #include "projectmanager.h"
#endif

#include "incremental_select_helper.h"

#define CONF_GROUP _T("/replace_options")

const int maxTargetCount = 100;

//On wxGTK changing the focus of widgets inside the notebook page change event doesn't work
//so we create this custom event (and associated handler) to do the focus change after
//the notebook page change is complete
DEFINE_EVENT_TYPE(wxDEFERRED_FOCUS_EVENT)

BEGIN_EVENT_TABLE(FindReplaceDlg, wxScrollingDialog)
    EVT_ACTIVATE(                        FindReplaceDlg::OnActivate)
    EVT_CHECKBOX(XRCID("chkRegEx1"),     FindReplaceDlg::OnRegEx)
    EVT_CHECKBOX(XRCID("chkRegEx2"),     FindReplaceDlg::OnSettingsChange)
    EVT_CHECKBOX(XRCID("chkMatchCase1"), FindReplaceDlg::OnSettingsChange)
    EVT_CHECKBOX(XRCID("chkMatchCase2"), FindReplaceDlg::OnSettingsChange)

    // Special events for Find/Replace
    EVT_CHECKBOX(XRCID("chkMultiLine1"), FindReplaceDlg::OnMultiChange)
    EVT_CHECKBOX(XRCID("chkMultiLine2"), FindReplaceDlg::OnMultiChange)
    EVT_CHECKBOX(XRCID("chkLimitTo1"),   FindReplaceDlg::OnLimitToChange)
    EVT_CHECKBOX(XRCID("chkLimitTo2"),   FindReplaceDlg::OnLimitToChange)
    EVT_RADIOBOX(XRCID("rbScope2"),      FindReplaceDlg::OnScopeChange)
    EVT_BUTTON(XRCID("btnBrowsePath"),   FindReplaceDlg::OnBrowsePath)
    EVT_BUTTON(XRCID("btnSelectTarget"), FindReplaceDlg::OnSelectTarget)
    EVT_CHOICE(XRCID("chProject"),       FindReplaceDlg::OnSearchProject)
    EVT_TEXT(XRCID("cmbFind1"),          FindReplaceDlg::OnSettingsChange)
    EVT_TEXT(XRCID("cmbFind2"),          FindReplaceDlg::OnSettingsChange)
    EVT_TEXT(XRCID("txtMultiLineFind1"), FindReplaceDlg::OnSettingsChange)
    EVT_TEXT(XRCID("txtMultiLineFind2"), FindReplaceDlg::OnSettingsChange)

    EVT_COMMAND(wxID_ANY, wxDEFERRED_FOCUS_EVENT, FindReplaceDlg::OnDeferredFocus)
END_EVENT_TABLE()

FindReplaceDlg::FindReplaceDlg(wxWindow* parent, const wxString& initial, bool hasSelection,
                       bool findMode, bool findReplaceInFilesOnly, bool findReplaceInFilesActive)
    : FindReplaceBase(parent, initial, hasSelection),
    m_findReplaceInFilesActive(findReplaceInFilesActive),
    m_findMode(findMode)
{
    wxXmlResource::Get()->LoadObject(this, parent, _T("dlgFindReplace"),_T("wxScrollingDialog"));
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));
    m_advancedRegex = cfg->ReadBool(_T("/use_advanced_regexes"), false);

    // load last searches
    FillComboWithLastValues(XRCCTRL(*this, "cmbFind1",    wxComboBox), CONF_GROUP _T("/last"));
    FillComboWithLastValues(XRCCTRL(*this, "cmbReplace1", wxComboBox), CONF_GROUP _T("/lastReplace"));
    FillComboWithLastValues(XRCCTRL(*this, "cmbFind2",    wxComboBox), CONF_GROUP _T("/last"));
    FillComboWithLastValues(XRCCTRL(*this, "cmbReplace2", wxComboBox), CONF_GROUP _T("/lastReplace"));

    // load last multiline searches
    XRCCTRL(*this, "txtMultiLineFind1",    wxTextCtrl)->SetValue(cfg->Read(CONF_GROUP _T("/lastMultiLineFind"),    _T("")));
    XRCCTRL(*this, "txtMultiLineReplace1", wxTextCtrl)->SetValue(cfg->Read(CONF_GROUP _T("/lastMultiLineReplace"), _T("")));
    XRCCTRL(*this, "txtMultiLineFind2",    wxTextCtrl)->SetValue(cfg->Read(CONF_GROUP _T("/lastMultiLineFind"),    _T("")));
    XRCCTRL(*this, "txtMultiLineReplace2", wxTextCtrl)->SetValue(cfg->Read(CONF_GROUP _T("/lastMultiLineReplace"), _T("")));

    // replace options
    XRCCTRL(*this, "cmbFind1", wxComboBox)->SetValue(initial);

    bool flgWholeWord = cfg->ReadBool(CONF_GROUP _T("/match_word"), false);
    bool flgStartWord = cfg->ReadBool(CONF_GROUP _T("/start_word"), false);
    bool flgStartFile = cfg->ReadBool(CONF_GROUP _T("/start_file"), false);
    XRCCTRL(*this, "chkLimitTo1", wxCheckBox)->SetValue(flgWholeWord || flgStartWord || flgStartFile);
    XRCCTRL(*this, "rbLimitTo1",  wxRadioBox)->Enable(flgWholeWord || flgStartWord || flgStartFile);
    XRCCTRL(*this, "rbLimitTo1",  wxRadioBox)->SetSelection(flgStartFile ? 2 : (flgStartWord ? 1 : 0));

    XRCCTRL(*this, "chkMatchCase1", wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/match_case"), false));
    XRCCTRL(*this, "chkRegEx1",     wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/regex"),      false));
    XRCCTRL(*this, "rbDirection",   wxRadioBox)->SetSelection(cfg->ReadInt(CONF_GROUP _T("/direction"), 1));
    XRCCTRL(*this, "rbDirection",   wxRadioBox)->Enable(!XRCCTRL(*this, "chkRegEx1", wxCheckBox)->GetValue()); // if regex, only forward searches
    XRCCTRL(*this, "rbOrigin",      wxRadioBox)->SetSelection(cfg->ReadInt(CONF_GROUP _T("/origin"), 0));
    XRCCTRL(*this, "rbScope1",      wxRadioBox)->SetSelection(hasSelection);
    // special key, uses same config for both find & replace options
    XRCCTRL(*this, "chkAutoWrapSearch", wxCheckBox)->SetValue(cfg->ReadBool(_T("/find_options/auto_wrap_search"), true));

    // replace in files options
    flgWholeWord = cfg->ReadBool(CONF_GROUP _T("/match_word2"), false);
    flgStartWord = cfg->ReadBool(CONF_GROUP _T("/start_word2"), false);
    flgStartFile = cfg->ReadBool(CONF_GROUP _T("/start_file2"), false);
    XRCCTRL(*this, "chkLimitTo2", wxCheckBox)->SetValue(flgWholeWord || flgStartWord || flgStartFile);
    XRCCTRL(*this, "rbLimitTo2",  wxRadioBox)->Enable(flgWholeWord || flgStartWord || flgStartFile);
    XRCCTRL(*this, "rbLimitTo2",  wxRadioBox)->SetSelection(flgStartFile ? 2 : (flgStartWord ? 1 : 0));

    XRCCTRL(*this, "cmbFind2",      wxComboBox)->SetValue(initial);
    XRCCTRL(*this, "chkMatchCase2", wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP    _T("/match_case2"), false));
    XRCCTRL(*this, "chkRegEx2",     wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP    _T("/regex2"),      false));
    XRCCTRL(*this, "rbScope2",      wxRadioBox)->SetSelection(cfg->ReadInt(CONF_GROUP _T("/scope2"),      0));

    XRCCTRL(*this, "chkMultiLine1", wxCheckBox)->SetValue(false);
    XRCCTRL(*this, "chkFixEOLs1",   wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/fix_eols1"), false));
    XRCCTRL(*this, "chkFixEOLs1",   wxCheckBox)->Enable(XRCCTRL(*this, "chkMultiLine1", wxCheckBox)->GetValue());

    XRCCTRL(*this, "chkMultiLine2",       wxCheckBox)->SetValue(false);
    XRCCTRL(*this, "chkFixEOLs2",         wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/fix_eols2"), false));
    XRCCTRL(*this, "chkFixEOLs2",         wxCheckBox)->Enable(XRCCTRL(*this, "chkMultiLine2", wxCheckBox)->GetValue());
    XRCCTRL(*this, "chkDelOldSearchRes2", wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/delete_old_searches2"), true));
    XRCCTRL(*this, "chkAutoOpen2",        wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/auto_open_first_result2"), true));
    XRCCTRL(*this, "chkAutoOpen2",        wxCheckBox)->Enable(XRCCTRL(*this, "rbScope2", wxRadioBox)->GetSelection() != 0);

    wxSize szReplaceMulti = XRCCTRL(*this, "nbReplaceMulti", wxPanel)->GetEffectiveMinSize();
    XRCCTRL(*this, "nbReplaceSingle", wxPanel)->SetMinSize(szReplaceMulti);
    XRCCTRL(*this, "nbReplaceMulti",  wxPanel)->SetMinSize(szReplaceMulti);

    wxSize szReplaceInFilesMulti = XRCCTRL(*this, "nbReplaceInFilesMulti", wxPanel)->GetEffectiveMinSize();
    XRCCTRL(*this, "nbReplaceInFilesSingle", wxPanel)->SetMinSize(szReplaceInFilesMulti);
    XRCCTRL(*this, "nbReplaceInFilesMulti",  wxPanel)->SetMinSize(szReplaceInFilesMulti);

    wxSize szFindMulti = XRCCTRL(*this, "nbFindMulti", wxPanel)->GetEffectiveMinSize();
    XRCCTRL(*this, "nbFindSingle", wxPanel)->SetMinSize(szFindMulti);
    XRCCTRL(*this, "nbFindMulti",  wxPanel)->SetMinSize(szFindMulti);

    wxSize szFindInFilesMulti = XRCCTRL(*this, "nbFindInFilesMulti", wxPanel)->GetEffectiveMinSize();
    XRCCTRL(*this, "nbFindInFilesSingle", wxPanel)->SetMinSize(szFindInFilesMulti);
    XRCCTRL(*this, "nbFindInFilesMulti",  wxPanel)->SetMinSize(szFindInFilesMulti);

    wxSize szSearchPath = XRCCTRL(*this, "pnSearchPath", wxPanel)->GetEffectiveMinSize();
    XRCCTRL(*this, "pnSearchProject", wxPanel)->SetMinSize(szSearchPath);
    XRCCTRL(*this, "pnSearchPath",  wxPanel)->SetMinSize(szSearchPath);

    ProjectManager *pm = Manager::Get()->GetProjectManager();
    ProjectsArray *pa = pm->GetProjects();
    cbProject *active_project = Manager::Get()->GetProjectManager()->GetActiveProject();

    // load search path options
    XRCCTRL(*this, "txtSearchPath", wxTextCtrl)->SetValue(cfg->Read(CONF_GROUP _T("/search_path"),
                                                                    (active_project ? active_project->GetBasePath() : wxT(""))));
    wxComboBox* cmbSearchMask = XRCCTRL(*this, "cmbSearchMask", wxComboBox);
    if (cfg->Exists(CONF_GROUP _T("/search_mask")))
    {
        // Migrate from previous config setting of "search_mask" string (since it used to be a textbox)
        // to new config setting of "search_masks" array for the combobox
        cmbSearchMask->Append(cfg->Read(CONF_GROUP _T("/search_mask")));
        cfg->UnSet(CONF_GROUP _T("/search_mask"));
    }
    else
        FillComboWithLastValues(cmbSearchMask, CONF_GROUP _T("/search_masks"));

    if (cmbSearchMask->GetCount() > 0)
        XRCCTRL(*this, "cmbSearchMask", wxComboBox)->SetSelection(0);

    XRCCTRL(*this, "chkSearchRecursively", wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/search_recursive"), false));
    XRCCTRL(*this, "chkSearchHidden", wxCheckBox)->SetValue(cfg->ReadBool(CONF_GROUP _T("/search_hidden"), false));

    wxChoice *chProject = XRCCTRL(*this, "chProject", wxChoice);
    wxChoice *chTarget = XRCCTRL(*this, "chTarget", wxChoice);
    chProject->Freeze();
    const unsigned int numProjects = pa->size();
    for (unsigned int i = 0; i < numProjects; ++i)
    {
        chProject->Append((*pa)[i]->GetTitle());
        if ((*pa)[i] == active_project)
        {
            chProject->SetSelection(i);
            chTarget->Freeze();
            chTarget->Append(_("All project files"));

            const bool selectScopeAll = cfg->ReadBool(CONF_GROUP _T("/target_scope_all"), true);

            const int targetCount = active_project->GetBuildTargetsCount();
            if (targetCount < maxTargetCount)
            {
                wxArrayString targetNames;
                for(int j=0;j<targetCount && j<maxTargetCount;++j)
                    targetNames.push_back(active_project->GetBuildTarget(j)->GetTitle());
                chTarget->Append(targetNames);
                const int targIdx = chTarget->FindString(active_project->GetActiveBuildTarget(), true);
                chTarget->SetSelection((selectScopeAll || targIdx < 0) ? 0 : targIdx );
            }
            else
            {
                if (selectScopeAll)
                    chTarget->SetSelection(0);
                else
                {
                    chTarget->Append(active_project->GetActiveBuildTarget());
                    chTarget->SetSelection(1);
                }
                chTarget->Enable(false);
            }

            chTarget->Thaw();
        }
    }

    chProject->Thaw();

    wxRadioBox* rbScope = XRCCTRL(*this, "rbScope2", wxRadioBox);
    EditorManager* edMgr = Manager::Get()->GetEditorManager();
    bool filesOpen = false;
    for (int i = 0; i < edMgr->GetEditorsCount(); ++i)
    {
        if (edMgr->GetBuiltinEditor(i))
        {
            filesOpen = true;
            break;
        }
    }
    if (!filesOpen)
    {
        if (rbScope->GetSelection() == 0)
            rbScope->SetSelection(1);
        rbScope->Enable(0, false);
    }

    if (pa->IsEmpty())
    {
        if (rbScope->GetSelection() == 1 || rbScope->GetSelection() == 2)
        {
            if (rbScope->IsItemEnabled(0))
                rbScope->SetSelection(0);
            else
                rbScope->SetSelection(3);
        }
        rbScope->Enable(1, false);
        rbScope->Enable(2, false);
    }

    switch (rbScope->GetSelection())
    {
        case 1:
            XRCCTRL(*this, "pnSearchPath",    wxPanel)->Hide();
            XRCCTRL(*this, "pnSearchPath",    wxPanel)->Disable();
            XRCCTRL(*this, "pnSearchProject", wxPanel)->Show();
            break;
        case 3:
            XRCCTRL(*this, "pnSearchPath",    wxPanel)->Show();
            XRCCTRL(*this, "pnSearchPath",    wxPanel)->Enable();
            XRCCTRL(*this, "pnSearchProject", wxPanel)->Hide();
            break;
        default:
            XRCCTRL(*this, "pnSearchPath",    wxPanel)->Show();
            XRCCTRL(*this, "pnSearchPath",    wxPanel)->Disable();
            XRCCTRL(*this, "pnSearchProject", wxPanel)->Hide();
            break;
    }
    (XRCCTRL(*this, "nbReplace", wxNotebook)->GetPage(1))->Layout();

    if (findMode)
    {
        SetTitle(_("Find"));
        XRCCTRL(*this, "nbReplaceSingle",        wxPanel)->Hide();
        XRCCTRL(*this, "nbReplaceInFilesSingle", wxPanel)->Hide();
        XRCCTRL(*this, "nbReplace",              wxNotebook)->SetPageText(0, _("Find"));
        XRCCTRL(*this, "nbReplace",              wxNotebook)->SetPageText(1, _("Find in files"));
        XRCCTRL(*this, "wxID_OK",                wxButton)->SetLabel(_("&Find"));
        XRCCTRL(*this, "chkFixEOLs1",            wxCheckBox)->Hide();
        XRCCTRL(*this, "chkFixEOLs2",            wxCheckBox)->Hide();
        XRCCTRL(*this, "chkDelOldSearchRes2",    wxCheckBox)->Show();
    }

    m_findPage = nullptr;
    if (findReplaceInFilesOnly)
    {
        // Remove, but don't destroy the Find/Replace page until this dialog is destroyed.
        XRCCTRL(*this,  "nbReplace", wxNotebook)->SetSelection(1);
        m_findPage = (XRCCTRL(*this, "nbReplace", wxNotebook)->GetPage(0)); // no active editor, so only replace-in-files
        (XRCCTRL(*this, "nbReplace", wxNotebook)->RemovePage(0)); // no active editor, so only replace-in-files
        XRCCTRL(*this,  "cmbFind2",  wxComboBox)->SetFocus();
        m_findReplaceInFilesActive = true;
    }
    else if (m_findReplaceInFilesActive)
    {
        XRCCTRL(*this, "nbReplace", wxNotebook)->SetSelection(1); // Search->Replace in Files was selected
        XRCCTRL(*this, "cmbFind2",  wxComboBox)->SetFocus();
    }
    else
        XRCCTRL(*this, "cmbFind1",  wxComboBox)->SetFocus();

    GetSizer()->SetSizeHints(this);

    // NOTE (jens#1#): Dynamically connect these events, to avoid asserts in debug-mode
    Connect(XRCID("nbReplace"), wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler(FindReplaceDlg::OnReplaceChange));

    CheckFindValue();
}

FindReplaceDlg::~FindReplaceDlg()
{
    ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));

    // save last searches (up to 10)
    if ( IsFindInFiles() )
    {
        SaveComboValues(XRCCTRL(*this, "cmbFind2",    wxComboBox), CONF_GROUP _T("/last"));
        SaveComboValues(XRCCTRL(*this, "cmbReplace2", wxComboBox), CONF_GROUP _T("/lastReplace"));

        // Save last multi-line search & replace
        cfg->Write(CONF_GROUP _T("/lastMultiLineFind"),    XRCCTRL(*this, "txtMultiLineFind2",    wxTextCtrl)->GetValue());
        cfg->Write(CONF_GROUP _T("/lastMultiLineReplace"), XRCCTRL(*this, "txtMultiLineReplace2", wxTextCtrl)->GetValue());
    }
    else
    {
        SaveComboValues(XRCCTRL(*this, "cmbFind1",    wxComboBox), CONF_GROUP _T("/last"));
        SaveComboValues(XRCCTRL(*this, "cmbReplace1", wxComboBox), CONF_GROUP _T("/lastReplace"));

        // Save last multi-line search & replace
        cfg->Write(CONF_GROUP _T("/lastMultiLineFind"),    XRCCTRL(*this, "txtMultiLineFind1",    wxTextCtrl)->GetValue());
        cfg->Write(CONF_GROUP _T("/lastMultiLineReplace"), XRCCTRL(*this, "txtMultiLineReplace1", wxTextCtrl)->GetValue());
    }

    if (!m_findReplaceInFilesActive)
    {
        // find(replace) options
        cfg->Write(CONF_GROUP _T("/fix_eols1"),  XRCCTRL(*this, "chkFixEOLs1",   wxCheckBox)->GetValue());
        cfg->Write(CONF_GROUP _T("/match_case"), XRCCTRL(*this, "chkMatchCase1", wxCheckBox)->GetValue());
        cfg->Write(CONF_GROUP _T("/regex"),      XRCCTRL(*this, "chkRegEx1",     wxCheckBox)->GetValue());
        cfg->Write(CONF_GROUP _T("/direction"),  XRCCTRL(*this, "rbDirection",   wxRadioBox)->GetSelection());
        cfg->Write(CONF_GROUP _T("/origin"),     XRCCTRL(*this, "rbOrigin",      wxRadioBox)->GetSelection());

        bool flgLimitTo = XRCCTRL(*this, "chkLimitTo1", wxCheckBox)->GetValue();
        int  valLimitTo = XRCCTRL(*this, "rbLimitTo1",  wxRadioBox)->GetSelection();

        cfg->Write(CONF_GROUP _T("/match_word"), flgLimitTo && valLimitTo == 0);
        cfg->Write(CONF_GROUP _T("/start_word"), flgLimitTo && valLimitTo == 1);
        cfg->Write(CONF_GROUP _T("/start_file"), flgLimitTo && valLimitTo == 2);

        // special key, uses same config for both find & replace options
        cfg->Write(_T("/find_options/auto_wrap_search"), XRCCTRL(*this, "chkAutoWrapSearch", wxCheckBox)->GetValue());
    }

    // find(replace) in files options
    bool flgLimitTo = XRCCTRL(*this, "chkLimitTo2", wxCheckBox)->GetValue();
    int  valLimitTo = XRCCTRL(*this, "rbLimitTo2",  wxRadioBox)->GetSelection();

    cfg->Write(CONF_GROUP _T("/match_word2"), flgLimitTo && valLimitTo == 0);
    cfg->Write(CONF_GROUP _T("/start_word2"), flgLimitTo && valLimitTo == 1);
    cfg->Write(CONF_GROUP _T("/start_file2"), flgLimitTo && valLimitTo == 2);

    cfg->Write(CONF_GROUP _T("/fix_eols2"),               XRCCTRL(*this, "chkFixEOLs2",         wxCheckBox)->GetValue());
    cfg->Write(CONF_GROUP _T("/match_case2"),             XRCCTRL(*this, "chkMatchCase2",       wxCheckBox)->GetValue());
    cfg->Write(CONF_GROUP _T("/regex2"),                  XRCCTRL(*this, "chkRegEx2",           wxCheckBox)->GetValue());
    cfg->Write(CONF_GROUP _T("/scope2"),                  XRCCTRL(*this, "rbScope2",            wxRadioBox)->GetSelection());
    cfg->Write(CONF_GROUP _T("/delete_old_searches2"),    XRCCTRL(*this, "chkDelOldSearchRes2", wxCheckBox)->GetValue());
    cfg->Write(CONF_GROUP _T("/auto_open_first_result2"), XRCCTRL(*this, "chkAutoOpen2",        wxCheckBox)->GetValue());

    cfg->Write(CONF_GROUP _T("/search_path"),      XRCCTRL(*this, "txtSearchPath",        wxTextCtrl)->GetValue());
    SaveComboValues(XRCCTRL(*this, "cmbSearchMask", wxComboBox), CONF_GROUP _T("/search_masks"));
    cfg->Write(CONF_GROUP _T("/search_recursive"), XRCCTRL(*this, "chkSearchRecursively", wxCheckBox)->GetValue());
    cfg->Write(CONF_GROUP _T("/search_hidden"),    XRCCTRL(*this, "chkSearchHidden",      wxCheckBox)->GetValue());
    cfg->Write(CONF_GROUP _T("/target_scope_all"),(XRCCTRL(*this, "chTarget",             wxChoice)->GetSelection() == 0));

    if (m_findPage!=nullptr)
        m_findPage->Destroy();

    Disconnect(XRCID("nbReplace"), wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler(FindReplaceDlg::OnReplaceChange));
}

wxString FindReplaceDlg::GetFindString() const
{
    if ( IsMultiLine() )
    {
        if ( IsFindInFiles() )
            return XRCCTRL(*this, "txtMultiLineFind2", wxTextCtrl)->GetValue();
        return XRCCTRL(*this, "txtMultiLineFind1", wxTextCtrl)->GetValue();
    }
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "cmbFind2", wxComboBox)->GetValue();

    return XRCCTRL(*this, "cmbFind1", wxComboBox)->GetValue();
}

wxString FindReplaceDlg::GetReplaceString() const
{
    if ( IsMultiLine() )
    {
        if ( IsFindInFiles() )
            return XRCCTRL(*this, "txtMultiLineReplace2", wxTextCtrl)->GetValue();
        return XRCCTRL(*this, "txtMultiLineReplace1", wxTextCtrl)->GetValue();
    }
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "cmbReplace2", wxComboBox)->GetValue();

    return XRCCTRL(*this, "cmbReplace1", wxComboBox)->GetValue();
}

bool FindReplaceDlg::IsFindInFiles() const
{
    return m_findReplaceInFilesActive;
}

bool FindReplaceDlg::GetDeleteOldSearches() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "chkDelOldSearchRes2", wxCheckBox)->GetValue();

    return true; // checkbox doesn't exist in Find dialog
}

bool FindReplaceDlg::GetSortSearchResult() const
{
    return true; // checkbox doesn't exist
}

bool FindReplaceDlg::GetMatchWord() const
{
    if ( IsFindInFiles() )
    {
        bool flgLimitTo = XRCCTRL(*this, "chkLimitTo2", wxCheckBox)->GetValue();
        return flgLimitTo && XRCCTRL(*this, "rbLimitTo2", wxRadioBox)->GetSelection() == 0;
    }

    bool flgLimitTo = XRCCTRL(*this, "chkLimitTo1", wxCheckBox)->GetValue();
    return flgLimitTo && XRCCTRL(*this, "rbLimitTo1", wxRadioBox)->GetSelection() == 0;
}

bool FindReplaceDlg::GetStartWord() const
{
    if ( IsFindInFiles() )
    {
        bool flgLimitTo = XRCCTRL(*this, "chkLimitTo2", wxCheckBox)->GetValue();
        return flgLimitTo && XRCCTRL(*this, "rbLimitTo2", wxRadioBox)->GetSelection() == 1;
    }
    bool flgLimitTo = XRCCTRL(*this, "chkLimitTo1", wxCheckBox)->GetValue();
    return flgLimitTo && XRCCTRL(*this, "rbLimitTo1", wxRadioBox)->GetSelection() == 1;
}

bool FindReplaceDlg::GetMatchCase() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "chkMatchCase2", wxCheckBox)->GetValue();
    return XRCCTRL(*this, "chkMatchCase1", wxCheckBox)->GetValue();
}

bool FindReplaceDlg::GetRegEx() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "chkRegEx2", wxCheckBox)->GetValue();
    return XRCCTRL(*this, "chkRegEx1", wxCheckBox)->GetValue();
}

bool FindReplaceDlg::GetAutoWrapSearch() const
{
    if ( IsFindInFiles() )
        return false; // not for replace in files
    return XRCCTRL(*this, "chkAutoWrapSearch", wxCheckBox)->GetValue();
}

bool FindReplaceDlg::GetFindUsesSelectedText() const
{
    return false; // not for replace
}

bool FindReplaceDlg::GetStartFile() const
{
    if ( IsFindInFiles() )
    {
        bool flgLimitTo = XRCCTRL(*this, "chkLimitTo2", wxCheckBox)->GetValue();
        return flgLimitTo && XRCCTRL(*this, "rbLimitTo2", wxRadioBox)->GetSelection() == 2;
    }
    bool flgLimitTo = XRCCTRL(*this, "chkLimitTo1", wxCheckBox)->GetValue();
    return flgLimitTo && XRCCTRL(*this, "rbLimitTo1", wxRadioBox)->GetSelection() == 2;
}

bool FindReplaceDlg::GetMultiLine() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "chkMultiLine2", wxCheckBox)->GetValue();
    return XRCCTRL(*this, "chkMultiLine1", wxCheckBox)->GetValue();
}

bool FindReplaceDlg::GetFixEOLs() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "chkFixEOLs2", wxCheckBox)->GetValue();
    return XRCCTRL(*this, "chkFixEOLs1", wxCheckBox)->GetValue();
}

int FindReplaceDlg::GetDirection() const
{
    if ( IsFindInFiles() )
        return 1;
    return XRCCTRL(*this, "rbDirection", wxRadioBox)->GetSelection();
}

int FindReplaceDlg::GetOrigin() const
{
    if ( IsFindInFiles() )
        return 1;
    return XRCCTRL(*this, "rbOrigin", wxRadioBox)->GetSelection();
}

int FindReplaceDlg::GetScope() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "rbScope2", wxRadioBox)->GetSelection();
    return XRCCTRL(*this, "rbScope1", wxRadioBox)->GetSelection();
}

bool FindReplaceDlg::GetRecursive() const
{
    return XRCCTRL(*this, "chkSearchRecursively", wxCheckBox)->IsChecked();
}

bool FindReplaceDlg::GetHidden() const
{
    return XRCCTRL(*this, "chkSearchHidden", wxCheckBox)->IsChecked();
}

wxString FindReplaceDlg::GetSearchPath() const
{
    return XRCCTRL(*this, "txtSearchPath", wxTextCtrl)->GetValue();
}

wxString FindReplaceDlg::GetSearchMask() const
{
    return XRCCTRL(*this, "cmbSearchMask", wxComboBox)->GetValue();
}

int FindReplaceDlg::GetProject() const
{
    return XRCCTRL(*this, "chProject", wxChoice)->GetSelection();
}

int FindReplaceDlg::GetTarget() const
{
    cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (!activeProject)
        return -1;

    wxChoice *control = XRCCTRL(*this, "chTarget", wxChoice);

    const wxString selectedString = control->GetString(control->GetSelection());

    const int targetCount = activeProject->GetBuildTargetsCount();
    for(int ii = 0; ii < targetCount; ++ii)
    {
        if (activeProject->GetBuildTarget(ii)->GetTitle() == selectedString)
            return ii;
    }

    return -1;
}

bool FindReplaceDlg::IsMultiLine() const
{
    if ( IsFindInFiles() )
        return XRCCTRL(*this, "chkMultiLine2", wxCheckBox)->GetValue();
    return XRCCTRL(*this, "chkMultiLine1", wxCheckBox)->GetValue();
}

// events

void FindReplaceDlg::OnScopeChange(cb_unused wxCommandEvent& event)
{
    wxRadioBox* rbScope = XRCCTRL(*this, "rbScope2", wxRadioBox);
    switch (rbScope->GetSelection())
    {
        case 1:
            XRCCTRL(*this, "pnSearchPath", wxPanel)->Hide();
            XRCCTRL(*this, "pnSearchPath", wxPanel)->Disable();
            XRCCTRL(*this, "pnSearchProject", wxPanel)->Show();
            break;
        case 3:
            XRCCTRL(*this, "pnSearchPath", wxPanel)->Show();
            XRCCTRL(*this, "pnSearchPath", wxPanel)->Enable();
            XRCCTRL(*this, "pnSearchProject", wxPanel)->Hide();
            break;
        default:
            XRCCTRL(*this, "pnSearchPath", wxPanel)->Show();
            XRCCTRL(*this, "pnSearchPath", wxPanel)->Disable();
            XRCCTRL(*this, "pnSearchProject", wxPanel)->Hide();
            break;
    }
    if (m_findPage==nullptr)
        (XRCCTRL(*this, "nbReplace", wxNotebook)->GetPage(1))->Layout();
    else
        (XRCCTRL(*this, "nbReplace", wxNotebook)->GetPage(0))->Layout();

    XRCCTRL(*this, "chkAutoOpen2", wxCheckBox)->Enable(rbScope->GetSelection() != 0);
}

void FindReplaceDlg::OnBrowsePath(cb_unused wxCommandEvent& event)
{
    const wxString txtSearchPath = XRCCTRL(*this, "txtSearchPath", wxTextCtrl)->GetValue();
    const wxString dir = ChooseDirectory(nullptr, _("Select search path"), txtSearchPath);
    if (!dir.IsEmpty())
        XRCCTRL(*this, "txtSearchPath", wxTextCtrl)->SetValue(dir);
}

void FindReplaceDlg::OnSearchProject(cb_unused wxCommandEvent& event)
{
    wxChoice *chProject = XRCCTRL(*this, "chProject", wxChoice);
    wxChoice *chTarget = XRCCTRL(*this, "chTarget", wxChoice);
    int i=chProject->GetSelection();
    if (i<0)
        return;
    cbProject *active_project=(*Manager::Get()->GetProjectManager()->GetProjects())[i];
    const bool targAll = (chTarget->GetSelection() == 0);
    chTarget->Freeze();
    chTarget->Clear();
    chTarget->Append(_("All project files"));
    for(int j=0;j<active_project->GetBuildTargetsCount();++j)
        chTarget->Append(active_project->GetBuildTarget(j)->GetTitle());

    chTarget->Thaw();
    const int targIdx = chTarget->FindString(active_project->GetActiveBuildTarget(), true);
    chTarget->SetSelection(targAll || targIdx < 0 ? 0 : targIdx);
}

void FindReplaceDlg::OnReplaceChange(wxNotebookEvent& event)
{
    // Replace / in files triggered
    wxComboBox* cmbFind1    = XRCCTRL(*this, "cmbFind1",          wxComboBox);
    wxComboBox* cmbFind2    = XRCCTRL(*this, "cmbFind2",          wxComboBox);
    wxTextCtrl* txtFind1    = XRCCTRL(*this, "txtMultiLineFind1", wxTextCtrl);
    wxTextCtrl* txtFind2    = XRCCTRL(*this, "txtMultiLineFind2", wxTextCtrl);
    wxComboBox* cmbReplace1 = XRCCTRL(*this, "cmbReplace1",       wxComboBox);
    wxComboBox* cmbReplace2 = XRCCTRL(*this, "cmbReplace2",       wxComboBox);

    if (txtFind1 && txtFind2 && cmbFind1 && cmbFind2 && cmbReplace1 && cmbReplace2)
    {
        if (event.GetSelection() == 0)
        {
            // Switched from "Replace in files" to "Replace"
            txtFind1->SetValue(txtFind2->GetValue());
            cmbFind1->SetValue(cmbFind2->GetValue());
            cmbReplace1->SetValue(cmbReplace2->GetValue());
            m_findReplaceInFilesActive = false;
        }
        else if (event.GetSelection() == 1)
        {
            // Switched from "Replace" to "Replace in files"
            txtFind2->SetValue(txtFind1->GetValue());
            cmbFind2->SetValue(cmbFind1->GetValue());
            cmbReplace2->SetValue(cmbReplace1->GetValue());
            cmbFind1->SetFocus();
            m_findReplaceInFilesActive = true;
        }
    }

    wxCommandEvent e(wxDEFERRED_FOCUS_EVENT,wxID_ANY);
    AddPendingEvent(e);
    CheckFindValue();
    event.Skip();
}

void FindReplaceDlg::OnDeferredFocus(cb_unused wxCommandEvent& event)
{
    if ( IsMultiLine() )
    {
        wxTextCtrl* tc = ( IsFindInFiles() ? XRCCTRL(*this, "txtMultiLineFind2", wxTextCtrl)
                                           : XRCCTRL(*this, "txtMultiLineFind1", wxTextCtrl) );
        if (tc) tc->SetFocus();
    }
    else
    {
        wxComboBox* cb =  ( IsFindInFiles() ? XRCCTRL(*this, "cmbFind2", wxComboBox)
                                            : XRCCTRL(*this, "cmbFind1", wxComboBox) );
        if (cb) cb->SetFocus();
    }
}

void FindReplaceDlg::OnRegEx(cb_unused wxCommandEvent& event)
{
    XRCCTRL(*this, "rbDirection", wxRadioBox)->Enable(!XRCCTRL(*this, "chkRegEx1", wxCheckBox)->GetValue());
    CheckFindValue();
}

void FindReplaceDlg::OnActivate(wxActivateEvent& event)
{
    if ( IsMultiLine() )
    {
        wxTextCtrl* tcp = ( IsFindInFiles() ? XRCCTRL(*this, "txtMultiLineFind2", wxTextCtrl)
                                            : XRCCTRL(*this, "txtMultiLineFind1", wxTextCtrl) );
        if (tcp) tcp->SetFocus();
    }
    else
    {
        wxComboBox* cbp =  ( IsFindInFiles() ? XRCCTRL(*this, "cmbFind2", wxComboBox)
                                             : XRCCTRL(*this, "cmbFind1", wxComboBox) );
        if (cbp) cbp->SetFocus();
    }
    event.Skip();
}

// special events for Replace

void FindReplaceDlg::OnMultiChange(wxCommandEvent& event)
{
    // Multi-Line replacements triggered
    wxComboBox* cmbFind1 = XRCCTRL(*this, "cmbFind1",          wxComboBox);
    wxComboBox* cmbFind2 = XRCCTRL(*this, "cmbFind2",          wxComboBox);
    wxTextCtrl* txtFind1 = XRCCTRL(*this, "txtMultiLineFind1", wxTextCtrl);
    wxTextCtrl* txtFind2 = XRCCTRL(*this, "txtMultiLineFind2", wxTextCtrl);
    wxCheckBox* chkMultiLine1 = XRCCTRL(*this, "chkMultiLine1", wxCheckBox);
    wxCheckBox* chkMultiLine2 = XRCCTRL(*this, "chkMultiLine2", wxCheckBox);
    wxCheckBox* chkFixEOLs1   = XRCCTRL(*this, "chkFixEOLs1",   wxCheckBox);
    wxCheckBox* chkFixEOLs2   = XRCCTRL(*this, "chkFixEOLs2",   wxCheckBox);

    bool      enabledMultiLine = false;
    wxWindow* ctrlToFocus      = nullptr;
    if (event.GetId() == XRCID("chkMultiLine1"))
    {
        enabledMultiLine = chkMultiLine1->GetValue();
        if (chkMultiLine2) chkMultiLine2->SetValue(enabledMultiLine);
        ctrlToFocus = enabledMultiLine ? dynamic_cast<wxWindow*>(txtFind1) : dynamic_cast<wxWindow*>(cmbFind1);
    }
    else if (event.GetId() == XRCID("chkMultiLine2"))
    {
        enabledMultiLine = chkMultiLine2->GetValue();
        if (chkMultiLine1) chkMultiLine1->SetValue(enabledMultiLine);
        ctrlToFocus = enabledMultiLine ? dynamic_cast<wxWindow*>(txtFind2) : dynamic_cast<wxWindow*>(cmbFind2);
    }
    else
        return;

    XRCCTRL(*this, "nbFindSingle", wxPanel)->Show(!enabledMultiLine);
    XRCCTRL(*this, "nbFindInFilesSingle", wxPanel)->Show(!enabledMultiLine);
    XRCCTRL(*this, "nbFindMulti", wxPanel)->Show(enabledMultiLine);
    XRCCTRL(*this, "nbFindInFilesMulti", wxPanel)->Show(enabledMultiLine);

    if (!m_findMode)
    {
        XRCCTRL(*this, "nbReplaceSingle", wxPanel)->Show(!enabledMultiLine);
        XRCCTRL(*this, "nbReplaceInFilesSingle", wxPanel)->Show(!enabledMultiLine);
        XRCCTRL(*this, "nbReplaceMulti", wxPanel)->Show(enabledMultiLine);
        XRCCTRL(*this, "nbReplaceInFilesMulti", wxPanel)->Show(enabledMultiLine);
    }

    if (chkFixEOLs1) chkFixEOLs1->Enable(enabledMultiLine);
    if (chkFixEOLs2) chkFixEOLs2->Enable(enabledMultiLine);
    if (ctrlToFocus) ctrlToFocus->SetFocus();

    //After hiding/showing panels, redo the layout in the notebook pages
    (XRCCTRL(*this, "nbReplace", wxNotebook)->GetPage(0))->Layout();
    if (m_findPage==nullptr)
        (XRCCTRL(*this, "nbReplace", wxNotebook)->GetPage(1))->Layout();

    Refresh();
    CheckFindValue();
    event.Skip();
}

void FindReplaceDlg::OnLimitToChange(wxCommandEvent& event)
{
    if (event.GetId() == XRCID("chkLimitTo1"))
        XRCCTRL(*this, "rbLimitTo1", wxRadioBox)->Enable(XRCCTRL(*this, "chkLimitTo1", wxCheckBox)->GetValue());
    else
        XRCCTRL(*this, "rbLimitTo2", wxRadioBox)->Enable(XRCCTRL(*this, "chkLimitTo2", wxCheckBox)->GetValue());
}

// special methods for Replace

void FindReplaceDlg::FillComboWithLastValues(wxComboBox* combo, const wxString& configKey)
{
    wxArrayString values;
    Manager::Get()->GetConfigManager(_T("editor"))->Read(configKey, &values);

    combo->Append(values);
}

void FindReplaceDlg::SaveComboValues(wxComboBox* combo, const wxString& configKey)
{
    // there should be only a maximum of entries in the config to limit data use
    // so we define a reasonable maximal of entries to be taken from
    // the combobox to be written into the config
    static const unsigned int max_value = 10u;

    wxArrayString values;

    values.Add(combo->GetValue());

    const unsigned int item_count = std::min(combo->GetCount(), max_value);
    for (unsigned int i = 0; i < item_count; ++i)
    {
        const wxString item = combo->GetString(i);

        if ( item.IsEmpty() )
            continue;

        if ( values.Index(item) == wxNOT_FOUND )
            values.Add(item);
    }

    Manager::Get()->GetConfigManager(_T("editor"))->Write(configKey, values);
}

void FindReplaceDlg::OnSelectTarget(cb_unused wxCommandEvent& event)
{
    cbProject* activeProject = Manager::Get()->GetProjectManager()->GetActiveProject();
    if (!activeProject)
        return;

    wxArrayString targetNames;
    const wxString strAllProjectFiles = _("All project files");
    targetNames.push_back(strAllProjectFiles);

    const int targetCount = activeProject->GetBuildTargetsCount();
    for(int ii = 0; ii < targetCount; ++ii)
        targetNames.push_back(activeProject->GetBuildTarget(ii)->GetTitle());

    IncrementalSelectArrayIterator iterator(targetNames);
    IncrementalSelectDialog dlg(this, &iterator, _("Select target..."), _("Choose target:"));
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxChoice *chTarget = XRCCTRL(*this, "chTarget", wxChoice);

        if (targetCount < maxTargetCount)
            chTarget->SetSelection(dlg.GetSelection());
        else
        {
            chTarget->Clear();
            const int selection = dlg.GetSelection();
            if (selection == 0)
            {
                chTarget->Append(targetNames[0]);
                chTarget->SetSelection(0);
            }
            else
            {
                chTarget->Append(strAllProjectFiles);
                chTarget->Append(targetNames[selection]);
                chTarget->SetSelection(1);
            }
        }
    }
}

bool FindReplaceDlg::GetHasToOpenFirstResult() const
{
    return (XRCCTRL(*this, "rbScope2", wxRadioBox)->GetSelection() != 0) &&
           (XRCCTRL(*this, "chkAutoOpen2", wxCheckBox)->GetValue());
}

void FindReplaceDlg::OnSettingsChange(wxCommandEvent& event)
{
    CheckFindValue();
    event.Skip();
}

void FindReplaceDlg::CheckFindValue()
{
    wxString statusText;
    const wxString value(GetFindString());
    if (value.empty())
    {
        statusText = _("Search string cannot be empty");
    }
#ifdef wxHAS_REGEX_ADVANCED
    else if (GetRegEx())
    {
        if (m_advancedRegex)
        {
            int flags = wxRE_ADVANCED;
            if (!GetMultiLine())
                flags |= wxRE_NEWLINE;

            if (!GetMatchCase())
                flags |= wxRE_ICASE;

            wxRegEx r(value, flags);
            if (!r.IsValid())
                statusText = _("Invalid advanced regular expression");
        }
    }
#endif

    XRCCTRL(*this, "stStatus", wxStaticText)->SetLabel(statusText);
    const bool isOk = statusText.empty();
    XRCCTRL(*this, "wxID_OK", wxButton)->Enable(isOk);

    // Colours
    const wxColour bg(isOk ? wxNullColour : *wxRED);
    const wxColour fg(isOk ? wxNullColour : *wxWHITE);

    wxComboBox *cb1 = XRCCTRL(*this, "cmbFind1", wxComboBox);
    cb1->SetBackgroundColour(bg);
    cb1->SetForegroundColour(fg);
    cb1->Refresh();

    wxComboBox *cb2 = XRCCTRL(*this, "cmbFind2", wxComboBox);
    cb2->SetBackgroundColour(bg);
    cb2->SetForegroundColour(fg);
    cb2->Refresh();

    wxTextCtrl *tc1 = XRCCTRL(*this, "txtMultiLineFind1", wxTextCtrl);
    tc1->SetBackgroundColour(bg);
    tc1->SetForegroundColour(fg);
    tc1->Refresh();

    wxTextCtrl *tc2 = XRCCTRL(*this, "txtMultiLineFind2", wxTextCtrl);
    tc2->SetBackgroundColour(bg);
    tc2->SetForegroundColour(fg);
    tc2->Refresh();
}
