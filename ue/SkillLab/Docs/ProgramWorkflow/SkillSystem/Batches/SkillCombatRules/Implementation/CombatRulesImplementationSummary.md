# SkillCombatRules 实现与验证摘要

## 交付范围

实现以 [策划需求](../../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md) 第 8 节已确认规则为准。共享机制落在 `SkillLabSkills` Runtime、Cast 生命周期与 `SkillLabGAS` 伤害入口；没有新增具体闪避技能、Stunned/Silenced Buff/GameplayEffect、UI 或表现体验。

- `ESkillLabOperationCategory` 区分技能、闪避、移动、用户取消和受击类别。普通请求沿用原 `SubmitSkillRequest`；通用闪避操作先终止当前非 Recovery Cast，再以请求中的技能定义继续完整 Runtime/GAS 激活门控。正式激活成功创建新 Cast；资源、状态、目标或定义门控失败时不恢复旧 Cast。项目没有闪避定义/Ability 内容，故自动化以现有 `Skill_Mend` 作为通用操作夹具，不表示它是闪避技能。
- GAS 即时伤害 API 可携带 `SkillLab.Event.Attack.Normal` 或 `SkillLab.Event.Attack.Heavy` 分类 Tag。只有造成实际生命损失的即时伤害才向目标 Runtime 通知；普通分类继续，重击取消当前 Cast，空/未知分类可观察记录并继续。没有按伤害量推断类别。现有技能效果伤害调用未声明攻击类别；当前没有生产战斗攻击调用者标记普通/重击，因此自动化通过正式 GAS 伤害 API 显式传 Tag 验证接线。
- Runtime 激活门控直接检查 ASC 有效 `State.SkillLab.Stunned` / `State.SkillLab.Silenced`；已激活 Cast 监听两 Tag 的有效计数变化并取消。未发现生产状态授予者；自动化的 loose Tag 只验证状态消费，不代表生产状态内容。
- 移动和用户取消有明确非打断 Runtime 通知，不改变 Cast；地面选点仍由输入组件本地管理，Escape 继续仅清除预览。现有移动输入无需请求 Runtime 仲裁，因此没有改输入绑定或移动流程。
- Recovery 期间技能请求进入单槽 latest-wins 缓存且不激活。Cast 自然完成时，Runtime 先结束 Ability、清空旧 Cast 身份，再通过下一帧 Timer 消费缓存；消费时清除缓存请求的旧 RequestId，并重新调用正式 Runtime/GAS 请求入口，因此仍经过定义、状态、目标、资源和冷却门控。门控失败报告真实拒绝原因并清槽；取消、死亡、Reset 和 EndPlay 清除缓存。自动化覆盖自然完成成功启动新 Cast、旧 Ability/Notify 隔离、新 RequestId/CastId，以及资源不足拒绝与清槽。
- 费用/冷却保持现有释放提交时发生；提交前取消不扣费，提交后取消不退款。Notify/Ability 中断沿现有生命周期取消路径，不自动重放效果。`FSkillLabCastLifecycle` 增加每 Cast 提交点身份幂等；释放占独立点，效果点以目标身份区分，重复点只拒绝，不终止 Cast。

## 修改文件

- `Source/SkillLabSkills/Public/SkillLabSkillTypes.h`：通用操作类别与输入意图类型。
- `Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h`、`Private/SkillLabSkillRuntimeComponent.cpp`：操作请求、状态/受击仲裁、缓存及清理、提交点接入。
- `Source/SkillLabSkills/Public/SkillLabCastLifecycle.h`、`Private/SkillLabCastLifecycle.cpp`：Cast 内提交点幂等身份。
- `Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h`、`Private/SkillLabAbilitySystemComponent.cpp`、`Public/SkillLabGASTags.h`、`Private/SkillLabGASTags.cpp`：显式攻击类别 Tag 与权威即时伤害受击事件。
- `Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp`、`SkillLabCastLifecycleTests.cpp`：Runtime/GAS 仲裁矩阵与纯生命周期幂等断言。
- 本专题策划/程序 README、需求验收矩阵、架构交接和实现准备文档同步为已确认规则/已实现状态；仅因 GAS 受击事件入口和 Skills Runtime 状态/仲裁入口新增，更新 `ProjectContext/Modules/SkillLabGAS.md` 与 `SkillLabSkillsRuntime.md`；未改全局架构地图、项目共享基线、SkillChainRefactor 历史或 UE 资产。

## 验证

构建命令：

```powershell
& 'D:\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' SkillLabEditor Win64 Development 'E:\agent\game\ue\SkillLab\SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=1 -NoUBA *> 'E:\agent\game\ue\SkillLab\Saved\Logs\SkillCombatRulesCacheFixVerifiedBuild.log'
```

结果：`Succeeded`。本次在修正后的当前工作树重建 UE 5.8 `SkillLabEditor Win64 Development`，新增 latent 自动化源码已编译并链接；日志：`Saved/Logs/SkillCombatRulesCacheFixVerifiedBuild.log`。使用单动作/无 UBA 参数以限制构建并行资源。

定向 Runtime/GAS 自动化命令：

```powershell
& 'D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\agent\game\ue\SkillLab\SkillLab.uproject' -nullrhi -unattended -nop4 -nosplash -NoSound -ExecCmds='Automation RunTests SkillLab.Skills.CastGameplayEvent;Quit' -TestExit='Automation Test Queue Empty' -abslog='E:\agent\game\ue\SkillLab\Saved\Logs\SkillCombatRulesCacheFixCastGameplayEvent-Verified.log'
```

`SkillLab.Skills.CastGameplayEvent`：`Result={Success}`，退出码 0；结果日志：`Saved/Logs/SkillCombatRulesCacheFixCastGameplayEvent-Verified.log`，命令启动日志：`Saved/Logs/SkillCombatRulesCacheFixCastGameplayEvent-VerifiedCommand.log`。日志显示自然完成后先以 `ActiveCast=0`、`AbilityValid=false` 消费 `Skill_BufferedSuccess`，再启动新 Cast (`RequestId=19`、`CastId=11`)；Recovery 中未提前启动，缓存 RequestId 未复用，旧 Cast Notify 被拒绝。独立失败夹具 `Skill_BufferedFail` 经正式资源门控返回 `InsufficientEnergy` 并清除槽。其余覆盖普通/未知类别继续、重击中断、提交后不退款、Stunned/Silenced 门控/中断、闪避正式激活和资源拒绝、移动/用户取消继续、latest-wins 替换，以及取消/死亡/Reset 清理。

回归分别使用 UnrealEditor-Cmd 执行 `Automation RunTests SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`、`SkillLab.GAS.DamageCalculation`；三项均 `Result={Success}`、退出码 0。新日志分别为 `Saved/Logs/SkillCombatRulesCacheFix-CastLifecycle-Verified.log`、`Saved/Logs/SkillCombatRulesCacheFix-DefinitionValidation-Verified.log`、`Saved/Logs/SkillCombatRulesCacheFix-DamageCalculation-Verified.log`。Lifecycle 的提交点单次/不同身份/新 Cast 重用身份测试通过。

`git diff --check` 通过。完整自动化日志含既有自动 SDK 探测对未安装平台的 `Condition failed` 噪声；上述目标用例均明确成功。

## 未验证与交接

- 没有执行可见 PIE/视口验收；NullRHI 只验证自动化与运行代码，不替代玩家观察。
- Notify 超时依赖 World Timer 的自动取消路径未单独在本专题自动化中等待到期；GameplayEvent 身份、Runtime 显式取消、迟到 Notify 拒绝及不产生额外效果路径受 CastGameplayEvent 测试覆盖。提交后中断不退款已通过重击中断路径验证；提交前取消不扣费通过提交前能量断言验证。
- 没有多来源 Stunned/Silenced GameplayEffect 可验证；未实现或声称状态来源所有权/叠层生产行为。
- 玩家输入没有闪避内容来源可接线。正式 GAS 受击 API 已能携带类别 Tag，但战斗内容调用者需在自身权威攻击定义处显式选择 Tag，不能按伤害数值推测。
- 没有生产攻击调用者选择普通/重击类别 Tag，也没有 Stunned/Silenced 生产授予者；当前自动化证明权威伤害 API 与 Runtime 对显式 Tag 的机制消费，不代表生产内容端到端接线。
- 本摘要记录实现方源码和当前自动化证据，不替代独立 Reviewer 对修复后实现及新日志的复查。
