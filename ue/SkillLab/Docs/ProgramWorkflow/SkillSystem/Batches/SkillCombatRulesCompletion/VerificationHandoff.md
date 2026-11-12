# 资源写入审计、修复与增量集成验证交接

## 当前范围和证据

依据[需求](../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md)第 7–9 节，消费 [A 执行所有权](TimingRulesHandoff.md)与 [B1/B2 声明及效果](SubmissionEffectsHandoff.md)的实际产物。任务分为 **C1 独立审计、R 按发现修复合法写入、C2 剩余验证与集成**；不按文档数分 Agent，也不要求三者串行完成。

已接受前置以 PM 转交原 Reviewer 结论为准：生产记录、GAS 独立资源历史、会话迟到消费、唯一费用/冷却、隔离解除与原身份补报已通过，P2-1/P2-2 关闭。[摘要](TimingRulesImplementationSummary.md)顶部记录十四项目标测试及 194 份 Source 哈希一致、启动错误和未做 PIE 的限制。旧摘要里的暂扣/生产来源缺失、100→50→52 是当时版本证据，不能当作当前账本或正式回归期望；当前 Begin 只预留不扣实际。

本轮是源码静态核查和交接更新，没有新构建/游戏运行结果。后续证据需记录实际源码/内容版本、命令、退出码、各项目标结果和未运行项；不可将历史通过冒称修改后通过。

## C1：已派发的独立只读审计

PM 已并行派发对话 `01a1cb61-3863-7280-b6d9-0612ca38f548`。该任务只读生产 Source/配置/资产，**独占本目录 ResourceWriteAudit.md 与自己的临时验证目录**；不修改三份交接、生产源码/内容/资产，也不使用共享 `SkillLabTimingRulesValidation` 构建目录。本交接仅规定消费边界，不重派或覆盖其报告。

审计不用等待 A/B。先核对 C++、初始化、Reset、AttributeSet、GE/Ability cost、二进制资产及动态配置入口的实际写入/加载路径，再按可执行范围验证。已有发现即可交 R，不必等所有资产结束。后续 A/B/R 合入后只增量复核受影响路径和引用的资产/GE，未受影响证据保留，不能从头重做全部审计。

首轮路径相对 Source/，分轮约 400 行内：

- SkillLabGAS/Private/SkillLabAbilitySystemResource.cpp 的 OnResourceChanged、Request/ApplyResourceChange、DrainPendingResourceChanges（约 150 行）；Public/SkillLabAbilitySystemComponent.h 的 reservation/历史/队列字段；CombatAttributeSet.cpp 的构造/PreAttributeChange/ClampVitals。沿结算疑点才补 ResolveEnergyReservation/查询，不加载 Skills 模块。
- AbilitySystemComponent.cpp 的 InitializeSkillLabAbilityActorInfo、ApplyMetaEffect、ApplyDamageOverTimeToTarget；CombatGameplayEffects.cpp 和 EffectResolver.Apply。它们能动态构造 GE，不能只搜索预制资产名。
- Skills Runtime 的 ResetSkills 是窄调用点；生产调用者通过 rg 定位 Set/InitEnergy、MaxEnergy、SetNumericAttributeBase、ApplyModToAttribute、RequestResourceChange、GE spec/成本应用。AttributeData、Blueprint 图及外部加载也必须列出，文本无命中不能视为资产安全。

报告每条路径的来源/入口、正式或仅故障注入、是否加载可达、协调或生效前拒绝方式、实测及未测。初始化默认 100/100、Reset 的受阻/恢复、ClampVitals fallback、资源重入队列与 GE 聚合分别证明，不能把诊断等同保护。

### 二进制资产的真实证明入口

至少覆盖项目角色/GameMode/地图默认对象、所授予 Ability 类、引用的 GameplayEffect、成本/执行计算和动态加载闭包；从 `/Game/SkillLab/BP_SkillLabCharacter`、`BP_SkillLabGameMode`、默认 NavTest/Test_Box 地图和运行时授予入口追踪，不按目录名忽略其他项目引用资产。

用 UE AssetRegistry 枚举并实际加载相关类/CDO，检查 GE 的 Modifiers、Executions、Duration/Period、Ability Cost/Cooldown 和 Blueprint 图中的属性写入/GE 应用，再沿其引用扩展。仅 AssetRegistry 标签/字符串导出也不能证明图和动态计算无写入。加载失败、缺插件、不可解析的动态类逐项记未知及影响。

本机已核实存在 UE 5.8 Build.bat 与 UnrealEditor-Cmd.exe；可在审计者自己的隔离副本编译/运行临时诊断，必须区分“生产源码相同”与“额外诊断代码”，记录其差异/哈希。当前没有项目 Energy 资产审计 commandlet，也未确认可用 Python 插件；不可虚写一个已有审计命令。若需小型只读 Editor automation/commandlet，可仅在自己的临时验证目录实现并运行，不改生产项目，不预建通用平台。临时工具只能证明已枚举/加载范围，不能宣称任意原生旁路都被拦截。

现有 SkillLabAnimationContent commandlet 只读回 Montage/Notify，不能审计 GE/Blueprint 能量写入。终端不可加载具体资产时，报告该项失败原因及复现命令；C++ 审计/其余自动化仍继续，不笼统判为“没有 MCP 所以都不能做”。本任务未要求使用 UE MCP。

## R：消费审计发现的合法写入修复

这是代码实现职责，不能交给只读 C1。目标是将发现的正式 Energy/MaxEnergy 写入纳入现有协调器，或在加载/应用前明确拒绝不合法配置，并验证实际/预留/可用不变量。不重做已接受结果协议，不把“旁路不支持”作为排除合法内容的理由。

R 独占 SkillLabGAS 的 AbilitySystemComponent.h/.cpp、AbilitySystemResource.cpp、CombatAttributeSet.h/.cpp 及必要 EffectResolver/CombatGameplayEffects 接入；可新增独立资源写入测试。A/B1/B2不并发改这些文件。保持 Skills 调用 GAS，不新增 GAS 对 Skills 的依赖；资源查询仍按原交易/operation 返回事实，不由动作 bool 代替。具体资产/生产配置调整交内容职责，R 只落实共享加载/应用校验和协调能力。

入口仍为 RequestResourceChange（Energy/MaxEnergy、Additive/Override），保持 Begin reservation、唯一 Resolve 和 QueryEnergySettlement 历史；当前 API 名以源码为准。需要扩大支持的 GE 类型时，必须在权威生效入口验证实际 spec/计算及受影响属性，不以非 virtual 函数隐藏或 PreAttributeChange 宣称全面拦截。能量回复、扣减、覆盖、MaxEnergy 降低和 active GE 聚合不能在预留期间破坏保护金额；无法接受的合法请求应在生效前可观察拒绝，不能先写坏再只报诊断。

初始化、Reset、费用、ClampVitals 和重入 queue 分别接线。ResetSkills 归 A 修改，R 提供协调接口/结果要求，由 A 保留未决时 ResetBlockedByUnconfirmedSubmission，不同人同时编辑 Runtime。B2若需新的 Resolver 参数，由 R唯一落公共声明，B2消费；纯 Skills 内提交上下文结构不进入 GAS。

### Reset 跨 GAS/Runtime 的受理与完成合同

**本次局部核对事实（2026-11-12）**：当前 CompleteReset 在取消/清理、generation 更新和 Health 写入后调用 RequestResourceChange，忽略返回值即清 bResetting。R 已在当前代码提供 RequestResourceChangeDetailed 的 Rejected/Queued/Applied；ResourceWriteDepth 内请求进入原 PendingResourceChanges，DrainPendingResourceChanges 重新校验后仍可能拒绝，CanWriteResources 失败时会丢弃队列。现有 ResourceWriteIntegration 的队列用例及 R 摘要支持“受理不等于完成”，**Reset 端到端尚未复现/验证，不写成已测失败**。本段仅补结果接口，不重审已接受 B1/合并执行或覆盖 R 正在进行的修改；当前所有者以 README 为准，原 A 为合并执行负责人。

**复用判断**：复用现有受控请求、FPendingResourceChange、重新校验/应用算法和队列 drain，给需要完成结果的这一个调用方增加原生回执。属性变化委托可能在写入校验前同步触发，无变化写入还可能无通知，不能作完成依据；QueryEnergySettlement/BindEnergyOperation 针对费用 reservation/transaction，Reset 没有这种交易，不能借它伪造费用、结算历史或第二份账本。现有队列无调用方身份/完成回调，因此仅改 Runtime 检查 Detailed 返回值仍不够。

**R 提供的最小接口**：在 RequestResourceChangeDetailed 上增加原生受跟踪重载或等价窄入口，沿用 Energy/MaxEnergy、Operation、Value 参数，附加调用方预先建立的不透明 AttemptId、弱所有者、执行前许可检查及一次完成回调。建议语义为 `BeforeApply() -> FName`（None 允许，其他为拒绝原因）和 `OnFinished(receipt)`；类型/字段名由 R 落地，GAS 不引用 Skills 类型或 Reset 业务。Runtime 自己建立 ID 并在调用前登记，避免为取 GAS 生成 ID 再开两阶段注册或发生同步回调时尚未绑定身份。现有不跟踪调用/兼容 bool 不改变受理语义。

| 层次 | 结果及保证 |
| --- | --- |
| 受理返回 | 继续 Rejected / Queued / Applied；Queued 只说明进入队列，不释放 Reset 屏障，不广播完成。受跟踪调用的最终处理统一走回执，不能由返回值与回调各结束一次 |
| 完成回执 | 含原 AttemptId、原 ASC 身份、Applied / Rejected / Failed / Unavailable（或等价互斥终态）、Reason；可附经验证的目标值/实际值快照。Applied 只证明这一次资源写入在其校验完成点已应用；Rejected 是未执行便拒绝；Failed 是已尝试但无法证明完成，不能宣称无副作用；Unavailable 表示会话/所有者不可继续。无 Queued 终态 |
| 回调时机 | 立即拒绝/应用也允许在请求返回前回调；队列项在最终重新校验及应用后回调一次。队列清弃、会话结束和执行前许可失败也必须闭合该项，不能无声删除使存活 Runtime 永久 Resetting；对象已失效则只保留可诊断终态，不回调失效对象 |

每个受跟踪队列项只存这一份请求上下文，不新增通用任务系统、轮询查询表或持久化结果历史。BeforeApply 由 A 以弱捕获检查 Runtime/Actor、原 ASC、AttemptId、Reset generation、当前 pending 状态及未决隔离；仅校验，不在其中重置或写属性。GAS 在 dequeue 后、实际写入前执行许可，并再次校验自身会话/资源条件。弱所有者失效或许可拒绝时该旧请求不得写入；完成端忽略旧结果本身不能防止旧恢复覆写新 Runtime/新请求。

R 须在回调前将该项置终态并移出待执行容器，回调可同步重入资源请求/Reset/EndPlay。核对当前 ApplyResourceChange 内部调用 drain 的递归路径，保证当前项先完成自身校验/终态、再安全派发与继续 drain，不让嵌套 drain 重执行同项、扰乱 expected-notification 状态或丢失余项。完成回调应在该次写入保护退出的安全点执行；回调前后重新核对对象/会话有效性，不持有可能被回调失效的数组/映射引用。后续合法排队写入可继续改变余额，回执不是“余额从此恒定”的承诺，也不以队列最终余额倒推某项成功。

ASC EndPlay/会话结束时先禁止新写入，再终结未执行项为 Unavailable；若组件销毁可绕过 EndPlay，也在实际销毁路径闭合。正在执行项只由其收束点终结一次；EndPlay 不能再向失效 ASC 读属性/写入/重 drain。存活调用方可收到原身份失败；接收方已失效不强行通知。线程永久不返回或进程退出不承诺回调，本段不增加定时重试或跨进程保证。

**A 的 Runtime 消费**：ResetSkills 在可重入的取消/清理前登记一次 ResetAttempt；CompleteReset 只推进这一次清理与资源请求，不因回调/FinishSubmissionTermination 重复取消、递增 generation、清冷却或再次入队。原 Runtime 身份、原 ASC、该次 generation 和 AttemptId 构成匹配条件；状态分清清理中、等待资源结果、已终结，bResetting 保持到本次结果终结。重复 Reset 保持现有进行中不重复受理语义，不排第二笔恢复。请求/回调都可能同步返回，先登记 pending，再调用 GAS，返回后不得覆盖已被回调终结的状态。

该次清理推进 generation 时，在任何后续效果/Health 广播前将尝试绑定到推进后的 generation，不能用 Reset 自己刚失效的旧 generation 校验其恢复；后续 EndPlay/替换或其他失效再令此 token 失效。完成处理先锁定本次终态再做最终通知，通知回调中发生新尝试也不能被旧栈尾清空状态。

进入资源请求前及真正执行前再次核对未决：有未确认提交仍按既定 ResetBlockedByUnconfirmedSubmission 结束本次 Reset 为受阻，保留 reservation、SkillId 隔离和冷却，不请求恢复、不凭 Reset 释放/重扣。取消/Health/效果清理的同步回调也可能新产生未决或 EndPlay，不能仅用 CompleteReset 开头的 bResetBlocked 快照。Applied 回调还须核对同一尝试及有效会话；只有本次清理和资源结果确实完成，才允许清除已确认冷却等最终成功步骤并标记 Reset 完成。新未决不得因成功收尾被清掉。

无未决但写入立即拒绝、排队后拒绝、尝试失败或会话失效，必须结束这次有效 Runtime 的 Reset 屏障并报告未完成，沿用 Rejected/Reason 通道，例如 ResetResourceChangeRejected/Failed/Unavailable；Queued 只可报告进行中，Applied 后才记录 ResetCompleted。日志/结果均关联 AttemptId 与 generation，成功可沿现有日志/窄原生观察方式记录，**不把 Cast 的 Completed/SubmissionResolved 事件冒充 Reset 成功**，不要求新增 UI/产品事件体系。原有 Health/效果清理不因此获得事务原子性；若已部分执行要保留真实事实，不伪造回滚、重启 Cast、补偿或自动重试资源恢复。

A 在 EndPlay 入口立即使 pending attempt 无效，随后旧队列项由许可检查阻止执行；完成回调使用弱捕获，不能持有 Runtime/Actor 强引用。ASC 仍活而 Runtime 已销毁/替换时，旧请求不得借新 Runtime 继续恢复。ASC 先结束但 Runtime 尚活时，Unavailable 回执解除其有效等待并报告；原已结束 Runtime 不再广播/改新 generation。超时不能被用来猜 Applied，不能用属性委托、HasPendingResourceChanges 或 Tick 轮询判定 Reset 完成。

**允许范围和就绪条件**：R（原 GAS 对话）独占 AbilitySystemComponent.h、AbilitySystemResource.cpp、实际 ASC EndPlay/销毁落点（当前 SkillLabEnergySettlement.cpp）及其必要资源回执类型/独立 GAS 测试；保留现有 reservation/历史算法及其他 R 修复。A（原合并执行对话）独占 RuntimeComponent.h/.cpp 的 ResetSkills/CompleteReset/EndPlay、必要 FinishSubmissionTermination 衔接与独立 Reset 集成测试；不让 R 同改 Runtime，不改生产 Lua/JSON/资产、需求或已接受功能。先由 R 落完整原生声明、执行许可和最终回执（包括清队/结束路径）并给出真实 GAS 用例证据，A 消费同一接口接线；A 可先按本合同准备自己的尝试状态/用例，最终交叉编译与端到端验证必须等接口实际就绪。无需再为每个函数回架构补方案，由 PM 各向原所有者派一次完整局部实现。

首轮上下文不超过约 400 行：R 读取当前 Resource.cpp 的 RequestResourceChangeDetailed/Apply/Drain、头中 FPendingResourceChange/资源字段、CanWriteResources 与 ASC EndPlay；A 读取 Runtime 的 ResetSkills/CompleteReset/EndPlay、Reset 字段、FinishSubmissionTermination 的 Reset 分支及 R 实际回执声明。只有同步委托/销毁直接依赖才扩读，不加载长历史、整份摘要、全部执行/Provider 或整套测试；现有文件已按职责拆分，本次不为行数再重构。

**真实验证分工**：R 在现有 ResourceWriteIntegration 或独立 GAS 测试验证立即 Applied/Rejected、Queued→Applied、Queued→Rejected、执行后失败、会话结束/接收者失效、回调重入及每项一次回执；拒绝/失效项不写入，后续队列不丢失。原 A 新增独立 `SkillLab.Skills.ResetResourceCompletion`（拟名，须实际注册），从正式 ResetSkills 调用，不直接 CompleteReset 或伪造最终回执：

- 无未决的正常立即应用与真实合法拒绝：恢复完成/未完成可观察，bResetting 正确终结；失败不清成“全部成功”、不补偿已发生的 Health/效果清理。
- 在真实受控属性写入委托里调用 Reset，使能量恢复实际进入队列；断言回调内仍 pending，drain 真正应用后才完成。再用真实 GE 聚合/其他合法条件变化造成 dequeue 时拒绝，断言正式失败及无伪报成功；若生产边界已阻止该组合，提交实际执行顺序和拒绝依据，不能捏造端到端失败。
- queued 等待、Health/资源/完成回调内重复 Reset、请求新技能、取消、EndPlay 或销毁 Runtime/ASC/Actor：只入队/写入/终结一次；有效失败解除屏障，失效旧回调不访问对象。旧 Runtime 销毁、替换 Runtime 新 generation/新 Cast 后，旧恢复既不执行也不影响新执行/余额。
- 首次未知及已计费后未知、取消收束期间新产生未决时，继续 ResetBlockedByUnconfirmedSubmission，跨 Reset 保留唯一预留、隔离和冷却；后续真实迟到确认仍只结转/通知一次。保留 EnergyLedger/EnergySettlementHistory 与受影响既有 Reset/重入测试，不重复无关已接受用例。

本段只给实现/验证合同，没有宣称这些场景已通过；失败若来自真实调用顺序，交相应代码所有者修复，无新的待产品定案项。

### GE execution 的局部兼容边界

**核对结论**：当前 SkillLabResourceEffects.cpp::CanApplyResourceEffect 对 Executions 非空一律拒绝，范围超过 Energy/MaxEnergy，不能仅凭“保守”作为本次资源需求的最终全局边界。C1 报告与现有代码显示：生产即时伤害/治疗使用 IncomingDamage/IncomingHealing modifier，持续伤害动态 GE 也无 execution；149 资产快照未发现独立 GE 资产或项目可配置任意 execution 类的技能入口。因此未证明已有生产 execution 回归，但原伤害测试通过也不能证明拒绝任意非资源 execution 合理。需求 8.1 要保护预留，不授权关闭无关非资源 GE 的引擎正式应用能力；不把此限制交内容负责人猜测确认。

**具体技术依据和最小修改**：UE 5.8 GameplayEffect.cpp 的 ExecuteActiveEffectsFrom（以及同文件另一 execution 执行分支）先调用 ExecCDO->Execute 得到 FGameplayEffectCustomExecutionOutput，再将每个输出交 InternalExecuteMod；后者在 ApplyModToAttribute 之前调用目标 AttributeSet::PreGameplayEffectExecute，返回 false 时不应用该项。捕获列表是输入声明，不限制输出属性；不得通过“未捕获 Energy”或仅查 GE 静态 Modifiers 来认证 execution 不写资源。无需执行两次 calculation，也无需修改引擎、建立类白名单或认证体系。

R 应保留对**静态直接资源 modifier/持续资源聚合**的应用前拒绝及纯数值协调入口，移除“只要有 execution 就拒绝”的全局分支；在项目 CombatAttributeSet 的 PreGameplayEffectExecute（新增 override）按**实际 EvaluatedData.Attribute** 拒绝原生 GE execution 发出的 Energy/MaxEnergy 输出，并提供可定位诊断。非资源输出仍按既有 GAS/AttributeSet 语义处理。协调器的 SetNumericAttributeBase 不经该 GE 输出路径，不借此新增绕过标志；不得无声把 execution 输出重放到 RequestResourceEffect，造成 calculation、Tag、Cue、条件效果或其他输出重复。

边界必须准确：该保护保证标准 GE 输出中的资源项在写入前被拒绝，**不保证整个 execution/GE 未执行或整体回滚**。混合 Health+Energy 输出时，非资源项可能已经生效，资源项拒绝须可观察，不能把句柄有效/GE Applied 说成所有输出均接受，亦不能伪报整个 GE 在任何副作用前被拒绝。当前没有生产混合 execution 需求，本任务不创作其部分成功玩法或承诺完整资源 execution 支持；未来若实际内容要求混合输出全有全无，须带具体类/输出/副作用回 PM 定义该能力，不能靠本补丁假装满足。calculation 任意直接调用 ASC/原生写入或其他副作用也不受输出 veto 全面保护；这属于具体实现审计，不能扩写成第三方任意代码安全承诺，也不能据此继续封禁所有无关 execution。

RequestResourceEffect 保持只转纯即时单资源数值 spec 的窄契约，不产生原 GE handle、执行 calculation 或实现完整 GE 语义；它不是本节非资源 execution 的替代入口。R 的当前“不支持复杂资源 GE”承诺保留，但范围必须按受影响属性及实际路径表达。

**允许范围/验证**：仍由 R 独占 ResourceEffects.cpp::CanApplyResourceEffect、CombatAttributeSet.h/.cpp 的新增 PreGameplayEffectExecute 与独立 GAS execution 测试；A 不参与此局部改动，不改生产配置/资产或新设通用框架。首轮只读上述三个项目符号、C1 报告的 GE/资产两行事实及 UE 的 execution→InternalExecuteMod→PreGameplayEffectExecute 直接片段，无需重做资产审计。与 Reset 回执可在 R 同一任务中推进，execution 修正不依赖 Runtime 接口消费。

新增最小原生测试 calculation 仅作正式 GE 夹具（优先放独立测试文件，拟名 SkillLab.GAS.ResourceExecutionBoundary）：经真实 ApplyGameplayEffectSpecToSelf/Target 覆盖①非资源 execution 确实执行且伤害/治疗或 Health 输出正确，Energy/预留不变；②仅输出 Energy/MaxEnergy 时，未预留和已有预留均在实际写入前阻止，base/current/reserved/available 不被破坏；③混合输出两种顺序，资源拒绝诊断准确、非资源事实不回滚、不谎称 GE 整体拒绝；④周期 execution 真正 Tick 后仍逐次保护资源，原 GE 周期/自然移除的非资源行为保留。夹具不能直接调用 PreGameplayEffectExecute 冒充正式路径；测试须证明进入 calculation，否则原 meta GE 回归不足。只对受影响 R 路径补证，不启动新 production execution 功能。上述为待实现/验证合同，不是已运行结果，无需新增产品决策。

首轮复用 C1 的直接符号包及与发现相关的少量调用，不全文复读审计。超过 400 行的 AbilitySystemResource.cpp 必须按“受控写入/排队”“reservation/结算”“历史查询/寿命”拆同一 ASC 的 cpp 单元（仅修改涉及的单元）；队列、reservation 和历史仍各一份。拆分以可独立测试为准，不能复制账本。拟新增 `SkillLab.GAS.ResourceWriteIntegration`：正式入口/GE 加减覆盖、MaxEnergy、接近上限、多笔 reservation、同步回调重入/对象销毁、拒绝后无副作用及迟到结算不重扣；保留 EnergyLedger/EnergySettlementHistory。

直接原生非 virtual 旁路作为故障注入只证明边界/诊断；基于当前版本记录实际/预留/可用及 sticky 未决，不使用旧 52 数值作成功期望。若具体合法路径经协调/加载校验仍无法保护，交付调用/资产、最小复现、影响和已试方案，才构成技术阻塞；不能笼统要求用户重定已经确认的预留语义。

## C2：可立即开始的验证与测试上下文

C2 独占新增共享测试 fixture 的实现及旧大测试的职责迁移；A/B1/B2/R 各自独占自己的新增测试文件。共享 SkillLabProductionFixture.h、SkillLabSubmissionTestWorld.h 的改动先交 C2，避免各任务同时追加 friend/World/目录状态。C2 不修生产源码，失败按拥有者回交。

A/B2 改掉旧 Active/单动作字段时，C2 同步将现有 fixture/旧测试的私有访问改为其唯一上下文/点表观察入口，不新增镜像运行状态；必要 friend 声明交 A/B2 各自维护。此兼容迁移与生产改动一起构成编译快照，不等待所有新用例完成。C2 提供 fixture API 后，各实现者只向各自测试文件添加用例，不能各复制一套全局目录恢复。

立即可运行/准备的部分：现有十四项回归、Notify 实际超时、外部 Ability 中断、控制状态真实 GE 多来源、死亡/Reset 缓存、现有持续 GE 寿命、动画资产只读回查和当前可见运行。B1 的声明用例随解析就绪运行，A 的双执行随所有权就绪运行，B2 的单 Cast 多点和交付寿命可独立先测；同目标多点、双 Cast 多点/资源竞争在 A+B2 接通后补齐。只对新修改和未决疑点扩测。

首轮先读 SkillLabSkills/Private/Tests/SkillLabSubmissionTestWorld.h、SkillLabProductionFixture.h 的 World/MakeUnit/Start/目录恢复（不把直接 Begin/Execute 当最终端到端证据），再读一个相关独立测试及项目 [测试入口文档](../../../../ProjectContext/Modules/TestAndVerification.md)，按符号控制约 300–400 行。验证需求只读第 7–8 节和本任务段；不把旧 CastGameplayEventTests.cpp 全篇列为必读。

开始时从旧 CastGameplayEventTests 的 World/Actor/目录恢复职责提取可复用 fixture，建议 SkillLabCombatTestFixture.h/.cpp，仅暴露真实请求、GameplayEvent、World Tick、必要观察和 RAII 目录快照；不复制 Runtime/Lifecycle/账本。先迁移一组规则窗口/类别状态或缓存用例到独立文件，证明脱离旧全局状态能运行，再增加本批用例。原测试名保留一条真实 Ability/GAS 集成链。latent fixture 直到命令/Timer 完成才恢复目录/销毁 World，不能在排队后函数返回即恢复。全局目录替换用例同一进程串行；可以在独立隔离进程并行，不共享可写验证工程。

### 必须补足的行为证据

- Notify timeout 真正等待 Timer 到期，验证取消、不补发、不计费、Ability/动画/输入等待清理及旧事件隔离。外部 Ability End、死亡、目标失效、取消、失败、Reset分别检查原因与终态，不互相代替。
- 两个正式有效 GE 授予同一控制 Tag，移除一个仍门控/中断，最后来源移除恢复；区分 loose Tag fixture 与真实 GE。一般状态按窗口重评估，普通窗口冲突固定 RuleWindowConflict，死亡/沉默/硬直仍强制处理。
- 共享规则多技能复用与差异选择、未知类别、普通/重击权威 Tag、移动/主动取消、Recovery、latest-wins 替换/消费、闪避先取消后激活成功/失败。没有生产攻击类别/控制授予者时报告缺口，不按伤害阈值推断或新增玩法。
- A：允许新请求且原行为继续，门控失败原执行不变，各自事件/Timer/清理/终态，同 spec 实例、共享资源竞争；混合第三请求、缓存重评估及闪避全体取消按已确认需求第 8.2 节末表验证，不再等待 PM 定案。
- B2：同目标不同点、重复/错点/错事件、失败重试、多个真实投射物、创建内同步 overlap/自毁、交付后旧 Cast 结束仍命中、独立持续 GE 周期/自然移除及 Reset。正式解析→请求→Ability/Event→真实 GE，不能只直调提交函数或塞成功结果。
- A/B2/R 接口改动后保留已接受的资源/生产/迟到身份回归：首次成功一次费用/冷却、后点 Transaction=0、真实生产查询/资源历史独立、旧 Runtime 同数字新 Cast 的原键反馈、未知不释放/重扣、接收方同步销毁三事实。费用/效果回调中的重入、取消、Reset、EndPlay 不按广播顺序推断原子性。

## 构建、自动化与可见入口

每个执行者使用自己的可写隔离目录；复制前冻结输入、记录生产 Source/配置/资产哈希及所加诊断/测试差异，运行后再次核对。不复用 C1 的私有目录；旧共享隔离路径只是历史证据，不作为多人同时构建入口。命令示例的 ProjectPath/ValidationLog 由执行者设为其私有绝对路径：

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development $ProjectPath -WaitMutex -NoHotReloadFromIDE -gather
& 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' $ProjectPath -unattended -nop4 -NullRHI -nosplash -nosound '-ExecCmds=Automation RunTests SkillLab.Skills.CastLifecycle+SkillLab.Skills.CastGameplayEvent+SkillLab.Skills.DefinitionValidation+SkillLab.GAS.EnergyLedger+SkillLab.GAS.EnergySettlementHistory+SkillLab.GAS.DamageCalculation+SkillLab.Skills.ProductionRecords+SkillLab.Skills.SubmissionReconciliation+SkillLab.Skills.LateSubmission+SkillLab.Skills.UnconfirmedSubmission+SkillLab.Skills.AreaSubmissionSettlement+SkillLab.Skills.SubmissionFeedbackRouting+SkillLab.Skills.SubmissionFeedbackIdentity+SkillLab.Presentation.SubmissionIdentity' '-TestExit=Automation Test Queue Empty' "-abslog=$ValidationLog"
```

这是已存在的十四项目标集合；新增拟名必须等 IMPLEMENT_* 实际注册后加入。看每项目标 Success、测试总数、退出码和实际编译文件，不只看进程 0。摘要的既有 Condition failed/GameFeatureData 启动错误仍单独记录，不能描述整份日志无错误。

动画资产只读入口已核实 Main 支持 Spec/Report，**不加 -Write**：

```powershell
& 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' $ProjectPath -run=SkillLabAnimationContent "-Spec=$MontageSpecPath" "-Report=$MontageReadbackPath" -unattended -nop4 -NullRHI
```

Spec 可使用项目 Config/Animation/FireballMontage.json 或各技能 notify-patch 的只读检查入口，确认实际 Montage/Slot/Section/Notify；回查不等于视觉或能量资产审计。可见运行使用 Run_SkillLab_DX11.bat（实际是 -game 窗口）及控制台 SkillAnimationSmoke/SkillPresentationSmoke、SkillTest/SkillArea/SkillBurn/SkillTestRecover/SkillReset；**PIE** 则打开 UnrealEditor.exe 对应项目和测试地图后进入 Play，不能把 -game 或 NullRHI 记为 PIE。

先核对默认地图角色输入/Notify/效果、普通终止 Montage 停止，再补可兼容双播放与多点通知。保存事件身份与效果/费用观察、具体输入步骤及可见证据位置。无可交互视口/无人可操作时，真实限制仅是该项可见验收待执行；命令行自动化、资产加载回查和测试准备继续。没有可见证据，不声明动画锁释放/动作质量或 PIE 完成。

## 派发、集成和真实阻塞

当前可并行：已派 C1 只读审计、B1 Types/Provider、A Runtime/Ability/Animation、B2 Behavior/Executor/Projectile、C2 fixture/现有验证；R 拿到具体审计发现即可在 GAS 独占范围修复。公共 Types 由 B1 唯一写，Runtime 由 A 唯一写，GAS 由 R 唯一写，fixture 由 C2 唯一写，ResourceWriteAudit.md 由 C1 唯一写。共享请求/提交签名保持，跨所有者参数调整由声明所有者一次落盘，消费方不各自建兼容状态。

不可避免的顺序只有：B1 公开类型合入后消费者共同编译；A 窄 CancelBehavior 入口落地后 B2 接失败终止；A/B2 字段变化与 C2 既有测试兼容迁移一起合入可编译快照；A 的 point 效果索引与 B2 点执行接通后验证同目标多点；A+B2 接通后验证双 Cast 多点；R 修改后增量复核受影响写入/资产。集成快照在所需任务合入后冻结，不要求所有任务结束才运行局部测试。C1 原证据未受改动影响则直接复用。

共同仲裁、闪避取消集合和缓存消费已在需求第 8.2 节定案，技术结构/测试载体由实现者完成。当前所有者以本批 README 为准，原 A/B2/C2 合并交付，C1 审计保持暂停并保留已有证据。具体资源无法支持所需双 Montage 或具体合法写入不能保护时，带实测回交相应能力冲突，不预设阻塞。当前尚未执行的资产审计/可见 PIE 是验证状态，不能当产品问题，也不能据此宣布整批完成。

本次 R 局部补充按上文两个小节接续：GAS 完整资源回执先就绪，原 A 再消费并验证 Reset 端到端；GE execution 按实际输出保护的兼容修正由 R 独立完成，不等待 A。现有已接受的合并执行/B1 不重开全面 Review，新增改动及直接影响由 PM 安排定向审查。
