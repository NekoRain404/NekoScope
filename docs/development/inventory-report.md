# Native Inventory Collector Report

Date: 2026-11-04

## Implementation

`neko::collectInventory()` remains the public real-host entry point and returns the established `devices` / `system` snapshot shape. Collection is split into:

- `native/InventoryCollector.cpp`: snapshot orchestration and common timestamp/provenance metadata.
- `native/inventory/InventorySupport.cpp`: safe reads, field/device maps, OS release and uptime helpers.
- `native/inventory/CpuMemory.cpp`: CPU, topology/cache/frequency/vulnerability, memory and NUMA.
- `native/inventory/Devices.cpp`: DMI, block/mount, PCI/USB, network, power, DRM displays and ALSA.

CMake integration needs those four `.cpp` files. The public interface is declared in `native/InventoryCollector.h`; deterministic fixture injection is available through `native/inventory/InventorySource.h` and accepts an alternate Linux-root path plus an optional fixed collection time.

Every snapshot now reports `system.collectedAt`, `system.collectorVersion`, and `system.provenance`. Each device and field carries the same UTC ISO 8601 millisecond timestamp; devices also carry the collector version. Existing device `summary` remains, with `subtitle` supplied as the presentation alias. Field metadata retains `key/value/source/status/sensitive`, adding `unit`, `rawValue`, and `reason` where relevant. Status may now be `read-error` in addition to the existing available/unavailable/permission-denied values.

CPU cache records are deduplicated by level/type/shared CPU topology and expose their individual sysfs sources. Missing package/core IDs no longer count as a fabricated package/core. Memory parses counts separately from byte quantities, rejects malformed units and overflow, distinguishes zero from absent, and computes used RAM only from valid total/available values. NUMA nodes expose CPU lists, maps, distance and meminfo. Block devices match `/proc/self/mountinfo` by major/minor, decode escaped mount paths, and only query filesystem free space against the actual host; SMART health stays explicitly unavailable until a backend exists. PCI names resolve through installed `pci.ids` databases when present. DMI serial/UUID/asset fields remain sensitive. Display EDID identity is parsed by optional `libdisplay-info` when the build enables it, with basic validated base-block metadata retained as a conservative no-library fallback. The display serial is sensitive.

## Verification

The initial Qt 6.12 test run built successfully and reproduced the missing-timestamp regression. After implementation, the standalone Qt 6.12/Ninja build passes 18/18 tests on a real system snapshot and deterministic temporary Linux-root fixtures. Fixtures cover empty and malformed proc/sys trees, exact memory units, CPU topology/cache/NUMA, mount association and escaped paths, DMI, pci.ids resolution, valid/invalid EDID, timestamps, provenance, and extreme uptime values.

A second Qt 6.12 build with `NEKO_HAS_DISPLAY_INFO`, libdisplay-info 0.3.0, AddressSanitizer, UndefinedBehaviorSanitizer and leak detection also passes 18/18. `git diff --check` passes for the inventory-owned files. These were independent temporary CMake projects under `/tmp`; root CMake integration remains owned by the controller.

## Remaining gaps

- PCI naming relies on an installed `pci.ids` file; no udev database/libudev decoding is integrated yet. USB names use sysfs strings.
- EDID validation has a bounded base-block fallback and the optional library path; complete chromaticity, timings, HDR and extension block presentation remains future work.
- No active UDisks2/libnvme SMART query, vendor GPU metrics, SPD/DIMM access, or audio endpoint enumeration is claimed.
- Firmware and device fields only appear where the running kernel exposes readable interfaces. No specific distro, hardware, or completeness claim follows from this host test.
- Root integration should treat unknown status strings (including `read-error`) as data state, and optionally enable `libdisplay-info` using `PkgConfig::DISPLAY_INFO` plus `NEKO_HAS_DISPLAY_INFO=1` when available.
