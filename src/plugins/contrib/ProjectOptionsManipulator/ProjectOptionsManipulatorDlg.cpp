/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "ProjectOptionsManipulatorDlg.h"

//(*InternalHeaders(ProjectOptionsManipulatorDlg)
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/intl.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
//*)

//(*IdInit(ProjectOptionsManipulatorDlg)
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHO_SCAN = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHO_SCAN_PROJECTS = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_RBO_OPERATION = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHO_OPTION_LEVEL = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_TXT_OPTION_SEARCH = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_BTN_SEARCH_COMPILER_SRC = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::TD_TXT_OPTION_REPLACE = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_BTN_SEARCH_COMPILER_DEST = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTION_REPLACE_PATTERN = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_RBO_OPTION_SEARCH = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_COMPILER = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_LINKER = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_RES_COMPILER = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_COMPILER_PATH = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_LINKER_PATH = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_RES_COMP_PATH = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_LINKER_LIBS = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHK_OPTIONS_CUSTOM_VAR = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_TXT_CUSTOM_VAR = wxNewId();
const wxWindowID ProjectOptionsManipulatorDlg::ID_CHO_TARGET_TYPE = wxNewId();
//*)

#include <wx/choicdlg.h>

#include <cbproject.h>
#include <compiler.h>
#include <compilerfactory.h>
#include <globals.h> // cbMessageBox
#include <manager.h>
#include <projectmanager.h>

BEGIN_EVENT_TABLE(ProjectOptionsManipulatorDlg,wxDialog)
	//(*EventTable(ProjectOptionsManipulatorDlg)
	//*)
	EVT_BUTTON(wxID_OK,ProjectOptionsManipulatorDlg::OnOk)
END_EVENT_TABLE()

ProjectOptionsManipulatorDlg::ProjectOptionsManipulatorDlg(wxWindow* parent,wxWindowID id)
{
	//(*Initialize(ProjectOptionsManipulatorDlg)
	wxBoxSizer* bszCustomVar;
	wxBoxSizer* bszMainH;
	wxBoxSizer* bszOperation;
	wxBoxSizer* bszScan;
	wxFlexGridSizer* flsOptions;
	wxFlexGridSizer* flsReplace;
	wxFlexGridSizer* flsSearch;
	wxStaticBoxSizer* sbsItem;
	wxStaticBoxSizer* sbsScope;
	wxStaticText* lblOptionReplace;
	wxStaticText* lblOptionSearch;
	wxStaticText* lblOptionsLevel;
	wxStaticText* lblScanColon;
	wxStaticText* lblScanWithin;
	wxStdDialogButtonSizer* sbzOKCancel;

	Create(parent, id, _("Project Options Plugin"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER, _T("id"));
	bszMainH = new wxBoxSizer(wxVERTICAL);
	bszScan = new wxBoxSizer(wxHORIZONTAL);
	lblScanWithin = new wxStaticText(this, wxID_ANY, _("Scan within:"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	bszScan->Add(lblScanWithin, 0, wxALIGN_CENTER_VERTICAL, 5);
	m_ChoScan = new wxChoice(this, ID_CHO_SCAN, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHO_SCAN"));
	m_ChoScan->SetSelection( m_ChoScan->Append(_("Workspace")) );
	m_ChoScan->Append(_("Project"));
	bszScan->Add(m_ChoScan, 0, wxLEFT|wxEXPAND, 5);
	lblScanColon = new wxStaticText(this, wxID_ANY, _T(":"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	bszScan->Add(lblScanColon, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5);
	m_ChoScanProjects = new wxChoice(this, ID_CHO_SCAN_PROJECTS, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHO_SCAN_PROJECTS"));
	m_ChoScanProjects->Disable();
	bszScan->Add(m_ChoScanProjects, 1, wxLEFT|wxEXPAND, 5);
	bszMainH->Add(bszScan, 0, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	flsOptions = new wxFlexGridSizer(0, 3, 0, 0);
	flsOptions->AddGrowableCol(1);
	bszOperation = new wxBoxSizer(wxVERTICAL);
	wxString __wxRadioBoxChoices_1[7] =
	{
	    _("Search for option present"),
	    _("Search for option NOT present"),
	    _("Remove option"),
	    _("Add option"),
	    _("Change option"),
	    _("Remove files w/o target"),
	    _("Replace compiler")
	};
	m_RboOperation = new wxRadioBox(this, ID_RBO_OPERATION, _("Operation:"), wxDefaultPosition, wxDefaultSize, 7, __wxRadioBoxChoices_1, 1, wxRA_SPECIFY_COLS, wxDefaultValidator, _T("ID_RBO_OPERATION"));
	m_RboOperation->SetSelection(0);
	bszOperation->Add(m_RboOperation, 0, wxEXPAND, 5);
	lblOptionsLevel = new wxStaticText(this, wxID_ANY, _("Search for option:"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	bszOperation->Add(lblOptionsLevel, 0, wxTOP|wxEXPAND, 5);
	m_ChoOptionLevel = new wxChoice(this, ID_CHO_OPTION_LEVEL, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHO_OPTION_LEVEL"));
	m_ChoOptionLevel->Append(_("At project level"));
	m_ChoOptionLevel->Append(_("At target level"));
	m_ChoOptionLevel->SetSelection( m_ChoOptionLevel->Append(_("At project and target level")) );
	bszOperation->Add(m_ChoOptionLevel, 0, wxEXPAND, 5);
	flsOptions->Add(bszOperation, 0, wxEXPAND, 5);
	sbsItem = new wxStaticBoxSizer(wxVERTICAL, this, _("Search item:"));
	lblOptionSearch = new wxStaticText(this, wxID_ANY, _("Option/Var (i.e. -Wl,--no-undefined):"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	sbsItem->Add(lblOptionSearch, 0, wxLEFT|wxRIGHT|wxEXPAND, 5);
	flsSearch = new wxFlexGridSizer(0, 2, 0, 0);
	flsSearch->AddGrowableCol(0);
	m_TxtOptionSearch = new wxTextCtrl(this, ID_TXT_OPTION_SEARCH, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TXT_OPTION_SEARCH"));
	m_TxtOptionSearch->SetToolTip(_("This is the compiler/linker option or path, linker lib or custom var to search for..."));
	flsSearch->Add(m_TxtOptionSearch, 1, wxEXPAND, 5);
	m_BtnSearchCompilerSrc = new wxButton(this, ID_BTN_SEARCH_COMPILER_SRC, _T("..."), wxDefaultPosition, wxSize(23,23), 0, wxDefaultValidator, _T("ID_BTN_SEARCH_COMPILER_SRC"));
	m_BtnSearchCompilerSrc->Disable();
	m_BtnSearchCompilerSrc->SetToolTip(_("Select compiler to search for"));
	flsSearch->Add(m_BtnSearchCompilerSrc, 0, wxLEFT|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	sbsItem->Add(flsSearch, 0, wxLEFT|wxRIGHT|wxEXPAND, 5);
	lblOptionReplace = new wxStaticText(this, wxID_ANY, _("Replace with:"), wxDefaultPosition, wxDefaultSize, 0, _T("wxID_ANY"));
	sbsItem->Add(lblOptionReplace, 0, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	flsReplace = new wxFlexGridSizer(0, 2, 0, 0);
	flsReplace->AddGrowableCol(0);
	m_TxtOptionReplace = new wxTextCtrl(this, TD_TXT_OPTION_REPLACE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("TD_TXT_OPTION_REPLACE"));
	m_TxtOptionReplace->Disable();
	flsReplace->Add(m_TxtOptionReplace, 1, wxEXPAND, 5);
	m_BtnSearchCompilerDest = new wxButton(this, ID_BTN_SEARCH_COMPILER_DEST, _T("..."), wxDefaultPosition, wxSize(23,23), 0, wxDefaultValidator, _T("ID_BTN_SEARCH_COMPILER_DEST"));
	m_BtnSearchCompilerDest->Disable();
	m_BtnSearchCompilerDest->SetToolTip(_("Select compiler to replace with"));
	flsReplace->Add(m_BtnSearchCompilerDest, 0, wxLEFT|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	sbsItem->Add(flsReplace, 0, wxLEFT|wxRIGHT|wxEXPAND, 5);
	m_ChkOptionReplacePattern = new wxCheckBox(this, ID_CHK_OPTION_REPLACE_PATTERN, _("Replace only search pattern"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTION_REPLACE_PATTERN"));
	m_ChkOptionReplacePattern->SetValue(false);
	m_ChkOptionReplacePattern->Disable();
	m_ChkOptionReplacePattern->SetToolTip(_("Either only the search pattern (strin) is replaced within the option\nor the whole options is replaced with the string to replace with."));
	sbsItem->Add(m_ChkOptionReplacePattern, 0, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	wxString __wxRadioBoxChoices_2[2] =
	{
	    _("Search for \"equals option\""),
	    _("Search for \"contains option\"")
	};
	m_RboOptionSearch = new wxRadioBox(this, ID_RBO_OPTION_SEARCH, _("When searching..."), wxDefaultPosition, wxDefaultSize, 2, __wxRadioBoxChoices_2, 1, 0, wxDefaultValidator, _T("ID_RBO_OPTION_SEARCH"));
	m_RboOptionSearch->SetSelection(0);
	sbsItem->Add(m_RboOptionSearch, 0, wxTOP|wxEXPAND, 5);
	flsOptions->Add(sbsItem, 1, wxLEFT|wxEXPAND, 5);
	sbsScope = new wxStaticBoxSizer(wxVERTICAL, this, _("Scope:"));
	m_ChkOptionsCompiler = new wxCheckBox(this, ID_CHK_OPTIONS_COMPILER, _("Compiler options"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_COMPILER"));
	m_ChkOptionsCompiler->SetValue(true);
	sbsScope->Add(m_ChkOptionsCompiler, 0, wxALIGN_LEFT, 5);
	m_ChkOptionsLinker = new wxCheckBox(this, ID_CHK_OPTIONS_LINKER, _("Linker options"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_LINKER"));
	m_ChkOptionsLinker->SetValue(false);
	sbsScope->Add(m_ChkOptionsLinker, 0, wxTOP|wxALIGN_LEFT, 5);
	m_ChkOptionsResCompiler = new wxCheckBox(this, ID_CHK_OPTIONS_RES_COMPILER, _("Resource compiler options"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_RES_COMPILER"));
	m_ChkOptionsResCompiler->SetValue(false);
	sbsScope->Add(m_ChkOptionsResCompiler, 0, wxTOP|wxALIGN_LEFT, 5);
	m_ChkOptionsCompilerPath = new wxCheckBox(this, ID_CHK_OPTIONS_COMPILER_PATH, _("Compiler path\'s"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_COMPILER_PATH"));
	m_ChkOptionsCompilerPath->SetValue(false);
	sbsScope->Add(m_ChkOptionsCompilerPath, 0, wxTOP|wxALIGN_LEFT, 5);
	m_ChkOptionsLinkerPath = new wxCheckBox(this, ID_CHK_OPTIONS_LINKER_PATH, _("Linker path\'s"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_LINKER_PATH"));
	m_ChkOptionsLinkerPath->SetValue(false);
	sbsScope->Add(m_ChkOptionsLinkerPath, 0, wxTOP|wxALIGN_LEFT, 5);
	m_ChkOptionsResCompPath = new wxCheckBox(this, ID_CHK_OPTIONS_RES_COMP_PATH, _("Resource compiler path\'s"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_RES_COMP_PATH"));
	m_ChkOptionsResCompPath->SetValue(false);
	sbsScope->Add(m_ChkOptionsResCompPath, 0, wxTOP|wxALIGN_LEFT, 5);
	m_ChkOptionsLinkerLibs = new wxCheckBox(this, ID_CHK_OPTIONS_LINKER_LIBS, _("Linker libs"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_LINKER_LIBS"));
	m_ChkOptionsLinkerLibs->SetValue(false);
	sbsScope->Add(m_ChkOptionsLinkerLibs, 0, wxTOP|wxALIGN_LEFT, 5);
	bszCustomVar = new wxBoxSizer(wxHORIZONTAL);
	m_ChkOptionsCustomVar = new wxCheckBox(this, ID_CHK_OPTIONS_CUSTOM_VAR, _("Custom var ="), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHK_OPTIONS_CUSTOM_VAR"));
	m_ChkOptionsCustomVar->SetValue(false);
	bszCustomVar->Add(m_ChkOptionsCustomVar, 0, wxALIGN_CENTER_VERTICAL, 5);
	m_TxtCustomVar = new wxTextCtrl(this, ID_TXT_CUSTOM_VAR, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TXT_CUSTOM_VAR"));
	m_TxtCustomVar->SetToolTip(_("This is the value to set for the custom var, if \"add option\" is chosen"));
	bszCustomVar->Add(m_TxtCustomVar, 1, wxLEFT|wxALIGN_CENTER_VERTICAL, 5);
	sbsScope->Add(bszCustomVar, 0, wxEXPAND, 5);
	m_ChoTargetType = new wxChoice(this, ID_CHO_TARGET_TYPE, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHO_TARGET_TYPE"));
	m_ChoTargetType->SetSelection( m_ChoTargetType->Append(_("All type of targets")) );
	m_ChoTargetType->Append(_("Application targets only"));
	m_ChoTargetType->Append(_("Static library targets only"));
	m_ChoTargetType->Append(_("Dynamic library targets only"));
	sbsScope->Add(m_ChoTargetType, 0, wxTOP|wxEXPAND, 5);
	flsOptions->Add(sbsScope, 0, wxLEFT|wxEXPAND, 5);
	bszMainH->Add(flsOptions, 1, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5);
	sbzOKCancel = new wxStdDialogButtonSizer();
	sbzOKCancel->AddButton(new wxButton(this, wxID_OK, wxEmptyString));
	sbzOKCancel->AddButton(new wxButton(this, wxID_CANCEL, wxEmptyString));
	sbzOKCancel->Realize();
	dynamic_cast <wxButton *> (this->FindWindow(wxID_OK))->SetDefault();
	bszMainH->Add(sbzOKCancel, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5);
	SetSizer(bszMainH);
	bszMainH->SetSizeHints(this);
	Center();

	Connect(ID_CHO_SCAN, wxEVT_COMMAND_CHOICE_SELECTED, (wxObjectEventFunction)&ProjectOptionsManipulatorDlg::OnScanSelect);
	Connect(ID_RBO_OPERATION, wxEVT_COMMAND_RADIOBOX_SELECTED, (wxObjectEventFunction)&ProjectOptionsManipulatorDlg::OnOperationSelect);
	Connect(ID_BTN_SEARCH_COMPILER_SRC, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ProjectOptionsManipulatorDlg::OnSearchCompilerClick);
	Connect(ID_BTN_SEARCH_COMPILER_DEST, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ProjectOptionsManipulatorDlg::OnSearchCompilerClick);
	Connect(ID_CHO_TARGET_TYPE, wxEVT_COMMAND_CHOICE_SELECTED, (wxObjectEventFunction)&ProjectOptionsManipulatorDlg::OnTargetTypeSelect);
	//*)
}

ProjectOptionsManipulatorDlg::~ProjectOptionsManipulatorDlg()
{
	//(*Destroy(ProjectOptionsManipulatorDlg)
	//*)
}

bool ProjectOptionsManipulatorDlg::GetScanForWorkspace()
{
  return ( m_ChoScan->GetSelection()==0 );
}

bool ProjectOptionsManipulatorDlg::GetScanForProject()
{
  return ( m_ChoScan->GetSelection()==1 );
}

size_t ProjectOptionsManipulatorDlg::GetProjectIdx()
{
  return ( GetScanForProject() ? m_ChoScanProjects->GetSelection() : -1 );
}

ProjectOptionsManipulatorDlg::EProjectScanOption ProjectOptionsManipulatorDlg::GetScanOption()
{
  switch ( m_RboOperation->GetSelection() )
  {
    case 0: { return eSearch;         } break;
    case 1: { return eSearchNot;      } break;
    case 2: { return eRemove;         } break;
    case 3: { return eAdd;            } break;
    case 4: { return eReplace;        } break;
    case 5: { return eFiles;          } break;
    case 6: { return eChangeCompiler; } break;
    default:                       break;
  }
  return eSearch; // should never happen, but is safe to do
}

ProjectOptionsManipulatorDlg::EProjectSearchOption ProjectOptionsManipulatorDlg::GetSearchOption()
{
  switch ( m_RboOptionSearch->GetSelection() )
  {
    case 0: { return eEquals;   } break;
    case 1: { return eContains; } break;
    default:                      break;
  }
  return eEquals; // should never happen, but is safe to do
}

wxString ProjectOptionsManipulatorDlg::GetSearchFor()
{
  return ( m_TxtOptionSearch->GetValue() );
}

wxString ProjectOptionsManipulatorDlg::GetReplaceWith()
{
  return ( m_TxtOptionReplace->GetValue() );
}

wxString ProjectOptionsManipulatorDlg::GetCustomVarValue()
{
  return ( m_TxtCustomVar->GetValue().Trim(true).Trim(false) );
}

bool ProjectOptionsManipulatorDlg::GetOptionActive(EProjectOption opt)
{
  if ( m_ChkOptionReplacePattern->GetValue() && (opt==eReplacePattern) ) return true;
  if ( m_ChkOptionsCompiler->GetValue()      && (opt==eCompiler)       ) return true;
  if ( m_ChkOptionsLinker->GetValue()        && (opt==eLinker)         ) return true;
  if ( m_ChkOptionsResCompiler->GetValue()   && (opt==eResCompiler)    ) return true;
  if ( m_ChkOptionsCompilerPath->GetValue()  && (opt==eCompilerPaths)  ) return true;
  if ( m_ChkOptionsLinkerPath->GetValue()    && (opt==eLinkerPaths)    ) return true;
  if ( m_ChkOptionsResCompPath->GetValue()   && (opt==eResCompPaths)   ) return true;
  if ( m_ChkOptionsLinkerLibs->GetValue()    && (opt==eLinkerLibs)     ) return true;
  if ( m_ChkOptionsCustomVar->GetValue()     && (opt==eCustomVars)     ) return true;
  return false;
}

bool ProjectOptionsManipulatorDlg::GetOptionActive(EProjectLevelOption opt)
{
  if ( m_ChoOptionLevel->GetSelection()==0 && (opt==eProject) ) return true;
  if ( m_ChoOptionLevel->GetSelection()==1 && (opt==eTarget)  ) return true;
  if ( m_ChoOptionLevel->GetSelection()==2 )                    return true;
  return false;
}

ProjectOptionsManipulatorDlg::EProjectTargetTypeOption ProjectOptionsManipulatorDlg::GetTargetTypeOption()
{
  switch ( m_ChoTargetType->GetSelection() )
  {
    case 0: { return eAll;         } break;
    case 1: { return eApplication; } break;
    case 2: { return eStaticLib;   } break;
    case 3: { return eDynamicLib;  } break;
    default:                         break;
  }
  return eAll; // should never happen, but is safe to do
}

void ProjectOptionsManipulatorDlg::OnOperationSelect(wxCommandEvent& event)
{
  if ( event.GetInt()==5 ) // Files w/o target
  {
    // Disable all
    m_ChoOptionLevel->Disable();  // first row
    m_RboOptionSearch->Disable(); // second row
    // third row:
    m_TxtOptionSearch->Disable();
    m_BtnSearchCompilerSrc->Disable();
    m_TxtOptionReplace->Disable();
    m_BtnSearchCompilerDest->Disable();
    m_ChkOptionReplacePattern->Disable();
    m_ChkOptionsCompiler->Disable();
    m_ChkOptionsLinker->Disable();
    m_ChkOptionsResCompiler->Disable();
    m_ChkOptionsCompilerPath->Disable();
    m_ChkOptionsLinkerPath->Disable();
    m_ChkOptionsResCompPath->Disable();
    m_ChkOptionsLinkerLibs->Disable();
    m_ChkOptionsCustomVar->Disable();
    m_TxtCustomVar->Disable();
    m_ChoTargetType->Disable();
  }
  else
  {
    // Enable all
    m_ChoOptionLevel->Enable();  // first row
    m_RboOptionSearch->Enable(); // second row
    // third row:
    m_TxtOptionSearch->Enable();
    m_ChkOptionsCompiler->Enable();
    m_ChkOptionsLinker->Enable();
    m_ChkOptionsResCompiler->Enable();
    m_ChkOptionsCompilerPath->Enable();
    m_ChkOptionsLinkerPath->Enable();
    m_ChkOptionsResCompPath->Enable();
    m_ChkOptionsLinkerLibs->Enable();
    m_ChoTargetType->Enable();

    // Only used for add/change:
    m_ChkOptionsCustomVar->Disable();
    m_TxtCustomVar->Disable();

    // Only used for change:
    m_TxtOptionReplace->Disable();
    m_ChkOptionReplacePattern->Disable();

    // Only used for compiler:
    m_BtnSearchCompilerSrc->Disable();
    m_BtnSearchCompilerDest->Disable();

    if ( (event.GetInt()==3) || (event.GetInt()==4) ) // Add or Change (Replace)
    {
      m_ChkOptionsCustomVar->Enable();
      m_TxtCustomVar->Enable();
    }

    if ( event.GetInt()==4 ) // Change option
    {
      m_TxtOptionReplace->Enable();
      m_ChkOptionReplacePattern->Enable();
    }

    if ( event.GetInt()==6 ) // Replace compiler
    {
      // Only used for compiler:
      m_BtnSearchCompilerSrc->Enable();
      m_TxtOptionReplace->Enable();
      m_BtnSearchCompilerDest->Enable();

      m_RboOptionSearch->Disable(); // second row
      // third row:
      m_ChkOptionsCompiler->Disable();
      m_ChkOptionsLinker->Disable();
      m_ChkOptionsResCompiler->Disable();
      m_ChkOptionsCompilerPath->Disable();
      m_ChkOptionsLinkerPath->Disable();
      m_ChkOptionsResCompPath->Disable();
      m_ChkOptionsLinkerLibs->Disable();
      m_ChkOptionsCustomVar->Disable();
      m_TxtCustomVar->Disable();
    }
  }
}

void ProjectOptionsManipulatorDlg::OnSearchCompilerClick(wxCommandEvent& event)
{
   wxArrayString compiler_array;
   for (size_t i=0; i<CompilerFactory::GetCompilersCount(); i++)
   {
     Compiler* compiler = CompilerFactory::GetCompiler(i);
     if (compiler)
       compiler_array.Add(compiler->GetName());
   }

   wxSingleChoiceDialog choose_dlg(this, _T("Available compilers"), _T("Choose a new compiler"), compiler_array);
   PlaceWindow(&choose_dlg);
   if (choose_dlg.ShowModal() == wxID_OK)
   {
     if      (event.GetId() == ID_BTN_SEARCH_COMPILER_SRC)
       m_TxtOptionSearch->SetValue(compiler_array[choose_dlg.GetSelection()]);
     else if (event.GetId() == ID_BTN_SEARCH_COMPILER_DEST)
       m_TxtOptionReplace->SetValue(compiler_array[choose_dlg.GetSelection()]);
   }
}

void ProjectOptionsManipulatorDlg::OnScanSelect(wxCommandEvent& event)
{
  m_ChoScanProjects->Clear();
  if ( event.GetInt()==1 ) // project
  {
    ProjectsArray* pa = Manager::Get()->GetProjectManager()->GetProjects();
    if (pa)
    {
      for (size_t i=0; i<pa->GetCount(); ++i)
      {
        cbProject* prj = pa->Item(i);
        if (prj)
          m_ChoScanProjects->Append( prj->GetTitle() );
      }

      if ( pa->GetCount() )
        m_ChoScanProjects->SetSelection(0);
    }
    m_ChoScanProjects->Enable();
  }
  else
    m_ChoScanProjects->Disable();
}

void ProjectOptionsManipulatorDlg::OnTargetTypeSelect(wxCommandEvent& event)
{
  if ( GetOptionActive(eProject) && (event.GetInt()!=0) )
  {
    cbMessageBox(_("Warning: You have selected to operate projects but limit target types.\n"
                   "Although this maybe intended, the limitation is only applied to targets\n"
                   "as projects don't have/define output files."), _("Warning"), wxICON_WARNING, this);
  }
}

void ProjectOptionsManipulatorDlg::OnOk(wxCommandEvent& WXUNUSED(event))
{
  if ( GetScanOption()==eFiles ) // No need to validate any further options
  {
    EndModal(wxID_OK);
    return;
  }

  // Validate that at least an option is provided
  if ( m_TxtOptionSearch->GetValue().Trim().IsEmpty() )
  {
    cbMessageBox(_("You need to provide an option to search for!"), _("Error"), wxICON_ERROR, this);
    return;
  }

  // Validate that at least a search option is selected
  if (   !m_ChkOptionsCompiler->GetValue()
      && !m_ChkOptionsLinker->GetValue()
      && !m_ChkOptionsResCompiler->GetValue()
      && !m_ChkOptionsCompilerPath->GetValue()
      && !m_ChkOptionsLinkerPath->GetValue()
      && !m_ChkOptionsResCompPath->GetValue()
      && !m_ChkOptionsLinkerLibs->GetValue()
      && !m_ChkOptionsCustomVar->GetValue() )
  {
    cbMessageBox(_("You need to select at least one option to search in!"), _("Error"), wxICON_ERROR, this);
    return;
  }

  EndModal(wxID_OK);
}
