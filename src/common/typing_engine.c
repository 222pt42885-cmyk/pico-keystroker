#include "typing_engine.h"

void typing_engine_init(typing_engine_t *eng,
                        const typing_engine_config_t *cfg) {
    if (eng == NULL) {
        return;
    }
    if (cfg != NULL) {
        eng->config = *cfg;
    } else {
        eng->config.language = LANG_EN_US;
        eng->config.typing_delay_ms = 8u;
    }
    eng->chars_typed = 0;
    eng->chars_skipped = 0;
}

uint32_t typing_engine_delay_ms(const typing_engine_t *eng) {
    if (eng == NULL) {
        return 0;
    }
    return eng->config.typing_delay_ms;
}

void typing_cursor_init(typing_cursor_t *cursor) {
    if (cursor == NULL) {
        return;
    }
    cursor->index = 0;
    cursor->release_pending = false;
}

bool typing_engine_next(typing_engine_t *eng, const char *str,
                        typing_cursor_t *cursor, hid_action_t *out) {
    if (eng == NULL || str == NULL || cursor == NULL || out == NULL) {
        return false;
    }
    if (cursor->release_pending) {
        out->modifier = HID_MOD_NONE;
        out->keycode = HID_KEY_NONE;
        out->is_release = true;
        cursor->release_pending = false;
        return true;
    }
    while (str[cursor->index] != '\0') {
        char c = str[cursor->index];
        uint8_t key = HID_KEY_NONE;
        uint8_t mod = HID_MOD_NONE;
        cursor->index++;
        if (hid_keycode_for_ascii(c, eng->config.language, &key, &mod)) {
            out->modifier = mod;
            out->keycode = key;
            out->is_release = false;
            cursor->release_pending = true;
            eng->chars_typed++;
            return true;
        }
        eng->chars_skipped++;
    }
    return false;
}

uint32_t typing_engine_type_string(typing_engine_t *eng, const char *str,
                                   hid_action_cb_t cb, void *user) {
    typing_cursor_t cursor;
    hid_action_t action;
    uint32_t n = 0;
    if (eng == NULL || str == NULL) {
        return 0;
    }
    typing_cursor_init(&cursor);
    while (typing_engine_next(eng, str, &cursor, &action)) {
        if (cb != NULL) {
            cb(&action, user);
        }
        n++;
    }
    return n;
}
