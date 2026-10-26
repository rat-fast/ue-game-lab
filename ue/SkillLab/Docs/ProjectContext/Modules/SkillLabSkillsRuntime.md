# SkillLabSkills 技能运行时

## Module Responsibility

`SkillLabSkills` 把外部技能请求转换为一次带身份的施法，负责定义加载、技能资格/loadout、请求门控、阶段推进、技能行为、效果执行和结构化事件；它通过公开运行时接口连接玩家输入、GAS 和表现层。

## Code And Assets

- 编排入口：`USkillLabSkillRuntimeComponent`、`FSkillLabCastLifecycle`、`USkillLabSkillLoadout`。
- 定义与数据：`FSkillLabSkillDefinition`、`FSkillLabSkillRequest`、`FSkillLabSkillResult`、`FSkillLabSkillDefinitionProvider`；数值来源为 `Config/SkillDefinitions.json`。
- 执行边界：`USkillLabSkillBehaviorRegistry`、`USkillLabSkillBehavior`、`USkillLabLuaSkillBehavior`、`USkillLabSkillEffectExecutor`、`USkillLabSkillEffectEventAdapter`。
- 当前行为样本：`USkillLabFireballSkill`/`ASkillLabFireballProjectile`、`USkillLabAreaBurstBehavior`、`USkillLabMendBehavior`、`USkillLabBurningBehavior` 的注册配置仍提供结构能力；四个当前定义实际使用通用 `USkillLabLuaSkillBehavior`，脚本位于 `Config/Skill_Fireball.lua`、`Config/Skill_AreaBurst.lua`、`Config/Skill_Mend.lua` 和 `Config/Skill_BurningBuff.lua`。
- Lua 依赖：`SkillLabSkills` 通过 Private dependency 使用 `SkillLabLuaRuntime` 的 opaque facade；Lua 5.4.8 唯一源码位于 `Source/SkillLabLuaRuntime/ThirdParty/lua-5.4.8/src/`，不再由 Skills 编译。技能宿主不调用 `luaL_openlibs`，只绑定 `SkillLabSkill_RequestAction()` 和 `SkillLabSkill_Finish()`。
- 目标解析与技能行为继续位于 `SkillLabSkills`；表现桥、反馈 Actor 与技能动画组件位于独立 `SkillLabPresentation` 模块。运行时只发布 `OnNativeSkillEvent`，不创建或持有表现对象。

## Runtime Entry

`USkillLabSkillRuntimeComponent::BeginPlay` 调用 `InitializeSkills`。玩家和测试命令最终调用 `SubmitSkillRequest` 或 `CastSkill`；技能行为通过 `NotifyAbilityActivated`、释放事件和 `CompleteBehavior` 回写运行时。

## Runtime Flow

`InitializeSkills` 获取拥有者的 `USkillLabAbilitySystemComponent`，初始化 actor info、loadout、效果执行器和表现路由。请求先检查初始化、施法者、技能授予、当前施法、生命/阻塞状态、冷却、能量和目标；通过后建立 `RequestId`/`CastId`，进入 `Windup`，按 Timer 或 GameplayEvent 释放，进入 `Active`，行为完成后进入 `Recovery` 或终态。Lua 定义在正式请求前进行脚本文件、编译和入口预检；脚本执行绑定当前 Runtime generation、CastId 和行为身份，并受指令预算限制。`USkillLabLuaSkillBehavior::RequestDeclaredAction` 按定义的 `Projectile`、`GroundArea` 或 `InstantTarget` 执行模板调用现有 Projectile、TargetingService 或 EffectExecutor；Lua 只请求动作和收尾，真实目标、实际效果、持续效果和终态仍由 C++/GAS 产生。过期阶段 token 不得推进新的 Cast；`EndPlay`、`ResetSkills` 和阻塞标签变化会清理活动状态。

## Current Behavior

公开枚举当前包含 `Self/Unit/Ground` 目标形状、`Self/Ally/Enemy/Any` 关系、`Damage/Healing/DamageOverTime` 效果、`InstantTarget/Projectile/GroundArea` 执行模板，以及 `Rejected` 到 `Completed` 的事件集合。当前四个定义由 JSON 提供数值、阶段、动画路径、阻塞标签和 Lua 脚本绑定；目标形状、关系、效果类型、执行模板及 Fireball 投射物参数仍来自对应 C++ 注册配置。四个当前技能均使用 Lua Behavior，Lua 不能改变定义外的目标或效果类型。

## Current Responsibilities And Evolution Boundaries

当前修改技能门控、阶段、身份和公共事件优先查看 `SkillLabSkillRuntimeComponent`、`SkillLabCastLifecycle` 与 `SkillLabSkillTypes`；修改 Lua 动作边界查看 `SkillLabSkillBehavior`；修改定义字段和脚本路径校验查看 `SkillLabSkillDefinitionProvider`；修改单项技能的目标/关系/执行模板/投射物默认值查看对应注册文件。当前 JSON 不是完整技能结构的唯一来源，Provider 先应用 C++ 注册配置，再覆盖已支持的 JSON 内容字段。若异步执行、并行施法或网络权威成为需求，需要重新审查 Runtime、GAS、Lua Behavior 和行为的所有权边界。

## Verification

自动化入口包括 `SkillLab.Skills.CastLifecycle` 与 `SkillLab.Skills.DefinitionValidation`。最新四技能 Lua 自动化日志为 `Saved/Logs/Batch02-Lua-FourSkill-Automation.log`，两项均为 `Result={Success}`，进程退出码为 `0`。最新 RuntimeProbe 为 `Saved/Logs/Batch02-Lua-FourSkill-RuntimeProbe.log`，记录四个 Lua 技能的阶段/效果事件并以 `SkillLabRuntimeProbe COMPLETE` 结束；运行使用 NullRHI，不能替代可见 PIE 验收。

## Current Limitations

没有当前验证证明网络同步、输入队列、并行 Cast、完整动画释放、四技能失败注入、坏内容恢复或超出四个样本的内容协议。Fireball 未命中/阻挡、AreaBurst 空区域/独立多目标、Mend 受伤治疗、脚本异常、效果失败、重复请求和 Reset/死亡恢复仍待专门验证。

## Extension Directions

新增技能应优先复用 `FSkillLabSkillDefinition`、统一请求/事件和效果执行边界；若新增执行模板或持续状态语义，应先补充代码验证与共享业务基线，再调整模块职责。
