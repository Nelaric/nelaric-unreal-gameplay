<!-- Copyright (c) 2026 Nelaric Contributors -->

# Demo 排级 AI：虚拟指挥与班级协同

排级在独立 Actor StateTree 中管理配置的多个班，不绑定排长身体或一个指挥班。
班级同样是逻辑对象，全部身体是普通士兵。伤亡和玩家控制只改变可用能力，
逻辑授权通过来源租约处理。直属排、班和人数不采用固定现实编制。
完整架构见[连级 AI](DemoCompany.zh-CN.md)。

## 所有者与接入

`TS_PlatoonCommand` 的 PuerTS Actor 代理及 `BP_DemoPlatoonCommand` 保存稳定 PlatoonId、
TeamId、Squads 和策略。每排配置 1 至 64 个班；所有登记班都是必要参与者，
普通能力不足或失联不静默移除一个班。

`PlatoonCoordinator.ts` 持有任务、分配、准备状态和许可。Actor 配置一个原生
`UDemoCompanyMembershipComponent` 和一个 `UGameAIStateTreeComponent`。
`ST_DemoPlatoonCommander` 使用 Game AI Schema，关闭 Require Possession，没有 Pawn 或 AIController。
每个状态一个 `TS_PlatoonPhase` 主 Task，通过显式转换推进：

```text
AwaitMission → BuildPlan → PublishAssignments → MonitorPlan → FinalizeMission → AwaitMission
```

MonitorPlan 长期 Running。有限任务完成后保留精确身份和结果；持续任务保持 Executing。
退出 Task 不撤销所有班的任务和长期职责。

## 任务、准备与许可

| 排级类型 | 班级语义 | 结果与持续行为 |
| --- | --- | --- |
| Move | Move | 必要班的有限结果汇总 |
| SecureArea | Control | 持续建立区域控制，阶段成果由规则确认 |
| Defend | Defend | 持续履职，可保持 Executing + Ready |
| Search | Search | 按配置子区推进，上传精确任务的覆盖证据 |
| Withdraw | Withdraw | 完成授权撤离任务 |
| Regroup | Regroup | 完成集结，恢复能力另按报告检查 |

排安排各班的目标子区与协同，继续使用已有班级路径与位置服务。
连级只读取排报告；占领、护送和当前区域控制由规则系统判定。
阶段成果确认与持续守备释放分开。

班级 ClaimMissionSource、SetMissionFromSource、CancelMissionFromSource 和 ReleaseMissionSource
检查同世界、阵营、精确来源、归属代次与任务版本。SetExecutionPermitFromSource
检查精确任务与递增 GateVersion，许可可以撤回。旧取消不能撤销新任务。

排通过 Membership 向连登记，CompanyAssignment 候选只在授权范围内准备，不立即覆盖正式任务。
仍承担旧持续职责的排不会自行离岗准备。接受与 Ready 分开，准备条件成立后才激活。
许可失效后各班受控保持，新的有效许可允许恢复。长期任务和许可不归 Task 的 ExitState 所有。

班报告 ObservedAt 采用最旧必要成员时间；排汇总继续采用最旧必要班时间。
转发和读取不刷新时间。ReportSequence 与 TaskSequence 独立，相同能力采样仍可处理新终态。
旧任务、旧 Ready、旧归属与旧世界回调不能解锁当前任务。
局部失败只重试失败班，默认最多两次，仍受原任务总期限约束；无法满足任务时上报连级。
玩家身体从自动执行能力中扣除，可继续计入规则允许的占领人数。
没有按首个士兵指定长官、专门长官席位或实体继任流程。

## 入口、恢复与诊断

代理提供 StartCommander、SubmitMission、MoveToArea、RegroupAt、DefendArea、SecureArea、
SearchArea、WithdrawToArea、CancelMission、GetSituationReport、GetDebugStatus 和 StopCommander。
已有连级来源时，直接入口不能覆盖任务；玩家命令使用连级共同约束入口。
Duration 为零允许长期等待，正值不超过 3600 秒，使用暂停感知的世界时间。
bool 仅表示接受，执行结果与 Ready 从报告取得。

存档保存逻辑任务、班级任务身份、几何、剩余期限与必要身体的稳定身份及符号绑定。
Actor 引用、委托、树句柄和临时路径请求不入档。恢复先验证语义与绑定，再对账原任务；
必要的底层执行恢复单独推进，语义相同的任务不整套重发。

独立 Content 仓库提供
`/Game/Demo/Demo1_GrandWarfront/AI/Platoon/BP_DemoPlatoonCommand` 和同目录的
`ST_DemoPlatoonCommander`。恢复匹配版本后，在编辑器中验证 Blueprint 与 StateTree 编译。
集成行为通过有上限的 PIE 场景验证；纯数据测试为 `Scripts/test_company.cjs`。

排心跳默认约 1 Hz，最大报告年龄 3 秒。关键变化立即通知，普通报告低频合并，
反馈不通过同步递归重规划。班级 Context 记录 PlanCancellationCount 和 RouteRequestCount，
供分析连级命令引发的下层工作。资产、类型、原生、运行与性能是不同证据；
实际通路容量、碰撞拥挤和最终关卡帧率需要结合目标场景验证。
