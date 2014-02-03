#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <poll.h>

#include <libspacemouse.h>

#include "options.h"
#include "util.h"

#include "commands.h"

int
event_command(char const *progname, options_t *options, int nargs, char **args)
{
  struct axis_event {
    unsigned int pos;
    unsigned int neg;
    char const * pos_str;
    char const * neg_str;
  };

  if (nargs)
    fail("%s: invalid non-command or non-option argument(s), use the "
         "'-h'/'--help' option to display the help message\n", progname);

  if (options->deviation == 0)
    options->deviation = MIN_DEVIATION;
  if (options->events == 0 && options->milliseconds == 0)
    options->events = N_EVENTS;

#ifndef AXIS_MAP_SPACENAVD
  struct axis_event axis_map[] = { { 0, 0, "right", "left" },
                                   { 0, 0, "back", "forward" },
                                   { 0, 0, "down", "up" },
                                   { 0, 0, "pitch back", "pitch forward" },
                                   { 0, 0, "roll left", "roll right" },
                                   { 0, 0, "yaw right", "yaw left" },
                                  };
#else
  struct axis_event axis_map[] = { { 0, 0, "right", "left" },
                                   { 0, 0, "up", "down" },
                                   { 0, 0, "forward", "back" },
                                   { 0, 0, "pitch back", "pitch forward" },
                                   { 0, 0, "yaw left", "yaw right" },
                                   { 0, 0, "roll right", "roll left" },
                                  };
#endif

  /* TODO: error check */
  int monitor_fd = spacemouse_monitor_open();

  {
    struct spacemouse *head, *iter;

    int err = spacemouse_device_list(&head, 1);

    if (err) {
      /* TODO: better message */
      fail("%s: spacemouse_device_list() returned error '%d'\n", progname,
           err);
    }

    spacemouse_device_list_foreach(iter, head) {
      int match = match_device(iter, &options->match);

      if (match == -1) {
        fail("%s: failed to use regex, please use valid ERE\n", progname);
      } else if (match) {
        if ((err = spacemouse_device_open(iter)) < 0)
          fail("%s: failed to open device '%s': %s\n", progname,
               spacemouse_device_get_devnode(iter), strerror(-err));

        if (options->grab && (err = spacemouse_device_set_grab(iter, 1)) < 0)
          fail("%s: failed to grab device '%s': %s\n", progname,
               spacemouse_device_get_devnode(iter), strerror(-err));
      }
    }
  }

  /* If piped to another program, that program will probably want to parse
   * the output by line.
   */
  setvbuf(stdout, NULL, _IOLBF, 0);

  while (true) {
    int err, mouse_fd, selected_fds, fds_idx = 0, nfds = 2;
    struct spacemouse *head, *iter;

    err = spacemouse_device_list(&head, 0);

    if (err) {
      /* TODO: better message */
      fail("%s: spacemouse_device_list() returned error '%d'\n", progname,
           err);
    }

    spacemouse_device_list_foreach(iter, head) {
      if (spacemouse_device_get_fd(iter) > -1)
        nfds++;
    }

    struct pollfd fds[nfds];
    fds[fds_idx++].fd = STDOUT_FILENO; /* no .events, just receive errors */

    fds[fds_idx].fd = monitor_fd;
    fds[fds_idx++].events = POLLIN;

    spacemouse_device_list_foreach(iter, head) {
      if ((mouse_fd = spacemouse_device_get_fd(iter)) > -1) {
        fds[fds_idx].fd = mouse_fd;
        fds[fds_idx++].events = POLLIN;
      }
    }

    selected_fds = poll(fds, nfds, -1);

    if (selected_fds < 1)
      perror("poll");

    for (fds_idx = 0; selected_fds && fds_idx < nfds; fds_idx++) {
      if (fds[fds_idx].revents == 0)
        continue;

      if (fds[fds_idx].fd == STDOUT_FILENO && fds[fds_idx].revents & POLLERR) {
        exit(EX_IOERR);
      } else if (fds[fds_idx].fd == monitor_fd) {
        struct spacemouse *mon_mouse;

        int action = spacemouse_monitor(&mon_mouse);

        if (action == SPACEMOUSE_ACTION_ADD) {
          int match = match_device(mon_mouse, &options->match);

          if (match) {
            if ((err = spacemouse_device_open(mon_mouse)) < 0)
              fail("%s: failed to open device '%s': %s\n", progname,
                   spacemouse_device_get_devnode(iter), strerror(-err));

            if (options->grab &&
                (err = spacemouse_device_set_grab(mon_mouse, 1)) < 0)
              fail("%s: failed to grab device '%s': %s\n", progname,
                   spacemouse_device_get_devnode(mon_mouse), strerror(-err));

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

            if (options->grab)
              spacemouse_device_set_grab(mon_mouse, 0);

            spacemouse_device_close(mon_mouse);
          }
        }

        selected_fds--;
      } else {
        spacemouse_device_list_foreach(iter, head) {
          mouse_fd = spacemouse_device_get_fd(iter);
          if (mouse_fd > -1 && fds[fds_idx].fd == mouse_fd) {
            spacemouse_event_t mouse_event = { 0 };
            int status = spacemouse_device_read_event(iter, &mouse_event);

            if (status == -1) {
              spacemouse_device_close(iter);
            } else if (status == SPACEMOUSE_READ_SUCCESS) {
              if (mouse_event.type == SPACEMOUSE_EVENT_MOTION) {
                int *axis_arr = &mouse_event.motion.x;

                for (int i = 0; i < 6; i++) {
                  unsigned int *axis_pol = NULL;
                  char const *axis_str = NULL;

                  if (axis_arr[i] > options->deviation) {
                    axis_pol = &axis_map[i].pos;
                    axis_str = axis_map[i].pos_str;
                  } else
                    axis_map[i].pos = 0;

                  if (axis_arr[i] < (-1 * options->deviation)) {
                    axis_pol = &axis_map[i].neg;
                    axis_str = axis_map[i].neg_str;
                  } else
                    axis_map[i].neg = 0;

                  if (axis_pol != NULL) {
                    if (options->milliseconds != 0) {
                      *axis_pol += mouse_event.motion.period;
                      if (*axis_pol > options->milliseconds) {
                        *axis_pol = *axis_pol % options->milliseconds;
                        printf("motion: %s\n", axis_str);
                      }
                    } else {
                      *axis_pol += 1;
                      if ((*axis_pol % options->events) == 0)
                        printf("motion: %s\n", axis_str);
                    }
                  }
                }
              } else if (mouse_event.type == SPACEMOUSE_EVENT_BUTTON) {
                printf("button: %d %s\n", mouse_event.button.bnum,
                       mouse_event.button.press ? "press" : "release");
              } else if (mouse_event.type == SPACEMOUSE_EVENT_LED) {
                printf("led: %s\n", mouse_event.led.state ? "on" : "off");
              }
            }

            break;
          }
        }
      }
    }
  }

  return EXIT_SUCCESS;
}
