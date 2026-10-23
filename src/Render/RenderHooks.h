#pragma once

#include <cstdint>
#include <functional>
#include <span>

namespace RE
{
	class BSBatchRenderer;
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
	};

	// True on the startup thread before the first render hook fires.
	[[nodiscard]] bool RenderHookRegistrationAllowed(const char* a_where);

	// Register only during Load or OnPostPostLoad.
	bool RegisterPostDeferredPrePass(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPreDeferredPrePass(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	void RegisterPreDeferredLightsImpl(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	void RegisterPostDeferredLightsImpl(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPreDeferredComposite(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	bool RegisterPostDeferredComposite(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	// Right after the main sun's cascades render, before focus shadows reuse them.
	bool RegisterPostSunShadowRender(RenderHookCallback callback, HookPriority priority = HookPriority::Default);
	// After the stock precipitation occlusion pass; it runs in clear weather too.
	bool RegisterPostPrecipitationOcclusion(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

	// Start of Render_PreUI, before the world camera or frame targets are set.
	bool RegisterPreWaterUpdate(RenderHookCallback callback, HookPriority priority = HookPriority::Default);

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

	// Holds matching world-accumulator batch-4 buckets out of DeferredPrePass and hands them to
	// the callback just before the world opaque decals. Nothing is held while the predicate is false.
	bool RegisterWorldPrepassHold(
		PrepassBucketPredicate a_predicate,
		HeldPrepassBucketsCallback a_replay,
		HookPriority a_priority = HookPriority::Default);
	// Draws a held bucket through the engine pass path; only valid inside the replay callback.
	void ReplayHeldPrepassBucket(const HeldPrepassBucket& a_bucket);

	// Install the post-dirty-state deferred draw anchor.
	bool EnsureDeferredDrawAnchorInstalled();
	void RegisterPreFullscreenDeferredLightDraw(
		RenderHookCallback callback,
		HookPriority priority = HookPriority::Default);
}
