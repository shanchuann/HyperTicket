#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"
read_env() { sed -n "s/^$1=//p" "$env_file" | tail -n 1 | tr -d '\r'; }

db_host="$(read_env DB_HOST)"
db_port="$(read_env DB_PORT)"
db_user="$(read_env DB_USER)"
db_name="$(read_env DB_NAME)"
export MYSQL_PWD="$(read_env DB_PASSWORD)"

mysql_server=(mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user")
mysql_db=("${mysql_server[@]}" "$db_name")
"${mysql_server[@]}" < "$project_dir/db/init.sql"
"${mysql_db[@]}" < "$project_dir/db/migrate_v10_event_catalog.sql"
"${mysql_db[@]}" < "$project_dir/db/migrate_v11_schema_version.sql"

version="$("${mysql_db[@]}" -Nse 'SELECT COALESCE(MAX(version),0) FROM schema_migrations')"
test "$version" = "11" || { echo "Expected schema version 11, found $version" >&2; exit 1; }
echo "HyperTicket schema bootstrapped at version 11."
