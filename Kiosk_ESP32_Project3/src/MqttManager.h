#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "ConfigManager.h"

// --- CẤU HÌNH MQTT SERVER ---
#define MQTT_SERVER "mqtt.toolhub.app"
#define MQTT_PORT 1883
#define MQTT_USER "demo"  // Thay bằng username của bạn
#define MQTT_PASS "demo" // Thay bằng password của bạn

// [MỚI] Kích thước buffer gói tin MQTT (byte).
// PubSubClient mặc định chỉ có 256 byte (tính cả header + tên topic). Gói verify_result
// có thêm time/queue/msg và tên tiếng Việt có dấu (UTF-8, 2-3 byte/ký tự) rất dễ vượt
// 256 byte -> thư viện sẽ ÂM THẦM BỎ gói tin và thiết bị chỉ thấy "Timeout".
#define MQTT_BUFFER_SIZE 1024

// --- CẤU HÌNH TOPIC (Sử dụng DeviceID để cá nhân hóa) ---
// Upstream: Thiết bị -> Server (Gửi dữ liệu)
// Downstream: Server -> Thiết bị (Nhận lệnh)
#define MQTT_TOPIC_UP_TEMPLATE "monitor_student/%s/data"
#define MQTT_TOPIC_DOWN_TEMPLATE "monitor_student/%s/cmd"
#define MQTT_TOPIC_RFID_TEMPLATE "monitor_student/%s/rfid"
#define MQTT_TOPIC_NFC_TEMPLATE "monitor_student/%s/nfc"
#define MQTT_TOPIC_QR_TEMPLATE "monitor_student/%s/qr"
#define MQTT_TOPIC_STARTUP "startup" // Topic chung cho mọi thiết bị báo danh
#define MQTT_CLIENT_ID_PREFIX "monitor_student-" // Công thức tạo mqtt client id = MQTT_CLIENT_ID_PREFIX + <deviceid>

// ============================================================================
// [MỚI] KẾT QUẢ XÁC THỰC / LỊCH HẸN NHẬN TỪ TOPIC DOWNSTREAM
// ----------------------------------------------------------------------------
// Được ghi trong MqttManager::callback() khi nhận gói JSON dạng:
// { "action": "verify_result", "status": "accepted", "mssv": "20210001",
//   "name": "Nguyen Van A", "time": "10:30", "queue": "05", "msg": "Quay so 3" }
//
// main.cpp đọc các biến này trong State Machine (không cần khai báo extern cục bộ nữa).
// Callback chạy bên trong mqttMgr.loop() -> cùng task với loop() nên không cần mutex.
// ============================================================================
extern bool mqttVerifyReceived;   // true khi vừa nhận 1 gói verify_result (main.cpp xoá sau khi xử lý)
extern bool mqttVerifyAccepted;   // true nếu status == "accepted" (không phân biệt hoa/thường)
extern String mqttVerifyMssv;     // "mssv"  - MSSV do backend trả về
extern String mqttVerifyName;     // "name"  - Họ tên sinh viên (có thể có dấu, UTF-8)
extern String mqttVerifyTime;     // [MỚI] "time"  - Giờ hẹn, ví dụ "10:30"
extern String mqttVerifyQueue;    // [MỚI] "queue" - Số thứ tự, ví dụ "05" (chấp nhận cả kiểu số)
extern String mqttVerifyMsg;      // [MỚI] "msg"   - Thông điệp, ví dụ "Quay so 3"
extern uint32_t mqttVerifyReqId;  // [MỚI] "req_id" (tuỳ chọn) - 0 nếu backend không gửi

/**
 * @brief [MỚI] Xoá toàn bộ kết quả verify cũ. Gọi NGAY TRƯỚC khi publish một lần quét mới
 *        để không dùng nhầm dữ liệu của lần quét trước.
 */
void mqttClearVerifyResult();

class MqttManager
{
private:
    WiFiClient espClient;
    PubSubClient client;

    /** giá trị thời gian (tính bằng milis giây) của lần cuối cùng thiết bị thử kết nối với Broker. Chu kì 5 giây */
    unsigned long lastReconnectAttempt = 0;

    /** trạng thái thành công/thất bại của lần kết nối kết nối cuối cùng với Broker. Chu kì 5 giây. True = thành công*/
    unsigned long lastReconnectAttemptStatus = false;

    char topicUp[64];
    char topicDown[64];
    char topicRfid[64];
    char topicNfc[64];
    char topicQr[64];

    // Gửi gói tin Startup và chờ xác nhận
    void sendStartupPacket();

    /**
     * @brief Hàm phản hồi (Callback) xử lý dữ liệu từ MQTT Broker gửi xuống.
     * @note Hàm này được thư viện PubSubClient gọi tự động mỗi khi có tin nhắn mới
     * từ các Topic mà thiết bị đã Subscribe (thông thường là topicDown).
     * @param topic   Con trỏ chuỗi chứa tên Topic vừa nhận được dữ liệu.
     * @param payload Mảng byte chứa nội dung tin nhắn (Dữ liệu nhị phân).
     * @param length  Độ dài của nội dung tin nhắn (tính theo byte).
     * @process:
     * 1. Chuyển đổi mảng byte 'payload' sang đối tượng String để dễ xử lý.
     * 2. In thông tin debug ra Serial Monitor.
     * 3. Kiểm tra nội dung lệnh (ví dụ: "reboot") và thực thi logic tương ứng.
     * 4. [MỚI] Gói JSON "verify_result": parse status, mssv, name, time, queue, msg, req_id
     *    vào các biến toàn cục mqttVerify* ở trên.
     */
    static void callback(char *topic, byte *payload, unsigned int length);

public:
    MqttManager();
    bool setup();
    /** Có kết nối thành công với MQTT Broker không. cập nhật sau mỗi 5 giây */
    bool isLastConnectionToBrokerOk();

    /**
     * @brief Duy trì kết nối MQTT (Non-blocking)
     */
    void loop();
    bool connected();
    void disconnect();

    // Hàm gốc điều phối gửi tin
    bool publish(const uint8_t *payload, size_t length, bool retained = true);
    bool publish(const char *topic, const uint8_t *payload, size_t length, bool retained = true);

    // --- BIẾN THỂ CHUỖI VĂN BẢN (JSON / String) ---
    bool publishString(String payload, bool retained = false);

    // --- BIẾN THỂ TEXT (CSV) ---
    void publishText(float v1);
    void publishText(float v1, float v2);
    void publishText(float v1, float v2, float v3, float v4);

    // --- BIẾN THỂ BINARY (Raw Bytes) ---
    void publishBin(float v1);
    void publishBin(float v1, float v2);
    void publishBin(float v1, float v2, float v3, float v4);

    // --- BIẾN THỂ GỬI DỮ LIỆU QUÉT THẺ ---
    // [THAY ĐỔI] Thêm tham số tuỳ chọn reqId: nếu khác 0 sẽ gửi kèm "req_id" để backend
    // trả lại trong verify_result -> thiết bị lọc được phản hồi đến muộn của lần quét cũ.
    // Mặc định = 0 -> payload giữ nguyên định dạng cũ (tương thích ngược).
    bool publishRfid(String cardUid, uint32_t reqId = 0);
    bool publishNfc(String cardUid, uint32_t reqId = 0);
    bool publishQr(String qrData, uint32_t reqId = 0);
};

void WakeupMQTT();
void ShutdownMQTT();

extern MqttManager mqttMgr;

#endif
