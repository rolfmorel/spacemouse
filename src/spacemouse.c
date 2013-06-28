/*
Copyright (c) 2013 Rolf Morel

This program is part of spacemouse-utils.

spacemouse-utils - a collection of simple utilies for 3D/6DoF input devices.

spacemouse-utils is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

spacemouse-utils is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with spacemouse-utils.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <getopt.h>

#include <poll.h>

#include <libspacemouse.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define EXIT_ERROR (EXIT_FAILURE + 1)

#define MIN_DEVIATION 256
#define N_EVENTS 16

enum command_t {
  NO_CMD = 1,
  LIST_CMD = 1 << 1,
  LED_CMD = 1 << 2,
  EVENT_CMD = 1 << 3
} command = NO_CMD;

struct axis_event {
  unsigned int n_events;
  unsigned int millis;
  char const * const event_str;
};

bool multi_call = false;

int regex_mask = REG_EXTENDED | REG_NOSUB;
int grab_opt = 0, deviation_opt = MIN_DEVIATION, events_opt = 0;
int millis_opt = 0;
char const *dev_opt = NULL, *man_opt = NULL, *pro_opt = NULL;

char const **help_strs = NULL;

#define COMMON_LONG_OPTIONS \
    { "devnode", required_argument, NULL, 'd' }, \
    { "manufacturer", required_argument, NULL, 'm' }, \
    { "product", required_argument, NULL, 'p' }, \
    { "ignore-case", no_argument, NULL, 'i' }, \
    { "help", no_argument, NULL, 'h' },

char const *opt_str_no_cmd = "d:m:p:ih";
struct option const long_options_no_cmd[] = {
  COMMON_LONG_OPTIONS
  { 0, 0, 0, 0 }
};

char const help_common_opts[] = \
"Options:\n"
"  -d, --devnode=DEV          regular expression (ERE) which devices'\n"
"                             devnode string must match\n"
"  -m, --manufacturer=MAN     regular expression (ERE) which devices'\n"
"                             manufacturer string must match\n"
"  -p, --product=PRO          regular expression (ERE) which devices'\n"
"                             product string must match\n"
"  -i, --ignore-case          makes regular expression matching case\n"
"                             insensitive\n";

char const help_event_opts[] = \
"  -g, --grab                 grab matched/all devices\n"
"  -D, --deviation=DEVIATION  minimum deviation on an motion axis needed\n"
"                             to register as an event\n"
"                             default is: " STR(MIN_DEVIATION) "\n"
"  -n, --events=N             number of consecutive events for which\n"
"                             deviaton must exceed minimum deviation before\n"
"                             printing an event to stdout\n"
"                             default is: " STR(N_EVENTS) "\n"
"  -M, --millis=MILLISECONDS  millisecond period in which consecutive\n"
"                             events' deviaton must exceed minimum deviation\n"
"                             before printing an event to stdout\n";

char const help_common_opts_end[] = \
"  -h, --help                 display this help\n"
"\n";

int run_regex(char const *regex, char const *string, int comp_mask)
{
  regex_t preg;
  int ret;

  if (regcomp(&preg, regex, comp_mask) != 0)
    return -1;

  ret = regexec(&preg, string, 0, NULL, 0);

  regfree(&preg);

  return ret;
}

int match_device(struct spacemouse *mouse)
{
  int match = 0;
  char const *opts[] = { dev_opt, man_opt, pro_opt };
  char const *members[] = { spacemouse_device_get_devnode(mouse),
                            spacemouse_device_get_manufacturer(mouse),
                            spacemouse_device_get_product(mouse) };

  for (int n = 0; n < 3; n++) {
    if (opts[n] != NULL) {
      int regex_success = run_regex(opts[n], members[n], regex_mask);
      if (regex_success == -1) {
        return -1;
      } else if (regex_success == 1)
        match++;
    }
  }
  return !match;
}

int parse_arguments(int argc, char **argv, char const *opt_str,
                    struct option const *long_options)
{
  int c;
  char const *cur_help_str = *help_strs;

  while ((c = getopt_long(argc, argv, opt_str, long_options, NULL)) != -1) {
    int tmp;
    switch (c) {
      case 'd':
        dev_opt = optarg;
        break;

      case 'm':
        man_opt = optarg;
        break;

      case 'p':
        pro_opt = optarg;
        break;

      case 'i':
        regex_mask |= REG_ICASE;
        break;

      case 'g':
        grab_opt = 1;
        break;

      case 'D':
        if ((tmp = atoi(optarg)) < 1) {
          fprintf(stderr, "%s: option '--deviation' needs to be a valid "
                  "positive integer\n", *argv);
          return EXIT_FAILURE;
        } else
          deviation_opt = tmp;
        break;

      case 'n':
        if ((tmp = atoi(optarg)) < 1) {
          fprintf(stderr, "%s: option '--events' needs to be a valid "
                  "positive integer\n", *argv);
          return EXIT_FAILURE;
        } else
          events_opt = tmp;
        break;

      case 'M':
        if ((tmp = atoi(optarg)) < 1) {
          fprintf(stderr, "%s: option '--millis' needs to be a valid "
                  "positive integer, in milliseconds\n", *argv);
          return EXIT_FAILURE;
        } else
          millis_opt = tmp;
        break;

      case 'h':
        do {
          printf(cur_help_str);
        } while ((cur_help_str = *(++help_strs)));

        return -1;
        break;

      case '?':
        return EXIT_FAILURE;
        break;
    }
  }

  if (events_opt != 0 && millis_opt != 0) {
    fprintf(stderr, "%s: options '--events' and '--millis' are mutually "
            "exclusive\n", *argv);
    return EXIT_FAILURE;
  } else if (events_opt == 0)
    events_opt = N_EVENTS;

  return 0;
}

int run_list_command(int argc, char **argv)
{
  {
    char const help_list_cmd[] = \
"Usage: spacemouse-list [OPTIONS]\n"
"       spacemouse-list (-h | --help)\n"
"Print device information of connected 3D/6DoF input devices.\n"
"\n";

    if (multi_call)
      help_strs = (char const *[]){ help_list_cmd, help_common_opts,
                                    help_common_opts_end, NULL };

    int status = parse_arguments(argc, argv, opt_str_no_cmd,
                                 long_options_no_cmd);
    if (status != 0)
      return status == -1 ? 0 : status;
  }

  if (optind != argc) {
    fprintf(stderr, "%s: invalid non-command or non-option argument(s), see "
            "'-h' for help\n", *argv);
    return EXIT_FAILURE;
  }

  int match;

  struct spacemouse *iter;
  spacemouse_device_list_foreach(iter, spacemouse_device_list_update())
    if ((match = match_device(iter)) == -1) {
      fprintf(stderr, "%s: failed to use regex, please use valid ERE\n",
              *argv);
      return EXIT_FAILURE;
    } else if (match) {
      printf("devnode: %s\n", spacemouse_device_get_devnode(iter));
      printf("manufacturer: %s\n", spacemouse_device_get_manufacturer(iter));
      printf("product: %s\n\n", spacemouse_device_get_product(iter));
    }

  return EXIT_SUCCESS;
}

int run_led_command(int argc, char **argv)
{
  {
    char const help_led_cmd[] = \
"Usage: spacemouse-led [OPTIONS]\n"
"       spacemouse-led [OPTIONS] (on | 1) | (off | 0)\n"
"       spacemouse-led [OPTIONS] switch\n"
"       spacemouse-led (-h | --help)\n"
"Print or manipulate the LED state of connected 3D/6DoF input devices.\n"
"\n";

    if (multi_call)
      help_strs = (char const *[]){ help_led_cmd, help_common_opts,
                                      help_common_opts_end, NULL };

    int status = parse_arguments(argc, argv, opt_str_no_cmd,
                                 long_options_no_cmd);
    if (status != 0)
      return status == -1 ? 0 : status;
  }

  int match, state_arg = -1;

  if (optind == (argc - 1)) {
    char *ptr;
    for (ptr = argv[optind]; *ptr != 0; ptr++)
      *ptr = tolower(*ptr);

    if (strcmp(argv[optind], "on") == 0 || strcmp(argv[optind], "1") == 0)
      state_arg = 1;
    else if (strcmp(argv[optind], "off") == 0 ||
             strcmp(argv[optind], "0") == 0)
      state_arg = 0;
    else if (strcmp(argv[optind], "switch") == 0)
      state_arg = 2;
    else {
      fprintf(stderr, "%s: invalid non-option argument -- '%s', see '-h' "
              "for help\n", *argv, argv[optind]);
      return EXIT_FAILURE;
    }
  } else if (optind != argc) {
    fprintf(stderr, "%s: expected zero or one non-option arguments, see "
                "'-h' for help\n", *argv);
    return EXIT_FAILURE;
  }

  struct spacemouse *iter;
  spacemouse_device_list_foreach(iter, spacemouse_device_list_update()) {
    if ((match = match_device(iter)) == -1) {
      fprintf(stderr, "%s: failed to use regex, please use valid ERE\n",
              *argv);
      return EXIT_FAILURE;
    } else if (match) {
      int led_state = -1;

      if (spacemouse_device_open(iter) == -1) {
        fprintf(stderr, "%s: failed to open device: %s\n", *argv,
                spacemouse_device_get_devnode(iter));
        return EXIT_FAILURE;
      }

      if (state_arg == -1 || state_arg == 2) {
        led_state = spacemouse_device_get_led(iter);
        if (led_state == -1) {
          fprintf(stderr, "%s: failed to get led state for: %s\n", *argv,
                  spacemouse_device_get_devnode(iter));
          return EXIT_FAILURE;
        }
      }

      if (state_arg == -1) {
        printf("%s: %s\n", spacemouse_device_get_devnode(iter),
               led_state ? "on": "off");
      } else {
        int state = state_arg;

        if (state_arg == 2)
          state = !led_state;
        if (spacemouse_device_set_led(iter, state) != 0) {
          fprintf(stderr, "%s: failed to set led state for: %s\n", *argv,
                  spacemouse_device_get_devnode(iter));
          return EXIT_FAILURE;
        }
        if (state_arg == 2) {
          printf("%s: switched %s\n", spacemouse_device_get_devnode(iter),
                 state ? "on": "off");
        }
      }

      spacemouse_device_close(iter);
    }
  }

  return EXIT_SUCCESS;
}

int run_event_command(int argc, char **argv)
{
  {
    char const *opt_str_event_cmd = "d:m:p:igD:n:M:h";
    struct option const long_options_event_cmd[] = {
      COMMON_LONG_OPTIONS
      { "grab", no_argument, NULL, 'g'},
      { "deviation", required_argument, NULL, 'D' },
      { "events", required_argument, NULL, 'n' },
      { "millis", required_argument, NULL, 'M' },
      { 0, 0, 0, 0 }
    };

    char const help_event_cmd[] = \
"Usage: spacemouse-event [OPTIONS]\n"
"       spacemouse-event [OPTIONS] (--events <N> | --millis <MILLISECONDS>)\n"
"       spacemouse-event (-h | --help)\n"
"Print events generated by connected 3D/6DoF input devices.\n"
"\n";

    if (multi_call)
      help_strs = (char const *[]){ help_event_cmd, help_common_opts,
                                    help_event_opts, help_common_opts_end,
                                    NULL };

    int status = parse_arguments(argc, argv, opt_str_event_cmd,
                                 long_options_event_cmd);
    if (status != 0)
      return status == -1 ? 0 : status;
  }

  if (optind != argc) {
    fprintf(stderr, "%s: invalid non-command or non-option argument(s), see "
            "'-h' for help\n", *argv);
    return EXIT_FAILURE;
  }

  /* strings based on use of AXIS_MAP_SPACENAVD macro during libspacemouse
   * compilation
   */
  struct axis_event axis_pos_map[] = { { 0, 0, "right" },
                                       { 0, 0, "up" },
                                       { 0, 0, "forward" },
                                       { 0, 0, "pitch back" },
                                       { 0, 0, "yaw left" },
                                       { 0, 0, "roll right" },
                                      };

  struct axis_event axis_neg_map[] = { { 0, 0, "left" },
                                       { 0, 0, "down" },
                                       { 0, 0, "back" },
                                       { 0, 0, "pitch forward" },
                                       { 0, 0, "yaw right" },
                                       { 0, 0, "roll left" },
                                      };

  int match, monitor_fd = spacemouse_monitor_open();

  struct spacemouse *iter;
  spacemouse_device_list_foreach(iter, spacemouse_device_list_update()) {
    if ((match = match_device(iter)) == -1) {
      fprintf(stderr, "%s: failed to use regex, please use valid ERE\n",
              *argv);
      return EXIT_FAILURE;
    } else if (match) {
      if (spacemouse_device_open(iter) == -1) {
        fprintf(stderr, "%s: failed to open device: %s\n", *argv,
                spacemouse_device_get_devnode(iter));
        return EXIT_FAILURE;
      }

      if (grab_opt && spacemouse_device_grab(iter) != 0) {
        fprintf(stderr, "%s: failed to grab device: %s\n", *argv,
                spacemouse_device_get_devnode(iter));
        return EXIT_FAILURE;
      }
    }
  }

  while(true) {
    int mouse_fd, fds_idx = 0;

    spacemouse_device_list_foreach(iter, spacemouse_device_list())
      if (spacemouse_device_get_fd(iter) > -1)
        fds_idx++;

    struct pollfd fds[fds_idx + 1];
    fds_idx = 0;

    fds[fds_idx].fd = STDOUT_FILENO;
    fds[fds_idx++].events = POLLERR;

    fds[fds_idx].fd = monitor_fd;
    fds[fds_idx++].events = POLLIN;

    spacemouse_device_list_foreach(iter, spacemouse_device_list())
      if ((mouse_fd = spacemouse_device_get_fd(iter)) > -1) {
        fds[fds_idx].fd = mouse_fd;
        fds[fds_idx++].events = POLLIN;
      }

    poll(fds, fds_idx, -1);

    iter = spacemouse_device_list();

    for (int n = 0; n < fds_idx; n++) {
      if (fds[n].revents == 0)
        continue;

      if (fds[n].fd == STDOUT_FILENO && fds[n].revents & POLLERR)
        return EXIT_ERROR;
      else if (fds[n].fd == monitor_fd) {
        int action;
        struct spacemouse *mon_mouse = spacemouse_monitor(&action);

        if (action == SPACEMOUSE_ACTION_ADD) {
          if (match_device(mon_mouse)) {
            if (spacemouse_device_open(mon_mouse) == -1) {
              fprintf(stderr, "%s: failed to open device: %s\n", *argv,
                      spacemouse_device_get_devnode(mon_mouse));
              return EXIT_FAILURE;
            }

            if (grab_opt && spacemouse_device_grab(iter) != 0) {
              fprintf(stderr, "%s: failed to grab device: %s\n", *argv,
                      spacemouse_device_get_devnode(iter));
              return EXIT_FAILURE;
            }

            printf("device: %s %s %s connect\n",
                   spacemouse_device_get_devnode(mon_mouse),
                   spacemouse_device_get_manufacturer(mon_mouse),
                   spacemouse_device_get_product(mon_mouse));
          }
        } else if (action == SPACEMOUSE_ACTION_REMOVE) {
          if (spacemouse_device_get_fd(mon_mouse) > -1) {
            printf("device: %s %s %s disconnect\n",
                   spacemouse_device_get_devnode(mon_mouse),
                   spacemouse_device_get_manufacturer(mon_mouse),
                   spacemouse_device_get_product(mon_mouse));

            if (grab_opt)
              spacemouse_device_ungrab(mon_mouse);

            spacemouse_device_close(mon_mouse);
          }
        }

      } else {
        spacemouse_device_list_foreach(iter, iter) {
          mouse_fd = spacemouse_device_get_fd(iter);
          if (mouse_fd > -1 && fds[n].fd == mouse_fd) {
            spacemouse_event mouse_event;
            int status;

            memset(&mouse_event, 0, sizeof mouse_event);

            status = spacemouse_device_read_event(iter, &mouse_event);
            if (status == -1)
              spacemouse_device_close(iter);
            else if (status == SPACEMOUSE_READ_SUCCESS) {
              if (mouse_event.type == SPACEMOUSE_EVENT_MOTION) {
                int *int_ptr = &mouse_event.motion.x;
                for (int i = 0; i < 6; i++) {
                  int print_event = 0;
                  struct axis_event *axis_map = 0, *axis_inverse_map;

                  if (int_ptr[i] > deviation_opt) {
                    axis_map = axis_pos_map;
                    axis_inverse_map = axis_neg_map;
                  } else if (int_ptr[i] < (-1 * deviation_opt)) {
                    axis_map = axis_neg_map;
                    axis_inverse_map = axis_pos_map;
                  } else {
                    axis_pos_map[i].n_events = 0;
                    axis_neg_map[i].n_events = 0;
                    axis_pos_map[i].millis = 0;
                    axis_neg_map[i].millis = 0;
                  }

                  if (axis_map != 0) {
                    if (millis_opt != 0) {
                      axis_map[i].millis += mouse_event.motion.period;
                      if (axis_map[i].millis > millis_opt) {
                        axis_map[i].millis = \
                            axis_map[i].millis % millis_opt;
                        print_event = 1;
                      }

                    } else {
                      axis_map[i].n_events += 1;
                      axis_inverse_map[i].n_events = 0;
                      if ((axis_map[i].n_events % events_opt) == 0)
                        print_event = 1;
                    }

                    if (print_event) {
                      printf("motion: %s\n", axis_map[i].event_str);
                    }
                  }
                }
              } else if (mouse_event.type == SPACEMOUSE_EVENT_BUTTON) {
                printf("button: %d %s\n", mouse_event.button.bnum,
                       mouse_event.button.press ? "press" : "release");
              }
            }
          break;
          }
        }
      }
    }
  }

  return 0;
}

int run_no_command(int argc, char **argv)
{
  char const help_no_cmd[] = \
"Usage: spacemouse [OPTIONS]\n"
"       spacemouse <COMMAND> [OPTIONS]\n"
"       spacemouse led [OPTIONS] (on | 1) | (off | 0)\n"
"       spacemouse led [OPTIONS] switch\n"
"       spacemouse event [OPTIONS] (--events <N> | --millis <MILLISECONDS>)\n"
"       spacemouse (-h | --help)\n"
"\n"
"Commands: (defaults to 'list' if no command is specified)\n"
"  list: Print device information of connected 3D/6DoF input devices\n"
"  led: Print or manipulate the LED state of connected 3D/6DoF input devices\n"
"  event: Print events generated by connected 3D/6DoF input devices\n"
"\n";

  help_strs = (char const *[]){ help_no_cmd, help_common_opts, help_event_opts,
                                help_common_opts_end, NULL };

  if (command == LED_CMD)
    return run_led_command(argc, argv);
  else if (command == EVENT_CMD)
    return run_event_command(argc, argv);

  return run_list_command(argc, argv);
}

int main(int argc, char **argv)
{
  /* If piped to another program, that program will probably want to parse
   * the output by line.
   */
  setvbuf(stdout, NULL, _IOLBF, 0);

  {
    int str_len = strlen(*argv);

    if (str_len >= 15 &&
        strcmp(*argv + (str_len - 15), "spacemouse-list") == 0) {
      multi_call = true;
      command = LIST_CMD;
    } else if (str_len >= 14 &&
               strcmp(*argv + (str_len - 14), "spacemouse-led") == 0) {
      multi_call = true;
      command = LED_CMD;
    } else if (str_len >= 16 &&
               strcmp(*argv + (str_len - 16), "spacemouse-event") == 0) {
      multi_call = true;
      command = EVENT_CMD;
    } else if (argc >= 2) {
      if (strcmp(argv[1], "list") == 0) {
        command = LIST_CMD;
        optind = 2;
      } else if (strcmp(argv[1], "led") == 0) {
        command = LED_CMD;
        optind = 2;
      } else if (strcmp(argv[1], "event") == 0) {
        command = EVENT_CMD;
        optind = 2;
      }
    }
  }

  if (multi_call)
    if (command == LED_CMD)
      return run_led_command(argc, argv);
    else if (command == EVENT_CMD)
      return run_event_command(argc, argv);
    else
      return run_list_command(argc, argv);
  else
    return run_no_command(argc, argv);
}
