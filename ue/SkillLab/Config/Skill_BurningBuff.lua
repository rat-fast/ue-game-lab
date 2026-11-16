SkillLabSkill_Define("targetShape", "Unit", "relation", "Enemy", "effect", "DamageOverTime", "execution", "InstantTarget", "combatRules", "Standard", "recoveryRules", "StandardRecovery")
-- TEMP-SPATIAL-VALIDATION：仅供下一次游戏空间验证；不是正式美术参数。
-- 无碰撞持续测试代理 Cube，非正式 VFX；跟随 Target，寿命依真实 BuffRemoved，不绑定 Cast。
-- JSON spatial.tempSpatialBuffMarker = {"mesh":"/Engine/BasicShapes/Cube.Cube","offset":[0,160,130],"worldDirection":[0,0,0],"visualRotation":[0,45,0],"visualScale":1.25}
-- 恢复：删除本注释块/下方 Spatial 声明和本技能 JSON spatial 字段（原来均不存在），然后 SkillReloadDefinitions 或重启。
-- BEGIN TEMP-SPATIAL-VALIDATION
SkillLabSkill_Spatial("tempSpatialBuffMarker", "Marker", "BuffApplied", "Target", "Reference", "Reference", "Follow", "FollowRotation")
-- END TEMP-SPATIAL-VALIDATION

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
