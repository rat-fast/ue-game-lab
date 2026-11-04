# 通用技能运行基建独立 Review

## 结论

**加载期定义接口 Review 通过，可由 PM 按原工作流重新派发四项 Lua/JSON 内容迁移。**源码提供了 Lua 分类声明、JSON 数值校验、单一规范化 `FSkillLabSkillDefinition`、加载期 Lua 源快照，以及只绑定通用 Ability/Behavior 的装配；未发现已核验范围内阻止迁移的实现缺陷。原 Notify/Ability 事件链接口结论继续成立。

此通过仅覆盖接口和实现，不表示四项内容已迁移或行为等价已验收。真实 JSON/Lua 仍是旧格式，当前 Provider 按预期拒绝其目录；配置迁移仍须由 PM 重新派发，之后执行真实四项运行与迁移前快照对照。

## 已核对的事件链

- `USkillLabSkillAnimationComponent::ForwardCastEvent` 将当前动画校验后的 CastId/Tag 交给 `Runtime::ForwardValidatedNotify`。Runtime 检查当前 Cast、活动 Ability、ASC 和已声明 Tag 后，创建带 Runtime/Ability/CastId 的私有 UObject 身份载荷，通过 `ASC->HandleGameplayEvent` 分发。
- `USkillLabSkillGameplayAbility::BindSkillCast` 为活动定义的释放 Tag 和 Behavior Tag 创建 `UAbilityTask_WaitGameplayEvent`。回调校验载荷类型、CastId、Ability、Runtime 和 Instigator，再转交 Runtime；Runtime 再检查当前 Ability/Cast/阶段/Tag，只有 Windup 中的匹配释放 Tag 才调用有身份的释放入口。
- 普通同 Tag ASC 事件没有私有身份对象，不能通过 Ability 校验。自动化测试还验证了释放前不扣资源、事件后只提交一次、错误 Cast/重复/旧 Cast/取消/Reset 后事件不推进释放。`EndAbility` 清除绑定身份并释放任务引用；`FinishCast` 与 `ClearPendingCast` 结束 Ability，GAS AbilityTask 生命周期负责随 Ability 结束清理等待任务。
- 释放后仍走原 Runtime 链：再次做门控和目标有效性检查，然后标记一次性释放、扣能量、记冷却、发阶段/目标事实，再启动原 Behavior、效果执行和恢复。投射物及持续效果通过后续独立事件/句柄处理，不以 Ability 终态代替其结果。
- Runtime 保留显式 `Timer` 释放分支：配置为 Timer 时按 Windup timer 释放；GameplayEvent 分支的 `ReleaseEventTimeout` 到期会取消，不会作为 Timer 兜底释放。当前 JSON 四项均配置 `GameplayEvent` 和 `SkillLab.Event.Cast`，因此保留分支未被本轮样本覆盖。

## 加载期定义接口核验

- Lua 加载期 `SkillLabSkill_Define(key, value, ...)` 只收字符串键值对，要求恰好调用一次，拒绝奇数参数、空/未裁剪值、重复键、缺失/未知键及未知枚举；声明必须同文件定义 `OnSkillCast`。分类字段由 Lua 赋值，不再由技能 ID builder 决定。
- JSON 使用严格已知字段集和 token-reader 重复键检查；数值读取区分 Number/Boolean/String，限制有限且绝对值不超过 100000，再检查非负/正数边界。`range` 必需；Ground 必需正 `radius`；Projectile 必需速度、碰撞半径、寿命、显示缩放和拦截布尔值，非 Projectile 拒绝这些键。周期伤害必需正 duration/period。Self/Relation、Ground/GroundArea、Projectile/Unit 等跨字段组合在整项校验中拒绝不相容项。测试实际断言了 Ground range/radius 和 Projectile 五个值进入规范化定义。
- Provider 只给每项补通用 Ability/`USkillLabLuaSkillBehavior` 类。技能 ID builder 注册和四个技能专属 behavior 类文件已删除；通用 Behavior 按规范化 Execution 分派 Projectile/GroundArea/InstantTarget。效果经 `Runtime::ApplyBehaviorEffect` 和 `SkillLabSkillEffectExecutor`，执行器复核当前 Cast、目标存活、关系后进入 GAS；请求和释放仍由门控与 TargetingService 校验。
- `SkillLabLua::Create()` 使用空 `luaL_newstate`，加载期及运行期均未调用 `OpenLibraries`；宿主只设置 Define、RequestAction、Finish 回调，无 Actor/ASC/World、反射或结算对象暴露。两阶段均设 100000 指令预算，加载期限制源文本 256 KiB。运行期使用定义内 `LuaSource` 重放，不重新从磁盘读取；正式文件加载入口是 Provider `ReloadDefinitions()`。
- 候选目录先在局部 map 完整解析，`ReloadDefinitions()` 仅在 `LoadCandidate` 成功后移动替换全局目录；LuaSource 与字段处于同一个定义值中。`Resolve()` 只拷贝当前缓存定义。`ParseCatalogText` 是纯候选解析，不改正式目录。

## 证据与范围

此前 `SkillChainRefactor-Automation.log` 的三个 Skills 自动化通过，且 `CastGameplayEvent` 直接验证 Runtime 适配入口到 ASC/当前 Ability；此前 `SkillChainRefactor-RuntimeProbe.log` 的四样本事件、资源/冷却、效果、持续伤害/移除、取消和 Reset 属于加载期定义实现前，不能作为当前 Provider 下的四技能运行或等价证据。两个 Probe 均为 NullRHI，也不是可见 PIE 验收。

基建通用能力范围是当前系统整体，四个样本不能作为整个系统覆盖证明。程序交接给出的当前定义清单是四项，但本次没有逐项清点所有公共链路分支或验证所有定义类别；新增内容线接口完成时需结合该范围重核，不能从四样本推出完整覆盖。

本轮实现摘要报告 UE 5.8 `SkillLabEditor Win64 Development` 构建成功；Saved 日志目录未提供独立编译日志供本 Review 重核。`SkillDefinitionLoading-Automation.log` 中 `CastGameplayEvent`、`CastLifecycle`、`DefinitionValidation` 均 `Result={Success}`，退出码 0。自动化覆盖有效声明/数值规范化、Lua 缺失/重复/未知/类型/执行错误、JSON 未知/重复/类型/非有限/范围边界、重复 ID、部分跨字段组合，以及 Projectile 数值落入定义对象。

`SkillDefinitionLoading-RuntimeProbe.log` 记录 Provider 因真实 `Skill_Fireball.lua` 缺少新 `SkillLabSkill_Define` 声明而拒绝目录；Fireball、AreaBurst、Mend、BurningBuff 和第二次 Fireball 请求均返回 `NotInitialized`。进程退出码 0 / `SkillLabRuntimeProbe COMPLETE` 只表示 Probe 正常结束，不能作为四技能运行或等价证据；该结果符合内容文件尚未迁移的边界。当前没有可见 PIE。

## 非阻塞证据缺口

以下是实现摘要明确未覆盖、且本轮日志未补足的既有行为对照证据。应在最终复查/验收记录中保留为未验证项；本 Review 不据缺少证据推断代码必然错误：

- Notify 缺失、动画中断、释放事件超时后取消，以及 Timer 配置路径；尚未和改造前可观察行为对照。
- Ability 激活失败的行为；当前自动化覆盖激活成功，没有触发失败路径。
- 释放前目标失效及目标状态变化；代码会在释放时重新验证，但日志没有复现该路径及其旧行为对照。
- 施法者死亡后 Reset 的组合清理行为；当前日志有持续效果运行、Reset 和被取消 Cast，但没有死亡后 Reset 对照。
- 可见 UE 5.8 PIE/游戏视口；NullRHI 仅支持运行链/日志观察，不支持宣称姿势、Notify 资源表现或玩家视觉验收通过。

加载期接口还有以下自动化/行为证据未直接覆盖；静态调用路径与实现支持当前结论，属于建议在迁移线或后续定向测试补充的证据，不阻止配置迁移：

- 原子性自动化目前在 `ParseCatalogText` 临时输出 map 上断言失败后原条目保留、坏条目未暴露；没有直接先装入 Provider 全局目录，再失败 reload 并断言旧 `Resolve()` 与旧 LuaSource 同时保留。生产代码只在候选全成功后替换全局 map，LuaSource 与定义同值保存。
- 没有自动化显式断言已缓存 LuaSource 后修改/删除磁盘文件仍执行旧快照；源码显示 `RunScript()` 只读 `Definition.LuaSource`。也没有对标准库和各禁止全局做自动化负向断言；源码显示 VM 未开库且 facade 只安装白名单回调。
- 自动化覆盖部分必需项和组合边界，但未逐字段覆盖缺少 Ground radius、缺少全部 Projectile 参数、缺少 DoT duration/period、每个数值字段的零/负边界及所有关系/执行组合。实现存在相应校验；此缺口不替代真实迁移内容验证。
- Probe 因目录加载前失败，当前没有已迁移 Lua 调用正式动作 API 的运行证明；配置线迁移后仍需验证动作接受、Targeting/GAS 实际效果和源快照行为。

## 复查入口

- 需求：[通用技能基建需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillInfrastructureRequirements.md)、[技能配置生产需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillContentProductionRequirements.md)、[共同约束](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)
- 架构：[通用运行基建交接](../SkillRuntimeInfrastructureHandoff.md)、[内容定义迁移交接](../SkillDefinitionMigrationHandoff.md)
- 实现与证据：[实现摘要](../ImplementationSummary.md)、`Saved/Logs/SkillChainRefactor-Automation.log`、`Saved/Logs/SkillChainRefactor-RuntimeProbe.log`
- 主要源码：`Source/SkillLabPresentation/Private/SkillLabSkillAnimationComponent.cpp`、`Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp`、`Source/SkillLabSkills/Private/SkillLabSkillGameplayAbility.cpp`、`Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`、`Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp`、`Source/SkillLabSkills/Private/SkillLabSkillBehaviorRegistry.cpp`
