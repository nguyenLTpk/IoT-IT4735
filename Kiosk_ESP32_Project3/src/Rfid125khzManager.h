#pragma once

#include <Arduino.h>
#include "HardwareConfig.h"

#if !ENABLE_RFID_125KHZ
	#error "Rfid125khzManager requires ENABLE_RFID_125KHZ=1 in HardwareConfig.h"
#endif


// Khởi tạo UART cho RFID
void rfid_init();

// Gọi thường xuyên trong loop để cập nhật dữ liệu từ RFID
void rfid_update();

// Trả về true nếu vừa đọc được một mã thẻ đầy đủ
bool rfid_has_new_tag();

// Lấy nội dung thẻ vừa đọc (chuỗi ASCII, ví dụ mã HEX), đồng thời clear cờ new_tag
String rfid_get_last_tag();

// Xả bộ đệm Serial2 và reset cooldown — gọi SAU KHI xử lý thẻ xong
void rfid_flush();
