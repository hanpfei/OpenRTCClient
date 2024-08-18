#pragma once

namespace awaudio {

const int kDefaultSampleRate = 44100;
// Delay estimates for the two different supported modes. These values are based
// on real-time round-trip delay estimates on a large set of devices and they
// are lower bounds since the filter length is 128 ms, so the AEC works for
// delays in the range [50, ~170] ms and [150, ~270] ms. Note that, in most
// cases, the lowest delay estimate will not be utilized since devices that
// support low-latency output audio often supports HW AEC as well.
const int kLowLatencyModeDelayEstimateInMilliseconds = 50;
const int kHighLatencyModeDelayEstimateInMilliseconds = 150;

} // namespace awaudio