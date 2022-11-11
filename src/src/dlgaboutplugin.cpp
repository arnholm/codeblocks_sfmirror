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
    #include <wx/intl.h>
    #include <wx/stattext.h>
    #include <wx/string.h>
    #include <wx/textctrl.h>
    #include <wx/xrc/xmlres.h>
#endif

#include "cbplugin.h"
#include "dlgaboutplugin.h" // class's header file

// class constructor
dlgAboutPlugin::dlgAboutPlugin(wxWindow* parent, const PluginInfo* pi)
{
	wxXmlResource::Get()->LoadObject(this, parent, "dlgAboutPlugin", "wxScrollingDialog");
    XRCCTRL(*this, "wxID_CANCEL", wxButton)->SetDefault();

	XRCCTRL(*this, "lblTitle", wxStaticText)->SetLabel(_(pi->title));
	XRCCTRL(*this, "txtDescription", wxTextCtrl)->SetValue(_(pi->description));
	XRCCTRL(*this, "txtThanksTo", wxTextCtrl)->SetValue(_(pi->thanksTo));
	XRCCTRL(*this, "txtLicense", wxTextCtrl)->SetValue(_(pi->license));
	XRCCTRL(*this, "lblName", wxStaticText)->SetLabel(_(pi->name));
	XRCCTRL(*this, "lblVersion", wxStaticText)->SetLabel(_(pi->version));
	XRCCTRL(*this, "lblAuthor", wxStaticText)->SetLabel(pi->author);
	XRCCTRL(*this, "lblEmail", wxStaticText)->SetLabel(pi->authorEmail);
	XRCCTRL(*this, "lblWebsite", wxStaticText)->SetLabel(pi->authorWebsite);

	Fit();
}

// class destructor
dlgAboutPlugin::~dlgAboutPlugin()
{
	// insert your code here
}
