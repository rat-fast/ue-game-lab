---
name: ue-project-context
description: "进入或处理本仓库 Unreal Engine 项目时，作为必读入口提供项目文档路由、目录职责和资产归属；不负责执行 Git 提交或实现游戏功能。"
---

# Unreal Engine Project Context

## When To Use

当任务进入本仓库、读取或修改 `ue/` 下的 Unreal Engine 项目，或需要判断 UE 项目文件是否属于项目成果时，必须先使用本 Skill。

## Required Reading

首次进入项目时掌握根目录 `AGENTS.md`、当前项目 `ue/<ProjectName>/Docs/README.md`（存在时）和本角色 Skill。已经完整提供或读取、且没有变化迹象的当前有效内容可以复用；新对话仍需取得自己的适用规则，不能只依赖其他 Agent 声称已读。

然后按职责选择上下文，不把“进入 UE 项目”等同于阅读全部程序文档。下表路径以 SkillLab 为例，其他项目使用其自身入口规定的对应材料。

| 角色 | 必须取得的任务上下文 | 按需补读 |
|---|---|---|
| 功能总控、策划 | `Docs/Baseline/<Domain>/README.md` 及当前目标涉及的基线正文、用户目标和当前策划需求 | 需要定位时的 `Docs/Planning/<Domain>/README.md`；明确复用的旧需求和长期方向 |
| 程序 PM | 当前需求与适用公共约定、代码地图的项目角色/模块总览、任务状态与交接映射 | 影响本次拆分的运行链、交接相关段落、交付摘要和 Review 发现 |
| 架构 | 当前需求与适用公共约定、代码地图的相关模块关系和主运行链、相关模块文档、真实上下游 | 受影响的其他模块、已有交接和验证入口 |
| 实现 | 自己任务的需求与适用公共约定、当前架构交接、落点文件及直接依赖 | 相关模块文档、代码地图相关章节和验证入口；不默认全文读取全局地图 |
| Review | 被审范围的需求与适用公共约定、同一当前交接、实际修改、实现摘要与验证证据 | 具体疑点的模块、地图章节和直接上下游；不默认读取整个实现对话或全局地图 |
| 基线/项目文档维护 | 本次允许维护的正文及其职责规则；按专项 Skill 选择确认依据 | 事实核验材料仅由具备该职责和授权的角色读取，不因维护导航而扩大读取范围 |

功能总控和策划不读取 `Docs/ProjectContext/`、`Docs/ProgramWorkflow/`、源码、配置、Blueprint、资产内部结构或运行日志来推断需求；技术未知项交程序侧核实。普通 UE 学习按学习目标选择材料，不自动套用程序工作流。

程序 PM、架构、实现和 Review 还须读取当前项目的 `Docs/ProjectContext/README.md`，取得项目专属规范路由，并读取 `ai-native-programming` 及本角色 Skill。SkillLab 的这些角色必须取得 `Docs/ProjectContext/ProgramArchitectureContract.md` 的当前有效约定；其他项目只使用其自身规范。项目入口对代码地图、模块索引的通用“先读”要求按上表选择相关内容，不扩大为所有角色默认全文加载；明确适用的公共规则不能因省上下文而略过。

相关基线按主题读取，不把入口链接的全部正文自动视为必读。已给出准确任务路径时直达相关正文，无需逐级重读 Planning、ProgramWorkflow、Batches、Modules 等目录索引；不知道位置时才用索引导航。旧批次须说明复用目的、适用章节和本次范围，不能因同属一个领域就通读全集或继承其范围。

`ProjectContext/AI-NativeWorkflow.md` 是理解/维护项目工作流时的参考，不是上述各角色的默认附加必读。根目录 `AI_NATIVE_GAME_DEV_PRINCIPLES.md` 仅在讨论 AI Native 方法论、生产架构或用户指定时读取相关章节；程序日常执行通过 `ai-native-programming` 获取约束。

文档缺失或仍为占位时明确记录缺口，不假装已获得事实。具备事实核验职责的角色沿真实产物和直接依赖补读；总控、策划保留待核实项，不以未来方案填补当前事实。

SkillLab 已知导航位置包括 `Docs/ProjectContext/SkillLabCodeArchitecture.md`、`Docs/ProjectContext/Modules/README.md` 和相关模块正文；目录存在不代表所有文件必读。`ue/AINativeRoguelite/` 的入口需在进入时检查，缺失则说明，不凭空套用 SkillLab 文档。

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
