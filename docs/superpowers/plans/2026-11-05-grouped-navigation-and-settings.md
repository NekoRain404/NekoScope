# Grouped navigation and richer preferences — implementation plan

Spec: `docs/superpowers/specs/2026-11-05-grouped-navigation-and-settings-design.md`

## Tasks

- [x] Add `deviceIds`/`deviceNames` to `SensorSnapshot` and derive device identity in the collector.
- [x] Expose `deviceId`/`device` on every `SensorService` reading.
- [x] Rebuild the sensors page as device → category → rows with collapsible groups, flat mode, counts and expand/collapse all.
- [x] Rebuild the hardware device tree as collapsible category → device rows, keeping selection, search, privacy and copy.
- [x] Add Preferences: sample interval, history samples, temperature unit, diagnostics, start page, reset.
- [x] Wire interval/history to the live services; apply start page at launch; apply temperature unit in sensors/chart/hardware fields.
- [x] Add device-identity, configurable-history, preference round-trip/reset tests.
- [x] Verify CTest, qmllint, offscreen/XWayland smoke and real screenshots.
