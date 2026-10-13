# Remote Control

Pin: shared `53c0c1b14` (upstream `dc75702c7`).
`src/Features/RemoteControl.{h,cpp}` and `RemoteControl/DevBenchBridge.{h,cpp}`; the tool names,
actions and descriptors are kept. Upstream's `RemoteControl/NvPerf.*` is not ported.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Divergence | `capture renderdoc` triggers a global capture of `frames` frames | Adds `device` (`d3d11` default, `d3d12`) and brackets `StartFrameCapture`/`EndFrameCapture` on that device's own frame boundary; `d3d12` is refused unless the temporal proxy is active. PR candidate | A global trigger misses the game D3D11 frame behind the D3D12 proxy ([RenderDoc](RenderDoc.md)) | `RenderDoc.cpp` `RequestCapture`, `OnGameFramePresented` |
| Divergence | `capture` returns `queued` and no artifact | Returns `requestId`; `communityshaders.capture.completed` reports `captures:[{path,bytes}]`, one per frame, once the files stop growing, or `error`. PR candidate | A caller cannot otherwise find the file | `RenderDoc.cpp` `PollCaptureCompletion` |
| Divergence | `feature toggle` flips `Feature::loaded` and answers `queued` | Flips the framework `enabled` switch of a loaded, toggleable feature and answers `{enabled}` after it applies. PR candidate | See [Feature enable switch](README.md#feature-enable-switch) | `RemoteControlFeatureTool.cpp` |
| Tweak | `feature set` ignores keys it does not know | Unknown keys and next-launch (restart-only) keys are refused with `unknown`/`restartOnly` and nothing is applied | Restart-only settings cannot change live | `RemoteControlFeatureTool.cpp` `Set` |
| Tweak | `feature reset` restores defaults in memory | The framework restore also saves the feature and its `enabled` switch | Feature restore persists as part of restoring | `Feature.cpp` `RestoreDefaults` |
| Tweak | `settings load` rereads the whole configuration | Rereads the TOML and applies each loaded feature's live fields; restart-only fields are untouched | Startup-only settings cannot change live | `Feature.cpp` `LoadAllSettings` |
| Tweak | `settings reset` restores every feature | Restores each loaded feature, then saves all | Unloaded features have no engine state to republish | `Feature.cpp` `ResetAllSettings` |
| Tweak | `inspect shadercache` reports the ShaderCache compile set and `currentFailedCount`; `communityshaders.shaderRecompiled` fires when that set ends | Reports the running runtime compile batch (first queued variant of an idle queue to the last finished one) and the disk-cache counters, without `currentFailedCount`; the event fires per batch | FO4 compiles variants on demand rather than in sets | `ShaderVariantCompilation.cpp` `BatchTracker` |
| N/A | `capture screenshot` queues the Screenshot feature | Returns `screenshot feature not available` | FO4 has no [Screenshot](../FEATURES.md) feature yet | `RemoteControlCaptureTool.cpp` |

## Pending

| Kind | Upstream behavior | Status | Where |
|---|---|---|---|
| Pending | `communityshaders.nvperf` tool and `nvperf.completed` event | Not ported; the tool table takes it as one added entry | `RemoteControl/NvPerf.*` |
