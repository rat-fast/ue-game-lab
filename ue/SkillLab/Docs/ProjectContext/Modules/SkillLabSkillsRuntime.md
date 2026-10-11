# SkillLabSkills 技能运行时

## Module Responsibility

`SkillLabSkills` 把外部技能请求转换为一次带身份的施法，负责定义加载、技能资格/loadout、请求门控、阶段推进、技能行为、效果执行和结构化事件；它通过公开运行时接口连接玩家输入、GAS 和表现层。

## Code And Assets

- 编排入口：`USkillLabSkillRuntimeComponent`、`FSkillLabCastLifecycle`、`USkillLabSkillLoadout`。
- 定义与数据：`FSkillLabSkillDefinition`、`FSkillLabSkillRequest`、`FSkillLabSkillResult`、`FSkillLabSkillDefinitionProvider`；数值来源为 `Config/SkillDefinitions.json`。
- 执行边界：`USkillLabSkillBehaviorRegistry`、`USkillLabSkillBehavior`、`USkillLabSkillEffectExecutor`、`USkillLabSkillEffectEventAdapter`。
- 当前行为样本：`USkillLabFireballSkill`/`AFireballProjectile`、`USkillLabAreaBurstSkill`、`USkillLabMendSkill`、`USkillLabBurningSkill`。
- 目标与表现桥：`FSkillLabTargetingService`、`USkillLabSkillPresentationRouter`、`ASkillLabSkillFeedbackActor`、`USkillLabSkillAnimationComponent`。

## Runtime Entry

`USkillLabSkillRuntimeComponent::BeginPlay` 调用 `InitializeSkills`。玩家和测试命令最终调用 `SubmitSkillRequest` 或 `CastSkill`；技能行为通过 `NotifyAbilityActivated`、释放事件和 `CompleteBehavior` 回写运行时。

## Runtime Flow

`InitializeSkills` 获取拥有者的 `USkillLabAbilitySystemComponent`，初始化 actor info、loadout、效果执行器和表现路由。请求先检查初始化、施法者、技能授予、当前施法、生命/阻塞状态、冷却、能量和目标；通过后建立 `RequestId`/`CastId`，进入 `Windup`，按 Timer 或 GameplayEvent 释放，进入 `Active`，行为完成后进入 `Recovery` 或终态。过期阶段 token 不得推进新的 Cast；`EndPlay`、`ResetSkills` 和阻塞标签变化会清理活动状态。

## Current Behavior

公开枚举当前包含 `Self/Unit/Ground` 目标形状、`Self/Ally/Enemy/Any` 关系、`Damage/Healing/DamageOverTime` 效果、`InstantTarget/Projectile/GroundArea` 执行模板，以及 `Rejected` 到 `Completed` 的事件集合。当前四个定义由 JSON 提供数值，行为类负责具体命中方式。

## Current Responsibilities And Evolution Boundaries

当前修改技能门控、阶段和公共事件优先查看 `SkillLabSkillRuntimeComponent`、`SkillLabCastLifecycle` 与 `SkillLabSkillTypes`；修改单项技能行为查看对应行为类。定义加载仍是 JSON provider 的当前事实，不应在文档中推断已有 DataAsset/DSL。若异步执行、并行施法或网络权威成为需求，需要重新审查 Runtime、GAS 和行为的所有权边界。

## Verification

自动化入口包括 `SkillLab.Skills.CastLifecycle` 与 `SkillLab.Skills.DefinitionValidation`。历史 smoke 日志中可见初始化、区域命中、修复和燃烧/火球拒绝事件；本轮未重新运行。

## Current Limitations

没有当前验证证明网络同步、输入队列、并行 Cast、完整动画释放或超出四个样本的内容协议。

## Extension Directions

新增技能应优先复用 `FSkillLabSkillDefinition`、统一请求/事件和效果执行边界；若新增执行模板或持续状态语义，应先补充代码验证与共享业务基线，再调整模块职责。
