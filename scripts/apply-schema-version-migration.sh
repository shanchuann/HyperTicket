#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"
read_env() { sed -n "s/^$1=//p" "$env_file" | tail -n 1 | tr -d '\r'; }

export MYSQL_PWD="$(read_env DB_PASSWORD)"
mysql_cmd=(mysql --protocol=tcp -h "$(read_env DB_HOST)" -P "$(read_env DB_PORT)" \
  -u "$(read_env DB_USER)" "$(read_env DB_NAME)")
"${mysql_cmd[@]}" < "$project_dir/db/migrate_v11_schema_version.sql"

version="$("${mysql_cmd[@]}" -Nse 'SELECT COALESCE(MAX(version),0) FROM schema_migrations')"
test "$version" = "11" || { echo "Expected schema version 11, found $version" >&2; exit 1; }
echo "Schema version 11 applied."
