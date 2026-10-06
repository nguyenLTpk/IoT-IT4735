#include "Rfid125khzManager.h"

static String rfidBuffer;
static bool rfidTagReady = false;
static bool rfidInFrame = false;
static unsigned long lastTagTime = 0;

static void finishTagIfValid() {
  if (rfidBuffer.length() > 0) {
    rfidTagReady = true;
  }
}

void rfid_init() {
  rfidBuffer.reserve(32);
  Serial2.begin(RFID_BAUD, SERIAL_8N1, RFID_RX_PIN, RFID_TX_PIN);
  Serial.println("[RFID] Initialized on Serial2");
}

void rfid_update() {
  // Bỏ qua tín hiệu mới nếu chưa đủ 2 giây kể từ lần nhận thẻ trước đó
  if (lastTagTime > 0 && (millis() - lastTagTime < 2000)) {
    while (Serial2.available() > 0) {
      Serial2.read(); // Xóa bộ đệm
    }
    rfidInFrame = false;
    rfidBuffer = "";
    return;
  }

  while (Serial2.available() > 0) {
    char c = (char)Serial2.read();

    // STX: bắt đầu frame RFID (chuẩn phổ biến của RDM6300)
    if (c == 0x02) {
      rfidBuffer = "";
      rfidInFrame = true;
      continue;
    }

    // ETX: kết thúc frame RFID
    if (c == 0x03) {
      if (rfidInFrame) {
        finishTagIfValid();
      }
      rfidInFrame = false;
      continue;
    }

    // Một số module trả dữ liệu theo dòng thay vì STX/ETX.
    // Nếu đang ở trong STX/ETX frame thì KHÔNG finalize theo CR/LF.
    if (c == '\n' || c == '\r') {
      if (!rfidInFrame) {
        finishTagIfValid();
      }
      continue;
    }

    // Lọc chỉ cho phép ký tự in được
    if (isPrintable(c)) {
      // Giới hạn độ dài để tránh tràn
      if (rfidBuffer.length() < 32) {
        rfidBuffer += c;
      }
    }
  }
}

bool rfid_has_new_tag() {
  return rfidTagReady;
}

String rfid_get_last_tag() {
  String tag = rfidBuffer;
  rfidBuffer = "";
  rfidTagReady = false;
  lastTagTime = millis();
  if (lastTagTime == 0) lastTagTime = 1; // Đảm bảo khác 0
  return tag;
}

void rfid_flush() {
  // Xả toàn bộ dữ liệu còn trong bộ đệm Serial2 (tránh đọc lại tag cũ)
  while (Serial2.available() > 0) {
    Serial2.read();
  }
  rfidBuffer = "";
  rfidTagReady = false;
  rfidInFrame = false;
  // Reset cooldown về thời điểm hiện tại để đảm bảo chờ đủ 2s kể từ BÂY GIỜ
  lastTagTime = millis();
  if (lastTagTime == 0) lastTagTime = 1;
}
