# MotionVectorFixes

No corresponding upstream feature.

| Kind | Difference | Engine reason | Where |
|---|---|---|---|
| Divergence | FO4-only previous-transform correction for player updates, sequence positioning, frozen/menu and LOD/landscape draws; skips LoadingMenu | FO4 prepass projects current/previous camera-relative positions from world/previousWorld, so stale transform history feeds native motion. Each correction's necessity still needs runtime evidence; the BLEND output gap is not repaired (engine-facts Prepass motion encoding) | `MotionVectorFixes.cpp` |
