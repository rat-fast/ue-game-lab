#include "Misc/AutomationTest.h"
#include "SkillLabSpatialTransform.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSpatialTransformTest, "SkillLab.Skills.SpatialTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabSpatialTransformTest::RunTest(const FString& Parameters)
{
	const FTransform Reference(FRotator(0, 90, 0), FVector(100, 200, 300), FVector(2));
	FTransform Result;
	FString Error;
	const FVector Offset(10, 0, 0);
	TestTrue(TEXT("reference offset"), FSkillLabSpatialTransform::Evaluate(Reference, FVector::ForwardVector, Offset,
		ESkillLabSpatialOffsetSpace::Reference, ESkillLabSpatialDirection::World, FRotator::ZeroRotator, Result, Error));
	TestTrue(TEXT("reference rotation applies without inherited scale"), Result.GetLocation().Equals(FVector(100, 210, 300), 0.01f));
	TestTrue(TEXT("world direction independent of position reference"), Result.GetRotation().Equals(FQuat::Identity, 0.001f));
	TestTrue(TEXT("world offset"), FSkillLabSpatialTransform::Evaluate(Reference, FVector::ForwardVector, Offset,
		ESkillLabSpatialOffsetSpace::World, ESkillLabSpatialDirection::Reference, FRotator::ZeroRotator, Result, Error));
	TestTrue(TEXT("world axes stay fixed"), Result.GetLocation().Equals(FVector(110, 200, 300), 0.01f));
	TestTrue(TEXT("execution offset"), FSkillLabSpatialTransform::Evaluate(Reference, FVector(0, -1, 0), Offset,
		ESkillLabSpatialOffsetSpace::ExecutionDirection, ESkillLabSpatialDirection::ExecutionDirection, FRotator::ZeroRotator, Result, Error));
	TestTrue(TEXT("execution axes used"), Result.GetLocation().Equals(FVector(100, 190, 300), 0.01f));
	TestFalse(TEXT("zero execution direction rejected"), FSkillLabSpatialTransform::Evaluate(Reference, FVector::ZeroVector, Offset,
		ESkillLabSpatialOffsetSpace::ExecutionDirection, ESkillLabSpatialDirection::World, FRotator::ZeroRotator, Result, Error));
	TestTrue(TEXT("failure includes reason"), Error.Contains(TEXT("execution direction")));
	return true;
}
#endif
