
#pragma once

#include <libudev.h>

struct userdata {
  int inotify_fd;
  struct udev* udev;
  struct udev_monitor *monitor;
};

int setup_inotify(struct userdata *u);

const char* path_get_card_id(const char *path);
void process_device(struct userdata *u, struct udev_device *dev);
int setup_udev(struct userdata *u);
