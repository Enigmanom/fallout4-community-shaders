# CS Editor

Ports the weather picker, weather lock, time controls, weather status/information panels and
per-feature weather-analysis seam from the pinned `src/Features/CSEditor.{h,cpp}` and
`src/CSEditor/EditorWindow.cpp`.

## Translations

| Kind | Upstream | Fallout 4 | Why | Where |
|---|---|---|---|---|
| Forced | `TESWeather::GetFormEditorID()` (names supplied by po3 Tweaks) | Editor IDs cached by a `TESWeather` `SetFormEditorID` (vslot 59) hook while forms load | FO4's `TESWeather` editor-ID vfuncs are the empty `TESForm` stubs | `World/EditorIDs.cpp` |
| Divergence | Weathers labelled by name/editor ID/decimal FormID | `EditorID (0xLocalID~Plugin)`, the per-weather TOML key | Weathers have no full name; the key is pasteable into `[features.<key>.weather]` | `World/Weather.cpp` `DescribeWeather` |
| Tweak | Lock redirects every scanned E8/E9 call site of `SetWeather`/`ForceWeather` | Detours on both function entries; the editor's own calls use the originals | Existing detour mechanism; also covers indirect callers | `World/WeatherControl.cpp` |
| Tweak | `Util::ForceWeather` resets the aurora root/model and cloud render passes | Plain `Sky::ForceWeather` | Skyrim workaround; FO4 `fw` shows no stale sky | `World/WeatherControl.cpp` |
| Tweak | Time-sensitive menus: Loading, SleepWait, Map | Loading, SleepWait, Pipboy | FO4 fast travel starts from the Pip-Boy map | `World/GameTime.cpp` |
| Pending | Game-hour scrubbing calls `Util::RequestTimeJumpTransition` | Not ported | Skyrim lighting/shadow refresh; FO4 need unverified | `WeatherPanel.cpp` |
| Forced | Type filters and colors Pleasant, Cloudy, Rainy, Snow, Aurora, Aurora Sun | Same flag bits labelled Pleasant, Cloudy, Rainy, Dusty, Radstorm, Radstorm Sun; radstorm outranks dust for color | Vanilla FO4 sets the Snow bit only on Glowing Sea haze, dust and radstorms, and the aurora bits only on radstorms | `WeatherPanel.cpp` |
| Tweak | Lightning color edited with `ColorEdit3` | Three 0–255 sliders | The forwarding UI has no color editor | `WeatherPanel.cpp` |
| Tweak | Wind panel shows effective wind and wind-vs-player using a 30.5° offset | Player heading only | The offset is Skyrim-calibrated | `WeatherPanel.cpp` |
| Divergence | Weather overrides edited in the Weather Widget, stored as per-weather JSON | Edited on the CS Editor page, stored in each feature's `[features.<key>.weather]` TOML map | Existing typed weather profiles | `WeatherOverrideEditor.cpp`, `World/WeatherProfiles.h` |
| Pending | Weather detail overlay window, editor window shell, form widgets, light editor, palette, interior-only panel | Not ported | — | — |
