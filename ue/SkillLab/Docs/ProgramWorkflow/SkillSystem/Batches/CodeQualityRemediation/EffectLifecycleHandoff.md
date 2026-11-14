# 持续效果定义及事件风险交接

## 范围与依据

处理主审 F4，定向核实周期 Spec 关联、Reset 移除载荷、源 Actor/ASC 销毁边界。业务依据是 `Docs/Baseline/SkillSystem/04-EffectsAndStates.md`、`05-OwnershipAndCleanup.md` 和 `Docs/Planning/SkillSystem/Batches/SkillChainRefactor/SkillInfrastructureRequirements.md`；问题定位见批次入口链接的主审及状态专项。持续效果独立于 Cast，事件表达真实结果；不新增效果叠层或观察迁移的产品规则。

## 运行链与落点

`SkillEffectExecutor` 经 GAS `EffectResolver` 调用源 ASC 的 `ApplyDamageOverTimeToTarget`，动态 GE 定义由源 ASC 的 `RuntimeEffects` 强引用，活跃 spec 在目标 ASC；技能侧 Adapter 以目标 ASC/handle 观察伤害和移除。定义需要覆盖真实活跃 spec 和同步回调寿命，不能以 Cast 结束或 Runtime EndPlay 为释放信号。核实 UE 5.8 活跃 spec 的引用、GC 和叠层语义后，选择实际可证明安全的回收或有界复用；不同 Period/Duration、来源、叠层和拒绝应用不能被无意合并。不要仅移除强引用或通过 Reset 清除所有效果。

首读 `Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp` 的周期回调及 DoT 应用、对应 public 头、`SkillLabCombatAttributeSet.cpp` 的周期记录；`Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp` 的 Track/HandleDamage/HandleRemoval/Clear 和 `SkillLabSkillRuntimeComponent.cpp` 的 EndPlay/Reset；按需读 `SkillLabEffectResolver.cpp`、`SkillLabSkillEffectExecutor.cpp`。建议落点以上 GAS 文件及必要的 Adapter；不改技能内容。F1/F3 并行涉及 Runtime，编辑该文件前与 PM 协调。

周期关联目前以 Spec 地址匹配，单项时有兜底。对同目标两个不同来源/数值周期效果及重入路径，比较 handle、来源、实际伤害与事件次数，结合引擎源码核实地址生命周期；证实错配才修复关联，不把推测写成故障。比较自然移除和 `Clear(true)` 的 actor 载荷与当前消费者：可恢复的弱引用应按实际契约处理，失效 Actor 不得强持有或伪造。`Clear(false)` 的观察解绑是已知边界，不要求迁移观察者；验证源 Runtime 结束后已交付效果继续，以及源 Actor/ASC 销毁后实际行为，记录证据和限制，有真实缺陷再处理。

## 验证与上下文

复用 `SkillLab.Skills.SubmittedActionLifetime`、GAS DamageCalculation、生产 Probe 和 `Docs/ProjectContext/Modules/TestAndVerification.md` 的构建运行入口；补定向覆盖反复应用/到期、失败应用、GC、并发不同参数及活跃中销毁/Reset，证明保留数量随活跃需求收敛且效果、来源、移除事实正确。GE 复用如改变叠层须显式验证原行为。结果写明源码版本、运行证据、未覆盖边界。此任务与历史任务均可能编辑 ASC 头，协调串行合并；不需等待测试拆分。
