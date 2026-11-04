# 技能系统链路与内容生产边界改造

## 目标与范围

本轮面向 SkillLab 当前 GAS 技能系统的通用链路和内容生产边界，在保持既有玩家可观察行为的前提下，明确公共规则、技能专属定义、数值和施法释放之间的职责。技能专属定义不应硬编码于 C++；用户倾向以 Lua 表达目标形态、关系、效果类型与技能组合，以 JSON 承载高频可调纯数值。GAS Ability 负责激活和施法生命周期；动画 Cast Notify 通过带 Tag 的 Gameplay Event 通知当前 Ability 到达释放时点，由 Ability 正式推进释放及效果提交。

通用职责边界面向当前技能系统，不以四技能为上限。`Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff` 是共享基线已有的代表性验收样本；本轮实际内容迁移数量与具体受影响链路尚待程序核实及任务拆分确认。排除 Batch01 六技能新增内容迁移和玩法扩展，不排除通用链路适用于当前系统。另排除前后摇操作规则、受击/VFX 新体验、工具、UI、热更新和安全策略。

## 读取与交接

1. [SkillSystem 共享基线](../../../../Baseline/SkillSystem/README.md)及本任务相关正文：已确认业务语义与证据边界。
2. [通用技能基建需求](SkillInfrastructureRequirements.md)：整个当前技能系统的公共能力、释放事件和独立验收。
3. [技能配置生产需求](SkillContentProductionRequirements.md)：技能定义、Lua/JSON 内容边界、基建依赖和独立验收。
4. [共同约束与样本验收](SkillChainRefactorRequirements.md)：两类需求共享的行为保持、旧需求取代关系、代表性样本及跨线核实。
5. [旧 Batch02 需求](../Batch02-LuaSkillCompositionPilot/LuaSkillCompositionPilotRequirements.md)：四技能既有语义与历史背景参考；冲突条款由本次需求取代。共享基线不因本轮需求而改写。

程序 PM 读取两类需求、共同约束和基线，统一安排架构接口、实现与 Review；配置生产实施须由不同于 C++ 通用基建的适当实现负责人承担，不把全部配置迁移混派给基建实现职责。具体 Agent 数量和技术拆分由 PM 在架构后确定。基建与配置负责人分别读取对应需求及共同约束；表现线核对既有 Cast Notify 与动画释放时点，不新增表现体验；验证线分别执行两类验收并记录证据。程序交接位置由程序侧确定。

## 状态

通用职责方向和释放事件关系由用户明确；Lua/JSON 的内容分工为用户倾向，细节交程序核实。四技能是代表性样本，并非已确定的完整迁移清单。既有运行行为、具体数值、异常处理和无 Notify 时的行为尚待核实；本需求不代表实现或验收完成。
