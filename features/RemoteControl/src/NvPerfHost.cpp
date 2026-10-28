#include "Features/RemoteControl/NvPerfHost.h"

#include "Render/Engine.h"
#include "Render/FrameAnnotations.h"
#include "F4SE/API.h"
#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"
#include "Render/FrameProfiler.h"

#include <optional>

namespace NvPerf::Host
{
	namespace
	{
		// Render thread only: a capture requests it at start and releases it at finish.
		std::optional<cs::render::frame_annotations::ScopedForce> g_force;
	}

	ID3D11Device* Device()
	{
		return cs::engine::GetDevice();
	}

	ID3D11DeviceContext* Context()
	{
		return cs::engine::GetImmediateContext();
	}

	std::uint64_t FrameCount()
	{
		return cs::render::profiling::FrameSequence();
	}

	bool FrameAnnotations()
	{
		return cs::render::frame_annotations::Enabled();
	}

	void SetFrameAnnotations(bool enabled)
	{
		// FO4: a forced hold replaces the flag so the saved setting is untouched.
		if (!enabled)
			g_force.reset();
		else if (!g_force && !cs::render::frame_annotations::Enabled())
			g_force.emplace();
	}

	std::filesystem::path LogDirectory()
	{
		// FO4: F4SE writes the plugin log under Documents\My Games\<save folder>\F4SE.
		wchar_t* known = nullptr;
		const auto result = REX::W32::SHGetKnownFolderPath(
			REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(known));
		std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> folder(known, REX::W32::CoTaskMemFree);
		if (!folder || result != 0)
			return {};
		auto name = F4SE::GetSaveFolderName();
		return std::filesystem::path(folder.get()) / "My Games" / (name.empty() ? "Fallout4" : std::string(name)) / "F4SE";
	}

	std::filesystem::path DataDirectory()
	{
		// FO4: the plugin data folder is relative to the game's working directory.
		return std::filesystem::path("Data") / "F4SE" / "Plugins" / "FO4CommunityShaders";
	}
}
