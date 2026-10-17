#include "Streamline.h"

#include <memory>

#include "Log.h"
#include "LogThrottle.h"

namespace cs::features
{
	namespace
	{
		auto* L = cs::log::Get("cs.feature.upscaling.streamline");

		constexpr std::uint64_t kReflexStatePollInterval = 30;

		using render::temporal::LatencyMarker;
		using render::temporal::LatencyMode;
		using render::temporal::LatencyOptions;
		using render::temporal::ProviderResult;
		using render::temporal::ProviderResultCode;

		ProviderResult Success() noexcept
		{
			return { .code = ProviderResultCode::kSuccess };
		}

		ProviderResult Failure(sl::Result a_result, const char* a_message)
		{
			return {
				.code = ProviderResultCode::kFailure,
				.sdkResult = static_cast<std::int64_t>(a_result),
				.message = a_message,
				.failureDomain = render::temporal::FailureDomain::kStreamline
			};
		}

		sl::ReflexMode ToReflexMode(LatencyMode a_mode) noexcept
		{
			switch (a_mode) {
			case LatencyMode::kLowLatency:
				return sl::ReflexMode::eLowLatency;
			case LatencyMode::kBoost:
				return sl::ReflexMode::eLowLatencyWithBoost;
			case LatencyMode::kOff:
				break;
			}
			return sl::ReflexMode::eOff;
		}

		sl::PCLMarker ToPCLMarker(LatencyMarker a_marker) noexcept
		{
			switch (a_marker) {
			case LatencyMarker::kInputSample:
				return sl::PCLMarker::eControllerInputSample;
			case LatencyMarker::kSimulationStart:
				return sl::PCLMarker::eSimulationStart;
			case LatencyMarker::kSimulationEnd:
				return sl::PCLMarker::eSimulationEnd;
			case LatencyMarker::kRenderSubmitStart:
				return sl::PCLMarker::eRenderSubmitStart;
			case LatencyMarker::kRenderSubmitEnd:
				return sl::PCLMarker::eRenderSubmitEnd;
			case LatencyMarker::kPresentStart:
				return sl::PCLMarker::ePresentStart;
			case LatencyMarker::kPresentEnd:
				return sl::PCLMarker::ePresentEnd;
			}
			return sl::PCLMarker::eSimulationStart;
		}
	}

	bool Streamline::IsAvailable() const noexcept
	{
		return initialized && deviceRegistered && featureReflex && featurePCL &&
		       slReflexSetOptions && slReflexSleep && slPCLSetMarker;
	}

	LatencyOptions Streamline::ResolveLatencyOptions() const noexcept
	{
		auto options = _latencyRequested;
		if (_dlssGForcesLatency) {
			// DLSS-G refuses to run unless Reflex is on.
			if (options.mode == LatencyMode::kOff) {
				options.mode = LatencyMode::kLowLatency;
			}
		} else if (_fsrGForcesLatencyOff) {
			options = {};
		}
		options.useMarkers = options.useMarkers && featurePCL;
		return options;
	}

	bool Streamline::ApplyLatencyOptions() noexcept
	{
		if (!IsAvailable()) {
			return false;
		}
		const auto effective = ResolveLatencyOptions();
		if (_latencyApplied == effective) {
			return true;
		}
		sl::ReflexOptions options{};
		options.mode = ToReflexMode(effective.mode);
		options.frameLimitUs = effective.frameLimitUs;
		options.useMarkersToOptimize = effective.useMarkers;
		if (SL_FAILED(result, slReflexSetOptions(options))) {
			CS_LOG_EVERY_MS(
				L,
				2000,
				spdlog::level::err,
				"Reflex options were rejected: {}",
				magic_enum::enum_name(result));
			return false;
		}
		_latencyApplied = effective;
		return true;
	}

	bool Streamline::SetLatencyConstraint(bool& a_flag, bool a_value) noexcept
	{
		std::scoped_lock lock(_latencyMutex);
		a_flag = a_value;
		return ApplyLatencyOptions();
	}

	ProviderResult Streamline::Configure(const LatencyOptions& a_options)
	{
		if (!IsAvailable()) {
			return { .code = ProviderResultCode::kUnavailable };
		}
		std::scoped_lock lock(_latencyMutex);
		_latencyRequested = a_options;
		return ApplyLatencyOptions() ? Success() : Failure(sl::Result::eErrorInvalidParameter, "Reflex options were rejected.");
	}

	ProviderResult Streamline::Sleep(std::uint32_t a_frame)
	{
		if (!IsAvailable()) {
			return { .code = ProviderResultCode::kUnavailable };
		}
		if (!EnsureFrameToken(a_frame)) {
			++_latencySleepFailures;
			return Failure(sl::Result::eErrorInvalidState, "Reflex sleep has no frame token.");
		}
		const auto calls = ++_latencySleepCalls;
		if (SL_FAILED(result, slReflexSleep(*frameToken))) {
			++_latencySleepFailures;
			CS_LOG_EVERY_MS(
				L,
				2000,
				spdlog::level::warn,
				"Reflex sleep failed: {}",
				magic_enum::enum_name(result));
			return Failure(result, "Reflex sleep failed.");
		}
		if (calls % kReflexStatePollInterval == 1) {
			PollReflexState();
		}
		return Success();
	}

	ProviderResult Streamline::SetLatencyMarker(
		LatencyMarker a_marker, std::uint32_t a_frame)
	{
		if (!IsAvailable()) {
			return { .code = ProviderResultCode::kUnavailable };
		}
		if (!EnsureFrameToken(a_frame)) {
			++_latencyMarkerFailures;
			return Failure(sl::Result::eErrorInvalidState, "PCL marker has no frame token.");
		}
		if (SL_FAILED(result, slPCLSetMarker(ToPCLMarker(a_marker), *frameToken))) {
			++_latencyMarkerFailures;
			CS_LOG_EVERY_MS(
				L,
				2000,
				spdlog::level::warn,
				"PCL marker {} failed: {}",
				static_cast<std::uint32_t>(a_marker),
				magic_enum::enum_name(result));
			return Failure(result, "PCL marker failed.");
		}
		return Success();
	}

	void Streamline::PollReflexState() noexcept
	{
		if (!slReflexGetState) {
			return;
		}
		// The state embeds 64 frame reports; keep it off the stack.
		const auto state = std::make_unique<sl::ReflexState>();
		if (slReflexGetState(*state) != sl::Result::eOk) {
			return;
		}
		_reflexLowLatencyAvailable.store(state->lowLatencyAvailable, std::memory_order_relaxed);
		_reflexReportAvailable.store(state->latencyReportAvailable, std::memory_order_relaxed);
		if (!state->latencyReportAvailable) {
			return;
		}
		const sl::ReflexReport* latest = nullptr;
		for (const auto& report : state->frameReport) {
			if (report.simStartTime && report.presentEndTime >= report.simStartTime &&
				(!latest || report.frameID > latest->frameID)) {
				latest = &report;
			}
		}
		if (!latest) {
			return;
		}
		_reflexReportFrame.store(latest->frameID, std::memory_order_relaxed);
		_reflexSimulationToPresentUs.store(
			latest->presentEndTime - latest->simStartTime, std::memory_order_relaxed);
		_reflexGpuFrameTimeUs.store(latest->gpuFrameTimeUs, std::memory_order_relaxed);
	}

	render::temporal::LatencyDiagnostics
	Streamline::GetLatencyDiagnostics() const noexcept
	{
		render::temporal::LatencyDiagnostics diagnostics;
		diagnostics.available = IsAvailable();
		diagnostics.markersAvailable = featurePCL && slPCLSetMarker;
		{
			std::scoped_lock lock(_latencyMutex);
			diagnostics.effective = _latencyApplied.value_or(LatencyOptions{});
		}
		diagnostics.lowLatencyAvailable = _reflexLowLatencyAvailable.load(std::memory_order_relaxed);
		diagnostics.latencyReportAvailable = _reflexReportAvailable.load(std::memory_order_relaxed);
		diagnostics.sleepCalls = _latencySleepCalls.load(std::memory_order_relaxed);
		diagnostics.sleepFailures = _latencySleepFailures.load(std::memory_order_relaxed);
		diagnostics.markerFailures = _latencyMarkerFailures.load(std::memory_order_relaxed);
		diagnostics.reportFrame = _reflexReportFrame.load(std::memory_order_relaxed);
		diagnostics.simulationToPresentUs = _reflexSimulationToPresentUs.load(std::memory_order_relaxed);
		diagnostics.gpuFrameTimeUs = _reflexGpuFrameTimeUs.load(std::memory_order_relaxed);
		return diagnostics;
	}
}
