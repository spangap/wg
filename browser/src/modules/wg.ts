import { useMenuStore } from 'spangap-browser/stores/menu'
import WireGuardPanel from '../panels/WireGuardPanel.vue'

export function registerWg() {
  useMenuStore().register('settings', 'Settings', 10, [
    { id: 'network', label: 'Network', type: 'submenu', order: 20,
      children: [
        { id: 'network.wireguard', label: 'WireGuard', type: 'panel', order: 30,
          component: WireGuardPanel },
      ],
    },
  ])
}
