#ifndef STATELED_H_
#define STATELED_H_

#include <wx/window.h>
#include <wx/bitmap.h>
#include <wx/dcclient.h>
#include <wx/thread.h>
#include <map>

#ifdef __WXMSW__
#ifndef DLLEXPORT
#define DLLEXPORT __declspec (dllexport)
#endif
#else
#define DLLEXPORT
#endif

/// Class to display a Led on the used dialogs page
class DLLEXPORT wxStateLed : public wxWindow
{
public :
    /** Constructor
      * @param parent The window parent
      * @param id If u want to specify a ID
      * @param disableColor If the window is disabled, which color it should have
      * @param onColor If the window is enabled and turned on, how it should glow
      * @param offColor If the window is enabled and turned off, how it should look
    */
    wxStateLed(wxWindow *parent, wxWindowID id = wxID_ANY, const wxColour &disabledColor = wxColour(128, 128, 128), const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);
    wxStateLed() = default;

    /// Destructor
    virtual ~wxStateLed() = default;

    bool Create(wxWindow *parent, wxWindowID id = wxID_ANY, const wxColour &disabledColor = wxColour(128, 128, 128), const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);

    /// Enable the Led
    bool Enable(bool enable = true) override;

    /// Disable the Led
    bool Disable();

    void RegisterState(int state, const wxColour &colour);

    void SetState(int state);

    int GetState() const {return m_state;}

    /** Change the disable Color
    * @param rgb The Color how the Led should glow if Disabled
    */
    void SetDisableColor(const wxColour &rgb);

    /** Test if the Led is Enabled or Disabled
      * @return function returns true, if the Led is enabled, false otherwise
    */
    bool IsEnabled() const {return m_isEnable;}

protected :

    wxColour                 m_Disable;         /// contains the Color for the Disable state
    wxBitmap                 m_bitmap;          /// contains the Led as a Bitmap
    bool                     m_isEnable;        /// is the Led enabled?
    std::map <int, wxColour> m_registeredState;
    int                      m_state;

    /** Function to paint the LED at the place in the dialog
      * @param event Normal wxWidgets Event
    */
    void OnPaint(wxPaintEvent &event);

    /** Function to create the Bitmap, which is paint on the dialog
      * @param color The used color for the LED
    */
    void SetBitmap(const wxColour& color);

    wxSize DoGetBestClientSize() const override;

private:

    DECLARE_EVENT_TABLE ()
};

#endif
