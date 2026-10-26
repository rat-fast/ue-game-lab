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

已定位的自动化测试名：`SkillLab.GAS.DamageCalculation`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`、`SkillLab.UI.WidgetTreeBeforeSlate`、`SkillLab.Animation.DefinitionProvider`。本轮四技能 Lua 自动化日志 `Saved/Logs/Batch02-Lua-FourSkill-Automation.log` 显示 `SkillLab.Skills.CastLifecycle` 和 `SkillLab.Skills.DefinitionValidation` 均为 `Result={Success}`，进程退出码为 `0`。四技能 RuntimeProbe 日志为 `Saved/Logs/Batch02-Lua-FourSkill-RuntimeProbe.log`，在 NullRHI 下记录四个 Lua 技能的事件链、真实效果结果、燃烧周期与 Reset 后资源状态，并以 `SkillLabRuntimeProbe COMPLETE` 结束。UE 5.8 `SkillLabEditor Win64 Development` 构建使用 `D:\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat SkillLabEditor Win64 Development E:\agent\game\ue\SkillLab\SkillLab.uproject -WaitMutex -NoHotReloadFromIDE`，最终结果为 `Succeeded`。

Lua Runtime 模块迁移后的定向证据见 `Saved/Logs/LuaRuntimeModule-Boundary-Automation-AfterDeletion.log`、`Saved/Logs/LuaRuntimeModule-Automation-AfterDeletion.log` 和 `Saved/Logs/LuaRuntimeModule-RuntimeProbe-AfterDeletion.log`：`SkillLab.LuaRuntime.Boundary`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation` 均为 `Result={Success}`，NullRHI RuntimeProbe 以 `SkillLabRuntimeProbe COMPLETE` 结束。`SkillLab Win64 Development` Game target 已成功构建，复核日志为 `Saved/Logs/LuaRuntimeModule-GameTarget-Build.log`。

四技能 RuntimeProbe 的关键结果为：Fireball 命中并记录请求伤害 `30`、实际伤害 `40`；AreaBurst 非空范围命中并记录请求伤害 `20`、实际伤害 `25`；Mend 两次满血治疗均记录请求 `40`、实际 `0`；BurningBuff 记录效果添加、三次 `DamageTick`（每次实际 `8`）和 `BuffRemoved/EffectTrackingCleared`，Reset 后能量为 `100`、Fireball 冷却为 `0.00`。该日志是 NullRHI 运行证据，不是可见 PIE 视口验收。未单独运行未命中/阻挡、空区域、受伤治疗、脚本失败、效果失败、重复 RequestId、激活失败、死亡后 Reset 和坏内容恢复场景。

## Current Limitations

日志中包含引擎插件 Python/toolset 噪声，不能单独作为项目失败结论。运行时代码对 Lua 脚本加载/回调失败有原生 locomotion 与技能动作回退；技能动作资源、Mesh、AnimInstance 或 Montage 失败只记录警告并跳过播放、保留技能运行。本轮未专门验证这些降级路径的玩家可读性。没有本轮可见 PIE 视口证据确认姿势质量、地面鼠标预览或技能视觉；Probe 观察到受控 Jump→Land，但没有保持在 Fall 的采样日志，也没有证明 Fall 视觉。代码 CastId 检查不等于旧/新 Cast 竞态已在运行中复现。CoreRedirects 已添加，但未通过旧 Blueprint/AnimBP 的本轮可见编辑器检查。

## Extension Directions

后续验证应先建立可重复的 UE 5.8 命令行/编辑器命令，再将日志中的事件断言与自动化测试关联；程序架构审查应检查测试入口是否持续复用同一请求、效果和清理语义。
