#ifndef GREETINGCARD_H
#define GREETINGCARD_H
#include "HardwareConfig.h"

#if !ENABLE_OLED_DISPLAY
	#error "OledBackdrop requires ENABLE_OLED_DISPLAY=1 in HardwareConfig.h"
#endif


#include <U8g2lib.h>

/**
 * Hiển thị màn hình chào với logo, thông tin trạng thái và phiên bản.
 * @param u8g2 Đối tượng màn hình truyền từ main
 * @param wifiSSID Tên mạng Wifi (truyền "" nếu không kết nối)
 * @param btName Tên Bluetooth (truyền "" nếu tắt)
 */
void showWelcomeScreen(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2);


// Hàm hiển thị thông số từ Flash
void showFlashConfig(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const char * moretext = NULL);

// Màn hình thông số AP
void showAPConfig(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2);

// Màn hình thông số MQTT
void showMqttConfig(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2);

// Phiên bản nhỏ gọn hơn, dùng font 6x12, bắt đầu từ Y=36 để tránh chồng MSSV
void drawVietnameseNameCompact(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, String studentName);

// Trang cài đặt Settings On/Off
void showSettingsPage(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, uint8_t cursorIndex);

// ============================================================================
// [MỚI] GIAO DIỆN NHẮC LỊCH HẸN (dùng với State Machine non-blocking trong main.cpp)
// ----------------------------------------------------------------------------
// Nguyên tắc: các hàm show*() vẽ TOÀN MÀN HÌNH 1 lần (sendBuffer), còn các hàm
// update*() chỉ vẽ lại 1 dải nhỏ (updateDisplayArea) và tự giới hạn tần suất,
// nên có thể gọi ở MỌI vòng loop() mà không làm chậm việc quét thẻ / đọc nút.
// ============================================================================

/**
 * Màn hình "Dang kiem tra..." khi đang chờ MQTT phản hồi, kèm thanh tiến trình timeout.
 * @param sourceLabel Nguồn quét, ví dụ "The RFID 125kHz", "Ma QR"
 */
void showProcessingScreen(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const char *sourceLabel);

/**
 * Cập nhật thanh tiến trình chờ (chỉ vẽ lại 16 hàng pixel dưới cùng, non-blocking).
 * @param elapsedMs Thời gian đã chờ (ms)
 * @param timeoutMs Tổng thời gian chờ tối đa (ms)
 */
void updateProcessingProgress(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                              unsigned long elapsedMs, unsigned long timeoutMs);

/**
 * Bố cục lịch hẹn khi MQTT trả về "accepted" (4 dòng):
 *   Dòng 1: "Xin chao, <Tên SV>"  (font tiếng Việt có dấu, tự chạy chữ nếu dài)
 *   Dòng 2: "Gio hen: <time>"
 *   Dòng 3: "STT: <queue>"
 *   Dòng 4: "<msg>"               (font nhỏ 5x8, tự chạy chữ nếu dài)
 */
void showAppointmentLayout(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2,
                           const String &studentName, const String &appointmentTime,
                           const String &queueNumber, const String &message);

/**
 * Màn hình khi MQTT trả về "denied": "Khong co lich hen!".
 * @param message msg từ backend (nếu có) hiển thị bằng font nhỏ ở đáy màn hình.
 * @note Hàm chỉ vẽ; main.cpp chịu trách nhiệm gọi buzzerMgr.beepError().
 */
void showNoAppointmentLayout(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const String &message = "");

/**
 * Màn hình lỗi (hết thời gian chờ, mất kết nối MQTT...).
 */
void showCheckErrorLayout(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2, const char *title, const char *detail);

/**
 * Chạy hiệu ứng chữ chạy (marquee) cho dòng tên / dòng msg nếu quá dài.
 * Gọi liên tục trong loop() khi đang hiển thị kết quả; tự giới hạn ~30ms/bước.
 */
void updateOledAnimations(U8G2_SH1106_128X64_NONAME_F_HW_I2C &u8g2);

/** Dừng mọi hiệu ứng (marquee, thanh tiến trình) - gọi trước khi chuyển sang màn hình khác. */
void stopOledAnimations();

/**
 * Thời gian (ms) để dòng chữ chạy dài nhất chạy hết 1 vòng (0 nếu không có dòng nào cần chạy).
 * main.cpp dùng để kéo dài thời gian hiển thị lịch hẹn cho sinh viên kịp đọc hết.
 */
unsigned long getOledAnimationCycleMs();

/**
 * Bỏ dấu tiếng Việt (UTF-8) -> ASCII, ví dụ "Quầy số 3" -> "Quay so 3".
 * Dùng cho các font ASCII nhỏ (U8g2 không có font tiếng Việt < 16px).
 */
String vnToAscii(const String &utf8Text);

#endif
