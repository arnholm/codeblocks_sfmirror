/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef SCRIPTSECURITYWARNINGDLG_H
#define SCRIPTSECURITYWARNINGDLG_H

#include "scrollingdialog.h"

enum ScriptSecurityResponse
{
    ssrAllow = 0,
    ssrAllowAll,
    ssrDeny,
    ssrTrust,
    ssrTrustPermanently
};

class ScriptSecurityWarningDlg : public wxScrollingDialog
{
    public:
        /** \brief Constructor for ScriptSecurityWarningDlg to ask user for script security permit
         *
         * \param parent wxWindow* Parent window
         * \param operation const wxString& operation to permit
         * \param command const wxString& some description
         * \param hasPath bool  true  if the operation is called from a script file, so we can ask the user if he wants to allow this file in future calls
         *                      false it here is no script file and we ask the user only to allow this specific call
         *
         */
        ScriptSecurityWarningDlg(wxWindow* parent, const wxString& operation, const wxString& command, bool hasPath);
        ~ScriptSecurityWarningDlg() override;

        ScriptSecurityResponse GetResponse();
        void EndModal(int retCode) override;
    protected:
    private:
        bool m_hasPath;
};



#endif // SCRIPTSECURITYWARNINGDLG_H
