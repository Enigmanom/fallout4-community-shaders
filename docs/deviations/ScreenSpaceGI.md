# Screen Space GI

The entire upstream `features/Screen Space GI/Shaders/ScreenSpaceGI` directory is staged unchanged
through `xmake\shared.lua`; all local XeGTAO copies are deleted. The blur correction is the single SSGI
entry in [shared seam edits](README.md#shared-seam-edits). Settings, full/half/quarter modes, formats,
noise, pass order, AO/GI/accumulation ping-pong, SH/YCoCg, blur and upsample use the pinned contracts.
The unchanged `readHistory` uses the current `RCP_OUT_FRAME_DIM`; main's reverted previous-extent
correction (`e9d16c20`) is not reintroduced. The upstream #2792 blur half-angle correction is adopted.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Forced | Compose in DeferredCompositeCS | Compose diffuse in the native 2D accumulator, 2D fog and cube IBL families; cube specular samples the same SH/HQ data before native material weighting | FO4 composes separate diffuse/specular accumulators rather than MainRW/reflectance; the cube equation applies `gloss * material.z² * 50` | `DFComposite.hlsl`, `FO4/ScreenSpaceGIConsumer.hlsli` |
| Forced | Current shaded diffuse radiance | Prepare `3 · albedo · (logical 33 + logical 35 if tiled) + emissive`, encoded at the unchanged RadianceToLinear input | Native targets hold unshaded linear lighting, not `forwardRenderTargets[0]` | `FO4/ScreenSpaceGI/Prepare.cs.hlsl` |
| Forced | G-buffer normal/glossiness input | Decode the native spherical view normal into unchanged `GBuffer::EncodeNormal`; full-resolution RG16F XY without `GI_SPECULAR`, RGBA16F with material.x glossiness in Z when enabled | `DFPrepass.hlsl` uses spherical rather than octahedral encoding; only `GI_SPECULAR` consumes Z | `ScreenSpaceGI.cpp`, `Prepare.cs.hlsl` |
| Forced | Prepared diffuse sampling footprint | Full-sized R11G11B10F storage when GI is enabled; quarter-resolution Prepare writes only the bilinear middle 2x2 of each 4x4 block and physical clamp edges; GI-off variants allocate no diffuse | `radianceDisocc.cs.hlsl` is the sole reader; sampled values are unchanged | `ScreenSpaceGI.cpp`, `Prepare.cs.hlsl` |
| Fix | Unconditional specular upsample and disocclusion I/O | Shared fork gates `upsample.cs.hlsl` and `radianceDisocc.cs.hlsl` specular reads/writes with `GI_SPECULAR`; the specular ping-pong pair exists only while enabled | Upstream upsampled a full-resolution RGBA16F pair every frame though nothing consumes it. PR pending | shared `upsample.cs.hlsl`, `radianceDisocc.cs.hlsl` |
| Forced | `Color::Ambient(GetAmbient(N))` with `Masks.z` | Powered FO4 DALC times albedo (`FO4Ambient::DiffuseAlbedo`), clamped to native diffuse before the sqrt-direct/full-ambient AO and diffuse bounce. With `SKYLIGHTING` it is scaled by Skylighting's per-channel ambient factor, as lighting already did; with IBL it carries the [IBL](IBL.md) replacement and luminance restore | FO4 folds the powered 2.2 DALC into diffuse and has no `Masks.z` | `FO4/AmbientConsumer.hlsli`, `FO4/ScreenSpaceGIConsumer.hlsli` `ComposeDiffuse` |
| Forced | Skyrim SSAO generation toggle | Per-frame native SAO_CS active/applied bits with the existing baseline snapshot | Native DrawModel/console rewrite them, and the host must control generation as well as composition | `ScreenSpaceGI.cpp` `ApplyVanillaSSAO`, `Engine.h` |
| Forced | `Masks2.x` vertex AO | `1−vertexAO` in emissive logical 31.a, shared with Skylighting (written when `SSGI` or `SKYLIGHTING` is defined); permutations that cannot carry it (blended hair, landscape, skin tint, eye) write 0, so vertex AO is 1 there | No separate vertex-AO channel exists; stock shaders leave emissive alpha unread | `DFPrepass.hlsl`, consumer `vertexAOStore` |
| Tweak | AOPower default 1, edit range 0–6 | Default 4, edit range 0–12 | Main `06032303` calibrates placed-light-dominated interiors | `ScreenSpaceGISettings.h` |

## Pending

| Kind | Upstream | Notes |
|---|---|---|
| Pending | Remaining specular consumer families | Native cube SH/HQ composition is present; other specular provider coverage still needs adapters. Missing consumers are not an engine incompatibility |
