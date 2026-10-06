#include "ButtonGestures.h"

ButtonGesture::ButtonGesture(int pin) : _pin(pin) {}

void ButtonGesture::begin() {
    pinMode(_pin, INPUT_PULLUP);
}

ButtonEvent ButtonGesture::update() {
    int reading = digitalRead(_pin);
    unsigned long now = millis();
    ButtonEvent event = NONE;

    if (reading == LOW) { // Đang nhấn
        if (!_isPressing) {
            _isPressing = true;
            _buttonPressedTime = now;
            _longPressExecuted = false;
        } else if (!_longPressExecuted && (now - _buttonPressedTime >= _longPressDelay)) {
            _longPressExecuted = true;
            _clickCount = 0; // Reset click count khi đã nhấn giữ
            return LONG_PRESS_2S;
        }
    } else { // Thả nút
        if (_isPressing) {
            unsigned long pressDuration = now - _buttonPressedTime;
            _isPressing = false;

            if (!_longPressExecuted && pressDuration > _debounceDelay) {
                _clickCount++;
                _lastClickTime = now;
            }
        }
    }

    // Kiểm tra Double Click hoặc Short Press sau khi thả nút
    if (_clickCount > 0 && (now - _lastClickTime > _doubleClickDelay)) {
        if (_clickCount == 1) event = SHORT_PRESS;
        else if (_clickCount >= 2) event = DOUBLE_CLICK;
        
        _clickCount = 0; // Reset
    }

    return event;
}