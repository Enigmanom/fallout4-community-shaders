# Terrain Blending

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Terrain depth prepass with offset, blended depth published for consumers | Loaded landscape is held out of the world prepass and drawn after all opaque geometry against a copy of the object-only depth; near-terrain Z-prepass entries are dropped so that copy holds none; consumers read ordinary depth | FO4 draws the prepass and G-buffer in one pass over shared depth |
| Forced | Every output blends by alpha | Colour, normal, motion and parallax shadow lerp; material scalar, class, metadata and vertex AO are replaced where alpha is at least 0.5 | Deferred G-buffer channels carry data that must not be averaged |
| Divergence | Terrain is blended per pass when its bound is within 1024 units | Terrain is blended per pixel when view depth is within 1024 units | The held bucket replays as one engine draw |
| Pending | Meshes flagged `kNoTransparencyMultiSample` skip blending | No exemption | No FO4 per-mesh flag source yet |
| N/A | `bEnableLandFade` forced off | Nothing changed | FO4 land fade darkens albedo and has no alpha path |
