# 代表性技能样本

## 用途

样本用于覆盖系统能力，不是商业技能列表。参数可调整，但每个样本必须保留目标、时序、效果、空间对象、表现和清理证据。

## 样本矩阵

| 样本 | 模板 | 目标 | 效果 | 主要证明 |
|---|---|---|---|---|
| Skill_TestPulse | Direct | 自身 | 瞬时属性修改 | 初始化、请求、激活、重置 |
| Firebolt | Projectile | 敌方单位 | 伤害+减速 | 投射物、命中重检、表现 |
| Mend | UnitTarget | 友方单位 | 治疗 | 友方过滤、目标失效 |
| BurnField | PersistentArea | 地面/敌方 | 周期伤害 | 区域存续、去重、重置 |
| ArcStrike | LineCone | 敌方范围 | 伤害+击退 | 方向、扇形查询、位移 |
| Barrier | Direct | 自身/友方 | 护盾 | 护盾吸收、状态事件 |
| ChannelBeam | Channel | 单位/方向 | 多段伤害 | 引导、受击打断、分段 |
| Cleanse | UnitTarget | 友方 | 驱散 | 状态筛选、优先级 |

## 每个样本的交接内容

必须记录设计意图、操作链、技能定义、模板、配置字段、目标解析与实际命中差异、资源/冷却/阶段、中断和效果叠加、表现事件、缺资源回退，以及无效目标、资源不足、冷却、取消、死亡和重置结果。

## 推荐顺序

先补 Skill_TestPulse 的运行证据，再做 Firebolt、BurnField、ChannelBeam，随后做 Mend、Barrier、Cleanse 和 ArcStrike。复杂样本不能绕过公共请求、效果和表现事件。
