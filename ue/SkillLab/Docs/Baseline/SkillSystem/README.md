# SkillSystem 共享业务基线

## 职责

本目录只记录跨策划、程序和表现任务共用的已确认技能业务语义、当前样本范围、业务边界及其证据状态。它不定义程序架构、Lua API、配置 Schema、字段、具体数值或批次验收方案。

## 当前承诺

- 技能请求、施法、目标、效果和运行结果是不同业务事实；请求或表现意图本身不代表技能已成功。
- 稳定规则由 C++/GAS 执行权威，Lua 用于表达技能流程组合，JSON 承载可调整内容。该方向来自当前 SkillSystem Batch02 策划确认，不代表四技能迁移、运行等价或验收已完成；技术实现细节不属于本基线。
- 当前 SkillSystem Batch02 业务样本为 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff`。样本行为摘要见 `07-ContentBoundary.md`。Batch01 的六技能扩展不属于本批或当前样本范围。
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

本基线核对到的程序侧文档版本为 2026-10-22：SkillSystem Batch02 的策划入口/需求和程序工作流交接/实现摘要当日更新，相关源码、配置及工作流文档在当前工作树中有未提交修改。因此这些材料用于识别实现方向和验证缺口，不作为稳定、已验收的代码事实。SkillSystem `ProjectContext` 目录入口及模块文档最后修改于 2026-10-11，入口明确仍为占位，不能作为当前实现证据。细节见 `07-ContentBoundary.md`。

## 关系

策划批次定义本批需求；ProjectContext 记录可核实的当前项目代码事实；ProgramWorkflow 记录具体任务交接和验证。它们均不因被本基线引用而自动成为业务承诺。
