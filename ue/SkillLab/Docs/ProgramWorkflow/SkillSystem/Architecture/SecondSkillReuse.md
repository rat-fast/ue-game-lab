# 第二技能复用专题

## 当前结论

`Skill_RecoverEnergy` 是首阶段第二个无资产瞬时技能。它使用自身目标、资源消耗 `5`、冷却 `0.25`、效果类型 `SkillEnergyAdd`、效果强度 `20`，由 `USkillLabRecoverEnergyAbility` 和 `USkillLabRecoverEnergyEffect` 承载。

它复用 `USkillLabSkillComponent::SubmitSkillRequest`、按 SkillId 的授予记录、`GrantedAbilityId`、`CastId`、`FSkillLabRequestResult`、`FSkillLabEffectApplicationResult`、`FSkillLabEventData` 和 `USkillLabPresentationRouter`。运行组件没有新增按技能分支的请求、扣费、冷却、目标、重置或清理生命周期；冷却按 SkillId 保存，允许两个技能独立使用。

## 接入边界

第二技能只增加以下内容：

- Provider 定义条目和默认装配列表成员；
- 新的 Ability/GameplayEffect 载体；
- `SkillTestRecover` 测试适配器，只构造统一请求。

通用组件只读取定义的 `ResourceCost`、`CooldownSeconds`、`TargetType`、`EffectClass` 和 `EffectMagnitude`，不会判断 `SkillId` 来复制生命周期。Ability 载体只负责将定义效果语义映射到对应属性：Pulse 修改 `TestValue`，RecoverEnergy 修改 `SkillEnergy`。

## 证据边界

命令行 `SkillTest,SkillTestRecover,SkillTestRecover,SkillReset,SkillTestRecover,quit` 已验证入口、默认授予、按 SkillId 查找、独立冷却、统一事件和效果结果。当前证据为 `Saved/Logs/SkillSystemGrantedIdentity_20261006.log`，其中包含 `EffectType=SkillEnergyAdd` 和 `AppliedMagnitude=20.00`；`SkillSystemSecondSkill_20261006.log` 仅作为身份上下文修复前的历史证据。本轮不做 PIE 或交互式编辑器验证。
