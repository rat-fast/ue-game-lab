# Batch02 Lua 技能组合层试点：架构交接

> 接管状态：独立架构对话 `01a16387-fb75-7271-8989-d3ccc811a8ff` 于 2026-10-22 形成本文时，范围是“通用 Lua 技能组合层 + 一个最小代表性技能验证”。随后用户将实施范围扩展为四个现有技能：`Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff`。本文保留单样本架构依据；四技能当前实现事实、编译/运行证据和未验证项以同目录 `ImplementationSummary.md` 及当前策划需求为准。

## 1. 结论、来源与证据边界

当前交接确认了基于现有 C++/GAS 能力建立受限 Lua 组合宿主的可行性；原交接以 `Skill_Mend` 自疗作为最小验证样本。四技能迁移由后续实现按同一窄接口扩展，不能把本文的单样本验证条件误读为四技能实现范围。

“通用”只要求稳定内核不按技能 ID 或技能专属流程分支；技能 ID 可出现在内容注册、脚本绑定和测试入口。接口只服务本样本实际调用，不提前覆盖六技能，也不建立通用技能框架。

权威与读取范围：

- **当前范围说明**：本文形成时只覆盖 `Skill_Mend`；当前四技能实现范围由策划需求和 `ImplementationSummary.md` 维护。Batch01 六技能仍不属于本轮。
- [Batch02 原需求](../../../Planning/SkillSystem/Batches/Batch02-LuaSkillCompositionPilot/LuaSkillCompositionPilotRequirements.md)：仅取第 3–7 节适用于本样本的权威、真实结果、单一内容来源、身份、失败隔离和表现边界；旧六技能覆盖及 L01–L13 全量矩阵只作背景。
- [共享基线入口](../../../Baseline/SkillSystem/README.md)：按本样本需要读取请求、效果、内容边界，不要求实现者通读 Batch01 六技能与 Infrastructure 全集。
- [程序职责约定](../../../ProjectContext/ProgramArchitectureContract.md)、[代码地图](../../../ProjectContext/SkillLabCodeArchitecture.md)及相关模块文档提供定位；下文事实另经直接源码核对。

本轮读取相关文档、源码、当前配置和第 8 节指定历史日志，仅修改本文。未核对 Blueprint 图、编译、启动 UE 或执行测试。“当前事实”“架构建议”“历史结果”分别陈述，均不等于本轮试点已运行通过。候选切片尚未派发，本架构对话不实施代码或修改其他文件。

## 2. 当前真实运行链与复用边界

### 2.1 本样本的请求到效果

角色初始化 ASC/runtime → provider 加载定义、loadout 授予 → `SkillTestRecover`/槽位索引 2 构造 `Skill_Mend` 请求 → `SubmitSkillRequest` 门控 → 建立 Cast、Windup、GAS 激活、Started → 内核 Timer 触发 `ReleaseCast` → 重检门控/自身目标 → Active、一次性释放、扣能量/启动冷却 → TargetSelected → `USkillLabMendBehavior::Execute` → `ApplyEffect(owner)` → Executor/Resolver/ASC 执行治疗 → 真实结果及事件 → `Complete(HealingApplied)` → Recovery → Completed。

- 成本、冷却由 runtime 手动维护，不是 GAS `CommitAbility`；本轮保留**释放时提交**。Ability 为 InstancedPerActor、LocalOnly，不承诺网络能力。
- `FSkillLabCastLifecycle` 已提供阶段 token、单次释放和互斥终态；runtime 已保存活动定义/请求快照，可复用。一个 runtime 同时一个活动 Cast。
- Mend 的目标是 Self，不是 Batch01 的 Self/Ally 版本；不需要新增空间查询、目标选择或执行对象。现有 `ValidateRequest` 与 Executor 的自身/存活/关系检查继续由 C++ 执行。
- 即时 GE 的成功依赖 `WasSuccessfullyApplied()`，不能改为检查 active handle 是否有效。治疗可应用成功但实际量为零；真实治疗量目前由 Resolver 的前后生命差得到。
- `Execute` 当前无论 `ApplyEffect` 返回什么都请求 Complete。流程 Completed 与效果应用成功必须分别观察，不能用 Cast 完成或字符串 `HealingApplied` 推导治疗成功；本轮不借迁移另定 GE 失败的玩法终态。

### 2.2 与本轮直接相关的缺口

| 当前事实 | 最小改动要求 |
|---|---|
| Skills 无技能 Lua 宿主；现有 Lua 在 Presentation 中 | 建立 Skills 侧受限宿主，保持 Presentation → Skills → GAS 的依赖方向 |
| 外部正 RequestId 直接接受，内部自增，无请求/操作去重 | 请求命名空间与重投识别明确；一个运行的同一治疗操作最多提交一次 |
| CastId 为 runtime 内自增值，UnitId 来自 UObject UniqueID | 动作权限结合有效 owner、运行代次、Cast 和脚本版本；不凭裸整数或技能 ID 验权 |
| `IsBehaviorCurrent` 未检查 `bFinishing` | 清理前撤销动作权限，Reset/结束中拒绝脚本动作 |
| `Emit` 同步广播 Blueprint/native，可重入；效果后连续通知缺完整防护 | 提交/记录结果后通知；重入不能重复治疗、用新 Cast 身份覆盖旧结果或继续旧组合 |
| Resolver 用前后生命差求实际量 | 核对并隔离本次治疗归属，避免属性监听中的嵌套变化污染；只补本样本需要的治疗结果，不扩建全部战斗结果系统 |
| 激活失败可先 Cancelled 后 Rejected，且未 Started | 修正请求接受/开始边界：开始前失败为 Rejected，无正式 Cast 终态和费用副作用 |
| 活动期为零时没有 Active 超时，旧代码依赖 Behavior 主动收尾 | Lua 回调必须有界；合法返回但遗漏收尾也要由 C++ 确定失败结束，不能无限忙碌 |

初始化的 `Completed(Initialized)` 使用 CastId=0，不计入真实 Cast 的终态。修正上述隔离/拒绝缺口属于本次接入所需加固；正常技能行为仍按第 3 节对照，不能把旧错误事件序列当成必须复制的基准。

### 2.3 内容、Reset 与表现的现状

`Config/SkillDefinitions.json` 目前包含四个样本。provider 先执行各 C++ Configure，再覆盖 JSON 已支持字段；未知字段拒绝，且要求已注册 Behavior/Ability。单条无效或重复 ID 会拒绝整个候选目录，初次失败后不自动重试；重载保留旧 catalog，已有 loadout 不会随重载重建。这不满足“坏样本明确不可用、其他有效技能继续”的隔离与恢复要求，需要最小局部改造。

`ResetSkills` 依次取消、清已跟踪出站 GE、清冷却、恢复生命/能量，没有覆盖整个 Reset 的屏障或运行代次。生命恢复使用 `SetNumericAttributeBase`，而 Dead Tag 同步位于 `PostGameplayEffectExecute` 的 `ClampVitals`，未见 Reset 显式恢复死亡状态；死亡后 Reset 能否重新施法仍需验证。Mend 没有持续对象，本轮只需把它的阶段、宿主、操作记录、相关表现和属性恢复纳入现有单位 Reset，不扩建场景级全对象 Reset 系统。

当前存在 AnimNotify → `ForwardAnimNotify` → `ForwardCastEvent` → `ReceiveCastEvent` 的业务回写接口；已有 Cast/动画/Montage instance 归属检查不代表拥有业务权威。本样本使用 Timer，且没有 BehaviorEventTags；必须保证迁移后仍只由内核释放，重复/缺失通知不影响治疗和费用。无需为此改造所有未迁移技能或新增表现事件系统。

## 3. 最小代表性技能与固定对照

### 3.1 选择现有 Skill_Mend 的理由

自疗只需要“读取已校验上下文 → 请求一次配置效果 → 消费真实结果 → 请求收尾”，足以验证 Lua 与稳定内核的组合交界。它已有可运行入口和历史治疗结果，还能用受伤、接近满血和满血覆盖实际治疗差异；无需先补投射物、方向、区域、引导或护盾。选择是架构技术建议，不是新增技能策划或要求用户先批准样本。

### 3.2 本次保持的当前版本

对照以本轮核对时的 `SkillLabMendSkill.cpp`、`SkillDefinitions.json` 及其 Runtime/Executor/GAS 路径为准，**不是 Batch01 同名 Mend**。实施者须在改代码前运行并记录原生基准，保留所用源码/配置/脚本版本标识或摘要、初始状态和输入；历史日志不替代这次成对基准。

| 内容/行为 | 固定值与语义 |
|---|---|
| 技能与目标 | `Skill_Mend`；Self/Self，owner 自疗；InstantTarget、Healing |
| 阶段 | Timer 释放；前摇 0.20 秒，主动时长 0，治疗后请求恢复，恢复 0.30 秒，再 Completed |
| 资源 | cost=15；cooldown=2.0 秒；释放重检成功后各提交一次；前摇取消不产生新费用/冷却 |
| 治疗 | magnitude=40；使用当前 GAS 生命上限钳制；不改变公式，不引入 Power 加成 |
| 阻塞 | 当前 Dead、Stunned、Silenced；开始/释放重检及既有取消规则保持 |
| 效果与完成 | 一次治疗操作返回后请求收尾；正常完成原因沿用 `HealingApplied`；效果成功和实际量由 C++ 结果判断 |
| 表现 | 保留当前 JSON 动画绑定；资源缺失允许无表现，不能影响释放、治疗、费用或终态 |

例如 MaxHealth=100、能量充足、无阻塞/冷却时，初始 Health=50/90/100 分别应产生实际治疗 40/10/0；满血仍按原规则支付且可应用成功。治疗未实际改变生命不等于效果失败，不要求新增 Batch01 的 NoChange/Overflow 公共协议。具体时序按项目 Timer 精度比较，不以帧级耗时一致作为条件。

### 3.3 未来边界

其他三个现有样本保持原生路径，只用于受影响范围的冒烟和失败隔离检查，不迁移其组合。Batch01 的 Firebolt 追踪/减速、ArcStrike 方向/扇形、BurnField 区域/成员/尾效、ChannelBeam 引导/分段支付、Barrier 护盾，以及 Batch01 Mend 的友方目标/零前后摇等均属未来范围。当前不补这些能力、不预留全量 API、不要求读取其技能案；六技能矩阵不得用于判定本轮通过。后续须由用户另行确认后再分析与派发。

## 4. 建议的最小职责与窄接口

### 4.1 权威、组合与内容

C++/GAS 继续负责请求门控、身份、阶段计时、释放、费用/冷却、自身目标重检、实际治疗、终态和 Reset。Lua 只负责该次运行中请求已有效果并按真实结果请求收尾；不能写属性、扣费、推进阶段、指定任意目标/效果数值、伪造效果或强制终态。

本样本的业务参数只来自一个已校验内容定义，Lua 不复制 cost、cooldown、治疗量或阶段时长。继续使用 JSON，并仅补本样本确需的脚本绑定与声明字段；Self/Healing 等内容可经受限声明映射到既有枚举/能力，不能在稳定宿主内写 `if SkillId == Skill_Mend`。已迁出 Configure 的同一字段不再在 C++ 保持另一份可生效值。其他未迁移样本不要求同批改造；明确的可选默认可以保留，必需字段缺失不能静默补成成功。

迁移后本样本只有 Lua 一份组合权威。旧原生实现可在迁移前用于采集基准，不作为同次运行的并行执行或故障回退；保留 Behavior 机制时仅作通用脚本接入，不继续执行自疗专属流程。对照阶段区分原生/迁移版本，不新增面向玩家的双实现切换产品功能。

### 4.2 当前调用方实际需要的交界

接口名称留给实现确定，只需以下三类交界：

1. **释放后的只读运行上下文**：有效 owner/运行代次/Cast、请求关联、内容/脚本版本，以及已由 C++ 确认的自身目标引用。Lua 获得受限标识，不获得可任意操作的 ASC 或 Actor 指针。
2. **请求一次配置效果**：运行句柄 + 本运行效果操作标识；C++ 仅允许调用定义中声明的已有治疗能力，对已校验自身施效，参数取该运行内容快照。无需开放任意查询、伤害/状态构造、计时、独立对象或分段支付 API。
3. **读取结果并请求收尾**：结果含适用的请求/Cast/源目标/操作身份、版本、应用成功与原因、名义治疗量、实际治疗量及生命前后值。Lua 可请求“组合已处理完毕”，C++ 校验当前身份及操作状态后沿既有 Recovery/Completed 链收尾；Lua 不提交效果成功标志或终态。

同一操作重复请求返回已记录结果或明确重复拒绝，不能再治疗；操作提交前先占用，形成不可变真实结果后再通知。也要限制本样本每运行只允许这一治疗操作，不能靠更换操作 ID 绕过次数约束；该限制来自受校验的运行能力声明，不在稳定内核硬编码 Mend。已提交结果被监听者 Reset/取消时保留，后续旧脚本动作必须拒绝；不能重新收费、补偿重试或把新运行数据填进旧事件。

即时效果返回值、结果通知和请求收尾应关联同一结果；若保留同步通知出口，必须处理其中的嵌套调用。治疗实际量需在本次权威治疗结算边界取得，不能简单把包含其他监听者生命改动的总差值归给本操作。具体实现可在现有 GAS 治疗出口补有限归属记录，不要求重做其他效果协议。

### 4.3 宿主与依赖

现有 Presentation 内含 Lua 5.4.8，每表现组件拥有 VM，桥仅有 PlaySkillAction、StopSkillAction、SetLocomotionIntent、Log，不能直接用作技能宿主。建议复用同一 Lua 编译来源，必要时抽取最小公共运行库；技能宿主位于 Skills，表现宿主留在 Presentation，不允许 Skills 反向依赖 Presentation。公共库的 UBT 接法、技能 VM 粒度属于技术选择，不是前置架构工程。

现有宿主使用 `luaL_openlibs` 和 `lua_pcall`，没有指令预算；新宿主只开放当前所需入口，装载和回调都须有界。异常、死循环、句柄失效及没有完成协议的正常返回均由 C++ 兜底结束，不依赖脚本自觉收尾。无需玩家脚本安全沙箱、任意异步调度器、通用技能图或热修产品能力。

### 4.4 表现只消费事实

保留正常动作/效果反馈，但只从本次权威事件取事实。活动内容的表现 binding 与业务快照绑定；本轮可禁止活动 Cast 时切换版本，避免大规模表现版本系统。样本不启用 AnimNotify 业务触发路径；须在接入处拒绝其影响脚本动作、费用和治疗，不能只在 Lua 注释约定。

表现加载失败或运行异常只清理本运行未完成表现。恢复资源不补发治疗或扣费；技能脚本失败也不能由表现侧补造成功。仅改必要桥接调用点，不迁移角色表现逻辑。

## 5. 身份、失败、Reset 与恢复

### 5.1 最小身份与去重范围

入口建立可区分 owner 实例的请求身份，区分合法再次施放和同请求重投；若继续接收外部 RequestId，需限定命名空间并处理与自动号的冲突。重复请求只能返回原受理结果或重复拒绝，不能启动第二次同请求施法。本轮只需当前本地运行内的有界记录，不建立网络/存档级全局账本。

运行权限结合 owner 有效引用、运行代次、Cast 与脚本/内容版本；Reset、结束、owner 销毁和版本切换先撤销旧权限，再清理。重复效果/完成调用以及捕获旧句柄的调用不能影响新的 Cast。无需给不存在的区域、护盾等预建身份类型。

### 5.2 失败处理

| 时点 | 本样本必须满足的边界 |
|---|---|
| 内容/脚本加载失败、请求前 | 本技能明确不可用/Rejected，无正式 Cast、费用、冷却或治疗；其他有效原生技能继续 |
| 已 Started，前摇中失效 | C++ 结束当前运行、清 Timer/宿主权限；尚未释放则无费用/冷却/治疗；不新增 Lua 前摇回调来凑测试 |
| 释放后、治疗提交前异常 | 已提交费用/冷却保留，不治疗；安全结束且后续请求不串扰 |
| 治疗提交后脚本/结果回调异常 | 已发生治疗与费用保留，不重复提交；撤销后续权限，单一失败终态 |
| 表现缺失或异常 | 权威请求、阶段、治疗、费用和终态照常；只清对应表现 |

本样本已有 Cancelled，无 Interrupted 需求。架构建议将**新引入的已开始脚本技术故障**映射为 Cancelled 加明确 ScriptError/ScriptUnavailable 原因；请求前仍为 Rejected。这复用现有终态，不采用 Batch01 新语义，不影响普通效果返回后的既有收尾。该建议供 PM 随本交接审核，不以先确认六技能故障策略作为技术实施前置。

加载/运行故障定位到技能、适用的请求/Cast、阶段和版本；异常运行先结束，该坏脚本版本随后明确不可用，验证并恢复有效内容后才接纳新请求。局部坏内容不能静默回退到旧脚本、原生自疗或另一技能，也不能使其他三个有效技能一起不可用。系统级 Lua 初始化失败时仅脚本样本不可用，原生样本应继续；UI 文案细化不阻塞权威结果验证。

### 5.3 单位 Reset 与开发恢复

复用当前 `SkillReset`，增加覆盖本单位完整 Reset 的屏障/代次：先拒绝新请求并失效旧句柄，再取消 Cast、清 Timer/宿主及效果操作记录、清本运行表现，沿现有职责清效果/冷却并恢复属性与死亡状态，最后解除屏障。结束和清理要幂等，广播中重入不能插入新 Cast；Reset 完成可由返回后的状态/日志核实，不要求新增六技能 ResetCompleted 公共协议或场景级协调器。

本样本无持续状态或独立对象，已提交即时治疗不做逆向回滚，Reset 自身仍按既有规则恢复初值。需要验证死亡 → Reset → 新请求可用，不能仅见 Health 回满就当恢复成功。其他样本既有出站效果清理由原职责保持，不顺带重设计所有权。

内容加载至少隔离可归属到单技能的错误与脚本依赖，初载失败可在修复后显式重载恢复，loadout/可用状态实际同步。整份载体语法破坏时若无法定位条目，实现可将试点内容拆为独立可校验单元，避免拖垮其他原生技能；文件组织由实现选择，不新增通用内容平台。

开发重载建议先限制在无活动 Cast 时切换候选版本；活动时明确拒绝/延后，旧句柄不得在切换后继续生效。无需活动 Cast 跨版本执行、VM 热迁移或产品版本回退。内容坏时旧快照仅可用于已经接纳的运行，不得让新的坏样本请求静默成功。

## 6. 最小实施切片、必读与依赖

原 A–D 划分不再沿用：原 A 只保留本样本所需隔离，原 C 收窄为最小宿主/内容；原 B 的六技能原生扩展和原 D 的六技能迁移全部退出本轮。以下是候选实施切片，不代表已派发，也不要求建立多个实现对话或额外交接文件。每个实施者先经项目/程序 Skill 入口，读本文第 1、3、7 节及自身章节；路径以下相对 `ue/SkillLab/`。

### 6.1 运行交界与必要加固

- 目标：提供第 4.2 节实际调用交界，补请求/操作去重、真实治疗结果、结束验权、故障结束、单位 Reset 屏障与死亡恢复；不补新的效果种类。
- 必读：本文第 2、4.1–4.2、5、8 节；Batch02 第 5–7 节中适用条款。源码 `Source/SkillLabSkills/{Public,Private}/SkillLabSkillRuntimeComponent.*`、`SkillLabCastLifecycle.*`、`SkillLabSkillTypes.h`、`SkillLabSkillBehavior.*`、`SkillLabSkillGameplayAbility.*`、`SkillLabSkillEffectExecutor.*`；治疗归属/死亡恢复按需读 `Source/SkillLabGAS/` 的 `SkillLabEffectResolver.*`、`SkillLabAbilitySystemComponent.*`、`SkillLabCombatAttributeSet.*`。
- 允许落点：这些现有职责及直接测试，Adapter 只在 Reset/通知直接需要时改动。运行侧维护公共句柄、效果操作和结果契约；GAS 侧继续拥有治疗与死亡一致性，不把结算搬进宿主或控制器。
- 依赖/验证：先采集原生基准，再加固；可先实现不依赖 Lua 的验权与重复/重入验证。与 6.2 约定当前三类交界即可，不提前冻结六技能 API。核实普通路径、激活拒绝、嵌套结果归属、重复效果、Reset/终态重入及死亡恢复。

### 6.2 最小 Lua 宿主与局部内容接入

- 目标：装载/执行有界脚本，绑定运行与版本，暴露只读上下文、一次配置效果和收尾请求；实现坏样本隔离、无活动时重载和恢复。公共内核不含样本 ID 分支。
- 必读：本文第 2.3、4、5 节；Batch02 第 3/6/7 节适用边界。源码 `Source/SkillLabSkills/` 的 `SkillLabSkillDefinitionProvider.*`、`SkillLabSkillBehaviorRegistry.*`、`SkillLabSkillLoadout.*`、Build.cs、`Config/SkillDefinitions.json`；`Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp`、Presentation Build.cs 和 `ThirdParty/lua-5.4.8/src/` 编译入口。仅在改表现桥时按需读 `SkillLabSkillAnimationComponent.cpp`、`SkillLabSkillAnimNotify.cpp`、router 与 `Config/CharacterPresentation.lua`。
- 允许落点：Skills 侧宿主/通用 Behavior 接入、最小公共 Lua 构建依赖、provider/registry/loadout 内容适配，及确需的表现绑定。公共 Runtime/Executor 契约由 6.1 维护，宿主只消费；必要改动先约定落点，不能两侧各造一份效果桥。
- 依赖/验证：可先核实 Lua/UBT 与加载，接入执行依赖 6.1 的窄交界。验证加载异常、指令预算/正常返回未收尾、非法/重复动作、旧句柄、样本内容损坏而原生技能仍可用、初载修复恢复和无活动重载。

### 6.3 单样本组合与成对验证

- 目标：仅把当前 `Skill_Mend` 的一次配置效果/收尾组合迁入 Lua，固定第 3 节内容并执行第 8 节验证。不承担任何六技能迁移。
- 必读：本文第 3–5、7–8 节，6.1/6.2 实际接口与证据；`Source/SkillLabSkills/Private/SkillLabMendSkill.cpp`、当前配置、`Source/SkillLab/Private/SkillLabTestCommandsComponent.cpp`；按需读 `Source/SkillLab/SkillLabPlayerController.cpp` 探针、Shortcut 和现有测试。
- 允许落点：样本 Lua、单一内容定义、既定 registry 接入和最小测试/日志适配。provider/宿主公共变动归 6.2，Runtime/GAS 公共变动归 6.1；发现缺口回到对应切片，不在脚本补权威。
- 依赖/验证：原生采集必须在改样本行为前完成；迁移运行依赖 6.1/6.2 可用。验证内容一致、真实量、阶段/费用/终态与失败隔离，其他三个样本只做必要冒烟。迁移成功标准以第 8 节为准，不等待 Batch01 原生基准或支付决定。

建议顺序：先记录当前原生基准并定窄交界 → 运行加固与宿主依赖/装载 → 单样本接入和故障注入 → 按受影响范围 Review。切片可由同一实现者连续完成，实际共享文件变更串行协调；不为拆分数量增设层或验证平台。

## 7. 进入实施的条件与待决边界

### 7.1 当前已能确定的范围

用户已明确收窄范围，无需再次询问是否先做单技能。可以按本文向 PM 提交最小技术实施建议，本对话不启动实现。当前样本沿同一已存在版本迁移，**共享基线与 Batch01 的支付冲突不阻塞本轮**；不得偷偷切换为激活支付、零前后摇、友方治疗等 Batch01 新行为。

本轮没有由六技能数值/体验选择产生的业务前置问题。脚本技术故障建议复用 Cancelled+原因、坏脚本版本局部不可用，PM 可据第 5 节审核；不要求先设计整套技能故障产品策略。若实施确实发现无法保持本样本现有业务语义的结构问题，附具体事实回交，不能自行扩大功能范围。

### 7.2 实现期需验证的问题

Lua 编译依赖复用、VM 粒度、句柄/有界记录形式、结果回传方式、单样本内容组织均属技术选择。必须通过运行核实治疗实际量在嵌套监听下的归属、请求/操作去重、同步 Reset/结束重入、死亡状态恢复、有界执行与局部加载恢复；它们是本轮技术验证，不要求用户先批准 API 名称或文件格式。

### 7.3 旧交接的降级与阶段结论

旧文档中的六技能能力差距、Channeling/Interrupted/ResetCompleted 扩展、独立对象所有权、六技能全字段/接口、场景 Reset、动态 DoT 刷新及六技能 L01–L13 对照均降为未来范围；仅保留第 3.3 节简短边界。它们不进入本轮必读、当前接口预留、依赖或验收。支付冲突与各 Batch01 技能待评审项留待未来另行授权时处理。

独立静态架构接管和收窄交接已完成，可供 PM 审核最小实施安排；未实施、未运行、未通过独立 Review。最终技术结论只覆盖“组合层支撑当前自疗样本且稳定内核不硬编码其流程”，不证明六技能迁移就绪。

## 8. 当前验证条件、入口与历史证据

以下条件是本轮单样本通过依据，均尚待实施后运行；不复用六技能 L01–L13 作为全量放行标准。

1. **同版本原生/Lua 正常对照**：固定第 3 节内容、同初态与请求，比较受伤、接近满血、满血三组。记录 Started/阶段/实际治疗/Recovery/唯一终态；实际量分别为 40/10/0，费用 15、冷却 2 秒且只在释放提交。事件身份可不同，但同运行关联和业务先后必须一致。结果采集须能证明实际走 Lua，不能只看宿主 ready 或请求 success。
2. **门控与阶段**：资源不足、冷却中、Dead/Stunned/Silenced、未授予分别拒绝，无正式 Cast/新费用/新效果；前摇取消不收费，恢复完成不再治疗。仅测试本样本适用目标，不新增友方/地面/方向场景。激活失败边界按第 2.2 节修正后验证。
3. **真实结果与重复**：重复同请求、重复同操作、换操作 ID 重复施效、重复收尾均无额外治疗/费用/终态；旧运行及另一 owner 句柄不能生效。属性/事件监听中嵌套变化不污染本次治疗实际量；监听中取消/Reset 后旧结果身份不串入新 Cast。强制效果拒绝须保留真实失败，不以 Completed 冒充效果成功。
4. **脚本与内容失败**：缺失/语法错/非法调用/无限循环/返回未收尾，及释放前失效、治疗前异常、提交后回调异常分别有界结束或请求前拒绝。费用和已提交治疗按第 5 节保留，不重试整条组合、不静默原生回退；其余有效原生样本仍可用。修复内容、重载后新请求真实恢复，不只打印 reload succeeded。
5. **Reset、销毁与重载**：分别在前摇、治疗结果通知和恢复中 Reset，覆盖同步重入；死亡后 Reset 再请求成功。无旧 Timer/宿主动作/相关表现残留，Reset 中不接纳新施法；owner 销毁后捕获句柄无效。活动 Cast 中重载明确拒绝/延后，无活动切换后旧版本句柄失效，不要求热迁移。
6. **内容与表现边界**：选一项现有参数（如治疗量）在原生/Lua 两侧使用相同修订版成对运行，再恢复固定版本，证明单一来源且 Lua 无重复数值。使当前动作资源或表现 Lua 失败，权威治疗/费用/终态与正常对照一致，恢复不补发业务；重复 AnimNotify 不产生额外动作。其他原生样本仅对共享改动做冒烟，不要求迁移。

这些是具体操作/断言，不要求新建通用测试平台或完整报告包。原生参考已有的隔离错误不能作为迁移后通过理由，修复点单列说明；正常业务差异应判失败或回交业务变更。

现有可复用入口：

- `SkillTestRecover` 请求现有槽位索引 2 的自疗；`SkillReset`、`SkillReloadDefinitions` 提供单位重置/目录恢复入口。精确初始生命/能量、故障注入及结果采集可在既有测试/探针增加最小适配，不通过 Lua 直接写属性。
- `SkillLab.Skills.CastLifecycle` 验证 token/单次释放/互斥终态，`SkillLab.Skills.DefinitionValidation` 验证字段/重复 ID；二者不替代 Runtime/宿主集成。源码为 `Source/SkillLabSkills/Private/Tests/SkillLabCastLifecycleTests.cpp`、`SkillLabDefinitionProviderTests.cpp`。GAS 现有 `SkillLab.GAS.DamageCalculation` 只是纯伤害算术，不提供本次治疗接入通过证据。
- `ASkillLabPlayerController` 的 `-SkillLabRuntimeProbe` 原探针依次请求四样本、取消与 Reset，约 17 秒游戏时间退出；可复用而非重建框架，但需补本样本精确初态、重复/异常和断言。`SkillAnimationSmoke`/`SkillPresentationSmoke` 及正常 DX11 入口可辅助表现检查，NullRHI 不能证明视觉正确。

本轮核对的历史日志：

- `Saved/Logs/SkillLabPresentation-ModuleAutomationAll.log`，UTC 2026-10-21 18:15：Animation.DefinitionProvider、GAS.DamageCalculation、Skills.CastLifecycle、Skills.DefinitionValidation 成功；UI.WidgetTreeBeforeSlate 失败（期望 SConstraintCanvas/SBox，实际 SObjectWidget）。未在本轮修复/重测，不宣称全套自动化通过。
- `Saved/Logs/SkillLabPresentation-ModuleProbe.log`，UTC 2026-10-21 18:09：NullRHI、表现 Lua ready、四样本链路及一次取消/Reset，最终 COMPLETE；其中 Mend 请求量 40、满血实际量 0。该记录仅证明历史原生路径存在，不是技能组合 Lua 已执行的证据。Reset 仅采样能量/阶段/冷却，动画多次 `anim=null`，均不足以证明恢复/清理或视觉通过。

实施交回记录本样本内容/源码/脚本版本、初态与输入、权威阶段/费用/实际量/终态、错误原因与清理结果及证据位置即可。当前第 1–6 项全部待验证，未来六技能矩阵不在本轮结论中。
