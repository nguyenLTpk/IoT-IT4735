import { useRef } from 'react'
import { CSV_COLUMNS, CSV_TEMPLATE } from '../csv.js'
import { SpinnerIcon, UploadIcon } from './icons.jsx'

// Nút "Nhập từ CSV": mở hộp chọn file, giao file cho onImport. Trong lúc nhập hiện tiến độ "3/20".
export default function CsvImportButton({ onImport, progress }) {
  const inputRef = useRef(null)
  const busy = progress != null

  function handleChange(e) {
    const file = e.target.files?.[0]
    e.target.value = '' // cho phép chọn lại đúng file đó lần sau (vd. sau khi sửa lỗi)
    if (file) onImport(file)
  }

  function downloadTemplate() {
    // Có BOM để Excel mở đúng tiếng Việt (UTF-8)
    const url = URL.createObjectURL(new Blob(['﻿', CSV_TEMPLATE], { type: 'text/csv;charset=utf-8' }))
    const a = Object.assign(document.createElement('a'), { href: url, download: 'mau-lich-hen.csv' })
    a.click()
    setTimeout(() => URL.revokeObjectURL(url), 1000) // thu hồi ngay có thể làm hỏng lượt tải trên Firefox
  }

  return (
    <div className="flex flex-col items-end gap-1">
      <button type="button" onClick={() => inputRef.current?.click()} disabled={busy}
        title={`File CSV gồm 4 cột: ${CSV_COLUMNS.join(', ')}`}
        className="inline-flex items-center gap-1.5 rounded-lg px-2.5 py-1.5 text-xs font-semibold text-indigo-700
                   ring-1 ring-indigo-200 ring-inset hover:bg-indigo-50 focus:outline-none focus-visible:ring-2
                   focus-visible:ring-indigo-500/50 disabled:cursor-wait disabled:opacity-70
                   dark:text-indigo-300 dark:ring-indigo-800 dark:hover:bg-indigo-500/10">
        {busy ? <SpinnerIcon className="h-4 w-4" /> : <UploadIcon className="h-4 w-4" />}
        {busy ? `Đang nhập ${progress.done}/${progress.total}…` : 'Nhập từ CSV'}
      </button>
      <button type="button" onClick={downloadTemplate}
        className="rounded text-xs text-slate-500 underline-offset-2 hover:text-indigo-600 hover:underline
                   focus:outline-none focus-visible:ring-2 focus-visible:ring-indigo-500/40
                   dark:text-slate-400 dark:hover:text-indigo-400">
        Tải file mẫu
      </button>
      <input ref={inputRef} type="file" accept=".csv,text/csv" onChange={handleChange} className="hidden" tabIndex={-1} />
    </div>
  )
}
