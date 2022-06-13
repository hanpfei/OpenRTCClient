

#include "audio_utils.h"

#if WEBRTC_ENABLE_PROTOBUF
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.h"
#else
#include "modules/audio_coding/audio_network_adaptor/config.pb.h"
#endif
#endif

bool BuildANAConfigString(const std::list<ANAConfigItem> &configlist,
    std::string *out_str) {
  if (configlist.empty() || !out_str) {
    return false;
  }
  webrtc::audio_network_adaptor::config::ControllerManager ana_config;
  for (auto iter = configlist.begin(); iter != configlist.end(); ++iter) {
    auto *controller = ana_config.add_controllers();
    switch (iter->type) {
    case ANAConfigType::kFecController: {
      auto fec_config = std::make_unique<
          webrtc::audio_network_adaptor::config::FecController>();

      auto fec_enabling_threshold = std::make_unique<
          webrtc::audio_network_adaptor::config::FecController_Threshold>();
      fec_enabling_threshold->set_low_bandwidth_bps(1313);
      fec_enabling_threshold->set_high_bandwidth_bps(2313);
      fec_enabling_threshold->set_low_bandwidth_packet_loss(0.5);
      fec_enabling_threshold->set_high_bandwidth_packet_loss(0.5);
      fec_config->set_allocated_fec_enabling_threshold(fec_enabling_threshold.release());

      auto fec_disabling_threshold = std::make_unique<
                webrtc::audio_network_adaptor::config::FecController_Threshold>();
      fec_disabling_threshold->set_low_bandwidth_bps(713);
      fec_disabling_threshold->set_high_bandwidth_bps(913);
      fec_disabling_threshold->set_low_bandwidth_packet_loss(0.3);
      fec_disabling_threshold->set_high_bandwidth_packet_loss(0.2);
      fec_config->set_allocated_fec_disabling_threshold(fec_disabling_threshold.release());

      fec_config->set_time_constant_ms(12345);

      controller->set_allocated_fec_controller(fec_config.release());
    }
      break;
    case ANAConfigType::kFecControllerRplrBased: {
      // FecControllerRplrBased has been removed and can't be used anymore.
      auto fec_rplr_based_config = std::make_unique<
          webrtc::audio_network_adaptor::config::FecControllerRplrBased>();
      controller->set_allocated_fec_controller_rplr_based(
          fec_rplr_based_config.release());
    }
      continue;
    case ANAConfigType::kFrameLengthController: {
      auto frame_length_config = std::make_unique<
          webrtc::audio_network_adaptor::config::FrameLengthController>();
      controller->set_allocated_frame_length_controller(
          frame_length_config.release());
    }
      break;
    case ANAConfigType::kChannelController: {
      auto channel_ctrl_config = std::make_unique<
          webrtc::audio_network_adaptor::config::ChannelController>();
      controller->set_allocated_channel_controller(
          channel_ctrl_config.release());
    }
      break;
    case ANAConfigType::kDtxController: {
      auto dtx_config = std::make_unique<
          webrtc::audio_network_adaptor::config::DtxController>();
      controller->set_allocated_dtx_controller(dtx_config.release());
    }
      break;
    case ANAConfigType::kBitrateController: {
      auto bitrate_config = std::make_unique<
          webrtc::audio_network_adaptor::config::BitrateController>();
      controller->set_allocated_bitrate_controller(bitrate_config.release());
    }
      break;
    case ANAConfigType::kFrameLengthControllerV2: {
      auto frame_length_v2_config = std::make_unique<
          webrtc::audio_network_adaptor::config::FrameLengthControllerV2>();
      controller->set_allocated_frame_length_controller_v2(
          frame_length_v2_config.release());
    }
      break;
    default:
      break;
    }

    if (iter->uplink_bandwidth_bps > 0
        || iter->uplink_packet_loss_fraction > 0.0f) {
      auto scoring_point = std::make_unique<
          webrtc::audio_network_adaptor::config::Controller_ScoringPoint>();
      if (iter->uplink_bandwidth_bps > 0) {
        scoring_point->set_uplink_bandwidth_bps(iter->uplink_bandwidth_bps);
      }
      if (iter->uplink_packet_loss_fraction > 0.0f) {
        scoring_point->set_uplink_packet_loss_fraction(
            iter->uplink_packet_loss_fraction);
      }
      controller->set_allocated_scoring_point(scoring_point.release());
    }
  }
  ana_config.set_min_reordering_squared_distance(23.0f);
  ana_config.set_min_reordering_time_ms(32145);
  return ana_config.SerializeToString(out_str);
}
