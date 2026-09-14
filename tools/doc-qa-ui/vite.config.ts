import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: '/doc-qa/',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
  server: {
    port: 5175,
    proxy: {
      '/doc-qa/api': {
        target: 'http://127.0.0.1:7171',
        changeOrigin: true,
      },
    },
  },
})
