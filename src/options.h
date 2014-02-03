#ifndef _OPTIONS_HDR_
#define _OPTIONS_HDR_

typedef struct match {
  bool ignore_case;

  char const *device, *manufacturer, *product;
} match_t;

typedef struct
{
  match_t match;

  /* event command specific options */
  bool grab;

  int deviation;
  int events;
  int milliseconds;
} options_t;

#include "commands.h"

#define init_options(options) (options).match = (match_t){ false, 0 }; \
                               /* event command specific options */ \
                               (options).grab = false; \
                               (options).deviation = 0; \
                               (options).events = 0; \
                               (options).milliseconds = 0;

int
parse_options(int argc, char **argv, options_t *options, cmd_t cmd);

#endif /* #ifndef _OPTIONS_HDR_ */
