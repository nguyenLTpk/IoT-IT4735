#include "WiFiSelfEnroll.h"
#include <Arduino.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // nếu board dùng GPIO2 cho LED on-board
#endif

// @brief print more debug information to serial
#define _DEBUG_

#if defined(ARDUINO_ARCH_ESP32)
    // @brief Press BOOT Button whenever restart to force device into Adhoc Station mode
    //#define BOOT_BUTTON GPIO_NUM_9
#elif defined(ARDUINO_ARCH_ESP8266)
    //#define BOOT_BUTTON 0   // GPIO 0  là nút bấm Flash với nội trở kéo lên
#endif    

// @brief  the current ssid. A pointer to parambuf
String WiFiSelfEnroll::ssid;
char WiFiSelfEnroll::myssid[32];
// @brief  the current password. A pointer to parambuf
String WiFiSelfEnroll::password;
char WiFiSelfEnroll::mypassword[20];
// @brief  the current deviceid. A pointer to parambuf
String WiFiSelfEnroll::deviceid;
char WiFiSelfEnroll::mydeviceid[30];
#if defined(ARDUINO_ARCH_ESP32)        
    // @brief  adhoc webserver to configure the new wifi network
    WebServer WiFiSelfEnroll::server(80);
#elif  defined(ARDUINO_ARCH_ESP8266)
    // @brief  adhoc webserver to configure the new wifi network
    ESP8266WebServer WiFiSelfEnroll::server(80);        
#endif    

WiFiClient WiFiSelfEnroll::wificlient;
/*-------------------------------------------------------------------------*/
// @brief send the homepage html to client
// @details entrypoint http://192.168.15.1/
void WiFiSelfEnroll::_HomePage() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

/*-------------------------------------------------------------------------*/
// @brief send the scan wifi html to client
// @details entrypoint http://192.168.15.1/enroll
void WiFiSelfEnroll::_EnrollPage() {
    File file = LittleFS.open("/enroll.html", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

/*-------------------------------------------------------------------------*/
// @brief send wifi list in csv format to web client
// @details entrypoint http://192.168.15.1/cgi/scan
// @example 812A,12345678,dce-ktmt,66668888
void WiFiSelfEnroll::_APIScan()  {
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    String res = String(200);
    res = "";
    if (n != 0) {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            res = res + WiFi.SSID(i) + ',' + WiFi.RSSI(i);
            if (i != n-1) {res = res +',';}
            Serial.println(res);
        delay(10);
        }
    }
#ifdef _DEBUG_        
    Serial.println(res);
#endif
    server.send(200,"text/plain", res);
}
/*-------------------------------------------------------------------------*/
// @brief WebAPI: receive ssid, pass from the HttpGet and save to permanent storage
// @details exntrypoint http://192.168.15.1/cgi/save?s=ssid&p=password
void WiFiSelfEnroll::_APISave()  {
#ifdef _DEBUG_       
    Serial.println("WiFiSelf: /cgi/save");
    Serial.println(server.uri());
#endif        
    
    // Handle "clear all params" (Factory Reset)
    if (server.hasArg("clear") && server.arg("clear") == "true") {
        if (LittleFS.exists("/ssid.txt")) LittleFS.remove("/ssid.txt");
        if (LittleFS.exists("/password.txt")) LittleFS.remove("/password.txt");
        if (LittleFS.exists("/deviceid.txt")) LittleFS.remove("/deviceid.txt");
        
        ssid = "";
        password = "";
        deviceid = "";
        server.send(200, "text/plain", "Factory Reset complete! Restart to apply.");
        return;
    }

    String myArg="s";
    // Save SSID
    if (server.hasArg(myArg)) {
        ssid = server.arg(myArg);
        File f = LittleFS.open("/ssid.txt", "w");
        f.print(ssid);
        f.close();
    }    

    // Save Password
    myArg="p";
    if (server.hasArg(myArg)) {
        password = server.arg(myArg);
        File f = LittleFS.open("/password.txt", "w");
        f.print(password);
        f.close();
    }    

    /// Save DeviceID
    myArg="d";
    if (server.hasArg(myArg)) {
        deviceid = server.arg(myArg);
        File f = LittleFS.open("/deviceid.txt", "w");
        f.print(deviceid);
        f.close();
    }    

    /// Response to the web client
    server.send(200, "text/plain", "Saved! Restart to apply.");
}

/*-------------------------------------------------------------------------*/
// @brief WebAPI: reponse the wifi configuration and device id
// @details exntrypoint http://192.168.15.1/cgi/save?n=ssid&p=password
void WiFiSelfEnroll::_APISettings()  {
    char buf[50];
    (ssid + "," + password + ',' + deviceid).toCharArray(buf,50);
#ifdef _DEBUG_       
    Serial.print("WiFiSelf: /cgi/settings - ");
    Serial.println(buf);
#endif
    // Response to the web client
    server.send(200,"text/plain", buf);
}

/*-------------------------------------------------------------------------*/
// @brief Read the ssid / deviceid into flash memory
// @return e.g. '812A,12345678'
void WiFiSelfEnroll::ReadWiFiConfig()  {
#ifdef _DEBUG_       
    Serial.printf("ReadWiFiConfig");
#endif    
    // Control the flash memory with its idendification namespace, and read mode
    if (LittleFS.exists("/ssid.txt")) {
        File f = LittleFS.open("/ssid.txt", "r");
        ssid = f.readStringUntil('\n');
        f.close();
    }

    if (LittleFS.exists("/deviceid.txt")) {
        File f = LittleFS.open("/deviceid.txt", "r");
        deviceid = f.readStringUntil('\n');
        f.close();
    }    

    if (LittleFS.exists("/password.txt")) {
        File f = LittleFS.open("/password.txt", "r");
        password = f.readStringUntil('\n');
        f.close();
    }  
}        
    
/*-------------------------------------------------------------------------*/
// @brief Reboot the device — shows countdown page first, then restarts
void WiFiSelfEnroll::_Reboot()  {
    // Send a nice farewell page before rebooting
    String html = "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Restarting...</title>"
        "<style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,sans-serif;background:#0f172a;color:#e2e8f0;"
        "min-height:100vh;display:flex;align-items:center;justify-content:center}"
        ".card{background:#1e293b;border:1px solid #334155;border-radius:16px;"
        "width:90%;max-width:400px;padding:2.5rem;text-align:center;"
        "box-shadow:0 20px 60px rgba(0,0,0,.5)}"
        ".icon{font-size:3rem;margin-bottom:1rem;animation:spin 2s linear infinite}"
        "@keyframes spin{from{transform:rotate(0)}to{transform:rotate(360deg)}}"
        "h2{font-size:1.3rem;margin-bottom:.5rem}"
        "p{color:#94a3b8;font-size:1rem;margin-bottom:1.5rem}"
        ".countdown{font-size:2.5rem;font-weight:800;color:#818cf8}"
        ".hint{font-size:.85rem;color:#64748b;margin-top:1.5rem}"
        "</style>"
        "</head><body><div class='card'>"
        "<div class='icon'>&#9881;</div>"
        "<h2>Restarting Device</h2>"
        "<p>Please wait while the device reboots...</p>"
        "<div class='countdown' id='cd'>3</div>"
        "<p class='hint'>WiFi AP will close. Reconnect to your normal WiFi.</p>"
        "</div>"
        "<script>"
        "let c=3;setInterval(()=>{c--;if(c>=0)document.getElementById('cd').textContent=c;"
        "if(c===0)document.getElementById('cd').textContent='Bye!';},1000);"
        "</script></body></html>";

    server.send(200, "text/html", html);
    
    // Give time for the HTTP response to be sent, then reboot
    delay(3500);
    
    #ifdef _DEBUG_             
    Serial.println("restart...");
    #endif              
    ESP.restart();
}

/*-------------------------------------------------------------------------*/
// @brief setup the Adhoc wifi
// @param ssid     Wifi name. E.g "My WiFi"
// @param password  Wifi secret password.
void WiFiSelfEnroll::SetupStation(const char * adhoc_ssid, const char * adhoc_password) {
    APMode = true;
    
    // Read the current wifi config
    ReadWiFiConfig();

    IPAddress local_ip(192,168,15,1);
    IPAddress gateway(192,168,15,1);
    IPAddress subnet(255,255,255,0);

    //LED_BUILTIN is used for wifi indicator 
    // pinMode(LED_BUILTIN, OUTPUT);

    // Indicator: start the wifi
    digitalWrite(LED_BUILTIN, HIGH);       

#ifdef _DEBUG_
    Serial.print("Adhoc WiFi: ");
    Serial.print(adhoc_ssid);
    Serial.print(" / ");
    Serial.println(adhoc_password);
#endif    
    
    // set as a Wi-Fi station and access point simultaneously
    WiFi.mode(WIFI_AP_STA);

    // Broadcast the Adhoc WiFi
    if (WiFi.softAPConfig(local_ip,gateway,subnet)) {
#ifdef _DEBUG_
        Serial.println("WiFi configuration is okay");
#endif                
    }
    if (WiFi.softAP(adhoc_ssid, adhoc_password,9,false)) {
#ifdef _DEBUG_
        Serial.println("WiFi is ready!");
#endif                 
    }

#ifdef _DEBUG_        
    // WiFi is ready
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
#endif    

    // Route the website
    server.on("/", _HomePage);
    server.on("/enroll", _EnrollPage);
    server.on("/cgi/scan", _APIScan);
    server.on("/cgi/save", _APISave);
    server.on("/cgi/settings", _APISettings);
    server.on("/restart", _Reboot);
    // Show the own website
    server.begin();
    _loop();
    _Reboot();
}

// @brief loop with led indicator — runs indefinitely until user restarts via web UI (/restart)
void WiFiSelfEnroll::_loop()  {          
    // AP mode runs forever — only exits when user presses /restart from the web interface
    // or physically resets the device.
    while (true) {
    #ifdef _DEBUG_     
        if (APMode) {
            Serial.printf("Stations connected to soft-AP = %d \n", WiFi.softAPgetStationNum());
        }
    #endif        
        //polling and call event functions
        server.handleClient();
        //Indicator
        digitalWrite(LED_BUILTIN, HIGH);       
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);        
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);       
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);                 
        delay(1000);
    }
}

char * WiFiSelfEnroll::GetSSID()  { 
    ssid.toCharArray(myssid, 30);
    return myssid;
}

char * WiFiSelfEnroll::GetPassword()  { 
    password.toCharArray(mypassword, 20);
    return mypassword;
}

char * WiFiSelfEnroll::GetDeviceID()  { 
    deviceid.toCharArray(mydeviceid, 30);
    return mydeviceid;
}


// @brief make sure WiFi ssid/password is correct. Otherwise, raise the Adhoc AP Station with ssid = SOICT_CORE_BOARD and password =  12345678
void WiFiSelfEnroll::setup() {
    WiFiSelfEnroll::setup(AP_WIFI_SSID, AP_WIFI_PASSWORD);
}

// @brief make sure WiFi ssid/password is correct. Otherwise, raise the Adhoc AP Station to enter the new config
// @param ssid     Wifi name. E.g "My WiFi"
// @param password  Wifi secret password.
// @note should let it at the first part of the global setup() function in Arduino Code.
// @example  WiFiSelfEnroll MyWiFi;  MyWiFi.setup("ABC","12345678");  
void WiFiSelfEnroll::setup(const char * adhoc_ssid, const char * adhoc_password) {
    SetupStation(adhoc_ssid, adhoc_password);
}
