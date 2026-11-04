# 技能定义加载与规范化接口架构交接

## 目标与边界

这是通用运行基建的 C++/GAS 实现子任务，补足配置迁移前所需的加载期内容接口。其职责是让本轮已纳入范围的技能定义可由 Lua 声明技能专属分类/组合、由 JSON 声明高频纯数值，再形成唯一、经过校验的 `FSkillLabSkillDefinition` 供正式请求和运行时使用。四技能仍只作为既有范围样本，不在此设计任何技能玩法，也不确定尚未批准的内容值。

需求依据是[技能配置生产需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillContentProductionRequirements.md)、[通用基建需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillInfrastructureRequirements.md)和[共同约束](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)。本交接定义跨层数据责任与核验边界，不冻结 Lua 函数名、JSON Schema 具体键名或加载器类结构。

## 当前事实与能力缺口

- `FSkillLabSkillDefinitionProvider::Parse` 通过 `FSkillLabSkillBehaviorRegistry::Apply(SkillId, Out)` 调 C++ Builder 先填定义，再解析 JSON 已知字段。C++ Builder 由 `SkillLabFireballSkill.cpp`、`SkillLabAreaBurstSkill.cpp`、`SkillLabMendSkill.cpp`、`SkillLabBurningSkill.cpp` 注册；它们按技能 ID 决定 `TargetShape`、`Relation`、`Execution`、`Effect`、BehaviorClass 和部分数值/投射物参数。这些正是内容迁移要移出技能专属 C++ Builder 的重复权威。
- `FSkillLabSkillDefinition` 已是下游 Runtime、Targeting、EffectExecutor 和 Loadout 使用的聚合类型，包含目标形态/关系、效果类别、执行模板、范围/半径、效果数值、投射物参数、阶段、Ability/Behavior class 和资源绑定。Provider 的 `Definitions` map 与 `Resolve()` 已提供唯一当前目录及读取入口；`ReloadDefinitions()` 先构造候选 Map，成功后替换，已有失败保留语义可复用。
- `SkillDefinitions.json` 当前 `KnownFields` 严格拒绝未知字段，但未支持 `range`、`radius` 或 Projectile 数值。JSON 目前覆盖成本、冷却、Magnitude、PowerCoefficient、阶段、持续时间/周期、标签、动画路径、Lua 脚本路径与释放设置。
- `USkillLabLuaSkillBehavior::ValidateScript` 目前只在施法前加载脚本、执行顶层代码并验证 `OnSkillCast` 入口；运行时脚本 API 为无参 `SkillLabSkill_RequestAction()` 和 `SkillLabSkill_Finish()`。`RequestDeclaredAction()` 依据已解析定义执行 C++ Projectile、GroundArea 或 InstantTarget 模板；Lua 无权声明定义，也不能选择这些模板的参数。
- 现有运行时权威边界可复用：Runtime 进行请求/施法门控和当前 Cast 身份检查；`SkillLabTargetingService` 校验目标/关系/距离/视线并查询范围；`SkillLabSkillEffectExecutor` 将效果请求交给 `SkillLabGAS`；ASC/AttributeSet 结算并报告真实结果。内容装载成功不意味着请求合法、命中或效果发生。

## 最小加载期定义设计

建议仍以 `FSkillLabSkillDefinition` 作为下游唯一规范化对象，增加加载期声明读取与数值字段解析，不另建一份长期并行的技能权威目录：

1. Provider 读取 JSON 内容数值与 Lua 技能声明，分别解析到临时候选记录；技能 ID/脚本绑定仍由目录入口将两者关联，Lua 声明不必再复制另一份 ID。
2. Lua 加载期声明只表达需求指向的技能专属目标形态、目标关系、效果类别，以及在允许的操作集合中选择/组合现有执行能力。声明必须可转换为有限类型/受控操作计划；不允许 Lua 提供 C++ 类名、对象/Actor 引用、任意函数入口或可绕过 Runtime 的回调。
3. JSON 承载适用的高频纯数值，包括本轮实际需要迁移的 `range`、`radius`、投射物速度/碰撞半径/寿命/显示缩放等候选数值，以及已存在的效果/阶段数值。具体哪些值归 JSON、是否需要其他数值键，以当前内容语义和迁移前快照核定；这里不写入新的值或玩法规则。
4. 对每项定义按字段权威合并：目标/关系/效果类别/执行组合来自 Lua；纯数值来自 JSON；通用 `AbilityClass`、宿主 Behavior 或执行器绑定由 C++ 稳定基建提供默认，而不是按 `SkillId` 生成内容。字段若在多个来源重复声明应拒绝并报告来源冲突，不静默采用优先级覆盖。
5. 候选记录完整转成单一 `FSkillLabSkillDefinition` 后，再做整项和跨字段校验。全部定义成功才原子替换 Provider 当前 Map；任一失败沿用现有目录，不能暴露部分新目录或半填充定义。请求阶段只读取该规范化快照，不再重新解释 Lua/JSON 并拼装第二份事实。

这可以保持 Provider 与下游消费者边界，新增能力集中在 Skills 内容装载/解析层。Lua 声明可与现有 `OnSkillCast` 放在同一个已绑定 Lua 文件，也可由实现选择单独声明资源；无论选择哪种形式，加载期声明解析与运行期组合入口应明确分开，并只产生同一个规范化定义。不要因此预建编辑工具、热更新机制或完整脚本框架。

## 字段来源与安全约束

| 内容 | 权威来源 | 约束 |
| --- | --- | --- |
| 目标形态、目标关系、效果类别、执行组合 | 受限 Lua 加载期声明 | 转换成明确枚举/允许操作；未知项、重复字段、无效组合拒绝加载 |
| 高频纯数值及适用的范围/投射物调参 | JSON | 类型、有限值、数值边界和与定义的必要一致性校验；不能靠 C++ Builder 缺省值掩盖必需值缺失 |
| 通用 Ability/Behavior 实现类、门控、目标合法性、真实执行、效果结算、结果事件、身份与清理 | C++/GAS | 稳定机制；不保留按技能 ID 写入同一份专属目标/效果/执行定义的隐性后备权威 |
| 动画路径和 Cast Notify tag | 当前动画/表现绑定路径，按原交接处理 | 本子任务只维持必要引用；不赋予动画或 Lua 结算权 |

Lua 不是不可信隔离边界的替代物。加载期和运行期均不得向脚本暴露 ASC、`AActor`/World、GameplayEffect 构造/应用、Projectile 对象、`ApplyEffect(Target)`、原生指针或可任意调用 C++ 的反射入口。脚本只能读取它自己的已校验声明/结果，并请求白名单操作；操作执行时由 C++ 再检查当前 Cast、目标关系/存活/距离/形态及执行限制，效果一律经 `SkillLabSkillEffectExecutor`/GAS。返回“接受动作”不等于命中或实际效果。

可复用 `SkillLabLuaRuntime` 当前不开放标准库、支持指令预算和 opaque facade 的环境。定义求值应限制可用全局/回调、返回值结构、指令预算和输出大小；脚本语法执行成功但声明缺字段、类型错误、带未知键或无法落入受控类型时不能成为已加载技能。安全限制是上述能力边界，不扩展成通用脚本安全产品或权限系统。

## C++/GAS 实现边界与内容线接口

本子任务归 C++/GAS 通用基建线，负责：

- 在 `SkillLabSkillDefinitionProvider` 加载期解析器接入 JSON 数值和 Lua 声明，产出唯一规范化 `FSkillLabSkillDefinition` 并维持全目录校验/原子切换。
- 扩展数值字段读取及类型、有限性、边界、必需项、重复来源、目标/执行/效果交叉一致性验证；对迁移前已存在但尚未确认的值不擅自设定新默认。
- 移除 BehaviorRegistry 及四个 C++ Builder 中技能 ID 专属的目标/关系/效果/执行/参数定义重复来源；保留通用注册接线或执行器实现所需 C++ 机制。具体 C++ 类可否删除以无调用者核查为准，不要求为表面清理重写全部执行能力。
- 将规范化定义交给现有 Loadout、Runtime、Targeting、Behavior 和 EffectExecutor 消费者；保持能力验证、正式请求与结算全在 C++/GAS 权威链中。

Lua/JSON 内容迁移线负责：基于实现线提供的受限声明格式和 Schema，把当前四个现存定义逐项迁入并引用对照快照；不改 Provider、GAS、目标权威或公共接口来规避缺口，不添加 Batch01 内容。接口先由 C++ 线交付并经 Review 核验，再迁移数据。配置线遇到无现成规则的值/组合时报告差异，不能自行发明策划语义。

建议的模块范围是 `SkillLabSkills`（Provider、类型、Lua Behavior/宿主、注册器和当前 Builder）与 `SkillLabLuaRuntime` 的既有 facade；`SkillLabGAS` 的效果权威作为依赖，不应因内容装载改写属性结算。`SkillLabPresentation` 仍只读规范化的动画绑定并消费事件，不承担技能定义加载。

## 修改与验证入口

实现前必读：

- `Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`、`Public/SkillLabSkillDefinitionProvider.h`、`Public/SkillLabSkillTypes.h`：解析、目录缓存、候选构建/替换与唯一规范化类型。
- `Private/SkillLabSkillBehavior.cpp`、`Public/SkillLabSkillBehavior.h`、`Private/SkillLabSkillBehaviorRegistry.cpp`：Lua API、动作分发及 Builder 注册。
- `Private/SkillLabFireballSkill.cpp`、`SkillLabAreaBurstSkill.cpp`、`SkillLabMendSkill.cpp`、`SkillLabBurningSkill.cpp`：技能专属 C++ 声明来源和仍需保留的执行机制。
- `Private/SkillLabTargetingService.cpp`、`Private/SkillLabSkillEffectExecutor.cpp`、`Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp`：重新验证发生在何处，以及效果如何保持由 GAS 结算。
- `Source/SkillLabLuaRuntime/Public/SkillLabLua.h` 与 `Private/SkillLabLuaRuntime.cpp`：可用的受限 Lua facade、调用和预算边界。
- `Private/Tests/SkillLabDefinitionProviderTests.cpp`，自动化名 `SkillLab.Skills.DefinitionValidation`；运行期结合 `SkillLab.Skills.CastLifecycle`、既有 RuntimeProbe/Exec 入口和实际四样本场景。
- 需求及上游交接：[配置迁移交接](SkillDefinitionMigrationHandoff.md)、[通用运行基建交接](SkillRuntimeInfrastructureHandoff.md)、[ProgramArchitectureContract](../../../../ProjectContext/ProgramArchitectureContract.md)。

至少验证：加载声明成功生成字段来源确定的规范化定义；必要定义缺失、重复来源、未知枚举/字段、Lua 执行错误、错误类型、非有限/越界数值、无效字段组合时拒绝候选且不替换上一份完整定义；对照 JSON 的 range/radius/Projectile 数值确实进入定义对象且校验失败可定位；源码不再由技能 ID Builder 保留同一组业务定义；加载成功仍不能越过正式请求门控、目标校验和 GAS 效果路径；恶意/错误脚本无法取得 Actor/ASC 或自行命中/结算；四个现存样本的目标、实际效果、持续状态和终态与记录快照核对。

测试结果要区分定义解析、规范化目录、Runtime/GAS 事实和完整玩家可见行为。原自动化可能包含对旧 Builder 的假设，需随迁移修正测试输入但不能降低运行链断言。只完成 Provider 单元测试或 Lua 声明成功不足以证明迁移行为等价。尚未冻结的样本数值与玩法细节均以迁移交接记录的当前快照作候选对照，不在本架构任务中确认为新规则。
