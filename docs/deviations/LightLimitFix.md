# Light Limit Fix

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | LIGH DATA bits 14/15 and falloff/FOV authoring | External `Lights\*.toml` keyed by plugin-local LIGH/reference IDs | Bits 14/15 are used by TESForm (temporary/visible-distant); no free bits exist |
| Forced | Runtime memory overlay, `ProcessLight` radius before clustering, light lifetime | Radius is enlarged before native culling and restored on removal; t8 metadata follows the t6 append index | FO4 clusters lights for forward consumers; the native tiled list stays authoritative for deferred lighting |
| Forced | Clustered and strict `Lighting.hlsl` light consumers | Native raster (shadow/spot/gobo/attenuation-only) and tiled families call LLF; unflagged lights keep native attenuation and color | Deferred light passes |
| Divergence | Effect lights scaled by 0.5 and `PointLight` pi under Linear Lighting | Effect lights keep FO4's native effect point lane (no 0.5, no pi), with the t34 spot cone term | Both factors are Skyrim calibrations |
| Divergence | Water clustered lights match Skyrim's water lane | Water clustered lights are 3x FO4's native water specular lane, fragment-to-light, replacing it in world draws | Keeps upstream's ratio over the native lane |
| Forced | Spot lights not represented | Spot cones travel in an FO4 sidecar buffer at t34 beside the upstream light record | Skyrim has no spot lights |
| Pending | Shadow-casting lights sample a persistent local shadow copy (`LocalShadowCopyCS`) | Shadow-casting lights light forward passes unshadowed; `localShadowIndex` stays 0 | Not ported; FO4 forward has no shadow-light path |
| Forced | Room index from the owning node's ancestors | Room is the owning fade node's direct parent | Engine association rule |
| Forced | Forward light color from CPU sRGB decode | Forward consumers decode FO4's native power 2.2 in shader | Native light color is `pow(diffuse, 2.2)` |
| Pending | First-person effects lit from their own depth | Lit at background depth | Needs a view-space reconstruction |
| N/A | Particle lights | | FO4 has no particle lights |
| N/A | Effects11 overrides, ACEScg and color-mode cache key | | No FO4 counterpart |
| N/A | Local shadow cache and scheduler | | FO4 has no shadow-caster cap |
| N/A | Surface clustered shading | | Opaque lighting is native tiled |
| N/A | Shadow Mask visualization | | No FO4 shadow mask |
| Forced | CPU-decoded light color, `BRDFScale` 1/pi after `PointLight` | Applied to inverse-square radiance; unflagged color decodes with power 2.2 in the consumer | Native local BRDF has no 1/pi and light color is `pow(diffuse, 2.2)` |
| Forced | 256-entry cluster list | Native 127-index tile appends are guarded so enlarged radii cannot spill into adjacent tiles | Native tile list size |
| Forced | `BSLight_GetLuminance` | Same attenuation on mean preconversion diffuse x dimmer x 4; render-only `currentFade` excluded | |
| Divergence | Lights are inverse-square only when authored | `derive_unauthored_lights` (default off, restart) fits `inverse_square`, `size` and an intensity scale per instance to the native falloff and radius; native radius kept; a TOML entry is authoritative | Gives every light inverse-square falloff without authoring |
| Divergence | Authored radius recomputed from the animated dimmer | Derived lights keep the native radius and the dimmer scales only fade | `LIGH::Update` writes only the dimmer for flicker/pulse; recomputing reach would slide the fade window |
| Pending | CSEditor per-light overrides, hide-regular/hide-ISL diagnostics, Light Placer override API | TOML supplies authored semantics; authoring is restart-only | |

## Upstream bugs

- `lightIndexList` is sized `clusterCount * 128` while `ClusterCullingCS` stores up to 256 indices per cluster.
