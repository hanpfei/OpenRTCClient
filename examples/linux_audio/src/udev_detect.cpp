#include "udev_detect.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "string_utils.h"

int setup_inotify(struct userdata *u) {
  int r;

  if (u->inotify_fd >= 0)
    return 0;

  if ((u->inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK)) < 0) {
    printf("inotify_init1() failed: %s\n", strerror(errno));
    return -1;
  }

  r = inotify_add_watch(u->inotify_fd, "/dev/snd",
  IN_ATTRIB | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF);

  if (r < 0) {
    int saved_errno = errno;
    close(u->inotify_fd);
    u->inotify_fd = -1;

    if (saved_errno == ENOENT) {
      printf(
          "/dev/snd/ is apparently not existing yet, retrying to create inotify watch later.\n");
      return 0;
    }

    if (saved_errno == ENOSPC) {
      printf(
          "You apparently ran out of inotify watches, probably because Tracker/Beagle took them all away. "
              "I wished people would do their homework first and fix inotify before using it for watching whole "
              "directory trees which is something the current inotify is certainly not useful for. "
              "Please make sure to drop the Tracker/Beagle guys a line complaining about their broken use of inotify.");
      return 0;
    }

    printf("inotify_add_watch() failed: %s\n", strerror(saved_errno));
    return -1;
  }

  return 0;
}

const char* path_get_card_id(const char *path) {
  const char *e;

  if (!path)
    return NULL;

  if (!(e = strrchr(path, '/'))) {
    printf("path_get_card_id e %s\n", e);
    return NULL;
  }

  if (!(startswith(e, "/card") == 0)) {
    return NULL;
  }

  return e + 5;
}

static bool is_card_busy(const char *id) {
//  char *card_path = NULL, *pcm_path = NULL, *sub_status = NULL;
//  DIR *card_dir = NULL, *pcm_dir = NULL;
//  FILE *status_file = NULL;
//  struct dirent *de;
//  bool busy = false;
//
//  pa_assert(id);
//
//  /* This simply uses /proc/asound/card.../pcm.../sub.../status to
//   * check whether there is still a process using this audio device. */
//
//  card_path = pa_sprintf_malloc("/proc/asound/card%s", id);
//
//  if (!(card_dir = opendir(card_path))) {
//    pa_log_warn("Failed to open %s: %s", card_path, pa_cstrerror(errno));
//    goto fail;
//  }
//
//  for (;;) {
//    errno = 0;
//    de = readdir(card_dir);
//    if (!de && errno) {
//      pa_log_warn("readdir() failed: %s", pa_cstrerror(errno));
//      goto fail;
//    }
//
//    if (!de)
//      break;
//
//    if (!pa_startswith(de->d_name, "pcm"))
//      continue;
//
//    if (pcm_is_modem(id, de->d_name + 3))
//      continue;
//
//    pa_xfree(pcm_path);
//    pcm_path = pa_sprintf_malloc("%s/%s", card_path, de->d_name);
//
//    if (pcm_dir)
//      closedir(pcm_dir);
//
//    if (!(pcm_dir = opendir(pcm_path))) {
//      pa_log_warn("Failed to open %s: %s", pcm_path, pa_cstrerror(errno));
//      continue;
//    }
//
//    for (;;) {
//      char line[32];
//
//      errno = 0;
//      de = readdir(pcm_dir);
//      if (!de && errno) {
//        pa_log_warn("readdir() failed: %s", pa_cstrerror(errno));
//        goto fail;
//      }
//
//      if (!de)
//        break;
//
//      if (!pa_startswith(de->d_name, "sub"))
//        continue;
//
//      pa_xfree(sub_status);
//      sub_status = pa_sprintf_malloc("%s/%s/status", pcm_path, de->d_name);
//
//      if (status_file)
//        fclose(status_file);
//
//      if (!(status_file = pa_fopen_cloexec(sub_status, "r"))) {
//        pa_log_warn("Failed to open %s: %s", sub_status, pa_cstrerror(errno));
//        continue;
//      }
//
//      if (!(fgets(line, sizeof(line) - 1, status_file))) {
//        pa_log_warn("Failed to read from %s: %s", sub_status,
//            pa_cstrerror(errno));
//        continue;
//      }
//
//      if (!pa_streq(line, "closed\n")) {
//        busy = true;
//        break;
//      }
//    }
//  }
//
//  fail: pa_xfree(card_path);
//  pa_xfree(pcm_path);
//  pa_xfree(sub_status);
//
//  if (card_dir)
//    closedir(card_dir);
//
//  if (pcm_dir)
//    closedir(pcm_dir);
//
//  if (status_file)
//    fclose(status_file);
//
//  return busy;
}

static void verify_access(struct userdata *u, struct device *d) {
//  char *cd;
//  pa_card *card;
//  bool accessible;
//
//  pa_assert(u);
//  pa_assert(d);
//
//  if (d->ignore)
//    return;
//
//  cd = pa_sprintf_malloc("/dev/snd/controlC%s", path_get_card_id(d->path));
//  accessible = access(cd, R_OK | W_OK) >= 0;
//  pa_log_debug("%s is accessible: %s", cd, pa_yes_no(accessible));
//
//  pa_xfree(cd);
//
//  if (d->module == PA_INVALID_INDEX) {
//
//    /* If we are not loaded, try to load */
//
//    if (accessible) {
//      pa_module *m;
//      bool busy;
//
//      /* Check if any of the PCM devices that belong to this
//       * card are currently busy. If they are, don't try to load
//       * right now, to make sure the probing phase can
//       * successfully complete. When the current user of the
//       * device closes it we will get another notification via
//       * inotify and can then recheck. */
//
//      busy = is_card_busy(path_get_card_id(d->path));
//      pa_log_debug("%s is busy: %s", d->path, pa_yes_no(busy));
//
//      if (!busy) {
//
//        /* So, why do we rate limit here? It's certainly ugly,
//         * but there seems to be no other way. Problem is
//         * this: if we are unable to configure/probe an audio
//         * device after opening it we will close it again and
//         * the module initialization will fail. This will then
//         * cause an inotify event on the device node which
//         * will be forwarded to us. We then try to reopen the
//         * audio device again, practically entering a busy
//         * loop.
//         *
//         * A clean fix would be if we would be able to ignore
//         * our own inotify close events. However, inotify
//         * lacks such functionality. Also, during probing of
//         * the device we cannot really distinguish between
//         * other processes causing EBUSY or ourselves, which
//         * means we have no way to figure out if the probing
//         * during opening was canceled by a "try again"
//         * failure or a "fatal" failure. */
//
//        if (pa_ratelimit_test(&d->ratelimit, PA_LOG_DEBUG)) {
//          int err;
//
//          pa_log_debug("Loading module-alsa-card with arguments '%s'", d->args);
//          err = pa_module_load(&m, u->core, "module-alsa-card", d->args);
//
//          if (m) {
//            d->module = m->index;
//            pa_log_info("Card %s (%s) module loaded.", d->path, d->card_name);
//          } else if (err == -PA_ERR_NOENTITY) {
//            pa_log_info("Card %s (%s) module skipped.", d->path, d->card_name);
//            d->ignore = true;
//          } else {
//            pa_log_info("Card %s (%s) failed to load module.", d->path,
//                d->card_name);
//          }
//        } else
//          pa_log_warn(
//              "Tried to configure %s (%s) more often than %u times in %llus",
//              d->path, d->card_name, d->ratelimit.burst,
//              (long long unsigned) (d->ratelimit.interval / PA_USEC_PER_SEC));
//      }
//    }
//
//  } else {
//
//    /* If we are already loaded update suspend status with
//     * accessible boolean */
//
//    if ((card = pa_namereg_get(u->core, d->card_name, PA_NAMEREG_CARD))) {
//      pa_log_debug("%s all sinks and sources of card %s.",
//          accessible ? "Resuming" : "Suspending", d->card_name);
//      pa_card_suspend(card, !accessible, PA_SUSPEND_SESSION);
//    }
//  }
}

static void card_changed(struct userdata *u, struct udev_device *dev) {
  struct device *d;
  const char *path;
  const char *t;
  char *n;
//  pa_strbuf *args_buf;
//
//  pa_assert(u);
//  pa_assert(dev);

  /* Maybe /dev/snd is now available? */
  setup_inotify(u);

  path = udev_device_get_devpath(dev);

//  if ((d = pa_hashmap_get(u->devices, path))) {
//    verify_access(u, d);
//    return;
//  }

//  d = pa_xnew0(struct device, 1);
//  d->path = pa_xstrdup(path);
//  d->module = PA_INVALID_INDEX;
//  PA_INIT_RATELIMIT(d->ratelimit, 10 * PA_USEC_PER_SEC, 5);

  if (!(t = udev_device_get_property_value(dev, "PULSE_NAME"))) {
    if (!(t = udev_device_get_property_value(dev, "ID_ID"))) {
      if (!(t = udev_device_get_property_value(dev, "ID_PATH"))) {
        t = path_get_card_id(path);
        printf("path %s, t3 %s\n", path, t);
      } else {
        printf("path %s, t2 %s\n", path, t);
      }
    } else {
      printf("path %s, t1 %s\n", path, t);
    }
  } else {
    printf("path %s, t0 %s\n", path, t);
  }

  printf("path %s, t %s\n", path, t);

//  n = pa_namereg_make_valid_name(t);
//  d->card_name = pa_sprintf_malloc("alsa_card.%s", n);
//  args_buf = pa_strbuf_new();
//  pa_strbuf_printf(args_buf, "device_id=\"%s\" "
//      "name=\"%s\" "
//      "card_name=\"%s\" "
//      "namereg_fail=false "
//      "tsched=%s "
//      "fixed_latency_range=%s "
//      "ignore_dB=%s "
//      "deferred_volume=%s "
//      "use_ucm=%s "
//      "avoid_resampling=%s "
//      "card_properties=\"module-udev-detect.discovered=1\"",
//      path_get_card_id(path), n, d->card_name, pa_yes_no(u->use_tsched),
//      pa_yes_no(u->fixed_latency_range), pa_yes_no(u->ignore_dB),
//      pa_yes_no(u->deferred_volume), pa_yes_no(u->use_ucm),
//      pa_yes_no(u->avoid_resampling));
//  pa_xfree(n);
//
//  if (u->tsched_buffer_size_valid)
//  pa_strbuf_printf(args_buf, " tsched_buffer_size=%" PRIu32, u->tsched_buffer_size);
//
//  d->args = pa_strbuf_to_string_free(args_buf);
//
//  pa_hashmap_put(u->devices, d->path, d);

  verify_access(u, d);
}

static void remove_card(struct userdata *u, struct udev_device *dev) {
  struct device *d;

//  pa_assert(u);
//  pa_assert(dev);
//
//  if (!(d = pa_hashmap_remove(u->devices, udev_device_get_devpath(dev))))
//    return;
//
//  pa_log_info("Card %s removed.", d->path);
//
//  if (d->module != PA_INVALID_INDEX)
//    pa_module_unload_request_by_index(u->core, d->module, true);
//
//  device_free(d);
}

void process_device(struct userdata *u, struct udev_device *dev) {
  const char *action, *ff;

//  pa_assert(u);
//  pa_assert(dev);

  if (udev_device_get_property_value(dev, "PULSE_IGNORE")) {
    printf("Ignoring %s, because marked so.\n",
        udev_device_get_devpath(dev));
    return;
  }

  if ((ff = udev_device_get_property_value(dev, "SOUND_CLASS"))
      && streq(ff, "modem")) {
    printf("Ignoring %s, because it is a modem.\n",
        udev_device_get_devpath(dev));
    return;
  }

  action = udev_device_get_action(dev);
  printf("action %s, ff %s\n", action, ff);

  if (action && streq(action, "remove")) {
    remove_card(u, dev);
  } else if ((!action || streq(action, "change"))
      && udev_device_get_property_value(dev, "SOUND_INITIALIZED")) {
    card_changed(u, dev);
  }

  /* For an explanation why we don't look for 'add' events here
   * have a look into /lib/udev/rules.d/78-sound-card.rules! */
}

static void process_path(struct userdata *u, const char *path) {
  struct udev_device *dev;

  if (!path_get_card_id(path)) {
    return;
  }

  printf("path %s\n", path);
  if (!(dev = udev_device_new_from_syspath(u->udev, path))) {
    printf("Failed to get udev device object from udev.\n");
    return;
  }

  process_device(u, dev);
  udev_device_unref(dev);
}

int setup_udev(struct userdata *u) {
  int fd = -1;
  struct udev_enumerate *enumerate = NULL;
  struct udev_list_entry *item = NULL, *first = NULL;

  if (!(u->udev = udev_new())) {
    printf("Failed to initialize udev library.\n");
    goto fail;
  }
  if (!(u->monitor = udev_monitor_new_from_netlink(u->udev, "udev"))) {
    printf("Failed to initialize monitor.\n");
    goto fail;
  }

  if (udev_monitor_filter_add_match_subsystem_devtype(u->monitor, "sound", NULL)
      < 0) {
    printf("Failed to subscribe to sound devices.\n");
    goto fail;
  }

  errno = 0;
  if (udev_monitor_enable_receiving(u->monitor) < 0) {
    printf("Failed to enable monitor: %s\n", strerror(errno));
    if (errno == EPERM)
      printf("Most likely your kernel is simply too old and "
          "allows only privileged processes to listen to device events. "
          "Please upgrade your kernel to at least 2.6.30.\n");
    goto fail;
  }

  if ((fd = udev_monitor_get_fd(u->monitor)) < 0) {
    printf("Failed to get udev monitor fd.");
    goto fail;
  }

  if (!(enumerate = udev_enumerate_new(u->udev))) {
    printf("Failed to initialize udev enumerator.");
    goto fail;
  }

  if (udev_enumerate_add_match_subsystem(enumerate, "sound") < 0) {
    printf("Failed to match to subsystem.");
    goto fail;
  }

  if (udev_enumerate_scan_devices(enumerate) < 0) {
    printf("Failed to scan for devices.");
    goto fail;
  }

  first = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(item, first) {
    process_path(u, udev_list_entry_get_name(item));
  }

  udev_enumerate_unref(enumerate);

  return 0;

  fail:
  if (enumerate)
    udev_enumerate_unref(enumerate);
  return -1;
}
