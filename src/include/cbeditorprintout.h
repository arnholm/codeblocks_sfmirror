/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef CBEDITORPRINTOUT_H
#define CBEDITORPRINTOUT_H

#include <vector>
#include <wx/print.h>

struct PageInfo
  {
  int ctrlIndex;
  int pageStart;
  int selEnd;
  };

class cbStyledTextCtrl;

class cbEditorPrintout : public wxPrintout
{
    public:
        cbEditorPrintout(const wxString& title, cbStyledTextCtrl* control, bool selectionOnly);
        ~cbEditorPrintout() override;
        bool OnPrintPage(int page) override;
        bool HasPage(int page) override;
        void GetPageInfo(int* minPage, int* maxPage, int* selPageFrom, int* selPageTo) override;
        bool OnBeginDocument(int startPage, int endPage) override;
        void AddControl(cbStyledTextCtrl* control);

    protected:
        void ScaleDC(wxDC *dc);

        bool m_selection;
        wxRect m_pageRect;
        wxRect m_printRect;
        std::vector <cbStyledTextCtrl*> m_controls;
        std::vector <PageInfo> m_pages;
};

#endif // CBEDITORPRINTOUT_H
