# SkillLab 代码质量主审查

## 审查状态

- **审查对象**：`ue/SkillLab` 当前工作树，重点覆盖 `SkillLabGAS`、`SkillLabSkills`、`SkillLabLuaRuntime`、`SkillLabPresentation` 以及角色接入边界。
- **源码状态**：分支 `main`，提交 `937171e7cdc9c8aa4b617b06e19bae114632a370`；审查开始时工作区干净。
- **审查方式**：生产文件、结构/测试边界、状态数据和生命周期专项均已完成静态核对，并已在本报告合并裁决。此前按模块逐文件阅读不能替代这些维度的专项核对。未重新构建、启动编辑器、运行自动化、启动 PIE 或改变运行状态。
- **证据限制**：仓库内已有构建、自动化和有限运行记录只作为背景证据，不替代本轮独立判断；提交未锁定为产品版本，不能把当前代码状态外推为未来版本结论。

## 给策划的总体判断

当前技能链的职责划分总体合理，已经形成了可追踪的事实链：

```text
输入/执行请求 -> Runtime 门控与仲裁 -> Provider/Loadout -> Activation Plan
-> GAS Ability -> Cast Notify/Gameplay Event -> Cast Behavior
-> Submission -> Targeting/Projectile/EffectExecutor -> SkillLabGAS
-> Presentation 消费真实技能事实
```

Lua 表达技能专属的目标形态、关系和效果组合，JSON 承载可调数值，C++/GAS 负责生命周期、门控、命中和结算；`FSkillLabCastExecution` 使并行 Cast 的状态不会互相覆盖；提交、投射物交付、命中、持续效果、资源预留/结算和未决确认分别有独立事实。技能运行模块不依赖表现模块，表现层消费运行事实而不是伪造伤害或命中。这些边界适合继续扩展内容，也使策划变更可以定位到内容、规则或公共机制中的一类。

这并不表示所有异常组合或代码质量维度已经被证明。当前静态审查确认的重点包括死亡后的 Reset 状态恢复、后续 GameplayEvent 提交点缺少结束保障、动态 GameplayEffect 定义和多份终态历史缺少会话内回收、结果字段重复/无效保存、Runtime 重建后的 Ability spec 归属，以及测试专用动作适配入口仍进入生产编译单元；运行时编排职责集中和 903 行综合测试属于维护成本问题。周期效果的正常结束路径已找到，但仍有指针关联和移除载荷差异等条件性风险。有限运行记录明确没有覆盖若干异常、可见 PIE 和生产内容来源。

## 覆盖范围与未覆盖范围

生产文件阅读范围按文件记录如下；结构、数据和生命周期维度已由专项补查并在下文合并，文件清单本身仍不代表动态行为或未列范围已经验证。

### `SkillLabGAS` 生产文件

已阅读实现逻辑：

- `Private/SkillLabAbilitySystemComponent.cpp`
- `Private/SkillLabAbilitySystemEnergyReservations.cpp`
- `Private/SkillLabAbilitySystemResource.cpp`
- `Private/SkillLabCombatAttributeSet.cpp`
- `Private/SkillLabCombatGameplayEffects.cpp`
- `Private/SkillLabEffectResolver.cpp`
- `Private/SkillLabEnergySettlement.cpp`
- `Private/SkillLabGAS.cpp`
- `Private/SkillLabGASTags.cpp`
- `Private/SkillLabResourceEffects.cpp`

已核对公开契约、字段和边界：

- `Public/SkillLabAbilitySystemComponent.h`
- `Public/SkillLabCombatAttributeSet.h`
- `Public/SkillLabCombatGameplayEffects.h`
- `Public/SkillLabDamageCalculation.h`
- `Public/SkillLabEffectResolver.h`
- `Public/SkillLabEnergySettlement.h`
- `Public/SkillLabGASTags.h`
- `SkillLabGAS.Build.cs`

### `SkillLabSkills` 生产文件

已阅读实现逻辑：

- `Private/SkillLabCastLifecycle.cpp`
- `Private/SkillLabFireballProjectile.cpp`
- `Private/SkillLabLuaDeclarationParsing.cpp`
- `Private/SkillLabProjectileTypes.cpp`
- `Private/SkillLabSkillBehavior.cpp`
- `Private/SkillLabSkillBehaviorLua.cpp`
- `Private/SkillLabSkillBehaviorProduction.cpp`
- `Private/SkillLabSkillBehaviorRegistry.cpp`
- `Private/SkillLabSkillDefinitionProvider.cpp`
- `Private/SkillLabSkillEffectEventAdapter.cpp`
- `Private/SkillLabSkillEffectExecutor.cpp`
- `Private/SkillLabSkillEntryParsing.cpp`
- `Private/SkillLabSkillGameplayAbility.cpp`
- `Private/SkillLabSkillLoadout.cpp`
- `Private/SkillLabSkillRuleParsing.cpp`
- `Private/SkillLabSkillRuntimeComponent.cpp`
- `Private/SkillLabSkillRuntimeConfirmation.cpp`
- `Private/SkillLabSkillRuntimeExecution.cpp`
- `Private/SkillLabSkillRuntimeOwnership.cpp`
- `Private/SkillLabSkillRuntimeRequests.cpp`
- `Private/SkillLabSkillRuntimeRules.cpp`
- `Private/SkillLabSkillRuntimeSubmission.cpp`
- `Private/SkillLabSkillRuntimeUnconfirmed.cpp`
- `Private/SkillLabSkills.cpp`
- `Private/SkillLabSubmissionDefinitionParsing.cpp`
- `Private/SkillLabSubmissionPointExecution.cpp`
- `Private/SkillLabSubmissionReconciliation.cpp`
- `Private/SkillLabSubmissionResults.cpp`
- `Private/SkillLabSubmittedEffect.cpp`
- `Private/SkillLabTargetingService.cpp`
- `Private/SkillLabUnconfirmedSubmissions.cpp`
- `Private/SkillLabSkillDefinitionParsing.h`

已核对以下 `Public/` 文件的类型、公开入口和模块契约，以及 `SkillLabSkills.Build.cs`：

- `Public/SkillLabCastExecution.h`
- `Public/SkillLabCastLifecycle.h`
- `Public/SkillLabFireballProjectile.h`
- `Public/SkillLabProjectileTypes.h`
- `Public/SkillLabSkillBehavior.h`
- `Public/SkillLabSkillBehaviorRegistry.h`
- `Public/SkillLabSkillDefinitionProvider.h`
- `Public/SkillLabSkillEffectEventAdapter.h`
- `Public/SkillLabSkillEffectExecutor.h`
- `Public/SkillLabSkillGameplayAbility.h`
- `Public/SkillLabSkillLoadout.h`
- `Public/SkillLabSkillRuntimeComponent.h`
- `Public/SkillLabSkillTypes.h`
- `Public/SkillLabSubmissionResults.h`
- `Public/SkillLabTargetingService.h`
- `Public/SkillLabUnconfirmedSubmissions.h`

测试文件按职责入口和关键分支核对，用于支撑本报告的行为和维护性判断；没有声称逐条重审所有测试断言。

### 相邻模块和明确剩余边界

- `SkillLabLuaRuntime`：只核对 `SkillLabSkills` 直接使用的 facade/opaque 边界，不审第三方 Lua 源码。
- `SkillLabPresentation`：只核对 `SkillLabSkillResult`、Notify、`SkillLabSkillEffectEventAdapter` 和表现消费边界，不做完整 Presentation 实现审查。
- 角色组装、输入和 HUD：只核对 Runtime 初始化及 `SubmitSkillRequest` 入口，不做完整角色、Blueprint 或资产审查。
- 未完成的范围：UE `.uasset`、非 K2 Control Rig、网络多人、全部 Blueprint 动态图、完整可见 PIE、所有测试逐断言、未来内容和外部动态代码。

因此，当前状态应写作“静态 Review 已完成，动态行为和未列入范围的边界仍未验证”。本报告综合了 [状态数据与生命周期专项](/E:/agent/game/docs/Investigations/SkillLab/CodeQualityReview/StateAndLifecycleReview.md) 与 [职责结构与测试边界专项](/E:/agent/game/docs/Investigations/SkillLab/CodeQualityReview/StructureAndTestBoundaryReview.md) 的证据；专项中的源码路径和行号是本报告的事实依据。“未验证”不等于已发现功能缺陷。

## 结构、数据与生命周期综合裁决

上一版遗漏这些问题的原因是审查按模块和运行链建立了文件清单，却没有把“职责分布、字段读写、集合增长、测试边界”作为独立证据维度；“逐文件阅读”只能证明看过实现，不能证明已核对每个字段的权威性、每个终态集合的保留策略或测试入口是否越过生产边界。以下裁决合并两份专项的独立证据，并对术语冲突按源码重新核对。

### 大文件、大类型与首轮阅读边界

- `SkillLabSkillRuntimeExecution.cpp` 共 555 行，至少包含四类可分开修改和验证的职责：事件入口/窗口处理与释放（[68-156](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:68)、[118-156](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:118)），Windup/Active/Recovery 的阶段推进和 Timer（[158-336](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:158)），Cast 终止、Ability 解绑和执行集合收束（[338-406](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:338)），效果应用、命中结果和提交终止（[426-554](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:426)）。这些职责共享 `USkillLabCastExecution`、Runtime 身份校验、事件重入和 `bSubmitting`/`DeferredSubmission*` 状态，不能靠机械移动函数就获得真正隔离；但修改某一类行为时不应默认首轮通读全部 555 行。
- `USkillLabSkillRuntimeComponent` 是 211 行公开头加 365 行实现的编排类型。其状态集合包括 Cast map、Ability binding、Pending intent、Reset attempt、Runtime generation、未决会话和多个 Timer（[SkillLabSkillRuntimeComponent.h:123-165](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h:123)）；实现又分散在 RuntimeComponent、Requests、Rules、Execution、Submission、Confirmation、Unconfirmed 等文件。它是共享状态根，不适合按字段机械拆成多个组件，但当前接口已让多个独立任务共享同一巨型状态对象，形成真实的首轮阅读成本。
- 具体修改的首轮必读范围应按契约选择：Notify/释放先读 `SkillLabSkillRuntimeExecution.cpp:68-258`、`SkillLabSkillGameplayAbility.*`、`SkillLabCastLifecycle.*`；目标/效果先读 `RuntimeExecution.cpp:426-486`、`SkillLabSkillEffectExecutor.*`、`SkillLabTargetingService.*`、`SkillLabGAS` 的效果入口；提交/未决/Reset 先读 `RuntimeExecution.cpp:505-554`、`SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeUnconfirmed.cpp`、`SkillLabSkillRuntimeConfirmation.cpp`、`SkillLabUnconfirmedSubmissions.*`、GAS 预留/结算文件。该矩阵是可独立验证的实际边界，不把约 400 行参考值单独当作判错依据。

综合结论是“职责集中并带来维护压力”的结构观察成立，但是否值得拆分、拆分后能否缩小安全修改面，取决于后续设计能否证明共享状态可由窄化接口隔离。不能把大文件本身升级为行为缺陷，也不能仅凭行数要求重构。

### 两种“周期”的生命周期核对

**周期效果（Periodic GameplayEffect）**：入口在 `USkillLabAbilitySystemComponent::ApplyDamageOverTimeToTarget`，创建带 Duration/Period 的 GE 并保存到 `RuntimeEffects`（[SkillLabAbilitySystemComponent.cpp:148-187](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp:148)）；AttributeSet 记录周期结果，ASC 回调按句柄转发；Skills 的 `USkillLabSkillEffectEventAdapter::Track` 绑定周期伤害和 GE 移除委托（[SkillLabSkillEffectEventAdapter.cpp:6-30](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp:6)），`HandleRemoval` 删除句柄记录并广播终态（[54-66](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp:54)），`PruneListeners`/`Clear` 负责解绑（[68-110](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp:68)）。Runtime EndPlay 解除观察，Reset 才显式移除持续效果；因此“周期入口和结束完全没有路径”未被确认。

已定位的生命周期问题是保留策略：`RuntimeEffects.Add(Effect)` 只增不减，ASC 会话内没有对应裁剪；它可能是为动态 GE 提供稳定 UObject 所有权的合理例外，但长会话反复施加 DoT 会保留每个生成的 GE 对象，内存成本和会话边界没有说明。另有 `PendingPeriodicDamage` 以 `FGameplayEffectSpec*` 关联回调的并发/副本错配风险，仍按待验证风险处理。

**对象/任务/提交周期**：每个 Cast 的 `FPointExecution` 由 `Waiting -> Executing -> AwaitingAction/Resolved/Unconfirmed/Cancelled` 推进；Script/Timer/GameplayEvent 入口都经过 `RequestSubmissionPoint`（[SkillLabSubmissionPointExecution.cpp:26-115](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionPointExecution.cpp:26)），Timer 在 `Cleanup` 清除，未确认投射物会销毁，已确认投射物由自身 Expire/Hit/Destroyed 终结（[SkillLabSkillBehavior.cpp:167-180](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp:167)、[SkillLabFireballProjectile.cpp:66-85](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabFireballProjectile.cpp:66)）。这些正常终态已有归属；真正不明确的是终态历史和测试绕过路径，而非所有 Timer 都缺少结束。

### 字段读写、结果数组与重复集合

- `USkillLabCastExecution::ActiveEffectResult` 在声明处保存单个结果，但生产代码只在 [SkillLabSkillRuntimeExecution.cpp:472](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:472) 写入，没有读取方；它不是 `UPROPERTY`，也没有公开 getter，当前没有反射、委托或序列化间接消费证据。
- `ActiveEffectResults` 在 [SkillLabCastExecution.h:54](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabCastExecution.h:54) 声明，在 [SkillLabSkillRuntimeExecution.cpp:452](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:452) 只取 `Num()` 生成 `SkillEffect_<Cast>_<index>` 名称，并在 [473](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:473) 追加；没有结果消费方。它实际同时承担“无外部需求的历史数组”和“操作序号计数器”两种概念。
- 同一成功结果又被追加到 `SuccessfulPointEffects[PointId]`（同一行 [473](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:473)），其读取方在 [443-450](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:443)，用于同一点按 Target 去重。后者是有实际行为用途的派生索引；前两者没有形成权威结果输出，造成重复存储和修改一致性成本。当前证据支持“确认的结构/数据问题”，但不支持直接删除 `SuccessfulPointEffects`，因为它承担去重语义。
- `FSkillLabProductionResult::Children` 不是同类冗余：父 Area 结果通过 Children 聚合子结果并决定 `AreaEmpty`、`AreaAnyApplied` 或 `AreaAllRejected`（[SkillLabSubmissionResults.cpp:58-100](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionResults.cpp:58)）。`UnconfirmedSubmissions::Records` 也不是生产结果副本，而是跨 Runtime 重建保留的资源/反馈交接状态。

### 测试专用动作适配入口

结构专项 R1 曾将该无参重载描述为公共方法；本次按头文件访问区和唯一调用者重新裁决，正确事实是它处于 `private:` 区域，公共的是 `RequestDeclaredAction()`。

`USkillLabLuaSkillBehavior::ExecuteDeclaredAction()` 在公开头的 `private:` 区域声明（[SkillLabSkillBehavior.h:58-75](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillBehavior.h:58)），不是对外的 public API；不要把它与同一头文件中 public 的 `RequestDeclaredAction()` 混同。实现位于生产编译单元 [SkillLabSkillBehaviorProduction.cpp:9-21](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillBehaviorProduction.cpp:9)，仓库内唯一直接调用者是 `Private/Tests/SkillLabProductionFixture.h:120` 的 `FSkillLabProductionFixture::Execute`；生产运行入口使用的是带 `FPointExecution&` 的重载 [SkillLabSubmissionPointExecution.cpp:91](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionPointExecution.cpp:91)。它没有 `WITH_DEV_AUTOMATION_TESTS` 宏边界，但通过 `friend class FSkillLabProductionFixture` 只给测试夹具访问。

这不是公开生产 API，也不是第二套效果机制：零参适配先取当前 Production handle，再调用相同的动作实现，用于“动作已经发生但同步交付回执丢失”的测试；正式测试随后显式 `AbortBehaviorSubmission`，验证未决隔离。因此其测试意图合理，问题在于测试专用入口和生产实现同编译单元，并且绕过了 `RequestSubmissionPoint` 的正式 Begin/Resolve 包装。应标为“生产边界结构问题/待专项确认”，不能称生产 Runtime 已调用该入口。

### 终态历史与会话内裁剪

| 集合 | 拥有者与键 | 增长来源 | 当前清理条件 | 真实依赖与初步判断 |
| --- | --- | --- | --- | --- |
| `EnergySettlementHistory` | ASC，`TransactionId` | 每次 `BeginEnergyEscrow` | 仅 ASC `EndPlay/BeginDestroy` 结束对象；没有 Remove/Reset | 迟到对账、幂等和 `BindEnergyOperation` 需要终态快照；确认的终态仍永久保留，确认存在会话内无界增长 |
| `UsedEnergyTransactionIds` | ASC，`uint64` id | 每次分配/开始预留（含失败的 Begin） | 未见会话内裁剪 | 防止事务 ID 复用；属于幂等历史，长期会话内无界增长 |
| `AllocatedEnergyTransactionIds` | ASC，`uint64` id | Allocate 后尚未 Begin 的 ID | Begin 时移除；正式提交通常立即 `Allocate -> Begin`，仅外部只 Allocate 不 Begin 的路径会保留 | 临时分配集合，不能算作正常每次施法永久累积的终态历史 |
| `FSkillLabSubmissionResults::Results` | `USkillLabUnconfirmedSubmissions`，`Operation` | 每次 `BeginProduction`，Area 还增加子 operation | `EndSession` 只把 InProgress 标成 Unavailable，不清除 map | Reconcile、迟到结果和父子 Area 聚合需要已终态记录；没有会话内裁剪，结果和 Children 快照持续累积 |
| `Records` | `USkillLabUnconfirmedSubmissions`，数组索引/完整 SubmissionKey | 每次未决交接 `Record` | 只在组件销毁时释放；Resolved/Notified 仍保留 | `BlocksSkill`、迟到确认和 Runtime 重建依赖完整记录；当前记录不会裁剪，`ReportedLostOwnerRecords` 还按索引增长 |
| `PendingFeedback` | 未决组件，`Records` 索引 | 每个已解决待通知记录 | 成功交付后 `Remove`；接收方失效时保留 | 这是暂存集合，正常情况下有终点，不属于主要无界历史 |
| `CooldownEnd` | 未决组件，`SkillId` | 每次已确认成功冷却 | 只有无未决时整体 `ResetConfirmedCooldowns` | 按 SkillId 有界，语义是当前会话冷却快照 |
| `RuntimeEffects` | ASC，动态 GE UObject 数组 | 每次 DoT 创建 | 未见会话内清理，随 ASC 销毁 | 可能是动态 GE 的所有权保活；长会话保留对象，需明确是否可按无活动句柄裁剪 |

因此“多份终态历史没有会话内裁剪”是已确认的结构/容量问题，但不能把它简化成删除历史。最小安全设计必须证明迟到回调、重复确认、同 Operation 查询和 Area 父子聚合在裁剪窗口外不会再到达；本轮不修改实现，也不假设该条件已成立。

### 新测试夹具与 903 行综合测试的重叠

`FSkillLabProductionFixture` 已把正式请求、真实 `BeginBehaviorSubmission`、生产句柄和真实动作执行封装在 `SkillLabProductionFixture.h:17-130`，并被 `ProductionRecords`、`AreaSubmissionSettlement`、`LateSubmission`、`SubmissionReconciliation`、`SubmissionFeedbackIdentity/Routing`、`SubmittedActionLifetime`、`ResetSubmissionDuringCompletion` 等测试使用。这些新测试已替代旧综合测试中对生产结果、Area 子结果、投射物交付和迟到/未决对账的部分职责。

旧 `CastGameplayEvent` 仍在同一 `RunTest` 中混合定义解析（[160-223](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:160)）、资源/提交重入（[269-396](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:269)）、Ability/GameplayEvent、规则窗口、单槽 latest-wins、取消、死亡 Tag 和 latent deferred consumption。新夹具和独立测试已经覆盖其中的生产/提交/Reset 子集，但没有替代跨 Provider -> Runtime -> GAS Ability -> GameplayEvent -> 规则仲裁 -> Recovery 消费这一条端到端烟囱链；这条链仍应保留，综合测试却不必继续承载已独立验证的生产记录细节。上一版只指出“903 行”，没有把替代关系和仍需保留的端到端职责写清楚。

## 已确认的合理部分

1. **运行时状态与单次 Cast 状态分离。** 每次 Cast 使用带 `RuntimeGeneration`、`Generation`、`CastId` 的执行对象，旧 Ability、旧 Notify 和旧回调可以通过身份校验被隔离。
2. **生命周期事实与表现事实分离。** Notify 只推进释放时点，实际命中、伤害、治疗和持续效果由权威运行机制产生，Presentation 只消费这些事实。
3. **提交和资源结算有独立边界。** Submission、投射物交付、命中和未决确认不是一个布尔值；Reset 会检查未确认提交，避免把未完成的资源事实静默覆盖。
4. **动作和持续效果拥有独立寿命。** 已交付 Projectile 不随 Cast 结束强制销毁，持续 GE 的自然结算与移除由 GAS/Adapter 负责；这不是“Cast 结束后所有对象都必须清空”的简单模型。
5. **未决交接和 Reset 有明确屏障。** 未确认提交先交给会话记录，资源回执按 Runtime/generation/operation 身份核对；Reset 在未知事实闭合前不擅自释放或重复扣费。
6. **内容载体职责清楚。** Lua/JSON 的内容组合与数值来源没有把技能专属 Builder 重新塞回公共 C++；公共生命周期和 GAS 结算仍在运行时边界内。
7. **测试目录已有按职责拆分的产物，但仍存在重叠。** Cast execution、control、request reentry、submission isolation、reset resource completion 以及新的 production/feedback fixture 测试已经独立验证若干子契约；这使 `CastGameplayEvent` 的重复职责成为更具体的边界问题，而不是整个测试目录都未拆分。

## 实质发现

### F1：死亡后 Reset 可能保留 `State.SkillLab.Dead`，导致血量恢复但技能仍不可施放

- **级别**：高优先级静态行为候选；setter/回调路径已确认，动态结果仍需一次复现锁定。
- **涉及代码**：
  - `USkillLabSkillRuntimeComponent::CompleteReset`，[SkillLabSkillRuntimeComponent.cpp:284](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp:284)；直接通过 [SkillLabSkillRuntimeComponent.cpp:311](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp:311) 调用 `SetNumericAttributeBase(Health, MaxHealth)`。
  - `USkillLabCombatAttributeSet::ClampVitals`，[SkillLabCombatAttributeSet.cpp:86](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:86)；死亡标签只在 [SkillLabCombatAttributeSet.cpp:101](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:101) 至 [SkillLabCombatAttributeSet.cpp:113](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:113) 的 `ClampVitals` 调用中按 Health 添加或移除。
  - `USkillLabCombatAttributeSet::PostGameplayEffectExecute` 在 [SkillLabCombatAttributeSet.cpp:45](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:45) 至 [SkillLabCombatAttributeSet.cpp:83](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabCombatAttributeSet.cpp:83) 调用 `ClampVitals`；`CompleteReset` 的直接 Attribute Base setter 路径没有显式调用该同步逻辑。
  - `USkillLabSkillRuntimeComponent::CheckGates`，[SkillLabSkillRuntimeRequests.cpp:70](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp:70) 至 [SkillLabSkillRuntimeRequests.cpp:78](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp:78)，同时检查 `IsAlive()` 和 `State_Dead`。

**引擎路径证据**：本机 UE 5.8 源码中，`UAbilitySystemComponent::SetNumericAttributeBase`（`Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent.cpp:460-464`）进入 `ActiveGameplayEffects.SetAttributeBaseValue`；`FActiveGameplayEffectsContainer::SetAttributeBaseValue`（`GameplayEffect.cpp:3986-4040`）调用的是 `PreAttributeBaseChange`/基础值更新/`PostAttributeBaseChange`。直接属性 setter 的回调说明见 `AttributeSet.cpp:72-100` 和 `AttributeSet.h:225-234`，不会进入 `PreGameplayEffectExecute`/`PostGameplayEffectExecute`。当前 `USkillLabCombatAttributeSet` 没有 override `PreAttributeBaseChange`、`PostAttributeBaseChange`，`CompleteReset` 也没有显式调用 `ClampVitals` 或移除 `State_Dead`。这使遗漏路径在静态机制上成立，但本轮没有运行时复现。

**调用链和影响**：

```text
真实伤害使 Health 归零
 -> PostGameplayEffectExecute -> ClampVitals
 -> ASC.AddLooseGameplayTag(State_Dead)
 -> CompleteReset -> SetNumericAttributeBase(Health, MaxHealth)
 -> UE 5.8 setter 只走 AttributeBaseChange 回调，当前项目没有对应清理
 -> Dead loose tag 仍可能存在
 -> CheckGates 继续返回 CasterDead
```

因此可能出现“Health 已恢复到 MaxHealth、Reset 日志完成，但下一次技能请求仍被 `CasterDead` 拒绝”的状态。该风险不是由旧 Review 通过或已有单次释放记录排除的；现有 `SkillLabCastGameplayEventTests.cpp` 的死亡段落 [872](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:872) 至 [882](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:882) 只手动 `AddLooseGameplayTag`/`RemoveLooseGameplayTag`，没有覆盖“Health=0 -> Reset -> 检查标签 -> 重新施法”。

**最小复核方式**：在现有 Runtime fixture 中，将 Health 通过真实伤害路径降为 0，确认 `State_Dead` 出现；调用 `ResetSkills()`，等待 Reset 完成；同时断言 Health 为 MaxHealth、`ASC->HasMatchingGameplayTag(State_Dead)` 为 false，并再次提交一个已授予技能。若标签仍在或提交原因为 `CasterDead`，即可确认缺陷；若动态路径清除了标签，仍应保留这条回归断言，以防 Reset 路径后续变化。

### F2：`CastGameplayEvent` 自动化测试成为 903 行单体测试，改动定位和失败隔离成本高

- **级别**：结构维护问题，中优先级；不代表当前功能必然错误。
- **涉及代码**：
  - 唯一测试入口 `FSkillLabCastGameplayEventTest::RunTest`，[SkillLabCastGameplayEventTests.cpp:141](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:141) 至 [SkillLabCastGameplayEventTests.cpp:144](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:144)。文件总长 903 行，实际测试主体延续至 [SkillLabCastGameplayEventTests.cpp:901](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:901)。
  - 同一 `RunTest` 从 [148](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:148) 起构造 JSON/Lua fixture 和无效配置，再覆盖规则窗口、即时释放、Lua 提交、Ability/GameplayEvent、并行 Cast、重入、资源不足、Reset、Dead Tag、latest-wins 缓存消费和延迟激活。

**影响**：策划要求“改一个释放规则”“新增一个行为”或“修复取消/结算”时，程序往往必须进入同一个大型 `RunTest`，并承受共享 fixture、共享 Actor/ASC、全局定义替换、latent command 和尾部清理的耦合。新的 `FSkillLabProductionFixture` 已把生产记录、Area 子结果、投射物交付和迟到反馈拆到多个测试，但旧入口仍保留解析、资源、规则、Ability/GameplayEvent、Reset 和 deferred consumption。测试入口中的普通 `TestTrue`/`TestEqual` 失败通常只记录失败并继续执行；但初始 fixture、解析、世界或定义准备失败处存在显式 `return false`，会跳过后续所有场景。这样既可能因为 setup 早退看不到后续场景，也可能在共享状态已不满足前置条件时继续运行并产生次生失败，失败日志难以区分内容解析、规则仲裁、Ability 时序、资源门控还是清理问题。这个问题是执行单元和状态隔离成本高，不是 UE 断言自动中止。

**最小复核方式**：在只改动某一规则时单独运行 `SkillLab.Skills.CastGameplayEvent`，观察失败是否能稳定指向一个业务场景；再让同一改动只影响对应的拆分测试（例如 `CastControlAndRules`、`CastRequestReentry`、`ResetResourceCompletion`）。后续可按“定义加载/事件身份、Cast 生命周期、规则仲裁、资源与 Reset、缓存消费”拆出独立测试，同时保留一条跨 Provider -> Runtime -> GAS Ability -> GameplayEvent 的端到端烟囱测试。

### F3：后续 GameplayEvent 提交点可以无限等待，释放点超时未覆盖（状态专项 SL-03）

- **级别**：P2 静态配置/运行缺口，尚未动态复现。
- `SkillLabSkillEntryParsing.cpp:108-145` 对释放点的 `active` 和 `completionEventTag` 有校验；`SkillLabSubmissionDefinitionParsing.cpp:107` 对后续 GameplayEvent 点只校验 Tag 和冲突，没有要求等待超时。`SkillLabSubmissionPointExecution.cpp:135-153` 会把该点保留为合法 Waiting，`TryFinish` 又要求所有点 Resolved；`SkillLabSkillRuntimeExecution.cpp:216-239` 释放时清理释放超时，只为 `ActiveSeconds > 0` 的阶段安装新 Timer。
- 因此合法配置可以在首个点已成功提交、后续 GameplayEvent 永不到达时，让 Cast/Ability 长期停留在 Active；现有 `MultipleSubmissions` 覆盖的是显式 `active:2` 并主动发末点事件，不能排除缺省 `active` 的路径。首点的费用和动作事实不应被静默退款或补发。
- 最小后续方向是让解析阶段拒绝没有结束保障的等待点，或为该类点提供明确运行时取消契约；不要求所有无等待技能强制填写固定时长。

### F4：持续效果结束后动态 GameplayEffect 定义仍由 ASC 保留（状态专项 SL-01）

- **级别**：P2 已确认的保留成本；对象占用和帧耗时未测量。
- `USkillLabAbilitySystemComponent::ApplyDamageOverTimeToTarget` 每次 DoT 应用尝试创建 `UGameplayEffect` 并加入 `RuntimeEffects`（[SkillLabAbilitySystemComponent.cpp:159-174](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp:159)；数组声明见 [SkillLabAbilitySystemComponent.h:178](/E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h:178)）。全生产引用只见创建、Add 和声明，没有按活跃效果释放、复用或裁剪；Adapter 的 `HandleRemoval` 删除的是跟踪记录，Reset 的 `Clear(true)` 删除活跃效果，不等于清除 ASC 对动态定义的持有。
- 结果是同一角色反复施加并自然结束持续效果时，活跃 Buff 可以归零，动态定义对象仍随尝试次数保留至 ASC 最终回收。该数组可能承担保证动态定义存活的所有权职责，不能直接删除或在 Cast 结束时销毁仍被活跃 spec 使用的对象；需要由 GAS 所有者定义安全复用或按真实存活引用回收策略。

### F5：终态历史和闭合未决记录没有会话内裁剪（状态专项 SL-02）

- **级别**：P2 已确认的增长/扫描结构；内存和每帧耗时未测量。
- 正常提交会分别增长 `EnergySettlementHistory`、`UsedEnergyTransactionIds`、`ProductionResults.Results` 及 Area `Children`；它们保留费用闭合、动作接受、父子聚合、迟到确认和幂等身份，不能被视为同一份可随意删除的账本。`EndSession` 只改变未终结状态，Runtime Reset 不清这些历史。`AllocatedEnergyTransactionIds` 不属于这组正常累积历史：正式路径在 Begin 时移除，只有 Allocate 后未 Begin 的外部路径才会留下条目。
- 发生未决交接时，`Records` 追加并在确认后仅标记 `Resolved/Notified`；`PendingFeedback` 会移除已投递项，但 `Records` 仍被 `ReconcileProductionResults`、`BlocksSkill` 和去重逻辑扫描。因此多次未决最终收敛后，活动未决为零，历史仍会参与会话 Tick。`EnergyReservations`、`PendingResourceChanges` 等活动集合有独立退出路径，不应与这些历史混为一谈。
- 裁剪必须先证明迟到回调、重复确认、跨 Runtime 查询、Projectile 交付和 Area 父子关系在窗口外不再依赖旧记录；不能直接设 TTL、删除最老记录或使用 `RemoveAt` 破坏数组下标关联。

### F6：只写字段和仅为计数保存的完整结果增加维护歧义（状态专项 SL-04）

- **级别**：P3 结构维护问题；没有证据证明已造成结果不一致。
- 生产搜索确认 `PendingSubmissionId`、`bEffectSubmitted`、`ActiveEffectResult`、`FSkillLabUnconfirmedSubmission::RuntimeId`、`bAwaitingResourceNotification` 以及 `FEnergyReservation::RuntimeGeneration/CastId` 目前只写/清零而没有真实门控、输出或序列化消费。该结论只针对专项列出的原生内部成员，不外推到所有 Blueprint 字段。
- `ActiveEffectResults` 的完整结果只用 `Num()` 生成 `SkillEffect_<Cast>_<index>` 操作名，成功结果另存入 `SuccessfulPointEffects[PointId]`，后者才用于同一点按目标去重；前者是局部重复保存，随 Cast 回收，不属于 F5 的会话无限增长。
- 可先删除经调用链确认无用途的私有成员，或将计数语义改成标量，但必须保持成功效果计数和操作 ID 行为；不能顺手移除承担去重的 `SuccessfulPointEffects` 或真实资源/生产历史。

### F7：Runtime 重建后 Loadout 授予的 Ability spec 没有对称撤销归属（状态专项 SL-05）

- **级别**：P3 生命周期归属问题；触发条件是同一 Actor/ASC 上重建 Runtime，不是每次施法或普通 Reset。
- 新 `SkillLabSkillLoadout` 初始化总向 ASC `GiveAbility`，Runtime EndPlay 清理 Ability 实例、事件和 Timer，但生产源码没有 `ClearAbility`/`ClearAllAbilities` 路径；旧 Loadout 的本地 handle 消失后，旧 spec 仍留在 ASC，新 Loadout 再授予一份。
- 当前正式请求使用新 Loadout 的精确 handle，静态证据不足以声称会立即重复触发旧 Ability；问题是反复 Runtime 替换会留下休眠 spec，且不清楚授予究竟属于 ASC/Actor 长寿命对象还是 Runtime。后续核对应比较替换前后的 activatable spec 数量，修复时不能用 `ClearAllAbilities` 误删其他能力，也不能连带取消已交付 Projectile/Buff。

## 待验证风险

- `PendingPeriodicDamage` 使用 `FGameplayEffectSpec` 指针关联周期效果结果；多个并发周期 GE 在引擎物化 Spec 副本时是否始终能回收到正确结果，需要运行时并发测试和引擎语义确认，当前静态证据不足以定性为缺陷。具体路径是：`SkillLabAbilitySystemComponent.cpp:17-28` 以 `&Spec` 作为 `TMap` key 记录结果，`SkillLabAbilitySystemComponent.cpp:30-51` 在回调时按 `&Spec` 查找并删除；指针不匹配且 pending 数量大于 1 时直接返回，不广播周期事实；数量恰为 1 时则把唯一记录兜底绑定给当前回调。生产记录来自 `SkillLabCombatAttributeSet.cpp:69`，消费点是 `SkillLabSkillEffectEventAdapter.cpp` 的 `OnPeriodicDamageResolved`。若多个周期效果回调拿到 transient Spec copy，可能丢失 `DamageTick`/实际结果或把唯一记录错误归属；当前代码注释已承认“通常同一引用，单个效果可兜底”，所以仍应标为待验证风险。最小复核是让两个独立周期 GE 在同一目标同时 tick，分别比较 Handle、来源、实际量和 `OnPeriodicDamageResolved` 数量。
- `SkillLabSkillEffectEventAdapter::Clear(true)` 为避免延长 Actor 寿命会清空 Origin 的 `SourceActor/TargetActor`，而自然移除路径会先恢复它们；`EffectInstanceId`、`UnitId`、`CastToken` 等身份仍保留。当前 Router 没有证据依赖这两个 Actor 字段，故这是 Reset 与自然移除载荷契约差异，需定向核对，不是已确认表现故障。
- Runtime EndPlay 的 `Clear(false)` 会解绑持续效果观察但不删除真实 GE，已交付 Projectile 也继续按原 Delivery/源 ASC 运行；基线未承诺观察者自动迁移到新 Runtime，因此这是生命周期边界而非当前必修缺陷。源 Actor/ASC 销毁后的持续效果和投射物行为仍需按具体 GAS 规则验证。
- `SkillLabSkillRuntimeExecution.cpp` 同时承载 Ability/Notify、阶段、取消和效果接入，存在继续增长后的维护压力；共享 `USkillLabCastExecution`、身份校验、重入和提交状态使机械拆分收益未被证明，本轮没有发现由文件长度直接导致的行为错误。

空区域语义已由当前基线定义，不再是产品待决：`03-TargetingAndSpatialQuery.md` 明确空扇形、空直线带、空区域周期是有效零命中结果；`Skill_ArcStrike.md` 明确空范围仍支付一次成本并完成；`SkillCombatRulesRequirements.md:140,146,148` 明确首次成功提交扣费/冷却，提交成功不等同命中或伤害。当前 `SkillLabSkillBehaviorProduction.cpp:53-72` 在区域查询完成且 Targets 为空时记录 `AreaEmpty`，仍返回成功提交；`SkillLabTargetingService.cpp:125-143` 也将空区域返回为完成且空目标。因此剩余问题是多目标、空区域和无效位置的运行时验证，不是语义待决。

## 典型修改场景对策划的含义

| 需求 | 内容改动本身 | 公共机制消费与回归 |
| --- | --- | --- |
| 调已有技能的伤害、Cost、Cooldown、Range、Radius、Speed、Duration/Period | 只改 `Config/SkillDefinitions.json` 中对应数值；Lua 不复制这些数值 | Provider、Targeting、EffectExecutor、GAS 通常只消费现有字段；回归加载、目标关系、实际结算、费用和冷却 |
| 改技能专属 target/effect/execution 组合 | 改对应 `Config/Skill_*.lua` 声明，必要时补 JSON 必需字段 | Provider 负责规范化；若现有枚举/模板已支持，不应改底层；回归目标关系、实际效果和提交结果 |
| 新增 execution template、提交点触发类型或 completion semantics | 修改对应技能声明和必需配置 | 合理跨 `SkillLabSkills` 的 Provider/Runtime/Behavior/Ability；只有新事实或新效果进入边界时才扩展 GAS/Presentation；需要跨模块自动化和可见验证 |
| 新增或调整规则窗口、取消、重入 | 修改规则配置或技能内容中的触发声明 | 触及 Runtime Requests、Activation Plan、Cast Execution、GAS Ability、GameplayEvent/Notify；回归优先级、缓存/取消/重入、旧 Cast 身份隔离和 Recovery 消费时机 |
| 修复死亡、Reset、资源回执或提交不确定 | 内容通常不应承载状态修复 | 合理修改 Runtime 与 GAS receipt 协议；不应通过 Lua 或 Presentation 补状态；回归活跃 Cast、Pending Intent、Loose Tag、Health/Energy、Cooldown 和未确认提交 |
| 改表现或动画释放点 | 修改 Presentation、Montage/Notify 及对应内容声明 | Runtime 只消费身份合法事件；Notify 只推进释放时点，不能代替命中或扣费；旧 Cast Notify 必须被拒绝，并做可见 PIE 验收 |

## 建议的验证优先级

1. 先为 F3 补一个合法多点配置但末个 GameplayEvent 永不到达的定向验证，明确加载期拒绝或运行期取消的契约；同时补 F1 的“真实伤害致死 -> `ResetSkills` -> Dead Tag 清除 -> 新技能可施放”回归断言。
2. 对 F4/F5 分别做长会话 DoT 反复施放和多次未决后确认的观测，先区分活动集合、必要终态和可裁剪历史，再决定所有权与裁剪策略；不要用一次短测试推断无界增长的实际代价。
3. 在确认 F1 后，按职责拆分 F2 的单体测试；拆分时保留一条端到端 GameplayEvent 烟囱测试，用于证明跨 Provider -> Runtime -> GAS Ability -> GameplayEvent 主链仍通。
4. F6 只整理已证明无用途的成员，F7 在 Runtime 替换场景核对 ASC spec 数量和归属；两项都不能用粗粒度清空替代身份正确的回收。
5. 对策划要验收的新增行为，分别提供自动化事实和可见 PIE 证据；不要用一次 NullRHI 日志、旧 Review 通过或“当前技能能释放”观察替代未覆盖场景。

## 结论

SkillLab 当前运行链和模块边界具备继续做技能内容与战斗规则的基础，但静态质量并非可以无条件验收。已确认的高信号问题是：F1 死亡后 Reset 可能恢复 Health 却保留 `State.SkillLab.Dead`；F3 合法后续 GameplayEvent 点可能无限等待；F4 动态 GameplayEffect 定义只增不减；F5 终态历史和闭合未决记录没有会话内裁剪并继续参与扫描。F2 是 903 行综合测试的中优先级隔离成本；F6 记录只写/重复字段；F7 记录 Runtime 重建后的 Ability spec 归属。R1 的接口事实已更正：无参 `ExecuteDeclaredAction()` 是 private 测试 seam，通过 friend 给 fixture 访问，但实现仍编入生产编译单元；R3 的 555 行文件是职责集中和维护压力观察，不能仅凭行数要求拆分。周期 Spec 指针临时归属、Reset 移除事件 Actor 载荷差异、源 Runtime 结束后的持续效果观察迁移属于条件性风险或边界，尚未动态确认。上述结论来自源码静态证据，未声称已发生可测量的内存、帧率或普遍重复扣费故障。

本轮没有运行构建、自动化、编辑器、PIE、运行时 Probe 或资产检查；未审范围仍包括完整 Blueprint/二进制资产、网络/预测/存档、完整 Lua VM、原生 GAS 旁路、所有测试逐断言和未来内容。历史运行记录仅作为背景，不能替代当前工作树验证。报告只维护本文件，没有修改源码、配置、资产或既有项目文档。
