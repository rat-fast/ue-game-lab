---
name: ue-project-context
description: "进入或处理本仓库 Unreal Engine 项目时，作为必读入口提供项目文档路由、目录职责和资产归属；不负责执行 Git 提交或实现游戏功能。"
---

# Unreal Engine Project Context

## When To Use

当任务进入本仓库、读取或修改 `ue/` 下的 Unreal Engine 项目，或需要判断 UE 项目文件是否属于项目成果时，必须先使用本 Skill。

## Required Reading

按以下顺序读取，不要跳过入口文件：

1. 根目录 `AGENTS.md`。
2. 当前项目的 `ue/<ProjectName>/Docs/README.md`（存在时）。
3. 当前项目文档中标记为通用项目上下文的入口（例如 `Docs/ProjectContext/README.md` 和代码架构地图）。
4. 根据任务从项目文档的模块目录读取匹配的模块文档，不要默认读取全部模块。
5. 最后读取任务涉及的源码、配置、资产和验证证据。

进入项目程序架构、实现、集成或 Review 任务时，遵循当前项目 `Docs/ProjectContext/README.md` 中的程序规范路由。SkillLab 的这些任务必须读取 `Docs/ProjectContext/ProgramArchitectureContract.md`；其他项目只读取其自身入口明确指定的规范，不把 SkillLab 约定推广为全仓库规则。

如果任务进入程序实现、架构、集成、编译或运行验证，再读取仓库 `.codex/skills/ai-native-programming/SKILL.md` 和对应的程序工作流 Skill。普通 UE 学习或文档浏览不自动加载程序约束 Skill。

如果项目缺少上述文档，明确记录“文档不存在”，再以真实源码、配置和资产为依据，不要假装已经读取。

当前仓库的已知入口：

- `ue/SkillLab/Docs/README.md`
- `ue/SkillLab/Docs/ProjectContext/README.md`
- `ue/SkillLab/Docs/ProjectContext/SkillLabCodeArchitecture.md`
- `ue/SkillLab/Docs/ProjectContext/Modules/<与任务匹配的模块>.md`
- `ue/AINativeRoguelite/` 当前没有 `Docs/README.md`，进入该项目时必须明确记录这一点。

## Documentation Routing

- 任务涉及项目文档初始化、更新、代码地图、功能到实现映射或模块职责时，再加载 `project-documentation` Skill。
- 仅做普通 UE 功能开发时，按上面的必读顺序读取项目文档，不自动执行文档整理。
- `project-documentation` Skill 是文档维护操作规则；本 Skill 是进入 UE 项目的上下文入口，二者职责不同。

## Repository Scope

- `ue/` 下的所有 Unreal Engine 项目使用同一套判断标准；不得因为当前项目或目录较小而改用“全部上传”的临时规则。
- UE 项目的 `Source/`、`Config/`、`.uproject`、项目文档以及项目自有的 `Content/` 资产属于可追踪成果。
- 项目自有的 Blueprint、关卡、Data Asset、输入、UI、AI 和其他需要回退历史的 `.uasset/.umap` 使用 Git LFS。
- 可重新下载且不保留历史的外部资产统一放在 `Content/Vendor/`、`Content/Marketplace/` 或 `Content/ThirdParty/`，这些目录默认不进入 Git。项目自有或修改后的派生资产放在项目自己的目录中。
- 不得把“美术资源”这个类型本身作为排除依据；现有来源不明的目录不能仅凭名称擅自忽略。
- `Binaries/`、`DerivedDataCache/`、`Intermediate/`、`Saved/`、`.vs/` 是 UE 生成内容，永远不作为项目成果提交。
- 关卡相关的 `__ExternalActors__/`、`__ExternalObjects__/` 需要与对应关卡一起判断，不能看到目录名就直接忽略。

## Classification Boundary

路径和目录职责优先于文件扩展名。`.uasset` 可能是 Blueprint、策划数据、材质、模型或其他资产；不能只根据扩展名决定是否保存。

如果资产路径、来源或是否需要保留历史不明确，列出待确认清单，不要擅自移动、删除、忽略或提交。不要直接在外部资源目录中修改资产；需要项目化修改时，创建项目自己的派生资产。

本 Skill 只提供 UE 项目上下文，不授权修改代码、资产、Git 配置或远端状态。
