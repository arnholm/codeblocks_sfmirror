#ifndef TEXTCTRLTASK_H
#define TEXTCTRLTASK_H

// For compilers that support precompilation, includes <wx/wx.h>
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "Task.h"
//#include "NassiView.h"

class NassiView;
class NassiFileContent;
class TextGraph;
class TextCtrl;

class TextCtrlTask : public Task
{
    public:
        TextCtrlTask(NassiView *view, NassiFileContent *nfc, TextCtrl *textctrl, TextGraph *textgraph, const wxPoint &pos);
        virtual ~TextCtrlTask();

        wxCursor Start() override;

        // events from window:
		void OnMouseLeftDown(wxMouseEvent &event, const wxPoint &position) override;
        void OnMouseRightDown(wxMouseEvent &event, const wxPoint &position) override;

        void OnMouseRightUp(wxMouseEvent& event, const wxPoint &position) override;
        void OnMouseLeftUp(wxMouseEvent &event, const wxPoint &position) override;
        HooverDrawlet *OnMouseMove(wxMouseEvent &event, const wxPoint &position) override;
        void OnKeyDown(wxKeyEvent &event) override;
        void OnChar(wxKeyEvent &event) override;

        bool Done() const override {return m_done;}

        // events from frame(s)
        bool CanEdit() const override { return true; }
        //bool CanCopy() const override;
        //bool CanCut() const override;
        bool CanPaste() const override;
        bool HasSelection() const override;
        void DeleteSelection() override;
        void Copy() override;
        void Cut() override;
        void Paste() override;

        void UpdateSize() override;
    protected:
        bool m_done;
        TextCtrl *m_textctrl;
        NassiView *m_view;
        NassiFileContent *m_nfc;
        TextGraph *m_textgraph;
    private:
        TextCtrlTask(const TextCtrlTask &p);
        TextCtrlTask &operator=(const TextCtrlTask &rhs);
    private:
        class EditPosition
        {
            public:
                wxUint32 line;
                wxUint32 column;
                bool operator==(const EditPosition &a)const {return (a.line == this->line) && (a.column == this->column);}
                bool operator!=(const EditPosition &a)const {return !(a == (*this));}
        };
        void CloseTask();
        EditPosition GetEditPosition(const wxPoint &pos);
    public:
        void UnlinkTextGraph();
};

#endif // TEXTCTRLTASK_H
