#ifndef KEYSTROKER_MINUNIT_H
#define KEYSTROKER_MINUNIT_H

/*
 * Minimal single-header test helper (no dependencies).
 * Counts checks; prints each failure with file:line. The Makefile builds
 * one binary per test_*.c; a binary exits 0 only when every check passes.
 */

#include <stdio.h>
#include <string.h>

static int mu_checks = 0;
static int mu_failed = 0;

#define MU_CHECK(cond) do {                       \
        ++mu_checks;                              \
        if (!(cond)) {                            \
            ++mu_failed;                          \
            printf("  FAIL %s:%d: %s\n",          \
                   __FILE__, __LINE__, #cond);    \
        }                                         \
    } while (0)

#define MU_EQ_LL(expected, actual) do {                                   \
        long long _e = (long long)(expected);                             \
        long long _a = (long long)(actual);                               \
        ++mu_checks;                                                      \
        if (_e != _a) {                                                   \
            ++mu_failed;                                                  \
            printf("  FAIL %s:%d: %s: expected %lld, got %lld\n",         \
                   __FILE__, __LINE__, #actual, _e, _a);                  \
        }                                                                 \
    } while (0)

#define MU_EQ_ULL(expected, actual) do {                                  \
        unsigned long long _e = (unsigned long long)(expected);           \
        unsigned long long _a = (unsigned long long)(actual);             \
        ++mu_checks;                                                      \
        if (_e != _a) {                                                   \
            ++mu_failed;                                                  \
            printf("  FAIL %s:%d: %s: expected %llu, got %llu\n",        \
                   __FILE__, __LINE__, #actual, _e, _a);                  \
        }                                                                 \
    } while (0)

#define MU_EQ_STR(expected, actual) do {                                  \
        const char *_e = (expected);                                      \
        const char *_a = (actual);                                        \
        int _same;                                                        \
        ++mu_checks;                                                      \
        if (_e == NULL || _a == NULL) {                                   \
            _same = (_e == _a);                                           \
        } else {                                                          \
            _same = (strcmp(_e, _a) == 0);                                \
        }                                                                 \
        if (!_same) {                                                     \
            ++mu_failed;                                                  \
            printf("  FAIL %s:%d: %s: expected \"%s\", got \"%s\"\n",    \
                   __FILE__, __LINE__, #actual,                           \
                   _e ? _e : "(null)", _a ? _a : "(null)");               \
        }                                                                 \
    } while (0)

#define MU_REPORT(suite) do {                                             \
        if (mu_failed == 0) {                                             \
            printf("PASS %s (%d checks)\n", (suite), mu_checks);          \
        } else {                                                          \
            printf("FAIL %s (%d checks, %d failures)\n",                  \
                   (suite), mu_checks, mu_failed);                        \
        }                                                                 \
    } while (0)

#endif /* KEYSTROKER_MINUNIT_H */
