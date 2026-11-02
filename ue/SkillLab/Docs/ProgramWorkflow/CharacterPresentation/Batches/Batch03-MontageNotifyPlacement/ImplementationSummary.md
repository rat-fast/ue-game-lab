# Batch03：Montage Notify 配置能力实现摘要

> 本文主体记录截至 2026-10-28 的程序实现和当时状态，后续四技能内容配置以 [配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)、[批次入口](README.md) 和 [Notify 架构交接](ArchitectureHandoff.md) 为准。历史结论不代表四技能仍未配置。

## 实际修改

- `Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`
  - 增加 `releaseTrigger` 解析，仅接受 `Timer` 或 `GameplayEvent`。
  - 增加 `releaseEventTag` 解析，并要求 Tag 已注册。
  - 增加标准字段 `releaseEventTimeout`。
  - `GameplayEvent` 必须配置合法事件 Tag 和正的 timeout；`Timer` 不能配置事件 Tag。
  - 未填写新字段时继续使用默认 `Timer` 和默认 timeout，保持旧配置兼容。
- `Source/SkillLabSkills/Private/Tests/SkillLabDefinitionProviderTests.cpp`
  - 增加 GameplayEvent 配置、旧 Timer 配置、缺失事件 Tag、Timer 携带事件 Tag 的校验覆盖。
  - 增加未知 `releaseTrigger`、未知 Tag、零/负/非有限/错误类型 timeout、字段空白校验。
  - 通过实际 `ReloadDefinitions`/`Resolve` 断言旧 Timer 的默认值：`Timer`、`ReleaseEventTimeout=3.0`、空 `ReleaseEventTag`。
- `Source/SkillLabGAS/Public/SkillLabGASTags.h` 与 `Source/SkillLabGAS/Private/SkillLabGASTags.cpp`
  - 注册原生 Tag `Event_Cast`（`SkillLab.Event.Cast`），供动画 Notify 配置和技能定义测试复用。

## JSON 配置示例

```json
{
  "id": "Skill_Fireball",
  "cost": 10,
  "cooldown": 1,
  "magnitude": 20,
  "releaseTrigger": "GameplayEvent",
  "releaseEventTag": "SkillLab.Event.Cast",
  "releaseEventTimeout": 1.5
}
```

旧配置可省略上述三个字段，继续按 `Timer` 释放。

## 运行链与边界

运行时复用现有链路：`USkillLabSkillAnimNotify::Notify` -> `USkillLabSkillAnimationComponent::ForwardAnimNotify` -> `ForwardCastEvent` -> `USkillLabSkillRuntimeComponent::ReceiveCastEvent`。GameplayEvent 的释放、超时和迟到保护逻辑已经存在，本次只补齐技能定义解析和校验。

Timer 语义保持兼容：运行时 Timer 分支使用 `Phases.WindupSeconds`，不使用 `ReleaseEventTimeout`。因此 Timer 可省略 timeout；若显式提供，仍要求它是有限数字，但允许 0 或负值并忽略其数值。GameplayEvent 仍要求合法事件 Tag 和正 timeout。

当前四个技能 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff` 的 `SkillDefinitions.json` 均未增加新字段，因此仍使用 `Timer`。本批没有修改四个 Lua，也没有批量修改动画资产；当前没有明确需要等待 Notify 的业务配置。

现有 `SkillLabAnimationContent` Commandlet 已支持持久 `UAnimMontage` 的 Notify 写入、保存、通过 UE `ReloadPackage` 替换包后回读和结构校验，但其入口通过 `StaticLoadObject(UAnimMontage::StaticClass(), ...)` 固定读取 Montage，不能对当前四技能的 `castAnimation` 源 `UAnimSequence` 写入或回读 Notify。运行时 `castAnimation -> AnimSequence -> CreateSlotAnimationAsDynamicMontage` 仍使用动态 Montage；动态对象没有持久资产路径，且不能把源 Sequence 秒数直接当作任意 Montage 秒数。因此本轮没有修改四技能动作绑定、没有批量迁移动画，也没有伪造动态 Montage 持久化证据。

当时状态（截至 2026-10-28）：**解析基础设施已完成；实际 Notify 写回/回读、四技能 GameplayEvent 配置和游戏内时序尚未完成。** 当时讨论的后续路线已由当前持久 Montage 配置流程推进；现有 `Config/Animation/FireballMontage.json` 指向的 `MM_Pistol_Fire_Montage` 仍不是四技能实际绑定的资产。

## 验证

- Batch03 临时持久 Montage Patch 成功路径：`Batch03-valid-reloadpackage-report-1.json`、`Batch03-valid-reloadpackage-report-2.json` 均为 `saved=true`、`reloaded=true`、`valid=true`、`diffValid=true`、`assetUnchanged=false`；覆盖 instant 新增/更新、Notify State 新增/更新、delete、稳定身份和无关 Notify 保留。
- Batch03 非法输入路径：source-only 映射、缺失 `evidence`、未知 State、未知 GameplayTag、越界时间、重复身份均拒绝；报告均为 `valid=false`、`assetUnchanged=true`。越界时间在 `ReloadPackage` 版本下再次验证，回滚后测试 `.uasset` 与备份 SHA-256 完全一致。

- 构建命令：
  `D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat SkillLabEditor Win64 Development E:/agent/game/ue/SkillLab/SkillLab.uproject -WaitMutex`
  - 结果：成功；`SkillLabSkillDefinitionProvider.cpp` 和 `SkillLabDefinitionProviderTests.cpp` 编译并完成 `SkillLabSkills` 链接，原生 Tag 已编译进 `SkillLabGAS`。目标 DLL 时间为 `UnrealEditor-SkillLabGAS.dll` 2026-10-28 01:58:31、`UnrealEditor-SkillLabSkills.dll` 2026-10-28 02:19:23。
- 自动化测试命令：
  `D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe E:/agent/game/ue/SkillLab/SkillLab.uproject -unattended -nop4 -nullrhi -ExecCmds="Automation RunTests SkillLab.Skills.DefinitionValidation; Quit"`
  - 结果：通过。编辑器成功启动并找到 1 个测试；日志 `E:/agent/game/ue/SkillLab/Saved/Logs/SkillLab.log` 记录 `Result={Success}`，最终 `EXIT CODE: 0`，进程退出码 0（本轮日志时间 `2026.10.27-18.19.48`）。

## 未验证范围

- 未验证正式技能 Montage 资产上的实际 Notify 写入结果和编辑器内可见表现；本轮只在临时测试 Montage 上执行 `SkillLabAnimationContent -Write`，测试结束后已恢复原始 `.uasset` 字节。
- 未验证当前动态 Montage 路径的 Notify 写回/回读，因为现有 Commandlet 没有 `UAnimSequence` 目标入口。
- 未验证四技能切换到 GameplayEvent；四个现有定义仍保持 Timer 配置。
- 已验证标准驼峰字段 `releaseEventTimeout` 与已注册 `SkillLab.Event.Cast` 可通过 Provider 校验；不再支持单独的 uppercase 别名字段。
