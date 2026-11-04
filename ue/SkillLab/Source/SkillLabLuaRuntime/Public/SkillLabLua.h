#pragma once

#include "CoreTypes.h"

// 项目级 opaque VM。Lua 头、原始 C ABI 和回调适配均封装在 Runtime DLL 内。
struct FSkillLabLuaState;

namespace SkillLabLua
{
	using FCallback = int32 (*)(FSkillLabLuaState* State);

	// 每次创建独立 VM；宿主负责配对 Close，不共享技能/表现状态。
	SKILLLABLUARUNTIME_API FSkillLabLuaState* Create();
	SKILLLABLUARUNTIME_API void Close(FSkillLabLuaState* State);
	SKILLLABLUARUNTIME_API void OpenLibraries(FSkillLabLuaState* State);
	SKILLLABLUARUNTIME_API void SetInstructionBudget(FSkillLabLuaState* State, int32 Instructions);
	SKILLLABLUARUNTIME_API bool LoadBuffer(FSkillLabLuaState* State, const char* Text, SIZE_T Length, const char* Name);
	SKILLLABLUARUNTIME_API bool Call(FSkillLabLuaState* State, int32 Arguments, int32 Results);
	SKILLLABLUARUNTIME_API void GetGlobal(FSkillLabLuaState* State, const char* Name);
	SKILLLABLUARUNTIME_API void SetGlobal(FSkillLabLuaState* State, const char* Name);
	SKILLLABLUARUNTIME_API bool IsFunction(FSkillLabLuaState* State, int32 Index);
	SKILLLABLUARUNTIME_API int32 GetTop(FSkillLabLuaState* State);
	SKILLLABLUARUNTIME_API bool IsString(FSkillLabLuaState* State, int32 Index);
	SKILLLABLUARUNTIME_API void GetField(FSkillLabLuaState* State, int32 Index, const char* Key);
	SKILLLABLUARUNTIME_API void Pop(FSkillLabLuaState* State, int32 Count);
	SKILLLABLUARUNTIME_API void NewTable(FSkillLabLuaState* State);
	SKILLLABLUARUNTIME_API void SetField(FSkillLabLuaState* State, int32 Index, const char* Key);
	SKILLLABLUARUNTIME_API void PushBoolean(FSkillLabLuaState* State, bool Value);
	SKILLLABLUARUNTIME_API void PushInteger(FSkillLabLuaState* State, int64 Value);
	SKILLLABLUARUNTIME_API void PushNumber(FSkillLabLuaState* State, double Value);
	SKILLLABLUARUNTIME_API void PushString(FSkillLabLuaState* State, const char* Value);
	SKILLLABLUARUNTIME_API void PushString(FSkillLabLuaState* State, const char* Value, SIZE_T Length);
	// 回调和上下文随 Lua closure 持有；宿主保证 Context 在调用及 Close 期间有效。
	SKILLLABLUARUNTIME_API void PushCallback(FSkillLabLuaState* State, FCallback Callback, void* Context);
	SKILLLABLUARUNTIME_API void* GetCallbackContext(FSkillLabLuaState* State);
	SKILLLABLUARUNTIME_API bool ToBoolean(FSkillLabLuaState* State, int32 Index);
	// 返回值借用 VM 内存；类型错误沿当前受保护调用返回，与 Lua 原检查语义一致。
	// Check* 可能触发 Lua longjmp，回调中先检查参数，再构造需析构的局部对象。
	SKILLLABLUARUNTIME_API int64 CheckInteger(FSkillLabLuaState* State, int32 Index);
	SKILLLABLUARUNTIME_API const char* CheckString(FSkillLabLuaState* State, int32 Index);
	SKILLLABLUARUNTIME_API const char* ToString(FSkillLabLuaState* State, int32 Index);
}
