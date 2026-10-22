/* Tests for src/common/hid_keymap.c */

#include <stdint.h>

#include "minunit.h"
#include "hid_keymap.h"

static void check_map(char c, lang_t lang, uint8_t key, uint8_t mod) {
    uint8_t k = 0xFF;
    uint8_t m = 0xFF;
    MU_CHECK(hid_keycode_for_ascii(c, lang, &k, &m));
    MU_EQ_ULL(key, k);
    MU_EQ_ULL(mod, m);
}

static void test_lang_strings(void) {
    MU_CHECK(lang_from_string("en_US") == LANG_EN_US);
    MU_CHECK(lang_from_string("EN-us") == LANG_EN_US);
    MU_CHECK(lang_from_string("en") == LANG_EN_US);
    MU_CHECK(lang_from_string("tr_TR") == LANG_TR_TR);
    MU_CHECK(lang_from_string("TR") == LANG_TR_TR);
    MU_CHECK(lang_from_string("tr-tr") == LANG_TR_TR);
    MU_CHECK(lang_from_string("de_DE") == LANG_DE_DE);
    MU_CHECK(lang_from_string("De") == LANG_DE_DE);
    MU_CHECK(lang_from_string("de-de") == LANG_DE_DE);
    MU_CHECK(lang_from_string("fr_FR") == LANG_FR_FR);
    MU_CHECK(lang_from_string("FR") == LANG_FR_FR);
    MU_CHECK(lang_from_string("fr-fr") == LANG_FR_FR);
    MU_CHECK(lang_from_string("xx") == LANG_UNKNOWN);
    MU_CHECK(lang_from_string("") == LANG_UNKNOWN);
    MU_CHECK(lang_from_string(NULL) == LANG_UNKNOWN);
    MU_CHECK(lang_from_string("en_US_but_longer!!") == LANG_UNKNOWN);

    MU_EQ_STR("en_US", lang_to_string(LANG_EN_US));
    MU_EQ_STR("tr_TR", lang_to_string(LANG_TR_TR));
    MU_EQ_STR("de_DE", lang_to_string(LANG_DE_DE));
    MU_EQ_STR("fr_FR", lang_to_string(LANG_FR_FR));
    MU_EQ_STR("unknown", lang_to_string(LANG_UNKNOWN));
    MU_EQ_STR("unknown", lang_to_string((lang_t)42));
}

static void test_en_us_letters_digits(void) {
    int c;
    /* a-z map to 0x04..0x1D without modifier ... */
    for (c = 'a'; c <= 'z'; c++) {
        check_map((char)c, LANG_EN_US, (uint8_t)(0x04 + (c - 'a')),
                  HID_MOD_NONE);
    }
    /* ... A-Z add left shift. */
    for (c = 'A'; c <= 'Z'; c++) {
        check_map((char)c, LANG_EN_US, (uint8_t)(0x04 + (c - 'A')),
                  HID_MOD_LSHIFT);
    }
    check_map('1', LANG_EN_US, 0x1E, HID_MOD_NONE);
    check_map('2', LANG_EN_US, 0x1F, HID_MOD_NONE);
    check_map('3', LANG_EN_US, 0x20, HID_MOD_NONE);
    check_map('4', LANG_EN_US, 0x21, HID_MOD_NONE);
    check_map('5', LANG_EN_US, 0x22, HID_MOD_NONE);
    check_map('6', LANG_EN_US, 0x23, HID_MOD_NONE);
    check_map('7', LANG_EN_US, 0x24, HID_MOD_NONE);
    check_map('8', LANG_EN_US, 0x25, HID_MOD_NONE);
    check_map('9', LANG_EN_US, 0x26, HID_MOD_NONE);
    check_map('0', LANG_EN_US, 0x27, HID_MOD_NONE);
}

static void test_en_us_punctuation(void) {
    check_map(' ', LANG_EN_US, HID_KEY_SPACE, HID_MOD_NONE);
    check_map('\t', LANG_EN_US, HID_KEY_TAB, HID_MOD_NONE);
    check_map('\n', LANG_EN_US, HID_KEY_ENTER, HID_MOD_NONE);
    check_map('\r', LANG_EN_US, HID_KEY_ENTER, HID_MOD_NONE);
    check_map('!', LANG_EN_US, 0x1E, HID_MOD_LSHIFT);
    check_map('"', LANG_EN_US, 0x34, HID_MOD_LSHIFT);
    check_map('#', LANG_EN_US, 0x20, HID_MOD_LSHIFT);
    check_map('$', LANG_EN_US, 0x21, HID_MOD_LSHIFT);
    check_map('%', LANG_EN_US, 0x22, HID_MOD_LSHIFT);
    check_map('&', LANG_EN_US, 0x24, HID_MOD_LSHIFT);
    check_map('\'', LANG_EN_US, 0x34, HID_MOD_NONE);
    check_map('(', LANG_EN_US, 0x26, HID_MOD_LSHIFT);
    check_map(')', LANG_EN_US, 0x27, HID_MOD_LSHIFT);
    check_map('*', LANG_EN_US, 0x25, HID_MOD_LSHIFT);
    check_map('+', LANG_EN_US, 0x2E, HID_MOD_LSHIFT);
    check_map(',', LANG_EN_US, 0x36, HID_MOD_NONE);
    check_map('-', LANG_EN_US, 0x2D, HID_MOD_NONE);
    check_map('.', LANG_EN_US, 0x37, HID_MOD_NONE);
    check_map('/', LANG_EN_US, 0x38, HID_MOD_NONE);
    check_map(':', LANG_EN_US, 0x33, HID_MOD_LSHIFT);
    check_map(';', LANG_EN_US, 0x33, HID_MOD_NONE);
    check_map('<', LANG_EN_US, 0x36, HID_MOD_LSHIFT);
    check_map('=', LANG_EN_US, 0x2E, HID_MOD_NONE);
    check_map('>', LANG_EN_US, 0x37, HID_MOD_LSHIFT);
    check_map('?', LANG_EN_US, 0x38, HID_MOD_LSHIFT);
    check_map('@', LANG_EN_US, 0x1F, HID_MOD_LSHIFT);
    check_map('[', LANG_EN_US, 0x2F, HID_MOD_NONE);
    check_map('\\', LANG_EN_US, 0x31, HID_MOD_NONE);
    check_map(']', LANG_EN_US, 0x30, HID_MOD_NONE);
    check_map('^', LANG_EN_US, 0x23, HID_MOD_LSHIFT);
    check_map('_', LANG_EN_US, 0x2D, HID_MOD_LSHIFT);
    check_map('`', LANG_EN_US, 0x35, HID_MOD_NONE);
    check_map('{', LANG_EN_US, 0x2F, HID_MOD_LSHIFT);
    check_map('|', LANG_EN_US, 0x31, HID_MOD_LSHIFT);
    check_map('}', LANG_EN_US, 0x30, HID_MOD_LSHIFT);
    check_map('~', LANG_EN_US, 0x35, HID_MOD_LSHIFT);
}

static void test_unmapped(void) {
    uint8_t k;
    uint8_t m;
    int c;
    /* Control characters (other than \t \n \r) and DEL have no mapping. */
    for (c = 0; c < 0x20; c++) {
        if (c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        MU_CHECK(!hid_keycode_for_ascii((char)c, LANG_EN_US, &k, &m));
    }
    MU_CHECK(!hid_keycode_for_ascii((char)0x7F, LANG_EN_US, &k, &m));
    /* Bytes >= 0x80 are outside ASCII. */
    MU_CHECK(!hid_keycode_for_ascii((char)0x80, LANG_EN_US, &k, &m));
    MU_CHECK(!hid_keycode_for_ascii((char)0xFF, LANG_DE_DE, &k, &m));
    /* NULL out-params are rejected. */
    MU_CHECK(!hid_keycode_for_ascii('a', LANG_EN_US, NULL, &m));
    MU_CHECK(!hid_keycode_for_ascii('a', LANG_EN_US, &k, NULL));
    MU_CHECK(!hid_keycode_for_ascii('a', LANG_EN_US, NULL, NULL));
}

static void test_de_overrides(void) {
    /* Y/Z swap vs US ... */
    check_map('y', LANG_DE_DE, 0x1D, HID_MOD_NONE);
    check_map('Y', LANG_DE_DE, 0x1D, HID_MOD_LSHIFT);
    check_map('z', LANG_DE_DE, 0x1C, HID_MOD_NONE);
    check_map('Z', LANG_DE_DE, 0x1C, HID_MOD_LSHIFT);
    /* ... '@' via AltGr+Q ... */
    check_map('@', LANG_DE_DE, 0x14, HID_MOD_RALT);
    /* ... everything else falls back to en_US. */
    check_map('a', LANG_DE_DE, 0x04, HID_MOD_NONE);
    check_map('!', LANG_DE_DE, 0x1E, HID_MOD_LSHIFT);
    check_map('\n', LANG_DE_DE, HID_KEY_ENTER, HID_MOD_NONE);
}

static void test_fr_overrides(void) {
    /* AZERTY letter positions: A<->Q, W<->Z, M on the US ';' key. */
    check_map('a', LANG_FR_FR, 0x14, HID_MOD_NONE);
    check_map('A', LANG_FR_FR, 0x14, HID_MOD_LSHIFT);
    check_map('q', LANG_FR_FR, 0x04, HID_MOD_NONE);
    check_map('Q', LANG_FR_FR, 0x04, HID_MOD_LSHIFT);
    check_map('w', LANG_FR_FR, 0x1D, HID_MOD_NONE);
    check_map('W', LANG_FR_FR, 0x1D, HID_MOD_LSHIFT);
    check_map('z', LANG_FR_FR, 0x1A, HID_MOD_NONE);
    check_map('Z', LANG_FR_FR, 0x1A, HID_MOD_LSHIFT);
    check_map('m', LANG_FR_FR, 0x33, HID_MOD_NONE);
    check_map('M', LANG_FR_FR, 0x33, HID_MOD_LSHIFT);
    /* Fallback to en_US for the rest. */
    check_map('b', LANG_FR_FR, 0x05, HID_MOD_NONE);
    check_map('!', LANG_FR_FR, 0x1E, HID_MOD_LSHIFT);
}

static void test_tr_overrides(void) {
    /* Turkish-Q: ASCII letters/digits match en_US; '@' is AltGr+Q. */
    check_map('@', LANG_TR_TR, 0x14, HID_MOD_RALT);
    check_map('a', LANG_TR_TR, 0x04, HID_MOD_NONE);
    check_map('Z', LANG_TR_TR, 0x1D, HID_MOD_LSHIFT);
    check_map('5', LANG_TR_TR, 0x22, HID_MOD_NONE);
}

static void test_unknown_lang_falls_back(void) {
    check_map('a', LANG_UNKNOWN, 0x04, HID_MOD_NONE);
    check_map('@', LANG_UNKNOWN, 0x1F, HID_MOD_LSHIFT);
    check_map('y', (lang_t)77, 0x1C, HID_MOD_NONE);
}

int main(void) {
    test_lang_strings();
    test_en_us_letters_digits();
    test_en_us_punctuation();
    test_unmapped();
    test_de_overrides();
    test_fr_overrides();
    test_tr_overrides();
    test_unknown_lang_falls_back();
    MU_REPORT("hid_keymap");
    return mu_failed != 0;
}
