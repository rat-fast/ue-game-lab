# Batch02：Lua 技能组合与内容配置试点

## 目标与范围

本批在保留 C++/GAS 稳定技能运行底座及其权威结算职责的前提下，规划当前已有四个技能样本接入 Lua 组合层和 JSON 内容配置层。Lua 是用户选定的试点语言；JSON 作为可调整内容的承载目标。本批要求同一内容版本、同一输入和同一初始状态下，Lua 组合路径与现有原生路径的外部技能行为一致。

本批只覆盖以下四个当前基线样本：

- `Skill_Fireball`：敌方单位目标的投射物伤害；
- `Skill_AreaBurst`：地面位置范围内敌方单位伤害；
- `Skill_Mend`：对自身的即时治疗；
- `Skill_BurningBuff`：对敌方单位即时施加持续伤害。

Batch01 的 `Firebolt`、`ArcStrike`、`BurnField`、`ChannelBeam`、`Barrier` 不属于本批实施范围，相关六技能迁移矩阵降为未来范围参考。

## 当前文档

- [Lua 技能组合与内容配置试点需求](LuaSkillCompositionPilotRequirements.md)：四技能范围、职责边界、当前确认事实、失败降级、程序待核实项和验收矩阵。

## 关系与读取顺序

1. 读取 [SkillSystem 共享基线](../../../../Baseline/SkillSystem/README.md)及其直接引用的正文，确认四技能已有业务事实。
2. 读取 [SkillSystem 策划入口](../../README.md)和当前相关策划内容；不将 Batch01 六技能扩展案带入本批。
3. 读取本目录需求，确认 Lua 组合、JSON 内容配置和兼容验收边界。
4. 如需关联表现，读取 [Batch02 Lua 角色表现试点](../../../CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/README.md)；表现只能消费真实技能事实。

本批先维护一个入口和一份需求正文，不创建技能子文档或技术实现文档。程序结构、Lua API、插件、C++/GAS 类、Blueprint 拓扑、JSON schema 和迁移步骤由程序侧另行核实，不能从本策划案推定已经存在。

## 状态

- 已确定：四个现有技能全部纳入 Lua 组合层/JSON 内容配置试点；C++/GAS 保留权威边界；外部行为必须保持一致；六技能扩展不在本批。
- 已确认事实：四技能的目标形状、关系、执行方式和效果类别见需求正文第 3 节；BurningBuff 的当前持续时间与周期也仅按共享基线记录。
- 待定：基线未确认的数值、具体组合选项、脚本失败时的玩家提示范围，以及 JSON 内容版本的产品管理规则。
- 未验证：Lua/GAS 事件和动作能力、脚本及内容隔离、身份与幂等清理、实际迁移结果和运行验收。

## 下游交接

- 程序 PM：读取本需求和四技能共享基线，再按项目程序入口读取 `Docs/ProjectContext/README.md`、`Docs/ProjectContext/ProgramArchitectureContract.md`、`Docs/ProjectContext/SkillLabCodeArchitecture.md` 及相关模块文档；实现与证据放在 `Docs/ProgramWorkflow/SkillSystem/` 的本任务目录。
- 表现线：读取本需求第 6 节及角色表现 Lua 试点策划入口，仅使用权威技能事实驱动表现。
- 验证线：按本需求第 10 节矩阵逐项记录；策划条件不等于实现证据。

