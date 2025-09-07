/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#include "ccmanager.h"

#ifndef CB_PRECOMP
    #include <algorithm>

    #include <wx/listctrl.h>
    #include <wx/menu.h>
#if wxUSE_POPUPWIN
    #include <wx/popupwin.h>
#endif
    #include <wx/timer.h>

    #include "cbeditor.h"
    #include "configmanager.h"
    #include "editormanager.h"
    #include "logmanager.h" // for F
#endif

#include <wx/html/htmlwin.h>
#include <wx/display.h>

#include "cbcolourmanager.h"
#include "cbstyledtextctrl.h"
#include "editor_hooks.h"
#include "debuggermanager.h"

namespace CCManagerHelper
{
    // shift points if they are past the insertion/deletion point
    inline void RipplePts(int& ptA, int& ptB, int len, int delta)
    {
        if (ptA > len - delta)
            ptA += delta;

        if (ptB > len - delta)
            ptB += delta;
    }

    // wxScintilla::FindColumn seems to be broken; re-implement:
    // Find the position of a column on a line taking into account tabs and
    // multi-byte characters. If beyond end of line, return line end position.
    inline int FindColumn(int line, int column, wxScintilla* stc)
    {
        const int lnEnd = stc->GetLineEndPosition(line);
        for (int pos = stc->PositionFromLine(line); pos < lnEnd; ++pos)
        {
            if (stc->GetColumn(pos) == column)
                return pos;
        }
        return lnEnd;
    }

    // test if an editor position is displayed
    inline bool IsPosVisible(int pos, wxScintilla* stc)
    {
        const int dist = stc->VisibleFromDocLine(stc->LineFromPosition(pos)) - stc->GetFirstVisibleLine();
        return !(dist < 0 || dist > stc->LinesOnScreen()); // caret is off screen
    }

    // return a hash of a calltip context (to avoid storing strings of each calltip)
    // used in m_CallTipChoiceDict and m_CallTipFuzzyChoiceDict
    static int CallTipToInt(const wxString& firstTip, int numPages)
    {
        int val = 33 * firstTip.length() ^ numPages;
        for (wxString::const_iterator itr = firstTip.begin();
             itr != firstTip.end(); ++itr)
        {
            val = 33 * val ^ static_cast<int>(*itr);
        }
        return val;
    }

    // (shamelessly stolen from mime handler plugin ;) )
    // build all HTML font sizes (1..7) from the given base size
    static void BuildFontSizes(int *sizes, int size)
    {
        // using a fixed factor (1.2, from CSS2) is a bad idea as explained at
        // http://www.w3.org/TR/CSS21/fonts.html#font-size-props but this is by far
        // simplest thing to do so still do it like this for now
        sizes[0] = int(size * 0.75); // exception to 1.2 rule, otherwise too small
        sizes[1] = int(size * 0.83);
        sizes[2] = size;
        sizes[3] = int(size * 1.2);
        sizes[4] = int(size * 1.44);
        sizes[5] = int(size * 1.73);
        sizes[6] = int(size * 2);
    }

    // (shamelessly stolen from mime handler plugin ;) )
    static int GetDefaultHTMLFontSize()
    {
        // base the default font size on the size of the default system font but
        // also ensure that we have a font of reasonable size, otherwise small HTML
        // fonts are unreadable
        const int size = wxNORMAL_FONT->GetPointSize();
        return (size < 9) ? 9 : size;
    }
}

template<> CCManager* Mgr<CCManager>::instance = nullptr;
template<> bool Mgr<CCManager>::isShutdown = false;

const int idCallTipTimer = wxNewId();
const int idAutoLaunchTimer = wxNewId();
const int idAutocompSelectTimer = wxNewId();
const int idShowTooltip = wxNewId();
const int idCallTipNext = wxNewId();
const int idCallTipPrevious = wxNewId();

DEFINE_EVENT_TYPE(cbEVT_DEFERRED_CALLTIP_SHOW)
DEFINE_EVENT_TYPE(cbEVT_DEFERRED_CALLTIP_CANCEL)

// milliseconds
#define CALLTIP_REFRESH_DELAY 90
#define AUTOCOMP_SELECT_DELAY 35
#define SCROLL_REFRESH_DELAY 500

/** the CC tooltip options in Editor setting dialog */
enum TooltipMode
{
    tmDisable = 0,
    tmEnable,
    tmForceSinglePage,
    tmKeyboundOnly
};

/** FROM_TIMER means the event is automatically fired from the ccmanager, not explicitly called
 *  by the user. For example, if the code suggestion is fired by the client code, such as:
 * @code
 * CodeBlocksEvent evt(cbEVT_COMPLETE_CODE);
 * Manager::Get()->ProcessEvent(evt);
 * @endcode
 * Then the event has int value 0.
 */
#define FROM_TIMER 1

//{ Unfocusable popup

// imported with small changes from PlatWX.cpp
class UnfocusablePopupWindow :
#if wxUSE_POPUPWIN
    public wxPopupWindow
#else
    public wxFrame
#endif // wxUSE_POPUPWIN
{
public:
#if wxUSE_POPUPWIN
    typedef wxPopupWindow BaseClass;

    UnfocusablePopupWindow(wxWindow* parent, int style = wxBORDER_NONE) :
        wxPopupWindow(parent, style)
#else
    typedef wxFrame BaseClass;

    UnfocusablePopupWindow(wxWindow* parent, int style = 0) :
        wxFrame(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                style | wxFRAME_NO_TASKBAR | wxFRAME_FLOAT_ON_PARENT | wxNO_BORDER | wxFRAME_SHAPED
#ifdef __WXMAC__
                | wxPOPUP_WINDOW
#endif // __WXMAC__
                )
#endif // wxUSE_POPUPWIN
    {
        Hide();
    }

    bool Destroy() override;
    void OnFocus(wxFocusEvent& event);
    void ActivateParent();

    void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO) override;
    bool Show(bool show = true) override;

private:
    DECLARE_EVENT_TABLE()
};

// On OSX and (possibly others) there can still be pending
// messages/events for the list control when Scintilla wants to
// close it, so do a pending delete of it instead of destroying
// immediately.
bool UnfocusablePopupWindow::Destroy()
{
#ifdef __WXMAC__
    // The bottom edge of this window is not getting properly
    // refreshed upon deletion, so help it out...
    wxWindow* p = GetParent();
    wxRect r(GetPosition(), GetSize());
    r.SetHeight(r.GetHeight()+1);
    p->Refresh(false, &r);
#endif
    if ( !wxPendingDelete.Member(this) )
        wxPendingDelete.Append(this);

    return true;
}

void UnfocusablePopupWindow::OnFocus(wxFocusEvent& event)
{
    ActivateParent();
    GetParent()->SetFocus();
    event.Skip();
}

void UnfocusablePopupWindow::ActivateParent()
{
    // Although we're a frame, we always want the parent to be active, so
    // raise it whenever we get shown, focused, etc.
    wxTopLevelWindow *frame = wxDynamicCast(wxGetTopLevelParent(GetParent()), wxTopLevelWindow);
    if (frame)
        frame->Raise();
}

void UnfocusablePopupWindow::DoSetSize(int x, int y,
                       int width, int height,
                       int sizeFlags)
{
    // convert coords to screen coords since we're a top-level window
    if (x != wxDefaultCoord)
        GetParent()->ClientToScreen(&x, nullptr);

    if (y != wxDefaultCoord)
        GetParent()->ClientToScreen(nullptr, &y);

    BaseClass::DoSetSize(x, y, width, height, sizeFlags);
}

bool UnfocusablePopupWindow::Show(bool show)
{
    const bool rv = BaseClass::Show(show);
    if (rv && show)
        ActivateParent();
#ifdef __WXMAC__
    GetParent()->Refresh(false);
#endif
    return rv;
}

BEGIN_EVENT_TABLE(UnfocusablePopupWindow, UnfocusablePopupWindow::BaseClass)
    EVT_SET_FOCUS(UnfocusablePopupWindow::OnFocus)
END_EVENT_TABLE()

//} end Unfocusable popup


// class constructor
CCManager::CCManager() :
    m_AutocompPosition(wxSCI_INVALID_POSITION),
    m_CallTipActive(wxSCI_INVALID_POSITION),
    m_LastAutocompIndex(wxNOT_FOUND),
    m_LastTipPos(wxSCI_INVALID_POSITION),
    m_WindowBound(0),
    m_OwnsAutocomp(true),
    m_CallTipTimer(this, idCallTipTimer),
    m_AutoLaunchTimer(this, idAutoLaunchTimer),
    m_AutocompSelectTimer(this, idAutocompSelectTimer),
#ifdef __WXMSW__
    m_pAutocompPopup(nullptr),
#endif // __WXMSW__
    m_pLastEditor(nullptr),
    m_pLastCCPlugin(nullptr)
{
    const wxString ctChars(",;\n()"); // default set
    m_CallTipChars[nullptr] = std::set <wxChar> (ctChars.begin(), ctChars.end());
    const wxString alChars(".:<>\"#/"); // default set
    m_AutoLaunchChars[nullptr] = std::set <wxChar> (alChars.begin(), alChars.end());

    m_LastACLaunchState.init(wxSCI_INVALID_POSITION, wxSCI_INVALID_POSITION, 0);

    // init documentation popup
    m_pPopup = new UnfocusablePopupWindow(Manager::Get()->GetAppFrame());
    m_pHtml = new wxHtmlWindow(m_pPopup, wxID_ANY, wxDefaultPosition,
                               wxDefaultSize, wxHW_SCROLLBAR_AUTO | wxBORDER_SIMPLE);
    int sizes[7];
    CCManagerHelper::BuildFontSizes(sizes, CCManagerHelper::GetDefaultHTMLFontSize());
    m_pHtml->SetFonts(wxEmptyString, wxEmptyString, sizes);
    m_pHtml->Connect(wxEVT_COMMAND_HTML_LINK_CLICKED,
                     wxHtmlLinkEventHandler(CCManager::OnHtmlLink), nullptr, this);

    // register colours
    ColourManager* cmgr = Manager::Get()->GetColourManager();
    cmgr->RegisterColour(_("Code completion"), _("Tooltip/Calltip background"), "cc_tips_back",      *wxWHITE);
    cmgr->RegisterColour(_("Code completion"), _("Tooltip/Calltip foreground"), "cc_tips_fore",      wxColour("DIM GREY"));
    cmgr->RegisterColour(_("Code completion"), _("Tooltip/Calltip highlight"),  "cc_tips_highlight", wxColour("BLUE"));

    // connect menus
    wxFrame* mainFrame = Manager::Get()->GetAppFrame();
    wxMenuBar* menuBar = mainFrame->GetMenuBar();
    if (menuBar)
    {
        const int idx = menuBar->FindMenu(_("&Edit"));
        wxMenu* edMenu = (idx != wxNOT_FOUND) ? menuBar->GetMenu(idx) : nullptr;
        if (edMenu) 
        {
            const wxMenuItemList& itemsList = edMenu->GetMenuItems();
            size_t insertPos = itemsList.GetCount();   //// ERROR HERE
            for (size_t i = 0; i < insertPos; ++i)
            {
                if (itemsList[i]->GetItemLabel() == _("Complete code"))
                {
                    insertPos = i + 1;
                    break;
                }
            }
            // insert after Edit->Complete code
            edMenu->Insert(insertPos,     idShowTooltip,     _("Show tooltip\tShift-Alt-Space"));
            edMenu->Insert(insertPos + 1, idCallTipNext,     _("Next call tip"));
            edMenu->Insert(insertPos + 2, idCallTipPrevious, _("Previous call tip"));                        
        }
        
    }

    mainFrame->Connect(idShowTooltip,     wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CCManager::OnMenuSelect), nullptr, this);
    mainFrame->Connect(idCallTipNext,     wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CCManager::OnMenuSelect), nullptr, this);
    mainFrame->Connect(idCallTipPrevious, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CCManager::OnMenuSelect), nullptr, this);

    // connect events
    typedef cbEventFunctor<CCManager, CodeBlocksEvent> CCEvent;
    Manager::Get()->RegisterEventSink(cbEVT_APP_DEACTIVATED,    new CCEvent(this, &CCManager::OnDeactivateApp));
    Manager::Get()->RegisterEventSink(cbEVT_EDITOR_DEACTIVATED, new CCEvent(this, &CCManager::OnDeactivateEd));
    Manager::Get()->RegisterEventSink(cbEVT_EDITOR_OPEN,        new CCEvent(this, &CCManager::OnEditorOpen));
    Manager::Get()->RegisterEventSink(cbEVT_EDITOR_CLOSE,       new CCEvent(this, &CCManager::OnEditorClose));
    Manager::Get()->RegisterEventSink(cbEVT_EDITOR_TOOLTIP,     new CCEvent(this, &CCManager::OnEditorTooltip));
    Manager::Get()->RegisterEventSink(cbEVT_SHOW_CALL_TIP,      new CCEvent(this, &CCManager::OnShowCallTip));
    Manager::Get()->RegisterEventSink(cbEVT_COMPLETE_CODE,      new CCEvent(this, &CCManager::OnCompleteCode));
    m_EditorHookID = EditorHooks::RegisterHook(new EditorHooks::HookFunctor<CCManager>(this, &CCManager::OnEditorHook));
    Connect(idCallTipTimer,        wxEVT_TIMER, wxTimerEventHandler(CCManager::OnTimer));
    Connect(idAutoLaunchTimer,     wxEVT_TIMER, wxTimerEventHandler(CCManager::OnTimer));
    Connect(idAutocompSelectTimer, wxEVT_TIMER, wxTimerEventHandler(CCManager::OnTimer));
    Connect(cbEVT_DEFERRED_CALLTIP_SHOW,   wxCommandEventHandler(CCManager::OnDeferredCallTipShow));
    Connect(cbEVT_DEFERRED_CALLTIP_CANCEL, wxCommandEventHandler(CCManager::OnDeferredCallTipCancel));
}

// class destructor
CCManager::~CCManager()
{
    m_pHtml->Disconnect(wxEVT_COMMAND_HTML_LINK_CLICKED, wxHtmlLinkEventHandler(CCManager::OnHtmlLink), nullptr, this);
    m_pHtml->Destroy();
    m_pPopup->Destroy();
    wxFrame* mainFrame = Manager::Get()->GetAppFrame();
    mainFrame->Disconnect(idShowTooltip,     wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CCManager::OnMenuSelect), nullptr, this);
    mainFrame->Disconnect(idCallTipNext,     wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CCManager::OnMenuSelect), nullptr, this);
    mainFrame->Disconnect(idCallTipPrevious, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(CCManager::OnMenuSelect), nullptr, this);
    Manager::Get()->RemoveAllEventSinksFor(this);
    EditorHooks::UnregisterHook(m_EditorHookID, true);
    Disconnect(idCallTipTimer);
    Disconnect(idAutoLaunchTimer);
    Disconnect(idAutocompSelectTimer);
    Disconnect(cbEVT_DEFERRED_CALLTIP_SHOW);
    Disconnect(cbEVT_DEFERRED_CALLTIP_CANCEL);
}

cbCodeCompletionPlugin* CCManager::GetProviderFor(cbEditor* ed)
{
    if (!ed)
        ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();

    if (ed == m_pLastEditor)
        return m_pLastCCPlugin; // use cached

    m_pLastEditor = ed;
    m_pLastCCPlugin = nullptr;
    m_LastACLaunchState.caretStart = wxSCI_INVALID_POSITION;
    const PluginsArray& pa = Manager::Get()->GetPluginManager()->GetCodeCompletionOffers();
    for (size_t i = 0; i < pa.GetCount(); ++i)
    {
        cbCodeCompletionPlugin::CCProviderStatus status = static_cast<cbCodeCompletionPlugin*>(pa[i])->GetProviderStatusFor(ed);
        if (status == cbCodeCompletionPlugin::ccpsActive)
        {
            m_pLastCCPlugin = static_cast<cbCodeCompletionPlugin*>(pa[i]);
            break;
        }
        else if (status == cbCodeCompletionPlugin::ccpsUniversal)
            m_pLastCCPlugin = static_cast<cbCodeCompletionPlugin*>(pa[i]);
    }

    return m_pLastCCPlugin;
}

void CCManager::RegisterCallTipChars(const wxString& chars, cbCodeCompletionPlugin* registrant)
{
    if (registrant)
        m_CallTipChars[registrant] = std::set<wxChar>(chars.begin(), chars.end());
}

void CCManager::RegisterAutoLaunchChars(const wxString& chars, cbCodeCompletionPlugin* registrant)
{
    if (registrant)
        m_AutoLaunchChars[registrant] = std::set<wxChar>(chars.begin(), chars.end());
}

void CCManager::NotifyDocumentation()
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        DoShowDocumentation(ed);
}

void CCManager::NotifyPluginStatus()
{
    m_pLastEditor   = nullptr;
    m_pLastCCPlugin = nullptr;
}

void CCManager::InjectAutoCompShow(int lenEntered, const wxString& itemList)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        ed->GetControl()->AutoCompShow(lenEntered, itemList);
        m_OwnsAutocomp = false;
        m_AutocompTokens.clear();
    }
}

// Change the current call tip to be the next or the previous.
// Do wrapping if the end is reached in both directions.
void CCManager::AdvanceTip(Direction direction)
{
    if (direction == Next)
    {
        ++m_CurCallTip;
        if (m_CurCallTip == m_CallTips.end())
            m_CurCallTip = m_CallTips.begin();
    }
    else
    {
        if (m_CurCallTip == m_CallTips.begin())
        {
            if (m_CallTips.size() > 1)
                m_CurCallTip = m_CallTips.begin() + m_CallTips.size() - 1;
        }
        else
            --m_CurCallTip;
    }
}

bool CCManager::ProcessArrow(int key)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return false;

    cbStyledTextCtrl* stc = ed->GetControl();
    bool wasProcessed = false;
    if (stc->CallTipActive() && m_CallTipActive != wxSCI_INVALID_POSITION && m_CallTips.size() > 1)
    {
        if (key == WXK_DOWN)
            AdvanceTip(Next);
        else if (key == WXK_UP)
            AdvanceTip(Previous);
        else
            return false;

        DoUpdateCallTip(ed);
        wasProcessed = true;
    }

    return wasProcessed;
}

// priority, then alphabetical
struct TokenSorter
{
    bool& m_PureAlphabetical;  // modify the passed argument(set to false) if weight are different
    bool m_CaseSensitive;

    TokenSorter(bool& alphabetical, bool caseSensitive): m_PureAlphabetical(alphabetical), m_CaseSensitive(caseSensitive)
    {
        m_PureAlphabetical = true;
    }

    bool operator()(const cbCodeCompletionPlugin::CCToken& a, const cbCodeCompletionPlugin::CCToken& b)
    {
        int diff = a.weight - b.weight;
        if (diff == 0)
        {
            if (m_CaseSensitive)
                diff = a.displayName.Cmp(b.displayName);
            else
            {   // cannot use CmpNoCase() because it compares lower case but Scintilla compares upper
                diff = a.displayName.Upper().Cmp(b.displayName.Upper());
                if (diff == 0)
                    diff = a.displayName.Cmp(b.displayName);
            }
        }
        else
            m_PureAlphabetical = false;

        return diff < 0;
    }
};

// cbEVT_COMPLETE_CODE
void CCManager::OnCompleteCode(CodeBlocksEvent& event)
{
    event.Skip();
    ConfigManager* cfg = Manager::Get()->GetConfigManager("ccmanager");
    if (!cfg->ReadBool("/code_completion", true))
        return;

    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbCodeCompletionPlugin* ccPlugin = GetProviderFor(ed);
    if (!ccPlugin)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    int tknEnd = stc->GetCurrentPos();
    int tknStart = stc->WordStartPosition(tknEnd, true);
    wxString trigger = stc->GetTextRange(tknStart, tknEnd);
    if (tknEnd == m_LastACLaunchState.caretStart && stc->GetZoom() == m_LastACLaunchState.editorZoom && !m_AutocompTokens.empty())
    {
        DoBufferedCC(stc);
        // If the completion trigger is the same as last, the cached completions have already been shown
        // else they've just been cached, but not yet shown.
        if (m_LastACLaunchState.trigger.Length() and (m_LastACLaunchState.trigger == trigger))
            return;
    }
    // Record the new completion trigger
    m_LastACLaunchState.trigger = stc->GetTextRange(tknStart,tknEnd);

    m_AutocompTokens = ccPlugin->GetAutocompList(event.GetInt() == FROM_TIMER, ed, tknStart, tknEnd);
    if (m_AutocompTokens.empty())
        return;

    if (m_AutocompTokens.size() == 1 && cfg->ReadBool("/auto_select_single", false))
    {
        // Using stc->AutoCompSetChooseSingle() does not send wxEVT_SCI_AUTOCOMP_SELECTION,
        // so manually emulate the behaviour.

        if (!stc->CallTipActive() && !stc->AutoCompActive())
            m_CallTipActive = wxSCI_INVALID_POSITION;

        m_OwnsAutocomp = true;
        m_LastACLaunchState.init(tknEnd, tknStart, stc->GetZoom());
        m_LastAutocompIndex = 0;

        wxScintillaEvent autoCompFinishEvt(wxEVT_SCI_AUTOCOMP_SELECTION);
        autoCompFinishEvt.SetText(m_AutocompTokens.front().displayName);

        OnEditorHook(ed, autoCompFinishEvt);

        return;
    }

    bool isPureAlphabetical = true;
    bool isCaseSensitive = cfg->ReadBool("/case_sensitive", false);
    TokenSorter sortFunctor(isPureAlphabetical, isCaseSensitive);
    std::sort(m_AutocompTokens.begin(), m_AutocompTokens.end(), sortFunctor);
    if (isPureAlphabetical)
        stc->AutoCompSetOrder(wxSCI_ORDER_PRESORTED);
    else
        stc->AutoCompSetOrder(wxSCI_ORDER_CUSTOM);

    wxString items;
    // experimentally, the average length per token seems to be 23 for the main CC plugin
    items.Alloc(m_AutocompTokens.size() * 20); // TODO: measure performance
    for (size_t i = 0; i < m_AutocompTokens.size(); ++i)
    {
        items += m_AutocompTokens[i].displayName;
        if (m_AutocompTokens[i].category == -1)
            items += '\r';
        else
            items += wxString::Format("\n%d\r", m_AutocompTokens[i].category);
    }
    items.RemoveLast();

    if (!stc->CallTipActive() && !stc->AutoCompActive())
        m_CallTipActive = wxSCI_INVALID_POSITION;

    stc->AutoCompSetIgnoreCase(!isCaseSensitive);
    stc->AutoCompSetMaxHeight(14);
    stc->AutoCompSetTypeSeparator('\n');
    stc->AutoCompSetSeparator('\r');
    stc->AutoCompShow(tknEnd - tknStart, items);
    m_OwnsAutocomp = true;
    if (isPureAlphabetical)
    {
        const wxString& contextStr = stc->GetTextRange(tknStart, stc->WordEndPosition(tknEnd, true));
        std::vector<cbCodeCompletionPlugin::CCToken>::const_iterator tknIt
                = std::lower_bound(m_AutocompTokens.begin(), m_AutocompTokens.end(),
                                   cbCodeCompletionPlugin::CCToken(-1, contextStr),
                                   sortFunctor);
        if (tknIt != m_AutocompTokens.end() && tknIt->displayName.StartsWith(contextStr))
            stc->AutoCompSelect(tknIt->displayName);
    }

    m_LastACLaunchState.init(tknEnd, tknStart, stc->GetZoom());
}

// cbEVT_APP_DEACTIVATED
void CCManager::OnDeactivateApp(CodeBlocksEvent& event)
{
    DoHidePopup();
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        if (stc->CallTipActive())
        {
            // calling 'stc->CallTipCancel()' directly can cause crashes for some users due to:
            // http://forums.codeblocks.org/index.php/topic,19117.msg130969.html#msg130969
            wxCommandEvent pendingCancel(cbEVT_DEFERRED_CALLTIP_CANCEL);
            AddPendingEvent(pendingCancel);
        }

        m_CallTipActive = wxSCI_INVALID_POSITION;
    }

    event.Skip();
}

// cbEVT_EDITOR_DEACTIVATED
void CCManager::OnDeactivateEd(CodeBlocksEvent& event)
{
    DoHidePopup();
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());
    if (ed)
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        if (stc->CallTipActive())
            stc->CallTipCancel();

        m_CallTipActive = wxSCI_INVALID_POSITION;
    }

    event.Skip();
}

static void setupColours(cbEditor *editor, ColourManager *manager)
{
    cbStyledTextCtrl* stc = editor->GetControl();
    stc->CallTipSetBackground(manager->GetColour("cc_tips_back"));
    stc->CallTipSetForeground(manager->GetColour("cc_tips_fore"));
    stc->CallTipSetForegroundHighlight(manager->GetColour("cc_tips_highlight"));
}

// cbEVT_EDITOR_OPEN
void CCManager::OnEditorOpen(CodeBlocksEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());
    if (ed)
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        stc->Connect(wxEVT_COMMAND_LIST_ITEM_SELECTED,
                     wxListEventHandler(CCManager::OnAutocompleteSelect), nullptr, this);

        setupColours(ed, Manager::Get()->GetColourManager());
    }
}

void CCManager::UpdateEnvSettings()
{
    EditorManager *editors = Manager::Get()->GetEditorManager();
    ColourManager* cmgr = Manager::Get()->GetColourManager();

    const int count = editors->GetEditorsCount();
    for (int ii = 0; ii < count; ++ii)
    {
        cbEditor *editor = editors->GetBuiltinEditor(editors->GetEditor(ii));
        if (editor)
            setupColours(editor, cmgr);
    }
}

// cbEVT_EDITOR_CLOSE
void CCManager::OnEditorClose(CodeBlocksEvent& event)
{
    DoHidePopup();
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinEditor(event.GetEditor());
    if (ed == m_pLastEditor)
        m_pLastEditor = nullptr;

    #if defined(__WXMSW__)
    // If the closing editor holds a popup wxEVT_MOUSEWHEEL connect, disconnect it.
    // DoHidePopup() above may have disconnected m_LastEditor but not the closing editor.
    // This happens when a non-active editor is closed while m_pLastEditor == the active editor.
    // This may not happen anymore, but it's happened in the past and CB hung. 2022/09/17
    if (ed and ed->GetControl() and m_EdAutocompMouseTraps.count(ed) )
    {
        ed->GetControl()->Disconnect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(CCManager::OnPopupScroll), nullptr, this);
        m_EdAutocompMouseTraps.erase(ed);
    }
    #endif

    if (ed && ed->GetControl())
    {
        // TODO: is this ever called?
        ed->GetControl()->Disconnect(wxEVT_COMMAND_LIST_ITEM_SELECTED,
                                     wxListEventHandler(CCManager::OnAutocompleteSelect), nullptr, this);
    }
}

// cbEVT_EDITOR_TOOLTIP
void CCManager::OnEditorTooltip(CodeBlocksEvent& event)
{
    event.Skip();

    const int tooltipMode = Manager::Get()->GetConfigManager("ccmanager")->ReadInt("/tooltip_mode", 1);
    if (tooltipMode == tmDisable) // disabled
        return;

    // if the event comes from user menu click, then its String field isn't empty
    // @see CCManager::OnMenuSelect() for details
    const bool fromMouseDwell = event.GetString().empty();
    if (wxGetKeyState(WXK_CONTROL) && fromMouseDwell)
        return;

    if (tooltipMode == tmKeyboundOnly && fromMouseDwell) // keybound only
        return;

    EditorBase* base = event.GetEditor();
    cbEditor* ed = base && base->IsBuiltinEditor() ? static_cast<cbEditor*>(base) : nullptr;
    if (!ed || ed->IsContextMenuOpened())
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    cbCodeCompletionPlugin* ccPlugin = GetProviderFor(ed);
    const int pos = stc->PositionFromPointClose(event.GetX(), event.GetY());
    if (!ccPlugin || pos < 0 || pos >= stc->GetLength())
    {
        if (stc->CallTipActive() && event.GetExtraLong() == 0 && m_CallTipActive == wxSCI_INVALID_POSITION)
            static_cast<wxScintilla*>(stc)->CallTipCancel();

        return;
    }

    int hlStart, hlEnd, argsPos;
    hlStart = hlEnd = argsPos = wxSCI_INVALID_POSITION;
    bool allowCallTip = true;
    const std::vector<cbCodeCompletionPlugin::CCToken>& tokens = ccPlugin->GetTokenAt(pos, ed, allowCallTip);
    std::set<wxString> uniqueTips;
    for (size_t i = 0; i < tokens.size(); ++i)
        uniqueTips.insert(tokens[i].displayName);

    wxStringVec tips(uniqueTips.begin(), uniqueTips.end());

    // If user specified "~NoSort~" as first token entry, reset tips as the user specified them.
    if (tokens.size() and (tokens[0].displayName == "~NoSort~"))
    {
        tips.clear();
        for (size_t i = 1; i < tokens.size(); ++i)
            tips.push_back( tokens[i].displayName);
    }

    const int style = event.GetInt();
    if (!tips.empty())
    {
        const int tknStart = stc->WordStartPosition(pos, true);
        const int tknEnd   = stc->WordEndPosition(pos,   true);
        if (tknEnd - tknStart > 2)
        {
            for (size_t i = 0; i < tips[0].length(); ++i)
            {
                const size_t hlLoc = tips[0].find(stc->GetTextRange(tknStart, tknEnd), i);
                if (hlLoc == wxString::npos)
                    break;

                hlStart = hlLoc;
                hlEnd = hlStart + tknEnd - tknStart;
                if (   (hlStart > 0 && (tips[0][hlStart - 1] == '_' || wxIsalpha(tips[0][hlStart - 1])))
                    || (hlEnd < static_cast<int>(tips[0].length()) - 1 && (tips[0][hlEnd] == '_' || wxIsalpha(tips[0][hlEnd]))) )
                {
                    i = hlEnd;
                    hlStart = hlEnd = wxSCI_INVALID_POSITION;
                }
                else
                    break;
            }
        }
    }
    else if (  allowCallTip
             && !(   stc->IsString(style)
                  || stc->IsComment(style)
                  || stc->IsCharacter(style)
                  || stc->IsPreprocessor(style) ) )
    {
        const int line = stc->LineFromPosition(pos);
        if (pos + 4 > stc->PositionFromLine(line) + (int)ed->GetLineIndentString(line).length())
        {
            const CallTipVec& cTips = ccPlugin->GetCallTips(pos, style, ed, argsPos);
            for (size_t i = 0; i < cTips.size(); ++i)
                tips.push_back(cTips[i].tip);

            if (!tips.empty())
            {
                hlStart = cTips[0].hlStart;
                hlEnd   = cTips[0].hlEnd;
            }
        }
    }

    if (tips.empty())
    {
        if (stc->CallTipActive() && event.GetExtraLong() == 0 && m_CallTipActive == wxSCI_INVALID_POSITION)
            static_cast<wxScintilla*>(stc)->CallTipCancel();
    }
    else
    {
        DoShowTips(tips, stc, pos, argsPos, hlStart, hlEnd);
        event.SetExtraLong(1);
    }

    m_CallTipActive = wxSCI_INVALID_POSITION;
}

void CCManager::OnEditorHook(cbEditor* ed, wxScintillaEvent& event)
{
    wxEventType evtType = event.GetEventType();
    if (evtType == wxEVT_SCI_CHARADDED)
    {
        const wxChar ch = event.GetKey();
        CCPluginCharMap::const_iterator ctChars = m_CallTipChars.find(GetProviderFor(ed));
        if (ctChars == m_CallTipChars.end())
            ctChars = m_CallTipChars.find(nullptr); // default

        // Are there any characters which could trigger the call tip?
        if (ctChars->second.find(ch) != ctChars->second.end())
        {
            const int tooltipMode = Manager::Get()->GetConfigManager("ccmanager")->ReadInt("/tooltip_mode", 1);
            if (   tooltipMode != 3 // keybound only
                || m_CallTipActive != wxSCI_INVALID_POSITION )
            {
                wxCommandEvent pendingShow(cbEVT_DEFERRED_CALLTIP_SHOW);
                AddPendingEvent(pendingShow);
            }
        }
        else
        {
            cbStyledTextCtrl* stc = ed->GetControl();
            const int pos = stc->GetCurrentPos();
            const int wordStartPos = stc->WordStartPosition(pos, true);
            CCPluginCharMap::const_iterator alChars = m_AutoLaunchChars.find(GetProviderFor(ed));
            if (alChars == m_AutoLaunchChars.end())
                alChars = m_AutoLaunchChars.find(nullptr); // default

            // auto suggest list can be triggered either:
            // 1, some number of chars are entered
            // 2, an interested char belong to alChars is entered
            const int autolaunchCt = Manager::Get()->GetConfigManager("ccmanager")->ReadInt("/auto_launch_count", 3);
            if (   (pos - wordStartPos >= autolaunchCt && !stc->AutoCompActive())
                || pos - wordStartPos == autolaunchCt + 4 )
            {
                CodeBlocksEvent evt(cbEVT_COMPLETE_CODE);
                Manager::Get()->ProcessEvent(evt);
            }
            else if (alChars->second.find(ch) != alChars->second.end())
            {
                m_AutoLaunchTimer.Start(10, wxTIMER_ONE_SHOT);
                m_AutocompPosition = pos;
            }
        }
    }
    else if (evtType == wxEVT_SCI_UPDATEUI)
    {
        if (event.GetUpdated() & (wxSCI_UPDATE_V_SCROLL|wxSCI_UPDATE_H_SCROLL))
        {
            cbStyledTextCtrl* stc = ed->GetControl();
            if (stc->CallTipActive())
            {
                // force to call the wxScintilla::CallTipCancel to avoid the smart indent condition check
                // @see cbStyledTextCtrl::CallTipCancel() for the details
                static_cast<wxScintilla*>(stc)->CallTipCancel();
                if (m_CallTipActive != wxSCI_INVALID_POSITION && CCManagerHelper::IsPosVisible(m_CallTipActive, stc))
                    m_CallTipTimer.Start(SCROLL_REFRESH_DELAY, wxTIMER_ONE_SHOT);
            }
            else if (m_CallTipTimer.IsRunning())
            {
                if (CCManagerHelper::IsPosVisible(stc->GetCurrentPos(), stc))
                    m_CallTipTimer.Start(SCROLL_REFRESH_DELAY, wxTIMER_ONE_SHOT);
                else
                {
                    m_CallTipTimer.Stop();
                    m_CallTipActive = wxSCI_INVALID_POSITION;
                }
            }
            if (m_AutoLaunchTimer.IsRunning())
            {
                if (CCManagerHelper::IsPosVisible(stc->GetCurrentPos(), stc))
                    m_AutoLaunchTimer.Start(SCROLL_REFRESH_DELAY, wxTIMER_ONE_SHOT);
                else
                    m_AutoLaunchTimer.Stop();
            }
            else if (stc->AutoCompActive())
            {
                stc->AutoCompCancel();
                m_AutocompPosition = stc->GetCurrentPos();
                if (CCManagerHelper::IsPosVisible(m_AutocompPosition, stc))
                    m_AutoLaunchTimer.Start(SCROLL_REFRESH_DELAY, wxTIMER_ONE_SHOT);
            }
        }
    }
    else if (evtType == wxEVT_SCI_MODIFIED)
    {
        if (event.GetModificationType() & wxSCI_PERFORMED_UNDO)
        {
            cbStyledTextCtrl* stc = ed->GetControl();
            if (m_CallTipActive != wxSCI_INVALID_POSITION && stc->GetCurrentPos() >= m_CallTipActive)
                m_CallTipTimer.Start(CALLTIP_REFRESH_DELAY, wxTIMER_ONE_SHOT);
            else
                static_cast<wxScintilla*>(stc)->CallTipCancel();
        }
    }
    else if (evtType == wxEVT_SCI_AUTOCOMP_SELECTION)
    {
        DoHidePopup();
        cbCodeCompletionPlugin* ccPlugin = GetProviderFor(ed);
        if (ccPlugin && m_OwnsAutocomp)
        {
            if (   m_LastAutocompIndex != wxNOT_FOUND
                && m_LastAutocompIndex < (int)m_AutocompTokens.size() )
            {
                ccPlugin->DoAutocomplete(m_AutocompTokens[m_LastAutocompIndex], ed);
            }
            else // this case should not normally happen
            {
                ccPlugin->DoAutocomplete(event.GetText(), ed);
            }

            CallSmartIndentCCDone(ed);
        }
    }
    else if (evtType == wxEVT_SCI_AUTOCOMP_CANCELLED)
        DoHidePopup();
    else if (evtType == wxEVT_SCI_CALLTIP_CLICK)
    {
        switch (event.GetPosition())
        {
            case 1: // up
                AdvanceTip(Previous);
                DoUpdateCallTip(ed);
                break;

            case 2: // down
                AdvanceTip(Next);
                DoUpdateCallTip(ed);
                break;

            case 0: // elsewhere
            default:
                break;
        }
    }

    event.Skip();
}

// cbEVT_SHOW_CALL_TIP
// There are some caches used in this function
// see the comment "search long term recall" and "search short term recall"
// They remember which page on the calltip was last selected, and show that again first next time it
// is requested.
// Short term recall caches the current calltip, so it can be displayed exactly if a calltip is
// re-requested in the same location.
// Long term recall uses two dictionaries, the first one based on the tip's content, and will
// display the last shown page when a new calltip is requested that has the same content.
// The second (fuzzy) dictionary remembers which page was shown based on the typed word prefix
// (this is to support FortranProject, which uses more dynamic calltips).
void CCManager::OnShowCallTip(CodeBlocksEvent& event)
{
    event.Skip();

    const int tooltipMode = Manager::Get()->GetConfigManager("ccmanager")->ReadInt("/tooltip_mode", 1);
    // 0 - disable
    // 1 - enable
    // 2 - force single page
    // 3 - keybound only
    if (tooltipMode == tmDisable)
        return;

    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    cbCodeCompletionPlugin* ccPlugin = GetProviderFor(ed);
    if (!ccPlugin)
        return;

    cbStyledTextCtrl* stc = ed->GetControl();
    if (!stc)
        return;

    const int pos = stc->GetCurrentPos();
    int argsPos = wxSCI_INVALID_POSITION;
    // save the current tip shown text for later recalling
    wxString curTip;
    // check whether m_CurCallTip is invalid(point to the end of m_CallTips)
    if (!m_CallTips.empty() && m_CurCallTip != m_CallTips.end())
        curTip = m_CurCallTip->tip;

    m_CallTips = ccPlugin->GetCallTips(pos, stc->GetStyleAt(pos), ed, argsPos);
    // since m_CallTips get updated, we should update the m_CurCallTip, they are done in
    // the following if/else statement.
    if (!m_CallTips.empty() && (event.GetInt() != FROM_TIMER || argsPos == m_CallTipActive))
    {
        int lnStart = stc->PositionFromLine(stc->LineFromPosition(pos));
        while (wxIsspace(stc->GetCharAt(lnStart)))
            ++lnStart; // do not show too far left on multi-line call tips

        if (   m_CallTips.size() > 1
            && tooltipMode == tmForceSinglePage ) // force single page
        {
            wxString tip;
            int hlStart, hlEnd;
            hlStart = hlEnd = wxSCI_INVALID_POSITION;
            for (CallTipVec::const_iterator itr = m_CallTips.begin();
                 itr != m_CallTips.end(); ++itr)
            {
                if (hlStart == hlEnd && itr->hlStart != itr->hlEnd)
                {
                    hlStart = tip.length() + itr->hlStart;
                    hlEnd   = tip.length() + itr->hlEnd;
                }

                tip += itr->tip + '\n';
            }

            m_CallTips.clear();
            m_CallTips.push_back(cbCodeCompletionPlugin::CCCallTip(tip.RemoveLast(), hlStart, hlEnd));
        }

        m_CurCallTip = m_CallTips.begin();
        if (m_CallTips.size() > 1)
        {
            // search long term recall
            // convert the wxString to a hash value, then search the hash value map cache
            // thus we can avoid directly lookup the cache by wxString
            std::map<int, size_t>::const_iterator choiceItr =
                m_CallTipChoiceDict.find(CCManagerHelper::CallTipToInt(m_CurCallTip->tip, m_CallTips.size()));

            // choiceItr is an iterator in the Dict, and choiceItr->second is the zero based index in m_CallTips
            if (choiceItr != m_CallTipChoiceDict.end() && choiceItr->second < m_CallTips.size())
                m_CurCallTip = m_CallTips.begin() + choiceItr->second;

            if (choiceItr == m_CallTipChoiceDict.end() || argsPos == m_CallTipActive)
            {
                int prefixEndPos = argsPos;
                while (prefixEndPos > 0 && wxIsspace(stc->GetCharAt(prefixEndPos - 1)))
                    --prefixEndPos;

                // convert the prefix string to hash, check to see whether the hash is in the cache
                const wxString& prefix = stc->GetTextRange(stc->WordStartPosition(prefixEndPos, true), prefixEndPos);
                choiceItr = m_CallTipFuzzyChoiceDict.find(CCManagerHelper::CallTipToInt(prefix, m_CallTips.size()));
                if (choiceItr != m_CallTipFuzzyChoiceDict.end() && choiceItr->second < m_CallTips.size())
                    m_CurCallTip = m_CallTips.begin() + choiceItr->second;
            }
            // search short term recall
            for (CallTipVec::const_iterator itr = m_CallTips.begin();
                 itr != m_CallTips.end(); ++itr)
            {
                if (itr->tip == curTip)
                {
                    m_CurCallTip = itr;
                    break;
                }
            }
        }

        m_CallTipActive = argsPos;
        DoUpdateCallTip(ed);
    }
    else
    {
        if (m_CallTipActive != wxSCI_INVALID_POSITION)
        {
            static_cast<wxScintilla*>(stc)->CallTipCancel();
            m_CallTipActive = wxSCI_INVALID_POSITION;
        }

        // Make m_CurCallTip to be valid iterator, pointing to the end.
        m_CurCallTip = m_CallTips.end();
    }
}

void CCManager::OnAutocompleteSelect(wxListEvent& event)
{
    event.Skip();
    m_AutocompSelectTimer.Start(AUTOCOMP_SELECT_DELAY, wxTIMER_ONE_SHOT);
    wxObject* evtObj = event.GetEventObject();
    if (!evtObj)
        return;

#ifdef __WXMSW__
    m_pAutocompPopup = static_cast<wxListView*>(evtObj);
#endif // __WXMSW__

    wxWindow* evtWin = static_cast<wxWindow*>(evtObj)->GetParent();
    if (!evtWin)
        return;

    m_DocPos = m_pPopup->GetParent()->ScreenToClient(evtWin->GetScreenPosition());
    m_DocPos.x += evtWin->GetSize().x;
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    wxRect edRect(ed->GetRect());
    if (!m_pPopup->IsShown())
    {
        cbStyledTextCtrl* stc = ed->GetControl();
        const int acMaxHeight = stc->AutoCompGetMaxHeight() + 1;
        const int textHeight = stc->TextHeight(stc->GetCurrentLine());
        m_DocSize.x = edRect.width * 5 / 12;
        m_DocSize.y = acMaxHeight * textHeight;
        evtWin->Connect(wxEVT_SHOW, wxShowEventHandler(CCManager::OnAutocompleteHide), nullptr, this);

        const int idx = wxDisplay::GetFromWindow(evtWin);
        m_WindowBound = m_DocPos.x + m_DocSize.x;
        if (idx != wxNOT_FOUND)
        {
            const wxPoint& corner = m_pPopup->GetParent()->ScreenToClient(wxDisplay(idx).GetGeometry().GetBottomRight());
            m_DocSize.y = std::max(9 * textHeight,      std::min(m_DocSize.y, corner.y - m_DocPos.y - 2));
            m_DocSize.x = std::max(m_DocSize.y * 2 / 3, std::min(m_DocSize.x, corner.x - m_DocPos.x - 2));
            m_WindowBound = std::min(corner.x - 2, m_WindowBound);
        }
    }

    if ((m_DocPos.x + m_DocSize.x) > m_WindowBound)
        m_DocPos.x -= evtWin->GetSize().x + m_DocSize.x; // show to the left instead
    else
        m_DocSize.x = std::min(m_WindowBound - m_DocPos.x, edRect.width * 5 / 12);
}

// Note: according to documentation, this event is only available under wxMSW, wxGTK, and wxOS2
void CCManager::OnAutocompleteHide(wxShowEvent& event)
{
    event.Skip();
    DoHidePopup();
    wxObject* evtObj = event.GetEventObject();
    if (evtObj)
        static_cast<wxWindow*>(evtObj)->Disconnect(wxEVT_SHOW, wxShowEventHandler(CCManager::OnAutocompleteHide), nullptr, this);

    if (m_CallTipActive != wxSCI_INVALID_POSITION && !m_AutoLaunchTimer.IsRunning())
        m_CallTipTimer.Start(CALLTIP_REFRESH_DELAY, wxTIMER_ONE_SHOT);
}

// cbEVT_DEFERRED_CALLTIP_SHOW
void CCManager::OnDeferredCallTipShow(wxCommandEvent& event)
{
    // Launching this event directly seems to be a candidate for race condition
    // and crash in OnShowCallTip() so we attempt to serialize it. See:
    // http://forums.codeblocks.org/index.php/topic,20181.msg137762.html#msg137762
    CodeBlocksEvent evt(cbEVT_SHOW_CALL_TIP);
    evt.SetInt(event.GetInt());
    Manager::Get()->ProcessEvent(evt);
}

// cbEVT_DEFERRED_CALLTIP_CANCEL
void CCManager::OnDeferredCallTipCancel(wxCommandEvent& WXUNUSED(event))
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (ed)
        static_cast<wxScintilla*>(ed->GetControl())->CallTipCancel();
}

#ifdef __WXMSW__
void CCManager::OnPopupScroll(wxMouseEvent& event)
{
    if (!m_pLastEditor)
    {
        event.Skip();
        return;
    }

    cbStyledTextCtrl* editor = m_pLastEditor->GetControl();
    if (!editor)
    {
        event.Skip();
        return;
    }
    if (not editor->AutoCompActive())
    {
        event.Skip();
        return;
    }

    const wxPoint& pos = editor->ClientToScreen(event.GetPosition());
    // Scroll when the current mouse position is within a shown popup window
    if (m_pPopup && m_pPopup->IsShown() && m_pPopup->GetScreenRect().Contains(pos))
    {
        if (m_pHtml)
            m_pHtml->GetEventHandler()->ProcessEvent(event);
    }
    else if (m_pAutocompPopup && m_pAutocompPopup->IsShown() && m_pAutocompPopup->GetScreenRect().Contains(pos))
    {
        m_pAutocompPopup->ScrollList(0, event.GetWheelRotation() / -4); // TODO: magic number... can we hook to the actual event?
    }
    else
        event.Skip();
}
#endif // __WXMSW__

void CCManager::OnHtmlLink(wxHtmlLinkEvent& event)
{
    cbCodeCompletionPlugin* ccPlugin = GetProviderFor();
    if (!ccPlugin)
        return;

    bool dismissPopup = false;
    const wxString& html = ccPlugin->OnDocumentationLink(event, dismissPopup);
    if (dismissPopup)
        DoHidePopup();
    else if (!html.empty())
        m_pHtml->SetPage(html);
    // plugins are responsible to skip this event (if they choose to)
}

void CCManager::OnTimer(wxTimerEvent& event)
{
    if (event.GetId() == idCallTipTimer) // m_CallTipTimer
    {
        wxCommandEvent evt(cbEVT_DEFERRED_CALLTIP_SHOW);
        evt.SetInt(FROM_TIMER);
        AddPendingEvent(evt);
    }
    else if (event.GetId() == idAutoLaunchTimer) // m_AutoLaunchTimer
    {
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (ed && ed->GetControl()->GetCurrentPos() == m_AutocompPosition)
        {
            CodeBlocksEvent evt(cbEVT_COMPLETE_CODE);
            evt.SetInt(FROM_TIMER);
            Manager::Get()->ProcessEvent(evt);
        }

        m_AutocompPosition = wxSCI_INVALID_POSITION;
    }
    else if (event.GetId() == idAutocompSelectTimer) // m_AutocompSelectTimer
    {
        cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
        if (ed)
        {
            cbStyledTextCtrl* stc = ed->GetControl();
            if (stc->AutoCompActive())
            {
                m_LastAutocompIndex = stc->AutoCompGetCurrent();
                DoShowDocumentation(ed);
            }
        }
    }
    else // ?!
        event.Skip();
}

void CCManager::OnMenuSelect(wxCommandEvent& event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    if (event.GetId() == idShowTooltip)
    {
        // compare this with cbStyledTextCtrl::EmulateDwellStart()
        cbStyledTextCtrl* stc = ed->GetControl();
        CodeBlocksEvent evt(cbEVT_EDITOR_TOOLTIP);
        wxPoint pt = stc->PointFromPosition(stc->GetCurrentPos());
        evt.SetX(pt.x);
        evt.SetY(pt.y);
        evt.SetInt(stc->GetStyleAt(stc->GetCurrentPos()));
        evt.SetEditor(ed);
        evt.SetExtraLong(0);
        evt.SetString("evt from menu");
        Manager::Get()->ProcessEvent(evt);
        return;
    }

    if (m_CallTips.empty() || m_CallTipActive == wxSCI_INVALID_POSITION)
        return;

    if (!ed->GetControl()->CallTipActive())
        return;

    if (event.GetId() == idCallTipNext)
    {
        AdvanceTip(Next);
        DoUpdateCallTip(ed);
    }
    else if (event.GetId() == idCallTipPrevious)
    {
        AdvanceTip(Previous);
        DoUpdateCallTip(ed);
    }
}

void CCManager::DoBufferedCC(cbStyledTextCtrl* stc)
{
    if (stc->AutoCompActive())
        return; // already active, no need to rebuild

    // fill list of suggestions
    wxString items;
    items.Alloc(m_AutocompTokens.size() * 20);
    for (size_t i = 0; i < m_AutocompTokens.size(); ++i)
    {
        items += m_AutocompTokens[i].displayName;
        if (m_AutocompTokens[i].category == -1)
            items += '\r';
        else
            items += wxString::Format("\n%d\r", m_AutocompTokens[i].category);
    }

    items.RemoveLast();
    if (!stc->CallTipActive())
        m_CallTipActive = wxSCI_INVALID_POSITION;

    // display
    stc->AutoCompShow(m_LastACLaunchState.caretStart - m_LastACLaunchState.tokenStart, items);
    m_OwnsAutocomp = true;

    // We need to check if the auto completion is active, because if there are no matches scintilla will close
    // the popup and any call to AutoCompSelect will result in a crash.
    if (stc->AutoCompActive() &&
        (m_LastAutocompIndex != wxNOT_FOUND && m_LastAutocompIndex < (int)m_AutocompTokens.size()))
    {
        // re-select last selected entry
        const cbCodeCompletionPlugin::CCToken& token = m_AutocompTokens[m_LastAutocompIndex];
        const int sepIdx = token.displayName.Find('\n', true);
        if (sepIdx == wxNOT_FOUND)
            stc->AutoCompSelect(token.displayName);
        else
            stc->AutoCompSelect(token.displayName.Mid(0, sepIdx));
    }
}

void CCManager::DoHidePopup()
{
#ifdef __WXMSW__
    if (m_pLastEditor && m_pLastEditor->GetControl() and (not m_pLastEditor->GetControl()->AutoCompActive()))
    {
        // Does this editor have a wxEVT_MOUSEWHEEL event connection? If so, disconnect it.
        if (m_EdAutocompMouseTraps.count(m_pLastEditor))
        {
            m_pLastEditor->GetControl()->Disconnect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(CCManager::OnPopupScroll), nullptr, this);
            // Remove this editor from the map indicating there is no longer a connection.
            m_EdAutocompMouseTraps.erase(m_pLastEditor);
        }
    }
#endif // __WXMSW__
    if (!m_pPopup->IsShown())
        return;

    m_pPopup->Hide();
}

void CCManager::DoShowDocumentation(cbEditor* ed)
{
#ifdef __WXMSW__
    if (ed->GetControl() and ed->GetControl()->AutoCompActive())
    {
        if (not m_EdAutocompMouseTraps.count(ed))
        {
            ed->GetControl()->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(CCManager::OnPopupScroll), nullptr, this);
            // Enter this editor into the map indicating that a connection exists.
            m_EdAutocompMouseTraps.insert(ed);
        }
    }
#endif // __WXMSW__


    if (!Manager::Get()->GetConfigManager("ccmanager")->ReadBool("/documentation_popup", true))
        return;

    cbCodeCompletionPlugin* ccPlugin = GetProviderFor(ed);
    if (!ccPlugin)
        return;

    if (m_LastAutocompIndex == wxNOT_FOUND || m_LastAutocompIndex >= (int)m_AutocompTokens.size())
        return;

    // If the AutoCompPopup is not shown, don't show the html documentation popup either (ticket 1168)
    cbStyledTextCtrl* stc = ed->GetControl();
    if ((not stc) or (not stc->AutoCompActive()) )
        return;

    const wxString& html = ccPlugin->GetDocumentation(m_AutocompTokens[m_LastAutocompIndex]);
    if (html.empty())
    {
        DoHidePopup();
        return;
    }

    cbDebuggerPlugin *plugin = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    if (plugin)
    {
        if (plugin->IsRunning() && plugin->ShowValueTooltip(0))
        {
            return; // Do not Show doc popup when the debigger is running
        }
    }

    m_pPopup->Freeze();
    m_pHtml->SetSize(m_DocSize);
    m_pHtml->SetPage(html);
    m_pPopup->SetClientSize(m_DocSize);
    m_pPopup->SetPosition(m_DocPos);
    m_pPopup->Thaw();
    if (!m_pPopup->IsShown())
    {
        m_pPopup->Show();
    }
}

void CCManager::DoUpdateCallTip(cbEditor* ed)
{
    wxStringVec tips;
    int hlStart = m_CurCallTip->hlStart;
    int hlEnd   = m_CurCallTip->hlEnd;
    size_t sRange = 0;
    size_t eRange = m_CurCallTip->tip.find('\n');
    while (eRange != wxString::npos)
    {
        tips.push_back(m_CurCallTip->tip.Mid(sRange, eRange - sRange));
        CCManagerHelper::RipplePts(hlStart, hlEnd, eRange, -1);
        sRange = eRange + 1;
        eRange = m_CurCallTip->tip.find('\n', sRange);
    }

    if (sRange < m_CurCallTip->tip.length())
        tips.push_back(m_CurCallTip->tip.Mid(sRange));

    // for multiple tips (such as tips for some overload functions)
    // some more text were added to the single tip, such as the up/down arrows, the x/y indicator.
    int offset = 0;
    cbStyledTextCtrl* stc = ed->GetControl();
    if (m_CallTips.size() > 1)
    {
        tips.front().Prepend("\001\002"); // up/down arrows
        offset += 2;

        // display (curTipNumber/totalTipCount)
        wxString tip;
        tip << '(' << (m_CurCallTip - m_CallTips.begin() + 1) << '/' << m_CallTips.size() << ')';

        tips.push_back(tip);
        // store for better first choice later
        m_CallTipChoiceDict[CCManagerHelper::CallTipToInt(m_CallTips.front().tip, m_CallTips.size())] = m_CurCallTip - m_CallTips.begin();
        // fuzzy store
        int prefixEndPos = m_CallTipActive;
        while (prefixEndPos > 0 && wxIsspace(stc->GetCharAt(prefixEndPos - 1)))
            --prefixEndPos;
        const wxString& prefix = stc->GetTextRange(stc->WordStartPosition(prefixEndPos, true), prefixEndPos);
        m_CallTipFuzzyChoiceDict[CCManagerHelper::CallTipToInt(prefix, m_CallTips.size())] = m_CurCallTip - m_CallTips.begin();
    }

    const int pos = stc->GetCurrentPos();
    int lnStart = stc->PositionFromLine(stc->LineFromPosition(pos));
    while (wxIsspace(stc->GetCharAt(lnStart)))
        ++lnStart;

#ifdef __WXMSW__
    m_LastTipPos = wxSCI_INVALID_POSITION; // Windows hack to fix display update
#endif // __WXMSW__
    DoShowTips(tips, stc, std::max(pos, lnStart), m_CallTipActive, hlStart + offset, hlEnd + offset);
}

void CCManager::DoShowTips(const wxStringVec& tips, cbStyledTextCtrl* stc, int pos, int argsPos, int hlStart, int hlEnd)
{
    const int maxLines = std::max(stc->LinesOnScreen() / 4, 5);
    const int marginWidth = stc->GetMarginWidth(wxSCI_MARGIN_SYMBOL) + stc->GetMarginWidth(wxSCI_MARGIN_NUMBER);
    int maxWidth = (stc->GetSize().x - marginWidth) / stc->TextWidth(wxSCI_STYLE_LINENUMBER, 'W') - 1;
    maxWidth = std::min(std::max(60, maxWidth), 135);
    wxString tip;
    int lineCount = 0;
    wxString lineBreak('\n');
    if (!tips.front().empty() && tips.front()[0] <= '\002')
    {
        // indent line break as needed, if tip prefixed with up/down arrows
        lineBreak += ' ';
        if (tips.front().length() > 1 && tips.front()[1] <= '\002')
            lineBreak += "  ";
    }

    for (size_t i = 0; i < tips.size() && lineCount < maxLines; ++i)
    {
        if (tips[i].length() > (size_t)maxWidth + 6) // line is too long, try breaking it
        {
            wxString tipLn = tips[i];
            while (!tipLn.empty())
            {
                wxString segment = tipLn.Mid(0, maxWidth);
                int index = segment.Find(' ', true); // break on a space
                if (index < 20) // no reasonable break?
                {
                    segment = tipLn.Mid(0, maxWidth * 6 / 5); // increase search width a bit
                    index = segment.Find(' ', true);
                }
                for (int commaIdx = index - 1; commaIdx > maxWidth / 2; --commaIdx) // check back for a comma
                {
                    if (segment[commaIdx] == ',' && segment[commaIdx + 1] == ' ')
                    {
                        index = commaIdx + 1; // prefer splitting on a comma, if that does not set us back too far
                        break;
                    }
                }
                if (index < 20 || segment == tipLn) // end of string, or cannot split
                {
                    tip += tipLn + lineBreak;
                    CCManagerHelper::RipplePts(hlStart, hlEnd, tip.length(), lineBreak.length());
                    tipLn.Clear();
                }
                else // continue splitting
                {
                    tip += segment.Mid(0, index) + lineBreak + ' ';
                    CCManagerHelper::RipplePts(hlStart, hlEnd, tip.length(), lineBreak.length() + 1);
                    // already starts with a space, so all subsequent lines are prefixed by two spaces
                    tipLn = tipLn.Mid(index);
                }
                ++lineCount;
            }
        }
        else // just add the line
        {
            tip += tips[i] + lineBreak;
            CCManagerHelper::RipplePts(hlStart, hlEnd, tip.length(), lineBreak.length());
            ++lineCount;
        }
    }

    tip.Trim(); // trailing linefeed

    // try to show the tip at the start of the token/arguments, or at the margin if we are scrolled right
    // an offset of 2 helps deal with the width of the folding bar (TODO: does an actual calculation exist?)
    const int line = stc->LineFromPosition(pos);
    if (argsPos == wxSCI_INVALID_POSITION)
        argsPos = stc->WordStartPosition(pos, true);
    else
        argsPos = std::min(CCManagerHelper::FindColumn(line, stc->GetColumn(argsPos), stc), stc->WordStartPosition(pos, true));

    const int offset = stc->PointFromPosition(stc->PositionFromLine(line)).x > marginWidth ? 0 : 2;
    pos = std::max(argsPos, stc->PositionFromPoint(wxPoint(marginWidth, stc->PointFromPosition(pos).y)) + offset);
    pos = std::min(pos, stc->GetLineEndPosition(line)); // do not go to next line
    if (stc->CallTipActive() && m_LastTipPos != pos)
        stc->CallTipCancel(); // force tip popup to invalidate (sometimes fails to otherwise do so on Windows)

    stc->CallTipShow(pos, tip);
    if (hlStart >= 0 && hlEnd > hlStart)
        stc->CallTipSetHighlight(hlStart, hlEnd);

    m_LastTipPos = pos;
}

void CCManager::CallSmartIndentCCDone(cbEditor* ed)
{
    CodeBlocksEvent event(cbEVT_EDITOR_CC_DONE);
    event.SetEditor(ed);
    // post event in the host's event queue
    Manager::Get()->ProcessEvent(event);
}

// ----------------------------------------------------------------------------
bool CCManager::DoShowDiagnostics( cbEditor* ed, int line)
// ----------------------------------------------------------------------------
{
	return GetProviderFor(ed)->DoShowDiagnostics(ed, line);
}

