# 时序、窗口、规则配置与仲裁实现摘要

## Runtime 实际反馈的原提交身份补齐（2026-11-11）

状态：**按 PM 转交的原 Reviewer 结论，P2-1 范围未知错误释放已关闭，保留修复和回归。本轮补齐 P2-2 的实际通道身份，重新构建与十四项自动化通过，源码/测试再次冻结，交原 Reviewer 定向复查；未宣称 P2-2 已接受或批次完成。** 下节十二项通过是本轮修改前证据。

- 依据仍为需求第 8.1 节的原身份关联、重试/通知与生命周期边界，以及当前三份交接。本轮只增加结果身份载荷及必要反馈适配，不改变结算、隔离、原 Cast 终止或新 Cast 门控语义；不改 Lua/JSON 生产内容、资产、策划和旧批次，不提交/推送，不重开 P2-1 或其他已关闭切片。
- **身份产物。** 复用 `FSkillLabSubmissionKey`，将其声明为 Blueprint 可读的结构：原操作创建时分配 `FGuid Id`，同时快照原 `RequestId`；原会话/ASC/Runtime 的 FObjectKey、generation、Cast、point、transaction、operation 与 SkillId 保留。Blueprint 通过 `SubmissionKey.Id` 比较稳定的不透明原操作身份，并可读原 RequestId/SkillId；原生接收者可读完整键。`WithCopy` 确保动态委托及反射值拷贝也保留完整原生键。新 Id 加入键相等检查，每个实际子操作另分配自己的 Id；它只用于关联，不提供确认/结算权限，也不持有原 Runtime 或解析失效对象。
- `FSkillLabSkillResult` 新增只读 `SubmissionKey`。Begin 时从原 `ActiveRequest.RequestId` 保存真实请求身份；`MakeResult` 仅在对象/generation/Cast 匹配当前原提交时附带其键，原取消/效果/提交反馈可用于后续关联。`OnSubmissionConfirmed` 直接复制确认中的原完整键和原 RequestId 到实际 `OnSkillEvent / OnNativeSkillEvent` 载荷，不使用替换 Runtime 的身份或默认 0。此前路由、通知去重及生命周期保护保持。
- **消费点核对。** 已检查现有 PresentationRouter 转发、表现 Lua 宿主、PlayerController RuntimeProbe 和 TestCommands smoke 观察点。Router 转发完整结果、Probe/smoke 只记录事件，不按迟到结果推进 Cast，无需改业务。表现 C++→Lua 宿主会另建表，因此仅补 `submissionId` 字符串透传，以及 `SubmissionCommitted / SubmissionResolved` 的真实事件名；没有修改生产脚本或动画策略。对应文件为 `Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp`；其头文件仅增加独立测试访问。
- 修改文件还包括 Skills 的 `SkillLabSubmissionResults.h/.cpp`、`SkillLabSkillTypes.h`、`SkillLabUnconfirmedSubmissions.h`、`SkillLabSubmissionReconciliation.cpp`、`SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeComponent.cpp`、`SkillLabSkillRuntimeConfirmation.cpp`。`SkillLabProductionFixture.h` 支持指定原 RequestId，原反馈路由测试新增实际动态/原生载荷的完整键断言；新增独立 `SkillLabSubmissionFeedbackIdentityTests.cpp` 与 `SkillLabPresentationSubmissionIdentityTests.cpp`。未扩大旧 CastGameplayEvent 测试。
- **正式同技能碰撞验证。** `SkillLab.Skills.SubmissionFeedbackIdentity` 从正式请求/Ability→RequestDeclaredAction→真实范围/GE→Resolve 产生接受/拒绝记录，仅在真实 GE 查询内暂挂既有未扣 reservation。原 Runtime 销毁后先在无接收方的间隙收敛并解除隔离；成功用例通过真实经过时间推进 World，等待原 0.1 秒冷却到期。随后替换 Runtime 正式激活相同 `Receipt_ShortArea`，并 Begin 相同数字 `Cast=1 / point=1` 的新操作，再派发旧结果。在真正动态 UFUNCTION 接收端与原生委托接收端分别核对原完整键、稳定 Id、非默认原 RequestId、实际成功/失败及反射副本，确认旧 Id 不等于新 Id，无须从当前 Runtime 反查身份。两个用例的原 RequestId 为 701 / 702，新请求为 1；各通道只补报一次，包含同步查询重入。
- 同一测试还断言新 Cast/阶段不变、新生产记录仍 InProgress、新 reservation 和实际余额不变、旧冷却不重启、真实 GE 不重放；旧反馈完成后，新操作仍能独立实际执行并正式结算，只产生自己的费用。`SkillLab.Presentation.SubmissionIdentity` 只验证既有 C++→Lua 适配器对 Id/请求/事件名的载荷保真，不冒充生产结果测试。原反馈缺失/重建接续、取消原等待时销毁接收方、动态回调销毁 Runtime 后停止原生调用、重复/重入及真实有限等待均继续回归。
- **最终证据。** `Saved/Logs/SubmissionFeedbackIdentityBuild.log` 第 29 行 `Result: Succeeded`，进程退出 0；新增文件后使用 `-gather` 刷新 UBT 源文件列表，最终构建确实编译新增表现适配测试。`Saved/Logs/SubmissionFeedbackIdentityValidation.log`，UTC **2026-11-11 15:55:35–38**，进程退出 **0**：上述两项新增测试及上轮十二项全部 Success，0 目标失败、0 AutomationController Error。第 **2894 / 2905** 行分别记录明确失败 / 冷却已过期成功的同技能碰撞，均为 `Skill=Receipt_ShortArea Cast=1 Point=1 OldRequest=701/702 NewRequest=1 Dynamic=1 Native=1`，且原/新 Id 不同；第 2911 行为正式身份测试 Success，第 2127 行为表现适配测试 Success。第 3246 / 3257 行继续保留已关闭销毁场景的三事实 `1/0/0` 与 `1/1/0`。仍有 13 条既有启动期 `Condition failed` 和 GameFeatureData 配置错误，不将日志描述为无错误。
- **验证源码一致性。** 继续使用 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation` 隔离项目；运行前同步本项目整个 `Source/`，运行后核对全部 **194 份文件**，当前工作区、隔离副本及运行前 SHA-256 快照差异 **0**，两侧文件数量相同。生成验证清单为 `Saved/Logs/SubmissionFeedbackIdentitySourceHashes.json`，不是生产配置。此后仅更新本摘要，不再修改源码/测试。
- 复现构建：`D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat SkillLabEditor Win64 Development C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/SkillLab.uproject -WaitMutex -NoHotReloadFromIDE -gather`。自动化使用完整 `D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe` 路径及该隔离项目，参数 `-unattended -nop4 -NullRHI -nosplash -nosound`、`-ExecCmds="Automation RunTests SkillLab.Skills.SubmissionFeedbackIdentity+SkillLab.Presentation.SubmissionIdentity+SkillLab.Skills.AreaSubmissionSettlement+SkillLab.Skills.SubmissionFeedbackRouting+SkillLab.GAS.EnergySettlementHistory+SkillLab.Skills.ProductionRecords+SkillLab.Skills.SubmissionReconciliation+SkillLab.Skills.LateSubmission+SkillLab.GAS.EnergyLedger+SkillLab.GAS.DamageCalculation+SkillLab.Skills.UnconfirmedSubmission+SkillLab.Skills.CastGameplayEvent+SkillLab.Skills.CastLifecycle+SkillLab.Skills.DefinitionValidation"`、`-TestExit="Automation Test Queue Empty"`，`-abslog` 指向上述最终日志。
- **剩余边界。** 结构中的完整键和 Id 服务于本会话关联，不承诺跨进程恢复/持久化，也不以它们补造缺失生产事实或未知扣款。对象失效、缺少反馈入口时的既有保护保留。未修改或运行 Blueprint 资产、未做可见 PIE/MCP；消费点核对为现有 C++ 接线和实际动态/原生通道测试，不等于二进制资产审计。多提交生产内容、异步所有权、全路径写入/资产 GE 审计与 PIE/Montage 等批次剩余范围不变。本轮无需要另定公共业务语义的阻塞。

## 独立 Review 两项 P2 局部修复（2026-11-11）

状态：**两项缺陷已修复，最终隔离构建与十二项自动化通过；本次源码与测试重新冻结，交 PM 安排原 Reviewer 定向复查。尚未取得复查接受结论，不宣称第 8.1 节或整个批次完成。** 下节十项通过是修复前证据，本节使用重新构建后的最终日志。

- 范围依据为 PM 交回的两项 P2、需求第 8.1 节“权威结果收敛 / 重试与通知 / Reset 与生命周期边界”，以及当前 `SubmissionEffectsHandoff.md`、`VerificationHandoff.md`、`TimingRulesHandoff.md`；沿项目入口和程序实现 Skill 取得分层约定与反馈/所有权基线。只修改正式提交入口、会话反馈接线及独立测试，不重做架构、不扩大需求、不改 Lua/JSON 生产内容、资产、策划或旧批次，不提交/推送。为核对同步销毁与重入，按直接依赖补读 Runtime 初始化、EndPlay、Emit、延迟终止，以及 GAS reservation 状态接口；未通读或重写 Provider/旧大集成测试。
- **P2-1：范围未知不能按 false 释放。** `SkillLabSkillRuntimeSubmission.cpp::ResolveBehaviorSubmission` 在执行态的动作返回 false 时，先按原完整键查询生产记录，只有 `ConfirmedRejected` 允许按失败结算；未确认/不可用结果通过既有 Abort 路径标记未决并交给会话所有者，先保留 reservation 与同技能隔离，再完成延迟 EndPlay。全部子操作明确拒绝仍安全释放；原动作接受路径及既有扣费算法不变。`SkillLabSkillRuntimeComponent.cpp::ApplyBehaviorEffect` 在根本没有进入执行器的明确失败分支，补写对应 Effect 生产拒绝记录，保证现有失败后重试仍有真实依据；投射物交付记录不由这条失败适配改写。
- **P2-2：反馈路由与原 Cast 身份分离。** 会话通过弱引用选择当前初始化成功的 Runtime 作为现有反馈入口，EndPlay 撤销该入口；`SkillLabSkillRuntimeConfirmation.cpp` 按同会话/同 ASC 校验交付，只有原 Runtime 对象、generation、Cast 全匹配才结束原未决等待。替换 Runtime 只转发现有 `OnSkillEvent / OnNativeSkillEvent` 的 `SubmissionResolved`，保留原 SkillId、Cast、point 和原完整生产键，不推进/结束新的 Cast，不重新执行效果。
- 会话分别保存业务已收敛和通知已送达的状态：`SkillLabSubmissionReconciliation.cpp` 保留待交付确认，Runtime 重建间隙可以先完成费用/冷却与隔离解除，但不提前标记已通知；有效接收方出现后仅重试反馈交付，不重做结算。交付前锁定通知以防重复/重入；若结束原等待的回调令 Runtime 失效、最终补报尚未发出，则留待有效入口。若动态反馈已经收到而同步销毁 Runtime，不再调用其原生通道，也不向替代 Runtime 重发已送达结果。现有会话观察委托在有效 Runtime 反馈派发后广播一次，身份仍是原操作。上述变化均为内部接线，未新增脚本结果确认入口或改动策划语义。
- 修改生产文件：`SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeComponent.cpp`、`SkillLabSkillRuntimeConfirmation.cpp`、`SkillLabSubmissionReconciliation.cpp`、`SkillLabSkillRuntimeComponent.h`、`SkillLabUnconfirmedSubmissions.h`。新增测试 `SkillLabAreaSubmissionSettlementTests.cpp`、`SkillLabSubmissionFeedbackRoutingTests.cpp` 和独立动态委托探针 `SkillLabSubmissionFeedbackProbe.h`；原 `SkillLabLateSubmissionTests.cpp` 补充真实等待到期后的原/替换 Runtime 补报断言。不扩旧 CastGameplayEvent 大测试，不修改本轮 GAS 实现。
- **正式路径验证。** `SkillLab.Skills.AreaSubmissionSettlement` 用正式请求/Ability → `RequestDeclaredAction` → 真实范围查询/GE → `ResolveBehaviorSubmission` 覆盖四种情况：首个 GE 拒绝并 EndPlay、后续子操作未知；所有子操作明确拒绝；单个子操作拒绝且 EndPlay（整体已知拒绝）；已知部分接受且 EndPlay。断言未知时资源历史 `ReservedNoDebit`、实际/预留/可用 `100/2/98`、未决所有权、重建后的同技能拒绝及其他新 Cast 不受影响；明确拒绝为 `ConfirmedReleased`、零预留、无费用/冷却；已知部分成功收费一次。没有人工丢弃动作返回或直接 Abort 替代正式结算。
- `SkillLab.Skills.SubmissionFeedbackRouting` 同样经过正式请求和动作/Resolve：仅在真实 GE 应用查询回调中使用已有资源接口暂挂未扣 reservation，使正式结算暂不可闭合；真实 GE 接受/拒绝和生产记录照常产生，后续沿既有未决资源接口收敛。成功/失败分别覆盖原等待、原 Runtime 的新 Cast、重建 Runtime 的新 Cast、重建间隙先结算后交付。动态和原生通道各收到一次实际结果；校验原完整键与 SkillId/Cast/point、费用/冷却事实、收费次数、真实效果未重放、重复查询及各层同步重入。特别验证新旧 Runtime 的 Cast 数字同为 1 时也不误结束新 Cast。另覆盖结束原等待时接收方销毁后的交付接续，以及动态反馈销毁 Runtime 后原生通道不调用、不重报。
- **最终证据。** `E:/agent/game/ue/SkillLab/Saved/Logs/ProductionReviewFixBuild.log` 第 29 行为 `Result: Succeeded`，构建进程退出 0。最终自动化日志 `ProductionReviewFixValidation.log`，UTC **2026-11-11 15:40:55–58**，进程退出 **0**：新增 `AreaSubmissionSettlement`、`SubmissionFeedbackRouting`，以及原 `DamageCalculation`、`EnergyLedger`、`EnergySettlementHistory`、`CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`LateSubmission`、`ProductionRecords`、`SubmissionReconciliation`、`UnconfirmedSubmission` 十二项均 Success，0 目标测试失败、0 AutomationController Error。第 2142 行记录正式范围未知时 `Actual=100 Reserved=2 Isolated=1`；第 2862–3018 行的八组正式反馈均为 `Dynamic=1 Native=1 Session=1`；第 2691 行记录真实迟到确认 `ConfirmedAt=1.111`；第 3173 / 3184 行保留原销毁诊断 `ActionSucceeded/ResourceResolved/CooldownRecorded=1/0/0` 与 `1/1/0`。仍有 13 条启动期 `Condition failed` 和既有 GameFeatureData 配置错误，不能称整份日志无错误。
- 隔离项目继续使用 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/SkillLab.uproject`。最终运行前同步 Skills/GAS，运行后再次核对这两个模块全部 **66 份源文件**与隔离副本和运行前快照的 SHA-256，差异 **0**；可复核的生成清单位于 `Saved/Logs/ProductionReviewFixSourceHashes.json`，仅为验证记录，不是生产配置。此后仅更新本摘要，未修改源码或测试。
- 可复现命令：`D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat SkillLabEditor Win64 Development C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/SkillLab.uproject -WaitMutex -NoHotReloadFromIDE`；随后完整路径启动 `D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe`，使用该隔离项目及 `-unattended -nop4 -NullRHI -nosplash -nosound`、`-ExecCmds="Automation RunTests SkillLab.Skills.AreaSubmissionSettlement+SkillLab.Skills.SubmissionFeedbackRouting+SkillLab.GAS.EnergySettlementHistory+SkillLab.Skills.ProductionRecords+SkillLab.Skills.SubmissionReconciliation+SkillLab.Skills.LateSubmission+SkillLab.GAS.EnergyLedger+SkillLab.GAS.DamageCalculation+SkillLab.Skills.UnconfirmedSubmission+SkillLab.Skills.CastGameplayEvent+SkillLab.Skills.CastLifecycle+SkillLab.Skills.DefinitionValidation"`、`-TestExit="Automation Test Queue Empty"`，`-abslog` 指向上述最终验证日志。最初两项定向测试也通过，但最终以十二项日志为准。
- **保留边界。** 范围中断且无已知成功、仍有缺失子结果时，继续未知与隔离，不凭未执行推断整体失败或补发动作；副作用至返回记录之间生产会话失效、资源扣款已尝试但无闭合证据仍不能凭空恢复。有效会话内暂缺 Runtime 的结果可等待交付；Actor/ASC/会话结束后仅保留可诊断事实，不向失效对象发送，不承诺跨进程恢复。未运行可见 PIE/MCP；多提交生产内容、异步效果所有权、全路径资源写入/资产 GE 审计及 PIE/Montage 仍按本批 README 接续。本次仅请求原 Reviewer 对这两项 P2 定向复查，不重开已关闭切片。

## 真实生产记录、资源事实查询与同会话迟到收敛（2026-11-11）

状态：**本次生产记录 → GAS 资源查询 → 会话消费的接入及十项自动化完成，源码与测试冻结，交 PM 安排原独立 Reviewer 审查；尚未宣称本轮 Review 通过或第 8.1 节整体完成。** 依据 PM 本轮交接，账本/Runtime-HUD、会话所有权/有限等待/冷却重建/逐记录诊断及结算委托销毁 P2 已在其限定范围关闭；下方各节“待复查”保留为当时状态，不重新打开。

- 输入采用本批需求第 8.1 节、当前 `SubmissionEffectsHandoff.md` 的“下一任务：真实生产记录与迟到结算接口”和“本任务首轮读取”、`VerificationHandoff.md` 的“入口与交付”，并按项目入口取得程序分层约定及相关共享业务基线。本轮不改 Lua/JSON 生产内容、资产或策划需求，不提交/推送；测试中的 Lua/JSON 是内存夹具，经现有 Provider 校验。
- 新增独立 `SkillLabSubmissionResults.h/.cpp`，由原同 ASC 会话组件持有生产记录。执行前分配不可复用 operation，完整键包含会话、ASC、Runtime 对象身份、generation、SkillId、Cast、point 和可选 transaction；TransactionId=0 的已计费后点仍有独立 operation。记录保存生产类型、费用/冷却快照、已有计费事实、版本、原 World 时钟及可选可信接受时间。查询无副作用，不存在或身份不符为 Unknown；InProgress、ConfirmedAccepted、ConfirmedRejected、Unavailable 分别保留。只有实际原生生产适配器可写，重复确认不递增版本，矛盾写入被诊断且不覆盖既有事实，脚本没有结果确认入口。
- 即时生产链为 Executor → Resolver → 正式 ASC/GE 调用；Resolver 通过原生 `OnApplicationReturned` 回调报告基于 `WasSuccessfullyApplied()` 的真实返回结果，Executor 按原键记录，早于 Runtime 的 Hit/EffectApplied 外层广播。GAS 属性及死亡委托仍先于该返回；测试在属性通知内观察 InProgress、在外层 Hit 内观察已确认。仅知道 GE 返回时刻，不伪造更早的接受时刻，迟到消费使用确认时间起算冷却。若属性委托结束生产会话而未能记录，保持 Unavailable，不从 Health 差额或命中补造接受事实。
- 范围生产代码移至 `SkillLabSkillBehaviorProduction.cpp`，调用真实 `FindAreaTargets`，该接口现返回查询是否有效完成。独立结果单元在执行前登记全部子操作：真实空查询记录空范围接受；任一已知子操作接受即记录当前 any-applied 边界的部分成功；只有全部计划子调用明确拒绝才记录整体拒绝。中断后仅有拒绝及缺失子结果时保持 InProgress，已确认子结果不依赖 Behavior 存活，也不重执行。
- 投射物 Initialize/BeginPlay 在持有生产交付键时保持碰撞关闭，FinishSpawning 完成后通过 `ConfirmDelivery` 确认创建/初始化交付并记录可信 World 时间，再开放碰撞和运动。真实同步 overlap 可在 activation 内应用 GE、回调并自毁；交付仍为已接受，Hit/Blocked/Expired/Destroyed 不改写该记录。Executor 只写 Effect 类型操作，避免同步 overlap 的 GE 把创建交付误当作效果回执。直接无生产键的既有投射物使用方式保持兼容。
- GAS 新增 `SkillLabEnergySettlement.h/.cpp` 及 ASC 的 `BindEnergyOperation`、`QueryEnergySettlement`。按原交易保留 owner/generation/Cast/费用、会话与 Runtime/operation 关联及已确认闭合版本；区分 ReservedNoDebit、SettlementInProgress、ConfirmedCommitted、ConfirmedReleased、DebitAttemptUnconfirmed、Unavailable。原扣费算法完成既有校验后才写入闭合历史，查询不以 reservation 消失、当前余额或 bDebitObserved 推断成功。ASC EndPlay/销毁后查询不可用；已有扣款尝试且无闭合证据时，成功重扣和失败释放均拒绝，保留预留。这是本交接要求的必要防护；旧 EnergyLedger 仅把该处“可按失败释放”的断言改为拒绝并保留未决，不扩展旧大集成用例。
- 会话消费位于独立 `SkillLabSubmissionReconciliation.cpp`：Tick 或无结果参数的 `ReconcileProductionResults` 只查询完整原键及两类事实，记录级 Resolving/Resolved 防止资源委托与通知重入。动作接受且资源从未尝试扣费时，沿原 Resolve 接口结转一次；已确认闭合直接复用历史；扣款未知、来源/资源身份不符或两类事实矛盾时继续隔离且不补偿。明确首次拒绝仅在资源从未扣款/已明确释放时闭合；已计费后点不结转费用，不重启或清除已有冷却。可信接受时间加原冷却时长形成唯一截止时间，无该时间取确认时间；截止时间已过也不重启。同步路径已交付过的冷却另有记录级事实，迟到消费不会重写它。
- 已收敛记录保留供诊断，仅在该技能全部未决记录收敛后解除隔离。补充结果广播前标记已解决/已通知，至多报告一次动作结果、资源闭合、费用及冷却事实；新增 `SubmissionResolved` 事件与 `SkillLabSkillRuntimeConfirmation.cpp` 接回原 Runtime 通道，不伪造 Hit/治疗/退款，不重启 Cast。只结束匹配原 Runtime/generation/Cast 的未决等待，不能影响当前其他 Cast 或替换 Runtime。资源、冷却和广播之后继续核对寿命；消费中 Actor 销毁为 ActionSucceeded=1/ResourceResolved=0/CooldownRecorded=0，会话组件 EndPlay 为 1/1/0，均不重扣或补偿。
- 新增独立 `ProductionRecords`、`EnergySettlementHistory`、`SubmissionReconciliation`、`LateSubmission` 四项测试；共用小 `SkillLabProductionFixture.h` 与提取的 `SkillLabSubmissionTestWorld.h`，原 UnconfirmedSubmission 仅复用 World 创建顺序，不扩旧 CastGameplayEvent 大测试。真实链覆盖即时接受/目标拒绝、正式 GE 应用查询拒绝、范围空/全部拒绝/部分接受/中断未知、生产会话丢失、创建握手后的同步 overlap 与自毁及外部 Destroy、资源结算委托重入/原生旁路故障注入、明确提交/释放历史、ASC EndPlay/销毁、首次及已计费后点、重复与乱序交接、通知重入/接收方 EndPlay。迟到测试只暂停结果查询，实际调用与记录已由生产链完成；真实墙钟推进 World/TimerManager 超过 1 秒，随后验证异常终态后的确认、Reset 后收敛、Runtime 重建/新 Cast 不受旧结果影响、可信/未知时间及已过期冷却、始终无结果继续隔离。后点覆盖为现有事务 API 边界，不代表多提交生产内容已完成。
- 最终 UE 5.8 Editor Win64 Development 隔离构建成功；日志 `E:/agent/game/ue/SkillLab/Saved/Logs/ProductionReconciliationBuild.log` 第 111 行为 `Result: Succeeded`。最终自动化日志为同目录 `ProductionReconciliationValidation.log`，UTC 2026-11-11 15:24:49–51，进程退出 0：`SkillLab.GAS.DamageCalculation`、`EnergyLedger`、`EnergySettlementHistory`，以及 `SkillLab.Skills.CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`LateSubmission`、`ProductionRecords`、`SubmissionReconciliation`、`UnconfirmedSubmission` 十项均 Success，无目标测试失败或 AutomationController Error。Skills/GAS 两模块全部 63 份源文件与 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/Source` 副本 SHA-256 一致。测试初版曾缺投射物必填数据、误用越上限预期以及未正确分发 BeginPlay/重复 EndPlay，已修正；以本次最终日志为准。
- 可复现命令：`D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat SkillLabEditor Win64 Development C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/SkillLab.uproject -WaitMutex -NoHotReloadFromIDE`；随后以完整路径 `D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe` 启动该项目，参数 `-unattended -nop4 -NullRHI -nosplash -nosound`、`-ExecCmds="Automation RunTests SkillLab.GAS.EnergySettlementHistory+SkillLab.Skills.ProductionRecords+SkillLab.Skills.SubmissionReconciliation+SkillLab.Skills.LateSubmission+SkillLab.GAS.EnergyLedger+SkillLab.GAS.DamageCalculation+SkillLab.Skills.UnconfirmedSubmission+SkillLab.Skills.CastGameplayEvent+SkillLab.Skills.CastLifecycle+SkillLab.Skills.DefinitionValidation"`、`-TestExit="Automation Test Queue Empty"`，以 `-abslog` 指向上述验证日志。实际命令行也保存在该日志启动部分。
- 证据边界：最终日志仍有 13 条启动期 `LogAutomationTest: Error: Condition failed` 及 GameFeatureData 资产管理配置错误，不称整份日志无错误。源会话在 GE 不可逆副作用到真实返回记录之间失效仍只能 Unknown/Unavailable；bDebitAttempted 且无资源闭合证据的情况仍保持隔离，不承诺此类未知一定能收敛。没有可见 PIE/MCP 或实际 Montage 停止验收，也没有跨进程恢复保证。本批多提交内容/异步动作所有权、全部合法资源写入及资产/GE 审计、PIE/Montage 仍按批次 README 的接续映射推进，不是永久排除；本次闭环交付不等于第 8.1 节或批次完成。原生旁路测试只证明不误确认，不证明旁路受保护。

## 结算属性委托销毁生命周期 P2 修复（2026-11-11）

状态：**修复及最终回归完成，源码与测试冻结，待原 Reviewer 复查；本节不代表 Review 已通过，第 8.1 节整体仍未完成。** 以下上一轮冷却寿命与逐记录诊断的验证记录保留，不代替本轮结算回调销毁路径的证据。

- 费用结算可能同步触发属性委托，销毁 Actor 或结束同 ASC 会话组件。`ResolveBehaviorSubmission` 现移至独立 `SkillLabSkillRuntimeSubmission.cpp`：在调用 `ResolveEnergyEscrow` 前保存结果身份、Actor/World 弱引用及冷却参数，返回后重新检查 Runtime、Actor、ASC 和会话组件寿命；`SubmissionCommitted` 同步广播返回后也重新检查，不继续向失效对象交接或设置等待计时器。
- 会话组件新增 `CanAcceptConfirmedCooldown`，`StartConfirmedCooldown` 返回是否实际接受写入；Runtime 消费该结果，不能把已 EndPlay 但 UObject 仍有效的组件当作冷却接收方。新请求遇到此组件明确返回 `SessionOwnerUnavailable`。Reset 的属性写入及动态事件广播返回后也增加寿命检查。
- 无法交付时，Resolve 返回 false 并通过 `OutFailureReason` 传递 `SubmissionDeliveryUnavailable`；Action/ApplyEffect 两个 Behavior 入口消费该原因，不报告完整成功或 completed。`FinishUnavailableSubmission` 分别记录 `ActionSucceeded`、`ResourceResolved`、`CooldownRecorded`，清除提交锁、延期意图、四项 Timer 及 Cast/Behavior/Ability 身份；销毁链不再访问失效 ASC/Actor，仅清本地状态和仍有效的对象。不退款、重扣、重试或凭动作成功补造资源确认。
- 独立 `SkillLabUnconfirmedSubmissionTests.cpp` 现为 375 行，新增 `TestSettlementDestruction`，复用小 Game World 夹具和正式 Ability/Behavior/Effect 执行链。在真实 Energy 扣费属性委托中分别执行 Actor `Destroy()` 和会话组件 `EndPlay()`，委托自移除；断言真实效果一次、扣费通知一次、观察余额 98、无完整提交成功事件、明确交付异常、Cast/提交锁/等待及阶段计时器/Behavior/Ability 清理。组件 EndPlay 分支同时确认 UObject 仍有效、实际余额 98、冷却为 0、reservation 已闭合且新请求被拒绝。旧 `SkillLabCastGameplayEventTests.cpp` 仅更新必要的 EndPlay/Reset 返回值断言。
- 两条定向事实诊断必须区分：Actor 销毁为 `ActionSucceeded=1 ResourceResolved=0 CooldownRecorded=0`，当前 GAS 返回资源未闭合，观察到扣费通知不等于资源结算已确认；会话组件 EndPlay 为 `ActionSucceeded=1 ResourceResolved=1 CooldownRecorded=0`，已确认费用保留，但冷却未写入，不能报告完整成功。两者均不尝试补偿；本轮未改变已接受的 GAS 契约或 Lua/JSON 生产内容。
- 最终验证沿用已完成结果，不因补摘要重复构建或测试：隔离 UE 5.8 Editor Win64 Development 构建成功，进程退出 0；`SkillLab.GAS.EnergyLedger`、`SkillLab.Skills.CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`UnconfirmedSubmission` 五项均 Success，完成时间 UTC 2026-11-11 14:33:44–46。构建证据为 `E:/agent/game/ue/SkillLab/Saved/Logs/UnconfirmedSettlementLifetimeBuild.log` 第 110 行；自动化及两条定向诊断为同目录 `UnconfirmedSettlementLifetimeValidation.log`。本轮九份源码与 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation` 隔离副本哈希一致。
- 最终日志启动阶段仍有 13 条 `LogAutomationTest: Error: Condition failed`，并有 GameFeatureData 资产管理器配置错误；不能称整份日志无错误。生产权威回执与迟到收敛、权威成功时间及迟到冷却结转、多提交、全部合法资源写入保护、实际 Montage 停止、可见 PIE 和跨进程恢复仍未完成或未验证；本轮局部回归不能替代这些验收或第 8.1 节整体验收。原 Reviewer 已受托复查，交付后保持源码与测试冻结。

## 原 Reviewer 两项 P2 修复：冷却寿命与逐记录诊断（2026-11-11）

状态：**修复及最终回归完成，冻结待原 Reviewer 复查；第 8.1 节整体未完成。** 下节曾有子 Reviewer 通过结论，原 Reviewer 随后发现本节两项缺陷，因此不能将旧结论视作最终验收。

- 已确认冷却的唯一 `CooldownEnd` 从 Runtime 移至既有同 ASC 会话组件 `USkillLabUnconfirmedSubmissions`；该组件现在同时持有会话结算隔离和已确认冷却门控。Runtime 的查询、首次成功启动冷却、激活门控及 Reset 均委托它，不保留第二份冷却。截止时间仍是原 World 时间，绑定原 ASC 对象身份；Runtime 重建不迁移、归零或重启截止时间。无未决时的既有 Reset 仍可清除冷却，有未决时继续受阻。没有实现迟到成功冷却结转。
- 无法确认诊断从组件级布尔改为按已去重执行记录分别追踪；EndPlay/销毁先报告 A 后，Record(B) 会补报 B，重复交接和重复清理不再次报告 A/B，也不调用失效 ASC 结算。
- 必要测试集中于独立 `SkillLabUnconfirmedSubmissionTests.cpp`（317 行）：使用独立 Game World 并按墙钟经过时间推进正式 World 时钟/TimerManager。20 秒非零冷却的首次确认及后点未决，分别在等待期与异常终态后重建 Runtime，核对原截止时间、真实递减、不重启、无重复扣费/预留及同技能隔离；另一已确认的 25 秒冷却也保留正式门控。EndPlay、Actor 销毁和 ASC 销毁三条终止路径均精确要求不同身份 A/B 各一条无法确认日志，并重复 Record 验证幂等。本轮未回填大集成测试或改动 Lua/JSON 内容及 GAS 契约。
- 最终隔离 UE 5.8 Editor Win64 Development 构建成功；`SkillLab.Skills.UnconfirmedSubmission`、`CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation` 及 `SkillLab.GAS.EnergyLedger` 五项成功，UTC 2026-11-11 14:18:18–19。证据为 `E:/agent/game/ue/SkillLab/Saved/Logs/UnconfirmedOwnershipP2Build.log` 与 `UnconfirmedOwnershipP2Validation.log`；本轮五份源码与隔离工程副本哈希一致。新增独立世界夹具曾有初始化/已注销组件 Tick 的调用顺序错误，已修正，最终五项均通过。
- 未验证边界沿用本切片：生产权威回执与迟到收敛、实际 Montage 停止、跨进程恢复、多提交及全部资源写入保护仍未完成或未运行。启动期既有 AutomationTest `Condition failed` 和 GameFeatureData 配置错误仍在，五项目标测试各自报告成功，不称整份日志无错误。

## 同会话未决所有权、隔离及有限等待（2026-11-11）

状态：**本切片实现与五项回归完成，待 PM 安排原独立 Reviewer 验收；第 8.1 节整体未完成。**

本轮独立子 Reviewer 已复核最终代码、摘要、构建和五项回归日志及隔离副本哈希，未发现剩余阻塞；该结论不代替 PM 安排的原 Reviewer 验收。

- 依据需求第 8.1 节及最新三份交接，新增角色级 `USkillLabUnconfirmedSubmissions`。它与特定有效 ASC 关联，独立于 Runtime/Cast/Reset 持有未决身份；ASC 仍是能量预留的唯一所有者。记录包含 SkillId、Runtime 的 FObjectKey/诊断 ID、generation、Cast、point、transaction、已有计费事实，以及尚未连接生产来源的 ResultSourceId。查询入口是 `GetRecords`，不是权威回执或结算授权；没有接入任何成功/失败确认或调用未决成功 Resolve。
- 首次和已计费后未决均拒绝同技能新请求，原因 `SubmissionUnconfirmed`；后者不新增预留或费用。Runtime 在任何延迟 Reset/EndPlay 前交接记录，再启动 1 秒等待。真正到期沿现有 Cancelled 事件以 `SubmissionUnconfirmed` 原因异常结束 Cast/Ability，清理等待、阶段计时器和缓存；预留及隔离继续存活。其他技能在 Cast 结束后按 available 和既有正式门控运行，等待期间仍遵循现有单 Cast 的缓存规则。
- Reset 会取消当前 Cast、清缓存和等待；存在任一未决记录时，资源恢复与冷却重置均受阻，通过 Rejected/`ResetBlockedByUnconfirmedSubmission` 报告未完成，保留实际余额、预留及隔离。Runtime 销毁及重建不销毁角色级记录；ASC/Actor 或记录组件结束时记录本会话无法确认，不向失效 ASC 结算。销毁后新增记录也即时诊断；线程永久阻塞及进程退出无定时恢复或持久化保证。
- 新增 `SkillLabUnconfirmedSubmissions.h/.cpp`（记录/诊断）、`SkillLabSkillRuntimeUnconfirmed.cpp`（交接/等待接线）及独立 `SkillLabUnconfirmedSubmissionTests.cpp`（262 行小夹具）；Runtime 只接入初始化、请求隔离、Reset 受阻和失效 ASC 清理保护，不复制账本或生命周期状态。旧大集成测试仅将一个缓存意图换为其他技能，符合新增同技能拒绝规则。现有 Runtime 的其余生命周期、规则和效果代码未作无关重构。
- 验证：最终源码 UE 5.8 Editor Win64 Development 隔离构建成功；`SkillLab.Skills.UnconfirmedSubmission`、`CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation` 和 `SkillLab.GAS.EnergyLedger` 五项均成功，日志 UTC 2026-11-11 14:07。证据 `E:/agent/game/ue/SkillLab/Saved/Logs/UnconfirmedOwnershipValidation.log`；隔离工程为 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation`。测试使用实际墙钟等待并驱动正式 TimerManager，未直接调用到期回调或伪造回执。
- 实际覆盖：首次无回执预留、1 秒到期/异常终态/Ability 与计时器及缓存清理、同技能拒绝、其他技能成功及余额不足拒绝、等待期与终态后 Reset、Runtime 重建、ASC 先销毁且 Runtime 存活、终态后的 Runtime/ASC/Actor 销毁、已有费用后的未决隔离和无二次预留。组件 EndPlay/Actor 销毁后新增记录是直接所有者 API 边界测试，不称为真实生产者迟到回执端到端。曾修复夹具未 BeginPlay 便 EndPlay 导致的断言，最终回归已通过。
- 未完成/未运行：生产者权威记录与查询协议、成功/失败迟到收敛、权威成功时间和冷却结转、补报通知、多提交、全部合法资源写入保护、跨进程恢复、可见 PIE/实际 Montage 停止。现有取消事件可供表现层停止动作，但本轮自动化不构造动画实例，不声称可见动画验收。启动日志仍有既有 AutomationTest/资产管理配置错误；五项目标测试各自成功，不称整份日志无错误。以下旧阶段的限制只代表当时版本。

## Runtime 能量账本接入（2026-11-11）

状态：**局部接入经独立 Review，无剩余阻塞；第 8.1 节未完成。**

- Runtime 的请求、释放和首次提交门控读取 ASC 可用能量；已提交的 Cast 不再重复检查首次费用。`GetEnergyValue` 返回实际能量，新增 `GetReservedEnergyValue` 和 `GetAvailableEnergyValue` 供正式查询；现有 HUD 仍显示实际能量，未增加可见字段。
- 首次费用沿用 ASC 分配的唯一交易 ID，在 Begin 登记 Runtime generation 和 CastId；按实际结果 Resolve，不能收束时保留或标记未决。结算期间使用 Resolving 状态阻止属性回调重入同一 Resolve，取消、Reset 和 EndPlay 仍通过既有延迟终止路径处理。
- 更新真实 Runtime/Ability 集成用例：验证三个门控、实际/预留/可用值、失败释放和成功扣费、扣费回调重入、同步终止以及旁路未决。旁路场景使用独立 ASC，避免生命周期内 sticky 诊断污染后续场景；这些故障注入不证明所有资源写入已受保护。
- 最终代码隔离 UE 5.8 Editor Win64 Development 构建成功；`SkillLab.Skills.CastGameplayEvent` 和 `SkillLab.GAS.EnergyLedger` 均成功，日志 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/Saved/Logs/SkillLab.log`，测试完成时间 UTC 2026-11-10 19:01。未运行可见 PIE。
- 独立 Review 复核了代码、两项结果及隔离源码哈希，结论为本轮范围无剩余阻塞。日志启动阶段仍有 `AutomationTest Condition failed` 和 `GameFeatureData` 资产管理器配置错误；两项目标自动化各自报告成功，不能称整份日志无错误。
- Runtime 的提交、结算和终止仍共享 `FSubmissionContext`、Lifecycle、Behavior 和 `FinishSubmissionTermination`，本轮没有复制状态或仅按行数拆文件；集成测试仍共享全局定义替换、World 和 latent 恢复，旁路场景已隔离 ASC，但未提取完整 fixture。约 400 行是首轮上下文参考，并非已满足的文件长度目标。
- 生产迟到权威回执、Reset/销毁后的未决所有权、1 秒真实到期、跨 Cast 同技能隔离、多提交和所有合法资源写入保护均未完成或未验证；不能声称第 8.1 节完成。以下旧阶段中关于暂扣和测试通过的描述只代表当时版本。

## 交叉入口 Review 修复（2026-11-11）

状态：**已修复本轮两个 P1，待原 Reviewer 复查；第 8.1 节仍未完成。**

- `BeginBehaviorSubmission` 对已存在的未决提交单独返回 `SubmissionUnconfirmed`，供另一 Behavior 入口保留 pending 和原因；提交执行前收到终止时，接收 `ResolveBehaviorSubmission` 返回的未决标志，并在退款或点收束失败时向调用者传递该原因。没有修改 ASC 资源职责拆分。
- 真实 Lua 脚本分别按 Action→ApplyEffect 和 ApplyEffect→Action 顺序运行；首次效果回调直接写入使事务未决，脚本断言第二次请求仍 pending、原因未确认且不报告成功或完成；测试断言效果仅一次、脚本返回后 Cast 仍活动。另在扣费回调触发同步取消，在退款通知中注入直接 Energy 写入导致退款不完整；两个真实 Lua 入口均断言返回表保留未决，测试核对没有执行效果、Cast 清理及未决账目保留。这些是针对现有不受支持旁路的故障注入，不证明完整资源事务。
- 最终修改后隔离 UE 5.8 Editor Win64 Development 构建通过；`SkillLab.Skills.CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`SkillLab.GAS.DamageCalculation` 四项自动化均通过，证据为 `C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation/Saved/Logs/SkillLab.log`，日志 UTC 时间 2026-11-10 16:24:44。未运行可见 PIE。
- 1 秒真实到期与跨 Cast 同技能隔离仍未验证/实现；当前 escrow 实际扣 GAS Energy、HUD 显示扣后值，actual/reserved/available、其他技能可用余额、Reset/销毁所有权、资产/Blueprint/dynamic GE 审计、生产迟到权威回执及收敛均未完成。不得据本轮修复声称事务稳定或本批功能完成。

## 未决反馈 Review 修复（2026-11-11）

状态：**三项问题已作局部修复，Review 验收尚未全部完成；第 8.1 节仍未完成。**

- `ResolveBehaviorSubmission` 在延迟取消/Reset/EndPlay 清理之前向原调用者返回未决标志；Begin 失败也通过原因传递未决，Behavior 不再依赖清理后的 ActiveCastGeneration 查询结果。Action/Effect 的真实 Lua 脚本分别验证效果回调中原生写入后同步取消和 Reset，断言 `accepted/applied/pending/completed/reason` 及 `Finish`；错误返回触发 Lua 错误并使测试失败。`SkillLabSkill_ApplyEffect` 已在 RunScript 注册，真实效果事件仍保留。
- `CompleteBehavior` 及投射物结果回调拒绝推进未决 Cast。集成测试在存在真实缓存意图时注入迟到完成、Hit、Expired 和 Destroyed，断言 Cast 及缓存未被结束或消费；投射物结果以直接回调注入，未构造真实飞行/碰撞场景。
- Lua Action 的 `applied` 仅在提交被确认且实际效果已应用时为 true，与 Effect 入口保持一致；实际效果事件和数值不回滚。
- 隔离 UE 5.8 Editor 构建和四项 Skills/GAS 自动化在最后一次测试断言前通过，日志 `Saved/Logs/SkillLab-backup-2026.11.10-16.15.54.log`；最终改动后构建及 `SkillLab.Skills.CastGameplayEvent` 通过，日志 `Saved/Logs/SkillLab.log`。未运行可见 PIE。
- **Review 未完成项**：现有 1 秒计时器尚未做真实到期验证，且到期后跨 Cast 同技能隔离根本未实现；因此无法通过用户要求的计时器、施法锁、缓存及同技能隔离整体验收。未决账本仍是真实暂扣，Reset 和迟到结果收敛仍不满足 8.1。不要把本次局部测试或四项回归表述为 Review 已通过或事务稳定；需要 PM 和 Reviewer 根据上述事实安排后续处理。

## 未决反馈修正阶段（2026-11-11）

状态：**部分实现，不满足第 8.1 节验收**。本轮修改 `SkillLabSkillBehavior.cpp`、`SkillLabSkillRuntimeComponent.cpp/.h`、`SkillLabCastGameplayEventTests.cpp` 和本摘要，保留此前资源职责拆分及其他改动。

- `RequestDeclaredAction` 和 `RequestDeclaredEffect` 消费 `ResolveBehaviorSubmission` 结果；已知未决返回 false、保留 `SubmissionUnconfirmed` 原因、标记 pending 并阻止同一 Behavior 再次请求。Action 未决时不报告 completed；ApplyEffect 的 Lua 结果新增 pending。已执行的真实效果事实不因此伪造回滚。已排队的完成请求在未决时不再结束 Cast。
- 未决时试接 1 秒计时器，到期调用 `CancelActiveCast(SubmissionUnconfirmed)`，Cast 清理取消该计时器。此实现尚未实测计时器真正到期，也没有完成跨 Cast 的同技能隔离、结果路由或可靠预留；不能称为第 8.1 节有限等待验收通过。普通取消、Reset/EndPlay 的未决长期隔离也尚未实现。
- 定向自动化通过真实 Runtime/Ability 和 GAS 效果回调注入原生写入，检查两个 Behavior C++ 入口在未决时返回 false、pending 以及不允许重复，Action 不报告 completed。尚未通过实际 Lua 脚本断言返回表及脚本完成路径，也未验证等待到期和 Lua `applied` 语义，故不能称完整 Lua 端到端通过。
- 隔离 UE 5.8 Editor 构建通过；四项 `SkillLab.Skills`/`SkillLab.GAS.DamageCalculation` 自动化在最后一个局部改动前通过，最终修改后重新构建及 `CastGameplayEvent` 通过。最终日志为隔离项目 `Saved/Logs/SkillLab.log`。
- actual/reserved/available 账本候选未实现：现有 `BeginEnergyEscrow` 仍实际暂扣 GAS Energy，HUD 显示扣后值；Reset、长期预留和多个未决仍不满足需求。非 virtual 原生写入已有旁路复现，运行 Blueprint/资产及动态 GE 仍未完成审计；不能声称仅凭诊断满足合法写入边界。正式即时、范围和投射物生产者仍无携带事务身份及成功时间的迟到回执或查询，不能从命中推断结果。
- Runtime 的提交/等待仍共享生命周期、Behavior 和终止状态；Provider 的 Lua/JSON 交叉校验与本切片无直接改动；现有集成测试依赖定义替换、World 和 latent 清理。三者本轮未拆分，后续需要建立可独立验证的提交/等待、解析和共享 fixture 接口；不是把约 400 行参考视作已经满足。未完成生产者身份适配、可靠账本、同技能隔离、迟到收敛、Reset/销毁保障、并发和可见 PIE。

## 第 8.1 节核查及资源职责拆分（2026-11-10）

状态：**未完成，不能验收第 8.1 节**。策划语义已定案，以下是当前实现事实和未完成的技术工作，不再将语义当作待决产品问题。

- 本轮将 `USkillLabAbilitySystemComponent` 的资源写入、托管与未决账目方法从 `SkillLabAbilitySystemComponent.cpp` 移到 `SkillLabAbilitySystemResource.cpp`，状态仍由同一个 ASC 持有；没有增加第二份账本或改变运行行为。该拆分允许资源协调与伤害结算分别阅读、修改和编译验证。修改文件为上述两个 C++ 文件及本摘要，保留工作树其他变更。
- 工作树现状仍不满足预留契约：`BeginEnergyEscrow` 立即修改 GAS Energy，而 `GetEnergyValue` 和 HUD 将其作为实际余额显示；`IndeterminateEnergy` 只存金额和 Cast 身份，既没有 SkillId、独立可用余额、结果来源身份与收敛接口，也没有跨 Cast 的同技能门控。Reset 仍清空冷却并将 Energy 覆盖到最大值。不能把旧局部通过称为实际/预留/可用三者正确、Reset 隔离或完整事务。
- 结果来源核查：即时效果的 `SkillLabEffectExecutor::Apply` 调用 `SkillLabEffectResolver::Apply` 并同步返回 `bApplied`；范围动作同步汇总目标，投射物动作以 `FinishSpawning` 后返回为创建交付。现链没有携带提交事务身份的迟到结果回调、查询接口或可供结转的权威成功时间；命中与持续伤害事件不能冒充提交回执。无回执时应保持未确认，仍需实现身份记录及可存活路由，不能虚构结果。
- 发现进一步的错误反馈：`USkillLabLuaSkillBehavior::RequestDeclaredAction` 忽略 `ResolveBehaviorSubmission` 返回值并返回动作执行的 `bSucceeded`；效果回调绕过导致事务转为不确定时，Lua 仍可能收到 `accepted=true`。现有旁路自动化断言动作返回 true 只证明效果路径执行，不证明事务成功。该行为尚未修复，需与未知状态的等待、脚本完成及终态一起改造，避免只改布尔值后脚本错误地结束或重试。
- 正式 C++ 文本审计：伤害、治疗 GE 作用于 IncomingDamage/IncomingHealing；Runtime Reset 的能量恢复和 AttributeSet 正常夹紧使用 `RequestResourceChange`。原生 ASC 直接写入非 virtual，已有 100→50、退还后 52 的复现依然成立；Blueprint/二进制资产、任意动态 GE 和所有聚合路径尚未完成审计。未证明所有合法写入可被约束，也未证明独立账本与 GAS 映射的不变量；不得以旁路诊断或本轮拆分替代可靠预留。
- 拆分判断：Runtime 的提交与终止依赖 `BeginBehaviorSubmission`、`ResolveBehaviorSubmission`、`AbortBehaviorSubmission`、`FinishSubmissionTermination` 和 `FinishCast` 的共享生命周期，尚未抽出稳定的独立所有者；Provider 本轮没有涉及声明解析，暂不为行数移动其跨字段校验；大测试共享定义替换、World 和 latent 恢复，尚未完成最小 fixture，暂不复制它们。后续必须围绕资源账本、未决身份路由及 Cast 等待形成可独立验证的接口后继续拆分。
- 验证：隔离 UE 5.8 Editor Win64 Development 构建成功；`SkillLab.Skills.CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`SkillLab.GAS.DamageCalculation` 四项自动化通过，日志为隔离项目 `Saved/Logs/SkillLab.log`。这是对本轮职责移动的回归，不是第 8.1 节验收。尚未实现或验证有限等待、同技能隔离、其他技能使用可靠可用量、迟到权威结果结转、Reset/销毁隔离、可见 PIE 和并发运行。

## 提交收束阶段（2026-11-10）

状态：**部分实现，未验收，不是并发或多提交交付**。按最新交接暂停未决账目的玩家语义，等待总控决定可见余额、恢复/退款、冷却、重试、终态与通知。

- Runtime 增加提交上下文及 `Escrowed/Executing/Committed/Failed/Indeterminate/Closed` 状态，保存 Cast token、Runtime generation、提交点、Behavior 和 ASC 分配的不重复交易编号。结束行为的当前性检查不再是闭合已拥有事务的唯一条件。提交调用增加作用域兜底；未解析结果保留为不确定，释放活动协调区和资源队列，不广播提交成功、不启动冷却、不以失败名义退款。
- EndPlay 在同步提交回调中先封锁新效果、延迟 Cast/Behavior 清理，待调用返回后以真实结果收束；扣费回调中出现 EndPlay、Reset 或取消时，动作尚未执行，先返还并处理队列，再终止。正常失败退款不足会保留未决账目并返回失败；效果回调中的可检测直接旁路转为不确定，不发布提交成功。
- ASC 的未决账目暂存交易编号、原托管额、已返还额、Runtime generation 和 CastId，仅提供技术记录；目前没有账目恢复或持久化策略。当前 Reset 仍可能覆盖可见 Energy，且重复请求、后续重试和未决状态对角色的长期影响没有产品语义，不可把当前行为作为最终承诺。ASC/Actor 销毁后的账目持久性也未建立。
- 隔离 UE 5.8 Editor 构建成功；`CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`GAS.DamageCalculation` 四项通过，证据为隔离项目 `Saved/Logs/SkillLab-backup-2026.11.09-18.43.53.log`；最后一次局部修改后构建和 `CastGameplayEvent` 重跑通过，证据为同目录 `SkillLab.log`。新增正式 Ability/GAS 夹具覆盖扣费和效果回调中 EndPlay、Reset，未决账目与排队写入、直接旁路、上限夹紧导致退款不足。测试中直接调用终止和 Abort 以覆盖同步边界；没有验证真实线程永久卡死、进程崩溃或持久化恢复。
- 同一游戏线程的同步回调若永不返回，后续 EndPlay、Reset、定时器与作用域析构均不能获得执行机会；本实现不承诺该情形自动收束。原生 ASC/GE 旁路仍不可拦截，通知监听只能诊断部分变化，不能声称对全部旁路提供原子性。
- 仍需：待定账目的正式处置；真实按 Cast 分离运行上下文、Ability/窗口/计时器/清理和并发仲裁；多提交及异步效果所有权；Reset、死亡和外部 Ability 中断的所有组合；运行资产/GE 写入审计及可见 PIE。当前新增上下文仍附着在单 Cast Runtime 上，不应称为并发隔离。测试文件和 Runtime 仍超过约 400 行，现有测试共享全局定义、World 和 latent 恢复；尚未建立独立 fixture/职责接口，避免机械拆分，后续应按交接继续拆分。

## 资源写入补充核查（2026-11-10）

状态：**仍未完成，不是稳定事务接口**。窗口冲突语义和职责拆分暂停，等待架构交接更新。

- `ClampVitals` 不再在每次 GE 结算时无条件调用 `SetEnergy`；确需夹紧时，通过 SkillLab ASC 的受控资源入口提交。普通伤害和治疗不应再额外产生无必要的能量写入；直接 GE 修改能量仍不因此得到拦截或授权。
- 托管期间，受控写入只将相应属性的一次通知视为自身通知，之后的重入变化可被标记为绕过。该检测依赖实际通知及监听顺序，不能作为所有原生写入的拦截保证。
- 正式 ASC 属性通知测试在扣费回调直接将 Energy 从暂扣后的 98 覆盖为 50：检测到绕过并在执行效果之前拒绝托管，返还托管 2 后余额为 52。测试明确断言 52，不将此结果描述成恢复原余额或原生写入安全。隔离 UE 5.8 Editor 构建及 `CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation`、`GAS.DamageCalculation` 四项测试通过，日志位于隔离项目 `Saved/Logs/SkillLab.log`。
- 静态核查发现同步提交期间调用 `EndPlay` 会立即使生命周期终止并改变 Runtime 身份；随后 `ResolveBehaviorSubmission` 的 `IsBehaviorCurrent` 检查可能提前返回，未收束活动托管。需结合每 Cast 执行上下文处理执行中终止、已发生效果及资源结果；当前未对此路径作运行验证，也未宣称已修复。失败返还、聚合与所有可配置 GE 的约束仍需继续核实。

## 资源托管阶段（2026-11-10）

状态：**未完成，不是稳定接口**。本次依据更新交接试做角色 ASC 级托管，未完成并发执行所有权及所有收束和安全证明；不可将本段局部通过当作事务或批次验收。

- `USkillLabAbilitySystemComponent` 新增 `BeginEnergyEscrow`、`ResolveEnergyEscrow`、`RequestResourceChange`。首次动作执行前以 CastId 暂扣能量，校验当前值、base、最大值和扣费差额；受控 Energy/MaxEnergy 加法和覆盖在活动期间入队，返还/成功后按调用顺序执行。首次成功仍由 Runtime 记录费用和冷却，失败尝试返还；原 Reset 写入和测试中已有的资源写入已改走受控入口。当前 ASC 仅允许一个事务，竞争返回失败，后续点仍沿现有单 Cast 状态处理。
- 项目源码和 Config 文本审计：正式 Energy 写入包括 Runtime 原扣费、Reset，以及 AttributeSet 的 `SetEnergy`/`ClampVitals`；原有伤害、治疗 GE 作用于 meta 属性，未发现文本配置指定能量 GE。AttributeSet 内部 clamp 仍是 GAS 写入，尚未完整证明它在所有 GE/聚合组合下与托管协调兼容；二进制 Blueprint/资产和第三方写入未全面审计。
- 受控路径的自动化覆盖：效果回调观察已扣完整费用并排队 Energy 覆盖、MaxEnergy 覆盖、能量增加；扣费属性委托重入排队；失败返还后处理队列；竞争事务拒绝；效果回调直接调用原生 `SetNumericAttributeBase` 时设置不支持标志并记录诊断。隔离 Editor 构建与 CastGameplayEvent、CastLifecycle、DefinitionValidation、GAS.DamageCalculation 四项测试通过，日志 `Saved/Logs/TimingRulesEscrow-Build.log` 和 `TimingRulesEscrow-Final.log`。直接绕过测试只证明诊断，不证明拦截或一致性。
- **资源风险**：UE 原生写入口非 virtual，协调器不能阻止任意 C++/GE 绕过。扣费期间 `bInternalResourceWrite` 覆盖同步委托，因此直接写入若发生在受控 GAS 写的回调内，不一定能被变化监听单独识别；余额校验能发现部分偏差，不能证明全部写入未发生。失败返还若因绕过/聚合/上限变化无法完整恢复仅报错，Runtime 尚未建立可恢复的结果处理；EndPlay/异常早退时事务和排队写入的完全收束、MaxEnergy 的复杂聚合、动态 GE 和正式运行资产写入尚未验证。不得声称对不受控路径提供原子性或完整退款。
- **仍未实现**：并发活跃 Cast、按 Cast 独立 Ability/行为/窗口/提交/清理、同技能实例化、表现 Notify 归属、跨 Cast 共用资源竞争及取消/Reset 的全路径处理；`bSubmitting`/`bCommitted` 仍为 Runtime 单份状态。普通状态窗口冲突仍未裁定。多提交内容与异步所有权仍未接入。
- 后续应先处理受控资源写入边界、聚合约束及失败/析构收束，并把托管事务绑定到真正的执行上下文；在此之前不应基于该 API 交付多提交。新托管实验尚未更新下文历史描述，以下先前证据对应其当时版本。

## 本次继续实现（2026-11-10）

- 新增已注册 Ability.SkillLab 分类 Tag，Lua 可通过 `categoryTag` 指定；共享策略 `abilityCategories` 按精确 Tag 声明已有三种请求结果，未声明的已注册类别返回 `UnknownAbilityCategory`，未注册或非 Ability 分类加载失败。旧 Skill/Dodge 字段及旧正式接口保持兼容；缓存保留类别身份。
- 共享策略可显式用 `interruptBy` 覆盖当前技能的一般状态打断集合，空数组表示不因一般状态打断；省略则继承技能声明。Runtime 订阅基础、Recovery 和各窗口策略所引用的状态，在技能开始、窗口变化、阶段变化和状态数量变化时重新求值；ASC 保持状态计数权威，死亡/沉默/硬直仍强制打断。新增 `State.SkillLab.Marked` 仅作为自动化语义夹具，没有生产授予者。
- 隔离 Editor 构建成功；CastGameplayEvent、CastLifecycle、DefinitionValidation、DamageCalculation 四项自动化成功。新增测试覆盖已存在状态进入窗口、两来源计数、最后来源移除、新增状态、额外类别匹配及未知类别。证据：`Saved/Logs/TimingRulesState-Build.log`、`TimingRulesState-Final.log`。
- **提交一致性实测缺陷**：隔离副本的临时测试在正式即时治疗 `EffectApplied` 回调中把能量设为 1，技能费用为 2。效果已经应用，随后 Runtime 仍广播 `SubmissionCommitted` 并启动冷却；AttributeSet 将能量夹紧到 0，实际只扣掉 1。`Saved/Logs/TimingRulesResourceRace.log` 含 `RESOURCE_RACE` 记录；该临时测试用于复现缺陷，不在工作区自动化套件中宣称通过事务验收。单纯延迟 Runtime 广播不能覆盖 GAS 同步属性回调或外部 ASC 写入。
- **仍未完成**：并发活跃 Cast 的上下文、Ability 实例/动画路由、跨 Cast 清理与共享资源仲裁均未实现；Allow 不存在，不应接入多提交内容。资源事务需要统一能量写入/预留契约，或针对任意外部同步写入定义可执行的冲突处理；在这种契约确定并实现前，不能承诺首次成功完整计费。旧的 `bSubmitting`、`bCommitted` 仍是组件级单份字段。新的本次状态/类别改动也仍运行在单 Cast 模型中。
- 当前窗口冲突的 Reject 使一般状态求值无法选择规则时仅记录原因，未自行设定一般状态打断优先级；死亡和已确认控制状态仍直接处理。多个并发 Cast 的规则冲突仍待实现。

状态：已有时序、窗口、部分可配置仲裁及一次计费接入，局部自动化通过；本任务尚未覆盖所有仲裁表达，需 PM 处理下述结构边界，不应标记为完整交付。以下正文替代原阶段摘要。

## 输入与修改范围

读取了项目入口、ProgramArchitectureContract、SkillSystem 基线入口、SkillCombatRulesRequirements、TimingRulesHandoff、更新后的 SubmissionEffectsHandoff、VerificationHandoff 和项目测试入口。未修改新批次以外的程序文档。

修改覆盖 SkillLabCastLifecycle、SkillLabSkillTypes、DefinitionProvider、RuntimeComponent、LuaSkillBehavior、GASTags、SkillAnimationComponent、PlayerInputComponent、四项技能 Lua/JSON 和三项 Skills 自动化。C++ 承担状态、身份、规则解释及提交事务；Lua 选择内容分类和共享规则；现有 JSON 目录承担共享规则数据、事件关系及数值；没有修改 UE 二进制资产。

## 内容入口与兼容

- SkillDefinitions.json 的根 combatRules 数组定义共享策略。每项必须明确 id、skill、dodge、movement、userCancel、normalHit、heavyHit、unknownHit，不靠字段顺序决定结果。生产的 Standard/StandardRecovery 保留已确认策略，四项 Lua 都显式引用它们。
- Lua 的 combatRules/recoveryRules 必须成对引用已知策略；category 可声明 Skill 或 Dodge，正式 SubmitSkillRequest 自动读取分类。旧文本未声明策略时保留原已确认兼容规则，不把它当成新增内容的隐含生产决策。
- 技能请求可选 Buffer、Reject、CancelAndActivate；非技能输入可选 Continue、Reject、Interrupt；已结算受击事实只能选 Continue 或 Interrupt，不能拒绝或撤销伤害。CancelAndActivate 明确先取消旧 Cast 再走正式门控；没有并行激活含义。Recovery 不能借配置绕过已确认的禁止激活规则。
- 未完成：要求中的允许新请求同时让当前行为继续，目前既没有该独立结果也没有多个活跃 Cast 的所有权；Runtime 的 ActiveCastId、ActiveAbility、ActiveBehavior、ActiveDefinition 和生命周期均为单实例。不能把该结果伪装为 Buffer 或 CancelAndActivate；应由 PM/架构明确允许与执行时机以及共享运行状态如何承载。当前类别仅支持 Skill/Dodge，状态打断列表也不按窗口选择；这些均不能宣称满足任意 Ability 类别和时机状态组合。
- windup/recovery 数值可省略，默认零。Timer 且零 windup 支持激活即提交。GameplayEvent 保留现有 Notify 路径。
- Lua 可声明 windupEnd=Timer，表达准备先独立结束、随后等待释放事件；需 GameplayEvent 且 windup 小于 releaseEventTimeout。独立准备未结束的释放通知不提交；准备结束发 WindupCompleted 事实，不自动命中或扣费。未选此项时保留原释放驱动准备边界。
- JSON 可声明 completionEventTag；需已注册、与释放不同的 Tag 及正 active 超时。行为可在已提交动作后等待完成事件，再结束/进入收尾。现有四项技能不改变为此模式。内部阶段仍保留 Windup/Active/Recovery 记账，但内容不必配置固定三段，阶段名称不裁定移动等策略。
- JSON 的 ruleWindows 为每项声明 id、beginEvent、endEvent、rules。Tag 必须注册、开关不同、事件不重复，且不能与释放/完成事件混用；策略必须存在。多个窗口必须在 Lua 显式声明 windowConflict=Reject，缺少声明则拒绝加载。当前没有推断优先级或自动选一个窗口的模式；生产四技能未新增窗口例外。
- blockedBy 保持原激活/释放校验；新增 interruptBy 独立声明状态变化时的打断订阅。省略时继承 blockedBy，显式空数组不继承。死亡、沉默、硬直仍遵守已确认的门控和打断规则。没有新增控制状态生产授予方。

示例窗口声明结构：

```json
"ruleWindows": [{
  "id": "Followup",
  "beginEvent": "SkillLab.Event.Window.Open",
  "endEvent": "SkillLab.Event.Window.Close",
  "rules": "Standard"
}]
```

## 身份、窗口及观察

- FSkillLabCastToken 在一次 Cast 内跨阶段稳定；Begin 时产生新 generation，即使复用 CastId 也不能继承旧身份。FSkillLabCastPhaseToken 仍随阶段变化，旧阶段计时器不能推进新阶段。
- 生命周期 OpenWindow/CloseWindow/IsWindowOpen 接收稳定 Cast token；Runtime 使用定义数组下标加一作为本 Cast 内窗口数值身份，对外可按内容 FName 查询。窗口跨阶段保留、终态清空；Runtime 对每项开/关事件各消费一次，重复 begin 不能在关闭后重开。
- 窗口事件经带 Runtime/Ability/Cast 身份的 ASC GameplayEvent 和当前 Ability 转发。NotifyState end 在验证原 Montage 实例后，将已声明 begin Tag 映射到 end Tag；未声明映射的状态结束不会伪造第二次释放。
- 重叠窗口按显式 Reject 策略报告 RuleWindowConflict；受击事实不回滚。普通状态与控制状态仍由 ASC 持有，不用窗口清除其他来源的状态。
- MakeResult 暴露 bHasReleased/bCostCommitted，成功提交另发 SubmissionCommitted，包含提交点数值身份；窗口变化、匹配策略和原因有日志。前摇结束、释放、提交和真实效果可以分别观察。
- 移动输入在发出 SimpleMoveToLocation 前调用 RequestNonSkillOperation。拒绝的是新输入，不代表停止此前已启动的移动；Ground Escape 仍仅取消选点预览。主动取消能力由同一接口解释，本次未新增按键。

## 共享提交与计费接口

- Lifecycle 的 BeginSubmission(CastToken, PointId) 预留非零提交点；ResolveSubmission(..., false) 释放预留供重试，true 才标为成功。已成功点不可重提，pending 点不可被旧 TryMarkSubmissionPoint 绕过。
- Runtime 的 BeginBehaviorSubmission(Behavior, CastId, PointId, Reason) 核对当前行为/身份、首次费用可用性并串行预留；ResolveBehaviorSubmission(..., Succeeded) 确认结果。脚本没有直接决定成功的接口，正式行为/执行器负责报告。
- 首次成功才扣能量并启动一次冷却；随后成功点不再扣费或重启冷却；失败不消费该机会。释放进入 Active 本身不扣费。新增 SubmissionCommitted 在确认后广播。
- 事务保持到扣费和广播完成；同步重入不能再提交。提交中发生的取消暂存首个终止事件，成功与计费确认后再终止；Reset 延后执行原有完整重置。提交后的独立效果不因取消伪造回滚。
- 即时同步执行器会先广播真实效果，再返回成功供事务扣费和广播 SubmissionCommitted；订阅者在效果回调中观察到的仍是扣费前值。事务阻止重入并延迟终止，但“效果事实与计费事实的广播顺序”需在集成 Review 时明确，不应声称两者同步原子对外可见。
- 兼容 Lua 单动作当前使用数值点 1：投射物生成成功属于异步动作提交，不以随后是否命中决定扣费；空范围检测也是成功执行的范围动作；即时效果以真实执行器接受为准。生成失败或效果执行失败不扣费。异步回调消费已成功提交，不再计费。

后续多提交任务必须继续完成内容点声明、事件到点映射、动作/目标/效果身份及异步所有权。当前 ApplyBehaviorEffect 仍按目标保留单动作兼容去重；不能把本任务的共享事务单测解释为“两个生产提交点已能命中同一目标”。后续需将效果去重升级为点和目标联合身份，并复用本次 Begin/Resolve 计费边界。提交事务为同步边界，不能跨 Tick 持有；异步动作在创建时确认提交，后续命中另处理。

## 验证与证据

当前编辑器占用原项目 DLL，直接构建曾得到 LNK1104。没有关闭编辑器；将 Source/Config/Content 和 uproject 复制至 C:/Users/Administrator/AppData/Local/Temp/SkillLabTimingRulesValidation，独立构建相同源码。必须使用完整路径启动 UE，Windows 短路径会导致内容挂载失败。

隔离 Editor Win64 Development 构建成功。运行：

```text
UnrealEditor-Cmd.exe <隔离项目>/SkillLab.uproject -unattended -nop4 -NullRHI -nosplash -nosound -ExecCmds="Automation RunTests SkillLab.Skills+SkillLab.GAS.DamageCalculation" -TestExit="Automation Test Queue Empty" -log=TimingRulesCompletion-Automation.log
```

CastLifecycle、CastGameplayEvent、DefinitionValidation、DamageCalculation 四项均 Success、进程退出码 0。覆盖窗口跨阶段/重复/终止/CastId 重用、正式 Ability 窗口开关和旧 end 隔离、显式冲突及无效配置、两技能共享与另一技能差异策略、修改共享策略的影响、即时/独立准备/事件收尾、失败重试、首点计费、后续点不重复计费、同步重入及同步取消，并回归原缓存、闪避、受击和控制规则。后续点计费测试直接调用共享事务 API，不代表多点内容端到端验收。

证据保存至原项目 Saved/Logs/TimingRulesCompletion-Build.log 和 TimingRulesCompletion-Automation.log。日志中的其他平台 SDK、插件启动自测和 MCP 端口占用噪声不代替上述项目测试结果。

生产四技能 NullRHI RuntimeProbe 已以 COMPLETE 结束，真实提交及效果事件见同目录 TimingRulesCompletion-RuntimeProbe.log；Fireball、AreaBurst、Mend、BurningBuff 均完成，提交前取消未扣费，Reset 后能量为 100。没有本次可见 PIE、真实鼠标移动或动画 NotifyState 窗口资产验收；Notify 超时真实到期、多来源控制 GE、独立持续效果来源及多提交生产内容由后续集成验证覆盖，不能把本任务局部通过写成这些场景已通过。

Review 后需同步的最小项目事实：SkillsRuntime 的规则/窗口与事务入口、GAS 的新增事件 Tag 语义、角色表现的 NotifyState end 转发、玩家输入移动门控和内容 Provider 新字段。本任务遵守授权，不修改新批次以外的既有程序文档。
