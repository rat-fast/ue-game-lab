#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabGASTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSubmissionEventTimeoutTest, "SkillLab.Skills.SubmissionEventTimeout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabSubmissionEventTimeoutTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const auto Json = TEXT(R"({"skills":[{"id":"TimeoutProbe","luaScript":"probe.lua","cost":7,"cooldown":4,"magnitude":1,"range":500,"active":0.2,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":0.3,"completionMode":"AllSubmissionPointsResolved","submissionPoints":[{"id":1,"trigger":"Release","target":"Source"},{"id":2,"trigger":"GameplayEvent","eventTag":"SkillLab.Event.Cast.Complete","target":"Source"}]}]})");
 const auto Lua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_Finish() end");
 const TMap<FString, FString> Sources = {{TEXT("probe.lua"), Lua}};
 for (const FString Invalid : {FString(Json).Replace(TEXT(",\"active\":0.2"), TEXT("")), FString(Json).Replace(TEXT("\"active\":0.2"), TEXT("\"active\":0"))})
 {
  TMap<FName, FSkillLabSkillDefinition> Definitions;
  FString Error;
  TestFalse(TEXT("Event submission without an Active timeout is rejected"), FSkillLabSkillDefinitionProvider::ParseCatalogText(Invalid, Sources, Definitions, Error));
  TestTrue(TEXT("Rejected event wait has an actionable diagnostic"), Error.Contains(TEXT("GameplayEvent submission points require a positive active timeout")));
 }
 if (!F.bReady || !F.LoadCatalog(Json, Sources)) return false;
 // 同一合法定义分别覆盖首个释放事件缺失与后续提交事件缺失。
 for (const bool bRelease : {false, true})
 {
  auto U = F.MakeUnit();
  int32 Effects = 0, Commits = 0, Cancels = 0;
  FName CancelReason;
  U.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& R)
  {
   Effects += R.Event == ESkillLabSkillEvent::EffectApplied;
   Commits += R.Event == ESkillLabSkillEvent::SubmissionCommitted;
   if (R.Event == ESkillLabSkillEvent::Cancelled) { ++Cancels; CancelReason = R.Reason; }
  });
  const auto Start = F.Submit(U, TEXT("TimeoutProbe"));
  if (!TestTrue(TEXT("Event timeout probe activates"), Start.bSuccess)) return false;
  const auto Ability = SkillLabObservedAbility(U.Runtime);
  if (bRelease) TestTrue(TEXT("First event releases and submits first point"), F.Notify(U, Start.CastId, SkillLabGASTags::Event_Cast));
  TestEqual(TEXT("Next request buffers during event wait"), F.Submit(U, TEXT("TimeoutProbe")).Reason, FName(TEXT("InputBufferedLatestWins")));
  F.Tick(0.45f);
  TestEqual(TEXT("Missing event cancels exactly once"), Cancels, 1);
  TestEqual(TEXT("Timeout reason identifies the wait"), CancelReason, FName(bRelease ? TEXT("ActiveTimeout") : TEXT("ReleaseEventTimeout")));
  TestTrue(TEXT("Timeout cleans Cast and buffered intent"), U.Runtime->GetExecutionTokens().IsEmpty());
  TestTrue(TEXT("Timeout ends Ability"), !Ability.IsValid() || !Ability->IsActive());
  TestEqual(TEXT("No missing submission is synthesized"), Effects, bRelease ? 1 : 0);
  TestEqual(TEXT("Only the real first point is committed"), Commits, bRelease ? 1 : 0);
  TestEqual(TEXT("Timeout preserves already committed cost only"), U.ASC->GetActualEnergy(), bRelease ? 93.0f : 100.0f);
  TestEqual(TEXT("Cooldown only starts for real submission"), U.Runtime->GetCooldownRemaining(TEXT("TimeoutProbe")) > 0.0f, bRelease);
  TestFalse(TEXT("Late final event is rejected"), F.Notify(U, Start.CastId, SkillLabGASTags::Event_Cast_Complete));
  F.Tick(0.1f);
  TestEqual(TEXT("Late event cannot emit extra effects"), Effects, bRelease ? 1 : 0);
  TestEqual(TEXT("Late event cannot repeat cancellation"), Cancels, 1);
  U.Runtime->OnNativeSkillEvent.Clear();
 }
 return true;
}
#endif
