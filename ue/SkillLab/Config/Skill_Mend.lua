SkillLabSkill_Define("targetShape", "Self", "relation", "Self", "effect", "Healing", "execution", "InstantTarget", "combatRules", "Standard", "recoveryRules", "StandardRecovery")
-- TEMP-SPATIAL-VALIDATION：仅供下一次游戏空间验证；不是正式美术参数。
-- 无碰撞测试代理 Cube，非正式 VFX；Source 自身反馈侧移/抬高，治疗仍仅自身。
-- JSON spatial.tempSpatialSelfMarker = {"mesh":"/Engine/BasicShapes/Cube.Cube","offset":[0,-180,140],"worldDirection":[0,45,0],"visualRotation":[20,0,0],"visualScale":1.8}
-- 恢复：删除本注释块/下方 Spatial 声明和本技能 JSON spatial 字段（原来均不存在），然后 SkillReloadDefinitions 或重启。
-- BEGIN TEMP-SPATIAL-VALIDATION
SkillLabSkill_Spatial("tempSpatialSelfMarker", "Marker", "Hit", "Source", "Reference", "World", "Once", "KeepRotation")
-- END TEMP-SPATIAL-VALIDATION

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
