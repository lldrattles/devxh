# DevilutionX Headless Dedicated Server

A port of DevilutionX that runs the game engine as a **headless dedicated
server** — no window, no display server, no window manager, no audio device.
It hosts a public multiplayer game (TCP/IP or ZeroTier), keeps an idle player
seated in the game so the world stays up, and runs forever until stopped —
ideal for a systemd service, a container, or any machine without a desktop.

The normal `devilutionx` client is unaffected and joins the server's game
exactly like any other multiplayer game.

## Building (Linux / WSL)

```bash
cd DevilutionX-1.5.5
cmake -B build-server -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-server -j$(nproc) --target devilutionx devilutionx-server
```

This produces:

- `build-server/devilutionx-server` — the headless server (and idle-player bot)
- `build-server/devilutionx` — the normal client (unchanged behavior)

## Quick start

With the game MPQs (`hellfire.mpq`, `hfmonk.mpq`, …) in a directory, e.g. the
project root:

```bash
cd build-server
./devilutionx-server --data-dir /path/to/mpqs \
    --save-dir ~/.local/share/devilutionx-server \
    --config-dir ~/.config/devilutionx-server
```

The server prints `Hosting game ... on TCP/IP`, binds TCP port **6112** on all
interfaces, and idles until killed. Stop it with `Ctrl+C` (SIGINT), `kill`
(SIGTERM), or `systemctl stop` — it shuts down cleanly and saves.

Then, on a client machine, run stock DevilutionX → Multiplayer →
TCP/IP → enter the server's IP → join `server-XXXX` (or your `--name`).

## Options

```
--server-tcp [ADDRESS]   Host a TCP/IP game (default). Binds to all interfaces
                         by default; give an address to bind to, e.g. a
                         ZeroTier interface address.
--server-zerotier        Host a ZeroTier game on the configured network.
--server-zt-network ID   Join the ZeroTier network with the given 16-hex-digit
                         ID before hosting (default: Diasurgical public Earth).
--join ADDRESS           Do not host: join the game at ADDRESS as an idle
                         player instead (TCP/IP host name or IP, or the
                         advertised game name for ZeroTier).
--name NAME              Game name shown in the game list (default: server-<pid>).
--password PASSWORD      Password of the game (hosted or joined; default: public).
--port PORT              TCP port to listen on (default: 6112).
--hero-class CLASS       Idle player class: warrior rogue sorcerer monk bard
                         barbarian (default: warrior).
--hero-name NAME         Idle player name (default: Server).
--game GAME              hellfire or diablo (default: hellfire).
--difficulty N           0 = normal, 1 = nightmare, 2 = hell (default: 0).
--full-quests            Enable the full quest set for the game.
--data-dir PATH          Asset (MPQ) search directory.
--save-dir PATH          Directory for the server hero save.
--config-dir PATH        Directory for configuration files.
-f                       Display frame count in the log.
--verbose                More verbose logging.
-h, --help               Show this help.
```

## Public games over TCP/IP

- The server binds `0.0.0.0:6112` by default, so anyone who can reach the
  machine can join. For a game open to the public internet, forward TCP 6112
  on your router/firewall to the server.
- Use `--name "My Public Server"` so players recognize it, and optionally
  `--password` to restrict access (clients will be prompted).
- Use `--port` if 6112 is taken; forward the same port.

## Public games over ZeroTier

1. Create (or pick) a ZeroTier network and have every player plus the server
   join it.
2. On the server: join the network with the ZeroTier CLI
   (`zerotier-cli join <network>`), authorize the node, then run:

   ```bash
   ./devilutionx-server --server-zerotier \
       --server-zt-network 0123456789abcdef \
       --name "My ZT Server" --data-dir ... --save-dir ... --config-dir ...
   ```

   (If the network is joined externally as above, `--server-zt-network` can be
   omitted; the server will use the network configured in its ini file —
   by default the Diasurgical public Earth network.)
3. Clients join the same ZeroTier network, run stock DevilutionX →
   Multiplayer → ZeroTier, and pick the game from the list.

You can also bind a TCP game to the ZeroTier interface only:
`--server-tcp <zt-ip-address>`.

## The idle player

The server seats a real player ("Server" by default) so the simulation stays
alive. On first start it creates a save for the configured `--hero-class`;
later runs reuse that save (level, equipment, etc. persist). The idle player
just stands in town — it takes no actions.

Bot mode (`--join`) behaves the same, but joins someone else's game instead of
hosting — e.g. `--join 192.168.1.10 --hero-name Bot`.

## Running as a service (systemd)

```ini
[Unit]
Description=DevilutionX headless dedicated server
After=network-online.target
Wants=network-online.target

[Service]
User=games
ExecStart=/opt/devilutionx/devilutionx-server \
    --data-dir /opt/devilutionx \
    --save-dir /var/lib/devilutionx-server \
    --config-dir /etc/devilutionx-server \
    --name "My Public Server" --server-tcp
Restart=on-failure
# The server needs no display, audio, or input devices.

[Install]
WantedBy=multi-user.target
```

The binary itself requires no window manager and works in containers
(`docker run -d -p 6112:6112 ...`) without extra device passthroughs.

## Logging and diagnostics

The server logs to stderr (journald picks it up under systemd):

- `Hosting game '...' on TCP/IP (idle player: Server)` — startup OK
- `Headless resync: 25%..100% (N ms)` — a player is joining and receiving the
  world state (host side also logs when others join/leave)
- `Headless resync failed after N ms` — a join did not complete within the
  120 s budget; the joiner retries automatically (NetInit loop)

Use `--verbose` for packet-level debug logging.

## Tests

Bash smoke/soak tests (paths are WSL-specific; adjust to your layout):

- `test/smoke_server.sh` — boots the server, checks the 6112 listener and
  low CPU, verifies clean SIGINT shutdown
- `test/e2e_host_join.sh` — host + `--join` bot over loopback; verifies the
  bot completes resync into the game
- `test/soak_host_join.sh` — the same pair runs for 60 s; checks connection
  stability, CPU, and clean shutdown of both

## Implementation notes

- The server reuses the stock multiplayer engine path: `HeadlessMode` (the
  same flag the timedemo CI tests use) suppresses rendering, audio, and the
  cursor; the headless branches replace the connection/hero/game-selection
  dialogs with deterministic code.
- Key files: `Source/headless_server.cpp` (entry point, CLI, lifecycle),
  `Source/headless_support.{hpp,cpp}` (hero save selection, shared config),
  plus small headless branches in `multi.cpp` (`InitMulti`), `msg.cpp`
  (`msg_wait_resync`), `storm_net.cpp`, `diablo.cpp` (loop pacing),
  `zerotier_native.cpp` (network ID override), and `base_protocol.h`.
- The host's game difficulty comes from the server binary
  (`--difficulty`); clients cannot change it (as with any hosted game).
- `bard`/`barbarian` classes require `hfbard.mpq`/`hfbarb.mpq`, which are not
  part of this data set.
- Join mode skips the client-side game-compatibility dialog (that UI is not
  available headless); version mismatches are still rejected by the protocol.
