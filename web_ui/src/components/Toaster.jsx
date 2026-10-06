import { useCallback, useEffect, useRef, useState } from 'react'
import { CheckCircleIcon, ExclamationCircleIcon, InfoCircleIcon, XMarkIcon } from './icons.jsx'

// Hệ thống thông báo góc màn hình (toast), không dùng thư viện ngoài.
//   const toast = useToasts()
//   toast.push({ type: 'success' | 'error' | 'info', title, details?: string[], duration?: ms })
//   <Toaster toasts={toast.toasts} onDismiss={toast.dismiss} />

const MAX_VISIBLE = 5
const DURATION = { success: 4000, info: 5000, error: 9000 } // lỗi để lâu hơn cho kịp đọc

const STYLE = {
  success: { Icon: CheckCircleIcon, icon: 'text-emerald-500 dark:text-emerald-400' },
  error: { Icon: ExclamationCircleIcon, icon: 'text-red-500 dark:text-red-400' },
  info: { Icon: InfoCircleIcon, icon: 'text-indigo-500 dark:text-indigo-400' },
}

export function useToasts() {
  const [toasts, setToasts] = useState([])
  const idRef = useRef(0)

  const dismiss = useCallback((id) => setToasts((ts) => ts.filter((t) => t.id !== id)), [])

  const push = useCallback((toast) => {
    const id = ++idRef.current
    // Giữ tối đa MAX_VISIBLE thông báo: cái cũ nhất bị đẩy ra trước
    setToasts((ts) => [...ts.slice(-(MAX_VISIBLE - 1)), { type: 'info', ...toast, id }])
    return id
  }, [])

  return { toasts, push, dismiss }
}

function ToastItem({ toast, onDismiss }) {
  // Rê chuột hoặc focus bàn phím vào toast thì tạm dừng đếm giờ tự đóng. Hai cờ riêng: rời chuột
  // trong khi nút đóng còn giữ focus không được làm toast biến mất (focus sẽ rơi về <body>).
  const [hovered, setHovered] = useState(false)
  const [focused, setFocused] = useState(false)
  const paused = hovered || focused
  const { Icon, icon } = STYLE[toast.type] ?? STYLE.info

  useEffect(() => {
    if (paused) return
    const timer = setTimeout(() => onDismiss(toast.id), toast.duration ?? DURATION[toast.type] ?? 5000)
    return () => clearTimeout(timer)
  }, [paused, toast, onDismiss])

  return (
    <div
      role={toast.type === 'error' ? 'alert' : 'status'}
      onMouseEnter={() => setHovered(true)} onMouseLeave={() => setHovered(false)}
      onFocus={() => setFocused(true)}
      onBlur={(e) => { if (!e.currentTarget.contains(e.relatedTarget)) setFocused(false) }}
      className="pointer-events-auto flex w-full items-start gap-3 rounded-xl bg-white p-4 shadow-lg ring-1 ring-slate-900/10
                 motion-safe:animate-toast-in sm:w-96 dark:bg-slate-800 dark:ring-white/10"
    >
      <Icon className={`mt-0.5 h-5 w-5 shrink-0 ${icon}`} />
      <div className="min-w-0 flex-1">
        <p className="text-sm font-semibold text-slate-900 dark:text-slate-100">{toast.title}</p>
        {toast.details?.length > 0 && (
          <ul className="mt-1 space-y-0.5 text-sm break-words text-slate-600 dark:text-slate-300">
            {toast.details.map((d, i) => <li key={i}>{d}</li>)}
          </ul>
        )}
      </div>
      <button type="button" onClick={() => onDismiss(toast.id)} aria-label="Đóng thông báo"
        className="-m-1 shrink-0 rounded-md p-1 text-slate-400 hover:bg-slate-100 hover:text-slate-600
                   focus:outline-none focus-visible:ring-2 focus-visible:ring-indigo-500/40
                   dark:hover:bg-slate-700 dark:hover:text-slate-200">
        <XMarkIcon className="h-4 w-4" />
      </button>
    </div>
  )
}

export function Toaster({ toasts, onDismiss }) {
  return (
    // Vùng aria-live luôn có sẵn trong DOM để trình đọc màn hình đọc được toast mới chèn vào
    <div aria-live="polite"
      className="pointer-events-none fixed inset-x-4 bottom-4 z-50 flex flex-col items-stretch gap-2
                 sm:inset-x-auto sm:right-6 sm:bottom-6 sm:items-end">
      {toasts.map((t) => <ToastItem key={t.id} toast={t} onDismiss={onDismiss} />)}
    </div>
  )
}
