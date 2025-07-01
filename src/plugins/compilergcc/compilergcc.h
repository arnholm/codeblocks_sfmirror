/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERGCC_H
#define COMPILERGCC_H

#include <wx/choice.h>
#include <wx/dynarray.h>
#include <wx/process.h>
#include <wx/timer.h>

#include <queue>

#include "cbplugin.h"
#include "cbproject.h"
#include "compileoptionsbase.h"
#include "compilerfactory.h"
#include "logger.h"
#include "sdk_events.h"
#include "settings.h" // SDK

#include "compilermessages.h"
#include "compilererrors.h"
#include "compiler_defs.h"

enum CompilerOptionsType
{
    cotGlobal = 0,
    cotProjectOrTarget
};

enum ErrorType
{
    etNone = 0,
    etError,
    etWarning
};

/// Helper enum for compiler's state. This state signifies the kind of build the compiler is working on.
enum BuildJob
{
    bjIdle = 0, ///< Not currently building
    bjWorkspace, ///< Building the workspace
    bjProject, ///< Building the project
    bjTarget ///< Building the target
};

/// Defines the current state of the compiler.
enum BuildState
{
    bsNone = 0,
    bsProjectPreBuild,
    bsTargetClean,
    bsTargetPreBuild,
    bsTargetBuild,
    bsTargetPostBuild,
    bsTargetDone,
    bsProjectPostBuild,
    bsProjectDone
};

enum LogTarget
{
    ltMessages  = 0x01,
    ltFile      = 0x02,

    ltAll       = 0xff
};

enum BuildAction
{
    baClean = 0,
    baBuild,
    baRun,
    baBuildFile
};

class cbArtProvider;
class wxComboBox;
class wxGauge;
class wxStaticText;

class BuildLogger;
class PipedProcess;

class CompilerGCC : public cbCompilerPlugin
{
    public:
        CompilerGCC();
        ~CompilerGCC() override;

        void OnAttach() override;
        void OnRelease(bool appShutDown) override;
        void BuildMenu(wxMenuBar* menuBar) override; // offer for menu space by host
        void BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data = 0) override; // offer for menu space by a module
        bool BuildToolBar(wxToolBar* toolBar) override;
        int GetToolBarPriority() override { return 1; }

        int Run(ProjectBuildTarget* target = nullptr) override;
        int Run(const wxString& target) override;
        virtual int RunSingleFile(const wxString& filename);
        int Clean(const wxString& target) override;
        int Clean(ProjectBuildTarget* target = nullptr) override;
        int DistClean(ProjectBuildTarget* target = nullptr) override;
        int DistClean(const wxString& target) override;
        int Build(ProjectBuildTarget* target = nullptr) override;
        int Build(const wxString& target) override;
        int Rebuild(ProjectBuildTarget* target = nullptr) override;
        int Rebuild(const wxString& target) override;
        int CleanWorkspace(const wxString& target = wxEmptyString) override;
        int BuildWorkspace(const wxString& target = wxEmptyString) override;
        int RebuildWorkspace(const wxString& target = wxEmptyString) override;
        int CompileFile(const wxString& file) override;
        virtual int CompileFileWithoutProject(const wxString& file);
        virtual int CompileFileDefault(cbProject* project, ProjectFile* pf, ProjectBuildTarget* bt);
        int KillProcess() override;
        bool IsRunning() const override;
        int GetExitCode() const override { return m_LastExitCode; }
        int Configure(cbProject* project, ProjectBuildTarget* target, wxWindow *parent) override;

        int GetConfigurationPriority() const override { return 0; }
        int GetConfigurationGroup() const override { return cgCompiler; }
        cbConfigurationPanel* GetConfigurationPanel(wxWindow* parent) override;

        bool IsValidTarget(const wxString& target) const;

        void SwitchCompiler(const wxString& id);
        const wxString& GetCurrentCompilerID();

        // used to read from the external process
        void OnIdle(wxIdleEvent& event);
        void OnTimer(wxTimerEvent& event);

        void OnCompile(wxCommandEvent& event);
        void OnCompileFile(wxCommandEvent& event);
        void OnCleanFile(wxCommandEvent& event);
        void OnRebuild(wxCommandEvent& event);
        void OnCompileAll(wxCommandEvent& event);
        void OnRebuildAll(wxCommandEvent& event);
        void OnCleanAll(wxCommandEvent& event);
        void OnClean(wxCommandEvent& event);
        void OnRun(wxCommandEvent& event);
        void OnProjectCompilerOptions(wxCommandEvent& event);
        void OnTargetCompilerOptions(wxCommandEvent& event);
        void OnCompileAndRun(wxCommandEvent& event);
        void OnKillProcess(wxCommandEvent& event);
        void OnSelectTarget(wxCommandEvent& event);
        void OnNextError(wxCommandEvent& event);
        void OnPreviousError(wxCommandEvent& event);
        void OnClearErrors(wxCommandEvent& event);
        void OnUpdateUI(wxUpdateUIEvent& event);
        void OnConfig(wxCommandEvent& event);
    private:
        friend class CompilerOptionsDlg;

        void Dispatcher(wxCommandEvent& event);
        void TextURL(wxTextUrlEvent& event);

        bool StopRunningDebugger();

        bool ReAllocProcesses();
        void AllocProcesses();
        void FreeProcesses();
        bool IsProcessRunning(int idx = -1) const;
        int GetNextAvailableProcessIndex() const;
        int GetActiveProcessCount() const;

        void SetupEnvironment();
        void OnProjectActivated(CodeBlocksEvent& event);
        void OnProjectLoaded(CodeBlocksEvent& event);
        void OnProjectUnloaded(CodeBlocksEvent& event);
        void OnWorkspaceClosed(CodeBlocksEvent& event);
        void OnCompileFileRequest(CodeBlocksEvent& event);
        void OnGCCOutput(CodeBlocksEvent& event);
        void OnGCCError(CodeBlocksEvent& event);
        void OnGCCTerminated(CodeBlocksEvent& event);
        void OnJobEnd(size_t procIndex, int exitCode);

        void SaveOptions();
        void LoadOptions();
        void DoRegisterCompilers();
        void DoPrepareQueue(bool clearLog);
        void NotifyCleanProject(const wxString& target);
        void NotifyCleanWorkspace();
        int DoRunQueue();
        void DoClearTargetMenu();
        void DoRecreateTargetMenu();
        void DoUpdateTargetMenu(int targetIndex);
        FileTreeData* DoSwitchProjectTemporarily();
        ProjectBuildTarget* DoAskForTarget();
        int DoGUIAskForTarget();
        void ClearLog(bool switchToLog);
        void PrepareCompileFile(wxFileName& file);
        void PrepareCompileFilePM(wxFileName& file);
        bool CheckProject();
        void AskForActiveProject();
        void StartCompileFile(wxFileName file);
        void DoGotoNextError();
        void DoGotoPreviousError();
        void DoClearErrors();
        wxString ProjectMakefile();
        void AddOutputLine(const wxString& output, bool forceErrorColour = false);
        void LogWarningOrError(CompilerLineType lt, cbProject* prj, const wxString& filename, const wxString& line, const wxString& msg);
        void LogMessage(const wxString& message, CompilerLineType lt = cltNormal, LogTarget log = ltAll, bool forceErrorColour = false, bool isTitle = false, bool updateProgress = false);
        void SaveBuildLog();
        void InitBuildLog(bool workspaceBuild);
        void PrintBanner(BuildAction action, cbProject* prj = 0, ProjectBuildTarget* target = 0);
        bool UseMake(cbProject* project = 0);

        struct CompilerValidResult
        {
            Compiler *compiler = nullptr;
            bool isValid = false;
        };
        CompilerValidResult CompilerValid(ProjectBuildTarget* target = 0);
        void PrintInvalidCompiler(ProjectBuildTarget *target, Compiler *compiler, const wxString &finalMessage);
        ProjectBuildTarget* GetBuildTargetForFile(ProjectFile* pf);
        wxString GetMakeCommandFor(MakeCommand cmd, cbProject* project, ProjectBuildTarget* target);
        int DoBuild(bool clean, bool build);
        int DoBuild(const wxString& target, bool clean, bool build, bool clearLog=true);
        int DoWorkspaceBuild(const wxString& target, bool clean, bool build, bool clearLog=true);
        void CalculateWorkspaceDependencies(wxArrayInt& deps);
        void CalculateProjectDependencies(cbProject* prj, wxArrayInt& deps);
        void InitBuildState(BuildJob job, const wxString& target);
        void ResetBuildState();
        void BuildStateManagement(); ///< This uses m_BuildJob.
        BuildState GetNextStateBasedOnJob();
        void NotifyJobDone(bool showNothingToBeDone = false);

        // wxArrayString from DirectCommands
        void AddToCommandQueue(const wxArrayString& commands);

        int GetTargetIndexFromName(cbProject* prj, const wxString& name);
        void UpdateProjectTargets(cbProject* project);
        wxString GetTargetString(int index = -1);
        void DoClean(const wxArrayString& commands);
        bool DoCleanWithMake(ProjectBuildTarget* bt);

        // active target, currently building project or active project
        wxString GetCurrentCompilerID(ProjectBuildTarget* target);

        wxString GetErrWarnStr();
        wxString GetMinSecStr();

        // test if executable exists
        bool ExecutableExists(cbProject* project);

        // when a build is about to start, a preprocessing step runs
        // in PreprocessJob(), that fills m_BuildJobTargetsList with
        // BuildJobTarget. It is a simple pair of project->target which
        // are to be built in order
        struct BuildJobTarget
        {
            BuildJobTarget(cbProject* p = 0, const wxString& n = wxEmptyString) : project(p), targetName(n) {}
            cbProject* project;
            wxString targetName;
        };
        typedef std::queue<BuildJobTarget> BuildJobTargetsList;
        BuildJobTargetsList m_BuildJobTargetsList;

        void ExpandTargets(cbProject* project, const wxString& targetName, wxArrayString& result);
        void PreprocessJob(cbProject* project, const wxString& targetName);
        BuildJobTarget GetNextJob();
        const BuildJobTarget& PeekNextJob();

        struct CompilerProcess
        {
            PipedProcess* pProcess;
            wxString      OutputFile;
            long int      PID;
        };
        typedef std::vector<CompilerProcess> CompilerProcessList;
        CompilerProcessList m_CompilerProcessList;

        /// List of targets contained in the active project (virtual and real).
        /// Contains only targets supported by the current platform.
        wxArrayString       m_Targets;
        int                 m_RealTargetsStartIndex;
        int                 m_RealTargetIndex;

        CompilerQueue       m_CommandQueue;
        wxString            m_CompilerId;
        int                 m_PageIndex;
        int                 m_ListPageIndex;
        wxMenu*             m_Menu;
        wxMenu*             m_TargetMenu;
        int                 m_TargetIndex;
        wxMenu*             m_pErrorsMenu;
        cbProject*          m_pProject;
        wxToolBar*          m_pTbar;
        wxTimer             m_timerIdleWakeUp;
        BuildLogger*        m_pLog;
        CompilerMessages*   m_pListLog;
        wxChoice*           m_pToolTarget;
        bool                m_RunAfterCompile;
        wxString            m_CdRun;
        wxString            m_RunCmd;
        int                 m_LastExitCode;
        CompilerErrors      m_Errors;
        wxString            m_LastTargetName;
        bool                m_NotifiedMaxErrors;
        wxLongLong          m_StartTime;
        bool                m_StartedEventSent;

        // build state management
        cbProject*          m_pBuildingProject; // +
        wxString            m_BuildingTargetName; // +
        BuildJob            m_BuildJob;
        BuildState          m_BuildState;
        BuildState          m_NextBuildState;
        cbProject*          m_pLastBuildingProject;
        ProjectBuildTarget* m_pLastBuildingTarget;
        // Clean and Build
        bool m_Clean;
        bool m_Build;
        // if set and we are reaching NotifyJobDone, we know that we have finished the
        // last step in a clean/build (aka rebuild)-process and send the cbEVT_COMPILER_FINISHED
        bool m_LastBuildStep;
        // to decide if post-build steps should run
        bool m_RunTargetPostBuild;
        bool m_RunProjectPostBuild;

        bool m_IsWorkspaceOperation; // true for workspace commands

        wxString   m_BuildLogFilename;
        wxString   m_BuildLogTitle;
        wxString   m_BuildLogContents;
        wxDateTime m_BuildStartTime;

        // build progress
        size_t m_MaxProgress;
        size_t m_CurrentProgress;
        bool   m_LogBuildProgressPercentage;

        cbArtProvider *m_pArtProvider;

        DECLARE_EVENT_TABLE()
};

#endif // COMPILERGCC_H
