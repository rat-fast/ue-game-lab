# Batch01 基建需求

## 这组文档解决什么问题

本目录从 Batch01 的六份技能需求中抽取公共能力。它回答“多个技能共同需要什么业务能力、能力怎样配合、怎样验收”，不重新描述某个技能的数值、手感或专属规则。

六个技能文档仍是需求起点：

- [Firebolt](../Skills/Skill_Firebolt.md)：单位目标、追踪投射物、伤害与减速；
- [Mend](../Skills/Skill_Mend.md)：Self/Ally、治疗实际值、溢出和 NoChange；
- [ArcStrike](../Skills/Skill_ArcStrike.md)：方向快照、扇形、多目标去重；
- [BurnField](../Skills/Skill_BurnField.md)：地面落点、独立区域、周期和成员状态；
- [ChannelBeam](../Skills/Skill_ChannelBeam.md)：持续会话、分段支付、直线查询和中断；
- [Barrier](../Skills/Skill_Barrier.md)：护盾吸收、替换、到期和耗尽。

## 阅读顺序

1. `00-CapabilityScope.md`：先确认本批公共范围和每项能力的技能来源；
2. `01-ActivationResourceCooldown.md`：确认技能什么时候可以开始、何时扣费和启动冷却；
3. `02-InputIntentRequest.md`：确认玩家、AI、Test 怎样形成统一请求和持续会话；
4. `03-TargetingAndSpatialQuery.md`：确认 Self/Ally/Enemy、单位/地面/方向和空间候选怎样解析；
5. `04-ExecutionObjectsAndLifecycle.md`：确认 Cast、投射物、区域和引导会话怎样运行；
6. `05-EffectsAndPersistentStates.md`：确认伤害、治疗、护盾、减速和周期状态怎样返回实际结果；
7. `06-TerminationOwnershipReset.md`：确认取消、打断、死亡、目标失效和 Reset 怎样终止及清理；
8. `07-SharedAcceptance.md`：只验收公共能力，技能数值和最终手感回到 `Skills/`。

## 文档边界

- 技能文档拥有具体技能的输入选择、目标关系、数值、阶段选择、效果组合和专属验收。
- 本目录拥有六个技能共同依赖的公共语义、不变量、跨技能接口要求和公共验收。
- 长期目标目录记录未来能力方向；它不自动扩大 Batch01 的实施范围。
- 程序文档记录技术映射、当前实现状态和验证证据；本目录不指定 GAS 类、C++ 文件、Blueprint、资产类型或 DSL。

## 读者拿到本目录后应知道什么

程序实现公共能力时，先读本目录；实现某个技能时，再读对应技能文档。每份文档都明确“固定业务规则、技能可配置内容、运行时事实和程序实现边界”，因此程序不需要从另一份技能案猜公共规则，也不应把技能专属数值写入公共逻辑。
