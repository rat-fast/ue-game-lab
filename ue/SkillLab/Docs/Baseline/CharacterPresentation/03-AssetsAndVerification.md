# 资产与验证边界

## 职责

说明资产可用、程序能力与玩家可见体验之间的区别，集中维护影响使用判断的验证状态和限制。具体证据与验证过程由程序交付文档保存。

## 当前承诺

- 资产存在、资源路径可定位或代码已接入，不证明完整视觉链已通过。
- 构建、自动化及不可见运行核验只能支持对应代码或事件范围，不能替代玩家可见体验验收。
- 目标体验、动作/状态、技能结果和对象清理分别判断，局部通过不能外推为完整体验通过。
- 占位资源和样本配置不构成正式美术标准、最终模型绑定或技能产品参数。

## 当前实现与验证状态

| 功能 | 已有支持与确认 | 验证边界 |
| --- | --- | --- |
| 基础角色表现 | 已有基础行动状态采集和表现接入 | 完整五态视觉及迁移仍未验收 |
| 技能动作 | 已有真实事件消费、按施法身份播放/停止及局部身份核验 | 不代表动作质量、视觉时点、姿势恢复或全部竞态已验收 |
| 空间配置与对象执行 | 已有配置解析、空间变换、真实生成点、视觉与碰撞隔离、独立关联来源和持续效果表现跟随/清理的局部核验 | 不代表生产模型绑定、全部错误边界或不同内容复用已充分验证 |
| 空间数据采集工具 | 已可从实际 SkillLab 运行读取配置和参照、提交指定技能请求并导出事件、空间样本及同步画面；定向 Review 确认原实现问题已修复 | 完整需求场景验收仍未完成，尤其没有有效单位目标、真实投射物链、持续效果和 RelatedActor 的完整端到端证据；单次请求不证明全资产覆盖或骨骼/Socket 精度，工具采集状态不代表命中、数据读解、适配或表现质量通过 |
| 玩家可见空间表现 | AreaBurst 在空区域下已有局部表现可见的确认 | 不覆盖有效敌人伤害范围、不同遮挡/摄像机距离、其他三技能空间观感或完整 PIE |

## 当前限制

- 生产模型的骨骼/Socket 唯一匹配尚未运行验证；关联对象跨 World 拒绝的实现边界尚缺专门运行验证。
- Fireball 在施法开始后移动角色/模型/Socket、于真实释放点采样的场景未证明；偏移起点下近墙、遮挡及重叠出生仍未验证。既有阻挡核验不能替代这些场景。
- Fireball 可见方向/尺寸、Mend 反馈、BurningBuff 跟随观感及不同模型/资源替换的完整复用仍缺充分证据。
- 基础五态、四技能差异化动作、视觉释放时点、取消/完成姿势恢复、缺资源降级可读性与旧/新动作竞态仍未全面验收。
- 当前资源与跟随能力的支持范围以 [技能动作与表现事件](02-SkillActionPresentation.md)为准。最终美术质量、正式 VFX、音频及受击表现没有已验收承诺。

## 依据入口与关系

空间配置的实现、详细验证范围及剩余风险见 [实现摘要](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/ImplementationSummary.md)与 [独立 Review](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/Review.md)；空间数据采集工具的范围及未完成验收见[工具实现摘要](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/ImplementationSummary.md)与[工具独立 Review](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/Review.md)。当前表现职责见 [表现模块](../../ProjectContext/Modules/CharacterPresentation.md)，项目验证材料由 [测试与验证文档](../../ProjectContext/Modules/TestAndVerification.md)定位。

基础表现交付见 [角色表现程序入口](../../ProgramWorkflow/CharacterPresentationLuaPilot/README.md)，体验要求见 [角色表现需求](../../Planning/CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/README.md)。本篇只保留功能状态和限制，不由单项交付推定全部体验完成。
