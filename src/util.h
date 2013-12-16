#ifndef _UTIL_H_
#define _UTIL_H_

#include <libspacemouse.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define ARRLEN(x) (sizeof x / sizeof *x)

// FreeBSD sysexits.h: An error occurred while doing I/O on some file.
#define EX_IOERR 74

/* fprintf to stderr */
void warn(char const *format, ...);

/* fprintf to stderr and exit failure */
void fail(char const *format, ...);

int match_device(struct spacemouse *mouse, char const *dev_re,
                 char const *man_re, char const *pro_re, bool case_sensitive);

#endif /* #ifndef _UTIL_H_ */
