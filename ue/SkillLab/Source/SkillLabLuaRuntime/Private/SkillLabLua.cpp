#include "SkillLabLua.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace
{
	lua_State* Native(FSkillLabLuaState* State) { return reinterpret_cast<lua_State*>(State); }
	FSkillLabLuaState* Opaque(lua_State* State) { return reinterpret_cast<FSkillLabLuaState*>(State); }

	int InvokeHost(lua_State* State)
	{
		const auto Callback = *static_cast<SkillLabLua::FCallback*>(lua_touserdata(State, lua_upvalueindex(1)));
		return Callback(Opaque(State));
	}

	void InstructionHook(lua_State* State, lua_Debug*)
	{
		luaL_error(State, "instruction budget exceeded");
	}
}

namespace SkillLabLua
{
	FSkillLabLuaState* Create() { return Opaque(luaL_newstate()); }
	void Close(FSkillLabLuaState* State) { lua_close(Native(State)); }
	void OpenLibraries(FSkillLabLuaState* State) { luaL_openlibs(Native(State)); }
	void SetInstructionBudget(FSkillLabLuaState* State, int32 Instructions)
	{
		lua_sethook(Native(State), &InstructionHook, LUA_MASKCOUNT, Instructions);
	}
	bool LoadBuffer(FSkillLabLuaState* State, const char* Text, SIZE_T Length, const char* Name)
	{
		return luaL_loadbuffer(Native(State), Text, Length, Name) == LUA_OK;
	}
	bool Call(FSkillLabLuaState* State, int32 Arguments, int32 Results)
	{
		return lua_pcall(Native(State), Arguments, Results, 0) == LUA_OK;
	}
	void GetGlobal(FSkillLabLuaState* State, const char* Name) { lua_getglobal(Native(State), Name); }
	void SetGlobal(FSkillLabLuaState* State, const char* Name) { lua_setglobal(Native(State), Name); }
	bool IsFunction(FSkillLabLuaState* State, int32 Index) { return lua_isfunction(Native(State), Index); }
	void Pop(FSkillLabLuaState* State, int32 Count) { lua_pop(Native(State), Count); }
	void NewTable(FSkillLabLuaState* State) { lua_newtable(Native(State)); }
	void SetField(FSkillLabLuaState* State, int32 Index, const char* Key) { lua_setfield(Native(State), Index, Key); }
	void PushBoolean(FSkillLabLuaState* State, bool Value) { lua_pushboolean(Native(State), Value); }
	void PushInteger(FSkillLabLuaState* State, int64 Value) { lua_pushinteger(Native(State), Value); }
	void PushNumber(FSkillLabLuaState* State, double Value) { lua_pushnumber(Native(State), Value); }
	void PushString(FSkillLabLuaState* State, const char* Value) { lua_pushstring(Native(State), Value); }
	void PushString(FSkillLabLuaState* State, const char* Value, SIZE_T Length) { lua_pushlstring(Native(State), Value, Length); }
	void PushCallback(FSkillLabLuaState* State, FCallback Callback, void* Context)
	{
		lua_State* Lua = Native(State);
		// 函数指针存于 Lua 自身持有的 userdata，避免函数指针和 void* 的不安全互转。
		auto* Storage = static_cast<FCallback*>(lua_newuserdatauv(Lua, sizeof(FCallback), 0));
		*Storage = Callback;
		lua_pushlightuserdata(Lua, Context);
		lua_pushcclosure(Lua, &InvokeHost, 2);
	}
	void* GetCallbackContext(FSkillLabLuaState* State) { return lua_touserdata(Native(State), lua_upvalueindex(2)); }
	bool ToBoolean(FSkillLabLuaState* State, int32 Index) { return lua_toboolean(Native(State), Index) != 0; }
	int64 CheckInteger(FSkillLabLuaState* State, int32 Index) { return luaL_checkinteger(Native(State), Index); }
	const char* CheckString(FSkillLabLuaState* State, int32 Index) { return luaL_checkstring(Native(State), Index); }
	const char* ToString(FSkillLabLuaState* State, int32 Index) { return lua_tostring(Native(State), Index); }
}
