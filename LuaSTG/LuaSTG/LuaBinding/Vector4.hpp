#pragma once
#include "lua.hpp"
#include "Core/Type.hpp"

namespace LuaSTG::Sub::LuaBinding {
	struct Vector4 {
		static std::string_view class_name;

		Core::Vector4<lua_Number> data;

		static bool is(lua_State* vm, int index);
		static Vector4* as(lua_State* vm, int index);
		static Vector4* create(lua_State* vm);
		static void registerClass(lua_State* vm);
	};
}
