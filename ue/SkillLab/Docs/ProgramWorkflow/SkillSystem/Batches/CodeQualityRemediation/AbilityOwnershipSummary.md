# Loadout Ability 归属整改交付

## 状态

本批 F7 生产实现与定向验证已完成。独立 `AbilityOwnershipReview.md` 指出的活跃 Ability 替换验证缺口，已于 2026-11-14 补测并通过实际工程构建及四项定向回归，现交回原 Reviewer 复查；P2 是否关闭由独立复查确认。实现遵循 `AbilityOwnershipHandoff.md` 与 `Docs/Baseline/SkillSystem/05-OwnershipAndCleanup.md`；本次补测仅修改 `SkillLabAbilityOwnershipTests.cpp` 与本摘要，没有修改生产实现、原始 Review 或效果任务文件，未提交或推送。

## 实现

- `Source/SkillLabSkills/Public/SkillLabSkillLoadout.h`、`Private/SkillLabSkillLoadout.cpp`：增加幂等 `RevokeGrantedAbilities`。仅对 Loadout 在 `GiveAbility` 时记录的有效 handle 调用 ASC `ClearAbility`，随后清空 handle 映射和弱 ASC 引用；不按 Ability class 搜索、不清理其他系统授予的 spec，也不对 ASC 调用 `ClearAllAbilities`。
- `Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp`：在 `FinishEndPlayCleanup` 的活动 Cast 取消、提交收束、效果观察解除及执行器关闭之后撤销 Loadout 授予。Reset 不调用该清理，不改变普通 Reset 中的 Loadout。
- `Source/SkillLabSkills/Private/Tests/SkillLabAbilityOwnershipTests.cpp`：新增 `SkillLab.Skills.AbilityOwnership`，让外部系统与 Runtime 授予同一 Ability class，验证 Runtime 重复销毁后仅外部精确 handle 留存。Review 补测改为连续两轮正式请求保持活跃 Cast 后替换 Runtime：替换前核对 Ability 与 GAS spec 均活跃、Ability 使用 Runtime 精确 handle；替换后强引用旧 Ability 并断言其技能/GAS 生命周期均已结束、旧 Cast 集合为空、旧 handle 消失、外部 handle 保留、新 Runtime 只有一个不同的新 handle；重复旧 Runtime 清理不得移除新/外部 spec。新增断言已运行通过。
- `Docs/ProjectContext/Modules/SkillLabSkillsRuntime.md`：同步 EndPlay 的当前责任边界，区分 Ability spec 撤销与持续 GE 观察解除。

GAS 源码 `AbilitySystemComponent_Abilities.cpp` 的 `ClearAbility` 按传入 handle 定位；处于 Ability 列表锁时将该 handle 标记待移除，未锁时调用 `OnRemoveAbility` 并移除 spec。Runtime 清理先取消活动 Cast 并等待提交终止，再调用 Loadout 撤销，降低活动 Ability 与 GAS 移除重入风险。ASC 不可用或正在销毁时不调用其 API，Loadout 仍清除本地映射。

已交付 Projectile 与目标 ASC 上持续 GE 不属于 Loadout 的 Ability spec；EndPlay 保持 `EffectEventAdapter->Clear(false)`，没有改动其既有生命周期。

## 验证

- 实际工程 `E:/agent/game/ue/SkillLab/SkillLab.uproject`，UE 5.8，`SkillLabEditor Win64 Development` 构建成功，退出码 0。
- 首轮实际工程 NullRHI 自动化共 4 项成功、0 项失败：`SkillLab.Skills.AbilityOwnership`、`CastExecutionOwnership`、`CastRequestReentry`、`SubmittedActionLifetime`。这些是 Review 补测前版本的证据：既有执行所有权测试在 Notify 超时、Ability 已结束后替换 Runtime，不能证明活跃 Ability 替换；重入测试覆盖回调内 Runtime 销毁，但没有精确 handle 与旧 Ability 结束联合断言。寿命测试覆盖已交付 Projectile/GE 在 Cast 或 Runtime 生命周期结束后的行为。日志：`Saved/Logs/AbilityOwnershipTargetedTests.log`。此前摘要对活跃 Ability 替换覆盖的表述已纠正。
- 自动化启动日志存在既有 13 条 `Condition failed`、GameplayCue 路径回退及 World cleanup 告警；四项目标测试均报告 Success。未运行可见 PIE、多人/预测或全量自动化。

Review 补测静态核对：fixture 的 `RebuildRuntime` 先销毁旧组件再在同一 Actor/ASC 创建新组件；`completionEventTag` 使用既有注册标签，使 Lua 空函数通过正式 `HasLegalWait` 保持等待，不再触发 `ScriptNoFinish`；旧 Ability 由 `TStrongObjectPtr` 保留，断言直接检查实例而不以弱引用失效代替结束。所有请求与替换仍沿既有公共入口，未改生产实现或受保护文件。

2026-11-14 补测结果：

- PM 释放共享验证时段后，实际工程 `SkillLabEditor Win64 Development` 构建 Succeeded，退出码 0；独立日志 `Saved/Logs/AbilityOwnershipActiveReplacementBuild.log`，包含新增测试编译和 Skills DLL 链接。
- `Saved/Logs/AbilityOwnershipActiveReplacementTests.log`：上述四项均 Success，队列完成 4 项，进程退出码 0。新增活跃替换断言连续两轮通过；启动阶段仍有既有 13 条 `Condition failed` 与夹具告警，目标测试无失败。
- 本次测试源码 SHA-256：`7C97C7FFBB3F357DF1841B097C44E62B6B047351778B511A756A56D297C25922`；实际 Skills DLL：`130B3326321834AA1F0E112F9D409C56B15274EEF46D40429A5C0E5369B2BF5A`；GAS DLL：`0A468362A5C401B489607D62DDB3FBD805831F1508B27DC2BA1C3EB74F41E7B2`，保留效果 P2 修复后的 GAS 产物。哈希用于定位版本，不表示新增全量验收。

复现命令：

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -unattended -nop4 -NullRHI -nosplash -nosound '-ExecCmds=Automation RunTests SkillLab.Skills.AbilityOwnership+SkillLab.Skills.CastExecutionOwnership+SkillLab.Skills.CastRequestReentry+SkillLab.Skills.SubmittedActionLifetime' '-TestExit=Automation Test Queue Empty' '-abslog=E:/agent/game/ue/SkillLab/Saved/Logs/AbilityOwnershipActiveReplacementTests.log'
```
