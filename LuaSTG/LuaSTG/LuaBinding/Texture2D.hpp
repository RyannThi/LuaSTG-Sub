#pragma once
#include "lua.hpp"
#include "Core/Graphics/Device.hpp"

namespace LuaSTG::Sub::LuaBinding {
	struct Texture2D {
		static std::string_view class_name;

		[[maybe_unused]] Core::Graphics::ITexture2D* data{};

		static bool is(lua_State* vm, int index);
		static Texture2D* as(lua_State* vm, int index);
		static Texture2D* create(lua_State* vm);
		static void registerClass(lua_State* vm);
	};
}
