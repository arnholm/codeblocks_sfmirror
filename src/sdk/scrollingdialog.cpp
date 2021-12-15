/////////////////////////////////////////////////////////////////////////////
// Name:        scrollingdialog.cpp
// Purpose:     wxScrollingDialog
// Author:      Julian Smart
// Modified by: Jens Lody
// Created:     2007-12-11
// RCS-ID:      $Id$
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////
#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include "wx/button.h"
    #include "wx/scrolwin.h"
    #include "wx/sizer.h"
#endif // CB_PRECOMP

#include "wx/module.h"
#include "wx/display.h"
#include "wx/bookctrl.h"

#include "scrollingdialog.h"

/*!
 * wxScrollingDialog
 */

IMPLEMENT_CLASS(wxScrollingDialog, wxDialog)

/*!
 * wxScrollingPropertySheetDialog
 */

IMPLEMENT_DYNAMIC_CLASS(wxScrollingPropertySheetDialog, wxPropertySheetDialog)
