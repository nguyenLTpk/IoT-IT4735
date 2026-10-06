#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include "HardwareConfig.h"

#if !ENABLE_WIFI
    #error "WiFiManager requires ENABLE_WIFI=1 in HardwareConfig.h"
#endif



enum WIFI_REGISTRATION_METHODS {
    SELF_STATION,
    SERIAL_INTERFACE,
};
 
/**
 * @brief Kết nối lại WiFi nếu cần. Nhưng sẽ bỏ qua nếu @wifiEnabled = false, 
 *        hoặc chưa quá khoảng thời gian interval
 * @param interval 
 * @see wifiEnabled 
 */
void CheckAndEstablishWiFiConnection(unsigned long interval = WIFI_CHECK_INTERVAL);
void WakeupWiFi();
void ShutdownWiFi();

/**
 * @brief Đăng ký thông tin WiFi dựa trên phương thức được chọn.
 * @param method Phương thức đăng ký (SELF_STATION: Web Captive Portal, SERIAL_INTERFACE: Qua Serial)
 * @return bool Trả về kết quả thành công hoặc thất bại của quá trình đăng ký.
 */
bool RegisterWiFi(WIFI_REGISTRATION_METHODS method = WIFI_REGISTRATION_METHODS::SELF_STATION);

#endif
