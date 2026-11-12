# 执行所有权、请求仲裁与事件归属交接

## 当前依据与范围

对应实现任务 **A：每 Cast 执行所有权与允许衔接**，依据[需求](../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md)第 2–6、8 节。本轮只更新交接。下文新类型、函数和文件名是实施建议，输入输出、所有权及不变量是接入合同，不能当作已有接口。

PM 已转交原 Reviewer 接受结论：真实生产记录 → GAS 独立资源查询 → 会话迟到消费、唯一费用/冷却、隔离解除及补报在限定范围通过；P2-1 范围未知释放、P2-2 实际 Runtime 反馈身份均关闭。[实现摘要](TimingRulesImplementationSummary.md)顶部“Runtime 实际反馈的原提交身份补齐”记录十四项目标测试、194 份源码一致性和日志限制；“待复查”是提交当时状态。A 只适配上下文访问，不重做账本、结果协议或 1 秒等待。整批尚未验收，跨进程恢复不在需求内。

## 已核对的运行链与缺口

`SubmitSkillRequest / SubmitSkillOperationRequest → SubmitSkillOperation → TryActivateAbility → NotifyAbilityActivated → BindSkillCast → ForwardValidatedNotify → HandleAbilityGameplayEvent → ReleaseCast → Behavior`。

- Runtime 的 Definition、Request、Lifecycle、Ability、Behavior、阶段/超时/未决 Timer、提交、效果索引和延迟终止均只有一份。活动期非 Reject/CancelAndActivate 请求仍进入单槽缓存，不能仅加 Allow 枚举后覆盖状态。
- Ability 是 `InstancedPerActor`，只有一个 BoundCastId 和一组 EventTasks；Loadout 每 SkillId 授予一个 spec。GameplayEvent 已校验 Runtime、Ability、Cast、Instigator，应扩充归属而非移除校验。
- Animation 的 PlayCastMontage 停止上一播放，只保存一份 ActiveCastId/Montage/InstanceId；ForwardAnimNotify 核对真实 MontageInstanceID。数字 Cast 大小和“最近播放”不能在重建/并行时充当身份。
- OnSubmissionConfirmed 已保存完整原 FSkillLabSubmissionKey，但只对组件单个 Cast 查找原等待；应改为查执行上下文，会话继续独占交付和去重。

## A 的执行合同

Runtime 管理按执行 token 查找的集合，建议 token 为 `{Runtime 对象身份, RuntimeGeneration, CastId}`，沿用生产键中的同一身份。建议 `FSkillLabCastExecution` 承载独立状态；反射结构或 UObject 由实现者选择，Ability/Behavior 必须正确参与 GC。

| 所有者 | 唯一拥有的状态及操作 |
| --- | --- |
| Cast 上下文 | Definition/Request 快照、Lifecycle、Ability/Behavior、窗口与消费记录、阶段/超时/未决 Timer、点状态、bCommitted、同步提交、效果索引、延迟取消/完成、终态 |
| Runtime/角色 | 执行集合、Request/Cast 分配与请求历史、ASC 状态订阅、单槽 PendingSkillIntent 和消费 Timer、Reset/EndPlay 屏障、激活绑定 |
| ASC | actual/reserved/available、唯一交易和资源闭合历史、属性及控制状态有效计数 |
| 既有会话组件 | ProductionResults、未决记录/SkillId 隔离、唯一冷却、PendingFeedback 和通知去重 |
| 表现适配 | token 到真实播放实例的映射；只消费事件 |

必须提供以下窄能力，命名可调整：

- `FindExecution(token)` 和 `FindExecution(Behavior, CastId)`，后者同时核对 Behavior 的 Runtime/generation。可能增删集合的同步委托前后重新查找，不能保留失效的 TMap 元素引用。
- `TryActivateRequest(request, definition, plan)` 以局部候选完成定义、授予、Caster、目标、状态、available、冷却及 GAS 门控；不能把候选写入旧 Cast。失败只清候选，成功发布自己的 Started。
- `CancelCast(token, reason)`、`FinishCast(token, outcome)`、`ReleaseCast(ability, token)` 和事件处理只作用于命中执行。现有无参查询及 CancelActiveCast 的兼容调用者须明确，不能随机取集合元素；业务内部改用 token。
- 向 Behavior 提供 `CancelBehavior(Behavior, CastId, reason)` 或等价按原归属终止入口，与现有 CompleteBehavior/ReportBehaviorMiss 同样核对 Runtime/generation。B2 的 Fail 不再调用无归属 CancelActiveCast。A 在本任务内部先落这一窄声明/实现供 B2 编译，不另开微任务或等待整个并行能力完成。
- 既有 Begin/Resolve/AbortBehaviorSubmission、GetProductionHandle 的调用契约保留，内部按 Behavior+Cast 找上下文。OnSubmissionConfirmed 只用原完整键结束原等待；替换 Runtime 只转发原身份反馈。A 同时把 ApplyBehaviorEffect 的成功效果索引改为原 Cast+point+目标/子操作，点从显式 ProductionHandle 或当前同步提交取得；不同点同目标不再互相阻断。这是 B2 所需的 Runtime 适配，由 A 独占实施。
- Ability 激活通过短寿命 token+spec 握手绑定，不从“最近 Cast”反查。优先核实并采用 `InstancedPerExecution` 支持同 spec 的独立实例，实测 LocalOnly 路径及实例释放；其他方案也必须证明不重绑/结束旧实例。本批不新增网络预测能力。

bSubmitting、bApplyingEffect、延迟取消属于每执行；Reset/EndPlay 是角色屏障。属性/效果回调中启动另一 Cast 时，原提交继续持有原 context/production/transaction。不能切换组件 Active 字段、复制 ASC 或会话状态来模拟隔离。

## 仲裁与角色共享事实

B1 解析任务提供显式 `Allow`（或等价名称）：新请求正式激活，原行为继续。A 消费规范化结果，不把面向移动/受击的 Continue 自动解释为技能激活。

Allow 的新门控失败不干扰旧执行。保留 CancelAndActivate 的“先取消，再尝试新激活，失败不恢复旧 Cast”语义及闪避行为。Recovery 不激活新请求；普通/重击、移动/主动取消继续按已确认规则。普通窗口冲突保持可观察 `RuleWindowConflict`，不增加优先级。死亡、沉默、硬直独立阻止激活并中断全部适用执行；一般状态按各执行窗口重评估。

角色仅有一个 latest-wins 意图。不能因某个并行 Cast 结束就消费仍受其他有效门控约束的请求，也不能复制每 Cast 缓存。取消、死亡、Reset、提交异常终止按原规则清槽；自然完成后的消费在旧 Ability/上下文清理后安全重评估，分配新身份。状态可只订阅一次，逐个 token 求值；广播可能删除执行，须重新查找。

**共同仲裁已定案（2026-11-12）**：权威依据为[需求第 8.2 节](../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md#82-多-cast-共同仲裁用户确认规则)，不再列产品待决。逐执行产生决策后按该节合成，不能取最新 Cast；请求、窗口变化、自然结束后的缓存重评估使用同一合成入口。闪避取消全部当前施法后再正式激活，不将其取消范围扩展为普通技能无条件取消全体的授权；独立效果的存续与有效控制状态分别处理。实现和正式测试覆盖该节末表，不以单来源双执行测试替代混合第三请求验收。

## 表现、终止与移交

A 同一实现上下文负责必要表现接线：播放记录按 Runtime/generation/Cast 对应真实 AnimInstance+MontageInstanceID，旧取消只停止自己的播放。Started/Cancelled/Completed 需带 Cast 归属；Router/表现 Lua 宿主只透传。已接受 SubmissionKey.Id/RequestId 继续保真，提交键不能代替尚未提交的 Cast 身份。

先以无需占 Montage 的即时/Timer 请求证明 Allow，再验证资源明确兼容的动作。同槽视觉替换不代表旧业务取消，也不能伪造旧 Notify；实际核实 slot/group、缺失通知超时与配置约束。若两个 Notify 动作确需并播而资源不足，报告具体资源并交内容/表现职责配置，不自行新增玩法。既有 Notify 超时取消、不补发规则保持。

终止先封闭该执行的新事件/提交，再收束已进入操作，最后清理自己的 Ability task、Timer、窗口、Behavior、检测，只发布一次终态。未知提交交既有会话，移除上下文不释放 reservation。Reset/EndPlay 等所有正在提交的执行完成交接后统一清理，持续复核接收者寿命；保留 SubmissionDeliveryUnavailable 的三事实和销毁保护。A 的清理接口区分未交付 Cast 资源与已移交动作，B2 负责后者及独立 GE 接入。

## 首轮上下文与拆分落点

以下路径相对 Source/，约 400 物理行是每工作单元首轮参考，不默认读完整 Runtime/旧大测试。

1. **所有权**：SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h 的 ISkillLabSkillCastContext、状态字段及 Behavior/Ability 接口；SkillLabCastLifecycle.h 的 token/提交接口；SkillLabSkillGameplayAbility.h/.cpp 的 Activate/Bind/End；Loadout 的 FindGrantedHandle。先控制在约 400 行，再补 Runtime 的 NotifyAbilityActivated/Ended、FinishCast/ClearPendingCast，以核对同步结束。
2. **请求/规则**：Runtime 的 SubmitSkillOperation、EvaluateAbilityCategory、ResolveOperationRules、Store/Schedule/ConsumePendingSkillIntent；Types 的决策/规则结构。请求约 170 行、规则/缓存约 230 行分轮读取；状态再补 ReevaluateInterruptStates/NotifyDamageTaken，不加载 Provider 全篇。
3. **提交接线**：RuntimeSubmission 的 Begin/Resolve、RuntimeUnconfirmed 的 StartIndeterminateWait、RuntimeConfirmation 的 OnSubmissionConfirmed 分轮适配；生产记录/GAS 算法只读。表现首轮读 ForwardAnimNotify/StopCastMontage 和 Ability 事件校验，再沿调用补播放及 Lua 适配。

A 开始时即将 Runtime 按“请求/缓存编排”“按 Cast 生命周期与终止”“规则快照/状态分派”拆内部单元，建议 SkillLabSkillRuntimeRequests.cpp、SkillLabSkillRuntimeExecution.cpp、SkillLabSkillRuntimeRules.cpp；提交/未决/确认沿现有独立 cpp。先建立唯一上下文和显式参数，再迁移调用，不能只搬函数保留一份 Active 状态。规则单元输出决策/原因，订阅和广播留编排层。先保持单 Cast 回归，再开放集合。

## 修改责任、顺序与验证

A 独占 Runtime 头及各 Runtime cpp、执行结构、GameplayAbility/事件载荷、必要 Loadout 接线、Animation/AnimNotify 与表现 Cast 身份适配；可新增内部单元和独立测试。不改 GAS 账本、会话协议、生产 Lua/JSON、资产及旧批次。Types/Provider 由 B1 独占，A 所需 Allow 枚举及公开 Cast 身份类型由 B1 提供，避免公共头双写。

旧 ProductionFixture/CastGameplayEvent 等测试直接访问 ActiveBehavior、Lifecycle、Timer 等私有状态；A 移除单实例字段会影响编译，不能忽略。A 提供统一按 token 的只读测试观察/查找入口，C2 负责旧 fixture/测试的兼容迁移；A 不在自己的分支复制另一套 fixture。合并编译快照须包含这部分兼容修改，但不要求 C2 全部新增验证完成。相关 friend 声明由 A 唯一改 Runtime/Ability 头。

A 可与 B1、B2、独立 C1 审计及 C2 准备并行：B2 保留现有 Behavior→Runtime 提交方法签名，在 Behavior 内实现点调度/动作状态与已交付 Projectile；A 迁移 Runtime 上下文并完成 point 去重及 CancelBehavior 适配。A 不改 Behavior/Projectile，B2 不改 Runtime，双方以方法合同和正式单 Cast 路径独立构建/测试；最后合入再验证双 Cast 多点。必要顺序是 B1 公开类型与 A 的窄终止接口先合入供对应消费者编译，以及 A+B2 的交叉验收必须在两者接通后进行，不要求完成 A 全任务才开发 B2。资源修复 R 如需改 Reset，给 A 窄接口要求，由 A 修改 Runtime。文档数不等于 Agent 数，A 可同一实现者分上述单元完成。

新增独立 `SkillLab.Skills.CastExecutionOwnership`（拟名，须实际注册）：真实请求/Ability 两次激活、Allow 门控失败旧执行不变、同 spec 实例隔离、各自 Notify/Timer/取消/外部 End/重入、Runtime 重建同数字旧事件、共享 available 竞争、控制状态与角色单槽缓存/Recovery。至少一条走真实 GameplayEvent，不直调 ReleaseCast。保留 CastLifecycle/CastGameplayEvent/DefinitionValidation 及已接受身份回归。共享 fixture/旧测试拆分由 [C2 验证任务](VerificationHandoff.md)负责。

交付实际 token/查找/终止接口、兼容调用者、源码拆分、正式测试名/证据、第 8.2 节共同仲裁结果和表现资源限制。编译通过不等于并行或可见 Montage 通过。
