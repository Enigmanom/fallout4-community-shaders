# Performance Overlay

`Profiler.{h,cpp}`, `CircularBuffer`, `DrawCallRow` and `ABTestAggregator` are consumed unchanged through
`xmake\shared.lua`, including the three-frame query ring, 128 timers, 300-sample pass histories, 60-frame
retirement, 600-sample default frame histories, EMA coefficients, graph thresholds and A/B
outlier/statistical rules.

## Translations

| Kind | Difference | Evidence / reason | Where |
|---|---|---|---|
| Forced | Native shader-family IDs and names replace Skyrim's enum; family 4 is labeled `DFPrePass / DFLight` | Both deferred subclasses store 4 at `BSShader+0x18`; native `SetDirtyStates(bool,bool)` supplies the post-state-submission timing boundary, not Skyrim's one-argument entry | `ShaderSubclassHooks.cpp`, `FrameProfiler.cpp` |
| Divergence | A/B snapshots contain only healthy, schema-declared live-effect fields; a failed restore keeps the persistence lock and exposes retry | Activation, ownership, restart-only fields and overlay state never enter the comparison; transient variants cannot be saved as presets | `LiveSettings.h`, `PerformanceOverlay.cpp` |
| Tweak | Sampling continues while the overlay is hidden; scene profiling rotates at post-composite while frame history/A/B use host observers; a failed VRAM query shows "unavailable" instead of hiding the overlay | Upstream samples through visible overlay drawing; native-frame cadence under FG, secondary views and menus is unproven | `PerformanceOverlay.cpp` `TickHostFrame`, `FrameProfiler.cpp` |
| Tweak | Startup live settings or an explicitly captured USER baseline replace reloading the last saved USER configuration | TOML edits persist through the host UI, so explicit capture provides a stable in-memory baseline | `PerformanceOverlay.cpp` `CaptureSettings` |

## Pending

| Kind | Upstream behavior | Status | Where |
|---|---|---|---|
| Pending | Manual shader-family/Total toggles and captured TEST columns in the draw-call table | Not ported; family instrumentation is read-only and a live ownership bypass is not acceptable | `HandleShaderToggle`/`HandleTotalRowToggle`; FO4 `DrawDrawCalls` |
| Pending | Full profiling-renderer presentation, including expandable per-pass history plots | Sorted CPU/GPU Avg/P95/P99/percent feature and leaf-pass tables are present; history interaction is not ported | `Menu/ProfilingRenderer.cpp`; FO4 `DrawPasses` |

## Upstream PR candidates

- `src/Features/PerformanceOverlay.cpp:1989–2001`: the measured-FG branch requires
  `!IsFrameGenerationActive()` inside an already-active block, so stable active FG always takes the 2x
  fallback. `e1340ed9` generalized an FSR-only exclusion into this check, and
  `DX12SwapChain::GetFrameTime` measures between calls rather than generated presents, so dropping the
  negation would show a misleading value. Needs a real post-FG measurement; not filed. FO4 preserves and
  labels the fallback.
- `src/Profiler.cpp` `Initialize`: query-creation HRESULTs are ignored before query pointers are
  passed to the context. community-shaders/skyrim-community-shaders#2921.
- `src/Features/PerformanceOverlay.cpp` `DrawABTestStatisticalValidity`: the valid and marginal tiers
  accept samples from one variant, so a run stopped before the first switch reads valid or marginal
  with no USER samples. community-shaders/skyrim-community-shaders#2918; FO4 matches upstream until it
  merges.
