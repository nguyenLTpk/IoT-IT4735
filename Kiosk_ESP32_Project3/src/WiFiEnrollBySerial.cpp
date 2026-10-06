#include "WiFiEnrollBySerial.h"
#include <Arduino.h>

/** Khoảng thời gian 30 giây để nhắc lại hướng dẫn */
#define PERIOD_REINTRODUCE 30000
/**
 * @brief In hướng dẫn sử dụng các lệnh cấu hình ra màn hình Serial.
 */
void printUsage() {
    Serial.println(F("\n--- SERIAL CONFIG MODE ---")); // In tiêu đề
    Serial.println(F("Commands:"));
    Serial.println(F("  ssid=[name]  : Set WiFi SSID"));         // Lệnh đặt tên WiFi
    Serial.println(F("  pass=[key]   : Set WiFi Password"));     // Lệnh đặt mật khẩu
    Serial.println(F("  id=[name]    : Set Device ID"));         // Lệnh đặt ID thiết bị
    Serial.println(F("  exit         : Save and Exit"));         // Lệnh thoát
    Serial.println(F("--------------------------------------"));
}

/**
 * @brief Vòng lặp chính xử lý việc đăng ký cấu hình qua Serial.
 */
bool EnrollBySerial() {
    unsigned long lastActivity = millis(); // Lưu thời điểm cuối cùng có tương tác
    bool shouldExit = false;               // Biến kiểm soát việc thoát vòng lặp

    printUsage(); // Hiển thị hướng dẫn ngay khi vào chế độ này

    while (!shouldExit) {
        // Kiểm tra nếu quá 30 giây mà không có bất kỳ ký tự nào được gửi lên
        if (millis() - lastActivity > PERIOD_REINTRODUCE) {
            printUsage();             // Gửi lại bảng hướng dẫn
            lastActivity = millis();  // Cập nhật lại thời gian để bắt đầu chu kỳ 30s mới
        }

        // Kiểm tra xem có dữ liệu trong bộ đệm Serial không
        if (Serial.available() > 0) {
            String input = Serial.readStringUntil('\n'); // Đọc chuỗi cho đến khi gặp ký tự xuống dòng
            input.trim();                                // Loại bỏ khoảng trắng hoặc ký tự \r thừa
            
            if (input.length() == 0) continue;           // Nếu chuỗi trống thì bỏ qua lượt này
            
            lastActivity = millis(); // Có dữ liệu mới, cập nhật lại thời điểm tương tác gần nhất

            // Kiểm tra lệnh thoát
            if (input == "exit") {
                Serial.println(F(">> Exiting Config Mode..."));
                shouldExit = true; // Đánh dấu để thoát vòng lặp while
            } 
            // Kiểm tra lệnh nạp SSID
            else if (input.startsWith("ssid=")) {
                String val = input.substring(5); // Cắt lấy phần nội dung sau "ssid="
                configMgr.setWiFiConfig(val, configMgr.params.password); // Lưu vào ConfigManager
                Serial.printf(">> OK: SSID set to [%s]\n", val.c_str());
            } 
            // Kiểm tra lệnh nạp Password
            else if (input.startsWith("pass=")) {
                String val = input.substring(5); // Cắt lấy phần nội dung sau "pass="
                configMgr.setWiFiConfig(configMgr.params.ssid, val); // Lưu vào ConfigManager
                Serial.println(F(">> OK: Password updated."));
            } 
            // Kiểm tra lệnh nạp Device ID
            else if (input.startsWith("id=")) {
                String val = input.substring(3); // Cắt lấy phần nội dung sau "id="
                configMgr.setDeviceID(val);      // Lưu vào ConfigManager
                Serial.printf(">> OK: Device ID set to [%s]\n", val.c_str());
            } 
            // Nếu nhập lệnh không hợp lệ
            else {
                Serial.println(F(">> Unknown command!"));
                printUsage();
            }
        }
        
        yield(); // Nhường quyền xử lý cho hệ thống (WiFi stack) để tránh lỗi Watchdog Timer (WDT)
    }

    return true; // Kết thúc quá trình nạp Serial
}