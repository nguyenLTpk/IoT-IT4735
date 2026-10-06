import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'

// Địa chỉ backend FastAPI. Vite chuyển tiếp mọi request /api/* tới đây,
// nên trình duyệt chỉ nói chuyện với 1 origin (không cần cấu hình CORS khi phát triển).
const API_TARGET = process.env.VITE_API_TARGET || 'http://127.0.0.1:8000'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  server: {
    proxy: { '/api': API_TARGET },
  },
  // `npm run preview` dùng chung proxy với dev server
})
