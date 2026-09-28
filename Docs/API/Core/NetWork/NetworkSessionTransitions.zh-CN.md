<!-- Copyright (c) 2026 Nelaric -->

[English](NetworkSessionTransitions.md) | 简体中文

# 网络会话与权威转换

## 目标与边界

`UNelaricSessionTransitionSubsystem` 协调客户端从一个远程服务器转移至另一个服务器。Unreal 为每个 GameInstance 创建一个子系统实例；请求、取消和回调均在游戏线程执行。

`GetNetMode()` 返回当前 World 观测到的网络模式；没有 World 时返回空值。旅行期间，该值可能描述旧 World 或中间 World，转换完成由目标 World 和连接检查共同确认。

## 转换路径

| 起点 | 目标 | 执行方式 |
| --- | --- | --- |
| 客户端 | 客户端 | 向当前服务器请求离开，向目标服务器请求接纳；两端批准后调用 `ClientTravel`。 |

`RequestTransition()` 要求起点和目标网络模式均为 `NM_Client`。调用方提供 `Nelaric::FTransitionDestination`，其中包含两个 `FNetworkEndpoint`：`GameEndpoint` 用于旅行连接，`BeaconEndpoint` 用于目标服审批连接。每个端点要求地址非空、端口在 1 到 65535 之间，并能构成主机非空的有效 URL。

接受请求时返回非零 `FTransitionHandle`。已有活动请求、没有 World、起点或目标模式不符、端点无效或审批传输未绑定时，返回零句柄且不调用终态回调。每个子系统同时只处理一个请求。

## 执行前的权威确认

内部传输取得第一个本地 `ANelaricPlayerController`，通过其服务端 RPC 请求离开。服务器对非零请求 ID 和非空目标地址返回批准；所属客户端收到结果后，传输层先检查返回地址是否与请求地址一致，再报告批准。

传输层同时创建 `ANelaricTransitionBeaconClient`，连接调用方指定的 Beacon 端点。`ANelaricGameModeBase::StartPlay()` 在监听服务器或独立服务器上、监听端口有效时启动 Online Beacon Host。`TransitionBeaconListenPort` 默认为 15000，目标端点中的 Beacon 端口应与服务器配置一致。

协调器只接收旅行开始前、属于当前请求 ID 的回复。拒绝会以 `AuthorityRejected` 结束请求；无法启动审批或联系目标权威时报告 `AuthorityUnavailable`。两端批准后，协调器在下一次 Tick 检查截止时间并取得本地玩家控制器，再向游戏端点发起绝对 `ClientTravel`。

目标 GameMode 的 `CanAcceptTransition()` 返回 `true`。该方法中的 `TODO(NELARIC-TRANSITION-CAPACITY-INTEGRATION)` 标记了取得当前生效的 `WorldStartupConfig` 并执行 `MaxPlayers` 限制的位置，包括正在接入的玩家。

## 完成与取消

接受的请求具有 30 秒截止时间，覆盖审批和旅行。成功要求当前 World 不同于起点 World、网络模式为 `NM_Client`，且服务端连接已打开；连接主机与游戏端点忽略大小写匹配，端口完全一致。World 或连接不满足这些检查时，请求继续等待至截止时间。

`FTransitionCallbacks` 提供可选的 `OnSucceeded`、`OnCancelled`、`OnTimedOut` 和 `OnFailed` 委托。每个已接受请求只有一个终态，只执行对应的已绑定回调。协调器先清除活动请求并释放传输状态，再调用回调；传输清理会移除离开审批监听并销毁目标 Beacon。

`CancelTransition()` 只对旅行开始前的活动句柄成功，并报告取消。旅行开始后返回 `false`。清除审批传输会以 `AuthorityUnavailable` 结束活动请求；协调器反初始化时取消活动请求。

## World 启动配置

`UNelaricWorldStartupConfig` 保存地图软引用、世界玩家策略和活动参与者默认策略。`HasValidPlayerLimits()` 与 `HasValidActivityParticipantLimits()` 检查人数非负，以及正数上限不小于最少人数。`HasValidStartupConfig()` 还检查地图引用和策略枚举值。资产中的校验函数检查配置值，不加载地图或执行旅行。
