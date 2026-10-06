---
name: ai-program-integration
description: 将多个 UE 功能模块接入当前项目，处理接口连接、编译、配置和必要资产外壳，使结果达到可进入 PIE 测试的状态。
---

# AI 程序集成

## 入口关系

本 Skill 由程序工作流总控通过独立本地任务对话调用。进入 UE 项目时，先完成 `ue-project-context` 的项目文档路由，再读取 `ai-native-programming`，最后读取本 Skill。集成对话接收模块交付和运行目标，不重新承担架构设计。

## 目标

把多个已实现模块接入当前 UE 项目，处理接口连接、初始化顺序、编译、配置和必要资产外壳，使结果达到可以进入 PIE 测试的状态。

## 项目路径和交接

默认使用以下相对仓库路径：

- 项目入口和代码地图：`<UEProjectRoot>/Docs/README.md`、`<UEProjectRoot>/Docs/ProjectContext/`；
- 架构基线：`<UEProjectRoot>/Docs/ProgramWorkflow/<TaskName>/ArchitectureBaseline.md`；
- 实现源码：`<UEProjectRoot>/Source/`；
- 配置和资产：`<UEProjectRoot>/Config/`、任务明确授权的 `<UEProjectRoot>/Content/`；
- 长期集成/验证记录：`<UEProjectRoot>/Docs/ProgramWorkflow/<TaskName>/Validation/`。

`<TaskName>` 是当前功能任务名，策划目录和程序工作流目录必须使用同一个任务名。`ProjectContext/` 提供项目通用事实，`Planning/<TaskName>/` 提供策划验收依据，`ProgramWorkflow/<TaskName>/` 只提供当前任务的程序交接；不得读取其他任务目录。

各模块实现摘要和接口变化由总控通过对话传递；集成 Agent 不要求为每个编译错误新建文档。
## 必须读取

- `.codex/skills/ai-native-programming/SKILL.md` 中与当前任务相关的程序约束；

- 总控给出的集成任务和目标运行链；
- 当前架构基线；
- 各模块的实现摘要、接口变化和未完成范围；
- 项目文档、代码状态、构建入口和本次运行入口。

集成 Agent 进入项目时，先读 `<UEProjectRoot>/Docs/ProjectContext/` 中的项目入口和代码架构地图，再读当前任务的基线、模块交付摘要和相关策划验收章节；不扫描全部 `Docs/`。

## 按需读取

- 解决编译或运行问题所需的具体源码；
- 相关配置、Blueprint、DataAsset、地图和资源引用；
- 验证 Agent 提供的复现步骤和失败证据。

不要重新分析所有策划文档，也不要因为集成问题默认读取或重写无关模块。

## 工作

- 开始集成前，确认 Review 已按三层顺序完成架构符合性审查且结论允许继续；若架构符合性未通过，停止集成并回交总控，不能用编译成功或一次运行成功替代放行。
- 检查模块接口、依赖、初始化、生命周期、清理和重置是否一致；
- 处理编译、模块依赖、配置、必要 Blueprint 外壳和资源连接；
- 接入配置或 UE 资产时，确认内容的权威来源、加载/导入路径、引用解析和校验方式；不能用临时资产或手工编辑器步骤掩盖缺失的 AI 可读生产入口；
- 连接首批纵向链路，使其能够启动、请求、执行、结束、重置并重复测试；
- 将普通代码/集成错误分派回适当模块，或在集成范围内修复；
- 发现架构冲突时记录事实、影响和回交建议，不用局部补丁掩盖职责问题。

## 约束

不重新设计模块规则，不把核心业务逻辑迁入 Blueprint 以绕过集成困难，不扩大未授权功能范围。需要公共接口变化时回交总控和架构 Agent。

## 交付

返回：已接入模块、读取依据、修改产物、接口连接结果、编译/运行状态、可进入 PIE 的路径、剩余问题、未覆盖范围、失败复现信息和需要总控记录的体验/架构/Blueprint 事项。











