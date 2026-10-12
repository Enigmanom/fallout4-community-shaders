# Exponential Height Fog

Uses the unchanged analytic and four-pass volumetric shaders from the shared pin (upstream
`488e408a9`), including the second fog layer, volumetric noise and the near/far volume split.
This replaces the native-ramp extinction approximation.

Set `load = true`, configure the relevant native shader ownership and restart.
`enabled` toggles the effect live; `volumetricFogEnabled` selects the volume pipeline.
Generated defaults contain the upstream settings and ranges. RGBA colors use four-element
TOML arrays. The Fog Factor debug view and telemetry remain available.
Remove legacy `density_multiplier` and `height_falloff_multiplier` overrides; they have
no equivalent in the upstream model. `enabled` is an upstream integer toggle (`0` or `1`).

## Weather profiles

Profiles live in the user configuration's `features.ExponentialHeightFog.weather` map.
Use a plugin-local weather form ID, not its load-order-prefixed runtime ID:

```toml
[features.ExponentialHeightFog.weather."0x123~Example.esp"]
__enabled = true
fogDensity = 0.02
fogHeight = 500.0
fogInscatteringColor = [0.2, 0.3, 0.4, 1.0]
volumetricFogEnabled = 1
```

Restart after editing profiles. Omitted variables fall back to user settings.
Float and RGBA values interpolate through FO4's weather transition; integer toggles
switch above 0.5. Profiles accept only the upstream 30 weather variables, not grid/history
controls. Fog density is independently authored; native near/far ramps are not fitted
into an extinction model.

## Current limits

Directional shadow scattering, analytic cubemap inscattering, CloudShadows and
clustered local-light providers are pending. Terrain shadow scattering uses the existing
validated heightfield when available, and sky-light scattering uses the Skylighting probe
array and the Image Based Lighting spherical harmonics when those features are healthy.
Native fog color is supplied by the reconstructed shader equations. The native weather uploader,
fade brightness, sky-color mappings, map/reflection coverage and native godray coexistence are
pending; see `docs\deviations\ExponentialHeightFog.md`.

