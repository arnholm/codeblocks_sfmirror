///////////////////////////////////////////////////////////////////////////////
// Name:        pdfdc.cpp
// Purpose:     Implementation of wxPdfDC
// Author:      Ulrich Telle
// Created:     2010-11-28
// Copyright:   (c) Ulrich Telle
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/// \file pdfdc.cpp Implementation of the wxPdfDC graphics primitives

// For compilers that support precompilation, includes <wx/wx.h>.
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/region.h>
#include <wx/font.h>
#include <wx/paper.h>
#include <wx/tokenzr.h>

#include "wx/pdfdc.h"
#include "wx/pdffontmanager.h"
#include "wx/pdfutility.h"

#include <math.h>

//-------------------------------------------------------------------------------
// wxPdfDC
//-------------------------------------------------------------------------------


IMPLEMENT_DYNAMIC_CLASS(wxPdfDC, wxDC)

wxPdfDC::wxPdfDC()
  : wxDC(new wxPdfDCImpl(this))
{
}

wxPdfDC::wxPdfDC(const wxPrintData& printData)
  : wxDC(new wxPdfDCImpl(this, printData))
{
}

wxPdfDC::wxPdfDC(wxPdfDocument* pdfDocument, double templateWidth, double templateHeight)
  : wxDC(new wxPdfDCImpl(this, pdfDocument, templateWidth, templateHeight))
{
}

wxPdfDocument*
wxPdfDC::GetPdfDocument()
{
  return ((wxPdfDCImpl*) m_pimpl)->GetPdfDocument();
}

void
wxPdfDC::SetResolution(int ppi)
{
  ((wxPdfDCImpl*) m_pimpl)->SetResolution(ppi);
}

int
wxPdfDC::GetResolution() const
{
  return ((wxPdfDCImpl*) m_pimpl)->GetResolution();
}

void
wxPdfDC::SetImageType(wxBitmapType bitmapType, int quality)
{
  ((wxPdfDCImpl*) m_pimpl)->SetImageType(bitmapType, quality);
}

void
wxPdfDC::SetMapModeStyle(wxPdfMapModeStyle style)
{
  ((wxPdfDCImpl*) m_pimpl)->SetMapModeStyle(style);
}

wxPdfMapModeStyle
wxPdfDC::GetMapModeStyle() const
{
  return ((wxPdfDCImpl*) m_pimpl)->GetMapModeStyle();
}

static double
angleByCoords(wxCoord xa, wxCoord ya, wxCoord xc, wxCoord yc)
{
  static double pi = 4. * atan(1.0);
  double diffX = xa - xc, diffY = -(ya - yc), ret = 0;
  if (diffX == 0) // singularity
  {
    ret = diffY > 0 ? 90 : 270;
  }
  else if (diffX >= 0) // quadrants 1, 4
  {
    if (diffY >= 0) // quadrant 1
    {
      ret = (atan(diffY / diffX) * 180.0 / pi);
    }
    else // quadrant 4
    {
      ret = 360 + (atan(diffY / diffX) * 180.0 / pi);
    }
  }
  else // quadrants 2, 3
  {
    ret = 180 + (atan(diffY / diffX) * 180.0 / pi);
  }
  return ret;
}

/*
 * PDF device context
 *
 */

// Critical section
#if wxUSE_THREADS
static wxCriticalSection gs_csImageCounter;
#endif

int wxPdfDCImpl::ms_imageCount = 0;

IMPLEMENT_ABSTRACT_CLASS(wxPdfDCImpl, wxDCImpl)

wxPdfDCImpl::wxPdfDCImpl(wxPdfDC* owner)
  : wxDCImpl(owner)
{
  Init();
  m_ok = true;
}

wxPdfDCImpl::wxPdfDCImpl(wxPdfDC* owner, const wxPrintData& data)
  : wxDCImpl(owner)
{
  Init();
  SetPrintData(data);
  m_ok = true;
}

wxPdfDCImpl::wxPdfDCImpl(wxPdfDC* owner, const wxString& file, int WXUNUSED(w), int WXUNUSED(h))
  : wxDCImpl(owner)
{
  Init();
  m_printData.SetFilename(file);
  m_ok = true;
}

wxPdfDCImpl::wxPdfDCImpl(wxPdfDC* owner, wxPdfDocument* pdfDocument, double templateWidth, double templateHeight)
  : wxDCImpl(owner)
{
  Init();
  m_templateMode = true;
  m_templateWidth = templateWidth;
  m_templateHeight = templateHeight;
  m_pdfDocument = pdfDocument;
}

wxPdfDCImpl::~wxPdfDCImpl()
{
  if (m_pdfDocument != NULL)
  {
    if (!m_templateMode)
    {
      delete m_pdfDocument;
    }
  }
}

int
wxPdfDCImpl::IncreaseImageCounter()
{
#if wxUSE_THREADS
  wxCriticalSectionLocker locker(gs_csImageCounter);
#endif
  return ++ms_imageCount;
}

void
wxPdfDCImpl::Init()
{
  m_templateMode = false;
  m_ppi = 72;
  m_pdfDocument = NULL;

  wxScreenDC screendc;
  m_ppiPdfFont = screendc.GetPPI().GetHeight();
  m_mappingModeStyle = wxPDF_MAPMODESTYLE_STANDARD;

  m_cachedRGB = 0;
  m_pdfPen = wxNullPen;
  m_pdfBrush = wxNullBrush;

  m_inTransform = false;
  m_matrix = wxAffineMatrix2D();
  m_pdfPenSaved = wxNullPen;
  m_pdfBrushSaved = wxNullBrush;

  m_jpegFormat = false;
  m_jpegQuality = 75;

  m_printData.SetOrientation(wxPORTRAIT);
  m_printData.SetPaperId(wxPAPER_A4);
  m_printData.SetFilename(wxS("default.pdf"));
}

wxPdfDocument*
wxPdfDCImpl::GetPdfDocument()
{
  return m_pdfDocument;
}

void
wxPdfDCImpl::SetPrintData(const wxPrintData& data)
{
  m_printData = data;
  wxPaperSize id = m_printData.GetPaperId();
  wxPrintPaperType* paper = wxThePrintPaperDatabase->FindPaperType(id);
  if (!paper)
  {
    m_printData.SetPaperId(wxPAPER_A4);
  }
}

void
wxPdfDCImpl::SetResolution(int ppi)
{
  m_ppi = ppi;
}

int
wxPdfDCImpl::GetResolution() const
{
  return (int) m_ppi;
}

void
wxPdfDCImpl::SetImageType(wxBitmapType bitmapType, int quality)
{
  m_jpegFormat = (bitmapType == wxBITMAP_TYPE_JPEG);
  m_jpegQuality = (quality >= 0) ? ((quality <= 100) ? quality : 75) : 75;
}

void
wxPdfDCImpl::Clear()
{
  // Not yet implemented
}

bool
wxPdfDCImpl::StartDoc(const wxString& message)
{
  wxCHECK_MSG(m_ok, false, wxS("Invalid PDF DC"));
  wxUnusedVar(message);
  if (!m_templateMode && m_pdfDocument == NULL)
  {
    m_pdfDocument = new wxPdfDocument(m_printData.GetOrientation(), wxString(wxS("pt")), m_printData.GetPaperId());
    m_pdfDocument->Open();
    //m_pdfDocument->SetCompression(false);
    m_pdfDocument->SetAuthor(wxS("wxPdfDC"));
    m_pdfDocument->SetTitle(wxS("wxPdfDC"));

    SetBrush(*wxBLACK_BRUSH);
    SetPen(*wxBLACK_PEN);
    SetBackground(*wxWHITE_BRUSH);
    SetTextForeground(*wxBLACK);
    SetDeviceOrigin(0, 0);
  }
  return true;
}

void
wxPdfDCImpl::EndDoc()
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (!m_templateMode)
  {
    m_pdfDocument->SaveAsFile(m_printData.GetFilename());
    delete m_pdfDocument;
    m_pdfDocument = NULL;
  }
}

void
wxPdfDCImpl::StartPage()
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (!m_templateMode)
  {
    // Begin a new page
    // Library needs it this way (always landscape) to size the page correctly
    m_pdfDocument->AddPage(m_printData.GetOrientation());
    wxPdfLineStyle style = m_pdfDocument->GetLineStyle();
    style.SetWidth(1.0);
    style.SetColour(wxPdfColour(0, 0, 0));
    style.SetLineCap(wxPDF_LINECAP_ROUND);
    style.SetLineJoin(wxPDF_LINEJOIN_MITER);
    m_pdfDocument->SetLineStyle(style);
    // Force that pen and brush are always set on a new page
    m_pdfPen = wxNullPen;
    m_pdfBrush = wxNullBrush;
  }
}

void
wxPdfDCImpl::EndPage()
{
  if (m_ok)
  {
    if (m_clipping)
    {
      DestroyClippingRegion();
    }
  }
}

void
wxPdfDCImpl::SetFont(const wxFont& font)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  m_font = font;
  if (!font.IsOk())
  {
    return;
  }
  int styles = wxPDF_FONTSTYLE_REGULAR;
  if (font.GetWeight() == wxFONTWEIGHT_BOLD)
  {
    styles |= wxPDF_FONTSTYLE_BOLD;
  }
  if (font.GetStyle() == wxFONTSTYLE_ITALIC)
  {
    styles |= wxPDF_FONTSTYLE_ITALIC;
  }
  if (font.GetUnderlined())
  {
    styles |= wxPDF_FONTSTYLE_UNDERLINE;
  }

  wxPdfFont regFont = wxPdfFontManager::GetFontManager()->GetFont(font.GetFaceName(), styles);
  bool ok = regFont.IsValid();
  if (!ok)
  {
    regFont = wxPdfFontManager::GetFontManager()->RegisterFont(font, font.GetFaceName());
    ok = regFont.IsValid();
  }

  if (ok)
  {
    ok = m_pdfDocument->SetFont(regFont, styles, ScaleFontSizeToPdf(font.GetPointSize()));
  }
}

void
wxPdfDCImpl::SetPen(const wxPen& pen)
{
  if (pen.Ok())
  {
    m_pen = pen;
  }
}

void
wxPdfDCImpl::SetBrush(const wxBrush& brush)
{
  if (brush.Ok())
  {
    m_brush = brush;
  }
}

void
wxPdfDCImpl::SetBackground(const wxBrush& brush)
{
  if (brush.Ok())
  {
    m_backgroundBrush = brush;
  }
}

void
wxPdfDCImpl::SetBackgroundMode(int mode)
{
  // TODO: check implementation
  m_backgroundMode = (mode == wxBRUSHSTYLE_TRANSPARENT) ? wxBRUSHSTYLE_TRANSPARENT : wxBRUSHSTYLE_SOLID;
}

void
wxPdfDCImpl::SetPalette(const wxPalette& palette)
{
  // Not yet implemented
  wxUnusedVar(palette);
}

void
wxPdfDCImpl::SetTextForeground(const wxColour& colour)
{
  if (colour.IsOk())
  {
    m_textForegroundColour = colour;
  }
}

void
wxPdfDCImpl::DestroyClippingRegion()
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (m_clipping)
  {
    m_pdfDocument->UnsetClipping();
    {
      wxPen x(GetPen()); SetPen(x);
    }
    {
      wxBrush x(GetBrush()); SetBrush(x);
    }
    {
      wxFont x(GetFont()); m_pdfDocument->SetFont(x);
    }
  }
  ResetClipping();
}

wxCoord
wxPdfDCImpl::GetCharHeight() const
{
  // default for 12 point font
  int height = 18;
  int width;
  if (m_font.Ok())
  {
    DoGetTextExtent("x", &width, &height);
  }
  return height;
}

wxCoord
wxPdfDCImpl::GetCharWidth() const
{
  int height;
  int width = 8;
  if (m_font.Ok())
  {
    DoGetTextExtent("x", &width, &height);
  }
  return width;
}

bool
wxPdfDCImpl::CanDrawBitmap() const
{
  return true;
}

bool
wxPdfDCImpl::CanGetTextExtent() const
{
  return true;
}

int
wxPdfDCImpl::GetDepth() const
{
  // TODO: check value
  return 32;
}

wxSize
wxPdfDCImpl::GetPPI() const
{
  int ppi = (int) m_ppi;
  return wxSize(ppi,ppi);
}

void
wxPdfDCImpl::ComputeScaleAndOrigin()
{
  m_scaleX = m_logicalScaleX * m_userScaleX;
  m_scaleY = m_logicalScaleY * m_userScaleY;
}

void
wxPdfDCImpl::SetMapMode(wxMappingMode mode)
{
  m_mappingMode = mode;
  switch (mode)
  {
    case wxMM_TWIPS:
      SetLogicalScale(m_ppi / 1440.0, m_ppi / 1440.0);
      break;
    case wxMM_POINTS:
      SetLogicalScale(m_ppi / 72.0, m_ppi / 72.0);
      break;
    case wxMM_METRIC:
      SetLogicalScale(m_ppi / 25.4, m_ppi / 25.4);
      break;
    case wxMM_LOMETRIC:
      SetLogicalScale(m_ppi / 254.0, m_ppi / 254.0);
      break;
    default:
    case wxMM_TEXT:
      SetLogicalScale(1.0, 1.0);
      break;
  }
}

void
wxPdfDCImpl::SetUserScale(double x, double y)
{
  m_userScaleX = x;
  m_userScaleY = y;
  ComputeScaleAndOrigin();
}

void
wxPdfDCImpl::SetLogicalScale(double x, double y)
{
  m_logicalScaleX = x;
  m_logicalScaleY = y;
  ComputeScaleAndOrigin();
}

void
wxPdfDCImpl::SetLogicalOrigin(wxCoord x, wxCoord y)
{
  m_logicalOriginX = x * m_signX;
  m_logicalOriginY = y * m_signY;
  ComputeScaleAndOrigin();
}

void
wxPdfDCImpl::SetDeviceOrigin(wxCoord x, wxCoord y)
{
  m_deviceOriginX = x;
  m_deviceOriginY = y;
  ComputeScaleAndOrigin();
}

void
wxPdfDCImpl::SetAxisOrientation(bool xLeftRight, bool yBottomUp)
{
  m_signX = (xLeftRight ?  1 : -1);
  m_signY = (yBottomUp  ? -1 :  1);
  ComputeScaleAndOrigin();
}

#if wxUSE_DC_TRANSFORM_MATRIX
bool
wxPdfDCImpl::CanUseTransformMatrix() const
{
  return true;
}

bool
wxPdfDCImpl::SetTransformMatrix(const wxAffineMatrix2D& matrix)
{
  wxCHECK_MSG(m_pdfDocument, false, wxS("Invalid PDF DC"));
  // At the moment concatinating the transformation matrix is not supported
  ResetTransformMatrix();

  // Do nothing, if the transformation matrix equals the identity matrix
  if (matrix.IsIdentity())
  {
    return true;
  }

  // Retrieve the components of the transformation matrix
  wxMatrix2D mat;
  wxPoint2DDouble tr;
  matrix.Get(&mat, &tr);
  m_matrix = matrix;

  // Start PDF transformation
  m_inTransform = true;
  m_pdfPenSaved = m_pdfPen;
  m_pdfBrushSaved = m_pdfBrush;
  m_pdfPen = wxNullPen;
  m_pdfBrush = wxNullBrush;
  m_pdfDocument->StartTransform();

  // Scale the translation vector according to PDF resolution
  double docScale = 72.0 / m_ppi;
  double scaleX = m_scaleX * docScale;
  double scaleY = m_scaleY * docScale;
  m_pdfDocument->Transform(mat.m_11, mat.m_12, mat.m_21, mat.m_22, scaleX * tr.m_x, scaleY * tr.m_y);

  return true;
}

wxAffineMatrix2D
wxPdfDCImpl::GetTransformMatrix() const
{
  wxCHECK_MSG(m_pdfDocument, wxAffineMatrix2D(), wxS("Invalid PDF DC"));
  return m_matrix; 
}

void
wxPdfDCImpl::ResetTransformMatrix()
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));

  if (m_inTransform)
  {
    m_pdfDocument->StopTransform();
    m_matrix = wxAffineMatrix2D();
    m_inTransform = false;
    m_pdfPen = m_pdfPenSaved;
    m_pdfBrush = m_pdfBrushSaved;
  }
}
#endif // wxUSE_DC_TRANSFORM_MATRIX

void
wxPdfDCImpl::SetLogicalFunction(wxRasterOperationMode function)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  // TODO: check implementation
  m_logicalFunction = function;
  switch(function)
  {
    case wxAND:
      m_pdfDocument->SetAlpha(0.5, 0.5);
      break;
    case wxCOPY:
    default:
      m_pdfDocument->SetAlpha(1.0, 1.0);
      break;
  }
}

// the true implementations

bool
wxPdfDCImpl::DoFloodFill(wxCoord x, wxCoord y, const wxColour& col, wxFloodFillStyle style)
{
  wxUnusedVar(x);
  wxUnusedVar(y);
  wxUnusedVar(col);
  wxUnusedVar(style);
  wxFAIL_MSG(wxString(wxS("wxPdfDCImpl::FloodFill: "))+_("Not implemented."));
  return false;
}

void
wxPdfDCImpl::DoGradientFillLinear(const wxRect& rect,
                              const wxColour& initialColour,
                              const wxColour& destColour,
                              wxDirection nDirection)
{
  // TODO: native implementation
  wxDCImpl::DoGradientFillLinear(rect, initialColour, destColour, nDirection);
}

void
wxPdfDCImpl::DoGradientFillConcentric(const wxRect& rect,
                                  const wxColour& initialColour,
                                  const wxColour& destColour,
                                  const wxPoint& circleCenter)
{
  // TODO: native implementation
  wxDCImpl::DoGradientFillConcentric(rect, initialColour, destColour, circleCenter);
}

bool
wxPdfDCImpl::DoGetPixel(wxCoord x, wxCoord y, wxColour* col) const
{
  wxUnusedVar(x);
  wxUnusedVar(y);
  wxUnusedVar(col);
  wxFAIL_MSG(wxString(wxS("wxPdfDCImpl::DoGetPixel: "))+_("Not implemented."));
  return false;
}

void
wxPdfDCImpl::DoDrawPoint(wxCoord x, wxCoord y)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  SetupPen();
  SetupAlpha();
  double xx = ScaleLogicalToPdfX(x);
  double yy = ScaleLogicalToPdfY(y);
  m_pdfDocument->SetFillColour(m_pdfDocument->GetDrawColour());
  m_pdfDocument->Rect(xx-0.5, yy-0.5, 1.0, 1.0);
  CalcBoundingBox(x, y);
}

#if wxUSE_SPLINES
void
wxPdfDCImpl::DoDrawSpline(const wxPointList* points)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  SetupPen();
  SetupAlpha();
  wxASSERT_MSG( points, wxS("NULL pointer to spline points?") );
  wxASSERT_MSG( points->GetCount() > 2 , wxS("incomplete list of spline points?") );
#if 0
  wxPoint* p;
  wxPdfArrayDouble xp, yp;
  wxList::compatibility_iterator node = points->GetFirst();
  while (node)
  {
    p = (wxPoint *)node->GetData();
    xp.Add(ScaleLogicalToPdfX(p->x));
    yp.Add(ScaleLogicalToPdfY(p->y));
    node = node->GetNext();
  }
  m_pdfDocument->BezierSpline(xp, yp, wxPDF_STYLE_DRAW);
#endif

  // Code taken from wxDC MSW implementation
  // quadratic b-spline to cubic bezier spline conversion
  //
  // quadratic spline with control points P0,P1,P2
  // P(s) = P0*(1-s)^2 + P1*2*(1-s)*s + P2*s^2
  //
  // bezier spline with control points B0,B1,B2,B3
  // B(s) = B0*(1-s)^3 + B1*3*(1-s)^2*s + B2*3*(1-s)*s^2 + B3*s^3
  //
  // control points of bezier spline calculated from b-spline
  // B0 = P0
  // B1 = (2*P1 + P0)/3
  // B2 = (2*P1 + P2)/3
  // B3 = P2

  double x1, y1, x2, y2, cx1, cy1, cx4, cy4;
  double bx1, by1, bx2, by2, bx3, by3;

  wxPointList::compatibility_iterator node = points->GetFirst();
  wxPoint* p = (wxPoint*) node->GetData();

  x1 = ScaleLogicalToPdfX(p->x);
  y1 = ScaleLogicalToPdfY(p->y);
  m_pdfDocument->MoveTo(x1, y1);

  node = node->GetNext();
  p = (wxPoint*) node->GetData();

  bx1 = x2 = ScaleLogicalToPdfX(p->x);
  by1 = y2 = ScaleLogicalToPdfY(p->y);
  cx1 = ( x1 + x2 ) / 2;
  cy1 = ( y1 + y2 ) / 2;
  bx3 = bx2 = cx1;
  by3 = by2 = cy1;
  m_pdfDocument->CurveTo(bx1, by1, bx2, by2, bx3, by3);

#if !wxUSE_STL
  while ((node = node->GetNext()) != NULL)
#else
  while ((node = node->GetNext()))
#endif // !wxUSE_STL
  {
    p = (wxPoint*) node->GetData();
    x1 = x2;
    y1 = y2;
    x2 = ScaleLogicalToPdfX(p->x);
    y2 = ScaleLogicalToPdfY(p->y);
    cx4 = (x1 + x2) / 2;
    cy4 = (y1 + y2) / 2;
    // B0 is B3 of previous segment
    // B1:
    bx1 = (x1*2+cx1)/3;
    by1 = (y1*2+cy1)/3;
    // B2:
    bx2 = (x1*2+cx4)/3;
    by2 = (y1*2+cy4)/3;
    // B3:
    bx3 = cx4;
    by3 = cy4;
    cx1 = cx4;
    cy1 = cy4;
    m_pdfDocument->CurveTo(bx1, by1, bx2, by2, bx3, by3);
  }

  bx1 = bx3;
  by1 = by3;
  bx3 = bx2 = x2;
  by3 = by2 = y2;
  m_pdfDocument->CurveTo(bx1, by1, bx2, by2, bx3, by3);
  m_pdfDocument->EndPath(wxPDF_STYLE_DRAW);
}
#endif

void
wxPdfDCImpl::DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if  (GetPen().IsNonTransparent())
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    m_pdfDocument->Line(ScaleLogicalToPdfX(x1), ScaleLogicalToPdfY(y1),
                        ScaleLogicalToPdfX(x2), ScaleLogicalToPdfY(y2));
    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
  }
}

void
wxPdfDCImpl::DoDrawArc(wxCoord x1, wxCoord y1,
                       wxCoord x2, wxCoord y2,
                       wxCoord xc, wxCoord yc)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doDraw || doFill)
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    const double start = angleByCoords(x1, y1, xc, yc);
    const double end   = angleByCoords(x2, y2, xc, yc);
    const double xx1 = ScaleLogicalToPdfX(x1);
    const double yy1 = ScaleLogicalToPdfY(y1);
    const double xxc = ScaleLogicalToPdfX(xc);
    const double yyc = ScaleLogicalToPdfY(yc);
    const double rx = xx1 - xxc;
    const double ry = yy1 - yyc;
    const double r = sqrt(rx * rx + ry * ry);
    int style = wxPDF_STYLE_FILLDRAW;
    if (!(doDraw && doFill))
    {
      style = (doFill) ? wxPDF_STYLE_FILL : wxPDF_STYLE_DRAW;
    }
    m_pdfDocument->Ellipse(xxc, yyc, r, 0, 0, start, end, style, 8, doFill);
    wxCoord radius = (wxCoord) sqrt( (double)((x1-xc)*(x1-xc)+(y1-yc)*(y1-yc)) );
    CalcBoundingBox(xc-radius, yc-radius);
    CalcBoundingBox(xc+radius, yc+radius);
  }
}

void
wxPdfDCImpl::DoDrawCheckMark(wxCoord x, wxCoord y,
                             wxCoord width, wxCoord height)
{
  // TODO: native implementation?
  wxDCImpl::DoDrawCheckMark(x, y, width, height);
}

void
wxPdfDCImpl::DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord width, wxCoord height,
                               double sa, double ea)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doDraw || doFill)
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    m_pdfDocument->SetLineWidth(ScaleLogicalToPdfXRel(1)); // pen width != 1 sometimes fools readers when closing paths
    if (doFill)
    {
      m_pdfDocument->Ellipse(ScaleLogicalToPdfX(x + (width + 1) / 2),
                             ScaleLogicalToPdfY(y + (height + 1) / 2),
                             ScaleLogicalToPdfXRel((width + 1) / 2),
                             ScaleLogicalToPdfYRel((height + 1) / 2),
                             0, sa, ea, wxPDF_STYLE_FILL, 8, true);
    }
    if (doDraw)
    {
      m_pdfDocument->Ellipse(ScaleLogicalToPdfX(x + (width + 1) / 2),
                             ScaleLogicalToPdfY(y + (height + 1) / 2),
                             ScaleLogicalToPdfXRel((width + 1) / 2),
                             ScaleLogicalToPdfYRel((height + 1) / 2),
                             0, sa, ea, wxPDF_STYLE_DRAW, 8, false);
    }
    CalcBoundingBox(x, y);
    CalcBoundingBox(x+width, y+height);
  }
}

void
wxPdfDCImpl::DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doDraw || doFill)
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    m_pdfDocument->Rect(ScaleLogicalToPdfX(x), ScaleLogicalToPdfY(y),
                        ScaleLogicalToPdfXRel(width), ScaleLogicalToPdfYRel(height),
                        GetDrawingStyle());
    CalcBoundingBox(x, y);
    CalcBoundingBox(x + width, y + height);
  }
}

void
wxPdfDCImpl::DoDrawRoundedRectangle(wxCoord x, wxCoord y,
                                    wxCoord width, wxCoord height,
                                    double radius)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (radius < 0.0)
  {
    // Now, a negative radius is interpreted to mean
    // 'the proportion of the smallest X or Y dimension'
    double smallest = width < height ? width : height;
    radius =  (-radius * smallest);
  }
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doDraw || doFill)
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    m_pdfDocument->RoundedRect(ScaleLogicalToPdfX(x), ScaleLogicalToPdfY(y),
                               ScaleLogicalToPdfXRel(width), ScaleLogicalToPdfYRel(height),
                               ScaleLogicalToPdfXRel(wxRound(radius)), wxPDF_CORNER_ALL, GetDrawingStyle());
    CalcBoundingBox(x, y);
    CalcBoundingBox(x + width, y + height);
  }
}

void
wxPdfDCImpl::DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doDraw || doFill)
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    m_pdfDocument->Ellipse(ScaleLogicalToPdfX(x + (width + 1) / 2),
                           ScaleLogicalToPdfY(y + (height + 1) / 2),
                           ScaleLogicalToPdfXRel((width + 1) / 2),
                           ScaleLogicalToPdfYRel((height + 1) / 2),
                           0, 0, 360, GetDrawingStyle());
    CalcBoundingBox(x - width, y - height);
    CalcBoundingBox(x + width, y + height);
  }
}

void
wxPdfDCImpl::DoCrossHair(wxCoord x, wxCoord y)
{
  wxUnusedVar(x);
  wxUnusedVar(y);
  wxFAIL_MSG(wxString(wxS("wxPdfDCImpl::DoCrossHair: "))+_("Not implemented."));
}

void
wxPdfDCImpl::DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y)
{
  DoDrawBitmap(icon, x, y, true);
}

void
wxPdfDCImpl::DoDrawBitmap(const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  wxCHECK_RET( IsOk(), wxS("wxPdfDCImpl::DoDrawBitmap - invalid DC") );
  wxCHECK_RET( bitmap.Ok(), wxS("wxPdfDCImpl::DoDrawBitmap - invalid bitmap") );

  if (!bitmap.Ok()) return;

  int idMask = 0;
  wxImage image = bitmap.ConvertToImage();
  if (!image.Ok()) return;

  if (!useMask)
  {
    image.SetMask(false);
  }
  wxCoord w = image.GetWidth();
  wxCoord h = image.GetHeight();

  const double ww = ScaleLogicalToPdfXRel(w);
  const double hh = ScaleLogicalToPdfYRel(h);

  const double xx = ScaleLogicalToPdfX(x);
  const double yy = ScaleLogicalToPdfY(y);

  int imageCount = IncreaseImageCounter();
  wxString imgName = wxString::Format(wxS("pdfdcimg%d"), imageCount);

  if (bitmap.GetDepth() == 1)
  {
    wxPen savePen = m_pen;
    wxBrush saveBrush = m_brush;
    SetPen(*wxTRANSPARENT_PEN);
    SetBrush(m_textBackgroundColour);
    DoDrawRectangle(x, y, w, h);
    SetBrush(m_textForegroundColour);
    m_pdfDocument->Image(imgName, image, xx, yy, ww, hh, wxPdfLink(-1), idMask, m_jpegFormat, m_jpegQuality);
    SetBrush(saveBrush);
    SetPen(savePen);
  }
  else
  {
    m_pdfDocument->Image(imgName, image, xx, yy, ww, hh, wxPdfLink(-1), idMask, m_jpegFormat, m_jpegQuality);
  }
}

void
wxPdfDCImpl::DoDrawText(const wxString& text, wxCoord x, wxCoord y)
{
  if (text.Find(wxS('\n')) == wxNOT_FOUND)
  {
    // text contains a single line: just draw it
    DoDrawRotatedText(text, x, y, 0.0);
  }
  else
  {
    // this is a multiline text: split it and print the lines one by one
    wxCoord charH = GetCharHeight();
    wxCoord curY = y;
    wxStringTokenizer tok( text, "\n" );
    while( tok.HasMoreTokens() ) {
      wxString s = tok.GetNextToken();
      DoDrawRotatedText(s, x, curY, 0.0);
      curY += charH;
    }
  }
}

void
wxPdfDCImpl::DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));

  wxFont* fontToUse = &m_font;
  if (!fontToUse->IsOk())
  {
    return;
  }
  wxFont old = m_font;
  wxCoord originalY = y;

  wxPdfFontDescription desc = m_pdfDocument->GetFontDescription();
  int height, descent;
  CalculateFontMetrics(&desc, fontToUse->GetPointSize(), &height, NULL, &descent, NULL);
  y += (height - abs(descent));

  if (m_cachedPdfColour.GetColourType() == wxPDF_COLOURTYPE_UNKNOWN ||
      m_textForegroundColour.GetRGB() != m_cachedRGB)
  {
    m_cachedRGB = m_textForegroundColour.GetRGB();
    m_cachedPdfColour.SetColour(wxColour(m_cachedRGB));
  }
  if (m_cachedPdfColour != m_pdfDocument->GetTextColour())
  {
    m_pdfDocument->SetTextColour(m_cachedPdfColour);
  }

  m_pdfDocument->SetFontSize(ScaleFontSizeToPdf(fontToUse->GetPointSize()));

  // Get extent of whole text.
  wxCoord w, h, heightLine;
  GetOwner()->GetMultiLineTextExtent(text, &w, &h, &heightLine);

  // Compute the shift for the origin of the next line.
  static double pi = 4. * atan(1.0);
  const double rad = (angle * pi) / 180.0;
  const double dx = heightLine * sin(rad);
  const double dy = heightLine * cos(rad);

  // Extract all text lines
  const wxArrayString lines = wxSplit(text, '\n', '\0');

  // Draw background for all text lines
  if (m_backgroundMode != wxBRUSHSTYLE_TRANSPARENT && m_textBackgroundColour.Ok())
  {
    if (angle != 0)
    {
      m_pdfDocument->StartTransform();
      m_pdfDocument->Rotate(angle, ScaleLogicalToPdfX(x), ScaleLogicalToPdfY(originalY));
    }
    wxBrush previousBrush = GetBrush();
    SetBrush(wxBrush(m_textBackgroundColour));
    SetupBrush(true);
    SetupAlpha();
    // draw rectangle line by line
    for (size_t lineNum = 0; lineNum < lines.size(); lineNum++)
    {
      DoGetTextExtent(lines[lineNum], &w, &h);
      m_pdfDocument->Rect(ScaleLogicalToPdfX(x), ScaleLogicalToPdfY(originalY + lineNum*heightLine), ScaleLogicalToPdfXRel(w), ScaleLogicalToPdfYRel(h), wxPDF_STYLE_FILL);
    }
    SetBrush(previousBrush);
    SetupAlpha();
    if (angle != 0)
    {
      m_pdfDocument->StopTransform();
    }
  }

  m_pdfDocument->StartTransform();
  SetupTextAlpha();
  
  // Draw all text line by line
  for (size_t lineNum = 0; lineNum < lines.size(); lineNum++)
  {
    // Calculate origin for each line to avoid accumulation of rounding errors.
    m_pdfDocument->RotatedText(ScaleLogicalToPdfX(x + wxRound(lineNum*dx)), ScaleLogicalToPdfY(y + wxRound(lineNum*dy)),
                               ScaleLogicalToPdfX(x + wxRound(lineNum*dx)), ScaleLogicalToPdfY(originalY + wxRound(lineNum*dy)), lines[lineNum], angle);
  }

  m_pdfDocument->StopTransform();

  if (*fontToUse != old)
  {
    SetFont(old);
  }
}

bool
wxPdfDCImpl::DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                    wxDC* source, wxCoord xsrc, wxCoord ysrc,
                    wxRasterOperationMode rop /*= wxCOPY*/, bool useMask /*= false*/,
                    wxCoord xsrcMask /*= -1*/, wxCoord ysrcMask /*= -1*/)
{
//  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  wxCHECK_MSG( IsOk(), false, wxS("wxPdfDCImpl::DoBlit - invalid DC") );
  wxCHECK_MSG( source->IsOk(), false, wxS("wxPdfDCImpl::DoBlit - invalid source DC") );

  wxUnusedVar(useMask);
  wxUnusedVar(xsrcMask);
  wxUnusedVar(ysrcMask);

  // blit into a bitmap
  wxBitmap bitmap((int)width, (int)height);
  wxMemoryDC memDC;
  memDC.SelectObject(bitmap);
  memDC.Blit(0, 0, width, height, source, xsrc, ysrc, rop);
  memDC.SelectObject(wxNullBitmap);

  // draw bitmap. scaling and positioning is done there
  DoDrawBitmap( bitmap, xdest, ydest );

  return true;
}

void
wxPdfDCImpl::DoGetSize(int* width, int* height) const
{
  int w;
  int h;
  if (m_templateMode)
  {
    w = wxRound(m_templateWidth * m_pdfDocument->GetScaleFactor());
    h = wxRound(m_templateHeight * m_pdfDocument->GetScaleFactor());
  }
  else
  {
    wxPaperSize id = m_printData.GetPaperId();
    wxPrintPaperType *paper = wxThePrintPaperDatabase->FindPaperType(id);
    if (!paper) paper = wxThePrintPaperDatabase->FindPaperType(wxPAPER_A4);

    w = 595;
    h = 842;
    if (paper)
    {
      w = paper->GetSizeDeviceUnits().x;
      h = paper->GetSizeDeviceUnits().y;
    }

    if (m_printData.GetOrientation() == wxLANDSCAPE)
    {
      int tmp = w;
      w = h;
      h = tmp;
    }
  }

  if (width)
  {
    *width = wxRound( w * m_ppi / 72.0 );
  }

  if (height)
  {
    *height = wxRound( h * m_ppi / 72.0 );
  }
}

void
wxPdfDCImpl::DoGetSizeMM(int* width, int* height) const
{
  int w;
  int h;
  if (m_templateMode)
  {
    w = wxRound(m_templateWidth * m_pdfDocument->GetScaleFactor() * 25.4/72.0);
    h = wxRound(m_templateHeight * m_pdfDocument->GetScaleFactor() * 25.4/72.0);
  }
  else
  {
    wxPaperSize id = m_printData.GetPaperId();
    wxPrintPaperType *paper = wxThePrintPaperDatabase->FindPaperType(id);
    if (!paper) paper = wxThePrintPaperDatabase->FindPaperType(wxPAPER_A4);

    w = 210;
    h = 297;
    if (paper)
    {
      w = paper->GetWidth() / 10;
      h = paper->GetHeight() / 10;
    }

    if (m_printData.GetOrientation() == wxLANDSCAPE)
    {
      int tmp = w;
      w = h;
      h = tmp;
    }
  }
  if (width)
  {
    *width = w;
  }
  if (height)
  {
    *height = h;
  }
}

void
wxPdfDCImpl::DoDrawLines(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (GetPen().IsNonTransparent())
  {
    SetupPen();
    SetupAlpha();
    int i;
    for (i = 0; i < n; ++i)
    {
      const wxPoint& point = points[i];
      double xx = ScaleLogicalToPdfX(xoffset + point.x);
      double yy = ScaleLogicalToPdfY(yoffset + point.y);
      CalcBoundingBox(point.x + xoffset, point.y + yoffset);
      if (i == 0)
      {
        m_pdfDocument->MoveTo(xx, yy);
      }
      else
      {
        m_pdfDocument->LineTo(xx, yy);
      }
    }
    m_pdfDocument->EndPath(wxPDF_STYLE_DRAW);
  }
}

void
wxPdfDCImpl::DoDrawPolygon(int n, const wxPoint points[],
                           wxCoord xoffset, wxCoord yoffset,
                           wxPolygonFillMode fillStyle /* = wxODDEVEN_RULE*/)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doDraw || doFill)
  {
    SetupBrush();
    SetupPen();
    SetupAlpha();
    wxPdfArrayDouble xp;
    wxPdfArrayDouble yp;
    int i;
    for (i = 0; i < n; ++i)
    {
      const wxPoint& point = points[i];
      xp.Add(ScaleLogicalToPdfX(xoffset + point.x));
      yp.Add(ScaleLogicalToPdfY(yoffset + point.y));
      CalcBoundingBox(point.x + xoffset, point.y + yoffset);
    }
    int saveFillingRule = m_pdfDocument->GetFillingRule();
    m_pdfDocument->SetFillingRule(fillStyle);
    int style = GetDrawingStyle();
    m_pdfDocument->Polygon(xp, yp, style);
    m_pdfDocument->SetFillingRule(saveFillingRule);
  }
}

void
wxPdfDCImpl::DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (n > 0)
  {
    bool doFill = GetBrush().IsNonTransparent();
    bool doDraw = GetPen().IsNonTransparent();
    if (doDraw || doFill)
    {
      SetupBrush();
      SetupPen();
      SetupAlpha();
      int style = GetDrawingStyle();
      int saveFillingRule = m_pdfDocument->GetFillingRule();
      m_pdfDocument->SetFillingRule(fillStyle);

      int ofs = 0;
      int i, j;
      for (j = 0; j < n; ofs += count[j++])
      {
        wxPdfArrayDouble xp;
        wxPdfArrayDouble yp;
        for (i = 0; i < count[j]; ++i)
        {
          const wxPoint& point = points[ofs + i];
          xp.Add(ScaleLogicalToPdfX(xoffset + point.x));
          yp.Add(ScaleLogicalToPdfY(yoffset + point.y));
          CalcBoundingBox(point.x + xoffset, point.y + yoffset);
        }
        m_pdfDocument->Polygon(xp, yp, style);
      }
      m_pdfDocument->SetFillingRule(saveFillingRule);
    }
  }
}

void
wxPdfDCImpl::DoSetClippingRegionAsRegion(const wxRegion& region)
{
  wxCoord x, y, w, h;
  region.GetBox(x, y, w, h);
  DoSetClippingRegion(x, y, w, h);
}

void
wxPdfDCImpl::DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  if (m_clipping)
  {
    DestroyClippingRegion();
  }

  m_clipX1 = (int) x;
  m_clipY1 = (int) y;
  m_clipX2 = (int) (x + width);
  m_clipY2 = (int) (y + height);

  // Use the current path as a clipping region
  m_pdfDocument->ClippingRect(ScaleLogicalToPdfX(x),
                              ScaleLogicalToPdfY(y),
                              ScaleLogicalToPdfXRel(width),
                              ScaleLogicalToPdfYRel(height));
  m_clipping = true;
}

void
wxPdfDCImpl::DoSetDeviceClippingRegion(const wxRegion& region)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  wxCoord x, y, w, h;
  region.GetBox(x, y, w, h);
  DoSetClippingRegion(DeviceToLogicalX(x), DeviceToLogicalY(y), DeviceToLogicalXRel(w), DeviceToLogicalYRel(h));
}

void
wxPdfDCImpl::DoGetTextExtent(const wxString& text,
                         wxCoord* x, wxCoord* y,
                         wxCoord* descent,
                         wxCoord* externalLeading,
                         const wxFont* theFont) const
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));

  const wxFont* fontToUse = theFont;
  if (!fontToUse)
  {
    fontToUse = const_cast<wxFont*>(&m_font);
  }

  if (fontToUse)
  {
    wxFont old = m_font;

    const_cast<wxPdfDCImpl*>(this)->SetFont(*fontToUse);
    wxPdfFontDescription desc = const_cast<wxPdfDCImpl*>(this)->m_pdfDocument->GetFontDescription();
    int myAscent, myDescent, myHeight, myExtLeading;
    CalculateFontMetrics(&desc, fontToUse->GetPointSize(), &myHeight, &myAscent, &myDescent, &myExtLeading);

    if (descent)
    {
      *descent = abs(myDescent);
    }
    if( externalLeading )
    {
      *externalLeading = myExtLeading;
    }
    if (x)
    {
      *x = ScalePdfToFontMetric((double)const_cast<wxPdfDCImpl*>(this)->m_pdfDocument->GetStringWidth(text));
    }
    if (y)
    {
      *y = myHeight;
    }
    if (*fontToUse != old)
    {
      const_cast<wxPdfDCImpl*>(this)->SetFont(old);
    }
  }
  else
  {
    if (x)
    {
      *x = 0;
    }
    if (y)
    {
      *y = 0;
    }
    if (descent)
    {
      *descent = 0;
    }
    if (externalLeading)
    {
      *externalLeading = 0;
    }
  }
}

bool
wxPdfDCImpl::DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const
{
  wxCHECK_MSG( m_pdfDocument, false, wxS("wxPdfDCImpl::DoGetPartialTextExtents - invalid DC") );

  /// very slow - but correct ??

  const size_t len = text.length();
  if (len == 0)
  {
    return true;
  }

  widths.Empty();
  widths.Add(0, len);
  int w, h;

  wxString buffer;
  buffer.Alloc(len);

  for ( size_t i = 0; i < len; ++i )
  {
    buffer.Append(text.Mid( i, 1));
    DoGetTextExtent(buffer, &w, &h);
    widths[i] = w;
  }

  buffer.Clear();
  return true;
}

bool
wxPdfDCImpl::MustSetCurrentPen(const wxPen& currentPen) const
{
  if (m_pdfPen == wxNullPen)
  {
    return true;
  }

  // Current pen has to be set for PDF if not identical to pen in use for PDF
  return (m_pdfPen.GetWidth()  != currentPen.GetWidth()) ||
         (m_pdfPen.GetJoin()   != currentPen.GetJoin())  ||
         (m_pdfPen.GetCap()    != currentPen.GetCap())   ||
         (m_pdfPen.GetStyle()  != currentPen.GetStyle()) ||
         (m_pdfPen.GetColour() != currentPen.GetColour());
}

bool
wxPdfDCImpl::MustSetCurrentBrush(const wxBrush& currentBrush) const
{
  if (m_pdfBrush == wxNullBrush)
  {
    return true;
  }

  // Current pen has to be set for PDF if not identical to pen in use for PDF
  return (m_pdfBrush.GetColour() != currentBrush.GetColour());
}

void
wxPdfDCImpl::SetupPen(bool force)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  // pen
  const wxPen& curPen = GetPen();
  if (curPen != wxNullPen)
  {
    if (force || MustSetCurrentPen(curPen))
    {
      wxPdfLineStyle style = m_pdfDocument->GetLineStyle();
      wxPdfArrayDouble dash;
      style.SetColour(wxColour(curPen.GetColour().Red(),
                               curPen.GetColour().Green(),
                               curPen.GetColour().Blue()));
      double penWidth = 1.0;
      if (curPen.GetWidth())
      {
        penWidth = ScaleLogicalToPdfXRel(curPen.GetWidth());
        style.SetWidth(penWidth);
      }
      switch (curPen.GetJoin())
      {
        case wxJOIN_BEVEL:
          style.SetLineJoin(wxPDF_LINEJOIN_BEVEL);
          break;
        case wxJOIN_ROUND:
          style.SetLineJoin(wxPDF_LINEJOIN_ROUND);
          break;
        case wxJOIN_MITER:
        default:
          style.SetLineJoin(wxPDF_LINEJOIN_MITER);
          break;
      }
      switch (curPen.GetCap())
      {
        case wxCAP_BUTT:
          style.SetLineCap(wxPDF_LINECAP_BUTT);
          break;
        case wxCAP_PROJECTING:
          style.SetLineCap(wxPDF_LINECAP_SQUARE);
          break;
        case wxCAP_ROUND:
        default:
          style.SetLineCap(wxPDF_LINECAP_ROUND);
          break;
      }
      switch (curPen.GetStyle())
      {
        case wxPENSTYLE_DOT:
          if (wxPDF_LINECAP_BUTT == style.GetLineCap())
          {
            dash.Add(1.0 * penWidth);
          }
          else
          {
            dash.Add(0.0 * penWidth);
          }
          dash.Add(2.0 * penWidth);
          style.SetDash(dash);
          break;
        case wxPENSTYLE_LONG_DASH:
          dash.Add(3.5 * penWidth);
          dash.Add(5.0 * penWidth);
          style.SetDash(dash);
          break;
        case wxPENSTYLE_SHORT_DASH:
          dash.Add(1.5 * penWidth);
          dash.Add(3.0 * penWidth);
          style.SetDash(dash);
          break;
        case wxPENSTYLE_DOT_DASH:
          {
            if (wxPDF_LINECAP_BUTT == style.GetLineCap())
            {
              dash.Add(1.0 * penWidth);
            }
            else
            {
              dash.Add(0.0 * penWidth);
            }
            dash.Add(2.0 * penWidth);
            dash.Add(3.0 * penWidth);
            dash.Add(2.0 * penWidth);
            style.SetDash(dash);
          }
          break;
        case wxPENSTYLE_SOLID:
        default:
          style.SetDash(dash);
          break;
      }

      m_pdfPen = curPen;
      m_pdfDocument->SetLineStyle(style);
    }
  }
  else
  {
    m_pdfDocument->SetDrawColour(0, 0, 0);
    m_pdfDocument->SetLineWidth(ScaleLogicalToPdfXRel(1));
  }
}

void
wxPdfDCImpl::SetupBrush(bool force)
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  // brush
  const wxBrush& curBrush = GetBrush();
  if (curBrush != wxNullBrush)
  {
    if (force || MustSetCurrentBrush(curBrush))
    {
      wxColour brushColour = curBrush.GetColour();
      wxString pdfPatternName;
      wxPdfPatternStyle pdfPatternStyle = wxPDF_PATTERNSTYLE_NONE;
      switch (curBrush.GetStyle())
      {
      case wxBRUSHSTYLE_BDIAGONAL_HATCH:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_BDIAGONAL_HATCH;
        pdfPatternName = "dcHatchBDiagonal";
        break;
      case wxBRUSHSTYLE_CROSSDIAG_HATCH:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_CROSSDIAG_HATCH;
        pdfPatternName = "dcHatchCrossDiag";
        break;
      case wxBRUSHSTYLE_FDIAGONAL_HATCH:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_FDIAGONAL_HATCH;
        pdfPatternName = "dcHatchFDiagonal";
        break;
      case wxBRUSHSTYLE_CROSS_HATCH:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_CROSS_HATCH;
        pdfPatternName = "dcHatchCross";
        break;
      case wxBRUSHSTYLE_HORIZONTAL_HATCH:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_HORIZONTAL_HATCH;
        pdfPatternName = "dcHatchHorizontal";
        break;
      case wxBRUSHSTYLE_VERTICAL_HATCH:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_VERTICAL_HATCH;
        pdfPatternName = "dcHatchVertical";
        break;
      case wxBRUSHSTYLE_STIPPLE:
        pdfPatternStyle = wxPDF_PATTERNSTYLE_IMAGE;
        pdfPatternName = "dcImagePattern";
        break;
      case wxBRUSHSTYLE_STIPPLE_MASK:
      case wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE:
        // Stipple mask / stipple mask opaque not implemented
      case wxBRUSHSTYLE_TRANSPARENT:
      case wxBRUSHSTYLE_SOLID:
      default:
        break;
      }
      if (pdfPatternStyle >= wxPDF_PATTERNSTYLE_FIRST_HATCH && pdfPatternStyle <= wxPDF_PATTERNSTYLE_LAST_HATCH)
      {
        double patternSize = 6.0 / m_pdfDocument->GetScaleFactor();
        wxString patternName = pdfPatternName + wxString::Format(wxS("#%8x"), brushColour.GetRGBA());
        m_pdfDocument->AddPattern(patternName, pdfPatternStyle, patternSize, patternSize, brushColour);
        m_pdfDocument->SetFillPattern(patternName);
      }
      else if (pdfPatternStyle == wxPDF_PATTERNSTYLE_IMAGE)
      {
        wxImage imagePattern = curBrush.GetStipple()->ConvertToImage();
        if (imagePattern.Ok())
        {
          imagePattern.SetMask(false);
          wxString patternName = pdfPatternName + wxString::Format(wxS("#%d"), IncreaseImageCounter());
          m_pdfDocument->AddPattern(patternName, pdfPatternStyle, ScaleLogicalToPdfXRel(imagePattern.GetWidth()), ScaleLogicalToPdfYRel(imagePattern.GetHeight()), brushColour);
          m_pdfDocument->SetFillPattern(patternName);
        }
        else
        {
          m_pdfDocument->SetFillColour(curBrush.GetColour().Red(), curBrush.GetColour().Green(), curBrush.GetColour().Blue());
        }
      }
      else
      {
        m_pdfDocument->SetFillColour(curBrush.GetColour().Red(), curBrush.GetColour().Green(), curBrush.GetColour().Blue());
      }
      m_pdfBrush = curBrush;
    }
  }
  else
  {
    m_pdfDocument->SetFillColour(0, 0, 0);
  }
}

void
wxPdfDCImpl::SetupAlpha()
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  const wxPen& curPen = GetPen();
  const wxBrush& curBrush = GetBrush();
  double lineAlpha = (curPen.IsOk()   && curPen.IsNonTransparent())   ? (double) curPen.GetColour().Alpha()   / 255.0 : 1.0;
  double fillAlpha = (curBrush.IsOk() && curBrush.IsNonTransparent()) ? (double) curBrush.GetColour().Alpha() / 255.0 : 1.0;
  m_pdfDocument->SetAlpha(lineAlpha, fillAlpha);
}

void
wxPdfDCImpl::SetupTextAlpha()
{
  wxCHECK_RET(m_pdfDocument, wxS("Invalid PDF DC"));
  double textAlpha = m_textForegroundColour.IsOk() ? (double) m_textForegroundColour.Alpha() / 255.0 : 1.0;
  m_pdfDocument->SetAlpha(textAlpha, textAlpha);
}

// Get the current drawing style based on the current brush and pen
int
wxPdfDCImpl::GetDrawingStyle()
{
  int style = wxPDF_STYLE_NOOP;
  bool doFill = GetBrush().IsNonTransparent();
  bool doDraw = GetPen().IsNonTransparent();
  if (doFill && doDraw)
  {
    style = wxPDF_STYLE_FILLDRAW;
  }
  else if (doDraw)
  {
    style = wxPDF_STYLE_DRAW;
  }
  else if (doFill)
  {
    style = wxPDF_STYLE_FILL;
  }
  return style;
}

double
wxPdfDCImpl::ScaleLogicalToPdfX(wxCoord x) const
{
  double docScale = 72.0 / (m_ppi * m_pdfDocument->GetScaleFactor());
  return docScale * (((double)((x - m_logicalOriginX) * m_signX) * m_scaleX) +
         m_deviceOriginX + m_deviceLocalOriginX);
}

double
wxPdfDCImpl::ScaleLogicalToPdfXRel(wxCoord x) const
{
  double docScale = 72.0 / (m_ppi * m_pdfDocument->GetScaleFactor());
  return (double)(x) * m_scaleX * docScale;
}

double
wxPdfDCImpl::ScaleLogicalToPdfY(wxCoord y) const
{
  double docScale = 72.0 / (m_ppi * m_pdfDocument->GetScaleFactor());
  return docScale * (((double)((y - m_logicalOriginY) * m_signY) * m_scaleY) +
         m_deviceOriginY + m_deviceLocalOriginY);
}

double
wxPdfDCImpl::ScaleLogicalToPdfYRel(wxCoord y) const
{
  double docScale = 72.0 / (m_ppi * m_pdfDocument->GetScaleFactor());
  return (double)(y) * m_scaleY * docScale;
}

double
wxPdfDCImpl::ScaleFontSizeToPdf(int pointSize) const
{
  double fontScale;
  double rval;
  switch (m_mappingModeStyle)
  {
    case wxPDF_MAPMODESTYLE_MSW:
      // as implemented in wxMSW
      fontScale = (m_ppiPdfFont / m_ppi);
      rval = (double) pointSize * fontScale * m_scaleY;
      break;
    case wxPDF_MAPMODESTYLE_GTK:
      // as implemented in wxGTK / wxMAC / wxOSX
      fontScale = (m_ppiPdfFont / m_ppi);
      rval = (double) pointSize * fontScale * m_userScaleY;
      break;
    case wxPDF_MAPMODESTYLE_MAC:
      // as implemented in wxGTK / wxMAC / wxOSX
      fontScale = (m_ppiPdfFont / m_ppi);
      rval = (double) pointSize * fontScale * m_userScaleY;
      break;
    case wxPDF_MAPMODESTYLE_PDF:
    case wxPDF_MAPMODESTYLE_PDFFONTSCALE:
      // an implementation where a font size of 12 gets a 12 point font if the
      // mapping mode is wxMM_POINTS and suitable scaled for other modes
      fontScale = (m_mappingMode == wxMM_TEXT) ? (m_ppiPdfFont / m_ppi) : (72.0 / m_ppi);
      rval = (double) pointSize * fontScale * m_scaleY;
      break;
    default:
      // standard and fall through
#if defined( __WXMSW__)
      fontScale = (m_ppiPdfFont / m_ppi);
      rval = (double) pointSize * fontScale * m_scaleY;
#else
      fontScale = (m_ppiPdfFont / m_ppi);
      rval = (double) pointSize * fontScale * m_userScaleY;
#endif
      break;
  }
  return rval;
}

int
wxPdfDCImpl::ScalePdfToFontMetric( double metric ) const
{
  double fontScale = (72.0 / m_ppi) / (double)m_pdfDocument->GetScaleFactor();
  return wxRound(((metric * m_signY) / m_scaleY ) / fontScale);
}

void
wxPdfDCImpl::CalculateFontMetrics(wxPdfFontDescription* desc, int pointSize,
                                  int* height, int* ascent, int* descent, int* extLeading) const
{
  double em_height, em_ascent, em_descent, em_externalLeading;
  int hheaAscender, hheaDescender, hheaLineGap;

  double size;
  if (((m_mappingModeStyle == wxPDF_MAPMODESTYLE_PDF) || (m_mappingModeStyle == wxPDF_MAPMODESTYLE_PDFFONTSCALE)) && (m_mappingMode != wxMM_TEXT))
  {
    size = (double) pointSize;
  }
  else
  {
    size = (double) pointSize * (m_ppiPdfFont / 72.0);
  }

  int os2sTypoAscender, os2sTypoDescender, os2sTypoLineGap, os2usWinAscent, os2usWinDescent;

  desc->GetOpenTypeMetrics(&hheaAscender, &hheaDescender, &hheaLineGap,
                           &os2sTypoAscender, &os2sTypoDescender, &os2sTypoLineGap,
                           &os2usWinAscent, &os2usWinDescent);

  if (hheaAscender)
  {
    em_ascent  = os2usWinAscent;
    em_descent = os2usWinDescent;
    em_externalLeading = (hheaLineGap - ((os2usWinAscent + os2usWinDescent) - (hheaAscender - hheaDescender)));
    if (em_externalLeading < 0)
    {
      em_externalLeading = 0;
    }
    em_height = em_ascent + em_descent;
  }
  else
  {
    // Magic numbers below give reasonable defaults for core fonts
    // This may need adjustment for CJK fonts ??
    em_ascent  = 1325;
    em_descent = 1.085 * desc->GetDescent();
    em_height  = em_ascent + em_descent;
    em_externalLeading = 33;
  }

  if (ascent)
  {
    *ascent = wxRound(em_ascent * size / 1000.0);
  }
  if (descent)
  {
    *descent = wxRound(em_descent * size / 1000.0);
  }
  if (height)
  {
    *height = wxRound(em_height * size / 1000.0);
  }
  if (extLeading)
  {
    *extLeading = wxRound(em_externalLeading * size / 1000.0);
  }
}
