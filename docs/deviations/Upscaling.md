# Upscaling

Upscaling, FrameGeneration and MotionVectorFixes are maintained in-house, not as upstream conversions.

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | FrameBuffer-adjusted samples in `ISReflectionsRayTracing` | Unadjusted b4 clamp; the SSLR chain (prepass, raytracing, blur H/V) runs in one full-proxy pass at render resolution and is copied back for water and composite | FO4 has no previous-frame reflection sample, and under dynamic resolution the stock quad made reflections slide with camera rotation |
| Forced | Single non-inverted perspective depth | Depth partitioned between first-person and world projections | See [README](README.md) |
| Pending | World-projection SDK depth (SR and FG) | Native partitioned depth is copied into typed R32_FLOAT, padded with sky=1; canonical t17 is not consumed | Typed storage does not reconcile projection partitions |
| Forced | Depth/refraction resolve writes SAOCameraZ MRT1 | Writes RefractionNormals and SV_Depth without SAOCameraZ | FO4 has no SAOCameraZ attachment |
| Forced | Shaders add `SharedData::MipBias`; engine samplers are never biased | Anisotropic engine samplers are swapped for `MipLODBias` copies around `DeferredPrePass` and `Forward`; shader-visible `MipBias` is 0 under `SAMPLER_MIP_BIAS`. Non-anisotropic samplers lose the bias | The bias would otherwise apply twice, and per-draw sampler switching is not available |
| Forced | Jitter and dynamic resolution published into the Skyrim viewport | Equivalent offsets and ratios published after FO4's native `UpdateTemporalData`, preserving top-left active-region proxies | FO4 state and target-manager layout |
| Forced | Menus and offscreen cameras render after upscaling | Pip-Boy-target renderers and the local-map callback run unscaled and unjittered (vendor upscalers only); offscreen cameras (precipitation, facegen) lock dynamic resolution. Barter/container/workbench full-frame renderers stay scaled | `Render_UI` calls these callbacks while render scale is live; a callback-wide scope would stretch the full-frame composite |
| Divergence | Pre-tonemap SR with upstream exposure/HDR policy | Post-tonemap/LUT gamma-2.2 R8G8B8A8_UNORM SR with automatic exposure | Explicit color contract |
| Divergence | Direct FidelityFX FSR3 dispatch | Streamline FSR3/FSR4 D3D12 providers; the encoder writes undilated motion to u2 for FSR | In-house SDK/backend |
| Divergence | FSR quality ratio for all providers | SDK render-size queries, quality 0-4, provider-specific sizing | DLSS/FSR optimal extents |
| Divergence | Provider selection and `upscaleMethodNoDLSS` | FSR4 is a method value; one SR preference is persisted | |
| Divergence | D3D11 RCAS | D3D12 RCAS on SDR output | In-house backend |
| Divergence | Default DLSS GPU/model presets | Explicit non-default preset policy alongside RCAS controls | |
| Pending | Reactive/transparency masks from TAA xy and water normal.z | Masks are zero | Production mask inputs not ported |
| Pending | Underwater-mask upscale (`UnderwaterMaskUpscalePS`) | Own depth/refraction publication and linear-depth regeneration instead | Underwater producer/consumer chain not ported |
