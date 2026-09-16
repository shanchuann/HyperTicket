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
"${mysql_cmd[@]}" < "$project_dir/db/migrate_v8_backend_reliability.sql"

verified="$("${mysql_cmd[@]}" -Nse "
  SELECT
    (SELECT COUNT(*) FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='reservations'
        AND COLUMN_NAME='deleted_at')=1
    AND
    (SELECT COUNT(*) FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='refunds'
        AND COLUMN_NAME IN ('attempt_count','max_attempts'))=2;")"
if [[ "$verified" != "1" ]]; then
  echo "Backend reliability migration verification failed." >&2
  exit 1
fi

echo "Backend reliability migration applied."
