# 程序结构映射

## 目标

把策划模块映射到 GAS、SkillLab C++、Blueprint、配置和表现适配层。本文不冻结最终类名，但冻结首阶段职责边界。

| 策划职责 | GAS | SkillLab C++ | Blueprint/资产 |
|---|---|---|---|
| 单位与 ASC | ASC、AttributeSet、AbilitySpec | 初始化、授予、重置、单位组件 | Character 外壳、默认数据 |
| 请求与生命周期 | GameplayAbility、标签、AbilityTask | RequestId/CastId、阶段、结果 | 调试入口、输入绑定 |
| 目标与命中 | Effect Context | 查询、过滤、命中事件、空间对象 | 预览和 TargetActor |
| 属性与效果 | GameplayEffect、Execution、AttributeSet | 项目伤害、治疗、护盾、驱散 | 效果数据和资源引用 |
| 表现 | GameplayCue 可触发基础 | 项目事件契约和适配 | Montage、Niagara、音频、UI |
| 观测 | GAS 回调/委托 | 事件总线、日志、重置报告 | 调试面板和测试场景 |

## 不变量

玩家、AI、测试从请求入口开始共享能力链；Blueprint 不复制激活、扣费、冷却、命中、效果和清理状态机；配置不直接执行任意代码；表现和日志读取运行事实；临时对象可由 CastId 追踪并在重置时清理。

## 首阶段映射

现有 SkillLabSkillComponent、SkillLabAbilitySystemComponent、SkillLabAttributeSet、SkillLabPulseAbility 和 SkillLabPulseEffect 承载 Skill_TestPulse。后续将硬编码身份、数值和效果迁移到可校验技能定义载体，不先引入任意脚本 DSL。

## 必须形成的技术决策

首批程序任务需要分别给出：技能定义载体、表现事件分发方式、投射物/区域承载方式、UI 状态读取方式、配置校验入口、运行时重置入口和每项选择的替代方案及影响。
