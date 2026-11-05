# SkillCombatRules 程序入口

本专题对应策划需求 [SkillCombatRules](../../../../Planning/SkillSystem/Batches/SkillCombatRules/README.md)。策划正文是需求权威来源；[架构交接](CombatRulesArchitectureHandoff.md)记录源码核实与职责边界，已确认产品规则以需求第 8 节为准。

## 状态

- 操作/打断、单槽缓存、资源/冷却、Notify 与多提交规则已确认，见策划案第 8 节；独立 Review 已确认缓存自然完成后重新门控消费通过源码及定向自动化复查，B1 已解除。详见 [独立程序 Review](Review/CombatRulesReview.md)。
- 受击入口现接收权威伤害调用显式传入的攻击类别 Gameplay Tag；当前没有战斗内容生产者自动赋予普通/重击类别。Stunned/Silenced 有效 Tag 会门控并中断 Runtime Cast，但当前未发现生产授予者。
- Review/验证状态：缓存修复 Review 已解除阻塞；UE 5.8 当前工作树构建、定向 CastGameplayEvent 与 CastLifecycle/DefinitionValidation/GAS DamageCalculation 回归均通过。生产攻击类别选择者、Stunned/Silenced 授予者、可见 PIE 和 Notify 超时独立运行仍未覆盖，详见[实现摘要](Implementation/CombatRulesImplementationSummary.md)与[Review](Review/CombatRulesReview.md)。测试注入只证明 Runtime 对有效 Tag 的消费行为。
- 原技能链路改造 `SkillChainRefactor` 独立；本专题只复用其已成立的当前运行事实，不继承其范围排除。

## 当前交接

- [CombatRulesArchitectureHandoff.md](CombatRulesArchitectureHandoff.md)：运行链、源码事实、实现任务边界、已确认规则与内容来源缺口。
- [CombatRulesImplementationPlan.md](CombatRulesImplementationPlan.md)：已确认规则的数据责任、实现切片和定向测试设计。
- [Implementation/CombatRulesImplementationSummary.md](Implementation/CombatRulesImplementationSummary.md)：改动、测试、构建和未覆盖场景；Review 报告由独立 Reviewer 维护。
