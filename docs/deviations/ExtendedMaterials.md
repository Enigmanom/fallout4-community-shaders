# Extended Materials

`ExtendedMaterials.hlsli`, `ExtendedMaterialsParallaxCore.hlsli` and `ExtendedMaterialsTerrain.hlsli` are
staged unchanged through `xmake\shared.lua`. Settings keys and defaults match the shared pin:
`EnableParallax`, `EnableHeightBlending` and `EnableParallaxWarpingFix` on, `EnableTerrain` (legacy terrain)
off. The GPU block is the shared `CPMSettings` layout; `EnableComplexMaterial` is published
as 0.

## Translations

| Kind | Upstream | Fallout 4 / evidence | Where |
|---|---|---|---|
| Forced | Consumers in `Lighting.hlsl` (`PARALLAX`, `LANDSCAPE`) | FO4 is deferred, so the marcher runs in the BSDFPrePass pixel stage before material sampling and offsets albedo, normal and `_s`: objects `TEXTURE && !LANDSCAPE` outside `MODELSPACENORMALS`, `LOD_LANDSCAPE`, `SKIN_TINT`, `HAIR`, `EYE`, `FACE`, `TREE_ANIM`, `GRASS`, `DISMEMBERMENT_MEATCUFF` and the screen variants; terrain `LANDSCAPE && !INSTANCED`. Tessellated variants and OG blends are excluded, since they carry no world position. Object LOD is rejected with `IsObjectLODShape`, as upstream undefines the feature under `LOD`, and the host's answer rides `cb2_pad.z` (carrier in [LOD Blending](LODBlending.md)) | `DFPrepass.hlsl`, `ExtendedMaterials.cpp` |
| Forced | Height in the `_p` parallax slot (objects) and the diffuse alpha (terrain) | FO4 parallax packs store height in the `_s` alpha: objects `g_tMaterial` channel 3, terrain each `g_tLandMaterial[i]` alpha. Like upstream's alpha-packed heights (complex material, terrain), objects compile without `PARALLAX`, so the march samples one mip coarser than the colour map and skips the `_s` grain | `DFPrepass.hlsl` |
| Forced | `viewDirection`, `tbnTr`, `WorldPosition` and `viewPosition.z` from the Lighting VS | The prepass interpolates view-space tangent rows and the world position. Its PS reads `ViewToWorld` and the camera origin from b12 c12-c14 (the rows DFComposite uses) to rebuild the world-space TBN and view vector, so the warping-fix derivatives stay in world space | `DFPrepass.hlsl` |
| Forced | Six landscape layers (`LandBlendWeights1/2`) | FO4 has four. The kernel gets the four weights normalised, as upstream does before the marcher, with layers 5-6 at zero weight. Height-blended weights replace `layerWeights` for every layer sample | `DFPrepass.hlsl` |
| Forced | Terrain Helper displacement layers (`ExtraFeatureDescriptor` `THLand*`) | FO4 has none, so the descriptor is a constant 0 and terrain parallax follows `EnableTerrain` alone | `DFPrepass.hlsl` |
| Forced | `PARALLAX` technique marks parallax materials | No FO4 draw identifies one: the BGSM parser never reads its parallax fields, the BGSM path never builds the Parallax/ParallaxOcc materials, and no prepass bit or constant carries them. The host admits an object only when its `_s` format stores alpha (BC2, BC3, BC7, RGBA8, BGRA8). In the shader, a height map whose last mip averages alpha 1 (no alpha channel, or flat 255) gets `HeightScale` 0, per object and per landscape layer, so it sits on the reference plane instead of shifting the UV. Upstream's legacy terrain assumes every layer carries height; FO4 landscapes mix height maps with BC1/BC5 layers. Admitted maps keep upstream's mid-grey reference plane | `ExtendedMaterials.cpp`, `Engine.h`, `DFPrepass.hlsl` |
| Forced | Terrain Variation offsets | The terrain march samples with Terrain Variation's `sharedOffset` (zero when Terrain Variation is off, which reduces to a plain sample), and the stochastic LOD base is taken from the parallax UV as upstream does. Meshes use `meshOffset` under the same `applyMeshTV` gate as their albedo | `DFPrepass.hlsl` |
| Tweak | Relief depth of 0.1 UV (`maxHeight`) | FO4 landscape spans twice Skyrim's world units per UV, so every admitted height map gets `HeightScale` 0.5 to keep Skyrim's relief depth; objects take the same factor | `DFPrepass.hlsl` |
| Divergence | Parallax soft shadows evaluated inside the forward lighting pass (`EnableShadows`) | FO4 lights from the G-buffer, so the prepass evaluates upstream's directional multiplier once per pixel (objects `GetParallaxSoftShadowMultiplier`, terrain `EvaluateTerrainDirectionalParallaxShadowMultiplier`) and writes it to `SV_Target6`, an EM-owned R8 target; every prepass draw writes it so overdraw cannot leave stale shadow. DFLight multiplies the sun's shadow by it at t66. The terrain sun-angle gate uses the geometric normal because the G-buffer normal is not built yet | `DFPrepass.hlsl`, `DFLight.hlsl`, `ExtendedMaterials.cpp` |

## Not supported

| Kind | Upstream | Evidence | Where |
|---|---|---|---|
| N/A | `bLandSpecular` INI enable on data load | FO4 landscape always binds a separate `_s` map | — |
| N/A | Point-light parallax soft shadows (`Lighting.hlsl` ~2510-2538) | FO4 lights from the G-buffer; each light needs its own tangent-space direction and height march, which no lighting pass can repeat from G-buffer data | — |
| Pending | Complex Material (`EnableComplexMaterial`): envmask-alpha height and RGB metal/colour | Extension: FO4's envmap loader has no separate mask, and the target packs don't use the convention | Extension slice |
| Pending | Parallax material gate | The `_s` format and its mean alpha stand in for the parallax flag the engine drops, so an `_s` whose alpha holds something other than height still marches. Recovering the BGSM flag would close it | Gap |
| Pending | TruePBR surface POM (`TRUE_PBR` + `HasDisplacement`) | Needs the TruePBR material loader | TruePBR slice |
