#include "audio_encoded_frame_decoder.h"
#include "audio_encoded_frame_file_writer.h"
#include "audio_frame_writer.h"
#include "media_demuxer.h"
#include "video_encoded_frame_decoder.h"
#include "video_encoded_frame_writer.h"
#include "video_frame_writer.h"

#include <iostream>
using namespace std;

int main(int argc, const char* argv[]) {
  std::shared_ptr<MediaDemuxer> demuxer_;
  std::shared_ptr<AudioEncodedFrameDecoder> audio_decoder_;
  std::shared_ptr<AudioFrameWriter> audio_writer_;

  std::shared_ptr<VideoEncodedFrameDecoder> video_decoder_;
  std::shared_ptr<VideoFrameWriter> video_writer_;

  FILE *outfile_resampled;
  if (!demuxer_) {
    const char *path = "/home/hanpfei/trailer.mp4";
    demuxer_ = std::make_shared<MediaDemuxer>();
    demuxer_->open(path);

    audio_decoder_ = std::make_shared<AudioEncodedFrameDecoder>();
    audio_decoder_->initialize(demuxer_->getAudioCodecParameters());
    demuxer_->setAudioPacketProcessing(audio_decoder_);

    audio_writer_ = std::make_shared<AudioFrameWriter>();
    audio_writer_->initialize("/home/hanpfei/trailer_.pcm");
    audio_writer_->setDestChannelLayout(AV_CH_LAYOUT_MONO);
    audio_writer_->setDestSampleFormat(AV_SAMPLE_FMT_S16);
    audio_writer_->setDestSampleRate(44100);

    outfile_resampled = fopen("/home/hanpfei/trailer_441.pcm", "wbe");
    if (!outfile_resampled) {
      printf("Could not open out resampled file: %s", strerror(errno));
      return -1;
    }

    video_decoder_ = std::make_shared<VideoEncodedFrameDecoder>();
    video_decoder_->initialize(demuxer_->getVideoCodecParameters());
    demuxer_->setVideoPacketProcessing(video_decoder_);

    video_writer_ = std::make_shared<VideoFrameWriter>();
    video_writer_->initialize("/home/hanpfei/trailer.yuv");
    video_decoder_->setVideoFrameProcessing(video_writer_);

    audio_writer_->setCallback(
        [outfile_resampled](const int16_t *data, int sample_rate, int channels,
            int samples_per_channel) {
          if (outfile_resampled) {
            fwrite(data, 1, samples_per_channel * channels * sizeof(int16_t),
                outfile_resampled);
          }
          return 0;
        });

    audio_decoder_->setVideoFrameProcessing(audio_writer_);
  }
  while (demuxer_->demuxOneFrame() == 0) {

  }

  if (outfile_resampled) {
    fclose(outfile_resampled);
    outfile_resampled = nullptr;
  }

  cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!
  return 0;
}
