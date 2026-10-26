#include "Render/RenderHooks.h"

#include "Log.h"
#include "Render/DeferredDrawAnchor.h"
#include "Render/Engine.h"
#include "Render/EngineCallSite.h"
#include "Render/FrameProfiler.h"
#include "Render/HookRegistry.h"
#include "Render/ShaderInjection.h"

#include "RE/B/BSGeometry.h"
#include "RE/B/BSShadowFrustumLight.h"
#include "RE/B/BSShadowParabolicLight.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <winrt/base.h>

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

		constexpr std::size_t kShadowLightKindCount = 3;

		std::vector<PrioritizedCallback> g_postDeferredPrePass;
		std::vector<PrioritizedCallback> g_preDeferredPrePass;
		std::vector<PrioritizedCallback> g_preDeferredLightsImpl;
		std::vector<PrioritizedCallback> g_postDeferredLightsImpl;
		std::vector<PrioritizedCallback> g_preDeferredComposite;
		std::vector<PrioritizedCallback> g_postDeferredComposite;
		std::vector<PrioritizedCallback> g_preFullscreenDeferredLightDraw;
		std::vector<PrioritizedCallback> g_postForwardSky;
		std::vector<PrioritizedCallback> g_prePrecipitationOcclusion;
		std::vector<PrioritizedCallback> g_postPrecipitationOcclusion;
		std::vector<PrioritizedCallback> g_postSunShadowRender;
		std::vector<PrioritizedCallback> g_preWaterUpdate;
		std::vector<PrioritizedCallback> g_postWaterUpdate;
		std::vector<PrioritizedCallback> g_preCommandBufferReplay;
		std::vector<PrioritizedCallback> g_postCommandBufferReplay;
		std::array<std::vector<PrioritizedCallback>, kShadowLightKindCount> g_preShadowLightRender;
		std::array<std::vector<PrioritizedCallback>, kShadowLightKindCount> g_postShadowLightRender;
		std::vector<PrioritizedCallback> g_postResetState;
		struct PrepassHold
		{
			HookPriority priority;
			PrepassBucketPredicate predicate;
			HeldPrepassBucketsCallback replay;
			std::vector<HeldPrepassBucket> held;
		};

		std::vector<PrepassHold> g_prepassHolds;
		struct ZPrepassShaderSelection
		{
			HookPriority priority;
			ZPrepassVertexShaderSelector selector;
		};
		std::vector<ZPrepassShaderSelection> g_zPrepassShaders;
		bool g_prePassInstalled = false;
		bool g_prepassHoldInstalled = false;
		bool g_zPrepassDrawInstalled = false;
		bool g_insideWorldPrepassBatch = false;
		bool g_lightsImplInstalled = false;
		bool g_compositeInstalled = false;
		bool g_deferredDrawAnchorInstalled = false;
		bool g_drawProfilingInstalled = false;
		bool g_drawFlushInstalled = false;
		bool g_forwardSkyInstalled = false;
		bool g_precipitationOcclusionInstalled = false;
		bool g_sunShadowRenderInstalled = false;
		bool g_frustumShadowRenderInstalled = false;
		bool g_parabolicShadowRenderInstalled = false;
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

		// Callbacks run plugin GPU work that may replace cached draw bindings.
		class ScopedForeignWork
		{
		public:
			explicit ScopedForeignWork(bool a_active) noexcept :
				_active(a_active)
			{}
			~ScopedForeignWork()
			{
				if (_active)
					InvalidateShaderInjectionBindings();
			}
			ScopedForeignWork(const ScopedForeignWork&) = delete;
			ScopedForeignWork& operator=(const ScopedForeignWork&) = delete;

		private:
			bool _active;
		};

		void Dispatch(const std::vector<PrioritizedCallback>& v)
		{
			const ScopedForeignWork work(!v.empty());
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
			const ScopedForeignWork work(!v.empty());
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

		struct DeferredPrePass_Hook
		{
			static void thunk()
			{
				MarkRegistrationClosed();
				Dispatch(g_preDeferredPrePass);
				func();
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
				if constexpr (Route == DrawRoute::kCommandBuffer)
					Dispatch(g_preCommandBufferReplay);
				{
					const DrawRouteScope scope(Route);
					func(a_this, a_args...);
				}
				if constexpr (Route == DrawRoute::kCommandBuffer)
					Dispatch(g_postCommandBufferReplay);
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

		// Command-buffer flushes precede records that bind their own state.
		std::optional<DeferredDrawAnchorDecision> SelectDrawFlushDecision(std::uint32_t a_residualR9d) noexcept
		{
			if (!g_deferredDrawAnchorInstalled || t_drawRoute == DrawRoute::kNone || t_drawRoute == DrawRoute::kCommandBuffer)
				return std::nullopt;
			return SelectDeferredDrawAnchorDecision(
				g_insideDeferredLightsImpl,
				g_insideDeferredComposite,
				t_drawRoute == DrawRoute::kTriShape,
				a_residualR9d);
		}

		void OnDrawStateFlushed(bool a_outputStateOnly, bool a_compute, const std::optional<DeferredDrawAnchorDecision>& a_decision) noexcept
		{
			auto* rendererData = RE::BSGraphics::GetRendererData();
			auto* context = rendererData ? reinterpret_cast<ID3D11DeviceContext*>(rendererData->context) : nullptr;
			if (t_drawRoute == DrawRoute::kNone) {
				// Flush is output-only and RunComputeShader is compute; draws are neither.
				if (!a_outputStateOnly && !a_compute)
					RecordUndispatchedInjectedDraw(context);
				return;
			}
			if (!a_decision)
				return;
			if (a_decision->dispatchFullscreenLightCallbacks)
				Dispatch(g_preFullscreenDeferredLightDraw);
			if (a_decision->dispatchInjections)
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
				const auto decision = SelectDrawFlushDecision(a_residualR9d);
				const ScopedEngineDrawFlush flush(a_outputStateOnly, a_compute, decision && decision->dispatchInjections);
				func(a_outputStateOnly, a_compute);
				if (g_drawProfilingInstalled)
					render::profiling::RecordDraw();
				if (g_deferredDrawAnchorInstalled)
					OnDrawStateFlushed(a_outputStateOnly, a_compute, decision);
			}
			static inline REL::Relocation<decltype(&RE::BSGraphics::SetDirtyStates)> func;
		};

		struct ResetState_Hook
		{
			static void thunk(RE::BSGraphics::Renderer* a_this)
			{
				func(a_this);
				InvalidateShaderInjectionBindings();
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
				Dispatch(g_prePrecipitationOcclusion);
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
				Dispatch(g_preShadowLightRender[static_cast<std::size_t>(ShadowLightKind::kDirectional)]);
				func(a_this, a_mask);
				constexpr std::uint32_t kMainSunMask = 7;
				if (a_mask == kMainSunMask && g_insideDeferredLightsImpl && a_this == GetSunShadowLight())
					DispatchGuarded(g_postSunShadowRender, "Post sun shadow render");
				Dispatch(g_postShadowLightRender[static_cast<std::size_t>(ShadowLightKind::kDirectional)]);
			}
			static inline REL::Relocation<void(RE::BSShadowDirectionalLight*, std::uint32_t)> func;
		};

		// Local lights have no stock-caller boundary to preserve.
		template <ShadowLightKind Kind, class Light>
		struct LocalShadowRender_Hook
		{
			static void thunk(Light* a_this, std::uint32_t a_mask)
			{
				Dispatch(g_preShadowLightRender[static_cast<std::size_t>(Kind)]);
				func(a_this, a_mask);
				Dispatch(g_postShadowLightRender[static_cast<std::size_t>(Kind)]);
			}
			static inline REL::Relocation<void(Light*, std::uint32_t)> func;
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
				DispatchGuarded(g_postWaterUpdate, "Post water update");
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
					const ScopedForeignWork work(true);
					try {
						hold.replay(hold.held);
					} catch (const std::exception& e) {
						L->error("Prepass hold replay failed: {}", e.what());
					} catch (...) {
						L->error("Prepass hold replay failed.");
					}
					hold.held.clear();
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// DoZPrePass world-camera opaque loop; first-person entries are left alone.
		struct WorldZPrepassDraw_Hook
		{
			static void thunk(
				RE::BSGraphics::RendererShadowState& a_state,
				RE::BSGraphics::ZPrePassDrawData& a_data,
				std::uint64_t& a_vertexDesc,
				std::uint16_t& a_cullMode,
				std::uint16_t& a_depthBiasMode)
			{
				// RegisterObject_Standard, the sole producer, queues &geometry->world.
				const auto* object = reinterpret_cast<const RE::NiAVObject*>(
					reinterpret_cast<std::uintptr_t>(a_data.world) - offsetof(RE::NiAVObject, world));
				const auto& geometry = static_cast<const RE::BSGeometry&>(*object);
				ID3D11VertexShader* shader = nullptr;
				for (const auto& entry : g_zPrepassShaders) {
					if ((shader = entry.selector(geometry)))
						break;
				}
				auto* context = shader ? GetImmediateContext() : nullptr;
				if (!context)
					return func(a_state, a_data, a_vertexDesc, a_cullMode, a_depthBiasMode);
				winrt::com_ptr<ID3D11VertexShader> engineShader;
				context->VSGetShader(engineShader.put(), nullptr, nullptr);
				context->VSSetShader(shader, nullptr, 0);
				func(a_state, a_data, a_vertexDesc, a_cullMode, a_depthBiasMode);
				context->VSSetShader(engineShader.get(), nullptr, 0);
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

		constexpr std::size_t kShadowRenderVtableSlot = 10;

		// Render is slot 10 on every shadow light; refuse a layout that disagrees.
		template <class Hook>
		bool EnsureShadowRenderInstalled(
			bool& a_installed,
			const auto& a_vtable,
			REL::VariantID a_render,
			const char* a_name)
		{
			if (a_installed) {
				return true;
			}
			REL::Relocation<std::uintptr_t> vtable{ a_vtable[0] };
			const auto slot = vtable.address() + kShadowRenderVtableSlot * sizeof(std::uintptr_t);
			const auto expected = a_render.address();
			if (*reinterpret_cast<const std::uintptr_t*>(slot) != expected) {
				L->error("{} vtable slot {} is not Render ({:#x}); shadow render hook unavailable", a_name, kShadowRenderVtableSlot, expected);
				return false;
			}
			stl::write_vfunc<kShadowRenderVtableSlot, Hook>(a_vtable[0]);
			a_installed = Hook::func.address() != 0;
			if (!a_installed) {
				L->error("Hook failed on {}::Render", a_name);
				return false;
			}
			L->info("Hook installed on {}::Render (vtable slot {})", a_name, kShadowRenderVtableSlot);
			return true;
		}

		bool EnsureSunShadowRenderInstalled()
		{
			if (g_sunShadowRenderInstalled) {
				return true;
			}
			if (!EnsureShadowRenderInstalled<SunShadowRender_Hook>(
					g_sunShadowRenderInstalled,
					RE::VTABLE::BSShadowDirectionalLight,
					RE::ID::BSShadowDirectionalLight::Render,
					"BSShadowDirectionalLight")) {
				return false;
			}
			// The inside-DeferredLightsImpl flag only flips while that hook is installed.
			EnsureDeferredLightsImplInstalled();
			return true;
		}

		bool EnsureShadowLightRenderInstalled(ShadowLightKind a_kind)
		{
			switch (a_kind) {
			case ShadowLightKind::kDirectional:
				return EnsureSunShadowRenderInstalled();
			case ShadowLightKind::kFrustum:
				return EnsureShadowRenderInstalled<LocalShadowRender_Hook<ShadowLightKind::kFrustum, RE::BSShadowFrustumLight>>(
					g_frustumShadowRenderInstalled,
					RE::VTABLE::BSShadowFrustumLight,
					RE::ID::BSShadowFrustumLight::Render,
					"BSShadowFrustumLight");
			case ShadowLightKind::kParabolic:
				return EnsureShadowRenderInstalled<LocalShadowRender_Hook<ShadowLightKind::kParabolic, RE::BSShadowParabolicLight>>(
					g_parabolicShadowRenderInstalled,
					RE::VTABLE::BSShadowParabolicLight,
					RE::ID::BSShadowParabolicLight::Render,
					"BSShadowParabolicLight");
			}
			return false;
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

		bool EnsureWorldZPrepassDrawInstalled()
		{
			if (g_zPrepassDrawInstalled)
				return true;
			constexpr CallSiteAnchor kWorldDraw{
				.name = "DoZPrePass -> world BSGraphics::RenderZPrePass",
				.function = RE::ID::BSGraphics::Renderer::DoZPrePass,
				.offset = { 0x429, 0x42A, 0x42A },
				.target = RE::ID::BSGraphics::RenderZPrePass
			};
			const auto site = ResolveCallSite(kWorldDraw);
			if (!site) {
				L->error("World Z-prepass draw hook unavailable: {}", site.error());
				return false;
			}
			stl::write_thunk_call<WorldZPrepassDraw_Hook>(*site);
			g_zPrepassDrawInstalled = true;
			L->info("Hook installed on {}", kWorldDraw.name);
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

		bool EnsureCommandBufferRouteInstalled()
		{
			return InstallDrawRoute<DrawRoute::kCommandBuffer, &RE::BSGraphics::Renderer::ProcessCommandBuffer>(
				RE::ID::BSGraphics::Renderer::ProcessCommandBuffer, "ProcessCommandBuffer");
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
			       EnsureCommandBufferRouteInstalled() &&
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
		InsertByPriority(g_postDeferredPrePass, PrioritizedCallback{ priority, std::move(callback) });
		EnsureDeferredPrePassInstalled();
		return true;
	}

	bool RegisterPreDeferredPrePass(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreDeferredPrePass"))
			return false;
		InsertByPriority(g_preDeferredPrePass, PrioritizedCallback{ priority, std::move(callback) });
		EnsureDeferredPrePassInstalled();
		return true;
	}

	bool RegisterPreDeferredLightsImpl(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreDeferredLightsImpl"))
			return false;
		InsertByPriority(g_preDeferredLightsImpl, PrioritizedCallback{ priority, std::move(callback) });
		EnsureDeferredLightsImplInstalled();
		return true;
	}

	bool RegisterPostDeferredLightsImpl(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostDeferredLightsImpl"))
			return false;
		InsertByPriority(g_postDeferredLightsImpl, PrioritizedCallback{ priority, std::move(callback) });
		EnsureDeferredLightsImplInstalled();
		return true;
	}

	bool RegisterPreDeferredComposite(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreDeferredComposite"))
			return false;
		InsertByPriority(g_preDeferredComposite, PrioritizedCallback{ priority, std::move(callback) });
		EnsureDeferredCompositeInstalled();
		return true;
	}

	bool RegisterPostDeferredComposite(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostDeferredComposite"))
			return false;
		InsertByPriority(g_postDeferredComposite, PrioritizedCallback{ priority, std::move(callback) });
		EnsureDeferredCompositeInstalled();
		return true;
	}

	bool RegisterPreWaterUpdate(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreWaterUpdate"))
			return false;
		InsertByPriority(g_preWaterUpdate, PrioritizedCallback{ priority, std::move(callback) });
		return EnsureWaterUpdateInstalled();
	}

	bool RegisterPostWaterUpdate(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostWaterUpdate"))
			return false;
		InsertByPriority(g_postWaterUpdate, PrioritizedCallback{ priority, std::move(callback) });
		return EnsureWaterUpdateInstalled();
	}

	bool RegisterPreCommandBufferReplay(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreCommandBufferReplay"))
			return false;
		InsertByPriority(g_preCommandBufferReplay, PrioritizedCallback{ priority, std::move(callback) });
		return EnsureCommandBufferRouteInstalled();
	}

	bool RegisterPostCommandBufferReplay(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostCommandBufferReplay"))
			return false;
		InsertByPriority(g_postCommandBufferReplay, PrioritizedCallback{ priority, std::move(callback) });
		return EnsureCommandBufferRouteInstalled();
	}

	bool RegisterPreShadowLightRender(ShadowLightKind a_kind, RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PreShadowLightRender"))
			return false;
		InsertByPriority(g_preShadowLightRender[static_cast<std::size_t>(a_kind)], PrioritizedCallback{ priority, std::move(callback) });
		return EnsureShadowLightRenderInstalled(a_kind);
	}

	bool RegisterPostShadowLightRender(ShadowLightKind a_kind, RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostShadowLightRender"))
			return false;
		InsertByPriority(g_postShadowLightRender[static_cast<std::size_t>(a_kind)], PrioritizedCallback{ priority, std::move(callback) });
		return EnsureShadowLightRenderInstalled(a_kind);
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

	bool RegisterWorldZPrepassVertexShader(ZPrepassVertexShaderSelector a_selector, HookPriority a_priority)
	{
		if (!RegistrationAllowed("WorldZPrepassVertexShader") || !EnsureWorldZPrepassDrawInstalled())
			return false;
		InsertByPriority(g_zPrepassShaders, ZPrepassShaderSelection{ a_priority, a_selector });
		return true;
	}

	bool RegisterPostResetState(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostResetState"))
			return false;
		InsertByPriority(g_postResetState, PrioritizedCallback{ priority, std::move(callback) });
		return EnsureResetStateInstalled();
	}

	bool RegisterPostForwardSky(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostForwardSky"))
			return false;
		InsertByPriority(g_postForwardSky, PrioritizedCallback{ priority, std::move(callback) });
		EnsureForwardSkyInstalled();
		return true;
	}

	bool RegisterPostSunShadowRender(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostSunShadowRender"))
			return false;
		InsertByPriority(g_postSunShadowRender, PrioritizedCallback{ priority, std::move(callback) });
		return EnsureSunShadowRenderInstalled();
	}

	bool RegisterPrePrecipitationOcclusion(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PrePrecipitationOcclusion"))
			return false;
		InsertByPriority(g_prePrecipitationOcclusion, PrioritizedCallback{ priority, std::move(callback) });
		return EnsurePrecipitationOcclusionInstalled();
	}

	bool RegisterPostPrecipitationOcclusion(RenderHookCallback callback, HookPriority priority)
	{
		if (!RegistrationAllowed("PostPrecipitationOcclusion"))
			return false;
		InsertByPriority(g_postPrecipitationOcclusion, PrioritizedCallback{ priority, std::move(callback) });
		return EnsurePrecipitationOcclusionInstalled();
	}

	void RegisterPreFullscreenDeferredLightDraw(
		RenderHookCallback callback,
		HookPriority priority)
	{
		if (!RegistrationAllowed("PreFullscreenDeferredLightDraw"))
			return;
		InsertByPriority(g_preFullscreenDeferredLightDraw, PrioritizedCallback{ priority, std::move(callback) });
		InstallDeferredDrawAnchor();
	}
}
