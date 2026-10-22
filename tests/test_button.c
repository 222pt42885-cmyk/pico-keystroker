/* Tests for src/common/button.c */

#include <stdint.h>

#include "minunit.h"
#include "button.h"

static void test_null_safe(void) {
    button_init(NULL, 30, 400);
    MU_CHECK(button_update(NULL, true, 100) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(NULL, false, 100) == BUTTON_EVENT_NONE);
}

static void test_no_debounce_click(void) {
    button_t b;
    button_init(&b, 0, 400);
    /* Immediate press, no debounce ... */
    MU_CHECK(button_update(&b, true, 0) == BUTTON_EVENT_PRESS);
    /* ... holding reports nothing ... */
    MU_CHECK(button_update(&b, true, 1) == BUTTON_EVENT_NONE);
    /* ... release reports nothing ... */
    MU_CHECK(button_update(&b, false, 100) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 101) == BUTTON_EVENT_NONE);
    /* ... second click inside the ignore window is dropped ... */
    MU_CHECK(button_update(&b, true, 102) == BUTTON_EVENT_DROPPED);
    MU_CHECK(button_update(&b, true, 103) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 104) == BUTTON_EVENT_NONE);
    /* ... and a click after the window counts again. */
    MU_CHECK(button_update(&b, true, 500) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, 501) == BUTTON_EVENT_NONE);
}

static void test_debounce_filters_bounce(void) {
    button_t b;
    button_init(&b, 30, 400);
    /* Contact bounce: rapid toggling never holds 30 ms, no event. */
    MU_CHECK(button_update(&b, true, 0) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 10) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 15) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 20) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 25) == BUTTON_EVENT_NONE);
    /* Held from t=25: stable at t=55. */
    MU_CHECK(button_update(&b, true, 54) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 55) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, true, 56) == BUTTON_EVENT_NONE);
    /* Release bounces too; debounced falling edge is silent. */
    MU_CHECK(button_update(&b, false, 100) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 105) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 110) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 140) == BUTTON_EVENT_NONE);
    /* Second click at t=200 holds until t=230: inside ignore window. */
    MU_CHECK(button_update(&b, true, 200) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 230) == BUTTON_EVENT_DROPPED);
    MU_CHECK(button_update(&b, true, 231) == BUTTON_EVENT_NONE);
    /* Release, then click well after the window: counts. */
    MU_CHECK(button_update(&b, false, 300) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 340) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 500) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 530) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, 600) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, false, 640) == BUTTON_EVENT_NONE);
}

static void test_ignore_window_boundary(void) {
    button_t b;
    button_init(&b, 0, 400);
    MU_CHECK(button_update(&b, true, 1000) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, 1001) == BUTTON_EVENT_NONE);
    /* Exactly ignore_ms later: window has elapsed, counts as PRESS. */
    MU_CHECK(button_update(&b, true, 1400) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, 1401) == BUTTON_EVENT_NONE);
    /* One ms earlier would have been dropped. */
    MU_CHECK(button_update(&b, true, 1799) == BUTTON_EVENT_DROPPED);
    MU_CHECK(button_update(&b, false, 1800) == BUTTON_EVENT_NONE);
}

static void test_zero_ignore_window(void) {
    button_t b;
    button_init(&b, 0, 0);
    /* With no suppression window every rising edge is a press. */
    MU_CHECK(button_update(&b, true, 50) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, 50) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 50) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, 51) == BUTTON_EVENT_NONE);
}

static void test_wraparound(void) {
    button_t b;
    const uint32_t near_wrap = UINT32_C(4294967295) - 10u;
    button_init(&b, 0, 400);
    MU_CHECK(button_update(&b, true, near_wrap) == BUTTON_EVENT_PRESS);
    MU_CHECK(button_update(&b, false, near_wrap + 5u) == BUTTON_EVENT_NONE);
    /* 16 ms after the press (across the 32-bit wrap): dropped. */
    MU_CHECK(button_update(&b, true, 5u) == BUTTON_EVENT_DROPPED);
    MU_CHECK(button_update(&b, false, 100u) == BUTTON_EVENT_NONE);
    MU_CHECK(button_update(&b, true, 600u) == BUTTON_EVENT_PRESS);
}

int main(void) {
    test_null_safe();
    test_no_debounce_click();
    test_debounce_filters_bounce();
    test_ignore_window_boundary();
    test_zero_ignore_window();
    test_wraparound();
    MU_REPORT("button");
    return mu_failed != 0;
}
