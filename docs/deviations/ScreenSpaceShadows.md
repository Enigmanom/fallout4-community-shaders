# Screen Space Shadows

All three upstream shaders and Bend's CPU header are consumed unchanged through `xmake\shared.lua`.
BendSettings names, keys, defaults and edit ranges match upstream. The output is upstream's R8G8_UNORM
front/back visibility, cleared to white each frame, with no history. Canonical t17 includes reprojected
near depth and Bend reads it directly at CS t0, so first person casts as upstream does but never
receives SSS.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Forced | Ordinary world-projection scene depth and its matching VP | Bind canonical R32_FLOAT (t17 content) directly to Bend's CS t0, using the copied world+jitter record's row-vector ViewProjection | Native world depth is `mad(raw,1.01,-0.01)` and near depth has a different projection; the R32 declaration is selected by upstream's `TERRAIN_BLENDING` define | `CanonicalDepth.cpp`, `ScreenSpaceShadows.cpp` |
| Forced | First-person forward materials never receive SSS | Explicit receiver gate returns 1 for raw depth `<=0.01` in FO4's shared deferred light passes; first person still casts through canonical depth | Upstream's `deferredPass` is cleared before RenderFirstPersonView, but FO4 lights world and first person in shared deferred passes | `FO4/ScreenSpaceShadowConsumer.hlsli` |
| Fix | Consumer adds 0.5 to pixel-centered `SV_POSITION` before integer conversion | Shared-fork seam `6f81ebc25` samples the pixel directly | Upstream Lighting, DistantTree and RunGrass pass pixel-centered `SV_POSITION` like FO4 DFLight, and Bend writes floored pixel coordinates. See [shared seam edits](README.md#shared-seam-edits) | `ScreenSpaceShadows.hlsli` |
| Forced | `Lighting.hlsl` separates direct visibility x and transmission visibility x/y by facing | Directional split families apply x to their native shared shadow term when front-facing and y to back-facing wrap/transmission; the no-cascade family applies direct and transmission separately | DFLight DIRSPLITS1/2/3 combine direct and transmission before the shared shadow, which already carries x for front transmission, so the extra multiplier is back-facing only | `FO4BackTransmissionScreenSpaceShadow`, `DFLight.hlsl` |
| Divergence | Active-extent mask, Skyrim lifecycle | Disabled, non-full-sky or failed-allocation frames output a full-extent white (unshadowed) mask | Fail-closed output; allocation retries back off | `ScreenSpaceShadows.cpp`, `ScreenSpaceShadowsResources.h` |

## Pending

| Kind | Upstream | Notes |
|---|---|---|
| Pending | DistantTree's 0.8 SSS strength and forward/alpha receiver coverage | No distant-tree discriminator is identified in deferred material receivers; do not invent one or bind the mask to unreached forward routes. Needs capture proof |
