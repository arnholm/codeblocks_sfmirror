/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"

#ifndef CB_PRECOMP
  #include <wx/button.h>
  #include <wx/checkbox.h>
  #include <wx/combobox.h>
  #include <wx/event.h>
  #include <wx/file.h>
  #include <wx/filename.h>
  #include <wx/intl.h>
  #include <wx/listbox.h>
  #include <wx/textctrl.h>
  #include <wx/xrc/xmlres.h>

  #include "cbeditor.h"
  #include "cbproject.h"
  #include "configmanager.h"
  #include "editormanager.h"
  #include "globals.h"
  #include "manager.h"
  #include "macrosmanager.h"
  #include "projectmanager.h"
#endif
#include "cbstyledtextctrl.h"

#include <wx/tokenzr.h>
#include <wx/dirdlg.h>
#include "classwizarddlg.h"

BEGIN_EVENT_TABLE(ClassWizardDlg, wxScrollingDialog)
    EVT_UPDATE_UI(-1,                          ClassWizardDlg::OnUpdateUI)
    EVT_TEXT     (XRCID("txtName"),            ClassWizardDlg::OnNameChange)
    EVT_TEXT     (XRCID("txtInheritance"),     ClassWizardDlg::OnAncestorChange)
    EVT_BUTTON   (XRCID("btnAddMemberVar"),    ClassWizardDlg::OnAddMemberVar)
    EVT_BUTTON   (XRCID("btnRemoveMemberVar"), ClassWizardDlg::OnRemoveMemberVar)
    EVT_BUTTON   (XRCID("btnCommonDir"),       ClassWizardDlg::OnCommonDirClick)
    EVT_CHECKBOX (XRCID("chkLowerCase"),       ClassWizardDlg::OnLowerCaseClick)
    EVT_CHECKBOX (XRCID("chkAddPathToProject"),ClassWizardDlg::OnAddPathToProjectClick)
    EVT_BUTTON   (XRCID("btnIncludeDir"),      ClassWizardDlg::OnIncludeDirClick)
    EVT_BUTTON   (XRCID("btnImplDir"),         ClassWizardDlg::OnImplDirClick)
    EVT_TEXT     (XRCID("txtHeader"),          ClassWizardDlg::OnHeaderChange)
    EVT_BUTTON   (XRCID("wxID_OK"),            ClassWizardDlg::OnOKClick)
    EVT_BUTTON   (XRCID("wxID_CANCEL"),        ClassWizardDlg::OnCancelClick)
END_EVENT_TABLE()

// ---------
// livecycle
// ---------

ClassWizardDlg::ClassWizardDlg(wxWindow* parent)
{
    wxXmlResource::Get()->LoadObject(this, parent, "dlgNewClass", "wxScrollingDialog");

    ProjectManager* prjMan = Manager::Get()->GetProjectManager();
    cbProject* prj = prjMan->GetActiveProject();
    if (prj)
    {
        XRCCTRL(*this, "txtIncludeDir", wxTextCtrl)->SetValue(prj->GetCommonTopLevelPath() + "include");
        XRCCTRL(*this, "txtImplDir", wxTextCtrl)->SetValue(prj->GetCommonTopLevelPath() + "src");
        XRCCTRL(*this, "txtCommonDir", wxTextCtrl)->SetValue(prj->GetCommonTopLevelPath());
    }
    else
    {
        XRCCTRL(*this, "txtIncludeDir", wxTextCtrl)->SetValue(::wxGetCwd());
        XRCCTRL(*this, "txtImplDir", wxTextCtrl)->SetValue(::wxGetCwd());
        XRCCTRL(*this, "txtCommonDir", wxTextCtrl)->SetValue(::wxGetCwd());
    }
    XRCCTRL(*this, "txtInheritanceFilename", wxTextCtrl)->SetValue("<>");
    XRCCTRL(*this, "cmbInheritanceScope", wxComboBox)->SetSelection(0);
    XRCCTRL(*this, "cmbMemberScope", wxComboBox)->SetSelection(2);
    XRCCTRL(*this, "txtHeaderInclude", wxTextCtrl)->SetValue("\"\"");

    ConfigManager *cfg = Manager::Get()->GetConfigManager("classwizard");
    if (cfg)
    {
        XRCCTRL(*this, "chkDocumentation", wxCheckBox)->SetValue(cfg->ReadBool("documentation"));
        XRCCTRL(*this, "chkCommonDir",     wxCheckBox)->SetValue(cfg->ReadBool("common_dir"));
        XRCCTRL(*this, "chkLowerCase",     wxCheckBox)->SetValue(cfg->ReadBool("lower_case"));
    }
    XRCCTRL(*this, "wxID_OK", wxButton)->SetDefault();
}

ClassWizardDlg::~ClassWizardDlg()
{
    // NOTE (Morten#3#): Not nice to have it here (should be in OnApply of the plugin)
    ConfigManager *cfg = Manager::Get()->GetConfigManager("classwizard");
    if (cfg)
    {
        cfg->Write("documentation", (bool) XRCCTRL(*this, "chkDocumentation", wxCheckBox)->GetValue());
        cfg->Write("common_dir",    (bool) XRCCTRL(*this, "chkCommonDir",     wxCheckBox)->GetValue());
        cfg->Write("lower_case",    (bool) XRCCTRL(*this, "chkLowerCase",     wxCheckBox)->GetValue());
    }
}

// ------
// events
// ------

void ClassWizardDlg::OnUpdateUI(wxUpdateUIEvent& WXUNUSED(event))
{
    bool inherits = XRCCTRL(*this, "chkInherits", wxCheckBox)->GetValue();
    XRCCTRL(*this, "txtInheritance",         wxTextCtrl)->Enable(inherits);
    XRCCTRL(*this, "txtInheritanceFilename", wxTextCtrl)->Enable(inherits);
    XRCCTRL(*this, "cmbInheritanceScope",    wxComboBox)->Enable(inherits);

    bool hasdestructor = XRCCTRL(*this, "chkHasDestructor", wxCheckBox)->GetValue();
    XRCCTRL(*this, "chkVirtualDestructor", wxCheckBox)->Enable(hasdestructor);

    bool memvars = (XRCCTRL(*this, "lstMemberVars", wxListBox)->GetCount()>0);
    XRCCTRL(*this, "btnRemoveMemberVar", wxButton)->Enable(memvars);

    bool genimpl = XRCCTRL(*this, "chkImplementation", wxCheckBox)->GetValue();
    XRCCTRL(*this, "txtImplementation", wxTextCtrl)->Enable(genimpl);
    XRCCTRL(*this, "txtHeaderInclude",  wxTextCtrl)->Enable(genimpl);

    bool commonDir = XRCCTRL(*this, "chkCommonDir", wxCheckBox)->GetValue();
    XRCCTRL(*this, "txtImplDir",    wxTextCtrl)->Enable(genimpl && !commonDir);
    XRCCTRL(*this, "btnImplDir",    wxButton)->Enable(genimpl && !commonDir);
    XRCCTRL(*this, "txtIncludeDir", wxTextCtrl)->Enable(!commonDir);
    XRCCTRL(*this, "btnIncludeDir", wxButton)->Enable(!commonDir);
    XRCCTRL(*this, "txtCommonDir",  wxTextCtrl)->Enable(commonDir);
    XRCCTRL(*this, "btnCommonDir",  wxButton)->Enable(commonDir);

    bool genguard = XRCCTRL(*this, "chkGuardBlock", wxCheckBox)->GetValue();
    XRCCTRL(*this, "txtGuardBlock", wxTextCtrl)->Enable(genguard);
}

void ClassWizardDlg::OnNameChange(wxCommandEvent& WXUNUSED(event))
{
    DoFileNames();
    DoGuardBlock();
}

void ClassWizardDlg::OnAncestorChange(wxCommandEvent& WXUNUSED(event))
{
    wxString name = XRCCTRL(*this, "txtInheritance", wxTextCtrl)->GetValue();

    while (name.Replace(_T("::"), _T("/")))
        ;

    wxString old = XRCCTRL(*this, "txtInheritanceFilename", wxTextCtrl)->GetValue();
    wxChar first = old.GetChar(0);
    wxChar last = old.Last();
    XRCCTRL(*this, "txtInheritanceFilename", wxTextCtrl)->SetValue(first + name + _T(".h") + last);
    DoGuardBlock();
}

void ClassWizardDlg::OnAddMemberVar(cb_unused wxCommandEvent& event)
{
    wxString member = XRCCTRL(*this, "txtMemberVar",    wxTextCtrl)->GetValue();
    int      memscp = XRCCTRL(*this, "cmbMemberScope",  wxComboBox)->GetSelection();
    bool     getter = XRCCTRL(*this, "chkGetter",       wxCheckBox)->GetValue();
    bool     setter = XRCCTRL(*this, "chkSetter",       wxCheckBox)->GetValue();

    bool     noprfx = XRCCTRL(*this, "chkRemovePrefix", wxCheckBox)->GetValue();
    wxString prefix = XRCCTRL(*this, "txtPrefix",       wxTextCtrl)->GetValue();

    // Valid strings are f.ex.: unsigned int foo, wxString bar, ...
    wxString memtyp = member.BeforeLast(_T(' ')).Trim();
    wxString memvar = member.AfterLast(_T(' ')).Trim();

    if (memtyp.IsEmpty())
    {
        cbMessageBox(_("Please specify a valid variable type to continue."),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    if (memvar.IsEmpty())
    {
        cbMessageBox(_("Please specify a valid variable name to continue."),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    std::vector<MemberVar>::iterator it = m_MemberVars.begin();
    while( it != m_MemberVars.end() )
    {
        if (DoMemVarRepr((*it).Typ, (*it).Var, (*it).Scp) == DoMemVarRepr(memtyp, memvar, memscp))
        {
            cbMessageBox(_("This variable does already exist."),
                         _("Error"), wxOK | wxICON_ERROR, this);
            return;
        }
        it++;
    }

    wxString method = ( (noprfx && memvar.StartsWith(prefix)) ?
                        memvar.Right(memvar.Length()-prefix.Length()) :
                        memvar );

    MemberVar mv;
    mv.Typ = memtyp;
    mv.Var = memvar;
    mv.Scp = memscp;
    if (getter) mv.Get = "Get" + method; else mv.Get = wxEmptyString;
    if (setter) mv.Set = "Set" + method; else mv.Set = wxEmptyString;
    m_MemberVars.push_back(mv);

    XRCCTRL(*this, "lstMemberVars", wxListBox)->Append(DoMemVarRepr(memtyp, memvar, memscp));
}

void ClassWizardDlg::OnRemoveMemberVar(cb_unused wxCommandEvent& event)
{
    wxString selection = XRCCTRL(*this, "lstMemberVars", wxListBox)->GetStringSelection();
    if (selection.IsEmpty())
    {
        cbMessageBox(_("Please select a variable to remove."),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    std::vector<MemberVar>::iterator it = m_MemberVars.begin();
    while( it != m_MemberVars.end() )
    {
        if (DoMemVarRepr((*it).Typ, (*it).Var, (*it).Scp) == selection)
        {
            m_MemberVars.erase(it);
            break; // end while loop
        }
        it++;
    }

    // Re-create the list box entries
    XRCCTRL(*this, "lstMemberVars", wxListBox)->Clear();
    it = m_MemberVars.begin();
    while( it != m_MemberVars.end() )
    {
        XRCCTRL(*this, "lstMemberVars", wxListBox)->Append(DoMemVarRepr((*it).Typ, (*it).Var, (*it).Scp));
        it++;
    }
}

void ClassWizardDlg::OnCommonDirClick(wxCommandEvent& WXUNUSED(event))
{
    wxString path = XRCCTRL(*this, "txtCommonDir", wxTextCtrl)->GetValue();
    wxDirDialog dlg (this, _("Choose a directory"), path);
    PlaceWindow(&dlg);
    if (dlg.ShowModal()==wxID_OK)
    {
        path = dlg.GetPath();
        XRCCTRL(*this, "txtCommonDir", wxTextCtrl)->SetValue(path);
    }
}

void ClassWizardDlg::OnAddPathToProjectClick(wxCommandEvent& event)
{
    XRCCTRL(*this, "chkRelativePath", wxCheckBox)->Enable(event.IsChecked());
}

void ClassWizardDlg::OnLowerCaseClick(wxCommandEvent& WXUNUSED(event))
{
    DoFileNames();
}

void ClassWizardDlg::OnIncludeDirClick(wxCommandEvent& WXUNUSED(event))
{
    wxString path = XRCCTRL(*this, "txtIncludeDir", wxTextCtrl)->GetValue();
    wxDirDialog dlg (this, _("Choose a directory"), path);
    PlaceWindow(&dlg);
    if (dlg.ShowModal()==wxID_OK)
    {
        path = dlg.GetPath();
        XRCCTRL(*this, "txtIncludeDir", wxTextCtrl)->SetValue(path);
    }
}

void ClassWizardDlg::OnImplDirClick(wxCommandEvent& WXUNUSED(event))
{
    wxString path = XRCCTRL(*this, "txtImplDir", wxTextCtrl)->GetValue();
    wxDirDialog dlg (this, _("Choose a directory"), path);
    PlaceWindow(&dlg);
    if (dlg.ShowModal()==wxID_OK)
    {
        path = dlg.GetPath();
        XRCCTRL(*this, "txtImplDir", wxTextCtrl)->SetValue(path);
    }
}

void ClassWizardDlg::OnHeaderChange(wxCommandEvent& WXUNUSED(event))
{
    wxString name = XRCCTRL(*this, "txtHeader", wxTextCtrl)->GetValue();
    XRCCTRL(*this, "txtHeaderInclude", wxTextCtrl)->SetValue(_T("\"") + name + _T("\""));
}

void ClassWizardDlg::OnOKClick(wxCommandEvent& WXUNUSED(event))
{
    // Reset
    m_Header         = XRCCTRL(*this, "txtHeader", wxTextCtrl)->GetValue();
    m_Implementation = XRCCTRL(*this, "txtImplementation", wxTextCtrl)->GetValue();

    ConfigManager *cfg = Manager::Get()->GetConfigManager("classwizard");
    cfg->Write("header_type", m_Header.AfterLast('.'));
    cfg->Write("source_type", m_Implementation.AfterLast('.'));


    // obtain variable for easy reference
    m_Name      = XRCCTRL(*this, "txtName", wxTextCtrl)->GetValue();
    m_Arguments = XRCCTRL(*this, "txtArguments", wxTextCtrl)->GetValue();
    // Error check
    if (m_Name.IsEmpty())
    {
        cbMessageBox(_("Please specify a class name to continue."),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }
    // Extract namespaces from class name
    wxStringTokenizer tkz(m_Name, "::");
    m_Name = wxEmptyString;
    while ( tkz.HasMoreTokens() )
    {
        // Store the old "class name" as (another) namespace
        if (!m_Name.IsEmpty())
        {
            m_NameSpaces.Add(m_Name);
        }
        // Stor the new "class name" as true class name
        m_Name = tkz.GetNextToken();
    }

    m_HasDestructor     = XRCCTRL(*this, "chkHasDestructor", wxCheckBox)->GetValue();
    m_VirtualDestructor = XRCCTRL(*this, "chkVirtualDestructor", wxCheckBox)->GetValue();
    m_HasCopyCtor       = XRCCTRL(*this, "chkHasCopyCtor", wxCheckBox)->GetValue();
    m_HasAssignmentOp   = XRCCTRL(*this, "chkHasAssignmentOp", wxCheckBox)->GetValue();
    if (!m_HasDestructor)
    {
        m_VirtualDestructor = false; // Fix error
    }

    m_Inherits         = XRCCTRL(*this, "chkInherits", wxCheckBox)->GetValue();
    m_Ancestor         = XRCCTRL(*this, "txtInheritance", wxTextCtrl)->GetValue();
    m_AncestorFilename = XRCCTRL(*this, "txtInheritanceFilename", wxTextCtrl)->GetValue();
    m_AncestorScope    = XRCCTRL(*this, "cmbInheritanceScope", wxComboBox)->GetValue();
    if (m_Ancestor.IsEmpty())
        m_Inherits = false; // Fix error

    m_Documentation = XRCCTRL(*this, "chkDocumentation", wxCheckBox)->GetValue();

    m_AddPathToProject = XRCCTRL(*this, "chkAddPathToProject", wxCheckBox)->GetValue();
    m_UseRelativePath  = XRCCTRL(*this, "chkRelativePath", wxCheckBox)->GetValue();
    m_CommonDir        = XRCCTRL(*this, "chkCommonDir", wxCheckBox)->GetValue();
    if (m_CommonDir)
    {
        m_IncludeDir = XRCCTRL(*this, "txtCommonDir", wxTextCtrl)->GetValue();
        m_ImplDir    = XRCCTRL(*this, "txtCommonDir", wxTextCtrl)->GetValue();
    }
    else
    {
        m_IncludeDir = XRCCTRL(*this, "txtIncludeDir", wxTextCtrl)->GetValue();
        m_ImplDir    = XRCCTRL(*this, "txtImplDir", wxTextCtrl)->GetValue();
    }

    m_GuardBlock = XRCCTRL(*this, "chkGuardBlock", wxCheckBox)->GetValue();
    m_GuardWord  = XRCCTRL(*this, "txtGuardBlock", wxTextCtrl)->GetValue();
    if (m_GuardWord.IsEmpty())
    {
        m_GuardBlock = false; // Fix error
    }

    m_GenerateImplementation = XRCCTRL(*this, "chkImplementation", wxCheckBox)->GetValue();
    m_HeaderInclude          = XRCCTRL(*this, "txtHeaderInclude", wxTextCtrl)->GetValue();

    // Common stuff
    bool usestabs = Manager::Get()->GetConfigManager("editor")->ReadBool("/use_tab",    false);
    int  tabsize  = Manager::Get()->GetConfigManager("editor")->ReadInt("/tab_size",    4);

    m_TabStr = usestabs ? wxString("\t") : wxString(' ', tabsize);
    m_EolStr = GetEOLStr();

    // actual file creation starts here
    bool success = DoHeader();
    if (success)
    {
        if (m_GenerateImplementation)
            success = DoImpl();
    }

    if (success)
        EndModal(wxID_OK);
}

void ClassWizardDlg::OnCancelClick(wxCommandEvent& WXUNUSED(event))
{
    EndModal(wxID_CANCEL);
}

// -------
// methods
// -------

bool ClassWizardDlg::DoHeader()
{
    // Create the header file
    wxFileName headerFname(UnixFilename(m_Header));
    headerFname.MakeAbsolute(m_IncludeDir);
    DoForceDirectory(headerFname);

    // Create a new editor/file (probably based on a template setup by the user)
    cbEditor* new_ed = Manager::Get()->GetEditorManager()->New(headerFname.GetFullPath());
    if (!new_ed)
    {
        cbMessageBox(_("Class wizard can't continue.\n"
                        "Possibly the header file name is invalid.\n"
                        "Please check the entered file name."),
                     _("Error"), wxICON_ERROR, this);
        return false;
    }

    // Obtain the buffer of the new file and replace any macros that might exist
    wxString buffer = new_ed->GetControl()->GetText();
    Manager::Get()->GetMacrosManager()->ReplaceMacros(buffer);

    // let's start with the header file
    if (m_GuardBlock)
    {
        buffer << _T("#ifndef ") << m_GuardWord << m_EolStr;
        buffer << _T("#define ") << m_GuardWord << m_EolStr;
        buffer << m_EolStr;
    }

    if (!m_AncestorFilename.IsEmpty() && !m_AncestorFilename.IsSameAs(_T("<>")))
    {
        buffer << "#include " << m_AncestorFilename << m_EolStr;
        buffer << m_EolStr;
    }

    for (unsigned int i=0; i<m_NameSpaces.GetCount(); ++i)
    {
        buffer << "namespace " << m_NameSpaces[i] << " {" << m_EolStr;
    }
    buffer << m_EolStr;

    // Begin of class
    buffer << "class " << m_Name;
    if (m_Inherits)
    {
        buffer << " : " << m_AncestorScope << " " << m_Ancestor;
    }
    buffer << m_EolStr;
    buffer << _T("{") << m_EolStr;

    // focus: public
    buffer << m_TabStr << "public:" << m_EolStr;

    // ctor
    if (m_Documentation)
    {
        buffer << m_TabStr << m_TabStr << _("/** Default constructor */") << m_EolStr;
    }
    buffer << m_TabStr << m_TabStr << m_Name << "(" << m_Arguments << ")"
           << (!m_GenerateImplementation ? wxString(" {}") : wxString(";") << m_EolStr);

    if (m_HasDestructor)
    {
        if (m_Documentation)
        {
            buffer << m_TabStr << m_TabStr << _("/** Default destructor */") << m_EolStr;
        }
        buffer << m_TabStr << m_TabStr;
        if (m_VirtualDestructor)
        {
            buffer << "virtual ";
        }
        buffer << '~' << m_Name << "()";
        buffer << (!m_GenerateImplementation ? wxString(" {}") : wxString(";")) << m_EolStr;
    }

    if (m_HasCopyCtor)
    {
        if (m_Documentation)
        {
            buffer << m_TabStr << m_TabStr
                   << _("/** Copy constructor") << m_EolStr;
            buffer << m_TabStr << m_TabStr
                   << " *  \\param other " << _("Object to copy from") << m_EolStr;
            buffer << m_TabStr << m_TabStr
                   << " */" << m_EolStr;
        }
        buffer << m_TabStr << m_TabStr;
        buffer << m_Name << "(const " << m_Name << "& other)";
        buffer << (!m_GenerateImplementation ? wxString(" {}") : wxString(";")) << m_EolStr;
    }

    if (m_HasAssignmentOp)
    {
        if (m_Documentation)
        {
            buffer << m_TabStr << m_TabStr
                   << _("/** Assignment operator") << m_EolStr;
            buffer << m_TabStr << m_TabStr
                   << " *  \\param other " << _("Object to assign from") << m_EolStr;
            buffer << m_TabStr << m_TabStr
                   << " *  \\return " << _("A reference to 'this'") << m_EolStr;
            buffer << m_TabStr << m_TabStr
                   << " */" << m_EolStr;
        }
        buffer << m_TabStr << m_TabStr;
        buffer << m_Name << "& " << "operator=(const " << m_Name << "& other)";
        buffer << (!m_GenerateImplementation ? _T("{ return *this; }") : _T(";")) << m_EolStr;
    }
    buffer << m_EolStr;

    bool addnewline = false;
    std::vector<MemberVar>::iterator it = m_MemberVars.begin();
    while( it != m_MemberVars.end() )
    {
        if (!(*it).Get.IsEmpty())
        {
            addnewline = true;
            if (m_Documentation)
            {
                buffer << m_TabStr << m_TabStr
                       << _("/** Access ") << (*it).Var << m_EolStr;
                buffer << m_TabStr << m_TabStr
                       << " * \\return " << _("The current value of ") << (*it).Var << m_EolStr;
                buffer << m_TabStr << m_TabStr
                       << " */" << m_EolStr;
            }
            buffer << m_TabStr << m_TabStr << (*it).Typ << " " << (*it).Get
                   << _T("() { return ") << (*it).Var << _T("; }") << m_EolStr;
        }
        if (!(*it).Set.IsEmpty())
        {
            addnewline = true;
            if (m_Documentation)
            {
                buffer << m_TabStr << m_TabStr
                       << _("/** Set ") << (*it).Var << m_EolStr;
                buffer << m_TabStr << m_TabStr
                       << " * \\param val " << _("New value to set") << m_EolStr;
                buffer << m_TabStr << m_TabStr
                       << " */" << m_EolStr;
            }
            buffer << m_TabStr << m_TabStr << "void " << (*it).Set << "("
                   << (*it).Typ << " val) { " << (*it).Var << " = val; }" << m_EolStr;
        }
        ++it;
    }

    if (addnewline)
        buffer << m_EolStr;

    addnewline = false;
    for( it = m_MemberVars.begin(); it != m_MemberVars.end(); ++it )
    {
        if ((*it).Scp == 0)
        {
            addnewline = true;
            buffer << m_TabStr << m_TabStr
                   << (*it).Typ << " " << (*it).Var << ";";

            if (m_Documentation)
                buffer << _(" //!< Member variable \"") << (*it).Var << "\"";

            buffer << m_EolStr;
        }
    }

    if (addnewline)
        buffer << m_EolStr;

    // focus: protected
    buffer << m_TabStr << "protected:" << m_EolStr;

    for( it = m_MemberVars.begin(); it != m_MemberVars.end(); ++it )
    {
        if ((*it).Scp == 1)
        {
            buffer << m_TabStr << m_TabStr
                   << (*it).Typ << " " << (*it).Var << ";";

            if (m_Documentation)
                buffer << _(" //!< Member variable \"") << (*it).Var << "\"";

            buffer << m_EolStr;
        }
    }
    buffer << m_EolStr;

    // focus: private
    buffer << m_TabStr << "private:" << m_EolStr;

    for( it = m_MemberVars.begin(); it != m_MemberVars.end(); ++it )
    {
        if ((*it).Scp == 2)
        {
            buffer << m_TabStr << m_TabStr
                   << (*it).Typ << " " << (*it).Var << _T(";");

            if (m_Documentation)
                buffer << _(" //!< Member variable \"") << (*it).Var << "\"";

            buffer << m_EolStr;
        }
    }

    // End of class
    buffer << _T("};") << m_EolStr;

    if (m_NameSpaces.GetCount())
    {
        buffer << m_EolStr;
        for (int i=m_NameSpaces.GetCount(); i>0; --i)
        {
            buffer << _T("} // namespace ") << m_NameSpaces[i-1] << m_EolStr;
        }
    }

    if (m_GuardBlock)
    {
        buffer << m_EolStr;
        buffer << _T("#endif // ") << m_GuardWord << m_EolStr;
    }

    new_ed->GetControl()->SetText(buffer);
    if (!new_ed->Save())
    {
        wxString msg;
        msg.Printf(_("Could not save header file %s.\nAborting..."), headerFname.GetFullPath().c_str());
        cbMessageBox(msg, _("Error"), wxICON_ERROR, this);
        return false;
    }

    m_Header = headerFname.GetFullPath();

    return true;
}

bool ClassWizardDlg::DoImpl()
{
    // Create the implementation file
    wxFileName implementationFname(UnixFilename(m_Implementation));
    implementationFname.MakeAbsolute(m_ImplDir);
    DoForceDirectory(implementationFname);

    // Create a new editor/file (probably based on a template setup by the user)
    cbEditor* new_ed = Manager::Get()->GetEditorManager()->New(implementationFname.GetFullPath());
    if (!new_ed)
    {
        cbMessageBox(_("Class wizard can't continue.\n"
                        "Possibly the implementation file name is invalid.\n"
                        "Please check the entered file name."),
                     _("Error"), wxICON_ERROR, this);
        return false;
    }

    // Obtain the buffer of the new file and replace any macros that might exist
    wxString buffer = new_ed->GetControl()->GetText();
    Manager::Get()->GetMacrosManager()->ReplaceMacros(buffer);

    buffer << _T("#include ") << m_HeaderInclude << m_EolStr;

    if (m_NameSpaces.GetCount())
    {
        buffer << m_EolStr;
        for (unsigned int i=0; i<m_NameSpaces.GetCount(); ++i)
        {
            buffer << "namespace " << m_NameSpaces[i] << " {" << m_EolStr;
        }
    }

    buffer << m_EolStr;
    buffer << m_Name << _T("::") << m_Name << _T("(") << m_Arguments << _T(")") << m_EolStr;
    buffer << _T("{") << m_EolStr;
    buffer << m_TabStr << _T("//ctor") << m_EolStr;
    buffer << _T("}") << m_EolStr;

    if (m_HasDestructor)
    {
        buffer << m_EolStr;
        buffer << m_Name << _T("::~") << m_Name << _T("()") << m_EolStr;
        buffer << _T("{") << m_EolStr;
        buffer << m_TabStr << _T("//dtor") << m_EolStr;
        buffer << _T("}") << m_EolStr;
    }

    if (m_HasCopyCtor)
    {
        buffer << m_EolStr;
        buffer << m_Name << "::" << m_Name << "(const " << m_Name << "& other)" << m_EolStr;;
        buffer << "{" << m_EolStr;
        buffer << m_TabStr << "//copy ctor" << m_EolStr;
        buffer << "}" << m_EolStr;
    }

    if (m_HasAssignmentOp)
    {
        buffer << m_EolStr;
        buffer << m_Name << "& " << m_Name << "::operator=(const " << m_Name << "& rhs)" << m_EolStr;;
        buffer << "{" << m_EolStr;
        buffer << m_TabStr << "if (this == &rhs) return *this; // handle self assignment" << m_EolStr;
        buffer << m_TabStr << "//assignment operator" << m_EolStr;
        buffer << m_TabStr << "return *this;" << m_EolStr;
        buffer << "}" << m_EolStr;
    }

    if (m_NameSpaces.GetCount())
    {
        buffer << m_EolStr;
        for (int i=m_NameSpaces.GetCount(); i>0; --i)
        {
            buffer << _T("} // namespace ") << m_NameSpaces[i-1] << m_EolStr;
        }
    }

    new_ed->GetControl()->SetText(buffer);
    if (!new_ed->Save())
    {
        wxString msg;
        msg.Printf(_("Could not save implementation file %s.\nAborting..."), implementationFname.GetFullPath());
        cbMessageBox(msg, _("Error"), wxICON_ERROR, this);
        return false;
    }

    m_Implementation = implementationFname.GetFullPath();

    return true;
}

void ClassWizardDlg::DoGuardBlock()
{
    m_Header = XRCCTRL(*this, "txtHeader", wxTextCtrl)->GetValue();
    wxString GuardWord = m_Header;
    GuardWord.MakeUpper();
    while (GuardWord.Replace(".", "_"))
        ;
    while (GuardWord.Replace("/", "_"))
        ;
    while (GuardWord.Replace("\\", "_"))
        ;
    XRCCTRL(*this, "txtGuardBlock", wxTextCtrl)->SetValue(GuardWord);
}

void ClassWizardDlg::DoFileNames()
{
    wxString name = XRCCTRL(*this, "txtName", wxTextCtrl)->GetValue();
    if (XRCCTRL(*this, "chkLowerCase", wxCheckBox)->GetValue())
        name.MakeLower();

    while (name.Replace("::", "/"))
        ;

    ConfigManager *cfg = Manager::Get()->GetConfigManager("classwizard");
    wxString  headerType  = cfg->Read("header_type", "h");
    wxString  sourceType  = cfg->Read("source_type", "cpp");

    XRCCTRL(*this, "txtHeader", wxTextCtrl)->SetValue(name + "." + headerType);
    XRCCTRL(*this, "txtImplementation", wxTextCtrl)->SetValue(name + "." + sourceType);
    XRCCTRL(*this, "txtHeaderInclude", wxTextCtrl)->SetValue("\"" + name + "." + headerType + "\"");
}

void ClassWizardDlg::DoForceDirectory(const wxFileName & filename)
{
    wxFileName parentname(filename);
    parentname.RemoveLastDir();

    if ((filename != parentname) && (parentname.GetDirCount() >= 1) )
        DoForceDirectory(parentname);

    if (!wxDirExists(filename.GetPath()))
        wxMkdir(filename.GetPath());
}

wxString ClassWizardDlg::DoMemVarRepr(const wxString & typ, const wxString & var, const int & scp)
{
    wxString scpstr;
    switch(scp)
    {
        case 0:
            scpstr = "pub :: ";
            break;

        case 1:
            scpstr = "pro :: ";
            break;

        case 2:
            scpstr = "pri :: ";
            break;
    }

    return (scpstr + _T("[") + typ + _T("] : ") + var);
}

wxString ClassWizardDlg::GetIncludeDir()
{
    if (!m_UseRelativePath)
        return m_IncludeDir;

    wxString relative = m_IncludeDir;
    wxFileName fname = m_IncludeDir;
    if (fname.IsAbsolute())
    {
        wxString basePath = Manager::Get()->GetProjectManager()->GetActiveProject()->GetCommonTopLevelPath();
        fname.MakeRelativeTo(basePath);
        relative = fname.GetFullPath();
    }
    return relative;
}
