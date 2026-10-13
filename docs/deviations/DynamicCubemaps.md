# Dynamic Cubemaps

Capture, filter, inference, 16-sample GGX and BC6H are staged unchanged from the shared fork; only the
two generic seams in [shared seam edits](README.md#shared-seam-edits) differ. The lighting-change
detector excludes sky while reflection accumulation includes it. Inference/filter texel addressing and
bilinear capture depth retain the pinned behavior.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Forced | Authored 1×1 sentinel envmaps, Creator export and the Reflectance target select materials for dynamic reflections | Selection uses the water REFLECTIONS route and native envmap materials at the six composite probe sites with FO4 weighting; authored per-material selection is Pending | Native cube-array registration rejects widths below 128 and the G-buffer has no reflectance/F0 channel | `Water.hlsl` `surfaceColor`; `DynamicCubemaps.cpp` `ResolveReflectionMode`; `FO4/DynamicCubemaps/Composite.hlsli` `GetMaterialEnvironment` |
| Forced | Capture before the deferred composite | Capture and publication run after the Forward cloud group | FO4 draws the sky inside `DrawWorld::Forward`, after the composite | `DynamicCubemaps.cpp` `Load` |
| Forced | Capture the main color target | Preparation rebuilds geometry as `3 · albedo · (diffuse A + diffuse B) + emissive`; sky comes from scene color | FO4 has no diffuse-only target | `FO4/DynamicCubemaps/PrepareCaptureCS.hlsl` |
| Forced | Sky depth reconstructs a finite far-plane position | Preparation places depth `1.0` on the camera far-plane direction | FO4's world projection has an infinite far plane (zero homogeneous w at depth 1) | `PrepareCaptureCS.hlsl` |
| Forced | Single world projection and near cutoff 16.5 | Decode the native world partition, exclude first-person pixels, keep 16.5 and bilinear sampling | First-person occupies the near partition | `PrepareCaptureCS.hlsl`, `FO4/Depth.hlsli` |
| Tweak | `activeReflections` from Skyrim's reflections prepass | Exterior water always uses the reflections variant | Retains main's exterior-water policy | `DynamicCubemaps.cpp` `ResolveReflectionMode` |
| Forced | Active variant infers uncaptured directions from the engine reflection cube (`kREFLECTIONS`, rendered every frame by Skyrim) | The host renders the sky into the engine cube and, once all six faces are fresh, the unchanged `UpdateReflections`/`InferReflections` read it; interiors, hidden sky or an unavailable provider keep the scene-sky fallback | FO4 never writes the engine cube on vanilla data | `DynamicCubemaps.cpp` `Load`, `ResolveReflectionMode`, `Inference` |
| Forced | Skyrim renders `kREFLECTIONS` natively each frame | `SkyReflectionCube` clicks two faces per frame (full cube every third frame) with the LOD roots and sky root, clearing to `pow(Sky-Lower, 2.2)`, from the previous world camera origin; skipped without a loaded world, in interiors and while the sky is hidden. IBL is a second consumer | The engine draws the cube only for water reflections with a rendered cube, which vanilla data never enables; contract in fallout4-re `docs\reflection-cube-capture-contract.md` | `Render/SkyReflectionCube.cpp`, `Render/RenderHooks.cpp` |
| Forced | Each cube face is culled against its own frustum; the forward Lighting shader draws LOD land and objects | Faces run with pre-culling temp-disabled; the LOD land/object flags are cleared around the clicks, so LOD draws with the base forward Lighting technique (an approximation of Skyrim's LOD shading) | `BSCuller::Add` accepts main-camera pre-selected objects without per-face rejection, and FO4 ships no forward Lighting permutation for LOD bases 8, 9, 13, 18, 19 | `Render/SkyReflectionCube.cpp` |
| Forced | Water blends the dynamic cube with `CubeMapTex` | Blends with water's sky-gradient reflection color | FO4 reconstructed reflection permutations shade a sky gradient | `Water.hlsl` `surfaceColor` |
| Forced | `Util::IsInterior()`: interior cell, or a worldspace with `kNoSky` or `kFixedDimensions` | `engine::IsInterior()`: interior cell, `kNoSky` worldspace, or `Sky::mode != kFull`; `kFixedDimensions` is not tested. Shared with Skylighting and IBL; interiors with full sky still count as interior | FO4 sets `kFixedDimensions` on open-sky worldspaces (Diamond City, Nuka-World Amphitheater and Market), so the verbatim port treated Diamond City as interior (no sky or reflection cubes, Skylighting skipped). No FO4 worldspace sets `kNoSky`; `Sky::mode` is the engine's own sky-rendered state | `World/Sky.cpp`, `SharedData.cpp`, `DynamicCubemaps.cpp` |
| Forced | Sentinel materials sample the dynamic cube with upstream normalization | Native envmap pixels replace the authored cube with the live cube normalized by its average luminance, rescaled to the authored cube's average and clamped to the authored LDR range; LOD is `(1 − gloss) × 8`; the reflection vector is flipped back to `reflect(-V, N)`; desaturation, SSLR blend, gloss/strength weighting and ambient modulation are unchanged. `material_reflections` (default 1) blends authored to dynamic, scaled by `1 −` native rain (b12[30].y); a null cube keeps the authored sample | The native composite multiplies this pattern by surface light, and the G-buffer stores gloss, not roughness | `FO4/DynamicCubemaps/Composite.hlsli` `GetMaterialEnvironment`; `DFComposite.hlsl` |
| Tweak | Loaded DC defines ENABLESSR, labeled for water | Live `enabled_ssr` gates all SSLR when DC is enabled; baseline stays stock | Main's global toggle avoids recompilation; SSLR feeds surfaces and water | `DynamicCubemaps.cpp`, `ISSSLRRaytracing.hlsl` |

## Not supported

| Kind | Upstream | Why | Where |
|---|---|---|---|
| Forced | Forward `Lighting.hlsl` sentinel path | FO4 world and first-person accumulators emit no forward BSLighting passes | Native Lighting family |

## Pending

| Kind | Upstream behavior | Remaining work | Where |
|---|---|---|---|
| Pending | Authored per-material selection and TruePBR/complex material reflectance (extension slice) | Needs an FO4 reflectance/selection contract and authored assets | `DFComposite.hlsl` |
| Pending | Material authoring/export workflow replacing the Creator sentinel DDS export | Define the workflow after choosing the FO4 selection contract; exporting 1×1 sentinel cubes alone cannot select native deferred materials | `DynamicCubemaps.cpp` |

`skylightingSpecular` is recorded in [Skylighting](Skylighting.md) and the IBL specular branch in [IBL](IBL.md).

## Upstream PR candidates

- `InferCubemapCS.hlsl:22`, `SpecularIrradianceCS.hlsl` `GetSamplingVector`: integer texel/extent
  addresses cube edges rather than centers. Retained exactly; candidate fix is adding 0.5.
- `CaptureCommon.hlsli` `SampleCapture`: bilinear depth can blend foreground/sky or projection
  partitions into a nonexistent surface. Retained; evaluate point depth or geometry-aware sampling.
