#pragma once

#include <stdint.h>

#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

// Forward declarations.
typedef struct _GtkWidget GtkWidget;
typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;
typedef struct _cairo cairo_t;

class MainWndCallback {
public:
  virtual void InitializeLoopConnectCall() = 0;
  virtual void GetCaptureDeviceList() = 0;
  virtual void StartLoopConnectCall(size_t capture_device_index) = 0;
  virtual void StopLoopConnectCall() = 0;
  virtual void UIThreadCallback(int msg_id, void *data) = 0;

protected:
  virtual ~MainWndCallback() {}
};

// Implements the main UI of the peer connection client.
// This is functionally equivalent to the MainWnd class in the Windows
// implementation.
class GtkMainWnd {
public:
  GtkMainWnd();
  virtual ~GtkMainWnd();

  virtual void RegisterObserver(MainWndCallback *callback);
  virtual bool IsWindow();
  virtual void SwitchToControlUI();
  virtual void ShowCaptureDeviceList(const std::vector<std::string> &devices);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char *caption, const char *text, bool is_error);
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface *remote_video);
  virtual void StopRemoteRenderer();

  virtual void QueueUIThreadCallback(int msg_id, void *data);

  // Creates and shows the main window with the |Connect UI| enabled.
  bool Create();

  // Destroys the window.  When the window is destroyed, it ends the
  // main message loop.
  bool Destroy();

  // Callback for when the main window is destroyed.
  void OnDestroyed(GtkWidget *widget, GdkEvent *event);

  // Callback for when the user clicks the "Connect" button.
  void OnClicked(GtkWidget *widget);

  // Callback for keystrokes.  Used to capture Esc and Return.
  void OnKeyPress(GtkWidget *widget, GdkEventKey *key);

  // Callback when the user double clicks a peer in order to initiate a
  // connection.
  void OnRowActivated(GtkTreeView *tree_view, GtkTreePath *path,
                      GtkTreeViewColumn *column);

  void OnRedraw();

  void Draw(GtkWidget *widget, cairo_t *cr);

protected:
  class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
  public:
    VideoRenderer(GtkMainWnd *main_wnd,
                  webrtc::VideoTrackInterface *track_to_render);
    virtual ~VideoRenderer();

    // VideoSinkInterface implementation
    void OnFrame(const webrtc::VideoFrame &frame) override;

    const uint8_t *image() const { return image_.get(); }

    int width() const { return width_; }

    int height() const { return height_; }

  protected:
    void SetSize(int width, int height);
    std::unique_ptr<uint8_t[]> image_;
    int width_;
    int height_;
    GtkMainWnd *main_wnd_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
  };

protected:
  GtkWidget *window_;    // Our main window.
  GtkWidget *draw_area_; // The drawing surface for rendering video streams.
  GtkWidget *vbox_;      // Container for the Connect UI.
  GtkWidget *capture_device_list_; // The list of peers.
                                   //  MainWndCallback* callback_;

  GtkWidget *init_button_;
  GtkWidget *get_capture_device_button_;
  GtkWidget *stop_button_;

  MainWndCallback *callback_;

  std::unique_ptr<VideoRenderer> remote_renderer_;
  int width_;
  int height_;
  std::unique_ptr<uint8_t[]> draw_buffer_;
  int draw_buffer_size_;
};
