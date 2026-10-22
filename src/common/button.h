#ifndef KEYSTROKER_BUTTON_H
#define KEYSTROKER_BUTTON_H

/*
 * Debounced button state machine with double-click suppression.
 *
 * The trigger button is sampled as a raw boolean (via hal_button_read()
 * on target, or a fake clock/signal in host tests) together with a
 * millisecond timestamp. A debounced *rising* edge yields
 * BUTTON_EVENT_PRESS -- but only if no earlier press is still inside the
 * ignore window: a second rising edge within `ignore_ms` of the previous
 * press yields BUTTON_EVENT_DROPPED instead, so double clicks collapse to
 * a single trigger (the second click is ignored).
 *
 * Debounce is time-based, not sample-counted: the raw level must be
 * stable for `debounce_ms` before it is accepted. All timestamp math is
 * unsigned so a 32-bit millisecond wraparound stays correct.
 * Host-testable with a fake clock; no hardware includes.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BUTTON_EVENT_NONE = 0,   /* no (debounced) edge */
    BUTTON_EVENT_PRESS,      /* debounced rising edge, outside ignore window */
    BUTTON_EVENT_DROPPED     /* rising edge inside the ignore window */
} button_event_t;

typedef struct {
    uint32_t debounce_ms;      /* raw level must hold this long to count */
    uint32_t ignore_ms;        /* second-press suppression window */
    bool stable_pressed;       /* debounced level */
    bool last_raw;             /* last observed raw level */
    uint32_t last_raw_change_ms;
    uint32_t last_press_ms;
    bool has_press;            /* a press was accepted at least once */
} button_t;

void button_init(button_t *b, uint32_t debounce_ms, uint32_t ignore_ms);

/*
 * Feed one raw sample. Returns PRESS once per accepted click, DROPPED for
 * a bounce-free second click inside the ignore window, NONE otherwise.
 */
button_event_t button_update(button_t *b, bool raw_pressed, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* KEYSTROKER_BUTTON_H */
