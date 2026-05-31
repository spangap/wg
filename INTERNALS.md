# wg — internals

## IDF 5.x `netif->state` workaround

The IDF 5.x lwIP layer rearranged how `netif->state` is owned, which
broke upstream `esp_wireguard`'s assumption that it could stash the
WG context there. The vendored fork drops the WG context into a
sidecar map keyed by netif pointer instead.

Required sdkconfig: `CONFIG_LWIP_PPP_SUPPORT=y`. PPP is not used; the
flag forces lwIP to allocate the netif state slot in a way that
matches what the WG netif driver assumes.

This vendor drops out as soon as upstream propagates the fix.

## Lifecycle

`wg.cpp` is one task that:

1. Subscribes to `NET_EV_UPSTREAM_UP` / `DOWN`.
2. On UP, calls `esp_wireguard_init(&ctx, &cfg)` with the keys + peer
   from storage, then `esp_wireguard_connect(&ctx)`.
3. Runs a keepalive timer; surfaces last-handshake / rx/tx counters as
   ephemeral keys.
4. On DOWN, calls `esp_wireguard_disconnect`.

## Routing

`s.wg.allowed_ips` is fed straight into the WG peer's allowed-IPs.
lwIP picks the WG netif via standard longest-prefix match — set this
to `0.0.0.0/0` to route all traffic through the tunnel, or to a CIDR
covering your management network to use it only for management.

## Crypto

`esp_wireguard/crypto.c` includes a software Curve25519 (MIT, Mike
Hamburg / Cryptography Research). [rns](../rns)
re-vendors the same Curve25519 implementation into its microreticulum
fork so RNS opportunistic SINGLE packets can do X25519 ECDH at sub-10 ms
per scalar mult instead of ~100 ms on the bare mbedTLS path.
