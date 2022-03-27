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
    #include <wx/dc.h>

    #include "manager.h"
    #include "logmanager.h"
    #include "cbeditor.h"
#endif

#include "cbstyledtextctrl.h"
#include "cbeditorprintout.h"
#include "printing_types.h"

#include <wx/paper.h>

cbEditorPrintout::cbEditorPrintout(const wxString& title, cbStyledTextCtrl* control, bool selectionOnly)
        : wxPrintout(title),
        m_TextControl(control)
{
    // ctor
    m_SelStart = 0;
    m_SelEnd = control->GetLength();
    if (selectionOnly && !control->GetSelectedText().IsEmpty())
    {
        m_SelStart = control->GetSelectionStart();
        m_SelEnd = control->GetSelectionEnd();
    }

    m_pPageSelStart = new wxArrayInt;
}

cbEditorPrintout::~cbEditorPrintout()
{
    // dtor
    delete m_pPageSelStart;
    m_pPageSelStart = nullptr;
}

bool cbEditorPrintout::OnPrintPage(int page)
{
    wxDC *dc = GetDC();
    if (dc)
    {
        // scale DC
        ScaleDC(dc);

        // print selected page
        const int maxpage = m_pPageSelStart->GetCount();
        if (page && page < maxpage)
            m_printed = (*m_pPageSelStart)[page-1];
        else
        {
            Manager::Get()->GetLogManager()->DebugLog(wxString::Format("OnPrintPage ERROR: page = %d, maxpage = %d", page, maxpage));
            return false;
        }

        //Manager::Get()->GetLogManager()->DebugLog("OnPrintPage: page %d , m_printed %d", page, m_printed);
        m_printed = m_TextControl->FormatRange(1, m_printed, m_SelEnd, dc, dc, m_printRect, m_pageRect);
        return true;
    }

    return false;
}

bool cbEditorPrintout::HasPage(cb_unused int page)
{
    return m_printed < m_SelEnd;
}

void cbEditorPrintout::GetPageInfo(int* minPage, int* maxPage, int* selPageFrom, int* selPageTo)
{
    // initialize values
    *minPage = 0;
    *maxPage = 0;
    *selPageFrom = 0;
    *selPageTo = 0;
    // get print page information and convert to printer pixels
    wxSize ppiScr;
    GetPPIScreen(&ppiScr.x, &ppiScr.y);
    if (ppiScr.GetWidth() == 0 || ppiScr.GetHeight() == 0)
        ppiScr = wxSize(96, 96);

    wxPrintPaperDatabase paperDB;
    paperDB.CreateDatabase();
    wxPrintData* ppd = &(g_printer->GetPrintDialogData().GetPrintData());
    wxPaperSize paperId = ppd->GetPaperId();
    if (paperId == wxPAPER_NONE)
    {
        cbMessageBox(_("No paper format specified from printer. Using DIN-A4 as default."),
                     _("Unespecified paper format"),
                     wxICON_WARNING | wxOK);

        paperId = wxPAPER_A4;
        ppd->SetPaperId(paperId);
    }

    wxSize page = paperDB.GetSize(paperId);
    if (ppd->GetOrientation() == wxLANDSCAPE)
        page = wxSize(page.GetHeight(), page.GetWidth());

    // We have to divide by 254.0 instead of 25.4, because GetSize() of paperDB returns tenth of millimeters
    page.Scale(ppiScr.GetWidth()/254.0, ppiScr.GetHeight()/254.0);
    m_pageRect = wxRect(page);

    // get margins information and convert to printer pixels
    int top    = 15; // default 25
    int bottom = 15; // default 25
    int left   = 20; // default 20
    int right  = 15; // default 20
// TODO (Tiwag#1#): get margins from PageSetup Dialog
//    wxPoint (top, left) = g_pageSetupData->GetMarginTopLeft();
//    wxPoint (bottom, right) = g_pageSetupData->GetMarginBottomRight();
    top    = static_cast <int> (top * ppiScr.GetHeight() / 25.4);
    bottom = static_cast <int> (bottom * ppiScr.GetHeight() / 25.4);
    left   = static_cast <int> (left * ppiScr.GetWidth() / 25.4);
    right  = static_cast <int> (right * ppiScr.GetWidth() / 25.4);
    m_printRect = wxRect(left, top, page.GetWidth() - (left + right), page.GetHeight() - (top + bottom));

    wxDC *dc = GetDC();
    ScaleDC(dc);

    // count pages and save SelStart value of each page
    m_pPageSelStart->Clear();
    m_pPageSelStart->Add(m_SelStart);
    m_printed = m_SelStart;
    while (HasPage(*maxPage))
    {
        //Manager::Get()->GetLogManager()->DebugLog(wxString::Format("CountPages: PageCount %d, m_printed %d", m_pPageSelStart->GetCount(), m_printed));
        m_printed = m_TextControl->FormatRange(0, m_printed, m_SelEnd, dc, dc, m_printRect, m_pageRect);
        m_pPageSelStart->Add(m_printed);
        *maxPage += 1;
    }

    if (*maxPage > 0)
        *minPage = 1;

    *selPageFrom = *minPage;
    *selPageTo = *maxPage;
    m_printed = m_SelStart;
}

bool cbEditorPrintout::OnBeginDocument(int startPage, int endPage)
{
    const bool result = wxPrintout::OnBeginDocument(startPage, endPage);
    // FIXME (Tiwag#1#): when the first time a printout is initiated
    // and you request a page to print which is out of bounds of available pages
    // it is not recognized by the above check, don't know how to fix this better
    const int maxpage = m_pPageSelStart->GetCount();
    if (startPage > maxpage || endPage > maxpage)
    {
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("OnBeginDocument ERROR: startPage %d, endPage %d, maxpage %d", startPage, endPage, maxpage));
        return false;
    }

    return result;
}

bool cbEditorPrintout::ScaleDC(wxDC* dc)
{
    if (!dc)
        return false;

    // get printer and screen sizing values
    wxSize ppiScr;
    GetPPIScreen(&ppiScr.x, &ppiScr.y);
    if (ppiScr.GetWidth() == 0 || ppiScr.GetHeight() == 0)
        ppiScr = wxSize(96, 96);

    wxSize ppiPrt;
    GetPPIPrinter(&ppiPrt.x, &ppiPrt.y);
    if (ppiPrt.GetWidth() == 0 || ppiPrt.GetHeight() == 0)
        ppiPrt = ppiScr;

    const wxSize dcSize = dc->GetSize();
    wxSize pageSize;
    GetPageSizePixels(&pageSize.x, &pageSize.y);

    // set user scale
    const double scale_x = (double)(ppiPrt.GetWidth()  * dcSize.GetWidth())  / (double)(ppiScr.GetWidth()  * pageSize.GetWidth());
    const double scale_y = (double)(ppiPrt.GetHeight() * dcSize.GetHeight()) / (double)(ppiScr.GetHeight() * pageSize.GetHeight());
    dc->SetUserScale(scale_x, scale_y);
    return true;
}
