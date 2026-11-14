# Effect Lifecycle 交付摘要

## 状态

本批实现和定向验证已完成，P2-EL-01 修复版本已交回原 Reviewer 定向复查。验证使用实际工程 `E:/agent/game/ue/SkillLab/SkillLab.uproject` 与 UE 5.8；未修改原始 Review 报告，未提交或推送。F1/F3 Review 涉及的 AttributeSet、解析器及其定向测试保持未改。

## 实现

- `Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp`、`Public/SkillLabAbilitySystemComponent.h`：移除 `RuntimeEffects` 永久强引用。动态定义使用局部 `TStrongObjectPtr<UGameplayEffect>`，其强引用覆盖定义创建、Spec 构造及目标 `ApplyGameplayEffectSpecToSelf` 的整个同步调用；成功返回后由目标 ASC 活跃 `FGameplayEffectSpec.Def` 持有，移除后可回收；拒绝返回后作用域结束并可回收。每次应用继续创建独立 Def，没有跨参数/来源复用，因此 GAS 按 Def 身份聚合的既有堆叠行为不变。周期结果仍按 Spec 地址精确关联；引擎源码确认周期执行从 `ActiveEffect.Spec` 执行并将同一引用传给 AttributeSet/ASC 回调。移除“唯一待处理项就猜为当前 Spec”的兜底，未精确匹配时不广播错误归属结果，也不清除其他待处理记录。
- `Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp`：Reset 的 `Clear(true)` 事件从已存弱引用恢复有效 Source/Target Actor，与自然移除载荷一致；失效 Actor 仍为空，不被强持有。
- `Source/SkillLabSkills/Private/Tests/SkillLabSubmittedActionLifetimeTests.cpp`：扩展 `SkillLab.Skills.SubmittedActionLifetime`，验证 Reset 载荷、活跃效果在 GC 后 Def 存活、移除后 Def 可回收、不同来源/数值/Period 的句柄与周期伤害结果分别正确，以及源 Actor/ASC 销毁后目标 ASC 上已接受 DoT 继续结算。针对 P2-EL-01，目标 ASC ApplicationQuery 在允许和拒绝分支均通过弱引用观察 `Spec.Def` 并在回调中触发 `CollectGarbage`；两分支均断言回调期间 Def 有效，允许分支断言应用成功且活跃效果继续保活、移除后可回收，拒绝分支断言返回后可回收且源 ASC 没有瞬态定义残留。

## 依据与职责边界

UE 5.8 引擎源码 `D:/Epic/UE_5.8/Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/`：`GameplayEffect.h:1202-1204` 定义 `FGameplayEffectSpec::Def` 为 `UPROPERTY TObjectPtr<const UGameplayEffect>`；`AbilitySystemComponent.h:1853-1855` 将活跃效果容器作为 UPROPERTY；`GameplayEffect.cpp:4780-4791` 从 `ActiveEffect.Spec` 执行周期效果并将同一 Spec 引用送入 Self/Source 回调；`GameplayEffect.cpp:3675-3694` 的堆叠查询比较 Def 指针，并按 AggregateByTarget 或 Source ASC 规则查找。由此采用逐应用动态定义、不做定义复用，依赖目标活跃 Spec 持有生命周期，不把定义寿命绑定到 Cast、Runtime 或源 ASC。

Adapter 仍由技能 Runtime 持有并按目标 ASC/handle 观察；Runtime `EndPlay` 的 `Clear(false)` 继续仅解除观察，不移除已经交付的 GE。Reset `Clear(true)` 仍仅移除该 Adapter 跟踪的效果，不扩展为目标 ASC 全局清理。未改变业务叠层、刷新、取消或表现观察迁移规则。

## 验证

- 实际工程 `SkillLabEditor Win64 Development` 构建：Succeeded，53 个动作完成。GAS 文件出现既有 `UGameplayEffect::StackingType` 弃用警告；相邻 `SkillLabResourceEffects.cpp` 有既有 `ConditionalGameplayEffects` 弃用警告。
- P2-EL-01 修复后再次实际工程 `SkillLabEditor Win64 Development` 构建：Succeeded，7 个动作完成。仅有本文件原有 `UGameplayEffect::StackingType` 弃用警告。
- `SkillLab.Skills.SubmittedActionLifetime`：Success。包括新增生命周期断言；多个不同来源/数值/Period 的周期结果按各自 handle 及数值回报；两份动态定义在 GC 后仍由活跃 Spec 保留，主动移除及 GC 后均转为可回收；应用查询拒绝时源 ASC 没有留下瞬态定义；源 Actor 销毁（包含其 ASC）后，目标 ASC 仍产生后续周期结算；Reset 清理事件含有效源/目标 Actor。
- `SkillLab.GAS.DamageCalculation`：Success。
- P2-EL-01 专项：`EffectLifecycleP2Tests.log` 中两项测试 Success、队列正常 `TestExit`。允许查询的 GC 发生在引擎调用栈进入 `Spec.Def->CanApply` 和活跃容器复制前；测试确认作用域强引用覆盖该窗口，允许后续应用并最终回收。拒绝查询的 GC 测试确认应用返回后释放作用域强引用，定义没有残留。
- 实际工程 `-game -nullrhi -SkillLabRuntimeProbe`：退出 0，日志有 `SkillLabRuntimeProbe COMPLETE`。BurningBuff 记录三次各 8 点周期结果，之后 Probe Reset 发出 `EffectTrackingCleared` 移除事件。此 Probe 的移除样本是 Reset 清理，不作为自然到期事件证据；自然到期和多来源场景由 SubmittedActionLifetime 自动化覆盖。NullRHI 不代表可见 PIE 验收。
- 日志：`Saved/Logs/EffectLifecycleTargetedTests.log`、`Saved/Logs/EffectLifecycleRuntimeProbe.log`、`Saved/Logs/EffectLifecycleP2Tests.log`。新 P2 日志启动阶段也有既有 13 条 `Condition failed` 与自动化 World cleanup 告警；目标自动化随后成功，Probe 完成，不能据此称日志无告警。
- 上一验证版本 DLL SHA-256：SkillLabGAS `9406E6339398DB1EFC88162D90D074907B1E35C9890D196DE8F142EEF3A953DB`；SkillLabSkills `777EA0D6317FDC8D8CFA31924FA7B85719B9D3EAD0CEF06CC6BDAAE3F1FA488B`。P2 修复版本 DLL SHA-256：SkillLabGAS `0A468362A5C401B489607D62DDB3FBD805831F1508B27DC2BA1C3EB74F41E7B2`；SkillLabSkills `17BF03822EC2D44C1D11500ACD9DC4931C2A2A281A1B55F3793866AFD9124AA6`。P2 专项自动化运行于后者的实际工程构建产物。

## 尚未覆盖

已覆盖 ApplicationQuery 同步 GC 及允许/拒绝、常规活跃 Spec GC、移除回收和源 Actor/ASC 同步销毁；没有单独销毁目标 Actor/ASC、网络复制/预测、多进程或 PIE 可见表现测试。多来源周期测试使用正式 GAS 应用接口与真实 AttributeSet 周期回调，未构造属性执行回调内嵌套施加另一个 GE 的特殊重入场景；精确地址匹配下此类非匹配不会误发结果，但相关待处理记录的极端嵌套/异常清理未专项压力验证。
