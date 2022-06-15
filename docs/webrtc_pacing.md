---
title: WebRTC 的平滑发送
date: 2022-06-014 21:05:49
categories: 音视频开发
tags:
- 音视频开发
---

# 平滑的发送

Paced sender，也常常被称为 “pacer”，它是 WebRTC RTP 栈的一部分，主要用于平缓发送到网络的数据包流。
<!--more-->
## 背景

考虑一个码率为 5Mbps 帧率为 30fps 的视频流。理想情况下，这个视频流的每个帧的大小大约 21kB，每个帧被打包成大约 18 个 RTP 数据包。虽然说每秒中滑动窗口的平均比特率是正确的 5Mbps，但在更短的时间尺度上，它可以被视为每隔 33 毫秒 167 Mbps 的突发传输，然后是 32 毫秒的静默期。此外，相当常见的是，在突然移动的情况下，尤其是在处理屏幕共享时，视频编码器会超出目标帧大小。比理想大小大 10 倍甚至 100 倍的帧是一个非常真实的场景。这些数据包突发可能会导致一些问题，例如网络拥塞和缓冲区膨胀，甚至是数据包丢失。大多数会话都有多个媒体流，比如一个视频轨道和一个音频轨道。如果你一次将一个视频帧发送到网络上，并且这些数据包需要 100 毫秒到达对端 —— 这意味着你现在也阻塞了任何音频数据包及时到达远端。

Paced sender 通过使用一个缓冲区来解决这个问题，媒体数据包先在这个缓冲区里排队，然后使用一个 _漏桶_ 算法将它们平缓地发送到网络上。缓冲区中包含所有媒体轨道的单独的 fifo 流，以便实现，比如音频可以优先于视频 - 并且可以以循环方式发送优先级相等的流，以避免任何一个流阻塞其它流。

由于 pacer 控制在网络上发送的比特率，因此它还用于在需要最小发送速率的情况下生成填充 - 如果使用比特率探测，则生成数据包序列。

## 数据包的生命周期

当使用 paced sender 时，媒体数据包的典型路径看起来就像这样：

1.  `RTPSenderVideo` 或 `RTPSenderAudio` 将媒体数据打包成 RTP 数据包。

2.  数据包被发送给 [RTPSender] 类进行传输。

3.  pacer 通过 [RtpPacketSender] 接口被调用以将数据包批量入队。

4.  数据包被放进 pacer 内的队列中，等待适当的时机发送它们。

5.  在计算好的时间，pacer 调用 `PacingController::PacketSender()` 回调方法，通常由 [PacketRouter] 类实现。

6.  router 基于数据包的 SSRC 将数据包转发到正确的 RTP 模块，并在其中的 `RTPSenderEgress` 类中打上最后的时间戳，可能会保存它以进行重传等。

7.  数据包被发送到底层的 `Transport` 接口，之后它现在超出了范围。

与此异步进行的是确定估计的可用发送带宽 - 并通过 `void SetPacingRates(DataRate pacing_rate, DataRate padding_rate)` 方法对 `RtpPacketPacker` 设置目标发送速率。

## 数据包优先级

pacer 基于两个标准对数据包进行优先级排序：

*   数据包类型，优先级从高到低如下：
    1.  音频
    2.  重传
    3.  视频和 FEC
    4.  填充

*   入队的顺序

入队顺序是在每个流 (SSRC) 的基础上执行的。给定相同的优先级，[RoundRobinPacketQueue] 在媒体流之间交替，以确保没有流不必要地阻塞其它流。

## 实现

当前 paced sender 有两个实现（尽管它们通过 `PacingController` 类共享大量的逻辑）。 传统的 [PacedSender] 使用专门的线程以 5ms 的间隔轮询 pacing 控制器，并具有保护内部状态的锁。顾名思义，更新的 [TaskQueuePacedSender] 使用 [TaskQueue] 来保护状态，并调度数据包处理，后者动态地基于实际的发送速率和约束。避免在新的应用中使用传统的 PacedSender，我们已经在计划移除它了。

## 数据包路由器

一个称为 [PacketRouter] 的相邻组件用于路由从 pacer 出来的数据包，并进入正确的 RTP 模块。它具有以下功能：

*   `SendPacket` 方法查找具有对应于数据包的 SSRC 的 RTP 模块，以进一步路由到网络。。
*   如果使用了发送端带宽估计，它会填充传输范围内的序列号扩展。
*   生成填充。支持基于负载的填充的模块被优先考虑，最后一个发送媒体的模块始终是第一选择。
*   发送媒体之后返回任何生成的 FEC。
*   转发 REMB 和/或 TransportFeedback 消息给适当的 RTP 模块。

目前 FEC 是基于每个 SSRC 生成的，因此总是在发送媒体后从 RTP 模块返回。希望有一天，我们将支持使用单个 FlexFEC 流覆盖多个流 - 则数据包路由器是这种 FEC 生成器可能存在的地方。它甚至可以用于 FEC 填充，作为 RTX 的替代方案。

## API

这一节概述了与 pacer 的几个不同用例相关的类和方法

### 数据包发送

要发送数据包，可以使用 `RtpPacketSender::EnqueuePackets(std::vector<std::unique_ptr<RtpPacketToSend>> packets)`。pacer 接收一个 `PacingController::PacketSender` 对象作为构造函数的参数，当需要实际发送数据包时使用这个回调。

### 发送速率

要控制发送速率，则使用 `void SetPacingRates(DataRate pacing_rate, DataRate padding_rate)`。如果数据包队列变为空，且发送速率掉到 `padding_rate` 以下，则 pacer 将从 `PacketRouter` 请求填充包。

要完全挂起/恢复发送数据（比如，由于网络可用性），则使用 `Pause()` 和 `Resume()` 方法。

在某些情况下，指定的 pacing 速率可能会被覆盖，例如由于极端的编码器过冲。使用 `void SetQueueTimeLimit(TimeDelta limit)` 来指定你希望数据包在 pacer 的队列中等待的最长时间（暂停除外）。实际发送速率可能会增加到超过 pacing_rate，以尝试使 _平均_ 排队时间小于请求的限制。这样做的理由是，如果发送队列长于三秒，最好冒丢包的风险，然后尝试使用关键帧进行恢复，而不是造成严重的延迟。

### 带宽估计

如果带宽估计器支持带宽探测，它可能会请求以指定速率发送一组数据包，以判断这是否会导致网络延迟/丢失增加。使用 `void CreateProbeCluster(DataRate bitrate, int cluster_id)` 方法 - 通过这个 `PacketRouter` 发送的数据包将在附加的 `PacedPacketInfo` 结构中用相应的 cluster_id 进行标记。

如果使用拥塞窗口 pushback，则可以使用 `SetCongestionWindow()` 和 `UpdateOutstandingData()` 更新状态。

还有一些方法可以帮我们控制如何 pace：

*   `SetAccountForAudioPackets()` 确定音频数据包是否计入带宽消耗。
*   `SetIncludeOverhead()` 确定是否把完整 RTP 数据包大小计入带宽使用（否则只计算媒体载荷）。
*   `SetTransportOverhead()` 设置每个数据包消耗的额外数据大小，表示比如 UDP/IP 头部。

### 统计数据

有几种方法用于在 pacer 状态中收集统计信息：

*   `OldestPacketWaitTime()`，自添加进队列中的最早的数据包被添加进队列以来的时间。
*   `QueueSizeData()`，当前在队列中的总字节数。
*   `FirstSentPacketTime()`，发送第一个数据包的绝对时间。
*   `ExpectedQueueTime()`，队列中的总字节数除以发送速率。

[RTPSender](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/rtp_rtcp/source/rtp_sender.h)
[RtpPacketSender](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/rtp_rtcp/include/rtp_packet_sender.h)
[RtpPacketPacer](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/rtp_packet_pacer.h)
[PacketRouter](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/packet_router.h)
[PacedSender](https://source.chromium.org/chromium/chromium/src/+/main:media/cast/net/pacing/paced_sender.h)
[TaskQueuePacedSender](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/task_queue_paced_sender.h)
[RoundRobinPacketQueue](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/pacing/round_robin_packet_queue.h)

[原文](webrtc/modules/pacing/g3doc/index.md)
