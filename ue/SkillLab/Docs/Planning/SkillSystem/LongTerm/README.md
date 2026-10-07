# 技能系统长期目标

## 定位

本文记录技能系统希望逐步具备的整体能力方向，用于规划后续批次。它不是当前共享基线，也不表示这些能力已实现或已确认具体规则。每批只选取当前技能样本确实需要的能力进入批次策划。

## 项目方向

SkillLab 当前方向是单机实时战斗、俯视角、MOBA 式操作与 ARPG 式技能效果丰富度。目标是验证 AI 能否基于公共能力生产可运行、可操作、可验证的角色与技能。最终游戏类型、多人需求、DSL、最终内容载体和是否全部由文本生成保持开放。

## 候选能力地图

长期能力主题按以下文档分组，当前是规划索引，不构成实现顺序或冻结契约：

- [M01 输入与技能装配](Infrastructure/M01-InputAndSkillLoadout.md)
- [M02 单位与能力基础](Infrastructure/M02-UnitAndAbilityFoundation.md)
- [M03 请求与施法生命周期](Infrastructure/M03-AbilityRequestAndLifecycle.md)
- [M04 目标与命中](Infrastructure/M04-TargetingAndHit.md)
- [M05 属性、效果与状态](Infrastructure/M05-AttributesEffectsStates.md)
- [M06 执行载体与移动](Infrastructure/M06-ExecutionCarriersAndMovement.md)
- [M07 表现与 UI 反馈](Infrastructure/M07-PresentationAndUIFeedback.md)
- [M08 AI 与测试调用](Infrastructure/M08-AIAndTestInvocation.md)
- [M09 可观测性与内容生产](Infrastructure/M09-ObservabilityAndContentProduction.md)

## 规划约束

1. 先从批次技能中识别玩家可感知需求，再判断哪些需求可抽象为公共基建。
2. 一项长期能力进入批次时，必须落到具体技能样本、明确业务输入输出和可观察验收。
3. 长期能力文档用于收纳候选方向；批次范围以对应批次的技能与基建策划为准。
4. 实现并验证的公共能力经确认后进入共享基线；未实现或未确认的能力继续留在长期目标或批次案。

