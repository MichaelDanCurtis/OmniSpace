import * as THREE from 'three'

/**
 * Dispose GPU resources under `root`. Geometries, materials and textures marked
 * `userData.shared = true` (caches reused across systems) are left alone; a
 * material's own map is freed only when it is flagged `userData.ownsMap`.
 */
export function disposeTree(root: THREE.Object3D) {
  root.traverse((o) => {
    const m = o as THREE.Mesh
    if (m.geometry && !m.geometry.userData.shared) m.geometry.dispose()
    const mats = (Array.isArray(m.material) ? m.material : m.material ? [m.material] : []) as THREE.Material[]
    for (const mat of mats) {
      if (mat.userData.shared) continue
      const map = (mat as THREE.MeshBasicMaterial).map
      if (map && mat.userData.ownsMap) map.dispose()
      mat.dispose()
    }
  })
}
