# 首阶段实现切片：Skill_TestPulse

> 本文记录当前实现事实和交接边界。历史审计快照见 `ImplementationAudit.md`，编译/运行证据见 `Validation/`。本切片不修改 Planning 文档。

## 当前实现事实

- `ASkillLabCharacter` 挂载 `USkillLabAbilitySystemComponent`、`USkillLabAttributeSet` 和 `USkillLabSkillComponent`；移动、镜头和导航链保持原职责。
- `FSkillLabSkillDefinitionProvider::ResolveDefinition` 返回 `Missing`、`Disabled`、`Invalid` 或 `Available`。当前提供 `Skill_TestPulse` 和 `Skill_RecoverEnergy` 两个版本 1、来源 `CppTypeProvider` 的定义。
- `InitializeUnit` 遍历 `GetDefaultSkillIds`，逐项调用通用 `GrantSkill`；授予记录分开保存业务 `GrantedAbilityId` 和 GAS `FGameplayAbilitySpecHandle`，`SubmitSkillRequest` 按请求 SkillId 查找授予记录。
- `FSkillLabSkillRequest` 必须携带 `CasterObject` 和 `CasterUnitId`。`SubmitSkillRequest` 要求对象等于组件 Owner、单位 ID 等于本组件 `UnitId`；拒绝不会创建 `CastId`、扣费或启动冷却。
- Unit 目标必须解析到已初始化且非零 `UnitId` 的 `USkillLabSkillComponent`；无组件、未初始化或零 ID 返回 `TargetUnitIdUnavailable`，请求 ID（若提供）不一致返回 `TargetUnitIdMismatch`。
- 请求和事件携带 `TargetLocation/TargetDirection` 快照；表现路由只读取事件快照。
- 提交成功后，运行组件保存定义版本、来源、执行模板和表现绑定快照。后续效果执行使用这份快照，不重新选择技能定义。
- 初始 `TestValue` 和 `SkillEnergy` 只由 `USkillLabAttributeSet` 的默认入口提供；`SkillLabSkillComponent` 不再保存重复初始值。
- GAS 接受激活后，组件创建 `CastId`、扣费、启动冷却并发布 `RequestStarted`。目标、效果结果和终态通过统一事件出口发布。
- `USkillLabSkillAbility::ApplyEffect` 是通用效果语义入口。`USkillLabPulseAbility` 只是首阶段载体；新增技能只需增加定义条目和新的载体实现，即可复用请求、生命周期、结果和事件路径。
- `FSkillLabEffectApplicationResult` 携带 `RequestId`、`CastId`、`SourceUnitId`、`TargetUnitId`、来源/目标对象、效果类型、请求数值、实际数值和结果状态。
- `FSkillLabEventData` 携带 `EventId`、阶段、请求/施法身份、`CasterUnitId`、`SourceUnitId`、`TargetUnitId`、来源/目标对象、定义版本/来源和效果结果。效果失败发布 `EffectRejected`，不会只写日志。
- `FSkillLabRequestResult.Status=Completed` 表示生命周期结束，不代表效果成功；调用方同时读取 `bEffectSucceeded` 和 `EffectResult.Status`。
- `Skill_RecoverEnergy` 使用 `GrantedAbilityId=2`、独立 `CastId`、`SkillEnergyAdd` 载体和统一事件链；命令行证据见 `Validation/SkillSystemValidation_20261006.md`，PIE 本轮未验证。
- `USkillLabPresentationRouter` 只订阅事件；`ResetStarted`、`Interrupted`、`ResetCompleted` 和终态事件由路由器自身清理表现状态，运行组件不直接调用清理方法。

## 测试适配

`SkillTest` 和 `SkillTestRecover` 只填写来源、当前 Pawn、当前 `UnitId`、对应 `SkillId` 和自身目标后提交 `FSkillLabSkillRequest`。`SkillReset` 只调用统一 `ResetUnit`。这些入口不读取资源、冷却或效果规则。

PIE 入口：

```text
SkillTest
SkillTestRecover
SkillReset
SkillTest
```

预期成功阶段为 `ActivationChecked → RequestStarted → TargetResolved → EffectApplied → Completed`；快速重复请求应为 `Rejected(Cooldown)`。重置应产生 `ResetStarted`，并在清理后产生 `ResetCompleted`。

## 当前载体与后续替换

当前内容权威来源是独立 C++ 类型化定义提供器，允许首阶段保留样例数值。未来可替换为 DataAsset 或其他结构化载体，必须继续提供版本、来源、目标、资源/冷却、执行模板、效果和表现绑定字段；运行组件、Character、PlayerController 和测试适配不应重新拥有这些内容规则。

## 验证入口

- 编译与地图启动记录：`Validation/BuildAndRuntime.md`。
- 本轮最新 P1 修复证据：`Validation/SkillSystemValidation_20261006.md`。
- 历史审计（重构前快照）：`ImplementationAudit.md`。
