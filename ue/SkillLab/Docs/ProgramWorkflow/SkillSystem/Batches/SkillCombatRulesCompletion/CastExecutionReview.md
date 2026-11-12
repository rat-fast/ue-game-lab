# Cast 执行集成独立 Review

2026-11-12。范围：当前 B1 定义解析与 A+B2+C2 执行集成；依据本批 README、三份当前交接、SkillCombatRulesRequirements 第 2–8.2 节及项目程序分层约定。保留此前已接受的账本、有限等待、同会话迟到收敛及原提交身份结论，不重开历史问题。

**当前结论：P2-CE-1 已关闭，本次 B1 与 A+B2+C2 代码集成在已审范围内通过。** 关闭依据为修复源码、修前真实失败、修后五场景回归与 15 项定向运行、409 文件三方哈希一致。旧 23 项是修前证据，没有在修后全套重跑；资源审计、可见验收及项目事实文档同步仍未关闭，不据此宣布整批完成。Reviewer 仅更新本报告，未修改生产代码、配置、资产或作者验证工程，也未启动其他 Agent。

## P2-CE-1 定向复查与关闭（2026-11-12）

本轮只复查 `SkillLabSkillRuntimeRequests.cpp` 的请求编排、新增 `Tests/SkillLabCastRequestReentryTests.cpp` 及直接门控/取消/缓存依赖，沿用原需求第 8.2 节和既有规范，没有重新全审。

- 清缓存同步通知后，在当前 Requests.cpp 第 147–156 行检查请求有效性、保留回调写入的较新意图，并重新使用同一 BuildActivationPlan；Reject 结束 D，Buffer 仅缓存 D，旧 Allow/取消列表不再直接沿用。
- 第 103–109 行捕获并核对原 RuntimeGeneration，以及组件、Owner、ASC、World 和 Reset/EndPlay 屏障。同步 Reset 已完成、bResetting 已恢复时，generation 变化仍能阻止旧请求。闪避清槽和取消后也检查原请求有效性。
- 第 158–177 行处理实际取消后的回调变化：重查共同决策和新缓存、会话、未决隔离与重复请求 ID；回调又产生新的取消来源时以 OperationReentered 收束，不循环追逐。TryActivateRequest 继续执行原状态、资源、冷却、目标和 GAS 门控。未发现本次修改在该直接请求链引入新的阻塞问题。
- 新测试通过正式解析、Lua、请求、Ability 完成事件触发原缺陷，五场景分别为 Reject E、Buffer E、Reset、真实 DestroyComponent/EndPlay、回调写入较新缓存。断言 D 不提前 Started/提交/施加效果/扣费，E 保持；Buffer D 仅在 E 自然结束后消费；旧 C 不复活，较新 C 按新请求身份消费；EndPlay 后不要求已失效接收者收到 native 事件。

证据继续位于下述原隔离工程 Saved/Logs，均为只读核对，Reviewer 没有重新构建或运行 UE：

| 本轮独立核对 | 结果 |
| --- | --- |
| 修前复现 | CastCE1BeforeBuild2.log 实际编译新测试并 Succeeded；CastCE1BeforeValidation2.log 第 2203 行为 CastRequestReentry Fail，第 2232 行为 1 tests performed。Reject/Buffer 下 D Started=1、能量应为 97 实为 90；Reset 下应为 100 实为 93。退出码虽为 0，测试仍明确失败。修前清单中的 Requests.cpp 哈希与旧已接受快照相同。 |
| 修后构建当前性 | CastCE1AfterBuild.log 实际编译 Requests.cpp；CastCE1FinalBuild.log 实际编译最终测试并 Succeeded。Requests 源码/obj 分别为 UTC 18:24:58 / 18:25:10，最终测试源码/obj 为 18:26:50 / 18:27:00，Skills DLL 为 18:27:01，均早于最终运行；最终测试与修前测试哈希不同，不把两次测试文件冒称完全相同。 |
| 修后运行 | CastCE1FinalValidation.log 独立提取 15 条不同目标 Test Completed，全部 Success；CastRequestReentry 为第 2825 行，第 3845 行为 15 tests performed，第 3848 行退出状态为 0。UTC 2026-11-11 18:27:39–43，即上海 2026-11-12 02:27:39–43。 |
| 修后文件 | 逐项计算当前生产与隔离副本 Source/Config/Content 的 SHA-256，与 CastCE1AfterSourceHashes.json 三方一致：409 文件、Source 217 文件，差异/多余/删除均为 0，uproject 相同。相对 CastExecutionAccepted23SourceHashes.json 仅 Requests.cpp 修改和新测试新增；修前清单相对旧快照仅新增测试。 |
| 直接回归 | 修后 15 项为 CastRequestReentry、SharedCastArbitration、CastExecutionOwnership、CastSubmissionIsolation、CastIntentLifecycle、CastGameplayEvent、MultipleSubmissions、ProductionRecords、SubmissionReconciliation、LateSubmission、UnconfirmedSubmission、AreaSubmissionSettlement、SubmissionFeedbackRouting、SubmissionFeedbackIdentity、Presentation.SubmissionIdentity，保留共同仲裁、闪避、缓存、资源竞争、多点和未决/迟到身份的直接回归。 |
| 保留限制 | 修后仍有 13 条启动 Condition failed、PIX 初始化失败及多平台 SetupSDK 失败；不能描述日志无错误。未重跑纯解析/GAS 算法、独立动作寿命、资产加载、生产 Probe 或可见运行；这些使用未变更代码及原限定证据，不当作修后运行结果。 |

证据与修复足以关闭原问题，无需为本次定向复查另起构建。以下保留首次审查定位与修前证据，均为历史记录。

## P2-CE-1 原始发现（已关闭；以下行号为修前）

- 定位：`Source/SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp:145–146`；同文件 `ClearPendingSkillIntent:251–261`、`TryActivateRequest:149–160`；`SkillLabSkillRuntimeComponent.cpp:257–268`。
- 事实：`SubmitSkillOperation` 先调用 `BuildActivationPlan`。当结果允许且没有待取消来源时，随后 `ClearPendingSkillIntent(BufferedIntentReplaced)` 会经 `Reject → Emit` 同步广播旧缓存意图的拒绝事件。监听者可以在这里正式启动另一 Cast。返回后外层直接调用 `TryActivateRequest`；该函数重查角色/资源/冷却/目标，却不重查所有有效 Cast 的操作规则。原仲裁结果此时已失效。清缓存后的入口也没有重新核对 Reset/EndPlay 屏障。
- 最小触发：A 限制期间缓存 C；A 自然完成移出执行集合，在其 Completed 监听中请求 D。D 此时取得 Allow；清除 C 时，C 的 `Rejected/BufferedIntentReplaced` 监听再正式启动 E，E 的规则为 `skill=Reject` 且保持运行。E 正常激活后，D 仍沿广播前的计划激活，与 E 并存。可分别让 E 返回 Buffer 或在通知中结束 Runtime，检查同一失效计划边界。
- 影响：E 的明确拒绝不能阻止 D，违反需求第 3、5、8.2 节“所有仍有效 Cast 共同判断、拒绝优先、全部允许后才激活”的规则。属于本次多执行及同步回调集成的正确性问题。
- 最小修复方向：在最后一次可能同步广播之后，重新确认 Runtime/生命周期有效性和完整共同仲裁结果，或以等效的请求编排屏障保证计划不会被回调改变；不能仅重查能量。
- 最小复查：复用正式目录/请求 fixture，实现上述 Completed → 缓存拒绝回调 → 新 E 的场景；断言 D 不产生 Started/效果/费用，E 原执行保持，Reject/Buffer 原因正确。修复影响请求编排后保留 SharedCastArbitration、CastExecutionOwnership、CastSubmissionIsolation 及已有身份/未决回归。

首次审查由当时源码的完整同步调用链静态确认，未新增或运行复现测试。当时 SharedCastArbitration 未在替换通知中改变执行集合，23 项 Success 不排除此问题；后续真实复现及关闭依据见上节。

## 首次审查已核对的实现与证据（修前）

实际核对了 Provider 候选替换与点/规则解析、每 Cast 执行所有权与 Ability 事件身份、共同仲裁和单槽缓存、点表及 Begin/Resolve/Abort 接口、同目标不同点去重、生产动作与交付后投射物、持续 GE 观察清理、表现播放 token/实例接口，以及对应新增测试的关键断言。

定义解析通过唯一候选入口规范化，失败候选保留旧结果；旧四生产定义的隐式单点兼容、Allow 限域、显式点校验及反射 token 复制均有最终运行测试。多点正式测试经过解析 → 请求 → Ability/Event/Timer → 真实 GE；投射物和持续效果测试有真实 Actor/ASC/GE，不是用成功 bool 代替端到端事实。Runtime、点编排、Lua 宿主、实际动作和已交付效果沿职责拆分；本轮未做全仓结构质量审计。

证据目录：`C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/`。

| 独立核对 | 结果与边界 |
| --- | --- |
| 冻结清单与当时文件 | 首次审查逐项 SHA-256 比对的 408 文件、Source 216 文件快照现备份为 `CastExecutionAccepted23SourceHashes.json`，结果汇总为 `CastExecutionAccepted23Verification.json`；当时三方一致、无清单外生产文件、uproject 相同。此数目不包含 Plugins 的全面审计。当前修后 409 文件见上节。 |
| 最终构建 | `CastExecutionFinalBuild.log` 明确 `Result: Succeeded`；最后编译 SubmissionPointExecution 并重新链接 Skills。相关新解析/测试 obj 均存在；检查到的 obj 晚于各自源码，Skills DLL 为 UTC 18:08:43，晚于最新源码 UTC 18:08:32，早于最终测试。退出码 0 由交付 JSON 记录。 |
| 最终自动化 | `CastExecutionFinalValidation2.log` 有 23 条不同目标的 Test Completed/Success，第 4047 行为 23 tests performed，第 4050 行 RequestExitWithStatus 为 0。运行时间为 UTC 2026-11-11 18:09:26–29，即上海 2026-11-12 02:09:26–29。未以汇总 JSON 或单纯进程 0 替代逐项结果。 |
| B1 当前性 | SubmissionDefinition（3303 行）及 DefinitionValidation（2942 行）均在最终进程 Success；B1 摘要“仅编译未运行”和旧失败记录是历史状态，不作为当前缺陷。 |
| 日志噪声 | 1620–1632 行有 13 条启动期 Condition failed；457 行 PixWinPlugin 初始化失败，955–967 行有 Android/IOS/Linux/Mac/TVOS SetupSDK 失败。AutoSDK 总返回 0，目标测试随后实际运行；不能称日志无错误，也不以旧 GameFeatureData 错误代替当前日志事实。 |

本轮只读核对既有构建/运行材料，没有重新构建或重跑 UE。未决和迟到身份的已接受十四项回归仍在本次 23 项中成功；不据此重新宣称需求 8.1 整体完成。

## 仍保留的交付边界

- ProductionProbe 为最终测试补充前构建 11 的真实 `-game/NullRHI` 生产链证据，不能冒称最终 23 项快照的完整生产端到端验收。
- 四个生产 Montage 只读加载均成功，但都使用 DefaultGroup/DefaultSlot；没有兼容双 group 并播或可见 PIE 证据。Lua stop 是适配器 token 绑定验证，不能替代真实双 Montage 视觉验收。
- 沉默/硬直多来源测试使用真实 GE；不代表已有可游玩的生产授予者。普通/重击测试也不证明所有生产攻击者已正确选择类别。
- 暂停的 C1 资源写入及二进制资产/GE 审计、可见运行验收仍待 PM 接续；本报告不代做全审，不承诺跨进程恢复。
- 当前项目事实文档尚需在修复及 Review 后同步多执行、点调度、交付后寿命、表现 token 和新增测试入口；本次仅写审查报告，不改这些被审文档。

P2-CE-1 已关闭，由 PM 接续剩余审计、可见验收和项目事实文档同步；本报告不宣布整批需求完成。


## 2026-11-12 R 资源修复与 Reset 跨层接入定向复审

**结论：本轮未发现需要立即修复的确定性代码缺陷；资源协调和 Reset 接入的已覆盖路径可接受，但交接要求的 Reset 专项验证尚未全部闭合。** 不重开已接受的 B1、多 Cast 和 P2-CE-1；不以本轮结论宣布需求 8.1 或整批完成。此前末尾“C1 资源写入暂停”的文字为历史状态，以本节为当前定向审查结论。

- `SkillLabAbilitySystemResource.cpp:36` 将钳制后的目标写入 base 并核对 current/base，先检查预留；资源变更队列在执行前重新校验，项移出队列后才回调，Queued 不是最终成功。Tracked 回执区分 Applied、Rejected、Failed、Unavailable；EndPlay/BeginDestroy 关闭待处理项，失效接收者不强行通知。失败可能已有副作用，不推断回滚。
- `SkillLabSkillRuntimeComponent.cpp:213` 在取消前登记 Attempt/ASC/generation；清理时绑定新 generation，进入 Waiting 后请求资源。`BeforeApply` 再核对身份和未决，完成处理核对回执身份，只有 Applied 且无未决才清已确认冷却；EndPlay 使旧尝试失效。资源写入及 Health/效果清理不具有整体事务原子性。
- 静态资源 modifier 在 GE 应用前拒绝；`SkillLabCombatAttributeSet.cpp:41` 对 execution 实际 Energy/MaxEnergy 输出逐项 veto，非资源输出仍可执行。混合 execution 不保证整笔 GE 回滚；调用方不得把 GE 整体成功当作资源输出已写入。
- GAS 集成用例覆盖立即与排队许可拒绝、后续队列、回调重入、执行后故障注入、EndPlay/BeginDestroy 和失效接收者；正式 Reset 用例覆盖立即及排队 Applied、Health 回调屏障、旧 Runtime 替换、故障注入 Failed、ASC 结束及 Actor 销毁。已有未决和迟到回归覆盖阻塞、预留/隔离、冷却及迟到收束。上述 Failed 使用不支持的重入写入故障注入，不是合法 Rejected。

**仍须补证：**交接要求正式 Reset 的无未决立即 Rejected、Queued 后合法 Rejected，或用实际执行顺序和生产边界证明相应组合不可达；当前通用 GAS 的人工许可拒绝不足以证明 Runtime 端到端拒绝路径，亦不能据此断言所有合法拒绝不可达。还需针对取消收束中新增未决以及成功资源回调重入的正式场景补充测试或可核查的不可达分析，核对一次终态、屏障、预留、隔离和冷却。现有代码的多次未决检查和成功路径静态证据不能替代这些专项运行证据。这些是交接验收缺口，当前不将其描述为已复现的生产缺陷。

**独立证据核对：**Reviewer 只读检查，未重新构建或运行。`C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/ResetResourceFinal3Hashes.json` 的 SHA-256 为 `1049706A67CAA6CBE2E46D2F7D3588ABD82A5849E33DF8748E2B96CF7EAF8A70`；逐项计算生产工程、隔离工程和清单的 Source/Config/Content 共 416 文件（224/43/149），三方一致、双方无额外或缺失文件，uproject 哈希一致。此比较不覆盖 Plugins 或引擎。

`ResetResourceFinal3Build.log` 显示实际编译 Reset 测试并链接 Skills DLL，Result Succeeded；隔离工程相关 GAS、Runtime、测试 obj 及 GAS/Skills DLL 均存在且早于最终运行。`ResetResourceFinal3Validation.log` 分别记录 EnergyLedger、EnergySettlementHistory、ResourceExecutionBoundary、ResourceWriteIntegration、CastRequestReentry、LateSubmission、ResetResourceCompletion、UnconfirmedSubmission 八个不同目标 Success，队列共 8 tests performed，RequestExitWithStatus 为 0。启动阶段仍有 13 条 `Condition failed` 和 PIX 初始化失败，不能称日志无错误；上述目标确实随后执行成功。旧隔离快照的 R 全量用例不冒称在此交叉快照重跑。

可见 PIE、双 Montage、资源资产审计的剩余范围及项目事实文档同步仍由 PM 接续；本次只更新本报告。


## 2026-11-12 Reset 三项补证定向复查

**结论：取消收束中新未决、成功路径重入两项补证可接受；合法资源 Rejected 的不可达论证仍缺关键前提，本轮资源/Reset 验收暂不完全闭合。** 本轮未重新审查 R+A 全部实现或已接受功能，也未改生产代码。

1. **合法 Rejected：仍需补证。** `SkillLabSkillRuntimeComponent.cpp:310` 先取得 ResetEnergy，随后第 311 行写 Health 并同步触发委托，直到第 326 行才提出资源请求。原论证将“取值时预留不超过旧 MaxEnergy”直接用于请求执行时，忽略 Health 委托可通过正式资源协调接口改变 MaxEnergy、Energy 和预留。可核查的最小场景：初始 MaxEnergy=100、Health 未满，Health 委托经协调接口将 MaxEnergy 和 Energy 均提升到 200，再经 `BeginEnergyEscrow` 为同 ASC 建立成本 150 的合法独立预留；无未确认提交时，外层 Reset 仍以旧目标 100 Override，`SkillLabAbilitySystemResource.cpp:151`–166 因低于当前预留返回 Rejected。`BeginEnergyEscrow` 的写深度和队列门控（`SkillLabAbilitySystemEnergyReservations.cpp:23`–38）不禁止这里的立即路径，因为 Health 委托本身不处于资源写入保护中。此为静态可达性反例，尚未运行复现；不预判生产内容一定存在该委托，也不把它称为已复现的代码缺陷。请使用正式 Reset、正式协调接口及合法预留验证 Rejected 的一次报告、屏障释放、预留与冷却事实，或指出可核查的现有业务约束为何排除该组合。

   对真正处于资源写保护内入队的路径，队列未空或写深度非零时禁止新增预留；先行合法 MaxEnergy 降低须保持现有预留，升高也不能单独增大旧预留，因此在有限、无聚合、有效会话且无原生旁路的限定下，排队后资源参数/预留 Rejected 的分析可接受。运行中失效与新增未决分别按 Unavailable 或许可阻断处理，不等同该资源 Rejected；不将此结论外推到任意原生旁路、非法初始状态或未审计内容。

2. **取消收束中新未决：可接受。** `SkillLabResetResourceCompletionTests.cpp:171` 从正式 Cast/Begin 和真实生产记录出发，提交中 Reset 后丢失返回，经未决交接再完成 Reset，断言一次 Blocked、实际 Energy 100、唯一预留 2、隔离及无早冷却；重复查询真实迟到结果只扣一次至 98、释放预留并启动冷却。对应代码先 `StartIndeterminateWait` 留存记录，再通过终止收束进入 `CompleteReset`。该结果闭合本项限定场景，不承诺跨进程恢复。

3. **成功路径重入：可接受。** 当前 `OnResetResourceFinished` 的成功分支清确认冷却，`ResetConfirmedCooldowns` 只操作内部映射；`FinishResetAttempt` 先清本次状态，成功只写日志，没有对外成功委托，故没有可由产品监听者进入的“成功回执内部新 Attempt”入口。新增测试覆盖资源写入期间实际 Energy 委托中的重复 Reset/请求屏障，以及写入中销毁旧 Runtime 后替换请求；Energy 委托发生在最终回执之前，不能称为成功回执回调测试。未来若新增成功通知，须重新验证通知中的新 Attempt 和旧栈收尾。

**独立证据：**`ResetEvidenceFinalHashes.json` SHA-256 为 `0B11F9F4BCBFF25B88486899C95178803A6AEDBF717A5A48B0A271EF9C0C2DBA`；生产工程、隔离副本和清单的 Source/Config/Content 416 文件逐项哈希一致（224/43/149），无额外或缺失文件，uproject 一致。与此前独立核对的 `ResetResourceFinal3Hashes.json` 相比仅 `SkillLabResetResourceCompletionTests.cpp` 改变；不覆盖 Plugins 或引擎。构建日志实际编译该测试并链接 Skills，Result Succeeded；运行日志的 ResetResourceCompletion 与 ResetSubmissionDuringCompletion 均 Success，2 tests performed，退出状态 0，启动期仍有 13 条 Condition failed。旧八项目标没有在此快照重跑，不称为十项同轮通过。Reviewer 未重新构建或运行。

本节更新上一节的三项缺口判断；可见 PIE、双 Montage、资源资产审计剩余范围及项目事实文档同步仍待后续，不能据此宣布整批完成。


## 2026-11-12 立即合法资源 Rejected 补证复查

**结论：上一节的立即合法 Rejected 验证缺口关闭；结合此前已接受的排队限定分析、取消收束中新未决及成功路径重入证据，R 资源修复与 Reset 跨层接入的本轮专项验收通过。** 这是限定于本次合同和证据范围的结论，不宣布整批完成。

`SkillLabResetResourceCompletionTests.cpp:36` 经正式技能建立已确认冷却，Health 设为 40 后调用正式 Reset；Health 委托经协调接口将 MaxEnergy/Energy 升至 200，建立独立预留 150。对照 `SkillLabSkillRuntimeComponent.cpp:310`–326，Reset 已在该委托前取得旧目标 100；`SkillLabAbilitySystemResource.cpp:151`–166 的预留校验因目标低于预留而拒绝，回执由 Runtime 映射为 `ResetResourceChangeRejected`，结束屏障且不清确认冷却。测试断言一次拒绝、Health 委托一次、Energy 仅有委托内一次写入，实际/最大/预留/可用为 200/200/150/50，已有冷却保持；另一技能可请求，独立预留可释放。日志实录资源 Rejected、Reset 拒绝及对应事件，支持该场景实际执行；没有将故障注入 Failed 当成合法拒绝，也没有补偿或自动重试。

Reviewer 独立核对 `ResetImmediateRejectedHashes.json` SHA-256 为 `7A0A64A063F094D9D37FDBD5288EB90DE749493A261793BAF405E054C220ACA1`。生产工程、隔离工程及清单 Source/Config/Content 416 文件逐项一致（224/43/149），无额外或缺失文件，uproject 一致；对比 `ResetEvidenceFinalHashes.json` 仅该测试文件变化，比较不覆盖 Plugins 或引擎。`ResetImmediateRejectedBuild.log` 实际编译测试、链接 Skills，Result Succeeded；`ResetImmediateRejectedValidation.log` 中 ResetResourceCompletion 为 Success、1 tests performed、退出状态 0。启动仍有 13 条 Condition failed 和 PIX 初始化失败；此前八项及两项证据保留在各自快照，不冒称本轮全部重跑。Reviewer 未重新构建或运行。

上一节关于立即 Rejected 待证的结论由本节关闭；此前排队分析的有效会话、有限资源、无聚合及无原生旁路等限定继续有效。可见 PIE、双 Montage、资源资产审计剩余范围及项目事实文档同步仍待后续，不作跨进程承诺。
