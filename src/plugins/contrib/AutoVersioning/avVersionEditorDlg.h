//{Info
/*
 ** Purpose:   Code::Blocks - Autoversioning Plugin
 ** Author:    JGM
 ** Created:   06/29/07 02:48:59 p.m.
 ** Copyright: (c) JGM
 ** License:   GPL
 */
//}

#ifndef DLGVERSIONINTIALIZER_H
#define DLGVERSIONINTIALIZER_H

#include <typeinfo>

//(*Headers(avVersionEditorDlg)
#include "scrollingdialog.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
//*)

#include <wx/event.h>

class avVersionEditorDlg: public wxScrollingDialog
{
private:
    long m_major;
    long m_minor;
    long m_build;
    long m_count;
    long m_revision;
    bool m_autoMajorMinor;
    bool m_dates;
	// GJH 03/03/10 Added manifest updating.
    bool m_updateManifest;
    bool m_useDefine;
    bool m_svn;
    bool m_commit;
    bool m_askCommit;
    long m_minorMaximun;
    long m_buildMaximun;
    long m_revisionMaximun;
    long m_revisionRandomMaximun;
    long m_buildTimesToMinorIncrement;
    long m_changes;
    wxString m_headerGuard;
    wxString m_namespace;
    wxString m_prefix;
    wxString m_svnDirectory;
    wxString m_status;
    wxString m_statusAbbreviation;
    wxString m_changesTitle;
    wxString m_language;
    wxString m_headerPath;
    wxString m_changesLogPath;

	void ValidateInput();

	DECLARE_EVENT_TABLE()


public:
		avVersionEditorDlg(wxWindow* parent,wxWindowID id = -1);
		virtual ~avVersionEditorDlg();

		//(*Identifiers(avVersionEditorDlg)
		static const wxWindowID ID_MAJOR_LABEL;
		static const wxWindowID ID_MAJOR_TEXT;
		static const wxWindowID ID_MINOR_LABEL;
		static const wxWindowID ID_MINOR_TEXT;
		static const wxWindowID ID_BUILD_LABEL;
		static const wxWindowID ID_BUILD_TEXT;
		static const wxWindowID ID_REVISION_LABEL;
		static const wxWindowID ID_REVISION_TEXT;
		static const wxWindowID ID_STATICLINE2;
		static const wxWindowID ID_COUNT_LABEL;
		static const wxWindowID ID_COUNT_TEXT;
		static const wxWindowID ID_VALUES_PANEL;
		static const wxWindowID ID_SATUS_LABEL;
		static const wxWindowID ID_STATUS_COMBOBOX;
		static const wxWindowID ID_STATICLINE4;
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_ABBREVIATION_COMBOBOX;
		static const wxWindowID ID_STATUS_PANEL;
		static const wxWindowID ID_MINORMAXIMUN_LABEL;
		static const wxWindowID ID_MINORMAXIMUM_TEXT;
		static const wxWindowID ID_BUILDNUMBERMAX_LABEL;
		static const wxWindowID ID_BUILDNUMBERMAX_TEXT;
		static const wxWindowID ID_REVISIONMAX_LABEL;
		static const wxWindowID ID_REVISIONMAX_TEXT;
		static const wxWindowID ID_REVISIONRANDOM_LABEL;
		static const wxWindowID ID_REVISIONRANDOM_TEXT;
		static const wxWindowID ID_BUILDTIMES_LABEL;
		static const wxWindowID ID_BUILDTIMES_TEXT;
		static const wxWindowID ID_SCHEME_PANEL;
		static const wxWindowID ID_HEADER_GUARD_LABEL;
		static const wxWindowID ID_HEADER_GUARD_TEXT;
		static const wxWindowID ID_NAMESPACE_LABEL;
		static const wxWindowID ID_NAMESPACE_TEXT;
		static const wxWindowID ID_PREFIX_LABEL;
		static const wxWindowID ID_PREFIX_TEXT;
		static const wxWindowID ID_CODE_PANEL;
		static const wxWindowID ID_AUTO_CHECK;
		static const wxWindowID ID_DATES_CHECK;
		static const wxWindowID ID_DEFINE_CHECK;
		static const wxWindowID ID_UPDATE_MANIFEST;
		static const wxWindowID ID_COMMIT_CHECK;
		static const wxWindowID ID_ASKCOMMIT_CHECK;
		static const wxWindowID ID_STATICLINE3;
		static const wxWindowID ID_HEADERPATH_LABEL;
		static const wxWindowID ID_HEADERPATH_TEXTCTRL;
		static const wxWindowID ID_HEADERPATH_BUTTON;
		static const wxWindowID ID_HEADERLANGUAGE_RADIOBOX;
		static const wxWindowID ID_STATICLINE1;
		static const wxWindowID ID_SVN_CHECK;
		static const wxWindowID ID_SVNDIR_TEXT;
		static const wxWindowID ID_SVNDIR_BUTTON;
		static const wxWindowID ID_SETTINGS_PANEL;
		static const wxWindowID ID_GENERATECHANGES_CHECKBOX;
		static const wxWindowID ID_CHANGESPATH_STATICTEXT;
		static const wxWindowID ID_CHANGESLOGPATH_TEXTCTRL;
		static const wxWindowID ID_CHANGESLOGPATH_BUTTON;
		static const wxWindowID ID_FORMAT_STATICTEXT;
		static const wxWindowID ID_CHANGESTITLE_TEXTCTRL;
		static const wxWindowID ID_FORMATS_STATICTEXT;
		static const wxWindowID ID_CHANGES_PANEL;
		static const wxWindowID ID_AV_NOTEBOOK;
		static const wxWindowID ID_STATICTEXT2;
		static const wxWindowID ID_ACCEPT;
		static const wxWindowID ID_CANCEL;
		static const wxWindowID ID_VALIDATE_TIMER;
		//*)

	protected:

		//(*Handlers(avVersionEditorDlg)
		void OnAcceptClick(wxCommandEvent& event);
		void OnCancelClick(wxCommandEvent& event);
		void OnSvnCheck(wxCommandEvent& event);
		void OnSvnDirectoryClick(wxCommandEvent& event);
		void OnChkCommitClick(wxCommandEvent& event);
		void OnChoStatusSelect(wxCommandEvent& event);
		void OnChoAbbreviationSelect(wxCommandEvent& event);
		void OnCmbStatusSelect(wxCommandEvent& event);
		void OnCmbAbbreviationSelect(wxCommandEvent& event);
		void OnChkChangesClick(wxCommandEvent& event);
		void OnnbAutoVersioningPageChanged(wxNotebookEvent& event);
		void OnTxtRevisionRandomText(wxCommandEvent& event);
		void OnTmrValidateInputTrigger(wxTimerEvent& event);
		void OnTextChanged(wxCommandEvent& event);
		void OnMouseEnter(wxMouseEvent& event);
		void OnHeaderPathClick(wxCommandEvent& event);
		void OnChangesLogPathClick(wxCommandEvent& event);
		//*)

		//(*Declarations(avVersionEditorDlg)
		wxBoxSizer* BoxSizer12;
		wxBoxSizer* BoxSizer19;
		wxBoxSizer* BoxSizer1;
		wxBoxSizer* BoxSizer2;
		wxBoxSizer* BoxSizer3;
		wxBoxSizer* BoxSizer4;
		wxBoxSizer* BoxSizer5;
		wxBoxSizer* BoxSizer6;
		wxBoxSizer* BoxSizer7;
		wxBoxSizer* BoxSizer8;
		wxBoxSizer* BoxSizer9;
		wxBoxSizer* buildNumberMaxSizer;
		wxBoxSizer* buttonsSizer;
		wxBoxSizer* changesSizer;
		wxBoxSizer* mainSizer;
		wxBoxSizer* minorMaxSizer;
		wxBoxSizer* schemeSizer;
		wxBoxSizer* settingsSizer;
		wxBoxSizer* statusSizer;
		wxBoxSizer* svnSizer;
		wxBoxSizer* valuesSizer;
		wxButton* btnAccept;
		wxButton* btnCancel;
		wxButton* btnChangesLogPath;
		wxButton* btnHeaderPath;
		wxButton* btnSvnDir;
		wxCheckBox* chkAskCommit;
		wxCheckBox* chkAutoIncrement;
		wxCheckBox* chkChanges;
		wxCheckBox* chkCommit;
		wxCheckBox* chkDates;
		wxCheckBox* chkDefine;
		wxCheckBox* chkSvn;
		wxCheckBox* chkUpdateManifest;
		wxComboBox* cmbAbbreviation;
		wxComboBox* cmbStatus;
		wxNotebook* nbAutoVersioning;
		wxPanel* pnlChanges;
		wxPanel* pnlCode;
		wxPanel* pnlScheme;
		wxPanel* pnlSettings;
		wxPanel* pnlStatus;
		wxPanel* pnlVersionValues;
		wxRadioBox* rbHeaderLanguage;
		wxStaticLine* StaticLine1;
		wxStaticLine* StaticLine2;
		wxStaticLine* StaticLine3;
		wxStaticLine* StaticLine4;
		wxStaticText* StaticText1;
		wxStaticText* lblBuild;
		wxStaticText* lblBuildNumberMaximun;
		wxStaticText* lblBuildTimes;
		wxStaticText* lblChangesFormats;
		wxStaticText* lblChangesPath;
		wxStaticText* lblChangesTitle;
		wxStaticText* lblCount;
		wxStaticText* lblCurrentProject;
		wxStaticText* lblHeaderGuard;
		wxStaticText* lblHeaderPath;
		wxStaticText* lblMajor;
		wxStaticText* lblMinor;
		wxStaticText* lblMinorMaximum;
		wxStaticText* lblNamespace;
		wxStaticText* lblPrefix;
		wxStaticText* lblRevision;
		wxStaticText* lblRevisionMax;
		wxStaticText* lblRevisionRandom;
		wxStaticText* lblStatus;
		wxTextCtrl* txtBuildCount;
		wxTextCtrl* txtBuildNumber;
		wxTextCtrl* txtBuildNumberMaximun;
		wxTextCtrl* txtBuildTimes;
		wxTextCtrl* txtChangesLogPath;
		wxTextCtrl* txtChangesTitle;
		wxTextCtrl* txtHeaderGuard;
		wxTextCtrl* txtHeaderPath;
		wxTextCtrl* txtMajorVersion;
		wxTextCtrl* txtMinorMaximun;
		wxTextCtrl* txtMinorVersion;
		wxTextCtrl* txtNameSpace;
		wxTextCtrl* txtPrefix;
		wxTextCtrl* txtRevisionMax;
		wxTextCtrl* txtRevisionNumber;
		wxTextCtrl* txtRevisionRandom;
		wxTextCtrl* txtSvnDir;
		wxTimer tmrValidateInput;
		//*)

public:
	void SetCurrentProject(const wxString& projectName);

	void SetMajor(long value);
	void SetMinor(long value);
	void SetBuild(long value);
	void SetRevision(long value);
	void SetCount(long value);

	void SetStatus(const wxString& value);
	void SetStatusAbbreviation(const wxString& value);

    long GetMajor() const {return m_major;}
    long GetMinor() const {return m_minor;}
    long GetBuild() const {return m_build;}
    long GetRevision() const {return m_revision;}
    long GetCount() const {return m_count;}

	wxString GetStatus() const {return m_status;}
	wxString GetStatusAbbreviation() const {return m_statusAbbreviation;}


	void SetSvn(bool value);
	void SetSvnDirectory(const wxString& value);
	void SetAuto(bool value);
	void SetDates(bool value);
	void SetDefine(bool value);
	// GJH 03/03/10 Added manifest updating.
	void SetManifest(bool value);
	void SetCommit(bool value);
	void SetCommitAsk(bool value);
	void SetLanguage(const wxString& value);
	void SetHeaderPath(const wxString& value);

	void SetMinorMaximum(long value);
	void SetBuildMaximum(long value);
	void SetRevisionMaximum(long value);
	void SetRevisionRandomMaximum(long value);
	void SetBuildTimesToMinorIncrement(long value);

	void SetChanges(bool value);
	void SetChangesLogPath(const wxString& value);
	void SetChangesTitle(const wxString& value);

	bool GetSvn() const {return m_svn;}
	wxString GetSvnDirectory() const {return m_svnDirectory;}
	bool GetAuto() const {return m_autoMajorMinor;}
	bool GetDates() const {return m_dates;}
	bool GetDefine() const {return m_useDefine;}
	// GJH 03/03/10 Added manifest updating.
	bool GetManifest() const {return m_updateManifest;}
	bool GetCommit() const {return m_commit;}
	bool GetCommitAsk() const {return m_askCommit;}
	wxString GetLanguage() const {return m_language;}
	wxString GetHeaderPath() const {return m_headerPath;}

	long GetMinorMaximum() const {return m_minorMaximun;}
	long GetBuildMaximum() const {return m_buildMaximun;}
	long GetRevisionMaximum() const {return m_revisionMaximun;}
	long GetRevisionRandomMaximum() const {return m_revisionRandomMaximun;}
	long GetBuildTimesToMinorIncrement() const {return m_buildTimesToMinorIncrement;}

	bool GetChanges() const {return m_changes;}
	wxString GetChangesLogPath() const {return m_changesLogPath;}
	wxString GetChangesTitle() const {return m_changesTitle;}

	void SetHeaderGuard(const wxString& value);
	void SetNamespace(const wxString& value);
	void SetPrefix(const wxString& value);

	wxString GetHeaderGuard() const {return m_headerGuard;}
	wxString GetNamespace() const {return m_namespace;}
	wxString GetPrefix() const {return m_prefix;}


};

#endif
