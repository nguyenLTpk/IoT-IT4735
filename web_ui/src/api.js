// Gọi REST API của backend FastAPI (đường dẫn tương đối /api -> Vite proxy hoặc chính FastAPI)

async function request(path, options = {}) {
  let res
  try {
    res = await fetch(path, {
      headers: options.body ? { 'Content-Type': 'application/json' } : undefined,
      ...options,
    })
  } catch {
    // Lỗi mạng của trình duyệt ("Failed to fetch"...) -> câu tiếng Việt; không có status
    throw new Error('Không kết nối được máy chủ')
  }
  if (res.status === 204) return null
  const data = await res.json().catch(() => null)
  if (!res.ok) {
    // Đính kèm mã HTTP để nơi gọi phân biệt lỗi dữ liệu (4xx) với lỗi máy chủ (5xx)
    throw Object.assign(new Error(errorMessage(res.status, data)), { status: res.status })
  }
  return data
}

// Chuyển lỗi FastAPI (422 validation / 409 / 404) thành câu tiếng Việt dễ hiểu
const FIELD_LABELS = {
  student_id: 'MSSV',
  student_name: 'Tên',
  appointment_time: 'Giờ hẹn',
  queue_number: 'STT',
  message: 'Lời nhắc',
  appointment_date: 'Ngày',
}

function errorMessage(status, data) {
  const detail = data?.detail
  if (typeof detail === 'string') return detail
  if (Array.isArray(detail)) {
    return detail
      .map((d) => {
        const field = FIELD_LABELS[d.loc?.[d.loc.length - 1]] ?? d.loc?.join('.')
        return `${field}: ${d.msg}`
      })
      .join('; ')
  }
  return `Lỗi máy chủ (HTTP ${status})`
}

export const getAppointments = () => request('/api/appointments')
export const getHealth = () => request('/api/health')
export const createAppointment = (body) =>
  request('/api/appointments', { method: 'POST', body: JSON.stringify(body) })
export const deleteAppointment = (id) => request(`/api/appointments/${id}`, { method: 'DELETE' })
