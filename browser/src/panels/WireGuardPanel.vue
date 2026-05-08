<template>
  <div class="q-gutter-y-md">
    <SettingToggle label="Enable" k="s.wg.enable" />

    <q-separator dark />

    <PanelHeading>Our Side</PanelHeading>
    <div class="q-gutter-y-sm">
      <SettingText label="IP Address" k="s.wg.address" />
      <SettingText label="Netmask" k="s.wg.netmask" />
      <SettingText label="DNS" k="s.wg.dns" />
      <SettingSlider label="Keepalive" k="s.wg.keepalive" :min="0" :max="300" />

      <div class="row items-center no-wrap">
        <div class="col-4 text-caption">Private Key</div>
        <div class="col text-caption" :style="{ opacity: 0.6 }">{{ hasPubkey ? '(generated)' : '(not set)' }}</div>
      </div>

      <div class="row items-center no-wrap">
        <div class="col-4 text-caption">Public Key</div>
        <div class="col row items-center no-wrap" style="gap:8px;min-width:0">
          <span class="text-caption" :style="{ opacity: pubkey ? 0.85 : 0.4, fontFamily: 'monospace', fontSize: '12px', wordBreak: 'break-all', userSelect: 'all' }">{{ pubkey || '—' }}</span>
          <button v-if="pubkey" type="button" class="copy-btn" title="Copy public key" @click="copyPubkey">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" /><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" /></svg>
          </button>
        </div>
      </div>

      <div class="row items-center no-wrap q-mt-sm">
        <div class="col-4"></div>
        <div class="col">
          <q-btn dense no-caps label="Generate Key" :disable="generating"
            :style="{
              fontSize: '13px',
              background: generating ? '#666' : 'white',
              color: generating ? '#aaa' : '#111',
            }"
            @click="generateKey" />
        </div>
      </div>
    </div>

    <q-separator dark />

    <PanelHeading>Other Side</PanelHeading>
    <div class="q-gutter-y-sm">
      <SettingText label="IP or Name" k="s.wg.endpoint" />
      <SettingText label="Public Key" k="s.wg.peer_pubkey" />
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useDeviceStore } from '../stores/device'

const device = useDeviceStore()

const pubkey = computed(() => String(device.get('s.wg.pubkey') ?? ''))
const hasPubkey = computed(() => pubkey.value.length > 0)
const generating = computed(() => Number(device.get('wg.keygen') ?? 0) === 1)

function generateKey() {
  device.set('wg.keygen', 1)
}

function copyPubkey() {
  if (pubkey.value) navigator.clipboard.writeText(pubkey.value)
}
</script>

<style scoped>
.copy-btn {
  display: inline-flex; align-items: center; justify-content: center;
  width: 28px; height: 28px; padding: 0; border: none; border-radius: 4px;
  background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.7); cursor: pointer;
  flex-shrink: 0;
}
.copy-btn:hover { background: rgba(255,255,255,0.16); }
.ellipsis { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
</style>
