SkillLabSkill_Define("targetShape", "Ground", "relation", "Enemy", "effect", "Damage", "execution", "GroundArea")

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
