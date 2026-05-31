import { useMenuStore } from 'spangap-browser/stores/menu'
import WireGuardPanel from '../panels/WireGuardPanel.vue'

export function registerWg() {
  useMenuStore().register('settings', 'Settings', [
    { id: 'network', label: 'Network', type: 'submenu',
      children: [
        { id: 'network.wireguard', label: 'WireGuard', type: 'panel',
          component: WireGuardPanel },
      ],
    },
  ])
}
