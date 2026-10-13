# WaterEffects

`WaterCaustics.hlsli` and `watercaustics.dds` are staged unchanged through `xmake/shared.lua`; the FO4
caustics kernel and CPU shader mirror are deleted. There are no caustics quality knobs. Water parallax
reads height that water texture mods ship in the normal alpha; vanilla alpha-less normals stay flat
(upstream #2837/#2838, in the pin). The consumer passes `perMaterial[9]` as the `normalsAmplitude`
argument. FLOWMAP variants have no FO4 flowmap path.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Forced | Skyrim cell lookup and water form | Loaded FO4 `GridCellArray` cells populate the eye-centred 5×5, 4096-unit b5 grid; `GetExteriorWaterHeight` resolves inherited heights; `GetWaterType` supplies averaged shallow/deep RGB times the sky water multiplier | FO4 has no reference/player-height query | `src/World/Water.cpp`, `SharedData.cpp` |
| Forced | RGB caustics multiply directional light colour in `Lighting.hlsl` | RGB direct diffuse/specular in all directional BSDFLight families; shadow-only RGB retains independent alpha; ambient/local light is unaffected | FO4 accumulates direct light separately in RGB targets | `DFLight.hlsl` |
| Tweak | Pinned `State.cpp` leaves `WaterSystemHeight` absent | Publish the camera cell's resolved plane relative to b4, or -FLT_MAX; not a water-mesh intersection query | Simplified data source | `Water.cpp` `FillWaterData` |
| Forced | `NormalsScale` in the water pixel shader | The vertex shader forwards its b1 `NormalsScale` as upstream's `TEXCOORD8`; BSWater VS and PS are replaced as a pair | The FO4 water PS has no `NormalsScale` constant, and a host-added interpolant cannot pair with a native stage | `Water.hlsl` |
| Tweak | Upstream applies one offset in its shaded-surface normal path | `surfaceNormal` computes the offset once per entry point for SPECULAR, surface, UNDERWATER and the SSLR ray passes (which rebuild the normal); LOD, VC, STENCIL and FOG skip it | The FO4-only SSLR ray passes must trace the normal that is shaded | `Water.hlsl` |
| Divergence | Parallax has no setting upstream | The framework `enabled` switch gates parallax with caustics through b7 `EnabledWaterParallax`; off skips the march and the offset is zero | Master switch | `WaterEffects.cpp`, `FO4SharedData.hlsli` |

## Not supported

| Kind | Upstream | Why | Where |
|---|---|---|---|
| Forced | WaterParallax FLOWMAP variants | Reconstructed FO4 `Water.hlsl` has no FLOWMAP path or flow-field input | `Water.hlsl` |
| Forced | Interior cell water height | FO4's exterior-height accessor rejects interior cells; the table keeps -FLT_MAX rather than borrowing the player's cached plane. Placed-water mesh intersections are outside the cell-height approximation | `Water.cpp` `CellWaterData` |

## Pending

| Kind | Upstream | Notes |
|---|---|---|
| Pending | Secondary mode-0/21 BSLighting caustics | World and first-person accumulators issue no BSLighting passes, so their caustics are deferred; secondary-view BSLighting execution and camera/resource lifetime are not integrated. A missing adapter is not an engine limitation |
