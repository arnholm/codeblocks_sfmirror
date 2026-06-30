
// Interface Dependencies ---------------------------------------------
#ifndef PasteTask_h
#define PasteTask_h

// For compilers that support precompilation, includes <wx/wx.h>
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "Task.h"
#include "NassiView.h"

// END Interface Dependencies -----------------------------------------

// --------------------------------------------------------------------
//
//  Name
//    Task
//
//  Description
//    Abstract base class for all tasks that interact
//    with the user using mouse events.
//
//  Notes:
//
// --------------------------------------------------------------------

class HooverDrawlet;

class PasteTask : public Task
{
  public:
		PasteTask(NassiView *view, NassiFileContent *nfc, NassiBrick *brick, const wxString &strc, const wxString &strs);
		virtual ~PasteTask();

		wxCursor Start() override;

        // events from window:
		void OnMouseLeftUp(wxMouseEvent &event, const wxPoint &position) override;
        void OnMouseLeftDown(wxMouseEvent &event, const wxPoint &position) override;
        void OnMouseRightDown(wxMouseEvent &event, const wxPoint &position) override;
        void OnMouseRightUp(wxMouseEvent& event, const wxPoint &position) override;
        HooverDrawlet *OnMouseMove(wxMouseEvent &event, const wxPoint &position) override;
        void OnKeyDown(wxKeyEvent &event) override;
        void OnChar(wxKeyEvent &event) override;

        // events from frame(s)
        bool CanEdit() const override;
        //bool CanCopy() const override;
        //bool CanCut() const override;
        bool CanPaste() const override;
        bool HasSelection() const override;
        void DeleteSelection() override;
        void Copy() override;
        void Cut() override;
        void Paste() override;

        bool Done() const override;
    private:
        PasteTask(const PasteTask &p);
        PasteTask &operator=(const PasteTask &rhs);
	private:
        NassiView *m_view;
        NassiFileContent *m_nfc;
        bool m_done;
        NassiBrick *m_brick;
        wxString m_strc, m_strs;
};

#endif //InsertBrickTask_h



