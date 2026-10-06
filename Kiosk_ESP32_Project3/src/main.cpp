#include <Arduino.h>
#include "HardwareConfig.h"

// Core modules (always included)
#include "ApiManager.h"
#include "ConfigManager.h"
#include "main.h"

// Display module
#if ENABLE_OLED_DISPLAY

// Màn hình và I2C
#include "OledBackdrop.h" // Giao diện chào trên màn hình điện tử
#include <U8g2lib.h>
#include <Wire.h>

#endif

// Communication modules
#if ENABLE_WIFI
#include "WifiManager.h" // Quản lý wifi
#endif

#if ENABLE_MQTT
#include "MqttManager.h" // Gửi dữ liệu lên MQTT mặc định mqtt.toolhub.app
#endif

// Input modules
#if ENABLE_CONFIG_BUTTON
#include "ButtonGestures.h" // Quản lý các hình thái bấm của 1 nút button
#endif

// Output modules
#if ENABLE_BUZZER
#include "BuzzerManager.h" // Quản lý Loa Buzzer
#endif

// RFID/NFC modules
#if ENABLE_RFID_125KHZ
#include "Rfid125khzManager.h" // Quản lý RFID 125kHz
#endif

#if ENABLE_NFC_MFRC522
#include "NfcManager.h" // Quản lý MFRC522 13.56MHz
#endif

#include "ApiManager.h"        // Request HTTPS kiểm tra MSSV với Google Sheet
#include "ButtonGestures.h"    // Quản lý các hình thái bấm của 1 nút button
#include "BuzzerManager.h"     // Quản lý Loa Buzzer
#include "MqttManager.h"       // Gửi dữ liệu lên MQTT mặc định mqtt.toolhub.app
#include "NfcManager.h"        // Quản lý MFRC522 13.56MHz
#include "OledBackdrop.h"      // Giao diện chào trên màn hình điện tử
#include "QrManager.h"         // Quản lý QR scanner qua UART
#include "Rfid125khzManager.h" // Quản lý RFID 125kHz
#include "WifiManager.h"       // Quản lý wifi
#include "main.h"              // Thông tin dev


// --- KHÔNG CÒN SỬ DỤNG MODULE BỤI SDS011 ---

// LED status indicator
// LED_BUILTIN is defined in HardwareConfig.h

// --- GLOBAL OBJECTS (conditionally defined based on features) ---

// OLED Display
#if ENABLE_OLED_DISPLAY
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

// Configuration button
#if ENABLE_CONFIG_BUTTON
// Chân nút cấu hình / nhập mật khẩu (mặc định = CFG_BUTTON_PIN trong HardwareConfig.h)
#ifndef BUTTON_PIN
#define BUTTON_PIN CFG_BUTTON_PIN
#endif
ButtonGesture configBtn(BUTTON_PIN);
#endif

// Chọn tên font mới
#define VIETNAMESE_FONT u8g2_font_unifont_t_vietnamese2

// --- Biến toàn cục chứa mode hoạt động --

#define MODE_WELCOME 0
#define MODE_INFO 1
#define MODE_SETTINGS 2
#define MODE_MQTT 3
#define MODE_NUM 4
char g_mode;

void renderCurrentMode(); // Khai báo để dùng trong setup()

uint8_t settingCursorIndex = 0;
#define MAX_SETTINGS_ITEM 2

// ============================================================================
// [MỚI] STATE MACHINE KHOÁ THIẾT BỊ BẰNG MẬT KHẨU NÚT BẤM (NON-BLOCKING)
// ----------------------------------------------------------------------------
//   STATE_BOOT ──(màn hình chào hiển thị đủ BOOT_SCREEN_MS)──> STATE_PASSWORD
//   STATE_PASSWORD ──(DOUBLE_CLICK rồi LONG_PRESS_2S)───────> STATE_IDLE
//   STATE_PASSWORD: sai thao tác / quá PASSWORD_TIMEOUT_MS không thao tác -> passwordStep = 0
//   STATE_IDLE: kiosk hoạt động bình thường (quét thẻ, chuyển mode, cấu hình WiFi...)
//
// Khi chưa mở khoá, WiFi/MQTT vẫn được duy trì để sẵn sàng ngay khi mở khoá, nhưng thẻ/QR
// quét vào bị bỏ qua (vẫn đọc ra để xả bộ đệm, tránh lượt quét cũ tự chạy sau khi mở khoá).
// ============================================================================

enum AppState : uint8_t {
  STATE_BOOT = 0, // Màn hình chào
  STATE_PASSWORD, // Chờ nhập mật khẩu bằng nút bấm
  STATE_IDLE      // Đã mở khoá: hoạt động bình thường
};

static const unsigned long BOOT_SCREEN_MS = 3000;      // Thời gian hiển thị màn hình chào
static const unsigned long PASSWORD_TIMEOUT_MS = 5000; // Không thao tác quá lâu -> nhập lại
static const unsigned long UNLOCKED_SCREEN_MS = 800;   // Giữ "Mật khẩu: * *" cho kịp nhìn thấy

static AppState g_appState = STATE_BOOT;
static unsigned long g_bootStartMs = 0;          // Thời điểm màn hình chào bắt đầu hiển thị
static int passwordStep = 0;                     // 0: chưa nhập; 1: đã DOUBLE_CLICK, chờ LONG_PRESS_2S
static unsigned long g_passwordLastInputMs = 0;  // Lần thao tác nút gần nhất ở STATE_PASSWORD
static unsigned long g_unlockedAtMs = 0;         // Thời điểm mở khoá
static bool g_idleScreenPending = false;         // Chưa vẽ màn hình mode lần đầu sau khi mở khoá
static unsigned long g_lastBtnLowMs = 0;         // Lần gần nhất thấy nút đang được nhấn
static bool g_ignoreBootGesture = false;         // Bỏ thao tác bắt đầu từ lúc còn màn hình chào
// Lớn hơn _doubleClickDelay (300ms) của ButtonGesture: sau khoảng lặng này chắc chắn không còn
// cú bấm nào của màn hình chào đang chờ được báo ra
static const unsigned long BOOT_GESTURE_QUIET_MS = 400;

// ============================================================================
// [MỚI] STATE MACHINE KIỂM TRA THẺ / NHẮC LỊCH HẸN (NON-BLOCKING)
// ----------------------------------------------------------------------------
// Trước đây handleCardCheck()/handleQrCardCheck() dùng
//     while (millis() - startWait < 5000) { mqttMgr.loop(); delay(10); }
// rồi thêm delay(2000) để giữ kết quả -> loop() bị treo tới ~7 giây: không đọc
// được nút bấm, QR/RFID/NFC dồn bộ đệm, WiFi không được duy trì.
//
// Nay tách thành 3 trạng thái, mỗi vòng loop() chỉ kiểm tra điều kiện rồi thoát ngay:
//
//   CHECK_IDLE ──(quét thẻ hợp lệ + publish OK)──> CHECK_WAITING
//   CHECK_IDLE ──(publish lỗi: mất MQTT)─────────> CHECK_SHOWING_RESULT (màn hình lỗi)
//   CHECK_WAITING ──(nhận verify_result)─────────> CHECK_SHOWING_RESULT (lịch hẹn / từ chối)
//   CHECK_WAITING ──(quá MQTT_RESPONSE_TIMEOUT_MS)> CHECK_SHOWING_RESULT (màn hình timeout)
//   CHECK_SHOWING_RESULT ──(hết giờ / bấm nút)───> CHECK_IDLE (vẽ lại màn hình theo g_mode)
//   CHECK_SHOWING_RESULT ──(quét thẻ KHÁC, kết quả đã hiện ≥ 2.5s)──> bắt đầu lượt mới ngay
//
// Hàng chờ 1 chỗ (g_pendingScan): thẻ/mã KHÁC quét lúc máy đang bận (đang chờ MQTT,
// màn hình timeout, hoặc kết quả vừa hiện chưa đủ 2.5s) không bị bỏ mất mà được bíp
// xác nhận, giữ lại và tự động kiểm tra khi kết quả của sinh viên trước đã hiển thị đủ lâu.
// ============================================================================

enum CheckState : uint8_t {
  CHECK_IDLE = 0,       // Rảnh: màn hình hiển thị theo g_mode, sẵn sàng nhận thẻ
  CHECK_WAITING,        // Đã publish lên MQTT, đang chờ gói verify_result
  CHECK_SHOWING_RESULT  // Đang hiển thị kết quả (lịch hẹn / từ chối / lỗi)
};

enum ScanSource : uint8_t { SRC_RFID = 0, SRC_NFC, SRC_QR };

/** Lần quét đang xếp hàng chờ máy rảnh */
struct PendingScan {
  bool valid;
  ScanSource source;
  String data;              // UID thẻ hoặc payload QR gốc
  String key;               // Khoá chống lặp, ví dụ "RFID:0A1B2C3D4E"
  unsigned long queuedMs;
};

static const unsigned long MQTT_RESPONSE_TIMEOUT_MS = 5000;  // Thời gian chờ server (như bản cũ)
static const unsigned long RESULT_ACCEPTED_MIN_MS = 6000;    // Lịch hẹn 4 dòng -> cho đọc lâu hơn
static const unsigned long RESULT_MAX_MS = 12000;            // Trần thời gian khi tên/msg phải chạy chữ
static const unsigned long RESULT_DENIED_MS = 3000;
static const unsigned long RESULT_ERROR_MS = 2500;
static const unsigned long RESULT_SCROLL_MARGIN_MS = 1500;   // Thêm sau 1 vòng chữ chạy
// Chống quét lặp: RDM6300 phát liên tục khi thẻ vẫn nằm trên đầu đọc (mỗi ~2s sau
// cooldown, cộng thời gian còi kêu). Bỏ qua cùng 1 thẻ/mã nếu vẫn còn thấy nó trong cửa sổ này.
static const unsigned long DUPLICATE_SCAN_WINDOW_MS = 3000;
// Kết quả của sinh viên trước được hiển thị tối thiểu bao lâu trước khi kiểm tra lượt đang xếp hàng
static const unsigned long PENDING_MIN_RESULT_MS = 2500;
static const unsigned long PENDING_MAX_AGE_MS = 20000;       // Lượt xếp hàng quá cũ -> bỏ

static CheckState g_checkState = CHECK_IDLE;
static unsigned long g_checkStateSinceMs = 0; // Thời điểm vào trạng thái hiện tại
static unsigned long g_resultDisplayMs = 0;   // Thời gian giữ màn hình kết quả hiện tại
static bool g_resultDefersNewScans = false;   // Màn hình timeout: hoãn lượt mới (phản hồi cũ có thể đến muộn)
static uint32_t g_checkReqId = 0;             // Mã lượt kiểm tra (gửi kèm "req_id")
static String g_checkExpectedMssv;            // MSSV biết trước (QR) để lọc phản hồi lạc
static String g_checkFallbackName;            // Tên parse từ QR, dùng khi backend không trả name
static String g_lastScanKey;                  // Thẻ/mã của lượt kiểm tra gần nhất
static bool g_lastCheckFailed = false;        // Lượt trước lỗi (timeout/mất MQTT)
static PendingScan g_pendingScan;             // Hàng chờ 1 chỗ (valid=false khi trống)
// Bộ nhớ chống lặp RIÊNG cho từng đầu đọc (RFID / NFC / QR): mỗi đầu đọc chỉ có thể
// đang giữ 1 thẻ/mã, nên lượt của sinh viên khác không làm "quên" thẻ còn nằm trên đầu đọc.
static String g_seenKey[3];
static unsigned long g_seenMs[3] = {0, 0, 0};
// Mốc đầu vòng loop() hiện tại và vòng trước: thời gian loop() bị chặn (MQTT reconnect,
// còi...) được cộng thêm vào cửa sổ chống lặp, vì khi đó đầu đọc không thể báo lại thẻ.
static unsigned long g_loopStartMs = 0;
static unsigned long g_prevLoopStartMs = 0;

static String urlDecode(const String &encoded) {
  String out;
  out.reserve(encoded.length());

  for (size_t i = 0; i < encoded.length(); i++) {
    const char c = encoded.charAt(i);
    if (c == '%' && (i + 2) < encoded.length()) {
      const char h1 = encoded.charAt(i + 1);
      const char h2 = encoded.charAt(i + 2);

      auto hexVal = [](char x) -> int {
        if (x >= '0' && x <= '9')
          return x - '0';
        if (x >= 'A' && x <= 'F')
          return x - 'A' + 10;
        if (x >= 'a' && x <= 'f')
          return x - 'a' + 10;
        return -1;
      };

      const int v1 = hexVal(h1);
      const int v2 = hexVal(h2);
      if (v1 >= 0 && v2 >= 0) {
        out += (char)((v1 << 4) | v2);
        i += 2;
        continue;
      }
    }

    out += (c == '+') ? ' ' : c;
  }

  return out;
}

static String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    const char c = input.charAt(i);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else {
      out += c;
    }
  }

  return out;
}

static bool parseHustCardUrl(const String &qrPayload, String &studentIdOut,
                             String &fullNameOut) {
  const String marker = "https://ctsv.hust.edu.vn/#/card/";
  if (!qrPayload.startsWith(marker))
    return false;

  String tail = qrPayload.substring(marker.length());
  tail.trim();

  const int firstSlash = tail.indexOf('/');
  if (firstSlash <= 0)
    return false;

  String studentId = tail.substring(0, firstSlash);
  String fullNameRaw = tail.substring(firstSlash + 1);

  // Chỉ lấy phần tên, bỏ các token hoặc query phía sau nếu có.
  const int nextSlash = fullNameRaw.indexOf('/');
  if (nextSlash >= 0) {
    fullNameRaw = fullNameRaw.substring(0, nextSlash);
  }
  const int queryPos = fullNameRaw.indexOf('?');
  if (queryPos >= 0) {
    fullNameRaw = fullNameRaw.substring(0, queryPos);
  }

  studentId = urlDecode(studentId);
  fullNameRaw = urlDecode(fullNameRaw);
  fullNameRaw.replace("_", " ");

  studentId.trim();
  fullNameRaw.trim();

  if (studentId.length() == 0 || fullNameRaw.length() == 0) {
    return false;
  }

  // MSSV HUST thường là dãy số, kiểm tra để tránh nhận nhầm QR URL khác.
  for (size_t i = 0; i < studentId.length(); i++) {
    if (!isDigit(studentId.charAt(i)))
      return false;
  }

  studentIdOut = studentId;
  fullNameOut = fullNameRaw;
  return true;
}

/**
 * @brief Kiểm tra xem payload QR có phải là mã sinh viên thuần (chỉ chứa số,
 *        6–10 ký tự — phổ biến nhất là 8 chữ số tại HUST).
 * @return true nếu payload hợp lệ dạng MSSV thuần.
 */
static bool isPlainStudentId(const String &qrPayload) {
  const size_t len = qrPayload.length();
  if (len < 6 || len > 10)
    return false;

  for (size_t i = 0; i < len; i++) {
    if (!isDigit(qrPayload.charAt(i)))
      return false;
  }
  return true;
}

/**
 * [MỚI] Nhận dạng QR hợp lệ (dùng chung cho lần quét mới và lượt lấy ra từ hàng chờ).
 *   Kiểu 1 — URL thẻ HUST:  https://ctsv.hust.edu.vn/#/card/MSSV/Ten
 *   Kiểu 2 — MSSV thuần:    chuỗi 6-10 chữ số (ví dụ: 20210001)
 */
static bool parseQrPayload(const String &qrPayload, bool &isHustUrl, String &studentIdOut,
                           String &nameOut) {
  isHustUrl = parseHustCardUrl(qrPayload, studentIdOut, nameOut);
  if (isHustUrl)
    return true;

  String trimmed = qrPayload;
  trimmed.trim();
  if (isPlainStudentId(trimmed)) {
    studentIdOut = trimmed;
    nameOut = ""; // Chưa biết tên — sẽ lấy từ backend
    return true;
  }
  return false;
}

static bool parseHustCardUrlToJson(const String &qrPayload, String &jsonOut) {
  String studentId;
  String fullName;
  if (!parseHustCardUrl(qrPayload, studentId, fullName))
    return false;

  const String studentIdEsc = jsonEscape(studentId);
  const String fullNameEsc = jsonEscape(fullName);
  const String rawUrlEsc = jsonEscape(qrPayload);

  jsonOut = "{";
  jsonOut += "\"type\":\"hust_card\",";
  jsonOut += "\"student_id\":\"" + studentIdEsc + "\",";
  jsonOut += "\"full_name\":\"" + fullNameEsc + "\",";
  jsonOut += "\"raw_url\":\"" + rawUrlEsc + "\"";
  jsonOut += "}";

  return true;
}

// --------------------------------------------------------
// HÀM CHÍNH: setup()
// --------------------------------------------------------
void setup() {
  // 1. Serial MẶC ĐỊNH (cho Debug/PC)
  Serial.begin(115200);
  Serial.println("\nStart Monitor Student Device...");

  // 2. Lấy cấu hình từ Flash
  if (configMgr.begin()) {
    configMgr.loadAll(); // Toàn bộ thông số từ LittleFS đã nằm trong
                         // configMgr.params
  }

  // 3. Cấu hình OSD
  u8g2.begin();
  u8g2.setFont(VIETNAMESE_FONT);

  // 4. Màn hinh chào - [MỚI] bắt đầu STATE_BOOT. Đếm 3 giây từ lúc màn hình chào xuất hiện;
  //    các bước khởi tạo bên dưới (WiFi...) chạy trong lúc màn hình chào đang hiển thị.
  showWelcomeScreen(u8g2);
  g_appState = STATE_BOOT;
  g_bootStartMs = millis();

  // 6. Led mặc định  LED_BUILTIN = D4
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // 7. Initialize Config Button
#if ENABLE_CONFIG_BUTTON
  configBtn.begin();
#endif

  // 8. Initialize Buzzer
#if ENABLE_BUZZER
  buzzerMgr.begin();
#endif
  // [THAY ĐỔI] Tiếng bíp báo boot xong được dời xuống ngay trước vòng chờ bên dưới:
  // còi giờ là non-blocking, nếu bíp ở đây thì các bước khởi tạo blocking phía sau
  // (WiFi, MFRC522 PCD_Init có delay(50)...) sẽ kéo dài tiếng bíp 80ms thành 150-300ms.

  // 9. Mode hoạt động đầu tiên
  g_mode = MODE_WELCOME;

  // 9. Bắt đầu khởi tạo internet
  if (configMgr.params.wifiEnabled) {
    // Kết nối lần đầu
    CheckAndEstablishWiFiConnection();
  }

  // [THAY ĐỔI] Luôn gọi mqttMgr.setup() (trước đây chỉ gọi khi WiFi bật lúc boot).
  // setup() luôn đặt server/callback/topic/buffer rồi mới kiểm tra WiFi; nếu WiFi chưa
  // sẵn sàng nó tự return và mqttMgr.loop() sẽ kết nối sau. Nhờ vậy khi thiết bị khởi
  // động với WiFi TẮT rồi bật lại trong Settings, MQTT vẫn có cấu hình để nhận verify_result.
  mqttMgr.setup();

  // Khởi tạo RFID 125kHz
  rfid_init();

  // Khởi tạo module NFC 13.56MHz (MFRC522)
  nfc_init();

  // Khởi tạo module QR scanner UART (MH-ET LIVE)
  qr_init();

  buzzerMgr.beepShort(); // Còi bíp 1 tiếng báo hiệu boot xong

  // [THAY ĐỔI] Bỏ vòng chờ 5 giây ở đây: thời gian giữ màn hình chào giờ do STATE_BOOT trong
  // loop() đảm nhận (non-blocking, buzzerMgr.update() vẫn chạy mỗi vòng nên tiếng bíp tự tắt).
  // Màn hình của g_mode chỉ được vẽ sau khi nhập đúng mật khẩu (xem enterIdleState()).
}

// --------------------------------------------------------
// HÀM CHÍNH: loop()
// --------------------------------------------------------
void renderCurrentMode() {
  // Màn hình mode đã (hoặc sẽ, qua finishCheck()) được vẽ -> không cần lần vẽ đầu sau mở khoá nữa
  g_idleScreenPending = false;

  // [MỚI] Đang kiểm tra thẻ / hiển thị kết quả -> KHÔNG vẽ đè màn hình của State Machine.
  // Mode mới (nếu người dùng bấm nút trong lúc chờ) vẫn được ghi nhận vào g_mode và sẽ
  // được vẽ ngay khi State Machine quay về CHECK_IDLE (xem finishCheck()).
  if (g_checkState != CHECK_IDLE)
    return;

  if (g_mode == MODE_WELCOME) {
    showWelcomeScreen(u8g2);
  } else if (g_mode == MODE_INFO) {
    showFlashConfig(
        u8g2, (mqttMgr.connected() ? "MQTT: Connected" : "MQTT: Disconnected"));
  } else if (g_mode == MODE_SETTINGS) {
    showSettingsPage(u8g2, settingCursorIndex);
  } else if (g_mode == MODE_MQTT) {
    showMqttConfig(u8g2);
  }
}

// ============================================================================
// [MỚI] CÁC HÀM HỖ TRỢ STATE MACHINE
// ============================================================================

static void startCardCheck(ScanSource source, const String &tag, bool ackBeep);
static void startQrCheck(const String &qrPayload, bool ackBeep);

static void enterCheckState(CheckState state) {
  g_checkState = state;
  g_checkStateSinceMs = millis();
}

/** Kết thúc lượt kiểm tra: về IDLE và vẽ lại màn hình của mode hiện tại. */
static void finishCheck() {
  stopOledAnimations();
  g_checkExpectedMssv = "";
  g_checkFallbackName = "";
  g_resultDefersNewScans = false;
  enterCheckState(CHECK_IDLE);
  renderCurrentMode();
}

/** Huỷ lượt kiểm tra đang dở + hàng chờ (không vẽ lại) - dùng trước khi vào cấu hình WiFi. */
static void abortCheck() {
  g_pendingScan.valid = false;
  if (g_checkState == CHECK_IDLE)
    return;
  Serial.println("[CHECK] Huy luot kiem tra dang do.");
  stopOledAnimations();
  mqttClearVerifyResult();
  g_checkExpectedMssv = "";
  g_checkFallbackName = "";
  g_resultDefersNewScans = false;
  enterCheckState(CHECK_IDLE);
}

/**
 * Thời gian giữ màn hình kết quả: tối thiểu baseMs, kéo dài thêm nếu tên/msg phải
 * chạy chữ để sinh viên kịp đọc hết 1 vòng (gọi SAU khi đã vẽ layout).
 */
static unsigned long resultHoldMs(unsigned long baseMs) {
  unsigned long ms = getOledAnimationCycleMs();
  ms = (ms > 0) ? ms + RESULT_SCROLL_MARGIN_MS : 0;
  if (ms < baseMs)
    ms = baseMs;
  if (ms > RESULT_MAX_MS)
    ms = RESULT_MAX_MS;
  return ms;
}

/** Giữ màn hình kết quả vừa vẽ trong durationMs rồi tự quay về IDLE. */
static void holdResultScreen(unsigned long durationMs, bool defersNewScans = false) {
  g_resultDisplayMs = durationMs;
  g_resultDefersNewScans = defersNewScans;
  enterCheckState(CHECK_SHOWING_RESULT);
}

/**
 * Hiển thị màn hình lỗi + còi báo lỗi.
 * @param defersNewScans true với lỗi timeout: phản hồi của lượt vừa rồi vẫn có thể đến muộn,
 *        nên lượt quét mới được xếp hàng tới khi màn hình lỗi kết thúc (tránh gán nhầm kết quả).
 */
static void showCheckError(const char *title, const char *detail, bool defersNewScans) {
  g_lastCheckFailed = true;
  showCheckErrorLayout(u8g2, title, detail);
  buzzerMgr.beepError(); // Non-blocking: còi tự kêu 3 nhịp nhờ buzzerMgr.update()
  holdResultScreen(resultHoldMs(RESULT_ERROR_MS), defersNewScans);
}

static String makeScanKey(ScanSource source, const String &id) {
  const char *prefix = (source == SRC_RFID) ? "RFID:" : (source == SRC_NFC) ? "NFC:" : "QR:";
  return String(prefix) + id;
}

/**
 * Quyết định xử lý 1 lần quét: true = kiểm tra ngay; false = bỏ qua hoặc đã xếp hàng.
 * 1. Thẻ/mã vẫn nằm trên CÙNG đầu đọc (thấy lại trong DUPLICATE_SCAN_WINDOW_MS): bỏ qua.
 *    Mỗi lần thấy đều "trượt" cửa sổ, kể cả khi bị bỏ qua / xếp hàng / lượt trước lỗi,
 *    nên thẻ đặt yên không bao giờ bị check-in lặp hay kêu lặp.
 * 2. Cùng thẻ/mã với lượt gần nhất: bỏ qua nếu đang chờ chính nó, hoặc kết quả của nó
 *    đang hiển thị (trừ khi lượt đó lỗi -> quẹt lại để thử lại).
 * 3. Đang bận (chờ MQTT / màn hình timeout / kết quả mới hiện chưa đủ PENDING_MIN_RESULT_MS /
 *    đã có người xếp hàng): xếp vào hàng chờ 1 chỗ và bíp xác nhận; hàng chờ đầy thì kêu lỗi.
 */
static bool acceptNewScan(const String &scanKey, ScanSource source, const String &data) {
  const unsigned long now = millis();

  // 1. Chống lặp theo từng đầu đọc. Cửa sổ được nới thêm đúng khoảng thời gian loop()
  //    bị chặn kể từ đầu vòng trước (vd. client.connect() treo 1-3s khi mất broker):
  //    thẻ nằm yên được đọc lại ngay sau khi hết chặn, không bị coi là lần quét mới.
  const uint8_t reader = (uint8_t)source;
  const unsigned long stallAllowanceMs = now - g_prevLoopStartMs;
  const bool stillOnReader =
      (scanKey == g_seenKey[reader]) &&
      (now - g_seenMs[reader] < DUPLICATE_SCAN_WINDOW_MS + stallAllowanceMs);
  g_seenKey[reader] = scanKey;
  g_seenMs[reader] = now;
  if (stillOnReader) {
    Serial.println("[CHECK] Quet trung lap (the/ma van con tren dau doc), bo qua.");
    return false;
  }

  // 2. Thẻ/mã của lượt gần nhất
  if (scanKey == g_lastScanKey) {
    if (g_checkState == CHECK_WAITING) {
      Serial.println("[CHECK] Dang kiem tra chinh the/ma nay, bo qua.");
      return false;
    }
    if (g_checkState == CHECK_SHOWING_RESULT && !g_lastCheckFailed) {
      Serial.println("[CHECK] Ket qua cua the/ma nay dang hien thi, bo qua.");
      return false;
    }
  }

  if (g_pendingScan.valid && scanKey == g_pendingScan.key) {
    return false; // Đã nằm trong hàng chờ
  }

  // 3. Máy đang bận -> xếp hàng. Kết quả vừa hiện được giữ tối thiểu PENDING_MIN_RESULT_MS
  //    để sinh viên trước kịp đọc giờ hẹn / STT (trước đây lượt mới đè ngay lập tức).
  unsigned long minShowMs = PENDING_MIN_RESULT_MS;
  if (minShowMs > g_resultDisplayMs)
    minShowMs = g_resultDisplayMs;
  const bool resultTooFresh = (g_checkState == CHECK_SHOWING_RESULT) &&
                              (g_resultDefersNewScans || now - g_checkStateSinceMs < minShowMs);
  const bool busy = (g_checkState == CHECK_WAITING) || g_pendingScan.valid || resultTooFresh;
  if (busy) {
    if (!g_pendingScan.valid) {
      g_pendingScan.valid = true;
      g_pendingScan.source = source;
      g_pendingScan.data = data;
      g_pendingScan.key = scanKey;
      g_pendingScan.queuedMs = now;
      Serial.print("[CHECK] May dang ban, da xep hang: ");
      Serial.println(scanKey);
      buzzerMgr.beepShort(); // Xác nhận đã nhận thẻ, sinh viên không cần quét lại
    } else {
      Serial.print("[CHECK] Hang cho da day, bo qua: ");
      Serial.println(scanKey);
      // Kêu lỗi để sinh viên quét lại sau. Thẻ đặt yên chỉ kêu 1 lần nhờ bước 1.
      buzzerMgr.beepError();
    }
    return false;
  }

  g_lastScanKey = scanKey;
  return true;
}

/** Lấy lượt đang xếp hàng ra kiểm tra. Trả về true nếu đã bắt đầu lượt mới. */
static bool startPendingScan() {
  if (!g_pendingScan.valid)
    return false;

  const PendingScan p = g_pendingScan;
  g_pendingScan.valid = false;

  if (millis() - p.queuedMs > PENDING_MAX_AGE_MS) {
    Serial.println("[CHECK] Luot xep hang qua cu, bo qua.");
    return false;
  }

  Serial.print("[CHECK] Bat dau luot da xep hang: ");
  Serial.println(p.key);
  g_lastScanKey = p.key; // Bộ nhớ chống lặp theo đầu đọc (g_seenKey) giữ nguyên
  if (p.source == SRC_QR)
    startQrCheck(p.data, false);
  else
    startCardCheck(p.source, p.data, false);
  return true;
}

/** Chuẩn bị lượt kiểm tra mới: xoá kết quả cũ, cấp req_id mới. Gọi TRƯỚC khi publish. */
static uint32_t prepareCheckRequest() {
  stopOledAnimations();
  mqttClearVerifyResult();
  g_checkReqId++;
  if (g_checkReqId == 0)
    g_checkReqId = 1; // 0 nghĩa là "không có req_id"
  return g_checkReqId;
}

/**
 * Sau khi publish: thành công -> vẽ "Dang kiem tra..." và chuyển sang CHECK_WAITING
 * (KHÔNG chờ ở đây nữa). Thất bại -> báo lỗi ngay thay vì chờ 5 giây vô ích.
 */
static void startWaitingForResult(bool published, const char *sourceLabel,
                                  const String &expectedMssv, const String &fallbackName) {
  if (!published) {
    Serial.println("[CHECK] Khong publish duoc (MQTT chua ket noi).");
    showCheckError("Mat ket noi!", "Chua ket noi may chu MQTT", false);
    return;
  }

  g_checkExpectedMssv = expectedMssv;
  g_checkFallbackName = fallbackName;
  g_resultDefersNewScans = false;
  showProcessingScreen(u8g2, sourceLabel);
  enterCheckState(CHECK_WAITING);
}

/** Đã nhận verify_result hợp lệ -> hiển thị lịch hẹn hoặc "Khong co lich hen!". */
static void showVerifyResult() {
  // Sao chép rồi tiêu thụ kết quả để callback có thể ghi gói mới mà không ảnh hưởng
  const bool accepted = mqttVerifyAccepted;
  const String mssv = mqttVerifyMssv;
  String name = mqttVerifyName;
  const String apptTime = mqttVerifyTime;
  const String queue = mqttVerifyQueue;
  const String msg = mqttVerifyMsg;
  mqttVerifyReceived = false;
  g_lastCheckFailed = false;

  // Ưu tiên tên từ backend -> tên parse từ QR -> MSSV
  if (name.length() == 0)
    name = g_checkFallbackName;
  if (name.length() == 0)
    name = mssv.length() > 0 ? mssv : g_checkExpectedMssv;

  if (accepted) {
    Serial.printf("[CHECK] Accepted: mssv=%s name=%s time=%s queue=%s msg=%s\n",
                  mssv.c_str(), name.c_str(), apptTime.c_str(), queue.c_str(), msg.c_str());

    showAppointmentLayout(u8g2, name, apptTime, queue, msg);
    buzzerMgr.beepOk(); // Bíp dài
    holdResultScreen(resultHoldMs(RESULT_ACCEPTED_MIN_MS));
  } else {
    Serial.printf("[CHECK] Denied: mssv=%s msg=%s\n", mssv.c_str(), msg.c_str());

    showNoAppointmentLayout(u8g2, msg); // "Khong co lich hen!"
    buzzerMgr.beepError();              // Bíp, bíp, bíp (lỗi)
    holdResultScreen(resultHoldMs(RESULT_DENIED_MS));
  }
}

/**
 * [MỚI] Trái tim của cơ chế non-blocking: gọi MỖI vòng loop(), không bao giờ chờ.
 */
static void updateCheckStateMachine() {
  const unsigned long elapsed = millis() - g_checkStateSinceMs;

  switch (g_checkState) {
  case CHECK_IDLE:
    // Phản hồi đến khi không có lượt nào đang chờ (đến muộn sau timeout, gửi trùng...)
    if (mqttVerifyReceived) {
      Serial.println("[CHECK] Bo qua verify_result khong mong doi (den muon / trung lap).");
      mqttVerifyReceived = false;
    }
    startPendingScan(); // Phòng hờ: còn lượt xếp hàng thì kiểm tra luôn
    break;

  case CHECK_WAITING:
    if (mqttVerifyReceived) {
      // Lọc phản hồi lạc của lượt trước:
      //  - Backend có echo req_id nhưng không khớp lượt hiện tại
      //  - Lượt QR đã biết MSSV nhưng backend trả về MSSV khác
      if (mqttVerifyReqId != 0 && mqttVerifyReqId != g_checkReqId) {
        Serial.printf("[CHECK] Bo qua phan hoi cua luot cu (req_id=%lu, dang cho %lu).\n",
                      (unsigned long)mqttVerifyReqId, (unsigned long)g_checkReqId);
        mqttVerifyReceived = false;
      } else if (g_checkExpectedMssv.length() > 0 && mqttVerifyMssv.length() > 0 &&
                 mqttVerifyMssv != g_checkExpectedMssv) {
        Serial.printf("[CHECK] Bo qua phan hoi lech MSSV (%s != %s).\n",
                      mqttVerifyMssv.c_str(), g_checkExpectedMssv.c_str());
        mqttVerifyReceived = false;
      } else {
        showVerifyResult();
      }
    } else if (elapsed >= MQTT_RESPONSE_TIMEOUT_MS) {
      Serial.println("[CHECK] Het thoi gian cho phan hoi MQTT.");
      showCheckError("Het thoi gian!", "May chu khong phan hoi", true);
    } else {
      // Thanh tiến trình đếm ngược timeout (tự giới hạn tần suất vẽ)
      updateProcessingProgress(u8g2, elapsed, MQTT_RESPONSE_TIMEOUT_MS);
    }
    break;

  case CHECK_SHOWING_RESULT: {
    // Có sinh viên đang xếp hàng: cho kết quả hiện tại hiển thị tối thiểu
    // PENDING_MIN_RESULT_MS (màn hình timeout thì hiển thị hết) rồi kiểm tra người kế tiếp.
    unsigned long minShowMs = g_resultDefersNewScans ? g_resultDisplayMs : PENDING_MIN_RESULT_MS;
    if (minShowMs > g_resultDisplayMs)
      minShowMs = g_resultDisplayMs;
    if (g_pendingScan.valid && elapsed >= minShowMs && startPendingScan())
      break;

    if (elapsed >= g_resultDisplayMs) {
      finishCheck(); // Hết giờ -> phục hồi trang cũ (thay cho delay(2000) trước đây)
    } else {
      updateOledAnimations(u8g2); // Chữ chạy cho tên / msg dài
    }
    break;
  }
  }
}

// ============================================================================
// [THAY ĐỔI] XỬ LÝ QUÉT THẺ - giờ chỉ "khởi động" lượt kiểm tra rồi thoát ngay,
// việc chờ phản hồi và hiển thị kết quả do updateCheckStateMachine() đảm nhận.
// ============================================================================

/** Publish UID thẻ RFID/NFC và chuyển sang CHECK_WAITING (không chờ). */
static void startCardCheck(ScanSource source, const String &tag, bool ackBeep) {
  const bool isRfid = (source == SRC_RFID);
  if (ackBeep)
    buzzerMgr.beepShort(); // Bíp ngắn chạm thẻ cực nhạy

  Serial.print(isRfid ? "[RFID/125kHz]" : "[NFC/MFRC522]");
  Serial.print(" Tag UID: ");
  Serial.println(tag);

  // [THAY ĐỔI] Xoá kết quả cũ + cấp req_id TRƯỚC khi publish
  const uint32_t reqId = prepareCheckRequest();

  // Gửi data lên MQTT theo từng topic riêng biệt
  const bool published = isRfid ? mqttMgr.publishRfid(tag, reqId) : mqttMgr.publishNfc(tag, reqId);

  // [THAY ĐỔI] Không còn vòng while chờ 5 giây ở đây -> chuyển sang CHECK_WAITING
  startWaitingForResult(published, isRfid ? "The RFID 125kHz" : "The NFC 13.56MHz", "", "");
}

/** Publish dữ liệu QR (đã kiểm tra hợp lệ) và chuyển sang CHECK_WAITING (không chờ). */
static void startQrCheck(const String &qrPayload, bool ackBeep) {
  bool isHustUrl = false;
  String parsedStudentId;
  String parsedName;
  if (!parseQrPayload(qrPayload, isHustUrl, parsedStudentId, parsedName))
    return;

  if (ackBeep)
    buzzerMgr.beepShort();
  Serial.print("[QR/UART] Parsed MSSV: ");
  Serial.print(parsedStudentId);
  Serial.print(" Name: ");
  Serial.println(parsedName.length() > 0 ? parsedName : "(chờ backend)");

  // [THAY ĐỔI] Xoá kết quả cũ + cấp req_id TRƯỚC khi publish
  const uint32_t reqId = prepareCheckRequest();

  // Gửi lên MQTT
  //   Kiểu 1 — gửi nguyên URL gốc (backend parse lại)
  //   Kiểu 2 — gửi MSSV thuần (backend tra cứu trực tiếp theo mssv)
  const String &mqttPayload = isHustUrl ? qrPayload : parsedStudentId;
  const bool published = mqttMgr.publishQr(mqttPayload, reqId);

  // [THAY ĐỔI] Hiển thị "Dang kiem tra..." rồi thoát ngay. MSSV + tên đã parse được lưu
  // lại để lọc phản hồi lạc và làm tên dự phòng khi hiển thị lịch hẹn.
  startWaitingForResult(published, "Ma QR", parsedStudentId, parsedName);
}

void handleCardCheck(String tag, const char *logPrefix) {
  const ScanSource source = (String(logPrefix).indexOf("RFID") >= 0) ? SRC_RFID : SRC_NFC;

  // [MỚI] Bỏ qua nếu thẻ vẫn nằm trên đầu đọc; xếp hàng nếu máy đang bận
  if (!acceptNewScan(makeScanKey(source, tag), source, tag))
    return;

  startCardCheck(source, tag, true);
}

void handleQrCardCheck(const String &qrPayload) {
  // Bước 1: Xác định loại QR (URL thẻ HUST hoặc MSSV thuần)
  bool isHustUrl = false;
  String parsedStudentId;
  String parsedName;
  if (!parseQrPayload(qrPayload, isHustUrl, parsedStudentId, parsedName)) {
    // Không phải kiểu nào hợp lệ → bỏ qua
    Serial.print("[QR/UART] QR không hợp lệ (không phải URL HUST / MSSV): ");
    Serial.println(qrPayload);
    return;
  }
  if (!isHustUrl) {
    Serial.print("[QR/UART] QR dạng MSSV thuần: ");
    Serial.println(parsedStudentId);
  }

  // [MỚI] Bỏ qua nếu máy quét đọc lặp cùng 1 mã; xếp hàng nếu máy đang bận
  if (!acceptNewScan(makeScanKey(SRC_QR, parsedStudentId), SRC_QR, qrPayload))
    return;

  // Bước 2-3: Gửi lên MQTT và chuyển sang chờ (non-blocking)
  startQrCheck(qrPayload, true);
}

// ============================================================================
// [MỚI] CÁC HÀM CỦA STATE MACHINE KHOÁ BẰNG MẬT KHẨU
// ============================================================================

/** Vẽ màn hình nhập mật khẩu: "Mật khẩu: _ _" -> "* _" -> "* *" theo số bước đã đúng. */
static void showPasswordScreen(uint8_t correctSteps) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);
  const char *title = "KIOSK CHECK-IN";
  u8g2.drawStr((128 - u8g2.getStrWidth(title)) / 2, 11, title);
  u8g2.drawHLine(0, 14, 128);

  String line = "Mật khẩu: ";
  line += (correctSteps >= 1) ? "*" : "_";
  line += " ";
  line += (correctSteps >= 2) ? "*" : "_";
  u8g2.setFont(VIETNAMESE_FONT); // Font 16px có dấu tiếng Việt
  u8g2.drawUTF8((128 - u8g2.getUTF8Width(line.c_str())) / 2, 40, line.c_str());

  // Không ghi gợi ý tổ hợp nút lên màn hình (sẽ lộ mật khẩu)
  u8g2.setFont(u8g2_font_5x7_tr);
  const char *hint = (correctSteps >= 2) ? "Da mo khoa!" : "Nhap mat khau bang nut";
  u8g2.drawStr((128 - u8g2.getStrWidth(hint)) / 2, 60, hint);

  u8g2.sendBuffer();
}

static void enterPasswordState() {
  g_appState = STATE_PASSWORD;
  passwordStep = 0;
  g_passwordLastInputMs = millis();
  // ButtonGesture giữ số lần bấm / trạng thái đang giữ qua lúc chuyển trạng thái: cú bấm làm dở
  // trên màn hình chào sẽ được báo ra ở đây (SHORT_PRESS/DOUBLE_CLICK muộn 300ms, LONG_PRESS nếu
  // giữ tiếp) -> bỏ qua tới khi nút được thả và yên lặng đủ BOOT_GESTURE_QUIET_MS.
  g_ignoreBootGesture = (digitalRead(BUTTON_PIN) == LOW) ||
                        (g_lastBtnLowMs != 0 && millis() - g_lastBtnLowMs <= BOOT_GESTURE_QUIET_MS);
  showPasswordScreen(0);
  Serial.println("[LOCK] Cho nhap mat khau.");
}

/** Mật khẩu đúng -> STATE_IDLE. Màn hình mode được vẽ sau UNLOCKED_SCREEN_MS (non-blocking). */
static void enterIdleState() {
  g_appState = STATE_IDLE;
  passwordStep = 0;
  g_unlockedAtMs = millis();
  g_idleScreenPending = true;
  Serial.println("[LOCK] Mat khau dung -> mo khoa.");
}

/** Nhập lại từ đầu. wrongInput=true: thao tác sai -> còi báo lỗi để người dùng biết. */
static void resetPassword(bool wrongInput) {
  Serial.println(wrongInput ? "[LOCK] Sai thao tac -> nhap lai tu dau."
                            : "[LOCK] Qua 5s khong thao tac -> nhap lai tu dau.");
  passwordStep = 0;
  showPasswordScreen(0);
  if (wrongInput)
    buzzerMgr.beepError();
}

static void updateBootState() {
  // Vẫn gọi update() để bộ nhận dạng cử chỉ luôn theo kịp trạng thái nút; thao tác trong
  // lúc đang hiện màn hình chào bị bỏ qua.
  configBtn.update();
  if (digitalRead(BUTTON_PIN) == LOW)
    g_lastBtnLowMs = millis();
  if (millis() - g_bootStartMs >= BOOT_SCREEN_MS)
    enterPasswordState();
}

/** Mật khẩu: DOUBLE_CLICK -> LONG_PRESS_2S. */
static void updatePasswordState() {
  ButtonEvent evt = configBtn.update();
  const unsigned long now = millis();

  // Đang giữ nút (vd. đang giữ cho đủ 2 giây) cũng là đang thao tác: không hết giờ giữa chừng
  if (digitalRead(BUTTON_PIN) == LOW) {
    g_passwordLastInputMs = now;
    g_lastBtnLowMs = now;
  }

  // Thao tác bắt đầu từ màn hình chào: bỏ qua (xem enterPasswordState())
  if (g_ignoreBootGesture) {
    evt = NONE;
    if (digitalRead(BUTTON_PIN) == HIGH && now - g_lastBtnLowMs > BOOT_GESTURE_QUIET_MS)
      g_ignoreBootGesture = false;
  }

  if (evt == NONE) {
    if (passwordStep > 0 && now - g_passwordLastInputMs > PASSWORD_TIMEOUT_MS)
      resetPassword(false);
    return;
  }
  g_passwordLastInputMs = now;

  if (passwordStep == 0 && evt == DOUBLE_CLICK) {
    passwordStep = 1;
    showPasswordScreen(1); // "Mật khẩu: * _"
  } else if (passwordStep == 1 && evt == LONG_PRESS_2S) {
    showPasswordScreen(2); // "Mật khẩu: * *"
    buzzerMgr.beepOk();
    enterIdleState();
  } else {
    resetPassword(true); // Sai thứ tự / sai kiểu bấm
  }
}

/**
 * Chưa mở khoá: vẫn đọc các đầu đọc nhưng bỏ dữ liệu, để bộ đệm UART không dồn mã cũ
 * (sẽ bị xử lý như lượt quét mới ngay sau khi mở khoá).
 */
static void discardScansWhileLocked() {
  qr_update();
  if (qr_has_new_payload()) {
    qr_get_last_payload();
    Serial.println("[LOCK] Chua mo khoa, bo qua ma QR.");
  }

  rfid_update();
  nfc_update();
  if (rfid_has_new_tag()) {
    rfid_get_last_tag();
    rfid_flush();
    Serial.println("[LOCK] Chua mo khoa, bo qua the RFID.");
  }
  if (nfc_has_new_tag()) {
    nfc_get_last_tag();
    Serial.println("[LOCK] Chua mo khoa, bo qua the NFC.");
  }
}

/** STATE_IDLE: toàn bộ hoạt động của kiosk (trước đây là thân của loop()). */
static void updateIdleState() {
  // Lần vẽ màn hình mode đầu tiên sau khi mở khoá (để "Mật khẩu: * *" kịp hiển thị)
  if (g_idleScreenPending && millis() - g_unlockedAtMs >= UNLOCKED_SCREEN_MS)
    renderCurrentMode();

  // [MỚI] Chạy State Machine kiểm tra thẻ: xử lý phản hồi / timeout / hết giờ hiển thị.
  // Không chờ -> các khối bên dưới (QR, nút bấm, RFID, NFC) vẫn chạy mỗi vòng.
  updateCheckStateMachine();

  // Cập nhật và xử lý dữ liệu từ module QR scanner
  qr_update();
  if (qr_has_new_payload()) {
    const String qrPayload = qr_get_last_payload();
    if (qrPayload.length() > 0) {
      handleQrCardCheck(qrPayload);
    }
  }

  // Kiểm tra sự kiện phím bấm
  ButtonEvent evt = configBtn.update();

  // [MỚI] Đang hiển thị kết quả -> bấm nút để đóng sớm và quay về màn hình trước.
  // Bấm nhanh / bấm đúp chỉ dùng để đóng (không đổi mode ngoài ý muốn);
  // giữ 2s vẫn đi tiếp vào chế độ đăng kí WiFi bên dưới.
  if (evt != NONE && g_checkState == CHECK_SHOWING_RESULT) {
    finishCheck();
    if (evt != LONG_PRESS_2S)
      evt = NONE;
  }

  // Hiển thị Led chỉ thị mặc định theo nút bấm
  if (evt == SHORT_PRESS) {
    if (g_mode == MODE_SETTINGS) {
      // NẾU: Đang ở trang Settings -> Bấm nhanh là XUỐNG DÒNG (Chuyển con trỏ)
      settingCursorIndex++;
      if (settingCursorIndex >= MAX_SETTINGS_ITEM) {
        // Nếu qua hết các tuỳ chọn -> Thoát để đi sang trang (g_mode) tiếp theo
        settingCursorIndex = 0;
        g_mode = (g_mode + 1) % MODE_NUM;
      }
      renderCurrentMode(); // Vẽ lại menu làm con trỏ nhảy dòng
    } else {
      Serial.println("Bam nhanh: Chuyen Mode");
      g_mode = (g_mode + 1) % MODE_NUM;
      renderCurrentMode(); // Vẽ lại ngay lập tức
    }
  } else if (evt == DOUBLE_CLICK) {
    if (g_mode == MODE_SETTINGS) {
      // Bấm đúp tại Settings -> Bật / Tắt giá trị mục đang đứng
      if (settingCursorIndex == 0) {
        configMgr.params.wifiEnabled = !configMgr.params.wifiEnabled;
        if (!configMgr.params.wifiEnabled)
          ShutdownWiFi();
        else
          WakeupWiFi();
      } else if (settingCursorIndex == 1) {
        if (configMgr.params.mqttEnabled) {
          ShutdownMQTT();
        } else {
          WakeupMQTT();
        }
      }

      buzzerMgr.beepShort(); // Kêu 1 tiếng báo hiệu đã lưu
      renderCurrentMode();   // Vẽ lại OLED (Để ON đổi thành OFF)

    } else if (g_mode == MODE_INFO) {
      if (configMgr.params.wifiEnabled) {
        Serial.println("Tắt WiFi");
        ShutdownWiFi();
      } else {
        Serial.println("Bật WiFi");
        WakeupWiFi();
        // hàm  handleWiFiConnection(); sẽ làm nốt phần việc còn lại ở đầu vòng
        // lắp
      }
    }
  } else if (evt == LONG_PRESS_2S) {
    if (g_mode == MODE_SETTINGS) {
      g_mode = MODE_INFO;
      settingCursorIndex = 0;
      renderCurrentMode();
    } else {
      Serial.println("Giữ 2s: Đăng kí WiFi");
      abortCheck(); // [MỚI] Huỷ lượt kiểm tra đang chờ trước khi vào cổng cấu hình WiFi
      // [MỚI] RegisterWiFi() chạy vòng lặp vô hạn (tới khi khởi động lại) nên update() không
      // được gọi nữa -> tắt còi ngay, tránh còi kêu mãi nếu đang bíp dở.
      buzzerMgr.stop();
      showAPConfig(u8g2);
      RegisterWiFi(WIFI_REGISTRATION_METHODS::SELF_STATION);
    }
  }

  // Cập nhật dữ liệu từ module RFID 125kHz
  rfid_update();

  // Cập nhật dữ liệu từ module MFRC522 13.56MHz
  nfc_update();

  // Nếu vừa đọc được thẻ RFID
  if (rfid_has_new_tag()) {
    handleCardCheck(rfid_get_last_tag(), "[RFID/125kHz]");
    rfid_flush(); // Xả buffer Serial2 + reset cooldown tránh xử lý trùng
  }

  // Nếu vừa đọc được thẻ NFC (Mifare)
  if (nfc_has_new_tag()) {
    handleCardCheck(nfc_get_last_tag(), "[NFC/MFRC522]");
  }
}

void loop() {
  g_prevLoopStartMs = g_loopStartMs; // [MỚI] Dùng để bù thời gian bị chặn cho bộ chống lặp
  g_loopStartMs = millis();

  // WiFi được duy trì liên tục. Sễ passthough nếu thành công rồi
  CheckAndEstablishWiFiConnection();
  // Duy trì kết nối MQTT (callback verify_result được gọi bên trong hàm này)
  // [MỚI] Khi MQTT đang mất kết nối, mqttMgr.loop() sẽ gọi client.connect() và có thể treo
  // 1-3 giây. Nếu còi đang kêu lúc đó, buzzerMgr.update() không chạy được -> tiếng bíp
  // bị kéo dài. Vì vậy hoãn việc kết nối lại cho tới khi còi kêu xong (chậm tối đa ~1 giây).
  // [MỚI] Cũng hoãn kết nối lại khi chưa mở khoá: trong 1-3 giây bị treo nút không được đọc,
  // cú bấm đúp nhập mật khẩu sẽ bị mất / nhận sai. Đã kết nối thì vẫn duy trì bình thường.
  if (wifiStatus && (mqttMgr.connected() || (!buzzerMgr.isBusy() && g_appState == STATE_IDLE))) {
    mqttMgr.loop();
  }

  // [MỚI] State Machine khoá thiết bị: chỉ khi đã mở khoá mới chạy hoạt động của kiosk
  switch (g_appState) {
  case STATE_BOOT:
    updateBootState();
    discardScansWhileLocked();
    break;
  case STATE_PASSWORD:
    updatePasswordState();
    discardScansWhileLocked();
    break;
  case STATE_IDLE:
    updateIdleState();
    break;
  }

  // [MỚI] Cập nhật còi non-blocking: bật/tắt GPIO đúng thời điểm theo millis().
  // Đặt ở CUỐI loop() để các tiếng bíp vừa được yêu cầu trong vòng này cũng được xử lý.
  buzzerMgr.update();
}
