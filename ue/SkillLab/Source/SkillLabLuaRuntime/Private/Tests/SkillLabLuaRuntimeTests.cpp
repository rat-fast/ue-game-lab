#include "SkillLabLua.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	int32 EchoCheckedInteger(FSkillLabLuaState* State)
	{
		++*static_cast<int32*>(SkillLabLua::GetCallbackContext(State));
		SkillLabLua::PushInteger(State, SkillLabLua::CheckInteger(State, 1));
		return 1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabLuaRuntimeTest, "SkillLab.LuaRuntime.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabLuaRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace SkillLabLua;
	FSkillLabLuaState* Skills = Create();
	FSkillLabLuaState* Presentation = Create();
	if (!Skills || !Presentation)
	{
		if (Skills) Close(Skills);
		if (Presentation) Close(Presentation);
		AddError(TEXT("无法创建独立 Lua VM"));
		return false;
	}
	auto Execute = [](FSkillLabLuaState* State, const ANSICHAR* Text)
	{
		return LoadBuffer(State, Text, FCStringAnsi::Strlen(Text), "boundary-test") && Call(State, 0, 1);
	};
	SetInstructionBudget(Skills, 100000);
	OpenLibraries(Presentation);
	TestTrue(TEXT("Skills 不打开标准库"), Execute(Skills, "return type == nil and math == nil"));
	TestTrue(TEXT("标准库隔离"), ToBoolean(Skills, -1));
	Pop(Skills, 1);
	TestTrue(TEXT("Presentation 保留标准库"), Execute(Presentation, "marker = 42; return math.sqrt(16) == 4"));
	TestTrue(TEXT("标准库可执行"), ToBoolean(Presentation, -1));
	Pop(Presentation, 1);
	TestTrue(TEXT("VM 全局状态隔离"), Execute(Skills, "return marker == nil"));
	TestTrue(TEXT("未共享全局变量"), ToBoolean(Skills, -1));
	Pop(Skills, 1);
	int32 Calls = 0;
	PushCallback(Skills, &EchoCheckedInteger, &Calls);
	SetGlobal(Skills, "Echo");
	TestTrue(TEXT("宿主回调和返回值"), Execute(Skills, "return Echo(17) == 17"));
	TestTrue(TEXT("保留整数值"), ToBoolean(Skills, -1));
	TestEqual(TEXT("回调上下文有效"), Calls, 1);
	Pop(Skills, 1);
	TestFalse(TEXT("参数类型错误由 protected call 捕获"), Execute(Skills, "return Echo({})"));
	TestNotNull(TEXT("类型错误留在栈顶"), ToString(Skills, -1));
	Pop(Skills, 1);
	TestFalse(TEXT("编译错误保持失败"), Execute(Skills, "function ("));
	Pop(Skills, 1);
	TestFalse(TEXT("无限循环受原指令预算限制"), Execute(Skills, "while true do end"));
	const char* BudgetError = ToString(Skills, -1);
	TestTrue(TEXT("预算错误文本保持不变"), BudgetError && FString(UTF8_TO_TCHAR(BudgetError)).Contains(TEXT("instruction budget exceeded")));
	Pop(Skills, 1);
	TestTrue(TEXT("错误后栈可继续使用"), Execute(Skills, "return true"));
	TestTrue(TEXT("错误隔离"), ToBoolean(Skills, -1));
	Close(Skills);
	TestTrue(TEXT("关闭临时 VM 后长生命周期 VM 仍有效"), Execute(Presentation, "return marker == 42"));
	TestTrue(TEXT("长生命周期状态保留"), ToBoolean(Presentation, -1));
	Close(Presentation);
	return true;
}
#endif
