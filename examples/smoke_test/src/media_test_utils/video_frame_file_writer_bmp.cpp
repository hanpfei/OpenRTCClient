#include "video_frame_file_writer_bmp.h"

#include "api/video/i420_buffer.h"
#include "byte_stream_utils.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "system_wrappers/include/clock.h"

#if defined(WEBRTC_WIN)
const char kSeparators[] = "\\";
#else   // FILE_PATH_USES_WIN_SEPARATORS
const char kSeparators[] = "/";
#endif  // FILE_PATH_USES_WIN_SEPARATORS

VideoFrameFileWriterBmp::VideoFrameFileWriterBmp()
    : VideoFrameFileWriterBmp(".") {}

VideoFrameFileWriterBmp::VideoFrameFileWriterBmp(const std::string& dirpath)
    : dirpath_(dirpath), width_(0), height_(0), video_frame_index_(0) {}

VideoFrameFileWriterBmp::~VideoFrameFileWriterBmp() = default;

void VideoFrameFileWriterBmp::OnFrame(const webrtc::VideoFrame& video_frame) {
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
      video_frame.video_frame_buffer()->ToI420());
  RTC_LOG(LS_INFO) << "Rotation " << video_frame.rotation();
  if (video_frame.rotation() != webrtc::kVideoRotation_0) {
    buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
  }
  buffer = webrtc::I420Buffer::Rotate(*buffer, webrtc::kVideoRotation_180);
  SetSize(buffer->width(), buffer->height());

  // TODO(bugs.webrtc.org/6857): This conversion is correct for little-endian
  // only. Cairo ARGB32 treats pixels as 32-bit values in *native* byte order,
  // with B in the least significant byte of the 32-bit value. Which on
  // little-endian means that memory layout is BGRA, with the B byte stored at
  // lowest address. Libyuv's ARGB format (surprisingly?) uses the same
  // little-endian format, with B in the first byte in memory, regardless of
  // native endianness.
  libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                     buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                     image_.get(), width_ * 4, buffer->width(),
                     buffer->height());

  char file_name[512] = {0};
  snprintf(file_name, sizeof(file_name), "%s%svideo_image_%d.bmp",
           dirpath_.c_str(), kSeparators, video_frame_index_);
  FILE* file = fopen(file_name, "wb+");
  if (!file) {
    RTC_LOG(LS_INFO) << "Create file failed: " << file_name;
    return;
  }

  WriteBMPHeader(file);

  fwrite(image_.get(), 1, width_ * height_ * 4, file);
  fclose(file);
  ++video_frame_index_;
}

void VideoFrameFileWriterBmp::SetSize(int width, int height) {
  if (width_ == width && height_ == height) {
    return;
  }

  width_ = width;
  height_ = height;
  image_.reset(new uint8_t[width * height * 4]);
}

void VideoFrameFileWriterBmp::WriteBMPHeader(FILE* file) {
#define SIZE_BITMAPFILEHEADER 14
#define SIZE_BITMAPINFOHEADER 40
#define HSIZE (SIZE_BITMAPFILEHEADER + SIZE_BITMAPINFOHEADER)
  int32_t n_bytes_image = width_ * height_ * 4;
  int32_t n_bytes = n_bytes_image + HSIZE;
  char buffer[SIZE_BITMAPFILEHEADER + SIZE_BITMAPINFOHEADER] = {0};

  buffer[0] = 'B';  // BITMAPFILEHEADER.bfType
  buffer[1] = 'M';  // do.
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 2,
           *reinterpret_cast<uint32_t*>(&n_bytes));  // BITMAPFILEHEADER.bfSize
  put_le16(reinterpret_cast<unsigned char*>(buffer) + 6,
           0);  // BITMAPFILEHEADER.bfReserved1
  put_le16(reinterpret_cast<unsigned char*>(buffer) + 8,
           0);  // BITMAPFILEHEADER.bfReserved2
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 10,
           HSIZE);  // BITMAPFILEHEADER.bfOffBits
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 14,
           SIZE_BITMAPINFOHEADER);  // BITMAPINFOHEADER.biSize
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 18,
           *reinterpret_cast<uint32_t*>(&width_));  // BITMAPINFOHEADER.biWidth
  put_le32(
      reinterpret_cast<unsigned char*>(buffer) + 22,
      *reinterpret_cast<uint32_t*>(&height_));  // BITMAPINFOHEADER.biHeight
  put_le16(reinterpret_cast<unsigned char*>(buffer) + 26,
           1);  // BITMAPINFOHEADER.biPlanes
  put_le16(reinterpret_cast<unsigned char*>(buffer) + 28,
           32);  // BITMAPINFOHEADER.biBitCount
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 30,
           0);  // BITMAPINFOHEADER.biCompression
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 34,
           *reinterpret_cast<uint32_t*>(
               &n_bytes_image));  // BITMAPINFOHEADER.biSizeImage
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 38,
           0);  // BITMAPINFOHEADER.biXPelsPerMeter
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 42,
           0);  // BITMAPINFOHEADER.biYPelsPerMeter
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 46,
           0);  // BITMAPINFOHEADER.biClrUsed
  put_le32(reinterpret_cast<unsigned char*>(buffer) + 50,
           0);  // BITMAPINFOHEADER.biClrImportant

  fwrite(buffer, 1, HSIZE, file);
}

FakeVideoSink::FakeVideoSink() : FakeVideoSink(5000, 5) {}

FakeVideoSink::FakeVideoSink(int64_t time) : FakeVideoSink(time, 5) {}

FakeVideoSink::FakeVideoSink(int64_t time, int32_t target_captured_frames)
    : write_file_(false),
      end_time_(webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds() + time),
      captured_video_frames_(0),
      target_captured_frames_(target_captured_frames),
      last_frame_incoming_time_(0) {}

FakeVideoSink::~FakeVideoSink() = default;

void FakeVideoSink::EnableWriteFile(bool enable) {
  std::unique_lock<std::mutex> l(lock_);
  if (write_file_ != enable) {
    write_file_ = enable;
    if (write_file_ && !file_writer_) {
      file_writer_ = std::make_unique<VideoFrameFileWriterBmp>();
    }
  }
}

void FakeVideoSink::OnFrame(const webrtc::VideoFrame& video_frame) {
  auto now = webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds();
  auto capture_frames = captured_video_frames_;
  ++captured_video_frames_;
  if (capture_frames < target_captured_frames_ &&
      last_frame_incoming_time_ < end_time_) {
    if (captured_video_frames_ >= target_captured_frames_) {
      capture_frames_.set_value(captured_video_frames_);
    } else if (now >= end_time_) {
      capture_frames_.set_value(captured_video_frames_);
    }

    std::unique_lock<std::mutex> l(lock_);
    if (write_file_ && file_writer_) {
      file_writer_->OnFrame(video_frame);
    }
  }
  last_frame_incoming_time_ = now;
}

int32_t FakeVideoSink::GetCapturedVideoFrames() {
  return captured_video_frames_;
}

std::future<int32_t> FakeVideoSink::GetCaptureFramesResult() {
  return capture_frames_.get_future();
}
