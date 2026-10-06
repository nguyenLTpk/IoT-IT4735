#include "OledBackdrop.h"
#include "ConfigManager.h" // Lấy thông số cấu hình để hiển thị
#include "WiFiSelfEnroll.h" // Lấy cấu hình AP mặc định
#include "main.h"           // Import version
#include <WiFi.h>           // Để lây thông tin địa chỉ IP


// Logo bụi mịn (Chuyển đổi từ ảnh 50x50 của bạn)
static const unsigned char logo_dust_50x50[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff,
    0xdf, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc0, 0xff, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xdf, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc1, 0xff, 0xff, 0xc0, 0xff, 0xff,
    0xff, 0x18, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xc6, 0x7f, 0x7f, 0x7f, 0xc0,
    0xff, 0xff, 0x00, 0xff, 0xbf, 0x7f, 0xc0, 0xff, 0xff, 0x7c, 0xff, 0xdf,
    0xff, 0xc0, 0xff, 0xfe, 0xfe, 0xfb, 0xdf, 0xff, 0xc0, 0xff, 0xfe, 0xfe,
    0xfb, 0xef, 0xff, 0xc0, 0xff, 0xfe, 0xff, 0x7f, 0xef, 0xff, 0xc0, 0xff,
    0xf8, 0xff, 0xff, 0xef, 0xff, 0xc0, 0xfe, 0xe3, 0xff, 0xff, 0xe3, 0xff,
    0xc0, 0xff, 0xcf, 0xff, 0xff, 0x9d, 0xff, 0xc0, 0xff, 0xdf, 0xff, 0xff,
    0xbe, 0xff, 0xc0, 0xff, 0xbf, 0xff, 0xff, 0xfe, 0xff, 0xc0, 0xff, 0xbf,
    0xff, 0xff, 0xff, 0x7f, 0xc0, 0xff, 0x7f, 0xef, 0xff, 0xff, 0x7f, 0xc0,
    0xff, 0x7f, 0xff, 0xff, 0xfe, 0x7f, 0xc0, 0xff, 0x7f, 0xff, 0xfd, 0xfe,
    0xff, 0xc0, 0xff, 0x7f, 0xff, 0xfd, 0xfc, 0xff, 0xc0, 0xff, 0x7f, 0xff,
    0xff, 0xf1, 0xff, 0xc0, 0xff, 0x7f, 0xff, 0xff, 0xfc, 0xff, 0xc0, 0xff,
    0xbf, 0xff, 0xff, 0xff, 0x7f, 0xc0, 0xff, 0xbf, 0xff, 0xe3, 0xff, 0x3f,
    0xc0, 0xff, 0xdf, 0xff, 0xf9, 0xff, 0xbf, 0xc0, 0xff, 0xcf, 0xff, 0xfd,
    0xff, 0xdf, 0xc0, 0xff, 0xf3, 0x1f, 0xfe, 0xff, 0xdf, 0xc0, 0xff, 0xf8,
    0x3f, 0xfe, 0xff, 0xdf, 0xc0, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xdf, 0xc0,
    0xff, 0xff, 0xff, 0xfe, 0xff, 0xdf, 0xc0, 0xff, 0xff, 0xff, 0xfd, 0xff,
    0xdf, 0xc0, 0xfe, 0x00, 0x07, 0xfd, 0xff, 0xbf, 0xc0, 0xff, 0xff, 0xff,
    0xf9, 0xff, 0xbf, 0xc0, 0xff, 0xff, 0xff, 0xf3, 0xff, 0x3f, 0xc0, 0xff,
    0xff, 0xff, 0xc7, 0xff, 0x7f, 0xc0, 0xfe, 0x00, 0x00, 0x1f, 0xfe, 0xff,
    0xc0, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff,
    0xf3, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc7, 0xff, 0xc0, 0xfc, 0x00,
    0x00, 0x00, 0x3f, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xc0};

void showWelcomeScreen(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2) {
  u8g2.clearBuffer();

  // 1. VÙNG TRẠNG THÁI (TOP)
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 10, "WiFi:");
  u8g2.drawStr(32, 10,
               (configMgr.params.wifiEnabled) ? configMgr.params.ssid.c_str()
                                              : "OFF");

  // 2. LOGO BÊN TRÁI (50x50)
  u8g2.drawXBM(2, 16, 50, 50, logo_dust_50x50);

  // 3. PHẦN CHỮ BÊN PHẢI
  u8g2.setFont(u8g2_font_8x13_tr);
  u8g2.drawStr(55, 26, "Monitor");
  u8g2.drawStr(55, 41, "Student");

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(55, 53, "RFID, QR");

  // Sử dụng hằng số từ main.h
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(55, 62);
  u8g2.print("Ver: ");
  u8g2.print(FIRMWARE_VERSION); // Gọi từ main.h

  u8g2.sendBuffer();
}

/**
 * Hàm hỗ trợ vẽ văn bản có tự động xuống dòng nếu quá dài
 */
void drawSmartText(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, int x, int &y,
                   const char *label, String value) {
  u8g2.setCursor(x, y);
  u8g2.print(label);

  int labelWidth = u8g2.getStrWidth(label);
  int valueWidth = u8g2.getStrWidth(value.c_str());
  int maxWidth = 120 - labelWidth; // Khoảng trống còn lại trên dòng

  if (valueWidth <= maxWidth) {
    u8g2.print(value);
    y += 12; // Xuống dòng cho mục tiếp theo
  } else {
    // Nếu quá dài, cắt chuỗi hoặc xuống dòng đơn giản
    // Cách đơn giản nhất: In nhãn, xuống dòng rồi in giá trị đầy đủ
    y += 10;
    u8g2.setCursor(x + 10, y); // Thụt lề một chút cho đẹp
    u8g2.print(value);
    y += 12;
  }
}

void showFlashConfig(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                     const char *moretext) {
  u8g2.clearBuffer();

  // Tiêu đề
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("DeviceId: ");
  u8g2.print(configMgr.params.deviceID);

  int y = 25;
  // Hiển thị SSID
  drawSmartText(u8g2, 0, y, "SSID: ", configMgr.params.ssid);

  // Hiển thị Device ID (Masked Password)
  String maskedPass = "****";
  if (configMgr.params.password.length() > 2) {
    maskedPass += configMgr.params.password.substring(
        configMgr.params.password.length() - 2);
  } else if (configMgr.params.password.length() > 0) {
    maskedPass += configMgr.params.password;
  } else {
    maskedPass = ""; // Empty password
  }
  drawSmartText(u8g2, 0, y, "Pass: ", maskedPass);

  // --- HIỂN THỊ TRẠNG THÁI WIFI (đọc real-time từ WiFi.status()) ---
  u8g2.setCursor(0, y);
  u8g2.print("WiFi: ");
  if (!configMgr.params.wifiEnabled) {
    u8g2.print("OFF");
  } else {
    // Đọc trực tiếp trạng thái WiFi hiện tại thay vì dùng biến cache
    if (WiFi.status() == WL_CONNECTED) {
      u8g2.print(WiFi.localIP().toString());
    } else {
      u8g2.print("fail [X]");
    }
  }

  // --- HIỂN THỊ TRẠNG THÁI MQTT ---
  y += 12;
  u8g2.setCursor(0, y);
  if (moretext != NULL) {
    u8g2.print(moretext);
  }

  u8g2.sendBuffer();
}

void showAPConfig(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2) {
  u8g2.clearBuffer();

  // Tiêu đề
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("--- Access Point ---");

  // Hiển thị SSID AP
  u8g2.setCursor(0, 25);
  u8g2.print("AP:");
  u8g2.print(AP_WIFI_SSID);

  // Mask AP password
  String apPass = "****";
  String rawPass = String(AP_WIFI_PASSWORD);
  if (rawPass.length() > 2) {
    apPass += rawPass.substring(rawPass.length() - 2);
  } else if (rawPass.length() > 0) {
    apPass += rawPass;
  } else {
    apPass = "";
  }

  // Hiển thị Pass
  u8g2.setCursor(0, 40);
  u8g2.print("P:");
  u8g2.print(apPass);
  // Hiển thị IP
  u8g2.setCursor(0, 55);
  u8g2.print("IP: 192.168.15.1");

  u8g2.sendBuffer();
}

void showMqttConfig(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2) {
  u8g2.clearBuffer();

  // Tiêu đề
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("--- MQTT Service ---");

  int y = 25;
  // Tạo chuỗi Upstream / Downstream
  String tUp = String("monitor_student/") + configMgr.params.deviceID + "/data";
  String tDown =
      String("monitor_student/") + configMgr.params.deviceID + "/cmd";

  // Hiển thị Topic Up
  drawSmartText(u8g2, 0, y, "Up: ", tUp);

  // Hiển thị Topic Down
  drawSmartText(u8g2, 0, y, "Dn: ", tDown);

  u8g2.sendBuffer();
}

// Phiên bản nhỏ gọn — dùng font ASCII 5x8 (U8g2 không có font Vietnamese < 16px)
void drawVietnameseNameCompact(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                               String studentName) {
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1);

  // Tính xem chuỗi có vừa 1 dòng (~21 ký tự với font 6px) không
  const int maxCharsPerLine = 20;

  if ((int)studentName.length() <= maxCharsPerLine) {
    // Vừa 1 dòng
    u8g2.drawUTF8(5, 40, studentName.c_str());
  } else {
    // Tìm dấu cách gần giữa chuỗi để ngắt dòng
    int splitIdx = -1;
    int mid = studentName.length() / 2;
    for (int i = mid; i < (int)studentName.length(); i++) {
      if (studentName[i] == ' ') {
        splitIdx = i;
        break;
      }
    }
    if (splitIdx == -1) {
      for (int i = mid; i >= 0; i--) {
        if (studentName[i] == ' ') {
          splitIdx = i;
          break;
        }
      }
    }

    if (splitIdx != -1) {
      String l1 = studentName.substring(0, splitIdx);
      String l2 = studentName.substring(splitIdx + 1);
      u8g2.drawUTF8(5, 38, l1.c_str());
      u8g2.drawUTF8(5, 55, l2.c_str());
    } else {
      // Không tìm được chỗ cắt → in 1 dòng cắt bớt
      u8g2.drawUTF8(5, 40, studentName.c_str());
    }
  }
}

void showSettingsPage(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                      uint8_t cursorIndex) {
  u8g2.clearBuffer();

  // Title
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(10, 15, "System Settings");
  u8g2.drawLine(0, 18, 128, 18); // Underline

  // Menu Item 0: WiFi
  if (cursorIndex == 0)
    u8g2.drawStr(0, 35, ">");
  u8g2.drawStr(10, 35, "WiFi:");
  u8g2.drawStr(100, 35, configMgr.params.wifiEnabled ? "ON" : "OFF");

  // Menu Item 1: MQTT (Or other configs)
  if (cursorIndex == 1)
    u8g2.drawStr(0, 52, ">");
  u8g2.drawStr(10, 52, "MQTT:");
  u8g2.drawStr(100, 52, configMgr.params.mqttEnabled ? "ON" : "OFF");

  // Footer - Guide
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 64, "DblClk:Toggle");

  u8g2.sendBuffer();
}

// ============================================================================
// [MỚI] GIAO DIỆN NHẮC LỊCH HẸN
// ----------------------------------------------------------------------------
// Bố cục 128x64 (đã căn theo metric thật của font U8g2):
//   y  0..15 : Dòng 1 - "Xin chao, <Tên>"  unifont tiếng Việt (cao 16px, baseline 14)
//   y 17     : đường kẻ ngang
//   y 20..32 : Dòng 2 - "Gio hen: <time>"  7x13B (baseline 31)
//   y 36..48 : Dòng 3 - "STT: <queue>"     7x13B (baseline 47)
//   y 53     : đường kẻ ngang
//   y 56..63 : Dòng 4 - "<msg>"            5x8 font nhỏ (baseline 63)
//
// Dòng 1 và dòng 4 nằm gọn trong các "tile row" 8px của SH1106 (dòng 1 = tile 0-1,
// dòng 4 = tile 7) nên khi chữ quá dài có thể chạy chữ (marquee) bằng
// updateDisplayArea(): chỉ gửi 256/128 byte qua I2C (~3-6ms) thay vì cả 1KB (~25ms).
// ============================================================================

#define APPT_NAME_FONT  u8g2_font_unifont_t_vietnamese2 // Dòng 1: tên có dấu
#define APPT_INFO_FONT  u8g2_font_7x13B_tr              // Dòng 2-3, tiêu đề
#define APPT_MID_FONT   u8g2_font_6x12_tr               // Font dự phòng khi chuỗi dài
#define APPT_SMALL_FONT u8g2_font_5x8_tr                // Dòng 4: msg (font nhỏ)

static const int16_t OLED_W = 128;
static const int16_t TEXT_MARGIN_X = 2;

// --- Thông số hiệu ứng chữ chạy ---
static const int16_t MARQUEE_GAP_PX = 24;                 // Khoảng trống giữa 2 lần lặp chữ
static const int16_t MARQUEE_STEP_PX = 1;                 // Mỗi bước dịch 1px cho mượt
static const unsigned long MARQUEE_STEP_MS = 30;          // ~33px/giây
static const unsigned long MARQUEE_START_PAUSE_MS = 1200; // Dừng ở đầu dòng cho dễ đọc

// --- Thanh tiến trình chờ MQTT (nằm trong tile 6-7 = y 48..63) ---
static const int16_t PROGRESS_X = 8;
static const int16_t PROGRESS_Y = 50;
static const int16_t PROGRESS_W = 112;
static const int16_t PROGRESS_H = 9;
static const int16_t PROGRESS_INNER_W = PROGRESS_W - 4;
static const unsigned long PROGRESS_MIN_INTERVAL_MS = 60;

/** Trạng thái 1 dòng chữ có thể chạy (marquee) */
struct MarqueeLine {
  bool used;           // Dòng có nội dung cần vẽ
  bool scrolling;      // Chữ rộng hơn màn hình -> cần chạy
  bool utf8;           // true: vẽ bằng drawUTF8 (có dấu), false: drawStr (ASCII)
  bool center;         // Căn giữa khi không cần chạy
  const uint8_t *font;
  String text;
  int16_t textWidth;
  int16_t offset;      // Độ dịch hiện tại (px)
  int16_t baselineY;
  uint8_t tileRow;     // Hàng tile 8px đầu tiên của vùng dòng chữ
  uint8_t tileRows;    // Số hàng tile
  unsigned long nextStepMs;
};

static MarqueeLine s_lineTop;    // Dòng 1 (tên sinh viên)
static MarqueeLine s_lineBottom; // Dòng 4 (msg) - dùng chung cho màn hình denied/lỗi

static bool s_progressActive = false;
static int16_t s_progressLastFill = -1;
static unsigned long s_progressLastDrawMs = 0;

// ----------------------------------------------------------------------------
// Bỏ dấu tiếng Việt
// ----------------------------------------------------------------------------

struct VnAccentGroup {
  char base;
  const char *variants; // Chuỗi UTF-8 chứa mọi biến thể có dấu của chữ cái gốc
};

static const VnAccentGroup VN_ACCENT_GROUPS[] = {
    {'a', "àáạảãâầấậẩẫăằắặẳẵ"}, {'A', "ÀÁẠẢÃÂẦẤẬẨẪĂẰẮẶẲẴ"},
    {'e', "èéẹẻẽêềếệểễ"},       {'E', "ÈÉẸẺẼÊỀẾỆỂỄ"},
    {'i', "ìíịỉĩ"},             {'I', "ÌÍỊỈĨ"},
    {'o', "òóọỏõôồốộổỗơờớợởỡ"}, {'O', "ÒÓỌỎÕÔỒỐỘỔỖƠỜỚỢỞỠ"},
    {'u', "ùúụủũưừứựửữ"},       {'U', "ÙÚỤỦŨƯỪỨỰỬỮ"},
    {'y', "ỳýỵỷỹ"},             {'Y', "ỲÝỴỶỸ"},
    {'d', "đ"},                 {'D', "Đ"},
};

/** Giải mã 1 code point UTF-8 tại vị trí i và tiến i tới ký tự kế tiếp. */
static uint32_t utf8NextCodepoint(const char *s, size_t len, size_t &i) {
  const uint8_t c = (uint8_t)s[i];
  uint32_t cp;
  size_t extra;

  if (c < 0x80) {
    i++;
    return c;
  } else if ((c & 0xE0) == 0xC0) {
    cp = c & 0x1F;
    extra = 1;
  } else if ((c & 0xF0) == 0xE0) {
    cp = c & 0x0F;
    extra = 2;
  } else if ((c & 0xF8) == 0xF0) {
    cp = c & 0x07;
    extra = 3;
  } else {
    i++; // Byte không hợp lệ -> bỏ qua 1 byte
    return 0xFFFD;
  }

  if (i + extra >= len) {
    i = len; // Chuỗi bị cắt cụt giữa ký tự
    return 0xFFFD;
  }

  for (size_t k = 1; k <= extra; k++) {
    const uint8_t cc = (uint8_t)s[i + k];
    if ((cc & 0xC0) != 0x80) {
      i++;
      return 0xFFFD;
    }
    cp = (cp << 6) | (cc & 0x3F);
  }
  i += extra + 1;
  return cp;
}

/** Tìm chữ cái gốc (không dấu) của 1 code point tiếng Việt; 0 nếu không thuộc bảng. */
static char vnBaseLetter(uint32_t cp) {
  for (size_t g = 0; g < sizeof(VN_ACCENT_GROUPS) / sizeof(VN_ACCENT_GROUPS[0]); g++) {
    const char *v = VN_ACCENT_GROUPS[g].variants;
    const size_t vlen = strlen(v);
    size_t j = 0;
    while (j < vlen) {
      if (utf8NextCodepoint(v, vlen, j) == cp)
        return VN_ACCENT_GROUPS[g].base;
    }
  }
  return 0;
}

/** Dấu câu Unicode hay gặp khi copy từ Word/Google Sheets -> ký tự ASCII tương đương. */
static const char *asciiPunctuation(uint32_t cp) {
  switch (cp) {
  case 0x00A0: case 0x2002: case 0x2003: case 0x2009: case 0x202F:
    return " ";   // Các loại dấu cách đặc biệt (NBSP...)
  case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2212:
    return "-";   // Gạch nối / gạch ngang dài / dấu trừ
  case 0x2018: case 0x2019: case 0x201A: case 0x2032:
    return "'";
  case 0x201C: case 0x201D: case 0x201E: case 0x2033:
    return "\"";
  case 0x2026:
    return "..."; // Dấu ba chấm 1 ký tự
  case 0x00B7: case 0x2022:
    return "*";
  default:
    return NULL;
  }
}

/** true nếu chuỗi UTF-8 chứa dấu tổ hợp rời U+0300..U+036F (tiếng Việt dạng NFD). */
static bool hasCombiningMarks(const String &utf8Text) {
  for (size_t i = 0; i + 1 < utf8Text.length(); i++) {
    const uint8_t b0 = (uint8_t)utf8Text[i];
    const uint8_t b1 = (uint8_t)utf8Text[i + 1];
    if ((b0 == 0xCC && b1 >= 0x80 && b1 <= 0xBF) || (b0 == 0xCD && b1 >= 0x80 && b1 <= 0xAF))
      return true;
  }
  return false;
}

String vnToAscii(const String &utf8Text) {
  String out;
  out.reserve(utf8Text.length());

  const char *s = utf8Text.c_str();
  const size_t len = utf8Text.length();
  size_t i = 0;

  while (i < len) {
    const uint8_t c = (uint8_t)s[i];
    if (c < 0x80) {
      out += (char)c;
      i++;
      continue;
    }

    const uint32_t cp = utf8NextCodepoint(s, len, i);
    if (cp >= 0x0300 && cp <= 0x036F)
      continue; // Dấu tổ hợp rời (Unicode dạng NFD) -> bỏ, giữ chữ gốc đứng trước

    const char base = vnBaseLetter(cp);
    if (base) {
      out += base;
      continue;
    }
    const char *punct = asciiPunctuation(cp);
    out += punct ? punct : "?";
  }
  return out;
}

// ----------------------------------------------------------------------------
// Tiện ích vẽ chữ
// ----------------------------------------------------------------------------

/** Cắt bớt chuỗi ASCII (thêm "..") cho vừa maxWidth với font hiện tại. */
static String fitAsciiToWidth(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const String &text,
                              int16_t maxWidth) {
  if ((int16_t)u8g2.getStrWidth(text.c_str()) <= maxWidth)
    return text;

  String cut = text;
  while (cut.length() > 0) {
    cut.remove(cut.length() - 1);
    const String candidate = cut + "..";
    if ((int16_t)u8g2.getStrWidth(candidate.c_str()) <= maxWidth)
      return candidate;
  }
  return "";
}

/**
 * Vẽ 1 dòng ASCII tại baseline y: thử lần lượt font 7x13B -> 6x12 -> 5x8 cho vừa
 * màn hình, nếu vẫn dài thì cắt bớt và thêm "..".
 */
static void drawFittedLine(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, int16_t y,
                           const String &asciiText, bool center) {
  static const uint8_t *const fonts[] = {APPT_INFO_FONT, APPT_MID_FONT, APPT_SMALL_FONT};
  const size_t fontCount = sizeof(fonts) / sizeof(fonts[0]);
  const int16_t maxWidth = OLED_W - 2 * TEXT_MARGIN_X;
  String text = asciiText;

  for (size_t f = 0; f < fontCount; f++) {
    u8g2.setFont(fonts[f]);
    const bool isLastFont = (f == fontCount - 1);
    if ((int16_t)u8g2.getStrWidth(text.c_str()) <= maxWidth || isLastFont) {
      if (isLastFont)
        text = fitAsciiToWidth(u8g2, text, maxWidth);
      const int16_t w = u8g2.getStrWidth(text.c_str());
      const int16_t x = center ? (OLED_W - w) / 2 : TEXT_MARGIN_X;
      u8g2.drawStr(x, y, text.c_str());
      return;
    }
  }
}

// ----------------------------------------------------------------------------
// Marquee (chữ chạy) - non-blocking
// ----------------------------------------------------------------------------

static void marqueeSetup(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, MarqueeLine &m,
                         const String &text, const uint8_t *font, bool utf8,
                         int16_t baselineY, uint8_t tileRow, uint8_t tileRows, bool center) {
  m.text = text;
  m.font = font;
  m.utf8 = utf8;
  m.center = center;
  m.baselineY = baselineY;
  m.tileRow = tileRow;
  m.tileRows = tileRows;
  m.offset = 0;
  m.used = text.length() > 0;

  u8g2.setFont(font);
  m.textWidth = m.used ? (int16_t)(utf8 ? u8g2.getUTF8Width(text.c_str())
                                        : u8g2.getStrWidth(text.c_str()))
                       : 0;
  m.scrolling = m.used && (m.textWidth > OLED_W - 2 * TEXT_MARGIN_X);
  m.nextStepMs = millis() + MARQUEE_START_PAUSE_MS;
}

static void marqueeDrawText(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const MarqueeLine &m,
                            int16_t x) {
  // Toạ độ âm được ép sang u8g2_uint_t: U8g2 hỗ trợ "wrap-around" để vẽ chữ
  // tràn mép trái (giống ví dụ ScrollingText chính thức của thư viện).
  if (m.utf8)
    u8g2.drawUTF8((u8g2_uint_t)x, m.baselineY, m.text.c_str());
  else
    u8g2.drawStr((u8g2_uint_t)x, m.baselineY, m.text.c_str());
}

/** Vẽ lại vùng của dòng chữ vào buffer (chưa gửi ra màn hình). */
static void marqueeDraw(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const MarqueeLine &m) {
  const int16_t top = m.tileRow * 8;
  const int16_t height = m.tileRows * 8;

  // Xoá sạch vùng dòng chữ
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, top, OLED_W, height);
  u8g2.setDrawColor(1);

  if (!m.used)
    return;

  u8g2.setFont(m.font);
  // Giới hạn vùng vẽ để chữ chạy không đè sang dòng khác
  u8g2.setClipWindow(0, top, OLED_W, top + height);

  if (!m.scrolling) {
    const int16_t x = m.center ? (OLED_W - m.textWidth) / 2 : TEXT_MARGIN_X;
    marqueeDrawText(u8g2, m, x);
  } else {
    // Vẽ nối tiếp các bản sao để chữ chạy vòng tròn liền mạch
    const int16_t period = m.textWidth + MARQUEE_GAP_PX;
    for (int16_t x = TEXT_MARGIN_X - m.offset; x < OLED_W; x += period) {
      marqueeDrawText(u8g2, m, x);
    }
  }

  u8g2.setMaxClipWindow();
}

/** Dịch chữ 1 bước nếu đến hạn, rồi chỉ gửi vùng của dòng đó ra màn hình. */
static void marqueeTick(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, MarqueeLine &m) {
  if (!m.used || !m.scrolling)
    return;

  const unsigned long now = millis();
  if ((long)(now - m.nextStepMs) < 0)
    return; // Chưa đến lượt -> thoát ngay, không tốn thời gian

  const int16_t period = m.textWidth + MARQUEE_GAP_PX;
  m.offset += MARQUEE_STEP_PX;
  if (m.offset >= period) {
    m.offset = 0; // Hết 1 vòng -> quay về đầu dòng và dừng một chút
    m.nextStepMs = now + MARQUEE_START_PAUSE_MS;
  } else {
    m.nextStepMs = now + MARQUEE_STEP_MS;
  }

  marqueeDraw(u8g2, m);
  u8g2.updateDisplayArea(0, m.tileRow, OLED_W / 8, m.tileRows);
}

static unsigned long marqueeCycleMs(const MarqueeLine &m) {
  if (!m.used || !m.scrolling)
    return 0;
  const unsigned long steps =
      (unsigned long)(m.textWidth + MARQUEE_GAP_PX + MARQUEE_STEP_PX - 1) / MARQUEE_STEP_PX;
  return MARQUEE_START_PAUSE_MS + steps * MARQUEE_STEP_MS;
}

void updateOledAnimations(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2) {
  marqueeTick(u8g2, s_lineTop);
  marqueeTick(u8g2, s_lineBottom);
}

void stopOledAnimations() {
  s_lineTop.used = false;
  s_lineBottom.used = false;
  s_progressActive = false;
}

unsigned long getOledAnimationCycleMs() {
  const unsigned long a = marqueeCycleMs(s_lineTop);
  const unsigned long b = marqueeCycleMs(s_lineBottom);
  return a > b ? a : b;
}

// ----------------------------------------------------------------------------
// Màn hình chờ phản hồi MQTT
// ----------------------------------------------------------------------------

static void drawProgressBar(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, int16_t fill) {
  // Xoá vùng tile 6-7 (y 48..63) rồi vẽ khung + phần đã chạy
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 48, OLED_W, 16);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(PROGRESS_X, PROGRESS_Y, PROGRESS_W, PROGRESS_H);
  if (fill > 0)
    u8g2.drawBox(PROGRESS_X + 2, PROGRESS_Y + 2, fill, PROGRESS_H - 4);
}

void showProcessingScreen(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const char *sourceLabel) {
  stopOledAnimations();
  u8g2.clearBuffer();

  drawFittedLine(u8g2, 22, "Dang kiem tra...", true);
  if (sourceLabel != NULL && sourceLabel[0] != '\0') {
    u8g2.setFont(APPT_MID_FONT);
    const String label =
        fitAsciiToWidth(u8g2, vnToAscii(sourceLabel), OLED_W - 2 * TEXT_MARGIN_X);
    u8g2.drawStr((OLED_W - u8g2.getStrWidth(label.c_str())) / 2, 39, label.c_str());
  }

  drawProgressBar(u8g2, 0);
  u8g2.sendBuffer();

  s_progressActive = true;
  s_progressLastFill = 0;
  s_progressLastDrawMs = millis();
}

void updateProcessingProgress(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                              unsigned long elapsedMs, unsigned long timeoutMs) {
  if (!s_progressActive || timeoutMs == 0)
    return;

  const unsigned long now = millis();
  if (now - s_progressLastDrawMs < PROGRESS_MIN_INTERVAL_MS)
    return; // Giới hạn tần suất vẽ để không chiếm bus I2C

  if (elapsedMs > timeoutMs)
    elapsedMs = timeoutMs;
  const int16_t fill = (int16_t)((uint32_t)elapsedMs * PROGRESS_INNER_W / timeoutMs);
  if (fill == s_progressLastFill)
    return;

  s_progressLastFill = fill;
  s_progressLastDrawMs = now;
  drawProgressBar(u8g2, fill);
  u8g2.updateDisplayArea(0, 6, OLED_W / 8, 2); // Chỉ gửi 2 tile row dưới cùng
}

// ----------------------------------------------------------------------------
// Màn hình kết quả
// ----------------------------------------------------------------------------

void showAppointmentLayout(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                           const String &studentName, const String &appointmentTime,
                           const String &queueNumber, const String &message) {
  stopOledAnimations();
  u8g2.clearBuffer();

  // Dòng 1: "Xin chao, <Tên SV>" - giữ nguyên dấu tiếng Việt, tự chạy chữ nếu dài.
  // Tên dạng NFD (chữ + dấu tổ hợp rời) sẽ bị unifont vẽ mỗi dấu thành 1 ô riêng
  // -> chuyển sang không dấu cho dễ đọc. Dạng NFC (thông dụng) giữ nguyên dấu.
  const String name = hasCombiningMarks(studentName) ? vnToAscii(studentName) : studentName;
  const String greeting = name.length() > 0 ? String("Xin chao, ") + name : String("Xin chao!");
  marqueeSetup(u8g2, s_lineTop, greeting, APPT_NAME_FONT, true, 14, 0, 2, false);
  marqueeDraw(u8g2, s_lineTop);

  u8g2.drawHLine(0, 17, OLED_W);

  // Dòng 2: Giờ hẹn (font ASCII -> bỏ dấu để chắc chắn hiển thị được)
  const String timeText =
      appointmentTime.length() > 0 ? vnToAscii(appointmentTime) : String("--:--");
  drawFittedLine(u8g2, 31, String("Gio hen: ") + timeText, false);

  // Dòng 3: Số thứ tự
  const String queueText = queueNumber.length() > 0 ? vnToAscii(queueNumber) : String("--");
  drawFittedLine(u8g2, 47, String("STT: ") + queueText, false);

  u8g2.drawHLine(0, 53, OLED_W);

  // Dòng 4: msg - font nhỏ 5x8, tự chạy chữ nếu dài hơn ~24 ký tự
  marqueeSetup(u8g2, s_lineBottom, vnToAscii(message), APPT_SMALL_FONT, false, 63, 7, 1, false);
  marqueeDraw(u8g2, s_lineBottom);

  u8g2.sendBuffer();
}

void showNoAppointmentLayout(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const String &message) {
  stopOledAnimations();
  u8g2.clearBuffer();

  // Biểu tượng: vòng tròn có dấu X (nét đôi cho dễ nhìn)
  u8g2.drawCircle(64, 16, 12);
  u8g2.drawLine(58, 10, 70, 22);
  u8g2.drawLine(59, 10, 71, 22);
  u8g2.drawLine(70, 10, 58, 22);
  u8g2.drawLine(71, 10, 59, 22);

  drawFittedLine(u8g2, 45, "Khong co lich hen!", true);

  // Lý do từ backend (nếu có) - font nhỏ ở đáy màn hình
  marqueeSetup(u8g2, s_lineBottom, vnToAscii(message), APPT_SMALL_FONT, false, 63, 7, 1, true);
  marqueeDraw(u8g2, s_lineBottom);

  u8g2.sendBuffer();
}

void showCheckErrorLayout(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const char *title,
                          const char *detail) {
  stopOledAnimations();
  u8g2.clearBuffer();

  // Biểu tượng: tam giác cảnh báo có dấu "!"
  u8g2.drawLine(64, 3, 50, 28);
  u8g2.drawLine(64, 3, 78, 28);
  u8g2.drawHLine(50, 28, 29);
  u8g2.drawBox(63, 11, 3, 10);
  u8g2.drawBox(63, 23, 3, 3);

  drawFittedLine(u8g2, 45, vnToAscii(title != NULL ? title : ""), true);

  marqueeSetup(u8g2, s_lineBottom, vnToAscii(detail != NULL ? detail : ""), APPT_SMALL_FONT,
               false, 63, 7, 1, true);
  marqueeDraw(u8g2, s_lineBottom);

  u8g2.sendBuffer();
}
