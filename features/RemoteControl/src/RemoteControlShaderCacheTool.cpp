#include "RemoteControlTools.h"

#include "Render/ShaderCacheClear.h"

namespace cs::features::remote_control
{
	namespace
	{
		Json Clear(const std::string& a_action, engine::ShaderCacheScope a_scope)
		{
			const auto cleared = engine::ClearShaderCache(a_scope);
			if (!cleared.succeeded)
				return Error("failed to clear the shader cache", { { "action", a_action }, { "detail", cleared.error } });
			return { { "action", a_action }, { "done", true } };
		}

		Json Handle(const Json& a_args)
		{
			const auto action = a_args.value("action", std::string{});
			if (action == "clear")
				return OnFrame([action] { return Clear(action, engine::ShaderCacheScope::kMemory); });
			if (action == "deleteDisk")
				return OnFrame([action] { return Clear(action, engine::ShaderCacheScope::kMemoryAndDisk); });
			return Error("unknown action (clear|deleteDisk)", { { "action", action } });
		}
	}

	const Tool kShaderCacheTool{
		"communityshaders.shadercache",
		R"({"description":"Manage Community Shaders' compiled shader cache; returns {action,done:true} once applied at the next frame. clear: drop the in-memory compiled variants only; with the disk cache present they reload from it rather than recompile, so this does NOT guarantee a recompile. deleteDisk: delete the on-disk cache records AND drop the in-memory variants, forcing a full cold recompile (use for compile benchmarks). Watch progress with communityshaders.inspect kind=shadercache and the communityshaders.shaderRecompiled event.","inputSchema":{"type":"object","properties":{"action":{"type":"string","enum":["clear","deleteDisk"]}},"required":["action"]}})",
		&Handle
	};
}
