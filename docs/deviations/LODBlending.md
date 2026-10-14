# LOD Blending

Settings keys, defaults and ranges match the pin: `LODTerrainBrightness`, `LODObjectBrightness`
(0.01-5), `LODTerrainGamma`, `LODObjectGamma` (0.1-3) and `DisableTerrainVertexColors`. The GPU block
defaults to 1.0 so an unpublished block is neutral. DistantTree has no upstream consumer.

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | Consumers in `Lighting.hlsl` (`LODOBJECTS`/`LODOBJECTSHD`, `LODLANDSCAPE`, `LANDSCAPE`, `LOD_LAND_BLEND`) and `RunGrass.hlsl` | FO4's world frame is deferred, so the consumers live in the reconstructed BSDFPrePass pixel stage: object LOD behind the gate below, terrain LOD `LOD_LANDSCAPE && !BONE_TINTING`, near terrain `LANDSCAPE` (+ `LAND_LOD_BLEND`), grass `GRASS` | `DFPrepass.hlsl` |
| Forced | `input.Color.xyz = 1`, else landscape max-channel normalization, before lighting reads vertex color | Rewrites the prepass input vertex color for `LANDSCAPE` and `GRASS` (normalization on `LANDSCAPE` only) so every later consumer, including the SSGI vertex-AO term, sees it | `DFPrepass.hlsl` |
| Forced | Object-LOD gate: compile-time `LODOBJECTS` / `LODOBJECTSHD` technique defines | FO4's prepass has no LOD technique or descriptor bit, and the engine markers miss BTO (`kLODObjects` is set on most BTO shapes but not all). The host classifies each draw: BTO shapes are the non-`kLODLandscape` shaders of geometry under `Main::GetLandLODRoot()`; terrain LOD under the same node has `kLODLandscape`. `Main::GetObjectLODRoot()` holds near-cell objects, not object LOD | `cs::engine::IsObjectLODShape` (`src/Render/ObjectLOD.cpp`), `LODBlending.cpp` `ClassifyObjectLOD` |
| Forced | Gate carrier | The result reaches the shader through the PS cb2 pad lane `cb2_pad.x` (a float4 shared by prepass features via `RegisterPrepassDrawClassifier`; Terrain Variation owns `.y`, Extended Materials `.z`). It is written at `BSDFPrePassShader::CreateCommandBuffer` (1285447 / 2318501) and `BSShader::BuildCommandBuffer` (833764 / 2318870) for command-buffer draws, and for immediate draws in the mapped level-2 constants: NG/AE (2317224) at the engine's map, OG (1.10.163) in `FlushConstantGroup` (1515598, RVA 0x2801E40) before the unmap. Landscape draws are skipped (that register is `land_material_gate`); a failed install fails the feature load, so the gate stays neutral | `ShaderSubclassHooks.cpp`, `DFPrepass.hlsl` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| Forced | `LODObjectSnowBrightness` / `LODObjectSnowGamma` and the projected-UV snow branch | FO4 BSDFPrePassShader has no `PROJECTED_UV` macro and the world frame has no BSLightingShader pass. The settings are omitted from the schema and UI; the ABI fields stay at 1.0 | `LODBlendingSettings.h`, `SharedFeatureData.h` |
