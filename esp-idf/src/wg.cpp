/**
 * WireGuard VPN — tunnel lifecycle.
 * Registers NET_EV_UP/DOWN/CFG_CHANGED/POLL callbacks with net.
 * All code runs on net's task context.
 */
#include "wg.h"
#include "net.h"
#include "storage.h"
#include "compat.h"
#include "pm.h"
#include "log.h"
#include "cli.h"
#include <cstring>
#include <cstdio>
#include <esp_timer.h>
#include <esp_wireguard.h>

extern "C" {
#include "wireguard-platform.h"
#include "wireguard.h"
#include "crypto.h"
}

static volatile bool tunnelUp = false;
static bool peerWasUp = false;
static uint32_t lastPollMs = 0;

static wireguard_config_t wgConfig = ESP_WIREGUARD_CONFIG_DEFAULT();
static wireguard_ctx_t wgCtx = {};

static char privKey[48];
static char peerPubKey[48];
static char address[16];
static char netmask[16];
static char endpoint[64];
static char endpointHost[64];
static int endpointPort = 51820;

static bool derivePubKey(const char* privKeyB64, char* pubKeyB64, size_t pubKeyB64Len);

static bool configValid() {
    storageGetStr("secrets.wg.key", privKey, sizeof(privKey));
    storageGetStr("s.wg.peer_pubkey", peerPubKey, sizeof(peerPubKey));
    storageGetStr("s.wg.address", address, sizeof(address));
    storageGetStr("s.wg.endpoint", endpoint, sizeof(endpoint));
    return privKey[0] && peerPubKey[0] && address[0] && endpoint[0];
}

static void wgStart() {
    if (tunnelUp) return;
    if (!configValid()) {
        info("missing config:%s%s%s%s\n",
             privKey[0]    ? "" : " wg_private_key",
             peerPubKey[0] ? "" : " wg_peer_pubkey",
             address[0]    ? "" : " wg_address",
             endpoint[0]   ? "" : " wg_endpoint");
        return;
    }
    storageGetStr("s.wg.netmask", netmask, sizeof(netmask), "255.255.255.0");
    safeStrncpy(endpointHost, endpoint, sizeof(endpointHost));
    endpointPort = 51820;
    char* colon = strrchr(endpointHost, ':');
    if (colon) { *colon = '\0'; endpointPort = atoi(colon + 1);
        if (endpointPort <= 0 || endpointPort > 65535) endpointPort = 51820; }
    wgConfig.private_key = privKey;
    wgConfig.listen_port = 0;
    wgConfig.public_key = peerPubKey;
    wgConfig.preshared_key = nullptr;
    wgConfig.allowed_ip = address;
    wgConfig.allowed_ip_mask = netmask;
    wgConfig.endpoint = endpointHost;
    wgConfig.port = endpointPort;
    wgConfig.persistent_keepalive = storageGetInt("s.wg.keepalive", 25);
    info("endpoint %s:%d address %s/%s\n", endpointHost, endpointPort, address, netmask);
    esp_err_t e = esp_wireguard_init(&wgConfig, &wgCtx);
    if (e != ESP_OK) { err("init failed: %s\n", esp_err_to_name(e)); return; }
    e = esp_wireguard_connect(&wgCtx);
    if (e != ESP_OK) { err("connect failed: %s\n", esp_err_to_name(e)); return; }
    tunnelUp = true;
    peerWasUp = false;
    lastPollMs = millis();
    info("handshake initiated\n");
}

static void wgStop() {
    if (!tunnelUp) return;
    esp_wireguard_disconnect(&wgCtx);
    tunnelUp = false;
    storageSet("wg.up", 0);
    info("tunnel stopped\n");
}

/* ---- Net event callbacks ---- */

static void wgOnUp(const char*) {
    if (storageGetInt("s.wg.enable")) wgStart();
}

static void wgOnDown(const char*) {
    wgStop();
}

static esp_timer_handle_t keygenTimer = nullptr;

static void keygenTimerCb(void*) {
    wgGenKey(nullptr);
    storageSet("wg.keygen", 0);
}

static void wgOnCfg(const char* key) {
    if (strcmp(key, "wg.keygen") == 0) {
        if (storageGetInt("wg.keygen") != 1) return;
        if (!keygenTimer) {
            esp_timer_create_args_t args = {};
            args.callback = keygenTimerCb;
            args.name = "wg_keygen";
            esp_timer_create(&args, &keygenTimer);
        }
        esp_timer_stop(keygenTimer);
        esp_timer_start_once(keygenTimer, 2000000);  /* 2 seconds */
        return;
    }
    if (strcmp(key, "s.wg.enable") == 0) {
        if (storageGetInt("s.wg.enable") && netIsUp()) wgStart();
        else wgStop();
    } else if ((strncmp(key, "s.wg.", 5) == 0 || strncmp(key, "secrets.wg.", 11) == 0) && tunnelUp) {
        info("config changed (%s), restarting tunnel\n", key);
        wgStop();
        if (storageGetInt("s.wg.enable") && netIsUp()) wgStart();
    }
}

static void wgOnPoll(const char*) {
    if (!tunnelUp || millis() - lastPollMs < 5000) return;
    lastPollMs = millis();
    bool peerUp = (esp_wireguardif_peer_is_up(&wgCtx) == ESP_OK);
    if (peerUp != peerWasUp) {
        info("peer %s\n", peerUp ? "connected" : "disconnected");
        peerWasUp = peerUp;
    }
    storageSet("wg.up", peerUp ? 1 : 0);
}

/* ---- Public API ---- */

static void wgCliCmd(const char* args) {
    if (strcmp(args, "help") == 0) { cliPrintf("  %-*s WireGuard tunnel\n", CLI_HELP_COL, "wg [up|down|keygen]"); return; }
    if (!*args || strcmp(args, "status") == 0) {
        wgStatus([](const char* d, size_t l) { cliPrintf("%.*s", (int)l, d); });
    } else if (strcmp(args, "keygen") == 0) {
        wgGenKey([](const char* d, size_t l) { cliPrintf("%.*s", (int)l, d); });
    } else if (strcmp(args, "up") == 0) {
        storageSet("s.wg.enable", 1);
    } else if (strcmp(args, "down") == 0) {
        storageSet("s.wg.enable", 0);
    }
}

/* Module config version. Bump when adding/changing defaults. See duckdns.cpp. */
#define WG_VERSION 1

void wgInit() {
    int v = storageGetInt("s.wg.version", 0);
    if (v < WG_VERSION) {
        storageDefaultTree("s.wg", R"({
            "enable": 0,
            "address": "",
            "netmask": "255.255.255.0",
            "endpoint": "",
            "peer_pubkey": "",
            "pubkey": "",
            "keepalive": 25,
            "dns": ""
        })");
        storageDefault("secrets.wg.key", "");
        storageSet("s.wg.version", WG_VERSION);
    }

    netRegister(NET_EV_UPSTREAM_UP,   wgOnUp);
    netRegister(NET_EV_UPSTREAM_DOWN, wgOnDown);
    netRegister(NET_EV_CFG_CHANGED,   wgOnCfg);
    netRegister(NET_EV_POLL,          wgOnPoll);
    cliRegisterCmd("wg", wgCliCmd);

    /* Migrate s.wg.key → secrets.wg.key (one-time, for existing devices) */
    char oldKey[48];
    storageGetStr("s.wg.key", oldKey, sizeof(oldKey));
    if (oldKey[0]) {
        storageSet("secrets.wg.key", oldKey);
        storageUnset("s.wg.key");
        info("migrated private key to secrets.wg.key\n");
    }

    /* Derive and publish pubkey from existing private key (if any) */
    char pk[48];
    storageGetStr("secrets.wg.key", pk, sizeof(pk));
    if (pk[0]) {
        char pub[48];
        if (derivePubKey(pk, pub, sizeof(pub)))
            storageSet("s.wg.pubkey", pub);
    }

}

bool wgIsUp() { return tunnelUp; }

static bool derivePubKey(const char* privKeyB64, char* pubKeyB64, size_t pubKeyB64Len) {
    uint8_t priv[32], pub[32];
    size_t len = 32;
    if (!wireguard_base64_decode(privKeyB64, priv, &len) || len != 32) return false;
    static const uint8_t basepoint[32] = { 9 };
    if (wireguard_x25519(pub, priv, basepoint) != 0) return false;
    size_t olen = pubKeyB64Len;
    return wireguard_base64_encode(pub, 32, pubKeyB64, &olen);
}

void wgStatus(cli_write_fn write) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "  wg: %s\n", tunnelUp ? "UP" : "DOWN");
    if (n > 0) write(buf, (size_t)n);
    char pk[48];
    storageGetStr("secrets.wg.key", pk, sizeof(pk));
    if (pk[0]) {
        char pub[48];
        if (derivePubKey(pk, pub, sizeof(pub))) {
            n = snprintf(buf, sizeof(buf), "  public key: %s\n", pub);
            if (n > 0) write(buf, (size_t)n);
        }
    }
    if (tunnelUp) {
        bool peerUp = (esp_wireguardif_peer_is_up(&wgCtx) == ESP_OK);
        n = snprintf(buf, sizeof(buf), "  peer: %s\n", peerUp ? "connected" : "handshaking");
        if (n > 0) write(buf, (size_t)n);
        n = snprintf(buf, sizeof(buf), "  address: %s/%s\n", address, netmask);
        if (n > 0) write(buf, (size_t)n);
        n = snprintf(buf, sizeof(buf), "  endpoint: %s:%d\n", endpointHost, endpointPort);
        if (n > 0) write(buf, (size_t)n);
    }
}

void wgGenKey(cli_write_fn write) {
    uint8_t key[32];
    wireguard_random_bytes(key, 32);
    key[0] &= 248;
    key[31] = (key[31] & 127) | 64;
    char b64[48];
    size_t olen = sizeof(b64);
    wireguard_base64_encode(key, 32, b64, &olen);
    storageSet("secrets.wg.key", b64);
    char pub[48];
    if (derivePubKey(b64, pub, sizeof(pub))) {
        storageSet("s.wg.pubkey", pub);
        if (write) {
            char buf[128];
            int n = snprintf(buf, sizeof(buf), "  public key: %s\n", pub);
            if (n > 0) write(buf, (size_t)n);
        }
    }
}
