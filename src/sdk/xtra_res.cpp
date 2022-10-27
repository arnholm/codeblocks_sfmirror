/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include "xtra_res.h"
    #include "logmanager.h"
    #include "scrollingdialog.h"
    #include <wx/wx.h>
#endif

#include <wx/xml/xml.h>

/////////////////////////////////////////////////////////////////////////////
// Name:        xh_toolb.cpp
// Purpose:     XRC resource for wxBoxSizer
// Author:      Vaclav Slavik
// Created:     2000/08/11
// RCS-ID:      $Id$
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
// Modified by Ricardo Garcia for Code::Blocks
// Comment: Things would've been much easier if field m_isInside had been
//          protected instead of private! >:(
/////////////////////////////////////////////////////////////////////////////

wxToolBarAddOnXmlHandler::wxToolBarAddOnXmlHandler() :
    m_isInside(FALSE), m_isAddon(false), m_toolbar(nullptr), m_ImageSize(0)
{
    XRC_ADD_STYLE(wxTB_FLAT);
    XRC_ADD_STYLE(wxTB_DOCKABLE);
    XRC_ADD_STYLE(wxTB_VERTICAL);
    XRC_ADD_STYLE(wxTB_HORIZONTAL);
    XRC_ADD_STYLE(wxTB_TEXT);
    XRC_ADD_STYLE(wxTB_NOICONS);
    XRC_ADD_STYLE(wxTB_NODIVIDER);
    XRC_ADD_STYLE(wxTB_NOALIGN);
}

void wxToolBarAddOnXmlHandler::SetToolbarImageSize(int size)
{
    m_ImageSize = size;
    m_PathReplaceString = wxString::Format("%dx%d", size, size);
}

void wxToolBarAddOnXmlHandler::SetCurrentResourceID(const wxString &id)
{
    m_CurrentID = id;
}

// if 'param' has stock_id/stock_client, extracts them and returns true
// Taken from wxWidgets.
static bool GetStockArtAttrs(wxString& art_id, wxString& art_client, const wxXmlNode* paramNode,
                             const wxString& defaultArtClient)
{
    if (paramNode)
    {
        art_id = paramNode->GetAttribute("stock_id", wxEmptyString);

        if (!art_id.empty())
        {
            art_id = wxART_MAKE_ART_ID_FROM_STR(art_id);

            art_client = paramNode->GetAttribute("stock_client", wxEmptyString);
            if (art_client.empty())
                art_client = defaultArtClient;
            else
                art_client = wxART_MAKE_CLIENT_ID_FROM_STR(art_client);

            return true;
        }
    }

    return false;
}

wxBitmap wxToolBarAddOnXmlHandler::GetCenteredBitmap(const wxString& param, const wxSize& size)
{
    wxXmlNode* paramNode = GetParamNode(param);
    // If there is no such tag as requested it is best to return null bitmap, so default processing
    // could have chance to work.
    if (!paramNode)
        return wxNullBitmap;

    wxBitmap bitmap;
    wxString artId, artClient;
    if (GetStockArtAttrs(artId, artClient, paramNode, wxART_TOOLBAR))
    {
        bitmap = wxArtProvider::GetBitmap(artId, artClient);
        if (bitmap.IsOk())
            return bitmap;
    }

    const wxString name(GetParamValue(paramNode));
    if (name.empty())
        return wxArtProvider::GetBitmap("sdk/missing_icon", wxART_TOOLBAR);

    wxString finalName(name);
#if wxCHECK_VERSION(3, 1, 6)
    if (finalName.Replace("22x22", "svg"))
    {
        finalName.Replace(".png", ".svg");
        bitmap = cbLoadBitmapBundleFromSVG(finalName, size, &GetCurFileSystem()).GetBitmap(wxDefaultSize);
        // Fallback
        if (!bitmap.Ok() && name.EndsWith(".png"))
        {
            finalName = name;
            if (finalName.Replace("22x22", m_PathReplaceString))
                bitmap = cbLoadBitmap(finalName, wxBITMAP_TYPE_PNG, &GetCurFileSystem());
        }
    }
    else
        bitmap = cbLoadBitmap(finalName, wxBITMAP_TYPE_PNG, &GetCurFileSystem());
#else
    finalName.Replace("22x22", m_PathReplaceString);
    bitmap = cbLoadBitmap(finalName, wxBITMAP_TYPE_PNG, &GetCurFileSystem());
#endif

    if (!bitmap.Ok())
    {
        Manager::Get()->GetLogManager()->LogError(wxString::Format("(%s) Failed to load image: '%s'", m_CurrentID, finalName));
        return wxArtProvider::GetBitmap("sdk/missing_icon", wxART_TOOLBAR);
    }

    if (size == bitmap.GetSize())
        return bitmap;

    const int bw = bitmap.GetWidth();
    const int bh = bitmap.GetHeight();
    const int w  = size.GetWidth();
    const int h  = size.GetHeight();

    const wxString msg = wxString::Format("(%s): Image \"%s\" with size [%dx%d] doesn't match "
                                          "requested size [%dx%d], resizing!",
                                          m_CurrentID, finalName, bw, bh, w, h);

    Manager::Get()->GetLogManager()->LogWarning(msg);

    wxImage image = bitmap.ConvertToImage();

    const int x = (w - bw) / 2;
    const int y = (h - bh) / 2;

    // If the image is bigger than the current size our code for resizing would do overflow the
    // buffers. Until the code is made to handle such cases just rescale the image.
    if (w < bw || h < bh)
        image.Rescale(w, h, wxIMAGE_QUALITY_HIGH);
    else if (image.HasAlpha()) // Resize doesn't handle Alpha... :-(
    {
        const unsigned char *data = image.GetData();
        const unsigned char *alpha = image.GetAlpha();
        unsigned char *rgb = (unsigned char *) calloc(w * h, 3);
        unsigned char *a = (unsigned char *) calloc(w * h, 1);

        // copy Data/Alpha from smaller bitmap to larger bitmap
        for (int row = 0; row < bh; ++row)
        {
            memcpy(rgb + ((row + y) * w + x) * 3, data + (row * bw) * 3, bw * 3);
            memcpy(a + ((row + y) * w + x), alpha + (row * bw), bw);
        }

        image = wxImage(w, h, rgb, a);
    }
    else
        image.Resize(size, wxPoint(x,y));

    return wxBitmap(image);
}

wxObject* wxToolBarAddOnXmlHandler::DoCreateResource()
{
    wxToolBar* toolbar = nullptr;
    if (m_class == "tool")
    {
        wxCHECK_MSG(m_toolbar, nullptr, _("Incorrect syntax of XRC resource: tool not within a toolbar!"));

        wxSize bitmapSize = wxSize(m_ImageSize, m_ImageSize);
#ifndef __WXMSW__
        const double scaleFactor = cbGetContentScaleFactor(*m_toolbar);
        bitmapSize.Scale(1.0/scaleFactor, 1.0/scaleFactor);
#endif

        if (GetPosition() != wxDefaultPosition)
        {
            m_toolbar->AddTool(GetID(),
                               wxEmptyString,
                               GetCenteredBitmap("bitmap",  bitmapSize),
                               GetCenteredBitmap("bitmap2", bitmapSize),
                               wxITEM_NORMAL,
                               GetText("tooltip"),
                               GetText("longhelp"));
        }
        else
        {
            wxItemKind kind = wxITEM_NORMAL;
            if (GetBool("radio"))
                kind = wxITEM_RADIO;

            if (GetBool("toggle"))
            {
                wxASSERT_MSG(kind == wxITEM_NORMAL, "Can't have both toggleable and radion button at once");
                kind = wxITEM_CHECK;
            }

            m_toolbar->AddTool(GetID(),
                               GetText("label"),
                               GetCenteredBitmap("bitmap",  bitmapSize),
                               GetCenteredBitmap("bitmap2", bitmapSize),
                               kind,
                               GetText("tooltip"),
                               GetText("longhelp"));
        }

        if (GetBool("disabled"))
            m_toolbar->EnableTool(GetID(), false);

        return m_toolbar; // must return non-nullptr
    }

    else if (m_class == "separator")
    {
        wxCHECK_MSG(m_toolbar, nullptr, "Incorrect syntax of XRC resource: separator not within a toolbar!");
        m_toolbar->AddSeparator();
        return m_toolbar; // must return non-nullptr
    }
    else /*<object class="wxToolBar">*/
    {
        m_isAddon = (m_class == "wxToolBarAddOn");
        if (m_isAddon)
        {   // special case: Only add items to toolbar
            toolbar = (wxToolBar*)m_instance;
            // XRC_MAKE_INSTANCE(toolbar, wxToolBar);
        }
        else
        {
            int style = GetStyle("style", wxNO_BORDER | wxTB_HORIZONTAL);
            #ifdef __WXMSW__
            // Force style
            style |= wxNO_BORDER;
            #endif

            // XRC_MAKE_INSTANCE(toolbar, wxToolBar)
            if (m_instance)
                toolbar = wxStaticCast(m_instance, wxToolBar);

            if (!toolbar)
                toolbar = new wxToolBar;

            toolbar->Create(m_parentAsWindow,
                            GetID(),
                            GetPosition(),
                            GetSize(),
                            style,
                            GetName());

            const wxSize margins(GetSize("margins"));
            if (!(margins == wxDefaultSize))
                toolbar->SetMargins(margins.x, margins.y);

            const long packing = GetLong("packing", -1);
            if (packing != -1)
                toolbar->SetToolPacking(packing);

            const long separation = GetLong("separation", -1);
            if (separation != -1)
                toolbar->SetToolSeparation(separation);
        }

        wxXmlNode *children_node = GetParamNode("object");
        if (!children_node)
           children_node = GetParamNode("object_ref");

        if (children_node == nullptr)
            return toolbar;

        m_isInside = true;
        m_toolbar = toolbar;
#if wxCHECK_VERSION(3, 1, 6)
        wxSize toolSize(m_ImageSize, m_ImageSize);
        // Hack for scale factors less than 1.75 not scaling at all
        const double scaleFactor = cbGetContentScaleFactor(*m_toolbar);
        if ((scaleFactor > 1.00) && (scaleFactor < 1.75))
            toolSize.Scale(scaleFactor, scaleFactor);

        m_toolbar->SetToolBitmapSize(toolSize);
#endif

        wxXmlNode* n = children_node;
        while (n)
        {
            if ((n->GetType() == wxXML_ELEMENT_NODE) &&
                (n->GetName() == "object" || n->GetName() == "object_ref"))
            {
                wxObject* created = CreateResFromNode(n, toolbar, nullptr);
                wxControl* control = wxDynamicCast(created, wxControl);
                if (!IsOfClass(n, "tool") &&
                    !IsOfClass(n, "separator") &&
                    control != nullptr &&
                    control != toolbar)
                {
                    //Manager::Get()->GetLogManager()->DebugLog(wxString::Format("control=%p, parent=%p, toolbar=%p", control, control->GetParent(), toolbar));
                    toolbar->AddControl(control);
                }
            }

            n = n->GetNext();
        }

        toolbar->Realize();

        m_isInside = false;
        m_toolbar = nullptr;

        if (!m_isAddon)
        {
            if (m_parentAsWindow && !GetBool("dontattachtoframe"))
            {
                wxFrame* parentFrame = wxDynamicCast(m_parent, wxFrame);
                if (parentFrame)
                    parentFrame->SetToolBar(toolbar);
            }
        }

        m_isAddon = false;
        return toolbar;
    }
}

bool wxToolBarAddOnXmlHandler::CanHandle(wxXmlNode *node)
{
// NOTE (mandrav#1#): wxXmlResourceHandler::IsOfClass() doesn't work in unicode (2.6.2)
// Don't ask why. It does this and doesn't work for our custom handler:
//    return node->GetPropVal(wxT("class"), wxEmptyString) == classname;
//
// This works though:
//    return node->GetPropVal(wxT("class"), wxEmptyString).Matches(classname);
//
// Don't ask me why... >:-|

    const bool istbar = node->GetAttribute("class", wxEmptyString).Matches("wxToolBarAddOn");
    const bool istool = node->GetAttribute("class", wxEmptyString).Matches("tool");
    const bool issep  = node->GetAttribute("class", wxEmptyString).Matches("separator");

    return ((!m_isInside && istbar) ||
            (m_isInside && istool) ||
            (m_isInside && issep));
}

IMPLEMENT_DYNAMIC_CLASS(wxScrollingDialogXmlHandler, wxDialogXmlHandler)

wxScrollingDialogXmlHandler::wxScrollingDialogXmlHandler() : wxDialogXmlHandler()
{
}

wxObject* wxScrollingDialogXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(dlg, wxScrollingDialog);

    dlg->Create(m_parentAsWindow,
                GetID(),
                GetText("title"),
                wxDefaultPosition, wxDefaultSize,
                GetStyle("style", wxDEFAULT_DIALOG_STYLE),
                GetName());

    if (HasParam("size"))
        dlg->SetClientSize(GetSize("size", dlg));

    if (HasParam("pos"))
        dlg->Move(GetPosition());

    if (HasParam("icon"))
        dlg->SetIcon(GetIcon("icon", wxART_FRAME_ICON));

    SetupWindow(dlg);

    CreateChildren(dlg);

    if (GetBool("centered", false))
        dlg->Centre();

    return dlg;
}

bool wxScrollingDialogXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, "wxScrollingDialog");
}
