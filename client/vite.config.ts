import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// Dev server proxies /api to the Python Stellar Forge backend.
export default defineConfig({
  plugins: [react()],
  base: './',
  server: {
    port: 5173,
    proxy: { '/api': 'http://127.0.0.1:8000', '/auth': 'http://127.0.0.1:8000' },
  },
  build: {
    chunkSizeWarningLimit: 2000,
    // stable file names so rebuilt copies overwrite in place
    rollupOptions: { output: { entryFileNames: 'assets/omnispace.js', chunkFileNames: 'assets/[name].js', assetFileNames: 'assets/[name][extname]' } },
  },
})
