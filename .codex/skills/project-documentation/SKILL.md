---
name: project-documentation
description: "整理和维护具体项目当前有效的 AI 可读文档，包括代码架构地图、功能到实现的映射、模块职责、运行链、资产配置入口和验证方式。用于项目文档初始化、模块文档更新、代码定位说明和文档结构维护；不负责历史工作记录或游戏功能实现。"
---

# Project Documentation

## Purpose

为具体项目建立简洁、可检索、与真实产物对应的当前状态文档。文档的作用是给 AI 提供定位地图和上下文，不复制源码，不记录日常工作流水，也不把当前实现写成永久架构约束。

## When To Use

- 初始化项目代码/功能文档结构。
- 用户要求整理项目当前状态、代码模块或运行链。
- 新增功能后补充功能到代码、配置、资产和验证方式的映射。
- 项目结构变化后更新模块职责和任务路由。

不要因普通代码修改自动加载本 Skill；只有任务包含项目文档整理、维护或代码地图需求时使用。

重要工作、讨论和历史判断的回顾由 `repository-worklog` Skill 负责；本 Skill 只维护项目当前有效状态。

## Source Of Truth

项目源码、配置、UE 资产、关卡对象、日志和实际运行结果是真实来源。先读取真实产物，再写文档；如果文档与真实产物冲突，以真实产物为准并修正文档。

## Project Documentation Layout

优先在项目自己的文档目录中组织内容，例如：

```text
<Project>/Docs/
├── README.md
├── <Project>CodeArchitecture.md
└── Modules/
    ├── <Module>.md
    └── ...
```

不要把具体项目事实写进仓库根目录的原则文档、学习地图或通用 Skill。跨项目稳定原则应进入原则文档；学习进度应进入学习地图；一次性实验可进入项目实验记录。

## Recommended Documents

### README.md

只做入口和阅读顺序，列出项目文档、事实来源和使用方式。

### Project Code Architecture

记录模块地图、主要对象关系、关键运行链、源代码目录和整体演进说明。

### Module Documents

按稳定职责而不是按单个文件或函数拆分模块。每个模块使用以下字段：

```markdown
# Module Name

## Module Responsibility
## Code And Assets
## Runtime Entry
## Runtime Flow
## Current Behavior
## Current Responsibilities And Evolution Boundaries
## Verification
## Current Limitations
## Extension Directions
```

## Granularity Rules

- 一个稳定职责通常对应一个模块文档。
- 不为每个函数、参数或 Blueprint 节点单独建文档。
- 只有当模块拥有独立入口、依赖、验证流程，或文档明显过长时才继续拆分。
- 文档描述职责、入口、关系、边界和证据，不粘贴实现代码。

## Evolution Boundaries

`Current Responsibilities And Evolution Boundaries` 不是禁止修改的清单。必须区分：

- 当前职责：当前版本实际由谁负责。
- 默认修改入口：简单修改优先查看哪里。
- 可接受的架构调整：新需求出现时可以迁移或新增什么。
- 重构触发条件：什么迹象说明当前归属已不合适。
- 当前阶段限制：暂不做什么，以及这只是范围控制而非永久约束。

不要把“当前使用某个 UE API/Blueprint/组件”写成未来唯一方案。

## Update Workflow

1. 确认项目路径和需要整理的范围。
2. 读取项目本地规则和现有文档。
3. 检查源码、配置、资产引用、关卡对象和验证证据。
4. 更新最小必要的入口、架构或模块文档。
5. 检查路径、类名、资产路径和运行链是否与真实产物一致。
6. 汇报新增/修改的文档以及仍未验证的内容。

文档维护本身不授权修改代码、Blueprint、资产或编辑器状态；如果任务同时要求实现功能，仍需单独确认实施范围。

## Responsibility Boundary

项目进入、上下文选择和文档读取顺序由对应的项目上下文入口 Skill 负责。本 Skill 只规定项目文档的组织与维护方式。

模块文档中的路径、类名、资产名和函数名应使用真实标识符；自然语言只补充这些标识符无法表达的关系、原因和验证要求。
