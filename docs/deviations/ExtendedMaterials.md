# Extended Materials

`ExtendedMaterials.hlsli`, `ExtendedMaterialsParallaxCore.hlsli` and `ExtendedMaterialsTerrain.hlsli` are
staged unchanged through `xmake\shared.lua`. Settings keys and defaults match the shared pin:
`EnableParallax`, `EnableHeightBlending` and `EnableParallaxWarpingFix` on, `EnableTerrain` (legacy terrain)
off. The GPU block is the shared `CPMSettings` layout; `EnableComplexMaterial` and `EnableShadows` are
published as 0.

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | Consumers in `Lighting.hlsl` (`PARALLAX`, `LANDSCAPE`) | FO4 is deferred, so the marcher runs in the BSDFPrePass pixel stage before material sampling and offsets albedo, normal and `_s`: objects `TEXTURE && !LANDSCAPE` outside `MODELSPACENORMALS`, `LOD_LANDSCAPE`, `SKIN_TINT`, `HAIR`, `EYE`, `FACE`, `TREE_ANIM`, `GRASS`, `DISMEMBERMENT_MEATCUFF` and the screen variants; terrain `LANDSCAPE && !INSTANCED`. Tessellated variants and OG blends are excluded, since they carry no world position. Object LOD is rejected with `IsObjectLODShape`, as upstream undefines the feature under `LOD`, and the answer rides `cb2_pad.z` (carrier in [LOD Blending](LODBlending.md)) | `DFPrepass.hlsl`, `ExtendedMaterials.cpp` |
| Forced | Height in the `_p` parallax slot (objects) and the diffuse alpha (terrain) | FO4 parallax packs store height in the `_s` alpha: objects `g_tMaterial` channel 3, terrain each `g_tLandMaterial[i]` alpha. Objects compile with `PARALLAX` defined, so mips follow the full-resolution height map | `DFPrepass.hlsl` |
| Forced | `viewDirection`, `tbnTr`, `WorldPosition` and `viewPosition.z` from the Lighting VS | The prepass interpolates view-space tangent rows and the world position. Its PS reads `ViewToWorld` and the camera origin from b12 c12-c14 (the rows DFComposite uses) to rebuild the world-space TBN and view vector, so the warping-fix derivatives stay in world space | `DFPrepass.hlsl` |
| Forced | Six landscape layers (`LandBlendWeights1/2`) | FO4 has four. The kernel gets the four weights normalised, as upstream does before the marcher, with layers 5-6 at zero weight. Height-blended weights replace `layerWeights` for every layer sample | `DFPrepass.hlsl` |
| Forced | Terrain Helper displacement layers (`ExtraFeatureDescriptor` `THLand*`) | FO4 has none, so the descriptor is a constant 0 and terrain parallax follows `EnableTerrain` alone | `DFPrepass.hlsl` |
| Forced | `PARALLAX` technique marks parallax materials | No FO4 draw identifies one. The BGSM parser never reads its parallax fields, the BGSM path never builds the Parallax/ParallaxOcc materials, and no prepass bit or constant carries them. So every eligible object marches, and flat height must produce no offset. FO4 packs author alpha 255 as the surface; `DisplacementOffset` -0.5 puts it on the kernel's reference plane, where the secant returns no offset (BC5 `_s` samples alpha 1 too). Relief therefore only recedes, and a map authored with mid-grey as the surface sits half the range low | `DFPrepass.hlsl` |
| Forced | Terrain Variation offsets | The terrain march samples with Terrain Variation's `sharedOffset` (zero when Terrain Variation is off, which reduces to a plain sample), and the stochastic LOD base is taken from the parallax UV as upstream does. Meshes use `meshOffset` under the same `applyMeshTV` gate as their albedo | `DFPrepass.hlsl` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| N/A | `bLandSpecular` INI enable on data load | FO4 landscape always binds a separate `_s` map | — |
| N/A | Point-light parallax soft shadows (`Lighting.hlsl` ~2510-2538) | FO4 lights from the G-buffer; each light needs its own tangent-space direction and height march, which no lighting pass can repeat from G-buffer data | — |
| Pending | Complex Material (`EnableComplexMaterial`): envmask-alpha height and RGB metal/colour | Extension: FO4's envmap loader has no separate mask, and the target packs don't use the convention (LTO `_m` maps are greyscale with alpha 255) | Extension slice |
| Pending | Directional parallax soft shadows (`EnableShadows`, `Lighting.hlsl` ~2314-2336; terrain `cachedDirectionalTerrainParallaxShadow` ~1394) | Extension: compute the sun term once in `DFPrepass` and pass an R8 multiplier to DFLight through the free MRT6 slot (a new cross-pass datum) | Extension slice |
| Pending | Parallax material gate | Ungated marching costs a full march (24 height taps for a flat map) on every eligible object pixel, flat or not. Closing it needs a host classifier that recovers the BGSM parallax flag the engine drops | Gap |
| Pending | TruePBR surface POM (`TRUE_PBR` + `HasDisplacement`) | Needs the TruePBR material loader | TruePBR slice |
