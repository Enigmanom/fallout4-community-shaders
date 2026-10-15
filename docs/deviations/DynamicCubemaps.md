# Dynamic Cubemaps

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Authored 1x1 sentinel envmaps, Creator export and the Reflectance target select dynamic-reflection materials | Selection uses the water REFLECTIONS route and native envmap materials at the composite probe sites; authored per-material selection is Pending | Native cube-array registration rejects widths below 128 and the G-buffer has no reflectance/F0 channel |
| Forced | Capture before the deferred composite | Capture runs after the Forward cloud group | FO4 draws the sky after the composite |
| Forced | Capture the main color target | Geometry rebuilt as `3 * albedo * (diffuse A + diffuse B) + emissive`; sky from scene color | FO4 has no diffuse-only target |
| Forced | Sky depth reconstructs a finite far-plane position | Sky depth 1.0 is placed on the far-plane direction | FO4's world projection has an infinite far plane |
| Forced | Single world projection | Native world depth partition decoded; first-person pixels excluded | First person occupies the near partition |
| Forced | Inference reads the engine reflection cube, rendered every frame | The host renders the sky into the engine cube (two faces per frame, LOD and sky roots, previous camera origin); interiors, hidden sky or no fresh cube fall back to scene sky. IBL is a second consumer | Vanilla FO4 never writes the engine cube |
| Forced | LOD land and objects drawn with their forward Lighting shaders | LOD draws with the base forward Lighting technique, an approximation of Skyrim's LOD shading | FO4 ships no forward Lighting permutation for LOD bases, and pre-culling skips per-face rejection |
| Forced | Water blends the dynamic cube with `CubeMapTex` | Water blends with its sky-gradient reflection color | FO4 water reflection permutations shade a sky gradient |
| Forced | Sentinel materials sample the dynamic cube | Native envmap pixels use the live cube, luminance-normalized to the authored cube; LOD is `(1 - gloss) * 8`; `material_reflections` (default 1) blends authored to dynamic, scaled by `1 -` rain | The native composite multiplies this pattern by surface light, and the G-buffer stores gloss, not roughness |
| Divergence | `activeReflections` from Skyrim's reflections prepass | Exterior water always uses the reflections variant | Fixed exterior-water policy |
| Divergence | Loaded DC defines `ENABLESSR` for water | Live `enabled_ssr` gates all SSLR while DC is enabled | Avoids recompilation; SSLR feeds surfaces and water |
| N/A | Forward `Lighting.hlsl` sentinel path | | FO4 world and first-person emit no forward BSLighting passes |
| Pending | Authored per-material selection and TruePBR/complex material reflectance | Needs an FO4 reflectance/selection contract and authored assets | |
| Pending | Material authoring/export workflow replacing the Creator sentinel DDS export | Needs the selection contract first; 1x1 sentinel cubes cannot select native deferred materials | |

## Upstream bugs

- `InferCubemapCS.hlsl`, `SpecularIrradianceCS.hlsl` `GetSamplingVector`: integer texel addressing hits cube edges instead of centers; add 0.5.
- `CaptureCommon.hlsli` `SampleCapture`: bilinear depth can blend foreground/sky or projection partitions into a nonexistent surface.
