# Cloud Shadows

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Pending | Per-layer occlusion cubes and `CloudSelfShadowTexture` (t26) for cloud self-shadowing | One composite occlusion cube; cloud layers do not shadow each other | Only Effects11 consumes the layers, and t26 is Screen Space GI's |
| Forced | Occlusion refreshes with the engine's reflection cube render | The host cube render refreshes one polar and one side face per frame, so each side face updates every fourth frame | FO4 renders no reflection cube natively |
| Pending | Effect and particle sun term shadowed by clouds | Compiled out | FO4 Effect lighting has no sun term; needs an extension |
| N/A | Legacy volumetric-lighting consumer | | FO4 godrays are native GFSDK |
| N/A | `BSParticleShader` receiver | | FO4 never draws the particle shader |
