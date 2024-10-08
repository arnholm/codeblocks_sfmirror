#ifndef ADDR2LINEUIMAIN_H
#define ADDR2LINEUIMAIN_H

//(*Headers(Addr2LineUIDialog)
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include <wx/arrstr.h>
#include <wx/fileconf.h>
#include <wx/string.h>

class Addr2LineUIDialog: public wxDialog
{
public:

  Addr2LineUIDialog(wxWindow* parent);
  virtual ~Addr2LineUIDialog();

private:

  //(*Handlers(Addr2LineUIDialog)
  void OnQuit(wxCommandEvent& event);
  void OnCrashLogFile(wxFileDirPickerEvent& event);
  void OnAddr2LineFile(wxFileDirPickerEvent& event);
  void OnDirPrependDir(wxFileDirPickerEvent& event);
  void OnOperateClick(wxCommandEvent& event);
  void OnReplaceClick(wxCommandEvent& event);
  //*)

  //(*Identifiers(Addr2LineUIDialog)
  static const wxWindowID ID_CRASH_LOG;
  static const wxWindowID ID_ADDR2LINE;
  static const wxWindowID ID_DIR_PREPEND;
  static const wxWindowID ID_CHK_REPLACE;
  static const wxWindowID ID_TXT_REPLACE_THIS;
  static const wxWindowID ID_LBL_REPLACE;
  static const wxWindowID ID_TXT_REPLACE_THAT;
  static const wxWindowID ID_CHK_SKIP_UNRESOLVABLE;
  static const wxWindowID ID_TXT_CRASH_LOG_CONTENT;
  static const wxWindowID ID_TXT_RESULT;
  static const wxWindowID ID_BTN_OPERATE;
  static const wxWindowID ID_BTN_QUIT;
  //*)

  //(*Declarations(Addr2LineUIDialog)
  wxButton* btnOperate;
  wxCheckBox* chkReplace;
  wxCheckBox* chkSkipUnresolvable;
  wxDirPickerCtrl* m_DPDirPrepend;
  wxFilePickerCtrl* m_FPAddr2Line;
  wxFilePickerCtrl* m_FPCrashLog;
  wxStaticText* lblReplace;
  wxTextCtrl* txtCrashLogContent;
  wxTextCtrl* txtReplaceThat;
  wxTextCtrl* txtReplaceThis;
  wxTextCtrl* txtResult;
  //*)

  wxFileConfig  mFileConfig;
  wxString      mCrashLog;
  wxArrayString mCrashLogFileContent;
  wxString      mAddr2Line;
  wxString      mDirPrepend;

  DECLARE_EVENT_TABLE()
};

#endif // ADDR2LINEUIMAIN_H
