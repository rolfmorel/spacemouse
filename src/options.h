#ifndef _OPTIONS_HDR_
#define _OPTIONS_HDR_

typedef struct
{
  bool re_ignore_case;

  char const *dev_re, *man_re, *pro_re;

  /* event command specific options */
  bool grab;

  int deviation;
  int events;
  int milliseconds;
} options_t;

#include "commands.h"

#define init_options(options) (options).re_ignore_case = false; \
                               (options).dev_re = NULL; \
                               (options).man_re = NULL; \
                               (options).pro_re = NULL; \
                               /* event command specific options */ \
                               (options).grab = false; \
                               (options).deviation = 0; \
                               (options).events = 0; \
                               (options).milliseconds = 0;

int parse_options(int argc, char **argv, options_t *options, cmd_t cmd);

#endif /* #ifndef _OPTIONS_HDR_ */