#include "config_parser.h"

#include <stdio.h>
#include <string.h>

#define CFG_LINE_MAX 256u

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static bool key_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (ascii_lower(*a) != ascii_lower(*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* Trim leading/trailing whitespace in place. */
static void trim(char *s) {
    size_t n;
    size_t i = 0;
    while (s[i] != '\0' && is_space(s[i])) {
        i++;
    }
    if (i > 0) {
        size_t j = 0;
        while (s[i] != '\0') {
            s[j] = s[i];
            j++;
            i++;
        }
        s[j] = '\0';
    }
    n = strlen(s);
    while (n > 0 && is_space(s[n - 1u])) {
        n--;
    }
    s[n] = '\0';
}

/*
 * Cut an inline trailing comment: '#' or ';' counts as a comment marker
 * only at the start of the value or right after whitespace, so values
 * containing those glyphs keep working.
 */
static void strip_inline_comment(char *v) {
    size_t i = 0;
    while (v[i] != '\0') {
        if ((v[i] == '#' || v[i] == ';') &&
            (i == 0 || v[i - 1u] == ' ' || v[i - 1u] == '\t')) {
            v[i] = '\0';
            return;
        }
        i++;
    }
}

static bool parse_u32(const char *s, uint32_t *out) {
    uint32_t acc = 0;
    size_t i = 0;
    if (s[0] == '\0' || !is_digit(s[0])) {
        return false;
    }
    while (s[i] != '\0') {
        uint32_t d;
        if (!is_digit(s[i])) {
            return false;
        }
        d = (uint32_t)(s[i] - '0');
        if (acc > (UINT32_C(4294967295) - d) / 10u) {
            return false; /* overflow */
        }
        acc = acc * 10u + d;
        i++;
    }
    *out = acc;
    return true;
}

static bool parse_int(const char *s, int *out) {
    uint32_t mag = 0;
    bool neg = false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') {
        neg = (s[0] == '-');
        i = 1;
    }
    if (s[i] == '\0' || !is_digit(s[i])) {
        return false;
    }
    while (s[i] != '\0') {
        if (!is_digit(s[i])) {
            return false;
        }
        mag = mag * 10u + (uint32_t)(s[i] - '0');
        if (mag > (uint32_t)CFG_BUTTON_PIN_MAX + 1u) {
            return false; /* out of plausible pin range, fail fast */
        }
        i++;
    }
    *out = neg ? -(int)mag : (int)mag;
    return true;
}

static void set_name(keystroker_config_t *cfg, const char *v) {
    size_t i = 0;
    if (v[0] == '\0') {
        return; /* empty value keeps the current (default) name */
    }
    while (v[i] != '\0' && i < CFG_NAME_MAX - 1u) {
        cfg->hid_keyboard_name[i] = v[i];
        i++;
    }
    cfg->hid_keyboard_name[i] = '\0';
}

static config_error_t parse_key_value(char *line, keystroker_config_t *cfg) {
    char *eq;
    char *key;
    char *val;
    uint32_t u;
    int pin;
    lang_t lang;

    trim(line);
    if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
        return CFG_OK;
    }
    if (line[0] == '[') {
        return CFG_OK; /* section header: ignored */
    }
    eq = strchr(line, '=');
    if (eq == NULL) {
        return CFG_OK; /* junk line without '=': ignored */
    }
    *eq = '\0';
    key = line;
    val = eq + 1;
    trim(key);
    trim(val);
    strip_inline_comment(val);
    trim(val);

    if (key_eq(key, "WRITE_MODE")) {
        if (key_eq(val, "button")) {
            cfg->write_mode = WRITE_MODE_BUTTON;
            cfg->has_write_mode = true;
        } else if (key_eq(val, "time")) {
            cfg->write_mode = WRITE_MODE_TIME;
            cfg->has_write_mode = true;
        } else {
            return CFG_ERR_BAD_MODE;
        }
    } else if (key_eq(key, "COUNTDOWN_MS")) {
        if (!parse_u32(val, &u) || u == 0 || u > CFG_COUNTDOWN_MAX_MS) {
            return CFG_ERR_BAD_COUNTDOWN;
        }
        cfg->countdown_ms = u;
        cfg->has_countdown_ms = true;
    } else if (key_eq(key, "HID_KEYBOARD_NAME")) {
        set_name(cfg, val);
    } else if (key_eq(key, "LANGUAGE")) {
        lang = lang_from_string(val);
        if (lang == LANG_UNKNOWN) {
            return CFG_ERR_BAD_LANG;
        }
        cfg->language = lang;
    } else if (key_eq(key, "BUTTON_PIN")) {
        if (!parse_int(val, &pin) ||
            pin < 0 || pin > CFG_BUTTON_PIN_MAX) {
            return CFG_ERR_BAD_PIN;
        }
        cfg->button_pin = pin;
    } else if (key_eq(key, "TYPING_DELAY_MS")) {
        if (!parse_u32(val, &u) || u > CFG_DELAY_MAX_MS) {
            return CFG_ERR_BAD_UINT;
        }
        cfg->typing_delay_ms = u;
    } else if (key_eq(key, "DOUBLE_CLICK_MS")) {
        if (!parse_u32(val, &u) || u > CFG_DELAY_MAX_MS) {
            return CFG_ERR_BAD_UINT;
        }
        cfg->double_click_ms = u;
    } else if (key_eq(key, "DEBOUNCE_MS")) {
        if (!parse_u32(val, &u) || u > CFG_DELAY_MAX_MS) {
            return CFG_ERR_BAD_UINT;
        }
        cfg->debounce_ms = u;
    } else {
        return CFG_OK; /* unknown key: ignored for forward compatibility */
    }
    return CFG_OK;
}

void config_set_defaults(keystroker_config_t *cfg) {
    if (cfg == NULL) {
        return;
    }
    cfg->write_mode = WRITE_MODE_BUTTON;
    cfg->has_write_mode = false;
    cfg->countdown_ms = 0;
    cfg->has_countdown_ms = false;
    set_name(cfg, CFG_DEFAULT_NAME);
    cfg->language = CFG_DEFAULT_LANGUAGE;
    cfg->double_click_ms = CFG_DEFAULT_DOUBLE_CLICK_MS;
    cfg->debounce_ms = CFG_DEFAULT_DEBOUNCE_MS;
    cfg->button_pin = CFG_DEFAULT_BUTTON_PIN;
    cfg->typing_delay_ms = CFG_DEFAULT_TYPING_DELAY_MS;
}

config_error_t config_parse_string(const char *text, keystroker_config_t *cfg) {
    char line[CFG_LINE_MAX];
    size_t li = 0;
    size_t i = 0;
    if (text == NULL || cfg == NULL) {
        return CFG_ERR_NULL;
    }
    config_set_defaults(cfg);
    for (;;) {
        char c = text[i];
        if (c == '\n' || c == '\0') {
            config_error_t perr;
            line[li] = '\0';
            perr = parse_key_value(line, cfg);
            if (perr != CFG_OK) {
                return perr;
            }
            li = 0;
            if (c == '\0') {
                break;
            }
            i++;
        } else if (c != '\r') {
            if (li < sizeof(line) - 1u) {
                line[li] = c;
                li++;
            }
            /* overlong lines are truncated to CFG_LINE_MAX - 1 chars */
            i++;
        } else {
            i++; /* bare CR is not significant outside CRLF handling */
        }
    }
    return CFG_OK;
}

config_error_t config_parse_file(const char *path, keystroker_config_t *cfg) {
    FILE *f;
    char line[CFG_LINE_MAX];
    if (path == NULL || cfg == NULL) {
        return CFG_ERR_NULL;
    }
    f = fopen(path, "r");
    if (f == NULL) {
        return CFG_ERR_FILE;
    }
    config_set_defaults(cfg);
    while (fgets(line, (int)sizeof(line), f) != NULL) {
        size_t n = strlen(line);
        config_error_t perr;
        if (n > 0 && line[n - 1u] != '\n' && !feof(f)) {
            int ch; /* overlong line: discard the remainder */
            do {
                ch = fgetc(f);
            } while (ch != '\n' && ch != EOF);
        }
        perr = parse_key_value(line, cfg);
        if (perr != CFG_OK) {
            fclose(f);
            return perr;
        }
    }
    fclose(f);
    return CFG_OK;
}

config_error_t config_validate(const keystroker_config_t *cfg) {
    if (cfg == NULL) {
        return CFG_ERR_NULL;
    }
    if (!cfg->has_write_mode) {
        return CFG_ERR_MISSING_MODE;
    }
    if (cfg->language == LANG_UNKNOWN) {
        return CFG_ERR_BAD_LANG;
    }
    if (cfg->button_pin < 0 || cfg->button_pin > CFG_BUTTON_PIN_MAX) {
        return CFG_ERR_BAD_PIN;
    }
    if (cfg->write_mode == WRITE_MODE_TIME) {
        if (!cfg->has_countdown_ms || cfg->countdown_ms == 0) {
            return CFG_ERR_BAD_COUNTDOWN;
        }
    }
    return CFG_OK;
}

const char *config_error_str(config_error_t err) {
    switch (err) {
    case CFG_OK:
        return "ok";
    case CFG_ERR_NULL:
        return "null argument";
    case CFG_ERR_FILE:
        return "cannot open config file";
    case CFG_ERR_MISSING_MODE:
        return "WRITE_MODE is required";
    case CFG_ERR_BAD_MODE:
        return "WRITE_MODE must be button|time";
    case CFG_ERR_BAD_COUNTDOWN:
        return "COUNTDOWN_MS must be a positive integer (required in time mode)";
    case CFG_ERR_BAD_UINT:
        return "value must be an unsigned integer in range";
    case CFG_ERR_BAD_PIN:
        return "BUTTON_PIN must be a GPIO number 0..29";
    case CFG_ERR_BAD_LANG:
        return "LANGUAGE must be en_US|tr_TR|de_DE|fr_FR";
    default:
        return "unknown error";
    }
}
