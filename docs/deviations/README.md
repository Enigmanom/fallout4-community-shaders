# Deviations from upstream

Upstream [Skyrim Community Shaders][upstream] is the specification for ported features (see `AGENTS.md`).
These files record only differences that change a feature: rendered or computed output, user-visible
behavior or defaults, settings semantics, shader math or ABI, and upstream PR candidates. Framework
plumbing (activation, persistence, UI, ownership gates, telemetry) is not recorded; engine addresses and
captures live in fallout4-re. Cross-cutting records are here, one file per feature below.

Each row has a Kind:

- **Forced**: a Fallout 4 engine difference leaves no alternative. Cite the engine evidence.
- **Fix**: a deliberate correction of an upstream bug, listed as an upstream PR candidate.
- **Tweak**: a minor FO4 adjustment under rule 2; never affects classification.
- **Divergence**: FO4's own architecture/algorithm or substantive behavior upstream lacks (rule 3).
- **Pending**: upstream behavior not ported yet; it does not establish parity.
- **N/A**: upstream behavior with no Fallout 4 counterpart.

Core/extension classification and its rules live in [FEATURES](../FEATURES.md#classification); it judges
design per slice, not by counting row Kinds.

## Shared seam edits

Shared pin: the `extern\community-shaders-shared` submodule commit; upstream base `dc75702c7`.
FO4 consumes unchanged files through `xmake\shared.lua`; shared include paths cannot be replaced.

| Kind | File | SHA | Why | Upstream PR status |
|---|---|---|---|---|
| Tweak | `src/Features/PerformanceOverlay.h`, `src/Features/PerformanceOverlay/{CircularBuffer,DrawCallRow}.h`, `src/Features/PerformanceOverlay/ABTesting/ABTestAggregator.{h,cpp}` | `6fd72a4a5` | Split portable history/timing rows from the Skyrim feature header | In the shared fork; PR candidate, not filed |
| Forced | `package/Shaders/Common/FrameBuffer.hlsli` | `e305ed0a4` | `FRAMEBUFFER_REGISTER` defaults to b12 and permits host binding at b4; FO4 engine shaders already bind the native per-frame buffer at b12 (`Water.hlsl`) | In the shared fork; no upstream PR recorded |
| Fix | `features/IBL/Shaders/IBL/IBL.hlsli` | `c766bc3d` | `GetIBLColorOccluded` read the environment SH in every `DALCMode`, though modes 2 and 3 never project it; it now delegates to `GetDiffuseIBLOccluded` with the vanilla DALC at the ray | community-shaders/skyrim-community-shaders#2882 |
| Forced | `features/Dynamic Cubemaps/Shaders/DynamicCubemaps/CaptureCommon.hlsli` | `83efe1ad9` | Optional prepared position/color/UV inputs, geometry/sky tags and capture origin isolate FO4's +Z, partitioned depth, infinite far plane and diffuse reconstruction; Skyrim defaults unchanged | In the shared fork; PR candidate, not filed |
| Forced | `features/Dynamic Cubemaps/Shaders/DynamicCubemaps/DynamicCubemaps.hlsli` | `83efe1ad9` | Optional cube registers/custom-consumer guard and extracted explicit-sampler normalization let native FO4 deferred slots/samplers call shared arithmetic; Skyrim defaults unchanged | In the shared fork; PR candidate, not filed |
| Fix | `features/Screen Space GI/Shaders/ScreenSpaceGI/{upsample,radianceDisocc}.cs.hlsl` | `54a2705b9` | Specular reads/writes gated by `GI_SPECULAR`; see [Screen Space GI](ScreenSpaceGI.md) | PR pending |
| Fix | `features/Screen Space Shadows/Shaders/ScreenSpaceShadows/ScreenSpaceShadows.hlsli` | `6f81ebc25` | Samples the pixel-centered `SV_POSITION` directly instead of adding 0.5 before integer conversion; see [Screen Space Shadows](ScreenSpaceShadows.md) | PR candidate, not filed |

## Shader replacement

Source paths follow the native fxp name. Effect and DistantTree stay owned for upstream fog and
terrain-shadow consumers (Effect also owns the Skylighting diffuse term, and IBL tints their fog).
Utility, Sky, Particle, BloodSplatter and Lighting stay stock because no current feature consumes them.

| Kind | Upstream | Fallout 4 translation | Why | Code |
|---|---|---|---|---|
| Tweak | Refraction uses the offset sample | Under `FO4CS_SUBSTRATE` the shore blend falls back to the mask-accepted sample when the refracted texel lies above the water plane | Stock reads the unmasked offset texel, so first-person hands and weapons fringe into water | `Water.hlsl` `refractedScene` |

## Shader contribution

| Kind | Translation | Why | Code |
|---|---|---|---|
| Forced | FO4 deferred prepass/light/composite/tiled families replace Skyrim forward Lighting consumers | Native `DFPrepass`, `DFLight`, `DFComposite` and `DFTiledLighting`; FO4 world draws emit no forward BSLighting passes | Per-feature `ShaderDefines.h` |
| Forced | Substrate stays at VS b4–b7; VS b10/b11 are never used | The engine rewrites VS b10/b11 mid-frame | `SharedData.cpp`, `SubstrateSlots.h` |

Samplers are bound per draw because the engine dirties all PS sampler slots and the native families do not declare these:

| Consumer | Slot / required mode |
|---|---|
| Fog composite | s13, linear clamp |
| Fog water/effect/distant tree | s15, linear clamp |
| Terrain light/composite/water/effect/distant tree | s13, linear clamp |
| Water caustics, light | s14, linear wrap |

## Effect lighting

Upstream: `Effect.hlsl` `GetLightingColor` / `GetLightingShadow` and the staged
`Common/ShadowSampling.hlsli`. `FO4/EffectLightingConsumer.hlsli` ports both under upstream's defines;
`Effect.hlsl` keeps FO4's native point lights. The directional part of `DLightColor` takes the
world/terrain shadow and the height-fog sun attenuation; the ambient part takes the Skylighting
diffuse visibility, as upstream. Effect lighting, ambient and sun are linear (b6 `effectLightingMult` is 1).

| Kind | Upstream | Fallout 4 translation and why | Code |
|---|---|---|---|
| Forced | `input.WorldPosition` | Camera-relative position rebuilt from `SV_POSITION` and the b4 inverse projection; first-person pixels use scene depth. Sun fog attenuation skips first person. FO4 `BSEffect` PS has no world-position input | `ForwardPosition.hlsli`, `EffectLightingConsumer.hlsli` |
| Forced | `HasDirectionalShadows` is `!IsInterior() \|\| IsActiveInteriorSun()` | Published as `!InInterior`; Interior Sun is not ported | `SharedData.cpp` |
| Forced | Skyrim point-light term | FO4's four-light term stays; `PLightingRadiusInverseSquared`, `Color::PointLight` and `EffectLightingMult` are not applied (FO4 `PerGeometry` layout) | `Effect.hlsl` |
| Forced | Falloff volume guard `VC && TEXCOORD && NORMALS && TEXTURE && FALLOFF && SOFT` | `TEXTURE` implies `TEXCOORD`; `GRAYSCALE_TO_ALPHA` is a compile define; soft depth is decoded with `FO4Depth::ProjectionDepth` | `EffectLightingConsumer.hlsli` |
| Pending | `ExtraFlags::InWorld` | No per-draw carrier, so every lit Effect draw counts as in-world; menu and Interface3D effects are not separated and the non-world `LinearToSrgb` is not ported | `EffectLightingConsumer.hlsli` |
| Pending | `ExtraFlags::SuppressExternalEmittance` | Compiles out; nothing carries `kExternalEmittance` per draw, and FO4 interior `DLightColor` is zero on lit effects | `EffectLightingConsumer.hlsli` |
| Pending | `Permutation::EffectRadius` | Zero, so `Get3DFilteredShadow` takes one sample at the pixel | `EffectLightingConsumer.hlsli` |
| Pending | `VOLUMETRIC_SHADOWS`, `CLOUD_SHADOWS`, `LIGHT_LIMIT_FIX`, Interior Sun | Features not ported; their blocks compile out. The IBL guard on the Skylighting ambient scale is kept ([IBL](IBL.md)) | `EffectLightingConsumer.hlsli` |

## Feature enable switch

| Kind | Upstream | Fallout 4 | Code |
|---|---|---|---|
| Divergence | Each feature has its own switch (`Enabled`, `Enable`, `EnableIBL`, `EnableTerrainShadow`) or none; the diagnostic RemoteControl bridge flips `Feature::loaded`, which stops CPU work while compiled shaders keep reading stale data. PR candidate | One framework `enabled` switch zeroes the feature's b6/b7 block so shaders take the vanilla path and its effect work stops. Upscaling, FrameGeneration, RenderDoc, PerformanceOverlay and InverseSquareLighting opt out | `Feature.h`, `FeatureBuffer.cpp` |

## Substrate

FrameBuffer, SharedData, SphericalHarmonics and its Math dependency are staged byte-for-byte. The pinned
b6 ABI contains **20** blocks, all mirrored in upstream order; absent features leave zero blocks, except
the host's neutral linear-color policy in the LinearLighting block.

| Kind | Difference | Why | Where |
|---|---|---|---|
| Forced | FrameBuffer binds at b4, not upstream's b12 | FO4 reconstructed shaders own b12 (`Water.hlsl` native PerFrame); b4/b7 are unused by injection targets | `SubstrateSlots.h` |
| Forced | One R32_FLOAT pass publishes canonical world-projection depth at t17; near pixels reproject through shadow +0x8A0, world pixels use `mad(d,1.01,-0.01)`, sky uses 1 | FO4 combines first-person and world projections; native targets use t0–t15 | `CanonicalDepth.cpp`, `FO4/CanonicalDepthCS.hlsl`, `FO4/Depth.hlsli` |
| Tweak | Upstream's scalar X clamp offset and Y clamp-to-ratio are preserved; OG publishes ratio 1 | OG deferred, water and tiled-lighting shaders read no dynamic-resolution constants | `SharedData.cpp`, `Common/FrameBuffer.hlsli` |
| Forced | World-channel DALC is packed into pre-power SH, then FO4's 2.2 power is applied once at linear consumer boundaries. SH is `(b/Y00,−ay/Y1,az/Y1,−ax/Y1)`, Y00=0.2820948, Y1=0.4886025 | Native lighting evaluates power 2.2; upstream `GetAmbient` is pre-power | `Engine.h`, `SharedDataLayout.h` `PackAmbientSH`, `FO4/FO4ShaderData.hlsli` |
| Forced | b6 publishes neutral linear-lighting inputs (`enableLinearLighting=1`, `colorGamma=1`, unit multipliers, DALC power 2.2, linear sun) | FO4 deferred HDR/sun inputs are linear; unchanged Color/cubemap kernels then see identity conversions | `SharedData.cpp` `PackFeatures` |
| Divergence | One 64-byte FO4-only b7 holds fullscreen debug owner/mode/params, `DeltaTime` and the enable flags `EnabledSSR`, `EnabledDynamicCubemaps`, `EnabledWaterParallax`, `EnabledTerrainVariation`, `EnabledSkylighting`, `EnabledScreenSpaceGI`; one host PS t61 serves the selected debug texture. Upstream b6 `cubemapCreatorSettings` keeps Creator semantics and stays zero | Single-substrate ABI; water tiles use b5, DR and NDC-to-view use b4, terrain enable fields use b6 | `SharedDataLayout.h`, `FO4/FO4SharedData.hlsli`, `SharedData.cpp` |
| Pending | FrameParams is zero; celestial/HDR/map/shadow fields keep upstream's absent values. SunDirection is toward-light; SunColor stays absent | No FO4 inverse-gamma/frame-flag or celestial/HDR source is consumed | `SharedData.cpp`, `SharedDataLayout.h` |

## Upstream PR candidates

- `src/Features/InverseSquareLighting.cpp:76–95`: balanced intensity/cutoff/size
  (`intensity=1`, `cutoff=0.5`, `size=2`) gives radius zero rather than NaN, so `ProcessLight` divides
  by zero for `invRadius`/`fadeZone` and gameplay smoothstep has a zero denominator. Clamp the radius
  to a documented positive minimum. FO4 retains the zero-radius behavior.
- `src/Utils/PerfUtils.h:41` and `ABTestAggregator.cpp:91`: implicit `size_t` to float raises C4267
  under `/W4 /WX`; use an explicit float conversion. FO4 scopes the warning in `src/Shared/PerfUtils.h`.
- `features/Screen Space GI/Shaders/ScreenSpaceGI/gi.cs.hlsl:232`: the experimental specular
  half-angle calculation has inconsistent angular units. Upstream #2792 (adopted at `bff82b03e`)
  corrects only the blur half-angle; the pinned behavior remains.
- `src/Deferred.cpp:362–364`: experimental HQ specular binds null diffuse Y/CoCg SRVs although
  `DeferredCompositeCS.hlsl:46–56` still samples them. FO4 retains that binding; settle the intended
  diffuse/HQ combination upstream.
- `package/Shaders/Common/FrameBuffer.hlsli:53,119`: clamp helpers bound X to a texel-edge clamp but Y to
  the raw ratio. An axis-specific clamp could avoid bottom-edge reads on reduced-resolution allocations;
  a candidate for investigation, not a proven defect.
- `package/Shaders/Common/SharedData.hlsli:423–430`: `GetWaterData` assumes coordinates become positive
  after adding 64 cells; `modf` returns a negative fraction west/south of -262144 units, and at an exact
  cell border `round` can select adjacent tiles by ties-to-even. Use `floor(absolutePosition / 4096)`
  minus `floor(CameraPosAdjust.xy / 4096)` plus 2.
- `src/Features/TerrainShadows.cpp:332–336,365–380`: readiness compares the child worldspace editor ID,
  but loading resolves inherited land to the parent. Resolve the same land identity in both paths. FO4
  applies the parent-readiness correction; no upstream PR recorded.
- `Lighting.hlsl` declares `sharedOffset` only under `EMAT`, so `LANDSCAPE` with `TERRAIN_VARIATION` and
  without `EMAT` does not compile. FO4 consumers declare their own offsets; not patched locally, the fork
  PR is tracked separately.
- `src/Features/RemoteControl/DevBenchBridge.cpp`, `capture` (`renderdoc`): add a `device` option and
  bracket the capture with `StartFrameCapture`/`EndFrameCapture` on that device. A global trigger misses
  frames presented through another API ([Remote Control](RemoteControl.md)).
- Same tool: return a `requestId` and emit `communityshaders.capture.completed` with the file `path` and
  `bytes` once RenderDoc has written it.
- Same file, `feature toggle`: flip a per-feature enable switch instead of `Feature::loaded`
  ([Feature enable switch](#feature-enable-switch)).

## Features

- [InverseSquareLighting](InverseSquareLighting.md)
- [WaterEffects](WaterEffects.md)
- [ExponentialHeightFog](ExponentialHeightFog.md)
- [Performance Overlay](PerformanceOverlay.md)
- [Terrain Shadows](TerrainShadows.md)
- [LOD Blending](LODBlending.md)
- [Terrain Variation](TerrainVariation.md)
- [Dynamic Cubemaps](DynamicCubemaps.md)
- [Image Based Lighting](IBL.md)
- [RenderDoc](RenderDoc.md)
- [Remote Control](RemoteControl.md)
- [Upscaling](Upscaling.md)
- [FrameGeneration](FrameGeneration.md)
- [MotionVectorFixes](MotionVectorFixes.md)
- [Screen Space GI](ScreenSpaceGI.md)
- [Screen Space Shadows](ScreenSpaceShadows.md)
- [Skylighting](Skylighting.md)

[upstream]: https://github.com/community-shaders/skyrim-community-shaders
