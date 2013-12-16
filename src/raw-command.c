#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

#include <libspacemouse.h>

#include "options.h"
#include "util.h"

#include "commands.h"

int
raw_command(char const *progname, options_t *options, int nargs, char **args)
{
  struct spacemouse *head, *iter;
  int monitor_fd, err;

  if (nargs)
    fail("%s: invalid non-command or non-option argument(s), use the "
         "'-h'/'--help' option to display the help message\n", progname);

  /* TODO: add error check */
  monitor_fd = spacemouse_monitor_open();

  if ((err = spacemouse_device_list(&head, 1)) != 0) {
    /* TODO: better message */
    fail("%s: spacemouse_device_list() returned error '%d'\n", progname, err);
  }

  if (head == NULL)
    printf("No devices connected.\n");

  spacemouse_device_list_foreach(iter, head) {
    int match = match_device(iter, options->dev_re, options->man_re,
                             options->pro_re, options->re_ignore_case);

    if (match == -1) {
      fail("%s: failed to use regex, please use valid ERE\n", progname);
    } else if (match) {
      printf("device id: %d\n"
             "  devnode: %s\n"
             "  manufacturer: %s\n"
             "  product: %s\n", spacemouse_device_get_id(iter),
             spacemouse_device_get_devnode(iter),
             spacemouse_device_get_manufacturer(iter),
             spacemouse_device_get_product(iter));

      if ((err = spacemouse_device_open(iter)) < 0)
        fail("%s: failed to open device '%s': %s\n", progname,
             spacemouse_device_get_devnode(iter), strerror(-err));
    }
  }

  while (true) {
    fd_set fds;
    int mouse_fd, max_fd = monitor_fd;

    FD_ZERO(&fds);
    FD_SET(monitor_fd, &fds);

    if ((err = spacemouse_device_list(&head, 0)) != 0) {
      /* TODO: better message */
      fail("%s: spacemouse_device_list() returned error '%d'\n", progname,
           err);
    }

    spacemouse_device_list_foreach(iter, head)
      if ((mouse_fd = spacemouse_device_get_fd(iter)) > -1) {
        FD_SET(mouse_fd, &fds);

        if (mouse_fd > max_fd)
          max_fd = mouse_fd;
      }

    if (select(max_fd + 1, &fds, NULL, NULL, NULL) == -1)
      fail("%s: select() error: %s", progname, strerror(errno));

    if (FD_ISSET(monitor_fd, &fds)) {
      struct spacemouse *mon_mouse;

      int action = spacemouse_monitor(&mon_mouse);

      int match = match_device(mon_mouse, options->dev_re, options->man_re,
                               options->pro_re, options->re_ignore_case);

      if (match) {
        if (action == SPACEMOUSE_ACTION_ADD) {
          printf("Device added, ");

          if ((err = spacemouse_device_open(mon_mouse)) < 0)
            fail("%s: failed to open device '%s': %s\n", progname,
                 spacemouse_device_get_devnode(mon_mouse), strerror(-err));
          else
            spacemouse_device_set_led(mon_mouse, 1);
        } else if (action == SPACEMOUSE_ACTION_REMOVE) {
          printf("Device removed, ");

          spacemouse_device_close(mon_mouse);
        }

        if (action == SPACEMOUSE_ACTION_ADD ||
            action == SPACEMOUSE_ACTION_REMOVE) {
          printf("device id: %d\n", spacemouse_device_get_id(mon_mouse));
          printf("  devnode: %s\n", spacemouse_device_get_devnode(mon_mouse));
          printf("  manufacturer: %s\n",
                 spacemouse_device_get_manufacturer(mon_mouse));
          printf("  product: %s\n", spacemouse_device_get_product(mon_mouse));
        }
      }
    }

    spacemouse_device_list_foreach(iter, head) {
      mouse_fd = spacemouse_device_get_fd(iter);

      if (mouse_fd > -1 && FD_ISSET(mouse_fd, &fds)) {
        spacemouse_event_t mouse_event = { 0 };
        int read_event = spacemouse_device_read_event(iter, &mouse_event);

        if (read_event < 0) {
          /* No need to handle error, monitor should handle removes */
          spacemouse_device_close(iter);
        } else if (read_event == SPACEMOUSE_READ_SUCCESS) {
          /* Safe guard for new events which we don't know how to handle. */
          if (mouse_event.type > -1 &&
              mouse_event.type <= SPACEMOUSE_EVENT_LED)
            printf("device id %d: ", spacemouse_device_get_id(iter));

          if (mouse_event.type == SPACEMOUSE_EVENT_MOTION) {
            printf("got motion event: t(%d, %d, %d) ",
                   mouse_event.motion.x, mouse_event.motion.y,
                   mouse_event.motion.z);
            printf("r(%d, %d, %d) period(%d)\n",
                   mouse_event.motion.rx, mouse_event.motion.ry,
                   mouse_event.motion.rz, mouse_event.motion.period);
          } else if (mouse_event.type == SPACEMOUSE_EVENT_BUTTON)
            printf("got button %s event: b(%d)\n",
                   mouse_event.button.press ? "press" : "release",
                   mouse_event.button.bnum);
          else if (mouse_event.type == SPACEMOUSE_EVENT_LED)
            printf("got led event: %s\n", mouse_event.led.state == 1 ?
                   "on" : "off");
        }

        break;
      }
    }
  }

  return EXIT_FAILURE;
}
