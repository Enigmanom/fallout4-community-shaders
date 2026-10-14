#include "ExtendedMaterials.h"

#include <DearModdingUI/Client.h>

#include <array>
#include <string>

#include <toml++/toml.hpp>

#include "Log.h"
#include "Menu/Section.h"
#include "Menu/SettingsEdit.h"
#include "Render/FeatureShaderBindings.h"
#include "Render/ObjectLOD.h"
#include "Render/ShaderInjection.h"
#include "Render/ShaderSubclassHooks.h"
#include "Render/SharedData.h"
#include "Settings/SettingsPersistence.h"
#include "Telemetry/Telemetry.h"

#include "RE/B/BSRenderPass.h"

namespace cs::features
{
	namespace
	{
		auto* L = cs::log::Get("cs.feature.extendedmaterials");

		// FO4: no parallax flag exists; only object LOD is kept out, as upstream.
		bool IsParallaxEligible(RE::BSRenderPass* a_pass, cs::engine::PrepassBakePath) noexcept
		{
			return !cs::engine::IsObjectLODShape(a_pass);
		}
	}

	ExtendedMaterials* ExtendedMaterials::GetSingleton()
	{
		static ExtendedMaterials instance;
		return &instance;
	}

	std::span<const FeatureDebugView> ExtendedMaterials::GetDebugViews() const noexcept
	{
		static constexpr std::array<FeatureDebugView, 1> kDebugViews{ { { "parallax_shift",
			"Parallax UV shift: red objects, green terrain, blue unshifted",
			FeatureDebugViewKind::kFullscreen } } };
		return kDebugViews;
	}

	void ExtendedMaterials::SetDebugView(std::string_view a_view) noexcept
	{
		_debugParallaxShift.store(a_view == "parallax_shift", std::memory_order_release);
	}

	FullscreenDebugData ExtendedMaterials::GetFullscreenDebugData() const noexcept
	{
		if (!_debugParallaxShift.load(std::memory_order_acquire) || !_injectionsOperational.load(std::memory_order_acquire))
			return {};
		return { .owner = FullscreenDebugOwner::ExtendedMaterials, .mode = ExtendedMaterialsDebugMode::ParallaxShift };
	}

	bool ExtendedMaterials::Configure(const toml::table& a_config, std::string& a_error)
	{
		auto candidate = _settings;
		if (!settings::Parse(extended_materials::kSchema, a_config, candidate, a_error))
			return false;
		_settings = candidate;
		_liveSettings = settings::BindLiveSettings(extended_materials::kSchema, _settings);
		return true;
	}

	bool ExtendedMaterials::SaveSettings()
	{
		return settings::SaveDelta(extended_materials::kSchema, GetConfigKey(), _settings, *L);
	}

	void ExtendedMaterials::Load()
	{
		if (!cs::engine::RegisterFeatureShaderBindings("ExtendedMaterials", *this)) {
			FailLoad("Extended Materials shader contribution registration failed.");
			return;
		}
		if (!cs::engine::RegisterPrepassDrawClassifier(cs::engine::PrepassLaneComponent::kZ, &IsParallaxEligible)) {
			FailLoad("Extended Materials prepass draw classifier installation failed.");
			return;
		}
		_registrationsReady.store(true, std::memory_order_release);
	}

	void ExtendedMaterials::CollectTelemetry(cs::telemetry::Sink& a_sink) const
	{
		a_sink
			.Field("operational", _injectionsOperational.load(std::memory_order_relaxed))
			.Field("registrations_ready", _registrationsReady.load(std::memory_order_relaxed));
		cs::engine::WritePrepassLaneTelemetry(a_sink, cs::engine::PrepassLaneComponent::kZ, "eligible");
	}

	bool ExtendedMaterials::ValidateShaderInjections(std::string& a_error)
	{
		_injectionsOperational.store(false, std::memory_order_release);
		if (!_registrationsReady.load(std::memory_order_acquire)) {
			a_error = "shader contributions did not all register";
			return false;
		}
		if (!cs::render::IsSharedDataReady()) {
			a_error = "the shared substrate is unavailable, so b6 carries no Extended Materials settings";
			return false;
		}
		if (!cs::engine::ValidateShaderInjectionRoutes("ExtendedMaterials", a_error))
			return false;
		_injectionsOperational.store(true, std::memory_order_release);
		return true;
	}

	cs::ExtendedMaterialsFeatureData ExtendedMaterials::GetCommonBufferData() const
	{
		if (!_injectionsOperational.load(std::memory_order_acquire))
			return {};
		cs::ExtendedMaterialsFeatureData data{};
		data.EnableParallax = _settings.EnableParallax ? 1u : 0u;
		data.EnableTerrainParallax = _settings.EnableTerrain ? 1u : 0u;
		data.EnableHeightBlending = _settings.EnableHeightBlending ? 1u : 0u;
		data.EnableParallaxWarpingFix = _settings.EnableParallaxWarpingFix ? 1u : 0u;
		return data;
	}

	void ExtendedMaterials::DrawSettings()
	{
		settings::SettingsEdit edit{ *this };
		const auto toggle = [&](const auto& field, const char* a_tooltip) {
			const std::string label = std::string(field.description) + "##" + std::string(field.key);
			edit.Discrete(dmui::ui::Checkbox(label.c_str(), &(_settings.*field.member)));
			if (const dmui::TooltipScope tooltip{ dmui::ui::HoveredFlags::kNone };
				tooltip.Visible()) {
				dmui::ui::Text("%s", a_tooltip);
			}
		};
		const auto& [parallax, terrain, heightBlending, warpingFix] = extended_materials::kSchema.fields;
		if (const ui::Section section{ "extended-materials-parallax", "Parallax" }; section) {
			toggle(parallax, "Enables parallax on standard meshes made for parallax.");
			toggle(terrain,
				"Enables terrain parallax using the alpha channel of each landscape specular (_s) texture. "
				"Therefore, all landscape textures must support parallax for this effect to work properly. ");
			toggle(heightBlending, "Enables landscape texture blending based on parallax. ");
			toggle(warpingFix, "Enables a fix reducing parallax scale on curved and smooth normal triangles.");
		}
	}

	void ExtendedMaterials::RestoreDefaultSettings()
	{
		_settings = Settings{};
		SaveSettings();
	}

	namespace
	{
		struct AutoRegister
		{
			AutoRegister() { FeatureManager::Get().Register(ExtendedMaterials::GetSingleton()); }
		};
		static AutoRegister _autoRegister;
	}
}
