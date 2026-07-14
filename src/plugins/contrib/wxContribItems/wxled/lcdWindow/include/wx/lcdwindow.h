#ifndef __LCDWINDOW_H__
#define __LCDWINDOW_H__

#include <wx/wx.h>

#ifdef __WXMSW__
    #ifndef DLLEXPORT
          #define DLLEXPORT __declspec (dllexport)
    #endif
#else
    #define DLLEXPORT
#endif

// This structure is used internally by the window.
struct wxDigitData
{
    char value;
    bool comma;
};

class DLLEXPORT wxLCDWindow : public wxWindow
{
public:
    wxLCDWindow(wxWindow *parent, wxWindowID = wxID_ANY, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);
    virtual ~wxLCDWindow() = default;

// Sets the desired number of digits our 7seg display.
// The default is 4.
    void SetNumberDigits( int ndigits );

// Gets the current number of digits.
    int GetNumberDigits() const;

/* Print a value on the display. The accepted characters are :
   - All the hexadecimal digits ( 0-F )
   - The characters :,.-EroC and space
   - The character ^ is displayed as an o, but displayed at the top. ( The symbol of degrees )
*/
    void SetValue(const wxString &value);

// Gets the value currently displayed.
    wxString GetValue() const;

// Sets the colour which the lighted parts of the display should have.
    void SetLightColour(const wxColour &c);

// Sets the colour that the grayed parts of the display will have.
    void SetGrayColour(const wxColour &c);

    wxColour GetLightColour() const;
    wxColour GetGrayColour() const;

// Returns the amount of digits required to display the current value.
// The amount of digits needed is not necessarily the length of the string.
// For example, a dot does not require an extra space in order to be displayed.
    int GetDigitsNeeded(const wxString &value) const;

protected:
    int mSegmentLen;
    int mSegmentWidth;
    int mSpace;

    int mNumberDigits;
    wxString mValue;
    wxColour mLightColour;
    wxColour mGrayColour;

// Internal functions used by the control.
// No time for documentation yet. Sorry.
    int GetDigitWidth() const;
    int GetDigitHeight() const;

    int GetBitmapWidth() const;
    int GetBitmapHeight() const;

    int DigitX(int digit) const;
    int DigitY(int digit) const;

    void DoDrawing(wxDC *dc);
    void DrawSegment(wxDC *dc, int digit, int segment, bool state);
    void DrawDigit(wxDC *dc, int digit, wxDigitData *data);
    void DrawTwoDots(wxDC *dc, int digit);

    unsigned char Decode(char c);

    void OnPaint(wxPaintEvent &event);

    wxSize DoGetBestClientSize() const override;

private:
    DECLARE_EVENT_TABLE()
};

#endif // __LCDWINDOW_H__
