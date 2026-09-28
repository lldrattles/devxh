# Changelog

## v1.0.1 — 2026-09-28

Packaging fix release — no gameplay or CLI changes.

### Fixed
- The release binary now runs on any modern Ubuntu LTS (22.04, 24.04,
  26.04) out of the box. v1.0.0 was dynamically linked against every
  dependency of its build host (Ubuntu 24.04) and failed at startup on
  other distros (missing `libSDL2_image-2.0.so.0`, then `libfmt.so.9`,
  which newer Ubuntu releases no longer ship).
- Vendored `libfmt` (10.0.0), `libsodium`, and `bzip2` are now statically
  bundled using upstream's release flag set
  (`DEVILUTIONX_SYSTEM_LIBFMT/LIBSODIUM/BZIP2=OFF`), eliminating
  version-specific dependencies like `libfmt.so.9` entirely.
- `libstdc++` is statically linked (`DEVILUTIONX_STATIC_CXX_STDLIB=ON`).
- New `Source/glibc_compat.c` backports newer-glibc symbols
  (`__isoc23_strtol*`, `arc4random`) at link time, pinning the runtime
  floor to **glibc 2.35 (Ubuntu 22.04 LTS)** even when built on a newer
  host. Only `libsdl2-2.0-0`, `libsdl2-image-2.0-0`, and `zlib1g` remain
  as installable runtime dependencies.

### Docs
- HANDBOOK.md (README-SERVER.md) gained a Requirements section and a
  release-parity build recipe.
- First Gate guide documents the dependency step explicitly.

## v1.0.0 — 2026-09-25

First public release of the DevXH headless dedicated server mod.

### Added
- `devilutionx-server` — headless entry point with full CLI
  (`--server-tcp`, `--server-zerotier`, `--server-zt-network`, `--join`,
  `--name`, `--password`, `--port`, `--hero-class`, `--hero-name`,
  `--game`, `--difficulty`, `--full-quests`, `--data-dir`, `--save-dir`,
  `--config-dir`)
- Idle host player with persistent hero save (level/gear carry across restarts)
- Bot mode: `--join ADDRESS` connects as an idle player to another game
- ZeroTier support with network-ID override, headless-aware wait times
- Heartbeat agent (`devxh-beat.sh` + systemd unit) for public gate listing
- Headless branches: `InitMulti` (direct game creation), `msg_wait_resync`
  (no-UI progress), loop pacing (≈2% CPU idle), explicit signal handling
- `headless_support.{hpp,cpp}` — shared hero-selection/config for the library

### Changed
- `diablo.cpp`: init/deinit functions hoisted to external linkage for the
  server entry point
- `storm_net.cpp`: headless provider init creates hero save without UI

### Notes
- Stock `devilutionx` client behavior is unchanged
- Game MPQs are not included; you must own Diablo and/or Hellfire
