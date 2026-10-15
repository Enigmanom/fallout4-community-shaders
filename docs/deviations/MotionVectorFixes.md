# MotionVectorFixes

No upstream counterpart.

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Divergence | None | Previous-transform correction for player updates, sequence positioning, frozen/menu and LOD/landscape draws; skipped in LoadingMenu. BLEND motion gaps are not repaired | Prepass projects from world/previousWorld, so stale transform history feeds native motion |
