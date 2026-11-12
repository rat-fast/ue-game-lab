SkillLabSkill_Define("targetShape", "Ground", "relation", "Enemy", "effect", "Damage", "execution", "GroundArea", "combatRules", "Standard", "recoveryRules", "StandardRecovery")

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
