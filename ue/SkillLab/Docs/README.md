# SkillLab Documentation

## Purpose

这里是 SkillLab 项目的代码、功能和运行结构说明，帮助人和 AI 快速定位功能入口、代码关系、资产配置和验证方式。

SkillLab 同时是一个技能领域实验项目：用于验证 AI 能否在受约束的生产接口中参与技能的理解、实现、验证和迭代。项目范围、当前假设和接口审计要求见 `AI-NativeWorkflow.md`；跨项目历史判断见仓库根目录 `docs/AI Native对话与决策索引.md`。

## Source Of Truth

真实实现以 C++ 源码、`.ini` 配置、UE 资产、关卡对象以及实际运行结果为准。本文档不复制源码；如果文档与实现不一致，应以真实产物为准并修正文档。

## Documents

- `SkillLabCodeArchitecture.md`：整体代码和模块地图。
- `Modules/GameplayFramework.md`：GameMode、Character、PlayerController。
- `Modules/Input.md`：Enhanced Input 和鼠标输入。
- `Modules/Camera.md`：镜头组件和镜头输入。
- `Modules/Navigation.md`：NavMesh 和点击移动。
- `Modules/LevelAndDebug.md`：测试关卡和调试反馈。
- `Modules/CharacterPresentation.md`：模型、骨骼和动画。
- `AI-NativeWorkflow.md`：项目实验目标、当前阶段、架构假设和接口审计要求。
- `SkillSystem/README.md`：技能系统设计索引、九个模块职责包、文档进度与分批开工条件。
- `SkillSystem/01-SystemScope.md`：技能系统目标与范围（`SkillSystemRequirements.md` 为兼容入口）。
- `SkillSystem/02-CapabilityModel.md`：第二层总体模型；各模块设计位于 `SkillSystem/02-Modules/`。

技能系统文档表达计划建设方向，不等于当前实现；旧 `AI-NativeWorkflow.md` 中的阶段描述可能尚未同步，技能建设范围以技能系统索引及范围文档为准。

## Reading Order

接手项目时先读整体架构，再按任务读取对应模块文档，最后读取列出的源码、配置和资产。模块文档描述当前版本的默认组织方式，不是永久架构约束。

