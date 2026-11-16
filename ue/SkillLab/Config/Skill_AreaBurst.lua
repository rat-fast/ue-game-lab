SkillLabSkill_Define("targetShape", "Ground", "relation", "Enemy", "effect", "Damage", "execution", "GroundArea", "combatRules", "Standard", "recoveryRules", "StandardRecovery")
-- TEMP-SPATIAL-VALIDATION：仅供下一次游戏空间验证；不是正式美术参数。
-- 无碰撞测试代理 Plane，非正式 VFX；约1.4m方形并偏离4.5m，不表示 radius=350 的伤害边界。
-- JSON spatial.tempSpatialGroundMarker = {"mesh":"/Engine/BasicShapes/Plane.Plane","offset":[450,0,10],"worldDirection":[0,35,0],"visualRotation":[0,0,0],"visualScale":0.2}
-- 恢复：删除本注释块/下方 Spatial 声明和本技能 JSON spatial 字段（原来均不存在），然后 SkillReloadDefinitions 或重启。
-- BEGIN TEMP-SPATIAL-VALIDATION
SkillLabSkill_Spatial("tempSpatialGroundMarker", "Marker", "TargetSelected", "Ground", "World", "World", "Once", "KeepRotation")
-- END TEMP-SPATIAL-VALIDATION

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
