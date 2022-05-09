#include <ev.h>
#include <iostream>
#include <string.h>
#include <strings.h>

#include <sys/inotify.h>
#include <unistd.h>

#include "string_utils.h"
#include "udev_detect.h"

#define NAME_MAX         255  /* # chars in a file name */

static void inotify_cb(struct ev_loop *defaultloop, ev_io *w, int revents) {
  printf("inotify_cb\n");
  struct userdata *u = static_cast<struct userdata*>(w->data);
  bool deleted = false;

  size_t event_buf_len = sizeof(struct inotify_event) + NAME_MAX;
  struct inotify_event *event_buf =
      reinterpret_cast<struct inotify_event *>(malloc(event_buf_len));

  for (;;) {
    ssize_t r;
    bzero(event_buf, event_buf_len);
    if ((r = read(u->inotify_fd, event_buf, event_buf_len)) <= 0) {
      if (r < 0 && errno == EAGAIN) {
        break;
      }

      printf("read() from inotify failed: %s\n", r < 0 ? strerror(errno) : "EOF");
      goto fail;
    }

    struct inotify_event *event = event_buf;
    while (r > 0) {
      size_t len;

      if ((size_t) r < sizeof(struct inotify_event)) {
        printf("read() too short.\n");
        goto fail;
      }

      len = sizeof(struct inotify_event) + event->len;

      if ((size_t) r < len) {
        printf("Payload missing.\n");
        goto fail;
      }
      printf("File path: %s, r %zu, len %zu, mask %u\n",
          event->name, r, len, event->mask);

      /* From udev we get the guarantee that the control
       * device's ACL is changed last. To avoid races when ACLs
       * are changed we hence watch only the control device */
      if (((event->mask & IN_ATTRIB) && (startswith(event->name, "controlC") == 0))) {
        printf("controlC file path: %s, r %zu, len %zu\n", event->name, r, len);
      }

      /* ALSA doesn't really give us any guarantee on the closing
       * order, so let's simply hope */
      if (((event->mask & IN_CLOSE_WRITE) && (startswith(event->name, "pcmC") == 0))) {
        printf("pcmC file path: %s, r %zu, len %zu\n", event->name, r, len);
      }

      /* /dev/snd/ might have been removed */
      if ((event->mask & (IN_DELETE_SELF | IN_MOVE_SELF))) {
        printf("Delete\n");
        deleted = true;
      }

      event = (struct inotify_event*) ((uint8_t*) event + len);
      r -= len;
    }
  }

  free(event_buf);
  if (!deleted) {
    return;
  }

  fail:
  free(event_buf);
  if (u->inotify_fd >= 0) {
    close(u->inotify_fd);
    u->inotify_fd = -1;
  }
}

static void monitor_cb(struct ev_loop *defaultloop, ev_io *w, int revents) {
  printf("monitor_cb \n");
  struct userdata *u = static_cast<struct userdata*>(w->data);;
  struct udev_device *dev;

  if (!(dev = udev_monitor_receive_device(u->monitor))) {
    printf("Failed to get udev device object from monitor.\n");
    goto fail;
  }

  if (!path_get_card_id(udev_device_get_devpath(dev))) {
    udev_device_unref(dev);
    return;
  }

  process_device(u, dev);
  udev_device_unref(dev);
  return;

  fail:
  return;
//  a->io_free(u->udev_io);
//  u->udev_io = NULL;
}

void evServerLoop(struct userdata *userData) {
  // use the default event loop unless you have special needs
  struct ev_loop *loop = EV_DEFAULT;

  ev_io myWatcher;
  ev_io_init(&myWatcher, inotify_cb, userData->inotify_fd, EV_READ);
  myWatcher.data = userData;
  ev_io_start(loop, &myWatcher);

  int fd = -1;
  if ((fd = udev_monitor_get_fd(userData->monitor)) < 0) {
    printf("Failed to get udev monitor fd.");
  }
  ev_io udev_watcher;
  ev_io_init(&udev_watcher, monitor_cb, fd, EV_READ);
  udev_watcher.data = userData;
  ev_io_start(loop, &udev_watcher);

  printf("Start io event loop inotify_fd %d, udev_fd %d\n",
      userData->inotify_fd, fd);

  // now wait for events to arrive
  ev_run(loop, 0);
}

int main() {
  struct userdata userData;
  memset(&userData, 0, sizeof(userData));
  userData.inotify_fd = -1;

  setup_inotify(&userData);
  setup_udev(&userData);

  evServerLoop(&userData);

  return 0;
}

