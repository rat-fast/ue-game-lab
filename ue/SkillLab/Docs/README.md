# SkillLab 项目文档入口

`Docs/` 按文档职责分为三个工作区域，以及一个供相关工作共同读取的共享业务基线入口。各类文档的用途和读取边界不同，不能因为它们都位于 `Docs/` 下就混合读取：

- `Planning/`：策划、领域规则、功能规格和体验要求。当前技能系统策划位于 `Planning/SkillSystem/`。
- `ProjectContext/`：SkillLab 当前有效的项目事实，包括代码架构地图、模块职责、运行入口和通用项目规范。
- `ProgramWorkflow/`：AI 写代码时产生的中间产物，按具体程序任务隔离。当前技能系统任务位于 `ProgramWorkflow/SkillSystem/`。
- `SkillSystem/`：当前技能系统已经确认的共享业务基线，供策划、程序和总控共同读取。它定义公共业务语义和能力边界，不替代策划需求、程序架构或代码事实。
- `CharacterPresentation/`：角色与技能表现的共享业务基线，供表现策划、程序、资源和总控共同读取。当前基线按 Git 提交 `a2c5969` 的完整文件树核对，包含该节点仍保留的历史功能；职责正文、实现事实、证据等级和限制分别见该目录四份文档。表现策划要求和程序批次交接仍由各自目录负责。

程序或表现 Agent 进入 SkillLab 后，先读取 `ProjectContext/README.md` 和当前任务相关的共享基线；然后只读取当前任务对应的 `Planning/<领域>/` 和 `ProgramWorkflow/<任务>/`。不得默认读取整个 `Docs/`，也不得读取其他任务的 `ProgramWorkflow/<任务>/`。
