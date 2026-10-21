# 技能动作与表现事件基线

## 职责

定义技能动作可以依赖的运行事实、释放/效果与动作时序的业务边界、取消和完成后的清理语义。不规定具体动画方案、Notify 命名、Slot、数据载体或生产工具接口。

## 已确认业务承诺

- 技能表现以运行层结构化事件为事实来源。`Started`、阶段变化、目标/命中、效果、持续伤害、取消和完成是不同事实；动画帧不能自行产生技能结果。
- 一次动作必须关联一次施法身份。迟到的旧事件不能清理或改变新施法的动作；取消和完成只影响对应施法。
- 没有可用动作资源时，技能运行事实仍然成立。降级表现不能伪造施法、命中、扣费或效果。
- 动作恢复与独立持续效果分开：燃烧等持续效果的生命周期以技能系统事件为准，不因动作结束而结束。

## a2c5969 中已实现事实

- `USkillLabSkillAnimationComponent` 订阅表现路由：收到 `Started` 时解析当前技能绑定并尝试播放动作；收到同一 `CastId` 的 `Cancelled`/`Completed` 时停止动作。
- 配置支持 `castMontage` 或 `castAnimation` 二选一。当前 `Config/SkillDefinitions.json` 的火球、区域爆发、修复、燃烧四项均填写 `castAnimation`；运行时对 Sequence 调用 UE 的 Dynamic Montage 创建能力。代码也支持直接加载正式 `UAnimMontage`，但本提交没有证明四项配置实际使用正式 Montage。
- `USkillLabSkillAnimNotify` 只携带事件 Tag；转发前检查当前动画对象、Montage 实例 ID 和 `CastId`。这提供了身份校验路径，但不等于任何资产已配置 Notify 或时点已经验收。
- 释放仍由技能运行时的 Timer 或 GameplayEvent 规则决定；动画组件只转发事件，不负责扣费、命中、效果应用或 Cast 终态。

## 现有证据已验证范围

- 本地 `Saved/Logs/SkillLab-C-PresentationSmoke-20261018.log` 曾记录四项技能依次进入 `Started/Active`、播放动态 Montage、产生各自效果并完成；燃烧在动作完成后继续产生 `DamageTick` 并最终移除，支持“持续效果独立于动作终态”的局部结论。
- 本地 `Saved/Logs/SkillLab-C-CastHandoff-20261018.log` 曾记录取消 Cast 1 后新 Cast 2 播放新 Montage，支持 Cast 身份隔离的运行链证据。`Saved/Logs/SkillLab-C-Automation-Editor-20261018.log` 曾记录 CastLifecycle 与 DefinitionValidation 通过。
- 这些日志来自历史运行，本轮没有重跑；它们不证明真实视口姿势质量、Notify 的实际时间对齐、缺资源降级、取消后基础姿势恢复或玩家输入路径下的四技能完整表现。

## 当前限制与待确认

- 尚无证据证明任何技能资产包含可触发的释放 Notify，也没有证明 Notify 时点与技能实际释放/效果事件一致。
- 缺少动作资源、无 Mesh/AnimInstance、Montage 播放失败时，代码会记录警告并继续技能运行；当前没有专门运行证据证明降级反馈足够清晰且不会误导。
- 不冻结 Montage/Sequence 选择、Slot/Section、Notify 标签、释放帧、动作时长、VFX/音频/镜头或未来技能动作设计。四项技能分别需要怎样的表现验收仍待确认。

## 与其他基线的关系

角色基础状态见 `01-CharacterPresentation.md`；资产和工具边界见 `03-AssetsAndVerification.md`；施法事实和终态以 `../SkillSystem/02-RequestLifecycle.md`、`../SkillSystem/06-FeedbackAndObservability.md` 为准。
