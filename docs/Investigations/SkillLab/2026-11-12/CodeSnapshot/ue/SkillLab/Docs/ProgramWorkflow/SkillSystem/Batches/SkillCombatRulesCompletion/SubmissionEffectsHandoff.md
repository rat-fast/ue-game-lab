# 多提交声明、执行与已交付效果生命周期交接

2026-11-12 接续说明：共同仲裁已写入权威需求第 8.2 节，本文原“混合第三请求产品问题”不再是阻塞。当前执行所有者与上下文分配以本批 README 为准：B1 定义解析与合并执行线两条并行，原 A/B2/C2 由合并执行负责人一并接通；技术合同保留，勿按旧分工重派任务。

## 范围与已接受前置

依据[需求](../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md)第 2、4–6、8、8.1 节，分为 **B1：规范化声明/解析与兼容**、**B2：多点执行/异步动作与独立效果** 两个可独立交付的实现任务。两者因直接共享声明合同放在本文件；不要求实现者读取另一任务的全部源码。下文新接口/文件名是实施建议，约定的身份和行为必须保持。

PM 已转交原 Reviewer 限定范围接受：生产记录、独立资源历史查询、会话迟到收敛/隔离解除/补报已接通，P2-1/P2-2 均关闭。事实来源为[实现摘要](TimingRulesImplementationSummary.md)顶部与当前源码；十四项目标通过及 194 份源码一致性不是多点/异步/全路径资源审计的完成证明。不再派发“下一步接生产来源”。跨进程恢复不在需求内。

当前链：Runtime Begin 创建生产键/预留 → BehaviorProduction 执行即时/范围/创建 → Executor 经 EffectResolver.OnApplicationReturned 写真实 GE 接受事实 → Runtime Resolve → 必要时既有会话 Reconcile。Projectile 已有 ConfirmDelivery：初始化且 BeginPlay 后记录交付，再开放碰撞；命中不改交付回执。范围已有 PlanArea/子结果聚合。必须复用这条链。

剩余缺口：RequestDeclaredAction/Effect 固定 point=1；Behavior 只有一组动作 flags/result 和一个 Projectile；ApplyBehaviorEffect 在整个 Cast 内按 TargetActor 去重；OnProjectileResolved 要求 IsCurrent，Cleanup 解绑并销毁投射物；RunScript 的 Lua VM 调用后立即关闭。Lifecycle 已有按点 BeginSubmission/ResolveSubmission 和成功点集合，不能另建一套成功权威。

## B1：声明与解析的完整交付

目标是让内容能明确声明有限提交点、触发、目标绑定及完成方式，并保持现有四技能内容原样可解析。不新增生产多段技能。本任务拥有 Skills/Public/SkillLabSkillTypes.h、SkillLabSkillDefinitionProvider.h/.cpp 及拆出的解析单元；公共 Cast token（供 A 使用）、Allow 决策和点类型由这一所有者统一落盘。

### 最小规范化合同

建议 `FSkillLabSubmissionPointDefinition`：正整数稳定 `Id`、`Trigger`、条件参数、`TargetBinding`。Id 在技能内唯一，直接成为 Lifecycle/ProductionKey.Point，不按触发次序重编号，不能使用 Lua 浮点保存大整数。取正 int32 即足够，拒绝 0、负数、分数和溢出。

建议文本权威入口为技能 JSON 条目的 `submissionPoints`，每点字段如下；这属于实现方案，不是新增玩法需求：

| 字段 | 含义与校验 |
| --- | --- |
| id | 正 int32，每技能唯一；所有运行入口引用此 id |
| trigger | Script / Release / Timer / GameplayEvent；必须明确，未知值拒绝 |
| delay | 仅 Timer 使用，自本 Cast 正式 Release 起算，有限且非负；零延迟也走同一调度入口 |
| eventTag | 仅 GameplayEvent 使用，须注册、在此 Cast 的正式 Ability 事件集合中；与窗口/释放/完成角色冲突时明确拒绝，不能由处理顺序决定 |
| target | Source / RequestTarget / RequestLocation，分别绑定原 Caster、校验后的原目标、原请求位置；只允许与现有 shape/relation/execution 相容的组合 |

每点继承已规范化技能的 execution/effect/relation 及参数快照，不在 JSON 再重复 Lua 中同一字段的权威定义；如实现确需逐点参数，使用明确可选覆盖并由同一解析路径规范化，不能顺便增加新效果种类。多个点可作用同一目标；范围点仍通过正式目标查询。目标失效按正式目标/效果校验报告，不从当前选中目标借身份。脚本不能注入任意目标、伤害或确认结果。

未声明 submissionPoints 的旧内容规范化为唯一隐式点 1、Script 触发、原目标语义，保留现有 OnSkillCast/RequestAction()/ApplyEffect() 兼容。显式列表不能为空；显式多点不接受无参调用猜点。JSON 保存触发/目标声明，Lua 调用只引用 id，不重复定义时点。

完成方式建议规范化为两种：旧脚本/既有 CompletionEventTag 控制完成；显式声明可选择“全部点获得明确结果后进入收尾”。后者的“结果”包含真实接受、明确拒绝和异步动作已解决，不把未知算失败；未知转既有有限等待。动作成功/伤害/命中仍分开。CompletionEvent 不能跳过尚待触发的声明点并伪报全部执行；若内容明确选择提前结束，剩余点被取消而非提交失败。具体字段名由 B1 落地并交 B2，不由 B2再次设计一份格式。

### 解析、共享接口与上下文拆分

保留 ParseCatalogText/ValidateJsonText/ReloadDefinitions 的统一入口、候选失败不替换当前目录及 LuaSource 快照。没有现成独立 schema；本次类型化校验即正式入口，不要求为形式新增另一权威 schema。

B1 实现时必须拆出内部解析职责，建议 Private/SkillLabSkillDefinitionParsing.h 声明窄函数：

- ParseLuaDeclaration(source, name, candidate, error)：封装 Lua capture/预算/声明和运行入口校验。
- ParseSkillEntry(json, luaSources, candidate, error) 与 ParseSubmissionPoints(json, candidate, error)：技能参数、点和跨字段校验；输出尚未发布的规范化候选。
- ParseRuleProfiles/ParseRuleWindows：共享规则、类别、窗口和 Allow 解析。Allow 仅对技能请求/abilityCategories 合法，不扩大受击/移动的决策域；保持 RuleWindowConflict/Reject，无优先级。
- Provider 保留目录组合、重复字段/ID 检查、规则引用、文件加载与原子替换；解析单元不能另存 Definitions 或 bLoadAttempted。

建议分别落 LuaDeclaration.cpp、SkillDefinitionParsing.cpp、SkillRuleParsing.cpp、SkillSubmissionDefinitionParsing.cpp，按职责决定是否合并小单元。不是复制整个 Provider/静态目录到多个 cpp。

首轮读取相对 Source/：Types 的决策/规则与 FSkillLabSkillDefinition、Provider 的 FLuaDefinitionCapture/CaptureLuaDefinition/ParseLuaDefinition（约 185 行）及 Parse 中允许字段/Release/Completion 校验，合计约 350–400 行。第二轮才读 ParseRuleProfiles/Windows 与 ParseCatalog/ParseText，以核对共享引用和候选替换；不读 Runtime 全篇。原 600+ 行 Provider 不可继续堆入点解析。

允许新增独立 SkillLabSubmissionDefinitionTests.cpp；拟注册 `SkillLab.Skills.SubmissionDefinition`。通过正式 ParseCatalogText 覆盖旧四类定义、两点、重复/未知/坏触发、错误目标组合、Tag 冲突、坏目录保留；另测 Allow 与类别精确匹配。保留 DefinitionValidation。最小 JSON/Lua 夹具使用正式解析，不能直接构造 Definition 冒充声明能力。交付实际字段、类型、错误原因及可供内容职责消费的最小样本。

## B2：多点执行和异步所有权的完整交付

消费 B1 规范化合同和 [A 交接](TimingRulesHandoff.md)的执行合同；无需等 A 全部完成才开发。现有 Behavior 已每 Cast 一个实例，Begin/Resolve/AbortBehaviorSubmission 与 GetProductionHandle 签名可保持，B2 可基于当前单 Cast Runtime 开发/测试点状态、触发和异步动作。A 负责将这些方法内部改为上下文查找及 point 效果索引，双方不同时写 Runtime/Behavior。B2 由同一实现上下文负责 Behavior/Executor/Projectile 的完整能力；最终双 Cast 多点验收才依赖 A 接通。混合第三请求的产品问题不阻塞单 Cast 多点。

Behavior::Fail 改用 A 提供的 CancelBehavior(Behavior, CastId, reason) 或等价按归属接口，不再调用无参 CancelActiveCast；Complete/ReportMiss 继续使用既有显式 Behavior+Cast 接口。A 在自己的完整任务内尽早交付窄终止入口，B2 仅该处编译接线等待它，点表/生产/异步逻辑并行推进，不为这个接口另起架构循环。

### 调度、Lua 宿主与幂等

在每 Cast 的 Behavior 中建立唯一点运行表，保存声明快照、触发/动作状态、原 ProductionHandle、点结果与已交付动作引用；A 的 Cast 上下文通过拥有 Behavior 间接拥有这张表，不再复制一份。Lifecycle 的已成功点集合仍是提交幂等权威，Runtime 的 bCommitted 仍控制首次唯一费用。点失败不得消耗唯一计费机会；已成功/正在执行/未确认点不能重发，明确失败仅在内容再次正式请求且条件仍有效时可重试，不自动补发。

统一 `RequestSubmissionPoint(token, pointId, triggerEvidence)`：查声明及 Cast/阶段 → 核对触发条件 → BeginBehaviorSubmission → 实际动作 → Resolve/Abort。未知点、早到/错 Tag/错 Cast、重复成功返回可区分原因，保持 Cast，不直接 Cancel。仅缺失必要通知超时、Ability 中断或其他既定终止规则取消。

Release/Timer/GameplayEvent 由 Behavior 的 C++ 点调度调用同一入口：Execute 对应 Release，ReceiveEvent 消费经 A 验证的事件，Timer 捕获 token+pointId 并由 Behavior::Cleanup 清理；A 从 B1 声明收集事件到 Ability 等待集合即可，不另建 Runtime 点调度状态。GameplayEvent 不从浮点 EventMagnitude 提取身份。Script 点仅可在内容声明允许的执行窗口由 Lua 请求。不要跨 Tick 持有 bSubmitting 或一个同步 reservation 调用栈。

宿主公开 `SkillLabSkill_RequestAction(pointId)`、`SkillLabSkill_ApplyEffect(pointId)`，无参仅兼容隐式单点；返回至少 pointId、原 submissionId、accepted/applied、pending、completed、reason。两入口消费同一套点表，不能各用一组 bool 互相绕过。RequestFinish/自动完成检查整组点和异步解决状态，不再以一个 bActionResolved 判定。声明列表存在时，C++ 自动触发点与脚本调用也经过相同防重入口。

当前 VM 是一次性调用；本方案由 C++ 持有定时与事件调度，不要求长期挂起 Lua/coroutine。RunScript 调用结束但仍有合法声明点或已交付动作等待时，不误报 ScriptNoFinish；无声明后续且无合法等待的旧脚本错误继续可观察。脚本调用不提供 Confirm/扣费/冷却入口。

### 每点效果和费用接线

ApplyBehaviorEffect 的去重键由 A 改为“原 Cast + point + 目标/实际子操作”，B2 传入真实 ProductionHandle，不继续 Cast 内按目标全局拒绝，也不能简单删除防重。范围的每目标子操作继续复用 PlanArea，去重/结果关联保留完整生产键，失败重试不得复用旧已终结 operation 伪造新生产事实。A 适配未合入前，同目标不同点的端到端断言明确待集成；B2 仍可用不同目标/范围及独立点表单测验证自己的完整接线。

每点 BeginProduction 保留 Point/Operation/Id/RequestId，首次成功按既有唯一交易结转；后点 Transaction=0，仍有独立完整生产身份。Resolve、未决交接、OnSubmissionConfirmed 适配 A 的上下文，不能替换会话结果系统。已计费后点未知不再预留，不重启冷却；等待结束后迟到结果只关联原点，不能误推进仍活动的另一执行。

### 已交付动作与持续 GE

Projectile 是已存在的自然独立 Actor，优先让其持有不可变提交快照（原 token/点/生产键、Source/Target/ASC 弱引用、定义与请求），不为本次新增万能异步管理框架。保留 ConfirmDelivery 的先记录后开放碰撞握手；多点用各自的 Actor/操作记录，不能覆写单个 Behavior::Projectile。

增加内部 `ApplySubmittedEffect(context, target, hitOperation)` 或等价窄入口：只接受已确认交付且未消费的动作能力，按原身份经正式目标/关系/GAS Resolver 结算。它不依赖“Cast 仍是当前”，不提供公共脚本绕过入口。可从现有 Executor 提取共享目标/效果执行，Cast 入口先校验活动上下文，已提交入口校验交付能力；避免复制伤害/治疗算法。命中操作与交付操作分别关联，Hit/Blocked/Expired/Destroyed 不回写交付成功，不再计费。

投射物自身幂等解决、清 Timer 并销毁；回调先按原动作归属派发事实，再由仍有效的原 Cast 决定是否完成等待，Cast 已结束只报告原结果，不推进新 Cast。创建失败/未交付由 Cast 清理；已确认交付的动作从 Behavior::Cleanup 的销毁清单移出，普通完成/取消不伪造撤回。Source/Target/ASC/World 真正销毁时按有效性结束检测、不向失效对象结算。Runtime 替换不能借新 Runtime 当前状态施加旧效果。

持续 GE 由目标 ASC/有效句柄持有，普通 Cast 清理不调用 RemoveActiveGameplayEffect。EffectEventAdapter 已按句柄保存 Origin，复用并补完整 point/SubmissionKey；周期/移除只报告原事实。Reset 保留现有显式清理已跟踪效果语义，EndPlay 区分解除观察与移除实际独立效果，不能因 Runtime 组件重建误清仍合法的 GE。若观察者失效，解除订阅，不补造 BuffRemoved。多来源状态按 ASC 有效计数，不按一个 Cast 的结束清空同 Tag。

### 落点、读取与验证

B2 独占 SkillLabSkillBehavior.h/.cpp、BehaviorProduction.cpp、FireballProjectile.h/.cpp、SkillEffectExecutor.h/.cpp、SkillEffectEventAdapter.h/.cpp；可新增 Private/SkillLabSubmissionPointExecution.*、SkillLabSubmittedEffect.* 或同类 cpp。Runtime 始终由 A 独占，B2通过既有方法及明确的参数合同消费，不必串行接管整个 Runtime。Types/Provider 修改交 B1；GAS 资源/Resolver 公共接口修改交 R，B2只复用。独立审计 C1 不改生产 Source。生产 Lua/JSON 和二进制资产不在代码任务修改范围。

首轮分三个直接依赖包，每包约 300–400 行：① Lifecycle Begin/Resolve 与 RuntimeSubmission Begin/Resolve、Behavior RequestDeclaredAction/Effect，核对唯一提交；② BehaviorProduction、Projectile ConfirmDelivery/Activate/ResolveImpact、Executor.Apply，核对真实创建和命中；③ Behavior RunScript/ReceiveEvent/RequestFinish/Cleanup、Adapter Track/HandleDamage/Removal，核对脚本等待和效果寿命。只在同步属性重入疑点补读 Runtime 的 FinishSubmissionTermination 或 GAS Resolver，注明依赖，不加载旧大测试全篇。

实现时即拆 Behavior 为 Lua 宿主适配、每点编排、实际生产动作三个单元；点表只有一份，生产记录继续在会话。独立测试复用 C2 的最小 fixture，不把新用例继续追加进 CastGameplayEventTests。拟注册 `SkillLab.Skills.MultipleSubmissions` 和 `SkillLab.Skills.SubmittedActionLifetime`：

- 正式 JSON/Lua → Request → Ability → Release/Event/Timer → 真实 GE，覆盖同目标不同点、同点重复/重入、失败再试、未声明点、事件乱序、目标失效、取消/Reset 和终态后通知；费用与冷却只首次成功一次。
- 多个真实 Projectile 各自创建/交付、创建时同步 overlap、自毁、命中/阻挡/过期；原 Cast 已完成/取消后命中仍按原身份结算，其他新 Cast 不变；命中后绝不改交付记录/再收费。
- 独立持续 GE 在普通完成/取消后继续真实周期并自然移除；两个来源同 Tag 逐个移除；Reset/对象销毁另断言，不用普通 Cancel 代替。
- 首点未知、已计费后点未知及迟到收敛继续经过真实生产/资源查询；保留现有 ProductionRecords、AreaSubmissionSettlement、SubmissionFeedbackIdentity 等相关回归，不重做已关闭缺陷。

正式夹具可以注入经校验的最小候选目录/脚本文本并暂时替换测试目录，结束后恢复；不能直接塞成功 bool/手工提交事件作为验收。具体四技能是否选用多点、触发时刻、数值及动画资源由内容配置职责消费 B1/B2 接口，本任务不要求新增生产多段玩法。生产内容确需迁移时只交明确字段/兼容差异给 PM 安排。

## 派发关系

B1、A、B2、已派独立 C1 审计、C2 准备可以并行，各自独占文件。B1 尽早落公开类型，A/B2 在类型合入前可基于本合同准备内部实现，不各自提交重复类型；此编译依赖不等于整任务串行。B2 的单 Cast 正式路径不等待 A 完成，A 的双执行可用旧单点内容验证。必要集成为 B1 类型/解析与 A 窄终止入口 → 对应消费者构建；A 上下文变化与 C2 旧私有字段测试迁移共同进入可编译快照；A+B2 接通 → 同目标多点/双 Cast 多点交叉验收。B2 若移除旧 Behavior 单动作私有字段，同样由 C2迁移既有 ProductionFixture，B2 不并发编辑共享夹具。R 的 GAS 修复消费 C1 已有发现即可开始，不必等审计所有条目结束。C2 在每条路径就绪时增量验证，不等全部完成，不重复 C1 已取得且未受影响的审计证据。
