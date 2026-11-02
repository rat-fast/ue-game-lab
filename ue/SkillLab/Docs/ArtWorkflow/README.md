# Art Workflow

## 目的

本目录是 SkillLab 的动作与角色表现工作流入口。它连接动作导出、AI 动作观察、技能 Notify 配置和最终游戏内验收，但每类产物保持独立来源。

## 目录

- `Exports/`：动作总览工具生成的逐帧图片、分页图、`manifest.json` 和批次 `batch.json`。这些文件是实际生成产物，不是人工编辑的语义结论。
- `Observations/`：AI 或内容负责人根据总览图写出的动作观察记录。记录必须引用 `generationId`、输入指纹、源帧和页格证据，不得把观察直接当成已写入 Notify 的事实。

## 工作链

```text
AnimSequence
  -> SkillLabActionOverview（C++ Editor，只读采样和 RHI 渲染）
  -> Exports/（帧图、分页图、manifest）
  -> AI 动作观察（Observations/）
  -> Notify 配置请求（结构化文本）
  -> C++ Editor 写入持久 UAnimMontage
  -> 保存、重载、回读和结构校验
  -> PIE/游戏内验证
```

动作总览只负责观察材料，不修改动画资产、Montage、Notify 或技能配置。观察记录可以提出候选阶段和候选时间，但必须保留不确定性；只有经过实际播放映射和内容验收后，才能进入 Notify 配置。

## 相关文档

- 动作总览需求：`../Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/AnimSequenceActionOverviewRequirements.md`
- 动作总览实现：`../ProgramWorkflow/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/ActionOverviewImplementation.md`
- Notify 架构交接：`../ProgramWorkflow/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/ArchitectureHandoff.md`
- Notify 实现摘要：`../ProgramWorkflow/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/ImplementationSummary.md`

## 观察记录格式

`Observations/` 中的 JSON 是 AI 可读的事实记录。每个判断至少包含：动作阶段、源帧范围、对应分页格、判断依据和不确定性。动作观察不能单独证明 Notify 已存在、技能已释放或业务效果已发生。
