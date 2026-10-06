"""Kiểm thử backend không cần broker:  python -m unittest discover -s tests  (chạy trong server/)"""
import json
import sqlite3
import sys
import tempfile
import threading
import unittest
from contextlib import closing
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import config  # noqa: E402
import models  # noqa: E402
from mqtt_service import encode_reply, handle_scan_message, student_id_from_qr  # noqa: E402

DEV = "toolhub_kiosk01"


def scan(conn, channel, body, device=DEV):
    raw = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
    return handle_scan_message(conn, f"monitor_student/{device}/{channel}", raw)


class BackendTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.db = Path(self.tmp.name) / "test.db"
        models.seed_sample_data(self.db)
        self.conn = models.connect(self.db)

    def tearDown(self):
        self.conn.close()
        self.tmp.cleanup()

    def status_of(self, student_id):
        return [r["status"] for r in self.conn.execute(
            "SELECT status FROM appointments WHERE student_id = ? ORDER BY id", (student_id,))]

    # ---------------- accepted ----------------
    def test_rfid_card_accepted_exact_format(self):
        topic, reply = scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 123})
        self.assertEqual(topic, f"monitor_student/{DEV}/cmd")
        self.assertEqual(reply, {
            "action": "verify_result", "status": "accepted", "mssv": "20210001",
            "name": "Nguyễn Văn An", "time": "10:30", "queue": 5, "msg": "Vao quay so 3", "req_id": 123,
        })
        self.assertEqual(self.status_of("20210001"), ["checked_in"])

    def test_nfc_card_lowercase_uid(self):
        _, reply = scan(self.conn, "nfc", {"device_id": DEV, "uid": "04a1b2c3", "req_id": 7})
        self.assertEqual((reply["status"], reply["mssv"], reply["queue"]), ("accepted", "20210002", 6))

    def test_qr_plain_student_id(self):
        _, reply = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210003", "req_id": 124})
        self.assertEqual((reply["status"], reply["mssv"], reply["req_id"]), ("accepted", "20210003", 124))

    def test_qr_hust_url_returns_the_same_mssv_the_firmware_expects(self):
        url = "https://ctsv.hust.edu.vn/#/card/20210004/Ph%E1%BA%A1m_Th%E1%BB%8B_Ph%C6%B0%C6%A1ng_Th%E1%BA%A3o"
        _, reply = scan(self.conn, "qr", {"device_id": DEV, "qr_data": url, "req_id": 9})
        self.assertEqual((reply["status"], reply["mssv"], reply["queue"]), ("accepted", "20210004", 12))

    def test_uid_that_is_a_student_id_is_used_directly(self):
        _, reply = scan(self.conn, "rfid", {"device_id": DEV, "uid": "20210002", "req_id": 1})
        self.assertEqual(reply["status"], "accepted")

    # ---------------- denied ----------------
    def test_known_card_without_appointment_denied(self):
        _, reply = scan(self.conn, "nfc", {"device_id": DEV, "uid": "04D5E6F7", "req_id": 123})
        self.assertEqual(reply, {"action": "verify_result", "status": "denied", "mssv": "20210005",
                                 "msg": "Ban khong co lich hen hom nay", "req_id": 123})

    def test_unknown_card_denied_without_mssv(self):
        _, reply = scan(self.conn, "rfid", {"device_id": DEV, "uid": "FFFFFFFFFFFF", "req_id": 5})
        self.assertEqual((reply["status"], reply["mssv"], reply["msg"]), ("denied", "", config.MSG_UNKNOWN_CARD))

    def test_invalid_qr_denied(self):
        _, reply = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "hello", "req_id": 5})
        self.assertEqual((reply["status"], reply["msg"]), ("denied", config.MSG_INVALID_QR))

    # ---------------- check-in rules ----------------
    def test_rescan_shortly_after_check_in_is_accepted_again_without_changing_db(self):
        scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 1})
        before = self.conn.execute("SELECT checked_in_at FROM appointments WHERE student_id='20210001'").fetchone()[0]
        _, reply = scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 2})
        self.assertEqual((reply["status"], reply["queue"], reply["req_id"]), ("accepted", 5, 2))
        after = self.conn.execute("SELECT checked_in_at FROM appointments WHERE student_id='20210001'").fetchone()[0]
        self.assertEqual(before, after)

    def test_rescan_after_window_is_denied(self):
        scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 1})
        old = (datetime.now() - timedelta(minutes=config.RECHECK_WINDOW_MINUTES + 1)).isoformat(timespec="seconds")
        self.conn.execute("UPDATE appointments SET checked_in_at = ? WHERE student_id = '20210001'", (old,))
        _, reply = scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 2})
        self.assertEqual(reply["status"], "denied")

    def test_earliest_pending_appointment_first(self):
        models.create_appointment(self.conn, "20210001", "Nguyễn Văn An", "09:00", "Quay so 1", queue_number=2)
        _, reply = scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 1})
        self.assertEqual((reply["time"], reply["queue"]), ("09:00", 2))
        self.assertEqual(sorted(self.status_of("20210001")), ["checked_in", "pending"])
        # Quẹt lại (vd. sau khi thiết bị timeout) -> hiển thị lại 09:00, KHÔNG check-in luôn lịch 10:30
        _, again = scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 2})
        self.assertEqual((again["status"], again["time"], again["queue"]), ("accepted", "09:00", 2))
        self.assertEqual(sorted(self.status_of("20210001")), ["checked_in", "pending"])
        # Hết cửa sổ quẹt lại -> lịch kế tiếp mới được check-in
        old = (datetime.now() - timedelta(minutes=config.RECHECK_WINDOW_MINUTES + 1)).isoformat(timespec="seconds")
        self.conn.execute("UPDATE appointments SET checked_in_at = ? WHERE status = 'checked_in'", (old,))
        _, nxt = scan(self.conn, "rfid", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 3})
        self.assertEqual((nxt["status"], nxt["time"]), ("accepted", "10:30"))

    def test_two_kiosks_at_once_check_in_only_once(self):
        results, barrier = [], threading.Barrier(8)

        def worker(i):
            with closing(models.connect(self.db)) as c:
                barrier.wait()
                results.append(scan(c, "rfid", {"device_id": f"toolhub_k{i}", "uid": "0A1B2C3D4E5F",
                                                "req_id": i + 1}, device=f"toolhub_k{i}"))

        threads = [threading.Thread(target=worker, args=(i,)) for i in range(8)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(len(results), 8)
        self.assertTrue(all(r[1]["status"] == "accepted" for r in results))
        self.assertEqual(self.status_of("20210001"), ["checked_in"])
        self.assertEqual({r[0] for r in results}, {f"monitor_student/toolhub_k{i}/cmd" for i in range(8)})

    # ---------------- protocol details ----------------
    def test_req_id_echo_rules(self):
        _, r = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210003"})
        self.assertNotIn("req_id", r)
        for bad in (True, "12", -1, 0, 1.5):
            _, r = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210003", "req_id": bad})
            self.assertNotIn("req_id", r, bad)
        _, r = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210003", "req_id": 4294967295})
        self.assertEqual(r["req_id"], 4294967295)

    def test_reply_goes_to_topic_device_id(self):
        topic, _ = scan(self.conn, "qr", {"device_id": "something_else", "qr_data": "20210003"}, device="toolhub_A")
        self.assertEqual(topic, "monitor_student/toolhub_A/cmd")

    def test_ignored_messages(self):
        self.assertIsNone(scan(self.conn, "data", b"23.5,40.1"))                      # dữ liệu CSV
        self.assertIsNone(scan(self.conn, "data", b"\x00\x01\x02\x03"))               # dữ liệu nhị phân
        self.assertIsNone(scan(self.conn, "data", {"temp": 30}))                      # JSON khác
        self.assertIsNone(scan(self.conn, "cmd", {"uid": "0A1B2C3D4E5F"}))            # kênh downstream
        self.assertIsNone(handle_scan_message(self.conn, "startup", b"a,b,c"))
        self.assertIsNone(handle_scan_message(self.conn, "other/toolhub_x/rfid", b'{"uid":"1"}'))
        self.assertIsNone(scan(self.conn, "rfid", b"[1,2]"))
        self.assertEqual(self.status_of("20210001"), ["pending"])

    def test_data_channel_scan_is_accepted_for_compatibility(self):
        _, r = scan(self.conn, "data", {"device_id": DEV, "uid": "0A1B2C3D4E5F", "req_id": 3})
        self.assertEqual(r["status"], "accepted")

    def test_device_prefix_enforcement(self):
        old = config.REQUIRE_DEVICE_PREFIX
        try:
            config.REQUIRE_DEVICE_PREFIX = True
            self.assertIsNone(scan(self.conn, "qr", {"qr_data": "20210003"}, device="dev01"))
            self.assertIsNotNone(scan(self.conn, "qr", {"qr_data": "20210003"}, device="toolhub_1"))
        finally:
            config.REQUIRE_DEVICE_PREFIX = old

    def test_encoded_reply_is_raw_utf8_and_fits_the_device_buffer(self):
        _, r = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210004", "req_id": 4294967295})
        data = encode_reply(r)
        self.assertIn("Phạm Thị Phương Thảo".encode("utf-8"), data)
        self.assertNotIn(b"\\u", data)
        topic = f"monitor_student/{DEV}/cmd"
        # PubSubClient: header (<=5) + 2 + len(topic) + payload phải <= MQTT_BUFFER_SIZE (1024)
        self.assertLess(5 + 2 + len(topic) + len(data), 1024)

    def test_only_todays_appointments_can_be_checked_in(self):
        self.conn.execute("UPDATE appointments SET appointment_date = '2000-01-01' WHERE student_id = '20210003'")
        _, r = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210003", "req_id": 1})
        self.assertEqual((r["status"], r["msg"]), ("denied", config.MSG_NO_APPOINTMENT))
        models.create_appointment(self.conn, "20210003", "Lê Hoàng Cường", "16:00", "Quay so 2")
        _, r = scan(self.conn, "qr", {"device_id": DEV, "qr_data": "20210003", "req_id": 2})
        self.assertEqual((r["status"], r["time"]), ("accepted", "16:00"))

    def test_qr_parser(self):
        self.assertEqual(student_id_from_qr(" 20210001 "), "20210001")
        self.assertEqual(student_id_from_qr("https://ctsv.hust.edu.vn/#/card/20210001/A_B?x=1"), "20210001")
        self.assertIsNone(student_id_from_qr("https://ctsv.hust.edu.vn/#/card/abc/Name"))
        self.assertIsNone(student_id_from_qr("12345"))
        self.assertIsNone(student_id_from_qr("２０２１０００１"))  # chữ số Unicode toàn chiều rộng


if __name__ == "__main__":
    unittest.main()
