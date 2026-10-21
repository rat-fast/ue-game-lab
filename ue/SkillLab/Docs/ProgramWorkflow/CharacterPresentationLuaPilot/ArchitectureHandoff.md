# Batch02 Lua 角色表现试点：架构交接（2026-10-22 核查）

## 依据、状态与范围

权威需求为 [CharacterPresentationLuaPilotRequirements.md](../../Planning/CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/CharacterPresentationLuaPilotRequirements.md)。本交接针对 P0 默认角色基础五态与火球纵向链、P1 三项差异化技能和终态恢复；受击与正式 VFX 按需求属于 P2，不是本批通过条件。项目程序边界以 [ProgramArchitectureContract.md](../../ProjectContext/ProgramArchitectureContract.md) 为准。

本文件是该 Lua 试点当前唯一的程序架构交接与实现/验证状态记录。角色表现代码迁移后的项目事实另见 `ProjectContext/Modules/CharacterPresentation.md`；本文件负责本批范围内的交接、证据和未完成门槛。

前半部分记录实现前的架构核对，不能覆盖下述实现结果。当前实现已接入项目内 Lua 5.4.8；UE 5.8 `SkillLabEditor Win64 Development` 构建通过，三个局部自动化通过，NullRHI RuntimeProbe 已运行。没有本轮可见 PIE/视口验收；静态资产路径和 NullRHI 事件记录不构成视觉通过证据。

## 当前运行链

正式游戏入口由 `Config/DefaultEngine.ini` 指向 `L_SkillLab_NavTest` 与 `BP_SkillLabGameMode`；`BP_SkillLabCharacter` 实例化 `ASkillLabCharacter`。角色构造 `USkillLabSkillRuntimeComponent`、`USkillLabSkillAnimationComponent`、`USkillLabCharacterAnimationStateComponent` 和 GAS ASC。玩家控制器/快捷输入将请求提交至 `USkillLabSkillRuntimeComponent::SubmitSkillRequest`。

运行时完成门控并分配 `RequestId`、`CastId`，推进 Windup/Active/Recovery/终态。`FSkillLabSkillResult` 经 `OnNativeSkillEvent` 输出结构化事件；技能行为执行投射物、范围、治疗或持续伤害，效果执行器/GAS 提供结果，`USkillLabSkillEffectEventAdapter` 观察周期伤害与效果移除，再回送运行时事件。事件中有 `Event`、`Stage`、`SkillId`、`RequestId`、`CastId`、目标/位置、效果、成功状态、实际量等字段。表现读取这些运行事实，不得自行推断命中、治疗、扣费或终态。

`USkillLabSkillPresentationRouter` 订阅运行时事件，产生当前占位屏幕/世界反馈并转发给 `USkillLabSkillAnimationComponent`。当前组件在 `Started` 解析动作绑定并播放 Montage（Sequence 可动态创建 Montage），匹配当前 `CastId` 的取消/完成时停止动作；Notify 转发检查动画对象、活动 Montage 实例与 CastId。Lua 接入需保留旧施法事件不能影响新施法的保护。

基础表现链独立：`USkillLabCharacterAnimationStateComponent` 从 Character/MovementComponent 读取运动事实，形成 Idle、Move、Jump、Fall、Land 快照并记录状态迁移；`USkillLabCharacterAnimInstance` 将快照桥接到 `ABP_Unarmed` 既有变量；UE AnimBP 继续负责姿势求值、融合与资源播放。当前快照有查询接口，但没有 Lua 可订阅的状态变化委托；新增接线应以变化通知或等价事件转发，避免 Lua 每帧轮询或成为运动状态事实源。

## 复用点、真实资产与技术状态

- 继续复用技能运行时、结构化事件与 `CastId`；不改技能生命周期、GAS、目标、命中、伤害/治疗或持续效果规则。
- 继续复用基础状态组件、AnimInstance、AnimBP 与 UE 动画系统。`SkillLabCharacterAnimationStateComponent` 的状态选择及移动缓冲属于 C++ 稳定运行事实；AnimBP 保持姿势求值职责。
- `Config/SkillDefinitions.json` 是四项技能参数及 `castAnimation` 软路径的现有权威来源；`Config/AnimationDefinitions.json` 是基础动画调参入口。Lua 不复制相同字段或另建技能动作表。源码配置显示四项技能均走 Sequence 绑定路径；资源路径存在不代表兼容、可播或经过视觉验收。
- 源码和文档定位到 `SK_Mannequin`、Manny/Quinn、`ABP_Unarmed`、Idle/Walk/Jog/Jump 等基础资源，以及火球 Sequence 与三个 Mannequin 攻击动作候选。当前未由编辑器核验资产内部 Mesh/Skeleton、Anim Class、Slot 或 Notify。
- UE 自带 ScriptPlugin 在当前安装中为 `WITH_LUA=0`，因此实现未依赖该插件。本项目将 Lua 5.4.8 源码放在 `Source/SkillLab/ThirdParty/lua-5.4.8/` 并由 SkillLab 模块构建；`USkillLabCharacterPresentationLuaComponent` 建立 Lua VM、加载 `Config/CharacterPresentation.lua` 并提供受限的表现回调。构建与 RuntimeProbe 证明当前集成可编译运行。
- `Source/SkillLabAnimationEditor/` 有 Montage 内容命令行工具及 schema，可读写/回读有限 Montage 内容；这不等价于 Lua 内容管线，也不覆盖当前需求的视觉验收。

## 职责边界与最小落点建议

**稳定运行与 UE 适配（C++）**拥有生命周期、权威状态、引擎对象引用与播放安全：技能运行时发出只读事实；基础状态组件提供迁移；角色 Lua 组件接收事件、映射类型化回调并校验对象/施法身份。技能动画组件执行 Montage 播放/停止，不改通用技能内核。

**Lua 表现编排**由 `Config/CharacterPresentation.lua` 承担，按五态事实映射 locomotion 意图，并记录/组合技能 Started、真实结果、Buff 与终态事件；燃烧效果生命周期独立于动作清理。Lua 不决定技能结果。脚本回调不可用时 C++ 提供原生 locomotion fallback。

**配置和资产**沿用现有 JSON 与 UE 动画资产单一来源。Lua 可组合，不复制 cost、cooldown、阶段时长、持续效果数值或动作路径。若确需表现专属配置，应先检查现有字段能否支持，不新增第二份事实源。Blueprint 仅用于现有对象组装/资源绑定需要；UE 负责动画求值与资产执行。

实现与验证状态：

- 已完成 C++/Lua 桥接、基础状态意图、技能事件回调、动作播放/停止接入和燃烧效果事件独立记录。
- 独立 Review 确认两项修复成立：AnimInstance 缓存 Lua locomotion 意图并每帧推送给 AnimBP；重复 Cast Started 幂等处理、较旧 CastId 的 Started 被忽略。
- 已运行 `SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`、`SkillLab.GAS.DamageCalculation`，均为 `Result={Success}`。
- NullRHI RuntimeProbe 观察到 Lua runtime ready、Idle/Move/Jump/Land、火球 Cast 结果、区域爆发、修复、燃烧 BuffApplied/DamageTick/BuffRemoved；观察到四项技能动作组件播放日志。未观察到 Fall，也未专门复现旧 Started/新 Cast 竞态。
- 暂无本轮可见 PIE 证据，差异化可见反馈仍由 C++ Router 提供占位表现。因此 P0/P1 整体尚不能判通过；需要按需求 CP01–CP09 在可见入口补齐。

随后由独立 Review 按 CP01–CP10 核对业务边界、身份隔离、配置权威性和运行/视觉证据。此拆分是基于运行链依赖的实现建议，不要求固定 Agent 数量。

## 实现必读文件与验证入口

项目入口与规范：`Docs/README.md`、`Docs/ProjectContext/README.md`、`Docs/ProjectContext/SkillLabCodeArchitecture.md`、`Docs/ProjectContext/ProgramArchitectureContract.md`、`Docs/ProjectContext/Modules/CharacterPresentation.md`、`Docs/ProjectContext/Modules/SkillLabSkillsRuntime.md`、`Docs/ProjectContext/Modules/TestAndVerification.md`；表现语义以 `Docs/Baseline/CharacterPresentation/01-CharacterPresentation.md` 至 `03-AssetsAndVerification.md` 为准。角色表现模块文档已同步至当前实现事实。

实现核对核心文件：

- 角色：`Source/SkillLab/SkillLabCharacter.{h,cpp}`、`Source/SkillLab/Public/SkillLabCharacterAnimationStateComponent.h`、`Source/SkillLab/Private/SkillLabCharacterAnimationStateComponent.cpp`、`Source/SkillLab/Public/SkillLabCharacterAnimInstance.h`、`Source/SkillLab/Private/SkillLabCharacterAnimInstance.cpp`。
- 技能事件/表现：`Source/SkillLabSkills/Public/SkillLabSkillTypes.h`、`SkillLabSkillRuntimeComponent.h`、`Private/SkillLabSkillRuntimeComponent.cpp`、`SkillLabSkillPresentationRouter.{h,cpp}`、`SkillLabSkillAnimationComponent.{h,cpp}`、`SkillLabSkillEffectEventAdapter.{h,cpp}`、`SkillLabSkillDefinitionProvider.{h,cpp}`。
- 配置/资产入口：`Config/SkillDefinitions.json`、`Config/AnimationDefinitions.json`、`Config/DefaultEngine.ini`、`SkillLab.uproject`、`Content/SkillLab/BP_SkillLabCharacter.uasset`、`Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset` 及需求列出的基础和技能动作资源。
- UE ScriptPlugin 核对：`D:/Epic/UE_5.8/Engine/Plugins/ScriptPlugin/ScriptPlugin.uplugin`、`Source/ScriptPlugin/ScriptPlugin.Build.cs`、构建定义 `Intermediate/Build/Win64/x64/UnrealEditor/Development/ScriptPlugin/Definitions.ScriptPlugin.h`；这些只证明本地插件声明/后端构建宏状态，不能证明本项目已启用或 Lua 调用已运行。

已有自动化入口包括 `SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`、`SkillLab.GAS.DamageCalculation`、`SkillLab.UI.WidgetTreeBeforeSlate`。前三项本轮逐项通过。运行入口为 `Run_SkillLab_DX11.bat`（UE 5.8 默认地图 game/windowed/DX11）及 `SkillSpawnTargets`、`SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset`、`SkillReloadDefinitions` 等控制台命令。静态代码没有确认 `SkillAnimationSmoke` 或 `SkillPresentationSmoke` Exec 命令。

最终 P0/P1 仍需 UE 5.8 可见视口复验；NullRHI 和自动化可验证脚本/事件、局部生命周期与结算，不能证明动作质量或正确恢复。验收按需求记录运行入口、输入、CastId、事实事件、可见结果与未覆盖边界。

## 阻塞事实与未决项

Lua 后端接入已解除技术阻塞。当前剩余验收缺口是可见 PIE/视口证据、Fall 状态与专门的 Cast 竞态复现，以及三项技能差异化可见反馈。

需求 §8 的体验问题（受击、收招形式、缺资源提示、正式 VFX、燃烧效果结束呈现、额外基础状态）不阻塞当前 P0/P1：受击/VFX 属 P2；恢复到真实基础动作与燃烧效果独立性已有明确要求；缺资源可按 CP08 先验证不伪造结果和可定位降级，不承诺未确认的显式玩家提示。

资产内部结构和最终视觉质量未本轮确认，是实现期编辑器/PIE 验证项，不是架构阶段的策划阻塞。
