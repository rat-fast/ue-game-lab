# SkillCombatRules 架构交接

## 依据与状态

需求来源：[策划入口](../../../../Planning/SkillSystem/Batches/SkillCombatRules/README.md)及 `SkillCombatRulesRequirements.md`。本案独立于 `SkillChainRefactor`。下列运行链与缺口是实现前核实快照；用户已确认行为以策划需求第 8 节为准，当前实现/验证事实见[实现摘要](Implementation/CombatRulesImplementationSummary.md)。

## 核实运行链

玩家快捷键由 `SkillLabSkillShortcutComponent` 交给 `SkillLabSkillInputComponent`。单位目标立即调用 Runtime，地面技能先生成预览，左键确认调用 Runtime，Escape 只取消地面预览。右键非瞄准时交由 `SkillLabPlayerInputComponent` 做导航移动；移动没有经过技能 Runtime 仲裁。

请求经 `USkillLabSkillRuntimeComponent::SubmitSkillRequest` 解析定义、执行门控、目标校验并建立单个 `CastId`，再通过 Loadout 授予的 GAS Ability 调用 `TryActivateAbility`。当前 `ActiveCastId != 0` 时 Runtime 先以 `SkillBusy` 拒绝新请求，GAS 不会收到该并发激活尝试；所以现状不能证明 GAS 的 Ability Tags 对并发技能有仲裁作用。

当前 Cast 由 `FSkillLabCastLifecycle` 串行推进 `Windup -> Active -> Recovery -> Completed`，可配置时长为零，但状态表达仍固定为这三段。JSON 的 `releaseTrigger` 只能选择 Timer 或 GameplayEvent，当前一次 `TryMarkRelease` 只允许一次释放；行为可异步完成，Active 时限到达则取消 Cast。`SkillLabSkillGameplayAbility` 按施法绑定事件 Tag，通过 AbilityTask 等待 Gameplay Event；Runtime 还校验 CastId、Ability、Runtime 身份。裸 ASC Event 或迟到/重复 Notify 不会直接推进当前 Cast。当前配置以 `SkillLab.Event.Cast` 对应释放点，未提供多个效果提交点。

释放前 Runtime 再查角色生命和当前技能 `blockedBy` Tag，释放时提交能量与冷却，然后执行器调用权威目标/效果路径。即时伤害/治疗由 `SkillLabGAS` 的 ASC 与 AttributeSet 结算；持续伤害以 target ASC 上的 Active GameplayEffect 运行，其句柄由 Skills 的效果事件适配器观察。Cast 结束不自动移除该持续效果；Runtime Reset/EndPlay 会清理适配器并移除其跟踪的效果。

## 已核实事实与缺口

| 主题 | 当前源码事实 | 对本案的影响 |
| --- | --- | --- |
| 新请求与当前 Cast | 每个 Runtime 只保留一个活动 Cast；活动期间的新请求返回 `SkillBusy`。 | 需扩展请求仲裁和当前 Cast 的策略入口；不能仅调整 GAS ActivationBlockedTags 达成技能级操作规则。 |
| `blockedBy` | Provider 校验 JSON 标签已注册；`CheckGates` 在激活前和释放前用 ASC 当前标签拒绝，原因分别为 `BlockedByState`；成功启动后 Runtime 监听死亡及当前定义的 blocked 标签，标签计数升至正数即取消 Cast。 | 现状同一列表既阻止新激活，也打断当前 Cast，没有窗口、事件类别或差异化原因策略。不得继承为目标规则。 |
| Ability 标签与取消 | Loadout 以 `GiveAbility(FGameplayAbilitySpec(AbilityClass, 1))` 授予；Ability 类仅设 `InstancedPerActor` 与 `LocalOnly`，未设置 Ability Tags、ActivationBlockedTags、ActivationOwnedTags 或 CancelAbilitiesWithTag/BlockAbilitiesWithTag；代码未调用 GAS 标签取消接口。Runtime 终止时显式 `FinishSkillAbility`。 | GAS 身份/生命周期可复用；策略需由 Runtime 的公共 C++ 仲裁执行，并与 Ability 终止/取消同步。 |
| 受击 | 源码未发现 Actor 受击/伤害回调向技能 Runtime 或 Ability 发出事件。现有 `Hit` 是技能效果执行结果事件，不是施法者受击输入。 | 新受击仲裁需要一个由权威战斗入口产生的结构化事件接口；本案不得把技能命中事件复用为受击。需另核对未来伤害来源。 |
| Tag 来源 | `Dead` 由 AttributeSet 按生命值通过 ASC loose tag 加/移除；`Stunned`、`Silenced` 目前只声明原生 Tag 并出现在 JSON blockedBy，未发现其授予方。ASC 查询已存在标签。 | 状态产生方是独立缺口；不能让 Tag 名称自动触发动作。规则读取有效状态，显式策略负责结果。 |
| 多来源与持续效果 | GAS 持有持续效果句柄及标签计数能力；当前技能持续伤害 GE 使用 `AggregateByTarget`、StackLimit 1、成功应用刷新时长，表现追踪器以句柄跟踪。没有可验证的控制状态 GE、多来源状态授予/移除场景；当前效果样例不能证明来源独立语义。 | 只有实际增加持续状态效果时，才需要核验 GE 聚合、标签计数及来源清理契约；不要新增另一套通用 Tag 计数器。周期伤害不由 Cast 终态持有。 |
| 输入缓存/移动取消 | 新技能键立即请求，未缓存；单位目标即时提交，地面目标在确认前属于独立选点状态；Escape 取消的是选点预览。右键导航移动未询问技能 Runtime。 | 缓存、主动取消窗口和移动策略属于新仲裁调用方接入；任何缓存结果仍须经正式请求及激活门控。 |
| 资源与冷却 | 当前在释放点 `bCommitted` 后扣能量并写入冷却；取消终态不撤销这些状态。释放前取消不扣费。 | 取消前后退款/冷却语义需产品确认，再决定提交时点或回滚机制。 |

## 实现拆分建议

**核心实现任务：共享仲裁 + Cast 时序/事件接入（建议同一实现单元）。** 两者共同围绕 Runtime 单活动身份、请求生命周期和 Ability 终态工作：仲裁决定要拒绝、允许、继续、取消当前 Cast 或（若获批）暂存请求；时序层必须在有效 Cast/窗口中执行它并保持事件、提交、清理和终态一致。分给两个实现单元会共享 Runtime/Definition 公共接口，且在请求结果无法独立验证。先交付无玩法默认值的 C++ 规则上下文、确定性匹配/原因与决策执行接口，再按确认后的策略填充内容。不要把规则匹配、Ability 生命周期或实际效果授权交给 Lua。

建议窄接口包含：运行事实快照（当前 Ability/Cast 身份、显式时序窗口、技能/操作类别、ASC 有效状态 Tag、事件语义和请求意图）；策略选择（来自已校验技能内容声明的共享规则引用和差异）；确定决策（允许/拒绝/继续/取消当前/等待或缓存/消费缓存及可观察原因）；C++ 执行端再次验证 Cast 身份并唯一执行相应结果。若缓存规则未获确认，先不实现缓存路径。Lua 只能组合已校验的规则引用或调用受限请求接口，不能报告已受理、提交、效果或终态。

**有条件的独立状态来源任务：GAS/状态效果授予与清理。** 可与核心并行核对，但只有产品确认本批需要新增可持续状态，或现有实际授予方确认后才形成实现任务。其接口契约是通过 GAS 权威状态/GE 来源维护目标 ASC 的有效状态；核心仲裁只读有效状态与转变事件，不持有或移除独立效果。用两个独立来源、各自移除和最后来源结束验证标签仍有效/解除。若复用 GAS 原生计数和 ActiveEffect 生命周期即可满足，不新增自研 refcount 框架。状态效果叠加、刷新、免疫规则不在本案自动决定。

表现模块不是实现依赖：它只消费 Runtime 结果；本案不扩展新表现体验。玩家输入接入应在核心决策契约稳定后修改 `SkillLab` 输入组件，保留移动、地面确认/取消和正式请求身份边界。

## 已确认规则与剩余接线问题

用户已确认：闪避请求取消当前 Cast 后立即尝试正式激活；普通受击不打断、重击打断；沉默/硬直阻止新激活并取消当前 Cast；移动和主动取消不打断；Recovery 期间拒绝激活新技能。输入缓存是单槽 latest-wins，只保存下一技能意图；Recovery 内不激活，但自然完成的 Cast 应保留缓存直到退出 Recovery，待旧 Cast/Ability 清理后重新经过正式门控。取消/死亡/Reset 清除缓存。资源/冷却在提交时发生，提交前取消不扣，提交后不退款；Notify 超时或 Ability 中断取消且不补效果；提交点每 Cast 至多一次，重复事件不取消 Cast。

普通/重击由权威战斗事件上的 Gameplay Tag 分类，不按伤害量推断。实现前没有受击事件进入 Runtime，也未发现 Stunned/Silenced 的生产授予者。当前权威即时伤害 API 可显式携带类别 Tag 并通知目标 Runtime；没有生产攻击调用者提供类别。Stunned/Silenced 仍没有生产授予路径。Tag 只携带语义，Runtime 仲裁才执行结果；详见实现摘要。

已确认策略的职责边界：共享 Runtime 入口按操作类别、有效 ASC 状态和生命周期结果仲裁；不按 SkillId 分散公共规则，也不把执行/结算授权交给 Lua。当前阶段枚举仍表达 Windup/Active/Recovery，规则仅在已确定的 Runtime 请求、受击和 Cast 终态入口执行；本专题没有把阶段改造成可配置语义窗口。

## 验收入口与证据缺口

本轮重用并扩展 `SkillLab.Skills.CastGameplayEvent`、`SkillLab.Skills.CastLifecycle`，并运行 `SkillLab.Skills.DefinitionValidation`、`SkillLab.GAS.DamageCalculation`；新增仲裁与缓存自动化证据见实现摘要。多来源状态、Notify 到期和可见 PIE 场景仍有验证缺口。

新增定向自动化建议覆盖：无固定三阶段的时序配置/事件推进、重复提交拒绝、释放/命中/结算事实分离、过期事件不能推进新 Cast、共享策略与单技能差异不互相污染、拒绝/取消原因稳定；按产品已确认项覆盖输入/事件到决策再到当前 Ability 终态；若新增状态 GE，则另测两个 GAS 来源逐个移除和末来源移除。测试经正式 Runtime、GAS 请求及结果接口，不直接伪造已受理或效果结果。

可见 PIE 仍需观察 Cast/释放/取消与最终效果、移动与地面选点取消路径及原因/资源状态。当前没有本轮可见 PIE 证据；自动化与 NullRHI 不能替代视口验收。用户已确认玩法，无需再等待语义决策。

## 实现必读源码

- `Source/SkillLabSkills/Public/SkillLabSkillTypes.h`、`Public/SkillLabSkillRuntimeComponent.h`、`Private/SkillLabSkillRuntimeComponent.cpp`：定义、请求结果、单 Cast 运行链、门控、计时/提交、Tag 监听和清理。
- `Public/SkillLabCastLifecycle.h`、`Private/SkillLabCastLifecycle.cpp`、`Public/SkillLabSkillGameplayAbility.h`、`Private/SkillLabSkillGameplayAbility.cpp`、`Private/SkillLabSkillLoadout.cpp`：生命周期身份、Ability 激活/事件/终止和授予。
- `Private/SkillLabSkillDefinitionProvider.cpp`、`Config/SkillDefinitions.json`、`Private/SkillLabSkillEffectExecutor.cpp`、`Private/SkillLabSkillEffectEventAdapter.cpp`：当前内容校验、配置样本、效果与持续效果观察生命周期。
- `Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h`、`Private/SkillLabAbilitySystemComponent.cpp`、`Private/SkillLabCombatAttributeSet.cpp`、`Public/SkillLabGASTags.h`：ASC 标签/GE 能力、伤害结算、当前状态授予者。
- `Source/SkillLab/Private/SkillLabSkillInputComponent.cpp`、`Private/SkillLabSkillShortcutComponent.cpp`、`Private/SkillLabPlayerInputComponent.cpp`、`SkillLabPlayerController.cpp`：技能、地面选点、Escape 与移动入口。
- `Source/SkillLabSkills/Private/Tests/SkillLabCastLifecycleTests.cpp`、`SkillLabCastGameplayEventTests.cpp`、`SkillLabDefinitionProviderTests.cpp` 及 `Docs/ProjectContext/Modules/TestAndVerification.md`：现有可复用自动化与运行验证入口。
