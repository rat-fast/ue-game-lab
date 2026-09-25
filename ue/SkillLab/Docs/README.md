# SkillLab 项目文档入口

`Docs/` 只按文档职责分为三个平行区域。三类文档的用途和读取边界不同，不能因为它们都位于 `Docs/` 下就混合读取：

- `Planning/`：策划、领域规则、功能规格和体验要求。当前技能系统策划位于 `Planning/SkillSystem/`。
- `ProjectContext/`：SkillLab 当前有效的项目事实，包括代码架构地图、模块职责、运行入口和通用项目规范。
- `ProgramWorkflow/`：AI 写代码时产生的中间产物，按具体程序任务隔离。当前技能系统任务位于 `ProgramWorkflow/SkillSystem/`。

程序 Agent 进入 SkillLab 后，先读取 `ProjectContext/README.md`；然后只读取当前任务对应的 `Planning/<领域>/` 和 `ProgramWorkflow/<任务>/`。不得默认读取整个 `Docs/`，也不得读取其他任务的 `ProgramWorkflow/<任务>/`。

