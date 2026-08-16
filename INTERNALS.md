# wg — internals

Maintainer reference for the WireGuard tunnel. The [README](README.md) is the
operator guide; this document is for changing the code without breaking it.

## 1. Everything the vendored fork changes or adds

The tunnel core under [`esp-idf/src/esp_wireguard/`](esp-idf/src/esp_wireguard/) is
vendored from [`trombik/esp_wireguard`](https://github.com/trombik/esp_wireguard)
v0.9.0 (wireguard-lwip core). The deltas from that baseline:

- **PR #45 crash fixes.** A NULL check in `wireguardif_output`, clearing the IP before
  teardown, and calling `wireguardif_fini` only *after* `netif_remove`. Without these,
  the device crashes on tunnel teardown / interface removal.
- **`netif->state` sidecar map.** ESP-IDF 5.x stores the `esp_netif` pointer in
  `netif->state`, but wireguard-lwip wants to stash its own `wireguard_device` there —
  the same slot, two owners. The fork keeps the WireGuard context in a map keyed by
  netif pointer instead of in `netif->state`. This is the reason the component is
  vendored at all rather than pulled from the registry; see the sdkconfig requirement
  below and the pitfall in §4.
- **Private headers exposed.** `CMakeLists.txt` adds
  `PRIV_INCLUDE_DIRS "src/esp_wireguard"` so `wg.cpp` can reach `crypto.h`,
  `wireguard.h`, and `wireguard-platform.h` directly — it needs
  `wireguard_random_bytes`, `wireguard_x25519`, and the base64 codec for key generation
  and public-key derivation, which are not part of the public `esp_wireguard.h` surface.

The vendored fork drops out as soon as upstream propagates the `netif->state` fix to a
registry-published version; at that point the sources can be deleted and
`idf_component.yml` can depend on `trombik/esp_wireguard` directly.

The fork also carries a software Curve25519 (MIT, Mike Hamburg / Cryptography Research)
in `esp_wireguard/crypto/`. [rns](../rns) re-vendors the same X25519 implementation
into its microReticulum fork for opportunistic-packet ECDH — keeping one constant-time
implementation shared rather than each straddle carrying its own.

### Kconfig tunables

The fork's [`Kconfig`](esp-idf/Kconfig) adds a menu gated `visible if SPANGAP_WG`
(the `CONFIG_SPANGAP_WG` switch is owned by spangap-core), so the options only appear
when the `wg` straddle is staged. All defaults suit the client-to-single-peer role and
rarely need changing:

| Option | Default | Meaning |
|---|---|---|
| `WIREGUARD_MAX_PEERS` | `1` | Peer slots. The device dials one server, so one slot suffices. |
| `WIREGUARD_MAX_SRC_IPS` | `2` | Allowed-source-IP entries per peer. |
| `MAX_INITIATIONS_PER_SECOND` | `2` | Rate limit on accepting inbound handshake initiations. Server-side; an outbound client effectively never hits it. |
| `WIREGUARD_ESP_ADAPTER_SELECTION` | `WIREGUARD_ESP_NETIF` | TCP/IP adapter. ESP-NETIF is correct for IDF ≥ 4.1; the legacy `WIREGUARD_ESP_TCPIP_ADAPTER` is for pre-4.1 / ESP8266 RTOS SDK only. |
| `WIREGUARD_x25519_IMPLEMENTATION` | `WIREGUARD_x25519_IMPLEMENTATION_DEFAULT` | X25519 backend. The default compiles `crypto/refc/x25519.c`; selecting `WIREGUARD_x25519_IMPLEMENTATION_NACL` switches to the NaCl `nacl/.../smult.c` reference instead. Both are vendored. |

## 2. Lifecycle

`wg.cpp` is **not a task**. `wgInit` registers four net callbacks and the `wg` CLI
verb; all tunnel work then runs on net's task context whenever an event fires:

- `wgOnUp` (`NET_EV_UPSTREAM_UP`) → `wgStart` if `s.wg.enable=1`.
- `wgOnDown` (`NET_EV_UPSTREAM_DOWN`) → `wgStop`.
- `wgOnCfg` (`NET_EV_CFG_CHANGED`) → key generation on `wg.keygen=1` (via a 2 s
  one-shot `esp_timer`), start/stop on `s.wg.enable`, and a stop+start restart on any
  other `s.wg.*` / `secrets.wg.*` write while the tunnel is up.
- `wgOnPoll` (`NET_EV_POLL`) → throttled to once per 5 s; reads
  `esp_wireguardif_peer_is_up`, logs transitions, mirrors `wg.up`.

`wgStart` reads the keys/peer/address from storage (`configValid` requires
`secrets.wg.key`, `s.wg.peer_pubkey`, `s.wg.address`, and `s.wg.endpoint` to be
non-empty), parses an optional `:port` off the endpoint (default `51820`), fills a
`wireguard_config_t`, and calls `esp_wireguard_init` then `esp_wireguard_connect`.
`wgStop` calls `esp_wireguard_disconnect` and clears `wg.up`.

The device's own address goes into the config's `allowed_ip`/`allowed_ip_mask` fields
(`s.wg.address` / `s.wg.netmask`) — this is the local tunnel-interface address, *not*
a list of peer-routed CIDRs. The bind `listen_port` is set to **0** (ephemeral): the
device is a client, not a server, so it must not bind the well-known `51820`.

### Crypto placement

Per-packet ChaCha20-Poly1305 runs inline on lwIP's `tcpip_thread` (≈20% CPU at a
14 fps HTTPS stream); handshake crypto (Curve25519, roughly every 2 minutes) runs
there too. The tcpip_thread has no competing workload on this device, so moving the
crypto to a dedicated task buys nothing and only adds complexity — keep it inline.

## 3. sdkconfig requirements (consumer-side)

The buildable that stages `wg` must set, in its `sdkconfig.defaults`:

```
CONFIG_LWIP_PPP_SUPPORT=y                # netif->state slot workaround (§4)
CONFIG_LWIP_TCPIP_CORE_LOCKING=y         # wg crypto calls lwIP under LOCK_TCPIP_CORE
CONFIG_LWIP_TCPIP_CORE_LOCKING_INPUT=y   # same, on the input path
```

## 4. Pitfalls

- **The `netif->state` slot has two would-be owners — keep `CONFIG_LWIP_PPP_SUPPORT=y`.**
  ESP-IDF 5.x's DHCP path dereferences `netif->state` as an `esp_netif`, while
  wireguard-lwip stores its `wireguard_device` there. With `LWIP_PPP_SUPPORT` enabled,
  esp-netif switches to `netif_get_client_data()` for its own pointer, leaving
  `netif->state` for the WireGuard driver. Without the flag the two collide and the
  device dereferences the wrong struct. This is the durable trap behind the vendored
  fork and the sidecar map; do not drop the flag while the fork is in tree.
- **`listen_port` must stay `0`.** Binding `51820` as the local port (treating the
  client like a server) breaks the connection; the device is outbound-only.
- **TCP-CORE-LOCKING is mandatory, not optional.** The crypto runs on the tcpip_thread
  and calls back into lwIP under `LOCK_TCPIP_CORE`; without core-locking those calls
  are unsafe.

## 5. Key generation

`wgGenKey` fills 32 random bytes via `wireguard_random_bytes`, clamps them to a valid
Curve25519 scalar (`key[0] &= 248; key[31] = (key[31] & 127) | 64`), base64-encodes the
result into `secrets.wg.key`, derives the public key with `wireguard_x25519` against the
base point `{9}`, and publishes it to `s.wg.pubkey`. `wgInit` re-derives and republishes
`s.wg.pubkey` at boot whenever a private key already exists, so the operator always sees
the matching public key. Keys are produced **only** by `wg keygen` (CLI) or the
`wg.keygen` button — nothing generates one implicitly on enable.

## 6. Version gate and key migration

`wgInit` carries two pieces of one-time bookkeeping:

- **Version gate.** It reads `s.wg.version` (default `0`); if that is below
  `WG_VERSION` (currently `1`) it seeds the `s.wg.*` defaults via `storageDefaultTree`
  (`enable`, `address`, `netmask`, `endpoint`, `peer_pubkey`, `pubkey`, `keepalive`,
  `dns`) plus the empty `secrets.wg.key`, then writes `s.wg.version = WG_VERSION`. The
  effect is that defaults are seeded exactly once and re-runs are no-ops.
- **Key migration.** If a legacy `s.wg.key` is present, its value is copied to
  `secrets.wg.key` and the old key is unset, so a private key from an older layout
  moves into the `secrets.*` namespace.

Both exist only for previously-flashed devices; a fresh install needs neither and they
are candidates for removal.

## 7. Settings UI

**Settings → WiFi & Network → WireGuard**. The pane is generated from the `settings:` block in `straddle.yaml` — both surfaces and the
storage defaults from that one source. It shows `s.wg.*` and the derived `s.wg.pubkey`
(copyable), and for the private key only the "generated / not set" sentence `wg.cpp`
publishes to `wg.key_state`, because `secrets.wg.key` is never sent to the browser.
Generate Key is a confirmation dialog over an edge write to `wg.keygen`: the flag may be
left set by an attempt that did not complete, and without the edge the next press would
write the value the key already holds.
