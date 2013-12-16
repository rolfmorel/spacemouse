#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util.h"
#include "commands.h"
#include "options.h"

int main(int argc, char **argv)
{
  cmd_t command = NO_CMD;
  size_t cmd_matches = 0;

  if (argc >= 2) {
    size_t arg_len = strlen(argv[1]);

    cmd_t cmds[] = { LIST_CMD, LIST_CMD, LED_CMD, EVENT_CMD, RAW_CMD };
    char const *cmd_strs[] = { "list", "ls", "led", "event", "raw" };

    for (size_t cmd_idx = 0; cmd_idx < ARRLEN(cmds); cmd_idx++) {
      if (strncmp(argv[1], cmd_strs[cmd_idx], arg_len) == 0) {
        command = cmds[cmd_idx];

        cmd_matches++;
      } else {
        cmds[cmd_idx] = NO_CMD; /* remove cmd value from array */
      }
    }

    if (cmd_matches >= 2) {
      warn("%s: command '%s' is ambiguous; possibilities:", argv[0], argv[1]);

      for (size_t cmd_idx = 0; cmd_idx < ARRLEN(cmds); cmd_idx++) {
          if (cmds[cmd_idx] != NO_CMD)
            warn(" '%s'", cmd_strs[cmd_idx]);
      }
      warn("\n");

      return EXIT_FAILURE;
    }
  }

  {
    options_t options;
    char **remaining_args = NULL;
    size_t args_left;

    init_options(options);

    size_t args_consumed = parse_options(argc, argv, &options, command);

    args_left = argc - args_consumed;

    if (args_left)
      remaining_args = argv + args_consumed;

    switch (command) {
      case LED_CMD:
        return led_command(argv[0], &options, args_left, remaining_args);
        break;

      case EVENT_CMD:
        return event_command(argv[0], &options, args_left, remaining_args);
        break;

      case RAW_CMD:
        return raw_command(argv[0], &options, args_left, remaining_args);
        break;

      case LIST_CMD:
      default:
        return list_command(argv[0], &options, args_left, remaining_args);
        break;
    }
  }
}
