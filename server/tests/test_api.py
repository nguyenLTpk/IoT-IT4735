"""Kiểm thử REST API:  python -m unittest discover -s tests  (chạy trong server/)"""
import json
import sqlite3
import sys
import tempfile
import threading
import unittest
from contextlib import closing
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import config  # noqa: E402
import models  # noqa: E402
from fastapi.testclient import TestClient  # noqa: E402

import api  # noqa: E402
from mqtt_service import handle_scan_message  # noqa: E402

NEW = {"student_id": "20219999", "student_name": "Đỗ Minh Khoa", "appointment_time": "15:30",
       "message": "Vao quay so 4"}


class ApiTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self._old_db = config.DB_PATH
        config.DB_PATH = Path(self.tmp.name) / "api.db"
        models.seed_sample_data()
        self.client = TestClient(api.app)

    def tearDown(self):
        config.DB_PATH = self._old_db
        self.tmp.cleanup()

    def test_list_today_sorted(self):
        r = self.client.get("/api/appointments")
        self.assertEqual(r.status_code, 200)
        rows = r.json()
        self.assertEqual([a["student_id"] for a in rows], ["20210001", "20210002", "20210003", "20210004"])
        self.assertEqual(set(rows[0]), {"id", "student_id", "student_name", "appointment_date", "appointment_time",
                                        "queue_number", "status", "message", "checked_in_at"})
        self.assertTrue(all(a["appointment_date"] == models.today_str() and a["status"] == "pending" for a in rows))

    def test_list_other_day_and_bad_date(self):
        self.assertEqual(self.client.get("/api/appointments", params={"date": "2000-01-01"}).json(), [])
        self.assertEqual(self.client.get("/api/appointments", params={"date": "hom-nay"}).status_code, 422)

    def test_create_auto_queue_number_and_shows_in_list(self):
        r = self.client.post("/api/appointments", json=NEW)
        self.assertEqual(r.status_code, 201, r.text)
        body = r.json()
        self.assertEqual((body["queue_number"], body["status"], body["student_name"]), (13, "pending", "Đỗ Minh Khoa"))
        ids = [a["id"] for a in self.client.get("/api/appointments").json()]
        self.assertIn(body["id"], ids)

    def test_create_explicit_queue_conflict(self):
        r = self.client.post("/api/appointments", json={**NEW, "queue_number": 5})
        self.assertEqual(r.status_code, 409)
        r = self.client.post("/api/appointments", json={**NEW, "queue_number": 20})
        self.assertEqual((r.status_code, r.json()["queue_number"]), (201, 20))
        # Ngày khác dùng lại STT 5 được
        r = self.client.post("/api/appointments", json={**NEW, "queue_number": 5, "appointment_date": "2030-01-02"})
        self.assertEqual(r.status_code, 201, r.text)

    def test_validation(self):
        bad = [
            {**NEW, "student_id": "abc"},
            {**NEW, "student_id": "123"},
            {**NEW, "appointment_time": "9:30"},     # phải có số 0: 09:30
            {**NEW, "appointment_time": "24:00"},
            {**NEW, "student_name": "   "},
            {**NEW, "student_name": "A" * 61},
            {**NEW, "message": "x" * 121},
            {**NEW, "message": "dong 1\ndong 2"},
            {**NEW, "queue_number": 0},
            {k: v for k, v in NEW.items() if k != "student_name"},
        ]
        for b in bad:
            self.assertEqual(self.client.post("/api/appointments", json=b).status_code, 422, b)

    def test_unicode_digits_rejected(self):
        r = self.client.post("/api/appointments", json={**NEW, "student_id": "２０２１０００１"})
        self.assertEqual(r.status_code, 422)

    def test_fresh_db_works_without_main_py(self):
        # `uvicorn api:app` trên máy mới: lifespan phải tự tạo schema
        config.DB_PATH = Path(self.tmp.name) / "fresh.db"
        with TestClient(api.app) as client:
            self.assertEqual(client.get("/api/appointments").json(), [])
            self.assertEqual(client.post("/api/appointments", json=NEW).status_code, 201)

    def test_whitespace_trimmed(self):
        r = self.client.post("/api/appointments", json={**NEW, "student_id": " 20219999 ", "student_name": "  Khoa  "})
        self.assertEqual((r.status_code, r.json()["student_id"], r.json()["student_name"]), (201, "20219999", "Khoa"))

    def test_delete(self):
        appt_id = self.client.get("/api/appointments").json()[0]["id"]
        self.assertEqual(self.client.delete(f"/api/appointments/{appt_id}").status_code, 204)
        self.assertEqual(self.client.delete(f"/api/appointments/{appt_id}").status_code, 404)
        self.assertNotIn(appt_id, [a["id"] for a in self.client.get("/api/appointments").json()])
        self.assertEqual(self.client.delete("/api/appointments/abc").status_code, 422)

    def test_created_appointment_is_checked_in_by_kiosk_and_status_visible(self):
        created = self.client.post("/api/appointments", json=NEW).json()
        with closing(models.connect()) as conn:
            _, reply = handle_scan_message(conn, "monitor_student/toolhub_k1/qr",
                                           json.dumps({"qr_data": "20219999", "req_id": 9}).encode())
        self.assertEqual((reply["status"], reply["queue"], reply["name"]), ("accepted", 13, "Đỗ Minh Khoa"))
        row = next(a for a in self.client.get("/api/appointments").json() if a["id"] == created["id"])
        self.assertEqual(row["status"], "checked_in")
        self.assertIsNotNone(row["checked_in_at"])

    def test_concurrent_creates_get_distinct_queue_numbers(self):
        numbers, barrier = [], threading.Barrier(6)

        def worker():
            barrier.wait()
            numbers.append(TestClient(api.app).post("/api/appointments", json=NEW).json()["queue_number"])

        ts = [threading.Thread(target=worker) for _ in range(6)]
        for t in ts:
            t.start()
        for t in ts:
            t.join()
        self.assertEqual(sorted(numbers), list(range(13, 19)))

    def test_health(self):
        r = self.client.get("/api/health").json()
        self.assertEqual((r["status"], r["mqtt_enabled"], r["today"]), ("ok", False, models.today_str()))

    def test_cors_for_vite_dev_server(self):
        r = self.client.options("/api/appointments", headers={
            "Origin": "http://localhost:5173", "Access-Control-Request-Method": "POST",
            "Access-Control-Request-Headers": "content-type"})
        self.assertEqual(r.headers.get("access-control-allow-origin"), "http://localhost:5173")

    def test_migration_from_old_schema(self):
        old = Path(self.tmp.name) / "old.db"
        with closing(sqlite3.connect(old)) as c:
            c.executescript("""
                CREATE TABLE appointments (id INTEGER PRIMARY KEY AUTOINCREMENT, student_id TEXT NOT NULL,
                  student_name TEXT NOT NULL, appointment_time TEXT NOT NULL, queue_number INTEGER NOT NULL,
                  status TEXT NOT NULL DEFAULT 'pending', message TEXT NOT NULL DEFAULT '', checked_in_at TEXT);
                INSERT INTO appointments (student_id, student_name, appointment_time, queue_number, message)
                  VALUES ('20210001', 'Cu', '10:00', 1, 'x');""")
            c.commit()
        models.init_db(old)
        models.init_db(old)  # chạy lại lần 2 không lỗi
        with closing(models.connect(old)) as conn:
            rows = models.list_appointments(conn)
        self.assertEqual([(a.student_name, a.appointment_date) for a in rows], [("Cu", models.today_str())])


if __name__ == "__main__":
    unittest.main()
