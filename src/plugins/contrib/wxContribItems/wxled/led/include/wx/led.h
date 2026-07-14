/////////////////////////////////////////////////////////////////////////////
// Name:        led.h
// Purpose:     wxLed class
// Author:      Thomas Monjalon
// Created:     09/06/2005
// Revision:    09/06/2005
// Licence:     wxWidgets
// mod   by:    Jonas Zinn
// mod date:    24/03/2012
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LED_H_
#define _WX_LED_H_

#include <wx/window.h>
#include <wx/bitmap.h>
#include <wx/dcclient.h>
#include <wx/thread.h>

#ifdef __WXMSW__
#ifndef DLLEXPORT
#define DLLEXPORT __declspec (dllexport)
#endif
#else
#define DLLEXPORT
#endif

/// Class to display a Led on the used dialogs page
class DLLEXPORT wxLed : public wxWindow
{
public :
    /** Constructor
      * @param parent The window parent
      * @param id If u want to specify a ID
      * @param disableColor If the window is disabled, which color it should have
      * @param onColor If the window is enabled and turned on, how it should glow
      * @param offColor If the window is enabled and turned off, how it should look
    */
    wxLed(wxWindow *parent, wxWindowID id = wxID_ANY, const wxColour &disableColour = wxColour(128, 128, 128), const wxColour &onColour = wxColour(0, 255, 0), const wxColour &offColour = wxColour(255, 0, 0), const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);
    wxLed() = default;

    /// Destructor
    virtual ~wxLed() = default;

    bool Create(wxWindow *parent, wxWindowID id = wxID_ANY, const wxColour &disableColour = wxColour(128, 128, 128), const wxColour &onColour = wxColour(0, 255, 0), const wxColour &offColour = wxColour(255, 0, 0));

    /// Enable the Led
    bool Enable(bool enable = true) override;

    /// Disable the Led
    bool Disable();

    /// If the Led is Enabled and on, switch the Led off else on.
    void Switch();

    /// If is Enabled turn the Led on
    void SwitchOn();

    /// If is Enabled turn the Led off
    void SwitchOff();

    /** Change the on Color
      * @param rgb The Color how the Led should glow in ON Mode
    */
    void SetOnColour(const wxColour &rgb);

    /** Change the off Color
    * @param rgb The Color how the Led should glow in OFF Mode
    */
    void SetOffColour(const wxColour &rgb);

    /** Change the disable Color
    * @param rgb The Color how the Led should glow if Disabled
    */
    void SetDisableColour(const wxColour &rgb);

    /**

    */
    void SetOnOrOff(bool on);

    /** Test if the Led is Enabled or Disabled
      * @return function returns true, if the Led is enabled, false otherwise
    */
    bool IsEnabled() const {return m_isEnable;}

    /** Test if the Led is ON or OFF
      * @return function returns true, if the Led is ON, false otherwise
    */
    bool IsOn() const {return m_isOn;}

protected :

    wxColour m_On;          /// contains the Color for the ON state
    wxColour m_Off;         /// contains the Color for the OFF state
    wxColour m_Disable;     /// contains the Color for the Disable state
    wxBitmap m_bitmap;      /// contains the Led as a Bitmap
    bool m_isEnable;        /// is the Led enabled?
    bool m_isOn;            /// is the Led in ON state?

    /** Function to paint the LED at the place in the dialog
      * @param event Normal wxWidgets Event
    */
    void OnPaint(wxPaintEvent &event);

    /** Function to create the Bitmap, which is paint on the dialog
      * @param color The used color for the LED
    */
    void SetBitmap(const wxColour &color);

    wxSize DoGetBestClientSize() const override;

private:

    DECLARE_EVENT_TABLE()
};

#endif // _WX_LED_H_
