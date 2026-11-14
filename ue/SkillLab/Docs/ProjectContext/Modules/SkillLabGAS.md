# SkillLabGAS 战斗属性与效果

## Module Responsibility

`SkillLabGAS` 提供 ASC、战斗属性、GameplayEffect 承载和伤害/治疗结算，并作为 Energy/MaxEnergy、预留与结算历史的单一权威。它不决定输入、目标选择、技能槽位或 Cast 阶段。

## Code And Assets

公开入口为 `Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h`、`SkillLabCombatAttributeSet.h`、`SkillLabEnergySettlement.h`；纯伤害计算定义在同目录 `SkillLabDamageCalculation.h`。属性包括 `Health/MaxHealth`、`Energy/MaxEnergy`、`Armor`、`Power`、`IncomingDamage/IncomingHealing`。

| `Source/SkillLabGAS/Private/` 定位入口 | 当前职责 |
| --- | --- |
| `SkillLabAbilitySystemComponent.cpp`、`SkillLabCombatAttributeSet.cpp` | actor info、属性、伤害/治疗与钳制 |
| `SkillLabCombatGameplayEffects.*`、`SkillLabEffectResolver.*`、`SkillLabGASTags.cpp` | 效果承载、效果分发与 Tag |
| `SkillLabAbilitySystemResource.cpp` | 受控 Energy/MaxEnergy 写入、队列及 tracked 回执 |
| `SkillLabAbilitySystemEnergyReservations.cpp`、`SkillLabEnergySettlement.cpp` | 预留、一次结算、资源历史及销毁收束 |
| `SkillLabResourceEffects.cpp` | GE 资源路径保护、聚合检查与有限 spec 转换 |

资源回执类型与 `RequestResourceChangeTracked` 声明在 ASC 公开头文件中，不另设资源权威对象。

## Runtime Entry

角色构造 `USkillLabAbilitySystemComponent`，技能运行时初始化 actor info 和属性集；效果执行器通过 ASC/Resolver 应用即时伤害、治疗或持续伤害。技能提交用 `BeginEnergyEscrow` / `ResolveEnergyEscrow`，外部资源调整用 `RequestResourceChangeDetailed` 或需最终结果的 `RequestResourceChangeTracked`。

## Runtime Flow

伤害和治疗通过 meta effect 进入 AttributeSet，统一执行减免、生命钳制与死亡 Tag 更新。`ApplyDamageToTarget` 可携带攻击类别；目标生命实际下降时广播 `OnSkillLabDamageTaken`，Runtime 决定是否打断 Cast。空/未知类别仍可通知，但不据此打断。

`ApplyDamageOverTimeToTarget` 每次创建独立的动态 GE 定义，局部 `TStrongObjectPtr` 从定义创建起覆盖 Spec 构造及整个同步 Apply 调用，包括目标 ASC 的应用查询回调。成功应用后由目标 ASC 活跃 Spec 的 `UPROPERTY Def` 接续保活；效果移除且无其他强引用后，定义可由 GC 回收。拒绝应用返回后局部强引用释放，源 ASC 不永久保留定义。当前定义配置为 `AggregateByTarget`，但 GAS 堆叠匹配还要求 Def 身份相同，逐次独立创建不会把不同应用合并；定义寿命不依赖原 Cast 或技能 Runtime 存活。

AttributeSet 将周期伤害结算暂存到目标 ASC，按本次 `FGameplayEffectSpec` 的精确地址关联；GAS 周期回调以同一活跃 Spec 身份取出结果，在广播 `OnPeriodicDamageResolved` 前移除暂存项，并附上效果句柄。不匹配时不猜测归属；技能侧按效果句柄保存来源并观察事件。

`GetActualEnergy` 读取 GAS Energy，`GetReservedEnergy` 汇总预留，`GetAvailableEnergy` 给出扣除预留后的可用视图。Begin 只预留，不先减实际能量；成功确认扣实际一次，明确失败释放，未知保持预留并由原事务对账。结算历史可独立查询，不依赖当前 Cast 存活。

Energy/MaxEnergy 的 Additive/Override 请求进入同一协调器：计算并钳制目标、检查预留/聚合条件，将钳制后的目标写入 GAS base，并核对 current/base 与预留。写保护期间的合法请求排队，不绕过已有预留；资源写入、预留和结算共同维持一致性。

## Current Behavior

`ESkillLabResourceChangeResult` 为 `Rejected/Queued/Applied`，Queued 仅表示排队受理。`RequestResourceChangeTracked` 接收 AttemptId、弱接收方、写前 `BeforeApply` 与最终 `OnFinished`；最终 `FSkillLabResourceReceipt` 包含 AttemptId、ASC 身份、`Applied/Rejected/Failed/Unavailable` 及原因。回调可能同步发生，调用方必须先登记等待身份；排队请求在出队/退出写保护后闭合，EndPlay/BeginDestroy 收束待处理项，失效接收方不被调用。

真实 `GameplayEffectApplicationQueries` 路径拒绝静态 Energy/MaxEnergy modifier；没有全局禁止带 execution 的 GE。execution 的实际 Energy/MaxEnergy 输出由 AttributeSet `PreGameplayEffectExecute` 否决，非资源输出保留。混合输出不提供整个 GE 的原子拒绝，非资源部分可能生效。

`RequestResourceEffect` 只把符合约束的纯即时、单资源数值 spec 转换为协调请求，支持范围内的 ScalableFloat/SetByCaller 数值；不授予原 GE 的句柄、Tag 或其他完整行为。持续资源聚合及不支持的混合/计算/组件配置拒绝，不把该转换接口描述成任意资源 GE 支持。

Reset 的 attempt、请求屏障、冷却清理及业务结果由 Skills Runtime 拥有，GAS 只提供资源写入与最终回执。Runtime 只有在原身份最终 Applied 且无未决时完成相应清理；详细时序与失败原因见 [Skills 模块](SkillLabSkillsRuntime.md)。

## Current Responsibilities And Evolution Boundaries

属性与伤害算术优先查看 AttributeSet/`SkillLabDamageCalculation`，资源调整查看 Resource，预留/历史查看 EnergyReservations/EnergySettlement，GE 保护查看 ResourceEffects。调用者需要完成事实时应消费最终回执，不能把 bool 或 Queued 当作 Reset 已成功。新增资源效果、叠层或其他写入来源时需明确是否进入协调路径，再调整边界。

## Verification

自动化入口为 `SkillLab.GAS.DamageCalculation`、`EnergyLedger`、`EnergySettlementHistory`、`ResourceWriteIntegration`、`ResourceExecutionBoundary`（后四项同属 `SkillLab.GAS.` 前缀）。已接受的 R 与 Reset 交叉证据、资产检查范围见 [TestAndVerification](TestAndVerification.md)，本次未重新构建/运行或审资产。

## Current Limitations

保护范围是已接入的协调 API、GE 应用查询及实际资源输出；不承诺拦截任意非 virtual 原生旁路。排队分析限有效会话、有限资源、无不支持聚合/旁路等既定条件。当前没有多人、存档恢复、护盾、免疫、暴击或复杂叠层的完整验证；ASC 的 replication 设置不能替代多人证据。生产控制状态授予链和所有攻击类别来源也未被本轮证明。

## Extension Directions

新资源入口应复用现有协调与回执边界，并验证 base/current、预留和最终结果；新效果应复用统一结算，避免技能行为重复实现生命/护甲规则。
