#ifndef CLANGLOCATOR_H
#define CLANGLOCATOR_H

#include <cstddef>
#include <wx/string.h>
#include <wx/filename.h>

class wxArrayString;

class ClangLocator
{
    public:
        ClangLocator();
        virtual ~ClangLocator();

        wxString Locate_ClangdDir();
        wxString Locate_ResourceDir(wxFileName fnClangd);

//-        wxString MSWLocate();
        wxArrayString GetEnvPaths() const;
        std::size_t ScanForFiles(wxString path, wxArrayString& foundFiles, wxString mask);
        bool ReadMSWInstallLocation(const wxString& regkey, wxString& installPath, wxString& llvmVersion);
        wxString GetClangdVersion(const wxString& clangBinary);
        wxString GetClangdVersionID(const wxString& clangdBinary);

        // PATH environment variable separator
        #ifdef __WXMSW__
        #define ENV_PATH_SEPARATOR ";"
        #else
        #define ENV_PATH_SEPARATOR ":"
        #endif

    private:
};

#endif // CLANGLOCATOR_H
