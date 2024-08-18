#pragma once

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace awaudio {

typedef std::numeric_limits<int16_t> limits_int16;

// The conversion functions use the following naming convention:
// S16:      int16_t [-32768, 32767]
// Float:    float   [-1.0, 1.0]
// FloatS16: float   [-32768.0, 32768.0]
// Dbfs: float [-20.0*log(10, 32768), 0] = [-90.3, 0]
// The ratio conversion functions use this naming convention:
// Ratio: float (0, +inf)
// Db: float (-inf, +inf)
static inline float S16ToFloat(int16_t v) {
  constexpr float kScaling = 1.f / 32768.f;
  return v * kScaling;
}

static inline int16_t FloatS16ToS16(float v) {
  v = std::min(v, 32767.f);
  v = std::max(v, -32768.f);
  return static_cast<int16_t>(v + std::copysign(0.5f, v));
}

static inline int16_t FloatToS16(float v) {
  v *= 32768.f;
  v = std::min(v, 32767.f);
  v = std::max(v, -32768.f);
  return static_cast<int16_t>(v + std::copysign(0.5f, v));
}

static inline float FloatToFloatS16(float v) {
  v = std::min(v, 1.f);
  v = std::max(v, -1.f);
  return v * 32768.f;
}

static inline float FloatS16ToFloat(float v) {
  v = std::min(v, 32768.f);
  v = std::max(v, -32768.f);
  constexpr float kScaling = 1.f / 32768.f;
  return v * kScaling;
}

void FloatToS16(const float* src, size_t size, int16_t* dest);
void S16ToFloat(const int16_t* src, size_t size, float* dest);
void S16ToFloatS16(const int16_t* src, size_t size, float* dest);
void FloatS16ToS16(const float* src, size_t size, int16_t* dest);
void FloatToFloatS16(const float* src, size_t size, float* dest);
void FloatS16ToFloat(const float* src, size_t size, float* dest);

}  // namespace awaudio