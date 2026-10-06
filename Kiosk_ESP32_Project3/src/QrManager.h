#pragma once

#include <Arduino.h>
#include "HardwareConfig.h"

// Khởi tạo UART cho module QR scanner
void qr_init();

// Gọi thường xuyên trong loop để cập nhật dữ liệu từ QR scanner
void qr_update();

// Trả về true nếu vừa đọc được một QR payload hoàn chỉnh
bool qr_has_new_payload();

// Lấy payload vừa đọc và reset cờ dữ liệu mới
String qr_get_last_payload();
