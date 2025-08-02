/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#include "prep.h"
#ifndef CB_PRECOMP
    #include <algorithm>
    #include <wx/arrstr.h>
    #include <wx/button.h>
    #include <wx/checkbox.h>
    #include <wx/checklst.h>
    #include <wx/choice.h>
    #include <wx/event.h>
    #include <wx/filename.h>
    #include <wx/listbox.h>
    #include <wx/menu.h>
    #include <wx/notebook.h>
    #include <wx/stattext.h>
    #include <wx/sizer.h>
    #include <wx/spinctrl.h>
    #include <wx/textdlg.h>
    #include <wx/treectrl.h>
    #include <wx/xrc/xmlres.h>

    #include "compiler.h"
    #include "compilerfactory.h"
    #include "configmanager.h"
    #include "globals.h"
    #include "macrosmanager.h"
    #include "manager.h"
    #include "logmanager.h"
    #include "projectmanager.h"
#endif
#include <wx/filedlg.h>
#include <wx/propgrid/propgrid.h>
#include <wx/xml/xml.h>

#include "advancedcompileroptionsdlg.h"
#include "annoyingdialog.h"
#include "cbexception.h"
#include "compilergcc.h"
#include "compileroptionsdlg.h"
#include "debuggermanager.h"
#include "editpathdlg.h"
#include "editpairdlg.h"
#include "compilerflagdlg.h"

// TO DO :  - add/edit/delete compiler : applies directly , so no cancel out (change this behaviour)
//          - compiler change of project/target -> check if the policy is still sound (both should have the same compiler)
//          - compiler change of project/target -> all options should be removed : different compiler is different options
//          - directory add/edit and library add/edit : check if it already existed

BEGIN_EVENT_TABLE(CompilerOptionsDlg, wxPanel)
    EVT_UPDATE_UI(            XRCID("btnEditDir"),                      CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnDelDir"),                       CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnClearDir"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnCopyDirs"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnMoveDirUp"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnMoveDirDown"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnEditVar"),                      CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnDeleteVar"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnClearVar"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbCompilerPolicy"),               CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbLinkerPolicy"),                 CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbIncludesPolicy"),               CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbLibDirsPolicy"),                CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbResDirsPolicy"),                CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnSetDefaultCompiler"),           CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnAddCompiler"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnRenameCompiler"),               CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnDelCompiler"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnResetCompiler"),                CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnAddLib"),                       CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnEditLib"),                      CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnDelLib"),                       CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnClearLib"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnCopyLibs"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnMoveLibUp"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnMoveLibDown"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtMasterPath"),                   CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnMasterPath"),                   CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnExtraAdd"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnExtraEdit"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnExtraDelete"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnExtraClear"),                   CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtCcompiler"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnCcompiler"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtCPPcompiler"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnCPPcompiler"),                  CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtLinker"),                       CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnLinker"),                       CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtLibLinker"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnLibLinker"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbDebugger"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtResComp"),                      CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnResComp"),                      CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("txtMake"),                         CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnMake"),                         CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("cmbCompiler"),                     CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnIgnoreAdd"),                    CompilerOptionsDlg::OnUpdateUI)
    EVT_UPDATE_UI(            XRCID("btnIgnoreRemove"),                 CompilerOptionsDlg::OnUpdateUI)
    //
    EVT_TREE_SEL_CHANGED(      XRCID("tcScope"),                        CompilerOptionsDlg::OnTreeSelectionChange)
    EVT_TREE_SEL_CHANGING(     XRCID("tcScope"),                        CompilerOptionsDlg::OnTreeSelectionChanging)
    EVT_CHOICE(                XRCID("cmbCategory"),                    CompilerOptionsDlg::OnCategoryChanged)
    EVT_CHOICE(                XRCID("cmbCompiler"),                    CompilerOptionsDlg::OnCompilerChanged)
    EVT_LISTBOX_DCLICK(        XRCID("lstVars"),                        CompilerOptionsDlg::OnEditVarClick)
    EVT_BUTTON(                XRCID("btnSetDefaultCompiler"),          CompilerOptionsDlg::OnSetDefaultCompilerClick)
    EVT_BUTTON(                XRCID("btnAddCompiler"),                 CompilerOptionsDlg::OnAddCompilerClick)
    EVT_BUTTON(                XRCID("btnRenameCompiler"),              CompilerOptionsDlg::OnEditCompilerClick)
    EVT_BUTTON(                XRCID("btnDelCompiler"),                 CompilerOptionsDlg::OnRemoveCompilerClick)
    EVT_BUTTON(                XRCID("btnResetCompiler"),               CompilerOptionsDlg::OnResetCompilerClick)
    EVT_BUTTON(                XRCID("btnAddDir"),                      CompilerOptionsDlg::OnAddDirClick)
    EVT_BUTTON(                XRCID("btnEditDir"),                     CompilerOptionsDlg::OnEditDirClick)
    EVT_LISTBOX_DCLICK(        XRCID("lstIncludeDirs"),                 CompilerOptionsDlg::OnEditDirClick)
    EVT_LISTBOX_DCLICK(        XRCID("lstLibDirs"),                     CompilerOptionsDlg::OnEditDirClick)
    EVT_LISTBOX_DCLICK(        XRCID("lstResDirs"),                     CompilerOptionsDlg::OnEditDirClick)
    EVT_BUTTON(                XRCID("btnDelDir"),                      CompilerOptionsDlg::OnRemoveDirClick)
    EVT_BUTTON(                XRCID("btnClearDir"),                    CompilerOptionsDlg::OnClearDirClick)
    EVT_BUTTON(                XRCID("btnCopyDirs"),                    CompilerOptionsDlg::OnCopyDirsClick)
    EVT_BUTTON(                XRCID("btnAddLib"),                      CompilerOptionsDlg::OnAddLibClick)
    EVT_BUTTON(                XRCID("btnEditLib"),                     CompilerOptionsDlg::OnEditLibClick)
    EVT_LISTBOX_DCLICK(        XRCID("lstLibs"),                        CompilerOptionsDlg::OnEditLibClick)
    EVT_BUTTON(                XRCID("btnDelLib"),                      CompilerOptionsDlg::OnRemoveLibClick)
    EVT_BUTTON(                XRCID("btnClearLib"),                    CompilerOptionsDlg::OnClearLibClick)
    EVT_BUTTON(                XRCID("btnCopyLibs"),                    CompilerOptionsDlg::OnCopyLibsClick)
    EVT_LISTBOX_DCLICK(        XRCID("lstExtraPaths"),                  CompilerOptionsDlg::OnEditExtraPathClick)
    EVT_BUTTON(                XRCID("btnExtraAdd"),                    CompilerOptionsDlg::OnAddExtraPathClick)
    EVT_BUTTON(                XRCID("btnExtraEdit"),                   CompilerOptionsDlg::OnEditExtraPathClick)
    EVT_BUTTON(                XRCID("btnExtraDelete"),                 CompilerOptionsDlg::OnRemoveExtraPathClick)
    EVT_BUTTON(                XRCID("btnExtraClear"),                  CompilerOptionsDlg::OnClearExtraPathClick)
    EVT_BUTTON(                XRCID("btnMoveLibUp"),                   CompilerOptionsDlg::OnMoveLibUpClick)
    EVT_BUTTON(                XRCID("btnMoveLibDown"),                 CompilerOptionsDlg::OnMoveLibDownClick)
    EVT_BUTTON(                XRCID("btnMoveDirUp"),                   CompilerOptionsDlg::OnMoveDirUpClick)
    EVT_BUTTON(                XRCID("btnMoveDirDown"),                 CompilerOptionsDlg::OnMoveDirDownClick)
    EVT_BUTTON(                XRCID("btnAddVar"),                      CompilerOptionsDlg::OnAddVarClick)
    EVT_BUTTON(                XRCID("btnEditVar"),                     CompilerOptionsDlg::OnEditVarClick)
    EVT_BUTTON(                XRCID("btnDeleteVar"),                   CompilerOptionsDlg::OnRemoveVarClick)
    EVT_BUTTON(                XRCID("btnClearVar"),                    CompilerOptionsDlg::OnClearVarClick)
    EVT_BUTTON(                XRCID("btnMasterPath"),                  CompilerOptionsDlg::OnMasterPathClick)
    EVT_BUTTON(                XRCID("btnAutoDetect"),                  CompilerOptionsDlg::OnAutoDetectClick)
    EVT_BUTTON(                XRCID("btnCcompiler"),                   CompilerOptionsDlg::OnSelectProgramClick)
    EVT_BUTTON(                XRCID("btnCPPcompiler"),                 CompilerOptionsDlg::OnSelectProgramClick)
    EVT_BUTTON(                XRCID("btnLinker"),                      CompilerOptionsDlg::OnSelectProgramClick)
    EVT_BUTTON(                XRCID("btnLibLinker"),                   CompilerOptionsDlg::OnSelectProgramClick)
    EVT_BUTTON(                XRCID("btnResComp"),                     CompilerOptionsDlg::OnSelectProgramClick)
    EVT_BUTTON(                XRCID("btnMake"),                        CompilerOptionsDlg::OnSelectProgramClick)
    EVT_BUTTON(                XRCID("btnAdvanced"),                    CompilerOptionsDlg::OnAdvancedClick)
    EVT_BUTTON(                XRCID("btnIgnoreAdd"),                   CompilerOptionsDlg::OnIgnoreAddClick)
    EVT_BUTTON(                XRCID("btnIgnoreRemove"),                CompilerOptionsDlg::OnIgnoreRemoveClick)
    EVT_CHOICE(                XRCID("cmbCompilerPolicy"),              CompilerOptionsDlg::OnDirty)
    EVT_CHOICE(                XRCID("cmbLinkerPolicy"),                CompilerOptionsDlg::OnDirty)
    EVT_CHOICE(                XRCID("cmbIncludesPolicy"),              CompilerOptionsDlg::OnDirty)
    EVT_CHOICE(                XRCID("cmbLibDirsPolicy"),               CompilerOptionsDlg::OnDirty)
    EVT_CHOICE(                XRCID("cmbResDirsPolicy"),               CompilerOptionsDlg::OnDirty)
    EVT_CHOICE(                XRCID("cmbLogging"),                     CompilerOptionsDlg::OnDirty)
    EVT_CHOICE(                XRCID("chLinkerExe"),                    CompilerOptionsDlg::OnDirty)
    EVT_CHECKBOX(              XRCID("chkAlwaysRunPost"),               CompilerOptionsDlg::OnDirty)
    EVT_CHECKBOX(              XRCID("chkNonPlatComp"),                 CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtCompilerOptions"),             CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtResourceCompilerOptions"),     CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtCompilerDefines"),             CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtLinkerOptions"),               CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtCmdBefore"),                   CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtCmdAfter"),                    CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtMasterPath"),                  CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtCcompiler"),                   CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtCPPcompiler"),                 CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtLinker"),                      CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtLibLinker"),                   CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtResComp"),                     CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtMake"),                        CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtMakeCmd_Build"),               CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtMakeCmd_Compile"),             CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtMakeCmd_Clean"),               CompilerOptionsDlg::OnDirty)
//    EVT_TEXT(                  XRCID("txtMakeCmd_DistClean"),           CompilerOptionsDlg::OnDirty)
    EVT_TEXT(                  XRCID("txtMakeCmd_AskRebuildNeeded"),    CompilerOptionsDlg::OnDirty)
//    EVT_TEXT(                  XRCID("txtMakeCmd_SilentBuild"),         CompilerOptionsDlg::OnDirty)
    EVT_CHAR_HOOK(CompilerOptionsDlg::OnMyCharHook)

    EVT_PG_CHANGED(            XRCID("pgCompilerFlags"),                CompilerOptionsDlg::OnOptionChanged)
    EVT_PG_RIGHT_CLICK(        XRCID("pgCompilerFlags"),                CompilerOptionsDlg::OnFlagsPopup)
    EVT_PG_DOUBLE_CLICK(       XRCID("pgCompilerFlags"),                CompilerOptionsDlg::OnOptionDoubleClick)
END_EVENT_TABLE()

class ScopeTreeData : public wxTreeItemData
{
    public:
        ScopeTreeData(cbProject* project, ProjectBuildTarget* target){ m_Project = project; m_Target = target; }
        cbProject* GetProject(){ return m_Project; }
        ProjectBuildTarget* GetTarget(){ return m_Target; }
    private:
        cbProject* m_Project;
        ProjectBuildTarget* m_Target;
};

struct VariableListClientData : wxClientData
{
    VariableListClientData(const wxString &key, const wxString &value) : key(key), value(value) {}
    wxString key, value;
};

class IntClientData : public wxClientData
{
    public:
        IntClientData(int value) : m_data(value) {}
        void SetData(int value) {m_data = value;}
        int GetData() const {return m_data;}

    private:
        int m_data;
};

namespace
{
    int GetIndex(wxChoice* choice, int n)
    {
        if (!choice || (n == -1))
            return -1;

        IntClientData* data = dynamic_cast <IntClientData *> (choice->GetClientObject(n));
        return data ? data->GetData() : -1;
    }

    int GetSelectionIndex(wxChoice* choice)
    {
        if (!choice)
            return -1;

        return GetIndex(choice, choice->GetSelection());
    }

    int GetIndexPosition(wxChoice* choice, int index)
    {
        int position = -1;
        if (choice)
        {
            const int count = choice->GetCount();
            for (int n = 0; n < count; ++n)
            {
                if (GetIndex(choice, n) == index)
                {
                    position = n;
                    break;
                }
            }
        }

        return position;
    }

    int SetSelection(wxChoice* choice, int index)
    {
        const int pos = GetIndexPosition(choice, index);
        if (choice)
            choice->SetSelection(pos);

        return pos;
    }
}

/*
    CompilerOptions can exist on 3 different levels :
    Level 1 : compiler level
        - the options exist on the global level of the compiler
    Level 2 : project level
        - the options exist on the level of the project
    Level 3 : the target level
        - the options exist on the level of the target
*/

CompilerOptionsDlg::CompilerOptionsDlg(wxWindow* parent, CompilerGCC* compiler, cbProject* project, ProjectBuildTarget* target) :
    m_FlagsPG(nullptr),
    m_Compiler(compiler),
    m_CurrentCompilerIdx(0),
    m_pProject(project),
    m_pTarget(target),
    m_bDirty(false),
    m_BuildingTree(false)
{
    wxXmlResource::Get()->LoadPanel(this, parent, _T("dlgCompilerOptions"));

    m_FlagsPG = new wxPropertyGrid(this, XRCID("pgCompilerFlags"), wxDefaultPosition, wxDefaultSize,
                                   wxTAB_TRAVERSAL|wxPG_SPLITTER_AUTO_CENTER);
    m_FlagsPG->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
    m_FlagsPG->SetColumnProportion(0, 70);
    m_FlagsPG->SetColumnProportion(1, 30);

    m_FlagsPG->SetMinSize(wxSize(400, 400));
    wxXmlResource::Get()->AttachUnknownControl(wxT("pgCompilerFlags"), m_FlagsPG);

    if (m_pProject)
    {
        bool hasBuildScripts = m_pProject->GetBuildScripts().GetCount() != 0;
        if (!hasBuildScripts)
        {
            // look in targets
            for (int x = 0; x < m_pProject->GetBuildTargetsCount(); ++x)
            {
                ProjectBuildTarget* curr_target = m_pProject->GetBuildTarget(x);
                hasBuildScripts = curr_target->GetBuildScripts().GetCount() != 0;
                if (hasBuildScripts)
                    break;
            }
        }

        XRCCTRL(*this, "lblBuildScriptsNote", wxStaticText)->Show(hasBuildScripts);
    }

    wxTreeCtrl* tree = XRCCTRL(*this, "tcScope", wxTreeCtrl);
    wxSizer* sizer = tree->GetContainingSizer();
    wxNotebook* nb = XRCCTRL(*this, "nbMain", wxNotebook);
    if (!m_pProject)
    {
        // global settings
        SetLabel(_("Compiler Settings"));
        sizer->Show(tree,false);
        sizer->Detach(tree);
        nb->DeletePage(6); // remove "Make" page
        nb->DeletePage(3); // remove "Commands" page
    }
    else
    {
        // project settings
        nb->DeletePage(8); // remove "Other settings" page
        nb->DeletePage(7); // remove "Build options" page
        nb->DeletePage(4); // remove "Toolchain executables" page

        // remove "Compiler" buttons
        wxWindow* win = XRCCTRL(*this, "btnAddCompiler", wxButton);
        wxSizer* sizer2 = win->GetContainingSizer();
        sizer2->Clear(true);
        sizer2->Layout();

        // disable "Make" elements, if project is not using custom makefile
        bool en = project->IsMakefileCustom();
        XRCCTRL(*this, "txtMakeCmd_Build", wxTextCtrl)->Enable(en);
        XRCCTRL(*this, "txtMakeCmd_Compile", wxTextCtrl)->Enable(en);
        XRCCTRL(*this, "txtMakeCmd_Clean", wxTextCtrl)->Enable(en);
        XRCCTRL(*this, "txtMakeCmd_DistClean", wxTextCtrl)->Enable(en);
        XRCCTRL(*this, "txtMakeCmd_AskRebuildNeeded", wxTextCtrl)->Enable(en);
        XRCCTRL(*this, "txtMakeCmd_SilentBuild", wxTextCtrl)->Enable(en);
    }

    // let's start filling in all the panels of the configuration dialog
    // there are compiler dependent settings and compiler independent settings

    // compiler independent (so settings these ones is sufficient)
    DoFillOthers();
    DoFillTree();
    int compilerIdx = CompilerFactory::GetCompilerIndex(CompilerFactory::GetDefaultCompilerID());
    if (m_pTarget)
        compilerIdx = CompilerFactory::GetCompilerIndex(m_pTarget->GetCompilerID());
    else if (m_pProject)
        compilerIdx = CompilerFactory::GetCompilerIndex(m_pProject->GetCompilerID());
    if ((m_pTarget || m_pProject) && compilerIdx == -1)
    {   // unknown user compiler
        // similar code can be found @ OnTreeSelectionChange()
        // see there for more info : duplicate code now, since here we still need
        // to fill in the compiler list for the choice control, where in
        // OnTreeSelectionChange we just need to set an entry
        // TODO : make 1 help method out of this, with some argument indicating
        // to fill the choice list, or break it in 2 methods with the list filling in between them
        // or maybe time will bring even brighter ideas
        wxString CompilerId = m_pTarget?m_pTarget->GetCompilerID():m_pProject->GetCompilerID();
        wxString msg;
        msg.Printf(_("The defined compiler cannot be located (ID: %s).\n"
                     "Please choose the compiler you want to use instead and click \"OK\".\n"
                     "If you click \"Cancel\", the project/target will remain configured for\n"
                     "that compiler and consequently can not be configured and will not be built."),
                    CompilerId.wx_str());
        Compiler* comp = nullptr;
        if ((m_pTarget && m_pTarget->SupportsCurrentPlatform()) || (!m_pTarget && m_pProject))
            comp = CompilerFactory::SelectCompilerUI(msg);

        if (comp)
        {   // a new compiler was chosen, proceed as if the user manually selected another compiler
            // that means set the compiler selection list accordingly
            // and go directly to (On)CompilerChanged
            int NewCompilerIdx = CompilerFactory::GetCompilerIndex(comp);
            DoFillCompilerSets(NewCompilerIdx);
            wxCommandEvent Dummy;
            OnCompilerChanged(Dummy);
        }
        else
        {   // the user cancelled and wants to keep the compiler
            DoFillCompilerSets(compilerIdx);
            if (nb)
                nb->Disable();
        }
    }
    else
    {
        if (!CompilerFactory::GetCompiler(compilerIdx))
            compilerIdx = 0;
        DoFillCompilerSets(compilerIdx);
        m_Options = CompilerFactory::GetCompiler(compilerIdx)->GetOptions();
        m_CurrentCompilerIdx = compilerIdx;
        // compiler dependent settings
        DoFillCompilerDependentSettings();
    }
    if (m_pTarget && m_pTarget->GetTargetType() == ttCommandsOnly)
    {
        // disable pages for commands only target
        nb->GetPage(0)->Disable(); // Compiler settings
        nb->GetPage(1)->Disable(); // Linker settings
        nb->GetPage(2)->Disable(); // Search directories
        nb->GetPage(5)->Disable(); // "Make" commands
        nb->SetSelection(3);       // Pre/post build steps
    }
    else
        nb->SetSelection(0);
    sizer->Layout();
    Layout();
    GetSizer()->Layout();
    GetSizer()->SetSizeHints(this);
#ifdef __WXMAC__
    // seems it's not big enough on the Apple/Mac : hacking time
    int min_width, min_height;
    GetSize(&min_width, &min_height);
    this->SetSizeHints(min_width+140,min_height,-1,-1);
#endif
    this->SetSize(-1, -1, 0, 0);
    // disable some elements, if project is using custom makefile
    // we do this after the layout is done, so the resulting dialog has always the same size
    if (project && project->IsMakefileCustom())
    {
        nb->RemovePage(2); // remove "Search directories" page
        nb->RemovePage(1); // remove "Linker settings" page
        nb->RemovePage(0); // remove "Compiler settings" page
        XRCCTRL(*this, "tabCompiler", wxPanel)->Show(false);
        XRCCTRL(*this, "tabLinker", wxPanel)->Show(false);
        XRCCTRL(*this, "tabDirs", wxPanel)->Show(false);
    }

    Fit();
} // constructor

CompilerOptionsDlg::~CompilerOptionsDlg()
{
    //dtor
}

void CompilerOptionsDlg::DoFillCompilerSets(int compilerIdx)
{
    wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
    cmb->Clear();
    const int defaultCompilerIdx = CompilerFactory::GetCompilerIndex(CompilerFactory::GetDefaultCompilerID());
    const int compilerCount = CompilerFactory::GetCompilersCount();
    for (int i = 0; i < compilerCount; ++i)
    {
        Compiler* compiler = CompilerFactory::GetCompiler(i);
        if (compiler) // && (!m_pProject || compiler->IsValid()))
        {
            wxString compilerName = compiler->GetName();
            if (i == defaultCompilerIdx)
                compilerName += " " + _("(default)");

            cmb->Append(compilerName, new IntClientData(i));
        }

    }

//    int compilerIdx = CompilerFactory::GetCompilerIndex(CompilerFactory::GetDefaultCompilerID());
//    if (m_pTarget)
//        compilerIdx = CompilerFactory::GetCompilerIndex(m_pTarget->GetCompilerID());
//    else if (m_pProject)
//        compilerIdx = CompilerFactory::GetCompilerIndex(m_pProject->GetCompilerID());

//    if (!CompilerFactory::GetCompiler(compilerIdx))
//        compilerIdx = 0;
//    m_Options = CompilerFactory::GetCompiler(compilerIdx)->GetOptions();
    SetSelection(cmb, compilerIdx);

//    m_CurrentCompilerIdx = compilerIdx;
} // DoFillCompilerSets

void CompilerOptionsDlg::DoFillCompilerDependentSettings()
{
    DoFillCompilerPrograms();    // the programs executable's ...
    DoLoadOptions();
    DoFillVars();
    // by the way we listen to changes in the textctrl, we also end up in the callbacks as
    // a result of wxTextCtrl::SetValue, the preceding called methods did some of those -> reset dirty flag
    m_bDirty = false;
    m_bFlagsDirty = false;
} // DoFillCompilerDependentSettings

void CompilerOptionsDlg::DoSaveCompilerDependentSettings()
{
    DoSaveCompilerPrograms();
    DoSaveOptions();
    DoSaveVars();
    if (m_bFlagsDirty)
        DoSaveCompilerDefinition();
    ProjectTargetCompilerAdjust();
    m_bDirty = false;
    m_bFlagsDirty = false;
} // DoSaveCompilerDependentSettings

inline void ArrayString2ListBox(const wxArrayString& array, wxListBox* control)
{
    control->Clear();
    int count = array.GetCount();
    for (int i = 0; i < count; ++i)
    {
        if (!array[i].IsEmpty())
            control->Append(array[i]);
    }
} // ArrayString2ListBox

inline void ListBox2ArrayString(wxArrayString& array, const wxListBox* control)
{
    array.Clear();
    int count = control->GetCount();
    for (int i = 0; i < count; ++i)
    {
        wxString tmp = control->GetString(i);
        if (!tmp.IsEmpty())
            array.Add(tmp);
    }
} // ListBox2ArrayString

void CompilerOptionsDlg::DoFillCompilerPrograms()
{
    if (m_pProject)
        return; // no "Programs" page

    const Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
    if (!compiler)
        return;
    const CompilerPrograms& progs = compiler->GetPrograms();

    XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->SetValue(compiler->GetMasterPath());
    XRCCTRL(*this, "txtCcompiler", wxTextCtrl)->SetValue(progs.C);
    XRCCTRL(*this, "txtCPPcompiler", wxTextCtrl)->SetValue(progs.CPP);
    XRCCTRL(*this, "txtLinker", wxTextCtrl)->SetValue(progs.LD);
    XRCCTRL(*this, "txtLibLinker", wxTextCtrl)->SetValue(progs.LIB);
    wxChoice *cmbDebugger = XRCCTRL(*this, "cmbDebugger", wxChoice);
    if (cmbDebugger)
    {
        cmbDebugger->Clear();
        // Add an invalid debugger entry and store the old value in the client data, so no user settings are changed.
        cmbDebugger->Append(_("--- Invalid debugger ---"), new wxStringClientData(progs.DBGconfig));
        cmbDebugger->SetSelection(0);

        const DebuggerManager::RegisteredPlugins &plugins = Manager::Get()->GetDebuggerManager()->GetAllDebuggers();
        for (DebuggerManager::RegisteredPlugins::const_iterator it = plugins.begin(); it != plugins.end(); ++it)
        {
            const DebuggerManager::PluginData &data = it->second;
            for (DebuggerManager::ConfigurationVector::const_iterator itConf = data.GetConfigurations().begin();
                 itConf != data.GetConfigurations().end();
                 ++itConf)
            {
                const wxString &def = it->first->GetSettingsName() + wxT(":") + (*itConf)->GetName();
                int index = cmbDebugger->Append(it->first->GetGUIName() + wxT(" : ") + (*itConf)->GetName(),
                                                new wxStringClientData(def));
                if (def == progs.DBGconfig)
                    cmbDebugger->SetSelection(index);
            }
        }
    }

    XRCCTRL(*this, "txtResComp", wxTextCtrl)->SetValue(progs.WINDRES);
    XRCCTRL(*this, "txtMake", wxTextCtrl)->SetValue(progs.MAKE);

    const wxArrayString& extraPaths = compiler->GetExtraPaths();
    ArrayString2ListBox(extraPaths, XRCCTRL(*this, "lstExtraPaths", wxListBox));
} // DoFillCompilerPrograms

void CompilerOptionsDlg::DoFillVars()
{
    wxListBox* lst = XRCCTRL(*this, "lstVars", wxListBox);
    if (!lst)
        return;
    lst->Clear();
    const StringHash* vars = nullptr;
    const CompileOptionsBase* base = GetVarsOwner();
    if (base)
    {
        vars = &base->GetAllVars();
    }
    if (!vars)
        return;
    for (StringHash::const_iterator it = vars->begin(); it != vars->end(); ++it)
    {
        wxString text = it->first + _T(" = ") + it->second;
        lst->Append(text, new VariableListClientData(it->first, it->second));
    }
} // DoFillVars

void CompilerOptionsDlg::DoFillOthers()
{
    if (m_pProject)
        return; // projects don't have Other tab

    ConfigManager* cfg = Manager::Get()->GetConfigManager("compiler");

    wxCheckBox* chk = XRCCTRL(*this, "chkIncludeFileCwd", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/include_file_cwd", false));

    chk = XRCCTRL(*this, "chkIncludePrjCwd", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/include_prj_cwd", false));

    chk = XRCCTRL(*this, "chkSkipIncludeDeps", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/skip_include_deps", false));

    chk = XRCCTRL(*this, "chkSaveHtmlLog", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/save_html_build_log", false));

    chk = XRCCTRL(*this, "chkFullHtmlLog", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/save_html_build_log/full_command_line", false));

    chk = XRCCTRL(*this, "chkBuildProgressBar", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/build_progress/bar", false));

    chk = XRCCTRL(*this, "chkBuildProgressPerc", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/build_progress/percentage", false));

    wxSpinCtrl* spn = XRCCTRL(*this, "spnParallelProcesses", wxSpinCtrl);
    if (spn)
        spn->SetValue(cfg->ReadInt("/parallel_processes", 0));

    spn = XRCCTRL(*this, "spnMaxErrors", wxSpinCtrl);
    if (spn)
    {
        spn->SetRange(0, 1000);
        spn->SetValue(cfg->ReadInt("/max_reported_errors", 50));
    }

    chk = XRCCTRL(*this, "chkRebuildSeperately", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/rebuild_seperately", false));

    wxListBox* lst = XRCCTRL(*this, "lstIgnore", wxListBox);
    if (lst)
    {
        wxArrayString IgnoreOutput;
        IgnoreOutput = cfg->ReadArrayString("/ignore_output");
        ArrayString2ListBox(IgnoreOutput, lst);
    }

    chk = XRCCTRL(*this, "chkNonPlatComp", wxCheckBox);
    if (chk)
        chk->SetValue(cfg->ReadBool("/non_plat_comp", false));
} // DoFillOthers

void CompilerOptionsDlg::DoFillTree()
{
    m_BuildingTree = true;
    wxTreeCtrl* tc = XRCCTRL(*this, "tcScope", wxTreeCtrl);
    tc->DeleteAllItems();

    wxTreeItemId root;
    wxTreeItemId selectedItem;

    if (!m_pProject)
    {
        // global settings
        root = tc->AddRoot(_("Global options"), -1, -1);
        selectedItem = root;
    }
    else
    {
        // project settings
        // in case you wonder : the delete of data will be done by the wxTreeCtrl
        ScopeTreeData* data = new ScopeTreeData(m_pProject, nullptr);
        root = tc->AddRoot(m_pProject->GetTitle(), -1, -1, data);
        selectedItem = root;
        for (int x = 0; x < m_pProject->GetBuildTargetsCount(); ++x)
        {
            ProjectBuildTarget* target = m_pProject->GetBuildTarget(x);
            data = new ScopeTreeData(m_pProject, target);
            wxTreeItemId targetItem = tc->AppendItem(root, target->GetTitle(), -1, -1, data);
            if (target == m_pTarget)
                selectedItem = targetItem;
        }
    }
    // normally the target should be found in the targets of the project
    // in case it is not, we will reset m_pTarget to 0 (in sync with tree selection)
    if (selectedItem == root)
        m_pTarget = nullptr;

    tc->Expand(root);
    tc->SelectItem(selectedItem);
    m_BuildingTree = false;
} // DoFillTree

void CompilerOptionsDlg::DoFillOptions()
{
    m_FlagsPG->Freeze();
    m_FlagsPG->Clear();
    typedef std::map<wxString, wxPropertyCategory*> MapCategories;
    MapCategories categories;

    // If there is a "General" category make sure it is added first.
    for (size_t i = 0; i < m_Options.GetCount(); ++i)
    {
        const CompOption* option = m_Options.GetOption(i);
        if (option->category == wxT("General"))
        {
            wxPropertyCategory *categoryProp = new wxPropertyCategory(option->category);
            m_FlagsPG->Append(categoryProp);
            categories[option->category] = categoryProp;
            break;
        }
    }

    // Add all flags and categories to the property grid
    for (size_t i = 0; i < m_Options.GetCount(); ++i)
    {
        const CompOption* option = m_Options.GetOption(i);
        wxPropertyCategory *categoryProp = nullptr;
        MapCategories::iterator itCat = categories.find(option->category);
        if (itCat != categories.end())
            categoryProp = itCat->second;
        else
        {
            categoryProp = new wxPropertyCategory(option->category);
            m_FlagsPG->Append(categoryProp);
            categories[option->category] = categoryProp;
        }

        wxPGProperty *prop = new wxBoolProperty(option->name, wxPG_LABEL, option->enabled);
        m_FlagsPG->AppendIn(categoryProp, prop);
#if wxCHECK_VERSION(3, 3, 0)
        m_FlagsPG->SetPropertyAttribute(prop, wxPG_BOOL_USE_CHECKBOX, true, wxPGPropertyValuesFlags::Recurse);
#else
        m_FlagsPG->SetPropertyAttribute(prop, wxPG_BOOL_USE_CHECKBOX, true, wxPG_RECURSE);
#endif
    }

    wxPGProperty *root = m_FlagsPG->GetRoot();
    if (root)
    {
        const unsigned count = root->GetChildCount();
        for (unsigned ii = 0; ii < count; ++ii)
#if wxCHECK_VERSION(3, 3, 0)
            m_FlagsPG->SortChildren(root->Item(ii), wxPGPropertyValuesFlags::Recurse);
#else
            m_FlagsPG->SortChildren(root->Item(ii), wxPG_RECURSE);
#endif
    }
    m_FlagsPG->Thaw();
} // DoFillOptions

void CompilerOptionsDlg::TextToOptions()
{
    // disable all options
    for (size_t n = 0; n < m_Options.GetCount(); ++n)
    {
        if (CompOption* copt = m_Options.GetOption(n))
            copt->enabled = false;
    }

    wxString rest;

    Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);

    XRCCTRL(*this, "txtCompilerDefines", wxTextCtrl)->Clear();
    unsigned int i = 0;
    while (i < m_CompilerOptions.GetCount())
    {
        wxString opt = m_CompilerOptions.Item(i);
        opt = opt.Strip(wxString::both);
        CompOption* copt = m_Options.GetOptionByOption(opt);
        if (copt)
        {
//            Manager::Get()->GetLogManager()->DebugLog("Enabling option %s", copt->option.c_str());
            copt->enabled = true;
            m_CompilerOptions.RemoveAt(i, 1);
        }
        else if (compiler && opt.StartsWith(compiler->GetSwitches().defines, &rest))
        {
            // definition
            XRCCTRL(*this, "txtCompilerDefines", wxTextCtrl)->AppendText(rest);
            XRCCTRL(*this, "txtCompilerDefines", wxTextCtrl)->AppendText(_T("\n"));
            m_CompilerOptions.RemoveAt(i, 1);
        }
        else
            ++i;
    }
    i = 0;
    while (i < m_LinkerOptions.GetCount())
    {
        wxString opt = m_LinkerOptions.Item(i);
        opt = opt.Strip(wxString::both);
        CompOption* copt = m_Options.GetOptionByAdditionalLibs(opt);
        if (copt)
        {
//            Manager::Get()->GetLogManager()->DebugLog("Enabling option %s", copt->option.c_str());
            copt->enabled = true;
            m_LinkerOptions.RemoveAt(i, 1);
        }
        else
            ++i;
    }

    XRCCTRL(*this, "lstLibs", wxListBox)->Clear();
    for (unsigned int j = 0; j < m_LinkLibs.GetCount(); ++j)
        XRCCTRL(*this, "lstLibs", wxListBox)->Append(m_LinkLibs[j]);

    m_LinkLibs.Clear();
} // TextToOptions

inline void ArrayString2TextCtrl(const wxArrayString& array, wxTextCtrl* control)
{
    control->Clear();
    int count = array.GetCount();
    for (int i = 0; i < count; ++i)
    {
        if (!array[i].IsEmpty())
        {
            control->AppendText(array[i]);
            control->AppendText(_T('\n'));
        }
    }
} // ArrayString2TextCtrl

inline void DoGetCompileOptions(wxArrayString& array, const wxTextCtrl* control)
{
/* NOTE (mandrav#1#): Under Gnome2, wxTextCtrl::GetLineLength() returns always 0,
                      so wxTextCtrl::GetLineText() is always empty...
                      Now, we 're breaking up by newlines. */
    array.Clear();
#if 1
    wxString tmp = control->GetValue();
    int nl = tmp.Find(_T('\n'));
    wxString line;
    if (nl == -1)
    {
        line = tmp;
        tmp = _T("");
    }
    else
        line = tmp.Left(nl);
    while (nl != -1 || !line.IsEmpty())
    {
//        Manager::Get()->GetLogManager()->DebugLog("%s text=%s", control->GetName().c_str(), line.c_str());
        if (!line.IsEmpty())
        {
            // just to make sure..
            line.Replace(_T("\r"), _T(" "), true); // remove CRs
            line.Replace(_T("\n"), _T(" "), true); // remove LFs
            array.Add(line.Strip(wxString::both));
        }
        tmp.Remove(0, nl + 1);
        nl = tmp.Find(_T('\n'));
        if (nl == -1)
        {
            line = tmp;
            tmp = _T("");
        }
        else
            line = tmp.Left(nl);
    }
#else
    int count = control->GetNumberOfLines();
    for (int i = 0; i < count; ++i)
    {
        wxString tmp = control->GetLineText(i);
        if (!tmp.IsEmpty())
        {
            tmp.Replace(_T("\r"), _T(" "), true); // remove CRs
            tmp.Replace(_T("\n"), _T(" "), true); // remove LFs
            array.Add(tmp.Strip(wxString::both));
        }
    }
#endif
} // DoGetCompileOptions

void CompilerOptionsDlg::DoLoadOptions()
{
    wxArrayString CommandsBeforeBuild;
    wxArrayString CommandsAfterBuild;
    bool AlwaysUsePost = false;
    wxArrayString IncludeDirs;
    wxArrayString LibDirs;
    wxArrayString ResDirs;

    if (!m_pProject && !m_pTarget)
    {
        // global options
        const Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
        if (compiler)
        {
            IncludeDirs = compiler->GetIncludeDirs();
            ResDirs = compiler->GetResourceIncludeDirs();
            LibDirs = compiler->GetLibDirs();
            m_CompilerOptions = compiler->GetCompilerOptions();
            m_ResourceCompilerOptions = compiler->GetResourceCompilerOptions();
            m_LinkerOptions = compiler->GetLinkerOptions();
            m_LinkLibs = compiler->GetLinkLibs();

            wxChoice* cmbLogging = XRCCTRL(*this, "cmbLogging", wxChoice);
            if (cmbLogging)
                cmbLogging->SetSelection((int)compiler->GetSwitches().logging);

            wxChoice *cmbLinkerExe = XRCCTRL(*this, "chLinkerExe", wxChoice);
            cmbLinkerExe->Show(false);
            wxStaticText *txtLinkerExe = XRCCTRL(*this, "txtLinkerExe", wxStaticText);
            txtLinkerExe->Show(false);
        }
    }
    else
    {
        if (!m_pTarget)
        {
            // project options
            SetLabel(_("Project build options"));
            IncludeDirs = m_pProject->GetIncludeDirs();
            ResDirs = m_pProject->GetResourceIncludeDirs();
            LibDirs = m_pProject->GetLibDirs();
            m_CompilerOptions = m_pProject->GetCompilerOptions();
            m_ResourceCompilerOptions = m_pProject->GetResourceCompilerOptions();
            m_LinkerOptions = m_pProject->GetLinkerOptions();
            m_LinkLibs = m_pProject->GetLinkLibs();

            CommandsAfterBuild = m_pProject->GetCommandsAfterBuild();
            CommandsBeforeBuild = m_pProject->GetCommandsBeforeBuild();
            AlwaysUsePost = m_pProject->GetAlwaysRunPostBuildSteps();

            XRCCTRL(*this, "txtMakeCmd_Build",            wxTextCtrl)->SetValue(m_pProject->GetMakeCommandFor(mcBuild));
            XRCCTRL(*this, "txtMakeCmd_Compile",          wxTextCtrl)->SetValue(m_pProject->GetMakeCommandFor(mcCompileFile));
            XRCCTRL(*this, "txtMakeCmd_Clean",            wxTextCtrl)->SetValue(m_pProject->GetMakeCommandFor(mcClean));
            XRCCTRL(*this, "txtMakeCmd_DistClean",        wxTextCtrl)->SetValue(m_pProject->GetMakeCommandFor(mcDistClean));
            XRCCTRL(*this, "txtMakeCmd_AskRebuildNeeded", wxTextCtrl)->SetValue(m_pProject->GetMakeCommandFor(mcAskRebuildNeeded));
            XRCCTRL(*this, "txtMakeCmd_SilentBuild",      wxTextCtrl)->SetValue(m_pProject->GetMakeCommandFor(mcSilentBuild));
        }
        else
        {
            // target options
            SetLabel(_("Target build options: ") + m_pTarget->GetTitle());
            IncludeDirs = m_pTarget->GetIncludeDirs();
            ResDirs = m_pTarget->GetResourceIncludeDirs();
            LibDirs = m_pTarget->GetLibDirs();
            m_CompilerOptions = m_pTarget->GetCompilerOptions();
            m_ResourceCompilerOptions = m_pTarget->GetResourceCompilerOptions();
            m_LinkerOptions = m_pTarget->GetLinkerOptions();
            m_LinkLibs = m_pTarget->GetLinkLibs();
            CommandsAfterBuild = m_pTarget->GetCommandsAfterBuild();
            CommandsBeforeBuild = m_pTarget->GetCommandsBeforeBuild();
            AlwaysUsePost = m_pTarget->GetAlwaysRunPostBuildSteps();
            XRCCTRL(*this, "cmbCompilerPolicy", wxChoice)->SetSelection(m_pTarget->GetOptionRelation(ortCompilerOptions));
            XRCCTRL(*this, "cmbLinkerPolicy",   wxChoice)->SetSelection(m_pTarget->GetOptionRelation(ortLinkerOptions));
            XRCCTRL(*this, "cmbIncludesPolicy", wxChoice)->SetSelection(m_pTarget->GetOptionRelation(ortIncludeDirs));
            XRCCTRL(*this, "cmbLibDirsPolicy",  wxChoice)->SetSelection(m_pTarget->GetOptionRelation(ortLibDirs));
            XRCCTRL(*this, "cmbResDirsPolicy",  wxChoice)->SetSelection(m_pTarget->GetOptionRelation(ortResDirs));

            XRCCTRL(*this, "txtMakeCmd_Build",            wxTextCtrl)->SetValue(m_pTarget->GetMakeCommandFor(mcBuild));
            XRCCTRL(*this, "txtMakeCmd_Compile",          wxTextCtrl)->SetValue(m_pTarget->GetMakeCommandFor(mcCompileFile));
            XRCCTRL(*this, "txtMakeCmd_Clean",            wxTextCtrl)->SetValue(m_pTarget->GetMakeCommandFor(mcClean));
            XRCCTRL(*this, "txtMakeCmd_DistClean",        wxTextCtrl)->SetValue(m_pTarget->GetMakeCommandFor(mcDistClean));
            XRCCTRL(*this, "txtMakeCmd_AskRebuildNeeded", wxTextCtrl)->SetValue(m_pTarget->GetMakeCommandFor(mcAskRebuildNeeded));
            XRCCTRL(*this, "txtMakeCmd_SilentBuild",      wxTextCtrl)->SetValue(m_pTarget->GetMakeCommandFor(mcSilentBuild));

            const LinkerExecutableOption linkerExecutable = m_pTarget->GetLinkerExecutable();
            XRCCTRL(*this, "chLinkerExe", wxChoice)->SetSelection(int(linkerExecutable));
        }
    }
    TextToOptions();

    DoFillOptions();
    ArrayString2ListBox(IncludeDirs,                XRCCTRL(*this, "lstIncludeDirs",             wxListBox));
    ArrayString2ListBox(LibDirs,                    XRCCTRL(*this, "lstLibDirs",                 wxListBox));
    ArrayString2ListBox(ResDirs,                    XRCCTRL(*this, "lstResDirs",                 wxListBox));
    ArrayString2TextCtrl(m_CompilerOptions,         XRCCTRL(*this, "txtCompilerOptions",         wxTextCtrl));
    ArrayString2TextCtrl(m_ResourceCompilerOptions, XRCCTRL(*this, "txtResourceCompilerOptions", wxTextCtrl));
    ArrayString2TextCtrl(m_LinkerOptions,           XRCCTRL(*this, "txtLinkerOptions",           wxTextCtrl));

    // only if "Commands" page exists
    if (m_pProject)
    {
        ArrayString2TextCtrl(CommandsBeforeBuild, XRCCTRL(*this, "txtCmdBefore", wxTextCtrl));
        ArrayString2TextCtrl(CommandsAfterBuild, XRCCTRL(*this, "txtCmdAfter", wxTextCtrl));
        XRCCTRL(*this, "chkAlwaysRunPost", wxCheckBox)->SetValue(AlwaysUsePost);
    }
} // DoLoadOptions

void CompilerOptionsDlg::OptionsToText()
{
    wxArrayString array;
    DoGetCompileOptions(array, XRCCTRL(*this, "txtCompilerDefines", wxTextCtrl));

    wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
    const Compiler* compiler = CompilerFactory::GetCompiler(GetSelectionIndex(cmb));

    for (unsigned int i = 0; i < array.GetCount(); ++i)
    {
        if (!array[i].IsEmpty())
        {
            if (array[i].StartsWith(compiler ? compiler->GetSwitches().genericSwitch : _T("-")))
            {
                if (m_CompilerOptions.Index(array[i]) == wxNOT_FOUND)
                    m_CompilerOptions.Add(array[i]);
            }
            else
            {
                if (compiler && m_CompilerOptions.Index(compiler->GetSwitches().defines + array[i]) == wxNOT_FOUND)
                    m_CompilerOptions.Add(compiler->GetSwitches().defines + array[i]);
            }
        }
    }

    wxArrayString compilerOpConflicts;
    wxArrayString linkerOpConflicts;
    for (size_t i = 0; i < m_Options.GetCount(); ++i)
    {
        CompOption* copt = m_Options.GetOption(i);
        if (copt->enabled)
        {
            if (!copt->option.Trim().IsEmpty()) // don't add empty options
                m_CompilerOptions.Insert(copt->option, 0);

            if (!copt->additionalLibs.Trim().IsEmpty())
            {
                if (m_LinkerOptions.Index(copt->additionalLibs) == wxNOT_FOUND)
                    m_LinkerOptions.Insert(copt->additionalLibs, 0);
            }
        }
        else
        {
            // mark items for removal
            if (m_CompilerOptions.Index(copt->option) != wxNOT_FOUND)
                compilerOpConflicts.Add(copt->option);
            if (m_LinkerOptions.Index(copt->additionalLibs) != wxNOT_FOUND)
                linkerOpConflicts.Add(copt->additionalLibs);
        }
    }

    if (!compilerOpConflicts.IsEmpty() || !linkerOpConflicts.IsEmpty())
    {
        wxString msg = _("The compiler flags\n  ")
                       + GetStringFromArray(compilerOpConflicts, wxT("\n  "))
                       + GetStringFromArray(linkerOpConflicts,   wxT("\n  "));
        msg.RemoveLast(2); // remove two trailing spaces
        msg += _("were stated in 'Other Options' but unchecked in 'Compiler Flags'.\n"
                 "Do you want to enable these flags?");
        AnnoyingDialog dlg(_("Enable compiler flags?"), msg, wxART_QUESTION,
                           AnnoyingDialog::YES_NO, AnnoyingDialog::rtNO);
        PlaceWindow(&dlg);
        if (dlg.ShowModal() == AnnoyingDialog::rtNO)
        {
            // for disabled options, remove relative text option *and*
            // relative linker option
            for (size_t i = 0; i < compilerOpConflicts.GetCount(); ++i)
                m_CompilerOptions.Remove(compilerOpConflicts[i]);
            for (size_t i = 0; i < linkerOpConflicts.GetCount(); ++i)
                m_LinkerOptions.Remove(linkerOpConflicts[i]);
        }
    }

    // linker options and libs
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    for (size_t i = 0; i < lstLibs->GetCount(); ++i)
        m_LinkLibs.Add(lstLibs->GetString(i));
} // OptionsToText

void CompilerOptionsDlg::DoSaveOptions()
{
    wxArrayString IncludeDirs;
    wxArrayString LibDirs;
    wxArrayString ResDirs;
    ListBox2ArrayString(IncludeDirs,               XRCCTRL(*this, "lstIncludeDirs",             wxListBox));
    ListBox2ArrayString(LibDirs,                   XRCCTRL(*this, "lstLibDirs",                 wxListBox));
    ListBox2ArrayString(ResDirs,                   XRCCTRL(*this, "lstResDirs",                 wxListBox));
    DoGetCompileOptions(m_CompilerOptions,         XRCCTRL(*this, "txtCompilerOptions",         wxTextCtrl));
    DoGetCompileOptions(m_ResourceCompilerOptions, XRCCTRL(*this, "txtResourceCompilerOptions", wxTextCtrl));
    DoGetCompileOptions(m_LinkerOptions,           XRCCTRL(*this, "txtLinkerOptions",           wxTextCtrl));
    OptionsToText();

    if (!m_pProject && !m_pTarget)
    {
        // global options
        Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
        if (compiler)
        {
            compiler->SetIncludeDirs(IncludeDirs);
            compiler->SetLibDirs(LibDirs);
            compiler->SetResourceIncludeDirs(ResDirs);
            compiler->SetCompilerOptions(m_CompilerOptions);
            compiler->SetResourceCompilerOptions(m_ResourceCompilerOptions);
            compiler->SetLinkerOptions(m_LinkerOptions);
            compiler->SetLinkLibs(m_LinkLibs);

            wxChoice* cmb = XRCCTRL(*this, "cmbLogging", wxChoice);
            if (cmb)
            {
                CompilerSwitches switches = compiler->GetSwitches();
                switches.logging = (CompilerLoggingType)cmb->GetSelection();
                compiler->SetSwitches(switches);
            }
        }
    }
    else
    {
        // only if "Commands" page exists
        wxArrayString CommandsBeforeBuild;
        wxArrayString CommandsAfterBuild;
        bool AlwaysUsePost = false;
        if (m_pProject)
        {
            AlwaysUsePost = XRCCTRL(*this, "chkAlwaysRunPost", wxCheckBox)->GetValue();
            DoGetCompileOptions(CommandsBeforeBuild, XRCCTRL(*this, "txtCmdBefore", wxTextCtrl));
            DoGetCompileOptions(CommandsAfterBuild, XRCCTRL(*this, "txtCmdAfter", wxTextCtrl));
        }
        if (!m_pTarget)
        {
            // project options
            m_pProject->SetIncludeDirs(IncludeDirs);
            m_pProject->SetResourceIncludeDirs(ResDirs);
            m_pProject->SetLibDirs(LibDirs);
            m_pProject->SetCompilerOptions(m_CompilerOptions);
            m_pProject->SetResourceCompilerOptions(m_ResourceCompilerOptions);
            m_pProject->SetLinkerOptions(m_LinkerOptions);
            m_pProject->SetLinkLibs(m_LinkLibs);
            m_pProject->SetCommandsBeforeBuild(CommandsBeforeBuild);
            m_pProject->SetCommandsAfterBuild(CommandsAfterBuild);
            m_pProject->SetAlwaysRunPostBuildSteps(AlwaysUsePost);

            m_pProject->SetMakeCommandFor(mcBuild, XRCCTRL(*this, "txtMakeCmd_Build", wxTextCtrl)->GetValue());
            m_pProject->SetMakeCommandFor(mcCompileFile, XRCCTRL(*this, "txtMakeCmd_Compile", wxTextCtrl)->GetValue());
            m_pProject->SetMakeCommandFor(mcClean, XRCCTRL(*this, "txtMakeCmd_Clean", wxTextCtrl)->GetValue());
            m_pProject->SetMakeCommandFor(mcDistClean, XRCCTRL(*this, "txtMakeCmd_DistClean", wxTextCtrl)->GetValue());
            m_pProject->SetMakeCommandFor(mcAskRebuildNeeded, XRCCTRL(*this, "txtMakeCmd_AskRebuildNeeded", wxTextCtrl)->GetValue());
//            m_pProject->SetMakeCommandFor(mcSilentBuild, XRCCTRL(*this, "txtMakeCmd_SilentBuild", wxTextCtrl)->GetValue());
            m_pProject->SetMakeCommandFor(mcSilentBuild, XRCCTRL(*this, "txtMakeCmd_Build", wxTextCtrl)->GetValue() + _T(" > $(CMD_NULL)"));
        }
        else
        {
            // target options
            m_pTarget->SetIncludeDirs(IncludeDirs);
            m_pTarget->SetResourceIncludeDirs(ResDirs);
            m_pTarget->SetLibDirs(LibDirs);
            m_pTarget->SetCompilerOptions(m_CompilerOptions);
            m_pTarget->SetResourceCompilerOptions(m_ResourceCompilerOptions);
            m_pTarget->SetLinkerOptions(m_LinkerOptions);
            m_pTarget->SetLinkLibs(m_LinkLibs);

            {
                int value = XRCCTRL(*this, "chLinkerExe", wxChoice)->GetSelection();
                LinkerExecutableOption linkerExe;
                if (value >= 0 && value < int(LinkerExecutableOption::Last))
                    linkerExe = LinkerExecutableOption(value);
                else
                    linkerExe = LinkerExecutableOption::AutoDetect;
                m_pTarget->SetLinkerExecutable(linkerExe);
            }

            m_pTarget->SetOptionRelation(ortCompilerOptions, OptionsRelation(XRCCTRL(*this, "cmbCompilerPolicy", wxChoice)->GetSelection()));
            m_pTarget->SetOptionRelation(ortLinkerOptions, OptionsRelation(XRCCTRL(*this, "cmbLinkerPolicy", wxChoice)->GetSelection()));
            m_pTarget->SetOptionRelation(ortIncludeDirs, OptionsRelation(XRCCTRL(*this, "cmbIncludesPolicy", wxChoice)->GetSelection()));
            m_pTarget->SetOptionRelation(ortLibDirs, OptionsRelation(XRCCTRL(*this, "cmbLibDirsPolicy", wxChoice)->GetSelection()));
            m_pTarget->SetOptionRelation(ortResDirs, OptionsRelation(XRCCTRL(*this, "cmbResDirsPolicy", wxChoice)->GetSelection()));
            m_pTarget->SetCommandsBeforeBuild(CommandsBeforeBuild);
            m_pTarget->SetCommandsAfterBuild(CommandsAfterBuild);
            m_pTarget->SetAlwaysRunPostBuildSteps(AlwaysUsePost);

            m_pTarget->SetMakeCommandFor(mcBuild, XRCCTRL(*this, "txtMakeCmd_Build", wxTextCtrl)->GetValue());
            m_pTarget->SetMakeCommandFor(mcCompileFile, XRCCTRL(*this, "txtMakeCmd_Compile", wxTextCtrl)->GetValue());
            m_pTarget->SetMakeCommandFor(mcClean, XRCCTRL(*this, "txtMakeCmd_Clean", wxTextCtrl)->GetValue());
            m_pTarget->SetMakeCommandFor(mcDistClean, XRCCTRL(*this, "txtMakeCmd_DistClean", wxTextCtrl)->GetValue());
            m_pTarget->SetMakeCommandFor(mcAskRebuildNeeded, XRCCTRL(*this, "txtMakeCmd_AskRebuildNeeded", wxTextCtrl)->GetValue());
//            m_pTarget->SetMakeCommandFor(mcSilentBuild, XRCCTRL(*this, "txtMakeCmd_SilentBuild", wxTextCtrl)->GetValue());
            m_pTarget->SetMakeCommandFor(mcSilentBuild, XRCCTRL(*this, "txtMakeCmd_Build", wxTextCtrl)->GetValue() + _T(" > $(CMD_NULL)"));
        }
    }
} // DoSaveOptions

void CompilerOptionsDlg::DoSaveCompilerPrograms()
{
    Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
    if (m_pProject || !compiler) // no "Programs" page or no compiler
        return;

    CompilerPrograms progs;
    wxString masterPath = XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->GetValue();
    progs.C       = (XRCCTRL(*this, "txtCcompiler",   wxTextCtrl)->GetValue()).Trim();
    progs.CPP     = (XRCCTRL(*this, "txtCPPcompiler", wxTextCtrl)->GetValue()).Trim();
    progs.LD      = (XRCCTRL(*this, "txtLinker",      wxTextCtrl)->GetValue()).Trim();
    progs.LIB     = (XRCCTRL(*this, "txtLibLinker",   wxTextCtrl)->GetValue()).Trim();
    progs.WINDRES = (XRCCTRL(*this, "txtResComp",     wxTextCtrl)->GetValue()).Trim();
    progs.MAKE    = (XRCCTRL(*this, "txtMake",        wxTextCtrl)->GetValue()).Trim();
    wxChoice *cmbDebugger = XRCCTRL(*this, "cmbDebugger", wxChoice);
    if (cmbDebugger)
    {
        const int index = cmbDebugger->GetSelection();
        const wxStringClientData* data = static_cast <const wxStringClientData *> (cmbDebugger->GetClientObject(index));
        progs.DBGconfig = data->GetData();
    }
    compiler->SetPrograms(progs);
    compiler->SetMasterPath(masterPath);
    // and the extra paths
    wxListBox* control = XRCCTRL(*this, "lstExtraPaths", wxListBox);
    if (control)
    {
        // get all listBox entries in array String
        wxArrayString extraPaths;
        ListBox2ArrayString(extraPaths, control);
        compiler->SetExtraPaths(extraPaths);
    }
} // DoSaveCompilerPrograms

void CompilerOptionsDlg::DoSaveVars()
{
    CompileOptionsBase* pBase = GetVarsOwner();
    if (pBase)
    {
        // let's process all the stored CustomVarActions
        for (unsigned int idxAction = 0; idxAction < m_CustomVarActions.size(); ++idxAction)
        {
            CustomVarAction Action = m_CustomVarActions[idxAction];
            switch(Action.m_Action)
            {
                case CVA_Add:
                    pBase->SetVar(Action.m_Key, Action.m_KeyValue);
                    break;
                case CVA_Edit:
                {
                    // first split up the KeyValue
                    wxString NewKey = Action.m_KeyValue.BeforeFirst(_T('=')).Trim(true).Trim(false);
                    wxString NewValue = Action.m_KeyValue.AfterFirst(_T('=')).Trim(true).Trim(false);
                    if (Action.m_Key != NewKey)
                    {   // the key name changed
                        pBase->UnsetVar(Action.m_Key);
                    }
                    pBase->SetVar(NewKey, NewValue);
                    break;
                }
                case CVA_Remove:
                    pBase->UnsetVar(Action.m_Key);
                    break;
                default:
                    break;
            } // end switch
        } // end for : idx : idxAction
        m_CustomVarActions.clear();
    }
} // DoSaveVars

void CompilerOptionsDlg::DoSaveCompilerDefinition()
{
    wxXmlNode* root = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("CodeBlocks_compiler_options"));
    Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
    const wxString name = wxT("name");
    const wxString value = wxT("value");
    wxXmlNode* node = new wxXmlNode(root, wxXML_ELEMENT_NODE, wxT("Program"));
    node->AddAttribute(name, wxT("C"));
    node->AddAttribute(value, compiler->GetPrograms().C);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Program")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("CPP"));
    node->AddAttribute(value, compiler->GetPrograms().CPP);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Program")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("LD"));
    node->AddAttribute(value, compiler->GetPrograms().LD);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Program")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("DBGconfig"));
    node->AddAttribute(value, compiler->GetPrograms().DBGconfig);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Program")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("LIB"));
    node->AddAttribute(value, compiler->GetPrograms().LIB);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Program")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("WINDRES"));
    node->AddAttribute(value, compiler->GetPrograms().WINDRES);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Program")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("MAKE"));
    node->AddAttribute(value, compiler->GetPrograms().MAKE);


    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("includeDirs"));
    node->AddAttribute(value, compiler->GetSwitches().includeDirs);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("libDirs"));
    node->AddAttribute(value, compiler->GetSwitches().libDirs);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("linkLibs"));
    node->AddAttribute(value, compiler->GetSwitches().linkLibs);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("defines"));
    node->AddAttribute(value, compiler->GetSwitches().defines);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("genericSwitch"));
    node->AddAttribute(value, compiler->GetSwitches().genericSwitch);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("objectExtension"));
    node->AddAttribute(value, compiler->GetSwitches().objectExtension);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("forceFwdSlashes"));
    node->AddAttribute(value, (compiler->GetSwitches().forceFwdSlashes ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("forceLinkerUseQuotes"));
    node->AddAttribute(value, (compiler->GetSwitches().forceLinkerUseQuotes ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("forceCompilerUseQuotes"));
    node->AddAttribute(value, (compiler->GetSwitches().forceCompilerUseQuotes ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("needDependencies"));
    node->AddAttribute(value, (compiler->GetSwitches().needDependencies ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("logging"));
    if (compiler->GetSwitches().logging == CompilerSwitches::defaultLogging)
        node->AddAttribute(value, wxT("default"));
    else if (compiler->GetSwitches().logging == clogFull)
        node->AddAttribute(value, wxT("full"));
    else if (compiler->GetSwitches().logging == clogSimple)
        node->AddAttribute(value, wxT("simple"));
    else if (compiler->GetSwitches().logging == clogNone)
        node->AddAttribute(value, wxT("none"));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("libPrefix"));
    node->AddAttribute(value, compiler->GetSwitches().libPrefix);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("libExtension"));
    node->AddAttribute(value, compiler->GetSwitches().libExtension);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("linkerNeedsLibPrefix"));
    node->AddAttribute(value, (compiler->GetSwitches().linkerNeedsLibPrefix ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("linkerNeedsLibExtension"));
    node->AddAttribute(value, (compiler->GetSwitches().linkerNeedsLibExtension ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("linkerNeedsPathResolved"));
    node->AddAttribute(value, (compiler->GetSwitches().linkerNeedsPathResolved ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("supportsPCH"));
    node->AddAttribute(value, (compiler->GetSwitches().supportsPCH ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("PCHExtension"));
    node->AddAttribute(value, compiler->GetSwitches().PCHExtension);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("UseFlatObjects"));
    node->AddAttribute(value, (compiler->GetSwitches().UseFlatObjects ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("UseFullSourcePaths"));
    node->AddAttribute(value, (compiler->GetSwitches().UseFullSourcePaths ? wxT("true") : wxT("false")));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("includeDirSeparator"));
    node->AddAttribute(value, compiler->GetSwitches().includeDirSeparator);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("libDirSeparator"));
    node->AddAttribute(value, compiler->GetSwitches().libDirSeparator);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("objectSeparator"));
    node->AddAttribute(value, compiler->GetSwitches().objectSeparator);
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("statusSuccess"));
    node->AddAttribute(value, wxString::Format(wxT("%d"), compiler->GetSwitches().statusSuccess));
    node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Switch")));
    node = node->GetNext();
    node->AddAttribute(name, wxT("Use83Paths"));
    node->AddAttribute(value, (compiler->GetSwitches().Use83Paths ? wxT("true") : wxT("false")));

    for (size_t i = 0; i < m_Options.GetCount(); ++i)
    {
        CompOption* opt = m_Options.GetOption(i);
        node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Option")));
        node = node->GetNext();
        wxString oName;
        opt->name.EndsWith(wxT("[") + opt->option + wxT("]"), &oName);
        if (oName == wxEmptyString)
            opt->name.EndsWith(wxT("[") + opt->additionalLibs + wxT("]"), &oName);
        if (oName == wxEmptyString)
            oName = opt->name;
        node->AddAttribute(name, oName.Trim());
        if (opt->option != wxEmptyString)
            node->AddAttribute(wxT("option"), opt->option);
        if (opt->category != wxT("General"))
            node->AddAttribute(wxT("category"), opt->category);
        if (opt->additionalLibs != wxEmptyString)
            node->AddAttribute(wxT("additionalLibs"), opt->additionalLibs);
        if (opt->checkAgainst != wxEmptyString)
        {
            node->AddAttribute(wxT("checkAgainst"), opt->checkAgainst);
            node->AddAttribute(wxT("checkMessage"), opt->checkMessage);
        }
        if (opt->supersedes != wxEmptyString)
            node->AddAttribute(wxT("supersedes"), opt->supersedes);
        if (opt->exclusive)
            node->AddAttribute(wxT("exclusive"), wxT("true"));
    }

    for (int i = 0; i < ctCount; ++i)
    {
        const CompilerToolsVector& vec = compiler->GetCommandToolsVector((CommandType)i);
        wxString op;
        if (i == ctCompileObjectCmd)
            op = wxT("CompileObject");
        else if (i == ctGenDependenciesCmd)
            op = wxT("GenDependencies");
        else if (i == ctCompileResourceCmd)
            op = wxT("CompileResource");
        else if (i == ctLinkExeCmd)
            op = wxT("LinkExe");
        else if (i == ctLinkConsoleExeCmd)
            op = wxT("LinkConsoleExe");
        else if (i == ctLinkDynamicCmd)
            op = wxT("LinkDynamic");
        else if (i == ctLinkStaticCmd)
            op = wxT("LinkStatic");
        else if (i == ctLinkNativeCmd)
            op = wxT("LinkNative");
        for (size_t j = 0; j < vec.size(); ++j)
        {
            node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Command")));
            node = node->GetNext();
            node->AddAttribute(name, op);
            node->AddAttribute(value, vec[j].command);
            if (!vec[j].extensions.IsEmpty())
                node->AddAttribute(wxT("ext"), GetStringFromArray(vec[j].extensions, DEFAULT_ARRAY_SEP, false));
            if (!vec[j].generatedFiles.IsEmpty())
                node->AddAttribute(wxT("gen"), GetStringFromArray(vec[j].generatedFiles, DEFAULT_ARRAY_SEP, false));
        }
    }

    const RegExArray& regexes = compiler->GetRegExArray();
    for (size_t i = 0; i < regexes.size(); ++i)
    {
        node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("RegEx")));
        node = node->GetNext();
        node->AddAttribute(name, regexes[i].desc);
        wxString tp;
        if (regexes[i].lt == cltNormal)
            tp = wxT("normal");
        else if (regexes[i].lt == cltWarning)
            tp = wxT("warning");
        else if (regexes[i].lt == cltError)
            tp = wxT("error");
        else if (regexes[i].lt == cltInfo)
            tp = wxT("info");
        node->AddAttribute(wxT("type"), tp);
        tp = wxString::Format(wxT("%d;%d;%d"), regexes[i].msg[0], regexes[i].msg[1], regexes[i].msg[2]);
        tp.Replace(wxT(";0"), wxEmptyString);
        node->AddAttribute(wxT("msg"), tp);
        if (regexes[i].filename != 0)
            node->AddAttribute(wxT("file"), wxString::Format(wxT("%d"), regexes[i].filename));
        if (regexes[i].line != 0)
            node->AddAttribute(wxT("line"), wxString::Format(wxT("%d"), regexes[i].line));
        tp = regexes[i].GetRegExString();
        tp.Replace(wxT("\t"), wxT("\\t"));
        node->AddChild(new wxXmlNode(wxXML_CDATA_SECTION_NODE, wxEmptyString, tp));
    }

    if (!compiler->GetCOnlyFlags().IsEmpty())
    {
        node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Sort")));
        node = node->GetNext();
        node->AddAttribute(wxT("CFlags"), compiler->GetCOnlyFlags());
    }
    if (!compiler->GetCPPOnlyFlags().IsEmpty())
    {
        node->SetNext(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Sort")));
        node = node->GetNext();
        node->AddAttribute(wxT("CPPFlags"), compiler->GetCPPOnlyFlags());
    }

    wxXmlDocument doc;
    doc.SetVersion("1.0");
    doc.SetRoot(root);

    const wxString folder(ConfigManager::GetFolder(sdDataUser)+"/compilers");
    if (!wxDirExists(folder))
        wxMkdir(folder);

    doc.Save(folder+"/options_"+compiler->GetID()+".xml");

    // update the in-memory cache
    compiler->SetOptions(m_Options);
} // DoSaveCompilerDefinition

// events

void CompilerOptionsDlg::OnDirty(cb_unused wxCommandEvent& event)
{
    m_bDirty = true;
} // OnDirty

void CompilerOptionsDlg::ProjectTargetCompilerAdjust()
{   // note this can also be called when on global compiler level, won't do anything (well reset a member which has
    // no use in this case)
    // check if the compilerID needs to be updated
    if (m_pTarget)
    { // target was the (tree) selection
        if (!m_NewProjectOrTargetCompilerId.IsEmpty() && m_pTarget->GetCompilerID() != m_NewProjectOrTargetCompilerId)
        {
            m_pTarget->SetCompilerID(m_NewProjectOrTargetCompilerId);
            cbMessageBox(_("You changed the compiler used for this target.\n"
                            "It is recommended that you fully rebuild this target, "
                            "otherwise linking errors might occur..."),
                            _("Notice"),
                            wxICON_EXCLAMATION);
        }
    }
    else if (m_pProject)
    {   // the project was the (tree) selection
        if (!m_NewProjectOrTargetCompilerId.IsEmpty() && m_pProject->GetCompilerID() != m_NewProjectOrTargetCompilerId)
        { // should be project then
            m_pProject->SetCompilerID(m_NewProjectOrTargetCompilerId);
            UpdateCompilerForTargets(m_CurrentCompilerIdx);
            cbMessageBox(_("You changed the compiler used for this project.\n"
                            "It is recommended that you fully rebuild this project, "
                            "otherwise linking errors might occur..."),
                            _("Notice"),
                            wxICON_EXCLAMATION);
        }
    }
    m_NewProjectOrTargetCompilerId = wxEmptyString;
} // ProjectTargetCompilerAdjust

void CompilerOptionsDlg::OnTreeSelectionChange(wxTreeEvent& event)
{
    if (m_BuildingTree)
        return;
    wxTreeCtrl* tc = XRCCTRL(*this, "tcScope", wxTreeCtrl);
    ScopeTreeData* data = (ScopeTreeData*)tc->GetItemData(event.GetItem());
    if (!data)
        return;

    wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
    int compilerIdx = data->GetTarget() ? CompilerFactory::GetCompilerIndex(data->GetTarget()->GetCompilerID()) :
                        (data->GetProject() ? CompilerFactory::GetCompilerIndex(data->GetProject()->GetCompilerID()) :
                        GetSelectionIndex(cmb));

    // in order to support projects/targets which have an unknown "user compiler", that is on the current
    // system that compiler is not (or no longer) installed, we should check the compilerIdx, in such a case it will
    // be '-1' [NOTE : maybe to the check already on the Id ?]
    // we then allow the user to make a choice :
    // a) adjust to another compiler
    // b) leave that compiler --> no settings can be set then (done by disabling the notebook,
    // as a consequence might need to be re-enabled when another target/project is chosen in the tree)
    if (compilerIdx != -1)
    {
        wxNotebook* nb = XRCCTRL(*this, "nbMain", wxNotebook);
        SetSelection(cmb, compilerIdx);
        // we don't update the compiler index yet, we leave that to CompilerChanged();
        m_pTarget = data->GetTarget();
        if (m_pTarget && !m_pTarget->SupportsCurrentPlatform())
        {
            if (nb)
                nb->Disable();
        }
        else
        {
            if (nb)
            {
                // enable/disable invalid pages for commands only target
                const bool cmd = (m_pTarget && m_pTarget->GetTargetType() == ttCommandsOnly);
                int pageOffset;
                if (!m_pProject->IsMakefileCustom())
                {
                    nb->GetPage(0)->Enable(!cmd); // Compiler settings
                    nb->GetPage(1)->Enable(!cmd); // Linker settings
                    nb->GetPage(2)->Enable(!cmd); // Search directories
                    pageOffset = 3;
                }
                else
                    pageOffset = 0;
                nb->GetPage(pageOffset + 2)->Enable(!cmd); // "Make" commands
                if (   cmd
                    && nb->GetSelection() != pageOffset   // Pre/post build steps
                    && nb->GetSelection() != pageOffset + 1 ) // Custom variables
                {
                    nb->SetSelection(pageOffset);
                }

                nb->Enable();
            }
            // the new selection might have a different compiler settings and/or even a different compiler
            // load all those new settings
            m_CurrentCompilerIdx = compilerIdx;
            Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
            if (compiler)
                m_Options = compiler->GetOptions();
            DoFillCompilerDependentSettings();
        }
    }
    else
    {
        m_pTarget = data->GetTarget();
        wxString CompilerId = m_pTarget?m_pTarget->GetCompilerID():data->GetProject()->GetCompilerID();
        wxString msg;
        msg.Printf(_("The defined compiler cannot be located (ID: %s).\n"
                    "Please choose the compiler you want to use instead and click \"OK\".\n"
                    "If you click \"Cancel\", the project/target will remain configured for that compiler and consequently can not be configured and will not be built."),
                    CompilerId.wx_str());
        Compiler* compiler = nullptr;
        if (m_pTarget && m_pTarget->SupportsCurrentPlatform())
            compiler = CompilerFactory::SelectCompilerUI(msg);

        if (compiler)
        {   // a new compiler was chosen, proceed as if the user manually selected another compiler
            // that means set the compiler selection list accordingly
            // and go directly to (On)CompilerChanged
            SetSelection(cmb, CompilerFactory::GetCompilerIndex(compiler));
            wxCommandEvent Dummy;
            OnCompilerChanged(Dummy);
        }
        else
        { // the user cancelled and wants to keep the compiler
            if (wxNotebook* nb = XRCCTRL(*this, "nbMain", wxNotebook))
                nb->Disable();
        }
    }

    {
        const bool show = (m_pTarget != nullptr);

        // Hide linker executable because it doesn't make sense to change it in the project.
        wxChoice *cmbLinkerExe = XRCCTRL(*this, "chLinkerExe", wxChoice);
        cmbLinkerExe->Show(show);
        wxStaticText *txtLinkerExe = XRCCTRL(*this, "txtLinkerExe", wxStaticText);
        txtLinkerExe->Show(show);
    }
} // OnTreeSelectionChange

void CompilerOptionsDlg::OnTreeSelectionChanging(wxTreeEvent& event)
{
    if (m_BuildingTree)
        return;
    wxTreeCtrl* tc = XRCCTRL(*this, "tcScope", wxTreeCtrl);
    ScopeTreeData* data = (ScopeTreeData*)tc->GetItemData(event.GetOldItem());
    if (data && (m_bDirty || m_bFlagsDirty))
    {   // data : should always be the case, since on global compiler level, there's no tree
        // when changes are made prompt the user if these changes should be applied
        // YES -> do the changes
        // NO -> no changes, just switch
        // CANCEL : don't switch

        AnnoyingDialog dlg(_("Project/Target change with changed settings"),
                    _("You have changed some settings. Do you want these settings saved ?\n\n"
                    "Yes    : will apply the changes\n"
                    "No     : will undo the changes\n"
                    "Cancel : will revert your selection in the project/target tree"),
                    wxART_QUESTION,
                    AnnoyingDialog::YES_NO_CANCEL);

        switch(dlg.ShowModal())
        {
            case AnnoyingDialog::rtYES :
                DoSaveCompilerDependentSettings();
                break;
            case AnnoyingDialog::rtCANCEL :
                event.Veto();
                break;
            case AnnoyingDialog::rtNO :
            default:
                {
                    m_bDirty = false;
                    m_bFlagsDirty = false;
                }
                break;
        } // end switch
    }
} // OnTreeSelectionChanging

void CompilerOptionsDlg::OnCompilerChanged(cb_unused wxCommandEvent& event)
{
    // when changes are made prompt the user if these changes should be applied
    // YES -> do the changes
    // NO -> no changes, just switch
    // CANCEL : don't switch
    bool bChanged = true;
    if (m_bDirty || m_bFlagsDirty)
    {
        switch(cbMessageBox(_("You have changed some settings. Do you want these settings saved ?\n\n"
                        "Yes    : will apply the changes\n"
                        "No     : will undo the changes\n"
                        "Cancel : will revert your compiler change."),
                        _("Compiler change with changed settings"),
                        wxICON_EXCLAMATION|wxYES|wxNO|wxCANCEL))
        {
            case wxID_CANCEL :
                SetSelection(XRCCTRL(*this, "cmbCompiler", wxChoice), m_CurrentCompilerIdx);
                bChanged = false;
                break;
            case wxID_YES :
                DoSaveCompilerDependentSettings();
                break;
            case wxID_NO :
            default:
                m_bDirty = false;
                m_bFlagsDirty = false;
                break;
        } // end switch
    }
    if (bChanged)
    {
        CompilerChanged();
        if (m_pProject)
        {   // in case of project/target --> dirty
            m_bDirty = true;
        }
    }
} // OnCompilerChanged

void CompilerOptionsDlg::CompilerChanged()
{
    m_CurrentCompilerIdx = GetSelectionIndex(XRCCTRL(*this, "cmbCompiler", wxChoice));
    // in case we are not on the global level (== project/target) we need to remember this switch
    // so that on "SAVE" time we can adjust the project/target with it's new compiler
    // SAVE time for this particular setting means (Apply or TreeSelection change
    // not compiler change since we could (re)change the compiler of that project/target
    if (m_pProject)
    {
        m_NewProjectOrTargetCompilerId = CompilerFactory::GetCompiler(m_CurrentCompilerIdx)->GetID();
    }

    //load the new options (== options of the new selected compiler)
    Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
    if (compiler)
        m_Options = compiler->GetOptions();

    DoFillCompilerDependentSettings();
} // CompilerChanged

void CompilerOptionsDlg::UpdateCompilerForTargets(int compilerIdx)
{
    int ret = cbMessageBox(_("You have changed the compiler used for the project.\n"
                            "Do you want to use the same compiler for all the project's build targets too?"),
                            _("Question"),
                            wxICON_QUESTION | wxYES_NO);
    if (ret == wxID_YES)
    {
        for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
        {
            ProjectBuildTarget* target = m_pProject->GetBuildTarget(i);
            Compiler* compiler = CompilerFactory::GetCompiler(compilerIdx);
            if (compiler)
                target->SetCompilerID(compiler->GetID());
        }
    }
} // UpdateCompilerForTargets

void CompilerOptionsDlg::AutoDetectCompiler()
{
    Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
    if (!compiler)
        return;

    wxString backup = XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->GetValue();
    wxArrayString ExtraPathsBackup = compiler->GetExtraPaths();

    wxArrayString empty;
    compiler->SetExtraPaths(empty);

    switch (compiler->AutoDetectInstallationDir())
    {
        case adrDetected:
        {
            wxString msg;
            msg.Printf(_("Auto-detected installation path of \"%s\"\nin \"%s\""), compiler->GetName().wx_str(), compiler->GetMasterPath().wx_str());
            cbMessageBox(msg);
        }
        break;

        case adrGuessed:
        {
            wxString msg;
            msg.Printf(_("Could not auto-detect installation path of \"%s\"...\n"
                        "Do you want to use this compiler's default installation directory?"),
                        compiler->GetName().wx_str());
            if (cbMessageBox(msg, _("Confirmation"), wxICON_QUESTION | wxYES_NO) == wxID_NO)
            {
                compiler->SetMasterPath(backup);
                compiler->SetExtraPaths(ExtraPathsBackup);
            }
        }
        break;

        default:
            break;
    }
    XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->SetValue(compiler->GetMasterPath());
    XRCCTRL(*this, "lstExtraPaths", wxListBox)->Clear();
    const wxArrayString& extraPaths = CompilerFactory::GetCompiler(m_CurrentCompilerIdx)->GetExtraPaths();
       ArrayString2ListBox(extraPaths, XRCCTRL(*this, "lstExtraPaths", wxListBox));
    m_bDirty = true;
} // AutoDetectCompiler

wxListBox* CompilerOptionsDlg::GetDirsListBox()
{
    wxNotebook* nb = XRCCTRL(*this, "nbDirs", wxNotebook);
    if (!nb)
        return 0;
    switch (nb->GetSelection())
    {
        case 0: // compiler dirs
            return XRCCTRL(*this, "lstIncludeDirs", wxListBox);
        case 1: // linker dirs
            return XRCCTRL(*this, "lstLibDirs", wxListBox);
        case 2: // resource compiler dirs
            return XRCCTRL(*this, "lstResDirs", wxListBox);
        default: break;
    }
    return 0;
} // GetDirsListBox

CompileOptionsBase* CompilerOptionsDlg::GetVarsOwner()
{
    return m_pTarget ? m_pTarget
                     : (m_pProject ? m_pProject
                                   : (CompileOptionsBase*)(CompilerFactory::GetCompiler(m_CurrentCompilerIdx)));
} // GetVarsOwner

void CompilerOptionsDlg::OnCategoryChanged(cb_unused wxCommandEvent& event)
{    // reshow the compiler options, but with different filter (category) applied
    DoFillOptions();
} // OnCategoryChanged

void CompilerOptionsDlg::OnOptionChanged(wxPropertyGridEvent& event)
{
    wxPGProperty* property = event.GetProperty();
    if (!property)
        return;
    // Make sure the property is bool. Other properties are ignored for now.
    if (!property->IsKindOf(CLASSINFO(wxBoolProperty)))
        return;
    CompOption* option = m_Options.GetOptionByName(property->GetLabel());
    wxVariant value = property->GetValue();
    if (value.IsNull() || !option)
        return;
    option->enabled = value.GetBool();
    if (option->enabled)
    {
        if (!option->checkAgainst.IsEmpty())
        {
            wxArrayString check = GetArrayFromString(option->checkAgainst, wxT(" "));
            for (size_t i = 0; i < check.Count(); ++i)
            {
                CompOption* against = m_Options.GetOptionByOption(check[i]);
                if (!against)
                    against = m_Options.GetOptionByAdditionalLibs(check[i]);
                if (against && against->enabled)
                {
                    const wxString message(option->checkMessage.empty() ?
                                               wxString::Format(_("\"%s\" conflicts with \"%s\"."), option->name, against->name) :
                                               option->checkMessage);

                    AnnoyingDialog dlg(_("Compiler options conflict"),
                                       message,
                                       wxART_INFORMATION,
                                       AnnoyingDialog::OK);
                    dlg.ShowModal();
                    break;
                }
            }
        }
        if (option->supersedes != wxEmptyString)
        {
            wxArrayString supersede = GetArrayFromString(option->supersedes, wxT(" "));
            for (size_t i = 0; i < supersede.Count(); ++i)
            {
                for (size_t j = 0; j < m_Options.GetCount(); ++j)
                {
                    if (option != m_Options.GetOption(j) &&
                        (supersede[i] == m_Options.GetOption(j)->option ||
                         supersede[i] == m_Options.GetOption(j)->additionalLibs))
                    {
                        m_Options.GetOption(j)->enabled = false;
                    }
                }

                for (wxPropertyGridIterator it = m_FlagsPG->GetIterator(); !it.AtEnd(); ++it)
                {
                    wxPGProperty* p = *it;
                    if (p->IsCategory() || p == property)
                        continue;
                    if (p->GetLabel().EndsWith(wxT("[") + supersede[i] + wxT("]")))
                        m_FlagsPG->SetPropertyValue(p, false);
                }
            }
        }
        if (option->exclusive)
        {
            for (size_t i = 0; i < m_Options.GetCount(); ++i)
            {
                if (option != m_Options.GetOption(i) &&
                    option->category == m_Options.GetOption(i)->category)
                {
                    m_Options.GetOption(i)->enabled = false;
                }
            }
            for (wxPropertyGridIterator it = m_FlagsPG->GetIterator(); !it.AtEnd(); ++it)
            {
                wxPGProperty* p = *it;
                if (p->IsCategory() || p == property)
                    continue;
                CompOption* opt = m_Options.GetOptionByName(p->GetLabel());
                if (option != opt && option->category == opt->category)
                    m_FlagsPG->SetPropertyValue(p, false);
            }
        }
    }
    m_bDirty = true;
}

// some handlers for adding/editing/removing/clearing of include/libraries/resources directories
void CompilerOptionsDlg::OnAddDirClick(cb_unused wxCommandEvent& event)
{
    EditPathDlg dlg(this,
            m_pProject ? m_pProject->GetBasePath() : _T(""),
            m_pProject ? m_pProject->GetBasePath() : _T(""),
            _("Add directory"));

    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString path = dlg.GetPath();

        wxListBox* control = GetDirsListBox();
        if (control)
        {
            control->Append(path);
            m_bDirty = true;
        }
    }
} // OnAddDirClick

void CompilerOptionsDlg::OnEditDirClick(cb_unused wxCommandEvent& event)
{
    wxListBox* control = GetDirsListBox();
    wxArrayInt selections;
    if (!control || control->GetSelections(selections) < 1)
        return;

    if (selections.GetCount()>1)
    {
        cbMessageBox(_("Please select only one directory you would like to edit."),
                    _("Error"), wxICON_ERROR);
        return;
    }

    EditPathDlg dlg(this,
                    control->GetString(selections[0]),
                    m_pProject ? m_pProject->GetBasePath() : _T(""),
                    _("Edit directory"));

    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString path = dlg.GetPath();
        control->SetString(selections[0], path);
        m_bDirty = true;
    }
} // OnEditDirClick

void CompilerOptionsDlg::OnRemoveDirClick(cb_unused wxCommandEvent& event)
{
    wxListBox* control = GetDirsListBox();
    wxArrayInt selections;
    if (!control || control->GetSelections(selections) < 1)
        return;

    if (cbMessageBox(_("Remove selected folders from the list?"),
                     _("Confirmation"),
                     wxOK | wxCANCEL | wxICON_QUESTION) == wxID_OK)
    {
        std::sort(selections.begin(), selections.end());
        for (unsigned int i=selections.GetCount(); i>0; --i)
            control->Delete(selections[i-1]);
        m_bDirty = true;
    }
} // OnRemoveDirClick

void CompilerOptionsDlg::OnClearDirClick(cb_unused wxCommandEvent& event)
{
    wxListBox* control = GetDirsListBox();
    if (!control || control->GetCount() == 0)
        return;

    if (cbMessageBox(_("Remove all directories from the list?"),
                     _("Confirmation"),
                     wxOK | wxCANCEL | wxICON_QUESTION) == wxID_OK)
    {
        control->Clear();
        m_bDirty = true;
    }
} // OnClearDirClick

void CompilerOptionsDlg::OnCopyDirsClick(cb_unused wxCommandEvent& event)
{
    if (!m_pProject)
        return;

    wxListBox* control = GetDirsListBox();
    wxArrayInt selections;
    if (!control || control->GetSelections(selections) < 1)
        return;

    wxArrayString choices;
    choices.Add(m_pProject->GetTitle());
    for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
    {
        ProjectBuildTarget* bt = m_pProject->GetBuildTarget(i);
        choices.Add(bt->GetTitle());
    }

    const wxArrayInt &sel = cbGetMultiChoiceDialog(_("Please select which target to copy these directories to:"),
                                                   _("Copy directories"), choices, this);
    if (sel.empty())
        return;

    wxNotebook* nb = XRCCTRL(*this, "nbDirs", wxNotebook);
    int notebookPage = nb->GetSelection();

    for (wxArrayInt::const_iterator itr = sel.begin(); itr != sel.end(); ++itr)
    {
        CompileOptionsBase* base;
        if((*itr) == 0)
            base = m_pProject; // "copy to project"
        else
            base = m_pProject->GetBuildTarget((*itr) - 1);

        if (!base)
        {
            Manager::Get()->GetLogManager()->LogWarning(_("Could not get build target in CompilerOptionsDlg::OnCopyLibsClick"));
            continue;
        }

        for (size_t i = 0; i < selections.GetCount(); ++i)
        {
            switch (notebookPage)
            {
                case 0: // compiler dirs
                    base->AddIncludeDir(control->GetString(selections[i]));
                    break;
                case 1: // linker dirs
                    base->AddLibDir(control->GetString(selections[i]));
                    break;
                case 2: // resource compiler dirs
                    base->AddResourceIncludeDir(control->GetString(selections[i]));
                    break;
                default:
                    break;
            }
        }
    }
} // OnCopyDirsClick

static void QuoteString(wxString &value, const wxString &caption)
{
    if (NeedQuotes(value))
    {
        AnnoyingDialog dlgQuestion(caption, _("Variable quote string"),
                                   _("The value contains spaces or strange characters. Do you want to quote it?"),
                                   wxART_QUESTION, AnnoyingDialog::YES_NO, AnnoyingDialog::rtSAVE_CHOICE,
                                   _("&Quote"), _("&Leave unquoted"));
        if (dlgQuestion.ShowModal() == AnnoyingDialog::rtYES)
            ::QuoteStringIfNeeded(value);
    }
}

void CompilerOptionsDlg::OnAddVarClick(cb_unused wxCommandEvent& event)
{
    wxString key;
    wxString value;
    EditPairDlg dlg(this, key, value, _("Add new variable"), EditPairDlg::bmBrowseForDirectory);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        key.Trim(true).Trim(false);
        value.Trim(true).Trim(false);
        QuoteString(value, _("Add variable quote string"));
        CustomVarAction Action = {CVA_Add, key, value};
        m_CustomVarActions.push_back(Action);
        XRCCTRL(*this, "lstVars", wxListBox)->Append(key + _T(" = ") + value, new VariableListClientData(key, value));
        m_bDirty = true;
    }
} // OnAddVarClick

void CompilerOptionsDlg::OnEditVarClick(cb_unused wxCommandEvent& event)
{
    wxListBox *list = XRCCTRL(*this, "lstVars", wxListBox);
    int sel = list->GetSelection();
    if (sel == -1)
        return;

    VariableListClientData *data = static_cast<VariableListClientData*>(list->GetClientObject(sel));
    wxString key = data->key;
    wxString value = data->value;

    EditPairDlg dlg(this, key, value, _("Edit variable"), EditPairDlg::bmBrowseForDirectory);
    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        key.Trim(true).Trim(false);
        value.Trim(true).Trim(false);
        QuoteString(value, _("Edit variable quote string"));

        if (value != data->value  ||  key != data->key)
        { // something has changed
            CustomVarAction Action = {CVA_Edit, data->key, key + _T(" = ") + value};
            m_CustomVarActions.push_back(Action);
            list->SetString(sel, key + _T(" = ") + value);
            data->key = key;
            data->value = value;
            m_bDirty = true;
        }
    }
} // OnEditVarClick

void CompilerOptionsDlg::OnRemoveVarClick(cb_unused wxCommandEvent& event)
{
    wxListBox *list = XRCCTRL(*this, "lstVars", wxListBox);
    int sel = list->GetSelection();
    if (sel == -1)
        return;
    const wxString &key = static_cast<VariableListClientData*>(list->GetClientObject(sel))->key;
    if (key.IsEmpty())
        return;

    if (cbMessageBox(_("Are you sure you want to delete this variable?"),
                    _("Confirmation"),
                    wxYES_NO | wxICON_QUESTION) == wxID_YES)
    {
        CustomVarAction Action = {CVA_Remove, key, wxEmptyString};
        m_CustomVarActions.push_back(Action);
        list->Delete(sel);
        m_bDirty = true;
    }
} // OnRemoveVarClick

void CompilerOptionsDlg::OnClearVarClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstVars = XRCCTRL(*this, "lstVars", wxListBox);
    if (lstVars->IsEmpty())
        return;

    if (cbMessageBox(_("Are you sure you want to clear all variables?"),
                        _("Confirmation"),
                        wxYES | wxNO | wxICON_QUESTION) == wxID_YES)
    {
        // Unset all variables of lstVars
        for (size_t i=0; i < lstVars->GetCount(); ++i)
        {
            const wxString &key = static_cast<VariableListClientData*>(lstVars->GetClientObject(i))->key;
            if (!key.IsEmpty())
            {
                CustomVarAction Action = {CVA_Remove, key, wxEmptyString};
                m_CustomVarActions.push_back(Action);
            }
        }
        lstVars->Clear();
        m_bDirty = true;
    }
} // OnClearVarClick

void CompilerOptionsDlg::OnSetDefaultCompilerClick(cb_unused wxCommandEvent& event)
{
    const int idx = GetSelectionIndex(XRCCTRL(*this, "cmbCompiler", wxChoice));
    CompilerFactory::SetDefaultCompiler(idx);

    wxString msg;
    Compiler* compiler = CompilerFactory::GetDefaultCompiler();
    msg.Printf(_("%s is now selected as the default compiler for new projects"), compiler ? compiler->GetName() : _("[invalid]"));
    cbMessageBox(msg);

    DoFillCompilerSets(idx);
} // OnSetDefaultCompilerClick

void CompilerOptionsDlg::OnAddCompilerClick(cb_unused wxCommandEvent& event)
{
    if (m_bDirty)
    {   // changes had been made to the current selected compiler
        switch(cbMessageBox(_("You have changed some settings. Do you want these settings saved ?\n\n"
                        "Yes    : will apply the changes\n"
                        "No     : will undo the changes\n"
                        "Cancel : will cancel your compiler addition."),
                        _("Compiler change with changed settings"),
                        wxICON_EXCLAMATION|wxYES|wxNO|wxCANCEL))
        {
            case wxID_CANCEL :
                return;
                break;
            case wxID_YES :
                DoSaveCompilerDependentSettings();
                break;
            case wxID_NO :
            default:
                // we don't clear the dirty flag yet (in case something goes wrong with the compiler copy we need to reload the
                // 'selected compiler' options omitting the current 'No'-ed changes
                break;
        } // end switch
    }

    wxString value = cbGetTextFromUser(_("Please enter the new compiler's name:"),
                                       _("Add new compiler"),
                                       wxString::Format(_("Copy of %s"), CompilerFactory::GetCompiler(m_CurrentCompilerIdx)->GetName()),
                                       this);
    if (!value.empty())
    {
        // make a copy of current compiler
        Compiler* newC = nullptr;
        try
        {
            newC = CompilerFactory::CreateCompilerCopy(CompilerFactory::GetCompiler(m_CurrentCompilerIdx), value);
        }
        catch (cbException& e)
        {
            // usually throws because of non-unique ID
            e.ShowErrorMessage(false);
            newC = nullptr; // just to be sure
        }

        if (!newC)
        {
            cbMessageBox(_("The new compiler could not be created.\n(maybe a compiler with the same name already exists?)"),
                        _("Error"), wxICON_ERROR);
            return;
        }
        else
        {
            m_CurrentCompilerIdx = CompilerFactory::GetCompilerIndex(newC);
            wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
            cmb->Append(value, new IntClientData(m_CurrentCompilerIdx));
            SetSelection(cmb, m_CurrentCompilerIdx);
            // refresh settings in dialog
            DoFillCompilerDependentSettings();
            cbMessageBox(_("The new compiler has been added! Don't forget to update the \"Toolchain executables\" page..."));
        }
    }

    if (m_bDirty)
    {   // something went wrong -> reload current settings omitting the NO-ed changes
        m_bDirty = false;
        CompilerChanged();
    }
} // OnAddCompilerClick

void CompilerOptionsDlg::OnEditCompilerClick(cb_unused wxCommandEvent& event)
{
    Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
    if (compiler)
    {
        const wxString curValue(compiler->GetName());
        wxString newValue(cbGetTextFromUser(_("Please edit the compiler's name:"), _("Rename compiler"), curValue, this));
        if (!newValue.empty() && (newValue != curValue))
        {
            compiler->SetName(newValue);
            if (compiler == CompilerFactory::GetDefaultCompiler())
                newValue << ' ' << _("(default)");

            // Delete and reappend to keep order
            wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
            cmb->Delete(GetIndexPosition(cmb, m_CurrentCompilerIdx));
            const int pos = cmb->Append(newValue, new IntClientData(m_CurrentCompilerIdx));
            cmb->SetSelection(pos);
        }
    }
} // OnEditCompilerClick

void CompilerOptionsDlg::OnRemoveCompilerClick(cb_unused wxCommandEvent& event)
{
    if (cbMessageBox(_("Are you sure you want to remove this compiler?"),
                    _("Confirmation"),
                    wxYES | wxNO| wxICON_QUESTION | wxNO_DEFAULT) == wxID_YES)
    {
        wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
        // Remove compiler from factory
        CompilerFactory::RemoveCompiler(CompilerFactory::GetCompiler(m_CurrentCompilerIdx));
        // Remove compiler from choice
        const int pos = GetIndexPosition(cmb, m_CurrentCompilerIdx);
        cmb->Delete(pos);
        // Adjust choice indexes > m_CurrentCompilerIdx
        const int count = (int)(cmb->GetCount());
        for (int n = 0; n < count; ++n)
        {
            IntClientData* data = dynamic_cast <IntClientData *> (cmb->GetClientObject(n));
            if (data)
            {
                const int idx = data->GetData();
                if (idx > m_CurrentCompilerIdx)
                    data->SetData(idx-1);
            }
        }

        // Select next compiler in the choice or last if the deleted one was the last
        cmb->SetSelection((pos < count) ? pos : (pos-1));
        // Update current compiler index
        m_CurrentCompilerIdx = GetSelectionIndex(cmb);
        DoFillCompilerDependentSettings();
    }
} // OnRemoveCompilerClick

void CompilerOptionsDlg::OnResetCompilerClick(cb_unused wxCommandEvent& event)
{
    if (cbMessageBox(_("Reset this compiler's settings to the defaults?"),
                    _("Confirmation"),
                    wxYES | wxNO| wxICON_QUESTION | wxNO_DEFAULT) == wxID_YES)
    if (cbMessageBox(_("Reset this compiler's settings to the defaults?\n"
                       "\nAre you REALLY sure?"),
                    _("Confirmation"),
                    wxYES | wxNO| wxICON_QUESTION | wxNO_DEFAULT) == wxID_YES)
    {
        Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
        if (compiler)
        {
            const wxString file = wxT("/compilers/options_") + compiler->GetID() + wxT(".xml");
            if (   wxFileExists(ConfigManager::GetDataFolder(true) + file)
                && wxFileExists(ConfigManager::GetDataFolder(false) + file) )
            {
                wxRemoveFile(ConfigManager::GetDataFolder(false) + file);
            }
            compiler->Reset();
        }
        // run auto-detection
        AutoDetectCompiler();
        CompilerFactory::SaveSettings();
        // refresh settings in dialog
        DoFillCompilerDependentSettings();
    }
} // OnResetCompilerClick

// 4 handlers for the adding/editing/removing/clearing of Linker Libs
void CompilerOptionsDlg::OnAddLibClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);

    EditPathDlg dlg(this,
            _T(""),
            m_pProject ? m_pProject->GetBasePath() : _T(""),
            _("Add library"),
            _("Choose library to link"),
            false,
            true,
            _("Library files (*.a, *.so, *.lib, *.dylib, *.bundle)|*.a;*.so;*.lib;*.dylib;*.bundle|All files (*)|*"));

    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxArrayString paths = GetArrayFromString(dlg.GetPath());
        for (size_t i = 0; i < paths.GetCount(); ++i)
            lstLibs->Append(paths[i]);
        m_bDirty = true;
    }
} // OnAddLibClick

void CompilerOptionsDlg::OnEditLibClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (!lstLibs)
        return;

    wxArrayInt sels;
    int num = lstLibs->GetSelections(sels);
    if      (num<1)
    {
      cbMessageBox(_("Please select a library you wish to edit."),
                   _("Error"), wxICON_ERROR);
    }
    else if (num == 1)
    {
      EditPathDlg dlg(this,
              lstLibs->GetString(sels[0]),
              m_pProject ? m_pProject->GetBasePath() : _T(""),
              _("Edit library"),
              _("Choose library to link"),
              false,
              false,
              _("Library files (*.a, *.so, *.lib, *.dylib, *.bundle)|*.a;*.so;*.lib;*.dylib;*.bundle|All files (*)|*"));

      PlaceWindow(&dlg);
      if (dlg.ShowModal() == wxID_OK)
      {
          lstLibs->SetString(sels[0], dlg.GetPath());
          m_bDirty = true;
      }
    }
    else
    {
      cbMessageBox(_("Please select only *one* library you wish to edit."),
                   _("Error"), wxICON_ERROR);
    }
} // OnEditLibClick

void CompilerOptionsDlg::OnRemoveLibClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (!lstLibs)
        return;

    wxArrayInt sels;
    int num = lstLibs->GetSelections(sels);
    if (num == 1) // mimic old behaviour
    {
        if (cbMessageBox(wxString::Format(_("Remove library '%s' from the list?"), lstLibs->GetString(sels[0])),
                         _("Confirmation"), wxICON_QUESTION | wxOK | wxCANCEL) == wxID_OK)
        {
            lstLibs->Delete(sels[0]);
            m_bDirty = true;
        }
    }
    else if (num > 1)
    {
        wxString msg;

        msg.Printf(_("Remove all (%d) selected libraries from the list?"), num);
        if (cbMessageBox(msg, _("Confirmation"), wxICON_QUESTION | wxOK | wxCANCEL) == wxID_OK)
        {
            // remove starting with the last lib. otherwise indices will change
            for (size_t i = sels.GetCount(); i>0; --i)
                lstLibs->Delete(sels[i-1]);
            m_bDirty = true;
        }
    }
    // else: No lib selected
} // OnRemoveLibClick

void CompilerOptionsDlg::OnClearLibClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (!lstLibs || lstLibs->GetCount() == 0)
        return;
    if (cbMessageBox(_("Remove all libraries from the list?"), _("Confirmation"), wxICON_QUESTION | wxOK | wxCANCEL) == wxID_OK)
    {
        lstLibs->Clear();
        m_bDirty = true;
    }
} // OnClearLibClick

void CompilerOptionsDlg::OnCopyLibsClick(cb_unused wxCommandEvent& event)
{
    if (!m_pProject)
        return;
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (!lstLibs || lstLibs->GetCount() == 0)
        return;

    wxArrayString choices;
    choices.Add(m_pProject->GetTitle());
    for (int i = 0; i < m_pProject->GetBuildTargetsCount(); ++i)
    {
        ProjectBuildTarget* bt = m_pProject->GetBuildTarget(i);
        choices.Add(bt->GetTitle());
    }

    const wxArrayInt &sel = cbGetMultiChoiceDialog(_("Please select which target to copy these libraries to:"),
                                                   _("Copy libraries"), choices, this);
    if (sel.empty())
        return;

    for (wxArrayInt::const_iterator itr = sel.begin(); itr != sel.end(); ++itr)
    {
        CompileOptionsBase* base;
        if((*itr) == 0)
            base = m_pProject; // "copy to project"
        else
            base = m_pProject->GetBuildTarget((*itr) - 1);

        if (!base)
        {
            Manager::Get()->GetLogManager()->LogWarning(_T("Could not get build target in CompilerOptionsDlg::OnCopyLibsClick"));
            continue;
        }

        for (size_t i = 0; i < lstLibs->GetCount(); ++i)
        {
            if (lstLibs->IsSelected(i))
                base->AddLinkLib(lstLibs->GetString(i));
        }
    }
} // OnCopyLibsClick

void CompilerOptionsDlg::OnAddExtraPathClick(cb_unused wxCommandEvent& event)
{
    EditPathDlg dlg(this, _T(""), _T(""), _("Add directory"));

    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxListBox* control = XRCCTRL(*this, "lstExtraPaths", wxListBox);
        if (control)
        {
            wxString path = dlg.GetPath();

            // get all listBox entries in array String
            wxArrayString extraPaths;
            ListBox2ArrayString(extraPaths, control);
            if (extraPaths.Index(path) != wxNOT_FOUND)
            {
                cbMessageBox(_("Path already in extra paths list!"), _("Warning"), wxICON_WARNING);
            }
            else
            {
                control->Append(path);
                m_bDirty = true;
            }
        }
    }
} // OnAddExtraPathClick

void CompilerOptionsDlg::OnEditExtraPathClick(cb_unused wxCommandEvent& event)
{
    wxListBox* control = XRCCTRL(*this, "lstExtraPaths", wxListBox);
    if (!control || control->GetSelection() < 0)
        return;

    wxFileName dir(control->GetString(control->GetSelection()) + wxFileName::GetPathSeparator());
    wxString initial = control->GetString(control->GetSelection()); // might be a macro
    if (dir.DirExists())
        initial = dir.GetPath(wxPATH_GET_VOLUME);

    EditPathDlg dlg(this, initial, _T(""), _("Edit directory"));

    PlaceWindow(&dlg);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxString path = dlg.GetPath();

        // get all listBox entries in array String
        wxArrayString extraPaths;
        ListBox2ArrayString(extraPaths, control);
        if (extraPaths.Index(path) != wxNOT_FOUND)
        {
            cbMessageBox(_("Path already in extra paths list!"), _("Warning"), wxICON_WARNING);
        }
        else
        {
            control->SetString(control->GetSelection(), path);
            m_bDirty = true;
        }
    }
} // OnEditExtraPathClick

void CompilerOptionsDlg::OnRemoveExtraPathClick(cb_unused wxCommandEvent& event)
{
    wxListBox* control = XRCCTRL(*this, "lstExtraPaths", wxListBox);
    if (!control || control->GetSelection() < 0)
        return;
    control->Delete(control->GetSelection());
    m_bDirty = true;
} // OnRemoveExtraPathClick

void CompilerOptionsDlg::OnClearExtraPathClick(cb_unused wxCommandEvent& event)
{
    wxListBox* control = XRCCTRL(*this, "lstExtraPaths", wxListBox);
    if (!control || control->IsEmpty())
        return;

    if (cbMessageBox(_("Remove all extra paths from the list?"), _("Confirmation"), wxICON_QUESTION | wxOK | wxCANCEL) == wxID_OK)
    {
        control->Clear();
        m_bDirty = true;
    }
} // OnClearExtraPathClick

void CompilerOptionsDlg::OnIgnoreAddClick(cb_unused wxCommandEvent& event)
{
    wxListBox*  list = XRCCTRL(*this, "lstIgnore", wxListBox);
    wxTextCtrl* text = XRCCTRL(*this, "txtIgnore", wxTextCtrl);

    wxString ignore_str = text->GetValue().Trim();
    if (   (ignore_str.Len()>0)
        && (list->FindString(ignore_str)==wxNOT_FOUND) )
    {
        list->Append(ignore_str);
        m_bDirty = true;
    }
} // OnIgnoreAddClick

void CompilerOptionsDlg::OnIgnoreRemoveClick(cb_unused wxCommandEvent& event)
{
    wxListBox* list = XRCCTRL(*this, "lstIgnore", wxListBox);
    if (!list || list->IsEmpty())
        return;

    int selection = list->GetSelection();
    if (selection!=wxNOT_FOUND)
    {
        list->Delete(selection);
        m_bDirty = true;
    }
} // OnIgnoreRemoveClick

void CompilerOptionsDlg::SwapItems(wxListBox* listBox, int a, int b)
{
    const wxString tmp(listBox->GetString(a));
    listBox->SetString(a, listBox->GetString(b));
    listBox->SetString(b, tmp);
}

void CompilerOptionsDlg::Reselect(wxListBox* listBox, const wxArrayInt& selected, int offset)
{
    const unsigned int len = listBox->GetCount();
    for (unsigned int i = 0; i < len; ++i)
    {
        if (selected.Index(i+offset) != wxNOT_FOUND)
            listBox->SetSelection(i);
        else
            listBox->Deselect(i);
    }
}

void CompilerOptionsDlg::OnMoveLibUpClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (!lstLibs)
        return;

    wxArrayInt sels;
    const int num = lstLibs->GetSelections(sels);
    if ((num == 0) || (sels[0] == 0))
        return;

    lstLibs->Freeze();

    // Move
    for (int i = 0; i < num; ++i)
    {
        const int n = sels[i];
        SwapItems(lstLibs, n, n-1);
    }

    // Reselect
    Reselect(lstLibs, sels, 1);

    lstLibs->Thaw();
    m_bDirty = true;
} // OnMoveLibUpClick

void CompilerOptionsDlg::OnMoveLibDownClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (!lstLibs)
        return;

    wxArrayInt sels;
    const int num = lstLibs->GetSelections(sels);
    if ((num == 0) || (sels.Last() == int(lstLibs->GetCount())-1))
        return;

    lstLibs->Freeze();

    // Move
    for (int i = num-1; i >= 0; --i)
    {
        const int n = sels[i];
        SwapItems(lstLibs, n, n+1);
    }

    // Reselect
    Reselect(lstLibs, sels, -1);

    lstLibs->Thaw();
    m_bDirty = true;
} // OnMoveLibDownClick

void CompilerOptionsDlg::OnMoveDirUpClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstDirs = GetDirsListBox();
    if (!lstDirs)
        return;

    wxArrayInt sels;
    const int num = lstDirs->GetSelections(sels);
    if ((num == 0) || (sels[0] == 0))
        return;

    lstDirs->Freeze();

    // Move
    for (int i = 0; i < num; ++i)
    {
        const int n = sels[i];
        SwapItems(lstDirs, n, n-1);
    }

    // Reselect
    Reselect(lstDirs, sels, 1);

    lstDirs->Thaw();
    m_bDirty = true;
} // OnMoveDirUpClick

void CompilerOptionsDlg::OnMoveDirDownClick(cb_unused wxCommandEvent& event)
{
    wxListBox* lstDirs = GetDirsListBox();
    if (!lstDirs)
        return;

    wxArrayInt sels;
    const int num = lstDirs->GetSelections(sels);
    if ((num == 0) || (sels.Last() == int(lstDirs->GetCount())-1))
        return;

    lstDirs->Freeze();

    // Move
    for (int i = num-1; i >= 0; --i)
    {
        const int n = sels[i];
        SwapItems(lstDirs, n, n+1);
    }

    // Reselect
    Reselect(lstDirs, sels, -1);

    lstDirs->Thaw();
    m_bDirty = true;
} // OnMoveDirDownClick

void CompilerOptionsDlg::OnMasterPathClick(cb_unused wxCommandEvent& event)
{
    wxString path = ChooseDirectory(this,
                                    _("Select directory"),
                                    XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->GetValue());
    if (!path.IsEmpty())
    {
        XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->SetValue(path);
        m_bDirty = true;
    }
} // OnMasterPathClick

void CompilerOptionsDlg::OnAutoDetectClick(cb_unused wxCommandEvent& event)
{
    AutoDetectCompiler();
} // OnAutoDetectClick

void CompilerOptionsDlg::OnSelectProgramClick(wxCommandEvent& event)
{
    // see who called us
    wxTextCtrl* obj = nullptr;
    if (event.GetId() == XRCID("btnCcompiler"))
        obj = XRCCTRL(*this, "txtCcompiler", wxTextCtrl);
    else if (event.GetId() == XRCID("btnCPPcompiler"))
        obj = XRCCTRL(*this, "txtCPPcompiler", wxTextCtrl);
    else if (event.GetId() == XRCID("btnLinker"))
        obj = XRCCTRL(*this, "txtLinker", wxTextCtrl);
    else if (event.GetId() == XRCID("btnLibLinker"))
        obj = XRCCTRL(*this, "txtLibLinker", wxTextCtrl);
    else if (event.GetId() == XRCID("btnResComp"))
        obj = XRCCTRL(*this, "txtResComp", wxTextCtrl);
    else if (event.GetId() == XRCID("btnMake"))
        obj = XRCCTRL(*this, "txtMake", wxTextCtrl);

    if (!obj)
        return; // called from invalid caller

    // common part follows
    wxString file_selection = _("All files (*)|*");
    if (platform::windows)
        file_selection = _("Executable files (*.exe)|*.exe");
    wxFileDialog dlg(this,
                     _("Select file"),
                     XRCCTRL(*this, "txtMasterPath", wxTextCtrl)->GetValue() + _T("/bin"),
                     obj->GetValue(),
                     file_selection,
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST | compatibility::wxHideReadonly );
    dlg.SetFilterIndex(0);

    PlaceWindow(&dlg);
    if (dlg.ShowModal() != wxID_OK)
        return;
    wxFileName fname(dlg.GetPath());
    obj->SetValue(fname.GetFullName());
    m_bDirty = true;
} // OnSelectProgramClick

void CompilerOptionsDlg::OnAdvancedClick(cb_unused wxCommandEvent& event)
{
    AnnoyingDialog dlg(_("Edit advanced compiler settings?"),
                        _("The compiler's advanced settings, need command-line "
                        "compiler knowledge to be tweaked.\nIf you don't know "
                        "*exactly* what you 're doing, it is suggested to "
                        "NOT tamper with these...\n\n"
                        "Are you sure you want to proceed?"),
                    wxART_QUESTION);
    if (dlg.ShowModal() == AnnoyingDialog::rtYES)
    {
        const int compilerIdx = GetSelectionIndex(XRCCTRL(*this, "cmbCompiler", wxChoice));
        AdvancedCompilerOptionsDlg dlg2(this, CompilerFactory::GetCompiler(compilerIdx)->GetID());
        PlaceWindow(&dlg2);
        dlg2.ShowModal();
        // check if dirty
        if (dlg2.IsDirty())
        {
//            m_bDirty = true;  // TO DO : Activate when implemented in the adv dialog
        }
    }
} // OnAdvancedClick

static void UpdateUIListBoxAndButtons(wxListBox &list, bool hasProject, wxButton &edit, wxButton &del, wxButton &clear, wxButton &copy,
                                      wxButton &up, wxButton &down)
{
    wxArrayInt selections;
    int num = list.GetSelections(selections);
    int itemCount = list.GetCount();
    bool en = (num > 0);

    edit.Enable(num == 1);
    del.Enable(en);
    clear.Enable(itemCount != 0);
    copy.Enable(en && hasProject);

    if (en)
    {
        int minIndex = selections.size();
        int maxIndex = 0;
        for (int index : selections)
        {
            minIndex = std::min(index, minIndex);
            maxIndex = std::max(index, maxIndex);
        }
        up.Enable(minIndex > 0);
        down.Enable(maxIndex < itemCount - 1);
    }
    else
    {
        up.Enable(false);
        down.Enable(false);
    }
}

void CompilerOptionsDlg::OnUpdateUI(cb_unused wxUpdateUIEvent& event)
{
    bool en = false;
    const bool hasProject = m_pProject;

    wxListBox* control = GetDirsListBox();
    if (control)
    {
        UpdateUIListBoxAndButtons(*control, hasProject, *XRCCTRL(*this, "btnEditDir",  wxButton),
                                  *XRCCTRL(*this, "btnDelDir",   wxButton), *XRCCTRL(*this, "btnClearDir", wxButton),
                                  *XRCCTRL(*this, "btnCopyDirs", wxButton), *XRCCTRL(*this, "btnMoveDirUp", wxButton),
                                  *XRCCTRL(*this, "btnMoveDirDown", wxButton));
    }

    // edit/delete/clear/copy/moveup/movedown lib dirs
    wxListBox* lstLibs = XRCCTRL(*this, "lstLibs", wxListBox);
    if (lstLibs)
    {
        UpdateUIListBoxAndButtons(*lstLibs, hasProject, *XRCCTRL(*this, "btnEditLib",  wxButton),
                                  *XRCCTRL(*this, "btnDelLib",   wxButton), *XRCCTRL(*this, "btnClearLib", wxButton),
                                  *XRCCTRL(*this, "btnCopyLibs", wxButton), *XRCCTRL(*this, "btnMoveLibUp", wxButton),
                                  *XRCCTRL(*this, "btnMoveLibDown", wxButton));
    }

    // edit/delete/clear/copy/moveup/movedown extra path
    if (!hasProject)
    {
        en = XRCCTRL(*this, "lstExtraPaths", wxListBox)->GetSelection() >= 0;
        XRCCTRL(*this, "btnExtraEdit",   wxButton)->Enable(en);
        XRCCTRL(*this, "btnExtraDelete", wxButton)->Enable(en);
        XRCCTRL(*this, "btnExtraClear",  wxButton)->Enable(XRCCTRL(*this, "lstExtraPaths", wxListBox)->GetCount() != 0);
    }

    // add/edit/delete/clear vars
    en = XRCCTRL(*this, "lstVars", wxListBox)->GetSelection() >= 0;
    XRCCTRL(*this, "btnEditVar",   wxButton)->Enable(en);
    XRCCTRL(*this, "btnDeleteVar", wxButton)->Enable(en);
    XRCCTRL(*this, "btnClearVar",  wxButton)->Enable(XRCCTRL(*this, "lstVars", wxListBox)->GetCount() != 0);

    // policies
    wxTreeCtrl* tc = XRCCTRL(*this, "tcScope", wxTreeCtrl);
    ScopeTreeData* data = (ScopeTreeData*)tc->GetItemData(tc->GetSelection());
    en = (data && data->GetTarget());
    XRCCTRL(*this, "cmbCompilerPolicy", wxChoice)->Enable(en);
    XRCCTRL(*this, "cmbLinkerPolicy",   wxChoice)->Enable(en);
    XRCCTRL(*this, "cmbIncludesPolicy", wxChoice)->Enable(en);
    XRCCTRL(*this, "cmbLibDirsPolicy",  wxChoice)->Enable(en);
    XRCCTRL(*this, "cmbResDirsPolicy",  wxChoice)->Enable(en);

    // compiler set buttons
    if (!hasProject)
    {
        en = !data; // global options selected
        wxChoice* cmb = XRCCTRL(*this, "cmbCompiler", wxChoice);
        const int idx   = GetSelectionIndex(cmb);
        const int count = cmb->GetCount(); // compilers count
        Compiler* compiler = CompilerFactory::GetCompiler(idx);

        XRCCTRL(*this, "btnSetDefaultCompiler", wxButton)->Enable(CompilerFactory::GetCompilerIndex(CompilerFactory::GetDefaultCompiler()) != idx);
        XRCCTRL(*this, "btnAddCompiler",        wxButton)->Enable(en);
        XRCCTRL(*this, "btnRenameCompiler",     wxButton)->Enable(en && count);
        XRCCTRL(*this, "btnDelCompiler",        wxButton)->Enable(en &&
                                                                  compiler &&
                                                                 !compiler->GetParentID().IsEmpty());
        XRCCTRL(*this, "btnResetCompiler",      wxButton)->Enable(en &&
                                                                  compiler &&
                                                                  compiler->GetParentID().IsEmpty());

        XRCCTRL(*this, "chkFullHtmlLog", wxCheckBox)->Enable(XRCCTRL(*this, "chkSaveHtmlLog", wxCheckBox)->IsChecked());
        XRCCTRL(*this, "btnIgnoreRemove", wxButton)->Enable(XRCCTRL(*this, "lstIgnore", wxListBox)->GetCount()>0);
        XRCCTRL(*this, "btnIgnoreAdd", wxButton)->Enable(XRCCTRL(*this, "txtIgnore", wxTextCtrl)->GetValue().Trim().Len()>0);
    }
} // OnUpdateUI

void CompilerOptionsDlg::OnApply()
{
    m_CurrentCompilerIdx = GetSelectionIndex(XRCCTRL(*this, "cmbCompiler", wxChoice));
    DoSaveCompilerDependentSettings();
    CompilerFactory::SaveSettings();

    //others (projects don't have Other tab)
    if (!m_pProject)
    {
        ConfigManager* cfg = Manager::Get()->GetConfigManager("compiler");
        wxCheckBox* chk = XRCCTRL(*this, "chkIncludeFileCwd", wxCheckBox);
        if (chk)
            cfg->Write("/include_file_cwd", (bool)chk->IsChecked());

        chk = XRCCTRL(*this, "chkIncludePrjCwd", wxCheckBox);
        if (chk)
            cfg->Write("/include_prj_cwd", (bool)chk->IsChecked());

        chk = XRCCTRL(*this, "chkSkipIncludeDeps", wxCheckBox);
        if (chk)
            cfg->Write("/skip_include_deps", (bool)chk->IsChecked());

        chk = XRCCTRL(*this, "chkSaveHtmlLog", wxCheckBox);
        if (chk)
            cfg->Write("/save_html_build_log", (bool)chk->IsChecked());

        chk = XRCCTRL(*this, "chkFullHtmlLog", wxCheckBox);
        if (chk)
            cfg->Write("/save_html_build_log/full_command_line", (bool)chk->IsChecked());

        chk = XRCCTRL(*this, "chkBuildProgressBar", wxCheckBox);
        if (chk)
            cfg->Write("/build_progress/bar", (bool)chk->IsChecked());

        chk = XRCCTRL(*this, "chkBuildProgressPerc", wxCheckBox);
        if (chk)
        {
            cfg->Write("/build_progress/percentage", (bool)chk->IsChecked());
            m_Compiler->m_LogBuildProgressPercentage = chk->IsChecked();
        }

        wxSpinCtrl* spn = XRCCTRL(*this, "spnParallelProcesses", wxSpinCtrl);
        if (spn && (((int)spn->GetValue()) != cfg->ReadInt("/parallel_processes", 0)))
        {
            if (m_Compiler->IsRunning())
                cbMessageBox(_("You can't change the number of parallel processes while building!\nSetting ignored..."), _("Warning"), wxICON_WARNING);
            else
            {
                cfg->Write("/parallel_processes", (int)spn->GetValue());
                m_Compiler->ReAllocProcesses();
            }
        }

        spn = XRCCTRL(*this, "spnMaxErrors", wxSpinCtrl);
        if (spn)
            cfg->Write("/max_reported_errors", (int)spn->GetValue());

        chk = XRCCTRL(*this, "chkRebuildSeperately", wxCheckBox);
        if (chk)
            cfg->Write("/rebuild_seperately", (bool)chk->IsChecked());

        wxListBox* lst = XRCCTRL(*this, "lstIgnore", wxListBox);
        if (lst)
        {
            wxArrayString IgnoreOutput;
            ListBox2ArrayString(IgnoreOutput, lst);
            cfg->Write("/ignore_output", IgnoreOutput);
        }

        chk = XRCCTRL(*this, "chkNonPlatComp", wxCheckBox);
        if (chk && (chk->IsChecked() != cfg->ReadBool("/non_plat_comp", false)))
        {
            if (m_Compiler->IsRunning())
                cbMessageBox(_("You can't change the option to enable or disable non-platform compilers while building!\nSetting ignored..."), _("Warning"), wxICON_WARNING);
            else
            {
                cfg->Write("/non_plat_comp", (bool)chk->IsChecked());
                CompilerFactory::UnregisterCompilers();
                m_Compiler->DoRegisterCompilers();
                m_Compiler->LoadOptions();
            }
        }
    }

    m_Compiler->SaveOptions();
    m_Compiler->SetupEnvironment();
    Manager::Get()->GetMacrosManager()->Reset();
    m_bDirty = false;
} // OnApply

void CompilerOptionsDlg::OnMyCharHook(wxKeyEvent& event)
{
    wxWindow* focused = wxWindow::FindFocus();
    if (!focused)
    {
        event.Skip();
        return;
    }
    int keycode = event.GetKeyCode();
    int id      = focused->GetId();

    int myid = 0;
    unsigned int myidx = 0;

    const wxChar* str_libs[4] = { _T("btnEditLib"),  _T("btnAddLib"),  _T("btnDelLib"),     _T("btnClearLib")   };
    const wxChar* str_dirs[4] = { _T("btnEditDir"),  _T("btnAddDir"),  _T("btnDelDir"),     _T("btnClearDir")   };
    const wxChar* str_vars[4] = { _T("btnEditVar"),  _T("btnAddVar"),  _T("btnDeleteVar"),  _T("btnClearVar")   };
    const wxChar* str_xtra[4] = { _T("btnExtraEdit"),_T("btnExtraAdd"),_T("btnExtraDelete"),_T("btnExtraClear") };

    if (keycode == WXK_RETURN || keycode == WXK_NUMPAD_ENTER)
    { myidx = 0; } // Edit
    else if (keycode == WXK_INSERT || keycode == WXK_NUMPAD_INSERT)
    { myidx = 1; } // Add
    else if (keycode == WXK_DELETE || keycode == WXK_NUMPAD_DELETE)
    { myidx = 2; } // Delete
    else
    {
        event.Skip();
        return;
    }

    if (     id == XRCID("lstLibs")) // Link libraries
        { myid =  wxXmlResource::GetXRCID(str_libs[myidx]); }
    else if (id == XRCID("lstIncludeDirs") || id == XRCID("lstLibDirs") || id == XRCID("lstResDirs")) // Directories
        { myid =  wxXmlResource::GetXRCID(str_dirs[myidx]); }
    else if (id == XRCID("lstVars")) // Custom Vars
        { myid =  wxXmlResource::GetXRCID(str_vars[myidx]); }
    else if (id == XRCID("lstExtraPaths")) // Extra Paths
        { myid =  wxXmlResource::GetXRCID(str_xtra[myidx]); }
    else
        myid = 0;

    // Generate the event
    if (myid == 0)
        event.Skip();
    else
    {
        wxCommandEvent newevent(wxEVT_COMMAND_BUTTON_CLICKED,myid);
        this->ProcessEvent(newevent);
    }
} // OnMyCharHook

int CompilerOptionsDlg::m_MenuOption = -1;

void CompilerOptionsDlg::OnFlagsPopup(wxPropertyGridEvent& event)
{
    int scroll = m_FlagsPG->GetScrollPos(wxVERTICAL);
    wxPGProperty *property = event.GetProperty();

    enum FlagsMenuOptions
    {
        FMO_None = -1,
        FMO_New = 0,
        FMO_Modify,
        FMO_Delete,
        FMO_COnly,
        FMO_CPPOnly,
        FMO_ExpandAll,
        FMO_CollapseAll
    };

    wxMenu* pop = new wxMenu;
    pop->Append(FMO_New, _("New flag..."));
    if (property && !property->IsCategory())
    {
        pop->Append(FMO_Modify, _("Modify flag..."));
        pop->Append(FMO_Delete, _("Delete flag"));
    }
    pop->AppendSeparator();
    pop->Append(FMO_COnly, _("C - only flags..."));
    pop->Append(FMO_CPPOnly, _("C++ - only flags..."));
    pop->AppendSeparator();
    pop->Append(FMO_ExpandAll, _("Expand all categories"));
    pop->Append(FMO_CollapseAll, _("Collapse all categories"));
    pop->Connect(wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&CompilerOptionsDlg::OnFlagsPopupClick);
    m_MenuOption = FMO_None;
    m_FlagsPG->PopupMenu(pop);
    delete pop;
    if (m_MenuOption == FMO_None)
        return;
    if (m_MenuOption == FMO_COnly)
    {
        Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
        wxTextEntryDialog dlg(this, _("List flags that will only be used during C compilation"),
                              _("C - only flags"), compiler->GetCOnlyFlags(), wxTextEntryDialogStyle|wxTE_MULTILINE|wxRESIZE_BORDER);
        // TODO: Hack for Ubuntu, see here: http://forums.codeblocks.org/index.php/topic,16463.msg115270.html#msg115270 (Remove if no longer needed.)
        if (dlg.GetSize().GetHeight() < 220)
        {
            dlg.SetSize(dlg.GetPosition().x, dlg.GetPosition().y - (220 - dlg.GetSize().GetHeight()) / 2,
                        dlg.GetSize().GetWidth(), 220);
        }
        PlaceWindow(&dlg);
        dlg.ShowModal();
        wxString flags = dlg.GetValue();
        flags.Replace(wxT("\n"), wxT(" "));
        flags.Replace(wxT("\r"), wxT(" "));
        flags.Replace(wxT("\t"), wxT(" "));
        flags = MakeUniqueString(flags, wxT(" "));
        if (flags != compiler->GetCOnlyFlags())
        {
            compiler->SetCOnlyFlags(flags);
            m_bDirty = true;
        }
        return;
    }
    else if (m_MenuOption == FMO_CPPOnly)
    {
        Compiler* compiler = CompilerFactory::GetCompiler(m_CurrentCompilerIdx);
        wxTextEntryDialog dlg(this, _("List flags that will only be used during C++ compilation"),
                              _("C++ - only flags"), compiler->GetCPPOnlyFlags(), wxTextEntryDialogStyle|wxTE_MULTILINE|wxRESIZE_BORDER);
        // TODO: Hack for Ubuntu, see here: http://forums.codeblocks.org/index.php/topic,16463.msg115270.html#msg115270 (Remove if no longer needed.)
        if (dlg.GetSize().GetHeight() < 220)
        {
            dlg.SetSize(dlg.GetPosition().x, dlg.GetPosition().y - (220 - dlg.GetSize().GetHeight()) / 2,
                        dlg.GetSize().GetWidth(), 220);
        }
        PlaceWindow(&dlg);
        dlg.ShowModal();
        wxString flags = dlg.GetValue();
        flags.Replace(wxT("\n"), wxT(" "));
        flags.Replace(wxT("\r"), wxT(" "));
        flags.Replace(wxT("\t"), wxT(" "));
        flags = MakeUniqueString(flags, wxT(" "));
        if (flags != compiler->GetCPPOnlyFlags())
        {
            compiler->SetCPPOnlyFlags(flags);
            m_bDirty = true;
        }
        return;
    }
    else if (m_MenuOption == FMO_Delete)
    {
        size_t i = 0;
        for (; i < m_Options.GetCount(); ++i)
        {
            if (m_Options.GetOption(i)->name == property->GetLabel())
                break;
        }
        m_Options.RemoveOption(i);
    }
    else if (m_MenuOption == FMO_ExpandAll)
    {
        m_FlagsPG->ExpandAll();
        return;
    }
    else if (m_MenuOption == FMO_CollapseAll)
    {
        m_FlagsPG->CollapseAll();
        return;
    }
    else
    {
        wxArrayString categ;
        for (size_t i = 0; i < m_Options.GetCount(); ++i)
        {
            CompOption* opt = m_Options.GetOption(i);
            if (!opt)
                break;
            bool known = false;
            for (size_t j = 0; j < categ.GetCount(); ++j)
            {
                if (categ[j] == opt->category)
                {
                    known = true;
                    break;
                }
            }
            if (!known)
                categ.Add(opt->category);
        }
        if (categ.IsEmpty())
            categ.Add(wxT("General"));
        CompOption copt;

        wxString categoryName;
        if (property)
        {
            if (m_MenuOption == FMO_Modify)
                copt = *m_Options.GetOptionByName(property->GetLabel());

            // If we have a selected property try to find the name of the category.
            if (property->IsCategory())
                categoryName = property->GetLabel();
            else
            {
                wxPGProperty *category = property->GetParent();
                if (category)
                    categoryName = category->GetLabel();
            }
        }
        CompilerFlagDlg dlg(nullptr, &copt, categ, categoryName);
        PlaceWindow(&dlg);
        if (dlg.ShowModal() != wxID_OK)
            return;
        if (m_MenuOption == FMO_New)
        {
            size_t i;
            if (property)
            {
                wxString name;
                if (property->IsCategory())
                {
                    wxPGProperty *child = m_FlagsPG->GetFirstChild(property);
                    if (child)
                        name = child->GetLabel();
                }
                else
                    name = property->GetLabel();
                for (i = 0; i < m_Options.GetCount(); ++i)
                {
                    if (m_Options.GetOption(i)->name == name)
                        break;
                }
            }
            else
                i = m_Options.GetCount() - 1;

            m_Options.AddOption(copt.name, copt.option,
                                copt.category, copt.additionalLibs,
                                copt.checkAgainst, copt.checkMessage,
                                copt.supersedes, copt.exclusive, i + 1);
        }
        else
        {
            if (property)
            {
                CompOption* opt = m_Options.GetOptionByName(property->GetLabel());
                wxString name = copt.name + wxT("  [");
                if (copt.option.IsEmpty())
                    name += copt.additionalLibs;
                else
                    name += copt.option;
                name += wxT("]");
                opt->name           = name;
                opt->option         = copt.option;
                opt->additionalLibs = copt.additionalLibs;
                opt->category       = copt.category;
                opt->checkAgainst   = copt.checkAgainst;
                opt->checkMessage   = copt.checkMessage;
                opt->supersedes     = copt.supersedes;
                opt->exclusive      = copt.exclusive;
            }
        }
    }
    DoFillOptions();
    m_FlagsPG->ScrollLines(scroll);
    m_bFlagsDirty = true;
}

void CompilerOptionsDlg::OnFlagsPopupClick(wxCommandEvent& event)
{
    m_MenuOption = event.GetId();
}

void CompilerOptionsDlg::OnOptionDoubleClick(wxPropertyGridEvent& event)
{
    wxPGProperty* property = event.GetProperty();
    // For bool properties automatically toggle the checkbox on double click.
    if (property && property->IsKindOf(CLASSINFO(wxBoolProperty)))
    {
        bool realValue = m_FlagsPG->GetPropertyValue(property);
        m_FlagsPG->ChangePropertyValue(property, !realValue);
    }
    event.Skip();
}
