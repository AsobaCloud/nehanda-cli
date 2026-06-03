import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

export default defineConfig({
  plugins: [react(), viteSingleFile()],
  build: {
    outDir: 'dist',
    // vite-plugin-singlefile inlines all assets
    assetsInlineLimit: 100000000,
    cssCodeSplit: false,
  },
});
