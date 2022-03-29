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
        m_TextControl(control), m_selection(selectionOnly)
{
    // ctor
}

cbEditorPrintout::~cbEditorPrintout()
{
    // dtor
}

bool cbEditorPrintout::OnPrintPage(int page)
{
    wxDC* dc = GetDC();
    if (dc)
    {
        // scale DC
        ScaleDC(dc);

        // print selected page
        const int maxpage = m_pPageSelStart.GetCount();
        if (!page || page > maxpage)
        {
            Manager::Get()->GetLogManager()->DebugLog(wxString::Format("OnPrintPage ERROR: page = %d, maxpage = %d", page, maxpage));
            return false;
        }

        //Manager::Get()->GetLogManager()->DebugLog("OnPrintPage: page %d, page start %d", page, m_pPageSelStart[page-1]);
        m_TextControl->FormatRange(1, m_pPageSelStart[page-1], m_selEnd, dc, dc, m_printRect, m_pageRect);
        return true;
    }

    return false;
}

bool cbEditorPrintout::HasPage(int page)
{
    return page && ((size_t)page <= m_pPageSelStart.GetCount());
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
        g_printer->GetPrintDialogData().SetPrintData(*ppd);
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

    // get print range start and end
    int selStart = 0;
    m_selEnd = m_TextControl->GetLength();
    if (m_selection && !m_TextControl->GetSelectedText().empty())
    {
        selStart = m_TextControl->GetSelectionStart();
        m_selEnd = m_TextControl->GetSelectionEnd();
    }

    wxDC* dc = GetDC();
    ScaleDC(dc);

    // count pages and save start value of each page
    m_pPageSelStart.Clear();
    while (selStart < m_selEnd)
    {
        //Manager::Get()->GetLogManager()->DebugLog(wxString::Format("CountPages: PageCount %d, page start %d", m_pPageSelStart.GetCount(), selStart));
        m_pPageSelStart.Add(selStart);
        selStart = m_TextControl->FormatRange(0, selStart, m_selEnd, dc, dc, m_printRect, m_pageRect);
        *maxPage += 1;
    }

    if (*maxPage > 0)
        *minPage = 1;

    *selPageFrom = *minPage;
    *selPageTo = *maxPage;
}

bool cbEditorPrintout::OnBeginDocument(int startPage, int endPage)
{
    const int maxpage = m_pPageSelStart.GetCount();
    if (startPage > endPage || endPage > maxpage)
    {
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("OnBeginDocument ERROR: startPage %d, endPage %d, maxpage %d", startPage, endPage, maxpage));
        return false;
    }

    return wxPrintout::OnBeginDocument(startPage, endPage);
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
