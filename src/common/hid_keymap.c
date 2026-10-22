#include "hid_keymap.h"

#include <stddef.h>

/* Base en_US table: index = ASCII code, value = HID usage ID (0 = unmapped).
 * Letters/digits/punctuation follow the USB HID Usage Tables (§10) boot
 * protocol values; '\n' and '\r' both mean ENTER, '\t' means TAB. */
static const uint8_t s_key[128] = {
    ['\t'] = 0x2Bu, ['\n'] = 0x28u, ['\r'] = 0x28u,
    [' '] = 0x2Cu,
    ['!'] = 0x1Eu, ['"'] = 0x34u, ['#'] = 0x20u, ['$'] = 0x21u,
    ['%'] = 0x22u, ['&'] = 0x24u, ['\''] = 0x34u, ['('] = 0x26u,
    [')'] = 0x27u, ['*'] = 0x25u, ['+'] = 0x2Eu, [','] = 0x36u,
    ['-'] = 0x2Du, ['.'] = 0x37u, ['/'] = 0x38u,
    ['0'] = 0x27u, ['1'] = 0x1Eu, ['2'] = 0x1Fu, ['3'] = 0x20u,
    ['4'] = 0x21u, ['5'] = 0x22u, ['6'] = 0x23u, ['7'] = 0x24u,
    ['8'] = 0x25u, ['9'] = 0x26u,
    [':'] = 0x33u, [';'] = 0x33u, ['<'] = 0x36u, ['='] = 0x2Eu,
    ['>'] = 0x37u, ['?'] = 0x38u, ['@'] = 0x1Fu,
    ['A'] = 0x04u, ['B'] = 0x05u, ['C'] = 0x06u, ['D'] = 0x07u,
    ['E'] = 0x08u, ['F'] = 0x09u, ['G'] = 0x0Au, ['H'] = 0x0Bu,
    ['I'] = 0x0Cu, ['J'] = 0x0Du, ['K'] = 0x0Eu, ['L'] = 0x0Fu,
    ['M'] = 0x10u, ['N'] = 0x11u, ['O'] = 0x12u, ['P'] = 0x13u,
    ['Q'] = 0x14u, ['R'] = 0x15u, ['S'] = 0x16u, ['T'] = 0x17u,
    ['U'] = 0x18u, ['V'] = 0x19u, ['W'] = 0x1Au, ['X'] = 0x1Bu,
    ['Y'] = 0x1Cu, ['Z'] = 0x1Du,
    ['['] = 0x2Fu, ['\\'] = 0x31u, [']'] = 0x30u, ['^'] = 0x23u,
    ['_'] = 0x2Du, ['`'] = 0x35u,
    ['a'] = 0x04u, ['b'] = 0x05u, ['c'] = 0x06u, ['d'] = 0x07u,
    ['e'] = 0x08u, ['f'] = 0x09u, ['g'] = 0x0Au, ['h'] = 0x0Bu,
    ['i'] = 0x0Cu, ['j'] = 0x0Du, ['k'] = 0x0Eu, ['l'] = 0x0Fu,
    ['m'] = 0x10u, ['n'] = 0x11u, ['o'] = 0x12u, ['p'] = 0x13u,
    ['q'] = 0x14u, ['r'] = 0x15u, ['s'] = 0x16u, ['t'] = 0x17u,
    ['u'] = 0x18u, ['v'] = 0x19u, ['w'] = 0x1Au, ['x'] = 0x1Bu,
    ['y'] = 0x1Cu, ['z'] = 0x1Du,
    ['{'] = 0x2Fu, ['|'] = 0x31u, ['}'] = 0x30u, ['~'] = 0x35u,
};

/* Shift (or other) modifier required on top of s_key[], 0 = none. */
static const uint8_t s_mod[128] = {
    ['!'] = HID_MOD_LSHIFT, ['"'] = HID_MOD_LSHIFT,
    ['#'] = HID_MOD_LSHIFT, ['$'] = HID_MOD_LSHIFT,
    ['%'] = HID_MOD_LSHIFT, ['&'] = HID_MOD_LSHIFT,
    ['('] = HID_MOD_LSHIFT, [')'] = HID_MOD_LSHIFT,
    ['*'] = HID_MOD_LSHIFT, ['+'] = HID_MOD_LSHIFT,
    [':'] = HID_MOD_LSHIFT, ['<'] = HID_MOD_LSHIFT,
    ['>'] = HID_MOD_LSHIFT, ['?'] = HID_MOD_LSHIFT,
    ['@'] = HID_MOD_LSHIFT,
    ['A'] = HID_MOD_LSHIFT, ['B'] = HID_MOD_LSHIFT,
    ['C'] = HID_MOD_LSHIFT, ['D'] = HID_MOD_LSHIFT,
    ['E'] = HID_MOD_LSHIFT, ['F'] = HID_MOD_LSHIFT,
    ['G'] = HID_MOD_LSHIFT, ['H'] = HID_MOD_LSHIFT,
    ['I'] = HID_MOD_LSHIFT, ['J'] = HID_MOD_LSHIFT,
    ['K'] = HID_MOD_LSHIFT, ['L'] = HID_MOD_LSHIFT,
    ['M'] = HID_MOD_LSHIFT, ['N'] = HID_MOD_LSHIFT,
    ['O'] = HID_MOD_LSHIFT, ['P'] = HID_MOD_LSHIFT,
    ['Q'] = HID_MOD_LSHIFT, ['R'] = HID_MOD_LSHIFT,
    ['S'] = HID_MOD_LSHIFT, ['T'] = HID_MOD_LSHIFT,
    ['U'] = HID_MOD_LSHIFT, ['V'] = HID_MOD_LSHIFT,
    ['W'] = HID_MOD_LSHIFT, ['X'] = HID_MOD_LSHIFT,
    ['Y'] = HID_MOD_LSHIFT, ['Z'] = HID_MOD_LSHIFT,
    ['^'] = HID_MOD_LSHIFT, ['_'] = HID_MOD_LSHIFT,
    ['{'] = HID_MOD_LSHIFT, ['|'] = HID_MOD_LSHIFT,
    ['}'] = HID_MOD_LSHIFT, ['~'] = HID_MOD_LSHIFT,
};

typedef struct {
    char ascii;
    uint8_t key;
    uint8_t mod;
} key_override_t;

/* de_DE (QWERTZ): physical Y key yields Z and vice versa; '@' is AltGr+Q. */
static const key_override_t s_de_overrides[] = {
    { 'y', 0x1Du, HID_MOD_NONE   },
    { 'Y', 0x1Du, HID_MOD_LSHIFT },
    { 'z', 0x1Cu, HID_MOD_NONE   },
    { 'Z', 0x1Cu, HID_MOD_LSHIFT },
    { '@', 0x14u, HID_MOD_RALT   },
};

/*
 * fr_FR (AZERTY): A/Q and W/Z pairs are swapped vs US, and M sits on the
 * physical key that is ';' on US. Digits/punctuation otherwise fall back
 * to en_US (best effort: real AZERTY digits need Shift -- extend this
 * table if exact FR output is required).
 */
static const key_override_t s_fr_overrides[] = {
    { 'a', 0x14u, HID_MOD_NONE   },
    { 'A', 0x14u, HID_MOD_LSHIFT },
    { 'q', 0x04u, HID_MOD_NONE   },
    { 'Q', 0x04u, HID_MOD_LSHIFT },
    { 'w', 0x1Du, HID_MOD_NONE   },
    { 'W', 0x1Du, HID_MOD_LSHIFT },
    { 'z', 0x1Au, HID_MOD_NONE   },
    { 'Z', 0x1Au, HID_MOD_LSHIFT },
    { 'm', 0x33u, HID_MOD_NONE   },
    { 'M', 0x33u, HID_MOD_LSHIFT },
};

/* tr_TR (Turkish-Q): ASCII letters/digits match en_US; '@' is AltGr+Q. */
static const key_override_t s_tr_overrides[] = {
    { '@', 0x14u, HID_MOD_RALT },
};

static void apply_overrides(char c, const key_override_t *table, size_t n,
                            uint8_t *key, uint8_t *mod) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (table[i].ascii == c) {
            *key = table[i].key;
            *mod = table[i].mod;
            return;
        }
    }
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static bool tag_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (ascii_lower(*a) != ascii_lower(*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

lang_t lang_from_string(const char *s) {
    char norm[16];
    size_t i = 0;
    if (s == NULL) {
        return LANG_UNKNOWN;
    }
    /* Lowercase into a bounded buffer; overlong tags are rejected. */
    while (s[i] != '\0' && i < sizeof(norm) - 1u) {
        norm[i] = ascii_lower(s[i]);
        i++;
    }
    if (s[i] != '\0') {
        return LANG_UNKNOWN;
    }
    norm[i] = '\0';
    if (tag_eq(norm, "en_us") || tag_eq(norm, "en-us") || tag_eq(norm, "en")) {
        return LANG_EN_US;
    }
    if (tag_eq(norm, "tr_tr") || tag_eq(norm, "tr-tr") || tag_eq(norm, "tr")) {
        return LANG_TR_TR;
    }
    if (tag_eq(norm, "de_de") || tag_eq(norm, "de-de") || tag_eq(norm, "de")) {
        return LANG_DE_DE;
    }
    if (tag_eq(norm, "fr_fr") || tag_eq(norm, "fr-fr") || tag_eq(norm, "fr")) {
        return LANG_FR_FR;
    }
    return LANG_UNKNOWN;
}

const char *lang_to_string(lang_t lang) {
    switch (lang) {
    case LANG_EN_US:
        return "en_US";
    case LANG_TR_TR:
        return "tr_TR";
    case LANG_DE_DE:
        return "de_DE";
    case LANG_FR_FR:
        return "fr_FR";
    case LANG_UNKNOWN:
    default:
        return "unknown";
    }
}

bool hid_keycode_for_ascii(char c, lang_t lang,
                           uint8_t *keycode, uint8_t *modifier) {
    unsigned char u = (unsigned char)c;
    uint8_t key = HID_KEY_NONE;
    uint8_t mod = HID_MOD_NONE;
    if (keycode == NULL || modifier == NULL) {
        return false;
    }
    if (u < sizeof(s_key)) {
        key = s_key[u];
        mod = s_mod[u];
    }
    switch (lang) {
    case LANG_DE_DE:
        apply_overrides(c, s_de_overrides,
                        sizeof(s_de_overrides) / sizeof(s_de_overrides[0]),
                        &key, &mod);
        break;
    case LANG_FR_FR:
        apply_overrides(c, s_fr_overrides,
                        sizeof(s_fr_overrides) / sizeof(s_fr_overrides[0]),
                        &key, &mod);
        break;
    case LANG_TR_TR:
        apply_overrides(c, s_tr_overrides,
                        sizeof(s_tr_overrides) / sizeof(s_tr_overrides[0]),
                        &key, &mod);
        break;
    case LANG_EN_US:
    case LANG_UNKNOWN:
    default:
        break; /* en_US base table (also the fallback) */
    }
    if (key == HID_KEY_NONE) {
        return false;
    }
    *keycode = key;
    *modifier = mod;
    return true;
}
