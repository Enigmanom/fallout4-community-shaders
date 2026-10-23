# Terrain Blending

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Every output blends by alpha | Colour, normal, motion and parallax shadow lerp; material scalar, class, metadata and vertex AO are replaced where alpha is at least 0.5 | Deferred G-buffer channels carry data that must not be averaged |
| N/A | Blended terrain and object prepass depth replaces main and prepass-copy depth for consumers | Consumers read ordinary depth | FO4 depth consumers run after the G-buffer pass, which writes true terrain depth |
| Pending | Meshes flagged `kNoTransparencyMultiSample` skip blending | No exemption | No FO4 per-mesh flag source yet |
| N/A | `bEnableLandFade` forced off | Nothing changed | FO4 land fade darkens albedo and has no alpha path |
