#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <poll.h>
#include <errno.h>

#include <libspacemouse.h>

#include "options.h"
#include "util.h"

#include "commands.h"

static bool
match_device_init(struct spacemouse *mouse, match_t const *match_opts,
                  bool grab_opt, char const *progname)
{
  int err, match = match_device(mouse, match_opts);

  if (match == -1) {
    fail("%s: failed to use regex, please use valid ERE\n", progname);
  } else if (match) {
    int *axis_array = calloc(sizeof(int), 6);

    if (axis_array == NULL)
        fail("%s: failed to allocate memory: %s\n", progname, strerror(errno));
    spacemouse_device_set_data(mouse, axis_array);

    if ((err = spacemouse_device_open(mouse)) < 0)
      fail("%s: failed to open device '%s': %s\n", progname,
           spacemouse_device_get_devnode(mouse), strerror(-err));

    if (grab_opt && (err = spacemouse_device_set_grab(mouse, 1)) < 0)
      fail("%s: failed to grab device '%s': %s\n", progname,
           spacemouse_device_get_devnode(mouse), strerror(-err));
  }

  return match;
}

int
event_command(char const *progname, options_t *options, int nargs, char **args)
{
  struct {
    char const *pos, *neg;
  } axis_str[] = {
    { "right", "left" },
    { "back", "forward" },
    { "down", "up" },
    { "pitch back", "pitch forward" },
    { "roll left", "roll right" },
    { "yaw right", "yaw left" },
  };

  if (nargs)
    fail("%s: invalid non-command or non-option argument(s), use the "
         "'-h'/'--help' option to display the help message\n", progname);

  if (options->deviation == 0)
    options->deviation = MIN_DEVIATION;
  if (options->events == 0 && options->milliseconds == 0)
    options->events = N_EVENTS;

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

    spacemouse_device_list_foreach(iter, head)
      match_device_init(iter, &options->match, options->grab, progname);
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

        if (action == SPACEMOUSE_ACTION_ADD &&
            match_device_init(mon_mouse, &options->match, options->grab,
                              progname)) {
          printf("device: %s %s %s connect\n",
                 spacemouse_device_get_devnode(mon_mouse),
                 spacemouse_device_get_manufacturer(mon_mouse),
                 spacemouse_device_get_product(mon_mouse));
        } else if (action == SPACEMOUSE_ACTION_REMOVE &&
                   spacemouse_device_get_fd(mon_mouse) > -1) {
          int *axis_array = spacemouse_device_get_data(mon_mouse);

          printf("device: %s %s %s disconnect\n",
                 spacemouse_device_get_devnode(mon_mouse),
                 spacemouse_device_get_manufacturer(mon_mouse),
                 spacemouse_device_get_product(mon_mouse));

          if (axis_array != NULL)
            free(axis_array);

          if (options->grab)
            spacemouse_device_set_grab(mon_mouse, 0);

          spacemouse_device_close(mon_mouse);
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
                int *axis_array = &mouse_event.motion.x;
                int *axis_cond_array = spacemouse_device_get_data(iter);

                for (int idx = 0; idx < 6; idx++) {
                  if (axis_array[idx] > options->deviation &&
                      axis_cond_array[idx] >= 0) {
                    if (options->milliseconds != 0) {
                      axis_cond_array[idx] += mouse_event.motion.period;

                      if (axis_cond_array[idx] > options->milliseconds) {
                        axis_cond_array[idx] %= options->milliseconds;

                        printf("motion: %s\n", axis_str[idx].pos);
                      }
                    } else {
                      axis_cond_array[idx] += 1;

                      if (axis_cond_array[idx] % options->events == 0)
                        printf("motion: %s\n", axis_str[idx].pos);
                    }
                  } else if (axis_array[idx] < -1 * options->deviation &&
                             axis_cond_array[idx] <= 0) {
                    if (options->milliseconds != 0) {
                      axis_cond_array[idx] -= mouse_event.motion.period;

                      if (axis_cond_array[idx] < -1 * options->milliseconds) {
                        axis_cond_array[idx] %= options->milliseconds;
                        axis_cond_array[idx] *= -1;

                        printf("motion: %s\n", axis_str[idx].neg);
                      }
                    } else {
                      axis_cond_array[idx] -= 1;

                      if (axis_cond_array[idx] % options->events == 0)
                        printf("motion: %s\n", axis_str[idx].neg);
                    }
                  } else {
                    axis_cond_array[idx] = 0;
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
