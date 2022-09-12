#include "sdk_precomp.h"

#include "cbart_provider.h"

#ifndef CB_PRECOMP
    #include "globals.h"
    #include "logmanager.h"
#endif // CB_PRECOMP

cbArtProvider::cbArtProvider(const wxString &prefix)
{
    m_prefix = prefix;
    if (!prefix.EndsWith(wxT("/")))
        m_prefix += wxT("/");
}

void cbArtProvider::AddMapping(const wxString &stockId, const wxString &fileName)
{
    m_idToPath[stockId] = Data(fileName, false);
}

void cbArtProvider::AddMappingF(const wxString &stockId, const wxString &fileName)
{
    m_idToPath[stockId] = Data(fileName, true);
}

wxBitmap cbArtProvider::DoCreateBitmap(const wxArtID& id, Manager::UIComponent uiComponent) const
{
    const MapStockIdToPath::const_iterator it = m_idToPath.find(id);
    if (it == m_idToPath.end())
        return wxNullBitmap;

    const int size = Manager::Get()->GetImageSize(uiComponent);
    const double uiScale = Manager::Get()->GetUIScaleFactor(uiComponent);

    wxString filepath = m_prefix;
    if (!it->second.hasFormatting)
        filepath += wxString::Format("%dx%d/%s", size, size, it->second.path);
    else
        filepath += wxString::Format(it->second.path, size, size);

    wxBitmap result = cbLoadBitmapScaled(filepath, wxBITMAP_TYPE_PNG, uiScale);
    if (!result.IsOk())
    {
        const wxString msg = wxString::Format("cbArtProvider: Cannot load image '%s'", filepath);
        Manager::Get()->GetLogManager()->LogError(msg);
    }

    return result;
}

wxBitmap cbArtProvider::CreateBitmap(const wxArtID& id, const wxArtClient& client,
                                     cb_unused const wxSize &size)
{
    if (client == wxT("wxART_MENU_C"))
        return DoCreateBitmap(id, Manager::UIComponent::Menus);
    else if (client == wxT("wxART_BUTTON_C"))
        return DoCreateBitmap(id, Manager::UIComponent::Main);
    else if (client == wxT("wxART_TOOLBAR_C"))
        return DoCreateBitmap(id, Manager::UIComponent::Toolbars);

    return wxNullBitmap;
}

#if wxCHECK_VERSION(3, 1, 6)
wxBitmapBundle cbArtProvider::CreateBitmapBundle(const wxArtID& id, const wxArtClient& client,
                                                 cb_unused const wxSize &size)
{
    static const int imageSize[] = {16, 20, 24, 28, 32, 40, 48, 56, 64};

    const MapStockIdToPath::const_iterator it = m_idToPath.find(id);
    if (it == m_idToPath.end())
        return wxBitmapBundle();

    Manager::UIComponent component;
    if (client == wxART_MENU)
        component = Manager::UIComponent::Menus;
    else if (client == wxART_BUTTON)
        component = Manager::UIComponent::Main;
    else if (client == wxART_TOOLBAR)
        component = Manager::UIComponent::Toolbars;
    else
        return wxBitmapBundle();

    // Get component size undoing scaling
    const double scaleFactor = Manager::Get()->GetUIScaleFactor(component);
    const int componentSize = wxRound(Manager::Get()->GetImageSize(component)/scaleFactor);

    wxVector <wxBitmap> bitmaps;
    for (const int sz : imageSize)
    {
        // Do not load bitmaps smaller than needed
        if (sz < componentSize)
            continue;

        wxString fileName;
        if (!it->second.hasFormatting)
            fileName.Printf("%dx%d/%s", sz, sz, it->second.path);
        else
            fileName.Printf(it->second.path, sz, sz);

        const wxBitmap bmp(cbLoadBitmap(m_prefix+fileName, wxBITMAP_TYPE_PNG));
        if (bmp.IsOk())
            bitmaps.push_back(bmp);
    }

    if (bitmaps.empty())
    {
        const wxString msg = wxString::Format("cbArtProvider: Cannot load image bundle '%s'", it->second.path);
        Manager::Get()->GetLogManager()->LogError(msg);
    }

    return wxBitmapBundle::FromBitmaps(bitmaps);
}
#endif
