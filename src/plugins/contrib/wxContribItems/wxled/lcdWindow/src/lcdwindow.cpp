#include "wx/lcdwindow.h"

#define LCD_NUMBER_SEGMENTS 8


BEGIN_EVENT_TABLE( wxLCDWindow, wxWindow )
    EVT_PAINT( wxLCDWindow::OnPaint )
END_EVENT_TABLE()

wxLCDWindow::wxLCDWindow(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size) : wxWindow(parent, id, pos, size, wxSUNKEN_BORDER | wxFULL_REPAINT_ON_RESIZE)
{
    mSegmentLen = 40;
    mSegmentWidth = 10;
    mSpace = 5;

    mNumberDigits = 6;

    mLightColour = wxColour(0, 255, 0);
    mGrayColour = wxColour(0, 64, 0);

    SetBackgroundColour(*wxBLACK);
}

void wxLCDWindow::OnPaint(wxPaintEvent &)
{
    wxPaintDC dc(this);

    int dw, dh;
    dc.GetSize(&dw, &dh);

    const int bw = GetBitmapWidth();
    const int bh = GetBitmapHeight();

    const double xs = (double) dw / bw;
    const double ys = (double) dh / bh;

    const double as = xs > ys? ys : xs;

    dc.SetUserScale(as, as);
    dc.SetDeviceOrigin( ( ( dw - bw * as ) / 2 ), ( ( dh - bh * as ) / 2 ) );
    DoDrawing(&dc);
}

void wxLCDWindow::DoDrawing(wxDC *dc)
{
    wxString buf(mValue);
    buf.Replace("..", ". .");

    const int buflen = buf.length();
    int ac = buflen - 1;

    for (int c = 0; c < mNumberDigits; c++)
    {
ReadString:
        const char current = (ac >= 0) ? buf.GetChar(ac) : wxUniChar(' ');
        const char next = (ac >= 0 && ac < buflen - 1) ? buf.GetChar(ac + 1) : wxUniChar(' ');
        if (current == '.')
        {
            ac--;
            goto ReadString;
        }

        wxDigitData data;
        data.value = current;
        data.comma = (next == '.');
        DrawDigit(dc, c, &data);
        ac--;
    }
}

void wxLCDWindow::DrawDigit(wxDC *dc, int digit, wxDigitData *data)
{
    unsigned char dec = Decode(data->value);

    if (data->value == ':')
    {
        DrawTwoDots(dc, digit);
        return;
    }

    for (int c = 0; c < LCD_NUMBER_SEGMENTS - 1; c++)
        DrawSegment(dc, digit, c, ( dec >> c ) & 1);

    DrawSegment(dc, digit, 7, data->comma);
}

void wxLCDWindow::DrawTwoDots(wxDC *dc, int digit)
{
    const int sl = mSegmentLen;
    const int sw = mSegmentWidth;
//    int sp = mSpace;

    int x = DigitX( digit );
    int y = DigitY( digit );

    wxBrush brushOn(mLightColour, wxBRUSHSTYLE_SOLID);

    x += ( sl / 2 ) - sw;
    y += ( sl / 2 ) - sw;

    dc->SetBrush(brushOn);
    dc->SetPen(wxPen( GetBackgroundColour(), 1, wxPENSTYLE_SOLID));

    dc->DrawEllipse(x, y, 2 * sw, 2 * sw);

    y += sl;

    dc->DrawEllipse(x, y, 2 * sw, 2 * sw);
}

void wxLCDWindow::DrawSegment( wxDC *dc, int digit, int segment, bool state )
{
    const int sl = mSegmentLen;
    const int sw = mSegmentWidth;
//    int sp = mSpace;

    int x = DigitX( digit );
    int y = DigitY( digit );

    dc->SetBrush(wxBrush(state ? mLightColour : mGrayColour, wxBRUSHSTYLE_SOLID));
    dc->SetPen(wxPen(GetBackgroundColour(), 1, wxPENSTYLE_SOLID));

    wxPoint points[4];

    switch (segment)
    {
    case 0:
        points[0].x = x;
        points[0].y = y;
        points[1].x = x + sl;
        points[1].y = y;
        points[2].x = x + sl - sw;
        points[2].y = y + sw;
        points[3].x = x + sw;
        points[3].y = y + sw;
        break;
    case 5:
        y += 2 * sl - sw;
        points[0].x = x + sw;
        points[0].y = y;
        points[1].x = x + sl - sw;
        points[1].y = y;
        points[2].x = x + sl;
        points[2].y = y + sw;
        points[3].x = x;
        points[3].y = y + sw;
        break;
    case 4:
        y += sl;
        x += sl - sw;
        points[0].x = x;
        points[0].y = y + sw / 2;
        points[1].x = x + sw;
        points[1].y = y;
        points[2].x = x + sw;
        points[2].y = y + sl;
        points[3].x = x;
        points[3].y = y + sl - sw;
        break;
    case 2:
        x += sl - sw;
        points[0].x = x;
        points[0].y = y + sw;
        points[1].x = x + sw;
        points[1].y = y;
        points[2].x = x + sw;
        points[2].y = y + sl;
        points[3].x = x;
        points[3].y = y + sl - sw / 2;
        break;
    case 3:
        y += sl;
        points[0].x = x;
        points[0].y = y;
        points[1].x = x;
        points[1].y = y + sl;
        points[2].x = x + sw;
        points[2].y = y + sl - sw;
        points[3].x = x + sw;
        points[3].y = y + sw - sw / 2;
        break;
    case 1:
        points[0].x = x;
        points[0].y = y;
        points[1].x = x;
        points[1].y = y + sl;
        points[2].x = x + sw;
        points[2].y = y + sl - sw / 2;
        points[3].x = x + sw;
        points[3].y = y + sw;
        break;
    case 6:
    default:
        break;
    }

    if (segment < 6)
    {
        dc->DrawPolygon( 4, points );
    }

    if (segment == 6)
    {
        y += sl - sw / 2;
        wxPoint p6[6];

        p6[0].x = x;
        p6[0].y = y + sw / 2;
        p6[1].x = x + sw;
        p6[1].y = y;
        p6[2].x = x + sl - sw;
        p6[2].y = y;
        p6[3].x = x + sl;
        p6[3].y = y + sw / 2;
        p6[4].x = x + sl - sw;
        p6[4].y = y + sw;
        p6[5].x = x + sw;
        p6[5].y = y + sw;

        dc->DrawPolygon( 6, p6 );
    }

    if (segment == 7)
    {
        y += 2 * sl;
        x += sl;

        dc->DrawEllipse( x + 1, y - sw, sw, sw );
    }
}

// Protected functions that calculate sizes.
// Needed by OnPaint

int wxLCDWindow::GetDigitWidth() const
{
    return mSegmentLen + mSegmentWidth + mSpace;
}

int wxLCDWindow::GetDigitHeight() const
{
    return 2*mSegmentLen + 2*mSpace;
}

int wxLCDWindow::GetBitmapWidth() const
{
    return mNumberDigits*GetDigitWidth() + mSpace;
}

int wxLCDWindow::GetBitmapHeight() const
{
    return GetDigitHeight();
}

int wxLCDWindow::DigitX(int digit) const
{
    return GetBitmapWidth() - (digit + 1)*GetDigitWidth();
}

int wxLCDWindow::DigitY(int WXUNUSED(digit)) const
{
    return mSpace;
}

// Public functions accessible by the user.

void wxLCDWindow::SetNumberDigits(int ndigits)
{
    mNumberDigits = ndigits;
    Refresh(false);
}

void wxLCDWindow::SetValue(const wxString &value)
{
    mValue = value;
    Refresh(false);
}

wxString wxLCDWindow::GetValue() const
{
    return mValue;
}

int wxLCDWindow::GetNumberDigits() const
{
    return mNumberDigits;
}

void wxLCDWindow::SetLightColour(const wxColour &c)
{
    mLightColour = c;
}

void wxLCDWindow::SetGrayColour(const wxColour &c)
{
    mGrayColour = c;
}

wxColour wxLCDWindow::GetLightColour() const
{
    return mLightColour;
}

wxColour wxLCDWindow::GetGrayColour() const
{
    return mGrayColour;
}

int wxLCDWindow::GetDigitsNeeded(const wxString &value) const
{
    wxString tst(value);
    tst.Replace(".", "");
    return tst.length();
}

// The decoder function. The heart of the wxLCDWindow.


//      ***0***
//     *       *
//     1       2
//     *       *
//      ***6***
//     *       *
//     3       4
//     *       *
//      ***5***

// A 10
// B 11
// C 12
// D 13
// E 14
// F 15

//     8421 8421
//     -654 3210
//---------------------
// 0 : 0011.1111 = 0x3F
// 1 : 0001.0100 = 0x14
// 2 : 0110.1101 = 0x6D
// 3 : 0111.0101 = 0x75
// 4 : 0101.0110 = 0x56
// 5 : 0111.0011 = 0x73
// 6 : 0111.1011 = 0x7B
// 7 : 0001,0101 = 0x15
// 8 : 0111.1111 = 0x7F
// 9 : 0111.0111 = 0x77
//   : 0000.0000 = 0x00
// - : 0100.0000 = 0x40
// E : 0110.1011 = 0x6B
// r : 0100.1000 = 0x48
// o : 0111.1000 = 0x78
// ^ : 0100.0111 = 0x47
// C : 0010.1011 = 0x2B


unsigned char wxLCDWindow::Decode(char c)
{
    struct DecodedDisplay
    {
        char ch;
        unsigned char value;
    };

    DecodedDisplay dec[] =
    {
        {'0', 0x3F},
        {'1', 0x14},
        {'2', 0x6D},
        {'3', 0x75},
        {'4', 0x56},
        {'5', 0x73},
        {'6', 0x7B},
        {'7', 0x15},
        {'8', 0x7F},
        {'9', 0x77},
        {' ', 0x00},
        {'-', 0x40},
        {'E', 0x6B},
        {'r', 0x48},
        {'o', 0x78},
        {'^', 0x47},
        {'C', 0x2B},
        { 0,  0x00}
    };

    unsigned char ret = 0;

    for (int d = 0; dec[d].ch != 0; d++)
    {
        if (dec[d].ch == c)
        {
            ret = dec[d].value;
            break;
        }
    }

    return ret;
}

wxSize wxLCDWindow::DoGetBestClientSize() const
{
    return wxSize(GetBitmapWidth(), GetBitmapHeight());
}
