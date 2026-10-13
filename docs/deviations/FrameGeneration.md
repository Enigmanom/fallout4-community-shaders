# FrameGeneration

Upstream: `src/Features/Upscaling` FG implementation. The SDK world-projection depth gap is recorded
in [Upscaling](Upscaling.md).

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Divergence | Upstream separate premultiplied UI texture | Pre-UI SDR HUD-less plus final-color packet and current presentation providers | Main's capture/composition architecture; upstream UI-redirection parity is not established | `TemporalFrameGenerationInputs.cpp` `CaptureHUDLessColor`, `DX12SwapChain.cpp` |
| Pending | Upstream premultiplied UI-redirection producer/consumer coverage | No corresponding upstream UI texture chain is ported | The HUD-less/final-color packet does not establish equivalent UI coverage | `TemporalFrameGenerationInputs.cpp`, `DX12SwapChain.cpp` |
| Divergence | Geometry-derived motion/depth on transparent first-person pixels | Pre/post-alpha color-difference×1000 blends motion toward zero and depth toward `min(depth,0.1)` | engine-facts Prepass motion encoding proves BLEND coverage gaps, not this heuristic's correctness | `CopyDepthForFrameGenerationCS.hlsl` |
| Divergence | Upstream FG frame-limit/Reflex policy | Fixed/dynamic generated-frame controls and Reflex policy | In-house presentation behavior, not asserted equivalent | `FrameGenerationSettings.h`, `Streamline.cpp` |
| Divergence | FG reset only on HDR parameter change; SR reset never tied to FG | FG reset is requested once per interruption (unprepared Present or world-less frame) and stays pending until a prepared FG Present consumes it; the SR reset follows pending FG only on frames where FG is latched on | DLSS-G shares one `slConstants.reset` with DLSS SR, and the FG proxy swapchain unprepares Presents upstream never skips; coalescing keeps a paused or menu interval from resetting SR every frame | `TemporalPipelineState.h`, `TemporalPipeline.cpp` |
