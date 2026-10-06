#ifndef WIFI_ENROLL_BY_SERIAL_H
#define WIFI_ENROLL_BY_SERIAL_H

#include <Arduino.h>
#include "ConfigManager.h"

/**
 * @brief Kích hoạt chế độ nhập cấu hình từ cổng Serial.
 *        Hàm này sẽ chặn luồng chính, đợi người dùng nhập thông tin SSID, Password và Device ID.
 * Cú pháp: ssid=..., pass=..., id=...
 * Thoát bằng lệnh: exit
 * @return bool Trả về true sau khi thoát chế độ cấu hình.
 */
bool EnrollBySerial();

#endif