# RenderDoc

The pinned `src/Features/RenderDoc.{h,cpp}` couples capture operations to Skyrim's Feature, globals, path
helpers and ImGui, so there is no separately compilable service. `CaptureService.{h,cpp}` is a portable
port of its capture contract, not an upstream-file replacement.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Divergence | `Enable RenderDoc Capture` defaults false and controls library loading | `features.RenderDoc.load` is the sole startup switch; no enable key | Main's load-with-feature implementation `d5b754f93` | `RenderDoc.cpp` `Load` |
| Tweak | Fixed Data/Renderdoc runtime and CommunityShaders/Captures paths | Installed-runtime registry discovery, explicit UTF-8/env-expanded DLL override, F4SE Documents capture directory | FO4 install layout; retains main's runtime discovery | `RenderDoc.cpp` `TryLoadRuntime`, `ApplyCapturePath` |
| Tweak | Global trigger without explicit device/window | Explicit D3D11/D3D12 target selection | Capture-target selection | `RenderDoc.cpp` |
| Fix | Global capture requests depend on presentation by the selected device | Every capture is explicit per-frame Start/EndFrameCapture on the chosen device, with a wildcard window for D3D11 behind the proxy | Main's correction `17bd6de7`: the proxy presents through D3D12, so global requests miss the game D3D11 frame | `RenderDoc.cpp` `OnGameFramePresented` |
| Tweak | ImGui sort specs and double-click open | Header buttons select a single sort column and filenames open on click | The forwarding UI lacks sort specs and double-click queries | `RenderDoc.cpp` `DrawCaptureFiles` |

## Upstream PR candidates

- Extract the capture service from `src/Features/RenderDoc.{h,cpp}` so both hosts can compile it
  unchanged; add explicit target binding.
- `RenderDoc.cpp:664–690`: global capture requests miss the game D3D11 frame when an interop proxy
  presents through D3D12. Expose explicit frame boundaries for the game device.
- `RenderDoc.cpp`: capture disk usage, inventory and deletion include every regular file, not only
  `.rdc`; an unavailable first new capture path consumes pending user comments; the unused
  `IsCapturing()` reports availability, not recording. community-shaders/skyrim-community-shaders#2919;
  FO4 retains the pinned policy until it merges.
