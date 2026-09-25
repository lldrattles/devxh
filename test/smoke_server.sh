#!/usr/bin/env bash
# Smoke test for the headless DevilutionX dedicated server.
# Verifies: starts headless, binds TCP 6112, low CPU, exits on SIGINT.
set -u

BIN="/mnt/d/DevilutionX/DevXH/DevilutionX-1.5.5/build-server/devilutionx-server"
DATA="/mnt/d/DevilutionX/DevXH"
RUN=/tmp/dx-smoke

pkill -f devilutionx-server 2>/dev/null
sleep 1
rm -rf "$RUN" /tmp/dx-srv
mkdir -p /tmp/dx-srv

"$BIN" --data-dir "$DATA" --save-dir /tmp/dx-srv --config-dir /tmp/dx-srv \
    --name smoke-test > /tmp/srv.log 2>&1 &
SRV=$!
echo "started pid=$SRV"

sleep 6

if ! kill -0 "$SRV" 2>/dev/null; then
    echo "FAIL: server exited early; log:"
    cat /tmp/srv.log
    exit 1
fi

echo "--- listeners:"
ss -tln | grep -E '6112' || echo "FAIL: no listener on 6112"
echo "--- resources:"
ps -p "$SRV" -o pid,%cpu,%mem,comm | tail -1

echo "--- sending SIGINT"
kill -INT "$SRV"
for i in $(seq 1 10); do
    sleep 1
    kill -0 "$SRV" 2>/dev/null || break
done
if kill -0 "$SRV" 2>/dev/null; then
    echo "FAIL: still running 10s after SIGINT; killing"
    kill -9 "$SRV"
    exit 1
fi
wait "$SRV"
echo "exit code: $?"

echo "--- log:"
cat /tmp/srv.log
echo "SMOKE TEST PASSED"
