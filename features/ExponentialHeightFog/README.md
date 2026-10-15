# Exponential Height Fog

Set `load = true`, configure the relevant native shader ownership and restart.
The framework `enabled` key beside `load` (default `false`) toggles the effect live;
`volumetricFogEnabled` selects the volume pipeline. RGBA colors use four-element TOML arrays.

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
switch above 0.5. Profiles accept only the 30 weather variables, not grid/history
controls. Fog density is authored independently of the native near/far ramps.

Unported upstream functionality is listed in `docs\deviations\ExponentialHeightFog.md`.
