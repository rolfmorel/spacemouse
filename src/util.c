#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>

#include "util.h"

int run_regex(char const *regex, char const *string, bool case_sensitive)
{
  regex_t preg;
  int ret, cflags = REG_EXTENDED | REG_NOSUB | case_sensitive ? REG_ICASE : 0;

  if (regcomp(&preg, regex, cflags) != 0)
    return -1;

  ret = regexec(&preg, string, 0, NULL, 0);

  regfree(&preg);

  return ret;
}

int match_device(struct spacemouse *mouse, char const *dev_re,
                 char const *man_re, char const *pro_re, bool case_sensitive)
{
  int match = 0;
  char const *strs[] = { dev_re, man_re, pro_re };
  char const *members[] = { spacemouse_device_get_devnode(mouse),
                            spacemouse_device_get_manufacturer(mouse),
                            spacemouse_device_get_product(mouse) };

  for (size_t idx = 0; idx < 3; idx++) {
    if (strs[idx] != NULL) {
      int regex_success = run_regex(strs[idx], members[idx], case_sensitive);
      if (regex_success == -1) {
        return -1;
      } else if (regex_success == 1)
        match++;
    }
  }

  return !match;
}
