# 终态历史与未决记录交接

## 范围与依据

处理主审 F5。先读 `Docs/Baseline/SkillSystem/02-RequestLifecycle.md`、`05-OwnershipAndCleanup.md` 与 `Docs/Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md` 第 8.1、8.2 节；状态专项 SL-02 给出证据和已有查询场景。原事务唯一结算、迟到确认、跨 Runtime 重建和独立交付必须保留；这不是把所有历史当成重复数据删除。

## 运行链与改动边界

Runtime Submission 创建生产 operation 和 Energy transaction；`FSkillLabSubmissionResults` 保存父子结果，ASC EnergyReservations 保存结算及已用 ID；未决组件跨 Runtime 留存，逐 Tick 对账并按身份投递反馈，Projectile 的 delivery 也持有 production handle。首读 `SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeUnconfirmed.cpp`、`SkillLabSubmissionReconciliation.cpp`、`SkillLabUnconfirmedSubmissions.*`、`SkillLabSubmissionResults.*`、`SkillLabAbilitySystemEnergyReservations.cpp`、`SkillLabEnergySettlement.cpp` 及 ASC 头；按需读 `SkillLabFireballProjectile.cpp`、`SkillLabSkillRuntimeConfirmation.cpp` 与对应测试。

实现前逐类列出活动引用和回收条件：未完成生产/Area 子项、已交付但仍可能查询的 Projectile、待对账资源、待反馈及重入期间不可回收；已闭合历史与幂等身份各自确定最小保留表示和查询契约。`Records`、`PendingFeedback`、`ReportedLostOwnerRecords` 目前以数组下标关联且回调可重入；迁移成稳定身份或等价安全机制后才能压缩，不能直接 RemoveAt。`UsedEnergyTransactionIds` 还接受外部指定 ID，不能只凭自增生成器删除防重；`AllocatedEnergyTransactionIds` 是不同用途。禁止凭时间、固定数量或 Reset 猜测迟到事件已经消失。若现有公开查询承诺与有界回收无法同时成立，给 PM 具体调用方、冲突与最小接口选项；可独立证明的活动扫描收敛先完成，不能宣称全部历史有界。

## 验证与依赖

复用 `SkillLab.GAS.EnergyLedger`、`EnergySettlementHistory`、`SkillLab.Skills.ProductionRecords`、`SubmissionReconciliation`、`UnconfirmedSubmission`、`LateSubmission`、`AreaSubmissionSettlement`、`SubmissionFeedbackRouting`、`SubmissionFeedbackIdentity`、`SubmittedActionLifetime`。补长循环正常和未决收敛后大小/扫描证据，以及未决期间、重复确认、迟到反馈、重建、Area 子项和投射物交付的身份/只扣一次回归。运行方式见 `Docs/ProjectContext/Modules/TestAndVerification.md`。与效果任务共享 ASC 头、与字段整理共享未决/GAS 文件，分支或同工作区按文件串行；先合入 F1/F3 的共享文件改动。查询和保留语义如需改变，交 PM 复核直接调用者。
