---
name: ai-program-module-implementation
description: 在已有架构基线和模块约定下，实现一个边界明确的 UE 模块或纵向功能切片，并留下可交接的实现摘要。
---

# AI 程序模块实现

## 入口关系

本 Skill 由程序工作流总控通过独立本地任务对话调用。进入 UE 项目时，先完成 `ue-project-context` 的项目文档路由，再读取 `ai-native-programming`，最后读取本 Skill。实现对话只处理总控交给的边界明确模块，不自行扫描或规划整个项目。

## 目标

在已有架构基线和模块约定下，实现一个边界明确的 UE 模块或纵向功能切片，并留下可被集成 Agent 和后续模块读取的实现摘要。

## 项目路径和交接

默认使用以下相对仓库路径：

- 领域规格：`ue/SkillLab/Docs/Planning/<TaskName>/` 中与当前模块对应的文档；
- 架构基线：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/ArchitectureBaseline.md`；
- 相关专题结论：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/Architecture/`；
- 实际实现：`ue/SkillLab/Source/SkillLab/`、`ue/SkillLab/Config/` 和任务明确授权的 `ue/SkillLab/Content/` 资产。

`<TaskName>` 是当前功能名，策划目录和程序工作流目录必须使用同一个任务名。`Planning/<TaskName>/`、`ProjectContext/` 和 `ProgramWorkflow/<TaskName>/` 分别表示策划知识、项目通用事实和当前任务交接；它们不是同一类上下文。

任务上下文包和本次交付默认通过对话传递，不要求为每个实现任务新建文档。若接口或实现说明需要跨多个后续对话复用，交由总控决定是否写入 `ue/SkillLab/Docs/ProgramWorkflow/`。
## 必须读取

- `.codex/skills/ai-native-programming/SKILL.md` 中与当前任务相关的程序约束；

- 总控给出的任务上下文包；
- `ue/SkillLab/Docs/ProjectContext/README.md`、架构地图和负责模块的项目文档；
- 当前任务 `ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/ArchitectureBaseline.md` 及适用专题结论；
- `ue/SkillLab/Docs/Planning/<TaskName>/` 中负责模块的策划文档；
- 直接依赖的上游交付摘要；
- 任务涉及的源码、配置、资产入口和现有验证入口。

## 按需读取

- 下游接口约定；
- 相关 Blueprint 或资产的真实状态；
- 与失败复现直接相关的其他源码。

不要默认读取全部策划文档、全部源码或无关模块。缺少关键上下文时先向总控报告需要哪一份产物。
不要读取其他任务的 `ProgramWorkflow/<OtherTask>/`。

## 工作

- 按基线实现任务范围内的代码、配置、结构化资产连接或必要 Blueprint 外壳；
- 优先复用已确认接口，保持身份、生命周期、清理和错误结果一致；
- 稳定运行逻辑优先放在 引擎原生能力/SkillLab C++，可变内容放在配置或结构化资产；Blueprint 只承担必要的外壳、组装和资源绑定；
- 对公共接口变化、跨模块修改和临时默认值做明确记录。

## 停止与回交

若需要改变公共接口、突破模块边界、引入核心 Blueprint 逻辑，或遇到未定义的体验规则，暂停该决策并回交总控。普通编译、类型、include、模块依赖和局部连接错误可以继续修复。

## 交付

返回：实现范围、读取依据、修改产物、公共接口变化、采用的假设、未实现范围、验证/复现入口、已知代码问题、需要用户判断的体验/架构/Blueprint 问题、对集成 Agent 的交接建议。











