# Terrain Shadows

Both `TerrainShadows/ShadowUpdate.cs.hlsl` and `TerrainShadows/TerrainShadows.hlsli` are staged unchanged.
Native DDS dimensions, R16G16_UNORM shadow heights, 128-thread scans, componentwise penumbra maxima,
one-degree softening, half-texel offsets, bounded UV, ZBlur, weight-1 full sweeps and weight-0.5
ordinary slices match the pin. There are no feature settings: the framework `enabled` replaces
`EnableTerrainShadow`. No downsampling setting or resize pass remains; upstream's allocation and scan of
the original heightmap dimensions is an accepted parity cost.

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | Skyrim native directional consumers | Reconstructed BSDFLight, BSDistantTree, BSWater and lit BSEffect directional terms; point/ambient terms stay separate (lit Effect shadows only the directional part) | `DFLight.hlsl`, `DistantTree.hlsl`, `Water.hlsl`, `Effect.hlsl` |
| Fix | Child readiness checks its own editor ID | Readiness checks the resolved inherited-land parent (see the README PR candidates) | `TerrainShadows.cpp` `ResolveWorldspaceEditorId` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| Forced | Skyrim RunGrass shader family | FO4 has no distinct RunGrass target; grass receives the terrain multiplier through the owned deferred directional BSDFLight routes | `DFLight.hlsl` |

## Pending

| Kind | Upstream | Remaining work | Where |
|---|---|---|---|
| Pending | Console/Papyrus GameHour hooks, fast-travel event and completed celestial generation | Wait/sleep/load/interior-exit events request a full refresh and hour-jump polling covers large changes; exact small forward-hour edits and active-light versus Sky transition equivalence still need host hooks | `TerrainShadows.cpp` `PollGameHourJump` |
| Pending | Particle and volumetric sunlight | Native FO4 particles contain only texture × vertex color × ColorScale; upstream reconstructs particle sunlight/ambient. No particle source is shipped, and imagespace supplies SSLR, not volumetric generation | Native Particle family |
| Pending | Reflections and other secondary views | BSLighting stays stock: secondary views need their own camera/depth publication | `SharedData.cpp` |
| Pending | BSDistantTree receiver | Unobserved in capture | `DistantTree.hlsl` |

## Heightmap pack

`package/Textures/Terrain/HeightMaps/` ships upstream-format maps for Commonwealth, SanctuaryHillsWorld,
DiamondCity, DLC03FarHarbor and NukaWorld, built from Fallout4.esm and the official DLC masters (Creation
Club excluded). Goodneighbor and NukaWorldMarket inherit parent land; DiamondCity's parent-use flags omit
land, so it gets its own map. `scripts/generate_terrain_heightmaps.py` writes xLODGen's 1:1 LAND export in
its lossless L16 format, which upstream accepts.
