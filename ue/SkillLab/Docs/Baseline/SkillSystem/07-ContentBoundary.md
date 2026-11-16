# 当前样本、能力与边界

## 职责

集中记录技能系统的当前能力、验证状态、使用限制和待确认事项。具体业务规则由对应基线正文负责，详细交付依据通过稳定文档引用。

## 当前承诺

当前四个代表性样本及其目标、执行和效果类别见 [03-TargetingAndExecution.md](03-TargetingAndExecution.md)，通用职责方向不限于四项样本。

技能专属目标形态、目标关系、效果类型和执行组合由 Lua 表达；高频可调参数由 JSON 承载；C++/GAS/Runtime 提供稳定共用机制，GAS Ability 负责激活与施法生命周期。具体参数不因配置载体或样本运行而成为批准的产品规则。

动画 Cast Notify 通过 Gameplay Event 通知当前 Ability 到达释放点，由正式运行链推进释放和效果提交。Notify 不表示命中或效果。实际目标验证、命中、伤害、治疗、持续效果及移除由权威运行机制产生，表现只消费真实事实。

操作仲裁、输入缓存和提交规则见 [SkillSystem 入口](README.md)及对应正文；空间配置语义由 [表现事件基线](../CharacterPresentation/02-SkillActionPresentation.md)统一维护，不在本篇建立另一套规则。

## 当前实现与验证状态

| 功能 | 当前能力 | 验证边界 |
| --- | --- | --- |
| 内容定义与正式释放 | 已接入 Lua 内容声明、JSON 参数、有效定义加载及 Notify/Ability 释放链 | 已有局部运行确认，不代表全部定义组合、失败场景或迁移行为等价 |
| 代表性技能执行 | 投射物伤害、地面范围伤害、自疗和目标持续伤害已有基本执行与效果路径 | 受伤治疗、多目标、无效位置及异常清理等仍缺充分验证 |
| 操作仲裁与输入缓存 | 已有共享施法仲裁、控制状态消费、受击分类、单槽输入缓存与提交幂等能力 | 局部核验支持机制，不证明生产控制状态来源、攻击类别选择者或可游玩闪避内容已接通 |
| 空间配置 | 已接入真实投射物生成位置和事件驱动表现对象的定位、方向、视觉尺寸与适用持续跟随 | 不等于四技能最终空间适配完成；生产绑定、起点边界与完整观感仍缺证据 |

用户关于技能可正常释放的观察只支持基本可释放，不补足新规则、全部边界或玩家可见体验验收。局部实现核验与单个样本确认均不自动成为完整产品验收。

## 当前限制

- **Fireball**：已有既有障碍阻挡和真实生成点偏移的局部核验；未命中、偏移后近墙/遮挡与重叠出生、真实释放前移动后采样及生产 Socket 仍未充分验证。
- **AreaBurst**：已有空区域释放与局部可见表现的确认；多目标、无效位置、有效敌人伤害范围和不同视角仍未全面验收。
- **Mend**：受伤治疗仍缺运行验证，已有确认未覆盖这一场景。
- **清理与释放**：异常/死亡清理、死亡后 Reset 组合路径，以及 Notify 缺失、中断、延迟和 Timer 分支的完整行为仍未充分验证。
- **操作仲裁的生产覆盖**：未证明生产攻击来源选择普通/重击类别，未证明沉默/硬直的生产授予链；没有新增可游玩闪避或控制状态效果内容，不能宣称对应端到端体验通过。
- **时序与组合**：差异化时序窗口、规则组合和多提交内容的完整使用范围仍缺充分验证；局部仲裁核验不代表所有扩展组合已实现。
- **定义加载与脚本快照**：无效重载时有效目录与脚本的原子保持、磁盘脚本变化后的快照行为、Lua 受限环境及全部字段边界组合仍缺充分验证。
- **玩家可见体验**：完整 PIE、技能动作和空间观感尚未全面验收；AreaBurst 的局部可见确认不能外推到其他样本或完整适配。空间资源与跟随的能力限制见 [表现基线](../CharacterPresentation/02-SkillActionPresentation.md)。
- **内容与产品范围**：正式技能参数、阶段参数和优先级的产品批准状态尚未确认。资料导出、独立读取、AI 自主空间选点与调整未实现；网络多人、未确认的未来技能、效果类型和高级目标/控制规则不属于当前承诺。

## 待确认问题

- 正式技能参数和内容版本的产品批准边界是什么？
- 脚本失败时面向玩家的反馈范围及终态映射是什么？
- 内容版本的批准、回退和变更记录是否需要产品规则？

## 依据入口与关系

- 内容定义与正式释放：见 [技能定义需求](../../Planning/SkillSystem/Batches/SkillChainRefactor/README.md)、[实现摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/ImplementationSummary.md)、[内容迁移摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Implementation/ContentMigrationSummary.md)、[基础设施 Review](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Review/InfrastructureReview.md)、[内容 Review](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Review/ContentMigrationReview.md)及 [运行验证摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Implementation/RuntimeValidationSummary.md)。
- 操作仲裁与输入缓存：业务依据见 [操作规则需求](../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md)，实现和验证范围见 [实现摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRules/Implementation/CombatRulesImplementationSummary.md)及 [独立 Review](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRules/Review/CombatRulesReview.md)。
- 空间配置：业务依据见 [共用边界](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/README.md)与 [空间配置需求](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/01-SkillSpatialConfigurationSupport.md)，详细实现、验证和剩余风险见 [实现摘要](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/ImplementationSummary.md)及 [独立 Review](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/Review.md)。空间功能状态与视觉限制由 [表现验证基线](../CharacterPresentation/03-AssetsAndVerification.md)集中说明。

各功能交付保留自己的范围；空间配置不改变既有目标/效果类别、战斗规则或其他功能的验证结论。
