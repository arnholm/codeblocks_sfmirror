/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILEROPTIONSDLG_H
#define COMPILEROPTIONSDLG_H

#include <vector>
#include <wx/intl.h>
#include <wx/string.h>
#include "configurationpanel.h"
#include "compileroptions.h"

class wxListBox;
class ScopeTreeData;
class cbProject;
class ProjectBuildTarget;
class CompilerGCC;
class wxSpinEvent;
class wxTreeEvent;
class wxCommandEvent;
class wxKeyEvent;
class wxPropertyGrid;
class wxPropertyGridEvent;
class wxUpdateUIEvent;

class CompilerOptionsDlg : public cbConfigurationPanel
{
    public:
        CompilerOptionsDlg(wxWindow* parent, CompilerGCC* compiler, cbProject* project = nullptr, ProjectBuildTarget* target = nullptr);
        ~CompilerOptionsDlg() override;

        wxString GetTitle() const override { return _("Global compiler settings"); }
        wxString GetBitmapBaseName() const override { return _T("compiler"); }
        void OnApply() override;
        void OnCancel() override {}
    private:
        enum CustomVarActionType
        {
            CVA_Add,
            CVA_Edit,
            CVA_Remove
        };

        struct CustomVarAction
        {
            CustomVarActionType m_Action;
            wxString            m_Key;
            wxString            m_KeyValue;
        };
        void TextToOptions();
        void OptionsToText();
        void DoFillCompilerSets(int compilerIdx);
        void DoFillCompilerPrograms();
        void DoFillVars();
        void DoFillOthers();
        void DoFillOptions();
        void DoFillCompilerDependentSettings();
        void DoSaveCompilerDependentSettings();
        void DoFillTree();
        void DoSaveOptions();
        void DoLoadOptions();
        void DoSaveCompilerPrograms();
        void DoSaveVars();
        void DoSaveCompilerDefinition();
        void CompilerChanged();
        void UpdateCompilerForTargets(int compilerIdx);
        void AutoDetectCompiler();
        wxListBox* GetDirsListBox();
        CompileOptionsBase* GetVarsOwner();
        void ProjectTargetCompilerAdjust(); //!< checks if compiler changed for project/target and takes actions accordingly
        void SwapItems(wxListBox* listBox, int a, int b);
        void Reselect(wxListBox* listBox, const wxArrayInt& selected, int offset);

        void OnRealApply(); // user clicked the "Apply" button (so not the Ok button !!!)
        void OnTreeSelectionChange(wxTreeEvent& event);
        void OnTreeSelectionChanging(wxTreeEvent& event);
        void OnCompilerChanged(wxCommandEvent& event);
        void OnCategoryChanged(wxCommandEvent& event);
        void OnOptionChanged(wxPropertyGridEvent& event);
        void OnAddDirClick(wxCommandEvent& event);
        void OnEditDirClick(wxCommandEvent& event);
        void OnRemoveDirClick(wxCommandEvent& event);
        void OnClearDirClick(wxCommandEvent& event);
        void OnCopyDirsClick(wxCommandEvent& event);
        void OnAddVarClick(wxCommandEvent& event);
        void OnEditVarClick(wxCommandEvent& event);
        void OnRemoveVarClick(wxCommandEvent& event);
        void OnClearVarClick(wxCommandEvent& event);
        void OnSetDefaultCompilerClick(wxCommandEvent& event);
        void OnAddCompilerClick(wxCommandEvent& event);
        void OnEditCompilerClick(wxCommandEvent& event);
        void OnRemoveCompilerClick(wxCommandEvent& event);
        void OnResetCompilerClick(wxCommandEvent& event);
        void OnAddLibClick(wxCommandEvent& event);
        void OnEditLibClick(wxCommandEvent& event);
        void OnRemoveLibClick(wxCommandEvent& event);
        void OnClearLibClick(wxCommandEvent& event);
        void OnCopyLibsClick(wxCommandEvent& event);
        void OnMoveLibUpClick(wxCommandEvent& event);
        void OnMoveLibDownClick(wxCommandEvent& event);
        void OnMoveDirUpClick(wxCommandEvent& event);
        void OnMoveDirDownClick(wxCommandEvent& event);
        void OnMasterPathClick(wxCommandEvent& event);
        void OnAutoDetectClick(wxCommandEvent& event);
        void OnSelectProgramClick(wxCommandEvent& event);
        void OnAdvancedClick(wxCommandEvent& event);
        void OnAddExtraPathClick(wxCommandEvent& event);
        void OnEditExtraPathClick(wxCommandEvent& event);
        void OnRemoveExtraPathClick(wxCommandEvent& event);
        void OnClearExtraPathClick(wxCommandEvent& event);
        void OnIgnoreAddClick(wxCommandEvent& event);
        void OnIgnoreRemoveClick(wxCommandEvent& event);
        void OnUpdateUI(wxUpdateUIEvent& event);
        void OnDirty(wxCommandEvent& event); // some controls who change their value -> dirty
        void OnMyCharHook(wxKeyEvent& event);
        void OnFlagsPopup(wxPropertyGridEvent& event);
        void OnFlagsPopupClick(wxCommandEvent& event);
        void OnOptionDoubleClick(wxPropertyGridEvent& event);

    private:
        wxPropertyGrid*              m_FlagsPG;
        CompilerGCC*                 m_Compiler;
        CompilerOptions              m_Options;
        wxArrayString                m_LinkerOptions;
        wxArrayString                m_LinkLibs;
        wxArrayString                m_CompilerOptions;
        wxArrayString                m_ResourceCompilerOptions;
        int                          m_CurrentCompilerIdx;
        cbProject*                   m_pProject;
        ProjectBuildTarget*          m_pTarget;
        bool                         m_bDirty;                       //!< true if a setting has changed since last save
        bool                         m_bFlagsDirty;                  //!< true if a flag (not its value) has changed since last save
        std::vector<CustomVarAction> m_CustomVarActions;             //!< the actions carried out on the custom vars that need to be saved/applied
        wxString                     m_NewProjectOrTargetCompilerId; //!< keeps track of the changes of compiler of the selected project/target

        bool                         m_BuildingTree;                 //!< flag to ignore tree changing events while building it
        static int                   m_MenuOption;

        DECLARE_EVENT_TABLE()
};

#endif // COMPILEROPTIONSDLG_H
