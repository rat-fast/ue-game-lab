#pragma once

#include "SkillLabSkillBehavior.h"
#include "SkillLabFireballSkill.generated.h"

class ASkillLabFireballProjectile;

UCLASS()
class SKILLLABSKILLS_API USkillLabFireballBehavior : public USkillLabSkillBehavior
{
	GENERATED_BODY()
public:
	virtual void Execute() override;
	virtual void Cleanup() override;
private:
	void OnProjectileResolved(AActor* Target, FName Reason);
	UPROPERTY(Transient) TObjectPtr<ASkillLabFireballProjectile> Projectile;
};
