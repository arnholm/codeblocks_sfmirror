/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef CBAUIBOOK_H_INCLUDED
#define CBAUIBOOK_H_INCLUDED

#include "prep.h"
#include "settings.h" // DLLIMPORT

#include <vector>

#include <wx/aui/auibook.h>
#include <wx/dynarray.h>

class wxTipWindow;
class cbAuiNotebook;

WX_DEFINE_ARRAY_PTR(wxAuiTabCtrl*,cbAuiTabCtrlArray);
WX_DEFINE_ARRAY_PTR(cbAuiNotebook*,cbAuiNotebookArray);

/** \brief A notebook class
  * This class is derived from wxAuiNotebook, to enhance its abilities.
  * It adds the ability to store (and restore) the visible tab-order, because
  * wxAuiNotebook-tabs can be reordered with drag and drop.
  * Another added feature is the possibility to add tooltips to the tabs belonging
  * to added panes.
  */
class DLLIMPORT cbAuiNotebook : public wxAuiNotebook
{
    public:
        /** \brief cbAuiNotebook constructor
         *
         * \param pParent the parent window, usually the app-window
         * \param id the notebook id
         * \param pos the position
         * \param size the size
         * \param style the notebook style
         */
        cbAuiNotebook(wxWindow* pParent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxAUI_NB_DEFAULT_STYLE);
        /** cbAuiNotebook destructor  */
        ~cbAuiNotebook() override;

        /** \brief Advances the selection
         *
         * In contrast to the base-classes function, it uses the visible tab-order, not the order
         * of creation and jumps to the first tab, if the last is reached (and vice versa)
         * \param forward if false direction is backwards
         */
        void AdvanceSelection(bool forward = true);
        /** \brief Save layout of the notebook
         * \param projectTitle only save the layout of the project with this title, save all projects layout, if empty
         * \return wxString the serialized layout
         */
        wxString SavePerspective(const wxString projectTitle = wxEmptyString);
        /** \brief Loads serialized notebook layout
         * \param layout the serialized layout
         * \param mergeLayouts try to merge the tab-layouts
         * \return bool true if successful
         *
         */
        bool LoadPerspective(const wxString& layout, bool mergeLayouts = false);
        /** \brief Get the tab index from tooltip text
         * \param text the notebooks name
         * \return int the tab's index
         * @remarks We use the name internally to store the tooltip text. To use it
         * in this function, we create a unique string from the relative filename
         * and the projects title. So it should be unique even after a
         * restart of C::B.
         */
        int GetTabIndexFromTooltip(const wxString& text);
        /** \brief Get the tab position
         *
         * Returns the position of the tab as it is visible.
         * Starts with 0
         * \param index the index of the tab in order of creation
         * \return int the visible position
         */
        int GetTabPositionFromIndex(int index);
        /** \brief Minimize free horizontal page
         *
         * Moves the active tab of all tabCtrl's to the rightmost place,
         * to show as many tabs as possible.
         */
        void MinimizeFreeSpace();
        /** \brief Delete Page
         *
         * Calls the base-class function and after that
         * MinimizeFreeSpace(), needed to hook into the close-events.
         * The system generated close event has to be veto'd, and Close()
         * has to be called manually, so we can handle it ourselves.
         * \param The index of the tab to be closed
         * \return true if successful
         */
        bool DeletePage(size_t page) override;
        /** \brief Remove Page
         *
         * Calls the base-class function and after that
         * MinimizeFreeSpace(), needed to hook into the close-events.
         * The system generated close event has to be veto'd, and Close()
         * has to be called manually, so we can handle it ourselves.
         * \param The index of the tab to be closed
         * \return true if successful
         */
        bool RemovePage(size_t page) override;
        /** \brief Move page
         *
         * Moves the tab containing page to new_idx
         * \param page The page to move (e.g. cbEditor*)
         * \param new_idx The index the page should be moved to
         * \return true if successful
         */
        bool MovePage(wxWindow* page, size_t new_idx);
        /** \brief Add Page
         *
         * Calls the base-class function and after that
         * MinimizeFreeSpace().
         * \param page The page to add
         * \param caption The caption of the page
         * \param select If true the page gets selected
         * \param bitmap The bitmap (or bitmap bundle since wx3.1.6) of the tab
         * \return true if successful
         */
        bool AddPage(wxWindow* page,
                     const wxString& caption,
                     bool select = false,
#if wxCHECK_VERSION(3, 1, 6)
                     const wxBitmapBundle& bitmap = wxBitmapBundle());
#else
                     const wxBitmap& bitmap = wxNullBitmap);
#endif
        /** \brief Insert Page
         *
         * Calls the base-class function and after that
         * MinimizeFreeSpace().
         * \param page_idx The index where the page should be inserted
         * \param page The page to add
         * \param caption The caption of the page
         * \param select If true the page gets selected
         * \param bitmap The bitmap (or bitmap bundle since wx3.1.6) of the tab
         * \return true if successful
         */
        bool InsertPage(size_t page_idx,
                        wxWindow* page,
                        const wxString& caption,
                        bool select = false,
#if wxCHECK_VERSION(3, 1, 6)
                        const wxBitmapBundle& bitmap = wxBitmapBundle());
#else
                        const wxBitmap& bitmap = wxNullBitmap);
#endif
        /** \brief Set zoom factor for builtin editors
         *
         * Sets the zoom factor for all visible builtin
         * editors.
         * \param zoom zoom factor to use
         */
        void SetZoom(int zoom);
        /** \brief Set Focus on the tabCtrl belonging to the active tab
         */
        void FocusActiveTabCtrl();

        /** \brief Returns a pointer to the page's tab control or nullptr.
         */
        wxAuiTabCtrl* GetTabCtrl(wxWindow *page);

        /** \brief Return a vector containing all pages that are in the same
         * tab control as the page passed as parameter.
         */
        void GetPagesInTabCtrl(std::vector<wxWindow*> &result, wxWindow *page);
    protected:
        /** \brief Create a unique id from the tooltip-text
         *
         * Tries to create a unique id from the tooltip.
         * Find the projectfile, get the relative filename and put it
         * together with the projects name.
         * We use it to save and load the pane layout.
         * By using the relative filename, it works even if the project
         * gets moved to another place.
         * \param text The tooltip text
         */
        wxString UniqueIdFromTooltip(const wxString& text);
        /** \brief Minimize free horizontal page of tabCtrl
         *
         * Moves the active tab of tabCtrl to the rightmost place,
         * to show as many tabs as possible.
         * \param tabCtrl The tabCtrl to act on
         */
        void MinimizeFreeSpace(wxAuiTabCtrl* tabCtrl);
        /** \brief Handle the navigation key event
         *
         * Tries to handle the navigation key-event and use "our" AdvanceSelection().
         * \param event
         * @remarks Works not reliable, due to OS/wxWidgets-limitations
         */
        void OnNavigationKeyNotebook(wxNavigationKeyEvent& event);
        /** \brief OnIdle
         *
         * \param event unused
         */
        void OnIdle(cb_unused wxIdleEvent& event);
        /** \brief Catch doubleclick-events from wxTabCtrl
         *
         * Sends cbEVT_CBAUIBOOK_LEFT_DCLICK, if double click was on a tab,
         * event-Id is the notebook-Id, event-object is the pointer to the window the
         * tab belongs to.
         * \param event holds the wxTabCtrl, that sends the event
         */
        void OnTabCtrlDblClick(wxMouseEvent& event);
        /** \brief Catch mousewheel-events from wxTabCtrl
         *
         * Sends cbEVT_CBAUIBOOK_MOUSEWHEEL, if double click was on a tab,
         * event-Id is the notebook-Id, event-object is the pointer to the window the
         * tab belongs to.
         * \param event holds the wxTabCtrl, that sends the event
         */
        void OnTabCtrlMouseWheel(wxMouseEvent& event);
        /** \brief Catch resize-events and call MinimizeFreeSpace()
         *
         * \param event unused
         */
        void OnResize(wxSizeEvent& event);
        /** \brief Catch dragdone-events from wxTabCtrl
         *
         */
        void OnDragDone(wxAuiNotebookEvent& event);
#ifdef __WXMSW__
        /** \brief Catch mouseenter-events from wxTabCtrl
         *
         * Set focus on wxTabCtrl
         * \param event holds the wxTabCtrl, that sends the event
         */
        void OnEnterTabCtrl(wxMouseEvent& event);
        /** \brief Catch mouseleave-events from wxTabCtrl
         *
         * \param event holds the wxTabCtrl, that sends the event
         */
        void OnLeaveTabCtrl(wxMouseEvent& event);
        // hack needed on wxMSW, because only focused windows get mouse wheel-events
        /** \brief Checks the old focus
         *
         * Checks whether the old focused window or one of it's
         * parents is the same as page.
         * If they are equal, we have to reset the stored pointer,
         * because we get a crash otherwise.
         * \param page The page to check against
         * \return bool
         */
        bool IsFocusStored(wxWindow* page);
        /** \brief Save old focus
         *
         * Save old focus and tab-selection,
         * \param event holds the wxTabCtrl, that sends the event
         */
        void StoreFocus();
        /** \brief Restore old focus
         *
         * Restore old focus or set the focus on the activated tab
         * \param event holds the wxTabCtrl, that sends the event
         */
        void RestoreFocus();
#endif // __WXMSW__
        /** \brief Reset tabctrl events
         */
        void ResetTabCtrlEvents();
        /** \brief Updates the array, that holds the wxTabCtrls
         */
        void UpdateTabControlsArray();
        /** \brief Check for pressed modifier-keys
         *
         * Check whether all modifier keys in keyModifier are pressed
         * or not
         * \param keyModifier wxSTring containing the modifier(s) to check for
         * \return true If all modifier-keys are pressed
         */
        bool CheckKeyModifier();
        /** \brief Holds the wxTabCtrls used by the notebook
         * @remarks Should be updated with UpdateTabControlsArray(),
         * before it's used
         */
        cbAuiTabCtrlArray m_TabCtrls;
#ifdef __WXMSW__
        // needed for wxMSW-hack, see above
        /** \brief Last selected tab
         *
         * Used to determine whether the tab-selection has changed between mouseenter
         * and mouseleave-event.
         */
        int m_LastSelected;
        /** \brief Id of last focused window
         *
         * Used to restore the focus after a mouseleave-event on wxTabCtrl.
         */
        long m_LastId;
#endif // __WXMSW__
        /** \brief If true, zoom for all editors
         * is set in next OnIdle-call
         */
        bool m_SetZoomOnIdle;
        /** \brief If true, MinimizeFreeSpace is called
         * in next OnIdle-call
         */
        bool m_MinimizeFreeSpaceOnIdle;
        /** \brief Holds the size of a tabCtrl after a resize event
         *
         * Needed to skip a resize event, if size did not change
         * it gets triggered on any tab-click
         */
        wxSize m_TabCtrlSize;

//static stuff (common to all cbAuiNotebooks)
    public:
        /** \brief Enable or disable tabtooltips globally
         *
         * \param use If true tooltips are allowed
         */
        static void UseToolTips(bool use = true);
        /** \brief Enable or disable tab-scrolling with mouse wheel
         *
         * \param allow If true scrolling is allowed
         */
        static void AllowScrolling(bool allow = true);
        /** \brief Sets the modifier keys for scrolling
         */
        static void SetModKeys(wxString keys = _T("Strg"));
        /** \brief Use modkey to advance through tabs with mouse wheel
         */
        static void UseModToAdvance(bool use = false);
        /** \brief Change direction of tab-advancing with mouse wheel
         *
         * \param invert If true advance direction is inverted
         */
        static void InvertAdvanceDirection(bool invert = false);
        /** \brief Change direction of tab-moving with mouse wheel
         *
         * \param invert If true move direction is inverted
         */
        static void InvertMoveDirection(bool invert = false);
    protected:
        /** \brief Enable or disable tab tooltips
         */
        static bool s_UseTabTooltips;
        /** \brief Enable or disable scrolling tabs with mouse wheel
         */
        static bool s_AllowMousewheel;
        /** \brief Holds an array of all existing cbAuiNotebooks
         */
        static cbAuiNotebookArray s_cbAuiNotebookArray;
        /** \brief Holds the modifier keys for scrolling
         */
        static wxString s_modKeys;
        /** \brief Use modkey to advance through tabs with mouse wheel
         */
        static bool s_modToAdvance;
        /** \brief Mouse wheel move direction: negative => invert
         */
        static int s_moveDirection;
        /** \brief Mouse wheel advance direction: negative => invert
         */
        static int s_advanceDirection;

    private:
        // the following two using directives remove the "-Woverloaded-virtual" warnings
        using wxAuiNotebook::AddPage;
        using wxAuiNotebook::InsertPage;

        DECLARE_EVENT_TABLE()
};

#endif // CBAUIBOOK_H_INCLUDED
