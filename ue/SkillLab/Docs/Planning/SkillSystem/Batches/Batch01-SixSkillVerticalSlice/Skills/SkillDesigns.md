# 六技能独立设计索引

## 定位

本目录每个技能一份正文。技能文档只描述该技能的操作意图、目标、阶段、参数、效果、表现和验收；公共能力由 `Infrastructure/` 维护，程序技术接口由 `ProgramWorkflow/SkillSystem/` 维护。

所有数值与默认交互仍是建议稿，待用户 review 与试玩验证。

## Batch01 技能

| 技能 | 独立设计 | 主要验证内容 |
|---|---|---|
| Skill_Firebolt 火焰弹 | [Skill_Firebolt.md](Skill_Firebolt.md) | 追踪投射物、伤害、减速 |
| Skill_Mend 治疗术 | [Skill_Mend.md](Skill_Mend.md) | 单体治疗、实际值、溢出、NoChange |
| Skill_ArcStrike 扇斩 | [Skill_ArcStrike.md](Skill_ArcStrike.md) | 方向快照、扇形、多目标去重 |
| Skill_BurnField 烬地 | [Skill_BurnField.md](Skill_BurnField.md) | 独立区域、周期、成员状态 |
| Skill_ChannelBeam 引导光束 | [Skill_ChannelBeam.md](Skill_ChannelBeam.md) | 按住会话、分段成本、受伤中断 |
| Skill_Barrier 护盾 | [Skill_Barrier.md](Skill_Barrier.md) | 吸收、Replace、到期与清理 |

## 阅读关系

- [技能配置入口](README.md)：本层职责和消费约定。
- [核心纵切片](../README.md)：批次范围与覆盖。
- [Batch01 基建需求](../Infrastructure/README.md)：公共能力实施子集和验收，不替代本技能正文。


