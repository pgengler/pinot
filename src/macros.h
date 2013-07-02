#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Suppress warnings for __attribute__((warn_unused_result)) */
#define IGNORE_CALL_RESULT(call) do { if (call) {} } while(0)

/* Macros for flags. */
#define FLAGOFF(flag) ((flag) / (sizeof(unsigned) * 8))
#define FLAGMASK(flag) (1 << ((flag) % (sizeof(unsigned) * 8)))
#define FLAGS(flag) flags[FLAGOFF(flag)]
#define SET(flag) FLAGS(flag) |= FLAGMASK(flag)
#define UNSET(flag) FLAGS(flag) &= ~FLAGMASK(flag)
#define ISSET(flag) ((FLAGS(flag) & FLAGMASK(flag)) != 0)
#define TOGGLE(flag) FLAGS(flag) ^= FLAGMASK(flag)

/* Macros for character allocation and more. */
#define charalloc(howmuch) (char *)nmalloc((howmuch) * sizeof(char))
#define charealloc(ptr, howmuch) (char *)nrealloc(ptr, (howmuch) * sizeof(char))
#define charmove(dest, src, n) memmove(dest, src, (n) * sizeof(char))
#define charset(dest, src, n) memset(dest, src, (n) * sizeof(char))

#ifdef ENABLE_NLS
/* Native language support. */
#define _(string) gettext(string)
#define P_(singular, plural, number) ngettext(singular, plural, number)
#else
#define _(string) (string)
#define P_(singular, plural, number) (number == 1 ? singular : plural)
#endif
#define gettext_noop(string) (string)
#define N_(string) gettext_noop(string)
/* Mark a string that will be sent to gettext() later. */

#ifdef DEBUG
#define DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif