# SkillLabGAS 战斗属性与效果

## Module Responsibility

`SkillLabGAS` 为技能运行时提供 ASC、战斗属性、GameplayEffect 承载和伤害/治疗结算。它是生命、能量、护甲、力量与效果结果的代码边界，不负责输入、目标选择或技能槽位。

## Code And Assets

入口为 `USkillLabAbilitySystemComponent`、`USkillLabCombatAttributeSet`、`SkillLabCombatGameplayEffects`、`SkillLabEffectResolver`、`SkillLabGASTags` 和纯函数 `SkillLabDamageCalculation::Resolve`。属性包括 `Health/MaxHealth`、`Energy/MaxEnergy`、`Armor`、`Power` 及 `IncomingDamage/IncomingHealing`。

## Runtime Entry

角色构造 `USkillLabAbilitySystemComponent`；技能运行时初始化 actor info 和属性集。效果执行器调用 ASC 的即时伤害、治疗或持续伤害接口，AttributeSet 在 `PostGameplayEffectExecute` 中处理减免、钳制和死亡 Tag。

## Runtime Flow

即时伤害将原始伤害写入 meta effect，AttributeSet 按 `max(raw - armor, 0)` 计算并把生命限制在 `[0, MaxHealth]`。治疗限制在生命上限。持续伤害创建带周期、时长和按目标聚合的 GameplayEffect；ASC 记录周期结果并广播 `OnPeriodicDamageResolved`。

## Current Responsibilities And Evolution Boundaries

修改属性默认值、钳制和伤害算术优先查看 AttributeSet 与 `SkillLabDamageCalculation`；修改效果承载和周期回调查看 ASC。当前代码设置了 ASC replication，但尚无多人运行验证，因此不能把网络语义写成已完成能力。新增伤害类型、叠层或控制效果需单独定义结算与生命周期测试。

## Verification

`SkillLab.GAS.DamageCalculation` 验证护甲只应用一次、实际伤害和生命下限。历史运行日志还显示技能效果应用和周期事件；本轮未重新运行。

## Current Limitations

当前没有证据支持护盾、免疫、暴击、复杂叠层、多人权威或存档恢复。死亡 Tag 的本地状态变化已由代码表达，但死亡后所有持续效果的战斗规则未冻结。

## Extension Directions

保持 AttributeSet 作为属性钳制和伤害结算入口；新效果应提供独立的结果字段与自动化测试，避免由技能行为重复实现生命/护甲规则。
