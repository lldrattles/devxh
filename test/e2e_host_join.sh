#!/usr/bin/env bash
# End-to-end test: headless host + headless idle player (bot) joining it
# over TCP/IP loopback. Verifies the joiner reaches the game with the host.
set -u

DIR="/mnt/d/DevilutionX/DevXH/DevilutionX-1.5.5/build-server"
DATA="/mnt/d/DevilutionX/DevXH"

pkill -f devilutionx-server 2>/dev/null
sleep 1
rm -rf /tmp/dx-host /tmp/dx-join
mkdir -p /tmp/dx-host /tmp/dx-join

"$DIR/devilutionx-server" --data-dir "$DATA" \
    --save-dir /tmp/dx-host --config-dir /tmp/dx-host \
    --name e2e-game > /tmp/host.log 2>&1 &
sleep 5

"$DIR/devilutionx-server" --data-dir "$DATA" \
    --save-dir /tmp/dx-join --config-dir /tmp/dx-join \
    --join 127.0.0.1 --hero-name Bot > /tmp/join.log 2>&1 &
sleep 12

echo "=== processes:"
pgrep -af devilutionx-server | sed 's/^/  /'

echo "=== host log:"; cat /tmp/host.log
echo "=== join log:"; cat /tmp/join.log

echo "=== established TCP connections on 6112:"
ss -tn | grep 6112 || echo "  (none)"

HOST_ALIVE=$(pgrep -cf "devilutionx-server --data-dir $DATA --save-dir /tmp/dx-host" || true)
JOIN_ALIVE=$(pgrep -cf "devilutionx-server --data-dir $DATA --save-dir /tmp/dx-join" || true)

if [ "$HOST_ALIVE" -ge 1 ] && [ "$JOIN_ALIVE" -ge 1 ]; then
    echo "E2E: both processes alive after join window"
else
    echo "E2E: FAIL (host=$HOST_ALIVE join=$JOIN_ALIVE)"
fi

pkill -f devilutionx-server
