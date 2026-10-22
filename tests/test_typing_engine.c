/* Tests for src/common/typing_engine.c */

#include <stddef.h>
#include <stdint.h>

#include "minunit.h"
#include "typing_engine.h"

#define CAP_ACTIONS 256

typedef struct {
    hid_action_t actions[CAP_ACTIONS];
    uint32_t count;
} collector_t;

static void collect(const hid_action_t *action, void *user) {
    collector_t *c = (collector_t *)user;
    if (c->count < CAP_ACTIONS) {
        c->actions[c->count] = *action;
        c->count++;
    }
}

static void engine_en(typing_engine_t *eng) {
    typing_engine_config_t cfg;
    cfg.language = LANG_EN_US;
    cfg.typing_delay_ms = 8;
    typing_engine_init(eng, &cfg);
}

static void test_init_and_delay(void) {
    typing_engine_t eng;
    typing_engine_config_t cfg;
    cfg.language = LANG_EN_US;
    cfg.typing_delay_ms = 1;
    typing_engine_init(NULL, NULL); /* safe no-op */
    typing_engine_init(NULL, &cfg);
    engine_en(&eng);
    MU_EQ_ULL(8, typing_engine_delay_ms(&eng));
    MU_CHECK(typing_engine_delay_ms(NULL) == 0);

    cfg.language = LANG_DE_DE;
    cfg.typing_delay_ms = 0;
    typing_engine_init(&eng, &cfg);
    MU_EQ_ULL(0, typing_engine_delay_ms(&eng));
    MU_CHECK(eng.config.language == LANG_DE_DE);

    typing_engine_init(&eng, NULL); /* NULL cfg -> built-in defaults */
    MU_CHECK(eng.config.language == LANG_EN_US);
    MU_EQ_ULL(8, typing_engine_delay_ms(&eng));
}

/* Counters start at zero after init. */
static void test_counters_start_zero(void) {
    typing_engine_t eng;
    engine_en(&eng);
    MU_EQ_ULL(0, eng.chars_typed);
    MU_EQ_ULL(0, eng.chars_skipped);
}

static void test_type_hello(void) {
    typing_engine_t eng;
    collector_t col = { { { 0 } }, 0 };
    uint32_t n;
    engine_en(&eng);
    n = typing_engine_type_string(&eng, "Hi\n", collect, &col);
    MU_EQ_ULL(6, n); /* press+release per char */
    MU_EQ_ULL(6, col.count);
    /* 'H' = shift + 0x0B */
    MU_EQ_ULL(HID_MOD_LSHIFT, col.actions[0].modifier);
    MU_EQ_ULL(0x0B, col.actions[0].keycode);
    MU_CHECK(!col.actions[0].is_release);
    /* release = all keys up */
    MU_EQ_ULL(0, col.actions[1].modifier);
    MU_EQ_ULL(0, col.actions[1].keycode);
    MU_CHECK(col.actions[1].is_release);
    /* 'i' = 0x0C, no modifier */
    MU_EQ_ULL(0, col.actions[2].modifier);
    MU_EQ_ULL(0x0C, col.actions[2].keycode);
    MU_CHECK(!col.actions[2].is_release);
    MU_CHECK(col.actions[3].is_release);
    /* '\n' = ENTER */
    MU_EQ_ULL(0, col.actions[4].modifier);
    MU_EQ_ULL(HID_KEY_ENTER, col.actions[4].keycode);
    MU_CHECK(!col.actions[4].is_release);
    MU_CHECK(col.actions[5].is_release);
    MU_EQ_ULL(3, eng.chars_typed);
    MU_EQ_ULL(0, eng.chars_skipped);
}

static void test_skips_unknown(void) {
    typing_engine_t eng;
    collector_t col = { { { 0 } }, 0 };
    uint32_t n;
    engine_en(&eng);
    n = typing_engine_type_string(&eng, "a\x01" "b\x7f", collect, &col);
    MU_EQ_ULL(4, n);
    MU_EQ_ULL(4, col.count);
    MU_EQ_ULL(2, eng.chars_typed);
    MU_EQ_ULL(2, eng.chars_skipped);
    /* Counters accumulate across calls. */
    n = typing_engine_type_string(&eng, "c", collect, &col);
    MU_EQ_ULL(2, n);
    MU_EQ_ULL(3, eng.chars_typed);
    MU_EQ_ULL(2, eng.chars_skipped);
}

static void test_empty_and_null(void) {
    typing_engine_t eng;
    collector_t col = { { { 0 } }, 0 };
    engine_en(&eng);
    MU_EQ_ULL(0, typing_engine_type_string(&eng, "", collect, &col));
    MU_EQ_ULL(0, col.count);
    /* NULL callback still counts actions. */
    MU_EQ_ULL(2, typing_engine_type_string(&eng, "z", NULL, NULL));
    MU_EQ_ULL(0, col.count);
    MU_EQ_ULL(0, typing_engine_type_string(NULL, "z", collect, &col));
    MU_EQ_ULL(0, typing_engine_type_string(&eng, NULL, collect, &col));
    MU_EQ_ULL(0, typing_engine_type_string(NULL, NULL, NULL, NULL));
    MU_CHECK(eng.chars_typed == 1); /* only the NULL-cb "z" counted */
}

static void test_next_iterator(void) {
    typing_engine_t eng;
    typing_cursor_t cur;
    hid_action_t out;
    engine_en(&eng);
    typing_cursor_init(&cur);
    typing_cursor_init(NULL); /* safe no-op */

    /* "A\x01" -> press A, release, (skip \x01), end. */
    MU_CHECK(typing_engine_next(&eng, "A\x01", &cur, &out));
    MU_CHECK(!out.is_release);
    MU_EQ_ULL(0x04, out.keycode);
    MU_EQ_ULL(HID_MOD_LSHIFT, out.modifier);
    MU_CHECK(typing_engine_next(&eng, "A\x01", &cur, &out));
    MU_CHECK(out.is_release);
    MU_EQ_ULL(0, out.keycode);
    MU_CHECK(!typing_engine_next(&eng, "A\x01", &cur, &out));
    MU_EQ_ULL(1, eng.chars_typed);
    MU_EQ_ULL(1, eng.chars_skipped);

    /* NULL guards. */
    MU_CHECK(!typing_engine_next(NULL, "a", &cur, &out));
    MU_CHECK(!typing_engine_next(&eng, NULL, &cur, &out));
    MU_CHECK(!typing_engine_next(&eng, "a", NULL, &out));
    MU_CHECK(!typing_engine_next(&eng, "a", &cur, NULL));
    MU_CHECK(!typing_engine_next(NULL, NULL, NULL, NULL));

    /* Release pending is honoured even for a different/empty string. */
    typing_cursor_init(&cur);
    MU_CHECK(typing_engine_next(&eng, "b", &cur, &out));
    MU_CHECK(!out.is_release);
    MU_CHECK(typing_engine_next(&eng, "", &cur, &out));
    MU_CHECK(out.is_release);
}

static void test_language_routing(void) {
    typing_engine_t eng;
    typing_engine_config_t cfg;
    collector_t col = { { { 0 } }, 0 };
    cfg.language = LANG_DE_DE;
    cfg.typing_delay_ms = 5;
    typing_engine_init(&eng, &cfg);
    MU_EQ_ULL(2, typing_engine_type_string(&eng, "y", collect, &col));
    /* de_DE: 'y' is on the US 'z' position. */
    MU_EQ_ULL(0x1D, col.actions[0].keycode);
    MU_EQ_ULL(HID_MOD_NONE, col.actions[0].modifier);
}

int main(void) {
    test_init_and_delay();
    test_counters_start_zero();
    test_type_hello();
    test_skips_unknown();
    test_empty_and_null();
    test_next_iterator();
    test_language_routing();
    MU_REPORT("typing_engine");
    return mu_failed != 0;
}
