"""Tầng dữ liệu SQLite cho kiosk check-in / nhắc lịch hẹn.

Bảng:
  appointments  - Lịch hẹn: student_id, student_name, appointment_date ('YYYY-MM-DD'),
                  appointment_time ('10:30'), queue_number (int),
                  status ('pending' | 'checked_in'), message.
  student_cards - Ánh xạ UID thẻ RFID/NFC -> MSSV (thay cho hàm dummy).

Chạy trực tiếp để tạo DB và nạp dữ liệu mẫu:
    python models.py --seed
"""
import argparse
import sqlite3
from contextlib import closing
from dataclasses import asdict, dataclass
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import List, Optional

import config

STATUS_PENDING = "pending"
STATUS_CHECKED_IN = "checked_in"

SCHEMA = """
CREATE TABLE IF NOT EXISTS appointments (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id       TEXT    NOT NULL,
    student_name     TEXT    NOT NULL,
    appointment_date TEXT    NOT NULL DEFAULT '',   -- 'YYYY-MM-DD'
    appointment_time TEXT    NOT NULL,              -- 'HH:MM', vd '10:30'
    queue_number     INTEGER NOT NULL,
    status           TEXT    NOT NULL DEFAULT 'pending'
                     CHECK (status IN ('pending', 'checked_in')),
    message          TEXT    NOT NULL DEFAULT '',
    checked_in_at    TEXT                            -- ISO time khi check-in (NULL nếu chưa)
);
CREATE INDEX IF NOT EXISTS idx_appointments_student ON appointments (student_id, status);

CREATE TABLE IF NOT EXISTS student_cards (
    card_uid   TEXT PRIMARY KEY,                     -- UID thẻ (chữ hoa), vd '04A1B2C3'
    student_id TEXT NOT NULL
);
"""


class QueueNumberTaken(Exception):
    """STT đã được dùng cho một lịch hẹn khác trong cùng ngày."""


@dataclass
class Appointment:
    id: int
    student_id: str
    student_name: str
    appointment_date: str
    appointment_time: str
    queue_number: int
    status: str
    message: str
    checked_in_at: Optional[str] = None
    # Không lưu DB: True nếu đây là lần quét lại của lịch đã check-in trước đó
    already_checked_in: bool = False

    @classmethod
    def from_row(cls, row: sqlite3.Row) -> "Appointment":
        return cls(**{k: row[k] for k in row.keys()})

    def to_dict(self) -> dict:
        data = asdict(self)
        data.pop("already_checked_in")
        return data


def today_str() -> str:
    """Ngày hôm nay theo giờ máy chủ, dạng 'YYYY-MM-DD'."""
    return date.today().isoformat()


def connect(db_path: Optional[Path] = None) -> sqlite3.Connection:
    """Mở kết nối mới cho mỗi lần xử lý (gói MQTT / request API) - an toàn giữa các thread."""
    conn = sqlite3.connect(str(db_path or config.DB_PATH), timeout=10, isolation_level=None,
                           check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn


def init_db(db_path: Optional[Path] = None, reset: bool = False) -> None:
    with closing(connect(db_path)) as conn:
        if reset:
            conn.executescript("DROP TABLE IF EXISTS appointments; DROP TABLE IF EXISTS student_cards;")
        conn.executescript(SCHEMA)
        # Migration: DB tạo bởi phiên bản trước chưa có cột appointment_date.
        # Gán ngày hôm nay cho các dòng cũ để chúng vẫn hiện trên Dashboard và vẫn check-in được.
        columns = {r["name"] for r in conn.execute("PRAGMA table_info(appointments)")}
        if "appointment_date" not in columns:
            conn.execute("ALTER TABLE appointments ADD COLUMN appointment_date TEXT NOT NULL DEFAULT ''")
        conn.execute("UPDATE appointments SET appointment_date = ? WHERE appointment_date = ''", (today_str(),))
        conn.execute("CREATE INDEX IF NOT EXISTS idx_appointments_date ON appointments (appointment_date)")


SAMPLE_APPOINTMENTS = [
    # student_id, student_name, appointment_time, queue_number, message
    ("20210001", "Nguyễn Văn An", "10:30", 5, "Vao quay so 3"),
    ("20210002", "Trần Thị Bình", "10:45", 6, "Vao quay so 1"),
    ("20210003", "Lê Hoàng Cường", "11:00", 7, "Vao quay so 2"),
    ("20210004", "Phạm Thị Phương Thảo", "14:15", 12, "Mang theo the sinh vien, quay so 3 tang 2"),
]

SAMPLE_CARDS = [
    # card_uid, student_id  (RDM6300 gửi 12 ký tự hex gồm checksum; MFRC522 gửi 8 ký tự hex)
    ("0A1B2C3D4E5F", "20210001"),
    ("04A1B2C3", "20210002"),
    ("1122334455AA", "20210003"),
    ("04D5E6F7", "20210005"),  # Sinh viên có thẻ nhưng KHÔNG có lịch hẹn -> denied
]


def seed_sample_data(db_path: Optional[Path] = None) -> None:
    """Xoá dữ liệu cũ và nạp dữ liệu mẫu cho HÔM NAY (mọi lịch hẹn ở trạng thái 'pending')."""
    init_db(db_path)
    today = today_str()
    with closing(connect(db_path)) as conn:
        conn.execute("BEGIN")
        conn.execute("DELETE FROM appointments")
        conn.execute("DELETE FROM student_cards")
        conn.executemany(
            "INSERT INTO appointments (student_id, student_name, appointment_date, appointment_time,"
            " queue_number, message) VALUES (?, ?, ?, ?, ?, ?)",
            [(sid, name, today, t, q, msg) for sid, name, t, q, msg in SAMPLE_APPOINTMENTS],
        )
        conn.executemany("INSERT INTO student_cards (card_uid, student_id) VALUES (?, ?)", SAMPLE_CARDS)
        conn.execute("COMMIT")


def student_id_for_card(conn: sqlite3.Connection, card_uid: str) -> Optional[str]:
    row = conn.execute(
        "SELECT student_id FROM student_cards WHERE card_uid = ?", (card_uid.strip().upper(),)
    ).fetchone()
    return row["student_id"] if row else None


def check_in(conn: sqlite3.Connection, student_id: str,
             recheck_window_minutes: int = config.RECHECK_WINDOW_MINUTES) -> Optional[Appointment]:
    """Check-in lịch hẹn 'pending' sớm nhất HÔM NAY của sinh viên, trả về lịch hẹn đó.

    - Thực hiện trong 1 transaction BEGIN IMMEDIATE: hai kiosk quét cùng lúc cũng chỉ
      có 1 lần chuyển pending -> checked_in.
    - Nếu sinh viên vừa check-in trong recheck_window_minutes (thiết bị timeout rồi gửi lại,
      hoặc quẹt lại để xem STT), trả về ĐÚNG lịch hẹn đó để hiển thị lại - kiểm tra TRƯỚC khi
      xét lịch 'pending', để lần quét lại không "tiêu" mất lịch hẹn kế tiếp của sinh viên.
    - Trả về None nếu không có lịch hẹn hợp lệ.
    """
    now = datetime.now()
    today = now.date().isoformat()
    now_iso = now.isoformat(timespec="seconds")
    since = (now - timedelta(minutes=recheck_window_minutes)).isoformat(timespec="seconds")
    result: Optional[Appointment] = None

    conn.execute("BEGIN IMMEDIATE")
    try:
        row = conn.execute(
            "SELECT * FROM appointments WHERE student_id = ? AND status = ? AND checked_in_at >= ?"
            " ORDER BY checked_in_at DESC, id DESC LIMIT 1",
            (student_id, STATUS_CHECKED_IN, since),
        ).fetchone()
        if row is not None:
            result = Appointment.from_row(row)
            result.already_checked_in = True
        else:
            row = conn.execute(
                "SELECT * FROM appointments WHERE student_id = ? AND status = ? AND appointment_date = ?"
                " ORDER BY appointment_time, queue_number, id LIMIT 1",
                (student_id, STATUS_PENDING, today),
            ).fetchone()
            if row is not None:
                conn.execute(
                    "UPDATE appointments SET status = ?, checked_in_at = ? WHERE id = ?",
                    (STATUS_CHECKED_IN, now_iso, row["id"]),
                )
                result = Appointment.from_row(row)
                result.status = STATUS_CHECKED_IN
                result.checked_in_at = now_iso
    except Exception:
        conn.execute("ROLLBACK")
        raise
    conn.execute("COMMIT")
    return result


def list_appointments(conn: sqlite3.Connection, day: Optional[str] = None) -> List[Appointment]:
    """Lịch hẹn của 1 ngày (mặc định hôm nay), sắp theo giờ rồi STT."""
    rows = conn.execute(
        "SELECT * FROM appointments WHERE appointment_date = ? ORDER BY appointment_time, queue_number, id",
        (day or today_str(),),
    ).fetchall()
    return [Appointment.from_row(r) for r in rows]


def create_appointment(conn: sqlite3.Connection, student_id: str, student_name: str,
                       appointment_time: str, message: str = "", queue_number: Optional[int] = None,
                       day: Optional[str] = None) -> Appointment:
    """Thêm lịch hẹn. STT để trống -> tự cấp STT lớn nhất trong ngày + 1.

    Raises QueueNumberTaken nếu STT đã có trong ngày đó.
    """
    day = day or today_str()
    conn.execute("BEGIN IMMEDIATE")  # Khoá ghi: 2 request cùng lúc không thể lấy trùng STT
    try:
        if queue_number is None:
            row = conn.execute("SELECT COALESCE(MAX(queue_number), 0) + 1 AS q FROM appointments"
                               " WHERE appointment_date = ?", (day,)).fetchone()
            queue_number = row["q"]
        elif conn.execute("SELECT 1 FROM appointments WHERE appointment_date = ? AND queue_number = ?",
                          (day, queue_number)).fetchone():
            raise QueueNumberTaken(queue_number)
        cur = conn.execute(
            "INSERT INTO appointments (student_id, student_name, appointment_date, appointment_time,"
            " queue_number, message) VALUES (?, ?, ?, ?, ?, ?)",
            (student_id, student_name, day, appointment_time, queue_number, message),
        )
        row = conn.execute("SELECT * FROM appointments WHERE id = ?", (cur.lastrowid,)).fetchone()
    except Exception:
        conn.execute("ROLLBACK")
        raise
    conn.execute("COMMIT")
    return Appointment.from_row(row)


def delete_appointment(conn: sqlite3.Connection, appointment_id: int) -> bool:
    """Xoá lịch hẹn theo id. Trả về False nếu không tồn tại."""
    return conn.execute("DELETE FROM appointments WHERE id = ?", (appointment_id,)).rowcount > 0


if __name__ == "__main__":
    import sys

    # Console / output chuyển hướng trên Windows có thể không phải UTF-8 -> tránh crash khi in tên có dấu
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    parser = argparse.ArgumentParser(description="Khởi tạo / nạp dữ liệu mẫu cho kiosk.db")
    parser.add_argument("--seed", action="store_true", help="xoá dữ liệu cũ và nạp dữ liệu mẫu cho hôm nay")
    parser.add_argument("--reset", action="store_true", help="xoá bảng và tạo lại schema")
    args = parser.parse_args()

    init_db(reset=args.reset)
    if args.seed:
        seed_sample_data()
    with closing(connect()) as c:
        for a in list_appointments(c):
            print(f"{a.student_id}  {a.appointment_time}  STT {a.queue_number:>3}  {a.status:<10}  {a.student_name}")
    print(f"DB: {config.DB_PATH}  (ngay {today_str()})")
