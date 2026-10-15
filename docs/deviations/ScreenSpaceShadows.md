# Screen Space Shadows

| Kind | Upstream | FO4 | Why |
|---|---|---|---|
| Forced | Scene depth with its matching ViewProjection | Bend reads canonical R32_FLOAT depth; first person casts but never receives SSS | Native world and near depth use different projections |
| Forced | First-person materials never receive SSS because `deferredPass` is cleared first | Receiver gate returns 1 for raw depth `<= 0.01` | FO4 lights world and first person in shared deferred passes |
| Forced | Direct and transmission visibility separated by facing | Directional split families apply x to the native shadow term when front-facing and y to back-facing wrap/transmission | DFLight combines direct and transmission before the shared shadow, which already carries x |
| Pending | DistantTree 0.8 SSS strength; forward/alpha receivers | Not ported | No distant-tree discriminator exists in deferred material receivers |
