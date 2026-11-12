# SkillLab 代码独立调查

调查日期：2026-11-12（Asia/Shanghai）。调查者职责：代码调查。工作目录：`E:/agent/game`。本报告只对本次保存的文件状态和已有运行材料作判断，不代表重新执行验收。

## 1. 结论

1. **比较基准应为 `803d1c7699c2639ba4bc72d36cb84874e73f89f7`（当前 HEAD），不能默认 HEAD~1。** HEAD 是 2026-11-10 的总控 Skill 调整；其父提交 `1ec0dee40e383f61ea0e7a48936fd1a70446109b`（2026-11-06）是前次功能提交。二者之间只有两份总控 Skill 差异，SkillLab 正式项目内容相同。因此本报告以 HEAD→当前工作区核对全部未提交变化，必要时以 1ec0dee 说明项目功能基准。
2. **工作区的主体确实是一轮较大的战斗规则补全，不只是四技能参数修改，也不是全仓 3406 份文件都属于新功能。** 正式项目变动源码/测试共 89 份（35 修改、54 新增），配置 5 份，需求/事实文档 7 份，任务交接/交付文档 10 份。另有两份仓库 Skill 修改、一个 Python 缓存，以及三个 B1 验证工程副本合计 3292 个未跟踪文件，约 1.13 GB。副本不得计作 3292 份新生产实现。
3. **核心机制有当前需求依据，主要职责分配合理。** 每 Cast 上下文解决允许并行时的单实例覆盖；共同仲裁解决旧 Cast 限制被新 Cast 绕过；多点声明/执行解决固定 point=1；独立投射物寿命解决 Cast 结束后已交付动作被误清；资源预留、生产结果和独立结算历史分别承载需求 8.1 的不同事实。没有因看到“框架”“账本”“身份”就认定过度设计，也未发现这次新增万能编辑器、跨进程恢复或无关技能内容的证据。
4. **已有代码和运行证据支持已覆盖的功能接入完成；不支持整批所有玩法和视觉验收完成。** 本次独立核对原始日志、代码及三方哈希，最终 416 个 Source/Config/Content 文件与保存的最终验证清单、临时集成工程完全一致。23/15/8/2/1 项测试分属不同版本，不能相加成最终版一次全套通过。当前实际工程也已有成功构建、正确 DLL 哈希、四技能 HUD 截图和 Mend 正式提交日志；PIE、兼容双 Montage 并播、完整键鼠和全部视觉体验仍没有足够证据。
5. **发现值得后续评估的问题，但不能据此把本轮整体判为无效。** 当前 Reset 恢复生命后可能保留死亡 Tag，是有直接源码依据的继承风险；大型 GameplayEvent 测试的职责集中仍未实质解决；权威需求中的部分“尚未实现/未验证”文字没有随当前证据更新。前者未动态复现，且 HEAD 已有同类路径；后两者分别是维护结构和交付可读性问题。本调查未安排修复。

## 2. 范围、方法和取证状态

### 2.1 授权与适用规则

自行读取了根目录 `AGENTS.md`、`.codex/skills/ue-project-context/SKILL.md`、`ai-native-programming/SKILL.md`、`ai-program-review/SKILL.md`、`git-repository-management/SKILL.md`，以及 SkillLab 的 Docs/README、ProjectContext/README、ProgramArchitectureContract。按 Review 角色只读调查理解规则；没有启动实现或派发工作流。

需求依据为 [SkillCombatRulesRequirements.md](E:/agent/game/ue/SkillLab/Docs/Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md:1)，尤其 2–6、7.1、8、8.1、8.2。取得 SkillSystem 基线 README 和所有权/清理正文，结合当前需求解释，不把旧基线“尚未测试”状态强行当成当前代码事实。

架构依据为本轮 [TimingRulesHandoff.md](E:/agent/game/ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/TimingRulesHandoff.md:18)、[SubmissionEffectsHandoff.md](E:/agent/game/ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/SubmissionEffectsHandoff.md:15)、[VerificationHandoff.md](E:/agent/game/ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/VerificationHandoff.md:1)。旧任务所有权以本轮 README 的收敛说明为准，不将旧分工视作当前要求。

### 2.2 实际检查

- Git 只读检查：status（含 `--untracked-files=all`）、log、HEAD 提交 stat、1ec0dee→HEAD 路径差异、未暂存 diff、cached diff、LFS status；没有 fetch、暂存、提交或推送。
- 全量枚举未忽略的修改/新增路径，保存大小、修改时间和 SHA-256。保留全部正式项目相关变动的原始文件副本及 tracked diff；按职责审查核心源码和直接依赖，而非逐行审计所有测试/全部工程。
- 独立沿请求→仲裁→Ability→事件→提交→效果→终止，以及资源写入→预留→结算→迟到反馈、Reset attempt→最终回执、Montage instance→Notify 身份三条重点链取证。
- 已有实现摘要、Review、资源审计用于找到日志和疑点；关键结论另用真实代码、原始日志和本次计算的哈希核实。没有读取另一项本次调查的报告，没有联系另一调查者，没有研究开发历史对话替代代码判断。
- 未启动游戏/编辑器、未编译、未运行 UE 测试或资产命令；只查看已有截图和日志。保存自己的报告、摘录、快照和哈希附件。

### 2.3 时点与复核附件

首次规则/Git 读取从上海时间 22:34:53 开始。初始文件清单与 Git 快照完成于约 22:36:06；正式相关 113 份变动文件复制完成于 22:41:07，副本 SHA-256 与初始清单全部一致。结束复核见 `CodeEvidenceFinalCheck.json`，如共享工作区后续变化，应以本目录快照和清单限定本结论。

附件均在本报告同目录：

| 文件 | 用途 |
| --- | --- |
| `CodeGitStatus.start.txt` | 每一个跟踪修改和未跟踪文件的初始清单，包含三个验证副本 |
| `CodeEvidenceManifest.start.json` | 3406 份初始变动文件的路径、大小、mtime、SHA-256、状态；不含本调查产物 |
| `CodeTrackedChanges.start.patch` | HEAD→工作区的正式项目/Skill tracked 文本差异 |
| `CodeSnapshot/` | 正式相关 113 份变动文件的原样副本，保留原目录；包括未跟踪源码和本轮文档 |
| `CodeSnapshotCheck.json` | 副本与初始清单一致性，本次差异 0 |
| `CodeChangeCounts.json` | 按正式源码、测试、配置、文档、副本分类的数量与字节数 |
| `CodeValidationSnapshotCheck.json` | 最终 416 文件与原验证清单及隔离工程的本次三方核对结果 |
| `CodeRuntimeEvidence.extract.txt` / `.manifest.json` | 12 份原日志的来源路径、SHA-256、原行号摘录；摘录不替代未采样的整份日志 |
| `CodeHUDDeliveryEvidence.png` | 已有实际工程 HUD 截图副本；本次已目视检查 |
| `CodeBaselineRuntime.HEAD.cpp.txt` / `CodeBaselineAttributes.HEAD.cpp.txt` | HEAD 中 Reset 与死亡 Tag 的原源码，便于区分继承问题 |
| `CodeEvidenceFinalCheck.json` | 22:45:58 复查 HEAD、index 和正式相关文件：HEAD 未变、暂存为空、113 文件漂移 0 |
| `CodeVersionEvidenceComparison.json` | 23/15/8/2/1 项所用五份冻结清单的独立哈希与相邻版本路径差异 |

上述快照不复制三个完整验证工程，也不把 UE 生成物当生产源码。

## 3. Git 基准和实际改动覆盖

### 3.1 为什么选 HEAD

`git log -8` 显示：

| 提交 | 作者时间（上海） | 含义 |
| --- | --- | --- |
| `803d1c7699c2639ba4bc72d36cb84874e73f89f7` | 2026-11-10 01:01:36 | `skills: 精简总控任务派发与上下文交接规则` |
| `1ec0dee40e383f61ea0e7a48936fd1a70446109b` | 2026-11-06 01:53:30 | `feat: 完善技能战斗规则与施法生命周期` |
| `66279ff172fccabc2ac96c874bceca5da1f4f058` | 2026-11-04 23:00:32 | `feat: 重构技能链配置与运行时` |

`git diff --name-status 1ec0dee HEAD` 只有 `.codex/skills/ai-game-feature-orchestrator/SKILL.md` 和 `ai-program-workflow-orchestrator/SKILL.md`；不是当前未提交的 architecture/review 两份 Skill。正式 SkillLab 项目基准未在 HEAD 改变。若把 HEAD~1→工作区当作全部本轮变化，会混入 HEAD 已提交的两份总控 Skill 修改。

初查在 `main`，49 份 tracked 修改全部未暂存；`git diff --cached --stat` 为空。无 tracked 删除或重命名。未跟踪目录展开后另有 3357 份文件，合计 3406。LFS status 未显示待提交的正式资产变化；本次 tracked 差异没有正式 Content 资产、`.uproject`、Build.cs 或依赖声明修改。未穷举忽略的 Saved/Binaries/Intermediate 等生成内容作为 Git 变更；它们仅用于已有运行证据。

### 3.2 数量和范围混入

| 类别 | tracked 修改 | 新增未跟踪 | 合计 | 判断 |
| --- | ---: | ---: | ---: | --- |
| 正式非测试源码 | 31 | 27 | 58 | 跨 Skills/GAS/Presentation/玩家输入的功能接入 |
| 正式测试/夹具 | 4 | 27 | 31 | 正式请求、身份、资源、生命周期等验证 |
| 正式配置 | 5 | 0 | 5 | JSON 共享规则及四 Lua 规则选择 |
| 需求/事实文档 | 7 | 0 | 7 | 两份策划入口/正文，五份项目代码事实文档 |
| 本轮程序交接/交付 | 0 | 10 | 10 | Completion 目录全部新增，含既有 Review/审计 |
| 仓库 Skill | 2 | 0 | 2 | architecture/review 增补上下文与文件拆分约定，非游戏运行代码 |
| Python 缓存 | 0 | 1 | 1 | `tools/__pycache__/`，不属本批功能实现 |
| 三个 B1 验证副本 | 0 | 3292 | 3292 | `ue/SkillLabB1Validation_20261112{,_02,_Final}/`，分别 1096/1096/1100 份 |

正式变动源码/测试 89 份当前共 12294 个物理行：非测试 7772、测试/夹具 4522。这是变动文件当前总行数，不是新增行数。tracked 源码 diff 为 +1837/-1684；54 份新源码/夹具共 6808 行，合计净增 6961 行。Git 的 49 files / +2143/-1848 stat 不含未跟踪文件，不能用它概括实际规模。文件数也不等于独立机制数，部分新增 cpp 是同一 Runtime/Provider/Behavior 的职责拆分。

三个验证副本未忽略文件共 1,128,710,152 字节，包含大量复制的 Content、第三方/插件与旧源码。它们是实际工作区混入物；本次没有删除，也没有据其存在推断浪费或违规。创建目的、是否还有回收价值和构建耗时合理性，需要过程证据，不能只凭这些路径或 mtime 判定。

## 4. 改动作用、必要性与合理性

下列源码路径均位于 `E:/agent/game/ue/SkillLab/Source/`，对应初始快照；更详细的文件清单见附件。

### 4.1 内容与公共运行机制

**事实。** [SkillDefinitions.json](E:/agent/game/ue/SkillLab/Config/SkillDefinitions.json:2) 新增 `Standard`、`StandardRecovery` 共享规则；四份 Lua 第一行选择它们，原 cost/cooldown/magnitude/资源引用未因此另立副本。Provider 把 Lua 声明、技能条目、规则、点定义解析拆到独立单元，唯一目录仍在 [SkillLabSkillDefinitionProvider.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp:12)，候选解析失败不覆盖当前目录。

[SkillLabSubmissionDefinitionParsing.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionDefinitionParsing.cpp:37) 处理无声明旧内容的隐式 point=1；显式点 ID 为正 int32，校验重复、触发、目标组合、注册事件和释放/完成冲突。共享规则解析在 [SkillLabSkillRuleParsing.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuleParsing.cpp:26)，Allow 仅用于技能请求，非技能操作保持单独的决策域。

**判断。** 对应需求 2.1、3、5、7.1、8。文本配置选择规则、C++ 执行公共机制，符合项目分层。不需要为本次类型化校验再预建第二套 schema。现有四技能仍是隐式单点、默认缓存策略；并行/多点能力主要由测试内容证明，不能宣称已创作生产多段或闪避技能。

### 4.2 每 Cast 所有权与共同仲裁

**事实。** [SkillLabCastExecution.h](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabCastExecution.h:11) 持有每次 Request/Definition/Lifecycle/Ability/Behavior、Timer、提交、窗口和效果索引；[Runtime 头](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h:132) 保留角色级执行集合、意图槽、订阅和 Reset 屏障。查询核对 Runtime 对象、generation、Cast，Behavior 查询还核对原 Behavior；无参兼容查询多 Cast 时返回空，不暗选最新项（[Ownership.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeOwnership.cpp:14)）。

[GameplayAbility.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillGameplayAbility.cpp:7) 改为 InstancedPerExecution；事件等待校验完整 token、Ability、Runtime 和 Instigator。新请求在局部构造候选，通过 GAS 激活握手后绑定，不覆盖已有 Cast。`BuildActivationPlan` 遍历所有 token，明确拒绝优先，需等待才缓存，Allow/取消计划分开（[Rules.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeRules.cpp:237)）。缓存下次 Tick 重评估，并给消费请求分配新 ID（[Requests.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp:317)）。

**判断。** 单实例原结构无法真实满足允许 A/B 共存，拆分有实际需求必要性。独立规则合成、单一角色缓存、候选门控和归属终止的边界成立。需要同步广播后的重评估亦有真实依据：原始 `CastCE1BeforeValidation2.log:2203,2211,2219` 记录修前 Fail、预期 97 实际 90；当前 Requests.cpp 的 generation/通知后重算与修后日志的 Success 相互支持。这是本批开发中真实缺陷及定向修复的证据，不应将同一验证反复描述为无意义重复，也不应把修前结果当成当前缺陷。

### 4.3 多提交、已交付动作和持续效果

**事实。** [SubmissionPointExecution.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionPointExecution.cpp:59) 以声明点、token、触发事实进入唯一 Begin→执行→Resolve/Abort；成功点拒绝重复，明确失败可显式重试。Runtime 成功效果索引按 point+目标，而非整个 Cast 只准同目标一次（[Execution.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:439)）。首次成功计费，后续点 transaction=0。

投射物先初始化/BeginPlay、记录真实交付，再开放碰撞；命中通过 [SubmittedEffect.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmittedEffect.cpp:7) 消费已交付记录和一次性命中权，不要求旧 Cast 仍在执行，不重写交付回执。普通 [Behavior 清理](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionPointExecution.cpp:171) 解绑动作，只有尚未确认交付者销毁；持续 GE 仍由目标 ASC 持有。源码测试 [SubmittedActionLifetimeTests.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabSubmittedActionLifetimeTests.cpp:40) 确实经真实投射物、世界 Tick 和周期 GE 检查取消后伤害、Runtime 替换和自然移除，并非只塞 bool 成功。

**判断。** 对应需求 2、6、8.2 的“提交、命中、施法终态和独立效果分别存在”，属于必要接入。Behavior 点执行状态与 Lifecycle 的成功提交集合承担不同职责，不是仅凭多张表就可认定第二套权威；尚未逐行验证每一异常分支。

### 4.4 未确认结果、资源与 Reset

**事实。** Runtime 提交前预留，首次真实成功结转一次费用/冷却；动作 false 只有真实 ConfirmedRejected 才授权失败释放，不能把丢失子结果当失败（[RuntimeSubmission.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeSubmission.cpp:31)）。[RuntimeUnconfirmed.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeUnconfirmed.cpp:13) 先交接原身份，再设置 1 秒有限等待，超时结束 Cast，不释放未知 reservation。

[AbilitySystemEnergyReservations.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabAbilitySystemEnergyReservations.cpp:25) 和 [EnergySettlement.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabEnergySettlement.cpp:5) 分别维护 reservation 和独立闭合历史；会话 [SubmissionReconciliation.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionReconciliation.cpp:36) 同时查真实生产记录及资源历史，核对完整身份，未闭合继续隔离，不再扣第二次，冷却按 AcceptedAt/确认时刻恢复。当前反馈仍保留原 Key 和 RequestId（[RuntimeConfirmation.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeConfirmation.cpp:26)）。

正式资源写入协调器在受控写入内排队并重验证，不能使实际资源低于其他 reservation。直接静态 Energy/MaxEnergy modifier 在 GE 应用前拒绝；execution 的实际资源输出在 [AttributeSet.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:42) veto，非资源 execution 没有继续全局禁用。[ResourceEffects.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabResourceEffects.cpp:60) 的 RequestResourceEffect 明确只接受纯即时单资源数值，不冒充完整 GE 支持。

[Runtime Reset](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp:213) 在取消前登记 attempt/ASC/generation；Queued 保持屏障，最终回执核对身份；仅 Applied 且无未决清冷却。Health/效果清理与能量写入不构成整体事务，Rejected 不自动补偿或重试。此边界与交接一致。

**判断。** 第 8.1 节明确要求“未知既不是失败也不是成功”、跨 Reset 保留、迟到收敛、去重及销毁保护，因此这些机制有直接用途。生产结果、资源事实和通知去重各有管理者；不应将它们删成单个 `bSucceeded`。合理性不等于所有边界已被证明：任意原生非 virtual 写入旁路、execution calculation 自身直接改属性/外部副作用、复杂资源 GE 的完整语义没有得到全面保证，当前代码和交接也不承诺这些能力。

### 4.5 表现和输入

[AnimationComponent.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabPresentation/Private/SkillLabSkillAnimationComponent.cpp:98) 按真实 MontageInstanceID、AnimInstance、资源、Mesh、原 token 核对 Notify；停止只作用于原播放实例。Lua 宿主将当前回调 token 传给 Play/Stop，不让数字 CastId 借用新执行。此改动是多 Cast 的必要直接依赖，不是把业务裁决交给表现。玩家 MoveToCursor 在 [PlayerInputComponent.cpp](E:/agent/game/ue/SkillLab/Source/SkillLab/Private/SkillLabPlayerInputComponent.cpp:216) 接正式 Movement 判断，避免移动输入旁路公共规则。

四个生产 Montage 仍共用 DefaultGroup/DefaultSlot；原始 23 项日志 `CastExecutionFinalValidation2.log` 的 CastAnimationResources 输出可定位资源组和 Notify，当前测试源码只加载读取资源。因此 token 隔离、业务多 Cast 与视觉并播必须分别评价。同组资源被引擎替换不等于业务取消，旧 Cast 最终可走 Notify 超时；这是一项真实资源/体验限制，不是已有双组资源的完成证明。

## 5. 完成情况：本次独立核对的证据

### 5.1 快照一致性

本次重新计算 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/ResetImmediateRejectedHashes.json` 的 SHA-256：`7A0A64A063F094D9D37FDBD5288EB90DE749493A261793BAF405E054C220ACA1`。其 416 项为 Source 224、Config 43、Content 149；正式工程、临时集成工程、清单三方逐文件一致，正式工程额外文件 0，uproject 双方一致。详见 `CodeValidationSnapshotCheck.json`。这是本调查实际执行的只读哈希核验，不是引用旧 Reviewer 的核验结论；不覆盖引擎、Plugins、运行进程内存或本轮文档。

### 5.2 原始日志分版本结果

以下日志根目录均为 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/`；时间由日志 UTC +8 换算为上海时间，原行号保持。

| 原始日志及原行号 | 实际结果 | 本报告解释 |
| --- | --- | --- |
| `CastExecutionFinalValidation2.log:2119–4022,4047,4050`，11-12 02:09:26–29 | 23 个不同目标逐项 Success，23 tests performed，退出 0 | 覆盖原 B1/多 Cast/多点/控制/寿命等；属于重入修复前快照 |
| `CastCE1BeforeValidation2.log:2203,2211,2219`，02:24:05 | CastRequestReentry Fail，能量断言 97/90 | 真实修前证据；进程退出不能替代断言 |
| `CastCE1FinalValidation.log:2111–3820,3845,3848`，02:27:39–43 | 15 个不同目标 Success，15 tests performed，退出 0 | 当前请求重入修复后的定向回归，不冒称重跑解析/GAS 全套 |
| `ResetResourceFinal3Validation.log:2116–2641,2666,2670`，20:38:26–29 | 8 项 Success，含 ResourceExecutionBoundary/WriteIntegration/ResetResourceCompletion | R+Runtime Reset 接入交叉证据 |
| `ResetEvidenceFinalValidation.log:2170,2202,2208,2211`，20:46:23 | 两项 Reset 补证 Success，退出 0 | 后续仅变动 Reset 测试，不冒称旧 8 项重新执行 |
| `ResetImmediateRejectedValidation.log:2183,2194,2197`，20:51:49 | ResetResourceCompletion 一项 Success，退出 0 | 当前最终 416 文件清单对应补证 |
| `FinalProductionRuntimeProbe.log:1758,1760`，20:57:39 | Probe COMPLETE，退出 0 | 同一最终代码的 NullRHI 生产内容样本，不是可见 PIE |

本次另外独立解析五份冻结清单（`CodeVersionEvidenceComparison.json`）：23 项版 408 文件；15 项版 409 文件，仅修改 Requests.cpp 并新增 CastRequestReentry 测试；R+Reset 8 项版 416 文件，相对前版新增 7、修改 8；随后 2 项和 1 项版均为 416 文件，相邻版本分别只修改同一个 Reset 测试文件。旧版目标 Success 因而有明确版本条件，最新两次补证没有再次改生产代码。该比较核对保存的清单内容，不等于本调查重新构建旧版或证明历史二进制完全可复现。

上述原日志测试 Success 与启动期 13 条 `Condition failed` 同时存在；Probe 另有 NiagaraToolsets Python AttributeError（1541–1549 行）。目标随后确实执行，不可称日志完全无错误，也不能只因存在启动噪声便抹去目标结果。

从源码抽查到的能力与日志相符：真实 GE 两个来源授予控制 Tag，移除一个仍门控，最后一个移除恢复（[CastControlTests.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastControlTests.cpp:44)）；Notify timeout 真正推进 Timer 到期、不计费、清 Timer（[CastExecutionTests.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastExecutionTests.cpp:81)）；独立投射物和周期 GE 经 World Tick 验证。本调查没有据测试名字直接认定覆盖，而是抽读关键前置、操作和断言。

### 5.3 实际工程交付

原始 `E:/agent/game/ue/SkillLab/Saved/Logs/HUDInputRegression_UserSession_20261112.log:2347`（上海 21:30:18）记录旧运行二进制拒绝 `combatRules`；当前源 Provider 支持该字段。随后 `HUDInputDeliveryBuild.log:256` 记录 Result: Succeeded，且日志实际编译了本次 Skills/GAS/Presentation 相关单元，不只是“已是最新”。本次读取实际 `Binaries/Win64/UnrealEditor-SkillLabSkills.dll` 哈希为 `8126AE03637123E3565B91DF6E891B858E4AB55F69CB5E99A0D131BAAEAA807A`，与交付记录一致。

`HUDInputDeliveryGame.log:1708`（22:09:41）为 HP=100/100、Energy=100/100、Skills=4；1778–1797 行（22:13:37–38）存在 Mend 的 Started/效果/SubmissionCommitted/Completed 链。已查看原 HUD 截图，确有血量、能量和四技能槽。由此可支持实际工程的定义加载/技能接入回归已补齐；不能据此断言四技能所有输入、所有失败分支和全部视觉体验已验收。文档转述的用户“技能能放出来”只作为转述，不在本调查中冒充已读原始用户消息。

## 6. 具体问题、风险与重要边界

### 6.1 继承的死亡后 Reset 风险：源码可定位，未运行复现

**事实。** 当前 [AttributeSet.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:83) 只在 PostGameplayEffectExecute 后调用 ClampVitals；死亡时在 107 行加 State_Dead，生命恢复时在 111 行移除。当前 Reset 311 行通过 SetNumericAttributeBase 直接写 Health，没有调用 ClampVitals 或撤销自己产生的死亡 Tag；CheckGates 在 [Requests.cpp:74](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp:74) 即使生命为正仍因 State_Dead 拒绝。

本机 UE 5.8 的 `D:/Epic/UE_5.8/Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent.cpp:460` 把 SetNumericAttributeBase 转给 SetAttributeBaseValue；同目录 `GameplayEffect.cpp:3986–4039` 经 Pre/PostAttributeBaseChange 和数值更新，不执行 PostGameplayEffectExecute。本项目 AttributeSet 没有另行覆盖 PostAttribute(Base)Change 同步死亡状态。只读搜索生产 Source，未找到其他撤销 State_Dead 的实现。

**推断。** 若角色通过正常伤害 GE 死亡，随后执行普通 Reset，可能恢复 HP 并记录 ResetCompleted，却继续因残留 State_Dead 报 CasterDead，直到另一次合法 GE 执行触发 ClampVitals。现有死亡测试或只检验死亡清理，或手工移除 loose dead Tag 后再 Reset（[CastGameplayEventTests.cpp:878](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:878)）；不能证明死亡→正式 Reset→恢复施法链通过。

**归属边界。** HEAD 原 Runtime 的 952–974 行同样直接写 Health；HEAD AttributeSet 的 77/88–95 行同样依赖 GE 后的 ClampVitals。附件保存了原文，因此这不是本次新建账本或多 Cast 引入的确定性回归。若 Reset 本来只承诺恢复部分资源，不承诺恢复死亡后的可施法状态，应先界定该行为；本报告不擅自新增复活玩法要求。最小后续核实是正常伤害致死→Reset→检查 HP/Dead Tag/新请求和 Reset 日志，无需重跑全批。受本次只读授权限制，本调查没有执行此验证。

### 6.2 长测试职责仍集中；Runtime 拆分总体有用但有机械残留

[CastGameplayEventTests.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:144) 从 HEAD 的 357 行增到 903 行；单个 RunTest 混合正式事件、Lua 返回、资源旁路/扣费重入、销毁、操作规则、缓存和死亡等场景，直到 800 多行仍追加规则/清理断言。已有专门 CastControl、ResourceWrite、Unconfirmed 等测试后，该文件仍保留多个独立变化原因；共享目录/World/latent 恢复也与其他 fixture 并存，排查局部失败仍须理解大段共同前置。

这超出“仅行数大”的问题：职责可以分别命名和验证，交接 C2 明确要求迁移旧大测试，当前只完成了部分提取。保留一条正式 Ability 集成链合理，保留 900 行多职责整块没有在当前摘要中找到具体不可拆理由。定位可从 18 行 latent 命令、144 行 RunTest、204–214 行 Lua 夹具、533–603 行资源故障、746 行共享规则、872 行死亡场景复核。它属于维护/诊断结构债务，不足以推翻已运行目标 Success，也不授权本调查拆文件。

Runtime 原主文件由 995 行降至 365 行，Provider/Behavior 也有实际职责拆分，不能说全部只是在搬文件。但 [RuntimeExecution.cpp](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:426) 仍为 555 行，同时承载 Notify、阶段、终止与效果执行编排；Requests/Rules/Execution 开头还重复保留两组 ToPublicStage/EffectReason 静态 helper，其中部分未使用。共享生命周期确实需要扩读，故单凭 555 行不认定阻塞；重复 helper 是机械迁移残留，未来处理局部职责时可一起消除。首轮阅读是否真正缩小应按具体修改任务判断，不再以新 cpp 个数证明。

### 6.3 当前权威需求和基线中的验证状态落后

[需求第 7 节](E:/agent/game/ue/SkillLab/Docs/Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md:99) 仍写“无多提交内容定义或可配置时序窗口”“多来源状态未验证”“Notify 超时未单测”；112 行写第 8.2 节尚待程序侧实现与验证。当前真实解析/测试和原日志已能证明至少对应机制及定向验证存在；这些文字不是当前完成状态的准确单一入口。第 8.2 节验收表本身也明确标注“待验证”。旧基线 README 的未测描述有同类时点问题。

Completion README、项目 TestAndVerification 和追加 Review 较新，但读者必须跨几份文档识别“历史尚未”与“当前已关闭”，部分摘要还留有提交当时“待原 Reviewer 关闭”措辞。需求仍是行为权威，旧验证列不应替代当前代码；同时不能要求下游靠猜测恢复版本。本调查不改现有需求/材料，只记录交付可读性问题。

### 6.4 没有足够证据关闭的体验范围

- 没有本轮完整 PIE、兼容双 Montage 并播、全部键鼠和动画姿势恢复证据；已有实际 HUD 截图、Mend 日志、用户转述和 NullRHI Probe不能填补这些不同问题。
- 测试动态 GE 多来源并不等于生产沉默/硬直属性授予者；本次未发现新增生产授予内容。即时伤害 API 可携带攻击类别，不代表四个生产技能都已有普通/重击分类选择者；不能按数值猜分类。
- 原资产审计和 149 资产快照仅用于定位既有事实；本调查没有启动资产加载、未遍历二进制图或验证专用非 K2 图。正式 Content 无 Git 修改不等于已证明所有动态资源写入路径安全。
- GE 资源输出 veto 不保证混合 execution 整体无副作用或事务回滚；代码中的受控接口也不承诺拦截任意 C++ 原生旁路。进程退出后的持久对账明确不在本批要求内，不能将其列为缺陷。

## 7. 限制和无法核实之处

本报告是对明确时点文件和已有证据的独立代码调查，未进行运行复现、编译或新的功能测试；没有承诺全仓逐行审计、性能/内存压力、网络、多用户并发、任意外部插件/资产、全部 GE execution 或进程崩溃恢复已安全。变动路径已全量覆盖，功能判断重点抽样核心运行链与风险相关测试，其他测试只做注册/日志与必要依赖核对。

未读取开发过程的原始历史对话，不能据文件 mtime、旧摘要、Agent 名称推断某人何时获得什么授权、改动是谁造成、耗时是否浪费或责任归属。需求中的用户确认消息 ID 是仓库给出的来源引用，本报告没有把它升级为自己已核实原始消息。此类过程事实由独立过程调查取证；未向另一调查者取结论。

除第 6.1 节条件性继承风险和第 6.2/6.3 节具体问题外，本次抽样没有发现足以直接宣称整轮功能代码错误或大部分机制无需求依据的证据。这不是无缺陷保证；也不替代剩余体验验收。没有修改生产源码、配置、资产、需求、交付材料或编辑器状态，没有暂存/提交/推送，没有直接安排修复。
