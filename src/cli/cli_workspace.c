#include "cli_internal.h"

// One static workspace avoids repeated large stack allocations
// Single definition lives here so all CLI modules share one consistent store
plainsight_workspace g_cli_workspace;

void plainsight_cli_workspace_init(void) {
    // Bind pixel storage once so all image backends share one consistent output target
    // This keeps large pixel buffers out of the stack and avoids accidental copies
    (void)plainsight_image_bind_storage(&g_cli_workspace.image,
                                g_cli_workspace.image_pixels,
                                sizeof(g_cli_workspace.image_pixels));

    // Clear decoded metadata so stale dimensions cannot leak into a new operation
    g_cli_workspace.image.width = 0u;
    g_cli_workspace.image.height = 0u;
    g_cli_workspace.image.channels = 0u;
    g_cli_workspace.image.data_len = 0u;
}
