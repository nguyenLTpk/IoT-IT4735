// Lọc danh sách lịch hẹn theo ô tìm kiếm + trạng thái.

export const STATUS_FILTERS = [
  { value: 'all', label: 'Tất cả' },
  { value: 'pending', label: 'Chờ check-in' },
  { value: 'checked_in', label: 'Đã check-in' },
]

// Bỏ dấu tiếng Việt + chữ thường: gõ "nguyen van an" vẫn tìm ra "Nguyễn Văn An"
export function foldText(s) {
  return s
    .normalize('NFD')
    .replace(/[̀-ͯ]/g, '')
    .replace(/[đĐ]/g, 'd')
    .toLowerCase()
    .replace(/\s+/g, ' ')
    .trim()
}

export function filterAppointments(appointments, { query, status }) {
  const q = foldText(query)
  return appointments.filter(
    (a) =>
      (status === 'all' || a.status === status) &&
      (!q || a.student_id.includes(q) || foldText(a.student_name).includes(q)),
  )
}
