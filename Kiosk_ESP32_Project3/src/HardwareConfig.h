#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/**
 * ============================================================================
 * HARDWARE CONFIGURATION HEADER
 * ============================================================================
 * This file centralizes all hardware-specific configurations including:
 * - MCU selection (ESP32, STM32, Arduino, etc.)
 * - Pin definitions
 * - Peripheral library selection
 * - Feature flags
 * 
 * Usage:
 * 1. Define your MCU in platformio.ini: -DMCU_IS_ESP32, -DMCU_IS_STM32, etc.
 * 2. All other configurations will be set automatically based on MCU type
 * 3. Override default pins by defining them before including this file
 * ============================================================================
 */

// ============================================================================
// 1. MCU SELECTION (define exactly ONE of these)
// ============================================================================

#if !defined(MCU_IS_ESP32) && !defined(MCU_IS_STM32) && !defined(MCU_IS_ARDUINO_MEGA) && \
    !defined(MCU_IS_ARDUINO_UNO) && !defined(MCU_IS_CUSTOM)
    #error "Must define one MCU type: MCU_IS_ESP32, MCU_IS_STM32, MCU_IS_ARDUINO_MEGA, MCU_IS_ARDUINO_UNO, or MCU_IS_CUSTOM"
#endif

// ============================================================================
// 2. FEATURE ENABLE/DISABLE FLAGS
// ============================================================================

// Display features
#ifndef ENABLE_OLED_DISPLAY
    #define ENABLE_OLED_DISPLAY 1  // 0 = disable, 1 = enable
#endif

// Communication modules
#ifndef ENABLE_NFC_MFRC522
    #define ENABLE_NFC_MFRC522 1   // 0 = disable, 1 = enable MFRC522 NFC module
#endif

#ifndef ENABLE_RFID_125KHZ
    #define ENABLE_RFID_125KHZ 1   // 0 = disable, 1 = enable 125kHz RFID module
#endif

#ifndef ENABLE_WIFI
    #define ENABLE_WIFI 1          // 0 = disable, 1 = enable WiFi
#endif

#ifndef ENABLE_MQTT
    #define ENABLE_MQTT 1          // 0 = disable, 1 = enable MQTT
#endif

#ifndef ENABLE_BUZZER
    #define ENABLE_BUZZER 1        // 0 = disable, 1 = enable Buzzer
#endif

#ifndef ENABLE_CONFIG_BUTTON
    #define ENABLE_CONFIG_BUTTON 1 // 0 = disable, 1 = enable Config button
#endif

#ifndef ENABLE_QR_SCANNER
    #define ENABLE_QR_SCANNER 1    // 0 = disable, 1 = enable UART QR scanner
#endif

// ============================================================================
// 3. LIBRARY SELECTION
// ============================================================================

// OLED Display library and type
#ifndef OLED_DISPLAY_TYPE
    #define OLED_DISPLAY_TYPE_SH1106 1  // Options: SH1106, SSD1306
#endif

#ifndef RFID_MODULE_TYPE
    #define RFID_MODULE_TYPE_RDM6300 1  // Options: RDM6300, EM4100, etc.
#endif

// ============================================================================
// 4. ESP32 PIN CONFIGURATION
// ============================================================================

#ifdef MCU_IS_ESP32

    // I2C pins for OLED display
    #ifndef I2C_SDA_PIN
        #define I2C_SDA_PIN 21
    #endif
    #ifndef I2C_SCL_PIN
        #define I2C_SCL_PIN 22
    #endif

    // SPI pins for NFC (MFRC522)
    #ifndef SPI_MOSI_PIN
        #define SPI_MOSI_PIN 23
    #endif
    #ifndef SPI_MISO_PIN
        #define SPI_MISO_PIN 19
    #endif
    #ifndef SPI_SCK_PIN
        #define SPI_SCK_PIN 18
    #endif
    #ifndef NFC_SS_PIN
        #define NFC_SS_PIN 5
    #endif
    #ifndef NFC_RST_PIN
        #define NFC_RST_PIN 32
    #endif

    // UART pins for 125kHz RFID
    #ifndef RFID_RX_PIN
        #define RFID_RX_PIN 16
    #endif
    #ifndef RFID_TX_PIN
        #define RFID_TX_PIN 17
    #endif

    // UART pins for QR scanner module (MH-ET LIVE)
    #ifndef QR_RX_PIN
        #define QR_RX_PIN 27
    #endif
    #ifndef QR_TX_PIN
        #define QR_TX_PIN 33
    #endif

    // GPIO pins
    #ifndef BUZZER_PIN
        #define BUZZER_PIN 12
    #endif
    #ifndef CFG_BUTTON_PIN
        #define CFG_BUTTON_PIN 4
    #endif
    // #ifndef LED_BUILTIN
    //     #define LED_BUILTIN 2
    // #endif

    // UART baudrates
    #ifndef SERIAL_DEBUG_BAUD
        #define SERIAL_DEBUG_BAUD 115200
    #endif
    #ifndef RFID_BAUD
        #define RFID_BAUD 9600
    #endif
    #ifndef QR_BAUD
        #define QR_BAUD 9600
    #endif

#endif // MCU_IS_ESP32

// ============================================================================
// 5. STM32 PIN CONFIGURATION
// ============================================================================

#ifdef MCU_IS_STM32

    // Add define for STM32 if needed, similar to ESP32 section.

#endif // MCU_IS_STM32

// ============================================================================
// 6. ARDUINO MEGA PIN CONFIGURATION
// ============================================================================

#ifdef MCU_IS_ARDUINO_MEGA

   // Add define for Arduino Mega if needed, similar to ESP32 section.

#endif // MCU_IS_ARDUINO_MEGA

// ============================================================================
// 7. ARDUINO UNO PIN CONFIGURATION
// ============================================================================

#ifdef MCU_IS_ARDUINO_UNO

    // Add define for Arduino Uno if needed, similar to ESP32 section.

#endif // MCU_IS_ARDUINO_UNO

// ============================================================================
// 8. CUSTOM CONFIGURATION
// ============================================================================

#ifdef MCU_IS_CUSTOM
    #warning "MCU_IS_CUSTOM selected. Please define all pins manually in your build flags or this file!"
    
    // Define your custom pins here or in platformio.ini

#endif

// ============================================================================
// 9. FIRMWARE INFORMATION
// ============================================================================

#ifndef FIRMWARE_VERSION
    #define FIRMWARE_VERSION "2.2"
#endif

#ifndef DEVICE_NAME
    #define DEVICE_NAME "STUDENT MONITOR"
#endif

// ============================================================================
// 10. FILESYSTEM CONFIGURATION
// ============================================================================

// LittleFS file definitions
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

// ============================================================================
// 11. COMMUNICATION SETTINGS
// ============================================================================

#ifndef WIFI_CHECK_INTERVAL
    #define WIFI_CHECK_INTERVAL 60000  // milliseconds
#endif

#ifndef MQTT_BROKER_ADDRESS
    #define MQTT_BROKER_ADDRESS "mqtt.toolhub.app"
#endif

#ifndef MQTT_BROKER_PORT
    #define MQTT_BROKER_PORT 1883
#endif

// Default WiFi credentials (can be overridden from Flash/LittleFS at runtime)
#ifndef WIFI_SSID_NAME
    #define WIFI_SSID_NAME "DefaultSSID"
#endif

#ifndef WIFI_SSID_PASS
    #define WIFI_SSID_PASS "DefaultPassword"
#endif

// ============================================================================
// 12. DISPLAY CONFIGURATION
// ============================================================================

#if ENABLE_OLED_DISPLAY

    #ifdef OLED_DISPLAY_TYPE_SH1106
        // SH1106 OLED 128x64 I2C
        #define OLED_DISPLAY_WIDTH 128
        #define OLED_DISPLAY_HEIGHT 64
        #define OLED_I2C_ADDRESS 0x3C
    #elif defined(OLED_DISPLAY_TYPE_SSD1306)
        // SSD1306 OLED 128x64 I2C
        #define OLED_DISPLAY_WIDTH 128
        #define OLED_DISPLAY_HEIGHT 64
        #define OLED_I2C_ADDRESS 0x3C
    #else
        #error "OLED_DISPLAY_TYPE must be defined: OLED_DISPLAY_TYPE_SH1106 or OLED_DISPLAY_TYPE_SSD1306"
    #endif

#endif // ENABLE_OLED_DISPLAY

// ============================================================================
// 13. DEBUG OUTPUT
// ============================================================================

// Uncomment to enable detailed debug output
// #define DEBUG_HARDWARE_CONFIG

#ifdef DEBUG_HARDWARE_CONFIG
    #warning "DEBUG_HARDWARE_CONFIG is enabled. This will increase code size."
#endif

#endif // HARDWARE_CONFIG_H
