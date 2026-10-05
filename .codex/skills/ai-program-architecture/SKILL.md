---
name: ai-program-architecture
description: 为 UE 功能建立或修订程序架构基线、公共接口和职责映射；在实现 Agent 开始前处理跨模块结构问题。
---

# AI 程序架构

## 入口关系

本 Skill 由程序工作流总控通过独立本地任务对话调用。进入 UE 项目时，先完成 `ue-project-context` 的项目文档路由，再读取 `ai-native-programming`，最后读取本 Skill。总控必须把当前任务的策划目录、项目上下文和工作流产物路径写入对话上下文。

## 目标

在实现开始前建立或修订可执行的程序架构基线、公共接口和职责映射。架构 Agent 负责结构判断，不负责把整个功能直接实现完。

## 项目路径和交接

默认使用以下相对仓库路径：

- 领域输入：`ue/SkillLab/Docs/Planning/<TaskName>/`；
- 当前代码地图：`ue/SkillLab/Docs/ProjectContext/SkillLabCodeArchitecture.md`；
- 全局架构基线：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/ArchitectureBaseline.md`；
- 专题架构产物：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/Architecture/`。

路径含义固定为：`Planning/` 是项目策划知识，`ProjectContext/` 是项目通用事实，`ProgramWorkflow/<TaskName>/` 是当前任务的程序中间产物。策划目录和程序工作流目录使用同一个任务名。

专题任务的上下文和任务要求由总控直接在对话中传递。只有会被后续对话长期复用的架构结论才写入专题目录或更新基线。
## 必须读取

- `.codex/skills/ai-native-programming/SKILL.md` 中与当前任务相关的程序约束；

- 当前架构任务和用户目标；
- `ue/SkillLab/Docs/ProjectContext/README.md`、`SkillLabCodeArchitecture.md` 和相关模块文档；
- 当前任务的 `ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/` 基线和已有结论；
- 当前架构基线及其变更记录（如果存在）；
- `ue/SkillLab/Docs/Planning/<TaskName>/` 中与任务直接相关的策划总览和模块文档；
- 相关真实代码入口。

## 按需读取

- 直接上下游模块的接口约定；
- 现有 C++、配置、Blueprint 或资产的具体实现；
- 相关失败记录和以前的专题结论。

不要读取其他任务的 `ProgramWorkflow/<OtherTask>/`，也不要默认读取全部策划模块。

专题架构任务不默认读取无关模块。只有任务会改变全局依赖、公共身份、生命周期或接口时，才扩展到完整架构基线。

## 必须明确

- 负责模块、排除项和上下游依赖；
- 对象身份、生命周期、清理、重置和错误结果；
- 引擎原生能力、SkillLab C++、配置/结构化资产和 Blueprint 外壳的职责边界；
- 哪些是已确认规则、AI 建议默认值、未决问题和风险；
- 哪些接口供实现 Agent 使用，哪些仍是候选方案；
- 这次结论是局部专题还是会改变全局架构基线。
- 每条结论属于“当前冻结规则 / 本轮临时适配 / 未来候选 / 待用户判断”中的哪一类；未标记类别的内容不能直接交给实现 Agent 当作要求。

## 约束

不要为尚未证明的需求设计过度通用框架。不要把最终 C++ 类名、配置格式或 引擎能力映射假设成已确认事实。专题结论不能绕过总控直接成为全局事实。

## 交付

按共享 `ai-native-programming` 的分层规则组织产物：`ArchitectureBaseline.md` 保留本阶段范围、公共约束、状态和专题索引；可独立交接的模块或专题放在当前任务 `Architecture/` 下；未来候选单独标识并按需读取。不预建固定数量的文件，不把实施偏离直接改写为允许项。交付时指出各任务必读的基线章节和对应专题，而非要求所有实现对话读取所有架构材料。

架构交付必须附带“实现约束清单”：当前任务必须满足的接口、生命周期、配置边界、Blueprint 边界和验收行为；以及允许的临时适配、适用范围和替换条件。没有这份清单，实现 Agent 不应开始修改代码。

返回一份可交接的架构产物：目标、读取依据、职责和依赖、对象/生命周期模型、公共接口和不变量、技术映射、假设、风险、未决问题、影响范围和建议的实现任务。若更新基线，明确版本变化和被替换的旧结论。











