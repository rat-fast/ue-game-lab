# 冗余状态整理交接

## 范围与落点

处理主审 F6 和状态专项 SL-04；不改变共享业务规则，基线入口为 `Docs/Baseline/SkillSystem/README.md`。逐符号重新搜索当前工作树，确认声明、写入、反射、序列化及测试用途，再整理 `PendingSubmissionId`、`bEffectSubmitted`、`ActiveEffectResult`、未决 `RuntimeId`、资源 `bAwaitingResourceNotification`、reservation 的 `RuntimeGeneration/CastId`。保留 `FSkillLabEnergySettlement` 中真实使用的身份字段。

`ActiveEffectResults` 只为操作名计数，建议改为仅成功效果递增的标量，保持失败、重复目标和多个提交点时原有 `SkillEffect_<Cast>_<index>` 语义；`SuccessfulPointEffects` 是按点/目标复用结果的有效去重索引，不得删除。首读 `SkillLabCastExecution.h`、`SkillLabSkillRuntimeExecution.cpp` 的效果应用、`SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeUnconfirmed.cpp`、`SkillLabUnconfirmedSubmissions.h`、`SkillLabAbilitySystemComponent.h`、`SkillLabAbilitySystemResource.cpp`、`SkillLabAbilitySystemEnergyReservations.cpp`；按需查所有调用者和测试。

## 验证与依赖

沿 `SkillLab.Skills.MultipleSubmissions`、`CastSubmissionIsolation`、`ProductionRecords`、`UnconfirmedSubmission` 和 `SkillLab.GAS.EnergyLedger`/`ResourceWriteIntegration` 回归成功/失败/重复效果及资源身份。测试入口见 `Docs/ProjectContext/Modules/TestAndVerification.md`。此任务与 F1/F3 的 `RuntimeExecution.cpp` 及历史任务的未决/GAS 文件重叠，应待这些改动合入后进行；不必为每个删除成员增加镜像测试。

`RuntimeExecution.cpp` 约 555 行：本次只需首读效果应用及直接调用链；Notify、阶段清理与效果应用共享 Cast 身份、重入和提交状态，单纯移动函数尚不能证明减少安全阅读范围，暂不要求拆分。若实施发现能独立验证且显著缩小上下文的边界，再提具体方案；不得复制共享状态。
