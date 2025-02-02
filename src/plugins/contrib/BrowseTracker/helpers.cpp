#include "helpers.h"
#include "manager.h"
#include "configmanager.h"

#include "helpers.h"

#define UNUSED_VAR(x) (void)(x)

// Initialize the static variable
int Helpers::currentMaxEntries = 0;

// ----------------------------------------------------------------------------
namespace Helpers
// ----------------------------------------------------------------------------
{
    int GetMaxAllocEntries()
    {
        // Return max entries found from the first read of Config
        // to avoid overhead of constant config reads.
        if (currentMaxEntries) return currentMaxEntries;

        // This function is also driven by OnUpdateUI(), so heavy overhead because
        // OnUpdateUI() calls GetMarkCount() which calls GetMaxEntries();
        // This conf read will happen only once, after which currentMaxEntries will
        // be returned by the above return statement.
        currentMaxEntries = Manager::Get()->GetConfigManager("BrowseTracker")->
                ReadInt("JumpViewRowCount", 20);

        return currentMaxEntries;
    }
}
