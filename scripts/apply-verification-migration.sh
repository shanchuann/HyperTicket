#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"
read_env() {
  local key="$1"
  awk -F= -v wanted="$key" '$1 == wanted { sub(/^[^=]*=/, ""); value=$0 } END { print value }' "$env_file"
}

export MYSQL_PWD="$(read_env DB_PASSWORD)"
mysql --protocol=tcp -h "$(read_env DB_HOST)" -P "$(read_env DB_PORT)" \
  -u "$(read_env DB_USER)" "$(read_env DB_NAME)" < "$project_dir/db/migrate_v6_verification.sql"
