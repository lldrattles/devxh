#!/usr/bin/env bash
# ============================================================
# DevXH Gate Heartbeat Agent
# Reports liveness + player count to the gate API every 60s.
#
# Install on the VPS running devilutionx-server:
#   1. Copy to /opt/devxh/devxh-beat.sh, chmod +x
#   2. Fill in SERVER_ID, API_KEY, API_URL below (or via env)
#   3. Install the systemd unit (devxh-beat.service) and enable it
#
# Player count = established TCP connections to the game port from
# NON-loopback peers (each connected client holds one). The idle host
# player connects to the listener over 127.0.0.1 and is excluded —
# verified on vps1: a healthy server always holds exactly one loopback
# self-connection, which made every gate show 1 more human than present.
# Same-machine human players via 127.0.0.1 are indistinguishable from
# the idle host and also excluded; real players join via the server's
# public/LAN/ZeroTier address, which is always counted.
#
# MULTIPLE GATES ON ONE VPS — scope this agent to one instance:
#   DEVXH_UNIT=devxh@hellfire-east.service   (systemd template units;
#       MainPID is queried from systemd, always exact)
#   DEVXH_PIDFILE=/run/devxh/east.pid        (manual/docker setups;
#       file contains the game PID, stale/reused PIDs are rejected)
#   Neither set = legacy behavior (first devilutionx-server process
#       found — only correct when a single gate runs on the box).
#
# Test one beat from the shell:  DEVXH_BEAT_ONCE=1 ./devxh-beat.sh
# ============================================================

set -u

SERVER_ID="${DEVXH_SERVER_ID:-1}"
API_KEY="${DEVXH_API_KEY:-devxh_REPLACE_ME}"
API_URL="${DEVXH_API_URL:-https://devxh.com/forum/api.php?action=heartbeat}"
GAME_PORT="${DEVXH_PORT:-6112}"
INTERVAL="${DEVXH_BEAT_INTERVAL:-60}"
GAME_UNIT="${DEVXH_UNIT:-}"
PIDFILE="${DEVXH_PIDFILE:-}"

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*"; }

game_pid() {
    # Resolve THIS gate's server PID, scoped to the instance when configured.
    local pid=""
    if [ -n "$GAME_UNIT" ] && command -v systemctl >/dev/null 2>&1; then
        # systemd template unit: ask systemd for the exact main PID
        pid=$(systemctl show -p MainPID --value "$GAME_UNIT" 2>/dev/null)
        [ "$pid" = "0" ] && pid=""
    elif [ -n "$PIDFILE" ]; then
        if [ -r "$PIDFILE" ]; then
            pid=$(head -1 "$PIDFILE" 2>/dev/null | tr -dc '0-9')
            # Stale/reused-PID guard: the PID must be a devilutionx-server
            # (stderr redirect comes first, or the open-failure message leaks)
            if [ -n "$pid" ] && ! tr '\0' ' ' 2>/dev/null < "/proc/$pid/cmdline" | grep -q devilutionx-server; then
                pid=""
            fi
        fi
    else
        # Legacy single-gate behavior
        pid=$(pgrep -f devilutionx-server | head -1)
    fi
    echo "$pid"
}

player_count() {
    # Established connections to the game port (IPv4+IPv6), excluding LISTEN
    # and excluding loopback peers (the idle host's self-connection).
    # ss columns (-Htn): $1=State $4=Local:Port $5=Peer:Port
    # NOTE: peers print as addr:port, so match loopback by PREFIX only —
    # a `$`-anchored regex never matches once the port suffix is there
    # (first attempt had exactly that bug; fixture-tested now).
    # 127. prefix covers all of 127.0.0.0/8; [::ffff: covers v4-mapped forms.
    local n=0
    if command -v ss >/dev/null 2>&1; then
        n=$(ss -Htn "sport = :${GAME_PORT}" 2>/dev/null | awk '
            $1 == "ESTAB" && $5 !~ /^(127\.|::1|\[::1\]|\[::ffff:)/ { c++ }
            END { print c+0 }')
    elif command -v netstat >/dev/null 2>&1; then
        n=$(netstat -tn 2>/dev/null | awk -v p=":${GAME_PORT}" '
            $4 ~ p && $6 == "ESTABLISHED" && $5 !~ /^(127\.|::1|\[::1\]|\[::ffff:)/ { c++ }
            END { print c+0 }')
    fi
    echo "$((n < 0 ? 0 : n))"
}

game_uptime() {
    # Uptime of this gate's devilutionx-server process in seconds; 0 if not found
    local pid
    pid=$(game_pid)
    if [ -n "$pid" ]; then
        ps -o etimes= -p "$pid" 2>/dev/null | tr -d ' '
    else
        echo 0
    fi
}

game_running() {
    [ -n "$(game_pid)" ]
}

send_beat() {
    local players cap uptime version body resp code
    players=$(player_count)
    uptime=$(game_uptime)
    cap="${DEVXH_CAPACITY:-4}"
    version="${DEVXH_VERSION:-1.5.5-dev}"

    if game_running; then
        body=$(printf '{"players":%d,"capacity":%d,"uptime":%d,"version":"%s"}' \
            "$players" "$cap" "$uptime" "$version")
    else
        # Process down: report zero players so the listing can show it dimmed
        body=$(printf '{"players":0,"capacity":%d,"uptime":0,"version":"%s"}' "$cap" "$version")
        log "WARN: devilutionx-server process not found for this instance"
    fi

    resp=$(mktemp)
    code=$(curl -s -o "$resp" -w "%{http_code}" --max-time 15 \
        -X POST "$API_URL" \
        -H "Content-Type: application/json" \
        -H "X-DevXH-Server: $SERVER_ID" \
        -H "X-DevXH-Key: $API_KEY" \
        -d "$body" 2>/dev/null) || code=000

    if [ "$code" = "200" ]; then
        log "beat OK: players=$players uptime=${uptime}s"
    else
        log "beat FAILED (HTTP $code): $(cat "$resp" 2>/dev/null | head -c 200)"
    fi
    rm -f "$resp"
}

log "DevXH heartbeat agent starting (server=$SERVER_ID interval=${INTERVAL}s port=$GAME_PORT unit=${GAME_UNIT:-none} pidfile=${PIDFILE:-none})"

if [ "${DEVXH_BEAT_ONCE:-0}" = "1" ]; then
    send_beat
    exit 0
fi

while true; do
    send_beat
    sleep "$INTERVAL"
done
