#ifdef __GNUG__
// #pragma interface
#endif

#ifndef __NASSI_FILE_CONTENT__
#define __NASSI_FILE_CONTENT__

#include "FileContent.h"


class NassiBrick;
class NassiFileContent : public FileContent
{
    public:
        NassiFileContent();
        virtual ~NassiFileContent();
    public:
        wxOutputStream& SaveObject(wxOutputStream& stream) override;
        wxInputStream& LoadObject(wxInputStream& stream) override;

        wxString GetWildcard() override;

    public:
        NassiBrick *GetFirstBrick();
        NassiBrick *SetFirstBrick(NassiBrick *brick);
    private:
        NassiFileContent(const NassiFileContent &p);
        NassiFileContent &operator=(const NassiFileContent &rhs);
        NassiBrick *m_firstbrick;
};
#endif
