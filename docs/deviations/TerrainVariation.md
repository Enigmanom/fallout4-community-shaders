# Terrain Variation

`TerrainVariation.hlsli` is staged unchanged through `xmake\shared.lua`. Settings keys, defaults and
tooltips match the shared pin: `enableLODTerrainTilingFix` and `enableMeshSupport`, both on. The GPU block
is the existing `TerrainVariationSettings` layout, published raw; an unpublished block is zero (LOD fix
and mesh variation off).

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | Consumers in `Lighting.hlsl` (`LANDSCAPE`, `LOD_LAND_BLEND`, `LOD_BLENDING` + `LODLANDSCAPE`, landscape-textured meshes) | FO4's world frame is deferred, so the consumers live in the reconstructed BSDFPrePass pixel stage: near terrain `LANDSCAPE`, the LOD blend texture `LANDSCAPE && LAND_LOD_BLEND`, terrain LOD `LOD_BLENDING && LOD_LANDSCAPE && !BONE_TINTING`, meshes `TEXTURE && !LANDSCAPE` outside the Skyrim exclusions (`LOD_LANDSCAPE`, `SKIN_TINT`, `HAIR`, `EYE`, `TREE_ANIM`). Object LOD is rejected with the `IsObjectLODShape` test shared with [LOD Blending](LODBlending.md); decals by `kDecal`/`kDynamicDecal`. The terrain-LOD result is the raw sample, so Skyrim's `Color::Diffuse` has no counterpart | `DFPrepass.hlsl` |
| Forced | `ComputeStochasticOffsets(TexCoord0.zw)` | FO4 has no `TexCoord0.zw`; the equivalent is the prepass `lodAlbedoUV` = layerUV/48 + chunk offset (Skyrim's zw = layerUV/96 + offset), so the lattice is fed `lodAlbedoUV * 0.5` and `WORLD_SCALE` 332.54 keeps its tile-relative lattice with the header unchanged. The coordinate wraps every 4-cell chunk (16384 units), leaving a seam inherited from the engine | `DFPrepass.hlsl` |
| Forced | Six landscape layers; spec/gloss in the normal alpha | FO4 landscape has four layers and a separate per-layer material map (`g_tLandMaterial`), sampled with the same shared offsets as albedo and normal; mesh variation does the same with `g_tMaterial` | `DFPrepass.hlsl` |
| Forced | Per-draw `TVMeshVariation` bit in the permutation `ExtraFeatureDescriptor` | The prepass has no feature descriptor; the host classifies each prepass draw and the answer rides `cb2_pad.y` (carrier described in [LOD Blending](LODBlending.md)). The host bakes eligibility only; the shader applies `enableMeshSupport`, so records baked earlier follow live toggles | `TerrainVariation.cpp`, `ShaderSubclassHooks.cpp` |
| Tweak | Landscape texture means `landscape/<file>` with no further subfolder (upstream #2717) | FO4 stores every terrain texture one folder below `landscape/`, so that rule matches about 7 vanilla materials. Eligible is `landscape/<folder>/...` (after `data/`, `textures/` and an optional `dlc0N/`) with `<folder>` in `rocks`, `dlc06rocks`, `ground`, `trees`, `dirtcliffs`, excluding the `rockriverstones*` atlas stems; roads, piers, `stonewall*`, grass, plants, glowingseaash, rrtracks and sinkhole tile poorly (a road-edge draw darkened to 0.58 albedo ratio) | `TerrainVariation.cpp` |
| Tweak | Non-variation sampling uses `SampleBias(..., MipBias)` | Stock FO4 prepass uses plain `Sample`, so draws without variation keep it; variation samples port unchanged and read `MipBias` as the residual 0 under `SAMPLER_MIP_BIAS` ([Upscaling](Upscaling.md)) because the landscape and mesh sampler slots are anisotropic | `DFPrepass.hlsl` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| Forced | Variation on instanced terrain (`INSTANCED && LANDSCAPE`, `Texture2DArray` layers) | The engine never binds it and `BSDFPrePassShader::CreateCommandBuffer` rejects technique 0x20; consumers are guarded with `!INSTANCED` | `DFPrepass.hlsl` |

## Pending

| Kind | Upstream | Remaining work | Where |
|---|---|---|---|
| Pending | Stochastic RMAOS for `TRUE_PBR` landscape and meshes | FO4 has no TruePBR consumer yet; the FO4 material map already uses the shared offsets | TruePBR slice |
| Pending | Residual seams in admitted textures | Mild cases remain (`rockslab02`, `driedgrass01`, some mud and silt specs, `dirtclifferoded`); admitted by folder and under review | `TerrainVariation.cpp` |
