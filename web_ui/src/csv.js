import Papa from 'papaparse'

// Đọc file CSV lịch hẹn: 4 cột theo thứ tự MSSV, Tên, Giờ hẹn, Lời nhắc (dòng tiêu đề là tuỳ chọn).
// Kiểm tra từng dòng theo đúng ràng buộc của backend (api.py: AppointmentIn) để báo lỗi kèm số dòng
// trước khi gửi, thay vì để server trả 422 khó hiểu.
//   -> { rows: [{ line, body }], errors: [{ line, message }] }

export const CSV_COLUMNS = ['MSSV', 'Tên', 'Giờ hẹn', 'Lời nhắc']

export const CSV_TEMPLATE =
  `${CSV_COLUMNS.join(',')}\r\n` +
  '20210001,Nguyễn Văn An,08:30,Vao quay so 3\r\n' +
  '20210002,Trần Thị Bình,09:15,\r\n'

async function readText(file) {
  const buf = await file.arrayBuffer()
  let text
  try {
    text = new TextDecoder('utf-8', { fatal: true }).decode(buf) // tự bỏ BOM nếu có
  } catch {
    // Excel "CSV (Comma delimited)" trên Windows tiếng Việt lưu bằng bảng mã ANSI windows-1258
    text = new TextDecoder('windows-1258').decode(buf)
  }
  return text.normalize('NFC') // windows-1258 ghép dấu thanh bằng ký tự tổ hợp -> gộp lại
}

// Chấp nhận '9:30', '09:30', '09:30:00', '9h30', '9h', '2:30 PM', '2:30 CH' -> 'HH:MM'
const TIME_RE = /^(\d{1,2})\s*(?::\s*(\d{2})|[hH]\s*(\d{2})?)(?::\d{2})?\s*(AM|PM|SA|CH)?$/i

export function normalizeTime(raw) {
  const m = raw.trim().match(TIME_RE)
  if (!m) return null
  let h = Number(m[1])
  const min = Number(m[2] ?? m[3] ?? 0)
  const suffix = m[4]?.toUpperCase()
  if (suffix) {
    if (h < 1 || h > 12) return null
    const pm = suffix === 'PM' || suffix === 'CH'
    if (pm && h !== 12) h += 12
    if (!pm && h === 12) h = 0
  }
  if (h > 23 || min > 59) return null
  return `${String(h).padStart(2, '0')}:${String(min).padStart(2, '0')}`
}

// Backend (api.py: no_control_chars) từ chối xuống dòng/tab... trong tên và lời nhắc.
// Ô Excel có Alt+Enter sẽ chứa xuống dòng -> gộp mọi khoảng trắng/ký tự điều khiển thành 1 dấu cách.
const cleanText = (s) => s.replace(/[\s\x00-\x1f\x7f]+/g, ' ').trim()

function validate(cells, delimiter) {
  const studentId = (cells[0] ?? '').replace(/\s+/g, '')
  const name = cleanText(cells[1] ?? '')
  const rawTime = (cells[2] ?? '').trim()
  // Lời nhắc có dấu phân cách nhưng quên đặt trong ngoặc kép -> bị tách thành nhiều cột; ghép lại
  const message = cleanText(cells.slice(3).join(delimiter))
  const time = rawTime ? normalizeTime(rawTime) : null

  const problems = []
  if (!studentId) problems.push('thiếu MSSV')
  else if (!/^\d{6,10}$/.test(studentId)) problems.push(`MSSV "${studentId}" phải gồm 6–10 chữ số`)
  if (!name) problems.push('thiếu tên sinh viên')
  else if (name.length > 60) problems.push('tên dài quá 60 ký tự')
  if (!rawTime) problems.push('thiếu giờ hẹn')
  else if (!time) problems.push(`giờ hẹn "${rawTime}" không hợp lệ (dùng HH:MM)`)
  if (message.length > 120) problems.push('lời nhắc dài quá 120 ký tự')

  if (problems.length) return { error: problems.join('; ') }
  return { body: { student_id: studentId, student_name: name, appointment_time: time, message } }
}

export async function parseAppointmentsCsv(file) {
  let text = await readText(file)

  // Excel đôi khi ghi dòng đầu "sep=;" để chỉ định dấu phân cách
  let delimiter = ''
  let lineOffset = 1 // số dòng trong file = chỉ số bản ghi + lineOffset
  const sep = text.match(/^sep=(.)\r?\n/i)
  if (sep) {
    delimiter = sep[1]
    text = text.slice(sep[0].length)
    lineOffset = 2
  }

  // Không bỏ dòng trống ở đây để tính đúng số dòng khi báo lỗi; tự lọc ở dưới.
  // delimiter '' = papaparse tự đoán giữa , ; tab | (Excel bản tiếng Việt hay dùng ';')
  const result = Papa.parse(text, { delimiter, skipEmptyLines: false })
  const usedDelimiter = result.meta.delimiter || ','

  // Số dòng thực trong file của từng bản ghi: 1 ô trong ngoặc kép có thể chứa xuống dòng (Alt+Enter
  // trong Excel) nên bản ghi thứ i không nhất thiết nằm ở dòng i.
  const lineOf = []
  let nextLine = lineOffset
  for (const record of result.data) {
    lineOf.push(nextLine)
    nextLine += 1 + record.reduce((n, c) => n + (c.match(/\n/g)?.length ?? 0), 0)
  }

  // Dấu ngoặc kép không đóng làm papaparse gộp MỌI dòng phía sau vào 1 ô. Nhập tiếp sẽ âm thầm làm mất
  // các dòng đó -> từ chối cả file để người dùng sửa rồi nhập lại.
  const quoteError = result.errors.find((e) => e.type === 'Quotes')
  if (quoteError) {
    const line = lineOf[quoteError.row] ?? lineOffset
    return {
      rows: [],
      errors: [{ line, message: 'dấu ngoặc kép (") không đóng hoặc sai vị trí — các dòng sau bị gộp lại, hãy sửa file rồi nhập lại' }],
    }
  }

  const rows = []
  const errors = []
  let headerChecked = false
  result.data.forEach((record, index) => {
    const cells = record.map((c) => c ?? '')
    while (cells.length && cells[cells.length - 1].trim() === '') cells.pop() // cột trống thừa ở cuối
    if (cells.length === 0) return // dòng trống

    // Dòng có dữ liệu đầu tiên là tiêu đề nếu cột 1 không phải số VÀ cột 3 không phải giờ.
    // (Kiểm tra cả cột giờ để 1 dòng dữ liệu có MSSV sai vẫn bị báo lỗi, không bị lặng lẽ bỏ qua.)
    const isFirst = !headerChecked
    headerChecked = true
    if (isFirst && !/^\d+$/.test(cells[0].replace(/\s+/g, '')) && !normalizeTime(cells[2] ?? '')) return

    const line = lineOf[index]
    const { body, error } = validate(cells, usedDelimiter)
    if (error) errors.push({ line, message: error })
    else rows.push({ line, body })
  })

  return { rows, errors }
}
