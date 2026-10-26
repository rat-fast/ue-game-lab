# 角色与技能表现策划入口

## 职责

本目录提出玩家可观察的角色与技能表现需求、具体表现配置、资源需求和验收条件。共享业务事实以 `../../Baseline/CharacterPresentation/` 为准；此处记录新增的策划承诺，不把程序实验、未提交实现或资源文件本身写成已验证结果。

## 阅读边界

1. 阅读 `../../Baseline/CharacterPresentation/README.md` 及当前需求直接引用的基线正文，确认共享表现语义。
2. 阅读当前批次入口和本次需求相关的策划文档；只使用用户提供的项目事实，不通过程序侧文档推断实现。
3. 不读取 `../../ProjectContext/`、`../../ProgramWorkflow/`、源码、配置、Blueprint、资产内部结构或运行日志。技术未知项写为交程序侧核实的问题。
4. 不读取归档表现文档作为当前规则，也不把运行时 JSON、UE 资产路径或程序中间产物当作策划权威来源。

## 计划结构

- `Batches/`：按目标和验收组织表现改造批次。
- 每批可分为 `Infrastructure/` 公共程序需求和 `Configurations/` 具体表现配置；两类文档分开维护。

当前策划入口：

- [Batch02-LuaCharacterPresentationPilot](Batches/Batch02-LuaCharacterPresentationPilot/README.md)：Lua 角色表现流程试点。
- [Batch03-MontageNotifyPlacement](Batches/Batch03-MontageNotifyPlacement/README.md)：AI 技能动作 Notify 配置能力；根据技能事件需求和给定动作自主确定时点，完成实际配置、回读与游戏内验证。当前为功能需求，Agent 工作流和动作读取方法由用户与总控另行讨论。

此前的 [Batch01-AIAdaptedAnimationPipeline](Batches/Batch01-AIAdaptedAnimationPipeline/README.md) 是先前需求与交接参考，不作为 Lua 试点当前入口；其中仍适用的体验边界由 Batch02 选择性引用。
