#ifndef KEYSTROKER_CONFIG_PARSER_H
#define KEYSTROKER_CONFIG_PARSER_H

/*
 * Minimal INI parser for keystroker config files.
 *
 * Supported keys (case-insensitive; '#' and ';' start a comment,
 * surrounding whitespace is ignored, '[section]' lines are skipped):
 *
 *   WRITE_MODE         "button" | "time"      (required)
 *   COUNTDOWN_MS       positive integer       (required in "time" mode,
 *                                             ignored in "button" mode;
 *                                             range 1..3600000)
 *   HID_KEYBOARD_NAME  string                 (default, see below)
 *   LANGUAGE           en_US | tr_TR | de_DE | fr_FR (default en_US)
 *   BUTTON_PIN         GPIO number 0..29      (default 15)
 *   TYPING_DELAY_MS    0..60000               (default 8)
 *   DOUBLE_CLICK_MS    0..60000               (default 400)
 *   DEBOUNCE_MS        0..60000               (default 30)
 *
 * Parsing is strict about *values* (a malformed value is an error) but
 * lenient about *structure*: unknown keys, '[section]' headers, blank
 * lines and lines without '=' are ignored so configs stay forward
 * compatible. Missing files are reported via CFG_ERR_FILE.
 *
 * No dynamic allocation: the caller owns keystroker_config_t (fixed-size
 * buffers) and parsing never allocates.
 */

#include <stdint.h>
#include <stdbool.h>

#include "hid_keymap.h" /* lang_t */

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_NAME_MAX 64u

#define CFG_DEFAULT_NAME "Arboard-2040-pico-keystroker"
#define CFG_DEFAULT_LANGUAGE LANG_EN_US
#define CFG_DEFAULT_BUTTON_PIN 15
#define CFG_DEFAULT_TYPING_DELAY_MS 8u
#define CFG_DEFAULT_DOUBLE_CLICK_MS 400u
#define CFG_DEFAULT_DEBOUNCE_MS 30u

#define CFG_COUNTDOWN_MAX_MS 3600000u
#define CFG_DELAY_MAX_MS 60000u
#define CFG_BUTTON_PIN_MAX 29

typedef enum {
    WRITE_MODE_BUTTON = 0,
    WRITE_MODE_TIME   = 1
} write_mode_t;

typedef enum {
    CFG_OK = 0,
    CFG_ERR_NULL,          /* NULL config (or NULL path/text) argument */
    CFG_ERR_FILE,          /* config file could not be opened/read */
    CFG_ERR_MISSING_MODE,  /* WRITE_MODE key absent (see config_validate) */
    CFG_ERR_BAD_MODE,      /* WRITE_MODE value is not button|time */
    CFG_ERR_BAD_COUNTDOWN,  /* COUNTDOWN_MS missing/invalid, or required
                               but absent in time mode */
    CFG_ERR_BAD_UINT,      /* a *_MS value is not a valid uint in range */
    CFG_ERR_BAD_PIN,       /* BUTTON_PIN out of range 0..29 */
    CFG_ERR_BAD_LANG       /* LANGUAGE tag not recognised */
} config_error_t;

typedef struct {
    write_mode_t write_mode;
    bool has_write_mode;
    uint32_t countdown_ms;
    bool has_countdown_ms;
    char hid_keyboard_name[CFG_NAME_MAX];
    lang_t language;
    uint32_t double_click_ms;
    uint32_t debounce_ms;
    int button_pin;
    uint32_t typing_delay_ms;
} keystroker_config_t;

/* Reset cfg to compiled-in defaults (WRITE_MODE left unset/required). */
void config_set_defaults(keystroker_config_t *cfg);

/*
 * Parse INI text into cfg. cfg is first reset to defaults, then every
 * recognised key overrides its default. Returns CFG_OK or the first
 * value error encountered; check config_validate() afterwards for
 * cross-key rules (required WRITE_MODE, COUNTDOWN_MS in time mode).
 */
config_error_t config_parse_string(const char *text, keystroker_config_t *cfg);

/* Same as config_parse_string but reads `path` from the filesystem. */
config_error_t config_parse_file(const char *path, keystroker_config_t *cfg);

/*
 * Cross-key validation: WRITE_MODE must be present; LANGUAGE/PIN must be
 * in range; in WRITE_MODE_TIME a positive COUNTDOWN_MS must be present.
 */
config_error_t config_validate(const keystroker_config_t *cfg);

/* Human-readable name for an error code (never NULL). */
const char *config_error_str(config_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* KEYSTROKER_CONFIG_PARSER_H */
