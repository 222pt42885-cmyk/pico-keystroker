/* Tests for src/common/config_parser.c */

#include <stdio.h>
#include <string.h>

#include "minunit.h"
#include "config_parser.h"

static void test_defaults(void) {
    keystroker_config_t cfg;
    memset(&cfg, 0xAA, sizeof(cfg));
    config_set_defaults(&cfg);
    MU_CHECK(!cfg.has_write_mode);
    MU_CHECK(!cfg.has_countdown_ms);
    MU_EQ_ULL(0, cfg.countdown_ms);
    MU_EQ_STR(CFG_DEFAULT_NAME, cfg.hid_keyboard_name);
    MU_CHECK(cfg.language == CFG_DEFAULT_LANGUAGE);
    MU_EQ_ULL(CFG_DEFAULT_DOUBLE_CLICK_MS, cfg.double_click_ms);
    MU_EQ_ULL(CFG_DEFAULT_DEBOUNCE_MS, cfg.debounce_ms);
    MU_EQ_LL(CFG_DEFAULT_BUTTON_PIN, cfg.button_pin);
    MU_EQ_ULL(CFG_DEFAULT_TYPING_DELAY_MS, cfg.typing_delay_ms);
    config_set_defaults(NULL); /* must not crash */
}

static void test_time_mode_full(void) {
    keystroker_config_t cfg;
    config_error_t err = config_parse_string(
        "# sample config\n"
        "[device]\n"
        "WRITE_MODE = time\n"
        "COUNTDOWN_MS = 5000\n"
        "HID_KEYBOARD_NAME = My Board\n"
        "LANGUAGE = de_DE\n"
        "DOUBLE_CLICK_MS = 350\n"
        "DEBOUNCE_MS = 25\n"
        "BUTTON_PIN = 3\n"
        "TYPING_DELAY_MS = 12\n",
        &cfg);
    MU_CHECK(err == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.write_mode == WRITE_MODE_TIME);
    MU_EQ_ULL(5000, cfg.countdown_ms);
    MU_CHECK(cfg.has_countdown_ms);
    MU_EQ_STR("My Board", cfg.hid_keyboard_name);
    MU_CHECK(cfg.language == LANG_DE_DE);
    MU_EQ_ULL(350, cfg.double_click_ms);
    MU_EQ_ULL(25, cfg.debounce_ms);
    MU_EQ_LL(3, cfg.button_pin);
    MU_EQ_ULL(12, cfg.typing_delay_ms);
}

static void test_button_mode_minimal(void) {
    keystroker_config_t cfg;
    /* COUNTDOWN_MS is ignored in button mode. */
    config_error_t err = config_parse_string(
        "write_mode=BUTTON\n"
        "countdown_ms=9999\n",
        &cfg);
    MU_CHECK(err == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.write_mode == WRITE_MODE_BUTTON);
    /* Defaults still apply to everything else. */
    MU_EQ_STR(CFG_DEFAULT_NAME, cfg.hid_keyboard_name);
    MU_CHECK(cfg.language == LANG_EN_US);
    MU_EQ_ULL(CFG_DEFAULT_TYPING_DELAY_MS, cfg.typing_delay_ms);
}

static void test_comments_whitespace_case(void) {
    keystroker_config_t cfg;
    config_error_t err = config_parse_string(
        "   ; leading comment\n"
        "\t# another comment\n"
        "\n"
        "   \n"
        "junk line without equals\n"
        "UNKNOWN_FUTURE_KEY = 123\n"
        "[section] trailing junk\n"
        "  Write_Mode\t =\t Time  \n"
        "COUNTDOWN_MS=1000 # one second\n"
        "BUTTON_PIN = 15 ; gpio\n",
        &cfg);
    MU_CHECK(err == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.write_mode == WRITE_MODE_TIME);
    MU_EQ_ULL(1000, cfg.countdown_ms);
    MU_EQ_LL(15, cfg.button_pin);
}

static void test_crlf_and_empty_value(void) {
    keystroker_config_t cfg;
    config_error_t err = config_parse_string(
        "WRITE_MODE=button\r\n"
        "HID_KEYBOARD_NAME=\r\n"
        "LANGUAGE=TR_tr\r\n",
        &cfg);
    MU_CHECK(err == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_EQ_STR(CFG_DEFAULT_NAME, cfg.hid_keyboard_name); /* empty kept */
    MU_CHECK(cfg.language == LANG_TR_TR);
}

static void test_inline_comment_rules(void) {
    keystroker_config_t cfg;
    /* '#' glued to the value is kept; after a space it starts a comment. */
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=button\nHID_KEYBOARD_NAME=My#Board\n", &cfg)
             == CFG_OK);
    MU_EQ_STR("My#Board", cfg.hid_keyboard_name);
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=button\nHID_KEYBOARD_NAME=My # Board\n", &cfg)
             == CFG_OK);
    MU_EQ_STR("My", cfg.hid_keyboard_name);
    /* Long names are truncated, always NUL-terminated. */
    {
        char text[160];
        size_t i;
        strcpy(text, "WRITE_MODE=button\nHID_KEYBOARD_NAME=");
        for (i = strlen(text); i < sizeof(text) - 2u; i++) {
            text[i] = 'x';
        }
        text[sizeof(text) - 2u] = '\n';
        text[sizeof(text) - 1u] = '\0';
        MU_CHECK(config_parse_string(text, &cfg) == CFG_OK);
        MU_EQ_ULL(CFG_NAME_MAX - 1u, strlen(cfg.hid_keyboard_name));
    }
}

static void test_value_errors(void) {
    keystroker_config_t cfg;
    MU_CHECK(config_parse_string("WRITE_MODE=voice\n", &cfg)
             == CFG_ERR_BAD_MODE);
    MU_CHECK(config_parse_string("WRITE_MODE=time\nCOUNTDOWN_MS=0\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string("WRITE_MODE=time\nCOUNTDOWN_MS=abc\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string("WRITE_MODE=time\nCOUNTDOWN_MS=-5\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string("WRITE_MODE=time\nCOUNTDOWN_MS=12x\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string("WRITE_MODE=time\nCOUNTDOWN_MS=\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=time\nCOUNTDOWN_MS=99999999999999999999\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=time\nCOUNTDOWN_MS=3600001\n", &cfg)
             == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nLANGUAGE=xx\n", &cfg)
             == CFG_ERR_BAD_LANG);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=30\n", &cfg)
             == CFG_ERR_BAD_PIN);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=-1\n", &cfg)
             == CFG_ERR_BAD_PIN);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=x\n", &cfg)
             == CFG_ERR_BAD_PIN);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=999\n", &cfg)
             == CFG_ERR_BAD_PIN);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=1x\n", &cfg)
             == CFG_ERR_BAD_PIN);
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=button\nTYPING_DELAY_MS=many\n", &cfg)
             == CFG_ERR_BAD_UINT);
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=button\nTYPING_DELAY_MS=60001\n", &cfg)
             == CFG_ERR_BAD_UINT);
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=button\nDOUBLE_CLICK_MS=42949672960\n", &cfg)
             == CFG_ERR_BAD_UINT);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nDEBOUNCE_MS=-\n", &cfg)
             == CFG_ERR_BAD_UINT);
    /* Boundary values are accepted. */
    MU_CHECK(config_parse_string(
                 "WRITE_MODE=time\nCOUNTDOWN_MS=3600000\n"
                 "BUTTON_PIN=0\nTYPING_DELAY_MS=0\n"
                 "DOUBLE_CLICK_MS=60000\nDEBOUNCE_MS=60000\n"
                 "LANGUAGE=fr_FR\n", &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_EQ_LL(0, cfg.button_pin);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=+7\n", &cfg)
             == CFG_OK);
    MU_EQ_LL(7, cfg.button_pin);
    MU_CHECK(config_parse_string("WRITE_MODE=button\nBUTTON_PIN=29\n", &cfg)
             == CFG_OK);
    MU_EQ_LL(29, cfg.button_pin);
    MU_CHECK(config_parse_string("WRITE_MODE=button\n=5\n", &cfg) == CFG_OK);
}

static void test_missing_and_null(void) {
    keystroker_config_t cfg;
    MU_CHECK(config_parse_string("", &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_ERR_MISSING_MODE);
    MU_CHECK(config_parse_string("COUNTDOWN_MS=100\n", &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_ERR_MISSING_MODE);
    MU_CHECK(config_parse_string("WRITE_MODE=time\n", &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_ERR_BAD_COUNTDOWN);
    MU_CHECK(config_parse_string(NULL, &cfg) == CFG_ERR_NULL);
    MU_CHECK(config_parse_string("WRITE_MODE=button\n", NULL) == CFG_ERR_NULL);
    MU_CHECK(config_parse_string(NULL, NULL) == CFG_ERR_NULL);
    MU_CHECK(config_parse_file(NULL, &cfg) == CFG_ERR_NULL);
    MU_CHECK(config_parse_file("nope.ini", NULL) == CFG_ERR_NULL);
    MU_CHECK(config_validate(NULL) == CFG_ERR_NULL);
    /* Hand-corrupted structs are rejected by validation. */
    MU_CHECK(config_parse_string("WRITE_MODE=button\n", &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    cfg.language = LANG_UNKNOWN;
    MU_CHECK(config_validate(&cfg) == CFG_ERR_BAD_LANG);
    cfg.language = LANG_EN_US;
    cfg.button_pin = 99;
    MU_CHECK(config_validate(&cfg) == CFG_ERR_BAD_PIN);
    /* Missing file. */
    MU_CHECK(config_parse_file("does/not/exist.ini", &cfg) == CFG_ERR_FILE);
}

static void test_error_strings(void) {
    MU_EQ_STR("ok", config_error_str(CFG_OK));
    MU_EQ_STR("null argument", config_error_str(CFG_ERR_NULL));
    MU_EQ_STR("cannot open config file", config_error_str(CFG_ERR_FILE));
    MU_EQ_STR("WRITE_MODE is required",
              config_error_str(CFG_ERR_MISSING_MODE));
    MU_EQ_STR("WRITE_MODE must be button|time",
              config_error_str(CFG_ERR_BAD_MODE));
    MU_CHECK(config_error_str(CFG_ERR_BAD_COUNTDOWN) != NULL);
    MU_CHECK(config_error_str(CFG_ERR_BAD_UINT) != NULL);
    MU_CHECK(config_error_str(CFG_ERR_BAD_PIN) != NULL);
    MU_CHECK(config_error_str(CFG_ERR_BAD_LANG) != NULL);
    MU_EQ_STR("unknown error", config_error_str((config_error_t)123));
}

static void write_tmp(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    MU_CHECK(f != NULL);
    if (f != NULL) {
        fputs(content, f);
        fclose(f);
    }
}

static void test_parse_file(void) {
    static const char *tmp = "build/host/tmp_config_test.ini";
    keystroker_config_t cfg;
    FILE *f;
    size_t i;

    /* Round-trip through a real file ... */
    write_tmp(tmp,
              "; file comment\n"
              "WRITE_MODE = time\r\n"
              "COUNTDOWN_MS = 2500\n"
              "LANGUAGE = en_US\n");
    MU_CHECK(config_parse_file(tmp, &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.write_mode == WRITE_MODE_TIME);
    MU_EQ_ULL(2500, cfg.countdown_ms);

    /* ... a bad value in a file reports the value error ... */
    write_tmp(tmp, "WRITE_MODE = time\nCOUNTDOWN_MS = nope\n");
    MU_CHECK(config_parse_file(tmp, &cfg) == CFG_ERR_BAD_COUNTDOWN);

    /* ... and an overlong line is truncated, its tail discarded. */
    f = fopen(tmp, "w");
    MU_CHECK(f != NULL);
    if (f != NULL) {
        fputs("WRITE_MODE=button", f);
        for (i = 0; i < 300u; i++) {
            fputc(' ', f);
        }
        fputs("\nLANGUAGE=de_DE\n", f);
        fclose(f);
    }
    MU_CHECK(config_parse_file(tmp, &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.language == LANG_DE_DE);

    /* Last line without trailing newline still parses. */
    write_tmp(tmp, "WRITE_MODE=button");
    MU_CHECK(config_parse_file(tmp, &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);

    remove(tmp);
}

static void test_overlong_string_line(void) {
    /* A single >255 char line in string input is truncated safely. */
    char text[420];
    keystroker_config_t cfg;
    size_t pos = 0;
    size_t i;
    const char *head = "WRITE_MODE=button";
    for (i = 0; head[i] != '\0'; i++) {
        text[pos++] = head[i];
    }
    for (i = 0; i < 300u; i++) {
        text[pos++] = ' ';
    }
    text[pos++] = '\n';
    strcpy(&text[pos], "LANGUAGE=fr_FR\n");
    MU_CHECK(config_parse_string(text, &cfg) == CFG_OK);
    MU_CHECK(config_validate(&cfg) == CFG_OK);
    MU_CHECK(cfg.language == LANG_FR_FR);
}

int main(void) {
    test_defaults();
    test_time_mode_full();
    test_button_mode_minimal();
    test_comments_whitespace_case();
    test_crlf_and_empty_value();
    test_inline_comment_rules();
    test_value_errors();
    test_missing_and_null();
    test_error_strings();
    test_parse_file();
    test_overlong_string_line();
    MU_REPORT("config_parser");
    return mu_failed != 0;
}
