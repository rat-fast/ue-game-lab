# 技能动作与表现事件基线

## 职责

定义技能动作表现可以依赖的运行事实、释放时机的业务边界和表现清理语义。本文不规定技能最终动画方案、数据载体、动画工具或 C++/Blueprint 接口。

## 当前共享承诺

- 技能表现以技能运行层发布的结构化事件为事实来源。表现可以显示开始、阶段变化、目标选定、命中/效果、持续伤害、取消和完成等事实；表现本身不能伪造这些事实。
- 一次技能动作必须能与一次施法身份对应。旧施法的延迟事件不能改变新施法的表现状态；取消或完成只影响其对应的当前动作。
- “动作开始”“释放/效果发生”和“施法终态”是不同业务事实。动画时间点、命中结果、效果应用和 Cast 完成不能仅凭同一个视觉帧互相推断。
- 没有可用动作资源时，技能运行事实仍然成立；缺少动画不能被解释为技能拒绝、未命中或效果失败。

这些承诺依据 `../SkillSystem/00-DomainModel.md`、`02-RequestLifecycle.md`、`06-FeedbackAndObservability.md`，并与基线运行时公开的 `Started`、`PhaseChanged`、`TargetSelected`、`Hit`、`DamageTick`、`EffectApplied`、`BuffApplied`、`BuffRemoved`、`Cancelled`、`Completed` 事件集合相一致。

## 当前实现与验证事实

- 基线角色包含技能动画适配组件；它订阅表现路由的运行事件，在 `Started` 尝试播放该技能定义的动作，在相同施法身份的 `Cancelled`/`Completed` 时停止动作。来源：`Source/SkillLabSkills/Public/SkillLabSkillAnimationComponent.h` 与对应 `.cpp`。
- 基线代码提供一个可携带 Gameplay Tag 的动画 Notify，并把 Notify 转回当前施法事件；代码会检查动画对象、活动动作、施法身份和动作实例是否匹配。来源：`SkillLabSkillAnimNotify.h/.cpp`、`SkillLabSkillAnimationComponent.cpp`。
- 基线表现路由会广播运行结果，并为目标选定、命中、周期伤害和持续效果添加屏幕消息或临时世界反馈。来源：`SkillLabSkillPresentationRouter.h/.cpp`、`SkillLabSkillFeedbackActor.*`。
- 基线 `Config/SkillDefinitions.json` 为 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff` 各提供一个 `castAnimation` 软路径；其中火球指向 `Content/SkillLab/Animations/AS_FireballCast.uasset`，其余指向 Unarmed Attack 目录动作。代码还允许定义读取 `castMontage`，但这只是实现能力事实，不是本基线的生产约定。
- `SkillLab.Skills.CastLifecycle` 自动化测试覆盖施法阶段、过期 token、取消和事件驱动释放；`SkillLab.Skills.DefinitionValidation` 覆盖定义文本的基本校验。两者都没有证明动画播放、Notify 释放或视觉时序。

## 当前限制

- 基线代码存在动作播放和 Notify 接线，但没有当前 UE 运行证据证明四个技能的动作资源都能加载、播放、对齐或回传事件。
- 当前验证没有证明某个动画 Notify 已写入资产，也没有证明 `castAnimation` 的软路径在默认地图中可解析。
- 运行时事件可被表现观察，不等于所有事件已有专用 VFX、音频、镜头或角色动作。
- 不冻结 Montage/Sequence 选择、Slot 名、Notify 命名集合、释放事件标签、事件到动画帧的时间表、完整动画框架、异步阶段或未来技能动作设计。

## 待确认问题

- 当前四个技能是否都需要动作驱动释放，还是允许按技能选择计时释放？
- 哪些事件需要角色动作，哪些只需要世界反馈、UI 或音频？
- 动作资源缺失时，后续体验验收应要求降级反馈还是直接标记为内容缺陷？

## 与其他基线的关系

施法阶段和技能结果以 `../SkillSystem/02-RequestLifecycle.md`、`06-FeedbackAndObservability.md` 为准；角色基础动作见 `01-CharacterPresentation.md`；资产路径和验证证据等级见 `03-AssetsAndVerification.md`。

