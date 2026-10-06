"""Giao tiếp MQTT với kiosk ESP32.

Luồng xử lý 1 lần quét:
  ESP32 --(monitor_student/<device_id>/rfid|nfc|qr)--> backend
        payload: {"device_id":"...","uid":"..."|"qr_data":"...","req_id":N}
  backend: xác định MSSV -> check-in lịch hẹn 'pending' trong SQLite
  backend --(monitor_student/<device_id>/cmd)--> ESP32
        {"action":"verify_result","status":"accepted"|"denied", ...,"req_id":N}

Phần xử lý được tách thành hàm thuần handle_scan_message() để kiểm thử không cần broker.
"""
import json
import logging
import re
import uuid
from contextlib import closing
from typing import Optional, Tuple
from urllib.parse import unquote_plus

import paho.mqtt.client as mqtt

import config
import models

log = logging.getLogger("kiosk.mqtt")

# QR thẻ sinh viên HUST mà firmware gửi nguyên văn (xem parseHustCardUrl trong main.cpp)
HUST_CARD_PREFIX = "https://ctsv.hust.edu.vn/#/card/"
# MSSV thuần: 6-10 chữ số (giống isPlainStudentId trong firmware)
PLAIN_STUDENT_ID = re.compile(r"^[0-9]{6,10}$")  # chỉ chữ số ASCII (\d của Python nhận cả '２')


def parse_topic(topic: str) -> Optional[Tuple[str, str]]:
    """'monitor_student/<device_id>/<channel>' -> (device_id, channel); None nếu không khớp."""
    parts = topic.split("/")
    if len(parts) != 3 or parts[0] != config.TOPIC_PREFIX or not parts[1]:
        return None
    return parts[1], parts[2]


def student_id_from_qr(qr_data: str) -> Optional[str]:
    """Lấy MSSV từ nội dung QR: URL thẻ HUST hoặc MSSV thuần. None nếu không hợp lệ."""
    qr = qr_data.strip()
    if qr.startswith(HUST_CARD_PREFIX):
        tail = qr[len(HUST_CARD_PREFIX):]
        student_id = unquote_plus(tail.split("/", 1)[0]).strip()
        return student_id if PLAIN_STUDENT_ID.match(student_id) else None
    return qr if PLAIN_STUDENT_ID.match(qr) else None


def student_id_from_card(conn, card_uid: str) -> Optional[str]:
    """Ánh xạ UID thẻ RFID/NFC -> MSSV qua bảng student_cards.

    Nếu UID không có trong bảng nhưng bản thân nó là một MSSV (thẻ được ghi sẵn MSSV)
    thì dùng luôn - tương đương hàm 'dummy' coi uid chính là student_id.
    """
    student_id = models.student_id_for_card(conn, card_uid)
    if student_id:
        return student_id
    uid = card_uid.strip()
    return uid if PLAIN_STUDENT_ID.match(uid) else None


def build_reply(status: str, req_id, mssv: str = "", appointment: Optional[models.Appointment] = None,
                msg: str = "") -> dict:
    reply = {"action": "verify_result", "status": status, "mssv": mssv}
    if appointment is not None:
        reply.update({
            "name": appointment.student_name,
            "time": appointment.appointment_time,
            "queue": appointment.queue_number,   # số nguyên; firmware tự hiển thị "05"
            "msg": appointment.message,
        })
    else:
        reply["msg"] = msg
    if req_id is not None:
        reply["req_id"] = req_id                 # trả lại đúng req_id thiết bị gửi lên
    return reply


def handle_scan_message(conn, topic: str, payload: bytes) -> Optional[Tuple[str, dict]]:
    """Xử lý 1 gói Upstream. Trả về (topic_trả_lời, nội_dung_JSON) hoặc None nếu bỏ qua."""
    parsed = parse_topic(topic)
    if parsed is None:
        return None
    topic_device_id, channel = parsed
    if channel not in config.UPSTREAM_CHANNELS:
        return None

    try:
        data = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        if channel != "data":  # kênh data còn chở dữ liệu CSV/nhị phân khác -> bỏ qua im lặng
            log.warning("Bo qua goi khong phai JSON tren %s", topic)
        return None
    if not isinstance(data, dict) or ("uid" not in data and "qr_data" not in data):
        return None  # Không phải gói quét thẻ

    # Trả lời theo device_id trên TOPIC: thiết bị subscribe đúng monitor_student/<deviceID>/cmd
    device_id = topic_device_id
    payload_device_id = str(data.get("device_id", ""))
    if payload_device_id and payload_device_id != device_id:
        log.warning("device_id trong payload (%s) khac topic (%s) - tra loi theo topic",
                    payload_device_id, device_id)
    if config.DEVICE_ID_PREFIX and not device_id.startswith(config.DEVICE_ID_PREFIX):
        if config.REQUIRE_DEVICE_PREFIX:
            log.warning("Tu choi thiet bi la: %s", device_id)
            return None
        log.warning("device_id %s khong co tien to %s", device_id, config.DEVICE_ID_PREFIX)

    req_id = data.get("req_id")
    if not (isinstance(req_id, int) and not isinstance(req_id, bool) and req_id > 0):
        req_id = None  # Firmware chỉ chấp nhận số nguyên dương; thiếu/sai -> không gửi req_id

    reply_topic = f"{config.TOPIC_PREFIX}/{device_id}/{config.DOWNSTREAM_CHANNEL}"

    # 1. Xác định MSSV
    if "qr_data" in data:
        raw = str(data.get("qr_data", ""))
        student_id = student_id_from_qr(raw)
        if student_id is None:
            return reply_topic, build_reply("denied", req_id, msg=config.MSG_INVALID_QR)
    else:
        raw = str(data.get("uid", ""))
        student_id = student_id_from_card(conn, raw)
        if student_id is None:
            log.info("[%s] The la uid=%s", device_id, raw)
            return reply_topic, build_reply("denied", req_id, msg=config.MSG_UNKNOWN_CARD)

    # 2. Check-in lịch hẹn
    appointment = models.check_in(conn, student_id)
    if appointment is None:
        log.info("[%s] %s: khong co lich hen", device_id, student_id)
        return reply_topic, build_reply("denied", req_id, mssv=student_id, msg=config.MSG_NO_APPOINTMENT)

    log.info("[%s] %s (%s) %s: %s STT %s", device_id, student_id, appointment.student_name,
             "da check-in truoc do, hien thi lai" if appointment.already_checked_in else "check-in",
             appointment.appointment_time, appointment.queue_number)
    return reply_topic, build_reply("accepted", req_id, mssv=student_id, appointment=appointment)


def encode_reply(reply: dict) -> bytes:
    # UTF-8 thật (không \uXXXX) và không khoảng trắng -> gói nhỏ, vừa buffer 1024 byte của firmware
    return json.dumps(reply, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


class MqttService:
    """Kết nối broker, subscribe các topic Upstream và trả lời từng lần quét."""

    def __init__(self, db_path=None):
        self.db_path = db_path
        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=f"monitor_student-backend-{uuid.uuid4().hex[:8]}",
        )
        if config.MQTT_USER:
            self.client.username_pw_set(config.MQTT_USER, config.MQTT_PASS)
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            log.error("Ket noi broker that bai: %s", reason_code)
            return
        # Subscribe lại mỗi lần (re)connect
        topics = [(f"{config.TOPIC_PREFIX}/+/{ch}", 1) for ch in config.UPSTREAM_CHANNELS]
        client.subscribe(topics)
        log.info("Da ket noi %s:%s, lang nghe: %s", config.MQTT_HOST, config.MQTT_PORT,
                 ", ".join(t for t, _ in topics))

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        log.warning("Mat ket noi broker (%s), dang tu ket noi lai...", reason_code)

    def _on_message(self, client, userdata, msg):
        # Gói retained là dữ liệu cũ broker phát lại mỗi lần subscribe (khởi động / kết nối lại),
        # KHÔNG phải lần quét thật (firmware luôn publish quét thẻ với retained=false).
        if msg.retain:
            log.warning("Bo qua goi retained tren %s (khong phai lan quet that)", msg.topic)
            return
        try:
            with closing(models.connect(self.db_path)) as conn:
                result = handle_scan_message(conn, msg.topic, msg.payload)
        except Exception:
            log.exception("Loi xu ly goi tin tren %s", msg.topic)
            return
        if result is None:
            return
        reply_topic, reply = result
        # retain=False: thiết bị kết nối lại không được nhận kết quả cũ
        client.publish(reply_topic, encode_reply(reply), qos=1, retain=False)
        log.debug("-> %s %s", reply_topic, reply)

    @property
    def is_connected(self) -> bool:
        return self.client.is_connected()

    def start(self):
        """Chạy MQTT trong thread nền của paho (tự kết nối lại) - dùng khi chạy chung với API."""
        log.info("Dang ket noi MQTT broker %s:%s ...", config.MQTT_HOST, config.MQTT_PORT)
        self.client.connect_async(config.MQTT_HOST, config.MQTT_PORT, keepalive=config.MQTT_KEEPALIVE)
        self.client.loop_start()

    def run_forever(self):
        """Chạy MQTT chặn luồng hiện tại (khi không bật API)."""
        log.info("Dang ket noi MQTT broker %s:%s ...", config.MQTT_HOST, config.MQTT_PORT)
        self.client.connect_async(config.MQTT_HOST, config.MQTT_PORT, keepalive=config.MQTT_KEEPALIVE)
        self.client.loop_forever(retry_first_connection=True)

    def stop(self):
        self.client.disconnect()
        self.client.loop_stop()
