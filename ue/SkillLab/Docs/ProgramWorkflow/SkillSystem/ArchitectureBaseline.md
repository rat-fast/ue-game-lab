# SkillSystem 架构基线

- 版本：v0.2（首阶段最小纵向闭环，GAS 已接入后的交接修订）
- 任务：SkillSystem
- 状态：供首批实现、集成和验证使用；不是完整技能系统终稿
- 目标链：单位初始化 → 获得一个能力 → 发起一次技能请求 → 激活检查 → 一个最简单目标 → 一个最简单效果 → 技能结束 → 清理和重置 → 再次施放

## 1. 基线依据与范围

本基线依据以下当前有效资料建立：

- `ue/SkillLab/Docs/README.md`
- `ue/SkillLab/Docs/ProjectContext/README.md`
- `ue/SkillLab/Docs/ProjectContext/SkillLabCodeArchitecture.md`
- `ue/SkillLab/Docs/ProjectContext/Modules/GameplayFramework.md`
- `ue/SkillLab/Docs/ProjectContext/Modules/Input.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/README.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/01-SystemScope.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/02-CapabilityModel.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/02-Modules/M01-InputAndSkillLoadout.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/02-Modules/M02-UnitAndAbilityFoundation.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/02-Modules/M03-AbilityRequestAndLifecycle.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/02-Modules/M04-TargetingAndHit.md`
- `ue/SkillLab/Docs/Planning/SkillSystem/02-Modules/M05-AttributesEffectsStates.md`
- `ue/SkillLab/Docs/ProgramWorkflow/SkillSystem/README.md`
- 当前真实代码：`Source/SkillLab/SkillLabCharacter.*`、`SkillLabPlayerController.*`、`SkillLabSkillComponent.*`、`SkillLabAbilitySystemComponent.*`、`SkillLabAttributeSet.*`、`SkillLabPulseAbility.*`、`SkillLabPulseEffect.*`、`SkillLab.Build.cs`、`SkillLab.uproject`

当前项目事实：基础运行入口仍是 `GameMode → PlayerController → Character → CharacterMovementComponent`；Character 保留移动和镜头职责，并已挂载 `USkillLabAbilitySystemComponent` 与 `USkillLabAttributeSet`，实现 `IAbilitySystemInterface`。`SkillLab.Build.cs` 已依赖 `GameplayAbilities`、`GameplayTags`、`GameplayTasks`，`.uproject` 已启用 `GameplayAbilities` 插件。`USkillLabSkillComponent` 已完成最小单位初始化、Provider 默认 SkillId 列表遍历、按 SkillId 保存 AbilitySpecHandle 的通用授予、请求门控、GAS 激活、瞬时效果、控制台测试入口和重置。因此，“GAS 是否接入”不再是首阶段前置条件；当前剩余工作是扩大定义/装配覆盖并补充运行证据。

首阶段只实现能够重复验证的最小样本：一个可授予的技能、一个有效施法者、一个简单目标、一个瞬时效果和显式重置。复杂目标预览、投射物、区域、持续引导、多段、复杂属性公式、完整槽位 UI 和网络同步属于后续扩展；接口应保留扩展位置，但不为它们提前建立过度通用框架。

## 2. 架构目标与模块边界

### 2.1 领域职责

| 职责 | 首阶段承载 | 不负责 |
| --- | --- | --- |
| 战斗单位与能力基础（M02） | 单位身份、阵营、属性/资源容器、能力系统归属、初始化、授予、死亡/重置通知 | 输入解释、命中查询、效果公式 |
| 技能请求与生命周期（M03） | 请求统一入口、激活检查、实例阶段、资源/冷却时点、结束与清理 | UI、空间查询、数值结算 |
| 目标与命中（M04） | 首阶段的自身/指定单位目标解析和一次统一合法性检查 | 输入、资源、效果应用 |
| 属性/效果/状态（M05） | 一个可观察的瞬时效果，经过统一应用入口并关联来源 | 目标选择、技能阶段推进 |
| 输入/装配（M01） | 首阶段可由测试调用或最小按键适配产生请求；槽位 UI 可延后 | 重新实现激活判定和效果规则 |
| 观测/测试（M09/M08） | 请求结果、阶段、效果、重置日志与可重复调用入口 | 代替业务模块拥有运行状态 |

首阶段允许测试入口直接提交技能标识和目标，绕过完整 UI 预览；该入口必须与玩家入口共用同一请求、激活和效果路径。

### 2.2 运行层分工

- **引擎/GAS**：`AbilitySystemComponent`、`GameplayAbility`、`GameplayEffect`、`AttributeSet`、Gameplay Tags、激活句柄和必要的 Effect Context。
- **SkillLab C++**：单位组件/接口、技能定义读取、统一请求与结果、目标解析适配、实例归属、重置协调、日志与测试入口。具体 C++ 类名由实现 Agent 根据现有代码和 UE 版本确定，不在本基线提前冻结。
- **配置/结构化内容**：技能身份、显示名（可选）、资源与冷却初始值、目标类型、效果参数、默认授予模板。首阶段可以用项目内 Data Asset 或等价可 diff 的数据载体；载体选择是实现决策，语义字段不得散落在 Character/Controller 代码中。
- **Blueprint**：已有 GameMode/Character/PlayerController 外壳、默认类关系、组件组装、Data Asset/输入/表现资源引用和必要的测试场景接线。Blueprint 不拥有跨模块激活状态机、扣费、冷却、命中合法性或效果公式。

## 3. 稳定身份模型

必须分开以下身份，日志和请求结果都可追踪到它们：

| 身份 | 生命周期 | 作用 |
| --- | --- | --- |
| 单位身份 `UnitId` | 单位生成至销毁；重置时保持或按实现重新生成，但同一轮测试内稳定 | 施法者、目标、阵营和属性归属 |
| 技能定义身份 `SkillId` | 内容资产/配置生命周期 | 可复用技能内容，不代表一次运行 |
| 能力授予身份 `GrantedAbilityId` | 授予至移除 | 表示单位当前拥有的可激活能力；关联来源和装配（若有） |
| 请求身份 `RequestId` | 请求创建至结果终结 | 关联一次调用尝试，拒绝也应有可读结果 |
| 施法实例身份 `CastId` | 成功开始至完成/取消/打断/重置清理 | 统一归属阶段、命中、效果、临时对象和结束原因 |
| 效果应用身份 `EffectApplicationId` | 效果应用记录生命周期 | 关联施法、来源技能、施法者、目标和结果 |

槽位身份（如果首阶段接入槽位）只负责“当前位置装配了哪个 `SkillId`”；提交请求时必须复制解析出的技能身份和装配版本/等价快照，不能在运行中重新读取槽位而改调另一个技能。移除或替换能力不会改写已经开始的 `CastId`。

## 4. 最小生命周期与状态机

### 4.1 单位初始化

```text
单位生成
 → 建立 UnitId/阵营
 → 创建或绑定 ASC 与属性环境
 → 初始化基础属性、资源和标签
 → 授予最小技能
 → 记录初始化完成
```

初始化必须幂等：重复调用不能重复授予同一能力、重复添加初始化效果或叠加属性。若实现选择显式拒绝重复初始化，必须返回可观察错误并保持第一次结果有效。

### 4.2 请求到施法实例

```text
Submit(Request)
 → 校验施法者/技能身份/授予关系
 → 检查死亡、标签门控、资源、冷却和目标
 → Rejected（未开始）或尝试 GAS 激活
 → GAS 接受激活后创建 CastId、扣除首阶段资源并启动冷却
 → ResolveTarget → ApplyEffect
 → Completed 或 Interrupted/Cancelled
 → 幂等清理
```

首阶段不要求缓冲；请求结果仍保留 `Buffered` 扩展值。缓冲未启用时，条件不满足直接 `Rejected`，不得创建伪施法实例。

### 4.3 实例状态不变量

- 只有通过全部激活检查后才能从请求进入“已开始”；
- GAS 的“接受激活”是“已开始”的唯一闸门；在此之前不得创建 `CastId`、扣费或启动冷却。对于同步执行的 `GameplayAbility`，应在 Ability 入口取得/提交请求上下文后再应用效果，不能等效果已应用后才补造身份。激活失败必须保持资源、冷却和施法身份不变；
- `CastId` 一旦创建，必须保存施法者和 `SkillId` 快照，直到终结；
- 每个 `CastId` 只产生一次终结结果和一次清理完成通知；
- 结束清理可重复调用，不得重复扣费、重复命中或重复应用效果；
- 请求拒绝不触发效果，不启动技能实例；
- 预览取消（若有）不产生正式效果；
- 死亡、销毁或场景重置禁止新请求，并向活跃实例发出终结信号；
- 资源、冷却、效果应用和临时对象都能回溯到技能/施法实例归属；
- 目标在解析后仍须经过最终合法性检查，不能把预览或测试输入视为命中保证。

## 5. 公共接口语义（候选名称，不冻结 C++ 类名）

### 5.1 单位能力接口

- `InitializeUnit(UnitInitData)`：建立身份、阵营、ASC/属性环境并应用初始模板；重复调用返回 `AlreadyInitialized` 或保持幂等成功。
- `GrantSkill(SkillId, GrantSource)`：授予可激活能力，返回授予身份/句柄；重复授予按配置校验，不静默生成重复能力。
- `RemoveSkill(SkillId or GrantedAbilityId)`：使未开始请求失效；活动实例交由生命周期模块按规则终结；返回真实移除结果。
- `ResetUnit(ResetReason)`：禁止新请求，终结活动实例，清除本模块拥有的效果/状态/冷却/临时授予，恢复初始属性和技能模板，发布完成事件。
- `CanAcceptSkillRequest()`：至少反映单位是否有效、已初始化、未死亡且未处于全局禁止状态。

### 5.2 技能请求接口

请求至少包含：

```text
RequestId
Source（Player/AI/Test）
CasterUnitId / CasterObject
SkillId（提交时快照）
TargetSpec（Self/Unit/位置/方向；首阶段至少 Self 或 Unit）
RequestTime 或等价追踪信息
LoadoutVersion（如果经槽位装配）
TargetId/TargetSpec（当前样本为自身目标；来源和目标语义不能只藏在日志文本中）
```

`SubmitSkillRequest(Request)` 返回即时结果对象，至少包括 `RequestId`、状态、原因、可选 `CastId`、`GrantedAbilityId`、技能身份、TargetUnitId/TargetSpec 快照和可读诊断信息。请求若已解析到有效授予记录，即使随后在目标、资源、冷却或 GAS 激活门控被拒绝，其结果和事件仍保留真实业务 `GrantedAbilityId`；技能未授予或身份无法解析时为 `0`。成功链事件也携带该业务身份；后续施法阶段通过 `CastId` 关联结果事件。

### 5.3 请求结果

对外稳定区分：

- `Rejected`：未开始；原因至少能区分无效施法者/未初始化、未授予、死亡/状态阻塞、资源不足、冷却中、目标无效和配置错误。
- `Buffered`：首阶段默认关闭，保留扩展位。
- `Started`：激活成功并创建 `CastId`；不等于已经命中。
- `Completed`：生命周期达到结束条件，不代表效果一定成功；通过 `bEffectSucceeded` 和 `EffectResult.Status` 读取效果结果。
- `Cancelled`：已开始后被明确取消。
- `Interrupted`：已开始后因死亡、重置或其他中断终结。
- `Expired`：缓冲/目标上下文过期时使用；首阶段可不产生。

效果应用还应区分 `Applied`、`Immune`、`InvalidTarget`、`Failed` 等结果，避免把“命中候选存在”冒充“效果已生效”。Unit 目标必须解析到已初始化且 `UnitId != 0` 的 `USkillLabSkillComponent`；无组件、未初始化或零 ID 都返回 `Rejected(TargetUnitIdUnavailable)`。

## 6. 首阶段最小样本默认值

以下是为可重复 PIE/测试闭环提出的 AI 建议默认值，除非用户后续修改，不应被实现 Agent 误写成永久策划规则：

- 一个玩家战斗单位，使用当前 `ASkillLabCharacter` 作为 Avatar 外壳；ASC/单位能力组件独立承载能力逻辑。
- 两个可授予技能：`Skill_TestPulse`（自身 `TestValue +1`）和 `Skill_RecoverEnergy`（自身 `SkillEnergy +20`）；二者均通过同一请求、授予、CastId、效果结果、事件和表现路由。
- 技能为瞬发、无前摇、无持续阶段；激活成功即扣除少量资源并启动短冷却（建议资源 10、冷却 0.5 秒，具体属性名/数值由实现与样本决定）。
- 效果为一次可观察的固定属性变化（建议对目标施加 `+1` 测试值或对自身生命/测试属性施加可回退修改），必须经过统一效果应用入口并写入日志。
- 首阶段可提供一个受控测试调用（BeginPlay 后自动试放、控制台命令或开发按键三者择一），并在同一流程末尾执行 `ResetUnit`，再提交第二次请求；最终证据需要能证明两次均独立完成，没有残留冷却/标签/效果。
- 为保证可观察性，至少记录 `Initialized → Granted → RequestStarted → ActivationChecked → TargetResolved → EffectApplied → Completed → ResetCompleted`，失败时记录阶段和原因。
- 首阶段不启用输入缓冲、复杂槽位换装、投射物、持续区域、多段、网络复制或完整 UI；这些不应阻塞最小闭环。

若自动试放影响现有导航测试，改为开发按键/控制台命令；调用入口必须仍然走统一请求接口。

## 7. 清理、重置与所有权

- **单位模块**拥有单位身份、能力授予记录、初始化句柄和属性初始值快照。
- **生命周期模块**拥有施法实例状态、阶段计时器、结果和清理协调。
- **目标模块**拥有解析结果和命中去重状态；不得持有技能效果。
- **效果模块/GAS**拥有已应用效果、效果句柄和状态标签；清理通过统一移除入口执行。
- **执行载体**（未来投射物/区域）必须持有 `CastId` 归属；技能槽位变化不能改变其归属。
- **表现/UI/日志**只订阅事实，不拥有能力、效果或临时对象。

`ResetUnit` 顺序建议为：阻止新请求 → 终结 `CastId` → 清理目标预览/缓冲 → 移除本轮效果和状态 →清除冷却/临时授予 → 恢复属性与初始标签 → 重新确认默认能力/装配 → 发布 `ResetCompleted`。各步骤须幂等；若某一步失败必须记录具体拥有者和残留对象。

## 8. 配置与 Blueprint 边界

### 可配置内容

技能身份、目标类型、基础资源/冷却、效果强度/类型、默认授予模板、阵营和初始属性、开发测试入口开关、表现资源引用。

### C++ 固定规则

请求结果和错误语义、激活顺序、资源/冷却归属、目标最终检查、效果统一入口、实例身份、清理/重置顺序、日志字段和幂等约束。

### Blueprint 允许范围

继续使用现有 GameMode/Character/PlayerController Blueprint 作为默认类与资源外壳；组装单位能力组件、指定默认数据资产、绑定输入和调试表现。Blueprint 可以触发测试调用，但不能在图表中复制激活检查、扣费、冷却、命中或清理状态机。

### 配置载体建议

首批优先使用可被 UE 编辑器读取、可引用资产、可差异比较的 `UDataAsset`/等价结构化载体。不要为了首阶段引入 DSL 或外部脚本；若实现发现载体对批量生产有明显限制，另建专题结论，不在代码中硬编码技能数值。

## 9. 与现有项目接入点

- `ASkillLabCharacter`：保留移动/镜头职责，增加能力系统挂接点或组件引用，不把技能规则直接堆入 Character。
- `ASkillLabPlayerController`：继续解释现有移动/镜头输入；首阶段若接开发按键，只负责形成 `Source=Player/Test` 请求并调用公共入口，不能自行判断资源/冷却。
- `BP_SkillLabGameMode`、`BP_SkillLabCharacter`、`BP_SkillLabPlayerController`：负责默认类关系、组件/数据/输入/表现资源绑定。
- `SkillLab.Build.cs` 与 `.uproject`：已声明 `GameplayAbilities`、`GameplayTags`、`GameplayTasks` 模块依赖并启用 `GameplayAbilities` 插件；后续只需继续验证 PIE 运行时行为，不应再把 GAS 接入本身列为首阶段阻塞项。
- `Content/SkillLab`：放项目自有技能数据、测试地图/单位和可选表现资源；本基线不要求新增资产才能开始 C++ 接口实现。

## 10. 风险与待用户判断

### 不阻塞首阶段实现的程序风险

- 当前 `ASC` Owner/Avatar 均使用单位 Actor，且编译已通过；仍需由运行验证确认 PIE 中的生命周期和重置结果。
- 当前只有一个默认 Character 和一个测试地图；首阶段继续使用自身目标，不把第二单位、多单位目标或复杂目标变成阻塞项。
- 当前 `Skill_TestPulse`、能量/冷却数值、`TestValue +1` 效果和控制台入口仍是代码内临时适配，必须保留可追踪身份和统一请求路径，但不应被当作最终内容配置方案。
- 当前 `Skill_RecoverEnergy` 已作为第二个无资产瞬时样本接入；其命令行复用证据已记录，PIE、激活失败和运行时恢复数值仍未验证。

### 需要总控/用户判断的体验问题

- 首批样本使用自身目标还是增加第二个可受击单位；建议先自身目标以最快证明完整闭环。
- 资源与冷却的最终数值、效果表现方式和场景重置是否保留装配；本基线给出的是可试玩默认值。
- 死亡时投射物/持续效果的存续、持续技能扣费与冷却、槽位换装和完整预览交互；它们不阻塞瞬发最小闭环。

### 影响范围

这是 SkillSystem 首阶段全局架构基线，会被 M02/M03/M04/M05 实现和集成任务引用。它冻结的是公共身份、结果语义、生命周期和职责边界；不会冻结最终 C++ 类名、资产文件名、完整配置 schema 或后续高级技能规则。若实现证明需要改变公共接口，必须更新版本并记录替换内容。

## 11. 建议实现顺序与进入 PIE 条件

建议实现顺序：

1. 保持现有 GAS 模块/插件接入，确认单位能力归属和最小属性环境。
2. 修正统一请求结果与最小追踪字段，至少贯通 `UnitId/SkillId/RequestId/CastId` 以及来源/目标语义。
3. 将 GAS 激活结果置于施法开始闸门之前；成功后才创建 `CastId`、扣费和启动冷却，并保持失败回滚/无副作用。
4. 保持一个开发测试调用，完成一次施放、结束、`ResetUnit` 和再次施放。
5. 再将调用接入 PlayerController 或最小 Blueprint 外壳，确保现有右键移动链不回归。

进入 PIE 前必须满足：

- 项目可编译，GameplayAbilities 依赖和插件状态明确；
- 默认单位初始化一次且日志可见，能力只授予一次；
- 第一次请求可观察到 `Started → EffectApplied → Completed`；
- 重置后属性/资源/冷却/标签/临时对象恢复初始状态；
- 第二次请求再次完成，且 `CastId`、效果和日志均为新实例；
- 请求失败至少有一条可复现门控证据（例如冷却中或未授予），证明激活检查确实生效；
- 若 PIE 受环境限制无法完成，必须在 `ProgramWorkflow/SkillSystem/Validation/` 留下编译、运行入口、失败输出和下一步。

## 12. 后续专题交接

### 给首阶段实现 Agent 的最小接口与不变量

- 请求入口保持单一路径（当前可由 `SkillTest` 调用），输入至少带 `Source`、`Caster/UnitId`、`SkillId`、`TargetSpec` 和 `RequestId`；返回状态、原因、`SkillId`、`RequestId`，成功开始后再返回 `CastId`。
- `UnitId` 在本轮测试内稳定；`SkillId` 固定标识当前样本；`RequestId` 对每次尝试递增且拒绝也可追踪；`CastId` 只为成功激活创建，并贯通结束、效果和清理日志。
- GAS 接受激活前不得扣费、启动冷却或发布 `Started`；失败不得留下 `CastId`、冷却或资源变化。成功后扣费与冷却只发生一次；同步 Ability 必须在效果应用前取得该次 `CastId`。
- 来源至少区分 `Player`、`AI`、`Test` 语义；目标至少记录 `Self`/目标身份语义。当前自身目标和控制台入口是临时适配，不得改变公共请求契约。
- `ResetUnit` 必须阻止新请求、终结活跃实例、恢复资源/属性/冷却并可重复调用；复杂目标、DataAsset、多单位和完整 SkillSystem 不属于本阶段阻塞项。

> **历史基线记录（已修复，2026-10-06）**：早期实现曾由 `USkillLabSkillComponent::RequestTestPulse` 走技能专用路径，并在 `TryActivateAbility` 前设置 `CastId`、扣费和冷却，且请求结果/日志缺少完整的 `SkillId`、来源和目标身份。该段只保留用于解释本基线的迁移背景，不代表当前源码状态。

当前实现已改为 `SubmitSkillRequest` 统一入口；Provider 默认装配通过 `GetDefaultSkillIds` 驱动通用 `GrantSkill`，授予记录分开保存业务 `GrantedAbilityId` 与 GAS `SpecHandle`，有效授予身份在后续目标、资源、冷却和激活门控前写入请求上下文；GAS 激活成功后才创建 `CastId`、扣费和启动冷却。结果/事件携带来源、目标、TargetSpec 快照和定义身份。`Skill_RecoverEnergy` 已作为第二个无资产瞬时技能接入同一路径。

实现 Agent 可直接使用本基线中的身份和结果语义。需要单独专题结论时，优先拆分：

- `Architecture/UnitAbilityOwnership.md`：ASC Owner/Avatar、初始化和重置实现；
- `Architecture/RequestLifecycleMapping.md`：GAS Ability 生命周期与请求结果映射；
- `Architecture/MinimalSkillContent.md`：数据资产字段和首阶段样本；
- `Validation/`：编译、PIE、日志和重置回归证据。

这些专题文档只能细化或修订本基线，不得悄悄改变公共身份、结果语义或清理不变量。
