# Loadout Ability 归属整改 Review

## 范围与依据

独立核对本批 F7，审查范围为 `AbilityOwnershipHandoff.md`、`AbilityOwnershipSummary.md`、技能系统基线 `Docs/Baseline/SkillSystem/05-OwnershipAndCleanup.md` 及相关请求/效果边界、`ProgramArchitectureContract.md`、实际源码和交付验证日志。程序 Review 规则来自 `.codex/skills/ue-project-context/SKILL.md`、`.codex/skills/ai-native-programming/SKILL.md`、`.codex/skills/ai-program-review/SKILL.md`。

## 结论

实现落点和归属策略符合当前约定：Loadout 只撤销自身记录的 Ability spec handle；Runtime EndPlay 在提交收束、Cast 取消及效果观察解绑后触发撤销；普通 Reset 不触发 Loadout 撤销；独立 Projectile 和持续 GE 沿原生命周期继续。没有发现已证实的业务行为或实现错误。

**Review 通过，原 P2 验证缺口已关闭。** 更新后的 AbilityOwnership 用例在同一 Actor/ASC 上连续两轮于 Ability 与 GAS spec 均活跃时替换 Runtime，并核对旧实例结束、旧精确 handle 撤销及同类外部授予保留。新源码哈希与构建、测试证据对应；本次按要求只读复查，没有重跑构建或测试。

## P2 复查

- 更新后的 `SkillLabAbilityOwnershipTests.cpp` 使用 `completionEventTag` 让正式 Cast 保持等待，并以 `TStrongObjectPtr` 保留旧 Ability。替换前分别断言技能 Ability、GAS Ability 和精确 Runtime spec active，且 Ability 的 spec handle 是 Runtime 授予而非同类外部授予。
- 连续两轮 `RebuildRuntime` 后，测试断言旧 Ability 的技能/GAS 生命周期均结束、旧 Runtime Cast 集合为空、旧 Runtime handle 消失、同类外部 handle 保留，且新 Runtime 只增加一个与旧/外部均不同的新 handle。随后再次销毁旧 Runtime 并确认新/外部 spec 仍在。
- 测试仍经既有正式请求入口和真实组件替换路径执行；未改生产实现。断言直接观察强引用 Ability 和 GAS spec 状态，不以 UObject 弱引用失效代替生命周期结束。
- 实际工程构建日志记录新增自动化编译、Skills DLL 链接及 `Result: Succeeded`。新增测试源码 SHA-256 为 `7C97C7FFBB3F357DF1841B097C44E62B6B047351778B511A756A56D297C25922`，Skills DLL 为 `130B3326321834AA1F0E112F9D409C56B15274EEF46D40429A5C0E5369B2BF5A`，GAS DLL 为 `0A468362A5C401B489607D62DDB3FBD805831F1508B27DC2BA1C3EB74F41E7B2`；本次只读核对所得哈希与摘要所列一致。
- 本次测试日志的命令行匹配四项目标回归；`AbilityOwnership`、`CastExecutionOwnership`、`CastRequestReentry`、`SubmittedActionLifetime` 均报告 `Success`，队列完成 4 项，退出状态为 0。新增 active replacement 断言随 AbilityOwnership 用例执行通过。

本轮未复跑验证。启动日志仍有 13 条既有 `Condition failed`、GameplayCue 搜索路径回退及 World cleanup 告警；目标测试自身无失败。仍未运行可见 PIE、多人/预测或全量自动化，这些不属于本 P2 的关闭条件。

## 已核对事实

- `USkillLabSkillLoadout::RevokeGrantedAbilities` 遍历仅由本 Loadout 记录的有效 handle 调用 `ClearAbility`，随后清空映射和弱 ASC；不按 Ability class 搜索，也不调用 `ClearAllAbilities`。
- `FinishEndPlayCleanup` 等待 `AnySubmitting()` 收束，调用 `CancelActiveCast`，解除效果观察并关闭执行器后再撤销 Loadout 授予。Reset 路径不调用 `RevokeGrantedAbilities`。持续 GE 由目标 ASC 持有，Runtime EndPlay 仍使用 `EffectEventAdapter->Clear(false)`。
- 原始 `AbilityOwnershipTargetedTests.log` 为补测前版本；活跃替换补测与最终回归证据以 `AbilityOwnershipActiveReplacementBuild.log`、`AbilityOwnershipActiveReplacementTests.log` 为准。摘要报告实际工程 UE 5.8 Development 构建成功；Review 已核对构建日志的编译/链接目标和结果，但未重跑。
- 补测日志启动阶段存在 13 条 `Condition failed`，另有 GameplayCue 路径回退及 World cleanup 告警；四项目标测试均报告 Success。未运行可见 PIE、多人/预测或全量自动化；这些不在本批验证范围内。
- `Docs/ProjectContext/Modules/SkillLabSkillsRuntime.md` 已同步 EndPlay 对 Ability spec 与持续 GE 观察/寿命的当前职责说明。

## 证据入口

- 交接与实现摘要：`AbilityOwnershipHandoff.md`、`AbilityOwnershipSummary.md`
- 业务边界：`Docs/Baseline/SkillSystem/05-OwnershipAndCleanup.md`；请求和效果边界见相邻 `02-RequestLifecycle.md`、`04-EffectsAndStates.md`
- 实现：`Source/SkillLabSkills/Private/SkillLabSkillLoadout.cpp`、`Source/SkillLabSkills/Public/SkillLabSkillLoadout.h`、`Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp`
- 测试与运行记录：`Source/SkillLabSkills/Private/Tests/SkillLabAbilityOwnershipTests.cpp`、`SkillLabCastExecutionTests.cpp`、`SkillLabCastRequestReentryTests.cpp`、`SkillLabSubmittedActionLifetimeTests.cpp`、`Saved/Logs/AbilityOwnershipActiveReplacementBuild.log`、`Saved/Logs/AbilityOwnershipActiveReplacementTests.log`。`AbilityOwnershipTargetedTests.log` 是补测前版本。
- 当前模块职责：`Docs/ProjectContext/Modules/SkillLabSkillsRuntime.md`
