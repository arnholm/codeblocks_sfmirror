#ifndef CLGDCCTOKEN_H
#define CLGDCCTOKEN_H

#include <cbplugin.h>
// Clangd_client CodeCompletionToken extension containing a CCToken
// ----------------------------------------------------------------------------
struct ClgdCCToken : public cbCodeCompletionPlugin::CCToken
// ----------------------------------------------------------------------------
{
    //  CCToken(int _id, const wxString& dispNm, const wxString& nm, int _weight, int categ = -1) :
    // id(_id), category(categ), weight(_weight), displayName(dispNm), name(nm) {}
   public:
    ClgdCCToken(int ccId, wxString& ccDispNm, wxString& ccNm, int ccWeight=5, int ccCateg=-1)
      : CCToken(ccId, ccDispNm, ccNm, ccWeight, ccCateg)
    {}
    int semanticTokenID = -1;

};

#endif //CLGDCCTOKEN_H
