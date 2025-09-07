/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include <wx/button.h>
    #include <wx/textctrl.h>
    #include <wx/xrc/xmlres.h>
#endif

#include "genericmultilinenotesdlg.h"

BEGIN_EVENT_TABLE(GenericMultiLineNotesDlg, wxScrollingDialog)
    //
END_EVENT_TABLE()

GenericMultiLineNotesDlg::GenericMultiLineNotesDlg(wxWindow* parent, const wxString& caption, const wxString& notes, bool readOnly)
    : wxScrollingDialog(),  
      m_Notes(notes),
      m_ReadOnly(readOnly)
{
    // Create and populate from XRC
    if (!wxXmlResource::Get()->LoadDialog(this, parent, _T("dlgGenericMultiLineNotes")))
    {
        wxLogError("Missing XRC resource: dlgGenericMultiLineNotes");
        return; // fail gracefully
    }

    SetTitle(caption);

    // Children now exist - lookup succeeds on all platforms
    wxTextCtrl* notesCtrl = XRCCTRL(*this, "txtNotes", wxTextCtrl);
    wxCHECK_RET(notesCtrl, "XRC: txtNotes not found");

    notesCtrl->SetValue(m_Notes);
    if (m_ReadOnly)
    {
        notesCtrl->SetEditable(false);
        if (wxWindow* win = FindWindowById(wxID_CANCEL, this))
        {
            win->Enable(false);
        }
        // If the control is editable the user cannot activate the default button with
        // the enter key, so we set the default button only for read only notes.
        XRCCTRL(*this, "wxID_OK", wxButton)->SetDefault();
    }
    else
        notesCtrl->SetFocus();
}

GenericMultiLineNotesDlg::~GenericMultiLineNotesDlg()
{
    //dtor
}

void GenericMultiLineNotesDlg::EndModal(int retCode)
{
    if (retCode == wxID_OK && !m_ReadOnly)
    {
        m_Notes = XRCCTRL(*this, "txtNotes", wxTextCtrl)->GetValue();
    }
    wxScrollingDialog::EndModal(retCode);
}
