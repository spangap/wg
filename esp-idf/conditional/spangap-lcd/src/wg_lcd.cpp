/**
 * wg_lcd.cpp — on-device Settings → Net → WireGuard pane (LVGL).
 *
 * Mirrors the browser WireGuardPanel (the private key stays device-only; we show
 * the public key + a Generate key action that triggers wg.keygen, same as the
 * browser button).
 *
 * This whole file lives under conditional/spangap-lcd/, compiled only when the
 * lcd straddle is staged, so no #if is needed. Registration happens via the
 * when:-gated wgLcdRegister init hook (spangap/spangap-lcd).
 */
#include "lcd.h"
#include "storage.h"

static void wgSettingsPane(void* arg) {
    lv_obj_t* p = static_cast<lv_obj_t*>(arg);
    lcdSettingSection(p, "Our Side");
    lcdSettingSwitch (p, "Enable",      "s.wg.enable");
    lcdSettingText   (p, "Address",     "s.wg.address");
    lcdSettingText   (p, "Netmask",     "s.wg.netmask");
    lcdSettingText   (p, "DNS",         "s.wg.dns");
    lcdSettingSlider (p, "Keepalive",   "s.wg.keepalive", 0, 300);
    lcdSettingValue  (p, "Public key",  "s.wg.pubkey");
    lcdSettingButton (p, "Generate key", [](void*) { storageSet("wg.keygen", 1); });
    lcdSettingSection(p, "Other Side");
    lcdSettingText   (p, "Endpoint",     "s.wg.endpoint");
    lcdSettingText   (p, "Peer pub key", "s.wg.peer_pubkey");
}

/* Register the WireGuard settings pane — a when:-gated init: hook
 * (spangap/spangap-lcd). Plain C++ linkage to match the generated dispatcher's
 * forward decl. */
void wgLcdRegister(void) {
    lcdRegisterSettings("Internet/WireGuard", "WireGuard", wgSettingsPane);
}
