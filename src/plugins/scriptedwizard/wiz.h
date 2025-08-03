/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef WIZ_H
#define WIZ_H

#include <cbplugin.h> // the base class we 're inheriting
#include <settings.h> // needed to use the Code::Blocks SDK
#include <cbexception.h>
#include <wx/bitmap.h>

class wxWizard;
class wxWizardPageSimple;
class WizProjectPathPanel;
class WizFilePathPanel;
class WizCompilerPanel;
class WizBuildTargetPanel;
class WizGenericSingleChoiceList;
class WizGenericSelectPathPanel;

struct WizardInfo
{
    TemplateOutputType output_type;
    wxString title;
    wxString cat;
    wxString script;
    wxBitmap templatePNG;
    wxBitmap wizardPNG;
    wxString xrc;
};

WX_DECLARE_OBJARRAY(WizardInfo, Wizards);
WX_DEFINE_ARRAY(wxWizardPageSimple*, WizPages);

class Wiz : public cbWizardPlugin
{
	public:
		Wiz();
		~Wiz() override;

        Wiz(const Wiz&) = delete;
        Wiz& operator=(const Wiz&) = delete;

		int GetCount() const override;
        TemplateOutputType GetOutputType(int index) const override;
		wxString GetTitle(int index) const override;
		wxString GetDescription(int index) const override;
		wxString GetCategory(int index) const override;
		const wxBitmap& GetBitmap(int index) const override;
        wxString GetScriptFilename(int index) const override;
		CompileTargetBase* Launch(int index, wxString* pFilename = nullptr) override;

		CompileTargetBase* RunProjectWizard(wxString* pFilename); // called by Launch() for otProject wizards
		CompileTargetBase* RunTargetWizard(wxString* pFilename); // called by Launch() for otTarget wizards (always returns NULL)
		CompileTargetBase* RunFilesWizard(wxString* pFilename); // called by Launch() for otFiles wizards (always returns NULL)
		CompileTargetBase* RunCustomWizard(wxString* pFilename); // called by Launch() for otCustom wizards (always returns NULL)

        // Scripting support
        void AddWizard(TemplateOutputType otype,
                        const wxString& title,
                        const wxString& cat,
                        const wxString& script,
                        const wxString& templatePNG,
                        const wxString& wizardPNG,
                        const wxString& xrc);

        TemplateOutputType GetWizardType();

        void EnableWindow(const wxString& name, bool enable);

        void CheckCheckbox(const wxString& name, bool check);
        bool IsCheckboxChecked(const wxString& name);

        void FillComboboxWithCompilers(const wxString& name);
        void FillContainerWithSelectCompilers(const wxString& name,
                                              const wxString& validCompilerIDs);
        void AppendContainerWithSelectCompilers(const wxString& name,
                                                const wxString& validCompilerIDs);
        wxString GetCompilerFromCombobox(const wxString& name);
        void FillContainerWithCompilers(const wxString& name, const wxString& compilerID,
                                        const wxString& validCompilerIDs);

        wxString GetComboboxStringSelection(const wxString& name);
        int GetComboboxSelection(const wxString& name);
        void SetComboboxSelection(const wxString& name, int sel);

        void SetComboboxValue(const wxString& name, const wxString& value);
        wxString GetComboboxValue(const wxString& name);

        int GetRadioboxSelection(const wxString& name);
        void SetRadioboxSelection(const wxString& name, int sel);

        int GetListboxSelection(const wxString& name);
        wxString GetListboxSelections(const wxString& name);
        wxString GetListboxStringSelections(const wxString& name);
        void SetListboxSelection(const wxString& name, int sel);

        wxString GetCheckListboxChecked(const wxString& name);
        wxString GetCheckListboxStringChecked(const wxString& name);
        bool IsCheckListboxItemChecked(const wxString& name, unsigned int item);
        void CheckCheckListboxItem(const wxString& name, unsigned int item, bool check);

        void SetTextControlValue(const wxString& name, const wxString& value);
        wxString GetTextControlValue(const wxString& name);

        void SetSpinControlValue(const wxString& name, int value);
        int GetSpinControlValue(const wxString& name);

        // project path page
        wxString GetProjectPath();
        wxString GetProjectName();
        wxString GetProjectFullFilename();
        wxString GetProjectTitle();

        // compiler page
        wxString GetCompilerID();
        bool GetWantDebug();
        wxString GetDebugName();
        wxString GetDebugOutputDir();
        wxString GetDebugObjectOutputDir();
        bool GetWantRelease();
        wxString GetReleaseName();
        wxString GetReleaseOutputDir();
        wxString GetReleaseObjectOutputDir();

        // build target page
        wxString GetTargetCompilerID();
        bool GetTargetEnableDebug();
        wxString GetTargetName();
        wxString GetTargetOutputDir();
        wxString GetTargetObjectOutputDir();

        // file path page
        wxString GetFileName();
        wxString GetFileHeaderGuard();
        bool GetFileAddToProject();
        int GetFileTargetIndex();
        void SetFilePathSelectionFilter(const wxString& filter);

        // compiler defaults
        void SetCompilerDefault(const wxString& defCompilerID);
        void SetDebugTargetDefaults(bool wantDebug,const wxString& debugName,
                                    const wxString& debugOut, const wxString& debugObjOut);
        void SetReleaseTargetDefaults(bool wantRelease, const wxString& releaseName,
                                      const wxString& releaseOut, const wxString& releaseObjOut);

        int FillContainerWithChoices(const wxString& name, const wxString& choices);
        int AppendContainerWithChoices(const wxString& name, const wxString& choices);
        wxString GetWizardScriptFolder(void);

        // pre-defined pages
        void AddInfoPage(const wxString& pageId, const wxString& intro_msg);
        void AddFilePathPage(bool showHeaderGuard);
        void AddProjectPathPage();
        void AddCompilerPage(const wxString& compilerID, const wxString& validCompilerIDs,
                             bool allowCompilerChange = true, bool allowConfigChange = true);
        void AddBuildTargetPage(const wxString& targetName, bool isDebug, bool showCompiler = false,
                                const wxString& compilerID = wxEmptyString,
                                const wxString& validCompilerIDs = _T("*"),
                                bool allowCompilerChange = true);
        void AddGenericSingleChoiceListPage(const wxString& pageName, const wxString& descr,
                                            const wxString& choices, int defChoice);
        void AddGenericSelectPathPage(const wxString& pageId, const wxString& descr,
                                      const wxString& label, const wxString& defValue);
        // XRC pages
        void AddPage(const wxString& panelName);

        void Finalize();
        void RegisterWizard();
        wxString FindTemplateFile(const wxString& filename);
	protected:
        void OnAttach() override;
        void OnRelease(bool appShutDown) override;
        void Clear();
        void CopyFiles(cbProject* theproject, const wxString&  prjdir, const wxString& srcdir);
        wxString GenerateFile(const wxString& basePath, const wxString& filename, const wxString& contents);

        Wizards m_Wizards;
        wxWizard* m_pWizard;
        WizPages m_Pages;
        WizProjectPathPanel* m_pWizProjectPathPanel;
        WizFilePathPanel* m_pWizFilePathPanel;
        WizCompilerPanel* m_pWizCompilerPanel;
        WizBuildTargetPanel* m_pWizBuildTargetPanel;
        int m_LaunchIndex;
        wxString m_LastXRC;

        // default compiler settings (returned if no compiler page is added in the wizard)
        wxString m_DefCompilerID;
        bool m_WantDebug;
        wxString m_DebugName;
        wxString m_DebugOutputDir;
        wxString m_DebugObjOutputDir;
        bool m_WantRelease;
        wxString m_ReleaseName;
        wxString m_ReleaseOutputDir;
        wxString m_ReleaseObjOutputDir;
        wxString m_WizardScriptFolder;
};

#endif // WIZ_H

