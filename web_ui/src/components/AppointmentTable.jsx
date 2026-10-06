import { SpinnerIcon, TrashIcon } from './icons.jsx'

const STATUS = {
  pending: {
    label: 'Chờ check-in',
    cls: 'bg-amber-100 text-amber-800 ring-amber-600/20 dark:bg-amber-900/40 dark:text-amber-300',
  },
  checked_in: {
    label: 'Đã check-in',
    cls: 'bg-emerald-100 text-emerald-800 ring-emerald-600/20 dark:bg-emerald-900/40 dark:text-emerald-300',
  },
}

function StatusBadge({ status, checkedInAt }) {
  const s = STATUS[status] ?? { label: status, cls: 'bg-slate-100 text-slate-700' }
  const time = checkedInAt ? checkedInAt.slice(11, 16) : null
  return (
    <span className={`inline-flex items-center gap-1 whitespace-nowrap rounded-full px-2.5 py-0.5 text-xs font-medium ring-1 ring-inset ${s.cls}`}
      title={status}>
      {s.label}
      {status === 'checked_in' && time && <span className="opacity-70">· {time}</span>}
    </span>
  )
}

function DeleteButton({ appt, deleting, onDelete }) {
  return (
    <span className="group relative inline-flex">
      <button type="button" onClick={() => onDelete(appt)} disabled={deleting}
        aria-label={`Xoá lịch hẹn của ${appt.student_name}`}
        className="inline-flex h-8 w-8 items-center justify-center rounded-lg text-slate-400 transition-colors
                   hover:bg-red-50 hover:text-red-600 focus:outline-none focus-visible:ring-2 focus-visible:ring-red-500/40
                   disabled:cursor-wait disabled:opacity-60 disabled:hover:bg-transparent
                   dark:text-slate-500 dark:hover:bg-red-500/10 dark:hover:text-red-400">
        {deleting ? <SpinnerIcon className="h-4 w-4" /> : <TrashIcon className="h-[18px] w-[18px]" />}
      </button>
      {/* Tooltip hiện bên TRÁI nút: khung bảng có overflow-x-auto nên tooltip phía trên hàng đầu sẽ bị cắt.
          aria-hidden vì aria-label của nút đã mô tả đầy đủ hơn. */}
      <span aria-hidden="true"
        className="pointer-events-none absolute top-1/2 right-full z-10 mr-2 -translate-y-1/2 whitespace-nowrap rounded-md
                   bg-slate-900 px-2 py-1 text-xs font-medium text-white opacity-0 shadow transition-opacity
                   group-hover:opacity-100 group-hover:delay-300 group-has-[:focus-visible]:opacity-100
                   dark:bg-slate-100 dark:text-slate-900">
        {deleting ? 'Đang xoá…' : 'Xoá lịch hẹn'}
      </span>
    </span>
  )
}

const th = 'whitespace-nowrap px-4 py-3 text-left text-xs font-semibold uppercase tracking-wide text-slate-500 dark:text-slate-400'
const td = 'px-4 py-3 text-sm text-slate-700 dark:text-slate-200'

export default function AppointmentTable({ appointments, onDelete, deletingIds, isFiltered, onClearFilters }) {
  if (appointments.length === 0) {
    return (
      <div className="rounded-xl border border-dashed border-slate-300 p-10 text-center text-sm text-slate-500 dark:border-slate-600 dark:text-slate-400">
        {isFiltered ? (
          <>
            Không có lịch hẹn nào khớp bộ lọc.{' '}
            <button type="button" onClick={onClearFilters}
              className="font-medium text-indigo-600 hover:underline dark:text-indigo-400">
              Xoá bộ lọc
            </button>
          </>
        ) : (
          'Chưa có lịch hẹn nào hôm nay. Thêm lịch hẹn bằng form bên cạnh.'
        )}
      </div>
    )
  }

  return (
    // `relative`: để phần tử sr-only (position:absolute) trong bảng cũng bị cắt theo khung cuộn,
    // không kéo giãn cả trang trên màn hình hẹp
    <div className="relative overflow-x-auto rounded-xl border border-slate-200 bg-white shadow-sm dark:border-slate-700 dark:bg-slate-800">
      <table className="min-w-full divide-y divide-slate-200 dark:divide-slate-700">
        <thead className="bg-slate-50 dark:bg-slate-800/60">
          <tr>
            <th className={th}>MSSV</th>
            <th className={th}>Tên</th>
            <th className={th}>Giờ hẹn</th>
            <th className={`${th} text-right`}>STT</th>
            <th className={th}>Lời nhắc</th>
            <th className={th}>Trạng thái</th>
            <th className={th}><span className="sr-only">Thao tác</span></th>
          </tr>
        </thead>
        <tbody className="divide-y divide-slate-100 dark:divide-slate-700/60">
          {appointments.map((a) => (
            <tr key={a.id} className="transition-colors hover:bg-indigo-50/60 dark:hover:bg-slate-700/40">
              <td className={`${td} font-mono tabular-nums`}>{a.student_id}</td>
              <td className={`${td} min-w-36 font-medium text-slate-900 dark:text-slate-100`}>{a.student_name}</td>
              <td className={`${td} tabular-nums`}>{a.appointment_time}</td>
              <td className={`${td} text-right font-semibold tabular-nums`}>
                {String(a.queue_number).padStart(2, '0')}
              </td>
              <td className={td}>
                {/* Xuống dòng thay vì cắt chữ: điện thoại không hiện tooltip, phải đọc được toàn bộ lời nhắc */}
                <div className="min-w-40 max-w-64 break-words">{a.message || '—'}</div>
              </td>
              <td className={td}><StatusBadge status={a.status} checkedInAt={a.checked_in_at} /></td>
              <td className={`${td} py-2 text-right`}>
                <DeleteButton appt={a} deleting={deletingIds.has(a.id)} onDelete={onDelete} />
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}
