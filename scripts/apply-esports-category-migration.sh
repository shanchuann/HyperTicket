#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"

read_env() {
  local key="$1"
  sed -n "s/^${key}=//p" "$env_file" | tail -n 1 | tr -d '\r'
}

db_host="$(read_env DB_HOST)"
db_port="$(read_env DB_PORT)"
db_user="$(read_env DB_USER)"
db_name="$(read_env DB_NAME)"
export MYSQL_PWD="$(read_env DB_PASSWORD)"

mysql_cmd=(mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name")
"${mysql_cmd[@]}" < "$project_dir/db/migrate_v9_esports_category.sql"

verified="$("${mysql_cmd[@]}" -Nse "SELECT COUNT(*) FROM tickets WHERE category='esports'")"
echo "Esports category migration applied (${verified} matching ticket(s))."
