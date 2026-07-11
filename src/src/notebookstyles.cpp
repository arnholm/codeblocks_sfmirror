/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */


#include <wx/window.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/settings.h>
#include <wx/image.h>
//- #include "cbauibook.h" unused directly (says cland)
#include "prep.h"
#include "notebookstyles.h"

#include <wx/dc.h>
#include <wx/dcclient.h>

// Some general constants:
namespace
{
    const int c_vertical_border_padding = 4;
}
// ----------------------------------------------------------------------------
#if !wxCHECK_VERSION(3, 3, 0) // wx 3.2.x and before (note the !)
// ----------------------------------------------------------------------------
/******************************************************************************
* Renderer for Microsoft (tm) Visual Studio 7.1 like tabs                     *
******************************************************************************/

NbStyleVC71::NbStyleVC71() : wxAuiDefaultTabArt()
{
}

wxAuiTabArt* NbStyleVC71::Clone()
{
    NbStyleVC71* clone = new NbStyleVC71();

    clone->SetNormalFont(m_normalFont);
    clone->SetSelectedFont(m_selectedFont);
    clone->SetMeasuringFont(m_measuringFont);

    return clone;
}

void NbStyleVC71::DrawTab(wxDC& dc, wxWindow* wnd,
                            const wxAuiNotebookPage& page,
                            const wxRect& in_rect, int close_button_state,
                            wxRect* out_tab_rect, wxRect* out_button_rect,
                            int* x_extent)
{
    // Visual studio 7.1 style
    // This code is based on the renderer included in wxFlatNotebook:
    // http://svn.berlios.de/wsvn/codeblocks/trunk/src/sdk/wxFlatNotebook/src/wxFlatNotebook/renderer.cpp?rev=5106

    // figure out the size of the tab

    wxSize tab_size = GetTabSize(dc,
                                 wnd,
                                 page.caption,
                                 page.bitmap,
                                 page.active,
                                 close_button_state,
                                 x_extent);

    wxCoord tab_height = m_tabCtrlHeight - 3;
    wxCoord tab_width = tab_size.x;
    wxCoord tab_x = in_rect.x;
    wxCoord tab_y = in_rect.y + in_rect.height - tab_height;
    int clip_width = tab_width;
    if (tab_x + clip_width > in_rect.x + in_rect.width - 4)
        clip_width = (in_rect.x + in_rect.width) - tab_x - 4;

    dc.SetClippingRegion(tab_x, tab_y, clip_width + 1, tab_height - 3);
    if (m_flags & wxAUI_NB_BOTTOM)
        tab_y--;

    dc.SetPen((page.active) ? wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DHIGHLIGHT)) : wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
    dc.SetBrush((page.active) ? wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)) : wxBrush(*wxTRANSPARENT_BRUSH));

    if (page.active)
    {
//        int tabH = (m_flags & wxAUI_NB_BOTTOM) ? tab_height - 5 : tab_height - 2;
        int tabH = tab_height - 2;

        dc.DrawRectangle(tab_x, tab_y, tab_width, tabH);

        int rightLineY1 = (m_flags & wxAUI_NB_BOTTOM) ? c_vertical_border_padding - 2 : c_vertical_border_padding - 1;
        int rightLineY2 = tabH + 3;
        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
        dc.DrawLine(tab_x + tab_width - 1, rightLineY1 + 1, tab_x + tab_width - 1, rightLineY2);
        if(m_flags & wxAUI_NB_BOTTOM)
            dc.DrawLine(tab_x + 1, rightLineY2 - 3 , tab_x + tab_width - 1, rightLineY2 - 3);
        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW)));
        dc.DrawLine(tab_x + tab_width , rightLineY1 , tab_x + tab_width, rightLineY2);
        if(m_flags & wxAUI_NB_BOTTOM)
            dc.DrawLine(tab_x , rightLineY2 - 2 , tab_x + tab_width, rightLineY2 - 2);

    }
    else
    {
        // We dont draw a rectangle for non selected tabs, but only
        // vertical line on the right
        int blackLineY1 = (m_flags & wxAUI_NB_BOTTOM) ? c_vertical_border_padding + 2 : c_vertical_border_padding + 1;
        int blackLineY2 = tab_height - 5;
        dc.DrawLine(tab_x + tab_width, blackLineY1, tab_x + tab_width, blackLineY2);
    }

    wxPoint border_points[2];
    if (m_flags & wxAUI_NB_BOTTOM)
    {
        border_points[0] = wxPoint(tab_x, tab_y);
        border_points[1] = wxPoint(tab_x, tab_y + tab_height - 6);
    }
    else // if (m_flags & wxAUI_NB_TOP)
    {
        border_points[0] = wxPoint(tab_x, tab_y + tab_height - 4);
        border_points[1] = wxPoint(tab_x, tab_y + 2);
    }

    int drawn_tab_yoff = border_points[1].y;
    int drawn_tab_height = border_points[0].y - border_points[1].y;

    int text_offset = tab_x + 8;

    int bitmap_offset = 0;
    if (page.bitmap.IsOk())
    {
        bitmap_offset = tab_x + 8;
        // draw bitmap
#if wxCHECK_VERSION(3, 1, 6)
        const wxBitmap bmp(page.bitmap.GetBitmapFor(wnd));
        dc.DrawBitmap(bmp,
                      bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height/2) - (bmp.GetHeight()/2),
                      true);

        text_offset = bitmap_offset + bmp.GetWidth();
#else
        dc.DrawBitmap(page.bitmap,
                      bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height/2) - (page.bitmap.GetHeight()/2),
                      true);

        text_offset = bitmap_offset + page.bitmap.GetWidth();
#endif
        text_offset += 3; // bitmap padding
    }
    else
    {
        text_offset = tab_x + 8;
    }


    // if the caption is empty, measure some temporary text
    wxString caption = page.caption;
    if (caption.empty())
        caption = wxT("Xj");

    wxCoord textx;
    wxCoord texty;
    if (page.active)
        dc.SetFont(m_selectedFont);
    else
        dc.SetFont(m_normalFont);

    dc.GetTextExtent(caption, &textx, &texty);
    // draw tab text
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    dc.DrawText(page.caption, text_offset,
                drawn_tab_yoff + drawn_tab_height / 2 - texty / 2 - 1);

    // draw 'x' on tab (if enabled)
    if (close_button_state != wxAUI_BUTTON_STATE_HIDDEN)
    {
        wxBitmap bmp;

#if wxCHECK_VERSION(3, 1, 6)
        if ((close_button_state == wxAUI_BUTTON_STATE_HOVER) ||
                    (close_button_state == wxAUI_BUTTON_STATE_PRESSED))
            bmp = m_activeCloseBmp.GetBitmapFor(wnd);
        else
            bmp = m_disabledCloseBmp.GetBitmapFor(wnd);
#else
        if ((close_button_state == wxAUI_BUTTON_STATE_HOVER) ||
                    (close_button_state == wxAUI_BUTTON_STATE_PRESSED))
            bmp = m_activeCloseBmp;
        else
            bmp = m_disabledCloseBmp;
#endif

        const int close_button_width = bmp.GetWidth();
        wxRect rect(tab_x + tab_width - close_button_width - 3,
                    drawn_tab_yoff + (drawn_tab_height / 2) - (bmp.GetHeight() / 2),
                    close_button_width, tab_height);

        // Indent the button if it is pressed down:
        if (close_button_state == wxAUI_BUTTON_STATE_PRESSED)
        {
            rect.x++;
            rect.y++;
        }

        dc.DrawBitmap(bmp, rect.x, rect.y, true);
        *out_button_rect = rect;
    }

    *out_tab_rect = wxRect(tab_x, tab_y, tab_width, tab_height);
    dc.DestroyClippingRegion();
}

int NbStyleVC71::GetBestTabCtrlSize(wxWindow* wnd,
                                    const wxAuiNotebookPageArray& WXUNUSED(pages),
                                    const wxSize& WXUNUSED(required_bmp_size))
{
//    m_requested_tabctrl_height = -1;
//    m_tab_ctrl_height = -1;
    wxClientDC dc(wnd);
    dc.SetFont(m_measuringFont);
    int x_ext = 0;
    wxSize s = GetTabSize(dc, wnd, wxT("ABCDEFGHIj"), wxNullBitmap, true,
                            wxAUI_BUTTON_STATE_HIDDEN, &x_ext);
    return s.y + 4;
}

NbStyleFF2::NbStyleFF2() : wxAuiDefaultTabArt()
{
}

wxAuiTabArt* NbStyleFF2::Clone()
{
    NbStyleFF2* clone = new NbStyleFF2();

    clone->SetNormalFont(m_normalFont);
    clone->SetSelectedFont(m_selectedFont);
    clone->SetMeasuringFont(m_measuringFont);

    return clone;
}

void NbStyleFF2::DrawTab(wxDC& dc, wxWindow* wnd,
                            const wxAuiNotebookPage& page,
                            const wxRect& in_rect, int close_button_state,
                            wxRect* out_tab_rect, wxRect* out_button_rect,
                            int* x_extent)
{

    // Firefox 2 style

    // figure out the size of the tab
    wxSize tab_size = GetTabSize(dc, wnd, page.caption, page.bitmap,
                                    page.active, close_button_state, x_extent);

    wxCoord tab_height = m_tabCtrlHeight - 2;
    wxCoord tab_width = tab_size.x;
    wxCoord tab_x = in_rect.x;
    wxCoord tab_y = in_rect.y + in_rect.height - tab_height;

    int clip_width = tab_width;
    if (tab_x + clip_width > in_rect.x + in_rect.width - 4)
        clip_width = (in_rect.x + in_rect.width) - tab_x - 4;

    dc.SetClippingRegion(tab_x, tab_y, clip_width + 1, tab_height - 3);

    wxPoint tabPoints[7];
    int adjust = 0;
    if (!page.active)
    {
        adjust = 1;
    }

    tabPoints[0].x = tab_x + 3;
    tabPoints[0].y = (m_flags & wxAUI_NB_BOTTOM) ? 3 : tab_height - 2;

    tabPoints[1].x = tabPoints[0].x;
    tabPoints[1].y = (m_flags & wxAUI_NB_BOTTOM) ? tab_height - (c_vertical_border_padding + 2) - adjust : (c_vertical_border_padding + 2) + adjust;

    tabPoints[2].x = tabPoints[1].x+2;
    tabPoints[2].y = (m_flags & wxAUI_NB_BOTTOM) ? tab_height - c_vertical_border_padding - adjust: c_vertical_border_padding + adjust;

    tabPoints[3].x = tab_x +tab_width - 2;
    tabPoints[3].y = tabPoints[2].y;

    tabPoints[4].x = tabPoints[3].x + 2;
    tabPoints[4].y = tabPoints[1].y;

    tabPoints[5].x = tabPoints[4].x;
    tabPoints[5].y = tabPoints[0].y;

    tabPoints[6].x = tabPoints[0].x;
    tabPoints[6].y = tabPoints[0].y;

//    dc.SetBrush((page.active) ? wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)) : wxBrush(wxAuiStepColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE),85)));
    dc.SetBrush((page.active) ? wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)) : wxBrush(*wxTRANSPARENT_BRUSH));

    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));

    dc.DrawPolygon(7, tabPoints);

    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)));
    if (page.active)
    {
        dc.DrawLine(tabPoints[0].x + 1, tabPoints[0].y, tabPoints[5].x , tabPoints[0].y);
    }

    int drawn_tab_yoff = tabPoints[1].y;
    int drawn_tab_height = tabPoints[0].y - tabPoints[2].y;

    int text_offset = tab_x + 8;

    int bitmap_offset = 0;

    if (page.bitmap.IsOk())
    {
        bitmap_offset = tab_x + 8;
        // draw bitmap
#if wxCHECK_VERSION(3, 1, 6)
        const wxBitmap bmp(page.bitmap.GetBitmapFor(wnd));
        dc.DrawBitmap(bmp,
                      bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height/2) - (bmp.GetHeight()/2),
                      true);

        text_offset = bitmap_offset + bmp.GetWidth();
#else
        dc.DrawBitmap(page.bitmap,
                      bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height/2) - (page.bitmap.GetHeight()/2),
                      true);

        text_offset = bitmap_offset + page.bitmap.GetWidth();
#endif
        text_offset += 3; // bitmap padding
    }
    else
    {
        text_offset = tab_x + 8;
    }

    // if the caption is empty, measure some temporary text
    wxString caption = page.caption;
    if (caption.empty())
        caption = wxT("Xj");

    wxCoord textx;
    wxCoord texty;
    if (page.active)
        dc.SetFont(m_selectedFont);
    else
        dc.SetFont(m_normalFont);

    dc.GetTextExtent(caption, &textx, &texty);
    // draw tab text
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    dc.DrawText(page.caption, text_offset,
                drawn_tab_yoff + drawn_tab_height / 2 - texty / 2 - 1);

    // draw 'x' on tab (if enabled)
    if (close_button_state != wxAUI_BUTTON_STATE_HIDDEN)
    {
        wxBitmap bmp;

#if wxCHECK_VERSION(3, 1, 6)
        if ((close_button_state == wxAUI_BUTTON_STATE_HOVER) ||
                    (close_button_state == wxAUI_BUTTON_STATE_PRESSED))
            bmp = m_activeCloseBmp.GetBitmapFor(wnd);
        else
            bmp = m_disabledCloseBmp.GetBitmapFor(wnd);
#else
        if ((close_button_state == wxAUI_BUTTON_STATE_HOVER) ||
                    (close_button_state == wxAUI_BUTTON_STATE_PRESSED))
            bmp = m_activeCloseBmp;
        else
            bmp = m_disabledCloseBmp;
#endif

        const int close_button_width = bmp.GetWidth();
        wxRect rect(tab_x + tab_width - close_button_width - 3,
                    drawn_tab_yoff + (drawn_tab_height / 2) - (bmp.GetHeight() / 2),
                    close_button_width, tab_height);

        // Indent the button if it is pressed down:
        if (close_button_state == wxAUI_BUTTON_STATE_PRESSED)
        {
            rect.x++;
            rect.y++;
        }

        dc.DrawBitmap(bmp, rect.x, rect.y, true);
        *out_button_rect = rect;
    }

    *out_tab_rect = wxRect(tab_x, tab_y, tab_width, tab_height);
    dc.DestroyClippingRegion();
}

int NbStyleFF2::GetBestTabCtrlSize(wxWindow* wnd,
                                    const wxAuiNotebookPageArray& WXUNUSED(pages),
                                    const wxSize& WXUNUSED(required_bmp_size))
{
//    m_requested_tabctrl_height = -1;
//    m_tab_ctrl_height = -1;
    wxClientDC dc(wnd);
    dc.SetFont(m_measuringFont);
    int x_ext = 0;
    wxSize s = GetTabSize(dc, wnd, wxT("ABCDEFGHIj"), wxNullBitmap, true,
                            wxAUI_BUTTON_STATE_HIDDEN, &x_ext);
    return s.y + 6;
}

// ----------------------------------------------------------------------------
#endif // NOT 3.3.0
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
#if wxCHECK_VERSION(3, 3, 0) //wx 3.3.0 and beyond
// ----------------------------------------------------------------------------
/******************************************************************************
* Renderer for Microsoft (tm) Visual Studio 7.1 like tabs   wx 3.3.0                  *
******************************************************************************/

NbStyleVC71::NbStyleVC71() : wxAuiDefaultTabArt()
{
}

wxAuiTabArt* NbStyleVC71::Clone()
{
    NbStyleVC71* clone = new NbStyleVC71();

    clone->SetNormalFont(m_normalFont);
    clone->SetSelectedFont(m_selectedFont);
    clone->SetMeasuringFont(m_measuringFont);

    return clone;
}

// ----------------------------------------------------------------------------
wxSize NbStyleVC71::GetPageTabSize(wxReadOnlyDC& dc, wxWindow* wnd,
                                  const wxAuiNotebookPage& page,
                                  int* x_extent)
// ----------------------------------------------------------------------------
{
    wxString caption = page.caption.empty() ? wxT("Xj") : page.caption;

    wxCoord textx = 0, texty = 0;
    dc.GetTextExtent(caption, &textx, &texty);

    wxCoord bmpW = 0, bmpH = 0;
    if (page.bitmap.IsOk())
    {
#if wxCHECK_VERSION(3, 1, 6)
        wxBitmap bmp(page.bitmap.GetBitmapFor(wnd));
        bmpW = bmp.GetWidth();
        bmpH = bmp.GetHeight();
#else
        bmpW = page.bitmap.GetWidth();
        bmpH = page.bitmap.GetHeight();
#endif
    }

    const int padding = 8;
    const int bitmapPadding = page.bitmap.IsOk() ? 3 : 0;

    wxCoord width = padding + bmpW + bitmapPadding + textx + padding;
    wxCoord height = wxMax(texty, bmpH) + 8;

    // --- Add close button width calculation ---
    for (size_t i = 0; i < page.buttons.size(); ++i)
    {
        const wxAuiTabContainerButton& btn = page.buttons[i];
        if (btn.id == wxAUI_BUTTON_CLOSE)
        {
            width += 16 + padding;
        }
    }

    if (x_extent)
        *x_extent = width;

    return wxSize(width, height);
}

// ----------------------------------------------------------------------------
int NbStyleVC71::DrawPageTab(wxDC& dc, wxWindow* wnd,
                             wxAuiNotebookPage& page,
                             const wxRect& rect)
// ----------------------------------------------------------------------------
{
    // Clip everything we do here to the provided rectangle.
    wxDCClipper clip(dc, rect);

    // Compute the size of the tab.
    int xExtent = 0;
    const wxSize tab_size = GetPageTabSize(dc, wnd, page, &xExtent);
    page.rect = wxRect(rect.GetPosition(), tab_size);

    wxCoord tab_height = m_tabCtrlHeight - 3;
    wxCoord tab_width = tab_size.x;
    wxCoord tab_x = rect.x;
    wxCoord tab_y = rect.y + rect.height - tab_height;
    int clip_width = tab_width;
    if (tab_x + clip_width > rect.x + rect.width - 4)
        clip_width = (rect.x + rect.width) - tab_x - 4;

    dc.SetClippingRegion(tab_x, tab_y, clip_width + 1, tab_height - 3);
    if (m_flags & wxAUI_NB_BOTTOM)
        tab_y--;

    dc.SetPen((page.active) ? wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DHIGHLIGHT)) : wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
    dc.SetBrush((page.active) ? wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)) : wxBrush(*wxTRANSPARENT_BRUSH));

    if (page.active)
    {
        int tabH = tab_height - 2;
        dc.DrawRectangle(tab_x, tab_y, tab_width, tabH);

        int rightLineY1 = (m_flags & wxAUI_NB_BOTTOM) ? c_vertical_border_padding - 2 : c_vertical_border_padding - 1;
        int rightLineY2 = tabH + 3;
        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
        dc.DrawLine(tab_x + tab_width - 1, rightLineY1 + 1, tab_x + tab_width - 1, rightLineY2);
        if(m_flags & wxAUI_NB_BOTTOM)
            dc.DrawLine(tab_x + 1, rightLineY2 - 3 , tab_x + tab_width - 1, rightLineY2 - 3);
        dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW)));
        dc.DrawLine(tab_x + tab_width , rightLineY1 , tab_x + tab_width, rightLineY2);
        if(m_flags & wxAUI_NB_BOTTOM)
            dc.DrawLine(tab_x , rightLineY2 - 2 , tab_x + tab_width, rightLineY2 - 2);
    }
    else
    {
        int blackLineY1 = (m_flags & wxAUI_NB_BOTTOM) ? c_vertical_border_padding + 2 : c_vertical_border_padding + 1;
        int blackLineY2 = tab_height - 5;
        dc.DrawLine(tab_x + tab_width, blackLineY1, tab_x + tab_width, blackLineY2);
    }

    wxPoint border_points[2];
    if (m_flags & wxAUI_NB_BOTTOM)
    {
        border_points[0] = wxPoint(tab_x, tab_y);
        border_points[1] = wxPoint(tab_x, tab_y + tab_height - 6);
    }
    else
    {
        border_points[0] = wxPoint(tab_x, tab_y + tab_height - 4);
        border_points[1] = wxPoint(tab_x, tab_y + 2);
    }

    int drawn_tab_yoff = border_points[1].y;
    int drawn_tab_height = border_points[0].y - border_points[1].y;

    int text_offset = tab_x + 8;
    int bitmap_offset = 0;
    if (page.bitmap.IsOk())
    {
        bitmap_offset = tab_x + 8;
#if wxCHECK_VERSION(3, 1, 6)
        const wxBitmap bmp(page.bitmap.GetBitmapFor(wnd));
        dc.DrawBitmap(bmp,
                      bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height/2) - (bmp.GetHeight()/2),
                      true);
        text_offset = bitmap_offset + bmp.GetWidth();
#else
        dc.DrawBitmap(page.bitmap,
                      bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height/2) - (page.bitmap.GetHeight()/2),
                      true);
        text_offset = bitmap_offset + page.bitmap.GetWidth();
#endif
        text_offset += 3;
    }
    else
    {
        text_offset = tab_x + 8;
    }

    wxString caption = page.caption.empty() ? wxT("Xj") : page.caption;
    wxCoord textx, texty;
    dc.SetFont(page.active ? m_selectedFont : m_normalFont);
    dc.GetTextExtent(caption, &textx, &texty);

    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    dc.DrawText(page.caption, text_offset,
                drawn_tab_yoff + drawn_tab_height / 2 - texty / 2 - 1);

    // Draw buttons (like the Close 'x' button) using wx3.3 standards
    for (size_t i = 0; i < page.buttons.size(); ++i)
    {
        wxAuiTabContainerButton& btn = page.buttons[i];
        if (btn.id == wxAUI_BUTTON_CLOSE)
        {
            wxRect btn_rect = rect;
            btn_rect.width = 16;
            btn_rect.height = 16;
            btn_rect.x = tab_x + tab_width - 20;
            btn_rect.y = drawn_tab_yoff + (drawn_tab_height / 2) - (btn_rect.height / 2) - 1;

            // Assign tracking geometry back to the engine for hit-testing
            btn.rect = btn_rect;

            // Render cleanly passing a valid tracking rect reference to prevent exceptions
            wxRect out_button_rect;
            DrawButton(dc, wnd, btn_rect, btn.id, btn.curState, wxTOP, &out_button_rect);
        }
    }

    dc.DestroyClippingRegion();
    return tab_width;
}

int NbStyleVC71::GetBestTabCtrlSize(wxWindow* wnd,
                                    const wxAuiNotebookPageArray& pages,
                                    const wxSize& required_bmp_size)
{
    wxClientDC dc(wnd);
    dc.SetFont(m_measuringFont);

    // Create a mock page to query sizes matching the new wx3.3 footprint signature
    wxAuiNotebookPage dummy_page;
    dummy_page.caption = wxT("ABCDEFGHIj");

    int x_ext = 0;
    wxSize s = GetPageTabSize(dc, wnd, dummy_page, &x_ext);
    return s.y + 4;
}


/******************************************************************************
* Renderer for Firefox 2 like tabs   wx 3.3.0                                        *
******************************************************************************/

NbStyleFF2::NbStyleFF2() : wxAuiDefaultTabArt()
{
}

wxAuiTabArt* NbStyleFF2::Clone()
{
    NbStyleFF2* clone = new NbStyleFF2();
    clone->SetNormalFont(m_normalFont);
    clone->SetSelectedFont(m_selectedFont);
    clone->SetMeasuringFont(m_measuringFont);
    return clone;
}

// ----------------------------------------------------------------------------
wxSize NbStyleFF2::GetPageTabSize(wxReadOnlyDC& dc, wxWindow* wnd,
                                  const wxAuiNotebookPage& page,
                                  int* x_extent)
// ----------------------------------------------------------------------------
{
    wxString caption = page.caption.empty() ? wxT("Xj") : page.caption;

    wxCoord textx = 0, texty = 0;
    dc.GetTextExtent(caption, &textx, &texty);

    wxCoord bmpW = 0, bmpH = 0;
    if (page.bitmap.IsOk())
    {
#if wxCHECK_VERSION(3, 1, 6)
        wxBitmap bmp(page.bitmap.GetBitmapFor(wnd));
        bmpW = bmp.GetWidth();
        bmpH = bmp.GetHeight();
#else
        bmpW = page.bitmap.GetWidth();
        bmpH = page.bitmap.GetHeight();
#endif
    }

    const int padding = 8;
    const int bitmapPadding = page.bitmap.IsOk() ? 3 : 0;

    wxCoord width = padding + bmpW + bitmapPadding + textx + padding;
    wxCoord height = wxMax(texty, bmpH) + 8;    // --- Add close button width calculation ---

    for (size_t i = 0; i < page.buttons.size(); ++i)
    {
        const wxAuiTabContainerButton& btn = page.buttons[i];
        if (btn.id == wxAUI_BUTTON_CLOSE)
        {
            // Add padding and standard button width
            width += 16 + padding;
        }
    }

    if (x_extent)
        *x_extent = width;

    return wxSize(width, height);
}

// ----------------------------------------------------------------------------
int NbStyleFF2::DrawPageTab(wxDC& dc, wxWindow* wnd,
                            wxAuiNotebookPage& page,
                            const wxRect& rect)
// ----------------------------------------------------------------------------
{

    // Clip everything we do here to the provided rectangle.
    wxDCClipper clip(dc, rect);

    // Compute the size of the tab.
    int xExtent = 0;
    const wxSize tab_size = GetPageTabSize(dc, wnd, page, &xExtent);
    page.rect = wxRect(rect.GetPosition(), tab_size);

    wxCoord tab_height = tab_size.y - 2;
    //wxCoord tab_height = tab_size.y;         // (ph 26/07/01)
    wxCoord tab_width = tab_size.x;
    wxCoord tab_x = rect.x;
    wxCoord tab_y = rect.y + rect.height - tab_height;

    int clip_width = tab_width;
    if (tab_x + clip_width > rect.x + rect.width - 4)
        clip_width = (rect.x + rect.width) - tab_x - 4;

    dc.SetClippingRegion(tab_x, tab_y, clip_width + 1, tab_height - 3);

    wxPoint tabPoints[7];
    int adjust = page.active ? 0 : 1;

    tabPoints[0].x = tab_x + 3;
    tabPoints[0].y = (m_flags & wxAUI_NB_BOTTOM) ? 3 : tab_height - 2;

    tabPoints[1].x = tabPoints[0].x;
    tabPoints[1].y = (m_flags & wxAUI_NB_BOTTOM) ? tab_height - (c_vertical_border_padding + 2) - adjust
                                                 : (c_vertical_border_padding + 2) + adjust;

    tabPoints[2].x = tabPoints[1].x + 2;
    tabPoints[2].y = (m_flags & wxAUI_NB_BOTTOM) ? tab_height - c_vertical_border_padding - adjust
                                                 : c_vertical_border_padding + adjust;

    tabPoints[3].x = tab_x + tab_width - 2;
    tabPoints[3].y = tabPoints[2].y;

    tabPoints[4].x = tabPoints[3].x + 2;
    tabPoints[4].y = tabPoints[1].y;

    tabPoints[5].x = tabPoints[4].x;
    tabPoints[5].y = tabPoints[0].y;

    tabPoints[6].x = tabPoints[0].x;
    tabPoints[6].y = tabPoints[0].y;

    dc.SetBrush(page.active ? wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE))
                            : wxBrush(*wxTRANSPARENT_BRUSH));
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW)));
    dc.DrawPolygon(7, tabPoints);

    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE)));
    if (page.active)
        dc.DrawLine(tabPoints[0].x + 1, tabPoints[0].y, tabPoints[5].x, tabPoints[0].y);

    int drawn_tab_yoff = tabPoints[1].y;
    int drawn_tab_height = tabPoints[0].y - tabPoints[2].y;

    int text_offset = tab_x + 8;
    if (page.bitmap.IsOk())
    {
        int bitmap_offset = tab_x + 8;
#if wxCHECK_VERSION(3, 1, 6)
        const wxBitmap bmp(page.bitmap.GetBitmapFor(wnd));
        dc.DrawBitmap(bmp, bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height / 2) - (bmp.GetHeight() / 2),
                      true);
        text_offset = bitmap_offset + bmp.GetWidth() + 3;
#else
        dc.DrawBitmap(page.bitmap, bitmap_offset,
                      drawn_tab_yoff + (drawn_tab_height / 2) - (page.bitmap.GetHeight() / 2),
                      true);
        text_offset = bitmap_offset + page.bitmap.GetWidth() + 3;
#endif
    }

    wxString caption = page.caption.empty() ? wxT("Xj") : page.caption;
    wxCoord textx, texty;
    dc.SetFont(page.active ? m_selectedFont : m_normalFont);
    dc.GetTextExtent(caption, &textx, &texty);

    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    dc.DrawText(page.caption, text_offset,
                drawn_tab_yoff + drawn_tab_height / 2 - texty / 2 - 1);

    // Draw buttons (like the Close 'x' button)
    for (size_t i = 0; i < page.buttons.size(); ++i)
    {
        wxAuiTabContainerButton& btn = page.buttons[i];
        if (btn.id == wxAUI_BUTTON_CLOSE)
        {
            wxRect btn_rect = rect;
            btn_rect.width = 16;
            btn_rect.height = 16;
            btn_rect.x = tab_x + tab_width - 20;
            btn_rect.y = tab_y + (tab_height / 2) - 8; // Baseline center

            // 1. Tell the notebook hit-tester where the button physically is
            // We add 3 pixels manually here so clicking works perfectly where it's drawn
            btn.rect = btn_rect;
            btn.rect.y += 3;

            // 2. Shift the Device Context origin down by 3 pixels to force the draw operation down
            dc.SetDeviceOrigin(0, 3);

            wxRect out_button_rect;
            DrawButton(dc, wnd, btn_rect, btn.id, btn.curState, wxTOP, &out_button_rect);

            // 3. IMMEDIATELY reset the Device Context origin so it doesn't break other tabs
            dc.SetDeviceOrigin(0, 0);
        }
    }

    dc.DestroyClippingRegion();

    return tab_width;
}
// ----------------------------------------------------------------------------
#endif //wx 3.3.0 and beyond
// ----------------------------------------------------------------------------

