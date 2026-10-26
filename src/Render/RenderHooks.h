#pragma once

#include <cstdint>
#include <functional>
#include <span>

struct ID3D11VertexShader;

namespace RE
{
	class BSBatchRenderer;
	class BSGeometry;
	class BSRenderPass;
}

namespace cs::engine
{
	bool EnsureDrawProfilingInstalled();
	using RenderHookCallback = std::function<void()>;

	// Late priority keeps additive lights after darkening.
	enum class HookPriority : int
	{
		Early = -100,
		Default = 0,
		Late = 100,
		// Brackets that enclose, or sit inside, every feature callback.
		OutermostPre = -1000,
		InnermostPost = -1000,
		InnermostPre = 1000,
		OutermostPost = 1000,
	};

	// True on the startup thread before the first render hook fires.
	[[nodiscard]] bool RenderHookRegistrationAllowed(const char* a_where);

	// Register only during Load or OnPostPostLoad.
	bool RegisterPostDeferredPrePass(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPreDeferredPrePass(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPreDeferredLightsImpl(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPostDeferredLightsImpl(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPreDeferredComposite(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPostDeferredComposite(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	// Right after the main sun's cascades render, before focus shadows reuse them.
	bool RegisterPostSunShadowRender(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	// Before the stock precipitation occlusion pass.
	bool RegisterPrePrecipitationOcclusion(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	// After the stock precipitation occlusion pass; it runs in clear weather too.
	bool RegisterPostPrecipitationOcclusion(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// Start of Render_PreUI, before the world camera or frame targets are set.
	bool RegisterPreWaterUpdate(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// After the update-water callback returns.
	bool RegisterPostWaterUpdate(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// Around each ProcessCommandBuffer replay (one technique bucket).
	bool RegisterPreCommandBufferReplay(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPostCommandBufferReplay(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	enum class ShadowLightKind : std::uint8_t
	{
		kDirectional,
		kFrustum,   // spot
		kParabolic  // omnidirectional
	};
	// Bracket every Render of that light kind, whatever its mask.
	bool RegisterPreShadowLightRender(ShadowLightKind a_kind, RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPostShadowLightRender(ShadowLightKind a_kind, RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// After all sky and cloud draws in the main color target, before water and alpha.
	bool RegisterPostForwardSky(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// After every engine ResetState, including loading-screen and UI callers.
	bool RegisterPostResetState(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// One world batch-4 bucket: every pass in it shares the technique.
	struct HeldPrepassBucket
	{
		RE::BSBatchRenderer* renderer;
		RE::BSRenderPass* head;
		std::uint32_t technique;
		bool alpha;
	};
	using PrepassBucketPredicate = bool (*)(std::uint32_t a_technique) noexcept;
	using HeldPrepassBucketsCallback = std::function<void(std::span<const HeldPrepassBucket>)>;

	// Holds matching world batch-4 buckets; replays them after opaque decals.
	bool RegisterWorldPrepassHold(
		PrepassBucketPredicate a_predicate,
		HeldPrepassBucketsCallback a_replay,
		HookPriority a_priority = HookPriority::Default);
	// Draws a held bucket via the engine; valid only inside the replay callback.
	void ReplayHeldPrepassBucket(const HeldPrepassBucket& a_bucket);

	// Vertex shader for one opaque world Z-prepass draw; null keeps the engine's.
	using ZPrepassVertexShaderSelector = ID3D11VertexShader* (*)(const RE::BSGeometry& a_geometry) noexcept;
	// The first non-null selection binds for that draw only.
	bool RegisterWorldZPrepassVertexShader(
		ZPrepassVertexShaderSelector a_selector,
		HookPriority a_priority = HookPriority::Default);

	// Install the post-dirty-state deferred draw anchor.
	bool EnsureDeferredDrawAnchorInstalled();
	void RegisterPreFullscreenDeferredLightDraw(
		RenderHookCallback callback,
		HookPriority priority = HookPriority::Default);
}
