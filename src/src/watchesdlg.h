/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef WATCHESDLG_H
#define WATCHESDLG_H

#include <vector>
#include <wx/panel.h>
#include <wx/popupwin.h>
#include <wx/timer.h>

#include <cbdebugger_interfaces.h>

class wxBoxSizer;
class wxPropertyGrid;
class wxPropertyGridEvent;
class wxPGProperty;
class WatchesProperty;

class WatchesDlg : public wxPanel, public cbWatchesDlg
{
    public:
        WatchesDlg();

        wxWindow* GetWindow() override { return this; }

        void AddWatch(cb::shared_ptr<cbWatch> watch) override;
        void AddSpecialWatch(cb::shared_ptr<cbWatch> watch, bool readonly) override;
        void RemoveWatch(cb::shared_ptr<cbWatch> watch) override;
        void RenameWatch(wxObject *prop, const wxString &newSymbol) override;
        void RefreshUI() override;
    private:
        void OnExpand(wxPropertyGridEvent &event);
        void OnCollapse(wxPropertyGridEvent &event);
        void OnPropertySelected(wxPropertyGridEvent &event);
        void OnPropertyChanged(wxPropertyGridEvent &event);
        void OnPropertyChanging(wxPropertyGridEvent &event);
        void OnPropertyLableEditBegin(wxPropertyGridEvent &event);
        void OnPropertyLableEditEnd(wxPropertyGridEvent &event);
        void OnPropertyRightClick(wxPropertyGridEvent &event);
        void OnIdle(wxIdleEvent &event);
        void OnKeyDown(wxKeyEvent &event);

        void OnMenuRename(wxCommandEvent &event);
        void OnMenuProperties(wxCommandEvent &event);
        void OnMenuDelete(wxCommandEvent &event);
        void OnMenuDeleteAll(wxCommandEvent &event);
        void OnMenuAddDataBreak(wxCommandEvent &event);
        void OnMenuExamineMemory(cb_unused wxCommandEvent &event);
        void OnMenuAutoUpdate(wxCommandEvent &event);
        void OnMenuUpdate(wxCommandEvent &event);
        void WatchToString(wxString &result, const cbWatch &watch, const wxString &indent = wxString());
        void OnMenuCopyToClipboardData(cb_unused wxCommandEvent &event);
        void OnMenuCopyToClipboardRow(cb_unused wxCommandEvent &event);
        void OnMenuCopyToClipboardTree(cb_unused wxCommandEvent &event);

        void OnDebuggerUpdated(CodeBlocksEvent &event);

        DECLARE_EVENT_TABLE()

        void DeleteProperty(WatchesProperty &prop);

        struct WatchItem
        {
            WatchItem() : readonly(false), special(false) {}

            cb::shared_ptr<cbWatch> watch;
            WatchesProperty *property;
            bool readonly;
            bool special;
        };
        struct WatchItemPredicate;

        typedef std::vector<WatchItem> WatchItems;

        wxPropertyGrid *m_grid;
        WatchItems m_watches;
        bool m_append_empty_watch;
};


class ValueTooltip :
#ifndef __WXMAC__
    public wxPopupWindow
#else
    public wxWindow
#endif
{
    public:
        ValueTooltip(const cb::shared_ptr<cbWatch> &watch, wxWindow *parent,
                     const wxPoint &screenPosition);
        ~ValueTooltip();

        void Dismiss();
        void UpdateWatch();
    protected:
        virtual void OnDismiss();
    private:
        void UpdateSizeAndFit(wxWindow *usedToGetDisplay, const wxPoint &screenPosition);
        void ClearWatch();
    private:

        void OnCollapse(wxPropertyGridEvent &event);
        void OnExpand(wxPropertyGridEvent &event);
        void OnTimer(wxTimerEvent &event);
    private:
        wxPropertyGrid *m_grid;
        wxBoxSizer *m_sizer;

        wxTimer m_timer;
        int m_outsideCount;

        cb::shared_ptr<cbWatch> m_watch;
    private:
        DECLARE_CLASS(ValueTooltip)
        DECLARE_EVENT_TABLE()
};

#endif // WATCHESDLG_H
