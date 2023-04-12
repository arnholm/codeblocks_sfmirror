#include "sdk_precomp.h"

#include "cbart_provider.h"

#ifndef CB_PRECOMP
    #include "globals.h"
    #include "logmanager.h"
#endif // CB_PRECOMP

cbArtProvider::cbArtProvider(const wxString& prefix)
{
    m_prefix = prefix;
    if (!prefix.EndsWith('/'))
        m_prefix += '/';
}

void cbArtProvider::AddMapping(const wxString& stockId, const wxString& fileName)
{
    m_idToPath[stockId] = Data(fileName, false);
}

void cbArtProvider::AddMappingF(const wxString& stockId, const wxString& fileName)
{
    m_idToPath[stockId] = Data(fileName, true);
}

wxString cbArtProvider::GetFileName(const wxArtID& id, const wxSize& size) const
{
    if (size == wxDefaultSize)
        return wxEmptyString;

    const MapStockIdToPath::const_iterator it = m_idToPath.find(id);
    if (it == m_idToPath.end())
        return wxEmptyString;

    wxString fileName;
    if (it->second.hasFormatting)
        fileName = it->second.path;
    else
        fileName = "%dx%d/"+it->second.path;

    if (fileName.EndsWith(".svg"))
        fileName.Replace("%dx%d", "svg");
    else
        fileName = wxString::Format(fileName, size.GetWidth(), size.GetHeight());

    return m_prefix+fileName;
}

double cbArtProvider::GetScaleFactor(const wxArtClient& client) const
{
    double scaleFactor = 1.0;

    if (client == wxART_MENU)
        scaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::Menus);
    else if (client == wxART_BUTTON)
        scaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::Main);
    else if (client == wxART_TOOLBAR)
        scaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::Toolbars);

    return scaleFactor;
}

wxSize cbArtProvider::GetSize(const wxArtClient& client) const
{
    int size = 16;

    if (client == wxART_MENU)
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Menus);
    else if (client == wxART_BUTTON)
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Main);
    else if (client == wxART_TOOLBAR)
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Toolbars);

    return wxSize(size, size);
}

wxSize cbArtProvider::DoGetSizeHint(const wxArtClient& client)
{
    const double scaleFactor = GetScaleFactor(client);
    return GetSize(client).Scale(1.0/scaleFactor, 1.0/scaleFactor);
}

wxBitmap cbArtProvider::ReadBitmap(const wxArtID& id, const wxSize& defaultSize, const wxSize& requiredSize) const
{
    const wxString filePath(GetFileName(id, requiredSize));
    if (filePath.empty())
        return wxNullBitmap;

    wxBitmap result;
#if wxCHECK_VERSION(3, 1, 6)
    if (filePath.EndsWith(".svg"))
        result = cbLoadBitmapBundleFromSVG(filePath, defaultSize).GetBitmap(requiredSize);
    else
        result = cbLoadBitmap(filePath);
#else
    result = cbLoadBitmap(filePath);
#endif

    if (!result.IsOk())
    {
        const wxString msg = wxString::Format("cbArtProvider: Cannot load image '%s'", filePath);
        Manager::Get()->GetLogManager()->LogError(msg);
    }

    return result;
}

wxBitmap cbArtProvider::CreateBitmap(const wxArtID& id, const wxArtClient& client, cb_unused const wxSize& size)
{
    const wxSize defaultSize(DoGetSizeHint(client));
    const wxSize requiredSize(GetSize(client));
    return ReadBitmap(id, defaultSize, requiredSize);
}

#if wxCHECK_VERSION(3, 1, 6)
wxBitmapBundle cbArtProvider::CreateBitmapBundle(const wxArtID& id, const wxArtClient& client, cb_unused const wxSize &size)
{
    wxBitmapBundle result;

    const wxSize defaultSize(DoGetSizeHint(client));
    const wxString filePath(GetFileName(id, defaultSize));
    if (filePath.empty())
        return wxBitmapBundle();

    if (filePath.EndsWith(".svg"))
    {
        result = cbLoadBitmapBundleFromSVG(filePath, defaultSize);
    }
    else
    {
        static const int imageSize[] = {16, 20, 24, 28, 32, 40, 48, 56, 64};
        wxVector <wxBitmap> bitmaps;

        for (const int sz : imageSize)
        {
            // Do not load bitmaps smaller than needed
            if (sz < defaultSize.GetWidth())
                continue;

            const wxSize requiredSize(sz, sz);
            const wxBitmap bmp(ReadBitmap(id, defaultSize, requiredSize));
            if (bmp.IsOk())
                bitmaps.push_back(bmp);
        }

        if (!bitmaps.empty())
            result = wxBitmapBundle::FromBitmaps(bitmaps);
    }

    if (!result.IsOk())
    {
        const wxString msg = wxString::Format("cbArtProvider: Cannot load image bundle '%s'", filePath);
        Manager::Get()->GetLogManager()->LogError(msg);
    }

    return result;
}
#endif
