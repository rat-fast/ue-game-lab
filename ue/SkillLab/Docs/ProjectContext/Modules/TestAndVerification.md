# TestAndVerification 测试入口与验证

## Module Responsibility

测试职责分为运行时训练场、控制器 Exec 命令和 UE 自动化测试。它们用于复现当前本地技能链和验证局部代码，不代表完整玩家验收。

## Code And Assets

`USkillLabTestArenaSubsystem`（`Source/SkillLab/SkillLabTestArenaSubsystem.cpp`）可禁用指定 NavTest 障碍并尝试生成最多三个训练假人（没有可用位置时会跳过）；`USkillLabTestCommandsComponent` 提供技能槽位调用、生成目标、重置和 JSON reload；`ASkillLabPlayerController` 暴露对应 `UFUNCTION(Exec)`。自动化测试位于 `Source/SkillLabSkills/Private/Tests`、`Source/SkillLabGAS/Private/Tests`，HUD 生命周期测试位于 `Source/SkillLab/Private/SkillLabHUDWidget.cpp`。

## Runtime Entry

控制台命令：`SkillSpawnTargets`、`SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset`、`SkillReloadDefinitions`。外部启动脚本为 `Run_SkillLab_DX11.bat`；历史 NullRHI smoke 使用 `-game -nullrhi -unattended -ExecCmds=...`。

## Runtime Flow

控制器占有 Pawn 后，测试命令初始化训练场并按快捷槽位提交技能；训练假人使用 `ASkillLabCharacter` 并把 `TeamId` 设为 2。`SkillReset` 调用运行时重置施法、持续效果、冷却与属性状态；reload 调用 `FSkillLabSkillDefinitionProvider::ReloadDefinitions`。

## Current Responsibilities And Evolution Boundaries

训练场只负责演示对象和场地，不应成为正式关卡逻辑；Exec 命令只复用正式请求路径，不应直接写效果状态。自动化测试应继续优先验证纯结算、生命周期、定义解析和 Widget 构造等稳定边界。

## Verification

已定位的自动化测试名：`SkillLab.GAS.DamageCalculation`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`、`SkillLab.UI.WidgetTreeBeforeSlate`。已有日志 `Saved/Logs/SkillLabGameplaySmoke.log`、`SkillLabVerticalSmoke.log` 等记录过 NullRHI 运行和事件输出；这些是历史证据。本轮只做静态文档重建，未启动 UE、未编译、未执行自动化测试。

## Current Limitations

日志中包含引擎插件 Python/toolset 噪声，不能单独作为项目失败结论。现有证据不足以证明正式地图视觉、碰撞、鼠标地面预览、动画释放或所有四技能路径在当前工作区重新运行仍通过。

## Extension Directions

后续验证应先建立可重复的 UE 5.8 命令行/编辑器命令，再将日志中的事件断言与自动化测试关联；程序架构审查应检查测试入口是否持续复用同一请求、效果和清理语义。
