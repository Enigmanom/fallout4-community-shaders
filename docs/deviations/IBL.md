# Image Based Lighting

Consumed unchanged: `DiffuseIBLCS.hlsl`, `IBL.hlsli` (apart from the Fix in [shared seam edits](README.md#shared-seam-edits)),
the IBL branch of `Common/ShadowSampling.hlsli` and the IBL blocks of `DynamicCubemaps.hlsli` and the
Exponential Height Fog scattering shader. Not staged: `DiffuseIBL.dds` and `SpecIBL.dds`. Settings keys,
defaults and ranges match the pin except the omitted `UseStaticIBL` and `DisableInWorldMap`. Weather
variables are upstream's set as `[features.ImageBasedLighting.weather]` profiles, evaluated once per
frame because b6 packs several times per frame. `EnableIBL` is weather-only (not in the user schema,
default on) and applies only while the framework `enabled` is on; weather never writes `enabled`.

## Translations

| Kind | Upstream | Fallout 4 | Where |
|---|---|---|---|
| Divergence | Always-on feature | Defaults off (`enabled` false); b6 `EnableIBL` mirrors it | `ImageBasedLighting.cpp` |
| Divergence | Each consumer `#undef`s `IBL` without `DYNAMIC_CUBEMAPS` | `Load` fails unless Dynamic Cubemaps is loaded (Effect and DistantTree are not DC targets, so a per-consumer test would disable them) | `ImageBasedLighting.cpp` `Load` |
| Forced | Sky source `cubemapRenderTargets[kREFLECTIONS]`, rendered every frame | The host `SkyReflectionCube` ([Dynamic Cubemaps](DynamicCubemaps.md)) is rendered only while a consumer asks; IBL asks only while enabled outside disabled scenes. A null cube (not yet fresh, or interiors) clears the sky SH to zero, which is neutral | `ImageBasedLighting.cpp`, `SkyReflectionCube.cpp` |
| Forced | `IsDisabledForCurrentScene`: main/loading menu, map menu, `Util::IsInterior` | `MainMenu`/`LoadingMenu` and `engine::IsInterior()` (see [Dynamic Cubemaps](DynamicCubemaps.md)); the scene test feeds b6 `EnableIBL` as upstream does | `ImageBasedLighting.cpp` `IsSceneDisabled` |
| Forced | Lighting replaces `directionalAmbientColor`, then `ApplySkylighting` scales it | `FO4_AMBIENT_VANILLA_DIFFUSE` replaces the vanilla diffuse ambient in `FO4Ambient::Diffuse` for DFLight and the tiled pass before the Skylighting scale. Reflection-direction ambient (`FO4Ambient::Specular`) keeps only the Skylighting scale. The vanilla input is the linear DALC (power 2.2) and the ray is the negated `ViewToWorld` normal | `AmbientConsumer.hlsli`, `DFLight.hlsl`, `DFTiledLighting.hlsl` |
| Forced | Composite SSGI recompose: `GetDiffuseIBLOccluded(vanillaDALC, -N, ...) * albedo`, YCoCg luminance replaced by `Masks.z` | `FO4Ambient::DiffuseAlbedo` ports the branch under `EnableIBL` and takes the luminance from a replay of the lighting ambient (`FO4Ambient::Diffuse` times albedo, same normal, position and vertex AO) because FO4 has no `Masks.z`; it omits only the `maxScale` clamp, which the recompose applies again. Without IBL the recompose is untouched | `AmbientConsumer.hlsli`, `SkylightingConsumer.hlsli` |
| Forced | Specular IBL in `DeferredCompositeCS`: `reflectance * (envSpecular + skySpecular)` | Every composite probe site evaluates the branch in `GetMaterialEnvironment` / `GetIBLProbe` for all four `DALCMode` values. The native composite multiplies the probe by the surface light sum `W` (the family's own multiplier) and has no reflectance channel, so unnormalized radiance terms (sky overlay, mode 0/1 `env * GetIBLRatio() * EnvIBLScale`) are divided by `W` (floored at 1/1024) and the native product restores them. The mode 2/3 environment term takes `W` as its DALC brightness, with the cube normalized to the authored cube's brightness, times `DALCAmount`; the DALC at the reflection vector is not recomputed. A pixel with no weight (sky) returns zero, and a zero channel drops its radiance. `ReflectionNormalisationScale` is unused. With `EnableIBL` off or a missing cube the former path is unchanged | `FO4/DynamicCubemaps/Composite.hlsli`, `DFComposite.hlsl` `FO4_PROBE_WEIGHT` |
| Forced | Fog: `GetFogIBLColor(fogColor)` before Exponential Height Fog in `Lighting`, `Water` and `ISSAOComposite` | `FO4_FOG_VANILLA_COLOR` tints the vanilla fog colour at the six DFComposite fog families, `Water.hlsl` `atmosphere()`, and `Effect.hlsl`/`DistantTree.hlsl` `FogParam`, with or without Exponential Height Fog. The tint applies to the ramp colour, ahead of the sun and grey blends FO4 adds per pixel | `FogConsumer.hlsli`, `DFComposite.hlsl`, `Water.hlsl`, `Effect.hlsl`, `DistantTree.hlsl` |
| Forced | Distant trees: `GetDiffuseIBL(GetAmbient(normal), -normal)` | The forward stage has no world position, so `FO4Forward::WorldPosition` rebuilds it from depth and the normal is the outward `-cross(ddx, ddy)`. Under IBL the vanilla ambient is the linear DALC at that normal; without IBL the stock flat `AmbientColor` is unchanged. Distant-tree DALC is not ported | `ImageBasedLightingConsumer.hlsli`, `DistantTree.hlsl` |
| Divergence | Effect lighting runs for every lit effect | The split stays gated by `FO4_EFFECT_LIGHTING`; IBL keeps upstream's guard that skips the Skylighting ambient scale while IBL is on. Alone it leaves effects at stock and only the fog tint applies | `EffectLightingConsumer.hlsli` |
| Forced | Fog scattering `hasIBL` | Flag bit 4 and CS t76/t77 require IBL healthy and enabled, outside disabled scenes, with every needed source projected this frame (a cleared SH would zero the sky light instead of keeping the fog tint) | `VolumetricFog.cpp` |

## Not supported

| Kind | Upstream | Why | Where |
|---|---|---|---|
| N/A | `DisableInWorldMap`, `isMapMenuOpen` | FO4 has no 3D world-map scene; b5 `InMapMenu` stays 0 | n/a |
| N/A | `UseStaticIBL`, `DiffuseIBL.dds`, `SpecIBL.dds`, `StaticDiffuseIBLTexture`, `ReflectionsPrepass` binds | Out-of-world draws receive no ambient term in FO4 (forward Interface3D routes never read directional ambient; deferred previews run no ambient or sun draw). The b6 field is retained and packed as 0 | n/a |
| N/A | `Water.hlsl` ambient and the underwater refraction fog tint | FO4 water ambient is the sun lobe `lightColor * perMaterial[10].z`, not DALC, and absorption uses material colours | `Water.hlsl` |
| N/A | `Lighting.hlsl`, `Particle.hlsl`, `RunGrass.hlsl`, `ISSAOComposite.hlsl` consumers and the forward `GetDynamicCubemapSpecularIrradiance` IBL block | FO4 emits no forward BSLighting or BSParticleShader passes; grass is drawn in the deferred prepass and the composite fog families and DFLight ambient cover the rest. The custom-consumer guard keeps the forward block out | n/a |

## Pending

| Kind | Upstream | Remaining work | Where |
|---|---|---|---|
| Pending | `EFFECTS11` blocks (`GetDiffuseIBL` sky tilt, volumetric rays IBL) | Effects11 integration | `IBL.hlsli` |
