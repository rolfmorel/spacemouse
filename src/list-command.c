#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libspacemouse.h>

#include "options.h"
#include "util.h"

#include "commands.h"

int
list_command(char const *progname, options_t *options, int nargs, char **args)
{
  if (nargs)
    fail("%s: invalid non-command or non-option argument(s), use the "
         "'-h'/'--help' option to display the help message\n", progname);

  {
    struct spacemouse *head, *iter;

    int err = spacemouse_device_list(&head, 1);

    if (err) {
      /* TODO: better message */
      fail("%s: spacemouse_device_list() returned error '%d'\n", progname,
           err);
    }

    spacemouse_device_list_foreach(iter, head) {
      int match = match_device(iter, options->dev_re, options->man_re,
                               options->pro_re, options->re_ignore_case);

      if (match == -1) {
        fail("%s: failed to use regex, please use valid ERE\n", progname);
      } else if (match) {
        printf("devnode: %s\n", spacemouse_device_get_devnode(iter));
        printf("manufacturer: %s\n", spacemouse_device_get_manufacturer(iter));
        printf("product: %s\n\n", spacemouse_device_get_product(iter));
      }
    }
  }

  return EXIT_SUCCESS;
}
