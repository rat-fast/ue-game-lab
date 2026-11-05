# SkillSystem 共享业务基线

## 职责

本目录只记录跨策划、程序和表现任务共用的已确认技能业务语义、当前样本范围、业务边界及其证据状态。它不定义程序架构、Lua API、配置 Schema、字段、具体数值或批次验收方案。

## 当前承诺

- 技能请求、施法、目标、效果和运行结果是不同业务事实；请求或表现意图本身不代表技能已成功。
- 技能专属定义不硬编码在 C++；目标形态、目标关系、效果类型和执行组合由 Lua 表达。高频反复调试的纯数值由 JSON 承载；具体数值不因采用该载体而自动成为产品基线。
- 稳定共用运行机制由 C++/GAS/Runtime 提供，GAS Ability 承担激活和施法生命周期。动画 Cast Notify 通过带 Tag 的 Gameplay Event 通知当前 Ability 到达释放点，由当前 Ability 推进正式释放与效果提交。Notify 只表达释放时点，不等同于命中或效果。
- 实际目标验证、命中、伤害、治疗、持续效果及其移除等结果由权威技能运行机制产生；表现只消费这些真实事实，不伪造业务结果。
- SkillCombatRules 已确认当前技能操作仲裁方向：闪避先取消当前施法再走正式激活；普通受击不打断、重击打断；沉默/硬直阻止新激活并打断当前施法；移动与主动取消不打断；Recovery 期间不激活新技能。单槽 latest-wins 输入意图在当前施法自然完成、退出 Recovery 并清理旧 Cast/Ability 后重新经过正式门控；取消、死亡、Reset 清槽。资源/冷却在提交时生效，提交前取消不扣、提交后不退款；Notify 超时或 Ability 中断取消且不补发效果；每个提交点每 Cast 至多成功一次，重复触发不因重复本身取消施法。
- 当前迁移的四个代表性配置为 `Skill_Fireball`、`Skill_AreaBurst`、当前自疗 `Skill_Mend`、`Skill_BurningBuff`。样本业务摘要见 `03-TargetingAndExecution.md`；Batch01 六技能扩展不属于本轮内容迁移。通用职责方向适用于当前技能系统，不限于四项样本。
- 表现层只消费技能运行层产生的真实事实，不裁定请求、命中、扣费、实际效果、持续状态或施法终态。

## 正文索引

1. `00-DomainModel.md`：业务对象及其关系；
2. `01-InputIntent.md`：输入意图与正式请求的边界；
3. `02-RequestLifecycle.md`：请求、施法和终态的业务概念；
4. `03-TargetingAndExecution.md`：四样本的目标与执行语义；
5. `04-EffectsAndStates.md`：已表达效果类别及效果事实边界；
6. `05-OwnershipAndCleanup.md`：施法、持续效果和重置的业务边界；
7. `06-FeedbackAndObservability.md`：表现和观察可消费的真实事实；
8. `07-ContentBoundary.md`：当前样本、实现证据、限制和待确认项。

## 实现与验证状态

当前实现与证据范围见 `07-ContentBoundary.md`。SkillChainRefactor 的迁移、加载期定义能力与 Notify/Ability 释放链路是独立交付；SkillCombatRules 是后续独立新增的操作仲裁与缓存能力，不重开或混并前者。SkillCombatRules 当前构建、定向自动化及缓存修复独立 Review 通过；生产攻击类别选择者、控制状态授予者、可见 PIE 和 Notify 超时到期测试仍未覆盖。此前用户关于技能正常释放的观察不覆盖本轮新战斗规则。

## 关系

策划批次定义本批需求；ProjectContext 记录可核实的当前项目代码事实；ProgramWorkflow 记录具体任务交接和验证。它们均不因被本基线引用而自动成为业务承诺。
