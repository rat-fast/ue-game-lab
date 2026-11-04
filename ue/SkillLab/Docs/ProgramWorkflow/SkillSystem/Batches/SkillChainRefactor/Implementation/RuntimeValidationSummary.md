# 四项定义迁移后运行验证摘要

## 结论与范围

当前四项 JSON/Lua 在正式运行入口中成功加载，Runtime Probe 中四项技能均成功请求并进入运行链。Fireball、AreaBurst 的一次目标伤害结果，以及 BurningBuff 的施法后周期伤害和移除事件，均有日志证据。Mend 两次均在满生命状态施放，不能据此验收受伤治疗。

本记录只报告迁移后候选内容的运行事实，不批准候选数值为产品规则，也不宣称所有技能行为等价或完成 PIE 验收。运行结果与旧 RuntimeProbe 中可观察的实际结算值相符，但两个 Probe 都只覆盖有限场景；迁移前快照仍是候选对照，不是已批准版本。

## 证据

- 正式 NullRHI RuntimeProbe：`Saved/Logs/SkillChainRefactor-MigrationValidation-RuntimeProbe.log`。日志接受完整目录（`Definition load accepted: 4 skills`），记录 Probe 完成；进程退出码为 0。
- D3D11 窗口 RuntimeProbe：`Saved/Logs/SkillChainRefactor-MigrationValidation-Viewport.log`。窗口模式日志同样接受四项目录并记录 Probe 完成；同时有 Niagara Toolsets Python 初始化 `AttributeError`，但后续 Probe 仍运行至完成。它不等同于交互式 PIE 验收。
- 可见性检查截图：`Saved/Logs/SkillChainRefactor-MigrationValidation-Viewport.png`。截图捕获到 Windows 锁屏，未显示游戏视口，因此不作为游戏画面或视觉验收证据。
- 候选对照：旧日志 `Saved/Logs/SkillChainRefactor-RuntimeProbe.log` 属于迁移前候选运行结果，不是批准版本。仅对本次实际覆盖的有限路径比较日志中的请求量、实际结果和资源/冷却记录。
- 内容输入：运行时加载的 `Config/SkillDefinitions.json` 与四份 `Config/Skill_*.lua` 的 SHA-256 已与内容迁移 Review 所审文件核对一致；本次未改配置、Lua、源码、Blueprint 或资产。

## 观测结果

NullRHI Probe 以角色自身作为测试目标。记录到：

| 样本 | 运行观察 | 边界 |
| --- | --- | --- |
| Fireball | 请求成功；单个目标由 100 降至 60，Probe 记载请求量 30、实际伤害 40；能量从 85 降至 75，冷却记录为 1 秒；随后投射物结果为 `Hit`。 | 只证明一次命中，不覆盖阻挡或未命中。 |
| AreaBurst | 请求成功；单个目标由 60 降至 35，请求量 20、实际伤害 25；能量从 75 降至 55，冷却记录为 3 秒。 | Probe 仅观察一个范围目标，不覆盖独立多目标、空区域或无效位置。 |
| Mend | 两次请求均成功，均以 100/100 生命施放；每次请求治疗 40，实际治疗 0。两次施法费用均被提交，冷却记录为 2 秒。 | 满生命结果不能证明受伤治疗路径。 |
| BurningBuff | 请求成功；施法提交后目标收到三次各 8 点周期伤害，生命由 35 降至 11；之后记录 `BuffRemoved` 与 `EffectTrackingCleared`。费用为 10，冷却记录为 2 秒。 | 施法终态与后续 DoT tick 分开记录；仅覆盖该目标和该次周期效果。 |

这些有限观察值与迁移前候选 Probe 对应路径中的记录一致：Fireball 实际伤害 40、AreaBurst 实际伤害 25、Mend 满生命时实际治疗 0、BurningBuff 三次 8 点周期伤害及效果跟踪清理。它们不能替代相同场景、相同初始状态的完整逐项对照，也不能推导未执行场景的等价性。

## 未验证项

- Mend 受伤后的实际治疗量。
- Fireball 被阻挡和未命中的结果。
- AreaBurst 独立多目标、空区域和无效位置结果。
- 异常或死亡条件下的效果清理，以及死亡后 Reset 组合路径。
- 可见交互式 PIE/游戏视口及动画 Notify 的玩家可见表现。D3D11 窗口截图是锁屏画面，不构成通过证据。
- Provider 已成功加载后再进行坏 reload 时，旧全局目录及 LuaSource 的原子保持。
- 缓存 LuaSource 后磁盘脚本发生变化或被删除时，仍执行缓存快照。
- 标准库与禁止全局的负向自动化，以及所有字段边界和组合的穷举验证。

基础设施 Review 中的这些缺口继续保持未验证；本轮没有通过修改代码或配置扩展覆盖范围。
