/////////////////////////////////////////////////////////////////////////////
// Name:        switcherdlg.h
// Purpose:     Pane switcher dialog
// Author:      Julian Smart
// Modified by:
// Created:     2007-08-19
// RCS-ID:      $Id$
// Copyright:   (c) Julian Smart
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SWITCHERDLG_H_
#define _WX_SWITCHERDLG_H_

#include <wx/dynarray.h>
#include <wx/html/htmlwin.h>

#include "scrollingdialog.h"

/*!
 * cbSwitcherItem
 * An object containing information about one item
 */

class cbSwitcherItem: public wxObject
{
    DECLARE_CLASS(cbSwitcherItem)
public:
    cbSwitcherItem() { Init(); }
    cbSwitcherItem(const cbSwitcherItem& item) : wxObject(item)
    {
    	Init();
    	Copy(item);
	}

    bool operator== (const cbSwitcherItem& item) const;

    void operator= (const cbSwitcherItem& item) { Copy(item); }

    void Init();

    void Copy(const cbSwitcherItem& item);

    cbSwitcherItem& SetTitle(const wxString& title) { m_title = title; return (*this); }
    const wxString& GetTitle() const { return m_title; }

    cbSwitcherItem& SetName(const wxString& name) { m_name = name; return (*this); }
    const wxString& GetName() const { return m_name; }

    cbSwitcherItem& SetDescription(const wxString& descr) { m_description = descr; return (*this); }
    const wxString& GetDescription() const { return m_description; }

    cbSwitcherItem& SetId(int id) { m_id = id; return (*this); }
    int GetId() const { return m_id; }

    cbSwitcherItem& SetIsGroup(bool isGroup) { m_isGroup = isGroup; return (*this); }
    bool GetIsGroup() const { return m_isGroup; }

    cbSwitcherItem& BreakColumn(bool breakCol = true) { m_breakColumn = breakCol; return (*this); }
    bool GetBreakColumn() const { return m_breakColumn; }

    cbSwitcherItem& SetRect(const wxRect& rect) { m_rect = rect; return (*this); }
    const wxRect& GetRect() const { return m_rect; }

    cbSwitcherItem& SetTextColour(const wxColour& colour) { m_textColour = colour; return (*this); }
    const wxColour& GetTextColour() const { return m_textColour; }

    cbSwitcherItem& SetFont(const wxFont& font) { m_font = font; return (*this); }
    const wxFont& GetFont() const { return m_font; }

    cbSwitcherItem& SetBitmap(const wxBitmap& bitmap) { m_bitmap = bitmap; return (*this); }
    const wxBitmap& GetBitmap() const { return m_bitmap; }

    cbSwitcherItem& SetRowPos(int pos) { m_rowPos = pos; return (*this); }
    int GetRowPos() const { return m_rowPos; }

    cbSwitcherItem& SetColPos(int pos) { m_colPos = pos; return (*this); }
    int GetColPos() const { return m_colPos; }

    cbSwitcherItem& SetWindow(wxWindow* win) { m_window = win; return (*this); }
    wxWindow* GetWindow() const { return m_window; }

protected:

    wxString    m_title;
    wxString    m_name;
    wxString    m_description;
    int         m_id;
    bool        m_isGroup;
    bool        m_breakColumn;
    wxRect      m_rect;
    wxBitmap    m_bitmap;
    wxColour    m_textColour;
    wxFont      m_font;
    int         m_rowPos;
    int         m_colPos;
    wxWindow*   m_window;
};

WX_DECLARE_OBJARRAY(cbSwitcherItem, cbSwitcherItemArray);

/*!
 * cbSwitcherItems
 * An object containing switcher items
 */

class cbSwitcherItems: public wxObject
{
    DECLARE_CLASS(cbSwitcherItems)
public:
    cbSwitcherItems() { Init(); }
    cbSwitcherItems(const cbSwitcherItems& items) : wxObject(items)
    {
    	Init();
    	Copy(items);
	}

    bool operator== (const cbSwitcherItems& items) const;

    void operator= (const cbSwitcherItems& items) { Copy(items); }

    void Init();

    void Copy(const cbSwitcherItems& items);

    // Public API

    cbSwitcherItem& AddItem(const wxString& title, const wxString& name, int id = 0, const wxBitmap& bitmap = wxNullBitmap);
    cbSwitcherItem& AddItem(const wxString& title, const wxString& name, const wxString& descr, int id = 0, const wxBitmap& bitmap = wxNullBitmap);
    cbSwitcherItem& AddItem(const cbSwitcherItem& item);
    cbSwitcherItem& AddGroup(const wxString& title, const wxString& name, int id = 0, const wxBitmap& bitmap = wxNullBitmap);

    void Clear();

    int FindItemByName(const wxString& name) const;
    int FindItemById(int id) const;

    void SetSelection(int sel);
    void SetSelection(const wxString& name); // by name
    int GetSelection() const { return m_selection; }

    // Find the index for the item associated with the current focus
    int GetIndexForFocus() const;

    // Hit test, returning an index or -1
    int HitTest(const wxPoint& pt) const;

    const cbSwitcherItem& GetItem(int i) const;
    cbSwitcherItem& GetItem(int i);

    int GetItemCount() const { return m_items.GetCount(); }

    void SetRowCount(int rows) { m_rowCount = rows; }
    int GetRowCount() const { return m_rowCount; }

    void SetColumnCount(int cols) { m_columnCount = cols; }
    int GetColumnCount() const { return m_columnCount; }

    void SetBackgroundColour(const wxColour& colour) { m_backgroundColour = colour; }
    const wxColour& GetBackgroundColour() const { return m_backgroundColour; }

    void SetTextColour(const wxColour& colour) { m_textColour = colour; }
    const wxColour& GetTextColour() const { return m_textColour; }

    void SetSelectionColour(const wxColour& colour) { m_selectionColour = colour; }
    const wxColour& GetSelectionColour() const { return m_selectionColour; }

    void SetSelectionOutlineColour(const wxColour& colour) { m_selectionOutlineColour = colour; }
    const wxColour& GetSelectionOutlineColour() const { return m_selectionOutlineColour; }

    void SetSelectionTextColour(const wxColour& colour) { m_selectionTextColour = colour; }
    const wxColour& GetSelectionTextColour() const { return m_selectionTextColour; }

    void SetItemFont(const wxFont& font) { m_itemFont = font; }
    const wxFont& GetItemFont() const { return m_itemFont; }

    // Implementation

    void PaintItems(wxDC& dc, wxWindow* win);
    wxSize CalculateItemSize(wxDC& dc);

protected:

    cbSwitcherItemArray m_items;
    int                 m_selection;
    int                 m_rowCount;
    int                 m_columnCount;

    wxColour            m_backgroundColour;
    wxColour            m_textColour;
    wxColour            m_selectionColour;
    wxColour            m_selectionOutlineColour;
    wxColour            m_selectionTextColour;

    wxFont              m_itemFont;
};


/*!
 * cbMultiColumnListCtrl
 * A control for displaying several columns (not scrollable)
 */

class cbMultiColumnListCtrl: public wxControl
{
    DECLARE_CLASS(cbMultiColumnListCtrl)
public:
    cbMultiColumnListCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0);
    cbMultiColumnListCtrl() { Init(); }

    bool Create(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0);

    // Public API

    void SetItems(const cbSwitcherItems& items) { m_items = items; }
    const cbSwitcherItems& GetItems() const { return m_items; }
    cbSwitcherItems& GetItems() { return m_items; }

    // Set an extra key that can be used to cycle through items,
    // in case not using the Ctrl+Tab combination
    void SetExtraNavigationKey(int keyCode) { m_extraNavigationKey = keyCode; }
    int GetExtraNavigationKey() const { return m_extraNavigationKey; }

    // Set the modifier used to invoke the dialog, and therefore to test for
    // release
    void SetModifierKey(int modifierKey) { m_modifierKey = modifierKey; }
    int GetModifierKey() const { return m_modifierKey; }

    // Event handlers

    void OnPaint(wxPaintEvent& event);
    void OnMouseEvent(wxMouseEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnKey(wxKeyEvent& event);
    void OnEraseBackground(wxEraseEvent& event);

    // Overrides
    wxSize DoGetBestSize() const override;

    // Implementation

    void CalculateLayout();
    void CalculateLayout(wxDC& dc);
    void InvalidateLayout() { m_items.SetColumnCount(0); Refresh(); }
    void Init();
    void GenerateSelectionEvent();
    void AdvanceToNextSelectableItem(int direction);
    void SendCloseEvent();

protected:

    /**
     * Mouse point initialized on Init function.
     *
     * Used because if the mouse is over the dialog when it's opened, it automatically
     * select the item under the mouse pointer.
     * Recording the mouse pointer position when the dialog is opened prevent this :
     * select the item only if the user move the mouse.
    */
    wxPoint             m_ptMouse;
    cbSwitcherItems     m_items;
    wxSize              m_overallSize;
    int                 m_extraNavigationKey;
    int                 m_modifierKey;

DECLARE_EVENT_TABLE()
};


/*!
 * cbSwitcherDialog shows a cbMultiColumnListCtrl with a list of panes
 * and tabs for the user to choose. Ctrl+Tab cycles through them.
 */

class cbSwitcherDialog: public wxScrollingDialog
{
public:
    // constructors and destructors
    cbSwitcherDialog() { Init(); }
    cbSwitcherDialog( const cbSwitcherItems& items, wxWindow *parent,
        wxWindowID id = -1, const wxString& title = _("Pane Switcher"),
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxSTAY_ON_TOP|wxDIALOG_NO_PARENT|wxBORDER_SIMPLE );

    bool Create(const cbSwitcherItems& items, wxWindow *parent,
        wxWindowID id = -1, const wxString& title = _("Pane Switcher"),
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxSTAY_ON_TOP|wxDIALOG_NO_PARENT|wxBORDER_SIMPLE );

    void Init();

    void OnCloseWindow(wxCloseEvent& event);
    void OnActivate(wxActivateEvent& event);
    void OnSelectItem(wxCommandEvent& event);
    void OnPaint(wxPaintEvent& event);

    // Get the selected item
    int GetSelection() const;

    void ShowDescription(int i);

    void SetBorderColour(const wxColour& colour) { m_borderColour = colour; }

    // Set an extra key that can be used to cycle through items,
    // in case not using the Ctrl+Tab combination
    void SetExtraNavigationKey(int keyCode);
    int GetExtraNavigationKey() const { return m_extraNavigationKey; }

    // Set the modifier used to invoke the dialog, and therefore to test for
    // release
    void SetModifierKey(int modifierKey);
    int GetModifierKey() const { return m_modifierKey; }

private:

    cbMultiColumnListCtrl*  m_listCtrl;
    wxHtmlWindow*           m_descriptionCtrl;
    bool                    m_closing;
    long                    m_switcherBorderStyle;
    wxColour                m_borderColour;
    int                     m_extraNavigationKey;
    int                     m_modifierKey;

    DECLARE_EVENT_TABLE()
};


#endif
    // _WX_SWITCHERDLG_H_

