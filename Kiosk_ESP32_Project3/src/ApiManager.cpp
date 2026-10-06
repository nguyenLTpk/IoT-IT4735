#include "ApiManager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

ApiManager apiMgr;

ApiManager::ApiManager() {}

static bool extractFieldValue(const String &payload, const char *key, String &valueOut) {
    const String keyNeedle = String("\"") + key + String("\"");
    const int keyStart = payload.indexOf(keyNeedle);
    if (keyStart < 0) return false;

    int colonPos = payload.indexOf(':', keyStart + keyNeedle.length());
    if (colonPos < 0) return false;

    // Nhảy qua dấu ':' rồi mới bỏ qua khoảng trắng
    int valuePos = colonPos + 1;
    while (valuePos < (int)payload.length() && isspace(static_cast<unsigned char>(payload.charAt(valuePos)))) {
        valuePos++;
    }

    if (valuePos >= (int)payload.length() || payload.charAt(valuePos) != '"') return false;

    const int valueStart = valuePos + 1;
    const int valueEnd = payload.indexOf('"', valueStart);
    if (valueEnd < 0) return false;

    valueOut = payload.substring(valueStart, valueEnd);
    return true;
}

static bool extractAnyFieldValue(const String &payload, String &valueOut) {
    static const char *const keys[] = {
        "student_id",
        "studentId",
        "mssv",
        "MSSV",
        "id"
    };

    for (const char *key : keys) {
        if (extractFieldValue(payload, key, valueOut)) {
            return true;
        }
    }

    return false;
}

static bool splitDelimitedResponse(const String &payload, String &studentIdOut, String &fullNameOut) {
    int sep = payload.indexOf('|');
    if (sep < 0) sep = payload.indexOf(',');
    if (sep < 0) return false;

    studentIdOut = payload.substring(0, sep);
    fullNameOut = payload.substring(sep + 1);
    studentIdOut.trim();
    fullNameOut.trim();
    return studentIdOut.length() > 0 && fullNameOut.length() > 0;
}

bool ApiManager::verifyStudent(String scannedData, bool isRfid, String &studentIdOut, String &fullNameOut) {
    studentIdOut = "";
    fullNameOut = "";

    if(WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[API] Lỗi: WiFi chưa kết nối!"));
        return false;
    }

    // Google Apps Script yêu cầu HTTPS và mã hoá
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Bỏ qua xác thực chứng chỉ SSL để tránh crash do hết hạn Root CA trên mạch cũ

    // Tạo URL: thêm tham số mã hoá
    // Ví dụ: ?type=rfid&data=123AB hoặc ?type=qr&data=https://...
    String requestUrl = String(MASTER_API_URL) + "?type=" + (isRfid ? "rfid" : "qr") + "&data=" + scannedData;
    
    Serial.print(F("[API] Đang gọi Webhook lên Google Sheet: "));
    Serial.println(requestUrl);

    // Google Apps Script luôn trả về mã HTTP 302 Moved Temporarily, 
    // phải set biến này để mạch chạy theo link redirect thì mới ra mã 200 OK chứa body kết quả.
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    if (http.begin(client, requestUrl)) {
        int httpCode = http.GET();
        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                String payload = http.getString();
                payload.trim(); 
                
                Serial.print(F("[API] Server Google phản hồi: "));
                Serial.println(payload);
                Serial.print(F("[API] Payload length: "));
                Serial.println(payload.length());
                if (payload.length() > 0) {
                    Serial.print(F("[API] Payload first char: 0x"));
                    Serial.println((int)payload.charAt(0), HEX);
                }
                http.end();
                
                if (payload.equalsIgnoreCase("error") || payload.length() == 0 || payload.equalsIgnoreCase("invalid")) {
                    return false;
                }

                if (payload.startsWith("{")) {
                    String parsedStudentId;
                    String parsedFullName;
                    if (extractAnyFieldValue(payload, parsedStudentId)) {
                        studentIdOut = parsedStudentId;
                    }
                    if (extractFieldValue(payload, "full_name", parsedFullName)) {
                        fullNameOut = parsedFullName;
                    } else if (extractFieldValue(payload, "name", parsedFullName)) {
                        fullNameOut = parsedFullName;
                    } else if (extractFieldValue(payload, "fullName", parsedFullName)) {
                        fullNameOut = parsedFullName;
                    }

                    Serial.print(F("[API] Parsed studentId='"));
                    Serial.print(studentIdOut);
                    Serial.print(F("' fullName='"));
                    Serial.print(fullNameOut);
                    Serial.println(F("'"));

                    if (fullNameOut.length() > 0 || studentIdOut.length() > 0) {
                        return true;
                    }
                    
                    // Nếu là JSON nhưng không chứa thông tin hợp lệ -> Trả về lỗi
                    return false;
                }

                if (splitDelimitedResponse(payload, studentIdOut, fullNameOut)) {
                    return true;
                }

                fullNameOut = payload;
                Serial.println(F("[API] Warning: response has no parsed MSSV field; treating whole payload as name only."));
                return true;
            } else {
                Serial.printf("[API] Lỗi HTTP Code: %d\n", httpCode);
                http.end();
            }
        } else {
            Serial.printf("[API] Yêu cầu HTTP thất bại, mã lỗi: %s\n", http.errorToString(httpCode).c_str());
            http.end();
        }
    } else {
        Serial.println(F("[API] Không thể thiết lập kết nối (Có thể do sai host hoặc quá tải)"));
    }

    return false;
}
