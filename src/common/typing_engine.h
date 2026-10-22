#ifndef KEYSTROKER_TYPING_ENGINE_H
#define KEYSTROKER_TYPING_ENGINE_H

/*
 * Hardware-independent typing engine: turns a payload string into a
 * sequence of HID actions (press + release per character).
 *
 * The engine is pure logic -- it never touches hardware. Each character
 * maps through hid_keymap.h for the configured language; '\n' (and '\r')
 * become ENTER press+release. Characters without a mapping are skipped
 * and counted in chars_skipped.
 *
 * Timing is the caller's job: after each emitted action the firmware waits
 * typing_delay_ms (see typing_engine_delay_ms()) and pushes the action
 * out via hal_hid_send(). Two APIs are offered:
 *   - typing_engine_type_string(): push whole string through a callback.
 *   - typing_engine_next(): pull iterator for one action at a time.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "hal.h"
#include "hid_keymap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t modifier; /* HID modifier bitmap for press; 0 on release */
    uint8_t keycode;  /* HID usage ID for press; 0 on release */
    bool is_release;  /* false = press, true = release (all keys up) */
} hid_action_t;

typedef void (*hid_action_cb_t)(const hid_action_t *action, void *user);

typedef struct {
    lang_t language;
    uint32_t typing_delay_ms; /* inter-action delay; enforced by caller */
} typing_engine_config_t;

typedef struct {
    typing_engine_config_t config;
    uint32_t chars_typed;   /* mappable chars emitted since init */
    uint32_t chars_skipped; /* unmapped chars skipped since init */
} typing_engine_t;

/* Resume position for typing_engine_next(). */
typedef struct {
    size_t index;         /* next string index to examine */
    bool release_pending; /* next action is the release of the last press */
} typing_cursor_t;

void typing_engine_init(typing_engine_t *eng,
                        const typing_engine_config_t *cfg);

/* Inter-action delay the caller should apply (0 when eng is NULL). */
uint32_t typing_engine_delay_ms(const typing_engine_t *eng);

void typing_cursor_init(typing_cursor_t *cursor);

/*
 * Pull the next action for `str`. Returns true and fills *out while
 * actions remain; returns false at end of string. Unmapped characters
 * are skipped silently (counted in chars_skipped).
 */
bool typing_engine_next(typing_engine_t *eng, const char *str,
                        typing_cursor_t *cursor, hid_action_t *out);

/*
 * Push the whole string through cb (cb may be NULL to just count).
 * Returns the number of actions emitted (2 per typed char).
 */
uint32_t typing_engine_type_string(typing_engine_t *eng, const char *str,
                                   hid_action_cb_t cb, void *user);

#ifdef __cplusplus
}
#endif

#endif /* KEYSTROKER_TYPING_ENGINE_H */
