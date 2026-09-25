---
name: ai-program-workflow-orchestrator
description: 协调大型 UE 功能的多对话程序实现；读取最小必要上下文，创建本地任务对话、接收交付并持续推进到可运行结果。仅用于总控协作，不替代模块实现。
---

# AI 程序工作流总控

## 目标

把用户给出的策划目标和项目现状转换成可执行的多 Agent 实现流程，协调架构、模块实现、集成和问题处理，直到项目达到用户可以进入 PIE 测试的状态。

总控负责任务、上下文、依赖和状态，不负责替代专项 Agent 设计每个模块或逐行检查代码。总控通过 `create_thread` 创建独立任务对话，通过对话消息传递上下文、接收交付和继续推进。创建时先用 `list_projects` 定位当前项目，再使用该项目的 `projectId` 和 `target.environment.type=local`；使用当前本地工作树状态，不创建新的 worktree。

## 固定 Skill 入口

总控必须直接知道并按需使用以下程序 Skill（相对仓库根目录）：

- `.codex/skills/ue-project-context/SKILL.md`：UE 项目入口和项目文档路由；
- `.codex/skills/ai-native-programming/SKILL.md`：程序实现的 AI Native 约束；
- `.codex/skills/ai-program-workflow-orchestrator/SKILL.md`：总控自身的调度规则；
- `.codex/skills/ai-program-architecture/SKILL.md`：架构基线和公共接口；
- `.codex/skills/ai-program-module-implementation/SKILL.md`：模块或纵向切片实现；
- `.codex/skills/ai-program-integration/SKILL.md`：跨模块接入和可运行集成；
- `.codex/skills/ai-program-validation-log/SKILL.md`：构建、运行、PIE 结果和待判断问题。

总控必须在任务开始时读取 `ue-project-context`、`ai-native-programming` 和后四个专项 Skill 的当前内容，确认它们的输入、输出和职责边界；不需要把这些 Skill 的全部业务上下文复制进自己的任务。

## 模型选择

根据任务难度选择模型，不默认所有子任务使用同一个模型：

- 全局架构、跨模块公共接口、重大职责冲突：`gpt-6-astra`，中等推理强度；
- 模块专题架构或复杂集成设计：优先 `gpt-6-astra`，中等推理强度；
- 已有接口下的直接模块实现、局部修复：`gpt-6-sol`，高推理强度；
- 常规编译修复、日志整理和简单验证记录：`gpt-6-sol`，高推理强度。


## 项目路径约定

以下路径相对仓库根目录；如果项目名称不同，将 `ue/SkillLab/` 替换为实际 UE 项目目录：

- 项目入口和总体代码地图：`ue/SkillLab/Docs/README.md`、`ue/SkillLab/Docs/ProjectContext/SkillLabCodeArchitecture.md`；
- 策划和领域规格：`ue/SkillLab/Docs/Planning/<TaskName>/`；
- 程序工作流任务根目录：`ue/SkillLab/Docs/ProgramWorkflow/`；每个具体任务必须使用自己的同级目录 `<TaskName>/`；
- 当前架构基线：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/ArchitectureBaseline.md`；
- 专题架构结论：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/Architecture/`；
- 集成和验证记录：`ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/Validation/`；
- 任务状态、上下文包和子 Agent 交付默认通过对话传递，不为每个子任务新建文档；只有需要跨对话长期复用的结论才写入上述目录。

当前功能的长期产物限定在 `ue/SkillLab/Docs/ProgramWorkflow/<TaskName>/`，包括 `ArchitectureBaseline.md`、`Architecture/` 和 `Validation/`。`<TaskName>` 必须与 `Planning/<TaskName>/` 的任务名一致；具体任务名不在本通用 Skill 中写死。
## 必须读取的通用上下文

每次启动总控时必须确认：

- 用户当前目标和明确范围；
- `.codex/skills/ai-native-programming/SKILL.md` 中与程序任务相关的约束；
- 仓库 `AGENTS.md` 及适用的项目入口 Skill；
- 项目文档入口、代码架构地图和当前实现状态；
- 当前任务已有的架构基线、公共接口约定和交接记录（如果存在）；
- 五个程序工作流 Skill 的职责和交付约定：总控、架构、模块实现、集成、验证与问题记录。

总控启动时按以下顺序建立上下文，不把三个文档区域混为一体：

1. 仓库根 `AGENTS.md`；
2. `.codex/skills/ue-project-context/SKILL.md`，并按其路由读取项目入口；
3. `.codex/skills/ai-native-programming/SKILL.md`；
4. 当前项目的 `ProjectContext/README.md`、代码架构地图和相关模块文档；
5. 当前任务的 `Planning/<TaskName>/`；
6. 当前任务的 `ProgramWorkflow/<TaskName>/`；
7. 与当前子任务直接相关的源码、配置、资产和运行入口。

其中第 4 项是项目通用上下文，第 5 项是当前任务策划知识，第 6 项是当前任务程序中间产物。不得把具体任务路径写入通用 Skill 规则，也不得读取其他任务目录。

总控需要知道其他 Skill 能做什么、需要什么输入、会交付什么，但不需要把其他 Skill 的全部业务文档内容复制进自己的上下文。

## 按需读取的上下文

只根据当前任务读取：

- 相关策划总览和模块文档；
- 相关架构专题或上游实现摘要；
- 任务涉及的源码、配置、资产入口和验证入口；
- 子 Agent 返回的交付摘要、失败记录和接口变化。

如果任务只涉及一个模块，不默认读取其他模块的完整文档；如果需要判断跨模块依赖，只读取相关章节或接口约定。

## 新建任务对话与上下文包

创建每个任务对话前，必须准备一份最小上下文包，至少包含：

- 任务目标和完成边界；
- 当前阶段和前置依赖；
- 必须读取的文档、架构产物、源码或资产入口；
- 可以按需读取的补充材料；
- 明确不需要读取或不负责处理的内容；
- 允许修改的产物范围；
- 必须遵守的公共接口和不变量；
- 当前采用的默认值及其来源；
- 必须交付的结果格式；
- 遇到什么情况必须停止并回交总控。

上下文包必须把文档路径写成可直接打开的路径：通用项目文档来自 `ProjectContext/`，当前任务策划来自 `Planning/<TaskName>/`，当前任务交接来自 `ProgramWorkflow/<TaskName>/`。总控不要求新对话自行扫描 `Docs/` 或猜测任务目录；创建后通过该对话继续补充上下文并接收交付。

上游文档和架构基线属于“必须给到”的材料；与当前任务无直接关系的模块文档、历史对话和全部源码属于“不要默认给到”的材料。总控根据任务进度补充新的按需材料，不让子 Agent 自行猜测缺失上下文。

## 任务判断

把任务判断为以下一种：

1. **直接实现**：模块边界、公共接口、输入输出和验证入口都已明确。
2. **专题设计后实现**：模块可以确定范围，但技术落点或接口还需要一个专项架构结论。
3. **全局架构重审**：新增领域、公共接口冲突、职责无法归属，或当前基线无法承载需求。
4. **集成/修复**：已有实现需要连接、编译、配置、运行或根据失败事实修正。

相同职责和上下文尽量复用同一 Agent；模块、权限或上下文发生变化时创建新的 Agent。专题架构结论必须经过统一架构基线吸收，才能分发给实现 Agent。

## 推进方式

- 先建立或复用任务状态，不因一开始信息很多就把全部文档分发出去。
- 根据依赖决定串行或并行；公共接口未稳定前，不并行实现依赖它的模块。
- 接收子 Agent 的交付摘要，更新状态和下一步，不要求总控阅读所有内部推理。
- 普通编译、类型、include、模块依赖和局部连接错误继续安排修复。
- 体验规则、架构原则、Blueprint 使用边界和文档冲突记录为待判断事项；在用户不在线时继续推进不依赖这些判断的工作。

## 总控自身必须产出

总控在每个阶段维护以下产物或摘要：

- 当前总目标和本阶段目标；
- 任务拆分、依赖关系和当前状态；
- 每个子 Agent 的上下文包和职责；
- 当前架构基线版本及其变化来源；
- 已完成、进行中、失败和未覆盖范围；
- 需要用户判断的体验/架构/Blueprint 问题；
- 下一步调度建议和进入 PIE 的剩余条件。

## 子 Agent 交付要求

要求每个子 Agent 返回：状态、完成内容、修改产物、公共接口变化、采用的假设、未完成范围、代码/集成问题、需要用户判断的问题、验证或复现入口、建议下一步。

总控把这些内容转换成后续 Agent 的上下文包；不要只转发一句“已完成”。

## 不做

- 不替代专项 Agent 逐行实现代码。
- 不在没有架构交接的情况下自行改变公共接口。
- 不把未确认的体验选择伪装成用户决定。
- 不要求所有 Agent 读取相同的完整上下文。











