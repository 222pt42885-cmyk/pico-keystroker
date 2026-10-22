#include "button.h"

void button_init(button_t *b, uint32_t debounce_ms, uint32_t ignore_ms) {
    if (b == NULL) {
        return;
    }
    b->debounce_ms = debounce_ms;
    b->ignore_ms = ignore_ms;
    b->stable_pressed = false;
    b->last_raw = false;
    b->last_raw_change_ms = 0;
    b->last_press_ms = 0;
    b->has_press = false;
}

button_event_t button_update(button_t *b, bool raw_pressed, uint32_t now_ms) {
    if (b == NULL) {
        return BUTTON_EVENT_NONE;
    }
    if (raw_pressed != b->last_raw) {
        b->last_raw = raw_pressed;
        b->last_raw_change_ms = now_ms;
    }
    if (b->stable_pressed == raw_pressed) {
        return BUTTON_EVENT_NONE;
    }
    if ((uint32_t)(now_ms - b->last_raw_change_ms) < b->debounce_ms) {
        return BUTTON_EVENT_NONE; /* still settling */
    }
    b->stable_pressed = raw_pressed;
    if (!raw_pressed) {
        return BUTTON_EVENT_NONE; /* debounced falling edge: no event */
    }
    if (b->has_press &&
        (uint32_t)(now_ms - b->last_press_ms) < b->ignore_ms) {
        return BUTTON_EVENT_DROPPED; /* double click: ignore this one */
    }
    b->last_press_ms = now_ms;
    b->has_press = true;
    return BUTTON_EVENT_PRESS;
}
