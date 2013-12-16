#ifndef _COMMANDS_HDR_
#define _COMMANDS_HDR_

typedef enum {
  NO_CMD,
  LIST_CMD,
  LED_CMD,
  EVENT_CMD,
  RAW_CMD
} cmd_t;

#include "options.h"

int
list_command(char const *progname, options_t *options, int nargs, char **args);

int
led_command(char const *progname, options_t *options, int nargs, char **args);

int
event_command(char const *progname, options_t *options, int nargs,
              char **args);

int
raw_command(char const *progname, options_t *options, int nargs, char **args);

/* event command specific */

#define MIN_DEVIATION 256
#define N_EVENTS 16

#endif /* #ifndef _COMMANDS_HDR_ */
