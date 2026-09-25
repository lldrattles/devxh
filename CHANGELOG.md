# Changelog

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
