/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef NOTEBOOKSTYLES_H
#define NOTEBOOKSTYLES_H

#include "cbauibook.h" //needed no matter what clangd says

class wxDC;
class wxWindow;
class wxRect;
class wxString;
class wxBitmap;

wxColor wxAuiStepColour(const wxColor& c, int percent);

// ----------------------------------------------------------------------------
#if !wxCHECK_VERSION(3, 3, 0) // wx 3.2.0 and before (note the ! )
// ----------------------------------------------------------------------------

class NbStyleVC71 : public wxAuiDefaultTabArt
{
public:
    NbStyleVC71();
    wxAuiTabArt* Clone() override;

    void DrawTab(wxDC& dc, wxWindow* wnd, const wxAuiNotebookPage& page,
                        const wxRect& in_rect, int close_button_state,
                        wxRect* out_tab_rect, wxRect* out_button_rect,
                        int* x_extent) override;

    int GetBestTabCtrlSize(wxWindow* wnd, const wxAuiNotebookPageArray& pages,
                            const wxSize& required_bmp_size) override;
};

class NbStyleFF2 : public wxAuiDefaultTabArt
{
public:
    NbStyleFF2();
    wxAuiTabArt* Clone() override;
    void DrawTab(wxDC& dc, wxWindow* wnd, const wxAuiNotebookPage& page,
                        const wxRect& in_rect, int close_button_state,
                        wxRect* out_tab_rect, wxRect* out_button_rect,
                        int* x_extent) override;

    int GetBestTabCtrlSize(wxWindow* wnd, const wxAuiNotebookPageArray& pages,
                            const wxSize& required_bmp_size) override;
};
#endif // not wx 3.3.0
// (ai 26/07/08)
// ----------------------------------------------------------------------------
#if wxCHECK_VERSION(3, 3, 0)
// ----------------------------------------------------------------------------
// (ai 26/07/08)
class NbStyleVC71 : public wxAuiDefaultTabArt
{
public:
    NbStyleVC71();
    wxAuiTabArt* Clone() override;

    int DrawPageTab(wxDC& dc, wxWindow* wnd,
                    wxAuiNotebookPage& page,
                    const wxRect& rect) override;

    wxSize GetPageTabSize(wxReadOnlyDC& dc, wxWindow* wnd,
                          const wxAuiNotebookPage& page,
                          int* x_extent) override;

    // This tells the notebook how tall the tab bar strip needs to be.
    int GetBestTabCtrlSize(wxWindow* wnd,
                           const wxAuiNotebookPageArray& pages,
                           const wxSize& required_bmp_size) override;
};

// (ai 26/06/30)
class NbStyleFF2 : public wxAuiDefaultTabArt
{
public:
    NbStyleFF2();

    wxAuiTabArt* Clone() override;

    int DrawPageTab(wxDC& dc, wxWindow* wnd,
                    wxAuiNotebookPage& page,
                    const wxRect& rect) override;

    wxSize GetPageTabSize(wxReadOnlyDC& dc, wxWindow* wnd,
                          const wxAuiNotebookPage& page,
                          int* x_extent = nullptr) override;
};

// ----------------------------------------------------------------------------
#endif //3.3.0
// ----------------------------------------------------------------------------



#endif // NOTEBOOKSTYLES_H
