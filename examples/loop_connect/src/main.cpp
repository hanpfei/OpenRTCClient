
#include <iostream>
#include <memory>
#include <thread>

#include <glib.h>
#include <gtk/gtk.h>

#include "capture_video_track_source.h"
#include "linux/main_wnd.h"
#include "loop_call_session.h"

using namespace std;

int main(int argc, char *argv[]) {
  //  auto loop_call_session = std::make_shared<LoopCallSesstion>();
  //  loop_call_session->StartLoopCall();

  gtk_init(&argc, &argv);
// g_type_init API is deprecated (and does nothing) since glib 2.35.0, see:
// https://mail.gnome.org/archives/commits-list/2012-November/msg07809.html
#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif
// g_thread_init API is deprecated since glib 2.31.0, see release note:
// http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if !GLIB_CHECK_VERSION(2, 31, 0)
  g_thread_init(NULL);
#endif

  GtkMainWnd wnd("1234", 80, false, false);
  wnd.Create();

  while (true) {
    while (gtk_events_pending()) {
      gtk_main_iteration();
    }
    if(!wnd.IsWindow()) {
      break;
    }
  }

  wnd.Destroy();

  return 0;
}
