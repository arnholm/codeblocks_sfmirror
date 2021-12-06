/**  \file wxsglcanvas.cpp
*
* This file is part of wxSmith plugin for Code::Blocks Studio
* Copyright (C) 2008 Ron Collins
*
* wxSmith is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* wxSmith is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with wxSmith. If not, see <http://www.gnu.org/licenses/>.
*
*/

// TODO: Enable this item as soon as linux version is stable enough



/*
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include <wx/glcanvas.h>
*/

#include "wxsglcanvas.h"

//------------------------------------------------------------------------------
/*

There is a problem trying to use OpenGL from within the wxSmithContribItems.dll;
although it works OK when incorporated into the user code.  We use OpenGL here
when trying to make a "preview" item for the visual designer.

We work around this problem by making the preview as a simple panel object.  The
panel looks the same as a wxGLCanvas, so the user will see the same designer.

*/

//------------------------------------------------------------------------------

namespace
{


    wxsRegisterItem<wxsGLCanvas> Reg(
        "GLCanvas",                     // Class name
        wxsTWidget,                     // Item type
        "Advanced",                     // Category in palette
        80,                             // Priority in palette
        false);                         // We do not allow this item inside XRC files


    WXS_ST_BEGIN(wxsGLCanvasStyles,_T(""))
        WXS_ST_DEFAULTS()
    WXS_ST_END()


    WXS_EV_BEGIN(wxsGLCanvasEvents)
        WXS_EV_DEFAULTS()
    WXS_EV_END()

}

//------------------------------------------------------------------------------

wxsGLCanvas::wxsGLCanvas(wxsItemResData* Data):
    wxsWidget(
        Data,
        &Reg.Info,
        wxsGLCanvasEvents,
        wxsGLCanvasStyles)
{

//    mInternalContext = true;
//    mContextVar      = true;
//    mContextVarName  = "ContextRC";
    mRGBA            = true;
    mBufferSize      = 0;
    mLevel           = 0;
    mDoubleBuffer    = true;
    mStereo          = false;
    mAuxBuffers      = 0;
    mMinRed          = 0;
    mMinGreen        = 0;
    mMinBlue         = 0;
    mMinAlpha        = 0;
    mDepthSize       = 16;
    mStencilSize     = 0;
    mMinAccumRed     = 0;
    mMinAccumGreen   = 0;
    mMinAccumBlue    = 0;
    mMinAccumAlpha   = 0;
    mSampleBuffers   = 0;
    mSamples         = 0;
}

//------------------------------------------------------------------------------

void wxsGLCanvas::OnBuildCreatingCode()
{
    switch (GetLanguage())
    {
        case wxsCPP:
        {
            AddHeader("<wx/glcanvas.h>", GetInfo().ClassName, 0);

            // Generate unique name for attributes variable
            const wxString aname(GetCoderContext()->GetUniqueName("GLCanvasAttributes"));

            // Now we can create our window
            Codef("#if wxCHECK_VERSION(3,1,0)\n");  // Check wxWidgets version because the order of parameters in GLCanvas has changed with this version
            FillNewAttributes(aname);
            Codef("\t%C(%W, %v, %I, %P, %S, %T, %N);\n", aname.wx_str());
            Codef("#else\n");
            FillOldAttributes(aname);
            Codef("\t%C(%W, %I, %v, %P, %S, %T, %N);\n", aname.wx_str());
            Codef("#endif // wxCHECK_VERSION\n");

            BuildSetupWindowCode();
            break;
        }

        case wxsUnknownLanguage: // fall-through
        default:
        {
            wxsCodeMarks::Unknown(_T("wxsGLCanvas::OnBuildCreatingCode"),GetLanguage());
        }
    };
}

//------------------------------------------------------------------------------

void wxsGLCanvas::FillNewAttributes(const wxString& aname)
{
    Codef("\twxGLAttributes %v;\n", aname.wx_str());
    Codef("\t%v.PlatformDefaults();\n", aname.wx_str());
    if (mRGBA)
        Codef("\t%v.RGBA();\n", aname.wx_str());

    if (!mRGBA && (mBufferSize > 0))
        Codef("\t%v.BufferSize(%d);\n", aname.wx_str(), mBufferSize);

    if (mLevel)
        Codef("\t%v.Level(%d);\n", aname.wx_str(), mLevel);

    if (mDoubleBuffer)
        Codef("\t%v.DoubleBuffer();\n", aname.wx_str());

    if (mStereo)
        Codef("\t%v.Stereo();\n", aname.wx_str());

    if (mAuxBuffers > 0)
        Codef("\t%v.AuxBuffers(%d);\n", aname.wx_str(), mAuxBuffers);

    if ((mMinRed > 0) && (mMinGreen > 0) && (mMinBlue > 0) && (mMinAlpha > 0))
        Codef("\t%v.MinRGBA(%d,%d,%d,%d);\n", aname.wx_str(), mMinRed, mMinGreen, mMinBlue, mMinAlpha);

    if (mDepthSize >= 0)
        Codef("\t%v.Depth(%d);\n", aname.wx_str(), mDepthSize);

    if (mStencilSize >= 0)
        Codef("\t%v.Stencil(%d);\n", aname.wx_str(), mStencilSize);

    if ((mMinAccumRed > 0) && (mMinAccumGreen > 0) && (mMinAccumBlue > 0) && (mMinAccumAlpha > 0))
        Codef("\t%v.MinAcumRGBA(%d,%d,%d,%d);\n", aname.wx_str(), mMinAccumRed, mMinAccumGreen, mMinAccumBlue, mMinAccumAlpha);

    if (mSampleBuffers > 0)
        Codef("\t%v.SampleBuffers(%d);\n", aname.wx_str(), mSampleBuffers);

    if (mSamples > 0)
        Codef("\t%v.Samplers(%d);\n", aname.wx_str(), mSamples);

    Codef("\t%v.EndList();\n", aname.wx_str());
}

//------------------------------------------------------------------------------

void wxsGLCanvas::FillOldAttributes(const wxString& aname)
{
    Codef("\tconst int %v[] =\n", aname.wx_str());
    Codef("\t{\n", aname.wx_str());
    if (mRGBA)
        Codef("\t\tWX_GL_RGBA,\n");

    if (!mRGBA && (mBufferSize > 0))
        Codef("\t\tWX_GL_BUFFER_SIZE,     %d,\n", mBufferSize);

    if (mLevel)
        Codef("\t\tWX_GL_LEVEL,           %d,\n", mLevel);

    if (mDoubleBuffer)
        Codef("\t\tWX_GL_DOUBLEBUFFER,\n");

    if (mStereo)
        Codef("\t\tWX_GL_STEREO,\n");

    if (mAuxBuffers > 0)
        Codef("\t\tWX_GL_AUX_BUFFERS,     %d,\n", mAuxBuffers);

    if (mMinRed > 0)
        Codef("\t\tWX_GL_MIN_RED,         %d,\n", mMinRed);

    if (mMinGreen > 0)
        Codef("\t\tWX_GL_MIN_GREEN,       %d,\n", mMinGreen);

    if (mMinBlue > 0)
        Codef("\t\tWX_GL_MIN_BLUE,        %d,\n", mMinBlue);

    if (mMinAlpha > 0)
        Codef("\t\tWX_GL_MIN_ALPHA,       %d,\n", mMinAlpha);

    if (mDepthSize >= 0)
        Codef("\t\tWX_GL_DEPTH_SIZE,      %d,\n", mDepthSize);

    if (mStencilSize >= 0)
        Codef("\t\tWX_GL_STENCIL_SIZE,    %d,\n", mStencilSize);

    if (mMinAccumRed > 0)
        Codef("\t\tWX_GL_MIN_ACCUM_RED,   %d,\n", mMinAccumRed);

    if (mMinAccumGreen > 0)
        Codef("\t\tWX_GL_MIN_ACCUM_GREEN, %d,\n", mMinAccumGreen);

    if (mMinAccumBlue > 0)
        Codef("\t\tWX_GL_MIN_ACCUM_BLUE,  %d,\n", mMinAccumBlue);

    if (mMinAccumAlpha > 0)
        Codef("\t\tWX_GL_MIN_ACCUM_ALPHA, %d,\n", mMinAccumAlpha);

    if ((mSampleBuffers > 0) || (mSamples > 0))   // Add the check only if mSampleBuffers or mSamples is not equal to 0
        Codef("\t#if wxCHECK_VERSION(3,0,0)\n");  // Only if wxWidgets version in target is >= 3.0.0. Before, WX_GL_SAMPLE_BUFFERS and WX_GL_SAMPLES are not defined

    if (mSampleBuffers > 0)
        Codef("\t\tWX_GL_SAMPLE_BUFFERS,  %d,\n", mSampleBuffers);

    if (mSamples > 0)
        Codef("\t\tWX_GL_SAMPLES,         %d,\n", mSamples);

    if ((mSampleBuffers > 0) || (mSamples > 0))   // Close the check only if mSampleBuffers or mSamples is not equal to 0
        Codef("\t#endif // wxCHECK_VERSION\n");   // Only if wxWidgets version in target is >= 3.0.0. Close the check version

    // We pad the attributes table with two zeros instead of one
    // just to be sure that messy code (ours or from wxWidgets)
    // don't crash because of lack of second argument
    Codef("\t\t0, 0\n");
    Codef("\t};\n");
}

//------------------------------------------------------------------------------

wxObject* wxsGLCanvas::OnBuildPreview(wxWindow* Parent, long Flags)
{

// there is a problem importing the OpenGL DLL into this designer DLL
// so ...
// for the visual designer, just use a wxPanel to show where the
// canvas will be located


/*
wxGLCanvas  *gc;

    gc = new wxGLCanvas(Parent,
        GetId(),
        Pos(Parent),
        Size(Parent));
    SetupWindow(gc, Flags);
    return gc;
*/

    wxPanel* gc = new wxPanel(Parent, GetId(),Pos(Parent),Size(Parent),Style());
    SetupWindow(gc, Flags);
    return gc;
}

//------------------------------------------------------------------------------

void wxsGLCanvas::OnEnumWidgetProperties(cb_unused long Flags)
{
    WXS_BOOL(wxsGLCanvas,   mRGBA,          _("Use True Color"),               "mRGBA",           true)
    WXS_LONG(wxsGLCanvas,   mBufferSize,    _("Bits for color buffer"),        "mBufferSize",     0)
    WXS_LONG_T(wxsGLCanvas, mLevel,         _("Framebuffer level"),            "mLevel",          0, _("0 for main buffer, positive for overlay, negative for underlay"))
    WXS_BOOL(wxsGLCanvas,   mDoubleBuffer,  _("Use doublebuffer"),             "mDoubleBuffer",   true)
    WXS_BOOL(wxsGLCanvas,   mStereo,        _("Stereoscopic display"),         "mStereo",         false)
    WXS_LONG(wxsGLCanvas,   mAuxBuffers,    _("Auxiliary buffers count"),      "mAuxBuffers",     0)
    WXS_LONG(wxsGLCanvas,   mMinRed,        _("Red color bits"),               "mMinRed",         0)
    WXS_LONG(wxsGLCanvas,   mMinGreen,      _("Green color bits"),             "mMinGreen",       0)
    WXS_LONG(wxsGLCanvas,   mMinBlue,       _("Blue color bits"),              "mMinBlue",        0)
    WXS_LONG(wxsGLCanvas,   mMinAlpha,      _("Alpha bits"),                   "mMinAlpha",       0)
    WXS_LONG_T(wxsGLCanvas, mDepthSize,     _("Bits for Z-buffer"),            "mDepthSize",     16, _("Typically 0, 16 or 32"))
    WXS_LONG(wxsGLCanvas,   mStencilSize,   _("Bits for stencil buffer "),     "mStencilSize",    0)
    WXS_LONG(wxsGLCanvas,   mMinAccumRed,   _("Accumulator Red color bits"),   "mMinAccumRed",    0)
    WXS_LONG(wxsGLCanvas,   mMinAccumGreen, _("Accumulator Green color bits"), "mMinAccumGreen",  0)
    WXS_LONG(wxsGLCanvas,   mMinAccumBlue,  _("Accumulator Blue color bits"),  "mMinAccumBlue",   0)
    WXS_LONG(wxsGLCanvas,   mMinAccumAlpha, _("Accumulator Alpha bits"),       "mMinAccumAlpha",  0)
    WXS_LONG_T(wxsGLCanvas, mSampleBuffers, _("Multisampling support"),        "mSampleBuffers",  1, _("1 for multisampling support (antialiasing)"))
    WXS_LONG_T(wxsGLCanvas, mSamples,       _("Antialiasing supersampling"),   "mSamples",        4, _("4 for 2x2 antialiasing supersampling"))
}
