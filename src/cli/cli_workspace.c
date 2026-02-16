#include "cli_internal.h"

// One static workspace avoids repeated large stack allocations
// Single definition lives here so all CLI modules share one consistent store
plainsight_workspace g_cli_workspace;
