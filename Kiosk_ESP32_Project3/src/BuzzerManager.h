#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>
#include "HardwareConfig.h"

#if !ENABLE_BUZZER
    #error "BuzzerManager requires ENABLE_BUZZER=1 in HardwareConfig.h"
#endif

/**
 * Quản lý còi buzzer theo cơ chế NON-BLOCKING (state machine + millis()).
 *
 * - beepOk() / beepShort() / beepError() chỉ "đặt lịch" một mẫu bíp rồi trả về NGAY,
 *   không còn delay(). Chân còi được bật ngay trong lúc gọi (nếu còi đang rảnh).
 * - update() phải được gọi liên tục trong loop(): nó kiểm tra millis() để tắt/bật
 *   chân GPIO đúng thời điểm và chuyển sang mẫu bíp kế tiếp.
 * - Nhiều lệnh bíp gọi liên tiếp (vd. bíp xác nhận quẹt thẻ rồi bíp lỗi) được xếp hàng
 *   và phát lần lượt, cách nhau một khoảng lặng ngắn để tai phân biệt được.
 */
class BuzzerManager {
public:
    BuzzerManager();

    // Cài đặt khởi tạo pin
    void begin();

    // Phát 1 tiếng bíp dài (Báo thành công) - 300ms
    void beepOk();

    // Phát 1 tiếng bíp cực ngắn (Báo đã nhận thẻ) - 80ms
    void beepShort();

    // Phát 3 tiếng bíp liên tục (Báo lỗi / Cảnh báo) - 3 x (100ms kêu + 100ms nghỉ)
    void beepError();

    /**
     * Cập nhật trạng thái còi theo millis(). GỌI MỖI VÒNG loop().
     * Rất nhẹ: khi còi rảnh chỉ là 1 phép so sánh rồi thoát.
     */
    void update();

    /** Tắt còi ngay lập tức và huỷ mọi tiếng bíp đang chờ (dùng trước các đoạn code blocking). */
    void stop();

    /** true nếu còi đang kêu, đang nghỉ giữa các nhịp, hoặc còn mẫu bíp trong hàng đợi. */
    bool isBusy() const;

private:
    enum State : uint8_t {
        IDLE = 0, // Không kêu, hàng đợi trống
        PLAYING,  // Đang phát 1 mẫu bíp (bước chẵn = kêu, bước lẻ = nghỉ)
        GAP       // Khoảng lặng ngắn giữa 2 mẫu bíp liên tiếp
    };

    /** Một mẫu bíp: dãy thời lượng (ms) xen kẽ KÊU, NGHỈ, KÊU, NGHỈ... */
    struct Pattern {
        const uint16_t *steps;
        uint8_t count;
    };

    static const uint8_t QUEUE_SIZE = 4;

    State _state;
    Pattern _current;
    uint8_t _stepIndex;
    unsigned long _stepStartMs; // Thời điểm bắt đầu bước hiện tại (hoặc khoảng lặng)
    unsigned long _lastEndMs;   // Thời điểm mẫu bíp gần nhất kết thúc
    bool _hasEnded;             // Đã từng có mẫu bíp kết thúc chưa (_lastEndMs hợp lệ)
    bool _outputOn;

    Pattern _queue[QUEUE_SIZE]; // Hàng đợi vòng các mẫu bíp chờ phát
    uint8_t _queueHead;
    uint8_t _queueCount;

    void play(const uint16_t *steps, uint8_t count);
    void startPattern(const Pattern &pattern, unsigned long now);
    void finishPattern(unsigned long now);
    void setOutput(bool on);
};

// Biến toàn cục dùng chung
extern BuzzerManager buzzerMgr;

#endif
