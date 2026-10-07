# Batch01：六技能纵切片

## 批次定位

Batch01 先完成六个具体技能的业务规则，再从六份技能案中抽取公共基建。技能需求和基建需求可以交给不同实现线，但它们来自同一批，基建范围必须由技能覆盖关系证明。

当前技能文档是首批规则稿；默认数值和部分体验选择仍需 review 与试玩确认。GAS、C++、Blueprint、最终数据载体和 DSL 不在策划案中预设。

## 六个技能

| 技能 | 核心样本 | 主要公共能力来源 |
|---|---|---|
| [Firebolt 火焰弹](Skills/Skill_Firebolt.md) | 追踪单位目标投射物 | 单位目标、投射物、伤害、减速 |
| [Mend 治疗术](Skills/Skill_Mend.md) | Self/Ally 单体治疗 | 关系、实际治疗、溢出、NoChange |
| [ArcStrike 扇斩](Skills/Skill_ArcStrike.md) | 方向扇形多目标 | 方向快照、空间查询、去重 |
| [BurnField 烬地](Skills/Skill_BurnField.md) | 地面持续区域 | 地面目标、区域、周期、成员状态 |
| [ChannelBeam 引导光束](Skills/Skill_ChannelBeam.md) | 持续会话分段效果 | 引导会话、分段成本、直线查询、中断 |
| [Barrier 护盾](Skills/Skill_Barrier.md) | Self/Ally 持续护盾 | 吸收、替换、到期、耗尽 |

技能索引见 [Skills/README.md](Skills/README.md)。

## 基建需求

基建文档由技能需求反向抽取，只定义可复用业务能力和公共验收：

- [基建入口](Infrastructure/README.md)
- [00 公共能力范围](Infrastructure/00-CapabilityScope.md)
- [01 激活、资源与冷却](Infrastructure/01-ActivationResourceCooldown.md)
- [02 输入意图与统一请求](Infrastructure/02-InputIntentRequest.md)
- [03 目标关系与空间查询](Infrastructure/03-TargetingAndSpatialQuery.md)
- [04 执行对象与生命周期](Infrastructure/04-ExecutionObjectsAndLifecycle.md)
- [05 效果与持续状态](Infrastructure/05-EffectsAndPersistentStates.md)
- [06 终止、所有权与 Reset](Infrastructure/06-TerminationOwnershipReset.md)
- [07 公共能力验收](Infrastructure/07-SharedAcceptance.md)

基建不复制技能数值，也不把单个技能专属选择提升为公共规则。

## 交付关系

```text
六份技能业务规则
        ↓ 抽取共同依赖
Batch01 公共基建需求
        ↓ 提供公共能力
六个技能分别实现和验收
        ↓ 联合试玩
公共能力证据与技能体验证据
```

技能实现读取对应技能文档和本批基建；基建实现读取 `Infrastructure/` 和 `07-SharedAcceptance.md`，不需要从其他技能猜公共规则。发生公共语义冲突时，回到本批技能需求和基建范围共同 review。

## 批次状态

- 技能规则：六份独立文档已按统一模板整理，仍有文档内待评审项。
- 基建需求：按七个公共问题重写，尚未代表程序实现完成。
- 验收状态：全部为策划验收条件，实际运行证据另行记录。
- 长期方向：见 [LongTerm](../../LongTerm/README.md)，不自动扩大本批范围。
