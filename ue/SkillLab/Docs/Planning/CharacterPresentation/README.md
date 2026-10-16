# 角色与技能表现策划入口

## 职责

本目录提出玩家可观察的角色与技能表现需求、具体表现配置、资源需求和验收条件。共享业务事实以 `../../CharacterPresentation/` 为准；此处记录新增的策划承诺，不把程序实验、未提交实现或资源文件本身写成已验证结果。

## 阅读边界

1. 阅读 `../../ProjectContext/README.md` 和 `../../CharacterPresentation/README.md`，分别确认项目事实入口和共享表现语义。
2. 阅读当前批次入口，再按任务进入 `Infrastructure/` 或 `Configurations/`。
3. 不读取归档表现文档作为当前规则，也不把运行时 JSON、UE 资产路径或程序中间产物当作策划权威来源。

## 计划结构

- `Batches/`：按目标和验收组织表现改造批次。
- 每批可分为 `Infrastructure/` 公共程序需求和 `Configurations/` 具体表现配置；两类文档分开维护。

当前正式批次：[Batch01-AIAdaptedAnimationPipeline](Batches/Batch01-AIAdaptedAnimationPipeline/README.md)。
