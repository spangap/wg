# wg — WireGuard tunnel

**wg** brings up a [WireGuard](https://www.wireguard.com) tunnel from a
[spangap](../spangap) device to a configured peer. It is a client only: it dials a
single remote endpoint, runs the Noise handshake and per-packet ChaCha20-Poly1305
inline, and exposes a tunnel network interface that the rest of the device routes
over.

Use it as the alternative to the `upnp` + `duckdns` + `acme` remote-access stack
when the device sits behind CG-NAT, or when the operator simply doesn't want to
expose anything on the public internet — the device reaches *out* to a WireGuard
server and becomes addressable on the tunnel's private subnet.

## Origins

The tunnel core is a vendored fork of
[`trombik/esp_wireguard`](https://github.com/trombik/esp_wireguard) (the de-facto
ESP-IDF port of the wireguard-lwip implementation), kept under
[`esp-idf/src/esp_wireguard/`](esp-idf/src/esp_wireguard/). The spangap wrapper
(`esp-idf/src/wg.cpp`) reads configuration from storage, drives the tunnel off net
events, generates keys, and publishes status. The fork's deltas and the sdkconfig it
needs are in [INTERNALS.md](INTERNALS.md).

## What this straddle owns

```
wg/
├── esp-idf/
│   ├── include/
│   │   ├── wg.h               public API (wgInit, wgIsUp, wgStatus, wgGenKey)
│   │   └── esp_wireguard.h    vendored upstream public header
│   ├── Kconfig                vendored-fork tunables (peers, x25519 impl)
│   └── src/
│       ├── wg.cpp             config-from-storage, lifecycle, keygen, status
│       └── esp_wireguard/     trombik/esp_wireguard, vendored + patched
```

There is no browser half: the settings pane is described in `straddle.yaml` and the
build lowers it to both surfaces.

## What it does

`wg` is not a task. `wgInit` registers four callbacks with [spangap-net](../spangap-net)
and everything afterwards runs on net's task context:

- **`NET_EV_UPSTREAM_UP`** — the STA has a real upstream (internet) link. If
  `s.wg.enable=1` and the config is complete, the tunnel starts here. It deliberately
  keys on *upstream*-up, not link-up: WireGuard needs to reach the peer over the
  internet, so there is no point starting in AP-only mode.
- **`NET_EV_UPSTREAM_DOWN`** — the tunnel is torn down.
- **`NET_EV_CFG_CHANGED`** — a write to any `s.wg.*` / `secrets.wg.*` key restarts a
  running tunnel; toggling `s.wg.enable` starts or stops it; writing `wg.keygen=1`
  triggers key generation.
- **`NET_EV_POLL`** — every 5 s while the tunnel is up, `wg` checks whether the peer
  handshake has completed, logs connect/disconnect transitions, and mirrors the
  result to `wg.up` for the UI.

The tunnel is started for you: if the `wg` straddle is in the build, `wgInit` runs
from the generated init and the callbacks are live. There is no consumer `main.cpp`
edit and no hand-written init call.

A high-bitrate consumer (e.g. a video/audio streamer) benefits from the tunnel
because UDP traffic passes through it without TCP flow control — see
[Streaming](#streaming) below.

## Public API

The C surface in [`wg.h`](esp-idf/include/wg.h):

| Function | Purpose |
|---|---|
| `wgInit()` | Register net callbacks + the `wg` CLI verb, seed storage defaults, derive/publish the public key. Called automatically by the generated init. |
| `wgIsUp()` | `true` once the tunnel interface is connected (handshake initiated; not the same as peer-handshake-complete). |
| `wgStatus(write)` | Render tunnel state + the device's public key to a `cli_write_fn`. |
| `wgGenKey(write)` | Generate a fresh Curve25519 private key, store it in `secrets.wg.key`, derive and publish `s.wg.pubkey`, and print the public key. |

(There are no `wgConnect`/`wgDisconnect` entry points — starting and stopping is
driven entirely by `s.wg.enable` and net events.)

## Storage variables

`wg` has no socket API — storage is the control surface. The **Settings → WiFi & Network →
WireGuard** pane is generated from the `settings:` block in
[`straddle.yaml`](straddle.yaml) on both surfaces. The keys below are seeded by `wgInit`
and read by `wg.cpp`.

### Settings (`s.wg.*`)

| Key | Default | Meaning |
|---|---|---|
| `s.wg.enable` | `0` | Master switch. When `1`, the tunnel starts on upstream-up and on a live toggle. |
| `s.wg.address` | `""` | The device's **own** tunnel-interface IP (e.g. `10.0.0.2`). Required. (Upstream's config field is confusingly named `allowed_ip`; it is the local address, not a peer allowed-IPs list.) |
| `s.wg.netmask` | `255.255.255.0` | Netmask for the tunnel address. |
| `s.wg.endpoint` | `""` | Server host or `host:port`. Required. Port defaults to `51820` when omitted. |
| `s.wg.peer_pubkey` | `""` | The server's base64 public key. Required. |
| `s.wg.keepalive` | `25` | Persistent-keepalive interval in seconds (`0` = off); the pane offers 0-600. |

### Published / derived (read-only to the operator)

| Key | Meaning |
|---|---|
| `s.wg.pubkey` | The device's own base64 public key, derived from `secrets.wg.key`. Published by `wgInit` and `wgGenKey`; this is the value you hand to the server's `[Peer] PublicKey`. |

### Status & command sentinels

| Key | Direction | Meaning |
|---|---|---|
| `wg.up` | status (written by `wg`) | `1` while the peer handshake is complete, `0` otherwise. Ephemeral; drives the UI's connected indicator. |
| `wg.keygen` | command (written by UI) | Write `1` to trigger key generation. `wg` runs it (after a 2 s debounce) and clears the key back to `0`. |

### Secrets

| Key | Meaning |
|---|---|
| `secrets.wg.key` | The device's base64 Curve25519 **private** key. Lives in the `secrets.*` namespace, so it is never sent to the browser — the web/LCD panels only ever see the derived `s.wg.pubkey` and a "generated / not set" indicator. |

There is also an internal `s.wg.version` key (not operator-facing): `wgInit` uses it
to seed the `s.wg.*` defaults exactly once. See [INTERNALS §6](INTERNALS.md#6-version-gate-and-key-migration).

## CLI

```
wg                  show tunnel status + the device's public key
wg up               enable the tunnel (sets s.wg.enable=1)
wg down             disable the tunnel (sets s.wg.enable=0)
wg keygen           generate a new private key, store it, print the public key
```

Run any of these on-device via `spangap cli "<command>"`.

## Setup

Generate the keypair on the device, point it at the server, and enable:

```
wg keygen                                  # prints the device's public key
set s.wg.address=10.0.0.2
set s.wg.endpoint=my.server.com:51820
set s.wg.peer_pubkey=<server's public key>
set s.wg.enable=1
```

Then add the device as a peer on the WireGuard server, using the public key `wg keygen`
printed and the tunnel address you set:

```
[Peer]
PublicKey = <public key from wg keygen>
AllowedIPs = 10.0.0.2/32
```

## Streaming

Throughput through the tunnel depends on the transport:

- **Browser traffic (WebSocket / TCP)** is bounded by `TCP_SND_BUF / RTT`. With a
  16 KB window and 100 ms RTT to the server, that is roughly **1.3 Mbps** — fine for
  config and CLI, slow for video.
- **UDP** passes through the tunnel without TCP flow control, so a native client
  speaking a UDP protocol (e.g. an app-level RTP/RTSP server in a consumer straddle)
  gets the full link bandwidth. This is the recommended path for high-bitrate
  audio/video.

## Dependencies

- [spangap-net](../spangap-net) — net events, the UDP socket, and the lwIP netif slot.

## What it does NOT do

- It is **not** a WireGuard server — it dials a single peer as a client.
- It does not negotiate keys with the server; you configure matching keys on both
  ends out of band.

## Read next

- [INTERNALS.md](INTERNALS.md) — the vendored-fork inventory, the netif sidecar-map
  workaround, the sdkconfig requirements, and where the crypto runs.
