#ifndef API_MANAGER_H
#define API_MANAGER_H

#include <Arduino.h>

// Thay thế URL này bằng đường link Web App (Google Apps Script) thật
// Lưu ý: phải giữ lại /exec ở cuối
#define MASTER_API_URL "https://script.google.com/macros/s/AKfycbyLpNcShSwP-djZiftcjqUS6fwbeXM1hiuPgDZknWimnHI4sdukUm2B2cvpjaZUDXs1hw/exec"

class ApiManager {
public:
    ApiManager();
    
    /**
     * @brief Gọi API Google Sheet để xác thực
     * @param scannedData Chuỗi URL quét được hoặc mã RFID
     * @param isRfid Biến bool đánh dấu xem đây là thẻ RFID hay mã QR (để Google phân loại dò tìm)
     * @param studentIdOut MSSV trả về từ Google Sheet
     * @param fullNameOut Họ tên trả về từ Google Sheet
     * @return true nếu hợp lệ, false nếu bị từ chối/lỗi
     */
    bool verifyStudent(String scannedData, bool isRfid, String &studentIdOut, String &fullNameOut);
};

extern ApiManager apiMgr;

#endif
