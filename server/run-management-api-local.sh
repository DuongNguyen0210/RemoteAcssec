#!/usr/bin/env bash
set -euo pipefail

server_dir="$(cd "$(dirname "$0")" && pwd)"
env_file="$server_dir/../infrastructure/.env"

if [[ -z "${JWT_SECRET:-}" ]]; then
    if [[ ! -f "$env_file" ]]; then
        echo "Missing $env_file; create it from infrastructure/.env.example and set JWT_SECRET." >&2
        exit 1
    fi
    JWT_SECRET="$(sed -n 's/^JWT_SECRET=//p' "$env_file" | tail -n 1)"
    export JWT_SECRET
fi
if [[ "${#JWT_SECRET}" -lt 32 ]]; then
    echo "JWT_SECRET must contain at least 32 bytes." >&2
    exit 1
fi

docker compose -f "$server_dir/docker-compose.yml" up -d --wait
export SPRING_DATASOURCE_URL="jdbc:postgresql://localhost:5432/remote_db"
export SPRING_DATASOURCE_USERNAME="admin"
export SPRING_DATASOURCE_PASSWORD="password123"
export SPRING_DATA_REDIS_HOST="localhost"
export SPRING_DATA_REDIS_PORT="6379"
exec mvn -f "$server_dir/pom.xml" -pl management-api spring-boot:run
