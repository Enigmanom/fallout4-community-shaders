# Extended Materials

`ExtendedMaterials.hlsli`, `ExtendedMaterialsParallaxCore.hlsli` and `ExtendedMaterialsTerrain.hlsli` are
staged unchanged through `xmake\shared.lua`. Settings keys and defaults match the shared pin:
`EnableParallax`, `EnableHeightBlending` and `EnableParallaxWarpingFix` on, `EnableTerrain` (legacy terrain)
off. The GPU block is the shared `CPMSettings` layout; `EnableComplexMaterial` and `EnableShadows` are
published as 0.

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | Consumers in `Lighting.hlsl` (`PARALLAX`, `LANDSCAPE`) | FO4 is deferred, so the marcher runs in the BSDFPrePass pixel stage before material sampling and offsets albedo, normal and `_s`: objects `TEXTURE && !LANDSCAPE` outside `MODELSPACENORMALS`, `LOD_LANDSCAPE`, `SKIN_TINT`, `HAIR`, `EYE`, `FACE`, `TREE_ANIM`, `GRASS`, `DISMEMBERMENT_MEATCUFF` and the screen variants; terrain `LANDSCAPE && !INSTANCED`. Tessellated variants and OG blends are excluded, since they carry no world position. Object LOD is rejected with `IsObjectLODShape`, as upstream undefines the feature under `LOD`, and the host's answer rides `cb2_pad.z` (carrier in [LOD Blending](LODBlending.md)) | `DFPrepass.hlsl`, `ExtendedMaterials.cpp` |
| Forced | Height in the `_p` parallax slot (objects) and the diffuse alpha (terrain) | FO4 parallax packs store height in the `_s` alpha: objects `g_tMaterial` channel 3, terrain each `g_tLandMaterial[i]` alpha. Objects compile with `PARALLAX` defined, so mips follow the full-resolution height map | `DFPrepass.hlsl` |
| Forced | `viewDirection`, `tbnTr`, `WorldPosition` and `viewPosition.z` from the Lighting VS | The prepass interpolates view-space tangent rows and the world position. Its PS reads `ViewToWorld` and the camera origin from b12 c12-c14 (the rows DFComposite uses) to rebuild the world-space TBN and view vector, so the warping-fix derivatives stay in world space | `DFPrepass.hlsl` |
| Forced | Six landscape layers (`LandBlendWeights1/2`) | FO4 has four. The kernel gets the four weights normalised, as upstream does before the marcher, with layers 5-6 at zero weight. Height-blended weights replace `layerWeights` for every layer sample | `DFPrepass.hlsl` |
| Forced | Terrain Helper displacement layers (`ExtraFeatureDescriptor` `THLand*`) | FO4 has none, so the descriptor is a constant 0 and terrain parallax follows `EnableTerrain` alone | `DFPrepass.hlsl` |
| Forced | `PARALLAX` technique marks parallax materials | No FO4 draw identifies one: the BGSM parser never reads its parallax fields, the BGSM path never builds the Parallax/ParallaxOcc materials, and no prepass bit or constant carries them. The host admits an object only when its `_s` format stores alpha (BC2, BC3, BC7, RGBA8, BGRA8; read from the `TextureHeader` embedded in `BSGraphics::Texture`). In the shader, a height map whose last mip averages alpha 1 (no alpha channel, or flat 255) gets `HeightScale` 0, per object and per landscape layer, so it sits on the reference plane instead of shifting the UV. Upstream's legacy terrain assumes every layer carries height; FO4 landscapes mix height maps with BC1/BC5 layers. Admitted maps keep upstream's mid-grey reference plane, which matches how parallax packs author height (LTO's median alpha is 130) | `ExtendedMaterials.cpp`, `Engine.h`, `DFPrepass.hlsl` |
| Forced | Terrain Variation offsets | The terrain march samples with Terrain Variation's `sharedOffset` (zero when Terrain Variation is off, which reduces to a plain sample), and the stochastic LOD base is taken from the parallax UV as upstream does. Meshes use `meshOffset` under the same `applyMeshTV` gate as their albedo | `DFPrepass.hlsl` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| N/A | `bLandSpecular` INI enable on data load | FO4 landscape always binds a separate `_s` map | — |
| N/A | Point-light parallax soft shadows (`Lighting.hlsl` ~2510-2538) | FO4 lights from the G-buffer; each light needs its own tangent-space direction and height march, which no lighting pass can repeat from G-buffer data | — |
| Pending | Complex Material (`EnableComplexMaterial`): envmask-alpha height and RGB metal/colour | Extension: FO4's envmap loader has no separate mask, and the target packs don't use the convention (LTO `_m` maps are greyscale with alpha 255) | Extension slice |
| Pending | Directional parallax soft shadows (`EnableShadows`, `Lighting.hlsl` ~2314-2336; terrain `cachedDirectionalTerrainParallaxShadow` ~1394) | Extension: compute the sun term once in `DFPrepass` and pass an R8 multiplier to DFLight through the free MRT6 slot (a new cross-pass datum) | Extension slice |
| Pending | Parallax material gate | The `_s` format and its mean alpha stand in for the parallax flag the engine drops, so an `_s` whose alpha holds something other than height still marches. Recovering the BGSM flag would close it | Gap |
| Pending | TruePBR surface POM (`TRUE_PBR` + `HasDisplacement`) | Needs the TruePBR material loader | TruePBR slice |
