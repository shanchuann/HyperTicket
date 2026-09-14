import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    host: true,
    proxy: {
      '/log-ai-detector': {
        target: 'http://127.0.0.1:7070',
        changeOrigin: true,
      },
    },
  },
})
