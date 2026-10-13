# InverseSquareLighting

Upstream #2825 merged ISL into Light Limit Fix; the feature keeps its name here. `GetAttenuation` lives in
the shared seam `LightLimitFix/Attenuation.hlsli`. It, `LightLimitFix/Common.hlsli`, `Common/Game.hlsli`
and `Common/Color.hlsli` are staged unchanged. The global replacement, global settings, comparison view and
b7 ISL block are deleted. CPU radius/luminance math matches the pin, including its zero-radius edge case
(see the README PR candidates).

The larger light footprint is an accepted parity cost: upstream `ProcessLight` applies intensity times four
and cutoffs 0.022 (shadow) / 0.05 before LLF clustering uses the calculated radius. `Lighting.hlsl`
rejects ISL attenuation below `1e-5`; restoring the old `0.001` cutoff would truncate upstream lighting.
FO4 publishes the radius before its native culling instead of LLF clustering.

## Translations

Engine evidence refers to fallout4-re `docs\engine-facts.md`, **Local lights**, and its OG/NG/AE Address
Library table.

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | LIGH DATA bits 14/15 and falloff/FOV authoring | External `Lights\*.toml` identifies plugin-local LIGH/reference IDs and keeps inverse-square/linear/cutoff/size semantics. DATA 0x4000 is nonshadow spot and 0x8000 disables specular; TESForm bits 14/15 are temporary/visible-distant, so no free bits exist | `LightAuthoring.{h,cpp}` |
| Forced | Skyrim runtime memory overlay, LLF `ProcessLight` radius before clustering, light lifetime and light-buffer publication | A typed NiLight sidecar keeps preconversion diffuse, dimmer, authored cutoff/size and original radius. Hooks (CreatePointLight `GenDynamic {30546,2198256,2198256}` / `AddLight {1109421,2317457,2317457}`; Update `{1022957,2198261,2198261}`; TestFrustumCull `{1440624,2318414,2318414}`; RemoveLight `{162205,2200909,2200909}` / `{1410391,2317464,2317464}`; tiled callback `{999390,2317525,2317525}`, AddLight `{1250844,2318542,2318542}`, UpdateStructuredBuffer `{402301,2276904,2276904}`) update all radius channels before native list admission, write t8 at the exact t6 dense append index, and restore the original radius on removal without defeating native sole-owner pruning | `InverseSquareLighting.cpp`, `LocalLights.cpp` |
| Forced | Lighting.hlsl clustered and strict light consumers | BSDFLight SetupGeometry `{976849,2319150,2319150}` and SetupPointLightGeometry `{212931,2319153,2319153}` supply b11 for all native local raster families (shadow/spot/gobo/attenuation-only). Tiled and raster call unchanged ISL and upstream PointLight color; unflagged lights keep native radial/color arithmetic and directional light is unchanged | `FO4/InverseSquareLighting{Consumer,Raster}.hlsli`, `DFLight.hlsl` |
| Forced | LightingEval `VanillaNormalization` 1/pi after `PointLight` | Native local BRDF (DFLight/DFTiledLighting) has no 1/pi, so the consumer applies it to ISL radiance; `lightGamma` 2.2 mirrors the native `pow(diffuse, 2.2)` light color | `FO4/InverseSquareLightingConsumer.hlsli`, `SharedData.cpp` |
| Forced | Upstream 256-entry cluster list | Native 127-index tile appends are guarded when ISL is contributed, so expanded radii cannot write into adjacent tiles; stock compilation stays byte-identical without the feature | `DFTiledLighting/TileCullCS.hlsli` |
| Forced | Skyrim `BSLight_GetLuminance` | FO4 `{170662,2318428,2318428}` calls the same attenuation with mean preconversion diffuse × dimmer × 4, keeping ignored/disabled light exclusions, shape masks and the cached luminance write, but not render-only currentFade | `LocalLights.cpp` `ShapeAttenuation` |
| Divergence | Lights are ISL only when authored | `derive_unauthored_lights` (default on, restart) derives `inverse_square`, `size` and an intensity scale per instance for every light with no TOML entry, from the native falloff `pow(1 - saturate(a + b*x^c), 2.2)`, native radius and `TESObjectREFR::GetFade`. The scale equals an author retuning LIGH fade; size matches ISL's half-intensity distance to the native curve (clamped 0.01..<50); gain matches the d²-weighted energy over [0, R]; derived lights keep the native radius. Kept native: DATA 0x4 negated color, b == 0, nonpositive native energy, radius <= 0.1. A TOML entry is authoritative and never merged. Reference XRDS and XLIG/ExtraLightData change radius and fade per reference, so the fit is per instance | `LightDerivation.{h,cpp}`, `LocalLights.cpp` |
| Divergence | Authored lights recompute radius from the animated dimmer every refresh | Derived lights keep the native radius fixed and the dimmer scales only `fade`; authored lights keep the upstream recompute. `TESObjectLIGH::Update` writes only `NiLight.dimmer` for flicker/pulse, so recomputing reach would slide the smoothstep fade window and over-pulse derived lights | `InverseSquareLightingData.cpp` |

## Pending

| Kind | Upstream | Notes |
|---|---|---|
| Pending | CSEditor live per-light overrides, hide-regular/hide-ISL diagnostics and Light Placer runtime override publication | TOML supplies authored base/reference semantics, not Skyrim's interactive editor or external placement API; authoring is restart-only |
