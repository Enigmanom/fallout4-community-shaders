# Light Limit Fix

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | LIGH DATA bits 14/15 and falloff/FOV authoring | External `Lights\*.toml` keyed by plugin-local LIGH/reference IDs | Bits 14/15 are used by TESForm (temporary/visible-distant); no free bits exist |
| Forced | Runtime memory overlay, `ProcessLight` radius before clustering, light lifetime | Radius is enlarged before native culling and restored on removal; t8 metadata follows the t6 append index | FO4 clusters lights for forward consumers; the native tiled list stays authoritative for deferred lighting |
| Forced | Clustered and strict `Lighting.hlsl` light consumers | Native raster (shadow/spot/gobo/attenuation-only) and tiled families call LLF; unflagged lights keep native attenuation and color | Deferred light passes |
| Forced | Clustered `Effect.hlsl` and `Water.hlsl` lights | Effect adds the native effect lane (no `PointLight` pi, no 0.5) with the t34 cone term; water adds 3x the native specular lane, fragment-to-light; shadow lights are lit unshadowed in both (no depth sampler, unflagged); both use the native curve for unflagged lights and fall back to native lanes when LLF is unhealthy | FO4 forward lanes differ from Skyrim's |
| Forced | CPU-decoded light color, `BRDFScale` 1/pi after `PointLight` | Applied to inverse-square radiance; unflagged color decodes with power 2.2 in the consumer | Native local BRDF has no 1/pi and light color is `pow(diffuse, 2.2)` |
| Forced | 256-entry cluster list | Native 127-index tile appends are guarded so enlarged radii cannot spill into adjacent tiles | Native tile list size |
| Forced | `BSLight_GetLuminance` | Same attenuation on mean preconversion diffuse x dimmer x 4; render-only `currentFade` excluded | |
| Divergence | Lights are inverse-square only when authored | `derive_unauthored_lights` (default off, restart) fits `inverse_square`, `size` and an intensity scale per instance to the native falloff and radius; native radius kept; a TOML entry is authoritative | Gives every light inverse-square falloff without authoring |
| Divergence | Authored radius recomputed from the animated dimmer | Derived lights keep the native radius and the dimmer scales only fade | `LIGH::Update` writes only the dimmer for flicker/pulse; recomputing reach would slide the fade window |
| Pending | CSEditor per-light overrides, hide-regular/hide-ISL diagnostics, Light Placer override API | TOML supplies authored semantics; authoring is restart-only | |
