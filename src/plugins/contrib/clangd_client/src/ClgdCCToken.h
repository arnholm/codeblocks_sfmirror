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
    // note that a semanticToken comes from a textDocument/semanticTokens response, not a textDocument/Completion response
    // and indexes into the m_SemanticTokensVec array;
    int semanticTokenID = -1;
    // Semantic token type converted from a clangd completion label kind by ConvertLSPCompletionSymbolKindToSemanticTokenType(labelKind);
    int semanticTokenType = -1;
};

#endif //CLGDCCTOKEN_H
