
#include <future>
#include <thread>

#include "api/scoped_refptr.h"
#include "api/test/create_frame_generator.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media_test_utils/test_base.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "pc/test/frame_generator_capturer_video_track_source.h"
#include "rtc_base/logging.h"

class VideoCodecTest : public testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}
};

static void get_codec_formats(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const std::vector<const char*> expect_codecs,
    std::map<const char*, webrtc::SdpVideoFormat>* expect_formats) {
  for (const auto& format : supported_formats) {
    auto format_str = format.ToString();
    RTC_LOG(LS_INFO) << "Format " << format_str;
    for (auto& codec_name : expect_codecs) {
      if (format.name == codec_name) {
        if (expect_formats &&
            expect_formats->find(codec_name) == expect_formats->end()) {
          expect_formats->insert({codec_name, format});
        }
      }
    }
  }
}

TEST_F(VideoCodecTest, decoder_supported_formats) {
  auto video_decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();
  auto supported_formats = video_decoder_factory->GetSupportedFormats();

  // clang-format off
  std::vector<const char*> expect_codec = {
      cricket::kVp8CodecName,
      cricket::kVp9CodecName,
      cricket::kH264CodecName
  };
  // clang-format on
  std::map<const char*, webrtc::SdpVideoFormat> expect_formats;
  get_codec_formats(supported_formats, expect_codec, &expect_formats);
  EXPECT_EQ(expect_formats.size(), expect_codec.size());
}

TEST_F(VideoCodecTest, decoder_instance_creation) {
  auto video_decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();
  auto supported_formats = video_decoder_factory->GetSupportedFormats();

  // clang-format off
  std::vector<const char*> expect_codec = {
      cricket::kVp8CodecName,
      cricket::kVp9CodecName,
      cricket::kH264CodecName
  };
  // clang-format on
  std::map<const char*, webrtc::SdpVideoFormat> expect_formats;
  get_codec_formats(supported_formats, expect_codec, &expect_formats);
  EXPECT_EQ(expect_formats.size(), expect_codec.size());

  for (auto iter = expect_formats.begin(); iter != expect_formats.end();
       ++iter) {
    RTC_LOG(LS_INFO) << "Selected " << iter->first
        << " decoder video format " << iter->second.ToString();
    auto video_decoder =
        video_decoder_factory->CreateVideoDecoder(iter->second);
    EXPECT_TRUE(video_decoder);
  }
}

TEST_F(VideoCodecTest, encoder_supported_formats) {
  auto video_encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
  auto supported_formats = video_encoder_factory->GetSupportedFormats();

  // clang-format off
  std::vector<const char*> expect_codec = {
      cricket::kVp8CodecName,
      cricket::kVp9CodecName,
      cricket::kH264CodecName
  };
  // clang-format on
  std::map<const char*, webrtc::SdpVideoFormat> expect_formats;
  get_codec_formats(supported_formats, expect_codec, &expect_formats);
  EXPECT_EQ(expect_formats.size(), expect_codec.size());
}

TEST_F(VideoCodecTest, codec_instance_creation) {
  auto video_encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
  auto supported_formats = video_encoder_factory->GetSupportedFormats();

  // clang-format off
  std::vector<const char*> expect_codec = {
      cricket::kVp8CodecName,
      cricket::kVp9CodecName,
      cricket::kH264CodecName
  };
  // clang-format on
  std::map<const char*, webrtc::SdpVideoFormat> expect_formats;
  get_codec_formats(supported_formats, expect_codec, &expect_formats);
  EXPECT_EQ(expect_formats.size(), expect_codec.size());

  for (auto iter = expect_formats.begin(); iter != expect_formats.end();
       ++iter) {
    RTC_LOG(LS_INFO) << "Selected " << iter->first
            << " encoder video format " << iter->second.ToString();
    auto video_encoder =
        video_encoder_factory->CreateVideoEncoder(iter->second);
    EXPECT_TRUE(video_encoder);
  }
}

class VideoStreamEnDecoder : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
                             public webrtc::EncodedImageCallback,
                             public webrtc::DecodedImageCallback {
 public:
  VideoStreamEnDecoder(
      std::unique_ptr<webrtc::VideoEncoder>&& video_encoder,
      int32_t expected_video_frames,
      std::unique_ptr<webrtc::VideoDecoder>&& video_decoder = nullptr,
      bool write_file = false)
      : video_encoder_(std::move(video_encoder)),
        expected_video_frames_(expected_video_frames),
        video_decoder_(std::move(video_decoder)),
        write_file_(write_file),
        file_stream_(nullptr),
        generated_encoded_frames_(0),
        generated_decoded_frames_(0) {
    video_encoder_->RegisterEncodeCompleteCallback(this);
    if (video_decoder_) {
      video_decoder_->RegisterDecodeCompleteCallback(this);
    }

    if (write_file_) {
      file_stream_ = fopen("test.h264", "wb");
    }
  }

  virtual ~VideoStreamEnDecoder() {
    video_encoder_->RegisterEncodeCompleteCallback(nullptr);
    video_encoder_->Release();

    if (video_decoder_) {
      video_decoder_->RegisterDecodeCompleteCallback(nullptr);
      video_decoder_->Release();
    }

    if (write_file_) {
      fclose(file_stream_);
      file_stream_ = nullptr;
    }
  }

  void OnFrame(const webrtc::VideoFrame& video_frame) override {
    std::vector<webrtc::VideoFrameType> frame_types;
    frame_types.push_back(webrtc::VideoFrameType::kVideoFrameKey);
    video_encoder_->Encode(video_frame, &frame_types);
  }

  Result OnEncodedImage(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info) override {
    ++generated_encoded_frames_;
    if (write_file_) {
      fwrite(encoded_image.data(), encoded_image.size(), 1, file_stream_);
    }

    if (video_decoder_) {
      video_decoder_->Decode(encoded_image, false, 0);
    } else {
      if (generated_encoded_frames_ == expected_video_frames_) {
        frames_.set_value(generated_encoded_frames_);
      }
    }
    return Result(Result::Error::OK);
  }

  int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
    ++generated_decoded_frames_;
    EXPECT_EQ(640, decodedImage.width());
    EXPECT_EQ(480, decodedImage.height());

    if (generated_decoded_frames_ == expected_video_frames_) {
      frames_.set_value(generated_decoded_frames_);
    }

    return 0;
  }

  void OnDroppedFrame(DropReason reason) override {}

  std::future<int32_t> GetResult() { return frames_.get_future(); }

  int32_t GetEncodedFrames() { return generated_encoded_frames_; }

  int32_t GetDecodedFrames() { return generated_decoded_frames_; }

 private:
  std::unique_ptr<webrtc::VideoEncoder> video_encoder_;
  const int32_t expected_video_frames_;
  std::unique_ptr<webrtc::VideoDecoder> video_decoder_;
  bool write_file_;
  FILE* file_stream_;
  int32_t generated_encoded_frames_;
  int32_t generated_decoded_frames_;
  std::promise<int32_t> frames_;
};

static void SetupVideoCodec(webrtc::VideoCodec& video_codec,
                            const webrtc::SdpVideoFormat& video_format) {
  webrtc::VideoEncoderConfig encoder_config;
  encoder_config.codec_type = webrtc::kVideoCodecH264;
  encoder_config.video_format = video_format;

  webrtc::VideoStream video_stream;
  video_stream.width = 640;
  video_stream.height = 480;
  video_stream.max_framerate = 60;
  video_stream.min_bitrate_bps = 30;
  video_stream.target_bitrate_bps = 530;
  video_stream.max_bitrate_bps = 1700;
  video_stream.max_qp = 56;
  video_stream.active = true;
  std::vector<webrtc::VideoStream> streams;
  streams.push_back(video_stream);

  ASSERT_TRUE(webrtc::VideoCodecInitializer::SetupCodec(encoder_config, streams,
                                                        &video_codec));
  video_codec.startBitrate = 227;
  video_codec.maxBitrate = 1700;
  video_codec.minBitrate = 30;
  video_codec.maxFramerate = 60;
  video_codec.active = true;
  video_codec.qpMax = 56;
}

static rtc::scoped_refptr<webrtc::FrameGeneratorCapturerVideoTrackSource>
CreateVideoTrackSource() {
  auto* videoTrackSource =
      new rtc::RefCountedObject<webrtc::FrameGeneratorCapturerVideoTrackSource>(
          webrtc::FrameGeneratorCapturerVideoTrackSource::Config(),
          webrtc::Clock::GetRealTimeClock(), false);

  rtc::scoped_refptr<webrtc::FrameGeneratorCapturerVideoTrackSource>
      video_track_source(videoTrackSource);
  return video_track_source;
}

TEST_F(VideoCodecTest, h264_encode) {
  auto video_encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
  auto supported_formats = video_encoder_factory->GetSupportedFormats();

  std::vector<const char*> expect_codec = {cricket::kH264CodecName};
  std::map<const char*, webrtc::SdpVideoFormat> expect_formats;
  get_codec_formats(supported_formats, expect_codec, &expect_formats);
  EXPECT_EQ(expect_formats.size(), expect_codec.size());

  auto& h264_format = expect_formats.begin()->second;
  RTC_LOG(LS_INFO) << "Selected H264 encoder video format " << h264_format.ToString();
  auto h264_encoder = video_encoder_factory->CreateVideoEncoder(h264_format);
  EXPECT_TRUE(h264_encoder);

  webrtc::VideoCodec video_codec;
  SetupVideoCodec(video_codec, h264_format);

  webrtc::VideoEncoder::Capabilities capabilities(false);
  webrtc::VideoEncoder::Settings settings(capabilities, 4, 1035);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            h264_encoder->InitEncode(&video_codec, settings));

  auto videoTrackSource = CreateVideoTrackSource();
  videoTrackSource->Start();

  auto stream_encoder =
      std::make_unique<VideoStreamEnDecoder>(std::move(h264_encoder), 40);
  videoTrackSource->AddOrUpdateSink(stream_encoder.get(),
                                    rtc::VideoSinkWants());

  auto future = stream_encoder->GetResult();
  auto status = future.wait_for(std::chrono::milliseconds(3000));
  EXPECT_NE(status, std::future_status::timeout);
  EXPECT_TRUE(future.get());

  videoTrackSource->Stop();
  videoTrackSource->RemoveSink(stream_encoder.get());

  auto encoded_frames = stream_encoder->GetEncodedFrames();
  RTC_LOG(LS_INFO) << "Encoded video frames " << encoded_frames;
  EXPECT_GT(encoded_frames, 30);
}

TEST_F(VideoCodecTest, h264_decode) {
  auto video_decoder_factory = webrtc::CreateBuiltinVideoDecoderFactory();
  auto supported_formats = video_decoder_factory->GetSupportedFormats();

  std::vector<const char*> expect_codec = {cricket::kH264CodecName};
  std::map<const char*, webrtc::SdpVideoFormat> expect_formats;
  get_codec_formats(supported_formats, expect_codec, &expect_formats);
  EXPECT_EQ(expect_formats.size(), expect_codec.size());

  auto& h264_format = expect_formats.begin()->second;

  RTC_LOG(LS_INFO) << "Selected H264 decoder video format " << h264_format.ToString();
  auto h264_decoder = video_decoder_factory->CreateVideoDecoder(h264_format);
  EXPECT_TRUE(h264_decoder);

  webrtc::VideoCodec video_codec;
  SetupVideoCodec(video_codec, h264_format);

  webrtc::VideoDecoder::Settings decoder_settings;
  decoder_settings.set_codec_type(webrtc::kVideoCodecH264);
  EXPECT_TRUE(h264_decoder->Configure(decoder_settings));

  auto video_encoder_factory = webrtc::CreateBuiltinVideoEncoderFactory();
  auto h264_encoder = video_encoder_factory->CreateVideoEncoder(h264_format);
  EXPECT_TRUE(h264_encoder);

  webrtc::VideoEncoder::Capabilities capabilities(false);
  webrtc::VideoEncoder::Settings settings(capabilities, 4, 1035);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            h264_encoder->InitEncode(&video_codec, settings));

  auto videoTrackSource = CreateVideoTrackSource();
  videoTrackSource->Start();

  auto stream_encoder = std::make_unique<VideoStreamEnDecoder>(
      std::move(h264_encoder), 40, std::move(h264_decoder));
  videoTrackSource->AddOrUpdateSink(stream_encoder.get(),
                                    rtc::VideoSinkWants());

  auto future = stream_encoder->GetResult();
  auto status = future.wait_for(std::chrono::milliseconds(3000));
  EXPECT_NE(status, std::future_status::timeout);
  EXPECT_TRUE(future.get());

  videoTrackSource->Stop();
  videoTrackSource->RemoveSink(stream_encoder.get());

  auto encoded_frames = stream_encoder->GetEncodedFrames();

  RTC_LOG(LS_INFO) << "Encoded video frames " << encoded_frames;
  EXPECT_GT(encoded_frames, 30);

  auto decoded_frames = stream_encoder->GetDecodedFrames();
  RTC_LOG(LS_INFO) << "Decoded video frames " << decoded_frames;
  EXPECT_GT(decoded_frames, 30);
}
