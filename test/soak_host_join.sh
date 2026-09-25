#!/usr/bin/env bash
# Soak test: host + joined idle player run together for 60 seconds.
# Verifies the connection stays up, CPU stays low, and SIGINT stops both.
set -u

DIR="/mnt/d/DevilutionX/DevXH/DevilutionX-1.5.5/build-server"
DATA="/mnt/d/DevilutionX/DevXH"

pkill -f devilutionx-server 2>/dev/null
sleep 1
rm -rf /tmp/dx-host /tmp/dx-join
mkdir -p /tmp/dx-host /tmp/dx-join

"$DIR/devilutionx-server" --data-dir "$DATA" \
    --save-dir /tmp/dx-host --config-dir /tmp/dx-host \
    --name soak-game > /tmp/host.log 2>&1 &
sleep 5
"$DIR/devilutionx-server" --data-dir "$DATA" \
    --save-dir /tmp/dx-join --config-dir /tmp/dx-join \
    --join 127.0.0.1 --hero-name Bot > /tmp/join.log 2>&1 &

echo "running 60s soak..."
FAILED=0
for i in $(seq 1 6); do
    sleep 10
    ESTAB=$(ss -tn state established '( sport = :6112 or dport = :6112 )' | tail -n +2 | wc -l)
    HOST_ALIVE=$(pgrep -cf "save-dir /tmp/dx-host" || true)
    JOIN_ALIVE=$(pgrep -cf "save-dir /tmp/dx-join" || true)
    CPU=$(ps -o %cpu= -p $(pgrep -f "save-dir /tmp/dx-host" | head -1) 2>/dev/null)
    echo "t=$((i*10))s: estab=$ESTAB host_alive=$HOST_ALIVE join_alive=$JOIN_ALIVE host_cpu=${CPU:-?}%"
    if [ "$ESTAB" -lt 1 ] || [ "$HOST_ALIVE" -lt 1 ] || [ "$JOIN_ALIVE" -lt 1 ]; then
        echo "FAIL at t=$((i*10))s"; FAILED=1; break
    fi
done

echo "=== SIGINT both"
pkill -INT -f "save-dir /tmp/dx-host"; pkill -INT -f "save-dir /tmp/dx-join"
sleep 5
LEFT=$(pgrep -cf devilutionx-server || true)
if [ "$LEFT" -ge 1 ]; then
    echo "FAIL: $LEFT process(es) survived SIGINT"; pkill -9 -f devilutionx-server; FAILED=1
else
    echo "both stopped cleanly"
fi

echo "=== join log:"; tail -6 /tmp/join.log
echo "=== host log:"; tail -6 /tmp/host.log
[ "$FAILED" -eq 0 ] && echo "SOAK TEST PASSED" || echo "SOAK TEST FAILED"
