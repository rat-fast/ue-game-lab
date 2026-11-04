# 四技能内容迁移独立 Review

## 结论

**静态接口 Review 通过，可进入四项运行验证；不代表运行行为等价或 PIE 通过。**本次仅审查 `Skill_Fireball`、`Skill_AreaBurst`、当前自疗 `Skill_Mend`、`Skill_BurningBuff`。未发现阻止进入运行验证的内容字段或接口兼容问题。

## 发现

无阻塞发现。实际配置 diff 限于 `Config/SkillDefinitions.json` 和上述四份 Lua：JSON 为四项增加加载器要求的数值字段，Lua 各增加一次分类声明。未见 Batch01 内容或 Mend Self/Ally 扩展混入。

## 核对依据

- 对照[定义迁移交接](../SkillDefinitionMigrationHandoff.md)的候选快照及本轮[配置生产需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillContentProductionRequirements.md)、[共同约束](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)：声明为 Fireball `Unit/Enemy/Damage/Projectile`、AreaBurst `Ground/Enemy/Damage/GroundArea`、Mend `Self/Self/Healing/InstantTarget`、BurningBuff `Unit/Enemy/DamageOverTime/InstantTarget`；JSON range 为 `1200/1000/800/900`，AreaBurst radius 为 `350`，Fireball 投射物参数为 `1250/18/5/0.62/true`。工作树逐项匹配。
- JSON 原有 cost、cooldown、magnitude、powerCoefficient（Mend 仍无该字段）、windup/recovery、BurningBuff duration/period、动画软路径、Lua 绑定、GameplayEvent/tag/timeout 和 blockedBy 均保留。四个 Lua 各有且仅有一次 `SkillLabSkill_Define`，声明之后的 `OnSkillCast` 与候选旧脚本一致；函数只调用 `SkillLabSkill_RequestAction()`，完成后调用 `SkillLabSkill_Finish()`。
- 对照加载期接口契约（[实现摘要](../ImplementationSummary.md)“加载期定义接口”小节）：字段名、枚举和值与 Lua 定义接口一致，必需 `range`、Ground `radius` 及 Projectile 五项 JSON 字段齐备。`InstantTarget`、`Projectile`、`GroundArea` 均为接口支持的执行方式；迁移未改 Provider、Schema 或公共运行接口。
- 独立复读工作树 JSON 并用 PowerShell JSON parser 解析，核对四项顺序和字段值；真实配置 diff 为 JSON 4 行替换、每份 Lua 仅插入 2 行声明。作者摘要[内容迁移摘要](../Implementation/ContentMigrationSummary.md)报告保存后回读并逐项比较旧字段、`luaparse` 0.3.1 解析四份 Lua、逐文件确认声明次数/值及原函数、以及 diff 范围核验。本 Review 环境没有 `luaparse` 安装，未独立重跑该解析器；Lua 兼容性此项依据作者记录和 diff 核对。

## 运行验证与剩余证据

内容可进入 Runtime Probe/真实 Provider 加载和四项运行验证。尚未验证真实 Provider 全目录成功加载、Runtime Probe 成功请求、四项动作/Targeting/GAS 结果与候选快照的终态对照，亦无可见 PIE 证据。不得据静态检查或旧 Probe 声称运行成功、行为等价或视觉验收通过。

以下加载期接口 Review 缺口仍是未验证项，不构成本次配置修改阻塞：Provider 全局目录先成功再坏 reload 后旧 `Resolve()` 与 `LuaSource` 原子保持；缓存后磁盘 Lua 变化仍执行快照；标准库/禁止全局负向自动化；所有字段边界和组合尚未穷举。静态源码/接口判断见[基础设施 Review](InfrastructureReview.md)，应保持其未验证状态。

## 最小后续验证

由实现/验证负责人使用当前四项配置运行真实目录加载和 Runtime Probe，再按[共同约束验收表](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)核对四项动作、目标、实际 GAS 结果、费用/冷却与终态，并保留可复现日志；随后执行适用的可见 PIE 检查。报告每项未运行场景，不以配置解析通过替代结果对照。
