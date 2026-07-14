#include "wx/stateLed.h"

#include <array>

#define WX_LED_WIDTH       17
#define WX_LED_HEIGHT      17
#define WX_LED_COLORS      5
#define WX_LED_XPM_COLS    (WX_LED_WIDTH + 1)
#define WX_LED_XPM_LINES   (1 + WX_LED_COLORS + WX_LED_HEIGHT)

BEGIN_EVENT_TABLE (wxStateLed, wxWindow)
    EVT_PAINT (wxStateLed ::OnPaint)
END_EVENT_TABLE ()

wxStateLed ::wxStateLed (wxWindow *parent, wxWindowID id, const wxColour &disabledColor, const wxPoint &pos, const wxSize &size)
{
    Create(parent, id, disabledColor, pos, size);
}

bool wxStateLed::Create(wxWindow *parent, wxWindowID id, const wxColour& disabledColor, const wxPoint &pos, const wxSize &size)
{
    if (!wxWindow::Create(parent, id, pos, size))
        return false;

    m_isEnable = false ;
    m_Disable  = disabledColor;
    m_state    = 0;
    Enable();
    return true;
}

bool wxStateLed::Enable(bool enable)
{
    if (!enable)
        return Disable();

    if (!m_registeredState.empty())
    {
        m_isEnable = true;
        SetBitmap(m_registeredState[m_state]);
        return true;
    }

    SetBitmap(m_Disable);
    return false;
}

bool wxStateLed::Disable()
{
    if (!m_isEnable)
        return false;

    m_isEnable = false;
    SetBitmap(m_Disable);
    return true;
}

void wxStateLed::RegisterState(int state, const wxColour &colour)
{
    m_registeredState[state] = colour;
}

void wxStateLed::SetState(int state)
{
    m_state = state;
    if(m_isEnable)
        SetBitmap(m_registeredState[m_state]);
}

void wxStateLed::SetDisableColor(const wxColour &rgb)
{
    m_Disable = rgb;
    if (!m_isEnable)
        SetBitmap(m_Disable);
}

void wxStateLed::OnPaint(wxPaintEvent & WXUNUSED (event))
{
    wxPaintDC dc(this);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.DrawBitmap(m_bitmap, (w-m_bitmap.GetWidth())/2, (h-m_bitmap.GetHeight())/2, true);
}

void wxStateLed::SetBitmap(const wxColour &color)
{
    std::array <char *, WX_LED_XPM_LINES>                 xpm;
    std::array <char,   WX_LED_XPM_LINES*WX_LED_XPM_COLS> xpmData;
    for (int i = 0 ; i < WX_LED_XPM_LINES ; ++i)
        xpm[i] = xpmData.data()+i*WX_LED_XPM_COLS;

    const wxString c2s = color.GetAsString(wxC2S_HTML_SYNTAX);

    // width height num_colors chars_per_pixel
    sprintf(xpm[0], "%d %d %d 1", WX_LED_WIDTH, WX_LED_HEIGHT, WX_LED_COLORS);
    // colors
    strncpy(xpm[1], "  c None", WX_LED_XPM_COLS);
    strncpy(xpm[2], "- c #C0C0C0", WX_LED_XPM_COLS);
    strncpy(xpm[3], "_ c #F8F8F8", WX_LED_XPM_COLS);
    strncpy(xpm[4], "* c #FFFFFF", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS], "X c ", WX_LED_XPM_COLS);
    strncpy((xpm[WX_LED_COLORS]) + 4, c2s.char_str(), 8);
    // pixels
    strncpy(xpm[WX_LED_COLORS +  1], "      -----      ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  2], "    ---------    ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  3], "   -----------   ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  4], "  -----XXX----_  ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  5], " ----XX**XXX-___ ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  6], " ---X***XXXXX___ ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  7], "----X**XXXXXX____", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  8], "---X**XXXXXXXX___", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS +  9], "---XXXXXXXXXXX___", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 10], "---XXXXXXXXXXX___", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 11], "----XXXXXXXXX____", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 12], " ---XXXXXXXXX___ ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 13], " ---_XXXXXXX____ ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 14], "  _____XXX_____  ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 15], "   ___________   ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 16], "    _________    ", WX_LED_XPM_COLS);
    strncpy(xpm[WX_LED_COLORS + 17], "      _____      ", WX_LED_XPM_COLS);
    m_bitmap = wxBitmap(xpm.data());
    Refresh ();
}

wxSize wxStateLed::DoGetBestClientSize() const
{
    return wxSize(WX_LED_WIDTH, WX_LED_HEIGHT);
}
