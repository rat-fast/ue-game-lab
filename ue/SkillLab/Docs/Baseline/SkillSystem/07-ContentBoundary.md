# 当前样本、证据与边界

## 职责

集中记录本领域当前承诺的范围、证据版本、实现状态、限制和待确认事项。策划批次范围及验收矩阵仍以对应 Planning 文档为准。

## 当前承诺

当前四个迁移样本及其目标/执行/效果类别见 `03-TargetingAndExecution.md`。本轮用户确认的通用职责方向适用于当前技能系统：技能专属目标形态、敌我/目标关系、效果类型和执行组合由 Lua 表达，不硬编码在 C++；高频反复调试的纯数值（如伤害、距离、速度、范围/半径及适用的投射物参数）由 JSON 承载；具体数值不因载体或迁移而自动成为产品基线。稳定共用机制由 C++/GAS/Runtime 提供，GAS Ability 负责激活和施法生命周期。

动画 Cast Notify 通过带 Tag 的 Gameplay Event 通知当前 Ability 到达释放点，由当前 Ability 推进正式释放和效果提交。Notify 只表示释放时点，不表示实际命中或效果。权威运行机制负责实际目标验证、命中、伤害/治疗/持续效果等结果；表现消费真实事实，不伪造业务结果。

Batch01 六技能扩展不属于本轮内容迁移；四项当前配置是代表性迁移样本，不限制通用职责方向的适用范围。当前候选内容中的具体数值、阶段、优先级及其产品批准状态，除已明确确认的样本语义外，不从实现或单次运行结果推定。

## 实现与验证证据

- **确认来源**：本轮用户明确了 Lua/JSON 职责、Ability 与 Cast Notify 释放契约，并补充“当前技能能在游戏中正常释放”的基本流程观察。用户观察只作为该范围的人工报告记录，不扩展为全场景或视觉验收结论。
- **程序实现事实**：[实现摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/ImplementationSummary.md)和[内容迁移摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Implementation/ContentMigrationSummary.md)记录：Lua 声明技能专属分类，JSON 承载数值，加载期合并为唯一规范化运行定义；技能专属 C++ Builder 已移除，通用 Ability/Behavior、Runtime、TargetingService、EffectExecutor 和 SkillLabGAS 保留权威职责。Cast Notify 经校验后以带身份的 Gameplay Event 到达当前 Ability，再经 Runtime 校验推进正式释放。以上是实现现状，不额外冻结内部 API 或 Schema。
- **独立 Review**：[基础设施 Review](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Review/InfrastructureReview.md)通过了事件链和加载期定义接口 Review；[内容迁移 Review](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Review/ContentMigrationReview.md)通过静态内容 Review。Review 通过不等同于所有运行场景或视觉验收通过。
- **自动化与构建**：程序记录 UE 5.8 `SkillLabEditor Win64 Development` 构建成功，定义加载、施法生命周期及 Gameplay Event 定向自动化通过。自动化覆盖部分有效/无效定义、边界组合和事件身份路径，未穷尽所有字段组合或异常场景。
- **有限运行观察**：[运行验证摘要](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/Implementation/RuntimeValidationSummary.md)记录迁移后候选内容通过 Provider 加载并进入四项 Probe。Fireball 一次单目标路径请求伤害 30/实际伤害 40；AreaBurst 一个目标请求 20/实际伤害 25；Mend 两次满血治疗请求各 40/实际 0；BurningBuff 三次各 8 点周期伤害后移除并清理跟踪。只对有限已运行路径与迁移前候选 Probe 记录相符；候选快照不是批准产品版本，不能证明未运行场景行为等价。
- **有限度说明**：NullRHI 结果是运行日志证据，不是玩家可见视口验收。另有用户进游戏观察“当前技能能正常释放”，可支持基本流程可释放这一观察，不能代替指定边界检查或正式 PIE 全面验收。

## 当前限制

- **Fireball**：阻挡与未命中未验证。
- **AreaBurst**：多目标、空区域及无效位置未验证。
- **Mend**：受伤治疗未验证；当前 Probe 仅覆盖满血治疗。
- **清理与释放**：异常/死亡清理及死亡后 Reset 组合路径未验证；Notify 缺失、中断、延迟以及 Timer 分支的既有行为对照未完成。
- **定义加载与脚本快照**：Provider 成功加载后再坏 reload 时旧目录和 LuaSource 原子保持未验证；缓存 LuaSource 后磁盘脚本变更时的快照行为未通过自动化验证；Lua 标准库/禁用全局负向自动化缺失；字段边界及组合未穷尽。
- **玩家可见验收**：正式可见 PIE 画面未验证；D3D11 截图被锁屏遮挡，不能作为游戏视口证据。用户报告的基本释放观察不补足此项。
- 具体技能数值是否批准为产品规则、部分阶段参数、优先级、脚本失败的玩家反馈及内容版本批准/回退规则均未确认。
- 网络多人、未来技能、其他效果类型以及高级目标/控制规则不属于当前承诺。
- 程序实现事实、候选快照数值、局部自动化或单次运行结果不自动提升为业务规则或完整验收结论。

## 待确认问题

- 四项候选数值和阶段参数中，哪些应批准为产品规则或冻结为正式对照内容版本？
- 脚本失败时面向玩家的反馈范围及终态映射是什么？
- JSON 内容版本的批准、回退和变更记录是否需要产品规则？

## 关系

本轮需求入口：[SkillChainRefactor](../../Planning/SkillSystem/Batches/SkillChainRefactor/README.md)。程序实现、迁移及验证状态见对应 [SkillChainRefactor 程序交接](../../ProgramWorkflow/SkillSystem/Batches/SkillChainRefactor/README.md)。旧 Batch02 仅作相容样本背景；冲突方向以本基线当前承诺和本轮确认内容为准。
