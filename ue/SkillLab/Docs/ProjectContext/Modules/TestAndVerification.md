# TestAndVerification 测试入口与验证

## Module Responsibility

训练场、Exec/smoke、RuntimeProbe 和 UE 自动化用于复现本地运行链及核对代码边界；可见游戏/PIE 验收单独提供玩家体验证据。本文区分已注册入口、各版本已运行结果和未验证范围。

## Code And Assets

`Source/SkillLab/SkillLabTestArenaSubsystem.cpp` 尝试生成最多三个 TeamId=2 的训练假人，并可禁用指定 NavTest 障碍；无可用位置时跳过。`Source/SkillLab/Private/SkillLabTestCommandsComponent.cpp` 复用快捷槽位请求、Reset 与 reload，`Source/SkillLab/SkillLabPlayerController.h/.cpp` 暴露并转发 Exec。自动化位于 Skills、GAS、Presentation 的 `Private/Tests/`；共享夹具包括 Skills 的 `SkillLabCombatTestFixture.h`、`SkillLabSubmissionTestWorld.h`、`SkillLabProductionFixture.h`。HUD 生命周期测试位于 `Source/SkillLab/Private/SkillLabHUDWidget.cpp`。

## Runtime Entry

控制台入口为 `SkillSpawnTargets`、`SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset`、`SkillReloadDefinitions`、`SkillAnimationSmoke`、`SkillPresentationSmoke`。后两项由控制器委派给测试组件；入口存在不代表本轮已运行。`-SkillLabRuntimeProbe` 提供 NullRHI 可读采样；`Run_SkillLab_DX11.bat` 启动 `-game` 游戏窗口，PIE 需编辑器 Play。

默认槽位命令为 `SkillTest`→0/Fireball、`SkillArea`→1/AreaBurst、`SkillTestRecover`→2/Mend、`SkillBurn`→3/BurningBuff，经 Shortcut 进入玩家选点链。`SkillAnimationSmoke` 转发 `AnimationStateSmoke`，`SkillPresentationSmoke` 则直接按序请求四技能，序列结束后主动 `RequestExit(false)`；后者不验证真实键鼠选点。

## Runtime Flow

控制器占有 Pawn 后，测试命令初始化训练场并走正式请求路径；reload 调用 `FSkillLabSkillDefinitionProvider::ReloadDefinitions`。`SkillReset` 进入 Runtime 的 attempt/屏障和资源最终回执链，可能受未决阻塞、排队或失败，不能描述为调用后所有状态立即恢复。详细行为见 [Skills](SkillLabSkillsRuntime.md) 与 [GAS](SkillLabGAS.md)。

## Current Behavior

下表列当前源码注册入口；同一行简称共用指定前缀，注册存在与本版运行通过分别判断。

| 范围/前缀 | 测试后缀 |
| --- | --- |
| `SkillLab.Skills.` 定义与阶段 | `CastLifecycle`、`DefinitionValidation`、`SubmissionDefinition`、`CastGameplayEvent` |
| `SkillLab.Skills.` 执行与仲裁 | `CastExecutionOwnership`、`SharedCastArbitration`、`CastIntentLifecycle`、`CastControlAndRules`、`CastRequestReentry`、`CastSubmissionIsolation` |
| `SkillLab.Skills.` 提交、寿命与来源 | `MultipleSubmissions`、`SubmittedActionLifetime`、`ProductionRecords`、`SubmissionReconciliation`、`LateSubmission`、`UnconfirmedSubmission`、`AreaSubmissionSettlement`、`SubmissionFeedbackRouting`、`SubmissionFeedbackIdentity` |
| `SkillLab.Skills.` Reset | `ResetResourceCompletion`、`ResetSubmissionDuringCompletion` |
| `SkillLab.GAS.` | `DamageCalculation`、`EnergyLedger`、`EnergySettlementHistory`、`ResourceWriteIntegration`、`ResourceExecutionBoundary` |
| 表现与动画完整名 | `SkillLab.Presentation.SubmissionIdentity`、`SkillLab.Presentation.CastAnimationResources`、`SkillLab.Animation.DefinitionProvider` |
| 其他既有入口，本次未复测 | `SkillLab.UI.WidgetTreeBeforeSlate`、`SkillLab.LuaRuntime.Boundary` |

## Current Responsibilities And Evolution Boundaries

训练场只负责演示对象和场地；Exec 复用正式请求/重置语义。自动化按定义、共同仲裁、提交身份与寿命、资源、表现适配等职责定位，夹具不等于生产内容。新异步路径应验证最终回执与来源，而非只断言请求受理；新增视觉结论需提供相应可见证据。

## Verification

本次文档维护只静态核对源码、当前实现摘要和已接受 Review，未构建、运行测试、审资产或重做可见验收。B1/多 Cast 与 P2-CE-1、R 资源修复及 Reset 专项的当前接受依据是 [CastExecutionReview](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/CastExecutionReview.md)，其中 **2026-11-12 立即合法资源 Rejected 补证复查**关闭最后该项缺口。报告较早章节的待证状态按其后续结论解读，专项通过不代表整批玩家验收完成。

以下均为既有隔离验证日志，根目录为 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/`：

| 证据版本 | 运行日志与目标结果 | 文件范围与解释 |
| --- | --- | --- |
| 原 B1/多 Cast 合并执行 | `CastExecutionFinalValidation2.log`：23 项 Success，含 `DefinitionValidation`、`SubmissionDefinition` | 408 个 Source/Config/Content 文件，Source 216；保留 `CastExecutionAccepted23SourceHashes.json` 快照，属于重入修复前版本 |
| P2-CE-1 修后 | `CastCE1FinalBuild.log` / `CastCE1FinalValidation.log`：构建成功、15 项 Success，含重入五场景 | 409 文件，Source 217；相对上版只修改 Requests.cpp 并新增 CastRequestReentry 测试；23 项未全套重跑 |
| R+Reset 交叉 | `ResetResourceFinal3Build.log` / `ResetResourceFinal3Validation.log`：构建成功、8 项 Success | 416 文件，Source/Config/Content=224/43/149；目标为 EnergyLedger、EnergySettlementHistory、ResourceExecutionBoundary、ResourceWriteIntegration、CastRequestReentry、LateSubmission、ResetResourceCompletion、UnconfirmedSubmission |
| Reset 收束/重入补证 | `ResetEvidenceFinalBuild.log` / `ResetEvidenceFinalValidation.log`：构建成功、ResetResourceCompletion 与 ResetSubmissionDuringCompletion 两项 Success | 416 文件，相对交叉版只修改 Reset 测试文件；旧 8 项未在此版本重跑 |
| 合法立即 Rejected 补证 | `ResetImmediateRejectedBuild.log` / `ResetImmediateRejectedValidation.log`：构建成功、ResetResourceCompletion 一项 Success | 416 文件，相对上一版只修改同一 Reset 测试文件；旧 8 项/2 项保留各自版本范围 |

上述运行退出状态均为 0；各版本数量不相加为一次全套通过。Reviewer 核对生产工程、隔离工程与清单的 Source/Config/Content 哈希及 uproject；不覆盖 Plugins 或引擎，也不构成全仓或跨进程承诺。启动仍有 13 条 `Condition failed`、PIX 初始化失败等记录，目标测试随后成功；不能称日志无错误。

最终生产内容运行样本为同一隔离目录下的 `FinalProductionRuntimeProbe.log`：四技能提交、Fireball 40 伤害、AreaBurst 25 伤害、满血 Mend 实际治疗 0、BurningBuff 三次各 8 点周期、提交前取消及 ResetCompleted 均有日志，Probe COMPLETE、退出 0。运行负责人报告运行前后 416 文件与已接受 `ResetImmediateRejectedHashes.json` 一致；PM 已核对日志及其 SHA-256 `E7710BE5158CD351E945A470D819CB6C636A096F969A88AF8CE7D539E4B6EF96`。这是最终版本的 `-game -nullrhi -SkillLabRuntimeProbe` 样本，不能证明可见画面或 PIE；启动另有 NiagaraToolsets Python AttributeError。该次 Probe 未启动可见 SkillLab；后续实际工程窗口证据单列如下。

**实际工程交付证据（2026-11-12）**：`E:/agent/game/ue/SkillLab/Saved/Logs/HUDInputDeliveryBuild.log` 记录实际工程 SkillLabEditor Win64 Development 构建 Succeeded，执行摘要记录退出 0，并确认游戏进程从该工程 `Binaries/Win64/` 加载 SkillLab、Skills、GAS、Presentation 模块。隔离构建不自动更新实际启动工程 DLL；交付需构建实际工程并核对加载来源。构建、模块身份及用户反馈的权威定位为 [CastExecutionImplementationSummary 的实际工程交付修复](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/CastExecutionImplementationSummary.md#实际工程交付修复2026-11-12)。

实际工程 `Saved/Logs/HUDInputDeliveryGame.log` 记录 DX11 `-game` 启动、四技能加载、1–4 绑定、HUD `Skills=4`，以及真实按键 3 的 Mend 动作、提交和完成；[实际游戏窗口截图](../../../Saved/Logs/HUDInputDeliveryWindow.png) 可见 HP、Energy 与四技能槽，用户随后人工确认技能可释放。按键 2/左键当次虽到达输入入口，地面投射失败，不能据此认定 AreaBurst 成功。该材料支持实际工程交付与基础 HUD 可见，不等于全面可见体验或 PIE 验收；交互职责见 [PlayerInteraction](PlayerInteraction.md)。本次只读核对既有源码、日志及截图，未启动或操作项目。

R 的写入边界和 Reset 的合法拒绝、失败/不可用、未决阻塞及重入收束证据，以 [ResourceWriteImplementationSummary](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/ResourceWriteImplementationSummary.md)、[CastExecutionImplementationSummary](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/CastExecutionImplementationSummary.md) 和上述 Review 为定位入口。尤其 Reset 的 Queued 不是完成，最终拒绝应保留冷却并释放屏障，不由恢复 Health 或请求返回推断成功。

资源资产检查沿用 [ResourceWriteAudit](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/ResourceWriteAudit.md) 的既有快照范围：实际加载 `/Game` 149 个资产，未枚举到独立 GE 资产；原生 Ability CDO Cost/Cooldown GE 为 None；角色/GameMode/PlayerController Blueprint 已读图中未见资源写入或 GE 应用节点。AnimBP 普通 K2 节点已读取，Control Rig 专用非 K2 图未覆盖，不能升级为全部图/外部动态类已审。该报告原先发现的写入/Reset 问题，其修复状态以 R+Reset 后续 Review 为准，本次没有重审资产。

历史 Lua 模块边界与 Game target 证据保留在 `Saved/Logs/LuaRuntimeModule-Boundary-Automation-AfterDeletion.log`、`LuaRuntimeModule-Automation-AfterDeletion.log`、`LuaRuntimeModule-GameTarget-Build.log`；历史四技能 Probe 为 `Saved/Logs/Batch02-Lua-FourSkill-RuntimeProbe.log`。这些不是当前多 Cast/R+Reset 版本的重新运行。Probe 的 `COMPLETE` 与 AnimBP 消费接口日志只支持当时运行链，不证明可见姿势；历史 Widget 全量运行 `Saved/Logs/SkillLabPresentation-ModuleAutomationAll.log` 的 `WidgetTreeBeforeSlate` 曾因期望 SConstraintCanvas/SBox、实际 SObjectWidget 失败，本次未复测，不据定向通过宣称全套绿色。

复现入口示例（仅供后续执行，本次未运行；工程路径应替换为获准的验证工程）：

隔离测试使用隔离工程路径；玩家实际交付使用实际工程路径并构建该工程，不以路径替换后的测试成功代替实际 DLL 更新。可见启动沿用 `Run_SkillLab_DX11.bat`；最小人工步骤见上述执行摘要的“最终运行与可见验收”，其中早期无截图的记录仅限当时 Probe 阶段。

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -unattended -nop4 -NullRHI -nosplash -nosound '-ExecCmds=Automation RunTests SkillLab.Skills.ResetResourceCompletion+SkillLab.Skills.ResetSubmissionDuringCompletion' '-TestExit=Automation Test Queue Empty' '-abslog=C:/Users/Administrator/AppData/Local/Temp/SkillLab-ResetVerification.log'
```

## Current Limitations

当前已有实际 `-game` HUD 截图、Mend 键盘释放日志及用户人工确认；完整键鼠组合、PIE、多 Cast 体验、兼容双 Montage、完整 Jump→Fall→Land、取消/完成姿势恢复、地面鼠标预览或缺资源降级的可读性仍未全面验收。生产四 Montage 均为 `DefaultGroup/DefaultSlot`，不能把 token 隔离和资源引用测试描述成同时播放成功。旧玩家 PIE 日志中的入场 Fall→Land 不是完整跳跃验收；NullRHI 不替代可见结果。

沉默/硬直多来源测试 GE 不等于生产授予者已接入，所有攻击类别的生产来源尚未证明。最终生产 Probe 仅支持上述日志事实；CoreRedirects 对旧资产的可见兼容及未覆盖资源资产范围仍需各自证据。各版本定向测试通过不升级为全部生产失败路径、多人或存档恢复已验证。

## Extension Directions

后续按实际变更选择受影响的测试与必要可见场景，在证据中保留工程版本、目标、日志与未覆盖范围；保持验证入口复用正式请求、提交、效果和清理链。
