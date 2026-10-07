# SkillSystem 四层架构与公共接口

## 文档状态

- 任务：SkillSystem
- 产物类型：专题架构结论，细化 `ArchitectureBaseline.md`，不替换基线中的公共身份、结果语义和清理不变量。
- 状态：供后续实现、集成和验证 Agent 交接；本文件中的“当前冻结规则”可以作为实现约束，“本轮临时适配”只允许在注明的边界内存在。
- 范围：首阶段 `Skill_TestPulse` 与 `Skill_RecoverEnergy` 接入所需的四层边界、定义/请求/结果/事件/表现接口和迁移顺序。
- 非范围：不设计复杂目标、投射物、持续区域、网络复制、完整槽位 UI、C# 或外部脚本运行时；不修改 C++、Blueprint、配置或资产。

## 读取依据与当前事实

本专题依据以下资料建立：

- `ue/SkillLab/Docs/README.md`
- `ue/SkillLab/Docs/ProjectContext/README.md`
- `ue/SkillLab/Docs/ProjectContext/SkillLabCodeArchitecture.md`
- `ue/SkillLab/Docs/ProjectContext/Modules/GameplayFramework.md`
- `ue/SkillLab/Docs/ProjectContext/Modules/Input.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/README.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/README.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/Batches/Batch01-SixSkillVerticalSlice/README.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/Batches/Batch01-SixSkillVerticalSlice/CapabilityRequirements.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/M02-UnitAndAbilityFoundation.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/M03-AbilityRequestAndLifecycle.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/M04-TargetingAndHit.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/M05-AttributesEffectsStates.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/M07-PresentationAndUIFeedback.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Infrastructure/M09-ObservabilityAndContentProduction.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Skills/README.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/Skills/SkillDesigns.md`
- `ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/ArchitectureBaseline.md`
- `ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/ImplementationSlice.md`
- `ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/ImplementationAudit.md`
- 当前真实入口：`SkillLabSkillComponent.*`、`SkillLabPulseAbility.*`、`SkillLabPulseEffect.*`、`SkillLabAbilitySystemComponent.*`、`SkillLabCharacter.*`、`SkillLabPlayerController.*`。

当前已确认事实：`ASkillLabCharacter` 挂载 ASC、AttributeSet 和 SkillComponent；`BeginPlay` 通过 `InitializeUnit` 初始化 ASC、属性并遍历 Provider 的默认 SkillId 列表，逐项调用通用 `GrantSkill`。授予记录按 `SkillId` 保存业务 `GrantedAbilityId` 与 GAS `FGameplayAbilitySpecHandle`；`SubmitSkillRequest` 按请求 SkillId 查找授予记录并调用 GAS，不扫描特定 Ability class。定义版本、来源、执行模板、表现绑定、目标、资源/冷却和效果参数由独立 Provider 提供；请求成功后才分配 CastId、扣能量和设置本地冷却；效果通过统一入口返回结构化结果并发布阶段事件；`ResetUnit` 会取消能力、恢复属性和清除本地冷却。当前已接入 `Skill_TestPulse` 和 `Skill_RecoverEnergy` 两个不同效果语义。

当前未完成事实：Provider 仍是首阶段类型化 C++ 载体，未来可迁移到结构化资产；GAS 激活失败无副作用、运行时数值恢复、表现路由内部状态和交互式 PIE 仍未验证。本轮已加入 `Skill_RecoverEnergy` 并通过命令行入口检查其复用路径；复杂目标、持续效果、投射物、网络复制和完整 UI 不属于本阶段。

## 一、四层边界

四层是职责边界，不是四个必须对应的 C++ 类。它们必须通过明确的结构化接口连接，不能互相复制状态机。

### 1. 内容定义层（Definition）

**负责**：提供稳定的 `SkillId`、显示信息、目标需求、资源/冷却、阶段、执行模板、效果引用和表现绑定；提供版本、来源和启用状态；在进入运行前完成引用和范围校验。

**不负责**：创建施法实例、扣费、启动冷却、判定激活、查询命中、直接写属性、播放资源或发布伪造事件。

**首阶段载体**：允许使用独立的类型化 C++ 定义提供器承载 `Skill_TestPulse`，但必须是独立提供器/注册表边界。未来优先迁移到可编辑、可 diff、可校验的 `UDataAsset` 或等价结构化载体；不得以“以后迁移”为理由把常量继续放入通用生命周期。

**权威数据**：同一字段只允许一个权威来源。显示名、内容参数和表现引用由定义层拥有；运行时只读取提交时形成的定义快照。

### 2. 运行机制层（Runtime）

**负责**：单位身份和授予关系、统一请求入口、激活检查、GAS 接受闸门、`RequestId/CastId` 生命周期、目标最终检查、资源和冷却时序、效果统一应用、终态、清理、重置和结构化事件发布。

**不负责**：键位含义、槽位 UI、技能专用数值、具体表现资源选择、测试命令文本或 AI 的技能选择策略。

**技术归属**：GAS (`AbilitySystemComponent`、`GameplayAbility`、`GameplayEffect`、`AttributeSet`、Gameplay Tags) 承载引擎运行能力；SkillLab C++ 承载跨模块身份、请求/结果/事件、定义访问、目标适配、重置协调和可观察错误。最终类名不在本专题冻结。

### 3. 测试/调用适配层（Invocation）

**负责**：将 Player、AI、Test、批量试放或控制台输入转换成统一 `SkillRequest`；提供请求结果给调用方；按授权调用统一重置接口；保留来源和输入上下文。

**不负责**：查资源、查冷却、创建 `CastId`、直接调用指定 Ability、构造 GameplayEffect、写 AttributeSet 或解释表现事件。

**首阶段适配**：`SkillTest` 可保留为控制台命令，但只填写 `Source=Test`、施法者、定义快照和自身目标后提交 `SubmitSkillRequest`；`SkillReset` 只调用统一 `ResetUnit`。测试命令不能绕过统一请求入口。

### 4. 表现适配层（Presentation）

**负责**：订阅运行事件，按 `SkillId`/事件类型读取绑定并播放动画、Niagara、音频、镜头和 UI；追踪表现实例；收到取消、打断、死亡、重置和对象销毁事件时停止或清理；记录缺失资源诊断。

**不负责**：重新判断技能是否可用、扣费、命中、效果数值、冷却或施法状态机；不把预览候选当作正式命中；缺失表现资源不能阻止运行机制完成。

**首阶段最小接入口**：可以先由日志观察适配器或最小 `PresentationRouter` 接收结构化事件，不要求一次实现完整 UI。即使暂时没有资源，事件对象和清理语义仍必须存在。

## 二、层间依赖和单向数据流

```text
DefinitionProvider ──读取──> Runtime
Invocation ──提交 SkillRequest──> Runtime
Runtime ──发布 SkillResult/EffectResult/SkillEvent──> Presentation、Observation
Presentation ──只读事件和定义绑定──> 播放/清理表现
```

运行时不能回调测试适配层来决定规则；表现层不能回写运行状态；定义层不能持有 `CastId` 或单位临时状态。所有入口（Player、AI、Test）都必须经过同一 `SubmitSkillRequest`，所有效果都必须经过同一 `ApplyEffect` 语义入口。

## 三、统一定义接口

以下是语义接口，名称是候选名称，不冻结最终 C++ 类名。

### `SkillDefinitionProvider`

```text
FindDefinition(SkillId, DefinitionVersion?) -> DefinitionLookup
ValidateDefinition(SkillId) -> DefinitionValidationResult
GetPresentationBinding(SkillId, EventType) -> PresentationBindingLookup
```

`SkillDefinition` 至少包含：

```text
SkillId, Version, Enabled
TargetType / TargetRelation / TargetPolicy
Cost(ResourceType, Amount), Cooldown
StagePolicy(Windup/Active/Recovery/Cancel/Interrupt)
ExecutionTemplate
EffectRefs + 应用阶段
PresentationBindingId
Source / ContentRevision
```

读取结果必须区分 `Missing`、`Disabled`、`Invalid`、`Available`。提交请求时复制 `SkillId`、版本或等价快照；施法开始后不得因槽位替换或定义热更改写正在运行的 `CastId`。

### 临时 C++ 定义提供器约束

- `Skill_TestPulse`、`Skill_RecoverEnergy` 及其参数只能出现在独立定义提供器/样例定义条目、各自执行载体或测试适配器中。
- 通用请求、激活、扣费、冷却、目标、效果和清理流程不得出现上述技能专用 ID、数值或分支。
- 提供器必须可替换：未来迁移到 DataAsset/结构化配置时，只替换加载和校验实现，不改请求、结果、事件和表现接口。

## 四、统一请求接口

### `SkillRequest`

```text
RequestId                 // 每次提交尝试唯一；拒绝也保留
Source                    // Player / AI / Test
CasterUnitId + CasterObject
SkillId + DefinitionSnapshot/Version
TargetSpec                // 首阶段至少 Self；保留 Unit/位置/方向扩展位
LoadoutVersion?           // 有装配时复制，不在运行中回读槽位
RequestTime / TraceData
```

### `SubmitSkillRequest(Request)`

调用顺序固定为：

1. 校验请求结构、施法者、单位初始化、死亡/销毁和技能定义。
2. 校验授予关系、状态标签、资源、冷却和目标最终合法性。
3. 任一检查失败返回 `Rejected`，不得创建 `CastId`、扣费、启动冷却或应用效果。
4. 调用 GAS 激活；只有 GAS 接受激活后才进入 `Started`，创建 `CastId` 并执行定义声明的扣费/冷却时点。
5. 用同一 `CastId` 推进目标解析、效果应用、阶段事件和终结清理。

`TryActivateAbility` 返回成功是“已开始”的唯一闸门。同步 Ability 必须在应用效果前取得该次 `CastId`；激活失败或上下文缺失必须返回失败结果并保持资源、冷却和实例身份不变。

Unit 目标必须解析到已初始化且 `UnitId != 0` 的 `USkillLabSkillComponent`；若请求携带 `TargetUnitId`，必须与解析 ID 一致。无 SkillComponent、未初始化或零 ID 返回 `Rejected(TargetUnitIdUnavailable)`，不得继续效果。事件携带请求形成的 `TargetLocation/TargetDirection` 快照，表现层只读快照。

## 五、统一结果接口

### `SkillRequestResult`

```text
RequestId, UnitId, SkillId, Source, TargetSpec
Status: Rejected | Buffered | Started | Completed | Cancelled | Interrupted | Expired
ReasonCode, Diagnostic
CastId?                  // 只有 Started 及其后续终态可有
GrantedAbilityId        // 有效授予记录可解析时保留；未授予或无法解析时为 0
DefinitionVersion?
bEffectSucceeded        // Completed 只表示生命周期结束
```

稳定 `ReasonCode` 至少包括：`NotInitialized`、`InvalidCaster`、`SkillNotGranted`、`InvalidTarget`、`StateBlocked`、`InsufficientResource`、`Cooldown`、`ConfigurationError`、`ActivationFailed`、`ActivationContextMissing`、`Cancelled`、`Interrupted`、`Expired`、`EffectFailed`。

### `EffectApplicationResult`

```text
EffectApplicationId / EffectId
CastId, SkillId, SourceUnitId, TargetUnitId
EffectType, RequestedMagnitude, AppliedMagnitude
Status: Applied | Immune | InvalidTarget | Blocked | Failed
ReasonCode, WorldTime
```

`Applied` 表示实际修改已经发生；命中候选、目标解析成功或 GAS Spec 创建成功都不能代替 `Applied`。瞬时效果也必须产生结果对象；持续/周期效果还要记录可移除句柄、周期序号和归属。

首阶段效果失败仍可返回 `Status=Completed`，此时 `bEffectSucceeded=false` 且 `EffectResult.Status=Failed`；`Completed` 不等价于效果成功。

### 终态不变量

- 一个 `RequestId` 只能有一个对外终结结果。
- 一个 `CastId` 只能有一次 `Completed/Cancelled/Interrupted` 和一次清理完成通知。
- 清理可重复调用，但不得重复扣费、启动冷却、命中、应用效果或播放终止事件。
- 已经发生的即时效果默认不因取消、打断或重置回滚；持续效果按定义的归属清理。

## 六、统一事件接口

### `SkillEvent`

事件是运行事实，不是拼接字符串。每个事件至少包含：

```text
EventId, EventType
UnitId, SkillId, Source
RequestId, CastId?
Stage, WorldTime
TargetSpec / TargetUnitId?, Location/Direction?
Result, ReasonCode
EffectId? / ObjectId?
```

最小事件类型：

- 单位：`Initialized`、`Granted`、`Removed`、`ResetStarted`、`ResetCompleted`。
- 请求/生命周期：`RequestReceived`、`ActivationChecked`、`Started`、`StageChanged`、`Cancelled`、`Interrupted`、`Completed`。
- 目标/效果：`TargetResolved`、`HitConfirmed`、`EffectApplied`、`EffectRejected`、`StatusAdded`、`StatusRemoved`。
- 资源/冷却：`ResourceChanged`、`CooldownStarted`、`CooldownEnded`。
- 清理/表现：`ObjectCreated`、`ObjectDestroyed`、`PresentationCleanupRequested`。

### `SkillEventSink`

```text
Publish(Event)                  // 运行机制唯一事实出口
Subscribe(Subscriber)
Unsubscribe(Subscriber)
```

运行模块发布一次事实；日志、测试断言和表现路由器都是订阅者/观察适配器，不得通过解析日志重建第二套状态。事件发布失败不得改变已经完成的逻辑结果，但必须有诊断记录。

## 七、统一表现接口

### `PresentationRouter`

```text
HandleSkillEvent(SkillEvent)
ResolveBinding(SkillId, EventType) -> PresentationBinding?
StopByCast(CastId, StopReason)
StopByObject(ObjectId, StopReason)
ResetPresentation(UnitId, ResetReason)
```

表现绑定至少描述 `EventType`、资源类型/引用、挂点或位置规则、是否循环、停止事件、`PreviewOnly`、优先级和缺失资源策略。表现实例必须保存 `EventId` 与 `CastId`/`ObjectId`；取消、打断、死亡和重置按归属停止持续表现。瞬时表现按自身生命周期结束。

表现适配器只读事件和定义绑定：

- 收到 `Started` 播放释放/前摇表现；
- 收到 `EffectApplied` 播放命中/效果表现；
- 收到 `Completed` 播放结束表现；
- 收到 `Cancelled`、`Interrupted`、`ResetCompleted` 停止对应持续实例；
- 资源缺失时按策略静默、占位或记录诊断，不阻塞 `SkillResult`。

## 八、重置与所有权边界

`ResetUnit(ResetReason)` 由运行机制统一协调，推荐顺序：

```text
阻止新请求
→ 终结活跃 CastId
→ 清除目标预览/缓冲
→ 移除本轮效果和状态
→ 清除冷却/临时授予/临时对象
→ 恢复属性与资源初值
→ 恢复默认能力/装配
→ 发布 ResetCompleted
```

所有权固定为：单位层拥有 `UnitId`、授予记录和初始属性快照；运行层拥有 `RequestId/CastId`、阶段、结果和清理协调；目标层拥有解析/命中记录；效果/GAS 拥有效果句柄和状态；执行对象拥有自己的 `ObjectId` 并回指 `CastId`；表现和日志只拥有订阅/表现实例。

## 九、从当前实现迁移的约束

迁移必须按接口先行、样例后迁移的顺序执行，不得通过放宽基线保留偏离。

### 阶段 A：建立接口壳

- 增加独立定义提供器和定义校验入口，提供 `Skill_TestPulse` 与 `Skill_RecoverEnergy`。
- 建立通用 `SkillRequest`、`SkillRequestResult`、`EffectApplicationResult`、`SkillEvent` 语义结构。
- 建立事件出口和最小表现订阅接口；日志改为订阅事件的观察适配器。

### 阶段 B：请求链迁移记录

- 已用通用 `SubmitSkillRequest` 替代技能专用公共入口。
- `SkillTest` 只构造 Test 请求；Player/AI 入口以后复用同一路径。
- `FSkillLabRequestResult` 可作为过渡外壳，但必须映射到统一结果字段；不得继续增加 Pulse 专用字段。

### 阶段 C：迁移 GAS 载体

- `USkillLabPulseAbility` 保留为首阶段执行载体时，只负责 GAS 接受、执行回调和结束；不得拥有 Pulse 数值、目标规则或通用扣费分支。
- `USkillLabPulseEffect` 保留为瞬时效果载体时，参数来自定义/效果描述；应用后返回 `EffectApplicationResult` 并发布 `EffectApplied/EffectRejected`。
- `CastId` 必须在 GAS 接受后、效果应用前可用，并贯通 Ability、Effect、结果和事件。

### 阶段 D：迁移表现与重置

- `ResetUnit` 通过事件出口发出 `ResetStarted`/终止事件/`ResetCompleted`，表现路由器按 `CastId` 清理。
- 控制器不再直接知道技能专用函数；保留 `SkillReset` 只作为统一重置命令适配。
- 完成 PIE 证据：成功链、冷却拒绝无副作用、激活失败无副作用、重置后再次施放、表现清理。

### 禁止出现技能专用内容的位置

以下位置禁止出现 `Skill_TestPulse`、`PulseCost`、`PulseCooldown`、`TestValue +1` 或 `TargetUnsupportedInFirstSlice` 等样例常量/分支：

- 通用请求、结果、事件结构；
- 通用激活、扣费、冷却、目标最终检查、效果应用和清理流程；
- `ASkillLabCharacter`、`ASkillLabPlayerController` 的业务规则；
- 通用表现路由和日志格式。

允许出现的位置只有：独立定义提供器的样例条目、测试调用适配器的默认请求、样例专用执行载体的定义读取代码，以及对应的验证/诊断文本。

## 十、实现约束清单

### 当前冻结规则

- 四层职责和单向依赖必须保持。
- Player、AI、Test 共享同一请求入口和激活/效果链。
- `SkillId`、来源、目标、`RequestId`、`CastId`、效果身份和事件身份可追踪。
- GAS 接受激活是创建 `CastId`、扣费、启动冷却和发布 `Started` 的闸门；失败无副作用。
- 效果必须通过统一效果入口并返回结构化结果；事件是事实出口，日志/表现只订阅。
- 取消、打断、死亡和重置必须有事件并可幂等清理表现和运行状态。
- Blueprint 只做组装、绑定、资源引用和调用转发，不能复制状态机。

### 本轮临时适配

- 允许独立 C++ 定义提供器临时承载 `Skill_TestPulse`。
- 允许 `SkillTest`、`SkillReset` 控制台命令继续存在，但只能形成统一请求/重置调用。
- 允许 Pulse Ability/Effect 作为 GAS 载体，前提是样例参数从定义/效果接口读取。
- 允许首阶段只实现 Self 目标、瞬时效果和最小表现事件，不提前实现复杂模板。

### 未来候选/待用户判断

- 定义载体最终选择 `UDataAsset`、DataTable 或其他结构化源。
- 表现路由器的具体组件归属、网络策略和资源缺失默认策略。
- 死亡时持续效果、投射物和区域的存续规则。
- 输入缓冲、槽位版本和多单位目标的具体体验规则。

## 十一、策划输入的程序候选映射（未冻结）

原策划配套材料中的技术映射、配置语义和运行交接在本节保留为程序侧候选输入。它们不是策划约束，也不替换本文件已有的基线规则；实现前由程序 Agent 根据当前代码、UE 版本和可验证结果逐项确认。

| 业务需求 | 程序候选关注点 |
| --- | --- |
| 稳定单位/技能/请求/施法/效果/对象身份 | 统一请求结果、GAS 上下文、对象归属和日志字段的实际映射 |
| 技能内容表达目标、阶段、空间、效果和表现 | 定义载体、引用解析、范围校验、版本与来源追踪 |
| 玩家、AI、Test 共用规则 | 单一请求入口、会话动作、批量 Reset 和可复现运行记录 |
| 弹体、区域、护盾和持续状态独立存续 | Cast/Object/Effect 生命周期承载、终止通知和跨单位清理 |
| 预览、命中、实际值、失败原因可见 | 事件事实出口、表现订阅、诊断与缺失资源回退 |

技术候选的接口名、类名、数据载体和字段名不由策划层冻结；如果实现采用不同映射，只要保持公共结果语义、可追踪身份、幂等清理和可验证证据即可。六技能的业务参数从 `Planning/SkillSystem/Skills/SkillDesigns.md` 索引进入独立正文，公共能力需求读取 `Planning/SkillSystem/Infrastructure/`。

## 十二、验收行为与交接路由

实现 Agent 必须先读本文件第 1～6 节和 `ArchitectureBaseline.md` 第 3～10 节；若涉及表现，再读第 7 节及 Infrastructure M07；若涉及定义载体，再读第 3 节和本文件第 11 节；若涉及重置，再读第 8 节及 Infrastructure M02。

最小验收矩阵：

| 场景 | 必须观察的结果 |
| --- | --- |
| 有效 `Skill_TestPulse` 请求 | `RequestReceived → ActivationChecked → Started → TargetResolved → EffectApplied → Completed`，同一 `CastId` 贯通 |
| 冷却中快速重复请求 | `Rejected(Cooldown)`，无新 `CastId`、无扣费、无新冷却、无效果 |
| GAS 激活失败/上下文缺失 | `Rejected(ActivationFailed/ActivationContextMissing)`，无资源和冷却副作用 |
| `SkillReset` | 活跃实例终结、表现清理、资源/属性/冷却恢复、`ResetCompleted`；重复调用幂等 |
| 重置后再次请求 | 新 `RequestId`、新 `CastId`、新效果结果，链路与第一次独立 |
| 缺失表现绑定 | 逻辑结果照常完成，并产生可定位诊断 |

本专题是局部 SkillSystem 架构结论，会被后续基础设施与技能实现任务引用；它不改变全局 `ArchitectureBaseline.md` 的版本。若实现必须改变公共身份、结果、事件或清理不变量，应先更新基线版本并记录被替换内容，再开始代码迁移。
