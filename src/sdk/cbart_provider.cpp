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

wxSize cbArtProvider::GetSize(const wxArtClient& client, bool unscaled) const
{
    int size = 0;
    double scaleFactor = 1.0;

    if (client == wxART_MENU)
    {
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Menus);
        scaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::Menus);
    }
    else if (client == wxART_BUTTON)
    {
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Main);
        scaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::Main);
    }
    else if (client == wxART_TOOLBAR)
    {
        size = Manager::Get()->GetImageSize(Manager::UIComponent::Toolbars);
        scaleFactor = Manager::Get()->GetUIScaleFactor(Manager::UIComponent::Toolbars);
    }

    if (!size)
        return wxDefaultSize;

    if (unscaled)
        size = cbFindMinSize16to64(wxRound(size/scaleFactor));

    return wxSize(size, size);
}

wxBitmap cbArtProvider::ReadBitmap(const wxArtID& id, const wxSize& size) const
{
    const wxString filePath(GetFileName(id, size));
    if (filePath.empty())
        return wxNullBitmap;

    wxBitmap result;
#if wxCHECK_VERSION(3, 1, 6)
    if (filePath.EndsWith(".svg"))
        result = cbLoadBitmapBundleFromSVG(filePath, size).GetBitmap(wxDefaultSize);
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
    return ReadBitmap(id, GetSize(client, false));
}

#if wxCHECK_VERSION(3, 1, 6)
wxBitmapBundle cbArtProvider::CreateBitmapBundle(const wxArtID& id, const wxArtClient& client, cb_unused const wxSize &size)
{
    wxBitmapBundle result;

    const wxSize requiredSize(GetSize(client, true));
    const wxString filePath(GetFileName(id, requiredSize));
    if (filePath.empty())
        return wxBitmapBundle();

    if (filePath.EndsWith(".svg"))
    {
        result = cbLoadBitmapBundleFromSVG(filePath, requiredSize);
    }
    else
    {
        static const int imageSize[] = {16, 20, 24, 28, 32, 40, 48, 56, 64};

        wxVector <wxBitmap> bitmaps;
        for (const int sz : imageSize)
        {
            // Do not load bitmaps smaller than needed
            if (sz < requiredSize.GetWidth())
                continue;

            const wxBitmap bmp(ReadBitmap(id, wxSize(sz, sz)));
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
