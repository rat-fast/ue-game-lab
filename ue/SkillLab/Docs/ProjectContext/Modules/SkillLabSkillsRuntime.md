# SkillLabSkills 技能运行时

## Module Responsibility

`SkillLabSkills` 负责内容定义、loadout、请求仲裁、多 Cast 生命周期、目标解析、提交点调度、生产结果与来源事件。运行时发布技能事实；GAS 拥有属性与资源结算，表现模块消费事件，技能运行时不持有表现对象。

## Code And Assets

以下文件位于 `Source/SkillLabSkills/`；公开类型在 `Public/`，实现位于 `Private/`。

| 定位入口 | 当前职责 |
| --- | --- |
| `SkillLabSkillRuntimeComponent.*` | 初始化、Reset/EndPlay 屏障与事件发布 |
| `SkillLabSkillRuntimeOwnership.cpp`、`SkillLabCastExecution.h` | 执行集合查询和每 Cast 可变上下文 |
| `SkillLabSkillRuntimeRequests.cpp`、`SkillLabSkillRuntimeRules.cpp` | 共同仲裁、门控、单槽意图、状态/规则窗口 |
| `SkillLabSkillRuntimeExecution.cpp`、`SkillLabSkillGameplayAbility.*` | Ability 绑定、阶段、Notify 与终止 |
| `SkillLabSkillRuntimeSubmission.cpp`、`SkillLabSkillRuntimeUnconfirmed.cpp`、`SkillLabSkillRuntimeConfirmation.cpp` | 提交事务、有限等待、原身份确认 |
| `SkillLabSkillDefinitionProvider.*`、`SkillLabLuaDeclarationParsing.cpp`、`SkillLabSkillEntryParsing.cpp`、`SkillLabSkillRuleParsing.cpp`、`SkillLabSubmissionDefinitionParsing.cpp` | 内容目录、Lua 声明、JSON 字段/规则/提交点校验；内部声明见 `SkillLabSkillDefinitionParsing.h` |
| `SkillLabSkillBehavior.*`、`SkillLabSkillBehaviorLua.cpp`、`SkillLabSubmissionPointExecution.cpp` | 通用 Lua Behavior、受限宿主及每 Cast 点表 |
| `SkillLabTargetingService.*`、`SkillLabSkillEffectExecutor.*`、`SkillLabSubmittedEffect.cpp`、`SkillLabFireballProjectile.*` | 目标检查、效果执行、已交付投射物命中 |
| `SkillLabSubmissionResults.*`、`SkillLabUnconfirmedSubmissions.*`、`SkillLabSkillEffectEventAdapter.*` | 生产记录、会话未决/冷却、持续效果来源观察 |

内容入口为 `Config/SkillDefinitions.json` 及四个 `Config/Skill_*.lua`。`SkillLabSkills` 私有依赖 `SkillLabLuaRuntime` facade，不自行编译 Lua 第三方源码。

## Runtime Entry

`BeginPlay -> InitializeSkills` 初始化 ASC actor info、loadout、效果执行器及本 ASC 状态/受击订阅。玩家与测试命令经 `SubmitSkillRequest` / `CastSkill` 进入同一请求路径；表现路由由表现组件创建并订阅 `OnNativeSkillEvent`。

外部按完整 `FSkillLabCastExecutionToken` 操作：`FindExecution`、`GetExecutionTokens`、`CancelCast`、`ForwardValidatedNotify`、`ForwardValidatedNotifyStateEnd`。`FindExecutionByCastId`、`GetSingleExecution` 是受限观察入口，不能把数字 CastId 当作跨 Runtime 身份；Behavior 终止入口另校验 Behavior/CastId。

## Runtime Flow

1. Provider 在请求前完成定义、脚本编译和入口预检，以有效候选原子替换目录。JSON 根字段为 `skills`、`combatRules`；候选失败保留旧目录，首次加载失败时没有可解析的技能。Lua `SkillLabSkill_Define` 提供目标形状、关系、效果、执行和规则声明；JSON 提供数值、阶段、动画、规则 profile、提交点及脚本绑定。Registry 只赋共享 Ability/Behavior 类，不再提供各技能结构内容。
2. Runtime 汇总活动执行的规则决定：`Reject` 优先，其次 `Buffer`；全部允许后仍执行初始化、授予、施法者/状态、冷却、可用能量和目标等完整门控。单槽意图 latest-wins；普通窗口冲突以 `RuleWindowConflict` 处理。闪避取消全部活动执行后再尝试激活，失败不恢复；普通请求只取消明确授权的来源。回调重入后重查计划和 generation，不能沿用旧取消计划跳过门控。
3. 创建 `USkillLabCastExecution`，保存定义/请求快照、生命周期、Ability/Behavior、Timer、窗口及提交状态。完整 token 包含 Runtime 弱引用、RuntimeGeneration、CastId。`USkillLabSkillGameplayAbility` 为 `InstancedPerExecution`、`LocalOnly`，以 spec 和 token 绑定激活；GameplayEvent 还校验 Runtime、Ability、token、Instigator。
4. 进入 Windup，按 Timer/GameplayEvent 释放并进入 Active；每 Cast Behavior 维护独立点表，按声明派发 Script、Release、Timer、GameplayEvent 点，统一走 Begin/Resolve/Abort 防重。行为完成后进入 Recovery/终态；控制状态和受击规则经同一执行身份处理。
5. 提交键 `FSkillLabSubmissionKey` 保留稳定 Id、RequestId、Runtime、generation、Cast、点、operation 与 transaction；生产接受、投射物交付、效果应用和命中分别记录。能量预留/结算由 GAS 执行；唯一费用和冷却按已确认提交事实处理，不由 Lua 返回值替代。

## Current Behavior

目标形状为 `Self/Unit/Ground`，关系为 `Self/Ally/Enemy/Any`，效果为 `Damage/Healing/DamageOverTime`，执行模板为 `InstantTarget/Projectile/GroundArea`。当前四个生产技能均使用 `USkillLabLuaSkillBehavior`；没有显式 `submissionPoints` 时生成隐式 Script 点 1，并保持旧目标绑定。显式点 id 为正 int32，声明包含 trigger、target，Timer 配 delay，GameplayEvent 配 eventTag；Provider 校验重复、角色冲突和目标适配。完成模式为 `ScriptOrEvent` 或 `AllSubmissionPointsResolved`，后者等待点表收束。

含 GameplayEvent 提交点的定义必须配置正数 `active` 超时；缺省或零值由 `SkillLabSkillEntryParsing.cpp` 在加载期拒绝。释放后该等待由既有 `ActiveTimeout` 取消契约保障结束，不能依赖已清理的释放事件超时；无此类提交点的定义不因该校验被强制配置 Active 时长。

Lua 宿主仅提供受限 `SkillLabSkill_RequestAction(pointId)`、`SkillLabSkill_ApplyEffect(pointId)` 和 `SkillLabSkill_Finish()`。无参动作只兼容隐式单点；成功提交一次，明确失败不消耗该 Cast 的唯一费用机会。VM 按次创建和关闭、受指令预算限制，不是常驻协程；Lua 不能直接访问 Actor/ASC 或绕过声明执行。当前生产 JSON 未显式使用多点，多点能力由专门测试覆盖。

普通 Cast 结束清理其 Timer、检测和回调，未确认交付的投射物销毁；已确认投射物脱离 Cast 寿命。命中走 `ApplySubmitted`，按已确认交付记录、原来源快照及一次命中消费执行效果，不要求原 Cast 仍活动；只在原 Runtime 有效时向其转发结果。持续 GE 由目标 ASC 持有，Adapter 按句柄保存 Origin、周期与移除事件，普通 Cast 终止不移除；Runtime EndPlay 在活动 Cast 与提交收束后以 `Clear(false)` 解除效果观察，并通过 Loadout 保存的精确句柄撤销该 Runtime 授予的 Ability spec，不移除已交付 GE；Reset `Clear(true)` 显式清理。Actor/ASC 失效不调用失效对象。

`USkillLabUnconfirmedSubmissions` 保存会话生产结果、唯一冷却、迟到去重和未决 SkillId 隔离。有限等待结束仍未知的提交保留预留与隔离；迟到确认按原身份对账，不归到新 Cast，也不承诺跨进程恢复。`ActiveRecordIndices` 以稳定记录下标索引待结算或待反馈项，每轮对账只扫描活动索引快照，反馈成功后才移出；`BlocksSkill`、`HasUnconfirmed` 和失主诊断也只遍历活动项。`Records` 本体保持追加，交接时的重复身份检测仍扫描历史；终态生产结果、Area 父子结果及 GAS 的能源结算与已用事务 ID 历史继续保留，以支持迟到查询和幂等，当前没有会话内裁剪。

Reset 在清意图/取消前登记 attempt、原 ASC 与 generation，保持请求屏障，等待提交收束后更新 generation 并清理效果观察。若仍有未决，报告 `ResetBlockedByUnconfirmedSubmission` 并保留其预留/隔离；否则快照能量恢复目标、恢复 Health，再登记 Waiting 并调用 GAS `RequestResourceChangeTracked`。写前与最终回执均核对 attempt、原 Runtime/ASC、generation、阶段和未决；只有最终 Applied 且无未决才清已确认冷却。Rejected/Failed/Unavailable 映射为 `ResetResourceChangeRejected/Failed/Unavailable`；先释放屏障再发布失败。成功只记 `ResetCompleted` 日志，没有对外成功委托。EndPlay 使旧 attempt 失效；Health/效果清理与 Energy 恢复不是整体事务，没有补偿或自动重试。

死亡状态由 GAS 的 `USkillLabCombatAttributeSet::PostAttributeChange` 随 Health 最终值同步 `State_Dead` loose tag，覆盖 GE 伤害/治疗及 Reset 直接恢复 Health 的路径；Runtime 消费该状态执行死亡门控与取消，不另行手工清除死亡标签。

## Current Responsibilities And Evolution Boundaries

请求与共同仲裁优先查看 Requests/Rules，单 Cast 状态查看 Execution/Ownership 与 `USkillLabCastExecution`，点声明与调度分别查看解析文件和 `SkillLabSubmissionPointExecution.cpp`；不要把每 Cast 状态搬回角色级单例字段。资源写入和回执提供方见 [GAS 模块](SkillLabGAS.md)，表现身份见 [表现模块](CharacterPresentation.md)。新增执行模板、异步语义或联网要求时可调整归属，但需重新明确来源、寿命及验证边界。

## Verification

当前自动化入口及 B1/多 Cast、重入修复、R+Reset 的分版本通过证据见 [TestAndVerification](TestAndVerification.md)。本次文档同步只做静态核对，未构建或执行测试。

实际工程已有四技能定义加载和释放证据；若快捷键到达后出现 `definition missing`，先沿 Provider 加载日志及实际启动工程的 DLL 定位。源码已接受 `combatRules`，旧 DLL 报 `unknown catalog field 'combatRules'` 的交付问题需更新实际工程构建，不应删改生产配置适配旧版。具体证据见测试模块的实际工程交付记录。

## Current Limitations

现有证据覆盖限定的本地多 Cast、点调度和异步收束，不代表四技能已完成可见多段玩法或网络验证。沉默/硬直多来源主要由测试 GE 提供，未证明生产授予链；攻击类别也未证明所有生产来源。可见动作、生产 Probe 和资源资产检查的范围以测试模块为准。

## Extension Directions

新增技能优先复用统一定义、请求、点调度、效果与来源记录；新增提交或持续效果语义时补充相应寿命、迟到确认和失败收束验证。
