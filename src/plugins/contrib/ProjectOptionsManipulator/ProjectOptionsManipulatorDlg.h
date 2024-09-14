/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef PROJECTOPTIONSMANIPULATORDLG_H
#define PROJECTOPTIONSMANIPULATORDLG_H

//(*Headers(ProjectOptionsManipulatorDlg)
#include <wx/dialog.h>
class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxFlexGridSizer;
class wxRadioBox;
class wxStaticBoxSizer;
class wxStaticText;
class wxStdDialogButtonSizer;
class wxTextCtrl;
//*)

class ProjectOptionsManipulatorDlg: public wxDialog
{
	public:
	  enum EProjectScanOption       { eSearch, eSearchNot, eRemove,
	                                  eAdd,    eReplace,   eFiles, eChangeCompiler };
	  enum EProjectLevelOption      { eProject, eTarget };
	  enum EProjectSearchOption     { eEquals, eContains };
	  enum EProjectOption           { eReplacePattern,
	                                  eCompiler,      eLinker,      eResCompiler,
	                                  eCompilerPaths, eLinkerPaths, eResCompPaths,
	                                  eLinkerLibs,    eCustomVars };
	  enum EProjectTargetTypeOption { eAll, eApplication, eStaticLib, eDynamicLib };

		ProjectOptionsManipulatorDlg(wxWindow* parent,wxWindowID id=wxID_ANY);
		virtual ~ProjectOptionsManipulatorDlg();

    bool                     GetScanForWorkspace();
    bool                     GetScanForProject();
    size_t                   GetProjectIdx();
    EProjectScanOption       GetScanOption();
    EProjectSearchOption     GetSearchOption();
    wxString                 GetSearchFor();
    wxString                 GetReplaceWith();
    wxString                 GetCustomVarValue();
    bool                     GetOptionActive(EProjectOption opt);
    bool                     GetOptionActive(EProjectLevelOption opt);
    EProjectTargetTypeOption GetTargetTypeOption();

		//(*Declarations(ProjectOptionsManipulatorDlg)
		wxButton* m_BtnSearchCompilerDest;
		wxButton* m_BtnSearchCompilerSrc;
		wxCheckBox* m_ChkOptionReplacePattern;
		wxCheckBox* m_ChkOptionsCompiler;
		wxCheckBox* m_ChkOptionsCompilerPath;
		wxCheckBox* m_ChkOptionsCustomVar;
		wxCheckBox* m_ChkOptionsLinker;
		wxCheckBox* m_ChkOptionsLinkerLibs;
		wxCheckBox* m_ChkOptionsLinkerPath;
		wxCheckBox* m_ChkOptionsResCompPath;
		wxCheckBox* m_ChkOptionsResCompiler;
		wxChoice* m_ChoOptionLevel;
		wxChoice* m_ChoScan;
		wxChoice* m_ChoScanProjects;
		wxChoice* m_ChoTargetType;
		wxRadioBox* m_RboOperation;
		wxRadioBox* m_RboOptionSearch;
		wxTextCtrl* m_TxtCustomVar;
		wxTextCtrl* m_TxtOptionReplace;
		wxTextCtrl* m_TxtOptionSearch;
		//*)

	protected:

		//(*Identifiers(ProjectOptionsManipulatorDlg)
		static const wxWindowID ID_CHO_SCAN;
		static const wxWindowID ID_CHO_SCAN_PROJECTS;
		static const wxWindowID ID_RBO_OPERATION;
		static const wxWindowID ID_CHO_OPTION_LEVEL;
		static const wxWindowID ID_TXT_OPTION_SEARCH;
		static const wxWindowID ID_BTN_SEARCH_COMPILER_SRC;
		static const wxWindowID TD_TXT_OPTION_REPLACE;
		static const wxWindowID ID_BTN_SEARCH_COMPILER_DEST;
		static const wxWindowID ID_CHK_OPTION_REPLACE_PATTERN;
		static const wxWindowID ID_RBO_OPTION_SEARCH;
		static const wxWindowID ID_CHK_OPTIONS_COMPILER;
		static const wxWindowID ID_CHK_OPTIONS_LINKER;
		static const wxWindowID ID_CHK_OPTIONS_RES_COMPILER;
		static const wxWindowID ID_CHK_OPTIONS_COMPILER_PATH;
		static const wxWindowID ID_CHK_OPTIONS_LINKER_PATH;
		static const wxWindowID ID_CHK_OPTIONS_RES_COMP_PATH;
		static const wxWindowID ID_CHK_OPTIONS_LINKER_LIBS;
		static const wxWindowID ID_CHK_OPTIONS_CUSTOM_VAR;
		static const wxWindowID ID_TXT_CUSTOM_VAR;
		static const wxWindowID ID_CHO_TARGET_TYPE;
		//*)

	private:

		//(*Handlers(ProjectOptionsManipulatorDlg)
		void OnScanSelect(wxCommandEvent& event);
		void OnOperationSelect(wxCommandEvent& event);
		void OnTargetTypeSelect(wxCommandEvent& event);
		void OnSearchCompilerClick(wxCommandEvent& event);
		//*)

		void OnOk(wxCommandEvent& event);

		DECLARE_EVENT_TABLE()
};

#endif
