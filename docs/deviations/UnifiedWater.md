# Unified Water

Optical distance blend and distant terrain water tiles are one feature. Evidence: fallout4-re
`out/water/uw-{hooks,contracts,addlodwater}.md` (OG/NG/AE).

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | `distanceBlendFactor = 1` drives normals, SSR fraction and specular fraction; fog moves to the pixel shader | FO4's `fade` is the same distance term; the override covers detail normals and the surface color blend, and the no-DEPTH alpha keeps the raw fade. FO4 SSLR has no distance term and fog is already per pixel | `Water.hlsl` |
| Forced | Tile UVs from `ObjectUV` scale/offset and `CellTexCoordOffset` | FO4 water UVs come from world XY and `PosAdjust`; `ObjectUV` is a UseVertexTexCoord flag, so tiles keep the native path | `Water.hlsl`, `TerrainWater.cpp` |
| Forced | `WaterMesh.nif` (stream 100) | Converted to FO4 stream 130 with identical vertices, UVs and triangles | `convert-water-mesh.py`, `FO4UnifiedWater.nif` |
| Forced | Tiles registered with `AddWater`, then removed from the water-system array | FO4 draws terrain water only after `AddLODWater` registers the WATER parent for dynamic visibility; bare `AddWater` tiles produced no draw calls (RenderDoc). Tiles clone the Prepare-built property, register through `AddLODWater`, and a call-site hook gives each tile its cell's WATR and height | `TerrainWater.cpp` `RegisterTileWater`, `UnifiedWater.cpp` `Attach` |
| Forced | Per-child `SetAppCulled` for loaded cells | FO4 hides loaded cells by masking segments and dereferences segment data unchecked, so visibility is replaced: segmented siblings keep native masking, tiles are culled per cell | `TerrainWater.cpp` `UpdateVisibility` |
| Forced | Cache workers read plugin records | FO4 safe-file cursors are engine-owned and unsynchronized, so records are read sequentially at DataLoaded; workers only build instructions | `WaterCache.cpp` |
| Forced | Skyrim sky-cell cleanup and map-menu exception | FO4 clears exterior state in `ReleaseLowResMaxHeightData`; the Pip-Boy map renders no exterior LOD | `UnifiedWater.cpp` |
| Forced | Material identity: noise texture names folded into `ComputeCRC32` (vtable slot 4) from `SetWaterShaderMaterialParams` in `InitializeWater` | FO4 `ComputeCRC32` is slot 3 and the material is filled by `TESWaterForm::UpdateWaterMaterial` inside `AddWater`; WATR has three noise textures. Vanilla Commonwealth forms never merge; modded forms differing only in textures would | `TerrainWater.cpp` `InstallWaterMaterialIdentity` |
| Forced | Cubemap reflection flag on tiles | FO4 water has no cubemap technique; tiles keep native reflection flags | `TerrainWater.cpp` `ConfigureTileProperty` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| N/A | World flowmap generation and flowmap normals | FO4 has no flowmap path | — |
| N/A | Tamriel precache | Skyrim-only data for unreadable LAND records | — |
