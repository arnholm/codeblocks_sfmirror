/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <wx/wxprec.h>

//(*Headers(Protocol)
#include "scrollingdialog.h"
class wxBoxSizer;
class wxButton;
class wxStaticBoxSizer;
class wxStaticText;
class wxTextCtrl;
//*)


class wxCommandEvent;
class wxWindow;
class wxString;

class Protocol: public wxScrollingDialog
{
public:

  Protocol(wxWindow* parent,wxWindowID id = -1);

  void SetProtocol(const wxArrayString& Protocol);

  //(*Identifiers(Protocol)
  static const wxWindowID ID_TXT_PROTOCOL;
  //*)

protected:

  //(*Handlers(Protocol)
  void OnBtnOKClick(wxCommandEvent& event);
  //*)

  //(*Declarations(Protocol)
  wxTextCtrl* m_Protocol;
  //*)

private:

  DECLARE_EVENT_TABLE()
};

#endif // PROTOCOL_H
