# SkillLab 当前代码架构地图

> 本文只记录当前工作区可定位的代码、配置、资产和验证事实。它不是未来程序架构方案，也不把当前类划分冻结为长期接口。

## 项目角色

SkillLab 是一个单机技能系统实验项目：`SkillLab` 模块提供玩家角色、控制器、输入、HUD 和测试场；`SkillLabSkills` 模块提供技能定义、loadout、请求门控、施法生命周期、目标/行为/事件；`SkillLabPresentation` 模块提供角色/技能动画适配、Lua 表现编排和通用反馈；`SkillLabGAS` 模块提供 Gameplay Ability System（GAS）承载的战斗属性、即时伤害/治疗和持续伤害结算。

项目文件为 `SkillLab.uproject`，声明四个 Runtime 模块（含 `SkillLabPresentation`）及一个 Editor 模块并启用 `GameplayAbilities`、`ModelContextProtocol`、`AllToolsets` 等插件。当前默认地图和 GameMode 来自 `Config/DefaultEngine.ini`：`/Game/SkillLab/L_SkillLab_NavTest` 与 `/Game/SkillLab/BP_SkillLabGameMode.BP_SkillLabGameMode_C`。

## 模块地图

| 稳定职责 | 代码入口 | 主要依赖与边界 |
| --- | --- | --- |
| 角色与玩家控制 | `Source/SkillLab/SkillLabCharacter.*`、`SkillLabPlayerController.*` | 组合 ASC、技能运行组件、动画组件、相机；控制器连接输入/HUD/测试命令，不直接结算效果 |
| 角色与技能表现 | `Source/SkillLabPresentation/` | 独立 Runtime 模块；含状态采集、AnimInstance 适配、Lua 5.4.8、事件 Router、技能动作和通用反馈。依赖 `SkillLabSkills` 读取技能事实 |
| 玩家输入、技能快捷键与 HUD | `Source/SkillLab/Public/SkillLab*Input*`、`SkillLabSkillShortcutComponent.*`、`SkillLabHUD*` | 快捷键列表是输入和 HUD 的共同来源；目标解析和展示通过 `SkillLabSkills` 的公开接口完成 |
| 技能运行时与内容 | `Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h` 及同模块实现 | 负责定义加载、loadout、请求门控、Cast 生命周期、行为注册/执行、Lua 组合宿主和事件转发；不拥有角色移动和 HUD 布局 |
| 目标与技能行为 | `SkillLabTargetingService.*`、`SkillLab*SkillBehavior.*`、`SkillLabFireball*`、`SkillLabAreaBurstSkill.*`、`SkillLabMendSkill.*`、`SkillLabBurningSkill.*` | 位于 `SkillLabSkills`；目标关系/距离/视线和具体执行方式在此表达；效果数值交给 `SkillLabGAS` |
| GAS 属性与效果结算 | `Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h`、`SkillLabCombatAttributeSet.h`、`SkillLabDamageCalculation.h` | 负责生命、能量、护甲、力量、伤害减免、生命/能量钳制、死亡 Tag 和持续效果句柄；不决定技能输入或技能槽位 |
| 测试场与命令 | `SkillLabTestCommandsComponent.*`、`SkillLabTestArenaSubsystem.*`、`Source/*/Private/Tests/*` | 运行时 Exec 命令和训练假人用于 smoke；自动化测试覆盖纯伤害计算、Cast 生命周期、定义解析和 HUD 构建 |

## 代码到运行链

```text
DefaultEngine.ini
  -> BP_SkillLabGameMode / BP_SkillLabPlayerController / BP_SkillLabCharacter
  -> ASkillLabPlayerController::BeginPlay / SetupInputComponent
  -> SkillLabPlayerInputComponent + SkillLabSkillShortcutComponent
  -> SkillLabSkillInputComponent（单位目标或 Ground 预览）
  -> USkillLabSkillRuntimeComponent::SubmitSkillRequest
  -> SkillLabSkillLoadout / SkillLabSkillDefinitionProvider
  -> FSkillLabCastLifecycle + USkillLabSkillGameplayAbility
  -> SkillLabSkillBehavior / USkillLabLuaSkillBehavior（即时、投射物或范围）
  -> Config/Skill_*.lua（仅由已校验的 Lua Behavior 执行受限动作请求）
  -> USkillLabSkillEffectExecutor
  -> USkillLabAbilitySystemComponent / USkillLabCombatAttributeSet
  -> SkillLabSkillEffectEventAdapter / PresentationRouter / HUD / FeedbackActor
```

角色表现另有两条相连的运行链：

```text
ASkillLabCharacter / UCharacterMovementComponent
  -> USkillLabCharacterAnimationStateComponent（Idle/Move/Jump/Fall/Land 快照）
  -> USkillLabCharacterPresentationLuaComponent
  -> Config/CharacterPresentation.lua
  -> locomotion intent
  -> USkillLabCharacterAnimInstance（写入 ABP_Unarmed 现有输入）
  -> ABP_Unarmed（UE 姿势求值与融合）

USkillLabSkillRuntimeComponent::OnNativeSkillEvent
  -> USkillLabCharacterPresentationLuaComponent 持有并绑定 USkillLabSkillPresentationRouter
  -> Router 广播表现事件给 Lua 表现组件，并执行通用占位反馈
  -> Started: Lua 请求 USkillLabSkillAnimationComponent 按 SkillId/CastId 读取 JSON 资源绑定并播放 Montage
  -> Cancelled/Completed: 按 CastId 停止对应动作
```

`ASkillLabCharacter` 构造 GAS、技能运行时和表现组件；`SkillLab` 游戏模块依赖 `SkillLabPresentation` 完成角色组装。技能运行时在 `BeginPlay` 初始化 ASC actor info、loadout 和效果执行器，并发布 `FSkillLabSkillResult` 事件；Lua 表现组件创建 Router 并订阅这些事件，EndPlay 时解绑。`SkillLabSkills` 不依赖表现模块、不创建表现对象。技能 Lua 宿主位于 `SkillLabSkills`，使用本模块内置的 Lua 5.4.8 源码，只开放 `SkillLabSkill_RequestAction()` 与 `SkillLabSkill_Finish()`，不能直接访问 ASC、Actor 或表现对象。控制器的 Exec 命令通过 `USkillLabTestCommandsComponent` 复用快捷键或重置运行时。

角色表现链中，`USkillLabCharacterAnimationStateComponent` 从 Character Movement 采集速度、方向、加速度与 Falling 状态，维护五态快照。`USkillLabCharacterPresentationLuaComponent` 订阅状态变化和表现路由事件，将状态/技能结果传入 Lua，并提供播放动作、停止动作、设置 locomotion 意图和日志等窄桥接。Lua 只组合表现策略：当前 `CharacterPresentation.lua` 对 Started 请求动作，对取消/完成请求停止，对命中、阶段、目标和持续效果仅记录日志。若 Lua 脚本或回调不可用，C++ 有原生基础 locomotion 和 Started/终态动作回退；动作资源、Mesh、AnimInstance 或 Montage 播放失败则只记警告并跳过动作，技能运行继续。

`USkillLabCharacterAnimInstance` 缓存 Lua locomotion 意图，并把状态快照字段映射到 `ABP_Unarmed` 的现有输入；Lua 不执行姿势求值。`USkillLabAnimInstance` 是单独的原生 AnimInstance 路径，直接采样 MovementComponent 和 `AnimationDefinitions.json`，不是 `ASkillLabCharacter` 当前表现组件使用的桥接类。

## 表现相关分层职责

| 层 | 当前职责 | 事实来源 |
| --- | --- | --- |
| C++ | 角色输入/移动与技能运行事实、五态状态采集、事件传递、对象和 `CastId` 校验、Montage 播放/停止及脚本失败回退 | `SkillLab`、`SkillLabSkills`、`SkillLabPresentation` 源码 |
| Lua | 将 Idle/Move/Jump/Fall/Land 和技能结构化事件映射为表现意图；当前除 locomotion 与 Started/终态动作命令外，结果类事件只日志 | `Config/CharacterPresentation.lua` |
| Blueprint/AnimBP | 由 GameMode/角色 Blueprint 组装对象并提供现有 Mesh/动画类引用；`ABP_Unarmed` 读取父类提供的输入并执行 UE 状态机、姿势求值与融合 | `DefaultEngine.ini`、角色构造代码、既有资产上下文；本轮未打开 Blueprint/AnimBP 核对节点内部 |
| JSON 配置 | `SkillDefinitions.json` 定义技能数值、阶段与动作软路径；`AnimationDefinitions.json` 定义原生 `USkillLabAnimInstance` 使用的移动阈值/方向范围 | `Config/` 与对应 DefinitionProvider |

玩家控制器输入经 PlayerInput/SkillInput/Shortcut 进入移动或技能请求路径；移动意图由 CharacterMovement 执行，状态组件观察其真实速度与运动模式，不把按键本身当作已发生的状态。技能输入最终调用 `SubmitSkillRequest`，表现侧只消费运行时生成的事件。

## 配置、资产和内容来源

- 数值与技能内容定义文件：`Config/SkillDefinitions.json`。当前列出 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff` 的 `cost`、`cooldown`、`magnitude`、`powerCoefficient`、阶段时间、燃烧 `duration/period`、阻塞标签和 `luaScript` 绑定。技能的目标形状、敌我关系、效果类型、执行模板及投射物结构参数仍由 `SkillLabSkills` 的注册配置提供，Provider 再覆盖 JSON 支持的内容字段。
- 基础动画调参：`Config/AnimationDefinitions.json` 提供 movement threshold 与方向夹角范围；由 `USkillLabAnimInstance` 路径读取。角色表现五态的阈值/状态采集由 C++ 状态组件负责。
- Lua 表现策略：`Config/CharacterPresentation.lua` 映射五态至 locomotion intent，并处理技能 Started/终态动作请求；当前命中、阶段和持续效果回调只记日志。Lua 5.4.8 源码位于 `Source/SkillLabPresentation/ThirdParty/lua-5.4.8/`，由 `SkillLabPresentation.Build.cs` 编入独立模块。
- 技能动作绑定：`SkillDefinitions.json` 中的 `castMontage`/`castAnimation` 软路径由 `SkillLabSkills` 定义提供器解析。当前四技能均配置 `castAnimation`；Sequence 由 UE 动态创建 Montage。`AnimationDefinitions.json` 不保存技能动作绑定。
- 技能组合脚本：`Config/Skill_Fireball.lua`、`Config/Skill_AreaBurst.lua`、`Config/Skill_Mend.lua`、`Config/Skill_BurningBuff.lua` 由 `luaScript` 相对路径绑定。四个当前技能均注册为 `USkillLabLuaSkillBehavior` 运行；脚本只能请求已由定义校验通过的执行动作，投射物、范围查询、即时效果、实际结算、持续效果和 Cast 终态仍由 C++/GAS 运行链产生。
- 输入资产：`Content/SkillLab/Input/IMC_SkillLab.uasset`、`IA_MoveClick.uasset`、`IA_CameraLook.uasset`、`IA_CameraZoom.uasset`；技能槽位与确认/取消键当前由 `USkillLabSkillShortcutComponent` 的默认值提供。
- 运行入口资产：`Content/SkillLab/BP_SkillLabGameMode.uasset`、`BP_SkillLabPlayerController.uasset`、`BP_SkillLabCharacter.uasset`、`L_SkillLab_NavTest.umap`；另有 `L_SkillLab_Test_Box.umap`。
- 外部启动入口：`Run_SkillLab_DX11.bat` 使用 UE 5.8 `UnrealEditor.exe` 以 `-game -windowed -ResX=1280 -ResY=720 -d3d11 -log` 启动。
- 迁移兼容：`DefaultEngine.ini` 的 `[CoreRedirects]` 将原 `/Script/SkillLab`、`/Script/SkillLabSkills` 中迁出的表现反射类型指向 `/Script/SkillLabPresentation`；资产未重存。编辑器动画内容工具依赖新模块提供的 AnimNotify。

## 正式入口与测试入口

正式默认入口是 `DefaultEngine.ini` 的 GameDefaultMap/GameMode，经 Blueprint 资产实例化 C++ 角色和控制器。玩家路径为右键移动、鼠标拖动镜头、滚轮缩放；数字键 `1`-`4` 触发四个默认技能，地面技能用左键确认、`Escape` 取消。

测试入口包括运行时命令、表现 smoke、RuntimeProbe 和 UE 自动化：

1. 控制器/测试组件 Exec：`SkillSpawnTargets`、`SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset`、`SkillReloadDefinitions`、`SkillAnimationSmoke`、`SkillPresentationSmoke`。两个 smoke 命令由 `ASkillLabPlayerController` 声明并委派给 `USkillLabTestCommandsComponent`；本轮未重新运行这两个 smoke。
2. `-SkillLabRuntimeProbe` 启动参数驱动运动采样、技能事件和取消/reset 流程，输出 NullRHI 可读日志。
3. UE 自动化：`SkillLab.GAS.DamageCalculation`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`，以及 `SkillLab.UI.WidgetTreeBeforeSlate`。

已有日志证明过 `-game -nullrhi -unattended -ExecCmds=SkillSpawnTargets,SkillTest,SkillArea,SkillBurn,SkillTestRecover` 的运行链，并观察到初始化、区域命中、修复和拒绝事件；这属于已有验证证据，不等同于本轮重新执行。

## 当前限制与未验证项

- 当前四技能 Lua 接入已经有构建和 NullRHI RuntimeProbe 证据，但尚未完成可见 PIE/视口验收。`Skill_Fireball` 未命中/阻挡、`Skill_AreaBurst` 空区域和独立多目标、Mend 受伤治疗迁移、脚本异常注入、效果失败、重复 RequestId、激活失败、Reset/死亡恢复和坏内容恢复仍未逐项运行验证。
- 当前配置与实现存在明确的分层事实：JSON 绑定 `luaScript` 和可调整内容；四个技能的目标/关系/效果/执行模板及 Fireball 投射物参数仍由各自 C++ 注册入口提供；Lua 宿主和 Runtime/Executor 负责通用动作、身份、去重、清理和真实结果边界。后续若要进一步收敛“纯配置接入”范围，应另行调整实现和文档，不能把当前文档描述当作已完成的架构重构。

- 程序工作流交接记录 UE 5.8 `SkillLabEditor Win64 Development` 构建通过，交接时列出的 CastLifecycle、DefinitionValidation、DamageCalculation 三项自动化逐项通过；对应既有日志分别为 `Saved/Logs/SkillLab-backup-2026.10.21-16.53.57.log`、`...16.54.14.log`、`...16.54.31.log`。模块迁移后本轮全量自动化运行见 `Saved/Logs/SkillLabPresentation-ModuleAutomationAll.log`：`SkillLab.Animation.DefinitionProvider`、`SkillLab.GAS.DamageCalculation`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation` 成功，`SkillLab.UI.WidgetTreeBeforeSlate` 失败，断言期望 `SConstraintCanvas`/`SBox`，实际为 `SObjectWidget`。因此本轮不是全套自动化通过。NullRHI RuntimeProbe 另见 `Saved/Logs/SkillLabPresentation-ModuleProbe.log`。
- `Saved/Logs/SkillLab.log` 记录 2026-10-21 玩家 PIE 中的输入、基础状态迁移和四个技能动作 Montage 播放日志；它是运行日志，不是可归属的用户视觉观察记录，也没有配套视口截图/录屏可作为本轮视觉验收。日志中的入场 Fall→Land 不是玩家跳跃后的完整 Fall 链验证。
- NullRHI RuntimeProbe 日志记录 `ABP_Unarmed_C_0 consumes CharacterAnimationState`，同时各采样点的 `USkillLabAnimInstance` 诊断样本为 null。前者证明该次运行中 AnimBP 实例报告消费了状态接口；后者只表示 Probe 查询的独立原生 AnimInstance 样本为空，不能据此否定 AnimBP 初始化，也不能证明 Lua locomotion intent 最终造成可见姿势变化。
- 没有本轮可见 PIE 视口验收材料确认五态视觉质量、技能差异、取消/完成恢复、缺资源反馈或完整跳跃 Fall 链。Lua 当前对技能结果/持续效果只记日志；差异化屏幕/世界占位反馈由 `SkillLabPresentation` 的 C++ `USkillLabSkillPresentationRouter` 提供。全量自动化中的 `SkillLab.UI.WidgetTreeBeforeSlate` 本轮失败，失败原因见上述日志和测试源码断言。
- 默认地图、Blueprint 属性覆盖、正式动画/音频/特效资源绑定和完整鼠标体验仍需在 UE 5.8 编辑器/游戏进程中复验。
- 当前代码事实是本地单机路径；网络权威、多人同步、输入缓冲、更多技能内容和正式内容生产协议没有证据支持。
- `SkillLabGAS` 的 ASC 在代码中设置为 replicated，但项目当前文档和验证没有证明多人同步行为，应由后续程序架构审查单独判断。

## 维护入口

模块细节见 `Modules/`。技能系统的业务语义继续以 `Docs/Baseline/SkillSystem/` 共享基线为准；本目录只补充代码类、资产路径、运行入口和验证证据。
