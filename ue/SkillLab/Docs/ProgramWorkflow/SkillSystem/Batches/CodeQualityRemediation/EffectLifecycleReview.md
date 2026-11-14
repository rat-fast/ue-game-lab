# 效果生命周期独立 Review

2026-11-14（Asia/Shanghai）定向复查。结论：**P2-EL-01 修复通过，本批效果生命周期任务可关闭。** 原先应用查询回调内 GC 的动态定义悬空窗口已由作用域 GC 强引用覆盖；allow/reject 两路径测试和实际工程定向日志支持该结论。首次 Review 的其他范围及验证限制仍有效。

## 范围与依据

按 `ue-project-context`、`ai-native-programming`、`ai-program-review` 取得仓库规则、SkillLab 文档入口与 `ProgramArchitectureContract.md`。审查依据为本批 `EffectLifecycleHandoff.md`、`EffectLifecycleSummary.md`、原主审 F4、`StateAndLifecycleReview.md` 的 SL-01 及周期事件边界、技能系统基线 `04-EffectsAndStates.md` / `05-OwnershipAndCleanup.md` 和 `SkillInfrastructureRequirements.md`。验证入口按 `ProjectContext/Modules/TestAndVerification.md` 核对。

首次 Review 与本次定向复查分别只读核对对应实现版本、摘要和已有证据；本次仅更新本 Review 报告，未修改被审代码、测试、配置或资产，未构建、运行、使用 UE MCP 或操作 Git。另一能力归属任务可继续使用构建入口，本报告只对下列明确版本负责。

## P2-EL-01 复查：通过

修复位于 `Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp:147`：局部 `TStrongObjectPtr<UGameplayEffect>` 在动态定义创建前建立并一直活到 `ApplyGameplayEffectSpecToSelf` 返回；Spec 使用 `Effect.Get()` 构造。返回后局部强引用释放，成功应用由目标活跃 Spec 接续保活，拒绝应用不留下 ASC 永久引用。此所有权窗口与 GAS 在活跃容器复制之前调用 `GameplayEffectApplicationQueries` 的顺序相符。

目标活跃 Spec 的 `UPROPERTY Def` 保活链只在效果进入可达活跃容器后成立，不能覆盖此前的调用栈。局部 `UGameplayEffect*` 和局部 Spec 内的 `TObjectPtr` 不会自行注册为 GC 根；`FGameplayEffectSpecHandle::Data` 是普通 `TSharedPtr<FGameplayEffectSpec>`，不是 GC 引用所有者。定义的 Outer 是源 ASC，也不能替代对该子对象的可达引用；现有“主动移除后定义可回收”测试本身也说明 Outer 不会永久保留定义。

本机引擎的实际顺序是：

- `GameplayAbilities/Private/AbilitySystemComponent.cpp:1037` 先执行 `GameplayEffectApplicationQueries`，`:1047` 再访问 `Spec.Def->CanApply`，`:1080` 才调用活跃容器的 `ApplyGameplayEffectSpec`。
- `GameplayAbilities/Public/GameplayEffectTypes.h:1624` 的 Handle 只拥有普通共享指针；其局部实例没有外部 GC 扫描入口。
- `GameplayAbilities/Public/AbilitySystemGlobals.h:99` 的 `PushCurrentAppliedGE` / `SetCurrentAppliedGE` / `PopCurrentAppliedGE` 默认为空，`FScopeCurrentGameplayEffectBeingApplied` 并不替调用方保活定义。

新增测试在目标 ASC 的 allow 与 reject ApplicationQuery 回调中均保存 `Spec.Def` 弱引用、触发 `CollectGarbage` 并断言定义仍有效。allow 分支继续断言 Apply 成功、目标活跃效果保留定义，移除并再次 GC 后定义可回收；reject 分支断言返回后定义可回收且源 ASC 的瞬态定义数未增长。这直接覆盖原发现的同步调用窗口及两个所有权交接结果。代码和断言相符，未发现仍有未保护的调用间隙，**P2-EL-01 通过复查并关闭**。

## 已支持的结论

- 旧 `RuntimeEffects` 的会话内永久保留已经移除。目标 ASC 的 `UPROPERTY ActiveGameplayEffects → GameplayEffects_Internal → FActiveGameplayEffect.Spec → UPROPERTY Def` 可达链成立；引擎依据分别见 `AbilitySystemComponent.h:1855`、`GameplayEffect.h:1911` / `:1420` / `:1204`。原有应用后 GC 和移除后 GC 测试支持正常活跃/移除边界；本轮新增查询回调内 GC 测试覆盖作用域强引用到活跃 Spec 或函数返回后回收的交接。
- 仍逐应用创建独立定义，没有跨来源、Period 或 Duration 复用。引擎 `GameplayEffect.cpp:3675` 的堆叠查找比较 Def 指针，因此此次变更没有额外合并不同应用。没有新增技能内容专属分支或改变模块职责。
- 周期结果现在只按精确 Spec 地址消费，并在广播前移除待处理记录。引擎 `GameplayEffect.cpp:4762` 从同一 `ActiveEffect.Spec` 执行属性结算，再传入周期委托，支持正常路径地址一致；删除单条猜测兜底不会将不匹配结果误归其他 handle。
- Adapter `Clear(true)` 从弱引用恢复有效 Actor，与自然移除一致；失效 Actor 保持为空。`Clear(false)` 仍只解绑观察，Reset 仍只移除本 Adapter 跟踪的效果。没有将 Cast 结束当作 GE 结束，也没有增加观察迁移规则。
- 本次直接改动的生产实现及测试均低于约 400 行参考值，职责落点集中在 GAS 定义/周期结果与技能事件适配，没有发现为整改引入额外框架的问题。

## 验证证据及限度

构建证据来自实现交付摘要 `EffectLifecycleSummary.md`：实际工程 `SkillLabEditor Win64 Development` 构建成功，P2 修复构建 7 个动作完成。Reviewer 未重做构建。修复 DLL 当前 SHA-256 与交付记录相同：GAS `0A468362A5C401B489607D62DDB3FBD805831F1508B27DC2BA1C3EB74F41E7B2`，Skills `17BF03822EC2D44C1D11500ACD9DC4931C2A2A281A1B55F3793866AFD9124AA6`。

| 已核对证据（项目相对路径） | 支持的结论 |
| --- | --- |
| `Saved/Logs/EffectLifecycleP2Tests.log:2095`、`:2259`、`:2266` | DamageCalculation、SubmittedActionLifetime Success，测试队列正常 TestExit。命令行指向实际工程；日志随后包含启动/清理告警，详见下文。 |
| `Source/SkillLabSkills/Private/Tests/SkillLabSubmittedActionLifetimeTests.cpp:182` 起 | Allow 与 Reject ApplicationQuery 都在回调内 GC；弱引用断言当时 Def 有效。Allow 继续验证成功应用、活跃 Spec 持有及移除后回收；Reject 验证函数返回后回收且源 ASC 无定义残留。 |
| `Source/SkillLabSkills/Private/Tests/SkillLabSubmittedActionLifetimeTests.cpp:127` 起 | 两个不同来源、伤害、Period/Duration 的活跃 handle/Def 分离；应用返回后 GC 保活；按 handle 检查 RawDamage；主动移除后 GC 回收；拒绝应用返回后无瞬态定义残留。 |
| 同一测试 `:100` 起及 `:202` 起 | Cast 取消/完成后的持续伤害、自然移除、Reset 停止并恢复移除事件 Actor 载荷、Runtime 销毁后继续伤害而不补造移除事件；源 Actor Destroy（包含组件销毁路径）后目标继续周期结算。 |
| `Saved/Logs/EffectLifecycleRuntimeProbe.log:1733`、`:1747`、`:1750`、`:1753`、`:1757` | BurningBuff 三次各 8 点周期结果，随后 Reset 的 EffectTrackingCleared，最后 Probe COMPLETE。不能将该 Reset 样本描述为自然到期。 |

修复 P2 日志的 SHA-256 为 `07A7730EF0C6E273CB80968E64564B9E4E17F56445B91BDBBCEAE80F26A06D6C`。该日志有启动期 13 条 `Condition failed`、HTTP 请求超时、GameplayCue fallback 和 World cleanup 告警；随后两项目标测试均 Success 并正常 TestExit，不能描述为全日志无告警或全套自动化成功。

仍未专项验证：反复自然到期应用后的定义数量收敛；属性回调内嵌套 GE 的重入；源 Actor Destroy 后再 GC 的定义/Outer 边界；目标 Actor/ASC 销毁；网络/预测及可见 PIE。多来源断言主要检查 RawDamage 与各 handle 至少产生若干事件，未逐次核验完整 ActualDamage、精确事件总数及所有来源事件载荷，不能扩写成这些场景全部已证。这些是本任务之外的验证边界，不阻塞本次明确的 P2-EL-01 修复关闭，也不新增业务规则。

## 被审版本定位

以下为修复版本及证据的 SHA-256；本报告没有冻结其他任务文件，也不声明整个工作树与某历史提交完全一致。首次 Review 读取的旧实现哈希保留在先前交付记录中，不作为本次通过结论的版本。

| 文件（相对项目根） | SHA-256 |
| --- | --- |
| `Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp` | `F67EF55E0CDD97462A903D42DEE6B099522C4B5703DD71A56AAD2FBBD679C6C5` |
| `Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h` | `174FCDD41EEB90E3B0BCD8B48312CE4D8BE2C3C89D06135810A1AD5B9E4F282E` |
| `Source/SkillLabSkills/Private/SkillLabSkillEffectEventAdapter.cpp` | `F906A232AE5EE60BB2522CBA7D91FC70FA6FD515A2E4F9480BF6F23969F871EF` |
| `Source/SkillLabSkills/Private/Tests/SkillLabSubmittedActionLifetimeTests.cpp` | `FF9D258AB64B6139973A759726FDC10D827C7A8EDFA2151DE56B004C8299DEF4` |
| `Saved/Logs/EffectLifecycleP2Tests.log` | `07A7730EF0C6E273CB80968E64564B9E4E17F56445B91BDBBCEAE80F26A06D6C` |
| P2 修复 GAS DLL | `0A468362A5C401B489607D62DDB3FBD805831F1508B27DC2BA1C3EB74F41E7B2` |
| P2 修复 Skills DLL | `17BF03822EC2D44C1D11500ACD9DC4931C2A2A281A1B55F3793866AFD9124AA6` |

已向 PM 回报复查结果：P2-EL-01 的代码修复、allow/reject 回调 GC 覆盖及实际工程定向验证通过；效果生命周期本批可关闭。无需修改原始主审报告或重新制定持续效果的业务叠层、死亡清理和观察迁移规则。
