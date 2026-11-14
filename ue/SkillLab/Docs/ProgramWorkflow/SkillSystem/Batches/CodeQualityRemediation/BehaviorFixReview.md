# F1 / F3 行为修复独立 Review

2026-11-13（Asia/Shanghai）。结论：**本批 F1、F3 可以关闭，未发现阻塞问题。** 结论针对 [BehaviorFixSummary.md](BehaviorFixSummary.md) 标识的最终验证版本及本批修改；不代表其他整改、全项目回归或玩家可见验收完成。

本次使用 `ue-project-context`、`ai-native-programming`、`ai-program-review`，独立读取真实实现、测试和已有日志；没有修改被审实现，没有构建、运行自动化、启动编辑器或 PIE。唯一新增产物为本报告。

## 依据与版本

- 问题依据：`docs/Investigations/SkillLab/CodeQualityReview/MainReview.md` 的 F1、F3，以及 `StateAndLifecycleReview.md` 的 SL-03。
- 业务依据：`Docs/Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md` 第 2、5、6、8 节；`Docs/Baseline/SkillSystem/README.md`、`02-RequestLifecycle.md`、`05-OwnershipAndCleanup.md`。重点为明确结束保障、超时取消且不补发、首次成功提交计费及取消/死亡/Reset 清槽。
- 公共约定：项目 Docs/ProjectContext 入口、`ProgramArchitectureContract.md`；验证入口取自 `Docs/ProjectContext/Modules/TestAndVerification.md`。本批范围由整改目录 README、实现摘要和本次委派限定；没有单独的行为修复架构交接文件，不以此作为缺陷。
- 实际审查：摘要列出的六个修改文件，沿直接依赖核对 Reset、属性 setter、Runtime 死亡门控/订阅、事件转发、Active Timer、终止清理、Ability task、提交点解析/执行、目录原子替换、共享 fixture 和正常多提交测试。

本次约 03:02 的只读采样核对了 `Saved/Logs/BehaviorFixFinalSourceHashes.json` 的 **228 项，缺失及哈希变化均为 0**，包括 226 个 Source 文件及两个实际工程 DLL。该清单自身 SHA-256 为 `4482D9477D087AE80A0AC54DA13DE85F3B1566DC58FCCD790D49C3C0D3BE6FDA`。这用于关联本次读取与已有运行证据，不表示独立审查全部 226 份源码，也不保证采样后共享工作区不再变化。

最终 DLL 标识：

| DLL | SHA-256 |
| --- | --- |
| `UnrealEditor-SkillLabGAS.dll` | `7D50C9A03A635B4CC75232E51BED9CB5B5F32310A3B615F069DF4DA55D12A306` |
| `UnrealEditor-SkillLabSkills.dll` | `4955BB1452DDE0254477F6D16394893A040E11D1C28656D54DE51E01DE7BA97F` |

## F1：通过，死亡 Reset 恢复缺陷可关闭

`Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:102` 将 Health 对应的 Dead loose tag 同步放在 `PostAttributeChange`，原 `ClampVitals` 保留数值钳制与资源公共写入。Runtime 的 `CompleteReset` 仍通过正式 `SetNumericAttributeBase(Health, MaxHealth)` 恢复生命值，没有另设手工删除死亡标签的旁路。

静态核对本机 UE 5.8 的 `GameplayEffect.cpp:3986`、`AbilitySystemComponent.cpp:476` 与 `AttributeSet.cpp:72`：Base setter 会更新最终 Current 值；最终属性写入通过 `SetNumericValueChecked` 调用 `PostAttributeChange`。因此修复覆盖原先只在 `PostGameplayEffectExecute -> ClampVitals` 同步标签而遗漏的直接 Base setter 路径。Reset 现有的 attempt、重入身份检查与资源最终回执屏障仍在。

`Source/SkillLabSkills/Private/Tests/SkillLabDeathResetTests.cpp:15` 连续两轮通过真实 `ApplyDamageToTarget` 致死，断言 Health=0、Dead 存在、旧 Cast/Ability 结束及 `CasterDead` 拒绝；随后断言 Reset 恢复资源并清 Dead、清缓存、拒绝旧事件，以新身份激活并经 Ability/GameplayEvent 正式提交、只扣一次费用。没有通过测试手动删 Dead 来制造恢复结果。

基线日志 `Saved/Logs/BehaviorFixBaselineTests.log:2128` 明确记录 Reset 后 Dead 未清及新技能无法激活，支持原 F1 的动态确认。基线另有一条错误的 ResetCompleted NativeSkillEvent 断言，已由作者纠正；本 Review 不将其计为生产缺陷。最终日志 `Saved/Logs/BehaviorFixFinalTests.log:2971` 的 DeathReset 为 Success，结合上述真实路径与断言，足以关闭本项。

## F3：通过，事件等待结束保障缺口可关闭

`Source/SkillLabSkills/Private/SkillLabSkillEntryParsing.cpp:194` 在提交点与 completionMode 校验后，对存在 GameplayEvent 提交点且 `active <= 0` 的内容明确拒绝。无事件提交点的即时、Script、Timer 内容不因新条件被强制配置 Active。`ParseCatalogText` 仍先解析临时候选，失败不会替换调用方目录；旧合法多点与规则窗口冲突 fixture 补充 Active 后仍核对原语义，未删除原诊断覆盖。

正 Active 定义复用既有 `SkillLabSkillRuntimeExecution.cpp:242` 的 Timer：释放后按本 Cast/阶段身份触发 `ActiveTimeout` 取消；`FinishCast` 清本 Cast 的 Timer、行为等待与 Ability，退出执行集合并清缓存。Ability 的 `EndAbility` 结束事件 task。未增加默认时长、第二套调度或内容专属分支，符合原 F3 允许的“加载期拒绝无结束保障配置”修复方向。

`Source/SkillLabSkills/Private/Tests/SkillLabSubmissionEventTimeoutTests.cpp:12` 覆盖缺省/零 Active 的加载拒绝与具体诊断；第 21 行起用同一合法定义分别不发送首个释放事件、首点成功后不发送后续事件，真实推进 World/Timer，核对单次取消及对应原因、Cast/Ability 结束、缓存清槽、迟到事件拒绝。已提交路径保留 93 能量与有效冷却、只有一次提交/效果；未释放路径保留 100 能量且无冷却。正常事件到达、所有点解决、重复点不重扣与冷却不重启由既有 `MultipleSubmissions` 补充覆盖。

基线日志确认缺省/零 Active 原先被接受；它未长期计时复现无限等待，本报告不声称已做该复现。最终日志 `Saved/Logs/BehaviorFixFinalTests.log:3169` 的 SubmissionEventTimeout 为 Success，且 `MultipleSubmissions`、`SubmissionDefinition`、`DefinitionValidation` 同次成功。加载拒绝加合法定义的真实超时取消证据，足以关闭 SL-03/F3。

## 验证证据及限制

已核对 `Saved/Logs/BehaviorFixFinalBuild.log` 的实际工程 `SkillLabEditor Win64 Development` 构建 Succeeded；最终运行日志第 1268/1269 行加载 `E:/agent/game/ue/SkillLab/Binaries/Win64/` 的 GAS/Skills DLL。日志逐项记录 **15 Success、0 Fail**，第 3339 行确认执行 15 项，第 3343 行记录退出状态 0。目标集合及复现命令见实现摘要，本次未重新执行。

最终日志启动阶段有 **13 条 `Condition failed`**，测试期间还有 World cleanup 等告警；这不支持“全日志无错误”的表述。目前未见这些消息令本批目标测试失败，但也未在本 Review 中查清其来源。最终构建同时包含当时并行效果任务的 Adapter 与寿命测试文件；本报告不裁定那些改动。后续恢复的效果、历史或字段整改若改变相关直接依赖，PM 应按最终合并版本组织受影响回归，不能把本次版本的成功自动迁移到后续 DLL。

本批六个修改文件均低于约 400 行拆分参考，新增测试各自围绕一个缺陷，复用正式请求/提交/资源路径，没有引入独立账本或混入生产内核的测试行为。未发现需要阻塞的职责、复用或结构问题，也未改变模块边界及正式运行入口。新测试名称已能由实现摘要和源码定位，后续整批文档同步时可纳入现有验证入口表。

本次没有可见游戏/PIE、动画资产、网络/预测、存档/跨进程或完整项目测试证据；不据此确认所有 Health 修改/状态来源、所有超时组合或所有生产技能体验。上述限制不阻塞本次两个有明确范围的代码缺陷关闭。
