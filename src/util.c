#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <regex.h>

#include "util.h"

void
warn(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vfprintf(stderr, format, ap);

    va_end(ap);
}

void
fail(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vfprintf(stderr, format, ap);

    va_end(ap);

    exit(EXIT_FAILURE);
}

static int
run_regex(char const *regex, char const *string, bool ignore_case)
{
  regex_t preg;
  int ret, cflags = REG_EXTENDED | REG_NOSUB | (ignore_case ? REG_ICASE : 0);

  if (regcomp(&preg, regex, cflags) != 0)
    return -1;

  ret = regexec(&preg, string, 0, NULL, 0);

  regfree(&preg);

  return ret;
}

int
match_device(struct spacemouse *mouse, match_t const *match_opts)
{
  int match = 0;
  char const *re_strs[] = { match_opts->device, match_opts->manufacturer,
                            match_opts->product };
  char const *members[] = { spacemouse_device_get_devnode(mouse),
                            spacemouse_device_get_manufacturer(mouse),
                            spacemouse_device_get_product(mouse) };

  for (size_t idx = 0; idx < 3; idx++) {
    if (re_strs[idx] != NULL) {
      int regex_success = run_regex(re_strs[idx], members[idx],
                                    match_opts->ignore_case);
      if (regex_success == -1) {
        return -1;
      } else if (regex_success == 1)
        match++;
    }
  }

  return !match;
}
