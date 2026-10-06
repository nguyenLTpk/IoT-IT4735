#ifndef WiFiSelfEnroll_h
#define WiFiSelfEnroll_h
#include <LittleFS.h>
#if defined(ARDUINO_ARCH_ESP32)
    #include <WiFi.h>
    #include <WebServer.h>             
#elif  defined(ARDUINO_ARCH_ESP8266)
    #include <WiFi.h>
    #include <WebServer.h>    
#endif


// @brief the default wifi SSID
#define AP_WIFI_SSID "Esp32_AP"
// @brief the default wifi password
#define AP_WIFI_PASSWORD "12345678"


// @brief AP mode runs indefinitely until user triggers /restart or physical reset.
// The old ADHOC_STATION_DURATION timeout has been removed.

class WiFiSelfEnroll{
    private:
        static String ssid;
        static char myssid[32];
        static String password;
        static char mypassword[20];
        static String deviceid;
        static char mydeviceid[30];
        static WiFiClient wificlient;
#if defined(ARDUINO_ARCH_ESP32)        
        static WebServer server;
#elif  defined(ARDUINO_ARCH_ESP8266)
        static ESP8266WebServer server;
#endif    
    private:
        // @brief true if the Adhoc WiFi is active
        bool    APMode;
        static void _HomePage();
        static void _EnrollPage();
        static void _APIScan() ;
        static void _APISave();
        static void _APISettings();
        static void _Reboot();  
        void _loop();
    public:
        void ReadWiFiConfig();
        void SetupStation(const char * ssid, const char * password);    
        void setup();  
        void setup(const char * adhoc_ssid, const char * adhoc_password);    
        char * GetSSID();
        char * GetPassword();
        char * GetDeviceID();
};
#endif