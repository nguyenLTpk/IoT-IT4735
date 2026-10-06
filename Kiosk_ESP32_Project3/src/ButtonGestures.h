#ifndef BUTTON_GESTURE_H
#define BUTTON_GESTURE_H

#include <Arduino.h>
#include "HardwareConfig.h"

#if !ENABLE_CONFIG_BUTTON
    #error "ButtonGesture requires ENABLE_CONFIG_BUTTON=1 in HardwareConfig.h"
#endif

// Configuration button pin (defined in HardwareConfig.h)
#define CFG_BUTTON CFG_BUTTON_PIN

enum ButtonEvent {
    NONE,
    SHORT_PRESS,
    DOUBLE_CLICK,
    LONG_PRESS_2S
};

class ButtonGesture {
private:
    int _pin;
    unsigned long _lastDebounceTime = 0;
    unsigned long _buttonPressedTime = 0;
    unsigned long _lastClickTime = 0;
    bool _isPressing = false;
    bool _longPressExecuted = false;
    int _clickCount = 0;

    const int _debounceDelay = 50;    // Chống nhiễu 50ms
    const unsigned long _longPressDelay = 2000; // Giữ 2 giây
    const unsigned long _doubleClickDelay = 300; // Khoảng cách giữa 2 lần nhấn để tính là đúp

public:
    ButtonGesture(int pin);
    void begin();
    ButtonEvent update(); // Hàm này sẽ được gọi liên tục trong loop()
};

#endif