# 通用技能运行基建架构交接

## 本次行为与范围

依据 [通用技能基建需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillInfrastructureRequirements.md) 和[共同约束](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)，为当前 SkillLab 技能系统提供一致的请求、GAS Ability 激活/施法、正式释放、目标/执行/效果及持续状态事实，并保持既有行为。公共能力范围是当前系统整体，不以四个样本为上限；本任务不迁移 Batch01 新增技能内容，不新增玩法、前后摇操作规则、受击/VFX 体验、工具、UI、热更新或安全策略。

## 已核实运行链

```text
Enhanced Input / 快捷键或测试命令
 -> USkillLabSkillRuntimeComponent::SubmitSkillRequest
 -> FSkillLabSkillDefinitionProvider + USkillLabSkillLoadout
 -> ASC::TryActivateAbility(通用 USkillLabSkillGameplayAbility)
 -> Runtime 建立 RequestId/CastId 与 FSkillLabCastLifecycle
 -> 当前 Cast 的蒙太奇播放
 -> USkillLabSkillAnimNotify(EventTag=SkillLab.Event.Cast)
 -> USkillLabSkillAnimationComponent 校验活动蒙太奇实例及 CastId
 -> 当前实现直接调用 Runtime::ReceiveCastEvent(CastId, Tag)（这不是 GAS Gameplay Event）
 -> 目标链路应由 Skills 公共窄接口把已校验的 Tag 送入 ASC Gameplay Event 通道
 -> 当前 Ability 接收匹配事件并调用带 Ability/Cast 身份的 Runtime 释放入口
 -> Runtime::ReleaseCast：门控/目标复核、一次性释放标记、扣能量/冷却、发事件并创建 Behavior
 -> Targeting / Projectile 或范围查询 / SkillEffectExecutor
 -> SkillLabGAS ASC、AttributeSet、GameplayEffect
 -> Runtime 真实结果事件 -> HUD / Presentation
```

脚本请求动作的链路为 `USkillLabLuaSkillBehavior::RunScript` -> `SkillLabSkill_RequestAction()` -> `RequestDeclaredAction()` -> 按当前 C++ 注册的执行模板进入 Fireball 投射物、AreaBurst 查询或 InstantTarget 效果。当前四份 Lua 脚本内容相同，只调用请求动作并在已完成时调用 `SkillLabSkill_Finish()`；这不是完整的 Lua 技能定义或自由组合层。

## 复用能力与实际缺口

可复用的能力：

- `FSkillLabSkillRequest`、RequestId 去重、唯一活动 Cast、CastId、Runtime generation、生命周期 phase token、一次性 release 标记及 EndPlay/Reset/死亡/阻塞标签清理。
- 请求门控、冷却/能量检查、`FSkillLabTargetingService` 的 Self/Unit/Ground、关系/距离/视线验证，以及释放前再次检查。
- `USkillLabSkillEffectExecutor`、效果事件适配器、ASC/AttributeSet 的伤害/治疗/周期伤害结算；投射物及持续 GameplayEffect 有独立后续生命周期。
- 动画侧已有带 Tag 的 `USkillLabSkillAnimNotify` 和当前 Montage/CastId/实例校验；内容已有 Notify 的资产报告及配置入口。

已确认缺口：

- `USkillLabSkillGameplayAbility` 当前只在 `ActivateAbility`/`EndAbility` 通知 Runtime 激活与结束；自身不等待或接收 Gameplay Event，也不拥有正式释放转移。
- `SkillLabSkillAnimationComponent::ForwardAnimNotify` 最终直接调 Runtime 的 `ReceiveCastEvent`，没有把 Tag 作为 GAS Gameplay Event 分发到 ASC/当前 Ability。`ReceiveCastEvent` 目前只由这条适配路径调用；JSON `releaseTrigger` 字段本身不构成 GAS 事件接线。因此不能把现有带 Tag 的函数调用描述为已经满足“Gameplay Event -> 当前 Ability”。
- 正式 `ReleaseCast`、能量/冷却提交和 Behavior 启动均在 Runtime。需要把“当前激活 Ability 收到匹配 Tag 后正式推进本次释放”的职责接通；事件只表示释放时点，不代表命中/效果。最小跨模块接口建议是：Presentation 只把 `CastId + EventTag` 交给 Skills Runtime 的公开适配入口；Runtime 核对当前 Montage/Cast 身份后，通过 ASC 的 Gameplay Event 通道投递事件；当前 `USkillLabSkillGameplayAbility` 接收/等待事件，再以 Ability 实例和 CastId 调用 Runtime 的有身份释放入口。Presentation 不直接依赖 GAS，Runtime 不把内容规则放进 Ability。必须维持当前 Cast 身份校验并拒绝重复、迟到、错误 Tag 或旧 Cast 事件。
- 技能定义结构、目标形状/关系、效果类别、执行模板及 Fireball 投射物参数仍由 C++ 注册配置提供。当前 JSON 不是完整技能专属定义来源；内容线依赖本任务明确的可表达接口。
- 当前 GAS 代码设置 ASC replication，但项目没有多人同步验证。本轮仍按已有本地单机行为核对，不把网络能力列为目标。

## 职责与窄接口建议

- `SkillLabSkills` 稳定运行层继续持有请求门控、当前施法身份、生命周期、正式提交和事实事件；GAS `USkillLabSkillGameplayAbility` 应成为当前 Cast 的 Ability 生命周期/事件接收者，并调用 Runtime 中有身份校验的释放入口。不要让 Lua、动画、UI 或 Actor 自行结算。
- `SkillLabPresentation` 的 AnimNotify 只携带 Tag；`USkillLabSkillAnimationComponent` 校验 Notify 来自当前 Montage 实例后，调用 Skills Runtime 的适配入口，由 Runtime 经 ASC Gameplay Event 通道交付 Tag 与可识别当前 Ability/Cast 的数据。Ability 必须拒绝不属于当前实例/当前 Cast 的事件。不要增加前摇、后摇、缺失 Notify 超时或退款规则；现有超时和缺失资源表现要由实现对照，不自动沿用成新业务承诺。
- 内容定义/组合与基础机制分层。基建公开最小、受校验的目标/执行/效果能力和结构化结果；内容实现消费该接口，不能赋予脚本直接访问 ASC/Actor 或跳过合法性检查的能力。字段/API/载体细节留给实现，遵守 `ProgramArchitectureContract.md` 的单一权威来源约定。
- `SkillLabGAS` 继续负责属性和实际效果承载；持续效果句柄/移除事实独立于原 Ability/Cast 终态。技能运行层报告实际结果，表现只消费事件。

## 实现落点与必读文件

- `Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp` 与 `Public/SkillLabSkillRuntimeComponent.h`：请求、激活、Cast 生命周期、事件入口、`ReleaseCast`、清理与结果事件。
- `Source/SkillLabSkills/Private/SkillLabSkillGameplayAbility.cpp` 与 `Public/SkillLabSkillGameplayAbility.h`：当前通用 Ability 激活/结束；加入本任务要求的事件接收和当前施法关联。
- `Source/SkillLabPresentation/Private/SkillLabSkillAnimationComponent.cpp`、`Public/SkillLabSkillAnimationComponent.h`、`Private/SkillLabSkillAnimNotify.cpp`、`Public/SkillLabSkillAnimNotify.h`：Notify Tag、Montage 实例/CastId 校验及送达通道。该模块依赖 Skills；不得反向让 Skills 依赖表现模块。
- `Source/SkillLabGAS/Public/SkillLabGASTags.h`、`Private/SkillLabGASTags.cpp`、`Public/SkillLabAbilitySystemComponent.h`：标签定义和 ASC 能力边界。
- `Source/SkillLabSkills/Public/SkillLabSkillTypes.h`、`Private/SkillLabSkillDefinitionProvider.cpp`、`Private/SkillLabSkillLoadout.cpp`：定义字段、事件配置校验及授予 Ability。
- 目标/结算直接依赖：`Private/SkillLabTargetingService.cpp`、`Private/SkillLabSkillEffectExecutor.cpp`、`Private/SkillLabSkillEffectEventAdapter.cpp`、`Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp`、`SkillLabCombatAttributeSet.cpp`。
- 动画配置入口：`Config/SkillDefinitions.json` 中 `castMontage`/`releaseTrigger`/`releaseEventTag`；`Config/Animation/Skill_<id>/` 中结构、patch、readback/report；对应资产在 `Content/SkillLab/Animations/AM_Skill_<id>.uasset`。编辑器命令实现位于 `Source/SkillLabAnimationEditor/Private/SkillLabAnimationContentCommandlet.cpp` 与 `SkillLabAnimationNotifyPatch.cpp`。只核验/维持当前释放 Notify，不扩展动作内容。
- 地图按需读取：`ProjectContext/SkillLabCodeArchitecture.md` 的技能运行链/配置段；`ProjectContext/Modules/SkillLabSkillsRuntime.md`、`SkillLabGAS.md`、`CharacterPresentation.md`、`TestAndVerification.md`。业务语义以 `Baseline/SkillSystem/` 为准；实现还须遵守 `ProjectContext/ProgramArchitectureContract.md`。

## 独立修改与验证

该工作与定义迁移线可由不同负责人处理，因为实现边界是 C++ Ability/Runtime/动画 Tag 适配和其自动化验证，内容线消费稳定接口，不应改写通用生命周期。内容线开始迁移前，双方需明确交付的定义能力和效果结果接口；不必等待全部视觉验收。

至少验证：GAS Ability 激活成功/失败；合法 Cast Notify 经带 Tag Gameplay Event 到达当前 Ability 后才推进释放；错误 Tag、重复/迟到事件、已结束 Cast 和新 Cast 隔离；释放时门控、资源、冷却与旧行为对照；目标失效、取消、死亡、Reset、动画中断或 Notify 不到达时与迁移前可观察行为对照；投射物命中/未命中、范围多目标/空区域、即时治疗与周期效果仍由权威层生成结果。优先扩展 `SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation` 及现有运行 Probe/Exec 入口；完整玩家视觉行为仍需 UE 5.8 可见 PIE/游戏运行验证。

不得把编译通过、定义解析成功或 NullRHI 单独当作完整行为等价；测试中的动画/事件注入仍应经正式 Ability 事件接口。

## 待验证与未决

- Notify 路径已对四个当前动画样本配置 `SkillLab.Event.Cast`，但没有证据说明所有可能当前技能/旧资源都拥有可用 Notify；需核实匹配资源及缺失/中断时旧结果。
- 策划要求正式释放依赖有效 Notify 事件，但旧行为中非动画路径、缺失资源和 `releaseEventTimeout` 的玩家可观察行为须对照。若无法保持既有结果，回交差异和最小选项；不得擅定 Timer fallback、退款、前后摇、受击或 VFX 规则。
- 目标合法性、取消/死亡/Reset、效果身份/移除的具体既有规则按现有需求和基线核验；未冻结部分记录事实，不推广四样本到所有情况。
- 本交接是架构建议，尚无实现或本轮运行验证证据。
