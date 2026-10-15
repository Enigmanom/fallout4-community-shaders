# ExponentialHeightFog

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Fog consumers in screen-space composite and forward passes | The six native fog body families (composite, effect, water, LOD water, distant tree) call the upstream fog functions | Native composite and forward fog equations differ per shader |
| Forced | Fog applied to the composed main-view sky | Sky depth excluded from composite fog; the final sky is fogged once after the native forward sky | FO4 draws the sky after the deferred composite |
| Forced | Sunlight attenuation in lighting consumers | Applied to directional BSDFLight families and wet-coat sun lobes; ambient and local light untouched | FO4 stores direct light separately from ambient |
| Forced | `originalFogColorAmount` takes the native fog color | Reconstructed native near/far, low/high color, before sun/grayscale coloration | Native fog bodies evaluate these colors explicitly |
| Forced | Weather profiles keyed by Skyrim form identity | FO4 `Sky` current/last weather and percent select plugin-local profiles | FO4 weather fields and local ID widths |
| Pending | Directional-shadow in-scattering | Not ported; the directional-shadow flag stays unset | Needs shadow SRV lifetime, world-to-shadow transforms and 1/2/3-cascade adaptation |
| Pending | Native weather fog uploader and fade brightness | Native color is mapped at the shader boundary; `respectVanillaFogFade` has no FO4 fade input | The b12 producer from TESWeather fog is not implemented |
| Pending | Analytic Dynamic Cubemaps inscattering | `DYNAMIC_CUBEMAPS` excluded; `useDynamicCubemaps`, tint and mip settings are inert | FO4 cubemap helpers do not match the upstream consumer contract |
| Pending | CloudShadows and clustered local lights | Providers unbound, flags off | Not ported |
| Pending | Weather editor reset actions | Profiles are authored in TOML or the [CS Editor](CSEditor.md) | Not ported |
| Pending | Map, reflection and secondary views; forward first-person fog | Stay stock | Need their own camera/depth publication and a first-person-to-world adapter |
| Pending | Native godray coexistence | Native godrays run later and are not disabled | No coexistence policy |

## Upstream bugs

- `ExponentialHeightFog.cpp` slice count: the denominator is zero when `volumetricFogDistance = volumetricFogStartDistance + 9.5` above the camera near plane (e.g. start 990.5, distance 1000); `VolumetricFogCommon.hlsli` recomputes the distribution, so constrain the settings rather than clamp in C++.
