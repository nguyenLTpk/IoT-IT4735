#include "ConfigManager.h"

ConfigManager::ConfigManager() {}

bool ConfigManager::begin() {
    return LittleFS.begin();
}

void ConfigManager::readFile(const char* path, String &val) {
    if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
            val = f.readStringUntil('\n');
            val.trim();
            f.close();
        }
    }
}

void ConfigManager::writeFile(const char* path, String val) {
    File f = LittleFS.open(path, "w");
    if (f) {
        f.println(val);
        f.close();
    }
}

void ConfigManager::loadAll() {
    readFile(FILE_WIFI_SSID, params.ssid);
    readFile(FILE_WIFI_PASS, params.password);
    readFile(FILE_DEVICE_ID, params.deviceID);
    
    String temp;
    readFile(FILE_WIFI_ENABLE, temp);
    // Nếu file trống (mới format), mặc định là "1" (bật)
    params.wifiEnabled = (temp != "0"); 
    
    readFile(FILE_MQTT_ENABLE, temp);
    params.mqttEnabled = (temp != "0"); 
}

void ConfigManager::saveAll() {
    writeFile(FILE_WIFI_SSID, params.ssid);
    writeFile(FILE_WIFI_PASS, params.password);
    writeFile(FILE_DEVICE_ID, params.deviceID);
    writeFile(FILE_WIFI_ENABLE, params.wifiEnabled ? "1" : "0");
    writeFile(FILE_MQTT_ENABLE, params.mqttEnabled ? "1" : "0");
}

void ConfigManager::setWifiEnabled(bool enabled) {
    params.wifiEnabled = enabled;
    writeFile(FILE_WIFI_ENABLE, enabled ? "1" : "0");
}

void ConfigManager::setMqttEnabled(bool enabled) {
    params.mqttEnabled = enabled;
    writeFile(FILE_MQTT_ENABLE, enabled ? "1" : "0");
}

void ConfigManager::setWiFiConfig(String ssid, String pass) {
    params.ssid = ssid;
    params.password = pass;
    writeFile(FILE_WIFI_SSID, ssid);
    writeFile(FILE_WIFI_PASS, pass);
}

void ConfigManager::setDeviceID(String id) {
    params.deviceID = id;
    writeFile(FILE_DEVICE_ID, id);
}

ConfigManager configMgr;