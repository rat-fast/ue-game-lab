# 通用运行基建实现摘要

## 交付范围

已将经过 Presentation 校验的 Cast Notify 接入 GAS Gameplay Event 通道，并由当前 `USkillLabSkillGameplayAbility` 接收活动定义声明的事件。Ability 将事件及 Cast 身份交回 Skills Runtime，由 Runtime 校验当前 Ability、Cast、阶段和 Tag 后推进释放。Timer 释放路径保留；资源提交、冷却、目标/效果执行及运行结果仍由现有 Runtime/GAS 权威链路处理。

事件载荷通过私有 UObject 携带 Runtime、Ability 和 CastId，不使用浮点 EventMagnitude 编码身份。普通 ASC Gameplay Event 即使 Tag 相同，也没有本次 Cast 身份，不能释放技能。取消、重复/迟到事件、旧 Cast 和 Reset 后事件均由身份及生命周期校验隔离。

## 配置线接口契约

- `FSkillLabSkillDefinition` 是当前运行定义承载体，提供请求/运行所需的定义字段。它不是另造一份平行内容来源。
- `USkillLabSkillBehavior::RequestDeclaredAction()` 是 Behavior 请求执行已声明动作的接口；调用会进入 Runtime 现有执行模板与效果执行链。
- `USkillLabSkillBehavior::GetEffectResult()` 可读取 Behavior 持有的结构化效果结果，适用于动作调用方读取该次请求结果。
- Runtime 发出的结构化 `FSkillLabSkillResult` 技能事件是技能运行阶段与实际效果结果的权威观察来源。内容逻辑不能仅凭请求接受、动作调用返回或 Cast 完成推定命中/结算成功。
- Ability 的 Gameplay Event 等待 Tag 由 Runtime 按活动定义的 `ReleaseEventTag` 与 `BehaviorEventTags` 配置。内容应沿定义、Behavior 与 Runtime 公开动作/结果接口接入，不绕过 Ability/Runtime 直接提交资源或结算效果。

本次没有迁移 JSON、Lua、Schema 或动画资产，也没有改变其内容取值。内容迁移线现在可依据本节接口进行样本迁移和快照对照；迁移前候选值不因此自动成为新批准的业务规则。

## 加载期定义接口

- 每个绑定的 Lua 文件在加载期调用一次 `SkillLabSkill_Define(key, value, ...)`，以字符串键值对声明 `targetShape`、`relation`、`effect`、`execution`。允许值映射到有限枚举/执行模板；缺字段、重复调用或键、未知键/值、类型错误、脚本执行失败及缺少 `OnSkillCast` 均拒绝该候选定义。
- JSON 继续以 `id` 和 `luaScript` 关联定义，并承载数值。`range` 是必需数值；Ground 定义还必须提供正数 `radius`。Projectile 定义必须提供 `projectileSpeed`、`projectileCollisionRadius`、`projectileLifetime`、`projectileVisualScale` 和 `projectileInterceptOtherUnits`，非 Projectile 定义不允许这些投射物键。未知/重复 JSON 字段、错误类型、非有限值、越界值及不一致字段组合均被拒绝。
- `SkillLabSkillDefinitionProvider` 将 Lua 声明、JSON 数值和通用 C++ Ability/Behavior 类合成一个经过完整校验的 `FSkillLabSkillDefinition` 目录；整份候选成功后才替换当前目录。Lua 源快照随规范化定义保存，施法时执行快照，不重新从磁盘读取。C++ Builder 不再按技能 ID 持有目标、关系、效果、执行模板或专属参数定义。
- `OnSkillCast` 运行时仍只通过 `SkillLabSkill_RequestAction()` 请求该规范化定义选出的白名单动作，并可用 `SkillLabSkill_Finish()` 结束脚本流程；脚本不能直接结算效果。Ability、Runtime、Targeting、EffectExecutor 与 GAS 继续拥有请求门控、当前 Cast 校验、目标合法性和真实效果权威。

## 验证

- UE 5.8 `SkillLabEditor Win64 Development` 本轮重新构建成功：`Build.bat SkillLabEditor Win64 Development <SkillLab.uproject> -WaitMutex -NoHotReloadFromIDE`，Result 为 `Succeeded`。
- 本轮重新运行 `UnrealEditor-Cmd.exe <SkillLab.uproject> -nullrhi -unattended -nosplash -NoSound -ExecCmds="Automation RunTests SkillLab.Skills;Quit" -TestExit="Automation Test Queue Empty"`，进程退出码 0；`SkillLab.Skills.CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation` 均为 `Result={Success}`。测试覆盖声明/数值规范化、非法/重复字段拒绝、整目录候选失败不暴露部分定义，以及 Gameplay Event/Ability/Runtime 的身份、释放、费用、取消和 Reset 路径。日志：`Saved/Logs/SkillDefinitionLoading-Automation.log`。
- 本轮重新运行 NullRHI RuntimeProbe，进程退出码 0 且记录 `SkillLabRuntimeProbe COMPLETE`，但这不是四技能行为通过：真实 `SkillDefinitions.json` 与四份 Lua 尚未迁移，Provider 因 `Skill_Fireball.lua` 缺少 `SkillLabSkill_Define` 而拒绝真实目录；Probe 中四项请求均为 `NotInitialized`。该拒绝符合当前迁移边界，不能以旧 Builder 默认值掩盖。日志：`Saved/Logs/SkillDefinitionLoading-RuntimeProbe.log`。
- 此前 `SkillChainRefactor-RuntimeProbe.log` 的四技能成功结果属于加载期定义实现前的阶段，不能证明当前加载器下的配置迁移或行为等价。当前没有可见 PIE 视口验收；动画表现及 Notify 相关场景仍未在本轮复验。

## 修改产物

通用实现涉及 `SkillLabSkillGameplayAbility`、`SkillLabSkillRuntimeComponent` 和 `SkillLabSkillAnimationComponent`；增加 `SkillLab.Skills.CastGameplayEvent` 自动化，并修正 `DefinitionValidation` 对当前 GameplayEvent 定义的过时 Timer 假设。配置、Lua、JSON、Schema 与动画资产均未修改。独立 Review 已通过；真实内容迁移与四项行为验收仍待配置线完成。

## 迁移状态与限制

加载期接口已交付，但本轮四项真实定义均未迁移；`SkillDefinitions.json` 缺少新加载器要求的 `range`，Ground/Projectile 项还需要各自的必需键，四份 Lua 缺少 `SkillLabSkill_Define`。当前真实目录因此无法初始化，配置线应按本节契约迁移后重新运行 RuntimeProbe，并逐项对照迁移前快照。未验证 Mend 受伤治疗、Fireball 阻挡/未命中、AreaBurst 空区域等场景，也未验证可见 PIE 表现。独立 Review 仍待完成。
