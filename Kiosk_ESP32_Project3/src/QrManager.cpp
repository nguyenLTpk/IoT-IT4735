#include "QrManager.h"

#if ENABLE_QR_SCANNER

// Dùng UART1 cho module QR để tách biệt khỏi luồng Serial debug và RFID (Serial2)
static HardwareSerial qrSerial(1);
static String qrFrameBuffer;
static String qrLastPayload;
static bool qrPayloadReady = false;
static unsigned long qrLastCharMs = 0;

static const size_t QR_MAX_LEN = 512;
static const unsigned long QR_FRAME_TIMEOUT_MS = 80;

static void finalizeQrFrame() {
	if (qrFrameBuffer.length() > 0) {
		qrLastPayload = qrFrameBuffer;
		qrPayloadReady = true;
		qrFrameBuffer = "";
	}
}

void qr_init() {
	qrFrameBuffer.reserve(256);
	qrLastPayload.reserve(256);
	qrSerial.begin(QR_BAUD, SERIAL_8N1, QR_RX_PIN, QR_TX_PIN);
	Serial.println("[QR] Scanner initialized on UART1");
}

void qr_update() {
	// Một số scanner không gửi ký tự kết thúc ổn định, timeout giúp chốt frame đang dở.
	if (qrFrameBuffer.length() > 0 && qrLastCharMs > 0 && (millis() - qrLastCharMs) > QR_FRAME_TIMEOUT_MS) {
		finalizeQrFrame();
	}

	while (qrSerial.available() > 0) {
		const char c = (char)qrSerial.read();
		qrLastCharMs = millis();

		// STX/ETX là khung phổ biến trên một số module scanner TTL.
		if (c == 0x02) {
			qrFrameBuffer = "";
			continue;
		}

		if (c == 0x03) {
			finalizeQrFrame();
			continue;
		}

		// Phần lớn scanner TTL gửi chuỗi kết thúc bằng CR/LF sau mỗi lần quét
		if (c == '\n' || c == '\r') {
			finalizeQrFrame();
			continue;
		}

		if (isPrintable(c) && qrFrameBuffer.length() < QR_MAX_LEN) {
			qrFrameBuffer += c;
		}
	}
}

bool qr_has_new_payload() {
	return qrPayloadReady;
}

String qr_get_last_payload() {
	String payload = qrLastPayload;
	payload.trim();

	qrLastPayload = "";
	qrPayloadReady = false;

	return payload;
}

#else

void qr_init() {}
void qr_update() {}
bool qr_has_new_payload() { return false; }
String qr_get_last_payload() { return String(); }

#endif
