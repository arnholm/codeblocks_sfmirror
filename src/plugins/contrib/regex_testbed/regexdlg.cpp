/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include <sdk.h>

#include "regexdlg.h"
#include <wx/regex.h>

#ifndef CB_PRECOMP
    #include <globals.h>
    #include <manager.h>
    #include <configmanager.h>
#endif

//(*InternalHeaders(RegExDlg)
#include <wx/xrc/xmlres.h>
//*)

//(*IdInit(RegExDlg)
//*)

BEGIN_EVENT_TABLE(RegExDlg,wxScrollingDialog)
END_EVENT_TABLE()

RegExDlg::VisibleDialogs RegExDlg::m_visible_dialogs;

RegExDlg::RegExDlg(wxWindow* parent,wxWindowID /*id*/)
{
    //(*Initialize(RegExDlg)
    wxXmlResource::Get()->LoadObject(this,parent,_T("RegExDlg"),_T("wxScrollingDialog"));
    m_regex = (wxTextCtrl*)FindWindow(XRCID("ID_REGEX"));
    StaticText4 = (wxStaticText*)FindWindow(XRCID("ID_STATICTEXT2"));
    m_quoted = (wxTextCtrl*)FindWindow(XRCID("ID_QUOTED"));
    m_syntax = (wxChoice*)FindWindow(XRCID("ID_SYNTAX"));
    m_nocase = (wxCheckBox*)FindWindow(XRCID("ID_NOCASE"));
    m_newlines = (wxCheckBox*)FindWindow(XRCID("ID_NEWLINES"));
    m_text = (wxTextCtrl*)FindWindow(XRCID("ID_TEXT"));
    m_output = (wxHtmlWindow*)FindWindow(XRCID("ID_OUT"));

    Connect(XRCID("ID_REGEX"),wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&RegExDlg::OnValueChanged);
    Connect(XRCID("ID_QUOTED"),wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&RegExDlg::OnQuoteChanged);
    Connect(XRCID("ID_SYNTAX"),wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&RegExDlg::OnOptionChanged);
    Connect(XRCID("ID_NOCASE"),wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&RegExDlg::OnOptionChanged);
    Connect(XRCID("ID_NEWLINES"),wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&RegExDlg::OnOptionChanged);
    Connect(XRCID("ID_TEXT"),wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&RegExDlg::OnOptionChanged);
    //*)

    assert(m_regex);
    assert(m_quoted);
    assert(m_syntax);
    assert(m_nocase);
    assert(m_newlines);
    assert(m_text);
    assert(m_output);

    m_text->MoveAfterInTabOrder(m_quoted);

#if wxCHECK_VERSION(3, 1, 6)
    m_syntax->Delete(1);  // v3.1.6 made wxRE_ADVANCED a synonym of wxRE_EXTENDED, so delete it
#endif

    m_syntax->SetSelection(0);
    m_output->SetBorders(0);

    m_visible_dialogs.insert(this);
}

RegExDlg::~RegExDlg()
{
}

void RegExDlg::OnClose(wxCloseEvent& /*event*/)
{
    VisibleDialogs::iterator it = m_visible_dialogs.find(this);
    if (it != m_visible_dialogs.end())
    {
        delete *it;
        m_visible_dialogs.erase(it);
    }
}


void RegExDlg::ReleaseAll()
{
    for (VisibleDialogs::iterator it = m_visible_dialogs.begin(); it != m_visible_dialogs.end(); ++it)
        delete *it;

    m_visible_dialogs.clear();
}

namespace
{
/**
    @brief Makes the input string to be valid html string (replaces <,>,&," with &lt;,&gt;,&amp;,&quot; respectively)
    @param [inout] s - string that will be escaped
*/
void cbEscapeHtml(wxString &s)
{
    s.Replace("&",  "&amp;");
    s.Replace("<",  "&lt;");
    s.Replace(">",  "&gt;");
    s.Replace("\"", "&quot;");
}
}

void RegExDlg::OnValueChanged(cb_unused wxCommandEvent& event)
{
    wxString tmp(m_regex->GetValue());
    tmp.Replace("\\", "\\\\");
    tmp.Replace("\"", "\\\"");
    m_quoted->ChangeValue(tmp);
    Reevaluate();
}

void RegExDlg::OnQuoteChanged(cb_unused wxCommandEvent& event)
{
    wxString tmp(m_quoted->GetValue());
    tmp.Replace("\\\\", "\\");
    tmp.Replace("\\\"", "\"");
    m_regex->ChangeValue(tmp);
    Reevaluate();
}

void RegExDlg::OnOptionChanged(cb_unused wxCommandEvent& event)
{
    Reevaluate();
}

void RegExDlg::Reevaluate()
{
    wxArrayString as(GetBuiltinMatches(m_text->GetValue()));
    if (as.IsEmpty())
    {
        m_output->SetPage("<html><center><b>no matches</b></center></html>");
        return;
    }

    wxString s("<html width='100%'><center><b>matches:</b><br><br><font size=-1><table width='100%' border='1' cellspacing='2'>");
    const size_t asCount = as.GetCount();
    for (size_t i = 0; i < asCount; ++i)
    {
        cbEscapeHtml(as[i]);
        s.append(wxString::Format("<tr><td width=35><b>%lu</b></td><td>%s</td></tr>", static_cast <unsigned long> (i), as[i]));
    }

    s.append("</table></font></html>");

    m_output->SetPage(s);
}

void RegExDlg::EndModal(int retCode)
{
    wxScrollingDialog::EndModal(retCode);
}

wxArrayString RegExDlg::GetBuiltinMatches(const wxString& text)
{
    wxArrayString ret;

#if wxCHECK_VERSION(3, 1, 6)
    // wxRE_ADVANCED is a synonym of wxRE_EXTENDED, so it has been deleted from the choice
    int flags = m_syntax->GetSelection() ? wxRE_BASIC : wxRE_EXTENDED;
#else
    int flags = m_syntax->GetSelection();
#endif

    if (m_newlines->IsChecked())
        flags |= wxRE_NEWLINE;

    if (m_nocase->IsChecked())
        flags |= wxRE_ICASE;

    if (!m_wxre.Compile(m_regex->GetValue(), flags))
    {
        m_regex->SetForegroundColour(*wxWHITE);
        m_regex->SetBackgroundColour(*wxRED);
        m_regex->GetParent()->Refresh();
        return ret;
    }

    m_regex->SetForegroundColour(wxNullColour);
    m_regex->SetBackgroundColour(wxNullColour);
    m_regex->GetParent()->Refresh();

    if (!text.empty() && m_wxre.Matches(text))
    {
        const size_t matchCount = m_wxre.GetMatchCount();
        for (size_t i = 0; i < matchCount; ++i)
            ret.Add(m_wxre.GetMatch(text, i));
    }

    return ret;
}
