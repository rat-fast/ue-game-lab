# SkillLabPresentation 角色与技能表现

## Module Responsibility

记录当前版本中 `SkillLabPresentation` 独立模块内的角色动画状态、Lua 表现编排、技能动作播放、AnimBP 和相关验证入口的真实代码事实。

## Code And Assets

源码集中在 `Source/SkillLabPresentation/Public/`、`Private/` 和 `ThirdParty/`，通过 `SkillLabPresentation.Build.cs` 与 `SkillLab.uproject` 注册为独立 Runtime 模块。`SkillLab` 依赖它组装角色和地面选点反馈；它依赖 `SkillLabSkills` 读取事件与动作绑定，技能模块不反向依赖表现模块。

- `USkillLabCharacterAnimationStateComponent` 采集 `Idle`、`Move`、`Jump`、`Fall`、`Land` 状态事实并广播迁移。
- `USkillLabCharacterPresentationLuaComponent` 加载 `Config/CharacterPresentation.lua`，将状态和技能结构化事件交给 Lua 编排，并通过类型化回调请求表现动作。
- Lua 由项目内 `Source/SkillLabPresentation/ThirdParty/lua-5.4.8/` 提供；Lua 负责表现意图，不负责技能规则或动画求值。
- `USkillLabCharacterAnimInstance` 是角色表现链的桥接 AnimInstance：缓存 Lua locomotion 意图，并将状态快照字段写入 `ABP_Unarmed` 既有输入；`USkillLabAnimInstance` 是单独的 MovementComponent 直接采样路径。
- `USkillLabSkillAnimationComponent` 按 `CastId` 播放/停止技能动作；重复 Started 幂等，旧于活动 Cast 的 Started 被忽略。此身份保护由先前程序 Review 确认，迁移后代码位于 `Source/SkillLabPresentation/Private/SkillLabSkillAnimationComponent.cpp`。
- `USkillLabSkillPresentationRouter` 与 `ASkillLabSkillFeedbackActor` 提供屏幕文字、范围环、目标框和数值占位反馈；FeedbackActor 也用于游戏模块的地面选点预览。`DefaultEngine.ini` 的 `[CoreRedirects]` 保留迁移前反射类、快照结构和状态枚举路径的兼容映射。
- `Config/SkillDefinitions.json` 和 `Config/AnimationDefinitions.json` 仍是技能内容与基础动画调参入口；Lua 不复制技能数值或动作资源路径。

## Runtime Entry

`ASkillLabCharacter` 组装状态采集与 Lua 表现组件；玩家控制器仍经既有技能运行时提交请求。正式可见运行入口为 `Run_SkillLab_DX11.bat`，结构化技能命令入口见 `TestAndVerification.md`。

## Runtime Flow

`ASkillLabCharacter` 创建状态、Lua 表现和技能动画组件。状态组件从 `UCharacterMovementComponent` 采集运动事实，按速度/加速度/方向、`IsFalling` 和落地窗口产生 Idle、Move、Jump、Fall、Land 快照并广播变化；Lua 将状态及移动/下落标志映射为 locomotion intent。`USkillLabCharacterAnimInstance` 缓存该意图，并同步快照的速度/方向等字段到 `ABP_Unarmed` 变量，AnimBP 负责姿势求值与融合。

Lua 表现组件在 BeginPlay 创建 Router 并将其绑定到技能运行时 `OnNativeSkillEvent`，在 EndPlay 解绑；技能运行时不再持有 Router。Router 发出表现事件后，Lua 在 `Started` 请求 `USkillLabSkillAnimationComponent` 解析技能绑定并播放 UE Montage，在 `Cancelled`/`Completed` 请求停止对应 `CastId`；动画组件不直接订阅事件。当前 Lua 对 Hit、Rejected、PhaseChanged、TargetSelected、EffectApplied、BuffApplied、DamageTick、BuffRemoved 只写日志，没有技能差异化动画或 VFX 组合。C++ Router 的屏幕文字/范围环/目标框/数值标记是当前占位反馈，不归 Lua 所有。持续效果事件与施法动作终态分开处理。

## Current Behavior

`Saved/Logs/SkillLab-backup-2026.10.21-16.51.16.log`（NullRHI RuntimeProbe）观察到 Lua runtime ready、Jump/Land 状态意图、四技能动作组件播放、火球/区域爆发/修复/燃烧运行事件及燃烧周期事件；同一日志记录 `ABP_Unarmed_C_0 consumes CharacterAnimationState`，但 Probe 对 `USkillLabAnimInstance` 的诊断样本为 null。该样本为空不否定上述 AnimBP 消费记录，也不能证明 Lua intent 最终造成可见姿势变化。该次 Probe 有 Jump→Land 状态变化，没有 Fall 状态。`Saved/Logs/SkillLab.log`（2026-10-21 玩家 PIE）记录移动输入、四技能 Montage 播放及角色入场 Fall→Land，但不是跳跃触发的 Fall 验收或用户视觉观察记录；没有本轮可见 PIE 视口验收材料。

## Current Responsibilities And Evolution Boundaries

- 当前职责：C++ 拥有运动/技能事实、对象安全与动画执行；Lua 拥有表现事件组合；UE AnimBP 拥有姿势求值与融合。
- 默认修改入口：基础意图查 `Config/CharacterPresentation.lua`；基础状态事实查 `USkillLabCharacterAnimationStateComponent`；技能事实沿用结构化运行事件；技能动画资源绑定查 `Config/SkillDefinitions.json` 与 UE 动画资产。
- 可接受的架构调整：当脚本接口无法承载新增表现语义时，按对应任务重新核对 C++/Lua/AnimBP 边界。
- 重构触发条件：Lua 状态与 AnimBP 输入不同步、施法终态误清新动作，或内容来源重复时复核当前分工。
- 当前阶段限制：本试点不扩展受击表现或正式 VFX；这是本批范围控制。

## Verification

程序工作流交接记录 UE 5.8 `SkillLabEditor Win64 Development` 构建通过；`Saved/Logs/SkillLab-backup-2026.10.21-16.53.57.log`、`...16.54.14.log`、`...16.54.31.log` 分别记录 CastLifecycle、DefinitionValidation、DamageCalculation 为 `Result={Success}`。NullRHI RuntimeProbe 证明脚本和结构化事件可运行，并记录 AnimBP 消费状态接口的初始化信息；它不证明 Lua intent 最终造成可见姿势变化、可见动作质量或完整五态。历史 `SkillLab.log` 有玩家输入和技能播放日志，但没有本轮可见 PIE 视口验收材料。

## Current Limitations

未验证跳跃到 Fall 到 Land 的完整可见链、Fall 视觉、取消/完成后姿势恢复、缺动作资源/AnimInstance 的运行降级表现，以及旧 Started 与新 Cast 交错的运行竞态。代码身份检查和程序 Review 不是竞态复现证据。Lua 目前对技能结果只记日志；三项技能没有 Lua 差异化视觉反馈，C++ Router 仍提供通用占位标记。

## Extension Directions

正式视觉验收应以可见运行证据为准；扩展受击或正式 VFX 前先确认对应体验范围与资源。
