# DevXH — DevilutionX Dedicated Server

A port of [DevilutionX](https://github.com/diasurgical/devilutionX) (1.5.5) that runs
the game engine as a **headless dedicated server** — no display, no window manager,
no audio device. It hosts a public multiplayer game (TCP/IP or ZeroTier), keeps an
idle player seated so the world stays up, and runs forever until stopped.

Ideal for a VPS. ~2% CPU idle. Stock DevilutionX clients join with no patches.

Runs on any modern distro — Ubuntu 22.04 / 24.04 / 26.04 LTS, Debian — with three
packages (`sudo apt install libsdl2-2.0-0 libsdl2-image-2.0-0 zlib1g`); `libfmt`,
`libsodium`, `bzip2`, and the C++ runtime are statically bundled.

**Website & public gate list:** [devxh.com](https://devxh.com)

## Quick start

```bash
# with your Hellfire/Diablo MPQs in ./assets
./devilutionx-server --data-dir ./assets \
    --save-dir ./saves --config-dir ./config --name "My Gate"
```

Players connect with stock DevilutionX: Multiplayer → TCP/IP → your server's IP.

Full CLI reference, ZeroTier setup, and the systemd guide: [HANDBOOK.md](HANDBOOK.md)
(also readable online at the website's [Guides](https://devxh.com/guides/)).

## Optional: list your gate publicly

1. Register at the [forum](https://devxh.com/forum/gates.php), create a gate,
   copy your API key
2. Edit `devxh-beat.service` (server id + key), then install:

```bash
sudo cp devxh-beat.sh /opt/devxh/ && chmod +x /opt/devxh/devxh-beat.sh
sudo cp devxh-beat.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now devxh-beat
```

Your gate appears on the website's Server List with live player counts.

## Building from source

Dev build:

```bash
cmake -B build-server -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-server -j$(nproc) --target devilutionx devilutionx-server
```

Produces `devilutionx` (unchanged stock client) and `devilutionx-server` (the mod).

For a **release binary** that runs on any Ubuntu 22.04+ distro regardless of build
host, use the release flag set documented in [HANDBOOK.md](HANDBOOK.md) — it statically
bundles `libfmt`/`libsodium`/`bzip2` and keeps the glibc 2.35 floor via
`Source/glibc_compat.c`.

## License & assets

DevilutionX is licensed under the Sustainable Use License — see upstream.
Diablo and Hellfire are trademarks of Blizzard Entertainment; **game MPQs are not
included and you must own the game**. DevXH is a non-commercial fan project, not
affiliated with or endorsed by Blizzard or the DevilutionX team.
