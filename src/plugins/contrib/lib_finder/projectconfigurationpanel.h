/*
* This file is part of lib_finder plugin for Code::Blocks Studio
* Copyright (C) 2007  Bartlomiej Swiecki
*
* wxSmith is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* wxSmith is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with wxSmith; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*
* $Revision$
* $Id$
* $HeadURL$
*/

#ifndef PROJECTCONFIGURATIONPANEL_H
#define PROJECTCONFIGURATIONPANEL_H

//(*Headers(ProjectConfigurationPanel)
#include <wx/panel.h>
#include <wx/timer.h>
class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxListBox;
class wxStaticBoxSizer;
class wxStaticText;
class wxTextCtrl;
class wxTreeCtrl;
class wxTreeEvent;
//*)

#include <configurationpanel.h>
#include <wx/treectrl.h>
#include "projectconfiguration.h"
#include "resultmap.h"


class ProjectConfigurationPanel: public cbConfigurationPanel
{
	public:

		ProjectConfigurationPanel(wxWindow* parent,ProjectConfiguration* Config,cbProject* Proj,TypedResults& KnownLibs);
		virtual ~ProjectConfigurationPanel();

	private:

        WX_DECLARE_STRING_HASH_MAP(wxTreeItemId,IdsMap);

        virtual wxString GetTitle() const;
        virtual wxString GetBitmapBaseName() const;
        virtual void OnApply();
        virtual void OnCancel();

        void LoadData();
        void StoreData();

        void FillKnownLibraries();

        void BuildEntry(const wxTreeItemId& Id,ResultArray& Array);
        wxTreeItemId OtherCategoryId();
        wxTreeItemId PkgConfigId();
        wxTreeItemId CategoryId(const wxString& Category);

        ProjectConfiguration* m_Configuration;
        ProjectConfiguration  m_ConfCopy;
        cbProject* m_Project;
        TypedResults& m_KnownLibs;
        IdsMap m_CategoryMap;
        bool m_IsOtherCategory;
        bool m_IsPkgConfig;

		//(*Declarations(ProjectConfigurationPanel)
		wxButton* Button1;
		wxButton* Button2;
		wxButton* m_Add;
		wxButton* m_AddScript;
		wxButton* m_AddUnknown;
		wxButton* m_Remove;
		wxCheckBox* m_NoAuto;
		wxCheckBox* m_Tree;
		wxListBox* m_UsedLibraries;
		wxStaticText* StaticText1;
		wxStaticText* m_EventText;
		wxTextCtrl* m_Filter;
		wxTextCtrl* m_UnknownLibrary;
		wxTimer Timer1;
		wxTreeCtrl* m_KnownLibrariesTree;
		//*)

		//(*Identifiers(ProjectConfigurationPanel)
		static const wxWindowID ID_LISTBOX1;
		static const wxWindowID ID_BUTTON6;
		static const wxWindowID ID_CHECKBOX2;
		static const wxWindowID ID_BUTTON4;
		static const wxWindowID ID_BUTTON1;
		static const wxWindowID ID_BUTTON2;
		static const wxWindowID ID_TREECTRL1;
		static const wxWindowID ID_STATICTEXT1;
		static const wxWindowID ID_TEXTCTRL2;
		static const wxWindowID ID_CHECKBOX1;
		static const wxWindowID ID_BUTTON5;
		static const wxWindowID ID_TEXTCTRL1;
		static const wxWindowID ID_BUTTON3;
		static const wxWindowID ID_STATICTEXT2;
		static const wxWindowID ID_TIMER1;
		//*)

		//(*Handlers(ProjectConfigurationPanel)
		void Onm_TreeClick(wxCommandEvent& event);
		void OnTimer1Trigger(wxTimerEvent& event);
		void Onm_FilterText(wxCommandEvent& event);
		void Onm_FilterTextEnter(wxCommandEvent& event);
		void Onm_KnownLibrariesTreeSelectionChanged(wxTreeEvent& event);
		void Onm_UsedLibrariesSelect(wxCommandEvent& event);
		void Onm_RemoveClick(wxCommandEvent& event);
		void Onm_AddClick(wxCommandEvent& event);
		void Onm_UnknownLibraryText(wxCommandEvent& event);
		void Onm_AddUnknownClick(wxCommandEvent& event);
		void Onm_AddScriptClick(wxCommandEvent& event);
		void OnButton2Click(wxCommandEvent& event);
		//*)

		wxString GetUserListName(const wxString &VarName);
		void DetectNewLibs( const wxString& IncludeName, ResultArray& known, wxArrayString& LibsList );

		DECLARE_EVENT_TABLE()
};

#endif
