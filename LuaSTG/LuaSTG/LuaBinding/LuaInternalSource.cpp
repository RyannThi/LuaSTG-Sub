﻿#include "LuaBinding/LuaInternalSource.hpp"

#pragma region x86 lib
static const std::string _InternalSource_x86 = R"(

package.cpath = ""
package.cpath = package.cpath .. ".\\x86\\?.dll;"
package.cpath = package.cpath .. ".\\?.dll;"

)";
#pragma endregion

#pragma region x64 lib
static const std::string _InternalSource_amd64 = R"(

package.cpath = ""
package.cpath = package.cpath .. ".\\amd64\\?.dll;"
package.cpath = package.cpath .. ".\\?.dll;"

)";
#pragma endregion

#pragma region main
static const std::string _InternalSource_Main = R"(

package.path = ""
package.path = package.path .. "?.lua;"
package.path = package.path .. "?/init.lua;"
package.path = package.path .. "src/?.lua;"

package.path = package.path .. ".\\?.lua;"
package.path = package.path .. ".\\?\\init.lua;"
package.path = package.path .. ".\\src\\?.lua;"

function GameInit()
end
function FrameFunc()
    return false
end
function RenderFunc()
end
function GameExit()
end
function FocusLoseFunc()
end
function FocusGainFunc()
end
function EventFunc(event, ...)
end

)";
#pragma endregion

#pragma region api
static const std::string _InternalSource_API = R"(

local lstg = require("lstg")

function lstg.ShowSplashWindow()
end

function lstg.PostEffectCapture()
end

function lstg.PostEffectApply()
end

function lstg.SystemLog(text)
    lstg.Log(2, text)
end

function lstg.Print(...)
    local args = {...}
    local argc = select('#', ...)
    for i = 1, argc do args[i] = tostring(args[i]) end
    lstg.Log(2, table.concat(args, '\t'))
end

print = lstg.Print

if cjson then
    package.loaded["cjson"] = cjson -- fuck you cjson
end

local rad = math.rad
local deg = math.deg
local sin = math.sin
local cos = math.cos
local tan = math.tan
local asin = math.asin
local acos = math.acos
local atan = math.atan
local atan2 = math.atan2 or math.atan
function lstg.sin(x) return sin(rad(x)) end
function lstg.cos(x) return cos(rad(x)) end
function lstg.tan(x) return tan(rad(x)) end
function lstg.asin(x) return deg(asin(x)) end
function lstg.acos(x) return deg(acos(x)) end
function lstg.atan(...) return deg(atan(...)) end
function lstg.atan2(y, x) return deg(atan2(y, x)) end

local Mesh = require("lstg.Mesh")
function Mesh:createVertexWriter()
	local assert = assert
	local select = select
	local mesh = self
	local vertex_count = mesh:getVertexCount()
	local M = {}
	local p = -1
	function M:seek(vertex_index)
		assert(vertex_index >= 0 and vertex_index < vertex_count, "vertex out of bounds")
		p = vertex_index - 1
		return M
	end
	function M:vertex(...)
		p = p + 1
		assert(p < vertex_count, "vertex out of bounds")
		if select("#", ...) > 0 then
			mesh:setVertex(p, ...)
		end
		return M
	end
	function M:position(...)
		assert(p >= 0, "forget to call the 'vertex' method?")
		mesh:setPosition(p, ...)
		return M
	end
	function M:uv(...)
		assert(p >= 0, "forget to call the 'vertex' method?")
		mesh:setUv(p, ...)
		return M
	end
	function M:color(...)
		assert(p >= 0, "forget to call the 'vertex' method?")
		mesh:setColor(p, ...)
		return M
	end
	function M:commit()
		mesh:commit()
		return M
	end
	return M
end
function Mesh:createIndexWriter()
	local assert = assert
	local ipairs = ipairs
	local mesh = self
	local index_count = mesh:getIndexCount()
	local M = {}
	local p = -1
	function M:seek(index_index)
		assert(index_index >= 0 and index_index < index_count, "index out of bounds")
		p = index_index - 1
		return M
	end
	function M:index(...)
		local args = {...}
		assert(p + #args < index_count, "index out of bounds")
		for _, i in ipairs(args) do
			p = p + 1
			mesh:setIndex(p, i)
		end
		return M
	end
	function M:commit()
		mesh:commit()
		return M
	end
	return M
end

)";
#pragma endregion

namespace LuaSTGPlus
{
	std::string LuaInternalSource_1() {
		if constexpr (sizeof(void*) >= 8)
		{
			return _InternalSource_amd64 + _InternalSource_Main;
		}
		else
		{
			return _InternalSource_x86 + _InternalSource_Main;
		}
	}
	std::string LuaInternalSource_2() {
		return _InternalSource_API;
	}
}
