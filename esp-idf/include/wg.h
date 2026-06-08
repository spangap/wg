/**
 * WireGuard VPN — tunnel lifecycle via net event callbacks.
 * Not a task — runs on net's task context when events fire.
 */
#ifndef SPANGAP_WG_H
#define SPANGAP_WG_H

#include "storage.h"  /* cli_write_fn */

/** Register WG callbacks with net. Call from main after netInit(). */
void wgInit();

/** Returns true if WireGuard tunnel is established. */
bool wgIsUp();

/** Print tunnel status via the given write function. */
void wgStatus(cli_write_fn write);

/** Generate a new private key, save to storage, print public key. */
void wgGenKey(cli_write_fn write);

#endif
