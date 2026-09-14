import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: '/log-ai-detector/',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
  server: {
    port: 5174,
    proxy: {
      '/log-ai-detector/api': {
        target: 'http://127.0.0.1:7070',
        rewrite: path => path.replace(/^\/log-ai-detector/, ''),
        changeOrigin: true,
      },
    },
  },
})
