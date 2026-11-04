# 四技能内容迁移摘要

## 范围和来源

迁移 `Skill_Fireball`、`Skill_AreaBurst`、当前仅自疗的 `Skill_Mend`、`Skill_BurningBuff`；不包含 Batch01 新增技能或 Mend Self/Ally 扩展。修改 `Config/SkillDefinitions.json` 与对应四份单技能 Lua。迁移前 JSON SHA-256 为 `504A2E3EBCF13399DD1A33F60686AD9A2A68EF804B503AFDB47703E0FE4B7DFF`，四份 Lua 均为 `24539FCCE29D65C9ED3A0DD076EB4F86843320D82B01336B5CD0603FF669F16F`，与 [迁移交接](../SkillDefinitionMigrationHandoff.md) 的候选快照一致；该快照不是已批准的产品版本。

## 内容归类与对照

目标形态、关系、效果类型和执行方式是技能专属分类，由各自 Lua 的单次 `SkillLabSkill_Define` 声明：

| 技能 | targetShape | relation | effect | execution |
| --- | --- | --- | --- | --- |
| Fireball | Unit | Enemy | Damage | Projectile |
| AreaBurst | Ground | Enemy | Damage | GroundArea |
| Mend | Self | Self | Healing | InstantTarget |
| BurningBuff | Unit | Enemy | DamageOverTime | InstantTarget |

高频可调数值放在 JSON：四项 `range` 分别为 1200、1000、800、900；AreaBurst 的 `radius` 为 350；Fireball 的 `projectileSpeed`、`projectileCollisionRadius`、`projectileLifetime`、`projectileVisualScale` 分别为 1250、18、5、0.62。`projectileInterceptOtherUnits=true` 是原候选拦截行为配置，按接口要求与投射物参数一起存于 JSON，并非纯数值。上述值来自旧定义候选快照，不构成新批准的业务规则。

原 JSON 的成本、冷却、效果量、系数、阶段、持续时间、周期、资源引用、释放事件、阻塞标签与脚本绑定均保留；Mend 未添加 `powerCoefficient`。原 `OnSkillCast` 内容不变，只请求声明动作并在完成时结束脚本，不裁定命中或效果。按[基建接口契约](../ImplementationSummary.md)和[独立 Review](../Review/InfrastructureReview.md)，分类以 Lua 为来源，参数以 JSON 为来源，加载后合成为单一规范化运行定义；Runtime/Targeting/GAS 保留权威结果。

## 检查与待验证

- 保存后从磁盘回读 JSON 并解析，逐项对比迁移前 HEAD 的旧字段和完整值；新增字段和值严格匹配上述候选，四项顺序及旧字段均一致。
- 用 `luaparse` 0.3.1 解析四份脚本成功；逐文件检查恰好一次声明、四个键和值，以及声明后的 `OnSkillCast` 与迁移前内容一致。
- 检查内容文件 diff，仅有上述授权 JSON 字段和四份 Lua 声明；本摘要为独立新增文件。未修改 C++、Schema、工具或动画资产。
- 未执行项目 Provider 对真实目录的加载/配置校验、Runtime Probe、游戏或 PIE；静态解析不证明运行等价。内容独立 Review 后由 PM 安排四项动作及 Targeting/GAS 真实结果与候选快照对照，包括 Fireball 命中/阻挡/未命中、AreaBurst 多目标/空区域、Mend 受伤/满生命、BurningBuff 周期/移除，以及费用、冷却和终态。当前没有可见 PIE 验证。
- 接口 Review 的证据缺口仍待补：正式 Provider 全局目录先成功再失败 reload 后 `Resolve()` 与 `LuaSource` 的原子保持未直接自动化；缓存 `LuaSource` 后修改或删除磁盘文件仍执行旧快照未自动化；标准库与禁止全局缺负向自动化；字段边界和组合未逐字段穷尽。此前加载失败的 Probe 不证明四项运行成功。
