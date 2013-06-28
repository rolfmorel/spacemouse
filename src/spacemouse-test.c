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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <libspacemouse.h>

int main()
{
  struct spacemouse *iter;

  int monitor_fd = spacemouse_monitor_open();

  if (spacemouse_device_list_update() == NULL)
    printf("No devices found.\n");

  spacemouse_device_list_foreach(iter, spacemouse_device_list()) {
    printf("device id: %d\n", spacemouse_device_get_id(iter));
    printf("  devnode: %s\n", spacemouse_device_get_devnode(iter));
    printf("  manufacturer: %s\n", spacemouse_device_get_manufacturer(iter));
    printf("  product: %s\n", spacemouse_device_get_product(iter));

    if (spacemouse_device_open(iter) == -1)
      fprintf(stderr, "Failed to open device: %s\n",
              spacemouse_device_get_devnode(iter));
    else
      spacemouse_device_set_led(iter, 1);
  }

  while(1) {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(monitor_fd, &fds);
    int mouse_fd, max_fd = monitor_fd;

    spacemouse_device_list_foreach(iter, spacemouse_device_list())
      if ((mouse_fd = spacemouse_device_get_fd(iter)) > -1) {
        FD_SET(mouse_fd, &fds);
        if (mouse_fd > max_fd) max_fd = mouse_fd;
      }

    if (select(max_fd + 1, &fds, NULL, NULL, NULL) == -1) {
      perror("select");
      break;
    }

    if (FD_ISSET(monitor_fd, &fds)) {
      int action;

      struct spacemouse *mon_mouse = spacemouse_monitor(&action);

      if (action == SPACEMOUSE_ACTION_ADD) {
        printf("Device added, ");

        if (spacemouse_device_open(mon_mouse) == -1)
          fprintf(stderr, "Failed to open device: %s\n",
                  spacemouse_device_get_devnode(iter));
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

    spacemouse_device_list_foreach(iter, spacemouse_device_list()) {
      mouse_fd = spacemouse_device_get_fd(iter);
      if (mouse_fd > -1 && FD_ISSET(mouse_fd, &fds)) {
        spacemouse_event mouse_event = { 0 };

        int read_event = spacemouse_device_read_event(iter, &mouse_event);
        if (read_event == -1)
          spacemouse_device_close(iter);
        else if (read_event == SPACEMOUSE_READ_SUCCESS) {
          printf("device id %d: ", spacemouse_device_get_id(iter));

          if (mouse_event.type == SPACEMOUSE_EVENT_MOTION) {
            printf("got motion event: t(%d, %d, %d) ",
                   mouse_event.motion.x, mouse_event.motion.y,
                   mouse_event.motion.z);
            printf("r(%d, %d, %d) period(%d)\n",
                   mouse_event.motion.rx, mouse_event.motion.ry,
                   mouse_event.motion.rz, mouse_event.motion.period);
          } else if (mouse_event.type == SPACEMOUSE_EVENT_BUTTON) {
            printf("got button %s event b(%d)\n",
                   mouse_event.button.press ? "press" : "release",
                   mouse_event.button.bnum);
          }
        }
      }
    }
  }

  return EXIT_FAILURE;
}
