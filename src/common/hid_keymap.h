#ifndef KEYSTROKER_HID_KEYMAP_H
#define KEYSTROKER_HID_KEYMAP_H

/*
 * ASCII (0-127) -> USB HID boot-protocol (keycode, modifier) mapping.
 *
 * The base table covers the en_US layout: a-z A-Z 0-9, space, TAB, ENTER
 * (for '\n' and '\r') and US punctuation. Per-language override tables
 * handle keys that differ on other layouts:
 *   tr_TR : '@' is AltGr+Q (ASCII letters/digits match en_US on the
 *           Turkish-Q layout, so no other overrides are needed).
 *   de_DE : Y/Z swapped vs US; '@' is AltGr+Q.
 *   fr_FR : AZERTY letter positions A<->Q, W<->Z, M on the US ';' key.
 * Anything not covered by an override falls back to the en_US mapping.
 * Callers that need exact national-layout punctuation/digits (e.g. digits
 * on AZERTY need Shift, several symbols move) must extend the override
 * tables below; unmapped bytes make hid_keycode_for_ascii() return false.
 *
 * All tables are small `static const` arrays with no dependencies, so they
 * are PROGMEM/flash friendly on the RP2040 target.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LANG_EN_US   = 0,
    LANG_TR_TR   = 1,
    LANG_DE_DE   = 2,
    LANG_FR_FR   = 3,
    LANG_UNKNOWN = 99
} lang_t;

/* HID boot-protocol modifier bitmap. */
#define HID_MOD_NONE   0x00u
#define HID_MOD_LCTRL  0x01u
#define HID_MOD_LSHIFT 0x02u
#define HID_MOD_LALT   0x04u
#define HID_MOD_LGUI   0x08u
#define HID_MOD_RCTRL  0x10u
#define HID_MOD_RSHIFT 0x20u
#define HID_MOD_RALT   0x40u /* AltGr on de/tr layouts */
#define HID_MOD_RGUI   0x80u

#define HID_KEY_NONE  0x00u
#define HID_KEY_ENTER 0x28u
#define HID_KEY_TAB   0x2Bu
#define HID_KEY_SPACE 0x2Cu

/*
 * Parse a language tag (case-insensitive). Accepts "en_US", "en-US" and
 * "en" style tags for en/tr/de/fr. Returns LANG_UNKNOWN for anything else
 * (including NULL).
 */
lang_t lang_from_string(const char *s);

/* Inverse of lang_from_string(); LANG_UNKNOWN maps to "unknown". */
const char *lang_to_string(lang_t lang);

/*
 * Translate one ASCII character to a (keycode, modifier) pair for the
 * given layout. Returns true on success, false when the character has no
 * mapping (e.g. control chars other than \t \n \r, DEL) or when keycode /
 * modifier is NULL. Caller passes the byte as `char`; values 0-127 only.
 */
bool hid_keycode_for_ascii(char c, lang_t lang,
                           uint8_t *keycode, uint8_t *modifier);

#ifdef __cplusplus
}
#endif

#endif /* KEYSTROKER_HID_KEYMAP_H */
