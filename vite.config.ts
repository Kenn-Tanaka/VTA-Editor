import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';

// https://vite.dev/config/
export default defineConfig({
  base: './',
  plugins: [
    react(),
    tailwindcss(),
  ],
  server: {
    port: 5173,
    host: '127.0.0.1',
    proxy: {
      '/v1': {
        target: 'http://127.0.0.1:8765',
        changeOrigin: true,
        secure: false,
      },
    },
    watch: {
      ignored: ['**/tools/**', '**/dist/**'],
    },
  },
  preview: {
    port: 4173,
    host: '127.0.0.1',
    proxy: {
      '/v1': {
        target: 'http://127.0.0.1:8765',
        changeOrigin: true,
        secure: false,
      },
    },
  },
});

