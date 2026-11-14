# SkillLab 状态数据与生命周期独立专项 Review

审查日期：2026-11-13（Asia/Shanghai）。对象为 `E:/agent/game/ue/SkillLab` 当前工作区的自写 `SkillLabGAS`、`SkillLabSkills` 生产实现；不是最近 diff 的审查。独立读取源码后形成判断，未读取 `MainReview.md`，不以主审结论作先验或验收标准。

本次只新增、维护本报告；没有修改代码、配置、资产、基线或需求，没有运行构建、自动化、编辑器、Probe，没有创建测试、工具或其他对话，没有使用子 Agent 或 Git。其他 Review 同时只读同一工作区，不构成源码冻结。下文“确认”指静态代码事实；运行后果如未实测会明确标注。

## 给策划的结论

当前代码已经认真区分“这次施法结束”“投射物交付”“真正命中”“持续伤害结束”“资源最终扣除”。这部分值得保留，不宜把全部状态压成一个成功开关，也不能要求火球和 Buff 随 Cast 一起销毁。

但收尾质量不均衡：活动 Cast、Ability task、Timer 大多有明确清理；持续效果的动态定义对象、生产结果及资源历史缺少会话内回收；还存在只写不读的字段和重复结果副本。另有一个可由合法配置触发的结束缺口：后续提交点等待 GameplayEvent，却可以没有任何超时。对策划而言，短时间试放四个样本可能正常，不能据此推断长时间练技能、换技能配置和 AI 扩展多段流程都容易维护。

本专项确认 3 项应优先处理的生命周期/长期运行问题（SL-01～03），以及 2 项维护成本问题（SL-04～05）。没有静态证据据此宣称当前四技能普遍重复扣费、施法结束后全部失控，或已发生可测量的内存/帧率故障。动态验证仍未执行，不能作整体验收通过结论。

## 依据与边界

- 规则：根 `AGENTS.md`，`.codex/skills/ue-project-context/SKILL.md`、`ai-native-programming/SKILL.md`、`ai-program-review/SKILL.md`。
- 项目约定：`Docs/README.md`、`Docs/ProjectContext/README.md`、`ProgramArchitectureContract.md`。适用原则是单一权威、明确所有权、复用现有机制、可定位与可验证；不另读或复制根 AI Native 原则正文。
- 业务：`Docs/Baseline/SkillSystem/README.md` 与 `02-RequestLifecycle.md`、`04-EffectsAndStates.md`、`05-OwnershipAndCleanup.md`；`Docs/Planning/SkillSystem/README.md` 路由至 SkillChainRefactor 的通用基建需求、共同约束，以及 SkillCombatRules 需求第 2、4～6、8、8.1、8.2 节。后者明确独立效果寿命、首次成功唯一计费、未决跨 Reset 隔离、有限异常等待、旧事件不得影响新 Cast。
- 交接：SkillChainRefactor 的 `SkillRuntimeInfrastructureHandoff.md`；SkillCombatRules 的 `CombatRulesArchitectureHandoff.md`；后续 SkillCombatRulesCompletion 的 `TimingRulesHandoff.md`、`SubmissionEffectsHandoff.md`。早期“单 Cast”“InstancedPerActor”等描述已落后，按真实代码的执行集合及 `InstancedPerExecution` 核实。
- 验证材料：相关测试源码和 `Docs/ProjectContext/Modules/TestAndVerification.md`，以及 Completion 的 CastExecution、ResourceWrite 实现摘要中与本范围直接相关的证据说明。摘要的“已通过”只作历史证据定位；本次未重新审日志、隔离工程或二进制一致性，不继承成当前全量验证结论。

## 实质发现

### SL-01｜P2：持续效果已结束，动态 GameplayEffect 定义仍被 ASC 永久保留

**已确认的保留成本；运行占用未测量。**

生产链：`ApplyBehaviorEffect → SkillEffectExecutor::ApplySelected → SkillEffectResolver::Apply → ASC::ApplyDamageOverTimeToTarget`。后者每次在 [SkillLabAbilitySystemComponent.cpp:159](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp:159) 创建一个新的 `UGameplayEffect`，在同文件第 174 行加入 `RuntimeEffects`；数组在 [SkillLabAbilitySystemComponent.h:178](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h:178) 是 `UPROPERTY(Transient) TArray<TObjectPtr<UGameplayEffect>>`。

这不是“完全没有用途的只写字段”：`UPROPERTY` 使 GC 保留这些 UObject，保证动态定义活着；这个间接用途成立。但全生产 Source 搜索只找到创建、Add 和声明，没有按活跃效果释放、复用定义、裁剪或清空的入口。目标效果自然到期由 GAS 移除，Adapter 在 `HandleRemoval` 删除跟踪；Reset 的 `Clear(true)` 也只删除活跃效果及观察记录。ASC 的 EndPlay/BeginDestroy 只关闭资源会话/队列，没有释放这组定义引用；最终随 ASC GC 才消失。

**正常场景**：同一施法角色反复对训练目标释放 BurningBuff，等其自然结束，再用现有 Reset 恢复能量继续。每次进入这个合法持续效果应用入口都会再保留一个定义对象；即使目标拒绝该次 GE，Add 也发生在实际应用返回之前。活跃 Buff 数量归零，保留的定义数量仍随尝试次数增加。未推算每次字节数或声称已经卡顿。

**影响**：长期会话内的对象和 GC 引用负担增加。AI 沿“Buff 到期清理”只查看 Adapter，会误以为资源已经完全收回；真正持有定义的源 ASC 在另一层。

**最小核对/后续验证**：只读搜索 `RuntimeEffects` 的所有引用，与 `HandleRemoval`、`Clear(true)` 对照即可确认没有回收路径。后续获准运行时，重复上述释放—到期—Reset 循环，观察动态定义对象是否持续留存。修复需由 GAS 所有者核实 UE 活跃 spec 对动态定义的引用要求，选择安全复用或按真实存活引用释放；不能直接删除数组，不能在 Cast 结束时销毁仍被持续 GE 使用的定义。

### SL-02｜P2：多份会话历史随正常提交累积，未决记录闭合后还会永久参加逐帧扫描

**已确认的增长和扫描结构；内存、帧耗时未测量。不是重复权威账本一概无用。**

生产链：每点 `BeginBehaviorSubmission` 在 [SkillLabSkillRuntimeSubmission.cpp:132](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeSubmission.cpp:132) 分配首个计费事务，并在第 148 行创建生产记录；`BeginProduction → Results::Begin` 在 [SkillLabSubmissionResults.cpp:32](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionResults.cpp:32) 写入 `Results`。范围点还在该文件第 65 行起为每个目标建立子结果。GAS 在 [SkillLabAbilitySystemEnergyReservations.cpp:18](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabAbilitySystemEnergyReservations.cpp:18) 保存已用事务身份，在第 47 行保存结算历史。

正常结算只删除 `EnergyReservations`，并更新 `EnergySettlementHistory`；不删除历史或已用身份。`FSkillLabSubmissionResults::EndSession`（第 103 行）只是将 InProgress 改成 Unavailable，也不是清空。Runtime Reset 只清请求去重历史和合条件的冷却，不清这几份历史。因此连“按 Reset 批量清空”都不适用于这些集合，更不能称为会话内有界。

未决是另一种增长入口：`StartIndeterminateWait → Session::Record` 在 [SkillLabUnconfirmedSubmissions.cpp:31](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabUnconfirmedSubmissions.cpp:31) 追加记录。确认成功后，[SkillLabSubmissionReconciliation.cpp:112](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionReconciliation.cpp:112) 只设 `bResolved`；投递成功移除的是 `PendingFeedback`，不是 `Records`。组件每 Tick 调 `ReconcileProductionResults`，其第 32～33 行遍历全部 Records，已解决项仍进入 `DeliverConfirmation` 后返回。`BlocksSkill`、`HasUnconfirmed` 和追加去重也扫描此数组。

**正常场景与成立条件**：

- 不需要任何异常：反复施放并正常完成 N 次单点技能，每次首次提交产生一笔结算历史和已用事务身份、一条生产记录；区域技能还按实际目标数累积子记录。同 Cast 后续成功点不再建立收费事务，却仍建立自己的生产记录。这是次数增长，不是当前活动数增长。
- 未决 Records 不应说成“每次普通施法都加一条”。只有发生丢回执/结算未闭合等交接才追加。若会话先后遇到 H 次未决且后来均收敛，`HasUnconfirmed` 可以为 false，但 H 条历史仍每帧被扫描；长期对账后没有退回只扫描活动未决的状态。
- 会话通常随 Actor/ASC 生命周期结束，尚无跨进程持久化承诺。这限制最终寿命，但没有给长时间同角色会话设上限。

**必须保留的用途**：生产结果证明动作接受，资源历史证明费用闭合，是两种事实；Projectile 命中还会查询旧交付记录；迟到确认核对原 Runtime/ASC/事务/operation，重复 Record、重复 Confirm 和原反馈不能再次扣款。测试也明确查询已关闭历史、跨 Runtime 重建重用结果。这些不是可以凭“重复”全部删除的数据。

**最小核对/后续验证**：只读跟踪 `Results.Add`、`EnergySettlementHistory.Add`、`UsedEnergyTransactionIds.Add`、`Records.Add` 与各自 Remove/Reset。获准后分别做“纯正常反复施放”和“多次未知后确认”，观察集合规模与 Tick 扫描，不用一次未知测试替代长期证据。建议先把活动对账集合与最小去重终态分开，定义已交付动作、未决、待通知对历史的引用条件，再讨论裁剪；不能任意设 TTL 或删最老若干条。`Records`、`PendingFeedback`、`ReportedLostOwnerRecords` 以数组下标关联，直接 RemoveAt 会改身份，重构必须一起处理。

### SL-03｜P2：后续 GameplayEvent 提交点可以无限等待，释放点超时并未覆盖它

**静态可成立的配置/运行缺口；未动态复现。当前四个生产技能没有显式多点，不能说四样本已卡死。**

位置和链路：

1. [SkillLabSkillEntryParsing.cpp:108](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillEntryParsing.cpp:108) 允许不填写 `active`，默认 0，负值才拒绝。第 145 行要求 `completionEventTag` 搭配正 Active 超时，这是已有的合理保护。
2. [SkillLabSubmissionDefinitionParsing.cpp:107](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionDefinitionParsing.cpp:107) 校验 GameplayEvent 点的 Tag/冲突/目标，但不要求等待超时。`AllSubmissionPointsResolved` 只要求显式点表且不同时配置 completionEventTag，没有把上述保护迁移到这类后续点。
3. [SkillLabSubmissionPointExecution.cpp:135](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionPointExecution.cpp:135) 将尚待 GameplayEvent 的点作为合法等待；`TryFinish` 第 153 行必须全部 Resolved 才结束。Lua VM 已关闭，合法等待使 `RunScript` 不报 ScriptNoFinish。
4. [SkillLabSkillRuntimeExecution.cpp:216](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:216) 在释放时清掉释放事件的超时；第 239 行仅在 ActiveSeconds > 0 时安装新超时。因此“已释放，但第二个事件没到”不能靠 ReleaseEventTimeout 退出。它也不是 Indeterminate：提交还没发起，1 秒未决 Timer 不会介入。

**具体场景**：在现有 MultipleSubmissions 夹具所用内容形态中，保留一个 Release 点和一个 GameplayEvent 点，完成方式为全部点解决，省略 `active`，Lua 不伪造第二点触发。首点执行并收费后，动画没有发出第二点 Tag。后续点一直 Waiting，Cast/Ability 留在 Active；使用默认 Buffer 规则时，后续普通技能输入一直只缓存，直到重击、控制、Reset 等外部取消。有限 Timer 点可以自行触发，不能据此证明事件点也有退出保障。

**需求影响**：违反“每个技能有明确结束行为”与 Notify/GameplayEvent 超时取消、不补效果的方向。内容作者或 AI 认为“释放事件已有 3 秒超时，所以后续事件也有保护”，正是容易发生的错误。

**最小核对/后续验证**：将解析条件、HasLegalWait 和 Runtime 的 SetTimer 条件逐项对照，当前即可只读确认。后续沿正式 ParseCatalog/Submit/Ability 事件路径，采用上述最小内容，不发送第二点事件并推进时间；验证要么加载期明确拒绝缺少结束保障的配置，要么运行期按约定取消，且已成功首点不退款、不补第二点。可复用现有 `MultipleSubmissions` 入口；本次没有修改夹具或运行。最小修复方向是补齐这类等待的校验/结束契约，不要求所有无等待技能强制填固定 Active 时长。

### SL-04｜P3：已确认数个只写不读字段，以及仅为计数保存的重复完整结果

**已确认的结构成本；没有证据证明这些副本已导致结果不一致。**

全生产 Source 的声明、赋值、调用、结构复制和事件入口核对结果如下。列出的只写字段本身不是 UPROPERTY，也没有对应序列化、反射、委托导出或自定义整体状态消费者；不能用“可能 Blueprint 在读”解释它们。

| 字段 | 声明与写入 | 当前消费与判断 |
| --- | --- | --- |
| `USkillLabCastExecution::PendingSubmissionId` | [CastExecution.h:45](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabCastExecution.h:45)；RuntimeSubmission:101、126、140、186；RuntimeUnconfirmed:60 | 只写/清零；真正核验的是 `Submission.PointId` 和生命周期 pending 集合。不是第二个有效防重门。 |
| `bEffectSubmitted` | [CastExecution.h:51](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabCastExecution.h:51)；RuntimeExecution:455 | 只置 true；没有门控读取。实际执行保护另用 bApplyingEffect、提交状态和成功点效果索引。 |
| `ActiveEffectResult` | [CastExecution.h:53](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabCastExecution.h:53)；RuntimeExecution:472 | 只复制最后一次完整结果，没有 getter/下游消费。Behavior 的点 Result/LastResponse 是另外的真实 Lua 返回路径，不能混为此字段的读取。 |
| `FSkillLabUnconfirmedSubmission::RuntimeId` | [UnconfirmedSubmissions.h:15](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabUnconfirmedSubmissions.h:15)；RuntimeUnconfirmed:21 | 记录会整体复制保存，但该成员没有被查询、比较、输出或投递；身份匹配使用 `RuntimeIdentity`、generation 和 ProductionKey。测试也有赋值，不能形成生产用途。 |
| `bAwaitingResourceNotification` | [AbilitySystemComponent.h:162](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h:162)；AbilitySystemResource:13、52、58 | 只写 true/false，不参与确认。扣费闭合真正读取 `bDebitObserved`、current/base、最大能量和旁路标记；不能把此字段当成“等待委托到达”的状态机。 |
| `FEnergyReservation::RuntimeGeneration/CastId` | [AbilitySystemComponent.h:137](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h:137)；EnergyReservations:38 | 预留结构副本内不读这两项；可查询的结算身份来自另一份 `FSkillLabEnergySettlement`，其 Generation/Cast 有真实读取，不建议误删那份身份。 |

“完整数组只取数量”准确落点是 [SkillLabSkillRuntimeExecution.cpp:452](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp:452)：`ActiveEffectResults.Num() + 1` 仅用于生成 EffectOperationId；第 473 行只在效果成功时 Add 同一结果。完整结果同时存入 `SuccessfulPointEffects[PointId]`，后者第 443～449 行按目标查找并返回已成功结果，确实承担防重复效果和结果复用。前者没有遍历、查询、事件重放或审计用途。每 Cast 成功 E 次效果，会额外再存 E 份完整结构；Cast 结束后随执行对象回收，这是局部重复，不是 SL-02 的无限终态历史。

这些字段没有自然形成第二套权威，但名字使 AI 必须猜“提交到底看哪个字段”“最后结果和点结果谁是真实来源”。若以后只更新某个副本，新增读取者就可能开始使用陈旧状态。当前只能确认阅读、复制、维护和潜在一致性成本，不能声称已有错扣或错伤害。

**最小核对/后续验证**：逐符号 `rg -n` 检查上述声明/写入与真实门控，再核对 UPROPERTY 和对外结果类型即可。最小整理可以是删除经确认无用途的私有成员，把仅计数数组改成表达原有计数语义的标量；需要保留当前“只按成功效果递增”的行为，不能顺手改变事件 ID 含义。公开或被测试适配使用的类型成员先做调用兼容确认。无需为每个删除字段另建镜像单测，沿既有多提交/重复事件回归即可。

### SL-05｜P3：Loadout 授予的 Ability spec 缺少对称的重建/撤销归属

**已确认的对象替换保留路径；触发条件是同 Actor/ASC 上重建 Runtime，不是每次施法或 Reset。**

[SkillLabSkillRuntimeComponent.cpp:114](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp:114) 每个新 Runtime 创建新 Loadout。其 [SkillLabSkillLoadout.cpp:20](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillLoadout.cpp:20) 只根据自身 GrantedSkills 是否为空避免重复初始化，第 32 行总向 ASC `GiveAbility`。Runtime EndPlay 清理当前 Ability 实例、事件和 Timer，却没有撤销 Loadout 授予的 spec；全生产 Source 没有 ClearAbility/ClearAllAbilities 路径。新 Loadout 不知道旧 handles，会再次授予。

所以“同 ASC → 销毁 Runtime → 新建 Runtime”会留下旧 spec，再添加新 spec；旧 spec 不随失去 Loadout 的本地映射而从 ASC 消失。当前正式请求用新 Loadout 的精确 handle，不能据此声称新施法会重复触发旧 Ability。源 Actor/ASC 销毁最终收回它们；一次普通 Reset 也不会增加 spec。

Runtime 替换不是凭空假设：现有 `SubmittedActionLifetime` 第 49～60 行、`CastExecutionOwnership` 第 87 行及共享 `SkillLabCombatTestFixture::RebuildRuntime` 第 81 行明确验证替换后保留原动作/身份。但测试没有核对 ASC 的授予 spec 数量。长期工具调试或未来复用此替换能力时，重复替换会累积休眠能力记录，且谁有权撤销授予不清楚。

**最小核对/后续验证**：只读对照 GiveAbility、Runtime EndPlay、新 Loadout 初始化和替换夹具。后续在既有替换场景观察 ASC activatable specs 的前后数量。修复先决定授予属于 Actor/ASC 的长寿命 loadout，还是 Runtime 专属授予并在清理时撤销自己的 handles；不能 ClearAllAbilities 误删别的能力，也不能顺手取消已交付 Projectile/Buff。此项可排在普通长会话增长问题之后，不要求为了它重写全部 Ability 生命周期。

## 周期效果事件与其他待验证点

以下与“结束是否明确”直接相关，但证据强度不同，不混成已确认运行故障。

**周期结果的临时归属有隐式假设。** [SkillLabAbilitySystemComponent.cpp:17](E:/agent/game/ue/SkillLab/Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp:17) 按 `FGameplayEffectSpec*` 保存 PendingPeriodicDamage；AttributeSet 在 `PostGameplayEffectExecute` 第 69 行登记原子结算，ASC 的 periodic execution 委托随后取出并发事件。第 37～48 行在 spec 地址不匹配时，若仅剩一条结果就把它当作本次执行结果。这是明确存在的兜底归属假设，不是完整效果身份匹配。当前正常链可能足够，尚无证据证明已串伤害。需要同 ASC 上多来源周期效果交错、属性回调重入、效果移除/销毁时的定向运行证据，才可判断是否会丢结果、残留或误配。不能把此 map 也描述为已证实每次周期都积累的终态历史。

**Reset 的移除事件载荷与自然移除不同。** [SkillLabSkillEffectEventAdapter.cpp:28](E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp:28) 为避免延长 Actor 寿命，清空 Origin 的 SourceActor/TargetActor 并另存弱引用；自然 `HandleRemoval` 第 61～62 行恢复它们。`Clear(true)` 第 105～109 行直接广播 Origin，没有恢复，故即使角色仍有效，`EffectTrackingCleared` 的两项 Actor 字段仍为空。EffectInstanceId、UnitId、CastToken 等仍保留，不能说事件完全失去身份。该事实不证明当前表现出错：直接 Router 对 BuffRemoved 没有依赖 Actor 字段的处理，现有测试只检查 Tick 停止和移除次数。建议后续最小核对 Reset 与自然到期的载荷契约，若消费方需要目标则补证并统一；本专项不凭未来表现需求阻塞四样本。

**源 Runtime 结束后的持续效果。** `Clear(false)` 有意解绑观察但不删除真实 GE；已交付 Projectile 也仍可按原 Delivery 查询及源 ASC 执行。原 Runtime 消失后不会自动把周期观察或命中反馈改投新 Runtime。已有替换测试验证实际伤害/费用，不等于验证新 Runtime 获得原 Buff 的全部后续反馈。基线未承诺自动迁移观察者，因此这里记录边界，不当作必修缺陷。源 Actor/ASC 真正销毁后，已交付投射物的效果执行因 SourceUnavailable 等校验停止；持续 GE 是否按目标 GAS 规则继续以及具体死亡清理不由本专项重新制定。

## 集合与历史盘点

下表覆盖本范围内声明的主要运行集合、历史、回执和结果容器。路径均相对于 `E:/agent/game/ue/SkillLab/Source/`；“销毁回收”指对象最终释放，不把 EndPlay 标记等同即时释放。

| 所有者 / 集合 | 键与增长入口 | 会话内退出、Reset、销毁 | 判定 |
| --- | --- | --- | --- |
| ASC `RuntimeEffects` | 新 UGameplayEffect；每次合法 DoT 应用尝试 Add | 无逐效果退出、无 Reset 清理；最终 ASC GC | SL-01 |
| ASC `EnergySettlementHistory` | transaction；每次成功 BeginEscrow Add，结算更新 | 不随 reservation 删除，不随 Runtime Reset 清空；结束会话后 Query 拒绝，最终 ASC GC | SL-02；迟到对账必需 |
| ASC `UsedEnergyTransactionIds` | uint64；Allocate/Begin Add，含失败的 Begin | 无裁剪；最终 ASC GC | SL-02；防身份重用必需，不能直接限长 |
| ASC `AllocatedEnergyTransactionIds` | Allocate 得到但尚未 Begin 的 ID | Begin 移除；正式提交立即 Allocate→Begin；外部只 Allocate 不 Begin 才留存 | 不能说正常每次施法永久多存一条 |
| ASC `EnergyReservations` | transaction；BeginEscrow | 成功/明确失败删除；Indeterminate 保留，Reset 不擅释；结束会话禁写，最终销毁回收 | 活动账，不是可删的终态历史 |
| ASC `PendingResourceChanges` | FIFO 排队项，含弱 owner、AttemptId、回调 | Drain 出队，EndPlay/BeginDestroy 尝试 Close；写保护中延后到退出保护；回调 Move 后一次消费 | 无独立回执历史。没有证据称正常无限堆积 |
| ASC `PendingPeriodicDamage` | spec 地址；属性执行登记 | periodic callback Remove/单条兜底 Reset；没有独立按时裁剪，最终 ASC 回收 | 临时结果归属待验证，不算已证实增长 |
| 会话 `ProductionResults.Results` | operation + 完整 Key 校验；每点/范围每目标 Add | Confirm 更新；EndSession 只改变未终结状态；Runtime Reset 不清，最终组件 GC | SL-02；交付与迟到查询必需 |
| 每条生产结果 `Children` | area 子提交 Key | 父结果持有到会话销毁 | 必要的父子关联，随 Results 一起增长 |
| 会话 `Records` | 数组位置；Record 按完整身份去重追加 | resolved/notified 仅标记，不删除；Reset 保留；最终组件 GC | SL-02；闭合后扫描成本 |
| 会话 `PendingFeedback` | Records 下标；确认时 Add | 成功投递 Remove；无接收者时保留至可用或销毁 | 必要的待交付，不是已交付重复历史 |
| 会话 `ReportedLostOwnerRecords` | Records 下标；每未知记录最多诊断一次 | 不清空，组件销毁回收 | 按未知次数增长；去日志重复用途成立 |
| 会话 `CooldownEnd` | SkillId；成功首提覆盖同技能截止时间 | 不按到期删除，查询钳成 0；无未决 Reset 时清空 | 按不同技能数增长，不是每施法一条；单一冷却权威合理 |
| Runtime `Executions` | CastId + 完整 token；每次正式激活 Add | FinishCast/ClearPendingCast/不可交付收束 Remove；Reset/EndPlay 取消整组 | 活动执行拥有者，退出基本明确 |
| Runtime `ActivationBindings` | 临时 spec/Cast/generation 栈 | TryActivateAbility 外围 Add/Pop | 同步握手，不是长历史 |
| Runtime `AcceptedRequestIds` + `AcceptedRequestOrder` | RequestId 集合 + 顺序队列 | 正式记忆时裁到 128；Reset 清空 | 集合和顺序是查询/淘汰索引，合理重复，不能归 SL-02 |
| Runtime `PendingSkillIntent` | 单个可选输入 | latest-wins 覆盖；消费/取消/死亡/Reset/EndPlay 清槽 | 有界，非已受理技能 |
| Runtime `InterruptHandles` | GameplayTag | Listen 去重；只在 Runtime 结束清理，不在每次 Cast 结束清 | 按遇过的不同 Tag 数累积订阅；当前注册域有限，无每次施法增长证据 |
| Cast `SuccessfulPointEffects` | point→各目标成功结果 | 同点同目标重用；执行结束后回收 | 必要防重索引；与 ActiveEffectResults 的用途不同 |
| Cast `ActiveEffectResults` / `ActiveEffectResult` | 成功列表 / 最后一份结果 | 执行结束后回收 | SL-04，重复/无读 |
| Lifecycle committed/pending/window sets 与 Cast ConsumedWindowEvents | 点 ID、窗口 ID、Tag | Begin 清集合；终态清 open/pending；随执行结束回收 | 有限声明域，幂等/窗口状态，不是会话历史 |
| Behavior `Points` / `LastResponse` | 声明点 ID / 最后一次宿主响应 | Cleanup 清 Timer、解绑动作；随 Cast 释放 Behavior | 点状态是调度状态，LastResponse 是 Lua 返回快照；有真实读取，不笼统称第二权威 |
| Ability `EventTasks` | 该 Cast 需要的事件 Tag | EndAbility 先 EndTask，再 Reset 数组、通知原 Runtime | task 拥有关系明确 |
| Adapter `Effects` / `Listeners` | GE handle / 目标 ASC | Removal 删除并 Prune；Track 也 Prune；Clear 显式解绑；弱目标失效可待下次 Prune 清理 | 活动观察，非终态历史；弱引用不延长 Actor 寿命 |
| Loadout `GrantedSkills` 与 ASC ability specs | SkillId→spec handle；初始化 GiveAbility | 本地 map 随 Loadout 销毁；ASC spec 没有对称撤销 | SL-05；不要把两者寿命混为一谈 |

## 生命周期责任链

| 周期 | 创建/推进所有者 | 正常结束 | 取消、Reset、销毁与回调 |
| --- | --- | --- | --- |
| Cast / Submission | Runtime Requests 建 Execution，ASC 激活 Ability；Ability 核对 token 后推进 Runtime；Behavior 点调度统一 Begin/Resolve/Abort | Behavior 完成→Recovery→FinishCast；生命周期终态封窗，执行先移出集合再发终态 | FinishCast:338、ClearPendingCast:365 清三个 Cast Timer、Cleanup Behavior、结束本 Ability。提交栈内只标延迟终止，外层收束后执行，避免半途退款/丢身份。SL-03 是事件等待保障的例外 |
| 已交付 Projectile | BehaviorProduction 创建/FinishSpawning，Projectile ConfirmDelivery 先记事实再开放碰撞 | 命中/阻挡/Expire→ResolveImpact，bResolved 防二次结束，清 ExpireTimer 后 Destroy | Behavior Cleanup 解绑 OnResolved，只销毁未交付对象；已交付对象带原 Definition/Delivery/源弱引用继续运行。外部 Destroy 在 EndPlay 报真实终止；世界退出不伪报失败。不是必须随 Cast 清除的遗漏 |
| 持续 GE | 目标 GAS 持有活跃 GE，源 ASC 创建定义；Executor 应用返回后 Adapter Track | GAS 周期结算→ASC PendingPeriodicDamage→Adapter DamageTick；GAS 移除→Adapter HandleRemoval 删除跟踪并发 BuffRemoved | 普通 Cast 完成/取消不移除；Reset Clear(true) 移除跟踪效果；Runtime EndPlay Clear(false) 仅解绑。定义对象寿命另见 SL-01；周期临时归属和 Reset 载荷边界见上节 |
| 能量预留/结算 | ASC Allocate/BeginEscrow；Runtime 只持 transaction 及生产身份 | 首次接受 Resolve 扣一次，明确拒绝释放；后点不二次收费 | 未知且已尝试扣费不猜测释放或再扣；Cast 超时不能抹掉预留。终态历史和活动预留是独立寿命 |
| 未决记录/迟到确认 | Runtime 先交给 Actor 会话，再安排 1 秒 Cast 等待；Session Tick 只查询原生产/资源事实 | 明确事实后对账、记录冷却、原身份补报，至多一次；原 Cast 已消失也不重执行动作 | Reset 保留隔离并受阻；Runtime 重建可接收原提交反馈；Actor/ASC 失效则标 unavailable/诊断，不承诺不存在对象上的恢复。闭合 Records 不退出见 SL-02 |
| Reset attempt / 资源回执 | Runtime 在取消通知前登记 attempt/ASC/generation；ASC tracked 请求返回最终回执 | 最终 Applied 且无未知才清冷却、释放屏障；Queued 不是成功 | BeforeApply 与最终回调均重核身份；旧 attempt 在 EndPlay 失效；拒绝/失败/不可用也结束屏障。只有单个 Optional attempt 和队列项，没有另一份 attempt 终态历史。Health/效果/Energy 不是全有全无事务 |
| Timer / 订阅 | Cast 三 Timer、Behavior 每点 Timer、Projectile ExpireTimer、Runtime next-tick 意图；Ability event task、Tag、damage 和 Adapter delegates 各有所有者 | 按各生命周期收束，弱回调加完整身份防旧事件 | PendingIntentTimer 在清意图/不可交付路径清；Cast 与 Behavior 清自己的 Timer，Projectile 自清；EndPlay 清 Tag/damage/Adapter。会话 Tick 是持续调度，不随最后一条未决关闭，因此历史扫描仍存在 |

## 现有测试支持了什么

本次只读取与上述问题相连的测试，不逐断言重审目录；下述“覆盖”是源码中存在对应场景，不等于本次执行通过。

- `SkillLabSubmittedActionLifetimeTests.cpp:29` 起覆盖双真实投射物、取消后伤害、阻挡、过期、同步 overlap、自毁、源销毁和 Runtime 替换；第 100 行起覆盖持续 GE 普通取消/完成后继续、自然移除、Reset 停 Tick、Runtime 结束只解绑。没有统计 RuntimeEffects 或 ASC 授予 spec 增长；Reset 未核对移除载荷 Actor 字段。它证明测试目标包含独立寿命，不能用“Cast 结束还有对象”反推缺陷。
- `SkillLabMultipleSubmissionTests.cpp:12` 的事件点夹具配置了 `active:2`，并主动发末点事件；覆盖同目标不同点、重复拒绝、只计费一次、全部点后结束。未覆盖 SL-03 的缺省 Active 且事件永不到达。
- `SkillLabCastExecutionTests.cpp:78` 已有实际推进 Notify 超时到期和旧 token 拒绝的测试源码；不能沿用较早文档将“没有任何 Notify 到期测试”写成当前事实。它测的是释放事件超时，不是已释放后的无界后续点等待。
- `SkillLabProductionRecordsTests.cpp:32` 起覆盖正式生产记录、真实范围子结果、原返回丢失、Runtime 重建、矛盾确认不改版本、Projectile 交付不被命中改写。`SkillLabSubmissionReconciliationTests.cpp:71` 起显式重用已经关闭的资源/生产历史，说明历史有必要用途；没有定义可安全裁剪的时机和长期体量上限。
- `SkillLabEnergySettlementTests.cpp:27` 起核对原身份、正在结算、只扣一次、明确释放、未知不能重扣/释放以及 EndPlay/销毁查询不可用。它验证终态保留和防重，没有证明历史有界。
- Reset 与资源写入测试仅定向检索了已有重入、BeginDestroy、回执收束等相关入口；没有逐项复审大文件。历史运行摘要提供不同版本的定向成功证据，未在本次重跑，未凭此宣称所有生命周期测试绿色。

## 覆盖、未审范围与建议顺序

**精确覆盖**：生产 `SkillLabCastExecution.h`、`SkillLabCastLifecycle.h/.cpp`、`SkillLabSubmissionResults.h/.cpp`、`SkillLabUnconfirmedSubmissions.h/.cpp`、`SkillLabSubmissionReconciliation.cpp`；`SkillLabSkillRuntimeComponent.h/.cpp`、Ownership、Requests、Rules、Execution、Submission、Unconfirmed、Confirmation；`SkillLabSkillGameplayAbility.h/.cpp`、`SkillLabSkillBehavior.h/.cpp`、BehaviorLua、BehaviorProduction、SubmissionPointExecution；EffectExecutor、SubmittedEffect、EffectEventAdapter、FireballProjectile 的声明/实现；SkillTypes、ProjectileTypes 声明、Loadout 声明/实现；GAS ASC 头与 Component、Resource、EnergyReservations、EnergySettlement，CombatAttributeSet 的周期结果与死亡入口。为核对 SL-03 补读 SkillEntryParsing、SubmissionDefinitionParsing 相关校验，为解释效果链补读 Resolver/ResourceEffects 相关入口。跨模块只沿直接消费读取 PresentationRouter、PresentationLua 的技能结果映射/回调及训练命令的引用定位。生产 `Config/SkillDefinitions.json` 用于区分当前四样本与通用多点能力。

本报告对字段/集合的搜索核对覆盖生产 Source；“未发现读取”只对明确列出的原生内部成员成立，不等于对所有 BlueprintReadOnly 字段作无用途判定。Definition/Request 快照、原提交键、弱 Actor 快照、结果事件向动态委托复制都保留了真实消费，未把整体传递误报为只写不读。

**未审**：内容加载/脚本声明完整架构、Lua VM 实现、伤害公式、测试 API/巨型文件结构、完整表现状态机、Editor 模块、Blueprint/二进制资产内部、网络/预测/存档、引擎源码/第三方/生成文件、历史快照与验证工程；没有审计任意原生 GAS 旁路。没有基于当前工作树作构建或运行。待验证项不升级成已复现故障。

**值得保留**：单一 ASC 资源权威；生产接受与费用事实分开；完整 Runtime/generation/Cast/point/operation 身份；一次消费和原身份查询；未决先交接再结束 Cast；Reset 的最终回执屏障；每 Cast 执行集合；已交付动作与施法分离；弱目标观察与显式解绑；请求历史真正限长。它们是 AI 修改时应维持的不变量，不应为了减少字段一次性抹平。

**建议顺序与边界**：先补 SL-03 的明确结束保障，避免合法多点配置把玩家留在施法等待；随后处理 SL-01 的动态定义所有权，以及 SL-02 的活动对账和终态保留策略。长会话回收要同时证明迟到回调、Projectile 和待通知仍安全，不能直接清账。SL-04 可小范围整理，SL-05 在支持 Runtime 替换的生命周期内补归属；周期临时配对与 Reset 载荷先定向补证。无需先做全系统重构，也无需为本报告新建验证框架。所有建议仅是后续工作输入，不构成修复、测试或重构的执行授权。

## 源码状态记录

2026-11-13 02:34（Asia/Shanghai）读取关键文件 SHA-256：

| 当前文件 | SHA-256 |
| --- | --- |
| `SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp` | `BA1259F699DED726ACA6E74CAA92BA4A795E67E99F28D2F3B198B0E0DF6A3488` |
| `SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp` | `0BC5F7BC3C57294E5DAE09D0003E469426653A4D3D3B4F680513871C2DBE1033` |
| `SkillLabSkills/Private/SkillLabSubmissionReconciliation.cpp` | `9C0595B77EE5168366BF4471A378F1FD71F889C97AE3B9FF48EFC6CDB2DB0A10` |
| `SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp` | `D3A58BBFD4267FF9A98F084B159BE4BC7C9D7D3FCA3D38AD0E655763685D09F3` |
| `SkillLabSkills/Private/SkillLabSkillLoadout.cpp` | `7CEB6477360E5A869360089C51CAB37F75455687165814612C338024EED4D264` |

本次中途对两个模块排除 Tests 后的 67 份文件取了只读哈希，交付前再次比较结果记录于下方；这不是首次读取前的冻结，也不包含整个 Source/Config/Content。未使用 Git，未知工作树相对提交的差异，不声称仓库干净或历史验证与当前源码完全等价。

交付前复核：两个模块排除 Tests 后仍为 67 份文件，相对本次中途采样的新增、缺失、SHA-256 变化均为 0。该比较没有覆盖首次采样前的变化、测试/配置/资产，也没有证明二进制与源码相符。
