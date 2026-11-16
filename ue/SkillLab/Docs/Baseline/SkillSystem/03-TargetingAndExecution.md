# 目标与执行语义

## 职责

记录当前四个样本已确认的目标形状、关系和执行类型，不把具体碰撞、距离、查询算法或命中回调写成公共业务规则。

## 当前承诺

| 技能 | 目标与关系 | 执行类别 | 效果类别 |
| --- | --- | --- | --- |
| `Skill_Fireball` | 敌方单位 | 投射物 | 伤害 |
| `Skill_AreaBurst` | 地面位置；作用于范围内敌方单位 | 地面范围 | 伤害 |
| `Skill_Mend` | 自身 | 即时目标 | 治疗 |
| `Skill_BurningBuff` | 敌方单位 | 即时施加持续效果 | 持续伤害 |

目标被选择、技能开始或动作播放不等于命中。命中与受影响对象须由权威技能运行事实表达，实际效果须由权威结算产生。

真实投射物生成位置可通过空间配置沿正式释放/提交链调整；这可能改变初始轨迹与碰撞结果，但不授权修改目标合法性、碰撞策略或战斗规则。纯表现的位置、方向和视觉尺寸不改变实际作用位置或范围；表现对象的边界不能自动当作伤害边界。Mend 仍自疗，BurningBuff 仍作用于既有敌方目标。业务来源见 [空间适配共用边界](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/README.md)及 [空间配置需求](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/01-SkillSpatialConfigurationSupport.md)，空间语义与支持范围统一见 [表现基线](../CharacterPresentation/02-SkillActionPresentation.md)。

## 当前实现与验证状态

Lua 表达技能专属目标形态、关系、效果类型和执行组合，JSON 承载高频可调参数，运行层消费加载后的有效定义。四项样本已有基本执行与效果路径的局部运行确认，真实投射物生成点配置和视觉/碰撞隔离也已有局部核验；这些确认不证明所有场景行为等价或最终适配完成。详细依据由 [内容与能力边界](07-ContentBoundary.md)引用的交付文档维护。

## 当前限制

实际目标选择细节、具体距离/范围数值和超出已确认场景的结果不在本篇冻结。Fireball 偏移后近墙/遮挡、生产 Socket、AreaBurst 有效敌人范围及四技能完整空间观感仍未充分验证，见 [表现验证边界](../CharacterPresentation/03-AssetsAndVerification.md)。

## 关系

效果语义见 [04-EffectsAndStates.md](04-EffectsAndStates.md)；表现事件边界见 [06-FeedbackAndObservability.md](06-FeedbackAndObservability.md)；各样本使用限制见 [07-ContentBoundary.md](07-ContentBoundary.md)。
