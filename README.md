# Kiosk Check-in — Hệ thống điểm danh & nhắc lịch hẹn IoT

Kiosk tự phục vụ dựa trên ESP32: sinh viên chạm thẻ hoặc quét mã QR, hệ thống xác nhận lịch hẹn trong ngày và hiển thị ngay giờ hẹn, số thứ tự (STT) cùng lời nhắc (ví dụ: "Vào quầy số 3").

| Thành phần | Công nghệ | Vai trò |
|---|---|---|
| [Kiosk_ESP32_Project3/](Kiosk_ESP32_Project3/) | ESP32 DevKit v1, Arduino/PlatformIO, OLED SH1106, RDM6300, MFRC522, QR scanner MH-ET LIVE | Thu thập định danh, gửi lên MQTT, hiển thị kết quả |
| [server/](server/) | Python, paho-mqtt, FastAPI, SQLite | Xử lý lượt quét, check-in lịch hẹn, REST API |
| [web_ui/](web_ui/) | React, Vite, Tailwind | Dashboard quản lý lịch hẹn, nhập CSV, theo dõi check-in |

---

## 1. Bài toán giải quyết

**Hiện trạng.** Ở các điểm tiếp nhận đông người (phòng Công tác sinh viên, phòng đào tạo, quầy một cửa), việc check-in thường làm thủ công: nhân viên tra danh sách giấy hoặc bảng tính, đối chiếu MSSV rồi đọc số thứ tự cho từng người. Cách làm này gây ra:

- Ùn tắc giờ cao điểm do mỗi lượt check-in phụ thuộc vào thao tác của nhân viên.
- Sai sót khi tra cứu, đánh dấu nhầm hoặc trùng người.
- Sinh viên không biết chính xác giờ hẹn, STT và quầy cần đến.
- Ban quản lý không có số liệu theo thời gian thực về số người đã đến.

**Giải pháp.** Một kiosk check-in tự động, kết nối với máy chủ trung tâm:

- **Tự phục vụ:** sinh viên tự check-in trong vài giây. Hệ thống tự cấp quyền (`accepted`) hoặc từ chối (`denied`) dựa trên lịch hẹn trong ngày, mỗi lịch hẹn chỉ được check-in một lần (chuyển trạng thái `pending → checked_in` trong một transaction).
- **Quản lý tập trung:** nhân viên tạo lịch hẹn trên Web UI (nhập từng dòng hoặc từ file CSV). Dashboard tự làm mới mỗi 3 giây để theo dõi trạng thái check-in.

**Một thiết bị, nhiều phương thức định danh.** Kiosk tích hợp ba đầu đọc chạy song song nên sinh viên dùng được loại định danh mình đang có:

| Phương thức | Phần cứng | Giao tiếp | Dữ liệu gửi lên |
|---|---|---|---|
| Thẻ từ 125 kHz (EM4100) | RDM6300 | UART2 (9600 baud) | UID thẻ (chuỗi hex) |
| Thẻ NFC 13.56 MHz (Mifare) | MFRC522 | SPI | UID thẻ (hex, **chỉ đọc**, không ghi lên thẻ) |
| Mã QR thẻ sinh viên HUST | MH-ET LIVE | UART1 (9600 baud) | URL `https://ctsv.hust.edu.vn/#/card/<MSSV>/<Họ_tên>` |
| Mã QR chứa MSSV thuần | MH-ET LIVE | UART1 | Chuỗi 6–10 chữ số |

Backend ánh xạ UID thẻ sang MSSV qua bảng `student_cards`. Mọi phương thức đều quy về cùng một khóa nghiệp vụ là **MSSV**.

**Khóa thiết bị bằng thao tác phần cứng.** Kiosk đặt ở nơi công cộng nên phải chống can thiệp trái phép (đổi WiFi, tắt MQTT, vào cổng cấu hình):

- Sau khi khởi động, thiết bị ở trạng thái **khóa**. Mọi lượt quét thẻ/QR bị loại bỏ, nút cấu hình bị vô hiệu.
- Muốn mở khóa phải nhập đúng **tổ hợp phím trên nút vật lý duy nhất: Double-click → Long-press 2 giây**.
- Màn hình chỉ hiện tiến độ `Mật khẩu: * _`, **không hiển thị gợi ý tổ hợp phím**. Nhập sai thứ tự sẽ bị còi báo lỗi và phải nhập lại từ đầu. Bỏ dở quá 5 giây cũng phải nhập lại.
- Thao tác bấm bắt đầu từ lúc còn màn hình chào không được tính, nên giữ nút từ lúc cấp nguồn cũng không mở được khóa.

---

## 2. Luồng hoạt động của hệ thống

```
[RFID/NFC/QR] ──UART/SPI──> [ESP32] ──MQTT: monitor_student/<id>/{rfid|nfc|qr}──> [Backend] ──> [SQLite]
                              ▲                                                       │
     OLED + còi <─────────────┴────────MQTT: monitor_student/<id>/cmd (verify_result)─┘
                                                                     [Web UI] <──REST /api──┘
```

### Bước 1 — Khởi động & Mở khóa (`STATE_BOOT → STATE_PASSWORD → STATE_IDLE`)

- **BOOT:** nạp cấu hình từ LittleFS, khởi tạo OLED, còi, WiFi, MQTT và ba đầu đọc. Màn hình chào hiển thị 3 giây, còi bíp một tiếng báo khởi động xong.
- **PASSWORD:** chờ nhập tổ hợp phím `DOUBLE_CLICK` rồi `LONG_PRESS_2S`. Trong lúc này WiFi/MQTT vẫn được duy trì để thiết bị sẵn sàng ngay khi mở khóa. Dữ liệu từ các đầu đọc vẫn được đọc ra nhưng bị **xả bỏ**, để lượt quét cũ không tự chạy sau khi mở khóa.
- **IDLE:** nhập đúng thì còi bíp dài, kiosk bắt đầu nhận thẻ/QR và cho phép thao tác menu (chuyển màn hình, bật/tắt WiFi/MQTT, giữ 2 giây để vào chế độ cấu hình WiFi).

### Bước 2 — Thu thập dữ liệu tại biên (Edge)

- **Non-blocking hoàn toàn:** mỗi vòng `loop()` gọi `rfid_update()`, `nfc_update()`, `qr_update()` để đọc phần dữ liệu đang có rồi thoát ngay, không dùng `delay()` chờ thẻ. Còi cũng chạy non-blocking (`buzzerMgr.update()`).
- **Tách frame:** RFID/QR nhận frame theo STX/ETX hoặc CR/LF. QR có thêm timeout 80 ms để chốt frame từ những scanner không gửi ký tự kết thúc, giới hạn 512 ký tự. NFC đọc UID rồi gọi `PICC_HaltA()` để giải phóng thẻ.
- **Lọc trùng nhiều tầng:**
  - Cooldown 2 giây ở mỗi driver đầu đọc.
  - Cửa sổ chống lặp 3 giây **riêng cho từng đầu đọc**, cộng thêm khoảng thời gian `loop()` bị chặn. Thẻ đặt yên trên đầu đọc không bị check-in lặp.
  - Bỏ qua thẻ của lượt đang chờ hoặc đang hiển thị kết quả.
- **Tiền kiểm tra QR:** firmware chỉ chấp nhận URL thẻ HUST hoặc MSSV 6–10 chữ số. QR rác bị loại ngay tại biên, không tốn băng thông.
- **Hàng chờ một chỗ:** nếu thiết bị đang bận (chờ server, hoặc kết quả vừa hiện chưa đủ 2,5 giây), lượt quét mới được giữ lại và bíp xác nhận, rồi tự xử lý khi rảnh.

### Bước 3 — Truyền dẫn (Network)

- Mỗi lượt quét được cấp một **`req_id`** tăng dần, và kết quả của lượt trước bị xóa trước khi gửi.
- Payload được đóng gói JSON (có escape ký tự đặc biệt) và publish theo từng kênh:

  | Topic | Payload |
  |---|---|
  | `monitor_student/<device_id>/rfid` | `{"device_id":"...","uid":"0A1B2C3D4E5F","req_id":7}` |
  | `monitor_student/<device_id>/nfc` | `{"device_id":"...","uid":"04A1B2C3","req_id":8}` |
  | `monitor_student/<device_id>/qr` | `{"device_id":"...","qr_data":"https://ctsv.hust.edu.vn/#/card/...","req_id":9}` |

- Gói quét thẻ luôn gửi với `retained=false`. Buffer MQTT được nới lên 1024 byte để nhận được tên tiếng Việt có dấu (UTF-8).
- Publish thất bại (mất broker) thì báo lỗi **ngay lập tức**. Publish thành công thì chuyển sang `CHECK_WAITING`, hiển thị "Đang kiểm tra…" kèm thanh đếm ngược **timeout 5 giây**.

### Bước 4 — Xử lý tại máy chủ (Server)

- `MqttService` (paho-mqtt, thread nền, tự kết nối lại) subscribe `monitor_student/+/{rfid,nfc,qr,data}` với QoS 1. **Gói retained bị bỏ qua** vì đó là dữ liệu cũ broker phát lại, không phải lượt quét thật.
- `device_id` được lấy từ topic, có cảnh báo hoặc từ chối nếu thiếu tiền tố `toolhub_`.
- **Xác định MSSV:**
  - **QR:** bóc tách MSSV từ URL `https://ctsv.hust.edu.vn/#/card/<MSSV>/...` (URL-decode rồi kiểm tra 6–10 chữ số ASCII), hoặc dùng trực tiếp nếu QR là MSSV thuần.
  - **RFID/NFC:** tra bảng `student_cards` (UID → MSSV). Nếu không có trong bảng nhưng bản thân UID là một MSSV hợp lệ thì dùng luôn.
- **Quyết định cấp quyền** (`models.check_in`, transaction `BEGIN IMMEDIATE` nên hai kiosk quét đồng thời vẫn an toàn):
  - Sinh viên vừa check-in trong 10 phút gần đây → trả lại **đúng lịch hẹn đó**. Quét lại để xem STT không làm mất lịch kế tiếp.
  - Ngược lại → lấy lịch `pending` sớm nhất **hôm nay** và chuyển sang `checked_in`.
  - Không có lịch, thẻ chưa đăng ký, hoặc QR không hợp lệ → `denied` kèm thông điệp tương ứng.

### Bước 5 — Phản hồi (Feedback)

- Backend publish kết quả về `monitor_student/<device_id>/cmd` (QoS 1, `retain=false`, JSON UTF-8 rút gọn), có echo lại `req_id`:

  ```json
  {"action":"verify_result","status":"accepted","mssv":"20210001","name":"Nguyễn Văn An",
   "time":"10:30","queue":5,"msg":"Vao quay so 3","req_id":7}
  ```

- Firmware parse gói trong callback MQTT, rồi **lọc phản hồi lạc**: bỏ gói có `req_id` không khớp lượt hiện tại, hoặc có MSSV khác với MSSV đã đọc từ QR.
- Hiển thị theo kết quả:

  | Kết quả | Màn hình OLED | Còi | Thời gian giữ |
  |---|---|---|---|
  | `accepted` | Họ tên, giờ hẹn, STT (dạng `05`), lời nhắc. Chữ chạy nếu dài | 1 bíp dài (300 ms) | 6–12 giây |
  | `denied` | "Không có lịch hẹn!" kèm lý do | 3 bíp ngắn | ≥ 3 giây |
  | Timeout / mất MQTT | Thông báo lỗi kết nối | 3 bíp ngắn | ≥ 2,5 giây |

- Bấm nút bất kỳ để đóng kết quả sớm. Hết thời gian giữ, thiết bị về `CHECK_IDLE` và vẽ lại màn hình trước đó. Nếu có lượt đang xếp hàng, lượt đó được xử lý ngay.
- Đồng thời, Web UI thấy lịch hẹn chuyển sang trạng thái **đã check-in** ở lần làm mới tiếp theo (≤ 3 giây).
