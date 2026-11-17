# 角色与技能表现共享基线

## 职责

本目录记录角色与技能表现的共享业务语义、当前能力和使用边界，不规定程序架构、Lua API、动画资产方案、状态机或视觉资源规范。

## 当前承诺

- 表现消费技能运行层真实事件，不裁定或伪造请求受理、命中、资源扣除、实际效果、持续状态及施法终态。
- 技能动作、技能运行事实和独立持续效果分别管理；动作播放或停止不能成为业务结果来源。
- 缺失、延迟或失败的表现资源不能改变权威技能结果。
- 空间配置按技能对象和既有事件区分真实执行对象与纯表现对象，支持范围见 [技能动作与表现事件](02-SkillActionPresentation.md)。
- 当前四技能是代表性接入样本，不限制通用能力的适用范围；样本可运行不等于最终空间适配或美术质量已验收。

## 当前能力与验证状态

基础角色状态、事件驱动技能动作和可配置空间对象已有接入。空间能力包括真实投射物生成位置，以及适用事件上的表现定位、方向、视觉尺寸和持续效果跟随。

代码与局部运行核验支持这些能力的有限使用范围；AreaBurst 在空区域下已有局部表现可见的确认。基础五态、技能动作质量、生产模型绑定、其他样本空间观感及完整交互仍未全面验收。验证状态与使用限制由 [资产与验证边界](03-AssetsAndVerification.md)集中说明，详细依据见 [空间能力实现摘要](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/ImplementationSummary.md)及 [独立 Review](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/Review.md)。

Batch04-02 已提供从实际 SkillLab 运行采集配置、事件、空间样本和同步画面的导出工具，作为人和 Agent 获取观察材料的入口。它不扩展上述运行时表现承诺，也不代表空间数据读解、适配或完整场景验收已完成；范围与证据见[工具实现摘要](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/ImplementationSummary.md)及[独立 Review](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/Review.md)。

## 正文索引

1. [角色基础表现](01-CharacterPresentation.md)：基础行动状态及其视觉边界；
2. [技能动作与表现事件](02-SkillActionPresentation.md)：事件、动作和空间配置的业务语义及支持范围；
3. [资产与验证边界](03-AssetsAndVerification.md)：使用能力时需要保留的验证限制与依据入口。

## 关系

目标、结算与效果寿命以 [SkillSystem 基线](../SkillSystem/README.md)为准。基础体验要求见 [角色表现需求](../../Planning/CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/README.md)，空间语义来源见 [空间适配共用边界](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/README.md)及 [空间配置需求](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/01-SkillSpatialConfigurationSupport.md)。需求中的目标不自动成为已实现能力。
