import { useState } from 'react'
import CsvImportButton from './CsvImportButton.jsx'

const EMPTY = { student_id: '', student_name: '', appointment_time: '', message: '', queue_number: '' }

const inputCls =
  'w-full rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm text-slate-900 shadow-sm ' +
  'placeholder:text-slate-400 focus:border-indigo-500 focus:outline-none focus:ring-2 focus:ring-indigo-500/30 ' +
  'dark:border-slate-600 dark:bg-slate-900 dark:text-slate-100'

function Field({ label, hint, required, children }) {
  return (
    <label className="block">
      <span className="mb-1 block text-sm font-medium text-slate-700 dark:text-slate-300">
        {label}
        {/* aria-hidden: input đã có thuộc tính required nên trình đọc màn hình tự báo "bắt buộc" */}
        {required && <span className="ml-0.5 text-red-500" aria-hidden="true">*</span>}
      </span>
      {children}
      {hint && <span className="mt-1 block text-xs text-slate-500 dark:text-slate-400">{hint}</span>}
    </label>
  )
}

export default function AppointmentForm({ onCreate, onImport, importProgress }) {
  const [form, setForm] = useState(EMPTY)
  const [submitting, setSubmitting] = useState(false)
  const [error, setError] = useState('')

  const set = (key) => (e) => setForm((f) => ({ ...f, [key]: e.target.value }))

  async function handleSubmit(e) {
    e.preventDefault()
    setSubmitting(true)
    setError('')
    try {
      const body = {
        student_id: form.student_id.trim(),
        student_name: form.student_name.trim(),
        appointment_time: form.appointment_time,
        message: form.message.trim(),
      }
      if (form.queue_number !== '') body.queue_number = Number(form.queue_number)
      await onCreate(body)
      setForm(EMPTY)
    } catch (err) {
      setError(err.message)
    } finally {
      setSubmitting(false)
    }
  }

  return (
    <form
      onSubmit={handleSubmit}
      className="rounded-xl border border-slate-200 bg-white p-5 shadow-sm dark:border-slate-700 dark:bg-slate-800"
    >
      <div className="mb-4 flex items-start justify-between gap-3">
        <h2 className="pt-1 text-base font-semibold text-slate-900 dark:text-slate-100">Thêm lịch hẹn</h2>
        <CsvImportButton onImport={onImport} progress={importProgress} />
      </div>
      <div className="space-y-3">
        <Field label="MSSV" required>
          <input className={inputCls} value={form.student_id} onChange={set('student_id')}
            required inputMode="numeric" pattern="\d{6,10}" maxLength={10} placeholder="20210001"
            title="6–10 chữ số" />
        </Field>
        <Field label="Tên sinh viên" required>
          <input className={inputCls} value={form.student_name} onChange={set('student_name')}
            required maxLength={60} placeholder="Nguyễn Văn An" />
        </Field>
        <div className="grid grid-cols-2 gap-3">
          <Field label="Giờ hẹn" required>
            <input type="time" className={inputCls} value={form.appointment_time}
              onChange={set('appointment_time')} required />
          </Field>
          <Field label="STT" hint="Trống = tự cấp">
            <input type="number" className={inputCls} value={form.queue_number}
              onChange={set('queue_number')} min={1} max={9999} placeholder="Tự động" />
          </Field>
        </div>
        <Field label="Lời nhắc" hint="Hiện ở dòng cuối OLED (font nhỏ, tự bỏ dấu)">
          <input className={inputCls} value={form.message} onChange={set('message')}
            maxLength={120} placeholder="Vao quay so 3" />
        </Field>
      </div>

      {error && (
        <p role="alert" className="mt-3 rounded-lg bg-red-50 px-3 py-2 text-sm text-red-700 dark:bg-red-950/50 dark:text-red-300">
          {error}
        </p>
      )}

      <button type="submit" disabled={submitting}
        className="mt-4 w-full rounded-lg bg-indigo-600 px-4 py-2 text-sm font-semibold text-white shadow-sm
                   hover:bg-indigo-500 focus:outline-none focus:ring-2 focus:ring-indigo-500/40 disabled:opacity-60">
        {submitting ? 'Đang lưu…' : 'Thêm lịch hẹn'}
      </button>
    </form>
  )
}
