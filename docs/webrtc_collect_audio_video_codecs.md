---
title: WebRTC 中收集音视频编解码能力
date: 2022-06-04 13:05:49
categories: 音视频开发
tags:
- 音视频开发
---

在 WebRTC 中，交互的两端在建立连接的过程中，需要通过 ICE 协议，交换各自的音视频编解码能力，及各个编解码器支持的传输控制能力，如支持的编解码器的种类和各编解码器的一些参数配置，以及是否启用传输拥赛控制和 NACK 等，并协商出一组配置和参数，用于后续的音视频传输过程。
<!--more-->
对于音频，编解码能力的一部分信息从 audio 的 encoder 和 decoder factory 中获取。Audio encoder 和 decoder factory 在创建 `PeerConnectionFactoryInterface` 的时候，由 webrtc 的用户传入，如在 webrtc 示例应用 peerconnection_client 中 (`webrtc/examples/peerconnection/client/conductor.cc`)：
```
bool Conductor::InitializePeerConnection() {
  RTC_DCHECK(!peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);

  if (!signaling_thread_.get()) {
    signaling_thread_ = rtc::Thread::CreateWithSocketServer();
    signaling_thread_->Start();
  }
  peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
      nullptr /* network_thread */, nullptr /* worker_thread */,
      signaling_thread_.get(), nullptr /* default_adm */,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
      nullptr /* audio_processing */);
```

WebRTC 内部用于创建音频***编码器***工厂的 `CreateBuiltinAudioEncoderFactory()` 函数实现 (`webrtc/api/audio_codecs/builtin_audio_encoder_factory.cc`) 如下：
```
rtc::scoped_refptr<AudioEncoderFactory> CreateBuiltinAudioEncoderFactory() {
  return CreateAudioEncoderFactory<

#if WEBRTC_USE_BUILTIN_OPUS
      AudioEncoderOpus, NotAdvertised<AudioEncoderMultiChannelOpus>,
#endif

      AudioEncoderIsac, AudioEncoderG722,

#if WEBRTC_USE_BUILTIN_ILBC
      AudioEncoderIlbc,
#endif

      AudioEncoderG711, NotAdvertised<AudioEncoderL16>>();
}
```

WebRTC 默认的音频***编码器***工厂，可以无条件支持 ISAC、G722、G711 和 L16 这几种编码器，同时还可以根据特定编解码器的开关配置支持 OPUS、多通道 OPUS 和 ILBC。AAC 这种常见的音频编解码格式，WebRTC 默认是不支持的。OPUS 由于它的良好特性，一般默认都是打开的。

WebRTC 内部用于创建音频***解码器***工厂的 `CreateBuiltinAudioDecoderFactory()` 函数实现 (`webrtc/api/audio_codecs/builtin_audio_decoder_factory.cc`) 如下：
```
rtc::scoped_refptr<AudioDecoderFactory> CreateBuiltinAudioDecoderFactory() {
  return CreateAudioDecoderFactory<

#if WEBRTC_USE_BUILTIN_OPUS
      AudioDecoderOpus, NotAdvertised<AudioDecoderMultiChannelOpus>,
#endif

      AudioDecoderIsac, AudioDecoderG722,

#if WEBRTC_USE_BUILTIN_ILBC
      AudioDecoderIlbc,
#endif

      AudioDecoderG711, NotAdvertised<AudioDecoderL16>>();
}
```

一般来说，一个音频编解码器库都会同时支持某种编解码器的编码和解码能力，尽管在 WebRTC 中编码器工厂和解码器工厂是两个类，但它们支持的 codec 集合是完全一样的。

`PeerConnectionFactoryInterface`/`webrtc::PeerConnectionFactory` 对象在创建的时候，会创建一系列重要的全局性对象，其中包括 `cricket::MediaEngineInterface`，以及 `ConnectionContext`：
```
#0  webrtc::ConnectionContext::ConnectionContext(webrtc::PeerConnectionFactoryDependencies*) (this=0x60b000005b00, dependencies=0x7ffff22f6d20)
    at webrtc/pc/connection_context.cc:81
#1  webrtc::ConnectionContext::Create(webrtc::PeerConnectionFactoryDependencies*) (dependencies=0x7ffff22f6d20)
    at webrtc/pc/connection_context.cc:78
#2  webrtc::PeerConnectionFactory::Create(webrtc::PeerConnectionFactoryDependencies) (dependencies=...)
    at webrtc/pc/peer_connection_factory.cc:86
#3  webrtc::CreateModularPeerConnectionFactory(webrtc::PeerConnectionFactoryDependencies) (dependencies=...)
    at webrtc/pc/peer_connection_factory.cc:72
#4  webrtc::CreatePeerConnectionFactory(rtc::Thread*, rtc::Thread*, rtc::Thread*, rtc::scoped_refptr<webrtc::AudioDeviceModule>, rtc::scoped_refptr<webrtc::AudioEncoderFactory>, rtc::scoped_refptr<webrtc::AudioDecoderFactory>, std::unique_ptr<webrtc::VideoEncoderFactory, std::default_delete<webrtc::VideoEncoderFactory> >, std::unique_ptr<webrtc::VideoDecoderFactory, std::default_delete<webrtc::VideoDecoderFactory> >, rtc::scoped_refptr<webrtc::AudioMixer>, rtc::scoped_refptr<webrtc::AudioProcessing>, webrtc::AudioFrameProcessor*)
    (network_thread=0x0, worker_thread=0x612000001840, signaling_thread=0x0, default_adm=..., audio_encoder_factory=..., audio_decoder_factory=..., video_encoder_factory=std::unique_ptr<webrtc::VideoEncoderFactory> = {...}, video_decoder_factory=
    std::unique_ptr<webrtc::VideoDecoderFactory> = {...}, audio_mixer=..., audio_processing=..., audio_frame_processor=0x0)
    at webrtc/api/create_peerconnection_factory.cc:70
```

WebRTC 在 `ConnectionContext` 对象的创建过程中，会创建 `cricket::ChannelManager`，在创建 `cricket::ChannelManager` 对象时，会初始化 `cricket::WebRtcVoiceEngine`，此时会从音频编解码器工厂中获取支持的编码器和解码器：
```
#0  cricket::WebRtcVoiceEngine::Init() (this=0x61300001ff40) at webrtc/media/engine/webrtc_voice_engine.cc:342
#1  cricket::CompositeMediaEngine::Init() (this=0x603000003250) at webrtc/media/base/media_engine.cc:172
#2  cricket::ChannelManager::Create(std::unique_ptr<cricket::MediaEngineInterface, std::default_delete<cricket::MediaEngineInterface> >, bool, rtc::Thread*, rtc::Thread*) (media_engine=std::unique_ptr<cricket::MediaEngineInterface> = {...}, enable_rtx=true, worker_thread=
    0x612000001840, network_thread=0x612000029a40) at webrtc/pc/channel_manager.cc:39
#3  webrtc::ConnectionContext::ConnectionContext(webrtc::PeerConnectionFactoryDependencies*)::$_2::operator()() const (this=0x7ffff3601380)
    at webrtc/pc/connection_context.cc:132
```

`cricket::WebRtcVoiceEngine::Init()` 的实现中，从音频编解码器工厂获取支持的音频编码器和解码器的代码如下（`webrtc/media/engine/webrtc_voice_engine.cc`）：
```
void WebRtcVoiceEngine::Init() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::Init";

  // TaskQueue expects to be created/destroyed on the same thread.
  low_priority_worker_queue_.reset(
      new rtc::TaskQueue(task_queue_factory_->CreateTaskQueue(
          "rtc-low-prio", webrtc::TaskQueueFactory::Priority::LOW)));

  // Load our audio codec lists.
  RTC_LOG(LS_VERBOSE) << "Supported send codecs in order of preference:";
  send_codecs_ = CollectCodecs(encoder_factory_->GetSupportedEncoders());
  for (const AudioCodec& codec : send_codecs_) {
    RTC_LOG(LS_VERBOSE) << ToString(codec);
  }

  RTC_LOG(LS_VERBOSE) << "Supported recv codecs in order of preference:";
  recv_codecs_ = CollectCodecs(decoder_factory_->GetSupportedDecoders());
  for (const AudioCodec& codec : recv_codecs_) {
    RTC_LOG(LS_VERBOSE) << ToString(codec);
  }
```

音频编码器工厂从各个具体的编码器实现类中获得该编码器实现支持的编码器的描述，如 `webrtc/api/audio_codecs/audio_encoder_factory_template.h` 中音频编码器工厂的实现：
```
template <typename T, typename... Ts>
struct Helper<T, Ts...> {
  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {
    T::AppendSupportedEncoders(specs);
    Helper<Ts...>::AppendSupportedEncoders(specs);
  }
. . . . . .
template <typename... Ts>
class AudioEncoderFactoryT : public AudioEncoderFactory {
 public:
  std::vector<AudioCodecSpec> GetSupportedEncoders() override {
    std::vector<AudioCodecSpec> specs;
    Helper<Ts...>::AppendSupportedEncoders(&specs);
    return specs;
  }
```

具体来说，WebRTC 的默认音频编码器工厂通过各个具体的编码器实现类的静态成员函数 `AppendSupportedEncoders(specs)` 获得该编码器支持的编码格式和参数。

如 OPUS 音频编码器，返回自身支持的编码格式描述的过程如下：
```
#0  webrtc::AudioEncoderOpusImpl::AppendSupportedEncoders(std::vector<webrtc::AudioCodecSpec, std::allocator<webrtc::AudioCodecSpec> >*) (specs=0x7ffff2fff6c0)
    at webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.cc:209
#1  webrtc::AudioEncoderOpus::AppendSupportedEncoders(std::vector<webrtc::AudioCodecSpec, std::allocator<webrtc::AudioCodecSpec> >*)
    (specs=0x7ffff2fff6c0) at webrtc/api/audio_codecs/opus/audio_encoder_opus.cc:24
#2  webrtc::audio_encoder_factory_template_impl::Helper<webrtc::AudioEncoderOpus, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioEncoderMultiChannelOpus>, webrtc::AudioEncoderIsacFloat, webrtc::AudioEncoderG722, webrtc::AudioEncoderG711, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioEncoderL16> >::AppendSupportedEncoders(std::vector<webrtc::AudioCodecSpec, std::allocator<webrtc::AudioCodecSpec> >*) (specs=0x7ffff2fff6c0)
    at webrtc/api/audio_codecs/audio_encoder_factory_template.h:49
#3  webrtc::audio_encoder_factory_template_impl::AudioEncoderFactoryT<webrtc::AudioEncoderOpus, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioEncoderMultiChannelOpus>, webrtc::AudioEncoderIsacFloat, webrtc::AudioEncoderG722, webrtc::AudioEncoderG711, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioEncoderL16> >::GetSupportedEncoders() (this=0x602000002030) at webrtc/api/audio_codecs/audio_encoder_factory_template.h:82
#4  cricket::WebRtcVoiceEngine::Init() (this=0x61300001ff40) at webrtc/media/engine/webrtc_voice_engine.cc:352
#5  cricket::CompositeMediaEngine::Init() (this=0x603000003250) at webrtc/media/base/media_engine.cc:172
```

OPUS 音频编码器支持的编码格式描述的详细内容可以参考如下这段代码 (`webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.cc`)：
```
void AudioEncoderOpusImpl::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  const SdpAudioFormat fmt = {"opus",
                              kRtpTimestampRateHz,
                              2,
                              {{"minptime", "10"}, {"useinbandfec", "1"}}};
  const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
  specs->push_back({fmt, info});
}

AudioCodecInfo AudioEncoderOpusImpl::QueryAudioEncoder(
    const AudioEncoderOpusConfig& config) {
  RTC_DCHECK(config.IsOk());
  AudioCodecInfo info(config.sample_rate_hz, config.num_channels,
                      *config.bitrate_bps,
                      AudioEncoderOpusConfig::kMinBitrateBps,
                      AudioEncoderOpusConfig::kMaxBitrateBps);
  info.allow_comfort_noise = false;
  info.supports_network_adaption = true;
  return info;
}
. . . . . .
absl::optional<AudioEncoderOpusConfig> AudioEncoderOpusImpl::SdpToConfig(
    const SdpAudioFormat& format) {
  if (!absl::EqualsIgnoreCase(format.name, "opus") ||
      format.clockrate_hz != kRtpTimestampRateHz || format.num_channels != 2) {
    return absl::nullopt;
  }

  AudioEncoderOpusConfig config;
  config.num_channels = GetChannelCount(format);
  config.frame_size_ms = GetFrameSizeMs(format);
  config.max_playback_rate_hz = GetMaxPlaybackRate(format);
  config.fec_enabled = (GetFormatParameter(format, "useinbandfec") == "1");
  config.dtx_enabled = (GetFormatParameter(format, "usedtx") == "1");
  config.cbr_enabled = (GetFormatParameter(format, "cbr") == "1");
  config.bitrate_bps =
      CalculateBitrate(config.max_playback_rate_hz, config.num_channels,
                       GetFormatParameter(format, "maxaveragebitrate"));
  config.application = config.num_channels == 1
                           ? AudioEncoderOpusConfig::ApplicationMode::kVoip
                           : AudioEncoderOpusConfig::ApplicationMode::kAudio;

  constexpr int kMinANAFrameLength = kANASupportedFrameLengths[0];
  constexpr int kMaxANAFrameLength =
      kANASupportedFrameLengths[arraysize(kANASupportedFrameLengths) - 1];

  // For now, minptime and maxptime are only used with ANA. If ptime is outside
  // of this range, it will get adjusted once ANA takes hold. Ideally, we'd know
  // if ANA was to be used when setting up the config, and adjust accordingly.
  const int min_frame_length_ms =
      GetFormatParameter<int>(format, "minptime").value_or(kMinANAFrameLength);
  const int max_frame_length_ms =
      GetFormatParameter<int>(format, "maxptime").value_or(kMaxANAFrameLength);

  FindSupportedFrameLengths(min_frame_length_ms, max_frame_length_ms,
                            &config.supported_frame_lengths_ms);
  if (!config.IsOk()) {
    RTC_DCHECK_NOTREACHED();
    return absl::nullopt;
  }
  return config;
}
```

音频编码格式描述的详细信息包括支持的音频 PCM 数据的采样率、通道数，编码的码率范围和支持的编码音频帧的时长范围，以及更直接用于弱网对抗的带内 FEC 开关和 CBR 开关等。

音频解码器工厂从各个解码器实现中获得该解码器支持解码的格式的详细描述，如 `webrtc/api/audio_codecs/audio_decoder_factory_template.h` 中音频解码器工厂的实现：
```
template <typename T, typename... Ts>
struct Helper<T, Ts...> {
  static void AppendSupportedDecoders(std::vector<AudioCodecSpec>* specs) {
    T::AppendSupportedDecoders(specs);
    Helper<Ts...>::AppendSupportedDecoders(specs);
  }
. . . . . .
template <typename... Ts>
class AudioDecoderFactoryT : public AudioDecoderFactory {
 public:
  std::vector<AudioCodecSpec> GetSupportedDecoders() override {
    std::vector<AudioCodecSpec> specs;
    Helper<Ts...>::AppendSupportedDecoders(&specs);
    return specs;
  }
```

与编码器工厂类似，WebRTC 的默认音频解码器工厂通过各个具体的解码器实现类的静态成员函数 `AppendSupportedDecoders(specs)` 获得该解码器支持的解码格式和参数。

如 OPUS 音频解码器返回自身支持的音频编解码格式的描述的过程如下：
```
#0  webrtc::AudioDecoderOpus::AppendSupportedDecoders(std::vector<webrtc::AudioCodecSpec, std::allocator<webrtc::AudioCodecSpec> >*) (specs=0x7ffff2fff900)
    at webrtc/api/audio_codecs/opus/audio_decoder_opus.cc:66
#1  webrtc::audio_decoder_factory_template_impl::Helper<webrtc::AudioDecoderOpus, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioDecoderMultiChannelOpus>, webrtc::AudioDecoderIsacFloat, webrtc::AudioDecoderG722, webrtc::AudioDecoderG711, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioDecoderL16> >::AppendSupportedDecoders(std::vector<webrtc::AudioCodecSpec, std::allocator<webrtc::AudioCodecSpec> >*) (specs=0x7ffff2fff900)
    at webrtc/api/audio_codecs/audio_decoder_factory_template.h:45
#2  webrtc::audio_decoder_factory_template_impl::AudioDecoderFactoryT<webrtc::AudioDecoderOpus, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioDecoderMultiChannelOpus>, webrtc::AudioDecoderIsacFloat, webrtc::AudioDecoderG722, webrtc::AudioDecoderG711, webrtc::(anonymous namespace)::NotAdvertised<webrtc::AudioDecoderL16> >::GetSupportedDecoders() (this=0x602000002050) at webrtc/api/audio_codecs/audio_decoder_factory_template.h:70
#3  cricket::WebRtcVoiceEngine::Init() (this=0x61300001ff40) at webrtc/media/engine/webrtc_voice_engine.cc:358
#4  cricket::CompositeMediaEngine::Init() (this=0x603000003250) at webrtc/media/base/media_engine.cc:172
```

OPUS 音频解码器支持的音频编解码格式的描述的详细内容可以参考如下这段代码 (`webrtc/api/audio_codecs/opus/audio_decoder_opus.cc`)：
```
void AudioDecoderOpus::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  AudioCodecInfo opus_info{48000, 1, 64000, 6000, 510000};
  opus_info.allow_comfort_noise = false;
  opus_info.supports_network_adaption = true;
  SdpAudioFormat opus_format(
      {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}});
  specs->push_back({std::move(opus_format), opus_info});
}
```

这里的描述信息与编码器的描述信息对应。***WebRTC 的这段代码有点奇怪，`AudioCodecInfo` 的描述和 `SdpAudioFormat` 描述的通道数竟然是不一样的。***

如在上面的 `cricket::WebRtcVoiceEngine::Init()` 的代码中看到的，`cricket::WebRtcVoiceEngine::Init()` 从编码器工厂和解码器工厂中获得了支持的编码器描述和解码器描述之后，通过 `cricket::WebRtcVoiceEngine` 的 `CollectCodecs()` 函数将这些音频编解码器描述与 payload type 建立关联，完善媒体参数，建立适当的音频 codec 的描述 `AudioCodec`，并保存起来。

Payload type 是媒体数据收发双方对于所收发的媒体数据的媒体格式信息的协议和约定。收发双方约定用 payload type 这样一个整数来表示一组媒体参数，如用 111 表示采样率为 48kHz，通道数为 2，编码帧时长为 10ms 这样一组参数。Payload type 后续在编码数据传输时由 RTP 包携带，以帮助接收解码端选择适当的解码器。`WebRtcVoiceEngine::CollectCodecs()` 函数的实现如下：
```
std::vector<AudioCodec> WebRtcVoiceEngine::CollectCodecs(
    const std::vector<webrtc::AudioCodecSpec>& specs) const {
  PayloadTypeMapper mapper;
  std::vector<AudioCodec> out;

  // Only generate CN payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_cn = {
      {8000, false}, {16000, false}, {32000, false}};
  // Only generate telephone-event payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_dtmf = {
      {8000, false}, {16000, false}, {32000, false}, {48000, false}};

  auto map_format = [&mapper](const webrtc::SdpAudioFormat& format,
                              std::vector<AudioCodec>* out) {
    absl::optional<AudioCodec> opt_codec = mapper.ToAudioCodec(format);
    if (opt_codec) {
      if (out) {
        out->push_back(*opt_codec);
      }
    } else {
      RTC_LOG(LS_ERROR) << "Unable to assign payload type to format: "
                        << rtc::ToString(format);
    }

    return opt_codec;
  };

  for (const auto& spec : specs) {
    // We need to do some extra stuff before adding the main codecs to out.
    absl::optional<AudioCodec> opt_codec = map_format(spec.format, nullptr);
    if (opt_codec) {
      AudioCodec& codec = *opt_codec;
      if (spec.info.supports_network_adaption) {
        codec.AddFeedbackParam(
            FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
      }

      if (spec.info.allow_comfort_noise) {
        // Generate a CN entry if the decoder allows it and we support the
        // clockrate.
        auto cn = generate_cn.find(spec.format.clockrate_hz);
        if (cn != generate_cn.end()) {
          cn->second = true;
        }
      }

      // Generate a telephone-event entry if we support the clockrate.
      auto dtmf = generate_dtmf.find(spec.format.clockrate_hz);
      if (dtmf != generate_dtmf.end()) {
        dtmf->second = true;
      }

      out.push_back(codec);

      if (codec.name == kOpusCodecName && audio_red_for_opus_enabled_) {
        std::string redFmtp =
            rtc::ToString(codec.id) + "/" + rtc::ToString(codec.id);
        map_format({kRedCodecName, 48000, 2, {{"", redFmtp}}}, &out);
      }
    }
  }

  // Add CN codecs after "proper" audio codecs.
  for (const auto& cn : generate_cn) {
    if (cn.second) {
      map_format({kCnCodecName, cn.first, 1}, &out);
    }
  }

  // Add telephone-event codecs last.
  for (const auto& dtmf : generate_dtmf) {
    if (dtmf.second) {
      map_format({kDtmfCodecName, dtmf.first, 1}, &out);
    }
  }

  return out;
}
```

`WebRtcVoiceEngine::CollectCodecs()` 收集所有的编解码器信息，对于编解码器工厂支持的编解码器配置，它通过 `cricket::PayloadTypeMapper` 将 SDP 音频格式映射为小整数形式的 payload type，并将编解码器配置和其对应的 payload type 一起保存起来；如果编解码器工厂返回的编解码器配置支持生成舒适噪声，则会包含对应采样率的生成舒适噪声的编解码器配置及其对应的 payload type；如果编解码器工厂返回的编解码器配置支持生成 DTMF，则会包含对应采样率的生成 DTMF 的编解码器配置及其对应的 payload type；如果开启了 RED 冗余，则对于 OPUS 会包含对应的 RED 编解码器配置及其对应的 payload type。这样最终获得的支持的编解码器配置，是从编解码器工厂获取的支持的编解码器配置的超集，它包括如下几部分：

 * 从编解码器工厂获取的支持的编解码器配置；
 * 舒适噪声生成 CNG 编解码器配置；
 * DTMP 生成编解码器配置；
 * RED 编解码器配置。

如果特定编解码器支持网络适配，`WebRtcVoiceEngine::CollectCodecs()` 还会为它添加支持 RTCP transport CC 反馈的信息。

`PeerConnection` 在创建及初始化的过程中，会创建 `SdpOfferAnswerHandler`/`WebRtcSessionDescriptionFactory`/`MediaSessionDescriptionFactory`，`MediaSessionDescriptionFactory` 在创建时通过 `cricket::ChannelManager` 从 `WebRtcVoiceEngine` 获得音频 codec 及其参数的描述：
```
#0  cricket::WebRtcVoiceEngine::send_codecs() const (this=0x61300001ff40) at webrtc/media/engine/webrtc_voice_engine.cc:651
#1  cricket::ChannelManager::GetSupportedAudioSendCodecs(std::vector<cricket::AudioCodec, std::allocator<cricket::AudioCodec> >*) const
    (this=0x607000004460, codecs=0x6150000210c8) at webrtc/pc/channel_manager.cc:68
#2  cricket::MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(cricket::ChannelManager*, cricket::TransportDescriptionFactory const*, rtc::UniqueRandomIdGenerator*) (this=0x6150000210c0, channel_manager=0x607000004460, transport_desc_factory=0x6150000210b0, ssrc_generator=0x617000020560)
    at webrtc/pc/media_session.cc:1540
#3  webrtc::WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(rtc::Thread*, cricket::ChannelManager*, webrtc::SdpStateProvider const*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, bool, std::unique_ptr<rtc::RTCCertificateGeneratorInterface, std::default_delete<rtc::RTCCertificateGeneratorInterface> >, rtc::scoped_refptr<rtc::RTCCertificate> const&, rtc::UniqueRandomIdGenerator*, std::function<void (rtc::scoped_refptr<rtc::RTCCertificate> const&)>)Python Exception <class 'gdb.error'> There is no member named _M_p.: 
    (this=0x615000021000, signaling_thread=0x6120000016c0, channel_manager=0x607000004460, sdp_info=0x617000020300, session_id=, dtls_enabled=true, cert_generator=
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> = {...}, certificate=..., ssrc_generator=0x617000020560, on_certificate_ready=...)
    at webrtc/pc/webrtc_session_description_factory.cc:139
#5  webrtc::SdpOfferAnswerHandler::Initialize(webrtc::PeerConnectionInterface::RTCConfiguration const&, webrtc::PeerConnectionDependencies&)
    (this=0x617000020300, configuration=..., dependencies=...) at webrtc/pc/sdp_offer_answer.cc:1008
#6  webrtc::SdpOfferAnswerHandler::Create(webrtc::PeerConnection*, webrtc::PeerConnectionInterface::RTCConfiguration const&, webrtc::PeerConnectionDependencies&) (pc=0x61a000000c80, configuration=..., dependencies=...) at webrtc/pc/sdp_offer_answer.cc:973
#7  webrtc::PeerConnection::Initialize(webrtc::PeerConnectionInterface::RTCConfiguration const&, webrtc::PeerConnectionDependencies)
    (this=0x61a000000c80, configuration=..., dependencies=...) at webrtc/pc/peer_connection.cc:628
#8  webrtc::PeerConnection::Create(rtc::scoped_refptr<webrtc::ConnectionContext>, webrtc::PeerConnectionFactoryInterface::Options const&, std::unique_ptr<webrtc::RtcEventLog, std::default_delete<webrtc::RtcEventLog> >, std::unique_ptr<webrtc::Call, std::default_delete<webrtc::Call> >, webrtc::PeerConnectionInterface::RTCConfiguration const&, webrtc::PeerConnectionDependencies) (context=..., options=..., event_log=std::unique_ptr<webrtc::RtcEventLog> = {...}, call=
    std::unique_ptr<webrtc::Call> = {...}, configuration=..., dependencies=...) at webrtc/pc/peer_connection.cc:478
#9  webrtc::PeerConnectionFactory::CreatePeerConnectionOrError(webrtc::PeerConnectionInterface::RTCConfiguration const&, webrtc::PeerConnectionDependencies) (this=0x60b000005d10, configuration=..., dependencies=...) at webrtc/pc/peer_connection_factory.cc:249
```

在 `MediaSessionDescriptionFactory` 创建过程中还会进一步对获取的音频编码器和解码器信息做一些处理和分类：
```
MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    ChannelManager* channel_manager,
    const TransportDescriptionFactory* transport_desc_factory,
    rtc::UniqueRandomIdGenerator* ssrc_generator)
    : MediaSessionDescriptionFactory(transport_desc_factory, ssrc_generator) {
  channel_manager->GetSupportedAudioSendCodecs(&audio_send_codecs_);
  channel_manager->GetSupportedAudioReceiveCodecs(&audio_recv_codecs_);
  channel_manager->GetSupportedVideoSendCodecs(&video_send_codecs_);
  channel_manager->GetSupportedVideoReceiveCodecs(&video_recv_codecs_);
  ComputeAudioCodecsIntersectionAndUnion();
  ComputeVideoCodecsIntersectionAndUnion();
}
 . . . . . .
void MediaSessionDescriptionFactory::ComputeAudioCodecsIntersectionAndUnion() {
  audio_sendrecv_codecs_.clear();
  all_audio_codecs_.clear();
  // Compute the audio codecs union.
  for (const AudioCodec& send : audio_send_codecs_) {
    all_audio_codecs_.push_back(send);
    if (!FindMatchingCodec<AudioCodec>(audio_send_codecs_, audio_recv_codecs_,
                                       send, nullptr)) {
      // It doesn't make sense to have an RTX codec we support sending but not
      // receiving.
      RTC_DCHECK(!IsRtxCodec(send));
    }
  }
  for (const AudioCodec& recv : audio_recv_codecs_) {
    if (!FindMatchingCodec<AudioCodec>(audio_recv_codecs_, audio_send_codecs_,
                                       recv, nullptr)) {
      all_audio_codecs_.push_back(recv);
    }
  }
  // Use NegotiateCodecs to merge our codec lists, since the operation is
  // essentially the same. Put send_codecs as the offered_codecs, which is the
  // order we'd like to follow. The reasoning is that encoding is usually more
  // expensive than decoding, and prioritizing a codec in the send list probably
  // means it's a codec we can handle efficiently.
  NegotiateCodecs(audio_recv_codecs_, audio_send_codecs_,
                  &audio_sendrecv_codecs_, true);
}
```

通过对发送 codecs 和接收 codecs 求交集，获得发送接收 codecs `audio_sendrecv_codecs_`；通过对发送 codecs 和接收 codecs 求并集，获得所有 codecs `all_audio_codecs_`。

ICE 连接建立，创建 Offer 消息的时候，获得音频 codec 的信息，并构造 SDP 消息，如：
```
#0  cricket::MediaSessionDescriptionFactory::GetAudioCodecsForOffer(webrtc::RtpTransceiverDirection const&) const
    (this=0x6150000210c0, direction=@0x611000014ce8: webrtc::RtpTransceiverDirection::kSendRecv) at webrtc/pc/media_session.cc:1952
#1  cricket::MediaSessionDescriptionFactory::AddAudioContentForOffer(cricket::MediaDescriptionOptions const&, cricket::MediaSessionOptions const&, cricket::ContentInfo const*, cricket::SessionDescription const*, std::vector<webrtc::RtpExtension, std::allocator<webrtc::RtpExtension> > const&, std::vector<cricket::AudioCodec, std::allocator<cricket::AudioCodec> > const&, std::vector<cricket::StreamParams, std::allocator<cricket::StreamParams> >*, cricket::SessionDescription*, cricket::IceCredentialsIterator*) const (this=0x6150000210c0, media_description_options=..., session_options=..., current_content=
    0x0, current_description=0x0, audio_rtp_extensions=std::vector of length 4, capacity 4 = {...}, audio_codecs=std::vector of length 14, capacity 16 = {...}, current_streams=0x7ffff24b6bb0, desc=0x608000045120, ice_credentials=0x7ffff24b6af0) at webrtc/pc/media_session.cc:2260
#2  cricket::MediaSessionDescriptionFactory::CreateOffer(cricket::MediaSessionOptions const&, cricket::SessionDescription const*) const
    (this=0x6150000210c0, session_options=..., current_description=0x0) at webrtc/pc/media_session.cc:1661
#3  webrtc::WebRtcSessionDescriptionFactory::InternalCreateOffer(webrtc::CreateSessionDescriptionRequest) (this=0x615000021000, request=...)
    at webrtc/pc/webrtc_session_description_factory.cc:346
#4  webrtc::WebRtcSessionDescriptionFactory::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&, cricket::MediaSessionOptions const&) (this=0x615000021000, observer=0x6060000160a0, options=..., session_options=...)
    at webrtc/pc/webrtc_session_description_factory.cc:247
#5  webrtc::SdpOfferAnswerHandler::DoCreateOffer(webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&, rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserver>) (this=0x617000020300, options=..., observer=...) at webrtc/pc/sdp_offer_answer.cc:2028
#6  webrtc::SdpOfferAnswerHandler::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&)::$_3::operator()(std::function<void ()>) const (this=0x7ffff2196970, operations_chain_callback=...) at webrtc/pc/sdp_offer_answer.cc:1123
#7  rtc::rtc_operations_chain_internal::OperationWithFunctor<webrtc::SdpOfferAnswerHandler::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&)::$_3>::Run() (this=0x608000040420) at webrtc/rtc_base/operations_chain.h:71
#8  rtc::OperationsChain::ChainOperation<webrtc::SdpOfferAnswerHandler::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&)::$_3>(webrtc::SdpOfferAnswerHandler::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&)::$_3&&) (this=0x6110000106c0, functor=...) at webrtc/rtc_base/operations_chain.h:154
#9  webrtc::SdpOfferAnswerHandler::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&) (this=0x617000020300, observer=0x60b000000880, options=...) at webrtc/pc/sdp_offer_answer.cc:1106
#10 webrtc::PeerConnection::CreateOffer(webrtc::CreateSessionDescriptionObserver*, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions const&) (this=0x61a000000c80, observer=0x60b000000880, options=...) at webrtc/pc/peer_connection.cc:1331
```

视频和音频稍微有一点不一样。

视频的 codec 描述信息，在 `PeerConnection` 创建及初始化的过程中，创建 `SdpOfferAnswerHandler`/`WebRtcSessionDescriptionFactory`/`MediaSessionDescriptionFactory`，`MediaSessionDescriptionFactory` 在创建时通过 `cricket::ChannelManager` 从 `WebRtcVideoEngine` 获得视频 codec 及其参数的描述：
```
#0  cricket::WebRtcVideoEngine::send_codecs() const (this=0x6040000040d0) at webrtc/media/engine/webrtc_video_engine.cc:632
#1  cricket::ChannelManager::GetSupportedVideoSendCodecs(std::vector<cricket::VideoCodec, std::allocator<cricket::VideoCodec> >*) const
    (this=0x607000004460, codecs=0x615000021128) at webrtc/pc/channel_manager.cc:86
#2  cricket::MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(cricket::ChannelManager*, cricket::TransportDescriptionFactory const*, rtc::UniqueRandomIdGenerator*) (this=0x6150000210c0, channel_manager=0x607000004460, transport_desc_factory=0x6150000210b0, ssrc_generator=0x617000020560)
    at webrtc/pc/media_session.cc:1542
#3  webrtc::WebRtcSessionDescriptionFactory::WebRtcSessionDescriptionFactory(rtc::Thread*, cricket::ChannelManager*, webrtc::SdpStateProvider const*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, bool, std::unique_ptr<rtc::RTCCertificateGeneratorInterface, std::default_delete<rtc::RTCCertificateGeneratorInterface> >, rtc::scoped_refptr<rtc::RTCCertificate> const&, rtc::UniqueRandomIdGenerator*, std::function<void (rtc::scoped_refptr<rtc::RTCCertificate> const&)>)Python Exception <class 'gdb.error'> There is no member named _M_p.: 
    (this=0x615000021000, signaling_thread=0x6120000016c0, channel_manager=0x607000004460, sdp_info=0x617000020300, session_id=, dtls_enabled=true, cert_generator=std::unique_ptr<rtc::RTCCertificateGeneratorInterface> = {...}, certificate=..., ssrc_generator=0x617000020560, on_certificate_ready=...)
    at webrtc/pc/webrtc_session_description_factory.cc:139
```

视频的 codec 描述信息并不是预先构造的，而是在获取的时候实时构造：
```
void AddDefaultFeedbackParams(VideoCodec* codec,
                              const webrtc::WebRtcKeyValueConfig& trials) {
  // Don't add any feedback params for RED and ULPFEC.
  if (codec->name == kRedCodecName || codec->name == kUlpfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
  codec->AddFeedbackParam(
      FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
  // Don't add any more feedback params for FLEXFEC.
  if (codec->name == kFlexfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamCcm, kRtcpFbCcmParamFir));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kRtcpFbNackParamPli));
  if (codec->name == kVp8CodecName &&
      IsEnabled(trials, "WebRTC-RtcpLossNotification")) {
    codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamLntf, kParamValueEmpty));
  }
}

// Helper function to determine whether a codec should use the [35, 63] range.
// Should be used when adding new codecs (or variants).
bool IsCodecValidForLowerRange(const VideoCodec& codec) {
  if (absl::EqualsIgnoreCase(codec.name, kFlexfecCodecName) ||
      absl::EqualsIgnoreCase(codec.name, kAv1CodecName) ||
      absl::EqualsIgnoreCase(codec.name, kAv1xCodecName)) {
    return true;
  } else if (absl::EqualsIgnoreCase(codec.name, kH264CodecName)) {
    std::string profileLevelId;
    // H264 with YUV444.
    if (codec.GetParam(kH264FmtpProfileLevelId, &profileLevelId)) {
      return absl::StartsWithIgnoreCase(profileLevelId, "f400");
    }
  }
  return false;
}

// This function will assign dynamic payload types (in the range [96, 127]
// and then [35, 63]) to the input codecs, and also add ULPFEC, RED, FlexFEC,
// and associated RTX codecs for recognized codecs (VP8, VP9, H264, and RED).
// It will also add default feedback params to the codecs.
// is_decoder_factory is needed to keep track of the implict assumption that any
// H264 decoder also supports constrained base line profile.
// Also, is_decoder_factory is used to decide whether FlexFEC video format
// should be advertised as supported.
// TODO(kron): Perhaps it is better to move the implicit knowledge to the place
// where codecs are negotiated.
template <class T>
std::vector<VideoCodec> GetPayloadTypesAndDefaultCodecs(
    const T* factory,
    bool is_decoder_factory,
    const webrtc::WebRtcKeyValueConfig& trials) {
  if (!factory) {
    return {};
  }

  std::vector<webrtc::SdpVideoFormat> supported_formats =
      factory->GetSupportedFormats();
  if (is_decoder_factory) {
    AddH264ConstrainedBaselineProfileToSupportedFormats(&supported_formats);
  }

  if (supported_formats.empty())
    return std::vector<VideoCodec>();

  supported_formats.push_back(webrtc::SdpVideoFormat(kRedCodecName));
  supported_formats.push_back(webrtc::SdpVideoFormat(kUlpfecCodecName));

  // flexfec-03 is supported as
  // - receive codec unless WebRTC-FlexFEC-03-Advertised is disabled
  // - send codec if WebRTC-FlexFEC-03-Advertised is enabled
  if ((is_decoder_factory &&
       !IsDisabled(trials, "WebRTC-FlexFEC-03-Advertised")) ||
      (!is_decoder_factory &&
       IsEnabled(trials, "WebRTC-FlexFEC-03-Advertised"))) {
    webrtc::SdpVideoFormat flexfec_format(kFlexfecCodecName);
    // This value is currently arbitrarily set to 10 seconds. (The unit
    // is microseconds.) This parameter MUST be present in the SDP, but
    // we never use the actual value anywhere in our code however.
    // TODO(brandtr): Consider honouring this value in the sender and receiver.
    flexfec_format.parameters = {{kFlexfecFmtpRepairWindow, "10000000"}};
    supported_formats.push_back(flexfec_format);
  }

  // Due to interoperability issues with old Chrome/WebRTC versions that
  // ignore the [35, 63] range prefer the lower range for new codecs.
  static const int kFirstDynamicPayloadTypeLowerRange = 35;
  static const int kLastDynamicPayloadTypeLowerRange = 63;

  static const int kFirstDynamicPayloadTypeUpperRange = 96;
  static const int kLastDynamicPayloadTypeUpperRange = 127;
  int payload_type_upper = kFirstDynamicPayloadTypeUpperRange;
  int payload_type_lower = kFirstDynamicPayloadTypeLowerRange;

  std::vector<VideoCodec> output_codecs;
  for (const webrtc::SdpVideoFormat& format : supported_formats) {
    VideoCodec codec(format);
    bool isFecCodec = absl::EqualsIgnoreCase(codec.name, kUlpfecCodecName) ||
                      absl::EqualsIgnoreCase(codec.name, kFlexfecCodecName);

    // Check if we ran out of payload types.
    if (payload_type_lower > kLastDynamicPayloadTypeLowerRange) {
      // TODO(https://bugs.chromium.org/p/webrtc/issues/detail?id=12248):
      // return an error.
      RTC_LOG(LS_ERROR) << "Out of dynamic payload types [35,63] after "
                           "fallback from [96, 127], skipping the rest.";
      RTC_DCHECK_EQ(payload_type_upper, kLastDynamicPayloadTypeUpperRange);
      break;
    }

    // Lower range gets used for "new" codecs or when running out of payload
    // types in the upper range.
    if (IsCodecValidForLowerRange(codec) ||
        payload_type_upper >= kLastDynamicPayloadTypeUpperRange) {
      codec.id = payload_type_lower++;
    } else {
      codec.id = payload_type_upper++;
    }
    AddDefaultFeedbackParams(&codec, trials);
    output_codecs.push_back(codec);

    // Add associated RTX codec for non-FEC codecs.
    if (!isFecCodec) {
      // Check if we ran out of payload types.
      if (payload_type_lower > kLastDynamicPayloadTypeLowerRange) {
        // TODO(https://bugs.chromium.org/p/webrtc/issues/detail?id=12248):
        // return an error.
        RTC_LOG(LS_ERROR) << "Out of dynamic payload types [35,63] after "
                             "fallback from [96, 127], skipping the rest.";
        RTC_DCHECK_EQ(payload_type_upper, kLastDynamicPayloadTypeUpperRange);
        break;
      }
      if (IsCodecValidForLowerRange(codec) ||
          payload_type_upper >= kLastDynamicPayloadTypeUpperRange) {
        output_codecs.push_back(
            VideoCodec::CreateRtxCodec(payload_type_lower++, codec.id));
      } else {
        output_codecs.push_back(
            VideoCodec::CreateRtxCodec(payload_type_upper++, codec.id));
      }
    }
  }
  return output_codecs;
}
 . . . . . .
std::vector<VideoCodec> WebRtcVideoEngine::send_codecs() const {
  return GetPayloadTypesAndDefaultCodecs(encoder_factory_.get(),
                                         /*is_decoder_factory=*/false, trials_);
}

std::vector<VideoCodec> WebRtcVideoEngine::recv_codecs() const {
  return GetPayloadTypesAndDefaultCodecs(decoder_factory_.get(),
                                         /*is_decoder_factory=*/true, trials_);
}
```

与构造音频编解码器描述信息的过程类似，构造视频的 codec 描述时，同样会首先从编码器或者解码器工厂中获得支持的编解码器的信息；然后根据需要为它们添加用于弱网对抗的反馈参数，如 RTCP NACK 反馈参数等；此外还会添加一些弱网对抗的 codec，如 RED，FLEXFEC、ULPFEC 等。

视频的编码器工厂和解码器工厂，同样是在创建 `PeerConnectionFactoryInterface` 的时候，由 webrtc 的用户传入。创建内建的视频编码器工厂的 `webrtc::CreateBuiltinVideoEncoderFactory()` 函数实现 (`webrtc/api/video_codecs/builtin_video_encoder_factory.cc`) 如下：
```
namespace webrtc {

namespace {

// This class wraps the internal factory and adds simulcast.
class BuiltinVideoEncoderFactory : public VideoEncoderFactory {
 public:
  BuiltinVideoEncoderFactory()
      : internal_encoder_factory_(new InternalEncoderFactory()) {}

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    // Try creating internal encoder.
    std::unique_ptr<VideoEncoder> internal_encoder;
    if (format.IsCodecInList(
            internal_encoder_factory_->GetSupportedFormats())) {
      internal_encoder = std::make_unique<EncoderSimulcastProxy>(
          internal_encoder_factory_.get(), format);
    }

    return internal_encoder;
  }

  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return internal_encoder_factory_->GetSupportedFormats();
  }

 private:
  const std::unique_ptr<VideoEncoderFactory> internal_encoder_factory_;
};

}  // namespace

std::unique_ptr<VideoEncoderFactory> CreateBuiltinVideoEncoderFactory() {
  return std::make_unique<BuiltinVideoEncoderFactory>();
}

}  // namespace webrtc
```

创建内建的视频解码器工厂的 `webrtc::CreateBuiltinVideoDecoderFactory()` 函数实现 (`webrtc/api/video_codecs/builtin_video_decoder_factory.cc`) 如下：
```
namespace webrtc {

std::unique_ptr<VideoDecoderFactory> CreateBuiltinVideoDecoderFactory() {
  return std::make_unique<InternalDecoderFactory>();
}

}  // namespace webrtc
```

WebRTC 提供的视频编码器工厂和视频解码器工厂的实际实现分别为 `webrtc::InternalEncoderFactory` 和 `webrtc::InternalDecoderFactory`。

`cricket::WebRtcVideoEngine` 通过 `GetPayloadTypesAndDefaultCodecs()` 函数从这些工厂中获得支持的视频 codec 的描述，如对于视频编码器：
```
#0  webrtc::InternalEncoderFactory::GetSupportedFormats() const (this=0x602000002090) at webrtc/media/engine/internal_encoder_factory.cc:41
#1  webrtc::(anonymous namespace)::BuiltinVideoEncoderFactory::GetSupportedFormats() const (this=0x602000002070)
    at webrtc/api/video_codecs/builtin_video_encoder_factory.cc:49
#2  cricket::(anonymous namespace)::GetPayloadTypesAndDefaultCodecs<webrtc::VideoEncoderFactory>(webrtc::VideoEncoderFactory const*, bool, webrtc::WebRtcKeyValueConfig const&) (factory=0x602000002070, is_decoder_factory=false, trials=...) at webrtc/media/engine/webrtc_video_engine.cc:166
#3  cricket::WebRtcVideoEngine::send_codecs() const (this=0x6040000040d0) at webrtc/media/engine/webrtc_video_engine.cc:632
``` 

详细的支持的视频编码器的描述 (`webrtc/media/engine/internal_encoder_factory.cc`) 如下：
```
std::vector<SdpVideoFormat> InternalEncoderFactory::SupportedFormats() {
  std::vector<SdpVideoFormat> supported_codecs;
  supported_codecs.push_back(SdpVideoFormat(cricket::kVp8CodecName));
  for (const webrtc::SdpVideoFormat& format : webrtc::SupportedVP9Codecs())
    supported_codecs.push_back(format);
  for (const webrtc::SdpVideoFormat& format : webrtc::SupportedH264Codecs())
    supported_codecs.push_back(format);
  if (kIsLibaomAv1EncoderSupported)
    supported_codecs.push_back(SdpVideoFormat(cricket::kAv1CodecName));
  return supported_codecs;
}

std::vector<SdpVideoFormat> InternalEncoderFactory::GetSupportedFormats()
    const {
  return SupportedFormats();
}
```

对于支持的视频解码器的描述的获取，过程则如下面所示：
```
#0  webrtc::InternalDecoderFactory::GetSupportedFormats() const (this=0x6020000020b0) at webrtc/media/engine/internal_decoder_factory.cc:46
#1  cricket::(anonymous namespace)::GetPayloadTypesAndDefaultCodecs<webrtc::VideoDecoderFactory>(webrtc::VideoDecoderFactory const*, bool, webrtc::WebRtcKeyValueConfig const&) (factory=0x6020000020b0, is_decoder_factory=true, trials=...) at webrtc/media/engine/webrtc_video_engine.cc:166
#2  cricket::WebRtcVideoEngine::recv_codecs() const (this=0x6040000040d0) at webrtc/media/engine/webrtc_video_engine.cc:637
```

详细的支持的视频解码器的描述 (`webrtc/media/engine/internal_decoder_factory.cc`) 如下：
```
std::vector<SdpVideoFormat> InternalDecoderFactory::GetSupportedFormats()
    const {
  std::vector<SdpVideoFormat> formats;
  formats.push_back(SdpVideoFormat(cricket::kVp8CodecName));
  for (const SdpVideoFormat& format : SupportedVP9DecoderCodecs())
    formats.push_back(format);
  for (const SdpVideoFormat& h264_format : SupportedH264Codecs())
    formats.push_back(h264_format);

  if (kIsLibaomAv1DecoderSupported ||
      (kDav1dIsIncluded && field_trial::IsEnabled(kDav1dFieldTrial))) {
    formats.push_back(SdpVideoFormat(cricket::kAv1CodecName));
  }

  return formats;
}
```

通过分析 WebRTC 中收集音视频编解码能力的过程，我们大致可以了解在 WebRTC 中添加音视频编解码器，修改音视频编解码器的配置，添加或删除编解码器弱网对抗反馈参数等的过程。

本文的分析中，含有一些函数调用栈的信息，函数调用栈的信息中甚至包含了代码所在的源文件及行号。这里的分析基于 **[OpenRTCClient](https://github.com/hanpfei/OpenRTCClient)** 中的 WebRTC M98 的源码进行。

Done。
