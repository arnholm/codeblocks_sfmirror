/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef CONFIGURATIONPANEL_H
#define CONFIGURATIONPANEL_H

#include "globals.h"
#include "settings.h"
#include "scrollingdialog.h"
#include <wx/panel.h>
#include <wx/string.h>

class wxButton;
class wxWindow;

/// Interface which is passed to the cbPlugin::GetConfigurationPanelEx implementation. It is used by
/// the Environment settings dialog to allow editing colours stored in the colour manager in the
/// configuration panel of a plugin.
class DLLIMPORT cbConfigurationPanelColoursInterface
{
    public:
        virtual ~cbConfigurationPanelColoursInterface() {}

        /// Get the colour value with a given id.
        virtual wxColour GetValue(const wxString &id) = 0;
        /// Set the colour value of a given id.
        virtual void SetValue(const wxString &id, const wxColour &colour) = 0;
        /// Reset the colour value to the default.
        virtual void ResetDefault(const wxString &id) = 0;
};

/** @brief Base class for plugin configuration panels. */
class DLLIMPORT cbConfigurationPanel : public wxPanel
{
    public:
        cbConfigurationPanel() : m_parentDialog(nullptr) { ; }
        ~cbConfigurationPanel() override{}

        /// @return the panel's title.
        virtual wxString GetTitle() const = 0;
        /// @return the panel's bitmap base name. You must supply two bitmaps: \<basename\>.png and \<basename\>-off.png...
        virtual wxString GetBitmapBaseName() const = 0;
        /// Called when the user chooses to apply the configuration.
        virtual void OnApply() = 0;
        /// Called when the user chooses to cancel the configuration.
        virtual void OnCancel() = 0;
        /// Called when the panel is about to be shown. Could be called repeatedly.
        virtual void OnPageChanging() {}

        /// Sets the panel's parent dialog
        void SetParentDialog(wxWindow* dialog)
        {
            m_parentDialog = dialog;
        }
        /// Gets the panel's parent dialog
        wxWindow* SetParentDialog()
        {
            return m_parentDialog;
        }
        /** Call global cbMessageBox with m_parentDialog as parent window when
            no parent window specified */
        int cbMessageBox(const wxString& message, const wxString& caption = wxEmptyString, int style = wxOK, wxWindow *parent = NULL, int x = -1, int y = -1)
        {
            if (parent)
                return ::cbMessageBox(message, caption, style, parent, x, y);
            else
                return ::cbMessageBox(message, caption, style, m_parentDialog, x, y);
        }
    private:
        wxWindow* m_parentDialog;
};

/// @brief A simple dialog that wraps a cbConfigurationPanel.
class DLLIMPORT cbConfigurationDialog : public wxScrollingDialog
{
    public:
        cbConfigurationDialog(wxWindow* parent, int id, const wxString& title);
        void AttachConfigurationPanel(cbConfigurationPanel* panel);
        ~cbConfigurationDialog() override;

        void EndModal(int retCode) override;
    protected:
        cbConfigurationPanel* m_pPanel;
        wxButton* m_pOK;
        wxButton* m_pCancel;
    private:

};

#endif // CONFIGURATIONPANEL_H
