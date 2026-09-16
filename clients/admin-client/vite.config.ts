import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
export default defineConfig({
  base: process.env.VITE_BASE_PATH || '/',
  plugins: [react()],
  clearScreen: false,
  server: { port: 5174, strictPort: true, watch: { ignored: ['**/src-tauri/**'] } },
  envPrefix: ['VITE_', 'TAURI_'],
});
