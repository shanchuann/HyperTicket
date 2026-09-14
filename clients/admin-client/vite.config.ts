import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
export default defineConfig({ plugins: [react()], clearScreen: false, server: { port: 5174, strictPort: true, watch: { ignored: ['**/src-tauri/**'] } }, envPrefix: ['VITE_', 'TAURI_'] });
