#include "Render/RenderHooks.h"

#include "Log.h"
#include "Render/DeferredDrawAnchor.h"
#include "Render/Engine.h"
#include "Render/EngineCallSite.h"
#include "Render/FrameProfiler.h"
#include "Render/HookRegistry.h"
#include "Render/ShaderInjection.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	auto* L = cs::log::Get("cs.hooks");
}

namespace cs::engine
{
	namespace
	{
		struct PrioritizedCallback
		{
			HookPriority priority;
			RenderHookCallback cb;
		};

		std::vector<PrioritizedCallback> g_postDeferredPrePass;
		std::vector<PrioritizedCallback> g_preDeferredPrePass;
		std::vector<PrioritizedCallback> g_preDeferredLightsImpl;
		std::vector<PrioritizedCallback> g_postDeferredLightsImpl;
		std::vector<PrioritizedCallback> g_preDeferredComposite;
		std::vector<PrioritizedCallback> g_postDeferredComposite;
		std::vector<PrioritizedCallback> g_preFullscreenDeferredLightDraw;
		std::vector<PrioritizedCallback> g_postForwardSky;
		std::vector<PrioritizedCallback> g_postPrecipitationOcclusion;
		std::vector<PrioritizedCallback> g_postSunShadowRender;
		std::vector<PrioritizedCallback> g_preWaterUpdate;
		std::vector<PrioritizedCallback> g_postResetState;
		struct PrepassHold
		{
			HookPriority priority;
			PrepassBucketPredicate predicate;
			HeldPrepassBucketsCallback replay;
			std::vector<HeldPrepassBucket> held;
		};

		std::vector<PrepassHold> g_prepassHolds;
		bool g_prePassInstalled = false;
		bool g_prepassHoldInstalled = false;
		bool g_insideWorldPrepassBatch = false;
		bool g_lightsImplInstalled = false;
		bool g_compositeInstalled = false;
		bool g_deferredDrawAnchorInstalled = false;
		bool g_drawProfilingInstalled = false;
		bool g_drawFlushInstalled = false;
		bool g_forwardSkyInstalled = false;
		bool g_precipitationOcclusionInstalled = false;
		bool g_sunShadowRenderInstalled = false;
		bool g_waterUpdateInstalled = false;
		bool g_resetStateInstalled = false;
		bool g_insideDeferredLightsImpl = false;
		bool g_insideDeferredComposite = false;

		// Registration closes when the first render hook runs.
		const DWORD g_registrationThreadId = ::GetCurrentThreadId();
		std::atomic_bool g_registrationClosed{ false };

		void MarkRegistrationClosed() noexcept
		{
			g_registrationClosed.store(true, std::memory_order_relaxed);
		}

		bool RegistrationAllowed(const char* a_where)
		{
			const bool onStartupThread = (::GetCurrentThreadId() == g_registrationThreadId);
			const bool stillOpen = !g_registrationClosed.load(std::memory_order_relaxed);
			assert(onStartupThread && "RenderHooks registration must run on the startup thread");
			assert(stillOpen && "RenderHooks registration must finish before render hooks fire");
			if (!onStartupThread || !stillOpen) {
				L->error("Rejected RenderHooks registration for {} (late or off-thread)", a_where);
				return false;
			}
			return true;
		}

		void InsertPrioritized(std::vector<PrioritizedCallback>& v, RenderHookCallback&& cb, HookPriority p)
		{
			InsertByPriority(v, PrioritizedCallback{ p, std::move(cb) });
		}

		void Dispatch(const std::vector<PrioritizedCallback>& v)
		{
			for (auto& entry : v) {
				entry.cb();
			}
		}

		class ScopedRenderPhase
		{
		public:
			explicit ScopedRenderPhase(bool& a_phase) noexcept :
				_phase(a_phase),
				_previous(std::exchange(a_phase, true))
			{}

			~ScopedRenderPhase()
			{
				_phase = _previous;
			}

			ScopedRenderPhase(const ScopedRenderPhase&) = delete;
			ScopedRenderPhase& operator=(const ScopedRenderPhase&) = delete;

		private:
			bool& _phase;
			bool _previous;
		};

		// Callbacks run inside engine frames, so failures are logged, not unwound.
		void DispatchGuarded(const std::vector<PrioritizedCallback>& v, const char* a_where) noexcept
		{
			for (auto& entry : v) {
				try {
					entry.cb();
				} catch (const std::exception& e) {
					L->error("{} callback failed: {}", a_where, e.what());
				} catch (...) {
					L->error("{} callback failed.", a_where);
				}
			}
		}

		class PostDispatchScope
		{
		public:
			explicit PostDispatchScope(
				const std::vector<PrioritizedCallback>& a_callbacks) noexcept :
				_callbacks(a_callbacks)
			{}
			~PostDispatchScope() noexcept
			{
				DispatchGuarded(_callbacks, "Deferred-lights post");
			}

			PostDispatchScope(const PostDispatchScope&) = delete;
			PostDispatchScope& operator=(const PostDispatchScope&) = delete;

		private:
			const std::vector<PrioritizedCallback>& _callbacks;
		};

		// Passes are disjoint, so nested callbacks pause this segment.
		class ProfiledSegment
		{
		public:
			explicit ProfiledSegment(std::string_view a_name) noexcept :
				_name(a_name)
			{}

			void Begin() noexcept { _active = render::profiling::BeginPass(_name); }
			void End() noexcept
			{
				if (std::exchange(_active, false))
					render::profiling::EndPass();
			}

		private:
			std::string_view _name;
			bool _active = false;
		};

		ProfiledSegment g_deferredPrePassTiming{ "DeferredPrePass" };

		struct DeferredPrePass_Hook
		{
			static void thunk()
			{
				MarkRegistrationClosed();
				Dispatch(g_preDeferredPrePass);
				g_deferredPrePassTiming.Begin();
				func();
				g_deferredPrePassTiming.End();
				Dispatch(g_postDeferredPrePass);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DeferredLightsImpl_Hook
		{
			static void thunk()
			{
				MarkRegistrationClosed();
				const PostDispatchScope post(g_postDeferredLightsImpl);
				Dispatch(g_preDeferredLightsImpl);
				{
					ScopedRenderPhase phase(g_insideDeferredLightsImpl);
					func();
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Routes flush once, then draw; command-buffer records bind their own state.
		enum class DrawRoute : std::uint8_t
		{
			kNone,
			kTriShape,
			kTriShapeIndexed,
			kTriShapeLargeIndices,
			kInstancedTriShape,
			kDynamicTriShape,
			kParticleShaderTriShape,
			kCommandBuffer,
			kCommandBufferRecord
		};

		thread_local DrawRoute t_drawRoute = DrawRoute::kNone;

		// Injection binds made inside the scope are restored when its draw returns.
		class DrawRouteScope
		{
		public:
			explicit DrawRouteScope(DrawRoute a_route) noexcept :
				_previous(std::exchange(t_drawRoute, a_route))
			{
				if (const auto* state = GetGraphicsState())
					BeginShaderInjectionFrame(state->frameCount);
			}
			~DrawRouteScope() { t_drawRoute = _previous; }

			DrawRouteScope(const DrawRouteScope&) = delete;
			DrawRouteScope& operator=(const DrawRouteScope&) = delete;

		private:
			DrawRoute _previous;
			ScopedPixelShaderInjectionBindings _bindings;
		};

		template <DrawRoute Route, auto Draw, class = decltype(Draw)>
		struct DrawRouteScope_Hook;

		template <DrawRoute Route, auto Draw, class... Args>
		struct DrawRouteScope_Hook<Route, Draw, void (RE::BSGraphics::Renderer::*)(Args...)>
		{
			static void thunk(RE::BSGraphics::Renderer* a_this, Args... a_args)
			{
				const DrawRouteScope scope(Route);
				func(a_this, a_args...);
			}
			static inline REL::Relocation<void(RE::BSGraphics::Renderer*, Args...)> func;
			static inline bool installed = false;
		};

		// Replaces a record's context draw; its bindings are final at this point.
		template <auto Draw, class... Args>
		void STDMETHODCALLTYPE CommandBufferRecordDraw(ID3D11DeviceContext* a_context, Args... a_args)
		{
			const DrawRouteScope scope(DrawRoute::kCommandBufferRecord);
			if (g_deferredDrawAnchorInstalled)
				DispatchInjectionsForBoundPixelShader(a_context);
			(a_context->*Draw)(a_args...);
		}

		void OnDrawStateFlushed(bool a_outputStateOnly, bool a_compute, std::uint32_t a_residualR9d) noexcept
		{
			auto* rendererData = RE::BSGraphics::GetRendererData();
			auto* context = rendererData ? reinterpret_cast<ID3D11DeviceContext*>(rendererData->context) : nullptr;
			if (t_drawRoute == DrawRoute::kNone) {
				// Flush is output-only and RunComputeShader is compute; draws are neither.
				if (!a_outputStateOnly && !a_compute)
					RecordUndispatchedInjectedDraw(context);
				return;
			}
			if (t_drawRoute == DrawRoute::kCommandBuffer)
				return;
			const auto decision = SelectDeferredDrawAnchorDecision(
				g_insideDeferredLightsImpl,
				g_insideDeferredComposite,
				t_drawRoute == DrawRoute::kTriShape,
				a_residualR9d);
			if (decision.dispatchFullscreenLightCallbacks)
				Dispatch(g_preFullscreenDeferredLightDraw);
			if (decision.dispatchInjections)
				DispatchInjectionsForBoundPixelShader(context);
		}

		// The fourth argument is residual r9d state, not a call parameter.
		struct SetDirtyStates_Hook
		{
			static void thunk(
				bool a_outputStateOnly,
				bool a_compute,
				std::uint32_t,
				std::uint32_t a_residualR9d)
			{
				func(a_outputStateOnly, a_compute);
				if (g_drawProfilingInstalled)
					render::profiling::RecordDraw();
				if (g_deferredDrawAnchorInstalled)
					OnDrawStateFlushed(a_outputStateOnly, a_compute, a_residualR9d);
			}
			static inline REL::Relocation<decltype(&RE::BSGraphics::SetDirtyStates)> func;
		};

		struct ResetState_Hook
		{
			static void thunk(RE::BSGraphics::Renderer* a_this)
			{
				func(a_this);
				DispatchGuarded(g_postResetState, "Post reset state");
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct DeferredComposite_Hook
		{
			static void thunk()
			{
				MarkRegistrationClosed();
				Dispatch(g_preDeferredComposite);
				{
					ScopedRenderPhase phase(g_insideDeferredComposite);
					func();
				}
				Dispatch(g_postDeferredComposite);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct PrecipitationOcclusion_Hook
		{
			static void thunk()
			{
				MarkRegistrationClosed();
				func();
				DispatchGuarded(g_postPrecipitationOcclusion, "Precipitation occlusion");
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		[[nodiscard]] const RE::BSShadowDirectionalLight* GetSunShadowLight() noexcept
		{
			const auto* scene = GetWorldShadowSceneNode();
			return scene ? scene->directionalShadowLight : nullptr;
		}

		// The sun's Render(7) is the only directional render in DeferredLightsImpl.
		struct SunShadowRender_Hook
		{
			static void thunk(RE::BSShadowDirectionalLight* a_this, std::uint32_t a_mask)
			{
				func(a_this, a_mask);
				constexpr std::uint32_t kMainSunMask = 7;
				if (a_mask == kMainSunMask && g_insideDeferredLightsImpl && a_this == GetSunShadowLight())
					DispatchGuarded(g_postSunShadowRender, "Post sun shadow render");
			}
			static inline REL::Relocation<void(RE::BSShadowDirectionalLight*, std::uint32_t)> func;
		};

		// DrawWorld::Forward renders sky batch 7, then cloud group 14; water and alpha follow.
		struct ForwardSkyGroup_Hook
		{
			static void thunk(
				RE::BSShaderAccumulator* a_accumulator,
				std::uint32_t a_group,
				bool a_alpha)
			{
				func(a_accumulator, a_group, a_alpha);
				Dispatch(g_postForwardSky);
			}
			static inline REL::Relocation<void(RE::BSShaderAccumulator*, std::uint32_t, bool)> func;
		};

		// Render_PreUI calls the water callback before world setup.
		struct UpdateWaterFunc_Hook
		{
			using Callback = RE::DrawWorld::UpdateWaterFunc;

			static void Invoke(RE::BSGeometryListCullingProcess* a_cullingProcess)
			{
				MarkRegistrationClosed();
				DispatchGuarded(g_preWaterUpdate, "Pre water update");
				original.load(std::memory_order_acquire)(a_cullingProcess);
			}

			static void thunk(Callback a_callback)
			{
				original.store(a_callback, std::memory_order_release);
				func(a_callback ? &Invoke : nullptr);
			}

			static inline REL::Relocation<decltype(thunk)> func;
			static inline std::atomic<Callback> original{ nullptr };
		};

		// World accumulator batch 4; the first-person call is left alone.
		struct WorldPrepassBatches_Hook
		{
			static void thunk(RE::BSShaderAccumulator* a_accumulator, std::int32_t a_shaderType, bool a_alpha, std::int32_t a_group)
			{
				for (auto& hold : g_prepassHolds)
					hold.held.clear();
				ScopedRenderPhase phase(g_insideWorldPrepassBatch);
				func(a_accumulator, a_shaderType, a_alpha, a_group);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Sole RenderPassImpl caller: every command-buffer-less bucket.
		struct PrepassBucket_Hook
		{
			static bool thunk(RE::BSBatchRenderer* a_renderer, RE::BSRenderPass* a_head, std::uint32_t a_technique, bool a_alpha)
			{
				if (g_insideWorldPrepassBatch) {
					for (auto& hold : g_prepassHolds) {
						if (hold.predicate(a_technique)) {
							hold.held.push_back({ a_renderer, a_head, a_technique, a_alpha });
							return false;
						}
					}
				}
				return func(a_renderer, a_head, a_technique, a_alpha);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct WorldOpaqueDecals_Hook
		{
			static void thunk(RE::BSShaderAccumulator* a_accumulator)
			{
				// Decal-group G-buffer must exist first; its Z-prepass depth is already set.
				func(a_accumulator);
				for (auto& hold : g_prepassHolds) {
					if (hold.held.empty())
						continue;
					g_deferredPrePassTiming.End();
					try {
						hold.replay(hold.held);
					} catch (const std::exception& e) {
						L->error("Prepass hold replay failed: {}", e.what());
					} catch (...) {
						L->error("Prepass hold replay failed.");
					}
					hold.held.clear();
					g_deferredPrePassTiming.Begin();
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		template <class Hook>
		bool EnsureDetourInstalled(bool& a_installed, REL::VariantID a_id, const char* a_name)
		{
			if (a_installed) {
				return true;
			}
			stl::detour_thunk<Hook>(a_id);
			if (Hook::func.address() == 0) {
				L->error("Hook failed on {}", a_name);
				return false;
			}
			a_installed = true;
			L->info("Hook installed on {}", a_name);
			return true;
		}

		bool EnsureWaterUpdateInstalled()
		{
			return EnsureDetourInstalled<UpdateWaterFunc_Hook>(
				g_waterUpdateInstalled, RE::ID::DrawWorld::SetUpdateWaterFunc, "DrawWorld::SetUpdateWaterFunc");
		}

		bool EnsureResetStateInstalled()
		{
			return EnsureDetourInstalled<ResetState_Hook>(
				g_resetStateInstalled, RE::ID::BSGraphics::Renderer::ResetState, "BSGraphics::Renderer::ResetState");
		}

		void EnsureForwardSkyInstalled()
		{
			if (g_forwardSkyInstalled) {
				return;
			}
			const auto runtimeIdx = static_cast<std::uint8_t>(REX::FModule::GetRuntimeIndex());
			constexpr std::ptrdiff_t offsets[] = { 0x2C9, 0x2D5, 0x2D5 };
			stl::write_thunk_call<ForwardSkyGroup_Hook>(
				RE::ID::DrawWorld::Forward.address() + offsets[runtimeIdx]);
			g_forwardSkyInstalled = true;
			L->info("Hook installed on DrawWorld::Forward cloud group call (post-sky boundary)");
		}

		bool EnsurePrecipitationOcclusionInstalled()
		{
			return EnsureDetourInstalled<PrecipitationOcclusion_Hook>(
				g_precipitationOcclusionInstalled, RE::ID::Precipitation::RenderOcclusionMap, "Precipitation::RenderOcclusionMap");
		}

		void EnsureDeferredLightsImplInstalled()
		{
			if (g_lightsImplInstalled) {
				return;
			}
			stl::detour_thunk<DeferredLightsImpl_Hook>(RE::ID::DrawWorld::DeferredLightsImpl);
			g_lightsImplInstalled = true;
			L->info("Hook installed on DrawWorld::DeferredLightsImpl");
		}

		bool EnsureSunShadowRenderInstalled()
		{
			if (g_sunShadowRenderInstalled) {
				return true;
			}
			constexpr std::size_t kRenderVtableSlot = 10;
			REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::BSShadowDirectionalLight[0] };
			const auto slot = vtable.address() + kRenderVtableSlot * sizeof(std::uintptr_t);
			const auto expected = RE::ID::BSShadowDirectionalLight::Render.address();
			if (*reinterpret_cast<const std::uintptr_t*>(slot) != expected) {
				L->error("BSShadowDirectionalLight vtable slot {} is not Render ({:#x}); sun shadow hook unavailable", kRenderVtableSlot, expected);
				return false;
			}
			stl::write_vfunc<kRenderVtableSlot, SunShadowRender_Hook>(RE::VTABLE::BSShadowDirectionalLight[0]);
			g_sunShadowRenderInstalled = SunShadowRender_Hook::func.address() != 0;
			if (!g_sunShadowRenderInstalled) {
				L->error("Hook failed on BSShadowDirectionalLight::Render");
				return false;
			}
			// The inside-DeferredLightsImpl flag only flips while that hook is installed.
			EnsureDeferredLightsImplInstalled();
			L->info("Hook installed on BSShadowDirectionalLight::Render (vtable slot {})", kRenderVtableSlot);
			return true;
		}

		void EnsureDeferredPrePassInstalled()
		{
			if (g_prePassInstalled) {
				return;
			}
			stl::detour_thunk<DeferredPrePass_Hook>(RE::ID::DrawWorld::DeferredPrePass);
			g_prePassInstalled = true;
			L->info("Hook installed on DrawWorld::DeferredPrePass");
		}

		void EnsureDeferredCompositeInstalled()
		{
			if (g_compositeInstalled) {
				return;
			}
			stl::detour_thunk<DeferredComposite_Hook>(RE::ID::DrawWorld::DeferredComposite);
			g_compositeInstalled = true;
			L->info("Hook installed on DrawWorld::DeferredComposite");
		}

		bool EnsurePrepassHoldInstalled()
		{
			if (g_prepassHoldInstalled)
				return true;
			constexpr CallSiteAnchor kWorldBatches{
				.name = "DeferredPrePass -> world BSShaderAccumulator::RenderBatches",
				.function = RE::ID::DrawWorld::DeferredPrePass,
				.offset = { 0x36A, 0x371, 0x371 },
				.target = RE::ID::BSShaderAccumulator::RenderBatches
			};
			constexpr CallSiteAnchor kWorldOpaqueDecals{
				.name = "DeferredPrePass -> world BSShaderAccumulator::RenderOpaqueDecals",
				.function = RE::ID::DrawWorld::DeferredPrePass,
				.offset = { 0x384, 0x38B, 0x38B },
				.target = RE::ID::BSShaderAccumulator::RenderOpaqueDecals
			};
			constexpr CallSiteAnchor kBucket{
				.name = "BSBatchRenderer::RenderBatches -> RenderPassImpl",
				.function = RE::ID::BSBatchRenderer::RenderBatches,
				.offset = { 0xD0, 0x257, 0x257 },
				.target = RE::ID::BSBatchRenderer::RenderPassImpl
			};
			const auto batches = ResolveCallSite(kWorldBatches);
			const auto decals = ResolveCallSite(kWorldOpaqueDecals);
			const auto bucket = ResolveCallSite(kBucket);
			for (const auto* site : { &batches, &decals, &bucket }) {
				if (!*site) {
					L->error("Prepass hold unavailable: {}", site->error());
					return false;
				}
			}
			stl::write_thunk_call<WorldPrepassBatches_Hook>(*batches);
			stl::write_thunk_call<WorldOpaqueDecals_Hook>(*decals);
			stl::write_thunk_call<PrepassBucket_Hook>(*bucket);
			EnsureDeferredPrePassInstalled();
			g_prepassHoldInstalled = true;
			L->info("Hooks installed on world batch-4 render, bucket render and opaque decals (prepass hold)");
			return true;
		}

		// FO4: SetDirtyStates is the native draw submission boundary on OG/NG/AE.
		bool EnsureDrawFlushInstalled()
		{
			return EnsureDetourInstalled<SetDirtyStates_Hook>(
				g_drawFlushInstalled, RE::ID::BSGraphics::SetDirtyStates, "BSGraphics::SetDirtyStates");
		}

		bool InstallCommandBufferRecordDraws()
		{
			static bool installed = false;
			if (installed)
				return true;
			constexpr ContextCallSiteAnchor kDrawIndexed{
				.name = "ProcessCommandBuffer -> ID3D11DeviceContext::DrawIndexed",
				.function = RE::ID::BSGraphics::Renderer::ProcessCommandBuffer,
				.offset = { 0x919, 0x8F3, 0x8F3 },
				.target = { 0x60 }
			};
			constexpr ContextCallSiteAnchor kDraw{
				.name = "ProcessCommandBuffer -> ID3D11DeviceContext::Draw",
				.function = RE::ID::BSGraphics::Renderer::ProcessCommandBuffer,
				.offset = { 0x96F, 0x943, 0x943 },
				.target = { 0x68 }
			};
			// Prove both sites before patching either.
			const auto drawIndexed = ResolveCallSite(kDrawIndexed);
			const auto draw = ResolveCallSite(kDraw);
			for (const auto* site : { &drawIndexed, &draw }) {
				if (!*site) {
					L->error("Command-buffer record draws unavailable: {}", site->error());
					return false;
				}
			}
			using Context = ID3D11DeviceContext;
			for (const auto& [site, thunk] : { std::pair{ *drawIndexed, reinterpret_cast<std::uintptr_t>(&CommandBufferRecordDraw<&Context::DrawIndexed, UINT, UINT, INT>) },
					 std::pair{ *draw, reinterpret_cast<std::uintptr_t>(&CommandBufferRecordDraw<&Context::Draw, UINT, UINT>) } }) {
				if (const auto redirected = RedirectContextCall(site, thunk); !redirected) {
					L->error("Command-buffer record draws unavailable: {}", redirected.error());
					return false;
				}
			}
			installed = true;
			return true;
		}

		template <DrawRoute Route, auto Draw>
		bool InstallDrawRoute(REL::VariantID a_id, const char* a_name)
		{
			using Hook = DrawRouteScope_Hook<Route, Draw>;
			return EnsureDetourInstalled<Hook>(Hook::installed, a_id, a_name);
		}

		bool InstallDrawRoutes()
		{
			using Renderer = RE::BSGraphics::Renderer;
			namespace ID = RE::ID::BSGraphics::Renderer;
			return InstallDrawRoute<DrawRoute::kTriShape, &Renderer::DrawTriShape>(
					   ID::DrawTriShape, "DrawTriShape") &&
			       InstallDrawRoute<DrawRoute::kTriShapeIndexed, &Renderer::DrawTriShapeIndexed>(
					   ID::DrawTriShapeIndexed, "DrawTriShapeIndexed") &&
			       InstallDrawRoute<DrawRoute::kTriShapeLargeIndices, &Renderer::DrawTriShapeLargeIndices>(
					   ID::DrawTriShapeLargeIndices, "DrawTriShapeLargeIndices") &&
			       InstallDrawRoute<DrawRoute::kInstancedTriShape, &Renderer::DrawInstancedTriShape>(
					   ID::DrawInstancedTriShape, "DrawInstancedTriShape") &&
			       InstallDrawRoute<DrawRoute::kDynamicTriShape, &Renderer::DrawDynamicTriShape>(
					   ID::DrawDynamicTriShape, "DrawDynamicTriShape") &&
			       InstallDrawRoute<DrawRoute::kParticleShaderTriShape, &Renderer::DrawParticleShaderTriShape>(
					   ID::DrawParticleShaderTriShape, "DrawParticleShaderTriShape") &&
			       InstallDrawRoute<DrawRoute::kCommandBuffer, &Renderer::ProcessCommandBuffer>(
					   ID::ProcessCommandBuffer, "ProcessCommandBuffer") &&
			       InstallCommandBufferRecordDraws();
		}

		void InstallDeferredDrawAnchor()
		{
			EnsureDeferredLightsImplInstalled();
			EnsureDeferredCompositeInstalled();
			if (g_deferredDrawAnchorInstalled) {
				return;
			}
			if (!InstallDrawRoutes() || !EnsureDrawFlushInstalled()) {
				L->error("Deferred draw anchor unavailable");
				return;
			}
			g_deferredDrawAnchorInstalled = true;
			L->info("Hooks installed on engine draw routes and SetDirtyStates (deferred draw anchor)");
		}
	}

	bool EnsureDeferredDrawAnchorInstalled()
	{
		if (!RegistrationAllowed("DeferredDrawAnchor"))
			return false;
		InstallDeferredDrawAnchor();
		return g_deferredDrawAnchorInstalled;
	}

	bool EnsureDrawProfilingInstalled()
	{
		if (g_drawProfilingInstalled)
			return true;
		if (!RegistrationAllowed("DrawProfiling") || !EnsureDrawFlushInstalled())
			return false;
		g_drawProfilingInstalled = true;
		return true;
	}

	bool RenderHookRegistrationAllowed(const char* a_where)
	{
		return RegistrationAllowed(a_where);
	}

	bool RegisterPostDeferredPrePass(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostDeferredPrePass"))
			return false;
		InsertPrioritized(g_postDeferredPrePass, std::move(callback), priority);
		EnsureDeferredPrePassInstalled();
		return true;
	}

	bool RegisterPreDeferredPrePass(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreDeferredPrePass"))
			return false;
		InsertPrioritized(g_preDeferredPrePass, std::move(callback), priority);
		EnsureDeferredPrePassInstalled();
		return true;
	}

	void RegisterPreDeferredLightsImpl(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreDeferredLightsImpl"))
			return;
		InsertPrioritized(g_preDeferredLightsImpl, std::move(callback), priority);
		EnsureDeferredLightsImplInstalled();
	}

	void RegisterPostDeferredLightsImpl(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostDeferredLightsImpl"))
			return;
		InsertPrioritized(g_postDeferredLightsImpl, std::move(callback), priority);
		EnsureDeferredLightsImplInstalled();
	}

	bool RegisterPreDeferredComposite(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreDeferredComposite"))
			return false;
		InsertPrioritized(g_preDeferredComposite, std::move(callback), priority);
		EnsureDeferredCompositeInstalled();
		return true;
	}

	bool RegisterPostDeferredComposite(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostDeferredComposite"))
			return false;
		InsertPrioritized(g_postDeferredComposite, std::move(callback), priority);
		EnsureDeferredCompositeInstalled();
		return true;
	}

	bool RegisterPreWaterUpdate(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreWaterUpdate"))
			return false;
		InsertPrioritized(g_preWaterUpdate, std::move(callback), priority);
		return EnsureWaterUpdateInstalled();
	}

	bool RegisterWorldPrepassHold(
		PrepassBucketPredicate a_predicate,
		HeldPrepassBucketsCallback a_replay,
		HookPriority a_priority)
	{
		if (!RegistrationAllowed("WorldPrepassHold") || !EnsurePrepassHoldInstalled())
			return false;
		InsertByPriority(g_prepassHolds, PrepassHold{ a_priority, a_predicate, std::move(a_replay), {} });
		return true;
	}

	void ReplayHeldPrepassBucket(const HeldPrepassBucket& a_bucket)
	{
		PrepassBucket_Hook::func(a_bucket.renderer, a_bucket.head, a_bucket.technique, a_bucket.alpha);
	}

	bool RegisterPostResetState(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostResetState"))
			return false;
		InsertPrioritized(g_postResetState, std::move(callback), priority);
		return EnsureResetStateInstalled();
	}

	bool RegisterPostForwardSky(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostForwardSky"))
			return false;
		InsertPrioritized(g_postForwardSky, std::move(callback), priority);
		EnsureForwardSkyInstalled();
		return true;
	}

	bool RegisterPostSunShadowRender(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostSunShadowRender"))
			return false;
		InsertPrioritized(g_postSunShadowRender, std::move(callback), priority);
		return EnsureSunShadowRenderInstalled();
	}

	bool RegisterPostPrecipitationOcclusion(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostPrecipitationOcclusion"))
			return false;
		InsertPrioritized(g_postPrecipitationOcclusion, std::move(callback), priority);
		return EnsurePrecipitationOcclusionInstalled();
	}

	void RegisterPreFullscreenDeferredLightDraw(
		RenderHookCallback callback,
		HookPriority priority)
	{
		if (!RegistrationAllowed("PreFullscreenDeferredLightDraw"))
			return;
		InsertPrioritized(g_preFullscreenDeferredLightDraw, std::move(callback), priority);
		InstallDeferredDrawAnchor();
	}
}
