SkillLabSkill_Define("targetShape", "Unit", "relation", "Enemy", "effect", "Damage", "execution", "Projectile", "combatRules", "Standard", "recoveryRules", "StandardRecovery")

function OnSkillCast(context)
    local result = SkillLabSkill_RequestAction()
    if result.completed then
        SkillLabSkill_Finish()
    end
end
