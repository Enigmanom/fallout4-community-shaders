#pragma once

#include "Host/HostPageCatalog.h"
#include "Host/HostRuntimeModel.h"
#include "Host/ShaderCompileOverlay.h"

#include <DearModdingUI/Client.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct ID3D11Device;
struct IDXGISwapChain;

namespace cs
{
	class Feature;
}

namespace cs::host
{
	class HostClient
	{
	public:
		static HostClient& Get();

		HostClient(const HostClient&) = delete;
		HostClient& operator=(const HostClient&) = delete;

		void DiscoverAndRegister() noexcept;
		void OnD3D11Bootstrap(
			ID3D11Device* a_device,
			IDXGISwapChain* a_swapChain,
			HWND a_window) noexcept;

		[[nodiscard]] dmui::Client& Client() noexcept { return _client; }

		void PostNotification(
			DMUI_StatusSeverity a_severity,
			std::string a_message,
			std::uint32_t a_durationMilliseconds,
			std::string a_title = {}) noexcept;
		void ResetOverlay() noexcept;

	private:
		HostClient();

		struct Page
		{
			HostPageDescriptor descriptor;
			Feature* feature{};
			DMUI_PageHandle handle{ DMUI_INVALID_PAGE_HANDLE };
		};

		struct OverlayChannel
		{
			Page* page{};
			FrameDemandTracker demand;
			std::optional<DMUI_Result> configurationFailure;
			std::optional<DMUI_Result> demandFailure;
			std::optional<DMUI_Result> focusFailure;
		};

		bool RegisterPages();
		bool RegisterActionsAndObservers();
		bool RegisterHotkeys();
		void PublishInitialStatus() noexcept;
		void DrawPage(Page& a_page);
		void DrawFeaturePage(Feature& a_feature);
		void DrawOverlayPage();
		void DrawShaderCompileOverlayPage();
		void ObserveFrame() noexcept;
		void ObservePageActivity(const dmui::PageActivity& a_activity) noexcept;
		void SyncPerformanceOverlay() noexcept;
		void SyncShaderCompileOverlay() noexcept;
		void SyncOverlay(
			OverlayChannel& a_overlay,
			std::string_view a_name,
			bool a_wanted,
			const DMUI_ManagedOverlayOptions& a_options) noexcept;
		void SyncOverlayFocus(
			OverlayChannel& a_overlay,
			std::string_view a_name,
			bool a_wanted) noexcept;
		void ObserveHotkeyBindings() noexcept;
		void ObserveHotkeyBinding(
			const char* a_name,
			const std::optional<DMUI_HotkeyActionHandle>& a_handle,
			std::string& a_snapshot) noexcept;
		void SetHotkeyEnabled(
			const char* a_name,
			const std::optional<DMUI_HotkeyActionHandle>& a_handle,
			bool a_enabled,
			std::optional<DMUI_Result>& a_failure) noexcept;
		void RetrySwapChain() noexcept;
		void LogFailure(std::string_view a_operation) const noexcept;
		void LogFailureOnce(
			std::string_view a_operation,
			std::optional<DMUI_Result>& a_lastResult) const noexcept;

		dmui::Client _client;
		std::vector<std::unique_ptr<Page>> _pages;
		OverlayChannel _performanceOverlay;
		OverlayChannel _shaderCompileOverlay;
		ShaderCompileOverlay _shaderCompile;
		std::optional<DMUI_HotkeyActionHandle> _overlayHotkey;
		std::optional<DMUI_HotkeyActionHandle> _captureHotkey;
		std::optional<DMUI_HotkeyActionHandle> _multiCaptureHotkey;
		std::optional<DMUI_HotkeyActionHandle> _dumpHotkey;
		std::array<std::string, 4> _hotkeyBindingSnapshots;
		std::array<std::optional<DMUI_Result>, 3> _hotkeyEnableFailures;
		std::optional<DMUI_Result> _videoMemoryFailure;
		std::atomic_bool _overlayVisible{ false };
		std::atomic_bool _registrationComplete{};
		std::atomic_bool _readyLogged{};
		std::atomic_bool _unavailableLogged{};
		std::mutex _swapChainMutex;
		IDXGISwapChain* _pendingSwapChain{};
	};
}
