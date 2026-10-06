#include <LittleFS.h>
#include <WiFi.h>
#include "WifiManager.h"
#include "WiFiSelfEnroll.h"
#include "WiFiEnrollBySerial.h"
#include "configmanager.h"
#include "main.h"

bool wifiStatus = false;
unsigned long lastWifiCheck = 0;

static bool wifiConnectAttemptInProgress = false;
static unsigned long wifiConnectStartMs = 0;
static unsigned long wifiLastProgressLogMs = 0;
static wl_status_t wifiLastStatusLogged = WL_IDLE_STATUS;
static bool wifiEventLoggerInitialized = false;

static const unsigned long WIFI_CONNECT_PROGRESS_LOG_MS = 3000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 60000;

static const char *WiFiStatusToString(wl_status_t status)
{
    switch (status)
    {
    case WL_IDLE_STATUS:
        return "IDLE";
    case WL_NO_SSID_AVAIL:
        return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
        return "SCAN_COMPLETED";
    case WL_CONNECTED:
        return "CONNECTED";
    case WL_CONNECT_FAILED:
        return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
        return "CONNECTION_LOST";
    case WL_DISCONNECTED:
        return "DISCONNECTED";
    default:
        return "UNKNOWN";
    }
}

/* 
 Chỉ có tác dụng in cấu hình kết nối Wifi, dùng khi debug
 */
static void WiFi_LogConnectionSnapshot(const char *prefix)
{
    const wl_status_t st = WiFi.status();
    Serial.print(prefix);
    Serial.print(F(" status="));
    Serial.print(WiFiStatusToString(st));
    Serial.print(F("("));
    Serial.print((int)st);
    Serial.print(F(")"));
    Serial.print(F(" ssid='"));
    Serial.print(configMgr.params.ssid);
    Serial.print(F("' mac="));
    Serial.print(WiFi.macAddress());

    if (st == WL_CONNECTED)
    {
        Serial.print(F(" ip="));
        Serial.print(WiFi.localIP());
        Serial.print(F(" rssi="));
        Serial.print(WiFi.RSSI());
    }

    Serial.println();
}

#if defined(ARDUINO_ARCH_ESP32)
static void WiFi_OnEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println(F("[WiFi][Event] STA connected to AP."));
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.print(F("[WiFi][Event] STA disconnected, reason="));
        Serial.println((int)info.wifi_sta_disconnected.reason);
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print(F("[WiFi][Event] STA got IP: "));
        Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
        break;
    default:
        break;
    }
}

static void WiFi_EnsureEventLogger()
{
    if (!wifiEventLoggerInitialized)
    {
        WiFi.onEvent(WiFi_OnEvent);
        wifiEventLoggerInitialized = true;
        Serial.println(F("[WiFi] Event logger initialized."));
    }
}
#else
static void WiFi_EnsureEventLogger() {}
#endif

void CheckAndEstablishWiFiConnection(unsigned long interval)
{
    WiFi_EnsureEventLogger();

    // 1. Quản lý vô hiệu hoá WiFi bằng phần mềm
    if (!configMgr.params.wifiEnabled)
    {
        if (wifiStatus)
        {
            WiFi.disconnect();
            wifiStatus = false;
            wifiConnectAttemptInProgress = false; // Tránh treo cờ
            Serial.println(F("WiFi Disabled. Disconnected."));
        }
        return; // Dừng lại ở đây
    }

    // 2. Cập nhật và theo dõi trạng thái WiFi hiện tại
    unsigned long currentMillis = millis();
    wl_status_t currentStatus = WiFi.status();
    wifiStatus = (currentStatus == WL_CONNECTED);

    if (currentStatus != wifiLastStatusLogged)
    {
        Serial.print(F("[WiFi] Status transition: "));
        Serial.print(WiFiStatusToString(wifiLastStatusLogged));
        Serial.print(F(" -> "));
        Serial.println(WiFiStatusToString(currentStatus));
        wifiLastStatusLogged = currentStatus;
    }

    // 3. State Machine (Cỗ máy trạng thái) của quá trình duy trì WiFi
    if (wifiStatus)
    {
        // Trạng thái 3A: Đã có mạng
        if (wifiConnectAttemptInProgress)
        {
            WiFi_LogConnectionSnapshot("[WiFi] Connected:");
            wifiConnectAttemptInProgress = false;
        }

        // Vẫn in log định kỳ báo đang có mạng
        if (currentMillis - lastWifiCheck >= interval || lastWifiCheck == 0)
        {
            lastWifiCheck = currentMillis;
            WiFi_LogConnectionSnapshot("[WiFi] Already connected:");
        }
    }
    else if (wifiConnectAttemptInProgress)
    {
        // Trạng thái 3B: Đang đợi bắt mạng (Attempting)
        if (currentMillis - wifiLastProgressLogMs >= WIFI_CONNECT_PROGRESS_LOG_MS)
        {
            wifiLastProgressLogMs = currentMillis;
            WiFi_LogConnectionSnapshot("[WiFi] Connecting...");
        }

        // Timeout 60s
        if (currentMillis - wifiConnectStartMs >= WIFI_CONNECT_TIMEOUT_MS)
        {
            WiFi_LogConnectionSnapshot("[WiFi] Connect timeout:");
            Serial.println(F("[WiFi] Timeout reached, forcing disconnect before next retry."));
            WiFi.disconnect();
            wifiConnectAttemptInProgress = false; // Quay về Trạng thái 3C để thử lại sau
        }
    }
    else
    {
        // Trạng thái 3C: Đã mất mạng và đang ở chế độ Rảnh (Idle)
        // Chờ đủ chu kỳ (interval) để tiến hành xin kết nối lại
        if (currentMillis - lastWifiCheck >= interval || lastWifiCheck == 0)
        {
            lastWifiCheck = currentMillis;

            WiFi.mode(WIFI_STA);
            WiFi.begin(configMgr.params.ssid.c_str(), configMgr.params.password.c_str());
            
            Serial.println(F("Attempting to reconnect WiFi..."));
            WiFi_LogConnectionSnapshot("[WiFi] Begin connect:");
            
            wifiConnectAttemptInProgress = true; // Chuyển sang Trạng thái 3B
            wifiConnectStartMs = currentMillis;
            wifiLastProgressLogMs = currentMillis;
        }
    }
}

void WakeupWiFi()
{
#ifdef _DEBUG_
    Serial.println("[WiFi] Waking up WiFi...");
#endif

#if defined(ARDUINO_ARCH_ESP8266)
    // Đánh thức ESP8266 khỏi chế độ forceSleep
    WiFi.forceSleepWake();
    delay(1);
#endif

    // Khởi động lại chế độ Station
    WiFi.mode(WIFI_STA);

    WiFi_LogConnectionSnapshot("[WiFi] Wakeup state:");

    // Ghi nhớ đã bật wifi
    configMgr.setWifiEnabled(true);
}

void ShutdownWiFi()
{
#ifdef _DEBUG_
    Serial.println("[WiFi] Shutting down WiFi to save power...");
#endif

    WiFi.disconnect(true); // Ngắt kết nối và xóa cấu hình tạm thời
    WiFi.mode(WIFI_OFF);   // Tắt chip Radio RF

#if defined(ARDUINO_ARCH_ESP8266)
    WiFi.forceSleepBegin(); // Lệnh đặc biệt cho ESP8266 để ngủ sâu hơn
#endif

    // Ghi nhớ đã tắt wifi
    configMgr.setWifiEnabled(false);
}

/** Quản lý cấu hình wifi động*/
WiFiSelfEnroll *MyWiFi = nullptr;

bool RegisterWiFi(WIFI_REGISTRATION_METHODS method)
{
    switch (method)
    {
    case WIFI_REGISTRATION_METHODS::SELF_STATION:
    {
        if (MyWiFi == nullptr)
        {
            MyWiFi = new WiFiSelfEnroll();
        }
        // Kích hoạt trạm phát AP để user vào cài đặt
        MyWiFi->setup();
        return true;
    }
    case WIFI_REGISTRATION_METHODS::SERIAL_INTERFACE:
    {
        return EnrollBySerial(); // Gọi hàm từ module mới
    }
    }
    return false;
}
