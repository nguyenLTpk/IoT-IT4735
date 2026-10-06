#include "BuzzerManager.h"

// Biến toàn cục định nghĩa sẵn
BuzzerManager buzzerMgr;

// ---------------------------------------------------------------------------
// Các mẫu bíp: thời lượng (ms) xen kẽ KÊU, NGHỈ, KÊU, ... (giữ đúng nhịp bản cũ)
// ---------------------------------------------------------------------------
static const uint16_t PATTERN_OK[] = {300};                              // Bíp dài
static const uint16_t PATTERN_SHORT[] = {80};                            // Bíp cực ngắn
static const uint16_t PATTERN_ERROR[] = {100, 100, 100, 100, 100, 100};  // Bíp, bíp, bíp

// Khoảng lặng giữa 2 mẫu bíp phát liên tiếp, để 2 tiếng không dính thành 1
static const unsigned long PATTERN_GAP_MS = 60;

#define PATTERN_LEN(p) ((uint8_t)(sizeof(p) / sizeof((p)[0])))

BuzzerManager::BuzzerManager()
    : _state(IDLE), _current{nullptr, 0}, _stepIndex(0), _stepStartMs(0), _lastEndMs(0),
      _hasEnded(false), _outputOn(false), _queueHead(0), _queueCount(0) {
}

void BuzzerManager::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // Mặc định tắt loa
    _outputOn = false;
    _state = IDLE;
    _queueCount = 0;
}

void BuzzerManager::beepOk() {
    // Kêu tít 1 tiếng đủ dài để báo được việc cập nhật hệ thống / quẹt đúng
    play(PATTERN_OK, PATTERN_LEN(PATTERN_OK));
}

void BuzzerManager::beepShort() {
    // Kêu tít 1 tiếng thật ngắn làm phản hồi khi đọc thấy thẻ
    play(PATTERN_SHORT, PATTERN_LEN(PATTERN_SHORT));
}

void BuzzerManager::beepError() {
    // Kêu 3 nhịp giật cục (bíp bíp bíp)
    play(PATTERN_ERROR, PATTERN_LEN(PATTERN_ERROR));
}

bool BuzzerManager::isBusy() const {
    return _state != IDLE;
}

void BuzzerManager::stop() {
    _queueCount = 0;
    _state = IDLE;
    setOutput(false);
    _lastEndMs = millis();
    _hasEnded = true;
}

/**
 * Đặt lịch 1 mẫu bíp. Còi rảnh -> phát ngay (bật GPIO ngay trong lần gọi này);
 * còi đang bận -> xếp vào hàng đợi. Không bao giờ chờ.
 */
void BuzzerManager::play(const uint16_t *steps, uint8_t count) {
    const Pattern pattern = {steps, count};
    const unsigned long now = millis();

    if (_state == IDLE) {
        if (!_hasEnded || now - _lastEndMs >= PATTERN_GAP_MS) {
            startPattern(pattern, now);
            return;
        }
        // Mẫu trước vừa tắt chưa đủ khoảng lặng -> chờ hết khoảng lặng rồi mới phát
        _state = GAP;
        _stepStartMs = _lastEndMs;
    }

    if (_queueCount < QUEUE_SIZE) {
        _queue[(_queueHead + _queueCount) % QUEUE_SIZE] = pattern;
        _queueCount++;
    } else {
        // Hàng đợi đầy: thay mẫu cuối cùng bằng mẫu mới nhất (thông tin mới quan trọng hơn)
        _queue[(_queueHead + QUEUE_SIZE - 1) % QUEUE_SIZE] = pattern;
    }
}

void BuzzerManager::startPattern(const Pattern &pattern, unsigned long now) {
    _current = pattern;
    _stepIndex = 0;
    _stepStartMs = now;
    _state = PLAYING;
    setOutput(true); // Bước 0 luôn là KÊU
}

void BuzzerManager::finishPattern(unsigned long now) {
    setOutput(false);
    _lastEndMs = now;
    _hasEnded = true;
    if (_queueCount > 0) {
        _state = GAP;
        _stepStartMs = now;
    } else {
        _state = IDLE;
    }
}

void BuzzerManager::update() {
    if (_state == IDLE)
        return;

    const unsigned long now = millis();

    if (_state == GAP) {
        if (now - _stepStartMs < PATTERN_GAP_MS)
            return;
        if (_queueCount == 0) { // Phòng hờ (stop() đã xoá hàng đợi)
            _state = IDLE;
            return;
        }
        const Pattern next = _queue[_queueHead];
        _queueHead = (_queueHead + 1) % QUEUE_SIZE;
        _queueCount--;
        startPattern(next, now);
        return;
    }

    // PLAYING: bước hiện tại chưa hết hạn -> thoát ngay
    if (now - _stepStartMs < _current.steps[_stepIndex])
        return;

    // Hết bước cuối -> tắt còi ngay. Nhờ vậy nếu loop() bị chặn lâu, tiếng bíp đơn
    // (beepOk/beepShort) tắt ngay ở lần update() đầu tiên sau đó, không bị kéo dài thêm.
    const uint8_t next = _stepIndex + 1;
    if (next >= _current.count) {
        finishPattern(now);
        return;
    }

    // Chỉ chuyển ĐÚNG 1 bước mỗi lần và neo mốc thời gian vào "now": nếu update() bị gọi
    // trễ, các nhịp sau chỉ lùi lại chứ không bị bỏ qua -> 3 tiếng bíp lỗi luôn tách rời.
    _stepIndex = next;
    _stepStartMs = now;
    setOutput((_stepIndex % 2) == 0); // Bước chẵn = KÊU, bước lẻ = NGHỈ
}

void BuzzerManager::setOutput(bool on) {
    if (on == _outputOn)
        return;
    _outputOn = on;
    digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
}
