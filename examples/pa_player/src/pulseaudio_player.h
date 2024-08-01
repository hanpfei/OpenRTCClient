
#include <memory>
#include <stdint.h>

#include <pulse/simple.h>

class PulseAudioPlayer {
public:
  PulseAudioPlayer(int16_t channels, int32_t sampleRateHz);
  ~PulseAudioPlayer();
  bool createPaSimple();

  int writeData(const void *data, size_t bytes);

  static std::unique_ptr<PulseAudioPlayer> create(int16_t channels,
                                                  int32_t sampleRateHz);

private:
  pa_simple *pa_simp;
  int16_t numberOfChannels = 0;
  int32_t sampleRateHz = 0;
};
