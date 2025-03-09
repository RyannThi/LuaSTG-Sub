#pragma once
#include "lua.hpp"
#include "Core/Type.hpp"

namespace LuaSTG::Sub::LuaBinding {
	struct Vector2 {
		static std::string_view class_name;

		Core::Vector2<lua_Number> data;

		static bool is(lua_State* vm, int index);
		static Vector2* as(lua_State* vm, int index);
		static Vector2* create(lua_State* vm);
		static void registerClass(lua_State* vm);
	};
}
