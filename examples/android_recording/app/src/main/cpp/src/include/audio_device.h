#pragma once

#include "audio_device_defines.h"

namespace awaudio {

enum AudioLayer {
  kPlatformDefaultAudio = 0,
  kAndroidJavaAudio,
  kAndroidOpenSLESAudio,
  kAndroidJavaInputAndOpenSLESOutputAudio,
  kAndroidAAudioAudio,
  kAndroidJavaInputAndAAudioOutputAudio,
};

enum {
  AUDIO_FORMAT_INVALID = -1,
  AUDIO_FORMAT_UNSPECIFIED = 0,

  /**
   * This format uses the int16_t data type.
   * The maximum range of the data is -32768 (0x8000) to 32767 (0x7FFF).
   */
  AUDIO_FORMAT_PCM_I16,

  /**
   * This format uses the float data type.
   * The nominal range of the data is [-1.0f, 1.0f).
   * Values outside that range may be clipped.
   *
   * See also the float Data in
   * <a href="/reference/android/media/AudioTrack#write(float[],%20int,%20int,%20int)">
   *   write(float[], int, int, int)</a>.
   */
  AUDIO_FORMAT_PCM_FLOAT,

  /**
   * This format uses 24-bit samples packed into 3 bytes.
   * The bytes are in little-endian order, so the least significant byte
   * comes first in the byte array.
   *
   * The maximum range of the data is -8388608 (0x800000)
   * to 8388607 (0x7FFFFF).
   *
   * Note that the lower precision bits may be ignored by the device.
   */
  AUDIO_FORMAT_PCM_I24_PACKED,

  /**
   * This format uses 32-bit samples stored in an int32_t data type.
   * The maximum range of the data is -2147483648 (0x80000000)
   * to 2147483647 (0x7FFFFFFF).
   *
   * Note that the lower precision bits may be ignored by the device.
   *
   * Available since API level 31.
   */
  AUDIO_FORMAT_PCM_I32
};

typedef int32_t audio_format_t;

class AudioRecorderDataCallback {
public:
  virtual int32_t DeliverRecordedData(const void *audio_buffer,
                                      const size_t samples_per_channel,
                                      const audio_format_t format,
                                      const size_t channels,
                                      const uint32_t samples_rate_hz,
                                      const uint32_t delay_mS,
                                      uint32_t &new_mic_level) = 0;

  virtual ~AudioRecorderDataCallback() {}
};

class AudioManager;

class AudioRecorderInterface {
public:
  static std::unique_ptr<AudioRecorderInterface> Create(AudioLayer audio_layer,
                                                        AudioManager *audio_manager);

  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(AudioLayer& audioLayer) const = 0;

  virtual int32_t Init() = 0;
  virtual int32_t Terminate() = 0;
  virtual bool Initialized() const = 0;

  virtual int32_t RecordingIsAvailable(bool& available) = 0;
  virtual int32_t InitRecording() = 0;
  virtual bool RecordingIsInitialized() const  = 0;

  virtual int32_t StartRecording() = 0;
  virtual int32_t StopRecording() = 0;
  virtual bool Recording() const = 0;
  virtual int32_t StereoRecordingIsAvailable(bool& available) = 0;
  virtual int32_t SetStereoRecording(bool enable) = 0;
  virtual int32_t StereoRecording(bool& enabled) const = 0;

  virtual void SetDataCallback(AudioRecorderDataCallback *data_callback) = 0;

  virtual bool BuiltInAECIsAvailable() const = 0;
  virtual int32_t EnableBuiltInAEC(bool enable) = 0;
  virtual bool BuiltInAGCIsAvailable() const = 0;
  virtual int32_t EnableBuiltInAGC(bool enable) = 0;
  virtual bool BuiltInNSIsAvailable() const = 0;
  virtual int32_t EnableBuiltInNS(bool enable) = 0;

  virtual ~AudioRecorderInterface() {}
};

class AudioPlayerDataCallback {
public:
  virtual int32_t RequestPlayoutData(const size_t samples_per_channel, const audio_format_t format,
                                     const size_t channels, const uint32_t samples_rate_hz) = 0;
  virtual int32_t GetPlayoutData(void *audio_buffer) = 0;

  virtual ~AudioPlayerDataCallback() {}
};

class AudioPlayerInterface {
public:
  static std::unique_ptr<AudioPlayerInterface> Create(AudioLayer audio_layer,
                                                      AudioManager *audio_manager);

  virtual int32_t ActiveAudioLayer(AudioLayer& audioLayer) const = 0;

  virtual int32_t Init() = 0;
  virtual int32_t Terminate() = 0;
  virtual bool Initialized() const = 0;

  virtual int32_t PlayoutIsAvailable(bool& available) = 0;
  virtual int32_t InitPlayout() = 0;
  virtual bool PlayoutIsInitialized() const = 0;

  virtual int32_t StartPlayout() = 0;
  virtual int32_t StopPlayout() = 0;
  virtual bool Playing() const = 0;

  virtual int SetSpeakerStreamType(int streamType) = 0;

  virtual int SpeakerVolumeIsAvailable(bool& available) = 0;
  virtual int SetSpeakerVolume(uint32_t volume) = 0;
  virtual int SpeakerVolume(uint32_t& volume) const = 0;
  virtual int MaxSpeakerVolume(uint32_t& max_volume) const = 0;
  virtual int MinSpeakerVolume(uint32_t& min_volume) const = 0;

  virtual int32_t StereoPlayoutIsAvailable(bool& available) = 0;
  virtual int32_t SetStereoPlayout(bool enable) = 0;
  virtual int32_t StereoPlayout(bool& enabled) const = 0;
  virtual int32_t PlayoutDelay(uint16_t& delay_ms) const = 0;

  virtual void SetDataCallback(AudioPlayerDataCallback *data_callback) = 0;

  virtual ~AudioPlayerInterface() {}
};

} // namespace awaudio