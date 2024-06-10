#ifndef HELPERS_H_INCLUDED
#define HELPERS_H_INCLUDED

#include "configmanager.h"
#include "annoyingdialog.h"

#define UNUSED_VAR(x) (void)(x)

namespace Helpers {

    static int currentMaxEntries = 0;
    static bool isShownNeedsRestart = false;

    // Suppress unused function warning from clangd
    static int GetMaxEntries() __attribute__((unused));

    // ----------------------------------------------------------------------------
    static int GetMaxEntries()
    // ----------------------------------------------------------------------------
    {
            UNUSED_VAR(currentMaxEntries);
            UNUSED_VAR(isShownNeedsRestart);

        // This function is also driven by OnUpdateUI(), so heavy overhead.
        int maxEntries = Manager::Get()->GetConfigManager("BrowseTracker")->
            ReadInt("JumpViewRowCount", 20); //(ph 2024/06/01)

        // Remember the first settings
        if (not currentMaxEntries) currentMaxEntries = maxEntries;

        if (maxEntries != currentMaxEntries) //maxEntries has been changed by user
        {
            // this code keep showing twice; Is deprecated until I find why.
            //    if (not isShownNeedsRestart)
            //    {
            //        // Can't change the entries without a restart, else crashes;
            //        isShownNeedsRestart = true;
            //
            //        wxString message = _("CodeBlocks needs to be *restarted*\nto honor the Browsetracker entry count change!");
            //        AnnoyingDialog dlg(_("Restart needed."),
            //                                   message,
            //                                   wxART_INFORMATION,
            //                                   AnnoyingDialog::OK);
            //        dlg.ShowModal();
            //    }
            return currentMaxEntries; //return the original count
        }

        return maxEntries;
    }

}


#endif // HELPERS_H_INCLUDED
