# 技能战斗规则框架

## 目标与范围

为 SkillLab 当前技能系统建立可复用、可组合的通用战斗规则：技能时序、激活和操作仲裁、取消与打断窗口、Gameplay Tag 语义、状态效果与清理，以及内容生产和验收。覆盖系统整体，不限既有四技能或某种固定阶段。已确认规则见策划案第 8 节。

本任务是[技能链路改造](../SkillChainRefactor/README.md)之外的独立功能：沿用 Ability 激活、动画释放事件和权威效果等共享语义；原任务不因本案扩展范围或自动重开。本案不新增具体技能批次、不指定最终 Tag 名称、接口、类、脚本格式或实施拆分，不将网络多人、工具、UI 或新表现体验列为必需交付。

## 阅读和交接

1. [SkillSystem 共享基线](../../../../Baseline/SkillSystem/README.md)：已确认语义与证据边界。
2. [战斗规则框架策划案](SkillCombatRulesRequirements.md)：需求、候选体验、内容边界和验收。
3. [技能链路改造入口](../SkillChainRefactor/README.md)：已有技能内容与释放契约的关联，不继承其本轮范围排除为本案排除。

程序实现与验证摘要见[程序专题入口](../../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRules/README.md)；独立 Review 仍待执行。本文仍是需求权威来源，不代表所有生产内容来源和可见场景已具备。

## 状态

通用规则方向与操作策略已确认，具体依据策划案第 8 节。程序已交付共享 Runtime 仲裁、GAS 分类受击事件接入和定向自动化；没有新增闪避或控制状态技能/Buff 内容。当前权威伤害调用者尚无生产攻击类别标记，Stunned/Silenced 尚无生产授予者；可见 PIE 与独立 Review 待完成，证据及限制见程序实现摘要。
