# SkillLab 代码调查补充：复杂度、状态权威与范围

调查日期：2026-11-12。对象：正式工作区相对 `HEAD 803d1c7699c2639ba4bc72d36cb84874e73f89f7` 的既有改动。本文补充同目录 `CodeInvestigation.md`，不替代其规模、版本和运行证据结论。

## 1. 结论与边界

**当前需求确实要求多 Cast 隔离、多提交点、未知结果独立收敛、资源预留和 Reset 跨层完成反馈；但不能由此推出当前每个字段、容器、接口及测试结构都必要。** 沿四条代表链追踪后，可以区分：

- 生产接受、资源扣款/释放、冷却和反馈交付是不同事实，有不同存活期及真实生产消费者。不能把这些对象统称为重复扣款账本，也不能仅为减少对象将其合成一个同步成功 bool。
- 每 Cast UObject、GUID 加多层身份、活跃预留与历史双 map、未决记录加反馈快照、同类型拆多份 cpp，均包含可替代的程序设计选择。当前需求不证明这些具体组织形式唯一或全部最小。
- 已定位确定的收束不足：仅写不读的状态字段、为取得数量重复保存完整结果，以及只服务既有测试的无参生产适配。它们是具体成本，尚无证据证明已经造成玩家功能故障。
- `RequestResourceEffect` 当前没有非测试调用者，但当前架构交接明确要求保留。应列作“有架构依据、未有当前生产使用的兼容入口”，而非直接定性为擅自扩张。
- 跨批 Skill 修改与本批 README 的范围表述不一致、上述兼容 API 最初引入的事先授权，已主动向总控请求原始消息补证。代码不裁定作者责任或授权先后。

本轮只读现有源码、配置、需求、架构交接、本人前报告与附件；未读取另一调查报告或其判断，未联系其他对话、增派 Agent、编译、运行测试、操作游戏/编辑器或安排修复。只新增本文和调查附件。总控已回复原始授权问题交原过程调查者补证，本报告保持待证，不等待或扩大调查范围。

以下路径若未写绝对前缀，均相对 `E:/agent/game/ue/SkillLab/`。行号对应本次未漂移工作区。附件 `CodeFollowupSymbolEvidence.txt` 保存字段、调用者、查询、资源 ID、Reset 与 HEAD 字段检索；`CodeFollowupFinalCheck.json` 保存交付前只读核验。静态“无读者”指正式项目 Source 的文本引用核查，不承诺第三方外部二进制 ABI 使用；所列普通 C++ 私有字段不属于 Blueprint 反射属性。

## 2. 代表链 A：每 Cast 所有权与共同仲裁

**最小问题。** 需求第 8.2 节（需求文件 168–174 行）要求 A/B 可同时施法；两者都约束新请求；单个 Cast 结束不能解除另一个限制；事件、Timer、取消与提交不能串到别的 Cast。单个角色共用一份 ActiveAbility/Behavior/Timer/提交状态无法满足这种隔离。角色级 latest-wins 缓存仍应只有一个。

**当前结构及真实用途。**

- `Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h:131` 的 `Executions` 持有每 Cast 执行对象；`SkillLabCastExecution.h:17` 起保存该 Cast 的 Ability、Behavior、请求/定义、Timer、Lifecycle、Token 及同步提交上下文。
- `Private/SkillLabSkillGameplayAbility.cpp:7` 使用 GAS `InstancedPerExecution`；不是另外造一套 Ability 执行器来绕过 GAS。公共 Runtime 仍进入原有正式门控及 Ability 激活。
- `Private/SkillLabSkillRuntimeRules.cpp:243` 的共同判断遍历执行集合，拒绝优先，等待与取消集合分开处理；`RuntimeRequests.cpp:317` 起消费角色级缓存时重新共同判断。Token 与重新查找应对同步广播中取消、替换或销毁，不能只拿旧数组元素引用继续写。

**必要与选择。** 每 Cast 独立可变状态和可验证身份是本需求必要机制；使用单独 UObject、具体容器、重复携带的 RequestId/Generation/Cast/Point/Operation/GUID，以及同类多 cpp 是实现选择。UObject 可提供 GC 与弱引用边界，但不能以“多 Cast 必需”替每个字段背书；也不能以文件变多就断言完成解耦。多个 cpp 仍共享 Runtime/Execution 生命周期，修改提交时仍需读相关取消与迟到收束。

**确定的冗余/遗留。** `SkillLabCastExecution.h:45/51/53` 的 `PendingSubmissionId`、`bEffectSubmitted`、`ActiveEffectResult` 全 Source 仅赋值/清零，未找到读取条件或结果消费者。`ActiveEffectResults`（54 行）保存完整成功结果，唯一读取用途是 `RuntimeExecution.cpp:452` 的 `.Num()+1` 命名；473 行又把同一结果加入 `SuccessfulPointEffects`，后者在 443 行用于同点重复效果检查。完整结果双存的当前必要性不能成立；仅为序号保留一份计数就是更小的状态表达之一，本文不安排改法。

归因必须收窄：`bEffectSubmitted`、`ActiveEffectResult`、`ActiveEffectResults` 在 HEAD 原 Runtime 头文件中已存在，当前是迁移后仍保留的残留；`SuccessfulPointEffects` 是当前多点结构。不能说这些旧字段都是本轮创造，也不能因继承就免于核对本轮迁移后的无用状态。`PendingSubmissionId` 在所比 HEAD 原头文件未找到，是当前变动中的状态，但不能从未提交 diff 指定由谁或在哪次派发添加。

## 3. 代表链 B：未知生产结果、资源预留与迟到通知

**最小问题。** 需求 148–160 行明确要求：首次成功才计费；未知只预留；有限等待结束 Cast 后仍隔离；Reset 不抹去未知；原 ASC 存活时迟到接受/拒绝继续收敛；不凭超时补扣、退款、重发；通知至多一次。动作成功、扣款成功和通知成功可能在不同回调、不同 Runtime 生命周期完成。

### 3.1 状态与权威矩阵

| 容器/状态 | 写入者与权威事实 | 实际读取者 | 存活与重复边界 |
| --- | --- | --- | --- |
| `FSkillLabSubmissionResults::Results` | Begin 分配 operation/GUID；真正生产路径 Confirm 接受/拒绝；Area 聚合子结果。权威事实是动作/提交是否接受，不是扣款 | Runtime 同步 Resolve、会话 Reconcile；已交付效果执行也查询原生产身份 | 会话拥有，长于 Cast；EndSession 仅将未完成项置 Unavailable，不清全表。保存 Cost/Cooldown/身份等输入快照，不另扣资源 |
| ASC `EnergyReservations` | 资源协调器记录活跃预留、结算中/尝试不明及金额 | AvailableEnergy、写入保护、Resolve、Query | 影响当前可用资源；结转/释放后可移除。不能以记录消失推断已扣还是已释放 |
| ASC `EnergySettlementHistory` | 同一资源协调器保存原交易的 ConfirmedCommitted/Released 等终态与版本 | `QueryEnergySettlement`，被 Runtime/迟到 Reconcile 真正使用 | 长于活跃预留，填补预留移除后的原交易证据；不是第二个可独立扣款的入口。活跃时 Query 会以 reservation 投影状态 |
| 会话 `Records` | Runtime 移交未知提交关联；会话标记 resolving/resolved/notified | 同技能隔离、HasUnconfirmed、Reconcile、Deliver | 长于 Runtime/Cast，可跨 Reset。重复保存部分 key/费用关联；记录本身不授权生产成功或扣款 |
| 会话 `CooldownEnd` | 确认成功后的截止时间，当前跨 Runtime 冷却权威 | 正式门控、Runtime 展示查询、迟到收敛 | 单一截止时间 map；生产记录里的 `bCooldownRecorded` 是该操作是否已完成冷却步骤的标记，不是另一份截止时间 |
| `PendingFeedback` | 资源/生产已收敛后形成确认快照 | 下一个有效 Runtime 的 Deliver | 比结算更晚交付；成功移除快照，Records 的 notified 保留去重。不是又一次结算 |

核心定位：`Private/SkillLabSubmissionResults.cpp:17/36/58/79/103`；`SkillLabEnergySettlement.cpp:5`；`SkillLabAbilitySystemEnergyReservations.cpp:14/23`；`SkillLabSubmissionReconciliation.cpp:36/62/110/124`；`Public/SkillLabUnconfirmedSubmissions.h:75` 起。

### 3.2 为什么这些区别有当前用途

`ReconcileRecord` 先核对 Session/ResourceOwner/Runtime/Generation/Cast/Point/Transaction，再读生产接受事实；只有资源历史仍为 `ReservedNoDebit` 才允许一次结转/释放，之后重新查询。`DebitAttemptUnconfirmed` 不等同失败，保持隔离，避免第二次扣款或错误释放。确认后 `PendingFeedback` 与 `bNotified` 分开处理，允许原 Runtime 已失效但会话仍活时向新的有效入口交付原身份结果。

`DeliverConfirmation`（124 行）广播前先锁通知位，接收失败才恢复，成功移除 pending。它解决的是同步重入下通知去重，不是试图把通知变成生产/资源权威。因此“三个账本反复存同一个是否成功”不准确；但多表之间复制 key、版本和阶段快照确有维护成本。

### 3.3 未被需求证明的具体结构成本

1. **独立事实不等于必须独立 map。** 活跃预留/历史可以有其他统一表示；现实现选择两张表并由 Query 合并。当前消费者证明必须能查询原资源事实，不能证明当前双表和每个复制字段都最小。
2. **长生命周期不等于永久全量历史。** `Results`、`EnergySettlementHistory`、`Records` 与 `UsedEnergyTransactionIds` 未见会话内终态裁剪。生产记录每次正常提交也创建，Area 还有子操作。需求要求未决与去重可用，不直接要求会话内永久保存全部正常终态。长期增长是具体结构成本；本轮未量测资源或性能，不把它提升为已复现泄漏/性能缺陷，也不增设清理指标。
3. **两类资源 ID 集合职责不同。** `UsedEnergyTransactionIds` 防止旧号复用，`AllocatedEnergyTransactionIds` 允许已分配号第一次 Begin；Begin 后移除后者。不能把两者都说成永久历史副本，或仅因多个 ID 集合判重复；其契约也可有更窄替代，但本报告不据此要求重构。
4. **1 秒等待是程序选择。** `RuntimeUnconfirmed.cpp:44–47` 使用 1 秒 Timer；需求 154 行明确把有限窗口具体时长交程序确定。它只结束 Cast 等待，不以到期伪结算，不能当作偷偷改了“超时退款”产品语义。

## 4. 代表链 C：Reset、资源队列与销毁

**最小问题。** Reset 恢复能量可能在同步属性回调中排队。Queued 只代表受理，出队前仍可能因预留/聚合/会话/调用者失效被拒绝；只检查返回 bool 或监听属性变化会误报完成。仅在完成回调中忽略旧 Runtime，也不能阻止旧排队请求已经覆写新 Runtime 的资源。

**当前链路。** `RuntimeComponent.cpp:213` 在可重入取消前登记 Attempt/ASC/generation；284 行推进清理并绑定新 generation；323 行先进入 Waiting，再请求 `RequestResourceChangeTracked`。326 行的 BeforeApply 使用弱 Runtime/ASC、原 Attempt/generation/phase 和未决检查，拒绝失效旧请求真正执行。261 行最终回执核对原身份；只在 Applied 且无未决时清确认冷却，终态先解除本次状态再通知。

GAS `AbilitySystemResource.cpp:97` 把调用方上下文放进原 `FPendingResourceChange`；109 行起完成回执；出队与重新校验沿原队列实现，结束路径 `ClosePendingResourceChanges` 闭合未执行项。`EnergySettlement.cpp:25/32` 在 EndPlay/BeginDestroy 先禁止会话写入并关闭待处理请求。这些检查应对已有同步回调和 UObject 销毁，不是只为假想网络/跨进程能力预建。

**合理性与选择。** 这是复用既有协调器和队列、补原调用方完成反馈，未另造 Reset reservation/持久化账本/通用任务查询表，也未新增 GAS→Skills 依赖。`VerificationHandoff.md:49–75` 明确限定同一请求上下文、BeforeApply、最终回执与两层所有权。因 Reset 无费用交易，不挪用 Energy history，边界合理。具体 enum、AttemptId 与 bResetting/phase 的表示仍是设计选择，不意味着每个布尔都必要。

**具体残留。** `SkillLabAbilitySystemComponent.h:162` 的 `bAwaitingResourceNotification` 只在 `AbilitySystemResource.cpp:13/52/58` 赋值，没有条件读者。回执正确性不能由这个残留字段证明，也没有证据表明它当前导致错误。其余 expected value/写保护判断另有实际用途，不能连带认定都冗余。

**产品与证明边界。** 失败可能已完成 Health/效果清理，当前并不承诺 Reset 全局事务回滚；交接 71 行明确如此，不能把该技术限制包装为完整 Reset 成功。此前报告的死亡后恢复 HP 可能仍保留 DeadTag 是 HEAD 已存在的静态继承风险，未动态复现，本轮不重新扩查或归责新机制。

## 5. 代表链 D：多点提交、投射物交付与测试适配

**最小问题。** 每个声明点最多成功一次；不同点可作用同一目标；投射物创建可同步触发 overlap/销毁；交付后的投射物和持续 GE 可以长于 Cast。不能用“目标见过一次”代替“该点提交成功”，也不能把等待命中当成仍拥有施法操作限制。

**当前责任分布。**

- `CastLifecycle.cpp:187–209` 的 pending/committed point 集合维护 Cast 级提交许可和成功幂等；未知与已成功的点都不能再次任意提交。
- `SubmissionPointExecution.cpp:60–103` 的 Behavior 点表维护触发类型、Timer、AwaitingAction/Resolved/Unconfirmed，以及异步结果/Projectile；调用 Runtime Begin/Resolve，未自己扣款。151 行的完成聚合属于所有点的脚本完成条件。
- Runtime 的 Submission 上下文负责当前同步资源/生产操作和重入屏障，不是所有已交付异步物体的常驻状态。生产结果表则在 Cast 后提供接受事实。
- `SkillBehaviorProduction.cpp:25–54` 创建真实 Projectile、设置回调、初始化后 FinishSpawning 再确认交付；`SubmissionPointExecution.cpp:167` 的清理只销毁未确认交付的投射物，已交付的独立对象不被 Cast 清理回滚。

**结构判断。** 各处 enum/集合对应不同阶段，存在投影和同步成本，但不能仅按点表数量定性为三套权威。尤其“交付被接受”与“随后命中/效果完成”不同。这一分工满足当前要求；点触发、操作结果、资源阶段合不合并属于可替代结构，不由需求指定当前类型数。前述 `ActiveEffectResults` 与 `SuccessfulPointEffects` 的完整结果重复才是能指向具体读写的冗余实例。

**测试侵入的窄例子。** `Public/SkillLabSkillBehavior.h:74` 注释明确说明无参 `ExecuteDeclaredAction()` 用于既有失联 fixture。实现 `Private/SkillLabSkillBehaviorProduction.cpp:9–22` 查询 Runtime 当前 production point，修改 Point/LastPointId，调用带参真实执行并更新 LastResponse。全 Source 唯一无参调用在 `Private/Tests/SkillLabProductionFixture.h:120`；正式路径在 `SubmissionPointExecution.cpp:91` 直接带 Point 调用。无参 wrapper 未以 `WITH_DEV_AUTOMATION_TESTS` 隔离，因而是约 14 行测试专用的生产编译适配，非玩家功能必需。它复用真实动作，不是伪造结果或另一套状态机；成本与结论都应保持这个量级。

只读 `GetPointResult/GetPointProduction/GetPointProjectile` 的当前外部消费者也主要是测试，观察接口与能触发动作的 wrapper 风险不同，不应等同定性。Provider 的 `CopyDefinitionsForAutomation/ReplaceDefinitionsForAutomation` 则有 `WITH_DEV_AUTOMATION_TESTS` 隔离。friend 声明本身不生成另一运行状态，不能单靠 friend 数量称测试污染严重。

## 6. 测试必要性与测试组织成本分别判断

未知回执、真正队列执行、同步重入和销毁不宜只用函数返回值或伪造成功测试；需求 162 行和交接 122–127 行确实要求这些边界。真实 World/ASC/Ability/GE、latent Timer、受控失联和版本快照是相应证据所需，并非见到测试多就可删。

但这些要求不自动证明当前测试组织高效：

- 新 `SkillLabCombatTestFixture.h` 共享真实 World/单位/Runtime/目录 RAII，减少夹具复制方向合理；旧 `SkillLabProductionFixture.h`、旧 903 行 `SkillLabCastGameplayEventTests.cpp` 仍并存各自前置、World/目录替换与恢复职责，未完全收束。旧大文件同时覆盖多种规则/状态/缓存/事件，不是只含不可拆的单条集成流程。
- `VerificationHandoff.md:110–118` 曾明确要求统一 fixture 观察入口、迁移一组职责、不能各复制全局目录恢复。保留旧集成测试名不等于所有旧场景必须继续集中在一个文件。当前存在部分提取后的并存成本；不能仅因多个 cpp 低于 400 行就称上下文目标完全达成。
- 独立验证工程用于避免并行写 Build/Intermediate 有技术理由；仓库内三份副本混入 3292 个未跟踪文件、约 1.13 GB，仍是交付组织成本。它们不属于生产机制，不可用作新增实现行数，也不可用测试隔离必要性为把副本留在工作区范围内自动背书。
- 当前既有测试通过证据的同版边界仍按前报告：23/15/8/2/1 项分轮，不是最终同版本全套；本轮只追踪代码与已存证据，没有新增运行通过声明。

## 7. 无当前生产消费者、语义变化与文档升格

### 7.1 `RequestResourceEffect`：有当前架构指定的兼容选择

`SkillLabGAS/Private/SkillLabResourceEffects.cpp:58` 只将纯即时、单资源数值 GE spec 转成协调请求，不产生原 GE handle/Tag/calculation 等完整语义。全 Source 非测试文本只有声明和定义；当前调用在 `Private/Tests/SkillLabResourceWriteIntegrationTests.cpp:237/243`。保护原生 GE 资源写入的应用前拒绝/实际输出 veto 有需求依据，但额外提供 Spec 转换 API 不等于保护本身不可缺少。

同时 `VerificationHandoff.md:98` 明确要求保留该窄契约，94 行要求保留纯数值协调入口。故事实是“当前无生产使用、属于已写入架构的兼容能力”；不能称当前生产使用，也不能仅据此宣布擅自扩张。`Docs/ProjectContext/Modules/SkillLabGAS.md:39` 目前准确描述技术限制，未把它写成任意 GE 支持。

原生 GE 静态 Energy/MaxEnergy modifier 拒绝、execution 的实际资源输出 veto，确实改变某些写入的接受范围。交接 94–102 行将非资源 execution 保留、混合输出不承诺整体回滚写得明确，当前代码也未建完整 GE 兼容平台。但这些具体边界最初是否事先批准，不能由当前交接文本证明其时间顺序，已交总控查原消息。

### 7.2 没有直接成立的“偷偷改产品语义”指控

- 有限等待的 1 秒是需求留给程序的参数选择；不把程序参数反向升为必须永久保持的策划常量。
- 空区域接受在所比 HEAD 原行为已有，不能把当前 AreaEmpty 的接受计费归为本次悄改。
- 明确失败点可再尝试与“每点最多成功一次”并不矛盾；交接 126 行也列失败重试。需求禁止的是未知后的重试与重复成功，不能偷换成任何失败一律不可再请求。
- 当前正式配置 diff 仅新增 Standard/StandardRecovery 共用规则并给四个 Lua 绑定，原四技能数值、目标和效果模板没有在该 diff 中变更。多点和并发测试目录证明框架能力，不代表本轮正式新增了若干玩家技能。

以上只排除这几项错误归因，不是对整个未提交功能宣告“所有产品语义均已逐条审清”。需求与架构中的“用户确认”字段本轮作为当前规格来源，不自行把文档声明当作原始授权消息。

### 7.3 已报总控的跨范围疑点

`SkillCombatRulesCompletion/README.md:52` 写“本偏好是本批交接约束，不修改通用 Skill”；工作区初始 diff 却在仓库 `ai-program-architecture` 和 `ai-program-review` 两份 Skill 加入约 400 行、拆分/例外核对要求。两者存在范围叙述不一致；但有可能分别来自更早的独立用户授权，mtime 和 diff 都不能证明越权。

已请求总控查原始授权时间、原文及事先/事后顺序；总控确认会交原过程调查者补证。结论保留为“待证范围问题”，不擅自修改 Skill、README 或需求，也不把它包装成玩家功能必需复杂度。

## 8. 任务切片的代码依赖判断

| 切片 | 可从代码得出的判断 | 代码不能证明的事 |
| --- | --- | --- |
| B1 类型/解析/规范化候选 | 通过稳定公开 Types/定义合同和最小样本可相对独立；不必默认通读 Runtime/大测试 | 当时实际公共类型是否及时冻结、每条调度是否遵守 |
| A 执行所有权 + B2 多点 + C2 fixture 迁移 | 共用 Runtime/Execution、Behavior 点身份、取消/提交重入与私有测试入口；反复拆给多人会制造真实编译和接口交接依赖。合并当前执行所有者有技术理由 | 最早拆分是否不合理、何时已到合并阈值、谁阻止合并 |
| R 资源协调 + Reset 消费 | GAS 层不依赖 Skills；R 原生声明/回执/许可先稳定，Runtime 再消费并交叉验证。两层独占不同文件能减少并发冲突 | 实际是否被拆成过多微任务、是否违反原始授权 |
| GE execution 兼容修正 | 在 GAS 实际属性输出入口，局部可独立于 Runtime Reset 消费推进，不需重做全面资产审计 | 是否发生不必要的重复审计/调度，需原过程证据 |

当前代码不支持“所有东西都不可拆”或“应按每个文件单独派发”。共享状态边界与分层接口都真实存在；实际多对话成本和授权结论须与原始过程证据合并判断。

## 9. 汇总分层与证据限制

| 类别 | 本轮判断 |
| --- | --- |
| 当前明确需求所需 | 每 Cast 隔离、共同仲裁、逐点成功幂等、未知独立保留、资源预留与终态可查、至多一次反馈、Reset 的实际完成及旧队列执行前失效检查 |
| 自主/架构实现选择 | 每 Cast UObject、具体身份字段组合、活跃/历史双表、Records/PendingFeedback 组织、1 秒窗口、cpp 拆分、窄 GE spec 转换入口 |
| 已有可定位的额外成本 | 仅写不读字段；只取 Num 的完整结果数组与同点结果双存；无参测试专用生产动作 wrapper；全会话终态保留；新旧 fixture 与旧大测试并存；工作区验证副本 |
| 可能范围扩张但未归责 | 两份通用 Skill 修改与本批 README 表述不一致；无生产消费者的 GE 兼容 API 最初授权/形成顺序待原消息补证 |
| 本轮未建立 | 性能泄漏的实测结论、由上述冗余引起的玩家故障、完整产品语义审计、某人擅自改动、最终同版全套或可见 PIE/双 Montage 已通过 |

前报告的总量和版本校验继续有效：正式变动源码/测试 89 份（35 修改、54 新增），当前合计 12294 行，其中测试 4522；这说明变动与验证成本较大，不能单凭行数认定全部必要或全部过度。原 113 份相关文件快照的交付前哈希、HEAD、暂存区核验见 `CodeFollowupFinalCheck.json`；本轮没有修改这些被调查输入。

本报告提供必要性与结构成本的独立代码证据，不进行修复排期。原始授权补证由总控协调，若后续补充只能改变授权归属或历史先后，应保留本文的静态事实及其版本边界。
