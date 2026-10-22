# Batch03：Montage Notify 点位准确性验证

## 目标与范围

用一个代表性 Montage 和一个经确认的 Notify 语义，验证 AI 能否依据明确的语义/时点意图，把点位放到与动画可观察动作相符的位置，并让结果可回读、可检查。具体 Montage 与语义尚未指定，本任务不预设资产或业务含义。

本任务只覆盖这个最小纵切片的意图表达、点位生产结果和可观察验收，不要求完成角色表现主流程收尾或重建动画生产链路。

## 文档

- [Montage Notify 点位需求](MontageNotifyPlacementRequirements.md)：输入意图、结果回读、业务边界、验收和待核实项。

## 读取与交接

下游核验只需先读本入口、[点位需求](MontageNotifyPlacementRequirements.md)、[角色表现共享基线](../../../../Baseline/CharacterPresentation/README.md)及其 `02-SkillActionPresentation.md`、`03-AssetsAndVerification.md`，再读[技能生命周期基线](../../../../Baseline/SkillSystem/02-RequestLifecycle.md)和[技能反馈基线](../../../../Baseline/SkillSystem/06-FeedbackAndObservability.md)。如候选 Montage 或现行生产入口需核对，由程序侧 PM 按其工作流另行读取技术事实。

程序侧 PM 核实候选资产、现有生产/回读能力、Notify 的运行语义及跨层接线边界；策划或用户冻结样本 Montage、Notify 语义和目标动作时点意图；验证记录则按需求比较意图、实际写入结果和游戏内可观察关系。

## 状态与边界

- 当前阶段：策划需求框架已建立，尚未进入程序核查或实现阶段。
- 待冻结：候选 Montage、Notify 语义及目标动作时点意图。冻结前不派发程序执行，也不把本需求框架视为可直接实施的完整任务。
- 排除：Fall 和其他基础状态的全面视觉回归、完整 PIE 视觉回归、技能差异化 VFX、受击表现及整体动画流水线改造。只有直接证明本次点位准确性所需的最小游戏内观察属于本任务。
- Notify 不自动成为命中、治疗、扣费、效果应用或成功的权威来源。若其需要触发技能运行行为，跨层职责、授权和接线须由程序 PM 核实。
