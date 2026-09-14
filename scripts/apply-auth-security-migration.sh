#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"

read_env() {
  local key="$1"
  awk -F= -v wanted="$key" '$1 == wanted { sub(/^[^=]*=/, ""); value=$0 } END { print value }' "$env_file"
}

db_host="$(read_env DB_HOST)"
db_port="$(read_env DB_PORT)"
db_user="$(read_env DB_USER)"
db_name="$(read_env DB_NAME)"
export MYSQL_PWD="$(read_env DB_PASSWORD)"

mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name" \
  < "$project_dir/db/migrate_v5_auth_security.sql"

mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name" -Nse \
  "SELECT table_name FROM information_schema.tables
   WHERE table_schema=DATABASE()
     AND table_name IN ('auth_throttles','security_audit')
   ORDER BY table_name;
   SHOW COLUMNS FROM admins LIKE 'last_login';"
