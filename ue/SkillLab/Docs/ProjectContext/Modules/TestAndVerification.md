# TestAndVerification 测试入口与验证

## Module Responsibility

测试职责分为运行时训练场、控制器 Exec/smoke 命令、RuntimeProbe 和 UE 自动化测试。它们用于复现当前本地技能链和验证局部代码，不代表完整玩家验收。

## Code And Assets

`USkillLabTestArenaSubsystem`（`Source/SkillLab/SkillLabTestArenaSubsystem.cpp`）可禁用指定 NavTest 障碍并尝试生成最多三个训练假人（没有可用位置时会跳过）；`USkillLabTestCommandsComponent` 提供技能槽位调用、生成目标、重置和 JSON reload；`ASkillLabPlayerController` 暴露对应 `UFUNCTION(Exec)`。自动化测试位于 `Source/SkillLabSkills/Private/Tests`、`Source/SkillLabGAS/Private/Tests`、`Source/SkillLabPresentation/Private/Tests`，HUD 生命周期测试位于 `Source/SkillLab/Private/SkillLabHUDWidget.cpp`。

## Runtime Entry

控制台命令：`SkillSpawnTargets`、`SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset`、`SkillReloadDefinitions`、`SkillAnimationSmoke`、`SkillPresentationSmoke`。后两项由 `ASkillLabPlayerController` 声明并转发到测试组件；本轮只核对到声明与组件实现存在，未重新运行这两个 smoke。外部启动脚本为 `Run_SkillLab_DX11.bat`；NullRHI RuntimeProbe 由 `-SkillLabRuntimeProbe` 启动参数触发。

## Runtime Flow

控制器占有 Pawn 后，测试命令初始化训练场并按快捷槽位提交技能；训练假人使用 `ASkillLabCharacter` 并把 `TeamId` 设为 2。`SkillReset` 调用运行时重置施法、持续效果、冷却与属性状态；reload 调用 `FSkillLabSkillDefinitionProvider::ReloadDefinitions`。

## Current Responsibilities And Evolution Boundaries

训练场只负责演示对象和场地，不应成为正式关卡逻辑；Exec 命令只复用正式请求路径，不应直接写效果状态。自动化测试应继续优先验证纯结算、生命周期、定义解析和 Widget 构造等稳定边界。

## Verification

已定位的自动化测试名：`SkillLab.GAS.DamageCalculation`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`、`SkillLab.UI.WidgetTreeBeforeSlate`、`SkillLab.Animation.DefinitionProvider`。模块迁移后的全量运行日志 `Saved/Logs/SkillLabPresentation-ModuleAutomationAll.log` 显示 DefinitionProvider、DamageCalculation、CastLifecycle、DefinitionValidation 成功；WidgetTreeBeforeSlate 失败，测试期望 HUD 返回 `SConstraintCanvas` 且技能卡返回 `SBox`，实际分别为 `SObjectWidget`（断言位于 `Source/SkillLab/Private/SkillLabHUDWidget.cpp`）。这不是全套自动化通过。程序工作流交接记录的三项既有技能/GAS 自动化曾单独通过。UE 5.8 `SkillLabEditor Win64 Development` 构建本轮通过；首次构建因 `SkillLabAnimationEditor` 缺少迁移后 AnimNotify 头文件依赖失败，补充对 `SkillLabPresentation` 的模块依赖后通过。新模块构建后重跑 NullRHI RuntimeProbe，日志为 `Saved/Logs/SkillLabPresentation-ModuleProbe.log`。

本轮 NullRHI RuntimeProbe（`Saved/Logs/SkillLabPresentation-ModuleProbe.log`）记录 Lua runtime ready、`ABP_Unarmed_C_0 consumes CharacterAnimationState`、入场 Fall→Land、受控 Jump→Land、四个技能动作组件报告 Montage 播放、燃烧 DamageTick/BuffRemoved、Cast 5 取消与 reset。各采样点的 `USkillLabAnimInstance` 诊断样本仍为 null；这不否定 AnimBP 消费状态接口的记录，也不能证明 Lua intent 造成可见姿势变化。该日志是 NullRHI 运行证据，不是可见 PIE 视口验收。历史玩家 PIE 日志 `Saved/Logs/SkillLab.log` 只记录移动输入、入场 Fall→Land 和技能 Montage 日志，不证明本轮视觉质量。

## Current Limitations

日志中包含引擎插件 Python/toolset 噪声，不能单独作为项目失败结论。运行时代码对 Lua 脚本加载/回调失败有原生 locomotion 与技能动作回退；技能动作资源、Mesh、AnimInstance 或 Montage 失败只记录警告并跳过播放、保留技能运行。本轮未专门验证这些降级路径的玩家可读性。没有本轮可见 PIE 视口证据确认姿势质量、地面鼠标预览或技能视觉；Probe 观察到受控 Jump→Land，但没有保持在 Fall 的采样日志，也没有证明 Fall 视觉。代码 CastId 检查不等于旧/新 Cast 竞态已在运行中复现。CoreRedirects 已添加，但未通过旧 Blueprint/AnimBP 的本轮可见编辑器检查。

## Extension Directions

后续验证应先建立可重复的 UE 5.8 命令行/编辑器命令，再将日志中的事件断言与自动化测试关联；程序架构审查应检查测试入口是否持续复用同一请求、效果和清理语义。
