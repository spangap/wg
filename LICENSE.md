# License

This repository, **wg** (WireGuard tunnel for spangap), is released under the
**Apache License, Version 2.0**.

Full license text: <https://www.apache.org/licenses/LICENSE-2.0>

Copyright (c) 2026 by spangap project contributors.

The Apache-2.0 license applies to the spangap glue (the straddle scaffolding,
`browser/`, `esp-idf/src/wg*.{c,h}`, README/INTERNALS, build files). The
vendored upstream sources under `esp-idf/src/esp_wireguard/` retain their own
licenses as listed below.

## Third-party software

### Vendored in this repository

The `esp-idf/src/esp_wireguard/` tree is a vendored copy of
[`trombik/esp_wireguard`](https://github.com/trombik/esp_wireguard) (a
WireGuard implementation for ESP-IDF), carried in-tree until the
IDF-5.x `netif->state` PPP fix has propagated to a tagged release.

| Sub-path | Origin | License |
|---|---|---|
| `esp_wireguard/` (top-level: `esp_wireguard.c`, `wireguardif.{c,h}`, `wireguard.{c,h}`, `wireguard-platform.{c,h}`, `crypto.{c,h}`) | Kenta Ida (trombik fork), originally Daniel Hope / Floorsense | **BSD-3-Clause** — see `esp_wireguard/LICENSE` |
| `esp_wireguard/crypto/refc/chacha20.{c,h}`, `chacha20poly1305.{c,h}` | Daniel Hope (Floorsense) | BSD-3-Clause |
| `esp_wireguard/crypto/refc/blake2s.{c,h}` | RFC 7693 reference implementation | **Public domain** |
| `esp_wireguard/crypto/refc/poly1305-donna.{c,h}`, `poly1305-donna-32.h` | Andrew Moon (`floodyberry/poly1305-donna`) | **Public domain or MIT** (dual; per the file header) |
| `esp_wireguard/crypto/refc/x25519.{c,h}` | Mike Hamburg / Cryptography Research, Inc. | **MIT** — see `esp_wireguard/crypto/refc/x25519-license.txt` |
| `esp_wireguard/nacl/crypto_scalarmult/curve25519/ref/` | Matthew Dempsky, derived from D. J. Bernstein's NaCl | **Public domain** |

The BSD-3-Clause non-endorsement clause prohibits use of the names
"Floorsense Ltd" or "Agile Workspace Ltd" to endorse or promote
products derived from this software without prior written permission.

### Build-time dependencies

Declared in `esp-idf/idf_component.yml` and `browser/package.json`:

| Component / package | Source | License |
|---|---|---|
| ESP-IDF (platform) | espressif/esp-idf | Apache-2.0 |
| Browser peer deps (Vue, Quasar, Pinia, vue-router) | npm | MIT |
