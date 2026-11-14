# 终态历史与未决记录整改摘要

## 交付状态

F5 已完成可证明安全的未决活动扫描收敛。原始 Review 未修改。`Records`、生产结果和能源结算历史仍保留查询契约；未采用 TTL、固定容量、Reset 清空或猜测迟到事件已消失的方式裁剪。

## 实现

- `USkillLabUnconfirmedSubmissions` 新增 `ActiveRecordIndices`，仅包含仍待资源/生产结算或仍待反馈的记录。`Records` 保持追加式，索引不移动，`GetRecords()` 和回调关联仍使用原索引。
- Tick 对账先复制活动索引快照，再逐个重查成员资格和记录有效性。同步回调重入追加的新记录留到下一轮；已完成回执但尚未成功送达反馈的记录继续留在活动集，反馈成功后才移出。
- `BlocksSkill`、`HasUnconfirmed` 和丢失资源诊断只遍历活动索引。原 `ReportedLostOwnerRecords` 历史索引集合改为每条记录的诊断标记，加一个单调会话诊断位，保留 `HasLostResourceOwnerDiagnosis()` 的历史查询语义。
- 未裁剪 `FSkillLabSubmissionResults::Results/Children`、ASC 的 `EnergySettlementHistory` 或 `UsedEnergyTransactionIds`。`AllocatedEnergyTransactionIds` 仍仅表示已分配未 Begin 的临时 ID；正常 Allocate→Begin 会移除。
- `Record()` 的重复身份检查仍扫描保留历史；这是未决交接写路径，不在逐帧对账中。本轮没有另存一份等量无界身份索引来优化低频去重。

## 验证

实际工程 `E:/agent/game/ue/SkillLab/SkillLab.uproject` 使用 UE 5.8 构建 `SkillLabEditor Win64 Development` 成功。运行测试的模块加载路径为实际工程 `Binaries/Win64/UnrealEditor-SkillLabGAS.dll` 与 `UnrealEditor-SkillLabSkills.dll`。

复现命令：

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -unattended -nop4 -NullRHI -nosplash -nosound '-ExecCmds=Automation RunTests SkillLab.Skills.SubmissionReconciliation+SkillLab.Skills.ProductionRecords+SkillLab.Skills.AreaSubmissionSettlement+SkillLab.Skills.LateSubmission+SkillLab.Skills.UnconfirmedSubmission+SkillLab.Skills.SubmissionFeedbackRouting+SkillLab.Skills.SubmissionFeedbackIdentity+SkillLab.Skills.SubmittedActionLifetime+SkillLab.GAS.EnergySettlementHistory+SkillLab.GAS.ResourceExecutionBoundary' '-TestExit=Automation Test Queue Empty' '-abslog=E:/agent/game/ue/SkillLab/Saved/Logs/HistoryLifecycleValidation2.log'
```

定向自动化日志：[`HistoryLifecycleValidation2.log`](../../../../../Saved/Logs/HistoryLifecycleValidation2.log)。10 项全部 `Success`，Automation Queue Empty，退出状态 0：

- `SkillLab.Skills.SubmissionReconciliation`：新增 128 条已闭合历史及 128 条未决记录的规模/活动集测试；验证重入追加延迟到下一轮、闭合后活动索引归零且历史仍可查询。
- `ProductionRecords`、`AreaSubmissionSettlement`：生产结果、Area 父子聚合与未决状态。
- `LateSubmission`、`UnconfirmedSubmission`：迟到结果、Reset/Runtime 重建后的隔离与收敛。
- `SubmissionFeedbackRouting`、`SubmissionFeedbackIdentity`：反馈接收方变化、旧身份迟到回调与新 Runtime 身份隔离。
- `SubmittedActionLifetime`：已交付 Projectile 的生产身份及命中/销毁不改写原交付回执。
- `SkillLab.GAS.EnergySettlementHistory`、`ResourceExecutionBoundary`：结算查询幂等、关闭历史、写入边界。

规模测试直接通过测试友元填充 128 条已确认记录和 128 条未决记录，不是 128 次真实施法的性能基准；实测未决项清空后 `Records.Num()==256`、活动索引数为 0。未测量帧耗时或内存。日志启动阶段有 13 条通用 `Condition failed` 和其他引擎测试输出，但所有被选自动化均报告 `Success`，进程退出为 0。

## 保留边界与待定接口

终态生产/资源记录目前没有安全回收条件：

- `FSkillLabSubmissionResults::Query(Key)` 仍供 `SkillLabSkillEffectExecutor.cpp`、`SkillLabSkillRuntimeExecution.cpp`、`SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeUnconfirmed.cpp` 和 `SkillLabSubmittedEffect.cpp` 查询。已交付动作可由 Projectile 延后命中；`PlanArea` 的父结果仍需 `Children` 做聚合。公开查询接口没有消费者释放/租约终止信号，因此无法证明某个旧 Operation 在任意固定窗口后再无迟到查询。
- `QueryEnergySettlement(TransactionId)` 被未决对账用于闭合资源事实，`BindEnergyOperation` 也要核对保留快照。公开 `BeginEnergyEscrow(TransactionId, ...)` 接受调用方指定 ID；裁掉 `UsedEnergyTransactionIds` 会允许历史 ID 在闭合后重用，破坏幂等。Allocator 自增不能约束外部传入 ID。

若要求上述账本有界，需 PM 决定并补齐契约：为生产句柄增加明确的消费者释放/租约生命周期（包含 Projectile 与 Area 子项），或改变查询承诺并规定调用方必须在何时完成最后查询；能源 ID 则需限制为 ASC 独占生成，或增加明确的历史终止/会话边界。契约落地前应继续保留这些历史。当前接口与有限历史无法同时证明安全，F5 不擅自改变其语义。

独立只读 Review 未发现阻断问题；Review 核对了活动索引移除时点、反馈未送达重试、回调重入快照、稳定数组下标及诊断语义。建议 PM 后续重点决定结果/能源账本的消费者终止契约；低频 `Record()` 去重扫描是否需要额外索引取决于实际规模证据。
