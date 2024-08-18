
#include <memory>
#include <stdint.h>

#include <pulse/simple.h>

class PulseAudioRecorder {
public:
	PulseAudioRecorder(int16_t channels, int32_t sampleRateHz);
  ~PulseAudioRecorder();
  bool createPaSimple();

  int readData(void *data, size_t bytes);

  static std::unique_ptr<PulseAudioRecorder> create(int16_t channels,
                                                  int32_t sampleRateHz);

private:
  pa_simple *pa_simp;
  int16_t numberOfChannels = 0;
  int32_t sampleRateHz = 0;
};
