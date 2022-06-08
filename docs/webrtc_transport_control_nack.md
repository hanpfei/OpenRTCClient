---
title: WebRTC 的音频弱网对抗之 NACK
date: 2022-06-05 21:05:49
categories: 音视频开发
tags:
- 音视频开发
---

本文梳理 WebRTC 的音频弱网对抗中的 NACK 机制的实现。音频的 NACK 机制在 WebRTC 中默认是关闭的，本文会介绍开启 NACK 机制的方法。

在网络数据传输中，NACK (NAK，negative acknowledgment，not acknowledged) 是数据接收端主动向发送端发送消息，请求一组特定包的机制。比如接收端收到了一些数据包，但序列号在这些包之前的一些包长时间没有收到，或者接收端收到数据包的时候，通过一些完整性校验机制发现数据包已经损坏等。遇到这些情况时，接收端可以向发送端发送 NACK 消息，向发送端请求一组特定的包。
<!--more-->
在 NACK 机制中，需要实现如下这些技术点：

 * 数据接收端收集在下次发送 NACK 消息时需要请求重传的数据包的列表；
 * 接收端寻找时机，发送 NACK 消息请求发送端发送需要重传的包；
 * 定义 NACK 消息的格式；
 * 发送端在每次发送了一个数据包之后，都要将数据包先缓存一端时间，否则收到 NACK 消息时，没有数据包可用于重传；
 * 发送端收到 NACK 消息，在数据包缓存中查找 NACK 消息中请求重传的数据包并重新发送，NACK 消息中请求重传的数据包没找到时，可能还要发送个错误消息。

只有当接收端的缓冲区足够大时，或者对于整个数据流的数据完整性有强制要求时，NACK 才有意义。接收端要检测到某个数据包需要重传，可能需要比较长的时间，随后接收端发送 NACK 消息，并等待发送端重传请求的包，也需要一定的时间。NACK 机制运行起来时，大概难免意味着比较大的延时。另外就是像 TCP 这样的场景，对数据流的完整性有强制要求，NACK 比较有用武。

WebRTC 的音频数据传输中，尽管对低延时有着很高的要求，但也实现了 NACK，以用于一些音质比延迟更重要的场景。

WebRTC 的音视频数据传输都基于 RTP/RTCP 进行，顺利成章地，音频弱网对抗的 NACK 消息由 RTCP 包承载。

## 跟踪潜在需要重传的音频数据包

在 WebRTC 里，NetEQ 的 `webrtc::NackTracker` 用来跟踪和记录可能需要请求重传的数据包。`webrtc::NackTracker` 用 NACK 列表 `NackList` 来记录可能需要请求重传的数据包，这个结构的定义 (`webrtc/modules/audio_coding/neteq/nack_tracker.h`) 如下：
```
  struct NackElement {
    NackElement(int64_t initial_time_to_play_ms, uint32_t initial_timestamp)
        : time_to_play_ms(initial_time_to_play_ms),
          estimated_timestamp(initial_timestamp) {}

    // Estimated time (ms) left for this packet to be decoded. This estimate is
    // updated every time jitter buffer decodes a packet.
    int64_t time_to_play_ms;

    // A guess about the timestamp of the missing packet, it is used for
    // estimation of `time_to_play_ms`. The estimate might be slightly wrong if
    // there has been frame-size change since the last received packet and the
    // missing packet. However, the risk of this is low, and in case of such
    // errors, there will be a minor misestimation in time-to-play of missing
    // packets. This will have a very minor effect on NACK performance.
    uint32_t estimated_timestamp;
  };

  class NackListCompare {
   public:
    bool operator()(uint16_t sequence_number_old,
                    uint16_t sequence_number_new) const {
      return IsNewerSequenceNumber(sequence_number_new, sequence_number_old);
    }
  };

  typedef std::map<uint16_t, NackElement, NackListCompare> NackList;
```

`NackList` 记录可能丢失的各个数据包的序列号，及预估的各个数据包的 timestamp 和播放时间。

只有当序列号小于最近收到的数据包的数据包，才会被认为有潜在丢失的风险，也才有需要发送端重传的请求。因而 `webrtc::NackTracker` 会记录收到的最近的数据包的序列号和 timestamp。最近解码的数据包之前的数据包，或者是预计很快就要播放的数据包，即使收到，也不会再被拿去解码、处理和播放，重传这些包是没有意义的，因而 `webrtc::NackTracker` 也会记录最近解码的音频包的序列号和 timestamp。同时 `webrtc::NackTracker` 还会限制 NACK 列表的最大大小。

WebRTC 收到音频数据包，音频数据包会从 `cricket::WebRtcVoiceMediaChannel` 一路传到 `webrtc::NetEqImpl`，`webrtc::NetEqImpl` 会进一步将收到的音频包的信息给到 `webrtc::NackTracker`，这个过程如下：
```
#0  webrtc::NackTracker::UpdateLastReceivedPacket(unsigned short, unsigned int)
    (this=0x60b000011130, sequence_number=11681, timestamp=3773850239) at webrtc/modules/audio_coding/neteq/nack_tracker.cc:69
#1  webrtc::NetEqImpl::InsertPacketInternal(webrtc::RTPHeader const&, rtc::ArrayView<unsigned char const, -4711l>) (this=
    0x61600005c480, rtp_header=..., payload=...) at webrtc/modules/audio_coding/neteq/neteq_impl.cc:566
#2  webrtc::NetEqImpl::InsertPacket(webrtc::RTPHeader const&, rtc::ArrayView<unsigned char const, -4711l>)
    (this=0x61600005c480, rtp_header=..., payload=...) at webrtc/modules/audio_coding/neteq/neteq_impl.cc:170
#3  webrtc::acm2::AcmReceiver::InsertPacket(webrtc::RTPHeader const&, rtc::ArrayView<unsigned char const, -4711l>) (this=
    0x61b000008e48, rtp_header=..., incoming_payload=...) at webrtc/modules/audio_coding/acm2/acm_receiver.cc:136
#4  webrtc::voe::(anonymous namespace)::ChannelReceive::OnReceivedPayloadData(rtc::ArrayView<unsigned char const, -4711l>, webrtc::RTPHeader const&) (this=0x61b000008c80, payload=..., rtpHeader=...) at webrtc/audio/channel_receive.cc:340
#5  webrtc::voe::(anonymous namespace)::ChannelReceive::ReceivePacket(unsigned char const*, unsigned long, webrtc::RTPHeader const&)
    (this=0x61b000008c80, packet=0x608000011fa0 "\220\357-\241\340\360b\177[\336W\232\276", <incomplete sequence \336>, packet_length=78, header=...) at webrtc/audio/channel_receive.cc:719
#6  webrtc::voe::(anonymous namespace)::ChannelReceive::OnRtpPacket(webrtc::RtpPacketReceived const&)
    (this=0x61b000008c80, packet=...) at webrtc/audio/channel_receive.cc:669
#7  webrtc::RtpDemuxer::OnRtpPacket(webrtc::RtpPacketReceived const&) (this=0x620000001330, packet=...)
    at webrtc/call/rtp_demuxer.cc:249
#8  webrtc::RtpStreamReceiverController::OnRtpPacket(webrtc::RtpPacketReceived const&) (this=0x6200000012d0, packet=...)
    at webrtc/call/rtp_stream_receiver_controller.cc:52
#9  webrtc::internal::Call::DeliverRtp(webrtc::MediaType, rtc::CopyOnWriteBuffer, long) (this=
    0x620000001080, media_type=webrtc::MediaType::AUDIO, packet=..., packet_time_us=1654500987922772) at webrtc/call/call.cc:1606
#10 webrtc::internal::Call::DeliverPacket(webrtc::MediaType, rtc::CopyOnWriteBuffer, long)
   e=webrtc::MediaType::AUDIO, packet=..., packet_time_us=1654500987922772)
    at webrtc/call/call.cc:1637
#11 cricket::WebRtcVoiceMediaChannel::OnPacketReceived(rtc::CopyOnWriteBuffer, long)::$_2::operator()() const
    (this=0x60600005f3c8) at webrtc/media/engine/webrtc_voice_engine.cc:2225
```

`webrtc::NackTracker` 获得新收到的音频数据包的信息之后，会做一些处理 (`webrtc/modules/audio_coding/neteq/nack_tracker.cc`) ：
```
void NackTracker::UpdateLastReceivedPacket(uint16_t sequence_number,
                                           uint32_t timestamp) {
  // Just record the value of sequence number and timestamp if this is the
  // first packet.
  if (!any_rtp_received_) {
    sequence_num_last_received_rtp_ = sequence_number;
    timestamp_last_received_rtp_ = timestamp;
    any_rtp_received_ = true;
    // If no packet is decoded, to have a reasonable estimate of time-to-play
    // use the given values.
    if (!any_rtp_decoded_) {
      sequence_num_last_decoded_rtp_ = sequence_number;
      timestamp_last_decoded_rtp_ = timestamp;
    }
    return;
  }

  if (sequence_number == sequence_num_last_received_rtp_)
    return;

  // Received RTP should not be in the list.
  nack_list_.erase(sequence_number);

  // If this is an old sequence number, no more action is required, return.
  if (IsNewerSequenceNumber(sequence_num_last_received_rtp_, sequence_number))
    return;

  UpdatePacketLossRate(sequence_number - sequence_num_last_received_rtp_ - 1);

  UpdateList(sequence_number, timestamp);

  sequence_num_last_received_rtp_ = sequence_number;
  timestamp_last_received_rtp_ = timestamp;
  LimitNackListSize();
}
```

`webrtc::NackTracker` 对新收到的音频数据包的信息的处理过程大体如下：

 * 从 NACK 列表中移除对应数据包序列号的记录；
 * 新收到的数据包的序列号比收到的最近的数据包的序列号小，也就意味着，这是收到了一个前面以为可能丢失的包，则除了在上一步从 NACK 列表中移除对应数据包序列号的记录之外，不需要再做其它处理；否则有潜在的更新 NACK 列表的需要，继续执行后面的处理；
 * 计算收到的最近的数据包到这次收到的数据包之间还没有收到的数据包的个数，并据此计算丢包率；
 * 更新 NACK 列表；
 * 更新记录收到的最近的数据包的序列号和 timestamp；
 * 执行 NACK 列表大小限制。

`webrtc::NackTracker` 提供了一个接口 `SetMaxNackListSize(size_t max_nack_list_size)` 来设置对 NACK 列表大小的限制。这个接口设置 NACK 列表的最大大小。如果收到的最近的数据包的序列号为 `N`，则 NACK 列表将不会包含序列号小于 N - `max_nack_list_size` 的数据包的元素。`webrtc::NackTracker` 定义了最大的最大大小为 `kNackListSizeLimit` 500，如果每个编码帧包含 20 ms 的音频数据，则请求重传的间隔最长的数据包为 10s 之前的包，如果每个编码帧包含 10 ms 的音频数据，则为 5s。
```
void NackTracker::SetMaxNackListSize(size_t max_nack_list_size) {
  RTC_CHECK_GT(max_nack_list_size, 0);
  // Ugly hack to get around the problem of passing static consts by reference.
  const size_t kNackListSizeLimitLocal = NackTracker::kNackListSizeLimit;
  RTC_CHECK_LE(max_nack_list_size, kNackListSizeLimitLocal);

  max_nack_list_size_ = max_nack_list_size;
  LimitNackListSize();
}

void NackTracker::LimitNackListSize() {
  uint16_t limit = sequence_num_last_received_rtp_ -
                   static_cast<uint16_t>(max_nack_list_size_) - 1;
  nack_list_.erase(nack_list_.begin(), nack_list_.upper_bound(limit));
}
```

更新 NACK 列表的处理过程如下：
```
absl::optional<int> NackTracker::GetSamplesPerPacket(
    uint16_t sequence_number_current_received_rtp,
    uint32_t timestamp_current_received_rtp) const {
  uint32_t timestamp_increase =
      timestamp_current_received_rtp - timestamp_last_received_rtp_;
  uint16_t sequence_num_increase =
      sequence_number_current_received_rtp - sequence_num_last_received_rtp_;

  int samples_per_packet = timestamp_increase / sequence_num_increase;
  if (samples_per_packet == 0 ||
      samples_per_packet > kMaxPacketSizeMs * sample_rate_khz_) {
    // Not a valid samples per packet.
    return absl::nullopt;
  }
  return samples_per_packet;
}

void NackTracker::UpdateList(uint16_t sequence_number_current_received_rtp,
                             uint32_t timestamp_current_received_rtp) {
  if (!IsNewerSequenceNumber(sequence_number_current_received_rtp,
                             sequence_num_last_received_rtp_ + 1)) {
    return;
  }
  RTC_DCHECK(!any_rtp_decoded_ ||
             IsNewerSequenceNumber(sequence_number_current_received_rtp,
                                   sequence_num_last_decoded_rtp_));

  absl::optional<int> samples_per_packet = GetSamplesPerPacket(
      sequence_number_current_received_rtp, timestamp_current_received_rtp);
  if (!samples_per_packet) {
    return;
  }

  for (uint16_t n = sequence_num_last_received_rtp_ + 1;
       IsNewerSequenceNumber(sequence_number_current_received_rtp, n); ++n) {
    uint32_t timestamp = EstimateTimestamp(n, *samples_per_packet);
    NackElement nack_element(TimeToPlay(timestamp), timestamp);
    nack_list_.insert(nack_list_.end(), std::make_pair(n, nack_element));
  }
}

uint32_t NackTracker::EstimateTimestamp(uint16_t sequence_num,
                                        int samples_per_packet) {
  uint16_t sequence_num_diff = sequence_num - sequence_num_last_received_rtp_;
  return sequence_num_diff * samples_per_packet + timestamp_last_received_rtp_;
}
. . . . . .
int64_t NackTracker::TimeToPlay(uint32_t timestamp) const {
  uint32_t timestamp_increase = timestamp - timestamp_last_decoded_rtp_;
  return timestamp_increase / sample_rate_khz_;
}
```

新收到的数据包和之前收到的最新的数据包之间的数据包有潜在丢失的风险，因而有可能需要请求重传。更新 NACK 列表即为在 NACK 列表中为这些数据包创建相应的记录。这个过程如下：

 * 如果新收到的数据包的序列号与之前收到的最新的数据包的序列号是连续的，则它们之间不可能再有需要请求重传的包，结束处理过程；
 * 计算每个数据包中的每通道采样数，对于音频 RTP 数据包，连续两个数据包的 timestamp 的差值即为数据包中包含的每通道的采样数，`webrtc::NackTracker` 根据新收到的数据包的序列号和 timestamp 及之前收到的最新的数据包的序列号和 timestamp 计算每个数据包中的每通道采样数；
 * 在 NACK 列表中为疑似丢失的数据包计算 timestamp 和预期播放时间，并创建相应的记录。

WebRTC 解码播放一个音频数据包，`webrtc::NetEqImpl` 解码音频数据包时，会将被解码的音频数据包的信息给到 `webrtc::NackTracker`，这个过程如下：
```
#0  webrtc::NackTracker::UpdateLastDecodedPacket(unsigned short, unsigned int)
    (this=0x60b000011130, sequence_number=22961, timestamp=1865149347) at webrtc/modules/audio_coding/neteq/nack_tracker.cc:159
#1  webrtc::NetEqImpl::ExtractPackets(unsigned long, std::__cxx11::list<webrtc::Packet, std::allocator<webrtc::Packet> >*) (this=0x61600005c480, required_samples=480, packet_list=0x7fffdcac1920) at webrtc/modules/audio_coding/neteq/neteq_impl.cc:1988
#2  webrtc::NetEqImpl::GetDecision(webrtc::NetEq::Operation*, std::__cxx11::list<webrtc::Packet, std::allocator<webrtc::Packet> >*, webrtc::DtmfEvent*, bool*, absl::optional<webrtc::NetEq::Operation>)
    (this=0x61600005c480, operation=0x7fffdcac19a0, packet_list=0x7fffdcac1920, dtmf_event=0x7fffdcac1960, play_dtmf=0x7fffdcac19b0, action_override=...) at webrtc/modules/audio_coding/neteq/neteq_impl.cc:1295
#3  webrtc::NetEqImpl::GetAudioInternal(webrtc::AudioFrame*, bool*, absl::optional<webrtc::NetEq::Operation>)
    (this=0x61600005c480, audio_frame=0x628000010110, muted=0x7fffdc8cf190, action_override=...)
    at webrtc/modules/audio_coding/neteq/neteq_impl.cc:818
#4  webrtc::NetEqImpl::GetAudio(webrtc::AudioFrame*, bool*, int*, absl::optional<webrtc::NetEq::Operation>)
    (this=0x61600005c480, audio_frame=0x628000010110, muted=0x7fffdc8cf190, current_sample_rate_hz=0x7fffdcd5b3b0, action_override=...)
    at webrtc/modules/audio_coding/neteq/neteq_impl.cc:239
#5  webrtc::acm2::AcmReceiver::GetAudio(int, webrtc::AudioFrame*, bool*)
    (this=0x61b000008e48, desired_freq_hz=48000, audio_frame=0x628000010110, muted=0x7fffdc8cf190)
    at webrtc/modules/audio_coding/acm2/acm_receiver.cc:151
#6  webrtc::voe::(anonymous namespace)::ChannelReceive::GetAudioFrameWithInfo(int, webrtc::AudioFrame*)
    (this=0x61b000008c80, sample_rate_hz=48000, audio_frame=0x628000010110) at webrtc/audio/channel_receive.cc:388
#7  webrtc::internal::AudioReceiveStream::GetAudioFrameWithInfo(int, webrtc::AudioFrame*) (this=
    0x61600005be80, sample_rate_hz=48000, audio_frame=0x628000010110) at webrtc/audio/audio_receive_stream.cc:393
#8  webrtc::AudioMixerImpl::GetAudioFromSources(int) (this=0x61d000021280, output_frequency=48000)
    at webrtc/modules/audio_mixer/audio_mixer_impl.cc:205
#9  webrtc::AudioMixerImpl::Mix(unsigned long, webrtc::AudioFrame*)
    (this=0x61d000021280, number_of_channels=2, audio_frame_for_mixing=0x6280000042e8)
    at webrtc/modules/audio_mixer/audio_mixer_impl.cc:175
#10 webrtc::AudioTransportImpl::NeedMorePlayData(unsigned long, unsigned long, unsigned long, unsigned int, void*, unsigned long&, long*, long*) (this=
    0x6280000041e0, nSamples=441, nBytesPerSample=4, nChannels=2, samplesPerSec=44100, audioSamples=0x61c000080080, nSamplesOut=@0x7fffdc8ce400: 0, elapsed_time_ms=0x7fffdc8ce4c0, ntp_time_ms=0x7fffdc8ce4e0) at webrtc/audio/audio_transport_impl.cc:215
#11 webrtc::AudioDeviceBuffer::RequestPlayoutData(unsigned long) (this=0x614000010058, samples_per_channel=441)
    at webrtc/modules/audio_device/audio_device_buffer.cc:303
#12 webrtc::AudioDeviceLinuxPulse::PlayThreadProcess() (this=0x61900000ff80)
    at webrtc/modules/audio_device/linux/audio_device_pulse_linux.cc:2106
#13 webrtc::AudioDeviceLinuxPulse::Init()::$_1::operator()() const (this=0x607000003cf0)
    at webrtc/modules/audio_device/linux/audio_device_pulse_linux.cc:174
```

音频的解码播放是个同步过程。`webrtc::NackTracker` 获得最近解码的音频数据包的信息之后，会做一些处理 (`webrtc/modules/audio_coding/neteq/nack_tracker.cc`) ：
```
void NackTracker::UpdateEstimatedPlayoutTimeBy10ms() {
  while (!nack_list_.empty() &&
         nack_list_.begin()->second.time_to_play_ms <= 10)
    nack_list_.erase(nack_list_.begin());

  for (NackList::iterator it = nack_list_.begin(); it != nack_list_.end(); ++it)
    it->second.time_to_play_ms -= 10;
}

void NackTracker::UpdateLastDecodedPacket(uint16_t sequence_number,
                                          uint32_t timestamp) {
  if (IsNewerSequenceNumber(sequence_number, sequence_num_last_decoded_rtp_) ||
      !any_rtp_decoded_) {
    sequence_num_last_decoded_rtp_ = sequence_number;
    timestamp_last_decoded_rtp_ = timestamp;
    // Packets in the list with sequence numbers less than the
    // sequence number of the decoded RTP should be removed from the lists.
    // They will be discarded by the jitter buffer if they arrive.
    nack_list_.erase(nack_list_.begin(),
                     nack_list_.upper_bound(sequence_num_last_decoded_rtp_));

    // Update estimated time-to-play.
    for (NackList::iterator it = nack_list_.begin(); it != nack_list_.end();
         ++it)
      it->second.time_to_play_ms = TimeToPlay(it->second.estimated_timestamp);
  } else {
    RTC_DCHECK_EQ(sequence_number, sequence_num_last_decoded_rtp_);

    // Same sequence number as before. 10 ms is elapsed, update estimations for
    // time-to-play.
    UpdateEstimatedPlayoutTimeBy10ms();

    // Update timestamp for better estimate of time-to-play, for packets which
    // are added to NACK list later on.
    timestamp_last_decoded_rtp_ += sample_rate_khz_ * 10;
  }
  any_rtp_decoded_ = true;
}
```

这个过程大体为移除 NACK 列表中，早于解码的数据包的记录，并更新其中各个数据包的记录预期播放时间。各个数据包的预期播放时间根据解码的数据包的 timestamp 和这个数据包的 timestamp 的差值计算获得。

## 接收端寻找时机发送 NACK 消息

WebRTC 在每次收到音频数据包，并把它送进 NetEQ 之后，就会立即去获取 NACK 列表，这个过程如下：
```
#0  webrtc::NackTracker::GetNackList(long) (this=0x60b000011130, round_trip_time_ms=0)
    at webrtc/modules/audio_coding/neteq/nack_tracker.cc:226
#1  webrtc::NetEqImpl::GetNackList(long) const (this=0x61600005c480, round_trip_time_ms=0)
    at webrtc/modules/audio_coding/neteq/neteq_impl.cc:476
#2  webrtc::acm2::AcmReceiver::GetNackList(long) const (this=0x61b000008e48, round_trip_time_ms=0)
    at webrtc/modules/audio_coding/acm2/acm_receiver.cc:325
#3  webrtc::voe::(anonymous namespace)::ChannelReceive::OnReceivedPayloadData(rtc::ArrayView<unsigned char const, -4711l>, webrtc::RTPHeader const&) (this=0x61b000008c80, payload=..., rtpHeader=...) at webrtc/audio/channel_receive.cc:349
#4  webrtc::voe::(anonymous namespace)::ChannelReceive::ReceivePacket(unsigned char const*, unsigned long, webrtc::RTPHeader const&)
    (this=0x61b000008c80, packet=0x60b000053b80 "\220oY\306o,:c\204\307ӧ\276", <incomplete sequence \336>, packet_length=94, header=...)
    at webrtc/audio/channel_receive.cc:719
```

`webrtc::NackTracker` 将 NACK 列表中的序列号以 `std::vector<uint16_t>` 的形式返回：
```
// We don't erase elements with time-to-play shorter than round-trip-time.
std::vector<uint16_t> NackTracker::GetNackList(int64_t round_trip_time_ms) {
  RTC_DCHECK_GE(round_trip_time_ms, 0);
  std::vector<uint16_t> sequence_numbers;
  if (config_.require_valid_rtt && round_trip_time_ms == 0) {
    return sequence_numbers;
  }
  if (packet_loss_rate_ >
      static_cast<uint32_t>(config_.max_loss_rate * (1 << 30))) {
    return sequence_numbers;
  }
  // The estimated packet loss is between 0 and 1, so we need to multiply by 100
  // here.
  int max_wait_ms =
      100.0 * config_.ms_per_loss_percent * packet_loss_rate_ / (1 << 30);
  for (NackList::const_iterator it = nack_list_.begin(); it != nack_list_.end();
       ++it) {
    int64_t time_since_packet_ms =
        (timestamp_last_received_rtp_ - it->second.estimated_timestamp) /
        sample_rate_khz_;
    if (it->second.time_to_play_ms > round_trip_time_ms ||
        time_since_packet_ms + round_trip_time_ms < max_wait_ms)
      sequence_numbers.push_back(it->first);
  }
  if (config_.never_nack_multiple_times) {
    nack_list_.clear();
  }
  return sequence_numbers;
}

void NackTracker::UpdatePacketLossRate(int packets_lost) {
  const uint64_t alpha_q30 = (1 << 30) * config_.packet_loss_forget_factor;
  // Exponential filter.
  packet_loss_rate_ = (alpha_q30 * packet_loss_rate_) >> 30;
  for (int i = 0; i < packets_lost; ++i) {
    packet_loss_rate_ =
        ((alpha_q30 * packet_loss_rate_) >> 30) + ((1 << 30) - alpha_q30);
  }
}
```

`webrtc::NackTracker` 还会对 NACK 列表中的数据包再做一个过滤，太老的或者很快就要播放的数据包不会发送重传请求。如果不允许针对同一个数据包重复发送 NACK 请求，则会将 NACK 列表清空；由 `NackTracker::UpdateLastReceivedPacket()` 的执行过程，可以看到，借助于记录的 `sequence_num_last_received_rtp_`，同一个包的记录不会被加入到 NACK 列表两次。

`webrtc::NackTracker` 在返回 NACK 列表时，没有针对特定的数据包去等待，收到一个数据包之后，它前面还没有收到的数据包，立即就会被认为已经丢失，并发送重传请求。在 `webrtc::NackTracker` 的实现中，检测需要重传的数据包的过程还是比较快的。

`webrtc::voe::(anonymous namespace)::ChannelReceive::OnReceivedPayloadData()` 的实现如下：

```
void ChannelReceive::OnReceivedPayloadData(
    rtc::ArrayView<const uint8_t> payload,
    const RTPHeader& rtpHeader) {
  if (!playing_) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.

    // If we have a source_tracker_, tell it that the frame has been
    // "delivered". Normally, this happens in AudioReceiveStream when audio
    // frames are pulled out, but when playout is muted, nothing is pulling
    // frames. The downside of this approach is that frames delivered this way
    // won't be delayed for playout, and therefore will be unsynchronized with
    // (a) audio delay when playing and (b) any audio/video synchronization. But
    // the alternative is that muting playout also stops the SourceTracker from
    // updating RtpSource information.
    if (source_tracker_) {
      RtpPacketInfos::vector_type packet_vector = {
          RtpPacketInfo(rtpHeader, clock_->CurrentTime())};
      source_tracker_->OnFrameDelivered(RtpPacketInfos(packet_vector));
    }

    return;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (acm_receiver_.InsertPacket(rtpHeader, payload) != 0) {
    RTC_DLOG(LS_ERROR) << "ChannelReceive::OnReceivedPayloadData() unable to "
                          "push data to the ACM";
    return;
  }

  int64_t round_trip_time = 0;
  rtp_rtcp_->RTT(remote_ssrc_, &round_trip_time, NULL, NULL, NULL);

  std::vector<uint16_t> nack_list = acm_receiver_.GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Can't use nack_list.data() since it's not supported by all
    // compilers.
    ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
}
. . . . . .
// Called when we are missing one or more packets.
int ChannelReceive::ResendPackets(const uint16_t* sequence_numbers,
                                  int length) {
  return rtp_rtcp_->SendNACK(sequence_numbers, length);
}
```

`webrtc::voe::(anonymous namespace)::ChannelReceive` 通过 `webrtc::ModuleRtpRtcpImpl2` 把 NACK 包发送出去。`webrtc::ModuleRtpRtcpImpl2` 对要请求重传的音频数据包再做个过滤，随后通过 `webrtc::RTCPSender` 把 NACK 包发送出去：
```
// Send a Negative acknowledgment packet.
int32_t ModuleRtpRtcpImpl2::SendNACK(const uint16_t* nack_list,
                                     const uint16_t size) {
  uint16_t nack_length = size;
  uint16_t start_id = 0;
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (TimeToSendFullNackList(now_ms)) {
    nack_last_time_sent_full_ms_ = now_ms;
  } else {
    // Only send extended list.
    if (nack_last_seq_number_sent_ == nack_list[size - 1]) {
      // Last sequence number is the same, do not send list.
      return 0;
    }
    // Send new sequence numbers.
    for (int i = 0; i < size; ++i) {
      if (nack_last_seq_number_sent_ == nack_list[i]) {
        start_id = i + 1;
        break;
      }
    }
    nack_length = size - start_id;
  }

  // Our RTCP NACK implementation is limited to kRtcpMaxNackFields sequence
  // numbers per RTCP packet.
  if (nack_length > kRtcpMaxNackFields) {
    nack_length = kRtcpMaxNackFields;
  }
  nack_last_seq_number_sent_ = nack_list[start_id + nack_length - 1];

  return rtcp_sender_.SendRTCP(GetFeedbackState(), kRtcpNack, nack_length,
                               &nack_list[start_id]);
}
```

`webrtc::ModuleRtpRtcpImpl2` 这里也会保证不针对同一个数据包重复发送 NACK 请求。`webrtc::RTCPSender` 构造 NACK 反馈包并通过 `webrtc::Transport` 发送出去。

## 接收端开启音频 NACK

在 NetEQ 中，只有当音频 NACK 开启时，才会支持获取音频 NACK 列表等与 NACK 有关的操作，如 (`webrtc/modules/audio_coding/neteq/neteq_impl.cc`)：
```
std::vector<uint16_t> NetEqImpl::GetNackList(int64_t round_trip_time_ms) const {
  MutexLock lock(&mutex_);
  if (!nack_enabled_) {
    return std::vector<uint16_t>();
  }
  RTC_DCHECK(nack_.get());
  return nack_->GetNackList(round_trip_time_ms);
}
```

在 NetEQ 中，NACK 默认是关闭的。如果需要开启 NACK，需要如下这样一个专门的过程来完成：
```
#0  webrtc::NetEqImpl::EnableNack(unsigned long) (this=0x61600005c480, max_nack_list_size=250)
    at webrtc/modules/audio_coding/neteq/neteq_impl.cc:455
#1  webrtc::acm2::AcmReceiver::EnableNack(unsigned long) (this=0x61b000008e48, max_nack_list_size=250)
    at webrtc/modules/audio_coding/acm2/acm_receiver.cc:315
#2  webrtc::voe::(anonymous namespace)::ChannelReceive::SetNACKStatus(bool, int)
    (this=0x61b000008c80, enable=true, max_packets=250) at webrtc/audio/channel_receive.cc:880
#3  webrtc::internal::AudioReceiveStream::AudioReceiveStream(webrtc::Clock*, webrtc::PacketRouter*, webrtc::AudioReceiveStream::Config const&, rtc::scoped_refptr<webrtc::AudioState> const&, webrtc::RtcEventLog*, std::unique_ptr<webrtc::voe::ChannelReceiveInterface, std::default_delete<webrtc::voe::ChannelReceiveInterface> >) (this=0x61600005be80, clock=0x602000003bb0, packet_router=0x61c000060908, 
    config=..., audio_state=..., event_log=0x613000011f40, channel_receive=std::unique_ptr<webrtc::voe::ChannelReceiveInterface> = {...})
    at webrtc/audio/audio_receive_stream.cc:140
#4  webrtc::internal::AudioReceiveStream::AudioReceiveStream(webrtc::Clock*, webrtc::PacketRouter*, webrtc::NetEqFactory*, webrtc::AudioReceiveStream::Config const&, rtc::scoped_refptr<webrtc::AudioState> const&, webrtc::RtcEventLog*)
    (this=0x61600005be80, clock=0x602000003bb0, packet_router=0x61c000060908, neteq_factory=
    0x0, config=..., audio_state=..., event_log=0x613000011f40) at webrtc/audio/audio_receive_stream.cc:98
#5  webrtc::internal::Call::CreateAudioReceiveStream(webrtc::AudioReceiveStream::Config const&)
    (this=0x620000001080, config=...) at webrtc/call/call.cc:954
#6  cricket::WebRtcVoiceMediaChannel::WebRtcAudioReceiveStream::WebRtcAudioReceiveStream(webrtc::AudioReceiveStream::Config, webrtc::Call*) (this=0x60b000010fd0, config=..., call=0x620000001080) at webrtc/media/engine/webrtc_voice_engine.cc:1216
#7  cricket::WebRtcVoiceMediaChannel::AddRecvStream(cricket::StreamParams const&) (this=0x619000017c80, sp=...)
    at webrtc/media/engine/webrtc_voice_engine.cc:2021
#8  cricket::BaseChannel::AddRecvStream_w(cricket::StreamParams const&) (this=0x619000018180, sp=...)
    at webrtc/pc/channel.cc:567
#9  cricket::BaseChannel::UpdateRemoteStreams_w(std::vector<cricket::StreamParams, std::allocator<cricket::StreamParams> > const&, webrtc::SdpType, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >*)
    (this=0x619000018180, streams=std::vector of length 1, capacity 1 = {...}, type=webrtc::SdpType::kOffer, error_desc=0x7ffff2387e00)
    at webrtc/pc/channel.cc:725
#10 cricket::VoiceChannel::SetRemoteContent_w(cricket::MediaContentDescription const*, webrtc::SdpType, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >*) (this=0x619000018180, content=
    0x6130000003c0, type=webrtc::SdpType::kOffer, error_desc=0x7ffff2387e00) at webrtc/pc/channel.cc:926
#11 cricket::BaseChannel::SetRemoteContent(cricket::MediaContentDescription const*, webrtc::SdpType, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >*)
    (this=0x619000018180, content=0x6130000003c0, type=webrtc::SdpType::kOffer, error_desc=0x7ffff2387e00)
    at webrtc/pc/channel.cc:292
#12 webrtc::SdpOfferAnswerHandler::PushdownMediaDescription(webrtc::SdpType, cricket::ContentSource, std::map<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, cricket::ContentGroup const*, std::less<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >, std::allocator<std::pair<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const, cricket::ContentGroup const*> > > const&)::$_20::operator()() const (this=0x7ffff2f47f00)
    at webrtc/pc/sdp_offer_answer.cc:4273
```

对 NetEQ 开启音频 NACK 时，NetEQ 创建 `webrtc::NackTracker` 对象，并为它设置最大 NACK 列表大小：
```
void NetEqImpl::EnableNack(size_t max_nack_list_size) {
  MutexLock lock(&mutex_);
  if (!nack_enabled_) {
    nack_ = std::make_unique<NackTracker>();
    nack_enabled_ = true;
    nack_->UpdateSampleRate(fs_hz_);
  }
  nack_->SetMaxNackListSize(max_nack_list_size);
}
```

`webrtc::internal::AudioReceiveStream` 对象创建时，有个配置项 `config.rtp.nack.rtp_history_ms` 用于控制是否开启 NACK。当`config.rtp.nack.rtp_history_ms` 的值大于 0 时，开启 NACK，否则不开启。`config.rtp.nack.rtp_history_ms` 的值根据 `WebRtcVoiceEngine` 的 `recv_nack_enabled_` 配置计算得到，而这个配置则来自于 codec spec 的 `nack_enabled`，codec spec 的配置来自于接收和发送的两方协调的 codec 配置的 SDP 消息：
```
  for (const AudioCodec& voice_codec : codecs) {
    if (!(IsCodec(voice_codec, kCnCodecName) ||
          IsCodec(voice_codec, kDtmfCodecName) ||
          IsCodec(voice_codec, kRedCodecName))) {
      webrtc::SdpAudioFormat format(voice_codec.name, voice_codec.clockrate,
                                    voice_codec.channels, voice_codec.params);

      voice_codec_info = engine()->encoder_factory_->QueryAudioEncoder(format);
      if (!voice_codec_info) {
        RTC_LOG(LS_WARNING) << "Unknown codec " << ToString(voice_codec);
        continue;
      }

      send_codec_spec = webrtc::AudioSendStream::Config::SendCodecSpec(
          voice_codec.id, format);
      if (voice_codec.bitrate > 0) {
        send_codec_spec->target_bitrate_bps = voice_codec.bitrate;
      }
      send_codec_spec->transport_cc_enabled = HasTransportCc(voice_codec);
      send_codec_spec->nack_enabled = HasNack(voice_codec);
      send_codec_spec->enable_non_sender_rtt = HasRrtr(voice_codec);
      bitrate_config = GetBitrateConfigForCodec(voice_codec);
      break;
    }
    send_codec_position++;
  }
```

更具体来说，判断一个 codec 是否支持 NACK 是根据 codec 是否支持反馈参数 NACK 来完成的 (`HasNack(voice_codec)`)。默认情况下，没有任何一种 codec 支持 NACK 反馈参数。

[WebRTC 中收集音视频编解码能力](https://github.com/hanpfei/OpenRTCClient/blob/m98_4758/docs/webrtc_collect_audio_video_codecs.md) 中我们看到，`WebRtcVoiceEngine::CollectCodecs()` 函数对于支持网络适配的 codec，会为它加上反馈参数 `kRtcpFbParamTransportCc`：
```
    if (opt_codec) {
      AudioCodec& codec = *opt_codec;
      if (spec.info.supports_network_adaption) {
        codec.AddFeedbackParam(
            FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
      }
```

要使某个 codec 支持 NACK，需要在这里为它加上对 NACK 反馈参数的支持，如下面这样：
```
    if (opt_codec) {
      AudioCodec& codec = *opt_codec;
      if (spec.info.supports_network_adaption) {
        codec.AddFeedbackParam(
            FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
        codec.AddFeedbackParam(
            FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
      }
```

这样就启用了对 NACK 的支持。

## 发送端发送数据后把数据包放进缓冲区

在 WebRTC 中，用于保存已经发送的音视频数据包的缓冲区为 `webrtc::RtpPacketHistory`。在 `webrtc::RtpSenderEgress` 中，除了将音视频数据包发送到网络之外，还会将数据包保存进 `webrtc::RtpPacketHistory` (`webrtc/modules/rtp_rtcp/source/rtp_sender_egress.cc`)：

```
  const bool send_success = SendPacketToNetwork(*packet, options, pacing_info);

  // Put packet in retransmission history or update pending status even if
  // actual sending fails.
  if (is_media && packet->allow_retransmission()) {
    packet_history_->PutRtpPacket(std::make_unique<RtpPacketToSend>(*packet),
                                  now_ms);
  } else if (packet->retransmitted_sequence_number()) {
    packet_history_->MarkPacketAsSent(*packet->retransmitted_sequence_number());
  }
```

这个过程如下：
```
#0  webrtc::RtpPacketHistory::PutRtpPacket(std::unique_ptr<webrtc::RtpPacketToSend, std::default_delete<webrtc::RtpPacketToSend> >, absl::optional<long>) (this=0x61b000006280, packet=std::unique_ptr<webrtc::RtpPacketToSend> = {...}, send_time_ms=...)
    at webrtc/modules/rtp_rtcp/source/rtp_packet_history.cc:124
#1  webrtc::RtpSenderEgress::SendPacket(webrtc::RtpPacketToSend*, webrtc::PacedPacketInfo const&)
    (this=0x61b000006400, packet=0x60e000050060, pacing_info=...) at webrtc/modules/rtp_rtcp/source/rtp_sender_egress.cc:278
#2  webrtc::ModuleRtpRtcpImpl2::TrySendPacket(webrtc::RtpPacketToSend*, webrtc::PacedPacketInfo const&)
    (this=0x61b000005b80, packet=0x60e000050060, pacing_info=...) at webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl2.cc:376
#3  webrtc::PacketRouter::SendPacket(std::unique_ptr<webrtc::RtpPacketToSend, std::default_delete<webrtc::RtpPacketToSend> >, webrtc::PacedPacketInfo const&) (this=0x61c000060108, packet=std::unique_ptr<webrtc::RtpPacketToSend> = {...}, cluster_info=...)
    at webrtc/modules/pacing/packet_router.cc:160
#4  webrtc::PacingController::ProcessPackets() (this=0x61a0000174a8)
    at webrtc/modules/pacing/pacing_controller.cc:585
#5  webrtc::TaskQueuePacedSender::MaybeProcessPackets(webrtc::Timestamp)
    (this=0x61a000017480, scheduled_process_time=...) at webrtc/modules/pacing/task_queue_paced_sender.cc:234
#6  webrtc::TaskQueuePacedSender::EnqueuePackets(std::vector<std::unique_ptr<webrtc::RtpPacketToSend, std::default_delete<webrtc::RtpPacketToSend> >, std::allocator<std::unique_ptr<webrtc::RtpPacketToSend, std::default_delete<webrtc::RtpPacketToSend> > > >)::$_8::operator()() (this=0x6040000c2158) at webrtc/modules/pacing/task_queue_paced_sender.cc:147
```

## 发送端接收并处理 RTCP NACK 反馈包

发送端的 `webrtc::internal::AudioSendStream` 接收接收端发回来的 RTCP 包，这个包的处理过程如下：

```
#0  webrtc::RTCPReceiver::IncomingPacket(unsigned char const*, unsigned long)
    (this=0x61b000005ef0, packet=0x6040000b79d0 "\257", <incomplete sequence \315>, packet_size=24)
    at webrtc/modules/rtp_rtcp/source/rtcp_receiver.h:102
#1  webrtc::ModuleRtpRtcpImpl2::IncomingRtcpPacket(unsigned char const*, unsigned long)
    (this=0x61b000005b80, rtcp_packet=0x6040000b79d0 "\257", <incomplete sequence \315>, length=24)
    at webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl2.cc:155
#2  webrtc::voe::(anonymous namespace)::ChannelSend::ReceivedRTCPPacket(unsigned char const*, unsigned long) (this=
    0x61600005a080, data=0x6040000b79d0 "\257", <incomplete sequence \315>, length=24) at webrtc/audio/channel_send.cc:636
#3  webrtc::internal::AudioSendStream::DeliverRtcp(unsigned char const*, unsigned long)
    (this=0x619000016880, packet=0x6040000b79d0 "\257", <incomplete sequence \315>, length=24)
    at webrtc/audio/audio_send_stream.cc:489
#4  webrtc::internal::Call::DeliverRtcp(webrtc::MediaType, rtc::CopyOnWriteBuffer)::$_3::operator()() const
    (this=0x6040000b7a18) at webrtc/call/call.cc:1544
```

`webrtc::RTCPReceiver` 解析 RTCP 包，并根据 RTCP 包的类型做处理：

```
void RTCPReceiver::IncomingPacket(rtc::ArrayView<const uint8_t> packet) {
  if (packet.empty()) {
    RTC_LOG(LS_WARNING) << "Incoming empty RTCP packet";
    return;
  }

  PacketInformation packet_information;
  if (!ParseCompoundPacket(packet, &packet_information))
    return;
  TriggerCallbacksFromRtcpPacket(packet_information);
}
 . . . . . .
// Holding no Critical section.
void RTCPReceiver::TriggerCallbacksFromRtcpPacket(
    const PacketInformation& packet_information) {
  // Process TMMBR and REMB first to avoid multiple callbacks
  // to OnNetworkChanged.
  if (packet_information.packet_type_flags & kRtcpTmmbr) {
    // Might trigger a OnReceivedBandwidthEstimateUpdate.
    NotifyTmmbrUpdated();
  }

  if (!receiver_only_ && (packet_information.packet_type_flags & kRtcpSrReq)) {
    rtp_rtcp_->OnRequestSendReport();
  }
  if (!receiver_only_ && (packet_information.packet_type_flags & kRtcpNack)) {
    if (!packet_information.nack_sequence_numbers.empty()) {
      RTC_LOG(LS_VERBOSE) << "Incoming NACK length: "
                          << packet_information.nack_sequence_numbers.size();
      rtp_rtcp_->OnReceivedNack(packet_information.nack_sequence_numbers);
    }
  }
```

`webrtc::ModuleRtpRtcpImpl2` 处理 NACK 包 (`webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl2.cc`) ：

```
void ModuleRtpRtcpImpl2::OnReceivedNack(
    const std::vector<uint16_t>& nack_sequence_numbers) {
  if (!rtp_sender_)
    return;

  if (!StorePackets() || nack_sequence_numbers.empty()) {
    return;
  }
  // Use RTT from RtcpRttStats class if provided.
  int64_t rtt = rtt_ms();
  if (rtt == 0) {
    rtcp_receiver_.RTT(rtcp_receiver_.RemoteSSRC(), NULL, &rtt, NULL, NULL);
  }
  rtp_sender_->packet_generator.OnReceivedNack(nack_sequence_numbers, rtt);
}
```

`webrtc::ModuleRtpRtcpImpl2` 通过 `webrtc::RTPSender` 找到需要重传的包并发送出去：

```
int32_t RTPSender::ReSendPacket(uint16_t packet_id) {
  // Try to find packet in RTP packet history. Also verify RTT here, so that we
  // don't retransmit too often.
  absl::optional<RtpPacketHistory::PacketState> stored_packet =
      packet_history_->GetPacketState(packet_id);
  if (!stored_packet || stored_packet->pending_transmission) {
    // Packet not found or already queued for retransmission, ignore.
    return 0;
  }

  const int32_t packet_size = static_cast<int32_t>(stored_packet->packet_size);
  const bool rtx = (RtxStatus() & kRtxRetransmitted) > 0;

  std::unique_ptr<RtpPacketToSend> packet =
      packet_history_->GetPacketAndMarkAsPending(
          packet_id, [&](const RtpPacketToSend& stored_packet) {
            // Check if we're overusing retransmission bitrate.
            // TODO(sprang): Add histograms for nack success or failure
            // reasons.
            std::unique_ptr<RtpPacketToSend> retransmit_packet;
            if (retransmission_rate_limiter_ &&
                !retransmission_rate_limiter_->TryUseRate(packet_size)) {
              return retransmit_packet;
            }
            if (rtx) {
              retransmit_packet = BuildRtxPacket(stored_packet);
            } else {
              retransmit_packet =
                  std::make_unique<RtpPacketToSend>(stored_packet);
            }
            if (retransmit_packet) {
              retransmit_packet->set_retransmitted_sequence_number(
                  stored_packet.SequenceNumber());
            }
            return retransmit_packet;
          });
  if (!packet) {
    return -1;
  }
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->set_fec_protect_packet(false);
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  packets.emplace_back(std::move(packet));
  paced_sender_->EnqueuePackets(std::move(packets));

  return packet_size;
}
 . . . . . .
void RTPSender::OnReceivedNack(
    const std::vector<uint16_t>& nack_sequence_numbers,
    int64_t avg_rtt) {
  packet_history_->SetRtt(5 + avg_rtt);
  for (uint16_t seq_no : nack_sequence_numbers) {
    const int32_t bytes_sent = ReSendPacket(seq_no);
    if (bytes_sent < 0) {
      // Failed to send one Sequence number. Give up the rest in this nack.
      RTC_LOG(LS_WARNING) << "Failed resending RTP packet " << seq_no
                          << ", Discard rest of packets.";
      break;
    }
  }
}
```

`webrtc::RTPSender` 在 `webrtc::RtpPacketHistory` 中查找要重传的包，如果找到，就通过 PacedSender 发送出去。这整个过程大概就像下面这样：

![1654572386229.jpg](https://upload-images.jianshu.io/upload_images/1315506-599b45fe7398d8bb.jpg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

图中蓝色箭头和红色方框中的这些逻辑是 NACK 数据包处理过程中，不同于一般采集、编码及发送流程的地方。

WebRTC 的音频弱网对抗的 NACK 实现大体如此。

本文的分析中，含有一些函数调用栈的信息，函数调用栈的信息中甚至包含了代码所在的源文件及行号。这里的分析基于 **[OpenRTCClient](https://github.com/hanpfei/OpenRTCClient)** 中的 WebRTC M98 的源码进行。

**参考文章**

[NACK (NAK, negative acknowledgment, not acknowledged)](https://www.techtarget.com/searchnetworking/definition/NAK)

Done.
