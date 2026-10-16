# Dynamic Cubemaps

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Authored 1x1 sentinel envmaps, Creator export and the Reflectance target select dynamic-reflection materials | Selection uses the water REFLECTIONS route and native envmap materials at the composite probe sites; authored per-material selection is Pending | Native cube-array registration rejects widths below 128 and the G-buffer has no reflectance/F0 channel |
| Forced | Capture before the deferred composite | Capture runs after the Forward cloud group | FO4 draws the sky after the composite |
| Forced | Capture the main color target | Geometry rebuilt as `3 * albedo * (diffuse A + diffuse B) + emissive`; sky from scene color | FO4 has no diffuse-only target |
| Forced | Sky depth reconstructs a finite far-plane position | Sky depth 1.0 is placed on the far-plane direction | FO4's world projection has an infinite far plane |
| Forced | Single world projection | Native world depth partition decoded; first-person pixels excluded | First person occupies the near partition |
| Forced | Inference reads the engine reflection cube, rendered every frame | The host renders the sky into the engine cube (two faces per frame, sky root, previous camera origin); interiors, hidden sky or no fresh cube fall back to scene sky. IBL is a second consumer | Vanilla FO4 never writes the engine cube |
| Forced | Engine cube holds LOD land, objects and trees plus the sky | Sky root only | Cube mode 21 (LOD only) draws nothing for FO4 LOD; full-pipeline LOD costs ~1 ms CPU per frame (~390 draws) |
| Forced | Water blends the dynamic cube with the engine cube `CubeMapTex` by distance | Water samples the dynamic cube at every distance; without a valid cube it keeps its stock sky-gradient reflection, and reflections weigh by Fresnel alone | The engine cube has no LOD scenery, so beyond 1024 units it reflects only sky |
| Forced | Sentinel materials sample the dynamic cube | Native envmap pixels use the live cube, luminance-normalized to the authored cube; LOD is `(1 - gloss) * 8`; `material_reflections` (default 1) blends authored to dynamic, scaled by `1 -` rain | The native composite multiplies this pattern by surface light, and the G-buffer stores gloss, not roughness |
| Divergence | `CreateCubemapRenderTarget_Reflections` and `CreateDepthStencil_Reflections` shrink the engine cube and its depth to 256 | Only the cube color target is 256; depth 7 stays 512 | Depth 7 is shared with Interface3D container and workbench |
| Divergence | `activeReflections` from Skyrim's reflections prepass | Exterior water always uses the reflections variant | Fixed exterior-water policy |
| Divergence | Loaded DC defines `ENABLESSR` for water | Live `enabled_ssr` gates all SSLR while DC is enabled | Avoids recompilation; SSLR feeds surfaces and water |
| N/A | Forward `Lighting.hlsl` sentinel path | | FO4 world and first-person emit no forward BSLighting passes |
| Pending | Authored per-material selection and TruePBR/complex material reflectance | Needs an FO4 reflectance/selection contract and authored assets | |
| Pending | Material authoring/export workflow replacing the Creator sentinel DDS export | Needs the selection contract first; 1x1 sentinel cubes cannot select native deferred materials | |

## Upstream bugs

- `InferCubemapCS.hlsl`, `SpecularIrradianceCS.hlsl` `GetSamplingVector`: integer texel addressing hits cube edges instead of centers; add 0.5.
- `CaptureCommon.hlsli` `SampleCapture`: bilinear depth can blend foreground/sky or projection partitions into a nonexistent surface.
