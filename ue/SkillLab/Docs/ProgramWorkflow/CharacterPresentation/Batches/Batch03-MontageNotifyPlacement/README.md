# Batch03：AI 技能动作 Notify 配置能力程序入口

## 当前状态

本批已完成动作总览工具实现及技能定义的 `releaseTrigger` 解析基础设施。动作总览产物统一归档到 `Docs/ArtWorkflow/Exports/`，AI 观察记录归档到 `Docs/ArtWorkflow/Observations/`。当前四个技能仍使用 `castAnimation` 和默认 `Timer`，尚未完成持久 Montage 写回、四技能 Montage 绑定、Notify 点位配置或游戏内时序验收。

需求入口为：

- `Docs/Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/README.md`
- `Docs/Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/AnimSequenceActionOverviewRequirements.md`
- `Docs/Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/MontageNotifyPlacementRequirements.md`

共享业务语义按需求入口读取角色表现与技能系统基线。旧 `AnimationWorkflow/Batch01` 仅作为现有 Editor 内容工具、读写回读和历史运行边界的复用依据，不作为本批单样本验收或当前实现结论。

## 交接映射

| 方向 | 当前交接 | 说明 |
|---|---|---|
| 动作总览程序架构 | [ActionOverviewArchitectureHandoff.md](ActionOverviewArchitectureHandoff.md) | AnimSequence 观察、分页图、manifest、输出路径、播放映射和下游输出边界 |
| Notify 配置与运行接入 | [ArchitectureHandoff.md](ArchitectureHandoff.md) | 多事件写回、回读、事件声明、身份/取消/迟到保护和运行接入 |
| 具体内容与游戏内验收 | 两份交接第 7、8、10 节 | 依赖具体技能事件、动作和容差确认 |

本入口只做导航和状态，不复制架构结论。后续实现拆为两块：A 为 C++ Editor/Runtime 的持久 `UAnimMontage` 读取、结构化 Patch、白名单 Notify 写回/保存/重载/回读/差异校验，以及现有 Notify 桥的身份、取消和迟到保护；B 为四个技能的持久 Montage 创建或绑定、`SkillDefinitions.json` 的 `castMontage`、按动作观察和业务语义填写 Patch，以及仅必要的 `GameplayEvent`/Lua 调整。A 完成并验证后，B 才能按具体技能配置和验收。

正式 Notify 资产目标是已保存的 `UAnimMontage`。`castAnimation` 动态 Montage 只作为当前代码事实，不作为本批 Notify 写回权威；动作观察、UE 资产和运行时事实的单一来源与边界见 [ArchitectureHandoff.md](ArchitectureHandoff.md)。
