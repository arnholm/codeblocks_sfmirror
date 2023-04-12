#ifndef CODEBLOCKS_ART_PROVIDER_H
#define CODEBLOCKS_ART_PROVIDER_H

#include <wx/artprov.h>
#include <unordered_map>
#include "wxstringhash.h"

#ifndef CB_PRECOMP
    #include "manager.h"
#endif // CB_PRECOMP

/// Custom art provider used to make menu item and bitmap button icons look good on HiDPI displays.
/// The size is set at the beginning of execution and cannot change. See Manager::GetImageSize. The
/// requested size is ignored. Use <bitmap stock_id="XXX/YYY"/> in the xrc file to load images from
/// this provider. Plugins can create their own providers (set the XXX part of the resource id to
/// something meaningful and unique for the plugin).
class DLLIMPORT cbArtProvider : public wxArtProvider
{
public:
    /// @param prefix Path where to search for images. It must contain folders of the form NNxNN.
    cbArtProvider(const wxString& prefix);

    /// Map a stockId to a path inside the prefix.
    void AddMapping(const wxString& stockId, const wxString& fileName);

    /// Map a stockId to a path inside the prefix. The fileName is expected to contain two integer
    /// printf formatting flags (%d). If you fail to provide them the behaviour is undefined!
    /// Example fileName could look like this: "some-secondary-prefix/%dx%d/filename.png".
    void AddMappingF(const wxString& stockId, const wxString& fileName);

protected:
    wxBitmap CreateBitmap(const wxArtID& id, const wxArtClient& client, const wxSize& size) override;
#if wxCHECK_VERSION(3, 1, 6)
    wxBitmapBundle CreateBitmapBundle(const wxArtID& id, const wxArtClient& client, const wxSize& size) override;
#endif
    /// Return the native (i.e. unescaled) size of the client
    wxSize DoGetSizeHint(const wxArtClient& client) override;

private:
    wxString GetFileName(const wxArtID& id, const wxSize& size) const;
    double GetScaleFactor(const wxArtClient& client) const;
    wxSize GetSize(const wxArtClient& client) const;
    wxBitmap ReadBitmap(const wxArtID& id, const wxSize& defaultSize, const wxSize& requiredSize) const;
    wxString m_prefix;

    struct Data
    {
        Data() : hasFormatting(false) {}
        Data(const wxString &path, bool hasFormatting) : path(path), hasFormatting(hasFormatting) {}

        wxString path;
        bool hasFormatting;
    };

    typedef std::unordered_map<wxString, Data> MapStockIdToPath;

    MapStockIdToPath m_idToPath;
};
#endif // CODEBLOCKS_ART_PROVIDER_H
