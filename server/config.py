"""Cấu hình backend - đọc từ biến môi trường, mặc định khớp với firmware ESP32.

Giá trị mặc định lấy từ src/MqttManager.h (MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS
và tiền tố topic "monitor_student/").
"""
import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent

# --- MQTT broker (giống firmware) ---
MQTT_HOST = os.getenv("MQTT_HOST", "mqtt.toolhub.app")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "demo")
MQTT_PASS = os.getenv("MQTT_PASS", "demo")
MQTT_KEEPALIVE = int(os.getenv("MQTT_KEEPALIVE", "30"))

# Tiền tố topic: firmware dùng "monitor_student/<device_id>/<kênh>"
TOPIC_PREFIX = os.getenv("TOPIC_PREFIX", "monitor_student")

# Các kênh Upstream mà firmware publish dữ liệu quét thẻ:
#   rfid / nfc  -> {"device_id":"...","uid":"...","req_id":N}
#   qr          -> {"device_id":"...","qr_data":"...","req_id":N}
#   data        -> kênh dữ liệu chung (nhận thêm cho tương thích, bỏ qua gói không phải quét thẻ)
UPSTREAM_CHANNELS = ("rfid", "nfc", "qr", "data")
DOWNSTREAM_CHANNEL = "cmd"

# Thiết bị kiosk có device_id bắt đầu bằng tiền tố này. Thiết bị khác vẫn được phục vụ
# nhưng có cảnh báo trong log (đặt REQUIRE_DEVICE_PREFIX=1 để từ chối hẳn).
DEVICE_ID_PREFIX = os.getenv("DEVICE_ID_PREFIX", "toolhub_")
REQUIRE_DEVICE_PREFIX = os.getenv("REQUIRE_DEVICE_PREFIX", "0") == "1"

# --- REST API (FastAPI) ---
# Mặc định chỉ nghe trên máy này (API chưa có đăng nhập). Đặt API_HOST=0.0.0.0 để máy khác
# trong mạng LAN truy cập - chỉ làm vậy trong mạng tin cậy.
API_HOST = os.getenv("API_HOST", "127.0.0.1")
API_PORT = int(os.getenv("API_PORT", "8000"))
# Origin được phép gọi API từ trình duyệt (Vite dev server). Phân tách bằng dấu phẩy.
API_CORS_ORIGINS = [o.strip() for o in os.getenv(
    "API_CORS_ORIGINS", "http://localhost:5173,http://127.0.0.1:5173").split(",") if o.strip()]
# Thư mục build của Web UI (npm run build). Nếu tồn tại, API phục vụ luôn giao diện tại "/".
WEB_UI_DIST = Path(os.getenv("WEB_UI_DIST", str(BASE_DIR.parent / "web_ui" / "dist")))

# --- Database ---
DB_PATH = Path(os.getenv("DB_PATH", str(BASE_DIR / "kiosk.db")))

# Sinh viên quét lại trong khoảng này sau khi đã check-in (vd. thiết bị bị timeout rồi
# gửi lại) vẫn nhận "accepted" với cùng lịch hẹn, thay vì bị báo "không có lịch".
RECHECK_WINDOW_MINUTES = int(os.getenv("RECHECK_WINDOW_MINUTES", "10"))

# --- Nội dung thông báo gửi xuống OLED (font ASCII nhỏ -> nên viết không dấu) ---
MSG_NO_APPOINTMENT = "Ban khong co lich hen hom nay"
MSG_UNKNOWN_CARD = "The chua duoc dang ky"
MSG_INVALID_QR = "Ma QR khong hop le"
