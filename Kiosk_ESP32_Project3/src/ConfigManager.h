#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

// --- ĐỊNH NGHĨA TÊN FILE TRONG LITTLEFS ---
#ifndef FILE_WIFI_SSID
    #define FILE_WIFI_SSID "/ssid.txt"
#endif

#ifndef FILE_WIFI_PASS
    #define FILE_WIFI_PASS "/password.txt"
#endif

#ifndef FILE_DEVICE_ID
    #define FILE_DEVICE_ID "/deviceid.txt"
#endif

#ifndef FILE_WIFI_ENABLE
    #define FILE_WIFI_ENABLE "/wifienabled.txt"
#endif

#ifndef FILE_MQTT_ENABLE
    #define FILE_MQTT_ENABLE "/mqtt_enabled.txt"
#endif

struct PersistentParams {
    String ssid;
    String password;
    String deviceID;
    bool wifiEnabled;
    bool mqttEnabled;
};

class ConfigManager {
public:
    PersistentParams params;

    ConfigManager();
    bool begin();
    void loadAll();
    void saveAll();

    // Các hàm cập nhật từng thành phần và lưu ngay
    void setWifiEnabled(bool enabled);
    void setMqttEnabled(bool enabled);
    void setWiFiConfig(String ssid, String pass);
    void setDeviceID(String id);

private:
    void readFile(const char* path, String &val);
    void writeFile(const char* path, String val);
};

extern ConfigManager configMgr; 

#endif
