# SkillLab 项目文档入口

`Docs/` 按用途组织项目事实、策划需求和程序工作流，并在 `Baseline/` 下集中存放跨任务共用的领域基线。参与相关领域任务的 Agent 应读取对应基线；不需要默认读取所有领域的全部文档：

- `Planning/`：策划、领域规则、功能规格和体验要求。当前技能系统策划位于 `Planning/SkillSystem/`。
- `ProjectContext/`：SkillLab 当前有效的项目事实，包括代码架构地图、模块职责、运行入口和通用项目规范。
- `ProgramWorkflow/`：AI 写代码时产生的中间产物，按具体程序任务隔离。当前技能系统任务位于 `ProgramWorkflow/SkillSystem/`。
- `Baseline/`：[共享基线入口](Baseline/README.md)，集中列出各领域基线及其读取边界。
- `Baseline/SkillSystem/`：技能系统已确认的共享业务语义与能力边界，不替代策划需求、程序架构或代码事实。
- `Baseline/CharacterPresentation/`：角色与技能表现已确认的共享语义、证据边界和当前限制。提交 `a2c5969` 仅作为原始静态核对的历史快照，不代表当前实现版本；当前承诺、实现证据和未验收限制以该目录 README 及正文为准。策划要求和程序任务产物由各自目录维护。

程序或表现 Agent 进入 SkillLab 后，先读取 `ProjectContext/README.md` 和当前任务相关的共享基线；然后只读取当前任务对应的 `Planning/<领域>/` 和 `ProgramWorkflow/<任务>/`。不得默认读取整个 `Docs/`，也不得读取其他任务的 `ProgramWorkflow/<任务>/`。
