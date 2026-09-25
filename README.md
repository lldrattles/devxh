# DevXH — DevilutionX Dedicated Server

A port of [DevilutionX](https://github.com/diasurgical/devilutionX) (1.5.5) that runs
the game engine as a **headless dedicated server** — no display, no window manager,
no audio device. It hosts a public multiplayer game (TCP/IP or ZeroTier), keeps an
idle player seated so the world stays up, and runs forever until stopped.

Ideal for a VPS. ~2% CPU idle. Stock DevilutionX clients join with no patches.

**Website & public gate list:** [devxh.d2hc.com](https://devxh.d2hc.com)

## Quick start

```bash
# with your Hellfire/Diablo MPQs in ./assets
./devilutionx-server --data-dir ./assets \
    --save-dir ./saves --config-dir ./config --name "My Gate"
```

Players connect with stock DevilutionX: Multiplayer → TCP/IP → your server's IP.

Full CLI reference, ZeroTier setup, and the systemd guide: [HANDBOOK.md](HANDBOOK.md)
(also readable online at the website's [Guides](https://devxh.d2hc.com/guides/)).

## Optional: list your gate publicly

1. Register at the [forum](https://devxh.d2hc.com/forum/gates.php), create a gate,
   copy your API key
2. Edit `devxh-beat.service` (server id + key), then install:

```bash
sudo cp devxh-beat.sh /opt/devxh/ && chmod +x /opt/devxh/devxh-beat.sh
sudo cp devxh-beat.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now devxh-beat
```

Your gate appears on the website's Server List with live player counts.

## Building from source

```bash
cmake -B build-server -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-server -j$(nproc) --target devilutionx devilutionx-server
```

Produces `devilutionx` (unchanged stock client) and `devilutionx-server` (the mod).

## License & assets

DevilutionX is licensed under the Sustainable Use License — see upstream.
Diablo and Hellfire are trademarks of Blizzard Entertainment; **game MPQs are not
included and you must own the game**. DevXH is a non-commercial fan project, not
affiliated with or endorsed by Blizzard or the DevilutionX team.
