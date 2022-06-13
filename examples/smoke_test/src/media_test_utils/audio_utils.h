#pragma once

#include <stdint.h>
#include <list>
#include <string>

enum ANAConfigType {
  kFecController = 21,
  kFrameLengthController = 22,
  kChannelController = 23,
  kDtxController = 24,
  kBitrateController = 25,
  kFecControllerRplrBased = 26,
  kFrameLengthControllerV2 = 27,
  CONTROLLER_NOT_SET = 0,
};

struct ANAConfigItem {
  ANAConfigType type = CONTROLLER_NOT_SET;
  int32_t uplink_bandwidth_bps = 0;
  float uplink_packet_loss_fraction = 0.0f;
};


bool BuildANAConfigString(const std::list<ANAConfigItem> &configlist, std::string *out_str);
