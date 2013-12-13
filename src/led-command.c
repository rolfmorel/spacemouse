#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <libspacemouse.h>

#include "options.h"
#include "util.h"

#include "commands.h"

typedef enum {
  LED_NONE = 0, /* no command specified, print led state of devices */
  LED_OFF,
  LED_ON,
  LED_SWITCH
} action_t;

static action_t
parse_arguments(char const *progname, int nargs, char **args)
{
  action_t action = LED_NONE;

  if (nargs == 1) {
    size_t arg_len = strlen(args[0]), action_matches = 0;
    char arg[arg_len + 1];

    for (size_t char_idx = 0; char_idx < arg_len + 1; char_idx++)
     arg[char_idx] = tolower(args[0][char_idx]);

    action_t actions[] = { LED_ON, LED_ON, LED_OFF, LED_OFF, LED_SWITCH,
                           LED_SWITCH };
    char const *action_strs[] = { "on", "1", "off", "0", "switch", "!" };

    for (size_t action_idx = 0; action_idx < ARRLEN(actions); action_idx++) {
      if (strncmp(arg, action_strs[action_idx], arg_len) == 0) {
        action = actions[action_idx];

        action_matches++;
      } else {
        actions[action_idx] = LED_NONE; /* remove action value from array */
      }
    }

    if (action_matches >= 2) {
      fprintf(stderr, "%s: command '%s' is ambiguous; possibilities:",
              progname, args[0]);

      for (size_t action_idx = 0; action_idx < ARRLEN(actions); action_idx++) {
        if (actions[action_idx] != LED_NONE)
          printf(" '%s'", action_strs[action_idx]);
      }
      puts(""); /* newline */

      exit(EXIT_FAILURE);
    } else if (action == LED_NONE) {
      fprintf(stderr, "%s: command argument '%s' is invalid, use "
              "the '-h'/'--help' option to display the help message\n",
              progname, args[0]);

      exit(EXIT_FAILURE);
    }
  } else if (nargs) {
    fprintf(stderr, "%s: expected zero or one non-option arguments, use "
            "the '-h' option to display the help message\n", progname);

    exit(EXIT_FAILURE);
  }

  return action;
}

int led_command(char const *progname, options_t *options, int nargs,
                char **args)
{
  action_t action = parse_arguments(progname, nargs, args);

  struct spacemouse *head, *iter;
  int err = spacemouse_device_list(&head, 1);

  if (err) {
    /* TODO: better message */
    fprintf(stderr, "%s: spacemouse_device_list() returned error '%d'\n",
            progname, err);

    exit(EXIT_FAILURE);
  }

  spacemouse_device_list_foreach(iter, head) {
    int match = match_device(iter, options->dev_re, options->man_re,
                             options->pro_re, options->re_ignore_case);

    if (match == -1) {
      fprintf(stderr, "%s: failed to use regex, please use valid ERE\n",
              progname);

      exit(EXIT_FAILURE);
    } else if (match) {
      int led_state = -1;

      if ((err = spacemouse_device_open(iter)) < 0) {
        fprintf(stderr, "%s: failed to open device '%s': %s\n", progname,
                spacemouse_device_get_devnode(iter), strerror(-err));

        exit(EXIT_FAILURE);
      }

      if (action == LED_NONE || action == LED_SWITCH) {
        if ((led_state = spacemouse_device_get_led(iter)) < 0) {
          fprintf(stderr, "%s: failed to get led state for '%s': %s\n",
                  progname, spacemouse_device_get_devnode(iter),
                  strerror(-led_state));

          exit(EXIT_FAILURE);
        }
      }

      if (action == LED_NONE) {
        printf("%s: %s\n", spacemouse_device_get_devnode(iter),
               led_state ? "on": "off");
      } else {
        int new_state = -1;

        if (action == LED_ON)
          new_state = 1;
        else if (action == LED_OFF)
          new_state = 0;
        else if (action == LED_SWITCH)
          new_state = !led_state;

        if ((err = spacemouse_device_set_led(iter, new_state)) < 0) {
          fprintf(stderr, "%s: failed to set led state for '%s': %s\n",
                  progname, spacemouse_device_get_devnode(iter),
                  strerror(-err));

          exit(EXIT_FAILURE);
        }

        if (action == LED_SWITCH) {
          printf("%s: switched %s\n", spacemouse_device_get_devnode(iter),
                 new_state ? "on": "off");
        }
      }

      spacemouse_device_close(iter);
    }
  }

  return EXIT_SUCCESS;
}
