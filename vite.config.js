import { defineConfig } from "vite";

export default defineConfig({
  server: { port: 4173, strictPort: true },
  build: {
    rollupOptions: {
      output: { manualChunks: { charts: ["chart.js"], icons: ["lucide"] } },
    },
  },
});
