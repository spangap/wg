# wg

## What is this?

**wg** is a [WireGuard](https://www.wireguard.com) tunnel for
[spangap](../spangap) devices. It vendors `trombik/esp_wireguard` (the
de-facto ESP-IDF WireGuard port) and brings the tunnel up against a
configured peer.

Use it as the alternative to the `upnp` + `duckdns` + `acme` remote-
access stack when the device is behind CG-NAT (or the operator just
doesn't want public-internet exposure).

## What this straddle owns

```
wg/
└── esp-idf/
    ├── include/
    │   ├── wg.h               public spangap API (wgInit, wgConnect/wgDisconnect)
    │   └── esp_wireguard.h    upstream public header (re-exported)
    ├── Kconfig
    └── src/
        ├── wg.cpp                 spangap wrapper task: config from storage,
        │                          connect/disconnect/keepalive, status surface
        └── esp_wireguard/         trombik/esp_wireguard vendored
            ├── esp_wireguard.c
            ├── wireguard.{c,h}
            ├── wireguardif.{c,h}
            ├── wireguard-platform.{c,h}
            └── crypto.{c,h}       includes a software Curve25519
                                   (the same X25519 reused by reticulous-core)
```

Browser settings panel: server pubkey, allowed-IPs, endpoint, our
keypair, persistent-keepalive.

## How others use it

```cpp
wgInit();    // after netInit
```

Configuration:

- `s.wg.enable` — on/off
- `s.wg.endpoint` — server `host:port`
- `s.wg.allowed_ips` — comma-separated CIDRs
- `s.wg.peer_pubkey` — server public key
- `secrets.wg.privkey` — our private key (Curve25519; generated on
  first enable if absent)

`wg` brings the tunnel up after `NET_EV_UPSTREAM_UP` and tears it down
on `NET_EV_UPSTREAM_DOWN`.

## Dependencies

- [spangap-net](../spangap-net) — UDP socket + lwIP netif slot.

## Why we vendor `trombik/esp_wireguard`

Upstream `esp_wireguard` has not yet propagated the ESP-IDF 5.x
`netif->state` / PPP workaround (the IDF 5 netif slot rewrite). We
carry the patched version directly until upstream catches up; when it
does, this straddle will drop the vendor and depend on the registry
component.

The software Curve25519 in `esp_wireguard/crypto.c` is also re-used by
[reticulous-core](../../r/reticulous-core)'s microreticulum fork (X25519
ECDH for RNS) — keeping the vendor here means both straddles share one
constant-time impl rather than each carrying their own.

## What it does NOT do

- It is **not** a WireGuard server — only a client.
- It does not negotiate the keypair with the server. You configure
  matching keys on both ends out-of-band.

## Read next

- [INTERNALS.md](INTERNALS.md) — netif glue, the IDF 5.x workaround,
  keepalive policy.
- Platform-wide doc: `spangap-core/docs/wireguard.md` (still in the
  old location).
