# Batch03：AI 技能动作 Notify 配置能力程序入口

## 当前状态

动作总览和动作语义产物已按 AnimSequence 资产归档于 `Docs/ArtWorkflow/Actions/<assetName>/`，主要文件为 `overview.json`、分页图、`observation.md`；不再使用批次型 `Exports/` 和集中式 `Observations/` 作为正式归档。

通用程序能力已提供持久 Montage 的 Notify Patch 写入、保存、重载、回读和差异报告，以及技能定义的 `releaseTrigger` 解析和运行时 Notify 转发。它们只提供可用接口，不代表四个技能已经完成内容配置。

四技能持久 Montage、Notify 和 `SkillDefinitions.json` 绑定配置已完成。四项写入、保存、重载、独立进程回读、差异校验及 Provider reload 均通过；最终测试候选值以[配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)为准：Fireball f11，AreaBurst f36，Mend f15，BurningBuff f16。配置产物位于各技能 `Config/Animation/<SkillId>/`，源动作映射位于对应 `Actions/<assetName>/source-to-montage.json`。未启动游戏或 PIE，游戏内动作和事件时序仍待用户验收。

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

本入口只做导航和状态，不复制架构结论。本批分为两块：A 是 C++ Editor/Runtime 的持久 `UAnimMontage` 读取、结构化 Patch、白名单 Notify 写回/保存/重载/回读/差异校验，以及 Notify 桥的身份、取消和迟到保护；A 的通用能力已有实现与工具验证。B 是四个技能的持久 Montage、`SkillDefinitions.json` 绑定和 Notify 内容配置，现已完成结构写入、资产回读及 Provider 验证。剩余工作是用户在游戏内验证动作与事件时序。

职责边界：A 是通用代码与编辑器工具能力，由程序实现职责维护；B 是技能内容配置，由配置 Agent 使用现有工具完成，可修改本任务授权的 Montage 资产、Patch JSON 和技能定义，不得扩展 C++、Schema、解析器或工具。Montage 写入/回读通过不等于技能在游戏中的实际时序验收。

正式 Notify 资产目标是已保存的 `UAnimMontage`。`castAnimation` 动态 Montage 只作为历史/兼容运行路径，不作为本批 Notify 写回权威；每个动作与实际 Montage 的映射记录在对应 Actions 资产目录的 `source-to-montage.json`。动作观察、UE 资产和运行时事实的单一来源与边界见 [ArchitectureHandoff.md](ArchitectureHandoff.md)。
