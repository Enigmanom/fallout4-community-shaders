# Unified Water

Specification: `extern/community-shaders-shared` at
`53c0c1b14b314fcfd3bb04b7f3f332247c568e40`,
`src/Features/UnifiedWater.{h,cpp}`, `src/Features/UnifiedWater/WaterCache.{h,cpp}`,
and `package/Shaders/Water.hlsl`. Optical blending and terrain tiles are one activation.
Flowmaps and the material CRC fix are outside this port's agreed scope.

## Engine evidence

Read-only FO4 RE: `fallout4-re/out/water/uw-hooks.md` and `uw-contracts.md`,
recorded by the user in engine-facts commit `ba6856f0a`. The contracts independently
inspect OG 1.10.163, NG 1.10.984 and AE 1.11.240. Static contracts do not establish
live rendering, performance, or placed-water pixel attribution.

| Kind | Upstream | Fallout 4 translation / boundary | Code |
|---|---|---|---|
| Forced | `distanceBlendFactor = 1` for unified normal/specular distance response; raw `distanceMul` retained | FO4's raw `fade` is the same distance expression. Override only normal/detail and surface optical color blending; retain no-DEPTH alpha fade. Its SSLR color fraction has no corresponding distance term. Fog already evaluates per pixel, so no VS-to-PS fog move exists | `package/Shaders/Water.hlsl` |
| Forced | Skyrim ObjectUV scale/offset and CellTexCoordOffset | FO4 has a scalar UseVertexTexCoord at property flags `0x100`; transformed world XY plus native PosAdjust supplies normal UVs. Tiles retain the native world-UV path. No Skyrim flowmap/wading constant upload is added | `TerrainWater.cpp` `ConfigureTileProperty`; `Water.hlsl` native VS |
| Forced | Skyrim stream-100 WaterMesh.nif | Deterministic stream-130 conversion preserves all four position/UV vertices and both triangles byte-for-byte. Translate triangle-count width, full-precision descriptor, node effects, header export string and particle suffix; canonicalize stream-130 data length. No vanilla mesh substitution | `scripts/convert-water-mesh.py`; `package/Meshes/Water/FO4UnifiedWater.nif` |
| Forced | Water objects removed from the water-system array; orphaned normal/reflection helpers queued | FO4 recursive lock at system+`0x118`, retained object/helper arrays and `NiObjectGarbage::Add` use the verified FO4 contracts. Queue admission is conditional and destruction is not promised to happen off the render thread | `src/Render/TerrainWater.cpp` |
| Forced | Terrain Attach/Detach and ordinary per-cell culling | FO4 terrain chunks replace only BTR WATER children and preserve normal multibound parenting. Validate both six-byte native gates before changing either; ordinary tiles cannot use the native unchecked segment traversal. Fully replace visibility at its original thread/timing, including nullable borrowed CELLs, loaded+active predicate, segmented siblings, draw refresh, and interior/range enable policy | `TerrainWater.{h,cpp}`; `UnifiedWater.cpp` |
| Forced | Worker-side duplicated TESFile readers | FO4 safe files are engine-owned cached cursors with unsynchronized registry access. Snapshot records sequentially at DataLoaded before streaming; workers only merge/write instructions and use immutable already-resolved WATR pointers. Retain snapshots even on cache hits so the debug regenerate button never performs native reads from UI/workers | `WaterCache.{h,cpp}` |
| Forced | Skyrim record and form APIs | Use winning CELL XCLW/XCWT, reverse LAND search beginning at that winner, exact subrecord sizes and the owning file's AddCompileIndex master resolution. Preserve upstream unquantized VHGT float accumulation, not FO4's integer-quantized native LAND path. No Tamriel developer precache is supplied for FO4 | `TerrainWater.cpp` `ReadCellData` |
| Forced | Worldspace/sky-cell cleanup, displacement callback and exterior-rendering MapMenu exception | FO4 SetWorldSpace and ReleaseLowResMaxHeightData have the verified typed signatures; the latter has no Skyrim sky-cell-unload tail. Native displacement updates refresh root culling every frame, including after LoadingMenu closure; do not mutate scenegraphs from the host's UI-thread menu callback. The Pip-Boy map has no Skyrim exterior-LOD MapMenu rendering path | `UnifiedWater.cpp` |
| Forced | Force Skyrim cubemap water property flag | FO4's native water archive has no CUBEMAP technique selection equivalent. Preserve FO4 reflection/refraction/depth flags while clearing terrain LOD; do not request an absent native technique | `TerrainWater.cpp` `ConfigureTileProperty`; `uw-hooks.md` water property/shader evidence |
| Framework | Global feature, JSON/menu state and ImGui progress | Central load=false registration, TOML activation, frozen shader contributions/ownership validation, forwarding-only DearModdingUI, runtime callback quarantine and cached telemetry | feature registration; `UnifiedWater.{h,cpp}` |
| Framework | Skyrim plugin cache directory and Windows helper/PCH | Plugin-owned FO4 directory; preserve WTCH/32-byte instruction layout, load-order/version FNV-1a and permissive hash-file retries. Reuse upstream WinApi declaration and port its unchanged P-core algorithm alone because its unrelated version reader assumes a Unicode PCH | `WaterCache.cpp`; `src/Utils/ProcessorTopology.cpp`; `xmake/shared.lua` |

## Portable parity inventory

- Optical normal consumers: surface, underwater, specular, SSLR surface and underwater rays.
- Cache selection: parent water inheritance, default heights/forms, invalid-height rejection,
  missing CELL/LAND rejection, exact height/form/flags merge barriers.
- Tiles: greedy square growth, LOD4 individual cells, LOD8/16/32 chunk/map bounds,
  negative-coordinate floor division, retained runtime cache snapshots.
- Scheduling: upstream P-core worker-count arithmetic, per-world worker generation,
  atomic progress/failure reporting, live regeneration publication.
- Lifetime: native Prepare precedes replacement; tiles clone the pinned mesh and initialize
  native water materials; no-instruction chunks use vanilla geometry/registration;
  owned detach restores the retained original children.
- Scope: neither gate intercepts AddWaterRefToWaterSystem. Thicket REFR `0x106E9D`
  uses its own full-LOD sky-cell Water1024 reference path, not terrain WATER.

## Validation status

Critical tests exercise coverage and merge barriers across negative/map/chunk boundaries,
LOD4 cell independence, uniform LOD8 merging and corrupt/truncated cache headers.
The complete repository suite, including AE and OG injected shader variants, passes.

An owned AE feature-off launch loaded the approved fixture (native load operation 1,
completed cursor 11, success=true, followed by LoadingMenu closure/player readiness).
The deployed CS build was `c41721c875ad133538eda74e7ef5a068a5a84a66+dirty`,
DLL SHA-256 `A725EFC18DB0A3D3DDB1A00C4E3ED1771F7F32B86AEF185C4ED6DD0ED187DADD`.
Papyrus SetPosition completed with the reflected Float/Float/Float → None signature.

Validation is blocked: DevBench reported an empty capture-extension list. Its only native
fallback explicitly marked both captures `visualRegressionInconclusive`; the requested
free-camera poses also did not match the rendered water-target views. The captures are
retained as failed/inconclusive test evidence, not off/on comparisons or water attribution.
The owned game was stopped gracefully; no game/shim remains and a fresh RootBuilder
`Clear complete!` followed this launch at `2026-10-13 09:17:57.674`.

Evidence: `bethesda-devbench/build/runtime-validation/controller/unified-water/uw-off-cs.log`,
native PNGs under `unified-water/adhoc/default/`, and downscaled JPGs in the original
brief's session `files/shots/`. Controller boot evidence:
`controller/2026-10-13T09-15-58-217Z-session-2/result.json`.

Feature-on execution, off/on continuity, tile UVs, streaming/interior round trips and distant
Thicket pixels remain unverified. Static scope preservation is not a visual validation result.
No performance claim has been made.
