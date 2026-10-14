# ExponentialHeightFog

All seven `ExponentialHeightFog` shaders plus their Random, Color, Shading, IBL and Skylighting
includes are staged unchanged; the ramp-derived FO4 kernel is deleted. The near volume ends at
`volumetricNearGridDistance`; a coarser far volume (`VOLUMETRIC_FOG_FAR_GRID`) continues to the view
distance and is sampled at t22 beside the near t19.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Forced | Screen-space composite and transparent fog consumers | Replace the six reconstructed native fog body families once; effect alpha/add/multiply blends, water surface/LOD and distant trees call unchanged upstream fog functions | Native composite and forward fog equations differ per shader | `FO4/FogConsumer.hlsli`, `FO4/ExponentialHeightFogConsumer.hlsli` |
| Forced | Fog sees the already-composed main-view sky | Exclude sky depth from composite EHF; fog the final MainTemp sky once after the native forward sky, preserving geometry and inactive pixels | FO4's sky group follows the deferred composite, and per-layer affine fog cannot preserve additive sun/stars or mask composition | `VolumetricFog.cpp` `PrepareSky`, `CompositeSky`; `FO4/ExponentialHeightFog/SkyCompositeCS.hlsl` |
| Forced | Sunlight attenuation in upstream lighting consumers | Attenuate all directional BSDFLight families and separate wet-coat sun lobes; ambient/local light and independent shadow alpha untouched | FO4 stores direct light separately from ambient and surface composition | `DFLight.hlsl`, forward consumers, lit Effect |
| Forced | Native fog color passed to `originalFogColorAmount` | Pass the reconstructed near/far, low/high native color before sun/grayscale coloration | The six native composite bodies evaluate these colors explicitly | `DFComposite.hlsl`, water/effect/tree consumers |
| Forced | Skyrim weather form identity/transition | FO4 `Sky.currentWeather`, `lastWeather`, `currentWeatherPct` select plugin-local weather profiles; the TOML weather open-map uses upstream's 30 variable names | Typed CommonLibF4 `Sky` fields; light and ordinary plugins use 12-bit and 24-bit local IDs | `ExponentialHeightFog.cpp`, `World/Weather.cpp` |

## Pending

| Kind | Upstream | Notes |
|---|---|---|
| Pending | Directional-shadow in-scattering | Shadow SRV lifetime, world-to-shadow transforms and 1/2/3-cascade adaptation are missing; the directional-shadow flag stays unset and no screen-space mask substitutes for a froxel shadow |
| Pending | Native weather fog uploader and fade brightness | The b12 producer fed from TESWeather fog is not implemented. Native color is mapped at the shader boundary; `respectVanillaFogFade` is persisted but has no FO4 fade input; sky original-fog color has no per-frame source |
| Pending | Analytic DynamicCubemaps input | FO4 cubemap helpers are not the converted upstream consumer contract. The fog include excludes `DYNAMIC_CUBEMAPS`; `useDynamicCubemaps`, tint and mip settings keep upstream values |
| Pending | CloudShadows and clustered local lights | Absent providers stay unbound and their flags off. Connected: terrain's t60 provider, the Skylighting probe array (t50, flag bit 8, only while Skylighting is healthy) and the [IBL](IBL.md) SH views (t76/t77, flag bit 4, while IBL is enabled and projected). `useSkyIBL` is a schema key (default 1) |
| Pending | Weather editor/reset and full weather-system lifecycle | Profiles are authored in TOML or the [CS Editor](CSEditor.md); upstream's editor reset actions are not ported. Registry reset defaults for density, original color and vanilla suppression differ from Settings defaults; user-setting fallback uses the latter |
| Pending | Map/reflection/secondary-view and forward first-person coverage | The three BSLighting families stay stock: secondary views need their own camera/depth publication. Forward raw near depth keeps native fog until a first-person-to-world camera adapter exists |
| Pending | Native godray coexistence | Native NVIDIA godrays run later and are not the upstream four-pass volume pipeline; no coexistence policy exists and they are not silently disabled |

## Upstream PR candidates

- `src/Features/ExponentialHeightFog.cpp:653–668,709–718`: weather-variable reset defaults
  (`originalFogColorAmount=1`, `fogDensity=0.02`, `disableVanillaFog=false`) disagree with
  `ExponentialHeightFog.h` Settings defaults (`0`, `0.005`, `true`). Confirm intent upstream.
- `src/Features/ExponentialHeightFog.cpp:457–465`: the slice denominator is zero when
  `volumetricFogDistance = volumetricFogStartDistance + 9.5` above the camera near plane
  (for example, start 990.5 and distance 1000, both accepted settings). Clamp the far
  boundary beyond the offset near plane. FO4 retains the formula and rejects nonfinite dispatch data.
