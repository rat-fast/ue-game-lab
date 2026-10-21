# Lua 角色表现试点：程序任务入口

## 当前状态

本任务承接策划侧 [Batch02 Lua 角色表现试点](../../Planning/CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/README.md)。当前有效的程序交接与状态记录为 [ArchitectureHandoff.md](ArchitectureHandoff.md)。

该交接记录 Lua 接入和部分验证已完成，但 P0/P1 尚未通过：仍缺可见 PIE/视口验收，Fall 状态与新旧施法竞态的专门复现，以及技能差异化可见反馈验收。现有证据和限制以交接正文及 [角色表现模块事实](../../ProjectContext/Modules/CharacterPresentation.md)、[测试与验证事实](../../ProjectContext/Modules/TestAndVerification.md) 为准。

## 后续程序产物

- 实现状态与验证摘要目前并入 `ArchitectureHandoff.md`，不另建重复的 `Implementation.md`。
- `Review.md`：尚未形成一份针对本 Lua 批次最终需求与运行证据的独立 Review 文档。交接中提到的局部修复核对不等同于整批 Review 结论。
- `Evidence/`：尚无本批归档的可见 PIE/视口验收记录。

程序侧应在补齐证据后更新当前交接或对应单一产物，并同步 `ProjectContext/` 当前事实；本入口只负责路由和状态，不规定技术方案。
