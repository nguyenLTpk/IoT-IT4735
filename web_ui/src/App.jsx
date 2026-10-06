import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import AppointmentFilters from './components/AppointmentFilters.jsx'
import AppointmentForm from './components/AppointmentForm.jsx'
import AppointmentTable from './components/AppointmentTable.jsx'
import { Toaster, useToasts } from './components/Toaster.jsx'
import { createAppointment, deleteAppointment, getAppointments, getHealth } from './api.js'
import { parseAppointmentsCsv } from './csv.js'
import { filterAppointments } from './filters.js'

// Tự làm mới để thấy trạng thái check-in từ kiosk gần như tức thì
const REFRESH_MS = 3000
// Số dòng lỗi tối đa liệt kê trong toast sau khi nhập CSV (phần còn lại gộp thành "…và N dòng khác")
const MAX_ERROR_LINES = 5

const apptKey = (a) => `${a.student_id}|${a.appointment_time}`

function Stat({ label, value, accent }) {
  return (
    <div className="rounded-xl border border-slate-200 bg-white px-4 py-3 shadow-sm dark:border-slate-700 dark:bg-slate-800">
      <div className="text-xs font-medium text-slate-500 dark:text-slate-400">{label}</div>
      <div className={`mt-1 text-2xl font-semibold tabular-nums ${accent}`}>{value}</div>
    </div>
  )
}

function ConnectionPill({ health, apiError }) {
  let cls = 'bg-emerald-100 text-emerald-800 dark:bg-emerald-900/40 dark:text-emerald-300'
  let text = 'MQTT đã kết nối'
  if (apiError) {
    cls = 'bg-red-100 text-red-800 dark:bg-red-900/40 dark:text-red-300'
    text = 'Mất kết nối API'
  } else if (!health) {
    cls = 'bg-slate-100 text-slate-600 dark:bg-slate-700 dark:text-slate-300'
    text = 'Đang kiểm tra…'
  } else if (!health.mqtt_enabled) {
    cls = 'bg-slate-100 text-slate-600 dark:bg-slate-700 dark:text-slate-300'
    text = 'MQTT tắt (--no-mqtt)'
  } else if (!health.mqtt_connected) {
    cls = 'bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300'
    text = 'MQTT đang kết nối lại…'
  }
  return (
    <span className={`inline-flex items-center gap-2 rounded-full px-3 py-1 text-xs font-medium ${cls}`}
      title={health?.broker}>
      <span className="h-2 w-2 rounded-full bg-current" />
      {text}
    </span>
  )
}

export default function App() {
  const [appointments, setAppointments] = useState([])
  const [health, setHealth] = useState(null)
  const [loaded, setLoaded] = useState(false)
  const [apiError, setApiError] = useState('')
  const [deletingIds, setDeletingIds] = useState(() => new Set())
  const [lastUpdated, setLastUpdated] = useState(null)
  const [query, setQuery] = useState('')
  const [statusFilter, setStatusFilter] = useState('all')
  const [importProgress, setImportProgress] = useState(null) // { done, total } khi đang nhập CSV
  const toast = useToasts()

  // Mỗi lần làm mới có 1 số thứ tự; chỉ kết quả của lần BẮT ĐẦU sau cùng được áp dụng.
  // Nhờ vậy 1 response đến muộn (mạng chậm) không đè lên dữ liệu mới hơn sau khi thêm/xoá.
  const seqRef = useRef(0)

  const refresh = useCallback(async () => {
    const seq = ++seqRef.current
    try {
      const [list, h] = await Promise.all([getAppointments(), getHealth()])
      if (seq !== seqRef.current) return
      setAppointments(list)
      setHealth(h)
      setApiError('')
      setLastUpdated(new Date())
    } catch (err) {
      if (seq !== seqRef.current) return
      setApiError(err.message || 'Không kết nối được API')
    } finally {
      if (seq === seqRef.current) setLoaded(true)
    }
  }, [])

  useEffect(() => {
    // Chuỗi setTimeout (không dùng setInterval): lần làm mới sau chỉ được lên lịch khi lần
    // trước đã xong -> không bao giờ dồn nhiều request khi API phản hồi chậm.
    let stopped = false
    let timer
    const tick = async () => {
      await refresh()
      if (!stopped) timer = setTimeout(tick, REFRESH_MS)
    }
    tick()
    return () => {
      stopped = true
      clearTimeout(timer)
    }
  }, [refresh])

  async function handleCreate(body) {
    const created = await createAppointment(body) // Lỗi được AppointmentForm hiển thị
    toast.push({
      type: 'success',
      title: 'Đã thêm lịch hẹn',
      details: [`${created.student_name} · ${created.appointment_time} · STT ${String(created.queue_number).padStart(2, '0')}`],
    })
    await refresh()
  }

  async function handleImport(file) {
    if (!/\.csv$/i.test(file.name)) {
      toast.push({ type: 'error', title: 'File không hợp lệ', details: [`"${file.name}" không phải file .csv`] })
      return
    }
    let parsed
    try {
      parsed = await parseAppointmentsCsv(file)
    } catch (err) {
      toast.push({ type: 'error', title: 'Không đọc được file CSV', details: [err.message] })
      return
    }
    const { rows, errors } = parsed
    if (rows.length === 0 && errors.length === 0) {
      toast.push({ type: 'error', title: 'File CSV không có dữ liệu' })
      return
    }

    // Bỏ qua dòng trùng MSSV + giờ hẹn với lịch đã có (hoặc với dòng trước đó trong file),
    // để lỡ nhập lại cùng 1 file không tạo bản sao. Lấy danh sách mới nhất, lỗi thì dùng bản đang hiển thị.
    const existing = await getAppointments().catch(() => appointments)
    const seen = new Set(existing.map(apptKey))
    const failures = [...errors]
    let added = 0
    let skipped = 0
    let abortedMsg = ''

    setImportProgress({ done: 0, total: rows.length })
    try {
      // Gửi TUẦN TỰ: backend cấp STT = MAX+1, gửi song song dễ trùng STT và khó báo lỗi theo dòng
      for (let i = 0; i < rows.length; i++) {
        const { line, body } = rows[i]
        if (seen.has(apptKey(body))) {
          skipped++
        } else {
          try {
            await createAppointment(body)
            seen.add(apptKey(body))
            added++
          } catch (err) {
            // Mất mạng (fetch lỗi, không có status) hoặc lỗi máy chủ 5xx: các dòng sau cũng sẽ hỏng -> dừng
            if (!err.status || err.status >= 500) {
              abortedMsg = `Dừng ở dòng ${line}: ${err.message}. ${rows.length - i} dòng chưa được nhập.`
              break
            }
            failures.push({ line, message: err.message }) // lỗi dữ liệu 4xx: bỏ dòng này, nhập tiếp
          }
        }
        setImportProgress({ done: i + 1, total: rows.length })
      }
    } finally {
      setImportProgress(null)
      await refresh()
    }

    if (added > 0) {
      toast.push({
        type: 'success',
        title: `Đã thêm thành công ${added} lịch hẹn`,
        details: skipped ? [`Bỏ qua ${skipped} dòng đã có (trùng MSSV + giờ hẹn)`] : undefined,
      })
    } else if (skipped > 0 && failures.length === 0 && !abortedMsg) {
      toast.push({ type: 'info', title: 'Không có lịch hẹn mới', details: [`Cả ${skipped} dòng đều đã có trong danh sách`] })
    }
    if (failures.length > 0 || abortedMsg) {
      failures.sort((a, b) => a.line - b.line)
      const details = failures.slice(0, MAX_ERROR_LINES).map((f) => `Dòng ${f.line}: ${f.message}`)
      if (failures.length > MAX_ERROR_LINES) details.push(`…và ${failures.length - MAX_ERROR_LINES} dòng lỗi khác`)
      if (abortedMsg) details.push(abortedMsg)
      // Không có toast thành công/thông tin nào nói về các dòng trùng -> báo luôn ở đây
      if (added === 0 && skipped > 0) details.unshift(`Bỏ qua ${skipped} dòng đã có (trùng MSSV + giờ hẹn)`)
      toast.push({
        type: 'error',
        title: abortedMsg ? 'Nhập CSV bị gián đoạn' : `${failures.length} dòng không nhập được`,
        details,
        duration: 15000,
      })
    }
  }

  async function handleDelete(appt) {
    if (!window.confirm(`Xoá lịch hẹn của ${appt.student_name} (${appt.student_id}) lúc ${appt.appointment_time}?`)) return
    setDeletingIds((s) => new Set(s).add(appt.id))
    try {
      await deleteAppointment(appt.id)
      toast.push({ type: 'success', title: 'Đã xoá lịch hẹn', details: [`${appt.student_name} (${appt.student_id})`] })
      await refresh()
    } catch (err) {
      toast.push({ type: 'error', title: 'Không xoá được lịch hẹn', details: [err.message] })
    } finally {
      setDeletingIds((s) => {
        const next = new Set(s)
        next.delete(appt.id)
        return next
      })
    }
  }

  const checkedIn = appointments.filter((a) => a.status === 'checked_in').length
  const counts = { all: appointments.length, pending: appointments.length - checkedIn, checked_in: checkedIn }
  const visible = useMemo(
    () => filterAppointments(appointments, { query, status: statusFilter }),
    [appointments, query, statusFilter],
  )
  const isFiltered = query.trim() !== '' || statusFilter !== 'all'
  const clearFilters = () => {
    setQuery('')
    setStatusFilter('all')
  }
  const today = health?.today
    ? new Date(`${health.today}T00:00:00`).toLocaleDateString('vi-VN', {
        weekday: 'long', day: '2-digit', month: '2-digit', year: 'numeric',
      })
    : ''

  return (
    <div className="min-h-screen bg-slate-100 text-slate-900 dark:bg-slate-900 dark:text-slate-100">
      <header className="border-b border-slate-200 bg-white dark:border-slate-700 dark:bg-slate-800">
        <div className="mx-auto flex max-w-7xl flex-wrap items-center justify-between gap-3 px-4 py-4 sm:px-6">
          <div>
            <h1 className="text-lg font-semibold">Kiosk Check-in · Lịch hẹn hôm nay</h1>
            <p className="text-sm text-slate-500 dark:text-slate-400">{today || 'Đang tải…'}</p>
          </div>
          <ConnectionPill health={health} apiError={apiError} />
        </div>
      </header>

      <main className="mx-auto max-w-7xl px-4 py-6 sm:px-6">
        {apiError && (
          <div role="alert" className="mb-4 rounded-lg bg-red-50 px-4 py-3 text-sm text-red-700 dark:bg-red-950/50 dark:text-red-300">
            {apiError}. Kiểm tra backend đã chạy chưa (<code className="font-mono">python main.py</code>).
          </div>
        )}

        <div className="mb-6 grid grid-cols-3 gap-3 sm:max-w-xl">
          <Stat label="Tổng lịch hẹn" value={appointments.length} accent="text-slate-900 dark:text-slate-100" />
          <Stat label="Đã check-in" value={checkedIn} accent="text-emerald-600 dark:text-emerald-400" />
          <Stat label="Đang chờ" value={appointments.length - checkedIn} accent="text-amber-600 dark:text-amber-400" />
        </div>

        {/* minmax(0,1fr) + min-w-0: bảng rộng cuộn ngang trong card, không kéo giãn cả trang */}
        <div className="grid grid-cols-1 gap-6 lg:grid-cols-[minmax(0,1fr)_320px]">
          <section className="min-w-0">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-x-4 gap-y-2">
              <div className="flex flex-wrap items-baseline gap-x-3 gap-y-0.5">
                <h2 className="text-base font-semibold">Danh sách lịch hẹn</h2>
                {lastUpdated && (
                  <span className="text-xs text-slate-500 dark:text-slate-400">
                    Cập nhật {lastUpdated.toLocaleTimeString('vi-VN')} · tự làm mới mỗi {REFRESH_MS / 1000}s
                  </span>
                )}
              </div>
              <AppointmentFilters query={query} onQueryChange={setQuery}
                status={statusFilter} onStatusChange={setStatusFilter} counts={counts} />
            </div>
            {loaded ? (
              <>
                <AppointmentTable appointments={visible} onDelete={handleDelete} deletingIds={deletingIds}
                  isFiltered={isFiltered} onClearFilters={clearFilters} />
                {isFiltered && visible.length > 0 && (
                  <p className="mt-2 text-xs text-slate-500 dark:text-slate-400">
                    Hiển thị {visible.length} / {appointments.length} lịch hẹn ·{' '}
                    <button type="button" onClick={clearFilters}
                      className="font-medium text-indigo-600 hover:underline dark:text-indigo-400">
                      Xoá bộ lọc
                    </button>
                  </p>
                )}
              </>
            ) : (
              <div className="rounded-xl border border-slate-200 bg-white p-10 text-center text-sm text-slate-500 dark:border-slate-700 dark:bg-slate-800">
                Đang tải…
              </div>
            )}
          </section>

          <aside>
            <AppointmentForm onCreate={handleCreate} onImport={handleImport} importProgress={importProgress} />
          </aside>
        </div>
      </main>

      <Toaster toasts={toast.toasts} onDismiss={toast.dismiss} />
    </div>
  )
}
