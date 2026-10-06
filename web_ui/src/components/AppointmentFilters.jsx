import { STATUS_FILTERS } from '../filters.js'
import { SearchIcon } from './icons.jsx'

const controlCls =
  'h-9 rounded-lg border border-slate-300 bg-white text-sm text-slate-900 shadow-sm ' +
  'focus:border-indigo-500 focus:outline-none focus:ring-2 focus:ring-indigo-500/30 ' +
  'dark:border-slate-600 dark:bg-slate-800 dark:text-slate-100'

export default function AppointmentFilters({ query, onQueryChange, status, onStatusChange, counts }) {
  return (
    // Điện thoại: xếp chồng 2 dòng (select dài "Chờ check-in (N)" sẽ chiếm hết chỗ của ô tìm kiếm)
    <div className="flex w-full flex-col gap-2 sm:w-auto sm:flex-row">
      <div className="relative min-w-0 sm:w-64">
        <SearchIcon className="pointer-events-none absolute top-1/2 left-3 h-4 w-4 -translate-y-1/2 text-slate-400" />
        <input
          type="search" value={query} onChange={(e) => onQueryChange(e.target.value)}
          placeholder="Tìm MSSV hoặc tên…" aria-label="Tìm theo MSSV hoặc tên sinh viên"
          className={`${controlCls} w-full pr-3 pl-9 placeholder:text-slate-400`}
        />
      </div>
      <select
        value={status} onChange={(e) => onStatusChange(e.target.value)} aria-label="Lọc theo trạng thái"
        className={`${controlCls} w-full shrink-0 cursor-pointer pr-8 pl-3 sm:w-auto`}
      >
        {STATUS_FILTERS.map((f) => (
          <option key={f.value} value={f.value}>{f.label} ({counts[f.value]})</option>
        ))}
      </select>
    </div>
  )
}
