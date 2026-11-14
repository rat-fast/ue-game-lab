# F1 / F3 行为修复交付

2026-11-13。范围仅为原始 Review 的 F1 死亡 Reset 恢复、F3 后续 GameplayEvent 等待结束保障，以及必要定向测试。本任务未修改原始 Review、配置或资产，未提交或推送；其他整改由 PM 分别协调。

## 依据与实际修改

已读取 `ue-project-context`、`ai-native-programming`、`ai-program-module-implementation`，项目 Docs/ProjectContext 入口及 `ProgramArchitectureContract.md`；任务入口、MainReview 的 F1/F3、StateAndLifecycleReview 的 SL-03；SkillSystem 基线入口及请求生命周期、清理正文；`SkillCombatRulesRequirements.md` 的时序、清理与第 8 节规则。验证入口来自 `ProjectContext/Modules/TestAndVerification.md`。

| 修改文件（相对项目根目录） | 修改与职责 |
| --- | --- |
| `Source/SkillLabGAS/Public/SkillLabCombatAttributeSet.h`、`Private/SkillLabCombatAttributeSet.cpp` | 将原 `ClampVitals` 中的 Dead 标签同步移到 Health 的 `PostAttributeChange`，由 GAS 属性所有者统一消费最终 Health。GE 伤害、治疗及 Reset 的 `SetNumericAttributeBase(Health, ...)` 均覆盖；Runtime 不新增手工删死亡标签的补丁。原生命值钳制、资源写入及 Reset 最终回执流程保留。 |
| `Source/SkillLabSkills/Private/SkillLabSkillEntryParsing.cpp` | 完成提交点及 completionMode 校验后，拒绝含 GameplayEvent 提交点但 `active <= 0` 的定义，诊断为 `GameplayEvent submission points require a positive active timeout`。释放超时在释放时已清理，后续等待使用既有 ActiveTimeout 取消契约；无需新增默认超时、字段或调度器。 |
| `Source/SkillLabSkills/Private/Tests/SkillLabDeathResetTests.cpp` | 新增独立 `SkillLab.Skills.DeathReset`。通过真实伤害致死、Reset、新请求和 Ability/GameplayEvent 路径核对恢复，连续执行两轮。 |
| `Source/SkillLabSkills/Private/Tests/SkillLabSubmissionEventTimeoutTests.cpp` | 新增独立 `SkillLab.Skills.SubmissionEventTimeout`。校验缺省/零 Active 的定义拒绝，并分别验证未收到首个释放事件、首点成功后未收到后续事件。 |
| `Source/SkillLabSkills/Private/Tests/SkillLabSubmissionDefinitionTests.cpp` | 为合法多点 fixture 及规则窗口冲突 fixture 补正 Active 超时，保留原有解析及诊断覆盖。 |

稳定机制仍由 C++/GAS 承担；内容参数仍来自既有 JSON、组合仍来自 Lua。本次未新增内容专属内核分支、公共业务 API 或 Blueprint 逻辑，也未改变模块职责或运行入口，无需扩写代码地图。

**F3 内容兼容变化**：原先可被接受的“GameplayEvent 提交点 + 缺省/零 Active”现在在加载期明确拒绝，调用方候选目录保持原有原子失败语义。四个生产技能不含该类显式点，既有定义验证通过。无 GameplayEvent 提交点的即时、Script 或 Timer 内容不因本次修改被强制添加 Active 时长。

## 修前复现与修后验证

所有构建、自动化均使用实际工程 `E:/agent/game/ue/SkillLab/SkillLab.uproject`，没有借用隔离工程 DLL。证据目录为项目 `Saved/Logs/`；原始日志与哈希不复制进长期工作流正文。

| 证据 | 结果及解释 |
| --- | --- |
| `BehaviorFixBaselineBuild.log`、`BehaviorFixBaselineTests.log` | 修前生产代码加定向测试后构建成功；DeathReset、SubmissionEventTimeout 均 Fail。真实伤害 Health=0 后 Reset 日志完成、Health 恢复，但 Dead 仍在，新请求仍为 CasterDead；缺省/零 Active 的事件点目录被接受。基线测试另有一条误以为 ResetCompleted 会发 NativeSkillEvent 的测试断言，已纠正为资源、状态及门控断言，不将该条视为生产缺陷证据。 |
| `BehaviorFixBuild.log`、`BehaviorFixTests.log` | 首次修后实际工程构建成功；15 项中 14 成功。唯一失败为旧规则窗口诊断 fixture 缺少 Active，被新校验提前拒绝，已补齐 fixture。 |
| `BehaviorFixFinalBuild.log` | 最终实际工程 `SkillLabEditor Win64 Development` 构建 Succeeded，命令退出 0。该次链接包含并行效果任务当时已落盘的 `SkillLabSkillEffectEventAdapter.cpp` 和 `SkillLabSubmittedActionLifetimeTests.cpp`；它们不是本任务修改或专项验收对象。已通知 PM 协调暂停，最终回归重新对本次 DLL 执行。 |
| `BehaviorFixFinalTests.log` | 同一次运行 15 项 Success、0 项 Fail，命令退出 0。日志明确加载实际工程 `Binaries/Win64/UnrealEditor-SkillLabGAS.dll` 和 `UnrealEditor-SkillLabSkills.dll`。 |
| `BehaviorFixFinalSourceHashes.json` | 最终回归版本的 226 个 Source 文件与两个 DLL 共 228 项 SHA-256；交付前复核全部一致，0 项变化。该清单用于定位实际验证版本，不代表对所有源码完成行为验收。 |

最终 15 项为：

- `SkillLab.Skills.`：`DeathReset`、`SubmissionEventTimeout`、`SubmissionDefinition`、`DefinitionValidation`、`CastGameplayEvent`、`MultipleSubmissions`、`ResetResourceCompletion`、`ResetSubmissionDuringCompletion`、`CastIntentLifecycle`、`CastControlAndRules`、`CastRequestReentry`、`UnconfirmedSubmission`。
- `SkillLab.GAS.`：`DamageCalculation`、`ResourceWriteIntegration`、`ResourceExecutionBoundary`。

DeathReset 断言覆盖真实死亡、Dead 标签、新激活拒绝、旧 Cast/Ability 结束、Reset 后 Health/Energy 恢复、Dead 清除、缓存清槽、旧事件拒绝，以及新身份施法、正式释放和一次扣费。事件超时测试覆盖两种缺事件路径的单次取消、原因、Ability/Cast 结束、缓存清理及迟到事件拒绝；首点已提交时不退款、不清冷却、不补第二点，未释放时不扣费、不启冷却。现有 MultipleSubmissions 继续证明正常后续事件到达可完成、重复点不重复收费。

最终 DLL SHA-256：

- GAS：`7D50C9A03A635B4CC75232E51BED9CB5B5F32310A3B615F069DF4DA55D12A306`
- Skills：`4955BB1452DDE0254477F6D16394893A040E11D1C28656D54DE51E01DE7BA97F`

复现命令：

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -unattended -nop4 -NullRHI -nosplash -nosound '-ExecCmds=Automation RunTests SkillLab.Skills.DeathReset+SkillLab.Skills.SubmissionEventTimeout+SkillLab.Skills.SubmissionDefinition+SkillLab.Skills.DefinitionValidation+SkillLab.Skills.CastGameplayEvent+SkillLab.Skills.MultipleSubmissions+SkillLab.Skills.ResetResourceCompletion+SkillLab.Skills.ResetSubmissionDuringCompletion+SkillLab.Skills.CastIntentLifecycle+SkillLab.Skills.CastControlAndRules+SkillLab.Skills.CastRequestReentry+SkillLab.Skills.UnconfirmedSubmission+SkillLab.GAS.ResourceWriteIntegration+SkillLab.GAS.ResourceExecutionBoundary+SkillLab.GAS.DamageCalculation' '-TestExit=Automation Test Queue Empty' '-abslog=E:/agent/game/ue/SkillLab/Saved/Logs/BehaviorFixFinalTests.log'
```

## 未验证项与交接限制

没有运行可见游戏、PIE、网络/预测、存档、跨进程或所有资产验证；NullRHI 定向结果不替代玩家体验。未复跑完整项目测试集合或生产 RuntimeProbe。F3 的缺省/零 Active 路径以加载拒绝回归关闭，不宣称修前无限等待已做长期计时测量；合法正 Active 定义的真实运行超时已执行。

最终日志启动阶段仍有 13 条 `Condition failed`，另有既有环境及 World cleanup 等告警；目标测试随后全部成功，不能称日志无错误。本次没有处理其他 Review 项，也不由这 15 项通过推定并行效果专项完成。剩余源码改动应由各负责人交付，并由 PM 组织独立 Review 与最终集成验证。
