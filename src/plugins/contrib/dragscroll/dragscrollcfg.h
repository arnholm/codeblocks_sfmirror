#ifndef DragScrollCfg_H
#define DragScrollCfg_H

#include "dragscroll.h"

//(*Headers(cbDragScrollCfg)
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/intl.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include "scrollingdialog.h"
//*)

#include "configurationpanel.h"
#include <wx/settings.h>

// ----------------------------------------------------------------------------
class cbDragScrollCfg: public cbConfigurationPanel
// ----------------------------------------------------------------------------
{

	public:
		cbDragScrollCfg(wxWindow* parent, cbDragScroll* pOwnerClass, wxWindowID id = -1);
		virtual ~cbDragScrollCfg();
    public:
        // virtual routines required by cbConfigurationPanel
        wxString GetTitle() const { return _("Mouse Drag Scrolling"); }
        wxString GetBitmapBaseName() const;
        void OnApply();
        void OnCancel(){}
        virtual void InitDialog() { } /*trap*/

        // pointer to owner of the configuration diaglog needed to
        // complete the OnApply/OnCancel EndModal() logic
        cbDragScroll* pOwnerClass;


		//Identifiers(cbDragScrollCfg)
		enum Identifiers
		{
		    ID_DONEBUTTON = 0x1000,
		    ID_ENABLEDCHECKBOX,
		    ID_EDITORENABLEDFOCUS,
		    ID_MOUSEENABLEDFOCUS,
		    ID_KEYCHOICE,
		    ID_RADIOBOX1,
		    ID_SENSITIVITY,
		    ID_STATICTEXT1,
		    ID_STATICTEXT2,
		    ID_STATICTEXT3,
		    ID_STATICTEXT4,
		    ID_STATICTEXTMRKC,
		    ID_STATICTEXT5,
		    ID_MOUSECONTEXTDELAY
		};

        bool GetMouseDragScrollEnabled() { return ScrollEnabled->GetValue(); }
        bool GetMouseEditorFocusEnabled(){ return EditorFocusEnabled->GetValue(); }
        bool GetMouseFocusEnabled()      { return MouseFocusEnabled->GetValue(); }
        int  GetMouseDragDirection()     { return ScrollDirection->GetSelection(); }
        int  GetchosenDragKey()          { return MouseKeyChoice->GetSelection(); }
        int  GetMouseDragSensitivity()   { return Sensitivity->GetValue(); }
        bool GetMouseWheelZoom()         { return MouseWheelZoom->GetValue(); }
        bool IsLogZoomSizePropagated()   { return PropagateLogZoomSize->GetValue(); }
        bool GetMouseWheelZoomReverse()  { return MouseWheelZoomReverse->GetValue(); } //2019/03/30

        void SetMouseDragScrollEnabled(bool value)
                { ScrollEnabled->SetValue(value); }
        void SetMouseEditorFocusEnabled(bool value)
                { EditorFocusEnabled->SetValue(value); }
        void SetMouseFocusEnabled(bool value)
                { MouseFocusEnabled->SetValue(value); }
        void SetMouseDragDirection(int selection)
                { ScrollDirection->SetSelection(selection); }
        void SetMouseDragKey(int selection)
                { MouseKeyChoice->SetSelection(selection); }
        void SetMouseDragSensitivity(int value)
                { Sensitivity->SetValue(value); }
        void SetMouseWheelZoom(bool value)
                { MouseWheelZoom->SetValue(value); }
        void SetPropagateLogZoomSize(bool value)
                { PropagateLogZoomSize->SetValue(value); }
        void SetMouseWheelZoomReverse(bool value)
                { MouseWheelZoomReverse->SetValue(value); } //2019/03/30

	protected:

		void OnDoneButtonClick(wxCommandEvent& event);

		//Declarations(cbDragScrollCfg)
		wxFlexGridSizer* FlexGridSizer1;
		wxStaticText* StaticText1;
		wxCheckBox* ScrollEnabled;
		wxCheckBox* EditorFocusEnabled;
		wxCheckBox* MouseFocusEnabled;
		wxCheckBox* MouseWheelZoom;
		wxCheckBox* PropagateLogZoomSize;
		wxRadioBox* ScrollDirection;
		wxStaticText* StaticText2;
		wxChoice* MouseKeyChoice;
		wxCheckBox* MouseWheelZoomReverse;

        // Mouse adjustment sliders
		wxStaticText* StaticText3;
		wxSlider* Sensitivity;
        wxStaticText* StaticText4;
		wxButton* DoneButton;
		wxStaticText* StaticTextMRKC;
		wxStaticText* StaticText5;


	private:

		DECLARE_EVENT_TABLE()
};

#endif // DragScrollCfg_H
//
