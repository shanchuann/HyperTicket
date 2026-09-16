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

mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name" \
  < "$project_dir/db/migrate_v7_payment_domain.sql"

verified="$(mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name" -Nse "
  SELECT
    (SELECT COUNT(*) FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments'
        AND COLUMN_NAME IN ('amount_minor','currency','provider','client_idempotency_key'))=4
    AND
    (SELECT COUNT(*) FROM information_schema.TABLES
      WHERE TABLE_SCHEMA=DATABASE()
        AND TABLE_NAME IN ('payment_events','refunds','payment_webhook_events'))=3
    AND
    (SELECT COUNT(*) FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='payments'
        AND COLUMN_NAME IN ('amount','method','settle_at'))=0;")"

if [[ "$verified" != "1" ]]; then
  echo "Payment domain migration verification failed." >&2
  exit 1
fi

echo "Payment domain migration applied."
