import { useMenuStore } from 'spangap-browser/stores/menu'
import WireGuardPanel from '../panels/WireGuardPanel.vue'

export function registerWg() {
  useMenuStore().register('settings/network/wireguard', 'WireGuard', { type: 'panel', component: WireGuardPanel })
}
