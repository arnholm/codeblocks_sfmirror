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
  #include <wx/choice.h>
  #include <wx/intl.h>
  #include <wx/string.h>
  #include <wx/xrc/xmlres.h>
#endif
#include "asktypedlg.h"


AskTypeDlg::AskTypeDlg(wxWindow* parent, const wxString StreamStart, const wxString StreamEnd)
{
    wxXmlResource::Get()->LoadObject(this, parent, "dlgAskType", "wxScrollingDialog");

    wxChoice *choice = XRCCTRL(*this, "chcCorrect", wxChoice);
    choice->Clear();
    choice->Append("keep line comment style and move it to the end of the line");
    choice->Append("keep line comment style at the current position");

    if (!StreamStart.IsEmpty())
        choice->Append(wxString::Format(_("switch to stream style comment (%s ... %s)"), StreamStart, StreamEnd));

    choice->SetSelection(0);
}

AskTypeDlg::~AskTypeDlg()
{
    //dtor
}

TypeCorrection AskTypeDlg::GetTypeCorrection() const
{
    return static_cast<TypeCorrection>(XRCCTRL(*this, "chcCorrect", wxChoice)->GetSelection());
}

