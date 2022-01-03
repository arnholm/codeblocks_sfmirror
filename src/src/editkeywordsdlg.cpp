/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <wx/button.h>
    #include <wx/intl.h>
    #include <wx/xrc/xmlres.h>
    #include <wx/spinctrl.h>
    #include <wx/stattext.h>
    #include <wx/textctrl.h>
    #include <wx/wxscintilla.h>
    #include "editorcolourset.h"
#endif

#include "editkeywordsdlg.h"

BEGIN_EVENT_TABLE(EditKeywordsDlg, wxScrollingDialog)
    EVT_SPINCTRL(wxID_ANY, EditKeywordsDlg::OnSetChange)
    EVT_BUTTON(wxID_OK, EditKeywordsDlg::OnExit)
END_EVENT_TABLE()

EditKeywordsDlg::EditKeywordsDlg(wxWindow* parent, EditorColourSet* theme, HighlightLanguage lang, const wxArrayString& descr)
    : m_pTheme(theme),
    m_Lang(lang),
    m_LastSet(0),
    descriptions(descr)
{
    //ctor
    wxXmlResource::Get()->LoadObject(this, parent, _T("dlgEditLangKeywords"),_T("wxScrollingDialog"));
    XRCCTRL(*this, "wxID_OK", wxButton)->SetDefault();

    spnSet      = XRCCTRL(*this, "spnSet",      wxSpinCtrl);
    txtKeywords = XRCCTRL(*this, "txtKeywords", wxTextCtrl);
    UpdateDlg();
}

EditKeywordsDlg::~EditKeywordsDlg()
{
    //dtor
}

void EditKeywordsDlg::OnExit(wxCommandEvent& event)
{
    SaveKeywords(m_LastSet);
    event.Skip();
}

void EditKeywordsDlg::OnSetChange(cb_unused wxSpinEvent& event)
{
    SaveKeywords(m_LastSet);
    m_LastSet = event.GetPosition()-1;
    UpdateDlg();
}

void EditKeywordsDlg::SaveKeywords(int index)
{
    if (txtKeywords->IsModified())
    {
        m_pTheme->SetKeywords(m_Lang, index, txtKeywords->GetValue());
        txtKeywords->DiscardEdits();
    }
}

void EditKeywordsDlg::UpdateDlg()
{
    txtKeywords->SetValue(m_pTheme->GetKeywords(m_Lang, m_LastSet));
    wxStaticText* txtDescription = XRCCTRL(*this, "txtDescription", wxStaticText);
    if ((size_t)m_LastSet < descriptions.GetCount())
        txtDescription->SetLabel(descriptions[m_LastSet] + ":");
    else
        txtDescription->SetLabel(_("Not highlighted keywords") + ":");
}
