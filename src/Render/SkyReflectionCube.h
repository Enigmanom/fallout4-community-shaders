#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>

struct ID3D11ShaderResourceView;

namespace cs::engine
{
	// The host renders the sky into the engine reflection cube for consumers.

	// Call during Load or OnPostPostLoad; false keeps the consumer's fallback.
	[[nodiscard]] bool RegisterSkyReflectionCubeConsumer(
		std::string_view a_name,
		std::function<bool()> a_wantsCube);

	// Render thread. Null unless all six faces are fresh.
	[[nodiscard]] ID3D11ShaderResourceView* GetSkyReflectionCubeSRV() noexcept;

	struct SkyReflectionCubeStatus
	{
		bool hookInstalled = false;
		bool disabled = false;
		bool valid = false;
		std::uint32_t faceMask = 0;
		std::uint32_t consumers = 0;
		std::uint64_t renders = 0;
		std::uint64_t faces = 0;
		std::uint64_t invalidations = 0;
		std::uint64_t unboundTargets = 0;
		std::uint64_t skippedNoDemand = 0;
		std::uint64_t skippedInterior = 0;
		std::uint64_t skippedSkyHidden = 0;
		std::uint64_t skippedNoCamera = 0;
		std::uint64_t skippedNoTargets = 0;
		std::uint64_t skippedNoWorld = 0;
		// LOD properties whose flags were cleared around the clicks.
		std::uint64_t lodFlagsToggledLast = 0;
		std::uint64_t lodFlagsToggledTotal = 0;
		std::uint64_t lodFlagsNsLast = 0;
		std::uint64_t lodFlagsNsTotal = 0;
		std::uint64_t lodFlagsSkipped = 0;
	};

	[[nodiscard]] SkyReflectionCubeStatus GetSkyReflectionCubeStatus() noexcept;

	// One terrain LOD scene root as the cube sees it just before the click.
	struct SkyReflectionCubeLodRoot
	{
		bool present = false;
		bool appCulled = false;
		std::uint16_t children = 0;
		std::uint16_t culledChildren = 0;
		// Per root child node, attach order: attached and app-culled children.
		std::array<std::uint16_t, 4> levelChildren{};
		std::array<std::uint16_t, 4> levelCulled{};
	};

	struct SkyReflectionCubeScene
	{
		bool sampled = false;
		bool preCullEnabled = false;
		bool preCullTempDisabled = false;
		bool terrainManager = false;
		// BGSTerrainManager +0x30 saved and +0x35 active cull bytes as bit masks.
		std::uint8_t savedCull = 0;
		std::uint8_t activeCull = 0;
		bool cullActive = false;
		SkyReflectionCubeLodRoot land;
		SkyReflectionCubeLodRoot objects;
		SkyReflectionCubeLodRoot trees;
	};

	// Sampled once per full cube cycle; thread safe.
	[[nodiscard]] SkyReflectionCubeScene GetSkyReflectionCubeScene();
}
