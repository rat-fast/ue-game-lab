# Batch02：Lua 角色表现流程试点

## 批次定位

本批是角色表现主流程的 Lua 试点策划需求，目标是以小范围、可观察、可回归的角色体验验证表现流程。Lua 是用户选定的试点方向；本批定义玩家体验与表现边界，不规定 UE 接入、插件、接口、脚本目录、数据格式、调用方式或代码迁移方案。

## 当前入口与需求文档

- [主需求：Lua 角色表现试点](CharacterPresentationLuaPilotRequirements.md)：阶段、玩家可观察需求、验收条件、资源依据、排除项及待确认/待核实问题。
- [程序任务入口](../../../../ProgramWorkflow/CharacterPresentationLuaPilot/README.md)：当前程序状态与后续产物路由；其中链接到[架构交接与实现验证](../../../../ProgramWorkflow/CharacterPresentationLuaPilot/ArchitectureHandoff.md)，记录 C++/Lua/UE 分工、实现结果、测试证据与未完成的可见验收门槛。
- 本批是角色表现策划目录当前的 Lua 试点入口。
- [Batch01：AI 适配的动画表现链路](../Batch01-AIAdaptedAnimationPipeline/README.md)是先前需求和交接参考，不是当前入口；仅选择性复用仍适用的体验边界。

## 范围摘要

- P0：`Idle`、`Move`、`Jump`、`Fall`、`Land` 基础状态；火球技能动作与施法运行事实的首条纵向链。
- P1：区域爆发、修复、燃烧的差异化表现；取消/完成、身份隔离、基础姿势恢复及持续效果边界。
- P2 候选：受击表现和正式 VFX。共享基线没有足够依据把它们列为本批必须通过项。

本批不扩成全游戏脚本迁移，不改变 GAS 或技能业务规则，不要求移除 C++、Blueprint 或 UE 资产，不重写 UE 动画求值、骨骼融合和姿势混合。
