# Volumetric Shadows

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Moments are the min of the sun cascades and the ESRAM shadow map | Moments come from the sun cascades alone | FO4 has no second directional shadow map |
| N/A | ESRAM shadow map resized to 1024 | | FO4 has no ESRAM shadow map |
| Divergence | Two cascades; the fade past the second returns lit | With three splits the soft shadow fades into the native cascade shadow, so the third cascade keeps it; with one split cascade 1 reads lit | FO4 renders one to three sun cascades |
| Forced | VSM soft shadow on every directional pass | Only the shadowed families (one to three splits); unshadowed sun passes keep unshadowed lobes | FO4 renders no cascades for them |
| Forced | Skylighting visibility replaces the VSM soft term per material lighting flag | It replaces it for the whole directional family when Skylighting is loaded | No per-material soft, rim or back flag reaches the light pass |
| Forced | Water sun glint and refracted diffuse sun shadowed along the refracted ray | Sun specular and the fixed-axis lobe are shadowed along the unrefracted view ray | FO4 water has no refraction direction |
| N/A | Refracted diffuse sun term and its short rays | | FO4 water has no diffuse sun term |
| Pending | Effect mist, fog-card and particle short-ray shadows | Not ported | FO4 effect lighting carries the weather color, not sunlight, and its pixel shader has no world position; shadowing needs a new sun term |
| N/A | `BSParticleShader` receiver | | FO4 never draws the particle shader |
