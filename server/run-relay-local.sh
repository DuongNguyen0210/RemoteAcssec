#!/usr/bin/env bash
set -euo pipefail
server_dir="$(cd "$(dirname "$0")" && pwd)"
# 8080 is commonly used by web stacks; keep this local launcher separate from Compose.
export RELAY_PORT="${RELAY_PORT:-18080}"
export MANAGEMENT_API_URL="${MANAGEMENT_API_URL:-http://127.0.0.1:9090}"
printf 'Starting local relay on TCP %s; set REMOTE_RELAY_PORT=%s on both static clients.\n' "$RELAY_PORT" "$RELAY_PORT"
exec mvn -f "$server_dir/pom.xml" -pl relay-proxy compile exec:java \
    -Dexec.mainClass=com.remotecontrol.relay.server.RelayServer
