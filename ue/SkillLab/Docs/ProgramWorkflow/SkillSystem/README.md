# SkillSystem 程序工作流

本目录只服务于 SkillLab 的 SkillSystem 实现任务。SkillSystem Agent 不读取其他任务的 `ProgramWorkflow/<任务>/`。

## 固定输入

- 项目通用事实：`../../ProjectContext/README.md`，随后读取 `../../ProjectContext/SkillLabCodeArchitecture.md` 和当前涉及的 `../../ProjectContext/Modules/<模块>.md`；
- SkillSystem 策划规格：`../../Planning/SkillSystem/` 中与当前阶段相关的总览或模块文档；
- 当前 SkillSystem 中间产物：本目录内已有的架构基线、专题结论、验证记录；
- 工作流规则：仓库根目录下 5 个 `ai-program-*/SKILL.md`，由总控统一读取并按 Agent 职责分发。

## 长期产物

- `ArchitectureBaseline.md`：SkillSystem 当前统一架构基线和公共接口；
- `Architecture/`：只对 SkillSystem 有效的专题架构结论；
- `Validation/`：SkillSystem 编译、运行、PIE 证据和待判断问题。

一次性子任务上下文、任务拆分和交付通过对话传递，不为每个子任务单独建文档。

