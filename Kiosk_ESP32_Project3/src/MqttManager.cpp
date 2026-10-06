#include "MqttManager.h"
#include <ArduinoJson.h>

// Các biến toàn cục để giao tiếp giữa MQTT Callback và hàm xử lý thẻ
// (khai báo extern trong MqttManager.h để main.cpp đọc được)
bool mqttVerifyReceived = false;
bool mqttVerifyAccepted = false;
String mqttVerifyMssv = "";
String mqttVerifyName = "";
String mqttVerifyTime = "";     // [MỚI] Giờ hẹn
String mqttVerifyQueue = "";    // [MỚI] Số thứ tự
String mqttVerifyMsg = "";      // [MỚI] Thông điệp hiển thị dòng 4
uint32_t mqttVerifyReqId = 0;   // [MỚI] req_id backend trả lại (0 = không có)

void mqttClearVerifyResult() {
    mqttVerifyReceived = false;
    mqttVerifyAccepted = false;
    mqttVerifyMssv = "";
    mqttVerifyName = "";
    mqttVerifyTime = "";
    mqttVerifyQueue = "";
    mqttVerifyMsg = "";
    mqttVerifyReqId = 0;
}

/**
 * @brief [MỚI] Đọc 1 trường JSON thành String, chấp nhận cả kiểu chuỗi lẫn kiểu số.
 * Ví dụ backend có thể gửi "queue": "05" hoặc "queue": 5 -> đều ra "05".
 * Trường không tồn tại / null -> trả về chuỗi rỗng (KHÔNG phải chữ "null").
 * Dùng JsonVariantConst để chạy được với cả ArduinoJson v6 và v7.
 */
static String jsonFieldToString(JsonVariantConst v, bool padTwoDigits = false) {
    if (v.isNull()) return "";
    if (v.is<const char *>()) {
        String s = v.as<const char *>();
        s.trim();
        return s;
    }
    if (v.is<long>()) {
        const long n = v.as<long>();
        // Số thứ tự kiểu số (5) -> hiển thị "05" cho đồng nhất với kiểu chuỗi
        if (padTwoDigits && n >= 0 && n < 10) return "0" + String(n);
        return String(n);
    }
    if (v.is<float>()) return String(v.as<float>(), 2);
    if (v.is<bool>()) return v.as<bool>() ? "true" : "false";
    return "";
}

/** [MỚI] Escape chuỗi trước khi nhúng vào JSON (dấu ", \ và ký tự điều khiển). */
static String jsonEscapeValue(const String &input) {
    String out;
    out.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); i++) {
        const char c = input.charAt(i);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if ((uint8_t)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(uint8_t)c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

/** [MỚI] Đóng gói payload quét thẻ: {"device_id":"..","<field>":".."[,"req_id":N]} */
static String buildScanPayload(const char *field, const String &value, uint32_t reqId) {
    String payload;
    payload.reserve(48 + configMgr.params.deviceID.length() + value.length());
    payload += "{\"device_id\":\"";
    payload += jsonEscapeValue(configMgr.params.deviceID);
    payload += "\",\"";
    payload += field;
    payload += "\":\"";
    payload += jsonEscapeValue(value);
    payload += "\"";
    if (reqId != 0) {
        payload += ",\"req_id\":";
        payload += String((unsigned long)reqId);
    }
    payload += "}";
    return payload;
}


/**  */
bool startupConfirmed = false;
unsigned long lastStartupAttempt = 0;

MqttManager::MqttManager() : client(espClient) {}

bool MqttManager::connected() {
    return client.connected();
}

void MqttManager::disconnect() {
    if (client.connected()) {
        client.disconnect();
    }
    lastReconnectAttemptStatus = false;
}


/**
 * @brief Gửi thông tin định danh hệ thống khi mới khởi động
 * Định dạng CSV: deviceID, SSID, Password, MAC
 */
void MqttManager::sendStartupPacket() {
    if (!client.connected()) return;

    char startupBuf[212];
    String mac = WiFi.macAddress();

    // Đóng gói thông tin nhạy cảm
    snprintf(startupBuf, sizeof(startupBuf), "%s,%s,%s,%s,%s",
             configMgr.params.deviceID.c_str(),
             WiFi.SSID().c_str(),
             mac.c_str(),
             topicUp,
             topicDown);

    Serial.println(F("[MQTT] Sending Startup Packet..."));

    // Gửi cho đến khi nhận được ACK hoặc thử lại trong loop
    client.publish(MQTT_TOPIC_STARTUP, startupBuf, true);
}

/**
 * @brief Khởi tạo và chuẩn bị các chuỗi Topic
 * @return True nếu kết nối thành công với MQTT Broker
 */
bool MqttManager::setup() {
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(this->callback);

    // [MỚI] Nới buffer (mặc định 256 byte) để không bị rớt gói verify_result dài
    // (tên có dấu + time/queue/msg) và gói QR dài (QR payload tối đa 512 ký tự).
    if (!client.setBufferSize(MQTT_BUFFER_SIZE)) {
        Serial.println(F("[MQTT] Canh bao: khong cap phat duoc buffer MQTT, giu kich thuoc cu."));
    }

    // Khởi tạo tên topic dựa trên DeviceID đã lưu trong Flash
    sprintf(topicUp, MQTT_TOPIC_UP_TEMPLATE, configMgr.params.deviceID.c_str());
    sprintf(topicDown, MQTT_TOPIC_DOWN_TEMPLATE, configMgr.params.deviceID.c_str());
    sprintf(topicRfid, MQTT_TOPIC_RFID_TEMPLATE, configMgr.params.deviceID.c_str());
    sprintf(topicNfc, MQTT_TOPIC_NFC_TEMPLATE, configMgr.params.deviceID.c_str());
    sprintf(topicQr, MQTT_TOPIC_QR_TEMPLATE, configMgr.params.deviceID.c_str());


    Serial.println(F("Kiểm tra MQTT Broker duy nhất khi khởi động.."));

    // Nếu WiFi chưa kết nối thì không cố gắng resolve DNS ngay,
    // tránh spam lỗi hostByName() DNS Failed lúc khởi động.
    if (!WiFi.isConnected()) {
        Serial.println(F("[MQTT] WiFi chua san sang, se thu ket noi trong loop()."));
        lastReconnectAttemptStatus = false;
        return false;
    }

    String mqttClientId = MQTT_CLIENT_ID_PREFIX + configMgr.params.deviceID;
    lastReconnectAttemptStatus = client.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASS);
    if (lastReconnectAttemptStatus) {
        Serial.println(F("MQTT thanh cong!"));
        client.subscribe(topicDown);
        // Gửi gói Startup ngay khi kết nối thành công lần đầu
        sendStartupPacket();
        return true;
    }

    return false;
}

bool MqttManager::isLastConnectionToBrokerOk()
{
    return lastReconnectAttemptStatus;
}

/**
 * @brief Xử lý dữ liệu nhận được từ Topic Downstream
 */
void MqttManager::callback(char* topic, byte* payload, unsigned int length) {
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

    Serial.printf("[MQTT] Lệnh nhận được [%s]: %s\n", topic, message.c_str());

    // Logic điều khiển thiết bị từ xa
    if (message == "reboot") {
        Serial.println("Đang khởi động lại theo lệnh MQTT...");
        delay(500);
        ESP.restart();
    } else if (message.startsWith("{")) {
        // [THAY ĐỔI] Chọn kiểu JsonDocument theo phiên bản ArduinoJson đang cài:
        // - v7: JsonDocument tự co giãn (StaticJsonDocument/containsKey đã bị deprecated)
        // - v6: StaticJsonDocument 1024 byte (đủ cho tên có dấu + msg dài)
#if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
#else
        StaticJsonDocument<1024> doc;
#endif
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            Serial.printf("[MQTT] JSON loi: %s\n", error.c_str());
            return;
        }

        JsonObjectConst obj = doc.as<JsonObjectConst>();
        if (obj.isNull() || !(obj["action"] == "verify_result")) {
            return; // Không phải gói kết quả xác thực -> bỏ qua
        }

        // [THAY ĐỔI] Dùng toán tử "|" để lấy giá trị mặc định khi thiếu trường
        // (thay cho containsKey() - đã deprecated ở v7, và tránh as<String>() trả về "null").
        const char *status = obj["status"] | "";
        mqttVerifyAccepted = (strcasecmp(status, "accepted") == 0);

        mqttVerifyMssv  = jsonFieldToString(obj["mssv"]);
        mqttVerifyName  = jsonFieldToString(obj["name"]);

        // [MỚI] Các trường phục vụ tính năng Nhắc lịch hẹn.
        // Luôn gán lại (kể cả rỗng) để không bị dính dữ liệu của sinh viên trước.
        mqttVerifyTime  = jsonFieldToString(obj["time"]);
        mqttVerifyQueue = jsonFieldToString(obj["queue"], true);
        mqttVerifyMsg   = jsonFieldToString(obj["msg"]);

        // [MỚI] req_id tuỳ chọn: backend echo lại để thiết bị lọc phản hồi lạc (đến muộn)
        mqttVerifyReqId = obj["req_id"] | (uint32_t)0;

        // Bật cờ CUỐI CÙNG, sau khi mọi trường đã được ghi đầy đủ
        mqttVerifyReceived = true;
        Serial.printf("[MQTT] Received Verify Result: status=%s mssv=%s time=%s queue=%s msg=%s req_id=%lu\n",
                      status, mqttVerifyMssv.c_str(), mqttVerifyTime.c_str(),
                      mqttVerifyQueue.c_str(), mqttVerifyMsg.c_str(),
                      (unsigned long)mqttVerifyReqId);
    }
}

void MqttManager::loop() {
    if (!configMgr.params.mqttEnabled) {
        if (client.connected()) {
            client.disconnect();
            lastReconnectAttemptStatus = false;
        }
        return;
    }
    if (!WiFi.isConnected()) return; // Chỉ chạy khi có WiFi

    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            // Tạo ClientID duy nhất dựa trên DeviceID
            String mqttClientId = MQTT_CLIENT_ID_PREFIX + configMgr.params.deviceID;

            lastReconnectAttemptStatus = client.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASS);
            // Tính chu kỳ 5s từ lúc connect() KẾT THÚC: connect() có thể chặn tới 15s (DNS / chờ
            // CONNACK); nếu tính từ lúc bắt đầu thì vừa thoát ra đã quá 5s và lại thử ngay -> đơ máy.
            lastReconnectAttempt = millis();
            if (lastReconnectAttemptStatus) {
                Serial.println("[MQTT] MQTT connected!");
                client.subscribe(topicDown); // Đăng ký nhận lệnh
            }
        }
    } else {
        client.loop();
    }
}

void WakeupMQTT()
{
    configMgr.setMqttEnabled(true);

    if (WiFi.isConnected() && mqttMgr.setup()) {
        Serial.println(F("[MQTT] Wakeup successful."));
    } else {
        Serial.println(F("[MQTT] Wakeup requested, reconnect will continue in loop()."));
    }
}

void ShutdownMQTT()
{
    Serial.println(F("[MQTT] Shutting down MQTT..."));
    mqttMgr.disconnect();
    configMgr.setMqttEnabled(false);
}


/**
 * HÀM GỐC: Chấp nhận cả chuỗi char* và mảng byte uint8_t*
 */
bool MqttManager::publish(const uint8_t* payload, size_t length, bool retained) {
    if (!configMgr.params.mqttEnabled || !client.connected()) return false;
    return client.publish(topicUp, payload, length, retained);
}

bool MqttManager::publishString(String payload, bool retained) {
    if (!configMgr.params.mqttEnabled || !client.connected()) return false;
    return client.publish(topicUp, (const uint8_t*)payload.c_str(), payload.length(), retained);
}

// ======================== NHÓM HÀM TEXT (CSV) ========================

void MqttManager::publishText(float v1) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%.1f", v1);
    publish((uint8_t*)buf, len);
}

void MqttManager::publishText(float v1, float v2) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%.1f,%.1f", v1, v2);
    publish((uint8_t*)buf, len);
}

void MqttManager::publishText(float v1, float v2, float v3, float v4) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%.1f,%.1f,%.1f,%.1f", v1, v2, v3, v4);
    publish((uint8_t*)buf, len);
}

// ======================== NHÓM HÀM BINARY (RAW) ========================

void MqttManager::publishBin(float v1 ) {
    publish((uint8_t*)&v1, sizeof(float));
}

void MqttManager::publishBin(float v1, float v2) {
    struct __attribute__((__packed__)) {
        float val[2];
    } data = { {v1, v2} };

    publish((uint8_t*)&data, sizeof(data));
}

void MqttManager::publishBin(float v1, float v2, float v3, float v4) {
    struct __attribute__((__packed__)) {
        float val[4];
    } data = { {v1, v2, v3, v4} };

    publish((uint8_t*)&data, sizeof(data));
}

// ======================== NHÓM HÀM QUÉT THẺ ========================

// [THAY ĐỔI] Dùng chung buildScanPayload(): escape JSON cho UID/QR (trước đây QR chứa dấu "
// sẽ làm hỏng JSON) và gửi kèm "req_id" nếu reqId != 0.
bool MqttManager::publishRfid(String cardUid, uint32_t reqId) {
    if (!configMgr.params.mqttEnabled || !client.connected()) return false;
    String payload = buildScanPayload("uid", cardUid, reqId);
    return client.publish(topicRfid, (const uint8_t*)payload.c_str(), payload.length(), false);
}

bool MqttManager::publishNfc(String cardUid, uint32_t reqId) {
    if (!configMgr.params.mqttEnabled || !client.connected()) return false;
    String payload = buildScanPayload("uid", cardUid, reqId);
    return client.publish(topicNfc, (const uint8_t*)payload.c_str(), payload.length(), false);
}

bool MqttManager::publishQr(String qrData, uint32_t reqId) {
    if (!configMgr.params.mqttEnabled || !client.connected()) return false;
    String payload = buildScanPayload("qr_data", qrData, reqId);
    return client.publish(topicQr, (const uint8_t*)payload.c_str(), payload.length(), false);
}

MqttManager mqttMgr;
