/////////////////////////////////////////////////////////////////////////////
// Name:        led.cpp
// Purpose:     wxLed implementation
// Author:      Thomas Monjalon
// Created:     09/06/2005
// Revision:    09/06/2005
// Licence:     wxWidgets
// mod   by:    Jonas Zinn
// mod date:    24/03/2012
/////////////////////////////////////////////////////////////////////////////

#include "wx/led.h"

#include <array>

#define WX_LED_WIDTH       17
#define WX_LED_HEIGHT      17
#define WX_LED_COLORS      5
#define WX_LED_XPM_COLS    (WX_LED_WIDTH + 1)
#define WX_LED_XPM_LINES   (1 + WX_LED_COLORS + WX_LED_HEIGHT)

BEGIN_EVENT_TABLE (wxLed, wxWindow)
    EVT_PAINT (wxLed::OnPaint)
END_EVENT_TABLE ()

wxLed::wxLed(wxWindow *parent, wxWindowID id, const wxColour &disableColour, const wxColour &onColour, const wxColour &offColour, const wxPoint &pos, const wxSize &size)
{
    Create(parent, id, disableColour, onColour, offColour);
}

bool wxLed::Create(wxWindow *parent, wxWindowID id, const wxColour &disableColour, const wxColour &onColour, const wxColour &offColour)
{
    if (!wxWindow::Create(parent, id, wxDefaultPosition, wxDefaultSize))
        return false;

    m_isEnable  = false;
    m_isOn      = false;
    m_Disable   = disableColour;
    m_On        = onColour;
    m_Off       = offColour;
    Enable();
    return true;
}

bool wxLed::Enable(bool enable)
{
    if (!enable)
        return Disable();

    if (m_isEnable)
        return false;

    m_isEnable = true;
    SetBitmap(m_isOn ? m_On : m_Off);
    return true;
}

bool wxLed::Disable()
{
    if (!m_isEnable)
        return false;

    m_isEnable = false;
    SetBitmap(m_Disable);
    return true;
}

void wxLed::Switch()
{
    if (m_isEnable)
    {
        m_isOn = !m_isOn;
        SetBitmap(m_isOn ? m_On : m_Off);
    }
}

void wxLed::SwitchOn()
{
    if (m_isEnable)
    {
        m_isOn = true;
        SetBitmap(m_On);
    }
}

void wxLed::SwitchOff()
{
    if (m_isEnable)
    {
        m_isOn = false;
        SetBitmap(m_Off);
    }
}

void wxLed::SetOnColour(const wxColour &rgb)
{
    m_On = rgb;
    if (m_isEnable && m_isOn)
        SetBitmap(m_On);
}

void wxLed::SetOffColour(const wxColour &rgb)
{
    m_Off = rgb;
    if (m_isEnable && !m_isOn)
        SetBitmap(m_Off);
}

void wxLed::SetDisableColour(const wxColour &rgb)
{
    m_Disable = rgb;
    if (!m_isEnable)
        SetBitmap(m_Disable);
}

void wxLed::SetOnOrOff(bool on)
{
    m_isOn = on;
    if (m_isEnable)
    {
        if( m_isOn)
            SetBitmap (m_On.GetAsString( wxC2S_HTML_SYNTAX)) ;
        else
            SetBitmap (m_Off.GetAsString( wxC2S_HTML_SYNTAX)) ;
    }
}

void wxLed::OnPaint(wxPaintEvent &WXUNUSED(event))
{
    wxPaintDC dc(this);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.DrawBitmap(m_bitmap, (w-m_bitmap.GetWidth())/2, (h-m_bitmap.GetHeight())/2, true);
}

void wxLed::SetBitmap(const wxColour &color)
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

wxSize wxLed::DoGetBestClientSize() const
{
    return wxSize(WX_LED_WIDTH, WX_LED_HEIGHT);
}
