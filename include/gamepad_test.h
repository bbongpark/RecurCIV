#pragma once
#include <stdbool.h>
#include <stdint.h>

/* A timed request, not an indication that the host received a report. */
typedef struct {
    bool active;
    uint64_t started_ms;
} GamepadButtonTest;

static inline void gamepad_button_test_start(GamepadButtonTest *test,uint64_t now_ms) {
    test->active=true;
    test->started_ms=now_ms;
}

static inline bool gamepad_button_test_expired(const GamepadButtonTest *test,uint64_t now_ms) {
    return test->active && now_ms-test->started_ms>=500;
}
