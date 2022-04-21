
#include <iostream>
#include <memory>
#include <thread>

#include <glib.h>
#include <gtk/gtk.h>

#include "capture_video_track_source.h"
#include "linux/main_wnd.h"
#include "loop_call_session.h"

using namespace std;

class Conductor : public MainWndCallback {
public:
  Conductor(GtkMainWnd *main_window);
  virtual ~Conductor();

  void InitializeLoopConnectCall() override;
  void GetCaptureDeviceList() override;
  void StartLoopConnectCall(size_t capture_device_index) override;
  void StopLoopConnectCall() override;
  void UIThreadCallback(int msg_id, void *data) override;

private:
  GtkMainWnd *main_window_;
  std::shared_ptr<LoopCallSesstion> loop_call_session_;
};

Conductor::Conductor(GtkMainWnd *main_window) : main_window_(main_window) {
  main_window_->RegisterObserver(this);
}

Conductor::~Conductor() { main_window_->RegisterObserver(nullptr); }

void Conductor::InitializeLoopConnectCall() {
  if (!loop_call_session_) {
    loop_call_session_ = std::make_shared<LoopCallSesstion>();
  } else {
    main_window_->MessageBox("Message", "Loop call has not been initialized",
                             true);
  }
  printf("Start to initialize.\n");
}

void Conductor::GetCaptureDeviceList() {
  if (!loop_call_session_) {
    main_window_->MessageBox("Message", "Loop call has not been initialized",
                             true);
    return;
  }
  std::vector<std::string> devices;
  if (!CaptureVideoTrackSource::GetCaptureDeviceList(devices)) {
    main_window_->MessageBox("Message", "Get capture device list failed", true);
  } else {
    main_window_->ShowCaptureDeviceList(devices);
  }
  printf("To get capture device list.\n");
}

void Conductor::StartLoopConnectCall(size_t capture_device_index) {
  if (loop_call_session_) {
    loop_call_session_->StartLoopCall(capture_device_index);

    main_window_->SwitchToStreamingUI();

    auto remote_video_track = loop_call_session_->GetRemoteVideoTrack();
    main_window_->StartRemoteRenderer(remote_video_track.get());
    printf("Start loop call remote video track %p.\n",
           remote_video_track.get());
  } else {
    main_window_->MessageBox("Message", "Loop call has not been initialized",
                             true);
  }
}

void Conductor::StopLoopConnectCall() {
  if (loop_call_session_) {
    loop_call_session_->StopLoopCall();
  } else {
    main_window_->MessageBox("Message", "Loop call has not been initialized",
                             true);
  }
  printf("Stop loop call.\n");
}

void Conductor::UIThreadCallback(int msg_id, void *data) {}

int main(int argc, char *argv[]) {
  rtc::LogMessage::SetLogToStderr(false);

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

  GtkMainWnd wnd;
  wnd.Create();

  std::shared_ptr<Conductor> conductor = std::make_shared<Conductor>(&wnd);

  while (true) {
    while (gtk_events_pending()) {
      gtk_main_iteration();
    }
    if (!wnd.IsWindow()) {
      break;
    }
  }

  wnd.Destroy();

  return 0;
}
