# LOD Blending

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Consumers in `Lighting.hlsl` and `RunGrass.hlsl` | Consumers live in the `DFPrepass` pixel stage: object LOD, terrain LOD, near terrain and grass | FO4's world frame is deferred |
| Forced | Vertex color normalized before lighting reads it | Prepass input vertex color is rewritten for landscape and grass, so every later consumer (including SSGI vertex AO) sees it | |
| Forced | Object LOD selected by `LODOBJECTS`/`LODOBJECTSHD` technique defines | Host classifies each draw: non-`kLODLandscape` shapes under the land LOD root are object LOD | The prepass has no LOD technique, and engine markers miss some BTO shapes |
| Forced | `LODObjectSnowBrightness`/`LODObjectSnowGamma`, projected-UV snow branch | Omitted from settings and UI | FO4 prepass has no `PROJECTED_UV` and no BSLightingShader world pass |
