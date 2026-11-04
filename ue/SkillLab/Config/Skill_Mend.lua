SkillLabSkill_Define("targetShape", "Self", "relation", "Self", "effect", "Healing", "execution", "InstantTarget")

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
