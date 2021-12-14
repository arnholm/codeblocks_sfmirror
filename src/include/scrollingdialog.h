/////////////////////////////////////////////////////////////////////////////
// Name:        scrollingdialog.h
// Purpose:     wxScrollingDialog
// Author:      Julian Smart
// Modified by: Jens Lody
// Created:     2007-12-11
// RCS-ID:      $Id$
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SCROLLINGDIALOG_H_
#define _WX_SCROLLINGDIALOG_H_

#include "wx/dialog.h"
#include "wx/propdlg.h"
#include "settings.h" // DLLIMPORT

/*!
 * Base class for layout adapters - code that, for example, turns a dialog into a
 * scrolling dialog if there isn't enough screen space. You can derive further
 * adapter classes to do any other kind of adaptation, such as applying a watermark, or adding
 * a help mechanism.
 */

class wxScrollingDialog;
class wxDialogHelper;

class wxBoxSizer;
class wxButton;
class wxScrolledWindow;

/*!
 * A class that makes its content scroll if necessary
 */

class DLLIMPORT wxScrollingDialog: public wxDialog
{
    DECLARE_CLASS(wxScrollingDialog)
public:

    wxScrollingDialog()
    {
        SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
    }
    wxScrollingDialog(wxWindow *parent,
             int id = wxID_ANY,
             const wxString& title = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxDEFAULT_DIALOG_STYLE,
             const wxString& name = _("dialogBox"))
    {
        SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
        Create(parent, id, title, pos, size, style, name);
    }
};

/*!
 * A wxPropertySheetDialog class that makes its content scroll if necessary.
 */

class wxScrollingPropertySheetDialog : public wxPropertySheetDialog
{
public:
    wxScrollingPropertySheetDialog() : wxPropertySheetDialog()
    {
        SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
    }

    wxScrollingPropertySheetDialog(wxWindow* parent, wxWindowID id,
                       const wxString& title,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& sz = wxDefaultSize,
                       long style = wxDEFAULT_DIALOG_STYLE,
                       const wxString& name = wxDialogNameStr)
    {
        SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
        Create(parent, id, title, pos, sz, style, name);
    }

protected:

    DECLARE_DYNAMIC_CLASS(wxScrollingPropertySheetDialog)
};

#endif
 // _WX_SCROLLINGDIALOG_H_

