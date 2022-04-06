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
        : wxPrintout(title), m_selection(selectionOnly)
{
    // ctor
    if (control)
        m_controls.push_back(control);
}

cbEditorPrintout::~cbEditorPrintout()
{
    // dtor
}

void cbEditorPrintout::AddControl(cbStyledTextCtrl* control)
{
    if (control)
    {
        m_controls.push_back(control);
        m_selection = false;
    }
}

bool cbEditorPrintout::OnPrintPage(int page)
{
    const int maxPage = m_pages.size();
    if (!page || page > maxPage)
    {
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("OnPrintPage ERROR: page = %d, maxpage = %d", page, maxPage));
        return false;
    }

    wxDC* dc = GetDC();
    if (!dc)
        return false;

    // scale DC
    ScaleDC(dc);

    //Manager::Get()->GetLogManager()->DebugLog("OnPrintPage: page %d, page start %d", page, m_pages[page-1].pageStart);
    m_controls[m_pages[page-1].ctrlIndex]->FormatRange(true, m_pages[page-1].pageStart, m_pages[page-1].selEnd, dc, dc, m_printRect, m_pageRect);
    return true;
}

bool cbEditorPrintout::HasPage(int page)
{
    return page && ((size_t)page <= m_pages.size());
}

void cbEditorPrintout::GetPageInfo(int* minPage, int* maxPage, int* selPageFrom, int* selPageTo)
{
    // initialize values
    *minPage = 0;
    *maxPage = 0;
    *selPageFrom = 0;
    *selPageTo = 0;

    wxDC* dc = GetDC();
    if (!dc)
        return;

    // get print page information and convert to printer pixels
    wxSize ppiScr;
    GetPPIScreen(&ppiScr.x, &ppiScr.y);
    if (ppiScr.GetWidth() == 0 || ppiScr.GetHeight() == 0)
        ppiScr = wxSize(96, 96);

    wxPrintPaperDatabase paperDB;
    paperDB.CreateDatabase();
    wxPrintData& ppd = g_printer->GetPrintDialogData().GetPrintData();
    wxPaperSize paperId = ppd.GetPaperId();
    if (paperId == wxPAPER_NONE)
    {
        cbMessageBox(_("No paper format specified from printer. Using DIN-A4 as default."),
                     _("Unspecified paper format"),
                     wxICON_WARNING | wxOK);

        paperId = wxPAPER_A4;
        ppd.SetPaperId(paperId);
        g_printer->GetPrintDialogData().SetPrintData(ppd);
    }

    wxSize page = paperDB.GetSize(paperId);
    if (ppd.GetOrientation() == wxLANDSCAPE)
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

    ScaleDC(dc);

    // count pages and save start value of each page
    m_pages.clear();
    const int numCtrl = m_controls.size();
    for (int n = 0; n < numCtrl; ++n)
    {
        PageInfo info;

        // get print range start and end
        info.ctrlIndex = n;
        info.pageStart = 0;
        info.selEnd = m_controls[n]->GetLength();
        if (m_selection && !m_controls[n]->GetSelectedText().empty())
        {
            info.pageStart = m_controls[n]->GetSelectionStart();
            info.selEnd = m_controls[n]->GetSelectionEnd();
        }

        while (info.pageStart < info.selEnd)
        {
            //Manager::Get()->GetLogManager()->DebugLog(wxString::Format("CountPages: PageCount %d, page start %d", (int)m_pages.size(), info.pageStart));
            m_pages.push_back(info);
            info.pageStart = m_controls[n]->FormatRange(false, info.pageStart, info.selEnd, dc, dc, m_printRect, m_pageRect);
        }
    }

    if (!m_pages.empty())
    {
        *minPage = 1;
        *maxPage = m_pages.size();
        *selPageFrom = *minPage;
        *selPageTo = *maxPage;
    }
}

bool cbEditorPrintout::OnBeginDocument(int startPage, int endPage)
{
    const int maxPage = m_pages.size();
    if (startPage > endPage || endPage > maxPage)
    {
        Manager::Get()->GetLogManager()->DebugLog(wxString::Format("OnBeginDocument ERROR: startPage %d, endPage %d, maxpage %d", startPage, endPage, maxPage));
        return false;
    }

    return wxPrintout::OnBeginDocument(startPage, endPage);
}

void cbEditorPrintout::ScaleDC(wxDC* dc)
{
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
}
