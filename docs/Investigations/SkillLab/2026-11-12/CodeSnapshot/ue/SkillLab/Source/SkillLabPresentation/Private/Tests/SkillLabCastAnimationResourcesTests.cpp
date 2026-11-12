#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "SkillLabSkillAnimNotify.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastAnimationResourcesTest,"SkillLab.Presentation.CastAnimationResources",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastAnimationResourcesTest::RunTest(const FString&)
{
 for (const auto* Name : {TEXT("Fireball"),TEXT("AreaBurst"),TEXT("Mend"),TEXT("BurningBuff")})
 {
  const FString Path = FString::Printf(TEXT("/Game/SkillLab/Animations/AM_Skill_%s.AM_Skill_%s"),Name,Name);
  const auto* Montage = LoadObject<UAnimMontage>(nullptr,*Path);
  if (!TestNotNull(*Path,Montage)) continue;
  int32 ReleaseCount = 0;
  for (const auto& Notify : Montage->Notifies)
   if (const auto* CastNotify = Cast<USkillLabSkillAnimNotify>(Notify.Notify); CastNotify && CastNotify->EventTag == FGameplayTag::RequestGameplayTag(TEXT("SkillLab.Event.Cast")))
   { ++ReleaseCount; AddInfo(FString::Printf(TEXT("%s Release=%0.6f"),Name,Notify.GetTime())); }
  TestEqual(TEXT("Production Montage has one authoritative release Notify"),ReleaseCount,1);
  FString Slots;
  for (const auto& Track : Montage->SlotAnimTracks) Slots += Track.SlotName.ToString() + TEXT(" ");
  AddInfo(FString::Printf(TEXT("%s Group=%s Slots=%s Length=%0.6f (read only)"),Name,*Montage->GetGroupName().ToString(),*Slots,Montage->GetPlayLength()));
 }
 return true;
}
#endif
