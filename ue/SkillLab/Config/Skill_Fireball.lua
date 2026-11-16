SkillLabSkill_Define("targetShape", "Unit", "relation", "Enemy", "effect", "Damage", "execution", "Projectile", "combatRules", "Standard", "recoveryRules", "StandardRecovery")
-- TEMP-SPATIAL-VALIDATION：仅供下一次游戏空间验证；不是正式美术参数。
-- 真实投射物；Source Actor 局部偏移，未使用 Socket；保留既有目的点/飞行与碰撞规则。
-- JSON spatial.tempSpatialProjectile = {"offset":[80,120,70],"worldDirection":[0,0,0],"visualRotation":[0,0,0],"visualScale":1.0}
-- 恢复：删除本注释块/下方 Spatial 声明和本技能 JSON spatial 字段（原来均不存在），然后 SkillReloadDefinitions 或重启。
-- BEGIN TEMP-SPATIAL-VALIDATION
SkillLabSkill_Spatial("tempSpatialProjectile", "Projectile", "Release", "Source", "Reference", "ExecutionDirection", "Once", "KeepRotation")
-- END TEMP-SPATIAL-VALIDATION

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
