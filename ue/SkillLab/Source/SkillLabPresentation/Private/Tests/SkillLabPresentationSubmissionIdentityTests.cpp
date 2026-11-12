#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "SkillLabCharacterPresentationLuaComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillAnimationComponent.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabLua.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabPresentationSubmissionIdentityTest, "SkillLab.Presentation.SubmissionIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabPresentationSubmissionIdentityTest::RunTest(const FString&)
{
	// 只验证既有表现适配器的载荷保真；生产事实和真实 Runtime 双通道由 Skills 正式路径测试验证。
	TStrongObjectPtr<USkillLabCharacterPresentationLuaComponent> Host(NewObject<USkillLabCharacterPresentationLuaComponent>());
	Host->Lua = SkillLabLua::Create();
	if (!TestNotNull(TEXT("Presentation adapter has an isolated Lua VM"), Host->Lua)) return false;
	ON_SCOPE_EXIT { SkillLabLua::Close(Host->Lua); Host->Lua = nullptr; };
	SkillLabLua::OpenLibraries(Host->Lua);
	FSkillLabSkillResult Result;
	Result.SubmissionKey.Id = FGuid::NewGuid();
	Result.SubmissionKey.Operation = 1;
	Result.RequestId = 701;
	Result.CastId = 1;
	Result.SkillId = TEXT("AdapterIdentity");
	const FString Script = FString::Printf(TEXT("function OnSkillEvent(r) assert(r.submissionId == '%s'); assert(r.requestId == 701 and r.castId == 1); assert(r.event == 'SubmissionResolved' or r.event == 'SubmissionCommitted'); end"),
		*Result.SubmissionKey.Id.ToString(EGuidFormats::DigitsWithHyphens));
	const FTCHARToUTF8 Utf8(*Script);
	if (!TestTrue(TEXT("Adapter test callback loads"), SkillLabLua::LoadBuffer(Host->Lua, Utf8.Get(), Utf8.Length(), "SubmissionIdentityAdapter")
		&& SkillLabLua::Call(Host->Lua, 0, 0))) return false;
	Result.Event = ESkillLabSkillEvent::SubmissionResolved;
	TestTrue(TEXT("Presentation Lua receives stable submission id and named final event"), Host->CallSkillCallback(Result));
	Result.Event = ESkillLabSkillEvent::SubmissionCommitted;
	TestTrue(TEXT("Presentation Lua preserves the same identity on initial commit"), Host->CallSkillCallback(Result));
	// 适配器级隔离：Lua 仍用旧 int 命令，宿主必须附加本次回调的完整 token。
	TStrongObjectPtr<USkillLabSkillRuntimeComponent> OldRuntime(NewObject<USkillLabSkillRuntimeComponent>());
	TStrongObjectPtr<USkillLabSkillRuntimeComponent> NewRuntime(NewObject<USkillLabSkillRuntimeComponent>());
	TStrongObjectPtr<USkillLabSkillAnimationComponent> Animation(NewObject<USkillLabSkillAnimationComponent>());
	Host->SkillAnimation = Animation.Get();
	Host->RegisterBridgeFunctions();
	FSkillLabCastAnimationPlayback OldPlayback, NewPlayback;
	OldPlayback.Token.Runtime = OldRuntime.Get(); OldPlayback.Token.RuntimeGeneration = 10; OldPlayback.Token.CastId = 1;
	NewPlayback.Token.Runtime = NewRuntime.Get(); NewPlayback.Token.RuntimeGeneration = 11; NewPlayback.Token.CastId = 1;
	Animation->Playbacks = {OldPlayback,NewPlayback};
	const char* StopScript = "function OnSkillEvent(r) SkillLabPresentation.StopSkillAction(r.castId) end";
	if (!TestTrue(TEXT("Lua stop callback loads"), SkillLabLua::LoadBuffer(Host->Lua, StopScript, FCStringAnsi::Strlen(StopScript), "CastTokenStop") && SkillLabLua::Call(Host->Lua,0,0))) return false;
	Result.CastToken = OldPlayback.Token; Result.Event = ESkillLabSkillEvent::Cancelled;
	Host->HandleSkillEvent(Result);
	TestTrue(TEXT("Old same-number Lua cancellation preserves replacement playback token"), Animation->Playbacks.Num() == 1 && Animation->Playbacks[0].Token == NewPlayback.Token);
	TestFalse(TEXT("Callback token is restored after dispatch"),Host->CallbackCastToken.IsValid());
	Result.CastToken = NewPlayback.Token; Result.Event = ESkillLabSkillEvent::Completed;
	Host->HandleSkillEvent(Result);
	TestTrue(TEXT("Matching complete callback removes only its own playback binding"),Animation->Playbacks.IsEmpty());
	return true;
}
#endif
