# SkillSystem 程序工作流

旧程序架构、实现交接和验证文档已删除；各新任务依据当前代码、共享功能基线和当前策划重新建立程序产物。

## 输入关系

- `../../ProjectContext/`：当前代码事实和代码定位；
- `../../Baseline/SkillSystem/`：当前共享功能基线；
- `../../Planning/SkillSystem/`：长期目标、当前批次目标和具体内容设计；
- `LongTerm/`：长期程序框架；
- `Batches/<BatchName>/`：某一批次的架构基线、实现交接和 Review 状态。

## 当前批次

- [SkillCombatRules](Batches/SkillCombatRules/README.md)：已确认规则的 Runtime/GAS 实现、定向自动化与独立 Review 已完成；Review/验证结果和剩余内容入口事实见专题摘要。
- [SkillChainRefactor](Batches/SkillChainRefactor/README.md)：通用技能运行基建与当前符合范围定义迁移的架构交接。

## 读取规则

不同 Agent 的读取清单由当前架构交接包明确给出。不得扫描整个 `Docs/`，不得恢复旧程序文档中的接口、类名、验证结论或样例假设。

## 当前状态

SkillChainRefactor 已有运行链交接和[加载期 Lua/JSON 技能定义能力架构交接](Batches/SkillChainRefactor/SkillDefinitionLoadingHandoff.md)；后续由 C++/GAS 通用基建线实现并独立 Review。配置迁移暂停，恢复条件见[本批程序入口](Batches/SkillChainRefactor/README.md#配置迁移恢复门槛)：须完成定义接口交接、基建补充实现及独立 Review，并由 PM 重新派发。
