/* tests/test_s3_contract.c — host contract test for targets/esp32s3/main.cpp.
 *
 * The S3 firmware (Arduino C++) cannot build on the host, but every
 * src/common API it relies on can: this suite exercises exactly those
 * call shapes against the real shared core so a core change that breaks
 * the S3 target fails here first:
 *
 *   config: config_set_defaults() + config_parse_string() +
 *           config_validate() (CFG_OK == 0 checked, defaults kept on
 *           failure) using the fallback config text from main.cpp.
 *   button: button_init(&b, debounce_ms, double_click_ms) then
 *           button_update(&b, hal_button_read(), now_ms) per poll, with
 *           PRESS typing once and DROPPED suppressing double clicks.
 *   typing: typing_engine_init() + typing_engine_type_string() with an
 *           emit callback shaped like main.cpp's emit_action() (press ->
 *           keys[0] = keycode, release -> all zero, push via hal_hid_send,
 *           space with typing_engine_delay_ms()).
 *
 * Fake HAL: static functions in this file only. No target hal file is
 * included; the S3 hal_port.cpp is Arduino/TinyUSB and never builds here.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "minunit.h"
#include "config_parser.h"
#include "hid_keymap.h"
#include "button.h"
#include "typing_engine.h"

/* Fallback config: same keys as the compiled-in fallback in
 * targets/esp32s3/main.cpp. */
static const char kS3FallbackConfig[] =
    "WRITE_MODE=button\n"
    "COUNTDOWN_MS=5000\n"
    "HID_KEYBOARD_NAME=Arboard-2040-pico-keystroker\n"
    "LANGUAGE=en_US\n"
    "TYPING_DELAY_MS=8\n"
    "DOUBLE_CLICK_MS=400\n"
    "DEBOUNCE_MS=30\n"
    "BUTTON_PIN=15\n";

/* --- Fake HAL: mirrors the hal.h surface main.cpp calls. --- */

static bool s_fake_pressed; /* scripted BOOT level: true = pressed */
static uint32_t s_fake_now_ms; /* scripted millis() */

static bool fake_hal_button_read(void) {
    return s_fake_pressed;
}

static uint32_t s_sent_press; /* hal_hid_send calls with a key down */
static uint32_t s_sent_release; /* hal_hid_send calls, all keys up */
static uint32_t s_delay_calls; /* hal_delay_ms calls */
static uint32_t s_last_delay_ms; /* last hal_delay_ms argument */
static uint8_t s_last_modifier;
static uint8_t s_last_keys[6];
static uint8_t s_first_modifier; /* modifier of the first press seen */
static uint8_t s_first_keycode; /* keycode of the first press seen */

static bool fake_hal_hid_ready(void) {
    return true;
}

static bool fake_hal_hid_send(uint8_t modifier, const uint8_t keycode[6]) {
    unsigned int i;

    if (keycode == NULL) {
        return false;
    }
    s_last_modifier = modifier;
    for (i = 0; i < 6u; i++) {
        s_last_keys[i] = keycode[i];
    }
    if (keycode[0] != 0) {
        if (s_sent_press == 0) {
            s_first_modifier = modifier;
            s_first_keycode = keycode[0];
        }
        s_sent_press++;
    } else {
        s_sent_release++;
    }
    return true;
}

static void fake_hal_delay_ms(uint32_t ms) {
    s_delay_calls++;
    s_last_delay_ms = ms;
}

/* Emit callback shaped like targets/esp32s3/main.cpp emit_action():
 * press packs keys[0], release sends all-zero, every action is pushed
 * via hal_hid_send() and spaced by typing_engine_delay_ms(). */
static void s3_emit_action(const hid_action_t *action, void *user) {
    const typing_engine_t *eng = (const typing_engine_t *)user;
    uint8_t keys[6] = { 0, 0, 0, 0, 0, 0 };

    if (action == NULL) {
        return;
    }
    if (!action->is_release) {
        keys[0] = action->keycode;
    }
    if (!fake_hal_hid_ready()) {
        return;
    }
    fake_hal_hid_send(action->modifier, keys);
    fake_hal_delay_ms(typing_engine_delay_ms(eng));
}

static void type_all_like_main(const keystroker_config_t *cfg,
                               const char *payload, typing_engine_t *eng,
                               uint32_t *actions_out) {
    typing_engine_config_t tec;

    tec.language = cfg->language;
    tec.typing_delay_ms = cfg->typing_delay_ms;
    typing_engine_init(eng, &tec);
    *actions_out =
        typing_engine_type_string(eng, payload, s3_emit_action, eng);
}

/* Config path: defaults + parse + validate, fallback to defaults. */
static void test_s3_config_path(void) {
    keystroker_config_t cfg;
    keystroker_config_t bad;

    config_set_defaults(&cfg);
    MU_CHECK(config_parse_string(kS3FallbackConfig, &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.write_mode == WRITE_MODE_BUTTON);
    MU_EQ_ULL(5000, cfg.countdown_ms);
    MU_CHECK(cfg.language == LANG_EN_US);
    MU_EQ_ULL(8, cfg.typing_delay_ms);
    MU_EQ_ULL(400, cfg.double_click_ms);
    MU_EQ_ULL(30, cfg.debounce_ms);

    /* main.cpp keeps compiled-in defaults when parse/validate fails. */
    config_set_defaults(&bad);
    MU_CHECK(config_parse_string("WRITE_MODE=time\n", &bad) == CFG_OK);
    MU_CHECK(config_validate(&bad) != CFG_OK); /* countdown missing */
    config_set_defaults(&bad); /* fallback, as main.cpp does */
    MU_CHECK(!bad.has_write_mode);
    MU_CHECK(bad.language == CFG_DEFAULT_LANGUAGE);
    MU_EQ_ULL(CFG_DEFAULT_TYPING_DELAY_MS, bad.typing_delay_ms);
}

/* Button path: button_update(&b, hal_button_read(), now) per poll.
 * Debounced BOOT press types once; a second press inside the
 * double-click window is DROPPED; a later press types again. */
static void test_s3_button_window(void) {
    keystroker_config_t cfg;
    button_t b;
    uint32_t t;

    config_set_defaults(&cfg);
    MU_CHECK(config_parse_string(kS3FallbackConfig, &cfg) == CFG_OK);
    button_init(&b, cfg.debounce_ms, cfg.double_click_ms);

    /* First BOOT press: hold 30 ms debounce, then PRESS. */
    s_fake_pressed = false;
    s_fake_now_ms = 1000;
    MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
             BUTTON_EVENT_NONE);
    s_fake_pressed = true;
    for (t = 1001; t <= 1030u; t++) {
        s_fake_now_ms = t;
        MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
                 BUTTON_EVENT_NONE);
    }
    s_fake_now_ms = 1031;
    MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
             BUTTON_EVENT_PRESS);

    /* Release settles; second press inside the 400 ms window is dropped. */
    s_fake_pressed = false;
    for (t = 1031; t <= 1061u; t++) {
        s_fake_now_ms = t;
        MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
                 BUTTON_EVENT_NONE);
    }
    s_fake_pressed = true;
    for (t = 1200; t < 1230u; t++) {
        s_fake_now_ms = t;
        MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
                 BUTTON_EVENT_NONE);
    }
    s_fake_now_ms = 1230;
    MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
             BUTTON_EVENT_DROPPED);

    /* Release again, then press after the window: PRESS. */
    s_fake_pressed = false;
    for (t = 1231; t <= 1261u; t++) {
        s_fake_now_ms = t;
        MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
                 BUTTON_EVENT_NONE);
    }
    s_fake_pressed = true;
    for (t = 2000; t < 2030u; t++) {
        s_fake_now_ms = t;
        MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
                 BUTTON_EVENT_NONE);
    }
    s_fake_now_ms = 2030;
    MU_CHECK(button_update(&b, fake_hal_button_read(), s_fake_now_ms) ==
             BUTTON_EVENT_PRESS);
}

/* Typing path: whole-string emit counts with the S3-shaped callback. */
static void test_s3_emit_counts(void) {
    keystroker_config_t cfg;
    typing_engine_t eng;
    uint32_t actions = 0;

    config_set_defaults(&cfg);
    MU_CHECK(config_parse_string(kS3FallbackConfig, &cfg) == CFG_OK);

    s_sent_press = 0;
    s_sent_release = 0;
    s_delay_calls = 0;
    s_first_modifier = 0;
    s_first_keycode = 0;
    type_all_like_main(&cfg, "Hi\n", &eng, &actions);
    MU_EQ_ULL(6, actions); /* press + release per char */
    MU_EQ_ULL(3, s_sent_press);
    MU_EQ_ULL(3, s_sent_release);
    MU_EQ_ULL(6, s_delay_calls); /* one delay per emitted action */
    MU_EQ_ULL(8, s_last_delay_ms); /* TYPING_DELAY_MS from config */
    MU_EQ_ULL(3, eng.chars_typed);
    MU_EQ_ULL(0, eng.chars_skipped);
    /* 'H' is shift + 0x0B on en_US. */
    MU_EQ_ULL(HID_MOD_LSHIFT, s_first_modifier);
    MU_EQ_ULL(0x0B, s_first_keycode);

    /* Unmapped bytes are skipped, not emitted (main types nothing extra). */
    s_sent_press = 0;
    s_sent_release = 0;
    s_delay_calls = 0;
    type_all_like_main(&cfg, "a\x01" "b", &eng, &actions);
    MU_EQ_ULL(4, actions);
    MU_EQ_ULL(2, s_sent_press);
    MU_EQ_ULL(2, s_sent_release);
    MU_EQ_ULL(1, eng.chars_skipped);

    /* NULL action guard never touches the HAL. */
    s_delay_calls = 0;
    s3_emit_action(NULL, &eng);
    MU_EQ_ULL(0, s_delay_calls);
}

int main(void) {
    test_s3_config_path();
    test_s3_button_window();
    test_s3_emit_counts();
    MU_REPORT("s3_contract");
    return mu_failed != 0;
}
